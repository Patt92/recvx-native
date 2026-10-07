## Originaler 3D-Dateiordner (Codex, 2026-10-07)

Die Host-Dateiliste wurde im gemeinsamen Statusmenü ersetzt. SearchTag,
SelectFile, SelectTag und FileSyu werden direkt aus fileview.c/itemview.c
extrahiert; eigene FV-/S_WORK-Bindung hält die originale Ordnerrotation,
Öffnen/Schließen, Eingabesperre und SE 0/3/6. Echtes ITEM1-Modell 139
(0138.bin), drei Ordner, Originalknotenpositionen und DrawSubItems Sonderpfad
(gedrehte Positionen, separate ang00, .58-Skalierung, Kamera-Z 8).
MakeTags originale Registergeometrie, Besitzbits und Graustufen 128/192;
Originalpfeile einschließlich Rotation und Helligkeit. FILE schließt die
Fensterblenden vor dem Ordnerwechsel; StatusToFileView nutzt SpriteH's
separaten Ausfahrzweig 0x1000000 und hält den Status-Task aktiv.
Der Bildschirm-Seitenwechsel wird im Software-Renderer aus derselben
Wallpapergrafik und Textlage zusammengesetzt, ohne PS2-Framebuffer-Upload.

Rechts/Links wählen die drei Ordner (originale Richtung), Bestätigen öffnet
zuerst den Ordner; Oben/Unten wählen nur vorhandene Register. Register-Cancel
schließt den Ordner, Ordner-Cancel führt zur FILE-Leiste. Nach dem Lesen
kehrt FileExit01 ebenfalls zur FILE-Leiste zurück, nicht in eine Host-Liste.
Direktes File/Examine des Handbuch-Items 145 nutzt die originale automatische
Ordneröffnung (0x40000000). Das Handbuch wird beim Verlassen wie original aus
itm[0..191] entfernt und ic_flg[15] 0x80000000 gesetzt. Originales
PlayPageCheck bestimmt 7–14 Seiten über ev_flg; MakeTag schaltet die
Handbuchdatei und optional Datei 11 über ssd_flg 0x1000 frei.

Gezielt: drei CTest-Gruppen, echte Ordner-/Dateiressourcen 0/2/17,
Registerbesitz/-farben, leere Ordner, Dreh-/Öffnungs-/Schließpfade und sechs
Handbuch-Fortschrittszustände bestanden. tools/verify_file_folder.py prüft
in RM_0060 beide Einstiege mit normalen Menütasten, Seitenwechsel, Rückkehr,
Handbuchentfernung/Leseflag und Status-Ende; keine Positionierung oder
Fortschrittsflags im Runtime-Replay. Ressourcenfixture schaltet für die
Ansichten alle Dateibesitzbits ein. Ansichten einschließlich echten
Runtime-Handbuchs visuell geprüft. Mac arm64 gebaut/signiert; Windows x64 nur
crossgebaut. Beide Starter aktualisiert, Paketdaten/-Runtime ebenfalls geprüft,
Runtime/SDL nur arm64 und sieben Benutzerdateien per SHA-256 unverändert.
Logs: build/validation/file-folder-20261007; Backup:
archived/releases/file-folder-before-20261007-154445.
Keine bitgenaue PS2-GS-/Licht-/vollständige Statusmenü-Zusage und kein
physischer Controller-/Durchspielnachweis. Kein Commit/Push.

## Inventar: Datei-Leseansicht und Ausrüstung (Codex, 2026-10-07)

Gemeinsame GameUi-Dateiansicht ersetzt den kleinen Host-Dialog: originale
parts_22-Wallpapergeometrie/UVs, bhDispMessage-Position (68/64), Schriftgröße,
Pfeile und EXIT aus ITEM1. Kein zusätzlicher Seitenzähler. Seitenwechsel nutzt
PageScrolls beschleunigte Bewegung, acht Warteframes und zwei Eingabewarteframes.
FileScrollSet-Steuerung: Links/Rechts blättern; Rechts auf der letzten Seite
wählt EXIT, Links wählt es zuerst ab, Bestätigen beendet nur mit gewähltem EXIT.
Cancel führt zurück zur Dateiauswahl, bei Aufnahme ins Spiel und beim
Item-Report zum Itemcursor. Leere Dateiliste öffnet keine unbesessene Datei.
MainCommand-Cancel schließt original direkt; CursorMove-Cancel wählt ITEM (3),
nicht EXIT. Keine Änderung auf ein vom Original abweichendes EXIT-first-Verhalten.
ArmsSet positioniert schmale Ausrüstung bei Center 3 +58, breite bei +24;
der originale breite Rahmen bleibt erhalten (Center 3 verschiebt sich beim Öffnen).

Gezielt: drei CTest-Gruppen bestanden; recvx-file-reader-tests mit echten
Dateien 0/2/17, Seiten-/EXIT-/Cancelpfaden und Pixelgrenzen schmaler/breiter
Ausrüstung. Ansichten der Pistole und Butler-Briefseite visuell geprüft.
Paketdaten bestanden; Mac arm64 gebaut/signiert, Windows x64 nur crossgebaut.
Beide Starter aktualisiert; Runtime/SDL nur arm64, sieben Benutzerdateien per
SHA-256 unverändert. Logs: build/validation/inventory-read-20261007; Backup:
archived/releases/inventory-read-before-20261007-152353.
Dateiauswahl bleibt Host-Liste: originaler animierter 3D-Ordner, Übergangs-
Capture und spezielle Fortschrittskürzung des Spielhandbuchs sind damit nicht
vollständig portiert. Keine vollständige 1:1-Inventarmenü-Zusage, kein
Durchspiel-/Controllernachweis, kein Commit/Push.

## Inventory EKG layers — 2026-10-07

Preserved original pulse vertex depth and sorted captured EKG quads with status
sprites, using ps2_NaDraw.c reciprocal z and SpriteDisp priorities. Trace 10 /
cure band 5 sit behind closing shutters 1. The old final overlay ignored depth.
Targeted status_condition_pulse passed; actual SYSTEM-resource pixel comparison
covers normal/heal/poison-cure across all 15 closing frames, visible when open,
fully covered after six shutter ticks. Package-data version passed too.
Mac arm64 / Windows x64 built (Windows not run); both starter binaries updated,
Mac app signed, INI/snapshot hashes preserved. Logs:
`build/validation/pulse-layer-20261007`; backup `archived/releases/pulse-layer-before-20261007-150605`.
No broad gameplay retest or full status-menu fidelity claim; no commit/push.

## BGM room exits — 2026-10-07

Fixed the shared door/event handover: original interaction index/flags and final
scenario pass precede room fade-out; DoorCall continues with return value 1.
Targeted Mac arm64 CTest: audio_movie, door_transition, bgm_room_transition and
native-gameplay-bindings passed. Actual room data: RM_0050 dog events 15/16
explicitly started, BGM 49, all three exits stop at 60 ticks; RM_0060 exit 9,
retained music without requests, track replacement/identical requests and
independent SFX passed. Runtime with diagnostic positions and original scripts:
RM_0060 → RM_0050 → RM_0080, normal and first door skipped, BGM stays stopped;
RM_0050 scripted exit → RM_0030 also passed without blocked commands.
No natural dog-fight or full playthrough claim. Windows x64 crossbuild only.
Logs: `build/validation/bgm-room-20261007`; no commit/push or broad room scan.
Both v0.5 starter binaries updated; signed Mac app and runtime/SDL arm64 only.
Packaged original data passed the BGM tests and normal/skipped runtime paths.
Seven INI/snapshot files unchanged by SHA-256; backup
`archived/releases/bgm-room-before-20261007-145630`.

## RM_0050: geskriptete Animationspose (Codex, 2026-10-07)

Gemeinsame Actor-Skriptbindung trennt die von bhSetMotion gesampelte Pose vom
anschließend weitergestellten frm_no. Rendering, Modellknoten und Linkmatrizen
verwenden die Pose vor dem Zählerfortschritt. Der echte RM_0050-Event 12 zeigte
zuvor nach Frame 84 noch einmal Frame 0, bevor der nächste Schedulerpass den
Zombie ausblendete; dieser End-Flackerfall ist gezielt reproduziert und behoben.
Common 24/25/34 berücksichtigt jetzt auch mtn_md-Bit 8 beim Fractional-Sampling,
nicht nur das MTN-Dateiflag. Common 34 behält originale Startframes 60/64.
Der gemeldete vollständige Rücksprung während des zweiten Ziehens wurde im
aktuellen Quellstand nicht reproduziert; keine vollständige Behebung dieses
zusätzlichen Symptoms behaupten. Das Originalskript selbst bleibt unverändert.
Neue Regression: tests/native/script_motion_test.cpp, echter Raumtrigger und
Event 12, letztes sichtbares Bild, Startframes, Halbframes, Pause und bewusster
Loop; zusätzliche Original-RM_0070-Motion. Mac arm64 bestanden, Actor-Test
bestanden. Windows x64 crossgebaut, kein Windows-Lauf. Beide Runtime-Binaries
im v0.5-Paket ersetzt; Mac-App signiert, arm64 geprüft. Projektstarter mit
Paketdaten: 160 Ticks/--validate-poses, keine Blockade, Zombie anschließend
versteckt. Sieben vorhandene Paket-INI-/Snapshot-Dateien per SHA-256 unverändert.
Logs: build/validation/script-motion-20261007. Backup: archived/releases/script-motion-before-20261007-104936.
Keine breite Neuprüfung, kein Commit/Push. Parallele fremde Portarbeit erhalten.


## Schießen / Zielwahl — 2026-10-06

Steves M93R (Weapon 4) kann im gemeinsamen Kampfpfad zielen und schießen.
Originale Einzel-/Dual-Suchroutinen und Armrotationen beim Build extrahiert;
L1/LB oder Q wechselt das Ziel. Custom Handgun hat den originaltabellengesteuerten
Dreischussmodus, Doppel-SMGs getrennte Armziele und verzögerten linken Schuss.
Typ-0-Mündungsfeuer mit originaler SYSTEM-Textur, UVs/Geometrie/Lebensdauer und
Ninja-Blend 8/10. Schuss-/Sichtabfragen berücksichtigen die ursprünglichen
0x400-/0x40000000-Filter für Wandtypen/Sichtattribute.

Mac arm64 und ASan/UBSan: 25 CTest-Gruppen sowie echte Combat- und SDL-Input-
Datensuiten bestanden. Combat-Datensuite prüft nächste/zyklische/verdeckte Ziele,
Wandfilter, zwei getrennte Treffer bei gemeinsamem Ammo-Abzug, Burst/Trigger-
freigabe, Leer-/Nachlade-/Infinite-/Modalzustand, Originalmodelle und Posen für
Spieler 0–3 sowie Mündungsgeometrie/Lebensdauer. SDL-Controller ist virtuell;
kein physischer Controller-Test. Windows x64 erfolgreich crossgebaut, keine
Windows-Ausführung oder Hardwareprüfung. Kein neuer vollständiger Raumscan.

Runtime: tools/verify_combat.py besteht auf Mac arm64 und ASan/UBSan.
Auch mit eigenen Mac-Paketdaten bestanden; Projektstarter SDL-Dummy-GUI:
60 Presents/81 Ticks, keine blockierten Befehle. Der natürliche Steve-
Replay läuft ohne Teleport, Flags, Debugitems oder Skip: Geschenk 12 Patronen,
Zielen, ein Schuss, 11 Patronen und Idle/Steuerungsfreigabe. Die bestehenden
Event-Replays einschließlich beider Steve-Spawns bestanden ebenfalls. Zwei
Dual-Ziele und Wechsel auf konzentriertes Ziel zusätzlich im echten RM_0020
mit diagnostischer Aufstellung geprüft: rechts/links 2/1 → 2/2.
Ansichten `m93r-shot.png` und `dual-aim.png` visuell geprüft.

Begrenzungen: Trefferadapter weiterhin en01-Zombies und Host-Wandvolumen;
keine vollständigen originalen Gelenkkapseln, Gegner-KI, Treffer-/Todesreaktionen,
alle Mündungsvarianten (u. a. 229), Rauch/Hülsen/Blut/Licht/Vibration,
Scope oder Projektil-/Spezialwaffen. Vollständige Originalinterpolation/Recovery
bleibt offen. Die Tests belegen keinen vollständigen Kampf-/Durchspiel-Port.

Logs: `build/validation/combat-20261006/`. Beide Starter-Runtimes aktualisiert,
Mac arm64-only samt SDL und neu geprüfter App-Signatur. Benutzer-INIs und
Snapshots per SHA-256 unverändert. Backup:
`archived/releases/combat-before-20261006-215740`. Kein Commit/Push.

## Tür-Schließphase und Original-ItemSort (Codex, 2026-10-06)

`bhControlDoorDemo` hält Statusbits 20–23 bis `FadeProc1` Bit 0x20 setzt.
Der Host konsumierte diese zweite Gruppe bereits beim Öffnen. Jetzt bleibt
sie bis zur Abblendphase erhalten: Schließ-SE 1 liegt bei Türtyp 0/5 auf Tick
163 vor Ende 164, bei Typ 4 auf 147 vor Ende 148. Keine Ersatz-SFX und keine
Raumregel. Voice-/Movie-Stop löscht nicht mehr die gesamte bereits gemischte
SDL-Queue; Tür, BGM und übrige Stimmen werden nicht zusammen mit einer Stimme
abgeschnitten. Vollständiger Audioreset beim Debug-Raumwechsel bleibt getrennt.

`ItemSort` aus sub1.c wird zusammen mit `GetItem` generiert. Der ILP32-Zugriff
über NJS_TEXNAME.texaddr wird zu pip[num1+2] übersetzt. Die originale Sortierphase
zwischen GetItem und ItemSet (StatusMain → SpriteOnOff → ItemSort) ist nach
jeder Aufnahme gebunden; GetItems vorweggenommene Equip-Korrektur wird nicht
doppelt angewandt.
Auch andere Inventarmutationen verwenden die Originalsortierung. Alle sechs
breiten Itemtypen, 0–6 vorherige schmale Items und weitere breite Aufnahmen
geprüft; Sonder-flgchk des gesamten StatusMain bleibt nicht vollständig gebunden.

Gemeinsame `bhSetMessage`-Startflags löschen jetzt alte Antwort-/End-/Seitenbits
bei Raum-, Skript- und Itemdialogen. RM_0060: Ja/Nein, Wiederholung nach Nein,
Erwerbsflag it_flg 133, Steuerungsfreigabe, natürliche Raumrückkehr und Snapshot-
Laden geprüft. Die Gefängniskarte belegt original keinen Inventarslot. Der
ursprünglich gemeldete Hänger ist im geprüften aktuellen Ablauf nicht
reproduziert; kein vollständiger Map-Renderer durch diesen Erwerbscheck.
RM_0070: Calico-Aufnahme nach drei schmalen Items, Original-ACK it_flg 136,
Sortierung, Equip und Screenshot geprüft. Diagnosefixture setzt ausschließlich
für den Test evflag 116 (Steves Handel abgeschlossen); keine Freigabe im Runtime.

Mac arm64: 25 Testgruppen, zehn Originaldaten-Suiten (inklusive 105 Türfälle
und Itemmenü), zwölf Runtime-Suiten bestanden. Feste Binary-Kopien verhindern
Änderungen während der Prüfung. Parallel ergänzte Targeting-/Waffen-/Muzzle-
Arbeit ist im geprüften Paketbuild enthalten; spätere Testergänzungen bleiben
im Arbeitsbaum. Windows x64 cross-gebaut, keine Windows-Hardware/Wine- oder
physische Controller-Prüfung. Kein vollständiger Durchspielnachweis.
Logs/Ansichten: build/validation/inventory-door-20261006; Repro:
`tools/verify_pickups.py`, originale Tür-PCM in `recvx-door-tests data`.

Beide Starterpakete aktualisiert; Mac-Binary und SDL ausschließlich arm64,
App signiert. Paketdaten-Aufnahmen und normaler Projektstarter über SDL-Dummy
geprüft; Benutzer-INIs/Snapshots erhalten. Backup:
archived/releases/inventory-door-before-20261006-215426.
Die zusätzlichen ASan/UBSan-Läufe wurden auf Nutzerhinweis abgebrochen; kein
vollständiger Sanitizer-Nachweis dieses Paketstands behauptet. Bereits erfolgte
Einzelprüfungen bleiben in den Logs. Künftig nur geänderte Mechanismen und ihre
konkreten Abhängigkeiten prüfen; bereits funktionierende Rodrigo-/Prolog-
Szenen nicht ohne neue Fehlerhinweise wiederholen.

## Event-Port 2026-10-06

Zehn bisher fehlende Eventbefehle ergänzt, bhPlItemLost durch extrahiertes
Original ersetzt; gemeinsame Flagmasken und WHILE/NEXT-/FOR-Zähler korrigiert.
25 Mac-arm64-Testgruppen und Game-Datensuite, Event-/Kontinuitäts-/Detector-
Replays bestanden. Vollständiger neuer Scan: 205/205 Berichte, 28 blockierte
Räume (600 Ticks, Spawn 0, keine Eingaben/Mods). Parserpräsenz und Pending-
Wirkung getrennt; keine vollständige Event-/Progressions- oder Durchspielzusage.
Details und offene Prioritäten: [EVENT-COVERAGE.md](EVENT-COVERAGE.md).
Logs: `build/validation/events-20261006/`.

Zusätzliche Zielprüfung: ASan/UBSan ebenfalls 25 Testgruppen und Game-Datensuite
bestanden; Windows x64 erfolgreich crossgebaut, kein Windows-Laufzeittest.
Die zwischenzeitlichen parallelen Combat-Buildfehler waren im danach geprüften
Gesamtstand behoben. Beide Starter-Binaries aktualisiert; Mac-App signiert.


# Metalldetektor, SQ-Transport und Security-Cursor — 2026-10-06

Gemeinsame Engine-Bindungen: vollständiger unkomprimierter Sony-SQ-Zeitstrom
mit Noten/Schleifen/Stop/Fade, originale Event-Slotkonfiguration; extrahierte
bhEff122/230, Texturgruppen 41/414 und Common-Typzustand; Common-9-Knotenwinkel;
Interaktionsantwort über cb_flg 0x1000 und bhFixEventCamPly 0x57. Disabled
CursorInit-Sprites werden in der Box nicht mehr gezeichnet. Keine geänderten
Raumskripte, künstlichen Alarmloops oder Ersatzgrafiken.

`tools/verify_detector.py` startet RM_0090 mit echten Ressourcen und prüft:
Alarm nach Metallkontakt, wechselnde rote UV-Animation, Ja/Nein am Schalter,
beide Rolltorbewegungen mit originalen Bewegungs-/Rastsequenzen, Audio-Stopp,
Kamera-/Steuerungsfreigabe, erneuten Alarm und metallfreien Durchgang nach
Einlagern. Der Lauf prüft jede Zwischenpose mit `--validate-poses`. Ergänzte
Originaldaten-Tests mischen fünf Alarmdurchläufe, vergleichen Tür-PCM inklusive
300-Sample-Anlauf, überprüfen Lampen und beide Rolltore sowie den fehlenden
orangen Cursorstreifen. Generische VM-Tests prüfen den einmaligen Antwort-ACK.
Diagnostische Positionsbefehle ersetzen keinen tatsächlichen Durchspieltest.

Logs und Ansichten: `build/validation/detector-20261006/`, darunter
`final-detector/{alarm,box}.png`. Boxansicht ohne orange Welle und rotes
Lampensprite visuell geprüft. Linke Boxliste und vollständige Originalfenster-
animation bleiben Host-Oberfläche bzw. unvollständig. SQ-Transport ist kein
bitgenauer Sony-Synthesizer: komprimierte SQ/Pitch/ADSR/Reverb/mehrschichtige
Programme/live CC-Modulation bleiben unvollständig. Innenraum-Regen und der
nicht reproduzierte RM_0070-Crash werden hier nicht als behoben ausgewiesen.

Mac arm64: 25 CTest-Gruppen, acht Originaldaten-Suiten und elf Runtime-Suiten
bestanden. ASan/UBSan: 25 Gruppen, acht Datensuiten und Event-/Kontinuitäts-/
Audio-Zustands-/Detector-Replays bestanden. Windows x64 cross-gebaut; kein
Windows-Laufzeit-/Hardware- oder physischer Controller-Test. Beide Starterpakete
aktualisiert; Mac-Runtime/SDL ausschließlich arm64, Signatur geprüft,
Benutzer-INIs und Snapshots unverändert. Kein Commit/Push oder neuer Raumscan.

Zusätzlich bestanden: vollständiger Detektor-Replay mit den eigenen Mac-Paketdaten
und Projektstarter im SDL-Dummy-GUI-Modus (90 Präsentationen, acht Renderbilder,
sieben Simulationstakte, Hauptmenü aktiv). Kein WindowServer-/physischer
Controller-Test. Paket-INIs und Snapshots nach diesen Starts erneut hashgeprüft.
Backup: `archived/releases/detector-before-20261006-211708`.

# Skriptgeschenke, Fenster-Repro und Tür-/Raumaudio — 2026-10-06

Dieser Abschnitt ergänzt den gemeinsamen Arbeitsbaum und hat für die unten
beschriebenen Änderungen Vorrang vor älteren Paketangaben. `GetItem` wird beim
Build vollständig aus `sub1.c` extrahiert. Raum-Items und Skriptgeschenke nutzen
seine Inventarmutation und Zustände: Fragen 157/153, Vollmeldung 154 und
Empfangsbestätigung 158, Original-Mengen/Stacks/zweibreite Slots/Personal-Items.
Das erste Steve-Ereignis vergibt M93R (Item 9), zwölf Patronen und Weapon 4,
wartet modal auf Bestätigung und vergibt das Geschenk auch nach Skip genau einmal.
Kein zusätzlicher M93R-Kampfport: Kampf unterstützt weiterhin Messer/Handgun.
Die Host-Oberfläche entspricht noch nicht der vollständigen Originalfensteranimation.
Claudes cam.ncut/Floor- und cb_flg-0x4000-Korrekturen bleiben erhalten.

Tür-SPQ ersetzt die MIDI-Programmbank 1; die zugehörigen vier SQ-Listen liegen
im dritten Block von COMMON.MLT. Tür-Audio löst jetzt Programm, Key 60 und
Velocity über diese originalen Sequenzen auf. Getrennte Stimmen 27–29 überleben
den Raumload und werden bei Zielraumfreigabe mit dem Originaltimer 250 ausgeblendet.
SoundTiming-Anforderungen werden einmalig weitergereicht; die originalen
Handler 5–9 sind leer und erzeugen selbst keine zusätzlichen Schließ-Samples.
PCM-Vergleich für Banken 2/7 einschließlich Raumload und Schließ-Fade bestanden.
105 Türvarianten beendet, dargestellt und übersprungen; keine Behauptung einer
vollständigen Sony-Sequencer-/Pitch-/ADSR-/Reverb-Emulation.

Regen: Die zusätzliche Host-Regel „kein BG im Zielpaket → Ambient stoppen“
ist auf Nutzervorgabe **entfernt**. Die früheren Innenraum-Stumm-Logs in diesem
Ordner beziehen sich auf den verworfenen Versuch, nicht den ausgelieferten Stand.
Keine nachträgliche Umschreibung dieser Belege. CheckSpecialBank/StopThePsgSound,
Originalanforderungen und Com_ExecRoomFadeIn enthalten keinen solchen Stop.
VolBgSe0/1 sind in RM_0060/0070 null und werden im Original-C nicht gelesen.
Der Regenfehler ist damit noch nicht als originalgetreu behoben nachgewiesen.

Belegter gemeinsamer Ladefehler korrigiert: RoomWorld lädt/consumiert keine
Soundbank mehr im Konstruktor. Runtime führt erst bhInitEvent aus und fragt dann
wie system.c bhSysCallSndMonitor RoomSoundCaseNo ab und lädt den Raum-SPQ, danach
RoomFadeIn. Dadurch gelten Sound-Varianten aus dem aktuellen Init für den
aktuellen Raum. `tools/verify_audio_state.py` prüft Cases 0/1 mit echten
SPQ/SQ/HD/BD-Ressourcen und isolierter Eventtabelle. Soundbank-Reihenfolge und Regenstopp sind unterschiedliche
Sachverhalte; die Korrektur wird nicht als unbelegter Regenfix ausgegeben.
Der Original-Türweg 0050 → 0060 → 0070 → 0060 → 0050 läuft weiterhin durch;
Ambient bleibt ohne originale Stop-Anforderung bestehen.

RM_0070: gemeldeter Crash während des Fenstersprungs bisher nicht reproduziert.
700-Tick-Replay mit jeder Zwischenpose, vorhandener Snapshot, Steve-Geschenk
vor dem Raumwechsel und normaler SDL-Renderlauf sowie produktiver Mac-Starter mit 1047 Simulationsticks
bestehen ohne Skeletonfehler. `--validate-poses` prüft auch im Headless-Modus
jede Zwischenpose statt nur das Schlussbild. Diagnosen enthalten Channel-/Bone-
Anzahl und beim Entity-Rendering Work-Art/Index, Identität, Motionquelle und Frame.
Keine Motion-Trunkierung, keine Ersatzpose und kein entfernter Gegner. Diese
Prüfungen sind ausdrücklich kein Nachweis, dass der Nutzercrash behoben ist.

Prüfungen: Mac arm64 25 CTest-Gruppen, acht Originaldaten-Suiten und zehn Runtime-
Suiten; ASan/UBSan 25 Gruppen, acht Datensuiten sowie Event-/Pickup-, Kontinuitäts-
und Audio-Zustands-Replays. Windows x64 erfolgreich cross-kompiliert; in dieser Änderung kein
Wine-/Windows-Hardware-Lauf. SDL-dummy prüft den normalen GUI-Simulations-/Render-
Pfad, jedoch kein physisches Fenster oder Controller. Logs/States:
`build/validation/room-audio-20261006/original-order`; frühere Versuche unter
`build/validation/room-audio-20261006`, frühere Pickup-Vergleiche zusätzlich
`build/validation/scripted-pickup-20261006`. Kein neuer vollständiger Raumscan
oder Durchspielnachweis. Beide produktiven Pakete aus diesem Arbeitsbaum aktualisiert, Mac-Runtime/SDL
ausschließlich arm64 und Signatur geprüft. Paketdaten-Event-/Pickup-Replays und
Audio-Zustandsprüfung bestanden. Start.command-SDL-dummy-Fensterreplay vollständig
abgeschlossen; INI-/Snapshot-Checksummen unverändert. Backup: `archived/releases/pickup-audio-before-20261006-202844`.

# Engine: Rasterkosten und frühe Progression — 2026-10-06

Dieser Abschnitt hat Vorrang vor älteren Scan-/Paketangaben. Renderer nutzt
konservative Scanline-Spannen und bei konstantem Texturalpha einen vereinfachten
Filterpfad, ohne Auflösung/Qualitätsstufe zu reduzieren. Kontrollierter Raster-
Vergleich: drei originale Räume, SS 1/2/3, je 90 identische Frames, ohne HD/NPCs:
18–26 % weniger Renderzeit. RM_0050 SS2: 23,74 → 18,03 ms; SS3: 50,67 → 38,56 ms.
Das ist kein vollständiger Gameplay-FPS-Vergleich. Software-Rendering bleibt
bei Ultra teuer. Pixelvergleich RM_0020 SS1: zwei von 921600 RGB-Kanälen weichen
um maximal eine Stufe ab (Rundung). GUI-Ultra-Lauf: 300 Renderbilder/380 Ticks,
13,67 s real/45,81 s User-CPU, 294 MB RSS; andere Szenen nicht gleich schnell.

Originale Raumnamen sind dezimal (system.c sprintf %1d%02d%1d), keine Hexwerte.
Gemeinsame RoomIdentity korrigiert Türziele, VM-Raumwerte und Soundbank-Schlüssel.
RoomSoundCase 0xb6, SoundVolumeSet 0x77 und BGSeOn2/Off2-Anforderungen laufen
im gemeinsamen Audiozustand. Init-Anforderungen werden nach Init abgespielt;
Ambient-PCM bleibt beim Bankwechsel erhalten. SQ CC11 wird berücksichtigt.
Sony-Programm-/Samplelautstärke, Pitch/ADSR/Pan/Sequencer sind nicht vollständig.
Keine reine Pegelkorrektur speziell für Regen oder einzelne Türen.

Steve RM_0040: fehlendes bhDelYakkyou 0x6b (zwei Bytes) hielt das Skript vor der
Figurenszene an. Gemeinsame Freigabe aktiver Effekt-IDs 0/7 ist gebunden. Common
14/15/16/21 und Sub_controll 80/8f/90/92/93 sowie die originalen subpl.c-
Bewegungs-/Dispatchroutinen sind angebunden. Leere Original-MN_WORK-Tabellen
behalten bei Szenenfreigabe die letzte Pose; sie dürfen nicht dekodiert werden.
Noch keine vollständige Ninja-Motion-Interpolation oder komplette Gegner-KI.
Originaler Sicherheitsraum-Dialog bis VOICE1/0075 und Steuerungsfreigabe bestanden.

Interaktionsreichweiten/Orientierungsmasken aus bhCheckExmAtari; player.c-
Initstatus 0x40000000 fließt in die gemeinsamen Kollisionsfilter ein. Hip-gpy ist
im Original eine Bodenschattenabfrage, kein automatischer Sprung auf Plattformen.
Host-Bodenanpassung überspringt dadurch die geskriptete Treppe nicht mehr.
Originale kurze Auf-/Abstiegaktionen dnu/dnd und vorhandene Treppen-/Leiterroutinen
gebunden. RM_0050: laufende MTN 35/36, beide Höhen, anschließend freigegebene
Steuerung geprüft. Rückweg RM_0050 → RM_0030 → RM_0020 läuft durch Originalskripte.
Prüfungen nutzen diagnostische Positionierung, kein Nachweis eines vollständigen
natürlichen Durchlaufens. Ungebundene Wasser-/Partikeldienste bleiben diagnostiziert.

Security Box nutzt originale ItemBoxInit-Spritegruppen, Icons, Namen, Reihen-
Geometrie und Restriktionsdialoge. Originale Einfahr-/Scroll-/Cursoranimationen
sind noch nicht vollständig übertragen; keine 1:1-Zusage für das ganze Statusmenü.

Mac arm64: 25 CTest-Gruppen, acht Daten-Suiten und bestehende Runtime-Replays
bestanden; zusätzliche Security-/Treppen-/Rücktürprüfung bestanden. ASan/UBSan:
25 Gruppen und neue Daten-/Runtime-/Box-Zeichenprüfungen bestanden. Windows x64
crossgebaut; kein Windows-/Wine-/physischer Controllerlauf in dieser Änderung.
Scan: 205/205 laden, 35 Räume blockieren bei 600 Ticks ohne Eingaben; kein
Durchspielnachweis. Pakete Mac arm64 und Windows aktualisiert, INIs erhalten.
Logs/Ansichten: build/validation/core-performance-20261006; docs/VALIDATION.md.

Reproduzierbare neue Prüfungen:

```sh
build/macos-arm64/recvx-continuity-tests data
python3 tools/verify_continuity.py --runtime build/macos-arm64/recvx-runtime --data data
cmake --build build/macos-arm64 --target recvx-render-benchmark
build/macos-arm64/recvx-render-benchmark data RM_0050.RDX 90 2
```

Raster-Baseline in raster-benchmark.log ist der gleiche Renderer mit den beiden
Optimierungen zurückgenommen, nicht ein unverändert ausgeführtes altes Paket.
GUI-Vorher/Nachher-Skripte haben unterschiedliche Engine-Fortschritte; ihre
User-CPU-/RSS-Werte sind deshalb keine kontrollierte Prozentmessung.
Original-Box-Bild: security-box-v2.png; Steve-Szene: steve-security.png.
Runtime-Prüfungen: continuity-animation, final-checks, sanitize-continuity-replays.
Fehlversuche mit bereits vorhandenen State-Dateien sowie ältere Zwischenstände
bleiben zur Nachvollziehbarkeit im selben Logordner; ready-checks.log und die
Sanitizer-Paketlogs benennen die abschließend erfolgreichen Läufe.

Aktualisierte Starter: Mac arm64 + Windows x64. Mac-Paket mit eigenen Daten:
verify_continuity und verify_doors bestanden. Start.command-GUI: 90 Presents,
36 Renderbilder/37 Ticks; keine Titel-Tastenleiste. Mac-Signatur geprüft,
Runtime + SDL ausschließlich arm64; INI-Hashes vor/nach Install und GUI identisch.
Windows wurde nur crossgebaut. Paketbackup: /Users/patrickhoffmann/Projects/recvx-decomp/archived/releases/core-performance-before-20261006-160800.

# Titelmenü ohne Host-Tastenleiste — 2026-10-06

Die zusätzlich gezeichnete OK-/Controller-/Auswahl-Leiste samt schwarzem Panel
ist aus Frontend::draw(Main) entfernt. Originale Titelressourcen und Menüauswahl
bleiben erhalten; Einstellungen und Beenden sind weiterhin Host-Erweiterungen.
Der Spiel-Skip-Hinweis prüft jetzt dieselben sp_flg-Sperrbits 0x1c040008 wie
EventVm::skip_cinematic. Frontend zeichnet vor diesem Spielpfad und kehrt zurück.

Mac arm64: Build, bestehender Frontend-Test und Frontend-Ressourcen-Replay
bestanden; aktualisiertes Paket mit eigenen Daten ebenfalls geprüft. Tatsächlicher
Start.command-Fensterlauf (90 Presents) und Bild main-gui.png zeigen keine untere
Tastenleiste. Signatur gültig und Benutzer-INIs erhalten. Windows x64 neu gebaut
und Paket aktualisiert; für diese Änderung kein erneuter Wine-/Hardwarelauf.
Logs/Bild: build/validation/title-clean-20261006. Diese kleine UI-Änderung
belegt keine vollständige Originaltreue des übrigen Titelmenüs oder Spiels.
Backup: archived/releases/title-clean-before-20261006-151332.

# Aktueller Mac-Stand: ausschließlich arm64 — 2026-10-06

Auf ausdrücklichen Nutzerauftrag sind Intel-/Rosetta-Mac-Builds abgelöst.
`Start.command` startet `build/packages/macos-arm64-v0.5/RECVX Native.app`;
Fallback nur `build/macos-arm64/recvx-runtime`. Buildwrapper erzeugt arm64,
Paketer prüft Runtime **und SDL2** auf ausschließlich arm64. Alte Universal-
Artefakte liegen im ignorierten Archiv: `/Users/patrickhoffmann/Projects/recvx-decomp/archived/releases/arm64-before-20261006-150523`.

Der neue arm64-Build besteht 24 Gruppen, sieben echte Datensuiten und alle acht
Runtime-Suiten. Dasselbe gilt gegen die **eigenen Paketdaten**. Projektstarter
headless und tatsächlicher SDL-Fensterstart (90 Presents) bestanden; keine
arch-/Rosetta-Aufrufe. App-Signatur und erhaltene Benutzer-INIs geprüft.
Ein anfänglich parallel doppelt gestarteter CTest kollidierte am gemeinsamen
Snapshot-Testfile; danach einzeln vollständig bestanden (Logs erhalten).
GUI innerhalb der Sandbox brach am Fensterdienst ab; erlaubter GUI-Lauf bestanden.
Frühere Intel-/Rosetta-Ergebnisse weiter unten sind historische Prüfungen.

Beide Starterpakete enthalten die Claude-Übernahme und den neuen Blendpfad.
Windows/Wine besteht 24 Gruppen/sieben Daten-/acht Runtime-Suiten einschließlich
Paketdaten und Start.cmd. Vorige Integration: `/Users/patrickhoffmann/Projects/recvx-decomp/archived/releases/handoff-before-20261006-145909`.
Die Mac-Umstellung ändert die Windows-x64-Binaries nicht.

# Claude-Übergabe und gemeinsamer Transparenzpfad — 2026-10-06

Dieser Abschnitt ersetzt ältere Integrationsangaben unten. Arbeitsbaum enthält
Claudes Änderungen plus die Übernahme durch Codex; kein neuer Commit/Push.
Logs/Ansichten: `build/validation/handoff-20261006`.

- Übernommen: originaler View-Clip, Vertex-Gouraud, Cinematic-Skip für getaktete
  Sprechdialoge, Feuerzeug-Effekt/-Licht, zusätzliche Spieler-Modelle/Pferdeschwanz,
  Treppen-/Leiterbrücke, SQ-Programm/Note, separate BG-/Tür-Voices, Snapshot,
  Grafikqualität, Inventarcursor und 30-Hz-3rd-Person. Scope/Lücken der Brücken
  bleiben wie in CLAUDE.md beschrieben; beispielsweise ersetzt eine einzelne
  SQ-Note noch keinen vollständigen Sony-Sequencer.
- Codex: `generate_chunk_blend.py` übernimmt `lNaCnkSrcAlphaMode`,
  `lNaCnkDstAlphaMode` und `njColorBlendingModeSys::alpha_tbl` direkt aus den
  Originalquellen. UA-Strips verwenden ihren GS-ALPHA-Wert (RGB=(A−B)*C+D),
  einschließlich FIX-Addition/-Subtraktion. Nicht unterstützte Originalpaare
  melden einen Fehler. Material-Chunks 17–23 schreiben Blendbits wie NJD_CB_BA.
  Texturalpha wirkt nun bei Nearest und Filterung; der bisherige ungewichtete
  Filterpfad behandelte auch einheitlich teiltransparente Texturen als deckend.
  Gemeinsame Kanten aller transparenten Dreiecke werden nur einmal überblendet.
  Host-Float-/Alpha-Normalisierung ist kein bitgenauer GS-Quantisierungsnachweis.
- Tatsächliche RM_0090-Fenster 32/49 mit Register 0x44 geprüft. Bilder
  `window-before.png` (voriges Paket) und `window-alpha.png` zeigen denselben
  Schnitt mit denselben Ressourcen; die Figur hinter der Scheibe ist jetzt
  sichtbar. `window-hd.bmp` prüft den Lauf mit den lokalen Mods. Keine extra
  Raum-Transparenzregel, andere Modelle/Items verwenden denselben Decoder.
- Steve: normale Wege aus **beiden** RM_0030-Tür-Spawns lösen alle zehn Tasks
  aus und geben die Steuerung frei. `steve-walk*.txt` enthält keine Position-
  oder Flagänderung und keine OK-Eingaben. Diagnose-/Skip-Prüfung bleibt separat.
  Dieser aktuelle Fehler war nicht reproduzierbar; kein neues Raumskript erfunden.
- Türen: beide natürlichen Durchgänge liefern 1/0/2 aus Originalbank 7.
  Drei separate Voices 27–29 bleiben beim Nachladen der Raum-Soundbank aktiv;
  neuer Ressourcencheck schützt insbesondere den Schließton vor Abschneiden.
- Host, macOS arm64/Intel-Rosetta, Windows/Wine und ASan/UBSan: jeweils
  **24 Testgruppen**, **sieben Datensuiten** sowie Events/Doors/Frontend/
  Progression/Snapshot/Combat/Debug/Event-State bestanden. Intel-Combat musste
  zunächst wegen eines Prüfskript-Pfadfehlers neu gestartet werden: --emulator
  bedeutete dort pauschal Windows-Z:-Pfade. Pfadkonvertierung jetzt nur für .exe;
  danach erfolgreich. Wine brauchte einen erlaubten lokalen wineserver-Socket.
- Raumscan: **205/205 laden, 38 blockieren**, unverändert. Begrenzte Replays,
  kein vollständiger Durchspiel-, physischer Controller- oder Windows-PC-Test.

# Flammen, Bewegungen, Security-Progression und Renderlast — 2026-10-06

Dieser Abschnitt ersetzt frühere Integrations-/Scanangaben weiter unten.
Logs und echte Ansichten: `build/validation/progression-20261006`.

- **Originaleffekte:** Generator übernimmt bhEff200/201/218 aus effsub5.c.
  EF_WRK, originale Atlasgruppen 58/59/70–79, Bildfolgen und Bodenanker laufen
  im gemeinsamen World-/Rendererpfad. Additive Polygone beachten die Tiefe,
  schreiben keine Tiefe und verdoppeln ihre gemeinsame Kante nicht.
  RM_0020 erzeugt elf Feuerquads und einen Funkenquad (24 Dreiecke).
  `cemetery-fire-runtime.png` zeigt die wirklichen Flammen nach dem originalen
  Friedhofereignis, nicht nur einen registrierten Effekt. Explosionsemitter,
  Rauch, andere Effekttypen und Linktyp 5 bleiben ungebunden; ihre Befehle
  bleiben als pending sichtbar. Die RNG-Startphase ist ein Host-LCG.
- **Animation:** en01-Raum-MTNs verbinden die originalen acht unteren mit den
  zehn oberen Kanälen (mtn_no+1). Fünf echte Raumressourcen geprüft.
  Common-Knotenänderungen enden erst beim nächsten tatsächlichen Schreiben
  durch die Motion (Position nur mit Key, Winkel auch bei leerem Key).
  Keine dauerhafte Null-Wurzel mehr; Claire/Rodrigo im Prolog wieder sichtbar.
  LoadWork selektiert NPC-Work ohne künstliche Steuerungssperre; SubCtr 0x80
  gibt die Animation an die originale InitType-Pose zurück. Keine vollständige KI.
- **Steve-Musik:** ADX-v3-Loopflag/Start/Ende werden gelesen. BGM1/0048 hat
  keinen Loop und endet nach rund 22,38 Sekunden, ohne künstliche Wiederholung.
  BGM1/0000 erhält sein Intro und die eingebettete Loop-Teilsequenz.
- **Metalldetektor:** bhCamSet2 schaltet die originale feste Kamera.
  bhCheckFloorP wird wie durch bhControlPlayer nur bei sp_flg Bit 0 ausgeführt.
  Zuvor startete die Bodenprüfung während der Ansage Event 11 erneut und
  verdrängte Event 2 im selben Tasklevel. Jetzt laufen VOICE1/0455, Kamera 9
  und der originale Abschluss. Nachricht 16 ist im Dump FF02/1/FFFF: bewusst
  leer, keine erfundene Untertitelzeile. `verify_progression.py` prüft Ansage,
  Kamera, Freigabe, Boxeingabe, Ablegen, erneutes Passieren und Zurückholen.
  Triggerpositionen sind diagnostisch gesetzt, kein Laufweg-Durchspielnachweis.
- **Security Box:** bhPlItemBlockCk, SecurityItemSet/Back und originales
  ItemBoxChange aus sub1.c (Quelle als 67,97 % matching kommentiert).
  Verwendet die originalen Inventarbereiche und Typ-/Schlüsselrestriktionen;
  Platzmangel rollt die Transaktion zurück. Tastatur/Controller nutzen den
  gemeinsamen modal_input-Hook. Das originale System-Boxflag öffnet das Menü.
  Linke Boxliste ist ausdrücklich eine Host-Ansicht; originale Fensteranimation
  und Spriteoberfläche sind noch nicht portiert. `security-box-final.png`
  wurde auf überlappende Texte geprüft.
- **Hunde:** Originale Bereichs-/Trap-Routinen aus zonzon.c/en04.c aktivieren
  Cerberus nach Floor/Gruppe, setzen originale Spawnposition/Richtung, Lauf-MTN
  und rm_flg. Zwei echte RM_0050-Traps und der wartende Hund geprüft.
  Verfolgung, Angriffe und die vollständige en04-Zustandsmaschine fehlen;
  eine aktivierte Laufanimation ist kein Beleg vollständiger Hund-KI.
- **CPU:** Texturpixel und Mipcache werden zwischen Framekopien geteilt;
  Änderungen lösen Copy-on-write und Cacheinvalidierung aus (auch Änderungen
  außerhalb der früheren Fingerprint-Stichprobe). Default höchstens vier Worker.
  GUI rastert/lädt nur bei neuem 30-Hz-Tick oder Eingabe, Präsentation bleibt
  VSync. Gleicher RM_0050-Lauf mit Mods, 180 Präsentationen: vorher 19,92 s
  User-CPU, jetzt 6,70 s (−66 %), 74 Neuzeichnungen; Laufzeit 2,93 → 3,34 s,
  RSS 271 → 275 MB. Kurze Messung auf diesem Host, kein allgemeiner M4-Benchmark.
  Software-Rendering/HD/Supersampling kosten weiterhin Rechenzeit.

Arbeitsbaum-Prüfung: **22 CTest-Gruppen**, sechs Originaldaten-Suiten
(Inventar-Engine, Game, Input, Combat, Audio, Progression), Frontend-/Tür-/
Cemetery-/Steve-/Security-/Snapshot-Replays auf Host, Mac arm64, Intel/Rosetta,
Windows/Wine und ASan/UBSan. Intel zusätzlich alle Unit-/Datentests; keine
physische Windows-Hardware oder Controller geprüft.
Raumscan: **205/205 laden; 38 blockieren** (vorher 37). Vergleich mit dem alten
Paket zeigt ausschließlich RM_9301 neu: Event 10 erreicht jetzt bhGameOverSet
0xc6, dessen Oberfläche nicht implementiert ist. Kein stilles Überspringen.
600 Ticks/Spawn 0 ohne Eingaben beweisen keine vollständige Progression.

Renderer/Filter/Threads werden wegen der gemeinsam geprüften CPU-/Effektarbeit
gezielt in diesen Engine-Commit integriert. Claudes HD-/Titel-/Equip-/Snapshot-/
Grafiksettings-/3rd-Person-Erweiterungen bleiben im Arbeitsbaum und den gemeinsam
geprüften Builds erhalten; das Paketmanifest nennt ihre zusätzlichen Quellen.

Beide v0.5-Starterpakete sind mit den Zielbinaries aktualisiert. Root-/Paketstarter
Mac arm64/Intel und Windows/Wine starten mit eigenen Daten; aktueller kompletter
Lighter-Replay, Security-Box-/Event-Replays der Pakete bestanden. Die erste
Lighter-Starterprüfung verwendete einen alten Replay mit zu schnellen Equip-
Eingaben; mit dem aktuellen Replay (4-Tick-Kommandofenster) bestanden. Keine
Änderung an Eventlogik/Tempo für diese Prüfung. Normale Mac-SDL-Ausgabe 90
Präsentationen/42 Renderings geprüft. App deep/strict signiert; Benutzer-INIs
unverändert. Backup: `archived/releases/progression-before-20261006-095700`. Originaldatenmanifest: 21.227 Dateien geprüft.
Der selektive Engine-Commit eb38b42 wurde zusätzlich separat aus HEAD plus
seinen Quellen gebaut: 20/20 Gruppen, Originalressourcen und Security-Replay
bestanden, unabhängig von Claudes uncommitteten Erweiterungen.

Szenenbilder bei Tick 500/520/540 zeigen den Zombie mit gebeugtem Oberkörper und
Armen beim Herausklettern/Fallen statt T-Pose. Das beweist nicht die komplette
Filmsequenz: Kamera 4 liefert bei 560–600 eine fast einfarbige Nahansicht;
Kamera-/Schnittprogression ist deshalb weiterhin nicht vollständig verifiziert.
Keine Kamerapositionen oder Eventzeiten passend zu einzelnen Screens ersetzt.

# Original event word state and game clock — 2026-10-06

This supersedes the earlier scan and package counts below.

- `event.c::bhCmpW` 0x07 reads signed player/enemy HP, live or latched
  `mes_sel`, and `stv_tm`. All six operators and unsigned 16-bit operands
  are tested independently. `bhSvW` 0x09 updates actual shared work state;
  fields 11–15 assign negative HP to enemies 0–4 as the original does.
  Missing enemy work blocks at the original PC. Real RM_0000 actor work
  confirms assignments and conditions reach the same entity, not a byte register.
- `bhRoomCaseNo` consumes two bytes and passes the variant through DoorCall.
  Message marker 0xfe09 sets the default question choice; page changes reset
  selection. Confirmed status-menu answers keep the original 100/101 offset,
  including after the question closes. This does not complete the original
  status-screen transition animation or item-box interface.
- `sync.c::bhControlVSync` game time is room-independent and advances two
  VSync units per 30-Hz game tick, gated by event flag 3, capped at 216000000.
  Title/debug pause do not advance it. `tools/verify_event_state.py` uses real
  exported room geometry with an isolated event-table fixture to verify this
  runtime contract; it is not a campaign script or playthrough test.
- Host, Mac arm64, Intel/Rosetta, Windows/Wine and ASan/UBSan passed **20 test
  groups**, five original-data suites, and original frontend/doors/events
  replays. All five architectures passed the extra clock/pause runtime check.
  No physical Windows hardware or controller was used.
- Same 600-tick scan: **205/205 rooms load; 37 report blocked commands**,
  previously 38. Later unbound commands remain visible; this scan does not
  prove full progression or enemy AI. Logs: `build/validation/event-conditions-20261006`.
- The 4-byte asset error reported in the older renderer handoff did not
  reproduce in baseline commit 1978aa0: original door/lighter replay passed
  before this change. No speculative audio/resource repair was added.
- Claude's HD/filter/supersampling/parallel-renderer work is preserved and
  included in tested working-tree binaries; those sources stay outside this
  engine commit. Source manifests distinguish committed and working-tree code.

Both v0.5 packages updated; root/package starters passed with their own data
on Mac arm64/Intel and Windows/Wine, including the full opening/lighter replay.
The normal Mac SDL inventory/examine path ran for 90 frames; screenshot:
`build/validation/event-conditions-20261006/package-gui.png`.
Mac app signature passed deep/strict verification and user INIs were preserved.
Backup: `archived/releases/event-conditions-before-20261006-083827`.
The packages include Claude's tested parallel-renderer sources; HD-pack files
remain unbundled. Binary/source SHA-256 manifests record the integration.

# World controls and scheduler integration — 2026-10-05

This supersedes the older scan/build counts below.

- Original `event.c::Common_controll` model-node changes (17–19, 36, 38),
  model-node position/angle interpolation (28–33, 44–45), key capture (48/49),
  low-byte type assignment (50) and `Obj_controll` 1/20 are bound. Overrides feed
  the same weighted vertices, normals and matrices. Object/item attachments
  preserve full parent-node orientation, including static MDL hierarchies;
  player link lookup resolves original MASK script indices. Complete enemy AI
  and every possible linked actor animation are not established by these checks.
- `Event_init` resets interpolation keys without discarding work selection or
  movement increments. Common 44/45 consume five bytes, including their trailing
  padding; 38 consumes ten. Tests verify subsequent opcode alignment.
- `Event_T_timer` advances once per active scheduler pass, including multiple
  tasks and an in-pass clear. `bhTFrameCheck`, `bhEventTimerClr`, `bhEvtKill`,
  `bhEneLifeSetM`, `bhLightParameterSet`, `bhGrdPosMoveCSet/Start` are bound.
  Spline countdown keys change the actual walls used by movement and ray checks.
- Enemy SE uses six independent voices and original MULTSPQ bank 3; actual room
  0002 sound PCM, stop isolation and room reload are checked. Spatial pan/volume,
  Sony sequencing, ADSR and reverb are not fully implemented by this change.
- Pony reset, shadow switch, vertex-morph setup, effect UV/local offsets and
  object-SE deletion now parse exact operands but retain `pending_bindings`;
  parser progress is not evidence these effects are rendered or sounded.
- **20/20** unit groups passed on Host, Mac arm64, Intel/Rosetta, Windows/Wine,
  ASan/UBSan. Original-data suites inventory-engine, game, input, combat and
  audio passed on each; frontend/doors/cemetery/Steve replays passed. Logs:
  `build/validation/world-control-20261005`. No physical controller/Windows PC.
- **205/205 initial rooms load; 38 report blocked commands**, previously 72.
  Newly reached RM_3180 DoorCall requests RM_3100 spawn 2, whereas this dump has
  only two POS entries. The shared destination preflight reports that condition
  in the event task and preserves the current room; no guessed spawn is used.
  This exact failure path was checked on all target architectures and sanitizer.
  The room scanner distinguishes initial load failures from runtime failures
  after a successful load. No complete-playthrough claim.
- Claude's concurrent HD/filter/supersampling changes are compiled and tested,
  preserved in the working tree, and excluded from the engine commit.


Both v0.5 package binaries were updated and their project/package starters checked
with their own data, preserving user INIs. Mac arm64 and Intel/Rosetta launch;
Windows/Wine root and package launch; full opening/lighter replay passed.
Mac codesign deep/strict validation passed. Backup: archived/releases/world-control-before-20261005-233728.
Package cemetery replay renders the original NPC meshes/motions and restores
control; this is not evidence of complete zombie AI. Handgun ammo view inspected
at room 0010; normal Mac SDL GUI inventory/examine ran for 90 frames.
Manifests retain source hashes and list Claude's concurrent compiled sources.

# Item camera-space and spatial event integration — 2026-10-05

This supersedes the previous integration snapshot below.

- The prior handgun-ammunition image was the actual host display, not a debug
  orientation. `DrawSubItem` uses an identity camera-space basis; the host had
  incorrectly added the room camera's Y/Z inversion and viewed the back.
  `SoftwareRenderer::draw_view` now shares clipping/rasterization with the room
  renderer while accepting the original item-view basis and 500-pixel focal
  distance (PS2 screen aspect 1.174). Framing still fits the decoded visible mesh;
  original ModelScaleSet/hoseipos framing is not completely ported.
- All 156 default rotation matrices are compared independently against the
  original DSP_WORK X/Y/Z construction. Euler decomposition at Y=±90° uses a
  stable equivalent pose. Action spin is a separate X/Y/Z matrix multiplication,
  not addition of Euler angles. All eleven menu transformations retain the
  original ChangeZoomOut / DrawSubItem subcsr 1/3 result poses through messages.
- `bhAreaSearchObj` 0x72: original player/enemy/object/item work lookup, two
  separate sign masks, inclusive minima/exclusive maxima; missing lookup blocks
  with retained PC. `bhGrdPosSet` 0xb9 mutates the shared ATR_WORK wall positions
  used by walking, ground probes, shot rays and the camera mod. Parser alignment,
  all four object kinds, signed bounds and live collision changes are tested.
- Room scan: **205/205 load; 72 rooms report blocked commands**, down from 81
  using the same 600-tick scan. Later commands can become visible after a blocker
  is removed. This is not proof of completed room/game progression.
- Host, macOS universal arm64 and Intel/Rosetta, Windows/Wine, ASan/UBSan:
  19 test groups passed, including the parallel HD-content-key test. Five
  original-data suites (inventory engine, game/events/assets, input, combat,
  audio) passed on all targets. Original frontend, door and cemetery/Steve
  runtime replays passed on Host/Mac/sanitizers/Windows-Wine.
- Both v0.5 starter packages updated and launched with their own data; macOS
  arm64/Intel and Windows/Wine passed. Real knife-pickup modal was rendered from
  the package; ammunition examination images are pixel-identical on Mac/Wine.
  Mac GUI item examination passed through the normal inventory path (no debug
  menu). App signature verified; user INIs preserved. Previous binaries:
  `archived/releases/item-view-before-20261005-230857`.
- Logs and render evidence: `build/validation/item-view-20261005/`.
  Physical controllers and physical Windows hardware remain untested.
- Claudes HD-code remains separate uncommitted work. It is compiled into these
  working-tree builds and passes the lookup test; the 3.4-GB replacement pack
  is not included/activated in the starter packages. HD matching tools and
  their reported matching coverage are not revalidated by this integration.

The previous limits remain: enemy AI, helicopter/particle effects, face/lip
morphs, MIDI sequencer/ADSR/reverb, HD UI, item box, map and save/load.

# Current integration validation — 2026-10-05

This section supersedes older snapshots below. Current sources include Claude's
27 room-loader corrections and Codex's shared timing/audio/actor/inventory work.

- Host, macOS universal arm64, Intel/Rosetta, Windows x64/Wine and ASan/UBSan:
  18/18 native test groups passed. Seven original-data suites (inventory engine,
  inventory UI, input, events/assets, combat, 105 door cases and audio) passed
  on arm64, Intel/Rosetta, Windows/Wine and ASan/UBSan.
- Runtime frontend/film/hint/lighter/Rodrigo, physical walking door round trips,
  OK door skip, original knife pickup/equip/attack, debug actions and both
  cemetery/Steve task groups passed. The cemetery is walked; Steve's trigger is
  positioned diagnostically. Windows uses Wine, not physical Windows hardware.
- 205/205 exported rooms load in the 600-tick scan; 81 still report blocked
  original event commands. This does not establish full game progression.
- One audio second produces exactly 48,000 stereo frames over 30 engine ticks;
  film durations and fades use the same clock. This fixes the doubled host rate.
- Original MASK-derived node mapping and segmented en01 body motion regression
  tests passed. Render inspection: Rodrigo's hands at his wrists; cemetery
  zombies have complete body poses; pickup default angles are oblique and the
  dialog does not overlap the item name.
- Render evidence: `build/validation/core30-visual/`; complete target logs:
  `build/validation/core30-full/`; room scan: `/tmp/recvx-core-roomscan.log`.

- Existing macOS universal and Windows x64 v0.5 packages updated. Root and
  package starters passed with their own loose data; macOS Intel package also
  started. Mac GUI launch ran 90 presentation frames with default window/UI.
  macOS app signature passed deep/strict verification. Existing controls/debug/
  third-person INIs retained. Backups: archived/releases/core30-before-20261005-
  224356 and 224819. Final launch logs: build/validation/core30-launch-final.
  source-snapshot.json in each package records the integrated source hashes.
- A final input regression checks that automatic inventory opening emits SE 3;
  keyboard/SDL virtual controller paths passed on all targets after that change.

Limits: enemy AI/attacks, full face/lip morphs, original explosion/particle
rendering (including helicopter), ambient MIDI/sequencer/envelopes/reverb,
three absent door-program requests, HD UI, item box/map/save/load remain open.
The display defaults to 1280×960, integer-scaled from 640×480; this is not
high-resolution native rendering. Physical controller/audio comparison remains
unverified. Native tests use original data and SDL virtual controllers.

# Local validation of native 0.5

Validated on 2026-10-05. This records checked behavior, not completion of the port.

| Check | Result |
|---|---|
| macOS universal build | arm64 and x86_64 slices present |
| Windows build | PE32+ x64 executable and bundled SDL2 |
| CTest | 14/14 on macOS; 14/14 under Wine |
| ASan + UBSan | 14/14 and original-data opening/controller/inventory regressions passed |
| Intel/Rosetta | original-data opening/UI and SDL virtual-controller regressions passed |
| Disc data manifest | 12,701 files; 6,574,841,350 bytes; both packaged trees verified |
| Room scene decoder | 205/205, 1,008,630 stage triangles, 470 active camera targets |
| Five playable bodies | 272 supported body clips; all 7,901 frames sampled |
| Opening without skipping | 15,000 simulated ticks, original movie completed, cell hint/camera 37 reached |
| Original skip/lighter path | hint confirmation, inventory equip, floor condition, Rodrigo voices, restored position/control |
| Post-event movement | 120 running ticks moved Claire through the same native movement/collision path |
| Dialogue rendering | no panel; white core and black edge; uncovered pixels preserve the scene |
| Side pack | original GM flag switches 8/10 normal cells and cursor bounds; frame row changes |
| Native skin normals | weighted influence rotation tested separately from positions |
| Packaged launch | own packaged data; Windows replay under Wine; bounded macOS SDL window/audio start |
| Configurable input | keyboard/gamepad rebind, axis inversion, deadzone, trigger, short presses, repeat, focus and hot-plug/removal passed with SDL virtual GameControllers |
| Gamepad opening flow | original cell skip/dialogue/inventory/lighter/Rodrigo/control-return path passed on arm64, Rosetta and Windows/Wine |
| Main menu | original ADV title; idle leaves event VM, film and game audio inactive; new game starts original opening |
| Settings screen | keyboard/controller pages, live rebind, Confirm/Cancel conflict swap, persistent atomic INI save, default restore, stick thresholds and Escape capture cancel checked |
| Running film skip | original active film skipped by keyboard Space and SDL virtual A; movie/PCM stop, cell hint/camera 37, fast inventory equip, Rodrigo VOICE1/0016 and control return verified |
| Automatic inventory after hint | original opening/late film skip; OK alone opens inventory; held forward input leaves actor position unchanged |
| Physical controller | no mapped controller or joystick detected locally; physical Windows XInput hardware not tested |
| Backward movement | original clip 16/key 25, advancing pose, signed curve, unchanged facing, return to idle and controller backstep checked |
| Original doors | 105 MRY parameter cases rendered, completed and skipped; original hinge/camera/fade procedures compiled natively |
| Room round trip | cell/hall/cell walked without teleport; natural and OK-skipped sequences give identical spawn, retain items/equipment/HP and do not replay opening; arm64, Rosetta, ASan/UBSan and Windows/Wine |
| Confirm during doors | keyboard Enter and SDL virtual A, held-key suppression and inventory blocking tested; binding remains configurable |
| Lighter resources | item ID 55 reads ITEM1/0054.bin and original PVP textures; rotated 3D view differs; old diagnostic screenshots archived |
| Solid wall collision | ATR types 0..5; room-height fallback, original player dimensions, floor attributes, thin walls both directions, signed diagonal walls, circular contacts, sliding and corners checked |

The original-data opening regression passes with 109 known unbound commands.
Particle effects, lighter/flame object, full MASK/face/lip processing, camera
interpolation, ambient/door SE routing, full enemy AI/projectile combat and complete
puzzle/progression/save systems remain pending. A successful room decode does
not verify that room's gameplay. Wine validation is not a physical Windows test.

The original ISO, previous Git and obsolete PS2 compiler/CRI/Katana distributions
are preserved under ignored archived/. The new source Git excludes private data,
archives and all build/package outputs. No remote was configured or pushed.

Direct root launchers `Start.command` and `Start.cmd` expose the existing packaged
builds. `STARTEN.md` describes launch, first equipment flow and door controls.
Door sound banks, special door lighting and rumble are not yet implemented.
The inventory regression also checks retained poison/HP/side-pack state across
VM reconstruction; door filters support signed below-ground floor IDs.

The main menu and settings use original title art and font atlases. They provide
a native development menu; the complete original title/difficulty/load system is
not yet ported. `tools/verify_frontend.py` verifies real-runtime default idle,
settings, active movie and the complete late-skip/rapid lighter flow.

Mac-Paket mit eigenen Daten: Event-/Steve-Geschenk-/RM_0070-Replays bestanden;
Paketstarter RM_9070 mit SDL-Dummy-GUI: 60 Presents, 61 Ticks, keine Blockade.
Mac-Runtime und SDL nur arm64, Signatur geprüft. Benutzer-INIs/Snapshots
per SHA-256 unverändert. Backup: archived/releases/events-before-20261006-214742.
Keine physische Controller-/Windows-Hardware-Prüfung, kein Commit/Push.

## Gameplay corrections, 2026-10-05

Added real-data regressions for automatic hint → inventory ownership, A/D turn
directions, continuous D-pad item inspection, voice dismissal, animated health
pulse, knife/ammunition Yes/No, full-inventory rejection and persistent pickup
flags. RM_0020's shared NPC texture banks now render without a texture-range
exception; the actual first-stairs door transition from original hall spawn 1
loads RM_0020 and its initial bytecode has no blocked commands. All eleven COMMON.MLT
programs decode to non-silent PCM. Original title/menu atlas sprites are used.
Hands/accessory libraries, ambient MIDI routing, full pickup animation/layout and
complete scene fidelity remain open; these tests do not certify those systems.

Verification for this revision: 12/12 groups on macOS universal and Windows/Wine;
12/12 under AddressSanitizer/UndefinedBehaviorSanitizer. Original-data gameplay,
controller and sound-bank checks passed on arm64, Rosetta and Windows/Wine.

## Room audio and attachment extension, 2026-10-05

Real-data tests now decode all five floor programs, select cell/hall sound packs,
stop room channels on reload, reject missing/invalid programs and verify healthy
walk/run/backstep contacts with silence between contacts and during idle. Walked
cell/hall round trips require original sound dispatches in both rooms; the automatic
hint/inventory test rejects footsteps while gameplay input is blocked.

Attachment tests use original room item geometry and check updated player-bone
translation, inherited visibility, release and Rodrigo model-node hide/restore.
Pose-only matrices are compared against geometry-sampling matrices. These tests
do not establish complete hand/accessory rendering, ambient MIDI or physical
controller/audio-device fidelity.

## Inventory combinations and item checks, 2026-10-05

The new inventory_combine_inspect group verifies all eight herb recipes in both
orders, invalid recipes, cancel/result cursor, full capacity, red-herb rejection,
healing, partial powder conversion and full-magazine dialogue/rollback. Four
original cases/briefcase variants use real SYSMES.ALD questions and contents,
missing-lockpick checks, wrong facing, Yes/No/Cancel, replacement at full capacity
and retained lockpick. Real ITEM1 packets render closed/opening/content views;
PS2 hinge geometry moves without losing model parts. Contents IDs are read from
idsettbl, including handgun parts 63→19 and magnum case 129→30→13.

Keyboard and SDL virtual GameController regressions exercise Combine and case
opening through GameplayInput, hold suppression, No/Cancel, error sounds and
invalid key Use keeping inventory ownership. A bounded event fixture checks
original floor parameters, UseItemCheck/ItemLost/flag commitment, 0x7c parameter
changes, TG-01/ammunition rejection messages and retained empty extinguisher. This is not verification of all
world puzzles, physical gamepads, original zoom/catch animation, hidden contents,
file registration or complex multi-item extraction.

Reproduce the additional real-data check with:

```sh
build/native/recvx-inventory-tests "$PWD/data"
# Optional rendered diagnostic fixtures; the output folder must contain no old BMPs.
build/native/recvx-inventory-tests "$PWD/data" "$PWD/build/validation/inventory"
```

The regular build script runs this data regression after CTest.

## Knife, handgun, aim and pickup (2026-10-05)

The combat_weapons_ammunition group checks original constants, target height and
wall occlusion. The real-data invocation additionally loads original weapon
models/MTNs, ready/aim/attack/reload clips, sound banks, magazine depletion/empty
shots, held-fire animation cadence, modal blocking, real RM_0020 zombie damage
and death, NPC immunity and frame-gated knife hits without ammunition usage.
The input regression exercises keyboard R/Ctrl/Enter and SDL virtual RB/A,
original knife/ammo pickups, suppression of pickup while aiming, and migration
of older customized INI files. verify_combat.py checks the actual runtime through
the opening, original pickup/equipment, knife attack/sound and return to idle.

```sh
build/macos-universal/recvx-combat-tests "$PWD/data"
python3 tools/verify_combat.py --runtime build/macos-universal/recvx-runtime --data "$PWD/data"
```

The new original-data combat and input checks passed on arm64, Intel/Rosetta,
ASan/UBSan and Windows x64 under Wine. Full enemy AI and original damage/death
animations remain unported; corpse rendering is a documented native fallback.

## Debug menu integration (2026-10-05)

Native host, macOS universal and ASan/UBSan builds: 17 CTest groups passed.
Windows/Wine: 15 CTest groups passed; Wine service output handles prevented the
CTest run from completing after child processes exited. The remaining scene and
actor checks passed separately with output redirected to log files.

Debug-specific unit and original-data runtime checks passed on macOS arm64,
Intel/Rosetta and Windows/Wine; runtime checks also passed with ASan/UBSan.
Verified: F2/default file, F6 remap, repeat guards, software/bundle configuration
paths, action registration/errors, input handover, 180 paused ticks, all five
original character bodies, equipment/items/HP/bag/wireframe and room/spawn changes.
Both existing package starters passed debug actions using their own packaged
data and debug.ini; macOS bundle signature verification passed. Existing user
controls.ini files were preserved. See DEBUG_MENU.md and tools/verify_debug.py.

## Claude grab handoff / original player voices — 2026-10-07

Claudes uncommitted grab/bite/player damage/death work is retained. Attack default
remains off and Game Over remains the host placeholder. Continued the missing
player voice bank binding from sdfunc.c RequestPlayerVoiceSoundBank: CORE key
ply_id|0xfff0 on the active disc, bank 4/slot 7; bank replacement invalidates PCM
and source-pack diagnostics now follow each independently loaded SE bank.

Targeted Mac arm64 checks passed: audio suite with original data (four character
banks on both discs, nonzero bite/death PCM, character replacement and retention
across room banks); enemy-damage suite with original RM_0020 grab/escape/death
and Claire voice PCM. Escape HP 200 → 170; fatal bite clip 412. The two related
CTest groups pass but skip data checks when invoked without data; the explicit
data runs provide the resource evidence. Runtime and package runtime using their
own data complete 300 ticks RM_0020 without blocked commands or unavailable
audio; no grab from this door spawn, so these runs do not prove a natural bite.
Project Start.command SDL dummy smoke test: 15 presents, 1 simulation tick,
no blocked commands. No new broad verify_* run, room scan or sanitizer campaign.

Windows x64 crossbuild passed; Windows/Wine execution was not performed. Both
v0.5 runtime packages updated. Mac runtime/SDL are arm64 only; app signature
passed deep/strict verification. SHA-256 checks preserve 21 INI/snapshot files.
Backup: `archived/releases/player-voice-before-20261007-084428`. Logs: `build/validation/player-voice-20261007`.
No subjective gameplay/timing check, full playthrough, physical controller check,
commit or push. Room voice volume, pitch/ADSR/reverb remain incomplete.


## Raumwechsel: Spielerhöhe vor erstem Frame (2026-10-07)

Ursache: Der Host übernahm POS.py als endgültige Körperhöhe. `room.c:bhSetRoom`
führt stattdessen `bhSetFloorNum` aus und setzt unmittelbar danach
`plp->py = plp->gpy = rom->grand[plp->flr_no + 2]`, vor
`bhInitRoomChangePlayer` und dem späteren `bhInitEvent` (`system.c`, Modus 10).
Der gemeinsame Ladepfad ergänzt diese Phase; Restore behält wie der Originalzweig
`cb_flg 0x800000` die gespeicherte Höhe. Es gibt keine Raumregel, Wartezeit oder
vorgezogenen vollständigen Welt-/Eventtick. Bewegung/Bodenkollision unverändert.

Beleg unter `build/validation/room-spawn-20261007`: originaler Türübergang
RM_0040 → RM_0090, diagnostische Aufstellung vor der Tür. Bei 225 Ticks ist die
Tür noch aktiv; bei 226 ist der Zielraum sichtbar. Alter Build: Y=1 bei 226,
Y=0 bei 227. Korrigierter Build: Y=0 bei 226/227/240. Kein Nachweis eines
vollständig natürlichen Laufwegs. `tools/verify_room_spawn.py` prüft diese Grenze
und die erhaltene Snapshot-Höhe vor dem nächsten Simulationstick. Continuity-
Datensuite prüft zehn originale Spawns in RM_0040/0090/0050/0020/0070 inklusive
oberer Etagen und ersten Welttick. Mac arm64 und Paket mit eigenen Daten bestanden.
Windows x64 crossgebaut, kein Windows-Laufzeittest. Beide Starter-Binaries
aktualisiert, Mac-App signiert; Konfigurations-/Snapshot-Hashes und Backup stehen
in `package.json` im Logordner. Keine breite Suite oder neuer Raumscan, kein Commit/Push.

Zusätzlicher Buildblocker im aktuellen Arbeitsbaum: En01-Funktionsextraktion
scheiterte an einem Originalkommentar zwischen Signatur und `{` bei
`bhEne01_CutHead`. Kommentarbereinigung im Generator ergänzt; keine Änderung
der Originalroutine.

## Blutlachen: Raumübergreifender Effektspeicher (2026-10-07)

`EffectParticles` verwendete einen globalen Blockpool. Destruktion eines
RoomWorld gab dessen Effekte nicht frei. `spawn_pool` registrierte alle aktiven
id-300-Blöcke, die der neuen Instanz noch unbekannt waren; alte Lachen wurden
mit ursprünglicher Position/Höhe übernommen. Original `effect.c:bhInitEffect`
leert dagegen `eff[512]` und setzt `bl_ct` zurück. Blockpool und Systemzustand
gehören jetzt zur Instanz; die extrahierten Routinen werden mit einem Scoped-
Kontext gebunden. Originale Blutposition, BP_WORK-Offset, Wachstum und Zeichnung
bleiben unverändert. Keine Raumsonderregel oder künstliche Bodenprojektion.

Logs unter `build/validation/blood-pool-20261007`. Neue Gegenprobe schlug zuvor
mit `world adopted another world's blood pool` fehl. Nachher: zwei gleichzeitige
Effektwelten (Y=20/0), Partikel ohne Querübernahme und 300 Raum-Lebenszyklen
bestanden; der erste Lachenoffset ist in jedem neuen Raum wieder 0.11.
`recvx-enemy-damage-tests data OUTPUT blood-only` führt echte RM_0020-
Pistolen-/Todesroutinen aus und prüft genau 14 Pooldreiecke auf Y=0.11;
Projekt-/Mac-Paketdaten bestanden. Dieser Prüfmodus beendet sich vor den
übrigen KI-/Biss-/Explosionsfällen. Screenshot aus Kamera 0 zeigt den
Leichenbereich nicht und wird ausdrücklich nicht als visueller Beleg benutzt.
Die konkrete Vorgeschichte des Nutzer-Screenshots ist unbekannt; der Mechanismus
für eine zusätzlich übernommene schwebende Lache ist reproduziert.

Mac arm64 gebaut, gezielter CTest `effect_pool_lifetime` bestanden. Windows x64
crossgebaut, kein Windows-Laufzeittest. Beide Starter-Binaries aktualisiert,
Mac-App signiert/arm64 geprüft; INI-/Snapshot-Hashes und Backup in `package.json`.
Keine breite Neuprüfung, kein Commit/Push.


## Empty pistol and RM_0070 shelf ammunition (2026-10-07)

`recvx-ammunition-tests data` and `tools/verify_ammunition.py` passed on Mac arm64.
The room fixture places an equipped empty handgun in pip[2], then uses the actual
room camera gating, pickup question and acknowledgement. The gun remains packed
as 0x00050000; 15 rounds appear in pip[3] as 0x000c000f. Repeated empty triggers
leave the inventory unchanged. Screenshots show the original zero glyph.
The same initial reproduction also preserved ammunition and showed zero in the
previously distributed Mac package. The reported disappearance is **not reproduced**.

The previous unsigned host decrement is replaced by generated weapon.c
`bhCountBullet`, preserving the original signed short guard, packed ID and flags.
Automatic reload now calls generated sub1.c `bhSearchBullet` / `Combi_99` / `ItemSort`.
Tests cover four owners, four modes, occupied/full/wide inventory, stack overflow,
0/1/signed boundary and infinite counts, three pistol variants, reserve ordering,
partial stacks and equipment identity. A 15-round stack legitimately disappears
when transferred completely into an empty pistol; the next shot leaves 14 rounds
in its magazine. This is not proof of complete 1:1 combat: host attack/reload phases,
input timing and the original gm_flg reload-animation latch remain adapters.
Corrupt saved counts are not silently repaired by this change.

The existing combat original-resource suite passed; Windows x64 crossbuild passed
without a Windows execution test. No unrelated progression scenes or sanitizers
were rerun. Logs and images: `build/validation/ammo-20261007`.

## Gegnerfreigabe, Hunde, Fräse, Dateien und Padlock (Codex, 2026-10-07)

Sub_controll 80/90/8f/92 übergibt en01/en04 wieder an die originale Work-
Zustandsmaschine (Modi/Typ/ct2, stflg/flg und Bibliothek). 93 behält seine
abweichende reine Moduszuweisung. Treffer lösen zusätzlich die Host-Pause;
keine Zombie-Knoten-Flinchkorrektur mehr auf Hunde. RM_0080 Event 40 hatte
seine Freigabe zuvor nur als pending Binding, damit blieb der Gegner nach
Common 23 eingefroren. Bewegung und Blut nach originaler Freigabe geprüft.
En04 Init/Schaden/DG00/DG01/DD00/Erholung und Tabellen aus en04.c generiert;
Hunde sind in Zielwahl/Treffer-/Kapselpfad gebunden. Schüsse wählen jetzt
EneDamNear/Mid/Far nach originaler Gegner-ID statt fest en01. Beide RM_0050-
Hunde mit echter Pistole, Originalmodell/MTNs, Blut und Tod geprüft; Aufstellung
ist diagnostisch, Wände bleiben aktiv. Keine vollständige Hunde-Verfolgungs-/
Angriffs-KI, Treppen-/Feuer-/Säuredienste bleiben teilweise ungebunden.

Statische MDL-Objekte werten Raum-MTN bei Geometrie, lokalen Knoten und
Linkmatrizen aus; gemeinsamer Positions-/Winkelsampler mit Actors. Originales
RM_0090 Event 22 setzt Objekt 8 / MTN 6: gezeichnete Fräsmaschinenknoten bewegen
sich jetzt. Zusätzliche Frässpäne/Emitter sind damit nicht vollständig portiert.
Dateiaufnahmen (Exm attr 0x10 / cb_flg 0x20000) benutzen originales
FileNumberSwitch und itm[383], ACK 0x800 und vorhandenen Dateibetrachter;
kein GetItem-Slot. Echtes RM_6040 Memo 132 / Datei 17, Öffnen/Schließen ohne
Wiederöffnung geprüft. Vorhandene falsche Inventareinträge werden nicht migriert.

Padlock-Frage RM_0050: vorhandener Antwortpfad war auf Interaktionsdialoge
(st_flg 0x2000) begrenzt. message.c setzt auch bei Skriptfragen cb_flg 0x1000 /
st_flg 0x4000. Gemeinsame Rückmeldung korrigiert; originale Schlüsselbenutzung,
Frage/Ja, PlItemLost 84 und Steuerungsfreigabe bestanden. Kein Beleg für eine
Autorzuordnung vorhandener Teiländerungen an Gemini. Die Blutlachen-
Raumlebensdauer war bereits korrigiert: bestehende Isolation und zusätzliche
RM_0020-Todes-/Wiedereintrittsprüfung bestätigen null alte Lachen im neuen Raum.

Gezielt: sechs CTest-Gruppen, neue Originaldaten-Regressionsgruppe,
Kampf-Datensuite und Friedhof-Blut-/Wiedereintritt bestanden. Mac arm64 und
Windows x64 gebaut (Windows nur crossgebaut). Beide v0.5-Runtimes ersetzt;
Mac-App signiert, Runtime/SDL nur arm64. Paketdaten-Regressionsgruppe und
RM_0050-Paketlauf 160 Ticks / --validate-poses ohne Blockade bestanden.
Sieben Benutzer-INI-/Snapshot-Dateien per SHA-256 unverändert. Logs:
build/validation/gameplay-fixes-20261007; Backup:
archived/releases/gameplay-fixes-before-20261007-143421.
Keine breite Prologprüfung, kein Durchspielnachweis, kein Commit/Push.
