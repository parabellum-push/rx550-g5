#!/bin/bash
# vce26.sh ROOT OUTDIR: VA-API encode (VCE) H.264 + HEVC with a Mesa root, decode in software, PSNR vs source.
R=${1:-$HOME/m26/root}; O=${2:-$HOME/m26/vce}; mkdir -p $O
L=$R/usr/lib/powerpc64-linux-gnu
export LD_LIBRARY_PATH=$L:$HOME/m26/llvm LIBVA_DRIVERS_PATH=$L/dri LIBVA_DRIVER_NAME=radeonsi
F="ffmpeg -nostdin -hide_banner -loglevel error -y"
SRC="-f lavfi -i testsrc2=size=640x360:rate=30:duration=2"
$F $SRC -pix_fmt nv12 -f rawvideo $O/src.nv12
r0=$(dmesg | grep -ciE "ring .*timeout")
for c in h264 hevc; do
  timeout 120 $F -vaapi_device /dev/dri/renderD128 -f rawvideo -pix_fmt nv12 -s 640x360 -r 30 -i $O/src.nv12 \
     -vf format=nv12,hwupload -c:v ${c}_vaapi -qp 24 $O/enc.$c 2> $O/enc.$c.err; rc=$?
  sz=$(stat -c %s $O/enc.$c 2>/dev/null)
  frames=$(ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of csv=p=0 $O/enc.$c 2>/dev/null)
  psnr=$(ffmpeg -nostdin -hide_banner -i $O/enc.$c -f rawvideo -pix_fmt nv12 -s 640x360 -r 30 -i $O/src.nv12 -lavfi psnr -f null - 2>&1 | grep -o "average:[^ ]*")
  echo "$c rc=$rc size=$sz frames=$frames psnr $psnr err=$(head -c 150 $O/enc.$c.err | tr "\n" " ")"
done
echo "ring timeouts before $r0 after $(dmesg | grep -ciE "ring .*timeout")"
