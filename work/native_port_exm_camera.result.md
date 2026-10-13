# bhCheckExmAtari: Raum-Untersuchungstexte, Fixkamera, Spielstillstand (2026-10-14)

## Original
- `hitchk.c:4786-4843` (Typ 3): solange `st_flg 0x80` frei ist
  - `attr & 0x9`: `fixcno=prm0`, `fixkno=prm2`, `exm_attr=attr`, `pad_ps=0`,
    `gm_flg 0x1000` (+`0x10000` bei `attr 0x8`), `st_flg |= 0x84`, `cb_flg |= 0x8`,
    `cam.flg 0x20 -> ef_flg 0x10`
  - `attr & 0x8000`: `bhSetMessage`, `pad_ps=0` ohne Kamera, `cb_flg 0x20`, `st_flg |= 0x2004`
- `game.c:115-286` (bhMainSequence): `st_flg 0x200000`-Abschluss, `cb_flg 0x8`
  (`sp_flg 0x38|0x80`, `exm_attr 0x2/0x4` -> `pt_flg` ~1/~2 + `lgtp[1].flg` ~2,
  `0x10` -> `sp 0x2`), `st_flg 0x80` (`sp 0x40`, Pad 0x200 -> 0x200000),
  `cb_flg 0x20` (`sp 0x30|...`), `gm_flg 0x1000` -> `bhSetFixedCut`/`bhSetEventFixedCut`,
  `bhCheckCut(gm_flg 0x800)`.
- Schließen: `game.c:160-192` (`sp=~0`, `pt_flg |= 3`, `st_flg &= ~1`, `gm_flg |= 0x800`),
  zeitgesteuert `message.c:312`.

## Nativ
- `EventVm::interact` (Typ-3-Zweig), `EventVm::control_exm` (vor `tick`, beide Runtime-Schleifen),
  Schließpfad in `confirm_dialog`, `message.c:312` in `tick`.
- Welt: `set_fixed_cut` (aus Opcode 0x2a gelöst), `set_event_camera(cno,kno,lock)`
  (0x13 = bhSetEventCamera mit Lock, Hotspot = bhSetEventFixedCut ohne Lock),
  `return_to_cut`, `clear_exm_light`, `mark_exm_cut_effect`.
- Zeichengates `pt_flg 0x1` (Spieler, `game.c:357`, Pose wird weiter gesampelt) und
  `0x2` (Gegner, `game.c:336/353`).

## Daten
901 Typ-3-Hotspots mit `attr & 0x8009`; 132 mit `attr & 9` in 74 Räumen, davon 126
mit `0x8` (Eventkamera), einer ohne Text. Keiner nutzt `0x10`/`0x20`.

## Prüfung
- `recvx-game-tests`: neue Fälle Nachricht ohne Kamera (`sp 0x30`) und 0xb80e-Form
  (Eventkamera, `pt_flg`, Licht, Rückkehr).
- Replay RM_0031 Medaillentür (`build/validation/exm-camera-claude-20261014`):
  Eventkamera 0/0, Spieler ausgeblendet, Gegner/Spieler stehen, nach Seite 3
  Rückkehr zur Schnittkamera, `sp_flg ~0`. `sp_flg 0x10` schaltet das Raumskript
  selbst (Matrix 6 Index 27).
- ctest 82/83; `script_motion` war vorher schon rot (`boxpad-claude-20261014`,
  `alphadomain-claude-20261013`, gleiche Meldung).

## Offen
- `bhCheckCut(1)` im Spieleraugen-Zweig (`gm_flg 0x40`) ohne nativen Besitzer.
- `cam.flg 0x20` aus `cut_fog_flags_` abgeleitet; nur gültig, wenn die letzte
  Schnittübergabe über `cut_fog_handover` lief.
- Native Schnittprüfung (`follow`) beachtet `sp_flg 0x40` nicht generell.
- Nicht ins Paket `build/packages/macos-arm64-v0.5` installiert.
