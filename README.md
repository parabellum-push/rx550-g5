# !Warning! This repo is vibe-coded.
I am only sharing it since it works on my machine. 

# AMD Radeon RX 550 on a Power Mac G5

Everything needed to run a Polaris GPU (RX 550, GFX8) in a **big-endian** Power Mac G5
(PPC970MP) under Debian / MintPPC ppc64: OpenGL 4.6, OpenGL ES 3.2, Vulkan 1.3, OpenCL 3.1
(Rusticl), VA-API decode and VCE encode.

The AMD drivers assume a little-endian CPU. This project fixes that in three places:

| Repository | What |
|---|---|
| [linux-rx550-be](https://github.com/parabellum-push/linux-rx550-be) | amdgpu kernel driver patches (linux 7.2.9) |
| [mesa-rx550-be](https://github.com/parabellum-push/mesa-rx550-be) | Mesa 26.2.6 (branch `g5-26`) and 24.1.6 (`g5-24`): radeonsi, RADV, Rusticl |
| this repo | Debian packaging, the `rx550-g5` support package, probes, test runners, CTS patches |

Ready-made packages are in the [Releases](https://github.com/parabellum-push/rx550-g5/releases).

## Install (packages)

See [docs/INSTALL.md](docs/INSTALL.md).
on Debian ppc64 / MintPPC with the AMD firmware available:

```sh
sudo apt install ./linux-image-*-rx550g5*.deb ./*.deb
sudo reboot
```

## Layout

- `packaging/mesa/`: release build (`build-rel.sh`, meson cross files for building on x86_64) and
  `mkpkgs26.py`, which lays the build out as replacements for the Debian 26.2.6-1 packages with
  identical file lists (plus `mesa-opencl-icd`, following Debian's ppc64el package).
- `packaging/support/`: the `rx550-g5` package (boot integration, driver options, X configuration,
  hotplug helper, apt pin).
- `probes/`: small C programs that each check one big-endian behaviour on the device
  (OpenCL private memory, half-float images, fp64, texel buffers, printf, ...).
- `test-runners/`: resumable runners for the Khronos GL, Vulkan and OpenCL CTS, plus the
  PC-side offline OpenCL compiler (`test-runners/ocl/offline`) that moves kernel compilation off the G5.
- `cts-patches/`: fixes for big-endian bugs **in the test suites themselves** (VK-GL-CTS,
  OpenCL-CTS), with the upstream commits they apply to.
- `docs/`: [STATUS](docs/STATUS.md) (what works, test results, known issues), [INSTALL](docs/INSTALL.md).

## Status in one paragraph

On the release packages: GL CTS (17,887 cases) and a stratified Vulkan CTS sample (63,560
cases) show no regressions between releases; OpenCL CTS quick list passes except for documented
hardware-precision cases. Tested on one G5 with one RX 550. This is not a Khronos conformance
submission, and nothing has been submitted upstream yet. Details in [docs/STATUS.md](docs/STATUS.md).

## Licenses

Mesa changes: MIT. Kernel patches: GPL-2.0. CTS patches: Apache-2.0.
Scripts, probes and packaging in this repo: MIT. Packages in the releases contain
Debian's own documentation and copyright files unchanged.
