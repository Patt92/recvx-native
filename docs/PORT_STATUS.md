# Native port status

This report counts only native mechanisms with explicit evidence. The complete original decompilation is deliberately neither the denominator nor a list of missing work.

Explicit evidence scope: **94.0 %** weighted, **88.3 %** fully evidenced (659/746 mechanisms; not overall game completion).

Event commands (bhScenarioJmpT): 223/223 bound, 0 consumed only, 0 missing.

| Area | Mechanisms | Evidence scope | Fully evidenced | Partial | Open |
|---|---:|---:|---:|---:|---:|
| Enemies/AI | 107/118 | 95.3 % | 90.7 % | 11 | 0 |
| Effects | 242/251 | 98.2 % | 96.4 % | 9 | 0 |
| Events/script | 89/97 | 95.9 % | 91.8 % | 8 | 0 |
| Player/combat | 63/81 | 88.9 % | 77.8 % | 18 | 0 |
| Room/camera/door | 33/49 | 83.7 % | 67.3 % | 16 | 0 |
| Menu/items/text | 51/64 | 87.5 % | 79.7 % | 10 | 3 |
| Models/animation | 13/18 | 86.1 % | 72.2 % | 5 | 0 |
| Audio | 29/32 | 95.3 % | 90.6 % | 3 | 0 |
| Flow/saving | 32/36 | 94.4 % | 88.9 % | 4 | 0 |
| Platform (host-replaced) | 24/41 | 79.3 % | 58.5 % | 17 | 0 |

Only ledger entries with independent native evidence count. Matching names, original sources, and quarantined generators never count as port evidence.

## Largest open functions

- `adv.c` Adv_GameOptionScreen (missing, Original option plates for display, sound, vibration, and key configuration remain unported; OPTION opens documented host settings instead.)
- `adv.c` CheckMcSelectPortInfoState (missing, Original memory-card port query remains unported; host slot selection reads occupied saves from the user-data directory.)
- `adv.c` FindFirstVmDrive (missing, Original memory-card search remains unported; host saves in the platform user-data directory enable LOAD GAME when a slot is occupied.)
- `MdlPut.c` bhPutModel (partial, Mirror pass from MdlPut.c:57-79/89-111 is partially connected: qualifying models draw a second time under cam.mtx*njMirror, with light based on unmirrored cam.mtx. Player floor, enemy, and object/item flags are connected; hair and weapons rely on measured status bytes. Child enemies check their own byte. mirror_pass has two red control cases.)
- `Motion.c` bhSetMotion:Spieler (partial, Player node pose uses the existing object tree before motion advance, including Hokan countdown. Action/event, combat, damage, grab, push, and draw edits are connected; player_motion_blend and pose traces cover transitions. Stairs, Game Over, and script-driven player motion still do not update this tree.)
- `adv.c` AdvGetCancelButton (partial, Host Cancel is used instead of the original keytype table {32,128,64}.)
- `adv.c` AdvGetOkButton (partial, Host Confirm represents both Start (0x800) and keytype OK; original keytype tables {192,96,160} are not bound.)
- `adv.c` ResetAdvSystem (partial, Only title fields NextReturnCode/ExtraFlag/NextMode/GenFlag and StopAdvScreenSaver(1) are connected; texture lists, option fields, and VM fields are missing.)
- `adxwrap.c` SetVolumeAdx2 (partial, See the detailed evidence and open bindings in docs/port_ledger.tsv.)
- `camera.c` bhControlCamera (partial, The runtime owns cam.ofx/ofy/ofz across rooms, and RoomWorld::camera adds the offset to cam.px. Original writers EN02/EN13/EN17, event 0x5a, and bhEff30a are connected. Room load clears the offset as bhInitCamera does. Runtime observation of room-change reset and a separate bhGetCameraPosition check remain open.)
- `cut.c` bhControlActiveCamera (partial, For stair state, the camera follows owP[0] from the last bhCalcModel rather than the player position. RM_3080 no longer stalls; camera_follow_target has a red control case. Free-control EXP_WORK interpolation, bhCheckCutAttribute floor input, and eye-camera return remain open.)
- `cut.c` bhSetEventCamera (partial, Opcode 0x13 mode 0 now installs signed key position/angles, perspective, lock angles, and fog in both branches before event-camera initialization. Previous-key fields, cam.ln/flags, and bhSetEventHideObjLgt remain open.)
- `effect.c` bhDrawTrsEffect3D (partial, See the detailed evidence and open bindings in docs/port_ledger.tsv.)
- `effsub1.c` bhEff009 (partial, See the detailed evidence and open bindings in docs/port_ledger.tsv.)
- `effsub1b.c` bhDraw136 (partial, See the detailed evidence and open bindings in docs/port_ledger.tsv.)

Generated with `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
