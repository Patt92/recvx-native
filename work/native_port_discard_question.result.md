# RM_1020: Freeze nach Wegwerfen der ID-Karte (Fix A)

## Befund

- Interaktion 13 (ATR 0xb800) startet Event 27: `bhEvtComSet 2` (Sicherheitsterminal,
  ID `NTC0394` aus `comevt_message_no2[16]`, sp_evt.c). Danach setzt das Event rm_flg 9.
- scd1 startet daraufhin Event 29. Es gibt die Türen frei, beendet die Kinoeinstellung mit
  `bhCineSet 1` und prüft, ob die ID-Karte (73) vorhanden ist. Dann setzt es selbst
  `bhSet 5,0x1d`, also **st_flg 0x4**, und stellt die Frage (Raummeldung 5).
- scd1 wertet die Antwort aus (`bhCmpW` mes_sel). Bei „Ja“ folgt `bhPlItemLost 73`.
  In beiden Zweigen folgt `bhMessageSet 1` (sp_flg |= 7). st_flg 0x4 löscht das Skript nicht.
- Im Original beendet erst der nächste Spielerdurchlauf das Bit:
  `bhControlPlayer` (player.c:1365, sp_flg 0x1) → mode0 1 `bhCPM0_action` →
  `bhControlPlayerPad` (player.c:1803/1838) `sys->st_flg &= ~0x4`.
- Nativ sperrte `native/runtime.cpp` den gesamten Spielerpass, solange st_flg 0x4 gesetzt
  war. Dieser Host-Gate ersetzt das Original-Pad-Gating. Weil der Pass nie lief, wurde das
  Bit nie gelöscht, und das Spiel hing dauerhaft (bei „Ja“ und bei „Nein“).

## Änderung (A)

`native/runtime.cpp`: Der Spielerpass läuft trotz st_flg 0x4 genau dann, wenn auch das
Original `bhCPM0_action` mit `bhControlPlayerPad` erreicht. Bedingungen: sp_flg 0x1,
plp->stflg 0x1000000 frei, mode0 1, plp->flg 0x10000 frei. Der vorhandene Port
`recvx_player_action_pad` löscht das Bit dann selbst. Es gibt keine Raum- oder
Event-Sonderbehandlung.

## Prüfung

- `tools/verify_discard_question.py` (neu): Terminal, Code, Frage „Ja“/„Nein“, Karte
  entfernt bzw. behalten, st_flg 0x4 frei, Spieler läuft wieder.
  Inventar und Platzierung sind Diagnose-Vorbedingungen.
- A/B: Die neue Build-Runtime `build/discard-claude-20261014` besteht.
  `build/macos-arm64/recvx-runtime` (Stand vorher) scheitert mit `st_flg 0x4016`.
- Nachbarsuiten auf der neuen Build sind grün: event_freeze, events, doors,
  item_box_object, push, stairs, computer, pickups, lighter, combat (runtime-only),
  frontend, gameover, continuity.
- Rot ist identischer Vorbestand, gemessen auf derselben Build ohne die Änderung:
  verify_progression (`fixed_camera == 9`, Zeile 66) und
  verify_enemy_ai (`graveyard-entry`, Lücke bhEff107).
- Prüfläufe: `build/validation/discard-1020-20261014/`.

## Variante B (2026-10-14, umgesetzt)

Der Host-Gate auf st_flg 0x4 ist entfernt. Es gelten die Original-Gates:
`bhControlPlayer` nur mit sp_flg 0x1 und plp->stflg 0x1000000 frei (player.c:1365);
`bhSysCallMain` setzt pad_on = 0 ohne sp_flg 0x20 und maskiert bei cb_flg 0x4 auf
0x1188F (system.c:522-534), auch fuer Zielen/Angriff des Kampfpasses und den Pad der
Effektprozeduren. st_flg 0x4 loescht weiterhin bhControlPlayerPad.
Pruefung `build/validation/padgate-20261014/`: discard_question, events, event_freeze,
doors, continuity, push, stairs, item_box_object, computer, pickups, lighter, gameover,
frontend, combat, palace_stairs gruen; verify_enemy_ai rot wie Vorbestand (dogs-3000).
