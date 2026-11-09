# Installing

## Requirements

- Power Mac G5 (tested: PPC970MP) with a Polaris card (tested: RX 550, 4 GB).
- Debian sid ppc64 (debian-ports) or MintPPC ppc64.
- `firmware-amd-graphics`. MintPPC carries it. On plain debian-ports it is missing (it is an
  `Architecture: all` package from Debian's non-free-firmware, which debian-ports does not mirror):
  download it from a Debian mirror and install it with the rest.
- From debian-ports: `libllvm22`, and for OpenCL `libclc-22`, `libclang-cpp22`,
  `libllvmspirvlib22.1`, `ocl-icd-libopencl1` (apt pulls them in).

If the original GPU stays installed and has no monitor attached, the G5 firmware can hang at boot.
An HDMI EDID dummy plug on the original card avoids that.

## Install

Download one release (all `.deb` files and `SHA256SUMS`) into an empty directory, then:

```sh
sha256sum -c SHA256SUMS
sudo apt-get install -s ./*.deb   # check: no unexpected "Remv" lines
sudo apt install ./*.deb
sudo reboot
```

The `rx550-g5` package makes the `-rx550g5` kernel the default, loads `amdgpu` before the display
manager, sets the driver options a G5 needs, configures X for the RX 550, and pins the Mesa packages
so distribution updates do not replace them.

## Check

```sh
glxinfo -B | grep -E "renderer|version"     # radeonsi, Mesa ...+rx550g5.N
vulkaninfo --summary | grep -E "deviceName|driverInfo"
clinfo | grep -E "Platform Name|Device Name"  # rusticl, AMD Radeon RX 550
vainfo
```

## Remove

`sudo apt remove rx550-g5` drops the pin and boot integration; reinstalling the distribution Mesa
packages (`apt install --reinstall` with the stock versions) returns to the stock drivers. The stock
kernel stays installed and can be chosen in GRUB.
