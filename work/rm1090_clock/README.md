# RM_1090 Standuhr (Private Study) — Sichtfenster, Reset-Freeze, Escape, Last (2026-10-15)

Gegner 0 (id 61, Subplayer-Work bhSubpl, Modell `0x4c5c4`, 26 Knoten): Knoten 6 trägt die
acht Zifferntafeln (Objekte 7–14, je 45°), Knoten 21 die Blende mit dem Sichtloch (Objekt 23,
Ruhe 22,5°) und das Ornament (Objekt 22). scd0 stellt beide Knoten mit Common_controll 9 auf
−20° (`65 [1,0,21]`/`[1,0,6]`, `69 cno=13 [1]`, `69 cno=9 [20]`), damit das Loch auf 12 Uhr steht.

## Ursachen und Fixes

1. **Loch auf „11“ statt „12“** — `native/world.cpp` Common_controll 9 schrieb für einen frei
   laufenden nativen Work in `node_overrides`, die der native Zeichenpfad (`sample_work` mit
   `enemy_obj`) nie liest. Befehle 8/18 schrieben bereits auf `enemy_obj`. Fix: 9 ebenso
   (event.c:12136 schreibt `objP[model_cno].ang`). Vorher Blende 22,5° daneben, jetzt 2,5°
   (Loch zeigt das Emblem „AR“ auf 12 Uhr, Screenshot `shot_init` vs. `prefix_init`).
2. **Freeze bei Reset nach L/R** — Reset mit gesetztem 4:7 startet Event 8/9 (bhObjDposCk 0xb1).
   Die installierte Start.command-App vom 10.10. (`build/start/macos-arm64/.previous-r6f2pybk`)
   kennt 0xb1 nicht: „unimplemented opcode 0xb1“, Tasks blockiert, Cinematic bleibt. Reset ohne
   vorheriges L/R nimmt den Zweig ohne Event 8/9 (scd1 @04b6) — daher „direkt Reset geht“.
   Der 0xb1-Port in HEAD (7d1f366) behebt es; seit `./build.sh` (heute) ist die Dev-App aktuell.
3. **Escape dreht „Links“** — `EventVm::cancel_dialog` bestätigte eine Skriptauswahl mit
   Auswahl 0. message.c:231-285 kennt bei Auswahlen nur OK/Links/Rechts; Cancel (pad 0x1000)
   liest allein das Raumskript per bhPadCheck (Event 13, Bit 12 → Uhr verlassen). Fix: Cancel
   bestätigt keine Skriptauswahl mehr (Item-Frage unverändert Auswahl 1).

## Was die Raumdaten zeigen (kein Hostfehler)

Zifferntafeln (Texturen des Modells): 1, 3, 4, 5, 6, 7, 8 und Emblem — keine 2, keine 9.
L = +180° (Event 6: 45 × 4°), R = −45° (Event 7: 30 × 1,5°). Von der Startlage (Emblem) zeigt
die Folge L, R, LR, RRR im Fenster **3, 1, 7, 1** (Screenshots `shot_L/LR/LRLR/full`); Position 2
und 4 sind dieselbe Tafel. „1974“ ist mit diesen Skripten nicht darstellbar; der Erfolg des
Rätsels wird in RM_1090 nicht über Knotenwinkel geprüft (ev_flg 61 setzen andere Räume).

## Last in RM_1090

Profil (`sample`, Dummy-SDL): kein raumspezifischer Hotspot; Blitz (Events 21–23, Effekt 0) und
Regen (122) liefen in den Messungen ohne Mehrkosten. Teuer ist allein die Kamera des
Schreibtischs (Spieler bei (33,0,13)): Blick durch das Glasdach, eine bildfüllende transparente
Fläche → `bilinear`/`blend_gs` je Pixel, ~2× CPU je Bild gegenüber den anderen Kameras
(29,4 s user / 156 Bilder vs. 10–14 s / 110 Bilder). Nachbarräume RM_1080/RM_1100 gleich teuer
wie die übrigen RM_1090-Kameras. Keine Änderung; Optimierung des Alpha-Pfads im Software-
Rasterizer wäre Hostarbeit.

## Nachweise

- `tools/verify_clock_puzzle.py --runtime <bin> --data data --output <dir>`: Reset direkt,
  L+Reset und Escape geben den Spieler frei (gegen `build/macos-arm64/recvx-runtime` und die
  Dev-App `build/start/macos-arm64/RECVX Native.app/Contents/MacOS/recvx-runtime` grün).
- `work/rm1090_clock/clockdiag.cpp` (+`build.sh`, Binary unter `$TMPDIR`): Harness gegen die
  Originalskripte (L/R/Reset-Folgen, Knotenwinkel), Geometrie-/Texturanalyse (`geom`, `hole`,
  `digits`, `dumptex`).
- CTest vorher/nachher: `effect_room_sprites` (bhEff219, seit 7d1f366) und
  `native-gameplay-bindings` (erwartet Lücke bhEff256, Effekt 256 heute fremd portiert) sind
  mit und ohne diese Fixes rot (A/B mit zurückgenommenen Hunks gemessen).
