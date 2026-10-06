Effekt-Update 2026-10-06: Flammen und Funken verwenden die originalen
EF_WRK-/Atlasressourcen aus losen Raum-RDTs, keine Ersatzbilder. Overrides:
`mods/textures/effects/RM_0020/0070/0000.tga` (entsprechend Atlas 58/59/70–79,
jeweils tatsächlichen Raum/Index einsetzen). Daten-TGAs am selben relativen
Pfad haben Vorrang vor der dekodierten RDT-Textur; HD-Content-Key bleibt erhalten.
Framekopien teilen Pixel und Mips. Bearbeitete Texturen werden abgetrennt und
invalidieren den Cache. Eff200/201/218 gebunden; kein vollständiger Effektrenderer.

# Native assets and mods 0.5

The productive tree is `data/`. Runtime and packages read regular loose files,
not ISO/AFS containers. All original dumped files and split archive entries are
included, including assets whose gameplay systems are still pending. Room,
playable-player, menu, font and item textures are cooked; the opening room also
has 53 exported NPC/object/room-item textures. All 18 movies have native packets.
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

Texture IDs are zero-based per bundle. The cinematic and playable Claire are
separate original asset bundles and require their respective overrides. Loose
original archive records remain under `archives/ARCHIVE_NAME/0000.bin`; playable
Claire defaults to SYSTEM/0010.bin, and the lighter (item ID 55, inventory slot 1) to ITEM1/0054.bin.

Use uncompressed RGB24/RGBA32 TGA without color maps, 1–8192 pixels per dimension.
Higher resolution preserves original UVs and asset identities. Original PS2
palette/alpha rules are applied during export; TGA uses ordinary 0–255 alpha.
The 2D renderer blends alpha. The 3D renderer currently discards fully transparent
pixels and draws other pixels opaque; material transparency/filtering/mipmaps
remain incomplete. Invalid overrides fail with a diagnostic.

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
Sound-bank playback, special light procedures and rumble remain pending.

The rotating examination view renders original 3D item geometry. Exported TGA
files are replaceable original resources; screenshots are not loaded by gameplay.
Obsolete verification images are stored under ignored `archived/diagnostics/`.

The main menu decodes the original title texture from ADV/0002.bin resource 0.
Its 640x480 visible crop uses the original padded 1024x512 texture. Override it
with `mods/textures/title/0000.tga`; HD replacements retain the crop proportions.
