# Native port status

This report counts only native mechanisms with explicit evidence. The complete original decompilation is deliberately neither the denominator nor a list of missing work.

Explicit evidence scope: **91.0 %** weighted, **82.7 %** fully evidenced (587/710 mechanisms; not overall game completion).

Event commands (bhScenarioJmpT): 223/223 bound, 0 consumed only, 0 missing.

| Area | Mechanisms | Evidence scope | Fully evidenced | Partial | Open |
|---|---:|---:|---:|---:|---:|
| Enemies/AI | 94/114 | 91.2 % | 82.5 % | 20 | 0 |
| Effects | 232/251 | 96.2 % | 92.4 % | 19 | 0 |
| Events/script | 78/93 | 91.9 % | 83.9 % | 15 | 0 |
| Player/combat | 51/69 | 87.0 % | 73.9 % | 18 | 0 |
| Room/camera/door | 16/40 | 70.0 % | 40.0 % | 24 | 0 |
| Menu/items/text | 47/61 | 85.2 % | 77.0 % | 10 | 4 |
| Models/animation | 10/16 | 78.1 % | 62.5 % | 5 | 1 |
| Audio | 29/32 | 95.3 % | 90.6 % | 3 | 0 |
| Flow/saving | 30/34 | 94.1 % | 88.2 % | 4 | 0 |
| Platform (host-replaced) | 22/38 | 78.9 % | 57.9 % | 16 | 0 |

Only ledger entries with independent native evidence count. Matching names, original sources, and quarantined generators never count as port evidence.

## Largest open functions

- `adv.c` Adv_GameOptionScreen (missing, Original option plates for display, sound, vibration, and key configuration remain unported; OPTION opens documented host settings instead.)
- `adv.c` CheckMcSelectPortInfoState (missing, Original memory-card port query remains unported; host slot selection reads occupied saves from the user-data directory.)
- `adv.c` FindFirstVmDrive (missing, Original memory-card search remains unported; host saves in the platform user-data directory enable LOAD GAME when a slot is occupied.)
- `face.c` bhControlMask:flg2-0x80 (missing, See the detailed evidence and open bindings in docs/port_ledger.tsv.)
- `ranking.c` ControlRanking (missing, See the detailed evidence and open bindings in docs/port_ledger.tsv.)
- `MdlPut.c` bhPutModel (partial, Mirror pass from MdlPut.c:57-79/89-111 is partially connected: qualifying models draw a second time under cam.mtx*njMirror, with light based on unmirrored cam.mtx. Player floor, enemy, and object/item flags are connected; hair and weapons rely on measured status bytes. Child enemies check their own byte. mirror_pass has two red control cases.)
- `Motion.c` bhSetMotion:Spieler (partial, Player node pose uses the existing object tree before motion advance, including Hokan countdown. Action/event, combat, damage, grab, push, and draw edits are connected; player_motion_blend and pose traces cover transitions. Stairs, Game Over, and script-driven player motion still do not update this tree.)
- `adv.c` AdvGetCancelButton (partial, Host Cancel is used instead of the original keytype table {32,128,64}.)
- `adv.c` AdvGetOkButton (partial, Host Confirm represents both Start (0x800) and keytype OK; original keytype tables {192,96,160} are not bound.)
- `adv.c` ResetAdvSystem (partial, Only title fields NextReturnCode/ExtraFlag/NextMode/GenFlag and StopAdvScreenSaver(1) are connected; texture lists, option fields, and VM fields are missing.)
- `adxwrap.c` SetVolumeAdx2 (partial, See the detailed evidence and open bindings in docs/port_ledger.tsv.)
- `camera.c` bhControlCamera (partial, The runtime owns cam.ofx/ofy/ofz across rooms, and RoomWorld::camera adds the offset to cam.px. Original writers EN02/EN13/EN17, event 0x5a, and bhEff30a are connected. Room load clears the offset as bhInitCamera does. Runtime observation of room-change reset and a separate bhGetCameraPosition check remain open.)
- `cut.c` bhControlActiveCamera (partial, For stair state, the camera follows owP[0] from the last bhCalcModel rather than the player position. RM_3080 no longer stalls; camera_follow_target has a red control case. Free-control EXP_WORK interpolation, bhCheckCutAttribute floor input, and eye-camera return remain open.)
- `cut.c` bhControlEventCamera (partial, RoomWorld::control_event_camera follows the mode0 0/1/2/3 state machine and 20 spline slots; the paused camera holds its pose. RM_0040 event 5 now holds key 1 as in RDT. A native owner for pt_flg 0x1 and proven CAM_WORK narrowing for cam.pers remain open.)
- `cut.c` bhSetCut (partial, RoomWorld::cut_fog_handover carries cut fog and fog_ct rules through initialize_camera_clip. Other bhSetCut clip/hide portions remain as previously recorded.)

## Unconfirmed historical ledger entries

- `en02.c` bhEne02: Historical extraction/generator evidence is invalid for the native port. A native Gulp Worm state machine on WorldEntity covers init, movement, underground pursuit, emergence/swallowing, rocks, retreat, damage, sand effects, player control, camera offset, audio, and hit marks; en02_world and RM_5830 headless checks are recorded. The uninitialized ang in SetSandEffectMain case 8 is evidenced by disassembly; three sprites are omitted while particles run. RM_3091/RM_7220 and visible window acceptance remain open.
- `event.c` bhPlItemChangeM: Historical extraction/generator evidence is invalid. First hit, packed ID, and original quantity/flag removal were checked.
- `event.c` bhPlItemTamaSet: Historical extraction/generator evidence is invalid. First hit and u16 little-endian quantity were checked.
- `hitchk.c` bhCheckWall2Box: Historical extraction/generator evidence is invalid. Original query and geometry helpers were extracted; static/mobile ATR walls and prior object position were bound, with wall and second-crate tests.
- `hitchk.c` bhGetGroundPosition: Historical extraction/generator evidence is invalid. The generated ground query kept room/mobile index and live ATR work. Player push/stair binding, linked casing effect, EE random state, and first-free eff[512] mapping were reported; other random consumers, exact EasyLight, full effect lifecycle, and general player work remain open.
- `ps2_NaSystem.c` njColorBlendingModeSys: Historical extraction/generator evidence is invalid. Native chunk_blend.cpp models nj/GS saved state, ALPHA register selection, and RGB=(A-B)*C+D; FBA=0, PABE, GS packet quantization, and dc_alpha remain unmodeled. Blend, effect, weapon-flash, lighter, and runtime checks are recorded.
- `sub1.c` GetItem: Historical extraction/generator evidence is invalid. GetItem including branch 153 and subsequent ItemSort were extracted; StatusCancel, sprite transitions, and full StatusMain were not completely bound.

Generated with `python3 tools/port_check.py --markdown docs/PORT_STATUS.md`.
