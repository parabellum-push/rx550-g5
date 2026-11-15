#!/bin/bash
# usage: build-toolchain.sh WORK   (WORK/sysroot must exist)
set -e
W=$1; SR=$W/sysroot; X=$W/xgcc
mkdir -p $X/bin $W/xgcc-src $W/xbin-src
# binutils 2.47 (Arch POWER's version). The distribution's powerpc64-linux-gnu binutils 2.42
# produced a broken TLS access (__tls_get_addr) in an earlier -fno-plt build; use the same
# linker as Arch POWER.
cd $W/xbin-src
[ -f binutils-2.47.tar.xz ] || curl -fLO https://ftp.gnu.org/gnu/binutils/binutils-2.47.tar.xz
[ -d binutils-2.47 ] || tar xf binutils-2.47.tar.xz
mkdir -p b && cd b
../binutils-2.47/configure --target=powerpc64-unknown-linux-gnu --prefix=$W/xbin --with-sysroot=$SR \
  --disable-nls --disable-werror --disable-gdb --disable-gprofng --enable-plugins
make -j15 && make install
for t in addr2line ar as ld ld.bfd nm ranlib strip objcopy objdump readelf; do
  ln -sf $W/xbin/bin/powerpc64-unknown-linux-gnu-$t $X/bin/powerpc64-unknown-linux-gnu-$t
done
cd $W/xgcc-src
[ -f gcc-16.2.0.tar.xz ] || curl -fLO https://ftp.gnu.org/gnu/gcc/gcc-16.2.0/gcc-16.2.0.tar.xz
[ -d gcc-16.2.0 ] || { tar xf gcc-16.2.0.tar.xz; (cd gcc-16.2.0 && ./contrib/download_prerequisites); }
mkdir -p b && cd b
export PATH=$X/bin:$PATH
../gcc-16.2.0/configure --target=powerpc64-unknown-linux-gnu --prefix=$X --with-sysroot=$SR \
  --with-native-system-header-dir=/usr/include \
  --with-as=$X/bin/powerpc64-unknown-linux-gnu-as --with-ld=$X/bin/powerpc64-unknown-linux-gnu-ld \
  --with-abi=elfv2 --with-cpu=970 --enable-languages=c,c++ --disable-multilib --disable-bootstrap \
  --disable-libsanitizer --disable-nls --enable-shared --enable-threads=posix --enable-__cxa_atexit \
  --enable-default-pie --enable-default-ssp --enable-gnu-unique-object --enable-linker-build-id \
  --with-glibc-version=2.44 --enable-secureplt --disable-werror
make -j15 && make install

# Rust std for the ELFv2 target
R=$W/rust; T=powerpc64-archpower-linux-gnu
mkdir -p $R/dummy/src $R/sysroot/lib/rustlib/$T/lib
cp "$(dirname "$0")/$T.json" $R/
cd $R/dummy
printf '[package]\nname="dummy"\nversion="0.1.0"\nedition="2021"\n[lib]\npath="src/lib.rs"\n' > Cargo.toml
: > src/lib.rs
RUSTC_BOOTSTRAP=1 cargo build --release -Zbuild-std=std,panic_unwind,core,alloc,proc_macro \
  -Zjson-target-spec --target ../$T.json
cp target/$T/release/deps/*.rlib target/$T/release/deps/*.rmeta $R/sysroot/lib/rustlib/$T/lib/
rm -f $R/sysroot/lib/rustlib/$T/lib/libdummy*
ln -sfn "$(rustc --print sysroot)/lib/rustlib/x86_64-unknown-linux-gnu" $R/sysroot/lib/rustlib/x86_64-unknown-linux-gnu
