# EN53 – Nosferatu-Kern: Verdrahtung (2026-10-14)

Build: `build/en53-claude-20261014` (Release, Ninja).

## Umfang
- `native/enemies/en53.c` in `recvx_game` (CMake, plus `-ffp-contract=off`).
- Dispatch: `jump[53] = recvx_en53` in `native/enemies/enemy_dispatch.c`.
- Bindung in `native/world_enemies.cpp`: `model_root`, `morph_pair`,
  `morph_indices`, `work_texture` auf dem vorhandenen Gegnerbesitzer.

## Offen (benannte Lücke, kein Ersatzwert)
- `child_ambient` (ChangeAmbient, en15.c:3350) ist **nicht** gebunden. Der Besitzer
  hat genau einen beschreibbaren Modellstrom `live_model`, der nur für mdl[0]
  gezeichnet wird (world.cpp:2086). Ein Materialbyte-Patch auf mbp[1]/mbp[2]
  bräuchte je Modell einen eigenen Strom und ist nicht belegt. Folge: der Init
  bricht vor mode0 = 1 ab und meldet „EN53 ChangeAmbient“.
- Damit läuft EN53 in der Welt noch nicht; Räume RM_5800/RM_6040/RM_6100 sind
  nicht gemessen.

## Nachtrag: zweite Lücke im Renderer (2026-10-14)
`ChangeAmbient` ist nativ auch mit eigenem Strom je mbp[k] wirkungslos: der Renderer
wertet das Ambient-Material (Chunks 18/19/22/23) nicht aus (scene.cpp:327 liest nur
Diffuse, world.cpp:2206 multipliziert den Raum-Ambient nicht mit NaCnkAmbientMaterial,
Original ps2_NinjaCnk.c:931-956). NaCnkAmbientMaterial ist im Original globaler Zustand
(Start 1.0, nirgends zurückgesetzt). Offene Frage vor einem Renderer-Port: beginnt jedes
Modell mit einem Ambient-Chunk, oder erbt es den Wert des zuvor gezeichneten? Die Änderung
würde alle Modelle betreffen und braucht eine Messung der Ambient-Bytes aller Modelle.

## Prüfung
- `recvx-en53-tests`: „EN53 component PASS“.
- `ctest -j8`: 92/92 grün (inkl. enemy_en53_component, enemy_en04/07/09/13/21/23/05, enemy_beam_effects).
- `tools/check_no_original_core.py`: PASS (nur Policy-Umfang).
