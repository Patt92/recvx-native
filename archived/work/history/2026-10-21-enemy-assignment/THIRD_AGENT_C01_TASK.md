# Historical third-agent assignment — complete enemy package C01–C08

## Scope correction from the user (2026-10-10)

The assignment covers **every original enemy**, not just dogs/zombies or their
trap activation. C01 remains one part of the complete assignment. Preserve
and place the activation analysis already underway, then continue beyond
`activate_enemy_spawn`.

The goal is a standalone native game on macOS arm64 and Windows x64 with
original gameplay: every enemy in the productive World path, including
spawn/event control, initialization, full animation set, AI, attack/grab,
damage/death, child lifecycle, rendering, and SFX wherever the original enemy
has them. No PS2 recreation, original core, or replacement game logic.

First establish the full ID/variant/dispatcher list from permitted original
references and room data. For each ID, record original mechanisms, existing
native owner, missing bindings, asset/motion source, dependencies, and
production evidence. Do not group by guessed enemy names; include bosses,
linked children, special forms, and EN00. Separate subplayers according to
the original table and include their shared-owner contract in C05.

Agree shared field/lifecycle/asset/motion/combat contracts (C02/F/E/G) with the
integrator first. Then port C03/EN01, C04/EN04, C06/EN07, all remaining IDs
(C07+), and C08 independently on the same native state source. A merely
generated or linked enemy list is not an implementation. Report missing shared
services as concrete connection tasks; do not replace them with no-ops.

**User's requested order:** port all enemies and assets together first, then
test them as a group and fix common causes globally. Smaller tasks divide the
work; they do not reduce the overall goal. Mandatory policy, build, and safety
checks still apply. The first required productive enemy acceptance was EN07 in
RM_0130 through original spawn/event flow: correct timing, fully visible
model, advancing original pose, AI, and matching SFX. Then accept the other
enemies together. A 205-room load scan, artificial injection, or direct
dispatch does not replace this acceptance. Demonstrate attacks, hits/HP,
death, and event consequences through real native flows; open dependencies
remain open.

Agree ownership of additional enemy files, tests, and reports with the
integrator before editing. Shared connection files remain with that owner;
the broader scope alone does not transfer them. The overview and partial
reports must distinguish implemented, connected in production, runtime
verified, and still blocked.

## C01 subtask — trap/enemy activation

B04 (hair) belongs to another agent. Codex currently owns camera/clip
connections, monitor call chains, and investigation of ice-room rendering.
Do not edit those areas.

## Goal and ownership

Port the disabled trap activation independently into
`native/enemies/enemy_spawn.cpp/.h`. `activate_enemy_spawn` currently always
returns false. Normal room placement exists but does not replace the
original later activation. Running AI after activation belongs to other
parts of the complete assignment; completing spawn alone does not complete
the enemy.

Owned files: `native/enemies/enemy_spawn.*`, new focused spawn tests,
`tools/verify_enemy_spawn.py`, and `work/native_port_C01_spawn.result.md`.
Do not directly edit `world.*`, `runtime.cpp`, `events.cpp`,
`effect_sprites.*`, `renderer.*`, `player_hair.*`, or CMake. Send necessary
connection/CTest changes as a small patch with an interface contract to
Codex for integration. Preserve others' changes.

## Sequence

1. Read/run `git status --short` and `python3 tools/check_no_original_core.py`.
   Repeat the policy check before every build.
2. Trace permitted semantic references completely: `bhEne_EnemyAtariCheck`
   in `zonzon.c` and `bhEne04_AtariCheck`/`bhEne04_AtariCheck2` in `en04.c`,
   including actual callers, ATR data, flags, and random services.
3. Port all associated activation branches first: ATR type-4 parameter
   sequence, floor/direction choice, zombie type 1, dog type 2, and both EN04
   contact cases. Contact geometry needs an evidenced existing native
   service; report missing services as dependencies.
4. Test together and fix common causes. Do not add a room/enemy special case
   after each individual branch.
5. Derive a suitable room and trigger from original room data. Natural spawn
   and actual runtime input must demonstrate original activation, placement,
   direction, and flag sequence. Artificial enemy injection or direct
   dispatch is not acceptance.

## Required interfaces

The existing WorldEntity/RoomWorld remains the only enemy owner.
`EnemySpawn`/`EnemyArea` are views and input/output contracts, not a second
work world. Existing player/room flags and the shared RNG are authoritative.
A separate random stream in context is no substitute: specify required RNG
calls and their order in the connection contract. Non-hits must not cause
mutations unsupported by the original. Positively check field changes, area
consumption, contact bits, room flags, and RNG advancement, including
boundary and non-hit cases.

## Limits and acceptance

No original core, generated function bodies, second sys/plp/ene work, PS2/
MMI/GS/VU recreation, or invented radius. Never read, search, or use the
`archived/FORBIDDEN_DONTUSE` quarantine. Original sources outside it are
semantic references only.

Report component, real runtime, and platform evidence separately. A Mac
build, policy PASS, and positive tests are required. Report Windows as
compiled/linked only when actually checked, not as executed. Name open
AI/combat/contact dependencies. Do not weaken tests or create a package,
commit, or push without a user request.
