# R02 und erster Teil D09 — Ergebnis

Stand: 2026-10-09. Auftrag: `docs/CLAUDE_NEXT_TASKS.md`. Kein Commit, kein Push.
Quarantäne `archived/FORBIDDEN_DONTUSE` nicht gelesen und nicht benutzt.

## R02 — Erzeugerrest geschlossen (abgeschlossen)

Der Auftrag nannte nur den ADX-Pfad. Die Prüfung ergab, dass **drei** Erzeuger
beim Build liefen und dabei die PS2-Quellen lasen:

| Erzeuger | Ausgabe | Inhalt |
|---|---|---|
| `generate_native_tables.py` | `adx_tables.inc` + 3 gesperrte Dateien | Mischgenerator |
| `generate_chunk_blend.py` | `chunk_blend.inc` | reine Daten |
| `generate_combat_tables.py` | `combat_tables.inc` | reine Daten |

Nach der Nutzerregel (2026-10-09) ist ein Erzeuger, der bei jedem Build aus
PS2-Source zieht, unzulässig — **auch für reine Daten**. Alle drei sind aus dem
Build entfernt; ihre Werte liegen einmalig eingefroren als native Module:

- `native/adx_tables.c/.h` — 128 ADX-Dämpfungsstufen
- `native/chunk_blend_data.inc` — GS-Blendindizes und Alpha-Tabelle
- `native/combat_tables_data.inc` — `WpnTab` und Waffentabellen

**Verifikation:** Alle drei byte-identisch gegen Original und vorherige
Generatorausgabe geprüft (Skriptvergleich der Werteliste, nicht nur Dateigröße).
Das Verzeichnis `build/.../generated/` existiert nach dem Neubau **nicht mehr** —
beim Build wird nichts erzeugt.

Die vorübergehende Lösung „erzeugen, danach löschen" ist entfernt. Sie war kein
Schutz: die Funktionskörper entstanden trotzdem.

### Prüfer ergänzt

`tools/check_no_original_core.py`:
- `generate_native_tables.py` in `BODY_GENERATORS`
- `status_pulse.inc`, `combinations.inc`, `ui_tables.inc` in `BODY_INCLUDES`
- **Negativfall** „erzeugen, danach löschen" wird erkannt und benannt
- **Strukturregel**: jeder `tools/(generate|build|extract)_*.py` als `COMMAND`
  ist ein Verstoß, unabhängig von der Ausgabe. `DEPENDS` allein zählt nicht,
  weil es nichts ausführt.

`tests/port_policy_test.py`, drei neue Fälle (24/24 grün):
- Mischgenerator ist nicht „data-only"
- erzeugen-dann-löschen wird abgelehnt
- Build-Zeit-Extraktor wird auch für reine Daten abgelehnt
- eingefrorenes natives Datenmodul bleibt zulässig

## D09 Teil 1 — Mengenanzeige nativ implementiert

`GameUi::quantity` (`native/ui.cpp`) ist eigenständig nativ geschrieben, nach
der Semantik von `BulletSet` (sub1.c) als Referenz. Kein kopierter Körper, kein
Generator.

Umgesetzte Originalregeln, mit Fundstelle `sub1.c BulletSet`:
- Anzeigeklasse aus `itemdata[].type & 0x1800`: `0x800` dreistellige Stückzahl,
  `0x1000` Prozentwert `menge / (max/100)` plus Prozentglyphe
- angefangene Ladung unter 1 % zeigt 1, nicht 0
- gepacktes Flagbit `0x800` (unendlich) zeigt nur die Sonderglyphe 11
- führende Nullen entfallen; letzte Stelle immer sichtbar; Prozentklasse
  behält ab der vorletzten Stelle alles, damit „0 %" nicht zusammenfällt
- Spaltenabstand 10 px, Höhe 32 px im Menü / 19 px in der Box
- Farbbank: `type & 1` wählt über Flagbits `0x7000` die Munitionssorte
  (Item 6 vier Varianten, Item 7 zwei, sonst Standard); `type & 0x56` hat feste
  Banken für 16, 17/36 und 37

### Befund aus den Originaldaten

Beim Prüfen der Testannahmen: es gibt **31 Items der Stückzahlklasse `0x800`**
mit `max` zwischen 1 und 30, **6 der Prozentklasse `0x1000`** (`max` 100–400)
und **keines** mit beiden Bits. Dreistellige Stückzahlen kommen also in den
Originaldaten nicht vor; die dritte Stelle ist nur über die Prozentklasse
erreichbar. Mein erster Test setzte Mengen über `max` und war damit kein
Originalfall — korrigiert.

### Test

`tests/native/ui_quantity_test.cpp`, CTest `ui_quantity`. Geprüft wird die
**sichtbare Wirkung** über die öffentliche `GameUi::draw`, nicht ein
Rückgabewert: ein- gegen zweistellige Menge ergibt mehr veränderte Pixel
gegenüber demselben Menü ohne Item, die Unendlich-Glyphe ergibt ein anderes
Bild als eine Zahl, und zwei Munitionssorten von Item 6 unterscheiden sich in
der Palette. Der Gesamtpixelzähler war untauglich (deckender Hintergrund);
verglichen wird gegen ein Referenzbild.

## Stand nach diesem Paket

- Configure und Build aus **leerem** Verzeichnis: exit 0, kein `generated/`
- Prüfer Quellen + Build + Binary: **PASS**
- **42/42 CTest**
- **205/205 Räume laden, 0 blockierte Eventbefehle**

### Getrennt nach Kategorie

**Positive Gameplay-Wirkung (neu):** Mengenanzeige im Inventar und in der
Itembox — Munitionszahlen, Prozentanzeige, Unendlich-Zeichen, Farbbank je
Munitionssorte.

**Lückenverträge (unverändert geprüft):** alles, was die Testsuite als
dokumentierte Lücke prüft — Türen, GetItem/ItemSort/ItemUse/Kombination,
Itembox-Transfer, Effekte, Gegnerschaden, Zombie-KI, EKG, Treppen, Push,
Targeting, Dateiordner, Game Over, Clip-Ebenen.

## Offen aus diesem Auftrag

- **D09 Teil 2:** `TrigerSet`/`original_command_parts` — das Itembefehlsfenster
  zeigt weiter keine Einträge.
- **D08:** EKG-Automat (`native/status_pulse.cpp`) vollständig offen.
- **D02/D07/D01/D06:** Inventar. **Nicht begonnen**, weil der Auftrag dafür
  vorab abgestimmte Feld-/Flag-/Slotverträge mit dem Integrator verlangt und
  `events.cpp` nur der Integrator ändern darf.
- **Windows x64:** in diesem Paket **nicht** gebaut und nicht gelinkt. Der
  macOS-Erfolg ist kein Windows-Nachweis.
- Kein Durchspielnachweis. Das Spiel lädt und läuft, ist mit den offenen
  Lücken aber nicht spielbar.

## Nicht angefasst

`native/face.*` und der Lip-Tick in `world.cpp` (policy-konform, F04a/F04b beim
Actor-Besitzer), der `size_t(-1)`-Guard in `world.cpp`, BGM-Regel, Math,
WeaponFlash, aktiver Lip-Cursor, `events.cpp`, gemeinsame World-/Runtime-Dateien.
