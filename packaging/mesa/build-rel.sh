#!/bin/bash
# usage: build-rel.sh VERSION   e.g. build-rel.sh 26.2.6-1+rx550g5.2
# Release build of the g5-26 branch: clean export (no .git, so no git hash in the version
# string), VERSION = full package version as debian/rules writes it, Debian option set,
# ninja -j15, install to stage-rel-VERSION, then lay out the .debs in rel-VERSION/.
set -e
VER=$1; [ -n "$VER" ] || { echo "usage: $0 VERSION"; exit 1; }
M=$HOME/g5-work/m26
SRC=$M/src-rel-$VER B=$M/b-rel-$VER STAGE=$M/stage-rel-$VER OUT=$M/rel-$VER
# OpenCL (rusticl) since g5.3: rust ppc64 target, bindgen (LIBCLANG_PATH), native mesa_clc/vtn_bindgen2 wrappers.
export PATH=$M/hosttools-ocl:$HOME/.cargo/bin:$M/hosttools/bin:$M/venv/bin:$HOME/g5-work/hosttools/usr/bin:$PATH
export LIBCLANG_PATH=/usr/lib/llvm-18/lib
if [ ! -d "$SRC" ]; then
   mkdir -p "$SRC"
   git -C $M/port archive g5-26 | tar -x -C "$SRC"
   echo "$VER" > "$SRC/VERSION"
fi
[ -f "$B/build.ninja" ] || $M/configure26-ocl.sh "$SRC" "$B" -Dgallium-rusticl=true -Dgallium-rusticl-enable-drivers=radeonsi \
   -Dmesa-clc=system -Dmesa-clc-bundle-headers=enabled > $M/conf-rel-$VER.log 2>&1
nice -n 10 ninja -j15 -C "$B" > $M/build-rel-$VER.log 2>&1 || { tail -20 $M/build-rel-$VER.log; exit 1; }
rm -rf "$STAGE"
DESTDIR="$STAGE" meson install -C "$B" --no-rebuild > $M/install-rel-$VER.log 2>&1
git -C $M/port diff v26 g5-26 > $M/rx550-g5-vs-mesa-26.2.6-$VER.diff
python3 $M/pkg/mkpkgs26.py "$STAGE" "$OUT" "$VER" $M/rx550-g5-vs-mesa-26.2.6-$VER.diff
git -C $M/port rev-parse g5-26 > "$OUT/SOURCE-COMMIT"
(cd "$OUT" && sha256sum *.deb > SHA256SUMS)
echo "release $VER in $OUT (commit $(cat $OUT/SOURCE-COMMIT))"
