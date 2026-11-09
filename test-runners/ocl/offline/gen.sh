#!/bin/bash
# gen.sh CACHEDIR: compile every dumped .cl (+ .options) to .spv with the PC offline compiler, 15 in parallel.
D=$1; INFO=${INFO:-$HOME/g5-work/ocl-cts/offline/rx550-device.info}
find "$D" -name "*.cl" -print0 | xargs -0 -P15 -I{} sh -c '
  f="{}"; o="${f%.cl}.options"; s="${f%.cl}.spv64"; [ -f "$s" ] && exit 0
  opt=""; [ -f "$o" ] && opt=$(head -1 "$o")
  ~/g5-work/ocl-cts/offline/cl_offline_compiler --source "$f" --output "$s" ${INFO:+--cl-device-info "$INFO"} --mode spir-v -- "$opt" 2>>"$f.err" || echo "FAIL $f"'
# Validate here: the G5's spirv-val misreads little-endian modules on big endian (run the CTS there
# with --disable-spirv-validation).
N=~/g5-work/m26/nativedeps/root
bad=$(find "$D" -name "*.spv64" -print0 | xargs -0 -P15 -I{} sh -c "LD_LIBRARY_PATH=$N/usr/lib/x86_64-linux-gnu $N/usr/bin/spirv-val --target-env opencl2.2 {} >/dev/null 2>&1 || echo {}" | wc -l)
echo "spv: $(find "$D" -name "*.spv64" | wc -l) / cl: $(find "$D" -name "*.cl" | wc -l), spirv-val failures: $bad"
