# 02 — Ergebnis: Gemeinsame native Enemy-Work-Arena

## Quelldateien

| Datei | Rolle |
| --- | --- |
| `native/core_enemy_arena.h` | Deklaration: `RECVX_ENE_ARENA_SIZE`, `core_bh_ene_arena_init/reset/base/cursor` |
| `native/core_enemy_arena.c` | nativer Arena-Träger (64-Byte-ausgerichteter `unsigned char[0xC40000]`) |
| `tests/native/core_enemy_arena_test.cpp` | CTest `core_enemy_arena` (Abnahme) |
| `CMakeLists.txt` | Library `recvx_core_enemy_arena`, INTERFACE-Link an `recvx_original_engine`, Testtarget |

## Architektur (kein Doppel-Allokator, keine Rename-Kollision)

Der produktive Allokator `bhEne_CallocWork` bleibt unverändert die **eine**
Implementierung. Der Generator (`generate_enemy_sources.py`,
`pointer_int_truncation`) hat ihn bereits host-sicher übertragen
(`uintptr_t` statt `unsigned int`, s. `build/native/generated/enemy/eneset.c`).
Die Arena-Datei stellt ausschließlich den **Träger** (`sys->memp` /
`sys->mempb`) und den Original-Lebenszyklus bereit; die Allokationssemantik
selbst wird nicht neu geschrieben. Damit ist jede produktive
`bhEneXX_Init`-Allokation strukturell an denselben Owner gebunden, ohne
`bhEne_CallocWork` umzubenennen (das einen Symbolkonflikt mit `eneset.c`
erzeugt hätte).

## Reset-Einstieg

- `core_bh_ene_arena_init()` — `sys->mempb = base`, `sys->memp = base`
  (entspricht `system.c:449` `mempb = memp` nach obwp/itwp).
- `core_bh_ene_arena_reset()` — `sys->memp = sys->mempb`
  (entspricht dem Raumwechsel-/Reinit-Zweig `system.c:1523`).

## Erhaltene Originalsemantik (`eneset.c:906`)

- Obergrenze `(base + size - mempb) > 0xC40000` → `NULL`, **kein** stiller Fallback.
- 64-Byte-Ausrichtung des Cursors vor Rückgabe.
- Rückgabe des ausgerichteten Starts, Cursor-Advance um genau `size` (nicht ausgerichtet).
- Nullung von exakt `size` Bytes.
- Zweiter Parameter unbenutzt.

## Testbeleg (`recvx-core-enemy-arena-tests`, CTest `core_enemy_arena`)

Der Test bindet `sys = &system_work` und ruft den **echten** `bhEne_CallocWork`
aus `eneset.c` (nicht eine lokale Nachbildung):

1. **Ausrichtung** — Basis 64-ausgerichtet; 1-Byte-Allok liegt bei `base`,
   Folge-Allok bei `base + 64`.
2. **Reihenfolge/Nichtüberlappung** — EN07-Größen: `exp0` `0x44` bei `base`,
   `exp1` `0x8` beginnt frühestens bei `base + 0x44`.
3. **Nullung** — verschmutzter Kopf wird nach Reset vollständig genullt
   (0x100 Bytes geprüft).
4. **Volle Arena** — exakte `0xC40000`-Belegung gelingt, das nächste Byte
   liefert `NULL`; Cursor steht auf `base + 0xC40000`.
5. **Raumreset** — `reset` stellt den Cursor auf `base` zurück, die nächste
   Allokation verwendet wieder den Arenakopf.
6. **Produktive Initialisierung** — `bhEne10_Init` (echte Gegner-Work-Init,
   `en10.c:94`, ruft `bhEne_CallocWork(0x24, 8)`) bezieht sein `exp0` aus dem
   gemeinsamen Arenakopf; Cursor-Advance `+0x24` stimmt.

Produktive Init via toter-Section-Elimination (`-ffunction-sections` am
`recvx_original_engine`, `-Wl,-dead_strip`/`--gc-sections` am Test) gelinkt,
damit nur `bhEne10_Init` und seine direkte Abhängigkeit `bhEne_CallocWork`
eingebunden werden und nicht die noch offenen Audio-/Renderer-Symbole des
übrigen Enemy-Graphen.

## Lauf

```
$ ctest --test-dir build/native -R core_enemy
Start 44: core_enemy_arena ............   Passed    0.01 sec
```

Gezielt bestanden zusammen mit `core_njplus_collision`, `core_effect_pool`,
`core_object_setup`, `core_enemy_layout` (5/5).

## Grenzen (offen, nicht verschwiegen)

- `core_bh_ene_arena_init/reset` sind noch **nicht** in den produktiven
  `RoomWorld`-Lade-/Reinitpfad eingehängt; das ist Aufgabe 07/08. Die Arena
  ist der gemeinsame Owner, nicht schon die Raumlebensdauer-Verdrahtung.
- `bhEne07_Init` selbst ist nicht gelinkt (seine Abhängigkeiten `mlwP`,
  `bhSetShadow`, `bhCollisionCheckLine` usw. liegen in den Folgeaufgaben 03–07);
  seine exakten Größen `0x44`/`0x8` sind über `bhEne_CallocWork` belegt.
