# Actor-/Asset-Port, 2026-10-09 (Codex)

Auftrag: alle Gegner zusammen auf die originalen nativen Actor-/Asset-Works
umstellen, danach gesammelt testen und gemeinsame Portierungsursachen beheben.
Keine PS2-Nachbildung, kein ILP32-/wasm2c-Kern, kein künstlicher Spawn oder
Gegner-/Raumsonderfall. Fremde lokale Änderungen erhalten; kein Commit/Push.

## Produktiver Pfad

`RoomWorld` entscheidet den Besitzer vor der Initialisierung. Alle originalen
Nicht-Subplayer-Gegner, einschließlich Zombies (1) und Hunde (4), verwenden
`core_enemy_world_control` → originales `bhControlEnemy`/`bhJumpEnemy`.
NPC-/Subplayer-Identitäten verwenden weiterhin ihren bestehenden nativen Port.

MDL-/MTN-/SKIN-Daten werden in eigenen nativen Speicher realisiert. Leere
MDL-Slots bleiben leer, Basic- und Chunk-Modelle bleiben verschieden. Der
Renderer iteriert die tatsächliche Work-Tabelle einschließlich originaler
Child-Works, statt nur Raum-Einträge zu zeichnen. Gewählte Modelle, Pose,
Matrizen, mutable Vertexdaten und Texturquelle kommen aus `mlwP`/`objP`/
`owP`/`texP`. Keine zweite Animationsfortschreibung auf den Core-Works.

Originale Common_controll-Fälle für Motion, Transform, Interpolation und Flags
werden aus event.c erzeugt und auf die gemeinsame Work angewendet. Render-Gate
0xa5 schreibt das originale mdflg-Bit 0x200. Auch beim Gegnergriff liest der
Renderer die tatsächliche Player-Work, nicht eine angenommene EN01-Bibliothek.

## Gemeinsame Ursachen und Belege

- Gepackte EXP-Zeigertabellen haben vier Byte große Slots. Native acht Byte
  große C-Zeigercursor überschrieben Skalarfelder bzw. lasen zwei Handles als
  einen Zeiger. Der Generator behandelt lokale Cursor, indizierte Zugriffe,
  übergebene Arrays und zurückgegebene Cursor anhand der Originalquelle.
  Reproduzierte Fälle: EN03-Init, EN13 Store/Restore, EN13B-MV01/GetChild.
- Room-/CUT-/Light-/POS-/Route-, Item-/Object- und Player-Works waren unvollständig
  gebunden. Originaler Player-Init und Gegner-Init vor dem Eventpass sowie die
  originalen Tabellen beseitigen die nachfolgenden Kollisions-/Link-Abstürze.
- Mutable Dispatch-Tabelle und Kamera werden pro World gesichert. EN03/EN13
  dürfen nicht die Dispatchquelle später geladener anderer Räume verändern.
- Die 22 historischen „nonfinite asset coordinate“-Ladefehler kamen aus MASK-
  Lip-Winkeln: `fmCnkSetParamLip` interpretiert die Bits von jawang/tangx/tangy
  als signed Integer. RM_5820 enthält z.B. jawang=0xffffffdc (-36) in Bank-Key 9.
  Das ist keine ungültige Koordinate. Die Winkelbits bleiben unverändert bis
  zur originalen Lip-Konvertierung; Koordinatenvalidierung bleibt bestehen.
- `bhSetLip` aktualisiert exp0[268], nicht den laufenden LIP_WORK.ptr.
  Ein aktiver Stream bleibt bis zur nächsten Aktivierung erhalten. Das war die
  Ursache des späteren RM_3230-Abbruchs; kein Zusatzterminator eingefügt.
- Originales EN01 samplet trotz Common-23-/MotionPauseSet-Flags weiter:
  ELF `bhEne01_SetMtn` 0x1868A0–0x186DE8 prüft das EXP-Flag 0x10, nicht den
  behaupteten Host-Pause-Gate. Tests wurden an diese belegte Semantik angepasst.

## Verifikation

- macOS arm64 Build von Runtime/Spielbibliothek erfolgreich.
- 48/48 CTest erfolgreich (letzter Gesamt-CTest: 49,97 s).
- Neue Regressionen: live Vertex-/Normaländerung, Work-Render-Gate und World-
  Isolation, negativer MASK-Lip-Winkel, Retarget eines aktiven Lip-Cursors.
- RM_5810 und RM_9302: jeweils 600 Ticks mit `--validate-poses`, kein Absturz.
- RM_2010: 600 Ticks mit `--validate-poses`, kein fester Zombie-Clip-Zugriff mehr.
- RM_3230: 600 Ticks mit `--validate-poses`, 11 originale Eventdispatches,
  kein Lip-Abbruch und keine blockierten Befehle.
- Originaler RM_0130-Replay: Spawn 1, 130 Ticks, jede Pose geprüft, sichtbare
  Fledermäuse. Bild `build/validation/actor-assets-20261009/bats.png` geprüft.
- Gesamter produktiver Raumsweep: **205/205 Räume laden; 200/205 beenden
  600 Ticks**. Kein Prozessabsturz. Fünf Läufe enden beim BGM-Dateizugriff:
  RM_9091/RM_9370 verlangen BGM1/0122.bin, RM_9340/RM_9360 BGM1/0124.bin,
  RM_9350 BGM1/0121.bin. Diese Dateien fehlen im aktuellen Datenbaum.
  Originale BGM-ID-/Bankzuordnung und Datenquelle sind weiter zu prüfen;
  keine erfundene Ersatzmusik oder Unterdrückung des Fehlers.
- Der Sweep meldet noch pdGetPeripheral (RM_1122) und StartVibrationEx
  (RM_2010/RM_5830). Sie werden nicht als erfolgreiche Bindungen ausgegeben.
- `verify_rooms.py` erfasst jetzt auch Prozess-Abstürze und Fehler nach dem
  initialen Laden. Der Sweep endet wegen der fünf BGM-Fehler bewusst mit Code 1.
- Start.command-Paket mit dem aktuellen arm64-Runtimebinary aktualisiert,
  signiert und Signaturprüfung erfolgreich; Paketdaten unverändert.
- Windows x64: `recvx_game` einschließlich Actor-/Asset-Code kompiliert. TIM2-
  Layout fest 64 Bit und doppelte Schnittstellenbesitzer korrigiert. Runtime-
  Link bleibt an weiteren Original-Render-/File-/Status-Imports offen; kein
  Windows-Laufnachweis, keine neuen Link-Stubs.

Messdaten: `build/validation/actor-assets-20261009/final-room-sweep-600.json`;
historische Zwischensweeps und separater Starter-Baseline-Sweep im selben Ordner.
CTest-Protokoll: `build/native/Testing/Temporary/LastTest.log`.

## Grenzen

Dies ist kein Durchspielnachweis oder Nachweis jeder Attacke/jedes Tod-/Griff-
Zweigs für alle Gegner. Morph-/Face-/Shadow- und gemeldete Plattformbindungen
sowie die Windows-Linkabhängigkeiten bleiben offen. Keine Vollständigkeits-
behauptung aus einem erfolgreichen Build, Unit-Test oder Raumsweep ableiten.
