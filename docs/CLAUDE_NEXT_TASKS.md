# Historical follow-up for Claude — preserve stable work, port missing gameplay

Status: 2026-10-09. Task IDs and other packages are in
`NATIVE_REIMPLEMENTATION_ROADMAP.md`. This was an implementation assignment,
not an instruction to disable more code or merely make tests green. Current
`AGENTS.md` and user decisions supersede this historical handoff.

## Required goal and starting point

Port the decoded/decompiled game independently and natively to macOS arm64 and
Windows x64, with original gameplay and all enemies complete in the productive
World. No PS2 recreation, separate original core, or invented replacement game
logic. Every agent follows `AGENTS.md`.

Your report again shows Configure/Build/Runtime link and 205/205 rooms loading
without blocked event commands. The source/build/binary checker for
`build/gapcheck-claude/recvx-runtime` also passes on recheck. The game is still
not fully playable. Your interim CTest result was 34/41; report the actual
final result, distinguishing gap-contract tests from positive gameplay effects.

Before work, read `git status --short` and `AGENTS.md`, and run the policy
check. Preserve existing changes; do not reset, commit, or push. Never read,
search, restore, or use `archived/FORBIDDEN_DONTUSE`. Original sources, data,
and disassembly outside quarantine are semantic references only: do not
compile/link them or copy, extract, or generate function bodies under any name.

## 1. Close the small generator remainder — R02

For ADX, CMake still calls `generate_native_tables.py`, produces three
prohibited function files, and deletes them afterward. Replace only this path
with a data-only generator or native data module with identical ADX values.
Temporary function output followed by deletion is not a safeguard. Add this
known mixed-generator case to the checker; test both the prohibited
“generate, then delete” case and permitted pure data.

Agree the narrow write scope with the integrator first: ADX data provision,
affected CMake rule, policy checker, and its tests. Do not start another build
restructure or removal wave. Preserve `item_tables.c/.h`, `room_table.c/.h`,
and `ui_tables_data.inc`.

## 2. Complete inventory on its native owner — D02/D07/D01/D06

The existing `Inventory` remains the sole owner. Before implementation, agree
field/flag/slot contracts for Player, Status, Message, and Event with the
integrator. No second SYS/Player/Inventory work, copy-in/copy-out core, or new
global shadow state.

Handle all original cases in this order, not just a happy path:

1. **D02a/D02b:** GetItem pickup question and modes, capacity/wide items,
   stack maxima, gifts/immediate use; shared sorting/search/deletion and
   correct updates to equipment and selected slots.
2. **D02c/D02d:** inventory-side Reload/CountBullet, packed flags/signed
   quantities; every use case, including healing, poison, spray, floor
   triggers, messages, closing, and pulse requests. Combat state machines
   stay with the combat owner; provide a small Inventory contract for them.
3. **D07:** all six Combi mechanisms, quantity checks, product changes,
   remainder, flags, slots, and messages. An invalid combination may fail
   under original rules; a valid one must have its actual effect.
4. **D01:** item-box selection/transfer/swap/stacking and original
   restrictions on the same inventory. Lid/object flow B05 is a separate
   connection.
5. **D06:** provide native contracts and a connection patch for the seven
   event mutations in the roadmap. Only the integrator edits `events.cpp`;
   do not hide a missing effect with early event completion.

Your module scope is `native/inventory.cpp/.h` and small associated native
modules. No other agent edits this Inventory concurrently. Check existing host
helpers against original semantics; their existence is not 1:1 evidence.

## 3. Connect status and visible controls — D09/D08

- **D09, `native/ui.cpp`:** independently port missing quantity/digit/color
  bank output and Triger/ItemCommand selection, visibility, height, and
  labels. Retain existing pure UI data. Menu input without drawn commands is
  not a complete UI.
- **D08, `native/status_pulse.cpp/.h`:** port the full original ECG state
  machine, including status/healing/poison/fade progression and real
  quad/UV/color output. It processes the same D02d effects; no invented
  curve or permanently neutral condition/heal_mode result.

Face/Lip, Actor/Morph/Shadow, collision/movement, enemies, and doors belong to
other packages. `face.*` is policy-compliant and is not removed here. Its two
known deviations are F04a/F04b with the Actor owner. Preserve existing BGM
rules, Math, WeaponFlash, and the active Lip cursor. Keep the `size_t(-1)`
guard in `world.cpp`; real shared effect textures remain E03. Do not edit shared
World/Runtime files concurrently: send precise missing contracts to the
integrator.

## 4. Verify the combined package and fix common causes

Implement and integrate the whole Inventory/UI/Status package, then test and
fix it together. Policy and build checks remain mandatory before every build;
do not run a full room sweep after every individual function. Classify the
seven remaining test failures by cause. Do not weaken positive effect checks
to “no mutation/no quads.”

Positive acceptance covers original pickup/cancel/capacity limits,
slot/equipment consequences, all Combi classes and remainders,
reload/ammunition flags, healing/poison/triggers, box transfers, event
mutations, visible quantities/commands, and ECG. Then check the same flows in
the productive native runtime. Debug injection or an isolated component
routine is not runtime proof. Trace failures to data/work/call causes and fix
shared causes, not room/item/enemy special cases.

Report source/disassembly evidence, native owners, actual tests and game
effects, and open connections under `work/native_port_<ID>.result.md`. Report
Windows x64 Build/Link and runtime separately; macOS success does not prove
Windows. Treat D03, D04, and D05 as separate follow-ups after this package.
The overall goal, including every enemy and bat acceptance in RM_0130,
remains open until demonstrated in production.
