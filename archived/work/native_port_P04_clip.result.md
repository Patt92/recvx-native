# P04: Tabellenprüfung und fehlende Clip-Zustandszweige

## Auftrag und Besitz

Bestehenden Agentenport auf Richtigkeit prüfen, danach fehlende Clipzweige
eigenständig nativ vervollständigen. Keine Quarantäne gelesen, kein Original-
Funktionskörper übernommen, keine zweite Kamera-/System-Work. Haar wurde als
konfliktarmer Folgeauftrag vorgeschlagen; keine andere Session angeschrieben.

## Semantikbelege

- `event.c:13205..13416`: Eventschlüssel stage/room/rcase/evc; erster Treffer.
  Standard bei `ts_flg & 0x200`: -2/-20000, sonst -1/-99; ein Tabellen-
  treffer überschreibt beide. Native Entfernungen haben umgekehrtes Vorzeichen.
- `event.c:13418..13557`: Raumabfrage ohne Rcase. Standard -2/-20000;
  `cam.flg & 0x40` überspringt die Tabelle vollständig und setzt near -1.
- Alle 113/51 Datenzeilen unabhängig gegen die zulässige Originalquelle
  verglichen: Reihenfolge, Schlüssel und IEEE-Float32-Werte identisch.
  Zusätzlich 492 Abfragen am separat kompilierten ursprünglichen Tabellenport
  gegen Originaldaten geprüft (Displaybit gesetzt, Monitor aus).
- `gameover.c:244/309`: zuerst explizites njClipZ(-2,-20000), später cam.flg
  0x40. Das Flag allein ist kein Aufruf von bhChangeViewClipRM; GameOver
  behält seine 2/20000. Ein zuerst versuchter Monitoranschluss wurde nach
  dem vollständigen Aufrufkettenabgleich zurückgenommen, bevor Abschluss.
- `ps2_dummy.c:Ps2AddOT`: ICE verändert OT-Buckets für ausgewählte Texturen,
  keine Clipentfernungen. Kein Hardwareflag, PS2-OT oder Renderer-Kern
  eingebaut. Sichtbare Gleichwertigkeit der Eisraum-Sortierung bleibt offen.

## Änderungen

`event_view_clip` erhält Displayflags; Runtime liest vorhandene
`EventVm::flags[12][0]`, keine neue Work. `room_view_clip` kann Monitoransichten
ausdrücklich auswählen, `monitor_view_clip` stellt deren native Entfernungen
bereit. GameOver bleibt in seiner bestehenden Clipsteuerung unverändert. Eff137 ist als
weiterer Monitor-Erzeuger noch nicht portiert, nicht künstlich aktiviert.

Renderer hält nur einen passiven Auszug der tatsächlich verwendeten Near/Far-
Grenzen; Runtime meldet ihn als `render_clip`. Kein Zweitbesitzer der Kamera.
Bestehende Third-Person-Mod-Sonderbehandlung unverändert, kein 1:1-Nachweis
für Modansichten. Tabellen des anderen Agenten unverändert erhalten.

## Gezielte Abnahme

macOS arm64, `build/native-ground-20261009`:

- `original_view_clip` und `gameover_original`: 2/2 bestanden.
- Komponenten-Rasterfälle: Objekt bei z=1.5 nur im Monitorbereich sichtbar;
  z=150 bei Event-Fehltreffer nur mit Displaybit; originale Far-170-Tabelle
  entfernt z=200, zeichnet z=150. Kein Test auf neutrale Rückgabe abgeschwächt.
- `tools/verify_view_clip.py`: sieben Übergänge durch produktive Event-VM
  und World-Kamera an RM_0020; Originalgeometrie und Originalkameradaten,
  ausschließlich Eventtabelle diagnostisch isoliert. Tatsächliche Renderer-
  grenzen geprüft: 24/5000, 25/20000, 1/99, 2/20000, Tabellenoverride trotz
  gelöschtem Displaybit, Rcase-Fehltreffer und Raumcut 4/20000. Keine
  blockierten oder pending Befehle, HP bleibt 160.
- Selbstzerstörungs-/GameOver-Eventpfad: aktive Phase 10, Renderbereich
  2/20000. Keine Behauptung natürlichen Kampftods oder vollständiger Effekte.
- Finale Logs, Zustände und BMPs: `build/validation/view-clip-20261010-final`.
  Der erste Lauf in `view-clip-20261010` hatte noch den unbelegten
  GameOver-Monitoranschluss und gilt nicht als finaler Semantiknachweis.
  Zwei Raumansichten visuell geprüft. Die Todesszenenaufnahme ist schwarz;
  der gemessene Clipbereich beweist nicht deren vollständige sichtbare Wirkung.

## Grenzen

Kein Storydurchlauf, kein vollständiger Kameraautomat-/ICE-Darstellungsnachweis,
kein Eff137-Port, kein Paket, Commit oder Push. G01 und Haar nicht bearbeitet.
## Zielbuild-Endstand

Runtime und betroffene Clip-/GameOver-Tests auf macOS arm64 erfolgreich gebaut.
Alle fünf gezielten CTest-Gruppen am finalen Stand grün: Clip, GameOver,
Renderer, Texturfilterung und Snapshot. Die zusätzlichen drei Testprogramme
wurden nach der Renderer-Headeränderung frisch gebaut. GameOver-Runtime-Regression
(Prompt, exakter Checkpoint-Retry, Todesstimme/Titel) grün. Quellen-, Build-
und Runtime-Binary-Prüfer PASS, `git diff --check` ohne Befund.

Windows x64: `recvx_game`, `recvx_scene`, `recvx-view-clip-tests.exe` und
`recvx-gameover-tests.exe` erfolgreich kompiliert/gelinkt. Kein eigener
Windows-Runtime-Build, Windows-Binaryaudit oder Windows-Lauf behauptet.
Die komplette Projekttestsuite wurde nicht erneut als Abnahme verwendet.
