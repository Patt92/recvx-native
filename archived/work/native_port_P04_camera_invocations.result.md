# P04 — Clip-Aufrufzeitpunkte, 2026-10-10

## Mechanismus und zentrale Korrektur

Der frühere Clip-Anschluss berechnete die Tabelle bei jedem Renderbild neu.
Das Original ruft die Clipdienste dagegen bei Kameraaktionen auf. Ein
Displayflag-/Rcase-Wechsel allein darf einen installierten Bereich nicht
überschreiben. Die bisherige Runtime-Prüfung verlangte genau diese falsche
Neuberechnung in zwei Fällen; sie wurde mit positivem Retentionsnachweis
ersetzt, nicht auf einen neutralen Rückgabewert abgeschwächt.

RoomWorlds bestehender Kamerabesitzer hält jetzt den installierten Bereich
und das letzte GameNear/GameFar-Paar. Das sind Clipwerte, keine zusätzliche
Kamera-/Spieler-/Systemwelt. `view_camera` liest nur noch den installierten
Bereich; der Renderer bleibt passiver Verbraucher.

Originalreferenzen (außerhalb der Quarantäne):

- `event.c bhCamSet`: Mode 0 installiert den Eventbereich. Mode 1 ruft
  `bhCheckCut(1)` und danach `njClipZ` auf: ohne ts_flg 0x200 ist es 1/99;
  mit gm_flg 0x40 ist es 1/20000; sonst GameNear/GameFar.
- `cut.c bhCheckCut`/`bhSetCut`: Raumclip bei neuem Cut oder erzwungener
  Rückkehr, nur mit ts_flg 0x200. Der Runtime-Aufrufer führt die Rückkehr
  vor dem nächsten Skriptbefehl aus; Flags später im Skript wirken nicht
  rückwirkend. Die Rauminitialisierung installiert vor Event_init.
- `cut.c bhSetFixedCut`: eigener Aufruf am Funktionsende (Zeile 537).
  Dieser verwendet über bhChangeViewClipRM **cam.ncut**, nicht den cno-
  Parameter der Fixkamera. Die beiden Funktionen nicht vermischen:
  bhSetCut hat den Aufruf am Anfang, nicht einen zweiten am Ende.
- `gameover.c`: unverändert ausdrücklich 2/20000. Kein automatischer
  Monitorwechsel durch das spätere cam.flg 0x40.

## Abnahme

macOS arm64: Runtime und betroffene Komponenten gebaut, Policy PASS.
`original_view_clip` und `native_ground_world` 2/2 grün. Der Worldtest
verwendet die echten RM_0020-Ressourcen und prüft unterschiedlichen
Raum-/Fixkameraschlüssel, Flagretention und beide Rückgabestandards.

`tools/verify_view_clip.py` bestätigt zwölf Produktionspfad-Fälle mit
Originalressourcen und diagnostischer Eventtabelle: Tabellenbereiche,
Flagretention, Rcaseretention, erneuten Eventaufruf, Fixkamera-Schlüssel,
normalen/non-game/eye-view-Rückweg. Keine blockierten/pending Befehle,
HP unverändert 160. Zusätzlich GameOver-Selbstzerstörung ausdrücklich
2/20000. Logs: `build/validation/view-clip-invocations-20261010`.

Die alte feste 13-Frame-Messkadenz war für verschieden lange Skriptphasen
nicht belastbar. Jeder Prüfpräfix endet jetzt in einer langen Wartephase;
gemessen wird erst, wenn der gewünschte Präfix vollständig ausgeführt ist.
Der frühere Fehlversuch in `view-clip-latch-20261010` ist kein finaler
Nachweis. Kein natürlicher Story-/Kampftodnachweis.

## Monitor und Eisraum: konkrete verbleibende Pakete

Eff137 ist weiter unportiert. `effsub1b.c bhEff137` setzt vor bhSetCut
cam.flg 0x40: dessen Raumclip-Aufruf sieht das Monitorbit und installiert
1/20000, bevor bhSetCut es löscht. Der Effekt setzt das Bit danach erneut.
Das erfordert den wirklichen Effekt-/Kamera-Besitzer und seinen gesamten
Steuerungs-/Zeichenablauf; kein Monitorflag aus SceneCamera-Assetflags
ableiten und keine Spielkameras pauschal auf 1/20000 stellen.

ICE ist keine Clipoperation. `ps2_dummy.c Ps2AddOT` ordnet die Materialien
26510..26614 und 107170 bei aktivem ICE anders ein. `Ps2DrawOTag` schaltet
den Tiefentest ab und zeichnet die Listen; der native Renderer benutzt
stattdessen Tiefentest, opaque zuerst und eine Transluzenzsortierung.
SceneTexture verliert bislang zudem die ursprüngliche globale
Materialidentität. Damit ist visuelle Gleichwertigkeit **nicht** bewiesen.
Erforderlich ist ein separater nativer Material-/Zeichenreihenfolgeport
mit Daten- und Bildnachweis, kein PS2-OT-/GS-Kern und keine geratene
Raum-/Texturpriorität. Der vollständige Kameraautomat bleibt ebenfalls
offen; diese Arbeit schließt die oben belegten Clip-Aufrufzeitpunkte.

B04 nicht übernommen oder zurückgesetzt. Während seines Schnittstellen-
wechsels scheiterte kurz der Runtime-Aufrufer; nach dem parallelen
B04-Anschluss war die Runtime wieder baubar. Kein Haarfix beansprucht.

Parallelauftrag für den dritten Agenten:
[historischer Gegnerauftrag](history/2026-10-21-enemy-assignment/THIRD_AGENT_C01_TASK.md). Keine Nachricht/Session erzeugt.
Kein Paket/Commit/Push.

## Abschlussstand dieses Durchgangs

Vollständiger macOS-Build erfolgreich. CTest 49/50; nur der parallel neu
eingebundene `player_hair`-Test scheitert mit "the chain did not trail
behind the backward motion". B04 bleibt bei seinem Agenten; dieser Test
wurde weder verändert noch als erledigt behauptet. GameOver-Runtime-
Regression (exakter Checkpoint-Retry und Titelrückkehr) grün, Logs unter
`build/validation/clip-invocations-gameover-20261010`.

Windows x64: `recvx_game`, `recvx-view-clip-tests.exe` und
`recvx-ground-world-tests.exe` neu kompiliert/gelinkt, nicht ausgeführt.
Kein Windows-Runtime-/Binaryaudit dieses Durchgangs behauptet.
