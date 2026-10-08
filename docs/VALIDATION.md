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
ctest --test-dir build/native --output-on-failure
```

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
build/native/recvx-inventory-engine-tests data docs/INVENTORY-COVERAGE.md
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
python3 tools/verify_frontend.py --runtime build/native/recvx-runtime --data data
```

## Raumscan

```sh
python3 tools/verify_rooms.py --runtime build/native/recvx-runtime --data data
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
