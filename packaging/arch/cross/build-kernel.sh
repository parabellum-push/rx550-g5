#!/bin/bash
# usage: build-kernel.sh WORK  - cross-build linux-ppc64-rx550g5 into WORK/kernel/stage
set -e
W=$1; H=$(cd "$(dirname "$0")/.." && pwd)/linux-ppc64-rx550g5; K=$W/kernel
mkdir -p $K && cd $K
[ -f linux-7.2.12.tar.xz ] || curl -fLO https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-7.2.12.tar.xz
rm -rf linux-7.2.12 && tar xf linux-7.2.12.tar.xz && cd linux-7.2.12
for p in debian-radeon-amdgpu-firmware-is-required-for-drm-and-kms-on-r600-onward.patch \
         0001-amdgpu.patch 0002-display.patch 0003-powerplay.patch 0004-macintosh.patch \
         0005-drm-amdgpu-allow-addfb2-on-big-endian.patch 0006-drm-fbdev-le-framebuffers-on-big-endian.patch; do
  patch -Np1 < $H/$p
done
cp $H/config .config
M="make ARCH=powerpc CROSS_COMPILE=powerpc64-linux-gnu-"
$M olddefconfig && $M -j15 vmlinux modules
KR=$($M -s kernelrelease); ST=$K/stage; rm -rf $ST
$M INSTALL_MOD_PATH=$ST/usr INSTALL_MOD_STRIP=1 DEPMOD=/bin/true modules_install
cp vmlinux $ST/usr/lib/modules/$KR/vmlinuz
echo linux-ppc64-rx550g5 > $ST/usr/lib/modules/$KR/pkgbase
rm -f $ST/usr/lib/modules/$KR/build $ST/usr/lib/modules/$KR/source
