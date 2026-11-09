# Status

Hardware: one Power Mac G5 (PPC970MP, 16 GB), one Radeon RX 550 (Polaris 11), Debian sid ppc64.
Release under test: Mesa `26.2.6-1+rx550g5.3`, kernel `7.2.9-1+rx550g5.1`, support package
`rx550-g5 1.0+20261109.1`.

## What works

- Desktop on the RX 550 (X11, glamor), up to 2560x1440@144.
- OpenGL 4.6 / OpenGL ES 3.2 (radeonsi; LLVM on big endian).
- Vulkan 1.3 (RADV, ACO).
- OpenCL 3.1 (Rusticl on radeonsi, on by default in these packages; fp64 with `RUSTICL_FEATURES=fp64`).
- VA-API decode (H.264, HEVC 8/10-bit, MPEG-2, VC-1, MJPEG) and VCE encode (H.264, HEVC).
  Mesa 25.3 removed VDPAU upstream.

## Test results (exact release package bytes)

| Suite | Cases | Result |
|---|---|---|
| GL CTS GLES2 / GLES3 / GLES31 / KHR-GL46 | 2,040 / 3,819 / 2,977 / 9,051 | 0 regressions vs previous release |
| Vulkan CTS stratified sample + mutable | 54,968 + 8,592 | 0 regressions; 2 fails (`info.*_extensions`: CTS 1.3.8 older than the driver) |
| OpenCL CTS quick list | see below | |
| VCE encode | H.264, HEVC 640x360 | PSNR 47.6 / 46.9 dB, decodes in software |

OpenCL CTS (quick list) known failures, none of them big-endian specific:

- Image sampling with float coordinates exactly at texel edges: the GFX8 sampler quantises
  unnormalized coordinates to about 9 fraction bits (measured: border down to -2^-9, texel 0 from -2^-10).
- `fma`, `multiply` with flushed denormals: the GPU detects tininess before rounding, so a result that
  rounds up to the smallest normal comes back as 0.
- `tgamma`: the CTS host reference on PPC (IBM double-double `long double`) expects inf where the true
  value underflows to -0; one more case is 16.7 ulp against a 16 ulp limit.
- OpenCL-GL sharing suite not built.

Big image suites need more than an hour on a 970MP because the CTS computes reference images on the
CPU. Use the offline SPIR-V flow in `test-runners/ocl/offline` to move kernel compilation to a fast
machine.

## Known issues

- One hard GPU hang (gfx ring timeout) was seen once during a GL CTS run while an OpenCL image test
  ran at the same time. It did not reproduce in three further full GLES3 runs under the same load.
- First build of an OpenCL program takes about 10 s on a 970MP (clang and libclc); later builds come
  from the shader cache (~1 s). Driver updates empty the cache.
- Rusticl prints "Patched Mesa libclc not detected" with Debian's libclc; that is the same on every
  Debian architecture.

## Upstream candidates (not big-endian specific)

- `ac_nir_to_llvm`: scratch loads/stores without their alignment land in the neighbouring lane on GFX6-8.
- radeonsi compute image copies with DCC on compute-only contexts lose float NaN payloads.
- `u_printf`: vector length modifiers dropped; unused arguments not skipped.
- rusticl: libclc 22 imports `__clc_flush_denormal_if_not_supported`, which rusticl does not provide.
- `nir_nextafter`: the `x * 1.0` denormal flush is removed by `nir_opt_algebraic`.
- radeonsi GFX8 reports sRGB texel buffers that the hardware cannot decode.
