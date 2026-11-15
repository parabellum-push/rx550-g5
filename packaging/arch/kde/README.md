# KDE patches (big-endian), for upstream

Found while bringing up KDE Plasma 6.7.5 on the RX 550 / Power Mac G5 (Arch POWER ppc64).
These are KDE bugs, not part of the rx550-g5 packages; they are here to be sent to KDE.

| Patch | Bug | Effect on big-endian |
|---|---|---|
| `plasma-workspace-6.7.5-outputorderwatcher-big-endian.patch` | `OutputOrderWatcher` (X11) dereferences the `uint8_t *` from `xcb_randr_get_output_property_data()`, reading 1 byte of the 32-bit `_KDE_SCREEN_INDEX` | Plasma **X11**: plasmashell sees no screen, black desktop with only a cursor. Tested on the G5 (equivalent 1-instruction binary change: desktop and panel appear). |
| `kwin-6.7.5-legacy-addfb-depth.patch` | DRM backend falls back to `drmModeAddFB(depth 24)` for every format when ADDFB2 returns `EOPNOTSUPP` | Plasma **Wayland** on kernels without big-endian ADDFB2: 10-bit scanout buffers shown as XRGB8888 (garbled colours), cursor without alpha. Not compile-tested; the rx550-g5 kernel (patch 0005) accepts ADDFB2, so this fallback is not reached with it. |
