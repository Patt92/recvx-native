# Prüfungen

Prüfungen belegen konkrete Originalmechanismen und ihre Abhängigkeiten.
Ein bestandener Test, ein Screenshot oder ein Replay ist kein Nachweis
vollständiger Portierung oder eines vollständigen Durchspielens. Die
Prüfmethode für Portierungsarbeit steht in [QUALITY.md](../QUALITY.md).

Es gibt drei Ebenen:

1. **CTest-Gruppen** – Einheitstests und Tests gegen Originaldaten.
2. **Prüfskripte** `tools/verify_*.py` – echte Runtime, Headless-Replays mit
   Originalräumen und -skripten.
3. **Raumscan** – startet jeden Raum und berichtet Lade- und Skriptfehler.

## CTest

```sh
./build.sh --test
```

```sh
# macOS: ./build.sh legt build/macos-arm64 an (Linux/CI nutzt build/native)
ctest --test-dir build/macos-arm64 --output-on-failure
```

Ein gezielter Lauf baut Testziele und führt nur passende Namen aus (leere Auswahl ist ein Fehler):

```sh
./build.sh --test-regex '^(gpu_raster.*|scene_renderer|texture_filtering|third_person_camera_mod)$'
```

`gpu_raster_parity` prüft das Plattformbackend, `gpu_raster_opengl_parity` das
explizite GL-Backend und `gpu_raster_opengl_copy_parity` dessen erzwungenen
GL-4.1-Kopierpfad. Sie vergleichen bei Punktabtastung gepackte Farbe inklusive
Alpha und Float-Tiefe bitweise gegen die Bandstufe, einschließlich Schattenphasen,
SSAA, NPOT-/mutierten Texturen und Kontextwechseln. Jeder erwartete GPU-Pass muss
wirklich auf der GPU abgeschlossen sein; Softwarefallback zählt nicht als PASS.
Ohne Gerät/Display enden sie mit 77 (SKIP); Shaderfehler sind Fehler, keine SKIPs.
Ein versteckter GL-Kontext braucht weiterhin einen Displayserver. GPU-Tests in
einer Sandbox ohne Display belegen weder Shaderkompilierung noch Bildparität.

`recvx-gpu-compare` unterstützt `--backend opengl` (am Ende) und anschließend
optional `--force-copy`; Punktabweichungen oder Softwarefallback liefern einen
Fehlerstatus. Gefilterte Hostqualität wird getrennt ausgewertet, nicht als
CPU-bitgleich vorausgesetzt. Messungen liefern CPU- und Wallzeit; ohne realen
GPU-Lauf gibt es keinen Leistungsnachweis.

Tests mit Originaldaten erwarten `data/`. Tests mit gemeinsamen Fixtures
(z. B. Snapshot) nicht parallel in mehreren Buildverzeichnissen starten.

| Bereich | Gruppen |
|---|---|
| Daten und Grundlagen | `expand`, `archive_room`, `math`, `matrix`, `texture_filtering`, `hd_texture_content_key` |
| Darstellung | `scene_renderer`, `original_chunk_blend`, `original_view_clip`, `original_face_geometry`, `active_camera_return` |
| Spieler und Bewegung | `player_motion_skin`, `wall_collision`, `player_push`, `core_continuity`, `script_motion` |
| Kampf und Gegner | `combat_weapons_ammunition`, `ammunition_pickup_zero`, `enemy_damage_original_en01`, `effect_collision_debris`, `effect_pool_lifetime` |
| Events und Ablauf | `native-gameplay-bindings`, `event_lifecycle_control_release`, `engine_progression_resources`, `door_transition`, `gameover_original`, `computer_terminal` |
| Inventar und Items | `inventory_events_assets`, `inventory_combine_inspect`, `original_inventory_table_coverage`, `original_file_reader`, `status_condition_pulse` |
| Audio und Filme | `audio_movie`, `bgm_room_transition` |
| Eingabe, Menüs, Host | `keyboard_controller_config`, `main_menu_settings_video_skip`, `debug_menu_hotkey_registry`, `snapshot_savefile_roundtrip`, `third_person_camera_mod` |

[INVENTORY-COVERAGE.md](INVENTORY-COVERAGE.md) wird mit Originaldaten erzeugt:

```sh
build/macos-arm64/recvx-inventory-engine-tests data docs/INVENTORY-COVERAGE.md
```

## Prüfskripte

Die Skripte nehmen `--runtime <binary>` und `--data <ordner>`, einige
zusätzlich `--output <ordner>`, und laufen headless. Diagnostische Aufstellungen (Position, Snapshot) sind als solche
gekennzeichnet; Raumskripte und Ressourcen bleiben unverändert.

| Skript | Prüft |
|---|---|
| `verify_frontend.py` | Hauptmenü, Filmskip, Feuerzeug-Ablauf |
| `verify_events.py` | Friedhof-/Hof-Tasks und Steuerungsübergabe |
| `verify_event_state.py` | VSync-Zeiteinheiten mit Menü und Debugpause |
| `verify_event_freeze.py` | Treppen, Fallenevent und Tür nach Freigabe |
| `verify_progression.py` | Metalldetektor- und Box-Skripte |
| `verify_detector.py` | Alarm, Schalterfrage, metallfreier Durchgang |
| `verify_scanner.py` | 3D-Scanner-Terminal und Deckelanimation |
| `verify_continuity.py` | Sicherheitsraum, Treppen, Rückwege |
| `verify_doors.py` | Türwege, Skip, Inventarerhalt |
| `verify_room_spawn.py` | Reihenfolge beim Raumladen |
| `verify_bgm_rooms.py` | Musik beim Verlassen, normale und übersprungene Tür |
| `verify_audio_state.py` | Event_init → RoomSoundCase → Soundbank |
| `verify_combat.py` | Messeraufnahme, Ausrüstung, Kampf |
| `verify_enemy_ai.py` | Zombie-Verfolgung, Griff/Biss, Hebelrütteln, Tod → Game Over (RM_0080) |
| `verify_ammunition.py` | Munitionsaufnahme mit leerer Pistole |
| `verify_pickups.py` | Karten- und breite Waffenaufnahmen |
| `verify_pickup_slide.py` | Aufnahmefrage ignoriert Eingaben während des Einfahrens |
| `verify_item_catalog.py` | Handbuch → `EraseItem` → Munition, Debug-Dateien |
| `verify_file_pickup.py` | Geskriptete Dateiaufnahme und Leseabschluss |
| `verify_file_folder.py` | Dateiordner über FILE und Handbuch |
| `verify_push.py` | Kistenschieben |
| `verify_crate_edge.py` | Kistenkanten und Abstieg |
| `verify_gameover.py` | `GameOverSet` und Retry |
| `verify_snapshot.py` | Snapshot speichern und in neuer Runtime laden |
| `verify_debug.py` | Debugmenü und Hotkeys |
| `verify_rooms.py` | Raumscan (siehe unten) |

Beispiel:

```sh
python3 tools/verify_frontend.py --runtime build/macos-arm64/recvx-runtime --data data
```

## Raumscan

```sh
python3 tools/verify_rooms.py --runtime build/macos-arm64/recvx-runtime --data data
```

Startet alle 205 Räume ohne Eingaben. Ladefehler lassen die Prüfung
scheitern; blockierte Event-Befehle werden als Fortschrittsbericht
ausgegeben.

## Weitere Prüfungen

- **Sanitizer:** eigenes Buildverzeichnis `build/native-sanitize` mit
  ASan/UBSan (macOS ohne LeakSanitizer).
- **Windows:** Crossbuild unter `build/windows-x64`; Tests unter Wine mit
  `WINEPREFIX=build/wine-native`. Physische Windows-Hardware und Controller
  werden damit nicht geprüft.
- **Pakete:** Replays gegen Paket-Runtime und Paketdaten; Benutzer-INIs und
  Snapshots dürfen sich dabei nicht ändern.

Logs und Ansichten einzelner Prüfläufe liegen lokal unter
`build/validation/`.

## Entscheidungen zu undefiniertem Originalverhalten

- **ATR-Zylinderdeckel bei abwärts gerichteten Linien** (Nutzerentscheidung
  2026-10-11). `bhCollisionCheckLineMain` (`hitchkl.c:350–365`,
  0x26A418..0x26A4CC) schreibt pro Schleifendurchlauf einen Deckelpunkt und
  testet sofort ein 8-Eck. In den Durchläufen 0..6 stehen also noch die
  letzte Seitenfläche (area[1..3]) und nie geschriebener Stack (area[4..7]).
  Durchlauf 0 trifft wegen area[0]==area[1] (Nullnormale) nie. Festgelegt
  ist: Nicht geschriebene Punkte umschließen den Schnittpunkt nie. Damit
  entscheidet nur das vollständige 8-Eck aus Durchlauf 7, danach gilt
  `p2->y = area[0].y`. Das stimmt mit dem Original überall überein, wo dessen
  Ergebnis nicht vom Stackinhalt abhängt. Abweichen kann es nur bei einem
  Schnittpunkt außerhalb des 8-Ecks, für den das Original abhängig vom
  Stackinhalt einen Phantomtreffer liefern könnte. Test:
  `native_line_reflection` (Mitte, innerhalb/außerhalb des 8-Ecks, Ecke).
  Vorher brach jede solche Abfrage die Runtime ab, z. B. die en07-Sichtlinie
  `bhEne07_SearchPlayer` in RM_2010.

## Live-Effekte Mündungsfeuer/0x49 (2026-10-11)
Details: work/native_port_live_effects.result.md. Build build/live-effects-20261011, Policy PASS, 13 CTest-Gruppen, verify_combat (Build und Paket), Pixelabnahme Frames 3502/3562 gegen Kontrollbuild (nur Flash-Draw aus). Paket arm64 SHA256 a0adeb29…1a9c, signiert. Windows crossgebaut, nicht ausgeführt.

## Gemeinsamer Effektpool / aufsteigender Pass (2026-10-11 abends)
Details: work/native_port_live_effects.result.md, Abschnitt „Nachtrag“.
Sprites, Mündungsfeuer, Partikelwurzeln/Zusatzarbeiten und Blutlachen liegen in
einem `eff[512]` (effsub3.c:67/92/145); der Effektpass läuft aufsteigend und
übernimmt die Spawn-Ausgabe nach jeder Prozedur (effect.c:742), Tickgate
`(sp_flg & 8) || (ef_flg & 1)` (effect.c:750), Zeichengate `pt_flg & 0x10`
(game.c:339) plus Spiegelterm `gm_flg & 0x4000 && mdflg & 0x40` (effect.c:1301);
0x49 löst den Empfängerjoint vor dem Ton auf. 62/62 CTest sequenziell,
verify_combat --runtime-only gegen Build und installierte Runtime, Pixelabnahme
3502/3562 je 570 Pixel in (163,244)–(213,266) bei identischem Zustand, über
Start.command reproduziert. Neue produktive Regression (RM_0090 bhEff125→126 im
selben Pass) ist gegen den gesicherten Vorherstand rot. `native_lighter_effect`
wurde nicht abgeschwächt: Ursache gemessen (drei bhEff182-Kinder zeichnen jetzt
im selben Bild), Test sucht die Flamme über ihren Atlaseintrag und ist in beiden
Ständen grün. Ausgelieferter Starter arm64 SHA256 f77140f4…fa23,
signiert/verifiziert; Backup
archived/releases/live-effects-shared-pool-before-20261011-203905 (c9a8de35…b190).
Windows x64 crossgebaut (SHA256 d86a54fc…1917), windows_link_audit bestanden,
nicht ausgeführt. Offen: ef_flg/pt_flg haben keinen nativen Schreiber, mdflg wird von
keiner Effektprozedur gesetzt, Partikelerzeuger nur über den quarantänierten
en01-Kern erreichbar (kein produktiver Blutspawn geprüft),
CallPlayerWeaponSeEx-Slot/Position bleibt gemeldete Lücke.

## Gegner-Angriffe: Hunde und Spielermotion im Griff (2026-10-12 abends)
Details: work/native_port_enemy_ai.result.md, Abschnitt „Nachtrag“. Build
build/enemy-ai-claude-20261011, Policy PASS. `tools/verify_enemy_ai.py` PASS
(build/validation/enemy-ai-20261012/verify-final): RM_0080 Zombies, RM_0060/RM_0031/RM_3070
natürlich, RM_1000 Hunde natürlich (Griff/Freigabe/Game Over), RM_0050 Falle (Diagnose-Snapshot).
76/77 CTest seriell (rot: shadow_modifier_pixels, parallele untracked Schattenarbeit, Baseline
identisch). Installiertes Starter-Binary arm64 SHA256 44bb6154…1743, signiert/verifiziert, Smoke über
Start.command (headless und Standardlauf mit SDL-Dummy); Backup
archived/releases/enemy-ai-dogs-before-20261012-204915. Windows x64 crossgebaut
(64db1186…0c06), Policy mit Builddir PASS, nicht ausgeführt. Logs:
build/validation/enemy-ai-20261012/dogs.

## Alpha-Domäne der Texturen, Punkt 1 des Transparenzplans (2026-10-13 nachts)
Eine Host-Domäne für Textur-Alpha (0..255, Faktor alpha/255 = originales As/128): Dekoder
ohne Ps2AlphaIsHalf-Halbierung, `write_tga` ohne ×2 (Export/Import verlustfrei), Renderer
mit einem Teiler für alle Blendarten, die vier handgeschriebenen ×2-Verdopplungen in
ui/computer/world entfernt. Details: work/native_port_alpha_domain.result.md.
CTest 79/81; rot `script_motion` (vorbestehend, mit
dem Binary von 22:00 in build/mapinput-claude-20261013 mit identischer Meldung belegt) und
`original_map_render` (neue, unfertige Parallelarbeit der Kartensitzung: 11 Füllungen statt
1, 0 Objekte, Licht 0 — keiner der Fehlschläge auf dem geänderten Pfad). Neuer Fall
`chunk_blend_test` (TGA-Alpha 0x80 → halbe Mischung) mit Gegenprobe gegen eine Kopie des
Renderers mit dem alten Teiler. verify_detector/-lighter/-scanner/-computer/-frontend/
-progression/-pickups/-events/-doors/-stairs/-combat/-snapshot/-continuity/-fog PASS,
Raumscan 205/205 lädt/0 blockiert, check_no_original_core PASS.
Dev-App über tools/build_native.py aktualisiert (build/macos-arm64, arm64-only, signiert und
verifiziert, SHA256 a55478ed…23d6); Smoke mit den App-Optionen inklusive `--mods` (HD-Paket)
über SDL-Dummy: 30 presents, kein Fehler. Bildvergleich RM_0090 Kamera 1/2 mit und ohne
HD-Paket: dieselbe Änderung (776 bzw. 242 Pixel Delta ≥ 8). Backup
archived/releases/alpha-domain-before-20261013-224118; die Dev-App selbst behält den
vorherigen Stand unter build/start/macos-arm64/.previous-*. Der Build enthält den
Arbeitsstand der parallelen Karten-/Schatten-/Fog-Sitzungen. Kein Paket, kein Commit/Push.

## Blendzustand der Original-Engine (Punkt 2 des Transparenzplans, 2026-10-13)
njColorBlendingMode/njColorBlendingModeSys als Zustandsautomat in native/chunk_blend.cpp
(nj- und gs-Save getrennt, Index 0/1 -> 0x01/0x44 ohne Tabellenzugriff, Paar mit Index < 2
laesst das Register stehen, ungueltige Paare als `blend_unavailable` in --state statt
Abbruch); gebunden an Effektsprite-, Partikel- und Muendungsfeuerpfade, ID-Whitelist und
ninja_blend_register entfallen. Details: work/native_port_blend_state.result.md.
CTest 79/81 (rot: script_motion vorbestehend,
original_map_render Parallelarbeit). verify_lighter/combat/detector/scanner/doors/
continuity/events PASS; verify_enemy_ai identisch zum Vorher-Binary rot (RM_0080 „kein
Biss", fremde Sitzung). Gemessen: RM_0090 Kamera 1/2 und die Lighter-Szene sind gegen das
Runtime von 22:41 pixelgleich (0 von 307200, Delta 0) — der Mechanismus ist ersetzt, die
sichtbaren Effektluecken liegen in den noch fehlenden Effektprozeduren (Kategorie + Paar).
Kein Paket, keine Dev-App, kein Commit.

## Effekt-Skriptbefehle: Blutlache 0x56/0x58, Dev-Auslieferung Punkt 2 (2026-10-13)
Dev-App build/start/macos-arm64 mit Punkt 2 ersetzt und signiert (Hash 2766db10…, Backup
archived/releases/blend-state-before-20261013-233239 = a55478ed…); Laufzeit-Suiten gegen
genau dieses Binary gruen, blend_unavailable in acht echten Raeumen leer, CTest 80/82 (rot
script_motion vorbestehend, original_map_render Parallelarbeit — verify_map scheitert mit dem
Vorher-Binary identisch). Danach 0x56 bhEffBloodPoolSet neu gebunden (Laenge 6) und 0x58
bhEffBloodPoolSet2 von „effect spawn pending" auf die Originalbindung umgestellt
(native/world.cpp, Tabellen evnt_BldTbl/evnt_BldTbl2 feldgleich als RECVX_ENEMY_BP_WORK);
der Poolmechanismus selbst (effsub3.c:145/234) war schon portiert. Nachweis
tests/native/gameplay_bindings_test.cpp (Bindung true, blood_particles +1 je Befehl,
epw->djnt_no) und gemessen RM_3140: 3 offene Bindungen inklusive 0x58 vorher, 2 nachher.
Gescannt (tools/scan_event_rooms.py, 205/205): weiter ohne Implementierung sind 0x2c
bhEffectSet, 0x4e bhEffBloodSet, 0x70/0x71 bhEffPush/bhEffPop, 0x96/0x97/0x98, 0xaa, 0xb0,
0xc3, 0xc8. Ebenfalls gemessen: im Scanner-Replay laeuft kein Effekt-Skriptbefehl, die
RM_0090-Fraeseffekte gehoeren zum Terminaltask/Raumobjekt, nicht zu sys->ef_*. Zusaetzlich
0x4e bhEffBloodSet (Laenge 14) gebunden: RoomWorld::script_blood_set schreibt epw->djnt_no
und ruft den portierten Kern zonzon.c:466 bhEne_SetBlood2; der T3-Dienst und der Weltzeiger
werden fuer genau diesen Aufruf gebunden (EnemyExtScope wie in control_enemy_works), sonst
meldet die Bibliothek "[zonzon.c:493 effect.c bhSetEffectTb]" als Luecke (gemessen).
CTest 80/82 unveraendert. Details: work/native_port_effect_script.result.md.
Danach die restliche Effekt-Skriptflaeche: 0x2c bhEffectSet belegt den ersten freien Platz des
geteilten eff[512]-Satzes (RoomWorld::allocate_effect) und meldet eine noch fehlende Prozedur
namentlich, statt die Eventaufgabe abzubrechen; 0x70/0x71 bhEffPush/bhEffPop sichern bzw.
ersetzen den ganzen Satz (sprites_ plus Partikelnutzlast, EffectParticles::save/restore) wie
bhPushEffectWork/bhPopEffectWork. Nachweis tests/native/gameplay_bindings_test.cpp. Dev-App
ersetzt/signiert (70cf7f60…), alle zwoelf verify-Suiten gegen genau dieses Binary PASS;
CTest rot nur script_motion (fremd, vorbestehend) und engine_progression_resources (ItemBox
Listen-Scroll, laufende Box-Sitzung). Ein Dev-Build um 23:57 enthielt deren halbfertigen Stand
und liess verify_progression rot werden; die Gegenprobe mit und ohne meine Befehle besteht die
Suite, sobald die Nachbardateien ihren naechsten Stand haben. Kein Paket, kein Commit.
