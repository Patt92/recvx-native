# Application icon

`app-icon.icns` and `app-icon.ico` are converted from
`data/app-resources/app-icon.jpg` (1024 × 1024, RGB). The source artwork is
unchanged: no crop, transparency or additional decoration. These ready-to-use
resources are versioned here so ordinary builds do not require Pillow or the
private data tree.

To regenerate after changing the source, install Pillow (conversion verified
with 12.3.0), then run from the project root:

```bash
python3 tools/build_app_icons.py
```

The optional converter uses Lanczos resampling. ICO contains 16, 24, 32, 48,
64, 128 and 256 px images; ICNS contains the macOS/Retina representations up to
1024 px. `ios/AppIcon*.png` are the iOS bundle icons (120, 180, 152 and
167 px RGB); `tools/build_native.py` copies them into the iOS app.
`tvos/AppIcon400x240(@2x).png` is the flat tvOS icon (400x240 pt, the square
artwork centred on the corner colour); the layered App Store icon is not produced. Both macOS bundle writers copy the ICNS before signing and set
`CFBundleIconFile`. Windows embeds the ICO in `recvx-runtime.exe` via RC;
SDL2 also selects that first group icon for its window.

Normal builds do not regenerate artwork. The source JPG remains part of the
local, ignored `data/` tree; provide it to reproduce or update these resources.
