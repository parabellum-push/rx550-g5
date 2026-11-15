# Install on Arch POWER (ppc64)

Packages for [Arch POWER](https://archlinuxpower.org/) `powerpc64` (big-endian, ELFv2). They are in
the release `rx550g5.3-arch1`. Same driver source as the Debian/MintPPC release `rx550g5.3`.

| Package | Replaces | What |
|---|---|---|
| `linux-ppc64-rx550g5` | (installs next to `linux-ppc64`) | linux 7.2.12, Arch POWER config, big-endian amdgpu patches |
| `mesa-rx550g5` | `mesa` | Mesa 26.2.6 big-endian GFX8 (radeonsi on LLVM, GL 4.6, EGL, GBM, VA-API) |
| `vulkan-radeon-rx550g5` | `vulkan-radeon` | RADV, Vulkan 1.3 |
| `vulkan-swrast-rx550g5` | `vulkan-swrast` | lavapipe |
| `vulkan-mesa-layers-rx550g5` | `vulkan-mesa-layers`, `vulkan-mesa-implicit-layers` | Mesa Vulkan layers |
| `opencl-mesa-rx550g5` | `opencl-mesa` | Rusticl (OpenCL 3.1) on radeonsi |
| `rx550-g5` | | boot integration, driver options, X config |
| `rx550-g5-kde` (optional) | | KDE Plasma X11 big-endian fix (pacman hook for plasma-workspace) |

Each Mesa package `provides` and `conflicts` with the Arch package, so pacman offers to replace the
stock one, and an Arch Mesa update does not replace the big-endian build later.

## Requirements

- Power Mac G5 with a Radeon RX 550 (other Polaris/GFX8 boards untested).
- Keep the original graphics card (GeForce 6600 or similar) installed: the G5 firmware needs it to boot.
- GRUB as the boot loader (the Arch POWER default on the G5).

## Install

Download all `.pkg.tar.zst` files and `SHA256SUMS` from the release into one directory, then:

```sh
sha256sum -c SHA256SUMS
sudo pacman -S --needed linux-firmware-amdgpu
sudo pacman -U $(ls *.pkg.tar.zst | grep -v rx550-g5-kde)   # answer y to replace mesa, vulkan-radeon, ...
sudo pacman -U ./rx550-g5-kde-*.pkg.tar.zst                  # optional, only with KDE Plasma installed
sudo reboot
```

GRUB then starts `linux-ppc64-rx550g5` by default (`/etc/default/grub.d/90-rx550-g5.cfg` sets
`GRUB_TOP_LEVEL`, and a pacman hook runs `grub-mkconfig` after kernel installs). The stock
`linux-ppc64` stays under "Advanced options".

Desktop: KDE Plasma **(Wayland)** works with the main packages and is the recommended session
(SDDM default). Plasma **(X11)** needs the optional `rx550-g5-kde` package: plasma-workspace 6.7.5
reads an X11 screen property as one byte, so on big-endian plasmashell shows a black desktop. The
package's pacman hook fixes that one instruction in `libkworkspace6.so` after every plasma-workspace
install or upgrade, changes nothing if the pattern is not found exactly once (for example once KDE
fixes it), and reverts it on removal (`/usr/lib/rx550-g5/kde-be-fix --check` shows the state). The
source patch for KDE is in `packaging/arch/kde/`. Suspend is disabled by `rx550-g5`: the G5 does not
wake from it.

To get a KDE desktop on a fresh Arch POWER install:

```sh
sudo pacman -S plasma sddm konsole        # the "plasma" group; plasma-meta is not in Arch POWER
sudo pacman -U ./rx550-g5-kde-*.pkg.tar.zst
sudo systemctl enable --now sddm
```

Check after the reboot:

```sh
uname -r                                   # 7.2.12-1-rx550g5
cat /sys/module/amdgpu/version             # g5-be-...
glxinfo -B | grep -E 'renderer|version'    # AMD Radeon RX 550 ... (radeonsi, polaris11, LLVM 22...)
vulkaninfo --summary | grep deviceName     # AMD Radeon RX 550 (RADV POLARIS11)
```

## Differences from the Debian packages

- Kernel: Arch POWER's configuration uses **64 KiB pages**, so the TTM limit that keeps GPU memory
  inside the G5's 2 GB DART window is `ttm pages_limit=24576` (1.5 GB) instead of 393216.
- No apt pin: the separate package names do the same job.
- Mesa carries four Arch patches (`packaging/arch/mesa-rx550g5/`): a glibc 2.44 C11-threads build fix;
  RADV host-pointer import alignment and sparse-image alignment for 64 KiB pages; EGL/Wayland
  byte-layout formats on big-endian (Wayland EGL clients crashed in `eglSwapBuffers`).
- The kernel carries two Arch patches on top of the amdgpu series: ADDFB2 accepted on big-endian
  (Wayland compositors otherwise fall back to a depth-24 legacy framebuffer and show garbage), and
  the text console byte order (was yellow instead of grey).
- Built with GCC 16 for ELFv2 (Debian: GCC 13, ELFv1).

## Remove / go back to stock

```sh
sudo pacman -S mesa vulkan-radeon vulkan-swrast vulkan-mesa-layers opencl-mesa   # y to replace
sudo pacman -R rx550-g5-kde      # if installed; reverts the KDE fix
sudo pacman -R rx550-g5 linux-ppc64-rx550g5
```

Recovery at boot: add `rx550.disable=1` to the kernel command line in GRUB (press `e`), or pick
`linux-ppc64` under "Advanced options".

## Building

`packaging/arch/` has native PKGBUILDs (`linux-ppc64-rx550g5`, `mesa-rx550g5`, `rx550-g5`) and
`packaging/arch/cross/`, the x86_64 cross build that produced the release packages.
