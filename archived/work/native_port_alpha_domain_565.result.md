# Alpha-Domäne der gekochten TGA: 128 oder 255? (2026-10-20)

Auslöser: Ein Neukochen über `prepare_native_data.py --iso` ergab 18.164 von 18.253 TGA,
die sich nur im Alphakanal vom vorhandenen `data/` unterscheiden (alt 255, neu 128 bei
deckenden Pixeln; RGB identisch). `data/` wurde dabei nicht verändert.

## Ergebnis

**255 ist richtig.** Die TGA im vorhandenen `data/` (gekocht 2026-10-04/05, vor `b3727b4`)
sind korrekt. Der heutige Decoder/Exporter erzeugt für ARGB1555- und RGB565-Texturen den
halben Wert.

## Belege

Original `Ps2CheckTextureAlpha` (ps2_dummy.c:1739-1849), aufgerufen beim Einhängen
(ps2_texture.c:149), arbeitet nach `OrgColorType`:

| Typ | Original (ps2_dummy.c) | GS-Alpha deckend |
|---|---|---|
| 0 ARGB1555 | nur `Ps2AlphaIs000` (:1796-1801) | Rohwert 0x80 |
| 1 RGB565 | nur `Ps2AlphaIs000` (:1803-1808) | Rohwert 0x80 |
| 2 ARGB4444 | `Ps2Alpha4to8` + `Ps2AlphaIsHalf` (:1810-1817) | 0xF0 → 0xFF → 0x80 |
| 6 ARGB8888 | `Ps2AlphaIsHalf` (:1819-1825) | 0xFF → 0x80 |

`Ps2AlphaIsHalf` ist `(a+1)>>1` (:1714-1722). `ClutChange` ist in allen 17.193 TIM2 der
Raum-, Spieler- und ITEM1-Daten 0, die Umrechnung läuft also immer.

Gemessene Rohalphas der benutzten Pixel (40 Räume): RGB565 100 % 0x80, ARGB1555 55 % 0 /
44 % 0x80, ARGB4444 überwiegend 0xF0. Die 16-Bit-Typen liegen auf der Disc also schon im
GS-Bereich 0..0x80.

Host-Konvention seit `b3727b4` (Ledger `Ps2CheckTextureAlpha`, renderer.cpp:399):
Blendfaktor `alpha/255` = `As/128`, also Host-Alpha = 2·As (gekappt auf 255).

- Vor `b3727b4`: Decoder halbierte 2/6, ließ 0/1 roh; `write_tga` schrieb `min(255, 2·a)`.
  Ergebnis für alle Typen 2·As → deckend 255. **Korrekt.**
- Seit `b3727b4`: Decoder halbiert 2/6 nicht mehr (Host = Rohwert ≈ 2·As, korrekt),
  `write_tga` verdoppelt nicht mehr — 0/1 bleiben aber roh = As. Deckend 128 → Faktor
  128/255 ≈ 0,5. **Falsch** für ARGB1555/RGB565.

Beispiel RM_0000 Textur 0: RGB565, 38 benutzte Paletteneinträge, alle 262.144 Pixel roh
0x80; alte TGA 255, Neuexport 128.

## Folgen

- Das laufende Spiel ist heute kaum betroffen, weil `apply_texture_files` die alten,
  korrekten TGA aus `data/` bevorzugt (RM_0020 in-game: 303 von 307.200 Pixeln anders).
- Betroffen sind jeder Neuexport (`--source`, `--iso`) und alle Texturen, die ohne
  gekochte Datei direkt aus dem Original dekodiert werden.
- Der Bericht `archived/work/native_port_alpha_domain.result.md` („ein erneutes Kochen erzeugt
  dieselben Dateien") trifft für Typ 0/1 nicht zu.

## Korrektur (2026-10-20)

`native/scene.cpp`, Palettenkopie: Host-Alpha ist für alle Typen `min(255, 2·As)`.
Typ 2/6 laufen durch `Ps2AlphaIsHalf` (`(a+1)>>1`, ps2_dummy.c:1714), Typ 0/1 sind
bereits As. Der erste Schritt (nur Typ 0/1 verdoppeln) ließ beim Neuexport noch 1.592
Texturen mit ±1 im Alpha übrig, alle bei ungeraden Rohwerten von Typ 2/6 (z. B. 17 statt
18): das Original quantisiert dort auf As, der Decoder behielt den ungeraden Rohwert.

Prüfungen:
- `tests/native/scene_test.cpp`: jeder Farbtyp einzeln (deckend, teiltransparent,
  ungerade Werte, ClutChange gesetzt). Gegenprobe ohne Korrektur: rot.
- Neuexport über `prepare_native_data.py --iso` aus einem ISO, das aus `data/` gebaut
  wurde: **21.227 von 21.227 Dateien bytegleich**, alle 18.253 Texturen gleich. Damit
  bleiben auch die MD5-Schlüssel der HD-Texturpakete gültig.
- `data/` wurde nicht verändert.
