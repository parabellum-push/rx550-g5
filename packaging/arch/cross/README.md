# Cross-building the Arch POWER packages on x86_64

The release packages for Arch POWER (ppc64, big-endian, **ELFv2**) were built on an x86_64 PC, not
on the G5. Arch POWER's ppc64 userland uses the ELFv2 ABI (`/usr/lib/ld64.so.2`), which the usual
`powerpc64-linux-gnu` cross toolchains (Debian/Ubuntu, ELFv1) cannot target, so the scripts build:

1. `sysroot.sh` - an Arch POWER sysroot from the repository packages listed in `sysroot-packages.txt`
   (download them on the G5 with `pacman -Sw --cachedir DIR ...`, copy DIR here).
2. `build-toolchain.sh` - binutils 2.47 and GCC 16.2.0 for `powerpc64-unknown-linux-gnu` with
   `--with-abi=elfv2 --with-cpu=970` against that sysroot (the same versions as Arch POWER),
   and Rust std for `powerpc64-archpower-linux-gnu.json` (the built-in `powerpc64-unknown-linux-gnu`
   target with `abi`/`llvm-abiname` = `elfv2` and the ELFv2 data layout) with `-Zbuild-std`.
3. `build-kernel.sh` - linux 7.2.12 + the patches + `../linux-ppc64-rx550g5/config`.
4. `build-mesa.sh` - meson cross build of the mesa-rx550-be tag with the options of
   `../mesa-rx550g5/PKGBUILD`, then the split into the five packages.
5. `mkpacpkg.py` - writes `.pkg.tar.zst` files (`.PKGINFO`, `.BUILDINFO`, `.MTREE`, root-owned) the
   way makepkg does, from a staged tree and a small JSON file (`meta/`).

Host tools: meson + mako in a venv, a native `wayland-scanner`, native `mesa_clc`/`vtn_bindgen2` from
the same Mesa source, rustc/cargo 1.99.1 with `rust-src`, `bindgen`, libclang 18. bindgen runs against
GCC 13's libstdc++ headers (`-nostdinc++`), because libclang 18 cannot parse GCC 16's; the only C++
declarations it reads are three LLVM functions with opaque types (`rusticl_llvm_bindings.hpp`).

Compiler flags are Arch POWER's `makepkg.conf` ones with `-mcpu=970`. Do not add `-fno-plt`: with it,
calls to `__tls_get_addr` became inline-PLT sequences and the first thread-local access in
libgallium crashed (binutils 2.42). The kernel is built with the distribution's
`powerpc64-linux-gnu-gcc` (any ppc64 GCC can build an ELFv2 kernel).

Two pitfalls when cross-building Mesa: meson's `sys_root` property exports `PKG_CONFIG_SYSROOT_DIR`,
so `arch-pkg-config` clears it for libclc variables (they are run-time paths compiled into Rusticl);
and `strings libRusticlOpenCL.so | grep share/clc` should print `/usr/share/clc/...`.

The native PKGBUILDs in `../` are the recipes for building the same packages on Arch POWER itself.
