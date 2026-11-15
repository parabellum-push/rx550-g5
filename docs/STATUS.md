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
- OpenCL FP32 throughput: kernels built on `fma()` reach about 1/4 of peak (370 GFLOP/s on an RX 550)
  because Polaris runs `v_fma_f32` at quarter rate; `mad()` or `a*b+c` compile to `v_mad/v_mac_f32` and
  reach 1,100 GFLOP/s. 32-bit integer multiply (`v_mul_lo_u32`) is also quarter rate; `mad24` is full rate.
  This is the hardware, not the port (`probes/fmaprobe.c`).
- Rusticl prints "Patched Mesa libclc not detected" with Debian's libclc; that is the same on every
  Debian architecture.

## Arch POWER (release `rx550g5.3-arch1`)

Hardware: Power Mac G5 Quad (PowerMac11,2, 2x PPC970MP dual core), same RX 550, Arch POWER
`powerpc64` (ELFv2, 64 KiB pages), KDE Plasma 6.7.5. Packages: kernel `linux-ppc64-rx550g5`
7.2.12, Mesa `26.2.6.rx550g5.3` (same driver source as the Debian release plus four Arch patches),
`rx550-g5 1.0+20261115.2`, optional `rx550-g5-kde 1.0+20261115.1`. Built on x86_64 with the cross
toolchain in `packaging/arch/cross/`.

| Suite | Cases | Result |
|---|---|---|
| GL CTS GLES2 / GLES3 / GLES31 / KHR-GL46 | 2,040 / 3,819 / 2,977 / 9,051 | 0 regressions vs Debian `rx550g5.3` |
| Vulkan CTS stratified sample + mutable | 54,968 + 8,592 | 0 regressions |
| Desktop | KDE Plasma Wayland and X11, SDDM | full desktop, correct colours and cursor (checked on screen) |

The three failures (Vulkan `info.*_extensions` x2, one GLES3 shader compile case) also fail in the
Debian baseline.

Found on Arch and fixed in these packages:

- Mesa EGL/Wayland used native-endian 8-bit RGB formats; on big-endian every Wayland EGL client,
  plasmashell included, crashed in `eglSwapBuffers` (Mesa patch 0004).
- The DRM core refused `ADDFB2` on big-endian, so KWin fell back to legacy `ADDFB`, which registers
  every buffer as XRGB8888: wrong colours for 10-bit frames and a blue ARGB cursor (kernel patch 0005).
- The fbdev emulation reported little-endian channel offsets wrongly on big-endian: yellow text console
  (kernel patch 0006).
- RADV host-pointer import and sparse-image alignment assumed 4 KiB pages (Mesa patches 0002, 0003).
- The TTM page limit that keeps GPU memory inside the 2 GB DART window is counted in 64 KiB pages
  (`pages_limit=24576`).
- Suspend: the G5 does not wake from s2idle, and Plasma's idle suspend looked like a hang; `rx550-g5`
  disables suspend and hibernation.

Not fixed in our packages (KDE bugs, patches in `packaging/arch/kde/` for KDE):

- plasma-workspace 6.7.5 reads the 32-bit X11 `_KDE_SCREEN_INDEX` property as one byte: black Plasma
  X11 desktop on big-endian. The optional `rx550-g5-kde` package fixes the installed library.
- KWin's legacy `ADDFB` fallback ignores the buffer's real format (not reached with our kernel; the
  patch is not compile-tested).

Known issues on Arch:

- An unclean shutdown can leave the HFS `/boot/grub` partition marked dirty, and Linux then mounts it
  read-only; `hfsprogs` fsck cannot check plain HFS on big-endian.

## Upstream candidates (not big-endian specific)

- `ac_nir_to_llvm`: scratch loads/stores without their alignment land in the neighbouring lane on GFX6-8.
- radeonsi compute image copies with DCC on compute-only contexts lose float NaN payloads.
- `u_printf`: vector length modifiers dropped; unused arguments not skipped.
- rusticl: libclc 22 imports `__clc_flush_denormal_if_not_supported`, which rusticl does not provide.
- `nir_nextafter`: the `x * 1.0` denormal flush is removed by `nir_opt_algebraic`.
- radeonsi GFX8 reports sRGB texel buffers that the hardware cannot decode.
