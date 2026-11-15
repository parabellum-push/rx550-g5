#!/bin/bash
# usage: sysroot.sh PKGDIR SYSROOT  - unpack Arch POWER packages (pacman -Sw output) into SYSROOT
set -e
mkdir -p "$2"
for f in "$1"/*.pkg.tar.*; do
  case $f in *.sig) continue;; esac
  tar -xf "$f" -C "$2" --exclude=.PKGINFO --exclude=.BUILDINFO --exclude=.MTREE --exclude=.INSTALL
done
