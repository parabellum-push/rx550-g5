#!/bin/bash
# usage: configure26.sh SRC BUILD   -- Debian 26.2.6-1 ppc64 option set (debian/rules), cross from x86
SRC=$1 B=$2; S=$HOME/g5-work/m26/sysroot
export PATH=$HOME/g5-work/m26/hosttools/bin:$HOME/g5-work/m26/venv/bin:$HOME/g5-work/hosttools/usr/bin:$PATH
meson setup "$B" "$SRC" --cross-file=$HOME/g5-work/m26/cross26.ini -Dbuild.pkg_config_path=$HOME/g5-work/m26/hosttools/pc \
 -Dprefix=/usr -Dlibdir=lib/powerpc64-linux-gnu -Dsysconfdir=/etc -Dlocalstatedir=/var \
 -Dbuildtype=plain -Doptimization=2 -Ddebug=true -Db_ndebug=true -Dwrap_mode=nodownload \
 -Dc_args="-mcpu=970 -idirafter $HOME/g5-work/m26/sysroot/usr/include -idirafter $HOME/g5-work/m26/sysroot/usr/include/powerpc64-linux-gnu -g -O2 -fstack-protector-strong -fstack-clash-protection -Wformat -Werror=format-security -Wall" \
 -Dcpp_args="-mcpu=970 -idirafter $HOME/g5-work/m26/sysroot/usr/include -idirafter $HOME/g5-work/m26/sysroot/usr/include/powerpc64-linux-gnu -g -O2 -fstack-protector-strong -fstack-clash-protection -Wformat -Werror=format-security -Wall" \
 -Ddri-drivers-path=/usr/lib/powerpc64-linux-gnu/dri -Dplatforms=x11,wayland \
 -Dvulkan-drivers=amd,swrast,virtio,gfxstream -Dvulkan-layers=anti-lag,device-select,intel-nullhw,overlay,screenshot \
 -Dvulkan-manifest-per-architecture=false -Dglvnd=enabled -Dbuild-tests=false -Dtools=drm-shim \
 -Dglx-direct=true -Dgbm=enabled -Dgallium-extra-hud=true -Dlmsensors=enabled -Dgallium-va=enabled -Dvideo-codecs=all \
 -Dllvm=enabled -Dgallium-drivers=softpipe,nouveau,r300,r600,virgl,radeonsi,zink,llvmpipe \
 -Dgles1=disabled -Dgles2=enabled -Dvalgrind=enabled -Dteflon=false "${@:3}"
