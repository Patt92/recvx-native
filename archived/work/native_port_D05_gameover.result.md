# D05 Game Over — nativ portiert

Stand 2026-10-09. **Implementiert und geprüft.** Semantikreferenz
`src/ps2/veronica/prog/gameover.c` (645 Zeilen) und der Bildschirmfade aus
`screen.c`. Kein Funktionskörper übernommen, kein Generator beteiligt; die
kleinen Konstanttabellen (`gov_yn`, `gov_etb`, `cnt_mes`, die benutzte Spalte
von `PlMtnAct`) sind Daten.

## Dispatcher und Werk

`com`-artig wie das Terminal: `gov_md0` ruft je Durchlauf **genau eine** der
vier Phasen (bhSelectContinue, bhInitGameOver, bhMainGameOver,
bhExitGameOver). Das Werk ist das GV_WORK: 16×16 Punkte (`pt`, `cp`, `op`),
256 UV-Paare, `pz/scl/tfn/tnn`, `cont_x/count/cur_ct/exp_ct`.

**bhInitGameOver** setzt `ts_flg 0x100`, löscht `cb_flg 0x4`, löscht die drei
originalen Zeitgeberflaggen `ev_flg 69/67/70`, nullt `evt_tmd/evt_tim`, löscht
in Kampfmodus `gm_flg 0x80000000`, startet den Fade auf `0xFFE0E0E0` über 60
Durchläufe und baut das Netz: `pt[x,y] = (x-7.5, -(y-7.5), 0)`, `cp` dieselbe
Lage **normalisiert**, `op` null. Die UVs sind acht konzentrische Ringe mit
Radius 112, 98 … 14 um den Mittelpunkt 128/128 der 256×256-Seite.

## Phasen des Hauptlaufs

| Modus | Verhalten | geprüfte Zahl |
|---|---|---|
| 0 | Vorlauf, dann Fade auf 0 über 30, Tonstopp, `sp_flg 0x20` | **71** Durchläufe |
| 1 | Netzwelle aus `cp`/`op`/`gov_yn`, `pz → -14`, `scl += 0.005`, `tfn` fällt | Prompt bei `gov_ct 50`, also **51** Durchläufe |
| 2 | Weiterspielen-Abfrage, Alpharampe 16 je Durchlauf, Auswahl links/rechts | **16** Durchläufe bis 255 |
| 3 | nach dem Fade `ts_flg 0x4000` und bhPopGameData | Retry |
| 10 | Todesszene mit `gov_etb` | **151** Durchläufe, **6** Explosionen |

Der Ausgang nach Modus 1 folgt der Eigenart des Originals: ab `gov_ct > 120`
wird bei ruhendem Fade immer wieder schwarz geblendet, und `gov_md0 = 3` fällt
in den `else`-Zweig — es greift also erst bei `gov_ct > 170`, **während** ein
Fade läuft. `bhExitGameOver` kehrt bei `gm_mode < 3` zum Titel zurück, sonst
setzt es `ts_flg 0x4000` und holt den Spielstand zurück.

Die Abfrage selbst: links/rechts bewegen die Auswahl nur in die jeweils
erlaubte Richtung (SE 2), „Ja" blendet über 10 schwarz, spielt SE 3 und geht
nach Modus 3, „Nein" kehrt nach Modus 1 zurück und spielt die Todesstimme —
die von Spieler 0, wenn `ev_flg 73` gesetzt ist, sonst die des aktiven
Charakters. In Kampfmodus (`gm_mode 3`) gibt es keine Abfrage: bei `gov_ct 50`
läuft nur die Stimme, danach führt der Ausgang direkt in den Retry.

## Todesszene

Erreichbar über `gm_flg 0x400` (die Zeitgebertode aus `bhCheckEvtTimer`). Sie
setzt `sp_flg 0x6C`, `st_flg 0x1|0x4`, `gm_flg 0x200080`, `cb_flg 0x40000000`
und stellt den Spielerkörper auf die Sterbemotion
`PlMtnAct[wpntp][dmlvl][3]`. `gov_etb` zündet bei 40, 54, 68, 82, 96 und 110:
jedes Mal `CallPlayerActionSe(534, 1)` und eine Explosionsanforderung. Beim
fünften wechselt der Körper auf Motion 25 mit der originalen
Interpolationsfenster-Länge 8 (deshalb steht `frm_no` einen Durchlauf still
und der Wechsel auf 26 greift), die Todesstimme läuft und der Schirm blitzt
weiß. Ab `gov_ct > 150` geht es mit `sp_flg 0x20` zurück in Modus 1.

**Cross-Paket-Lücke, nicht ersetzt:** `bhSetExplosionEffect` liegt im
Effektpaket, das weiterhin eine dokumentierte Lücke ist
(`set_explosion_effect` nimmt die Anfrage entgegen und verwirft sie). Die
Anfrage wird gestellt und gezählt (`unsupported_blasts`); es wird nichts
Ersatzweises gezeichnet.

## Netz- und Texturdaten

`bhDrawGameOver` baut je Zeile einen Streifen aus 32 Punkten (zwei Gitterreihen
verschränkt) und zeichnet daraus 30 Dreiecke — 15 Streifen, 450 Dreiecke.
Texturseite ist Eintrag 4 der gemeinsamen SYSTEM/0001-Liste (256×256, passend
zum UV-Mittelpunkt 128 und Radius ≤ 112). Die Liste wird über die Blockkette
des Archiveintrags gefunden (`system_texture_bundle_offset` in `asset_io`,
dieselbe Kette, die `export_assets` schon läuft) — das ist kein geratener
Offset und sagt nichts über die weiterhin offene Kategorienzuordnung des
Effektpakets. Perspektive 11832, Clip 2…20000, Kamera nur in der Todesszene
(0, 10, 35).

## Fade

`bhSetScreenFade`/`bhControlScreenFade` sind für diesen Task portiert:
Zielfarbe, Dauer `1 + ct`, der Vorlaufdurchlauf bei `fade_ct == 0`, lineare
Fortschreibung und der Abschluss genau auf die Zielfarbe. `cb_flg 0x2` ist die
Bedingung, auf die Modus 1 und 3 warten. Der globale Bildschirmfade des Hosts
bleibt davon getrennt.

## Prüfung

**`gameover_original` (CTest, mit Originaldaten).** Fährt den Ablauf fünfmal
durch: Dispatcher und gelöschte Flaggen, die Passzahlen 71/51/16, Auswahl mit
Cursorton, „Ja" bis zum Retry samt `ts_flg 0x4000`, „Nein" mit Todesstimme
(`ADV/0008`) bis zum Titel, Kampfmodus ohne Abfrage (`ADV/0009`), und die
Todesszene mit 151 Durchläufen, sechs Explosionen und wiederhergestelltem
`sp_flg 0x20`. Zum Schluss wird geprüft, dass Netz und Abfrage **tatsächlich
Pixel schreiben** — nicht, dass nichts passiert.

**`tools/verify_gameover.py`** prüft Wirkung im laufenden Spiel, nicht nur
Flaggen:

| Ablauf | Ergebnis |
|---|---|
| Checkpoint (2 Bilder) | Zustand, den bhPushGameData gesichert hat |
| Prompt (170) | Abfrage sichtbar, Alpharampe läuft, `ev_flg 69/67/70` gelöscht, `ts_flg 0x100`, kein anderer Bildschirm darunter |
| Ja (190) | `retry_ct` 1, Dispatcher beendet, **HP, Inventar und Raum exakt wie im Checkpoint** |
| Nein (400) | Todesstimme `ADV/0008`, Titel, kein gezählter Retry |

Die frühere Erwartung „HP 160" prüfte den Startzustand, nicht den Game Over:
ein gewöhnlicher `--in-game`-Raumstart meldet bereits ohne Game Over HP 1. Die
neue Prüfung vergleicht gegen den gemessenen Checkpoint und ist damit
schärfer, nicht weicher. **Der HP-1-Start bleibt ein offener Befund außerhalb
dieses Pakets** (er tritt bei jedem direkten Raumstart auf).

## Stand

Mac arm64: **45/45 CTest**, Prüfer PASS, 24/24 Policy. Windows x64 neu gebaut,
Audit bestanden; ein tatsächlicher Windows-Lauf fehlt weiterhin.

Prüfsuiten: grün sind events, progression, item_catalog, file_pickup,
file_folder, snapshot, frontend, debug, audio_state, scanner, computer und
**gameover**. Rot bleiben `verify_detector`, `verify_pickups`,
`verify_ammunition`, `verify_continuity`, `verify_combat` und `verify_doors` —
offene Projektaufgaben in den Paketen Effekte, Inventaraufnahme, Kampf,
Treppen und Türen.
