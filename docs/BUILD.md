# Build: was `./build.sh` macht, welche Ordner es liest und schreibt

Kurzfassung:

| Ordner im Projekt | Inhalt | wer schreibt ihn | wer liest ihn |
|---|---|---|---|
| `data/` | ungepackte gekochte Spieldaten (TGA Typ 2) | `./build.sh --iso <image>` (nur wenn `data/` fehlt) | Dev-App, Tests, Pakete ohne `--compressed` |
| `data_compressed/` | dieselben Daten verlustfrei gepackt (Farbtabellen-TGA, neu gepackte Filme) | `./build.sh --compressed` (nur wenn er fehlt; aus `data/`) | Pakete mit `--compressed`, tvOS immer |
| `mods/hd-source/` | **Eingang** fuer HD-Pakete als DDS (PCSX2 oder Dolphin, beliebige Unterordner) | du | `./build.sh` (HD-Import) |
| `mods/textures/hd/` | HD-Texturen als TGA, Name = MD5 der gekochten PS2-Textur | `./build.sh` (HD-Import) oder du | Dev-App zur Laufzeit, Paketbau (eingebacken) |
| `mods/textures/<Raum/Gruppe>/<n>.tga`, `mods/rooms/`, `mods/doors/` ... | Ersatz je Datei, siehe [NATIVE_ASSETS.md](NATIVE_ASSETS.md) | du | Dev-App zur Laufzeit, Paketbau (eingebacken) |

## 1. Entwicklung: die Dev-App laedt alles LOSE

`./build.sh` (ohne `--package`) baut die Runtime und aktualisiert
`build/start/macos-arm64/RECVX Native.app`, die `Start.command` startet. Diese App ist
absichtlich nur ~9 MB gross: **sie enthaelt keine Daten**. Ihr Starter uebergibt

```
--data <Projekt>/data --mods <Projekt>/mods --controls <Projekt>/controls.ini --debug-config <Projekt>/debug.ini
```

Sie liest also die Dateien direkt aus `data/` und `mods/`. Eine geaenderte oder neue
Mod-Datei wirkt beim naechsten Start (oder beim Raumwechsel bzw. mit dem Debugmenue-Eintrag
"Textur-Mods (HD)") **ohne neuen Build**. `data_compressed/` benutzt die Dev-App nicht.

Ausnahme: DDS-Pakete. Die Runtime liest keine DDS. Neue oder geaenderte Dateien in
`mods/hd-source/` brauchen einen Lauf von `./build.sh` (oder direkt
`python3 tools/import_hd_textures.py`), der sie nach `mods/textures/hd/` uebersetzt.

## 2. HD-Import: `mods/hd-source/` -> `mods/textures/hd/`

Jeder `./build.sh`-Lauf (alle Plattformen) prueft `mods/hd-source/`:

1. Jede `*.dds` mit PCSX2-Namen (`<TEX0-Hash>-[<CLUT-Hash>-]<bits>.dds`) oder Dolphin-Namen
   (`tex1_<w>x<h>_....dds`) wird auf die Originalgroesse aus dem Namen verkleinert und per
   Bildinhalt mit den gekochten Texturen derselben Groesse in `data/textures` verglichen.
   Die Hashes in den Namen sind Emulator-Speicherabbilder und fuer den Port nicht verwendbar.
2. Nur eindeutige Treffer werden genommen. Pro Treffer entsteht
   `mods/textures/hd/<md5>.tga` (4-fache PS2-Groesse, Farbe aus dem Paket, Alpha aus der
   PS2-Textur). Die DDS bleiben unveraendert in `mods/hd-source/`.
3. `mods/textures/hd-import.json` listet jede geschriebene Datei, jeden Treffer (DDS -> PS2-Datei,
   Anzahl gleicher Texturen in anderen Raeumen) und jede nicht verwendbare DDS mit Grund.
4. Ist nichts geaendert, wird nichts neu gerechnet ("HD import up to date"). Entfernte DDS
   entfernen beim naechsten Lauf ihre TGA wieder. Dateien in `textures/hd/`, die nicht vom
   Import stammen, werden nie angefasst.

Wichtig: ein Paket deckt nur die Texturen ab, fuer die es ein Gegenstueck gibt. Das
PCSX2-Paket mit 312 DDS ergab 108 Treffer (247 gekochte Dateien von 18.253) — der Rest des
Spiels bleibt bei PS2-Texturen. Welche Raeume betroffen sind, steht in `hd-import.json`;
die Runtime meldet je Raum `RM_xxxx.RDX textures: H HD, ...`.

DDS direkt in `mods/textures/` werden nie gelesen; Runtime und Build-Zusammenfassung warnen dann.

## 3. Daten aus der ISO, gepackte Daten

```bash
./build.sh --iso /pfad/game.iso                 # data/ anlegen (nur wenn es fehlt)
./build.sh --iso /pfad/game.iso --compressed    # zusaetzlich data_compressed/ aus data/
./build.sh --compressed                         # data_compressed/ aus vorhandenem data/
```

Beide Baeume werden nur erzeugt, wenn sie fehlen; zum Neuerzeugen den Ordner loeschen.
`--compressed <ordner>` waehlt einen anderen Zielordner.

## 4. Pakete und Apps: Daten werden mitgeliefert, Mods sind eingebacken

```bash
./build.sh --package [--output <neuer Ordner>]                # macOS/Windows: data/
./build.sh --package --compressed [--output <neuer Ordner>]   # macOS/Windows: data_compressed/
./build.sh --platform ios --ios-bundle-data [--compressed]    # iOS: Daten im App-Bundle
```

Ablauf beim Paketbau:

1. Quelle waehlen: `data_compressed/` mit `--compressed` (und auf tvOS immer), sonst `data/`.
2. Privaten Arbeitsbaum `build/bundle-data/<plattform>/` anlegen (Hardlinks, Quelle bleibt
   unveraendert). Vom Plattformprofil nicht gelesene Teile (iOS/tvOS: `files/MOVIE`,
   `archives/RDX_LNK`) fehlen darin.
3. **Mods einbacken**: `recvx-mod-bake` ersetzt im Arbeitsbaum genau die Dateien, die die
   Runtime aus `mods/` nehmen wuerde (Textur je Asset vor HD-Datei vor `data/`, Raeume,
   Spieler/Waffen, Tueren, Items, Filme, `COMMON.MLT`). Die MD5-Zuordnung der HD-Texturen wird
   dabei einmal beim Build gerechnet; bei gepackten Daten werden die eingesetzten Texturen
   ebenfalls gepackt. Ergebnis steht in `data/bundle-data.json` im Paket.
4. Diesen Baum ins Paket kopieren (macOS `RECVX Native.app/Contents/Resources/data`, Windows
   `data\` neben `Start.cmd`, iOS `recvx-runtime.app/data`). **Kein `mods/`-Ordner im Paket.**

Mod-Unterstuetzung zur Laufzeit:

| Plattform | Daten im Paket | Mods zur Laufzeit |
|---|---|---|
| macOS, Windows, Linux | `data/` oder mit `--compressed` `data_compressed/` | ja: ein selbst angelegter Ordner `mods` neben `Start.cmd` bzw. neben `RECVX Native.app` wird zusaetzlich gelesen |
| iOS/iPadOS | `data/` oder `data_compressed/` (nur mit `--ios-bundle-data`, sonst `Documents/data`) | nein (`RECVX_ENABLE_MODS=OFF`), nur eingebackene Mods |
| tvOS | immer `data_compressed/` (App-Groessengrenze) | nein, nur eingebackene Mods. **Es gibt noch kein tvOS-Buildziel**; das Profil (`tools/data_profiles.py`) legt die Regel fest. |

## 5. Was ein Lauf gemacht hat

Am Ende jedes `./build.sh`-Laufs steht eine Zusammenfassung, z. B.:

```
== RECVX build summary ==
Target:       macos
Data:         /…/data
HD import:    HD import up to date: 312 DDS in /…/mods/hd-source -> 108 HD textures in /…/mods/textures/hd
Package:      /…/build/packages/macos-arm64-v0.5
Package data: /…/data_compressed (compressed) copied into the package; mods: Baked mods from /…/mods: 0 per-asset textures, 247 textures from 108 HD files, 0 other files (textures compressed); no mods folder shipped
Runtime:      /…/build/macos-arm64/recvx-runtime
Dev app:      /…/build/start/macos-arm64/RECVX Native.app (Start.command) reads LOOSE files: --data /…/data --mods /…/mods; …
```

Beim Start meldet die Runtime im Terminal bzw. Log:

- `Data: <ordner> (textures: uncompressed TGA | colour-mapped RLE TGA …)`
- `Data: app bundle tree, build-time mods baked in` bei Paketdaten
- `Mods: <ordner>; textures/hd: N files` und Hinweise auf DDS in `hd-source/` bzw. falsch abgelegte DDS
- je Raum `RM_xxxx.RDX textures: H HD, M mod files, D data, O decoded`
