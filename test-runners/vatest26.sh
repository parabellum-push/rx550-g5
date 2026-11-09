#!/bin/bash
# vatest26.sh OUTDIR: VA-API decode with the private Mesa 26 build vs ffmpeg software decode.
# Per stream: framemd5 hw (hwdownload) vs sw in the same pixel format. Logs dmesg ring timeouts.
O=${1:-$HOME/m26/va}; mkdir -p $O; cd ~/rx550-video-fixtures-20261026
. ~/m26/env26.sh
export LIBVA_DRIVERS_PATH=$HOME/m26/root/usr/lib/powerpc64-linux-gnu/dri LIBVA_DRIVER_NAME=radeonsi
F="ffmpeg -nostdin -hide_banner -loglevel error"
for s in rx550-h264-gop-640x360-20261026.h264:nv12 rx550-hevc8-gop-20261026.hevc:nv12 rx550-mpeg2-gop-20261026.m2v:nv12 \
         rx550-hevc10-gop-20261026.hevc:p010le rx550-mjpeg-420-20261027.mjpg:nv12 SA00040.vc1:nv12; do
  f=${s%%:*}; p=${s##*:}; n=${f%.*}
  timeout 120 $F -hwaccel vaapi -hwaccel_device /dev/dri/renderD128 -hwaccel_output_format vaapi -i $f \
     -vf hwdownload,format=$p -f framemd5 $O/$n.hw.md5 2> $O/$n.hw.err; rc=$?
  timeout 300 $F -i $f -pix_fmt $p -f framemd5 $O/$n.sw.md5 2> $O/$n.sw.err
  hw=$(grep -vc '^#' $O/$n.hw.md5 2>/dev/null); sw=$(grep -vc '^#' $O/$n.sw.md5 2>/dev/null)
  same=$(diff <(grep -v '^#' $O/$n.hw.md5 | awk -F, '{print $NF}') <(grep -v '^#' $O/$n.sw.md5 | awk -F, '{print $NF}') | grep -c '^<')
  echo "$n rc=$rc frames hw=$hw sw=$sw differing=$same err=$(head -c 120 $O/$n.hw.err | tr '\n' ' ')"
  dmesg | tail -50 | grep -iE 'timeout|ring .* reset|uvd.*fail' | tail -2
done
