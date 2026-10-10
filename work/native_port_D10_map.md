# D10 — Kartenbildschirm (map.c) nativ portieren

Stand: 2026-10-10. Eigenständiger Arbeitsauftrag, disjunkt zu den laufenden
Paketen. Kann unverändert als Agentenauftrag eingefügt werden.

## Warum dieses Paket, Abgrenzung

Laufend und NICHT Teil dieses Auftrags: Gegnerpaket C01–C08 / T1–T5
(Orchestrierungs-Session, `native/en*`, `enemy_*`, `subplayer.*`,
`ninja_math.*`, `ninja_matrix.*`), Codex Kamera/Clip/Monitor/Eisraum (P04/G03),
B04 Haar, B01 Treppen, B06 Headwork. Diese Dateien und Themen nicht anfassen.

Der Kartenbildschirm ist nirgends portiert: Die Statusleiste meldet bei MAP
nur den Fehler-SE (`native/ui.h:62`, CLAUDE.md „Statusleiste“), AGENTS.md:2323
und `docs/NATIVE_GAMEPLAY.md:179` führen ihn als fehlend. Kein Ledger-Eintrag,
kein Bericht in `work/`, kein Besitzer.

## Pflichtlektüre und Regeln

`AGENTS.md` vollständig, `QUALITY.md`, `docs/ORCHESTRATOR_HANDOVER.md`
(Regeln). Vor Arbeit und vor jedem Build `python3 tools/check_no_original_core.py`.

- `archived/FORBIDDEN_DONTUSE` niemals lesen, durchsuchen, kopieren oder
  indirekt nutzen.
- `src/ps2/veronica/prog/map.c` (3028 Zeilen), `map.h`, `system.c` und das
  Original-ELF sind AUSSCHLIESSLICH Semantikreferenz. Keine Funktionskörper
  kopieren, extrahieren, generieren, kompilieren, linken oder aufrufen.
  Reine Datentabellen (MapPal, MapCol, ItmDat, CncDatA/B u. ä.) dürfen als
  native Daten übernommen werden, mit Regressionstest gegen die Quelle.
- Kein zweites sys/plp/rom, keine Spiegelwelt. Flags (`bhFlagSet` Gruppen 8/9),
  Inventar (`sys->itm`), Raum-/Stage-Nummern und Player-ID kommen aus den
  vorhandenen nativen Besitzern (EventVm-Flags, Inventory, RoomWorld/Runtime).
- Keine Heuristik, geratene Konstante, Raumsonderregel, stiller No-op oder
  abgeschwächte Testerwartung. Ungeklärtes als benannte Lücke mit Datei:Zeile.
- Kein PS2-/GS-/VU-Nachbau: Sprites, Linien, Flächen und Polygonfüllungen
  (`MapDrawLine/Fill/PolyFill/Sprite/Background`) auf den vorhandenen nativen
  Scene-/2D-Ausgabepfad abbilden, ohne `renderer.*`/`scene.h` zu ändern.
  Fehlt dort ein Primitive, als Anschlussvertrag melden.
- Gemeinsamer Arbeitsbaum: nichts zurücksetzen, stashen, löschen. Kein Commit,
  Push oder Paket ohne ausdrücklichen Nutzerauftrag.

## Dateibesitz

Exklusiv neu: `native/map_screen.*` (ggf. `native/map_tables.*` für Daten),
`tests/native/map_screen_test.cpp`, `tools/verify_map.py`,
`work/native_port_D10_map.result.md`.

NICHT direkt ändern: `CMakeLists.txt`, `native/world.*`, `native/runtime.cpp`,
`native/events.cpp`, `native/renderer.*`, `native/scene.h`, `native/ui.*`,
`native/input.*`. Nötige Anschlüsse (CMake-Ziel, CTest, Runtime-Task,
MAP-Knopf in `StatusMenu`, Eingabe über `modal_input`) als kleinen Patch mit
Schnittstellenvertrag an den Integrator übergeben.

## Umfang (vollständig, Original-Reihenfolge)

1. Einstiege und Lebenszyklus aus `system.c:785–879`: `MP_SET_EVENT`,
   `MP_SET_GAME`, `MP_SET_SUB`; `bhInitMap` → `bhSetMap` → `bhControlMap` bis
   Ende → `bhExitMap` inkl. `sys->gm_flg &= ~0x80000`, Texturrettung/-rückgabe
   (`rom_texP`, nativ als Ressourcenerhalt, kein Speicherkopie-Nachbau),
   `mp_prm[0..3]`, `dor_partid`, SE 3 beim Öffnen.
2. Datenladen `bhReadMapData` (RequestReadInsideFile über `prti_no`): Quelle
   in `data/archives` belegen, Format dokumentieren, nativ parsen.
3. `MapCnvStatus2Flag`: Flaggruppe 8/9 aus Player-ID und Inventar über
   `ItmDat` — auf den vorhandenen Flag-/Inventory-Besitzern.
4. Steuerung `bhControlMap`, `MapPadMain`, `MapViewMain`, `MapLightMain`,
   `MapPaletteMain`, `MapCodeProcess`, Etagenwahl `MapGetFloorNo`.
5. Task-/Funktionssystem (`MapFunc*`, `MapEntryTask`, `Ftsk*`: Wait/Exit/Read/
   Normal/Zoom) als native Zustandsmaschine mit identischer Reihenfolge.
6. Darstellung: Marker, Silhouette, Pfeile (`FsprArrowDraw/2`), Gauges
   (`FsubGaugeDrawX/Z`), Zoom-C‍ursor/-Info/-Screen, Kompass, Modusmeldung,
   Raumnamen (`MapDrawMessage`), Tag-/Verbindungslogik (`MapTag*`, `MapCnc*`,
   `MapCheckNextMap`) einschließlich Tür-/Raumverbindungsstatus.
7. SFX der Karte über den vorhandenen `AudioMixer` (System-SE), keine Ersatztöne.

## Abnahme

- Komponententest: Datentabellen gegen Quelle, Flagumrechnung (alle Player-IDs,
  Item vorhanden/fehlend), Verbindungsstatus, Etagenwahl, Taskfolge Open →
  Normal → Zoom → Exit mit Flag-/SE-Folge, Nichttrefferfälle.
- Runtime: `tools/verify_map.py` öffnet die Karte über den echten Statusmenü-
  Pfad (MAP-Knopf) in einem Originalraum mit Karte im Besitz; Screenshot,
  Rückkehr ins Statusmenü, `gm_flg`-Freigabe. Ein Raum mit Kartenerwerb
  (z. B. RM_0060-Gefängniskarte, it_flg 133) als Event-Einstieg prüfen.
  Debug-Positionierung ausdrücklich kennzeichnen.
- Bericht trennt: implementiert / produktiv angeschlossen (erst nach
  Integrator-Patch) / komponentengeprüft / Runtime-geprüft / Windows.
  Offene Lücken (z. B. fehlende 2D-Primitive) namentlich mit Datei:Zeile.
- Mac arm64 Build in eigenem Buildverzeichnis, keine parallelen Builds im
  Verzeichnis von Peers; Policy-Prüfer gegen Quellen und genutztes Binary.
