# Historical handoff: coordinating the native game port

Status: 2026-10-10, Europe/Berlin. Repository:
`/Users/patrickhoffmann/Projects/recvx-decomp`.
This records an earlier assignment and inventory, not gameplay acceptance.
Current `AGENTS.md` and user decisions take precedence.

## Successor assignment

Take over integration and coordination of open packages. Preserve existing
ports, coordinate clear file ownership, and connect mechanisms to productive
native gameplay. Merely distributing tasks or making tests green is
insufficient. The overall goal remains a standalone native port of the
decoded/decompiled game on macOS arm64 and Windows x64, preserving original
gameplay with **every enemy** in the productive World. No PS2 recreation,
original core, or invented replacement logic. Preserve original order, flags,
tables, states, and side effects; fix common causes.

## Non-negotiable rules

- Read the current `AGENTS.md` fully. Newer user clarifications override
  historical states and success reports.
- Run `python3 tools/check_no_original_core.py` before work and every build.
  Report/resolve violations first; no bypass or new exception/allowlist.
- Never read, search, copy, restore, or use `archived/FORBIDDEN_DONTUSE`, even
  indirectly through history, other tasks, archives, or caches. Only new,
  explicit user authorization could permit restoration.
- Original sources and disassembly outside quarantine are semantic
  references. Do not compile/link/call the original engine or use copied or
  generated function bodies under another name. ELF/MMI evidence can clarify
  behavior, not become a runtime component.
- Existing native Player/RoomWorld/WorldEntity/Inventory/Effect owners remain
  authoritative. No second sys/plp/rom/ene/eff world, mirrored `BH_PWORK`
  table, or copy-in/copy-out original engine.
- No heuristics, guessed constants, room/enemy special cases, or silent no-ops
  as substitutes. Record unresolved original semantics as open.
- Do not weaken effect tests to neutral gap responses.
- Local changes and untracked files belong to the user/peers. No reset,
  stashing/removing others' work, commit, push, or starter package without an
  explicit request. This handoff requested `apply_patch` for file edits.

## Initial steps recorded by this handoff

1. Run `git status --short`, read the current `AGENTS.md`, and run the mandatory
   checker. At this handoff it reported PASS; HEAD was `8b951f5` (savepoint).
   Many important native files, tests, reports, and task documents were
   untracked. Active peers could change the tree at any time.
2. Determine active agents and their actual claimed files. Recent user
   messages do not replace a current ownership check. Message other chats
   only with user authorization; use agent tools for explicitly delegated
   subtasks, and do not create new chats without a request. Avoid duplicate
   work.
3. Maintain a compact ownership/dependency table and enemy-ID matrix. Give
   new parallel tasks disjoint files and minimal contracts.
4. Compare document status with current code and newest package reports.
   `CLAUDE_NEXT_TASKS.md` and much of the roadmap are historical: do not
   treat claims such as “Inventory/Computer/ECG missing” or “R02 open” as new
   tasks without checking. Resolve visible conflicts.
5. Determine the current build state in a separate build directory without
   disturbing peer builds. This handoff claimed no new full build; only
   status, policy, and documents had been checked.

## Ownership and assignment at that time

An integrator owned `CMakeLists.txt`, `native/world.*`, `runtime.cpp`,
`events.cpp`, shared state/asset contracts, and documentation integration.
Renderer/Scene interfaces also required exclusive agreement. Peers provided
their modules and small connection patches; no concurrent changes to these
files. Any ownership transfer had to be agreed and documented first.

**Enemy agent:** The user explicitly broadened the initial C01 task to
**C01–C08 including all C07+ IDs**. The controlling handoff was
[historical enemy assignment](../work/history/2026-10-21-enemy-assignment/THIRD_AGENT_C01_TASK.md). The agent was last analyzing zombie/dog traps
in `enemy_spawn.*`; that was useful C01 work, not the entire assignment.
Confirm the agent read the updated file; do not claim the change had reached
them already.

Derive the complete original ID/variant/child/dispatcher map, agree C02
contracts on the existing owner, then port every enemy mechanism and asset:
init, event spawn, motion/interpolation, AI, attack/grab/damage/death, child
lifecycle, draw, and SFX where present. Separate C05/Subplayer by actual ID
table; include EN00. Implement and integrate all enemies/assets first, then
test/fix them together. Policy and required build/safety checks remain active.
The first required productive acceptance was EN07 in RM_0130 through original
spawn/events, a complete visible model, advancing animation, AI, and SFX.
Accept the remaining enemies afterward; bat acceptance does not finish the
overall goal.

**Smaller model, if delegated:** A source-backed enemy-ID/asset inventory in
`work/enemy_id_asset_inventory.md` only: original routine, native site,
model/motion, gaps, and file/line evidence for each ID. No code changes,
invented semantics, generators, or quarantine. “File exists” is not proof of
functionality. This subtask had been proposed, not started.

**Other peers:** B04/hair and B01/stairs had been handled separately. Reports
claimed progress and rework, but active owners needed rechecking.
`native/player_head.c/.h` were then untracked; do not start a second head-work
port without comparison. Combat A01–A05, remaining effects, pose, and renderer
had their own contracts; the enemy agent must not quietly recreate them with
replacement logic.

## Partial results and open limits at that time

These claims came from package reports, `AGENTS.md`, or user messages and were
not all rerun at the handoff.

| Area | Reported state | Open check |
|---|---|---|
| R02/policy/data | Reports said build generators were removed, pure data frozen, function generators quarantined; mandatory checker PASS. | Reconcile historical R02 gaps; PASS is not gameplay acceptance. |
| D01–D09 | Inventory, combinations, event mutations, ECG/UI, inspection/files, computer, and Game Over were reported natively implemented and positively tested. | Do not disable/port them again. Check package limits; natural combat death and complete visible Game Over were not demonstrated. |
| B00/P03/E01 | Native ATR ground/wall/line/reflection mechanisms and free player-wall connection had RM_0050 evidence. Missing EN01 state must not clamp HP to 1. | Do not claim complete collision; check real `bhCheckL2Wall`, other capsule/push/enemy services, and cylinder-cap semantics. |
| B01 | New report claimed six stair/step/ladder routines and a productive connection. | Ladder path was reportedly unreachable in original data and only component-tested. Check head-work/effect services and latest runtime acceptance. |
| B04 | Follow-up reported corrected order after locomote and shared pitch/roll binding. | Earlier 49/50 with reverse player hair failed; do not treat the old error as current. Independently check rework and agree head-work owner. |
| B05 | Native item-box lid control, sound, menu pass, return; five original-room tests and an RM_1080 diagnostic run. | General object/player dispatcher and reserved/linked owners remain open; no story proof. |
| E03/E04 | Eff009 with UV banks, 40 shared textures, lighter hand node 9/object flame, RGB light types 1–6. | Original arm pose/IK `bhArmIkMdk`, general `pt_flg`/draw gates, other sprite/blood/explosion/particle effects, and SFX/light services partly missing. |
| P04/renderer | Clip tables and evidenced install times checked natively; flag changes alone install no new clip. Game Over 2/20000 retained. | Eff137, full camera state machine, original material identity/order, visible ice-room match open. No ICE/OT/GS core as a substitute. |
| G01 | Report claimed native door packet sequences, camera/leaf/fade/sound/light, and further procedures. | Check current final acceptance and limits. Doors no longer just close immediately, but do not infer complete gameplay. |
| F01–F05 | Existing native decoders/Actor/Face/Lip/Math preserved. | Check face-node angle transfer and eyemat gaze, morph owner, shadow, shared pose/motion services. Old original-core results are not acceptance. |
| Windows | Report said a runtime binary was built and link-input/comparison audit implemented. | Independently check audit and actual Windows execution. Mac success and PE link do not replace this. |

Preserve established invariants: check
`common_effect_textures_offset == size_t(-1)` before access; absent player
state has no HP/death effect; `bhSetLip` must not redirect an active Lip stream;
retain existing BGM disc rule and native Math/WeaponFlash ports.

## Acceptance and reporting duties

For each package, list all original branches, mutations, flags, call order,
and data dependencies with evidence. Distinguish missing / implemented /
connected in production / component tested / runtime tested / Windows tested.
An unconnected implementation remains open.

Implement the agreed package first, then run a shared test block and
root-cause analysis. No full 205-room sweep after each function. Loading,
diagnostic runtime checks, and natural gameplay are different forms of
evidence. Label debug positioning; enemy injection/direct dispatch is not
required spawn acceptance.

After integration, run affected positive component and runtime flows,
regressions, and source/build/actual-binary policy checks. Record build path,
binary, platform, commands, results, and logs in the package report. For
Windows, distinguish compile, link, audit, and execution. Sort red tests by
cause: regression, missing mechanism, or evidenced wrong expectation.

Update README/gameplay docs/ledger only from this evidence. Do not estimate a
percentage from test counts or claim all enemies. `Start.command` may run an
older package; compare the actual launched binary with the tested build
without changing packaging on your own. Commit/push is a separate user
request; `git push` alone does not upload untracked files.

## Related documents

- `AGENTS.md` and [historical enemy assignment](../work/history/2026-10-21-enemy-assignment/THIRD_AGENT_C01_TASK.md) — rules and enemy assignment at the time.
- `docs/NATIVE_REIMPLEMENTATION_ROADMAP.md`, `docs/port_ledger.tsv`,
  `docs/NATIVE_GAMEPLAY.md` — planning/status, partly historical.
- `work/native_port_B05_item_box_object.result.md`.
- `work/native_port_P04_camera_invocations.result.md`, `work/native_port_P04_clip.result.md`.
- `work/native_port_B04_hair_followup.result.md`, `work/native_port_B01_stairs.result.md`.
- `work/native_port_B00_motion.result.md`, `work/native_port_P03_E01_ground.result.md`,
  `work/native_port_E01_line_reflection.result.md`.
- `work/native_port_E03_lighter.result.md`, `work/native_port_G01_doors.result.md`.
- `work/native_port_inventory_tests.result.md`, `work/native_port_D03_files_inspection.result.md`,
  `work/native_port_D04_computer.result.md`, `work/native_port_D05_gameover.result.md`.
- `work/native_port_R02_D09.result.md`, `work/native_port_windows_audit.md`.

Historical enemy/morph reports involving original-core paths are not
acceptance of today's core-free port. Old success claims must not revive
prohibited mechanisms.
