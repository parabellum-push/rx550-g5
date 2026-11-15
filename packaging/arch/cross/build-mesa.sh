#!/bin/bash
# usage: build-mesa.sh WORK  - cross-build mesa-rx550g5 into WORK/mesa/split/<package>
# Needs WORK/sysroot, WORK/xgcc, WORK/rust (build-toolchain.sh) and host tools on PATH
# (meson, mako, wayland-scanner, mesa_clc, vtn_bindgen2, bindgen); LIBCLANG_PATH for bindgen.
set -e
W=$1; T=$(cd "$(dirname "$0")" && pwd); H=$T/../mesa-rx550g5; M=$W/mesa
export ARCH_SYSROOT=$W/sysroot RUSTC_BOOTSTRAP=1
sed -e "s#@W@#$W#g" -e "s#@T@#$T#g" $T/cross-arch.ini.in > $W/cross-arch.ini
mkdir -p $M && cd $M
rm -rf src && git clone --depth 1 -b 26.2.6-1+rx550g5.3 https://github.com/parabellum-push/mesa-rx550-be.git src
echo "26.2.6-1.rx550g5.3" > src/VERSION
(cd src && patch -Np1 < $H/0001-glibc-c11-threads-casts.patch)
# Arch POWER makepkg.conf CFLAGS (no -fno-plt: inline-PLT __tls_get_addr calls broke TLS), -mcpu=970
F="-mcpu=970 -O2 -pipe -fexceptions -Wp,-D_FORTIFY_SOURCE=2 -Wformat -Werror=format-security -fno-omit-frame-pointer -g1"
LF="-Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now"
rm -rf b
meson setup b src --cross-file=$W/cross-arch.ini \
 -Dprefix=/usr -Dlibdir=lib -Dsysconfdir=/etc -Dlocalstatedir=/var -Dbuildtype=plain -Db_ndebug=true -Dwrap_mode=nodownload \
 -Dc_args="$F" -Dcpp_args="$F -Wp,-D_GLIBCXX_ASSERTIONS" -Dc_link_args="$LF" -Dcpp_link_args="$LF" \
 -Dplatforms=x11,wayland -Dvulkan-drivers=amd,swrast -Dvulkan-layers=anti-lag,device-select,overlay,screenshot \
 -Dvulkan-manifest-per-architecture=false \
 -Dglvnd=enabled -Dbuild-tests=false -Dtools=[] -Dglx-direct=true -Dgbm=enabled -Dgallium-extra-hud=true -Dlmsensors=enabled \
 -Dgallium-va=enabled -Dvideo-codecs=all -Dllvm=enabled -Dshared-llvm=enabled \
 -Dgallium-drivers=softpipe,llvmpipe,nouveau,r300,r600,radeonsi,virgl,zink \
 -Dgallium-rusticl=true -Dgallium-rusticl-enable-drivers=radeonsi -Dmesa-clc=system -Dmesa-clc-bundle-headers=enabled \
 -Dgles1=disabled -Dgles2=enabled -Dvalgrind=disabled -Dlibunwind=disabled -Dteflon=false -Dandroid-libbacktrace=disabled
ninja -j15 -C b
rm -rf stage split && DESTDIR=$M/stage meson install -C b --no-rebuild
SP=$M/split; mkdir -p $SP; cp -a stage $SP/mesa-rx550g5; cd $SP/mesa-rx550g5
pick(){ p=$1; shift; for f in "$@"; do for g in $f; do mkdir -p $SP/$p/$(dirname $g); mv $g $SP/$p/$g; done; done; }
pick vulkan-radeon-rx550g5 usr/lib/libvulkan_radeon.so usr/share/vulkan/icd.d/radeon_icd.json usr/share/drirc.d/00-radv-defaults.conf
pick vulkan-swrast-rx550g5 usr/lib/libvulkan_lvp.so usr/share/vulkan/icd.d/lvp_icd.json usr/share/drirc.d/00-lavapipe-defaults.conf
pick vulkan-mesa-layers-rx550g5 "usr/lib/libVkLayer_*.so" usr/share/vulkan/explicit_layer.d usr/share/vulkan/implicit_layer.d \
     usr/bin/mesa-overlay-control.py usr/bin/mesa-screenshot-control.py
pick opencl-mesa-rx550g5 etc/OpenCL "usr/lib/libRusticlOpenCL.so*"
rmdir -p usr/share/vulkan/icd.d usr/bin etc 2>/dev/null || true
cd $SP
for p in *; do install -Dm644 $M/src/docs/license.rst $p/usr/share/licenses/$p/license.rst; done
install -Dm644 $H/README.rx550-g5 mesa-rx550g5/usr/share/doc/mesa-rx550g5/README.rx550-g5
find . -type f -name "*.so*" -exec powerpc64-linux-gnu-strip --strip-unneeded {} +
for p in *; do python3 $T/mkpacpkg.py $SP/$p $T/meta/$p.json $W/out; done
