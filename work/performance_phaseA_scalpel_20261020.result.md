# Leistung, Phase A + erste Skalpellschnitte (2026-10-20)

Plan: [performance_optimization_plan_20261020.md](performance_optimization_plan_20261020.md).
Messartefakte: `build/validation/performance-20261020/`.
Stand: Arbeitsbaum und Dev-App (`build/start/macos-arm64/RECVX Native.app`, Build 15:23) enthalten die
Änderungen; kein Paket ersetzt, kein Commit.

## Messungen (Sandbox: kein Display, kein sudo — nur Benchmark/Headless)

Maschine laut `os.cpu_count()`: 14 logische Kerne (sysctl war gesperrt; M4 Pro = 10 P + 4 E angenommen,
nicht gemessen). Benchmark `recvx-render-benchmark` (statisches Raumbild, Kamera 1, 200 Bilder,
Release `-O3`), CPU-Zeit = Prozesszeit aller Threads (`getrusage`). Vor den Schnitten:

| Raum | SSAA | Worker | ms/Bild Wand | ms/Bild CPU |
|---|---|---|---|---|
| RM_0020 | 1 | 1 | 17.9 | 17.9 |
| RM_0020 | 1 | auto (12) | 2.0 | 20.4 |
| RM_0020 | 2 (Standard „High“) | 1 | 62.0 | 61.9 |
| RM_0020 | 2 | 6 | 10.8 | 62.6 |
| RM_0020 | 2 | 10 | 7.1 | 65.2 |
| RM_0020 | 2 | auto (12) | 6.5 | 70.3 |
| RM_0020 | 2 | 14 | 6.3 | 75.8 |
| RM_0050 | 2 | 1 / auto | 69.1 / 7.5 | 69.0 / 78.7 |
| RM_1090 | 2 | 1 / auto | 59.2 / 6.5 | 59.1 / 66.9 |

Vollständige Reihe: `baseline-renderer-threads.txt`. Bildhash je Raum/SSAA für jede Workerzahl identisch.

Befunde:
- **Ein statisches Raumbild in Standardqualität kostet ≈ 62–70 ms CPU.** Bei 30 Bildern/s sind das
  ≈ 2 CPU-Sekunden je Spielsekunde allein für den Raum, ohne Spieler/Gegner/Effekte.
- **Die Simulation ist vernachlässigbar:** 600 Headless-Ticks RM_0020 mit `--enemy-ai on` = 0.154 s,
  also ≈ 0.26 ms/Tick (Verhältnis Raster : Simulation ≈ 250 : 1).
- **Workerzahl ändert die CPU-Zeit bis 6 nicht**, 8–10 Worker +4 %, 12 (Automatik) +13 %, 14 +22 %.
  Das ist kein Threadstartaufwand (der wäre < 1 ms), sondern langsamere Worker jenseits der
  P-Kerne. CPU-Sekunden sind dort kein Energiemaß; die energieärmste Workerzahl ist offen
  und braucht `powermetrics` (siehe unten).
- **Profil (`sample`, 1 Worker, SSAA 2):** 99.6 % im Bandrasterer; davon ≈ 55 % Pixelschleife,
  ≈ 45 % Texturabtastung `bilinear` (nicht inlined). Setup/Clipping/Sortierung/Allokationen
  < 0.5 % — die geplanten Scratch-Puffer/Band-Vorzuordnung (Plan B2) bringen für das
  statische Bild nichts Messbares und wurden nicht umgesetzt. Profil: `sample-ss2-1thread.txt`.

## Schnitte

1. **`bilinear` erzwungen inline** (`native/renderer.cpp`, Makro `RECVX_RENDER_INLINE`): Arithmetik
   unverändert, Bildhash identisch. CPU/Bild RM_0020: 17.9 → 16.2 (SSAA 1), 62.0 → 57.3 (SSAA 2);
   RM_0050 69.1 → 63.9. **≈ −8 %**, Wiederholstreuung ≈ 1 %.
   Zwei weitere Versuche (lokale Setup-Kopie gegen Aliasing, Mip-Level-Hoisting) blieben im Rauschen
   und wurden zurückgenommen.
2. **Genau ein Zeichenpass je Takt** (`native/runtime.cpp`, SDL-Schleife): Bewegungsereignisse
   (Maus, Controller-/Joystick-Achsen, Finger) setzen `render_dirty` nicht mehr, außer im Titel
   (dort liest die Tastenaufnahme Achsen). Vorher löste jedes Achsenrauschen/Mausbewegen ein
   zusätzliches Vollbild zwischen zwei Takten aus — Verschwendung **und** Abweichung, weil
   `player_sleep_draw()` (game.c:357 `pl_sleep_cnt--` im Zeichenpass, world.h:336) pro Zeichnung
   abzählt. Nicht in der Sandbox prüfbar (kein Display); Prüfung am Starter siehe unten.
3. **`--render-threads <0-64>`** (Host-Diagnose, Standard 0 = Rendererautomatik): setzt die
   Workerzahl des Hauptbilds und wird an Monitor-Renderziel (`small_screen`), Vollbildquelle
   (`world.cpp` `source`) und Terminalfenster (`computer.cpp`) weitergereicht. Headless-Bilder
   RM_0020 (60 Takte) für 0/1/3 byteidentisch (`rt0/rt1/rt3.bmp`).
4. `tools/benchmark_renderer.cpp`: CPU-Zeit, Workerzahl-Argument, Verhältnis CPU/Wand.
   Aufruf: `recvx-render-benchmark data RM_0020.RDX 200 2 [threads|auto] [new.bmp]`.

## Prüfungen

- `tools/check_no_original_core.py` PASS vor Arbeit und im Build.
- `texture_filtering`, `scene_renderer`, `shadow_modifier_pixels` (mit `data/archives/SYSTEM/0001.bin`),
  `native_presentation_gates`: PASS am neuen Renderer (Testziele nach Reconfigure `BUILD_TESTING=ON`
  neu gebaut; `./build.sh` schaltet es beim nächsten Lauf wieder aus).
- Headless-Replay `graveyard-walk` 300 Takte: läuft, Zustandsexport geschrieben.
- Nicht geprüft: Fenstermodus/Titelstart (Sandbox ohne Display), Windows-Crossbuild, Energie.

## Vom Nutzer am Starter zu messen (Sandbox kann es nicht)

Dev-App ist aktualisiert; `Start.command` wählt sie. Für A/B mit identischem Bild:

```bash
sudo powermetrics --samplers cpu_power,gpu_power -i 1000 -n 60 | grep -E 'CPU Power|GPU Power|Combined'
```

parallel dazu das Spiel in derselben Szene (z. B. 60 s Stillstand in RM_0020, dann 60 s Laufen),
jeweils mit `--render-threads 4`, `6`, `10` und ohne Flag (Automatik 12), z. B.

```bash
./Start.command --in-game --room RM_0020.RDX --render-threads 6
```

Beim Beenden druckt die Runtime `Presentation: N presents, M renders, K simulation ticks`:
nach Schnitt 2 muss `renders` ≈ `ticks` sein (vorher mit Controller/Maus deutlich darüber).
`presents` bleibt Displayfrequenz (VSync) — bewusst unverändert.

## Offen (nach Plan)

- Energieoptimale Workerzahl und ggf. persistenter Workerpool (Startkosten gemessen < 1 ms, deshalb
  zurückgestellt).
- Präsentationsschleife: Present ohne neues Bild, `SDL_Delay(1)`, Hochfrequenzdisplays — nur mit
  Fenstermessung bewertbar.
- Vorbestehende Abweichung: holt die Schleife zwei Takte nach (dt > 33 ms), wird einmal gezeichnet,
  `pl_sleep_cnt` damit nur einmal abgezählt; Original zeichnet je Takt. Nicht Teil dieses Schnitts.
- Der strukturelle Hebel bleibt der GPU-Renderer (Plan Phase D); die CPU-Pixelarbeit lässt sich bei
  identischem Bild nur noch in kleinen Schritten senken.
