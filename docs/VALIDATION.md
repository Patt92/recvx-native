# Local validation of native 0.5

Validated on 2026-10-05. This records checked behavior, not completion of the port.

| Check | Result |
|---|---|
| macOS universal build | arm64 and x86_64 slices present |
| Windows build | PE32+ x64 executable and bundled SDL2 |
| CTest | 8/8 on macOS; 8/8 under Wine |
| ASan + UBSan | 8/8 and original-data opening regression passed |
| Intel/Rosetta | original-data opening/UI regression passed |
| Disc data manifest | 12,702 files; 6,575,300,120 bytes; both packaged trees verified |
| Room scene decoder | 205/205, 1,008,630 stage triangles, 470 active camera targets |
| Five playable bodies | 272 supported body clips; all 7,901 frames sampled |
| Opening without skipping | 15,000 simulated ticks, original movie completed, cell hint/camera 37 reached |
| Original skip/lighter path | hint confirmation, inventory equip, floor condition, Rodrigo voices, restored position/control |
| Post-event movement | 120 running ticks moved Claire through the same native movement/collision path |
| Dialogue rendering | no panel; white core and black edge; uncovered pixels preserve the scene |
| Side pack | original GM flag switches 8/10 normal cells and cursor bounds; frame row changes |
| Native skin normals | weighted influence rotation tested separately from positions |
| Packaged launch | own packaged data; Windows replay under Wine; bounded macOS SDL window/audio start |

The original-data opening regression passes with 127 known unbound commands.
Particle effects, lighter/flame object, full MASK/face/lip processing, camera
interpolation, original SE banks/footsteps, combat, room transitions and complete
puzzle/progression/save systems remain pending. A successful room decode does
not verify that room's gameplay. Wine validation is not a physical Windows test.

The original ISO, previous Git and obsolete PS2 compiler/CRI/Katana distributions
are preserved under ignored archived/. The new source Git excludes private data,
archives and all build/package outputs. No remote was configured or pushed.
