# Native assets and mods

The productive tree is `data/`. Runtime and packages read regular loose files,
not ISO/AFS containers. Builds may optionally extract a PS2 ISO9660 image into
that tree (`./build.sh --iso game.iso`, see below); the game itself still opens
neither ISO nor AFS. All original dumped files and split archive entries are
included, including assets whose gameplay systems are still pending. Room,
playable-player, menu, font and item textures are cooked, and so are the NPC, enemy,
object and room-item textures of all 205 rooms (`recvx-world-assets`, 8,526 files
added; 18,253 TGA in total, 6,178 unique by content). All 18 movies have native packets.
`archived/` is historical storage, not an input to normal builds or launches.

## Overrides

Put replacements under the same relative texture path in external `mods/`.
Windows places it beside Start.cmd; macOS places it beside RECVX Native.app.
`--mods <directory>` selects another location. Reload/restart after changes.
External mods do not alter the app signature.

| Asset | Path in data/mods |
|---|---|
| Stage texture | `textures/rooms/RM_0000/0000.tga` |
| Playable Claire texture | `textures/players/0010/0000.tga` |
| Opening cinematic Claire texture | `textures/npcs/RM_0000/0000/0000.tga` |
| Rodrigo texture | `textures/npcs/RM_0000/0001/0000.tga` |
| Room object texture | `textures/objects/RM_0000/0004/0000.tga` |
| Room item texture | `textures/room-items/RM_0000/0000/0000.tga` |
| Lighter examination texture | `textures/items/0055/0000.tga` |
| Menu atlases | `textures/menu/0000.tga` through `0004.tga` |
| Glyph atlases | `textures/fonts/0000.tga` through `0003.tga` |
| Stage geometry/events | `rooms/RM_0000.RDT` |
| Player model/motion/skin packet | `players/0010.bin` (override only) |
| Item model packet | `items/0055.bin` (override only) |
| Voice or music | `audio/VOICE1/0000.adx`, `audio/BGM1/0000.adx` (override only) |
| Menu sound bank | `audio/system/COMMON.MLT` (override only) |
| Room shot sound pack | `audio/MULTSPQ1/0000.bin` (override only; pack ID from MULTSPQ1.IDX) |
| Native film/audio packet | `movies/0000.rvv` |
| Room effect atlas | `textures/effects/RM_0020/0070/0000.tga` (room and atlas index of the effect) |

Texture IDs are zero-based per bundle. The cinematic and playable Claire are
separate original asset bundles and require their respective overrides. Loose
original archive records remain under `archives/ARCHIVE_NAME/0000.bin`; playable
Claire defaults to SYSTEM/0010.bin, and the lighter (item ID 55, inventory slot 1) to ITEM1/0054.bin.

Use uncompressed RGB24/RGBA32 TGA without color maps, 1–8192 pixels per dimension.
The runtime also reads RLE (type 10) and 8-bit colour-mapped TGA (types 1/9, 24/32-bit palette
entries); both expand to the same pixels. `prepare_native_data.py --compressed` writes every cooked
texture as colour-mapped RLE through `recvx-texture-compress` (all cooked textures have at most 256
colours; 4.2 GB -> 0.8 GB), verifying each file against its source. HD keys (`mods/textures/hd`)
stay the MD5 of the uncompressed cooked file, so author HD packs against `data/`, not a compressed tree.
Higher resolution preserves original UVs and asset identities. Original PS2
palette/alpha rules are applied during export; TGA uses ordinary 0–255 alpha.
The 2D renderer blends alpha. The 3D renderer filters textures trilinearly with up to 4x anisotropic taps
(alpha-weighted mip chains), blends partly covered cut-out pixels by their coverage
and renders with 2x2 supersampling on all CPU cores (`--ss 1..4`, `--nearest` for the
old point sampling; `--render-threads N` fixes the raster worker count for energy
measurements, the image is identical for every count). On macOS the band stage is rasterized by
Metal (`native/gpu_raster.h`, `video.ini` `renderer = gpu|software`, `--renderer`): the CPU keeps
clipping, strip rules, sorting and shadow masks, the GPU draws the same setup list; point-sampled
output is bit-identical (`gpu_raster_parity`). On Windows the same setup list is rasterized by Direct3D 11
through rasterizer ordered views; GPUs without ROV support keep the software path. That backend is
written but not yet verified on hardware. The original punch-through effect pass instead filters RGB independently
of alpha, rejects alpha-zero samples and writes the remaining colour and depth without blending.
Its host mip chain keeps the same independent RGB filtering; filtered alpha-test quantization
and equal-depth comparisons remain open. Other material transparency gaps remain documented
in the port ledger. Invalid overrides fail with a diagnostic.

## HD texture packs (content-keyed)

Besides per-asset paths, `<mods>/textures/hd/<md5>.tga` replaces every texture
whose cooked data file (`data/textures/...`) has that MD5, in any room or asset.
One file therefore serves all rooms that share a texture (5,130 unique cooked
textures behind 9,727 files). Precedence: per-asset mod file > HD file > cooked
data > decoded original. Alpha comes from the cooked texture; RGB from the pack.

Building a pack from a Dolphin GameCube HD pack (`GCDE08`, DDS BC7, 4x):

```sh
python3 -m venv build/texvenv && build/texvenv/bin/pip install pillow numpy
build/texvenv/bin/python tools/match_hd_textures.py --pack <.../Load/Textures/GCDE08> --output build/hd-matches.json
build/texvenv/bin/python tools/build_hd_textures.py --matches build/hd-matches.json --output build/hd-textures --scale 2
build/macos-arm64/recvx-runtime --data data --mods build/hd-textures
```

Importing a PCSX2 texture replacement pack (`<TEX0 hash>-<CLUT hash>-<bits>.dds`, e.g. copied
straight into `mods/textures/`). Its keys are xxHash values over PCSX2's GS memory image, which the
port does not have, so the files cannot be read directly; the importer takes the PS2 size from the
name's `bits` (TW/TH), box-downscales each DDS to it and matches it against cooked textures of that
size (same tiers as below, Pillow only):

```sh
python3 tools/import_pcsx2_textures.py --pack mods/textures --output mods            # --scale 4 default
```

It writes `mods/textures/hd/<md5>.tga` plus `mods/textures/hd-manifest-pcsx2.json` (matched and
unmatched files with NCC/PSNR) and keeps existing HD files. At startup the runtime reports a pack it
finds in `mods/textures/` and whether it was imported. Name plates and texts the pack redraws (map
names, file screens) only match if the cooked texture still has the same layout.

The runtime looks in `<mods>` first; without `--mods` it uses `mods/` beside `data/`
(the app/Start scripts do the same), so a pack placed in `mods/textures/hd/` is
picked up automatically. Delete the folder to go back to the original look.

Matching decodes each DDS, box-downsamples it to the GameCube size in its file
name and compares luma correlation (NCC) with PS2 textures of the same size.
Tier A: NCC >= 0.90 and >= 0.05 ahead of the runner-up; tier B: 0.80..0.90 and
>= 0.15 ahead. Everything else keeps the original. With the GCDE08 pack and the
complete export:

| Category | unique | matched |
|---|---|---|
| rooms | 4,893 | 3,341 (68 %) |
| npcs/enemies | 453 | 301 (66 %) |
| objects | 524 | 238 (45 %) |
| players | 32 | 25 (78 %) |
| items | 135 | 17 (13 %) |
| menu | 5 | 2 |
| fonts | 39 | 4 |
| room-items | 97 | 2 |
| total | 6,178 | 3,930 (64 %), covering 10,330 of 18,253 files |

2x output is 3.7 GB uncompressed TGA (4x would be about 18 GB). Sampled pairs were
all correct (rooms tier A 24/24, tier B 12/12, actors/objects/players 18/18), but
this is statistical matching, not a proof. Remaining gap: items, menu, fonts and
room-items match poorly because the pack covers them sparsely or their atlas
layout differs; the mipmap/filter limits of the software renderer still apply.
Exact matching through the GameCube dump is possible (see AGENTS.md) but not built.

## Cooking and integrity

The existing complete export can be checked without any historical source:

```sh
python3 tools/prepare_native_data.py --output data --verify
```

To rebuild the manifest after intentional cooking changes:

```sh
python3 tools/prepare_native_data.py --output data --refresh
```

Refresh records the new tree; use verify for integrity checking. It does not
validate new files against the disc. A separate fresh export from an already
extracted directory is supported explicitly with `--source <dump-directory>
--output <new-directory>`; it is not required for the current build.
`recvx-world-assets data RM_0000` exports the supported opening actor/object
textures and refuses overwrites. `python3 tools/cook_movies.py --data data`
converts loose dumped PSS movies with build-time FFmpeg/ffprobe; existing outputs
are skipped. The game itself needs neither FFmpeg nor the dumped PSS files.
See AUDIO.md for conversion format and synchronization.

### Building from a PS2 ISO (optional)

Instead of supplying an extracted disc directory, pass one or more PS2 ISO9660
images. A source checkout plus the ISO is enough to produce the complete `data/`
tree:

```sh
./build.sh --iso /path/to/game.iso
```

The build compiles the exporters first and then runs, in a staging directory:

1. ISO extraction to the original disc layout (temporary),
2. `recvx-assets`: disc files, split AFS entries, expanded rooms and the room,
   player, menu, font and item textures, verified against the extracted disc,
3. `recvx-world-assets` for every exported room: NPC/enemy, object and
   room-item textures,
4. `tools/cook_movies.py`: native movie packets from the dumped PSS files,
5. `asset-manifest.json`.

Only a fully successful run is moved to the `--data` path; a failure leaves no
partial tree behind. The build needs FFmpeg/ffprobe for step 4 (`--ffmpeg`,
`--ffprobe` on `prepare_native_data.py`); the game itself does not.

`--iso` is repeatable; identical files shared by both discs are merged and
conflicting files are rejected. If `data/` already contains a completed export,
the build keeps it and does not extract again; an existing directory without a
completed export is refused rather than overwritten. Use a fresh `--data`
directory to re-extract. The standalone tool lists or extracts an image without
building:

```sh
python3 tools/iso9660.py game.iso --list
python3 tools/iso9660.py game.iso --extract build/disc-tree
```

The same path is available directly:

```sh
python3 tools/prepare_native_data.py --iso game.iso --output data-iso --exporter build/macos-arm64/recvx-assets
```

The reader uses only the Python standard library, accepts 2048-byte ISO and raw
2352/2336-byte captures, verifies both-endian ISO metadata, rejects unsafe path
names and refuses interleaved or multi-extent directories. The runtime and the
packaged game still read only `data/`; no ISO/AFS access is added to gameplay.

## Model import boundary

The current model override accepts the supported original Ninja packet format,
including compatible skeleton, skin and motion records. glTF/FBX/RE Engine model
import and automatic retargeting are not implemented. Importing a RE2 Remake
model will require geometry/material conversion, rest-pose conversion, mapping
to Claire's 22-bone body skeleton and compatible weights. Merely renaming an FBX
file to .bin cannot work. PlayerAsset is separated from inventory/event state,
providing a boundary for a future importer.

Cooked data and packages are local ignored outputs, not part of source Git.
Packages include the full manifest and external empty mods directory.

## Door resources

Door type 0 defaults to `archives/MRY/0071.bin`, type 1 to `0072.bin`, etc.
These MPAC packets contain the original MDL, PVP textures, sound-bank reference
and PMB camera/door/fade parameters. Native procedures are generated from
`src/ps2/veronica/prog/door.c` and compiled for the host.
Use `mods/doors/0000.bin` to override the complete packet for door type 0,
or `mods/textures/doors/0000/0000.tga` for its first texture. Other types follow
the same numbering. Original embedded PVP textures provide the base fallback.
Special light procedures and rumble remain pending; door sounds are described in AUDIO.md.

The rotating examination view renders original 3D item geometry. Exported TGA
files are replaceable original resources; screenshots are not loaded by gameplay.
Obsolete verification images are stored under ignored `archived/diagnostics/`.

The main menu decodes the original title texture from ADV/0002.bin resource 0.
Its 640x480 visible crop uses the original padded 1024x512 texture. Override it
with `mods/textures/title/0000.tga`; HD replacements retain the crop proportions.
