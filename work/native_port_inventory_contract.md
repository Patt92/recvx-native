# Inventarpaket D02/D07/D01/D06 — Feld-, Flag- und Slotvertrag

Stand: 2026-10-09. Semantikreferenz `src/ps2/veronica/prog/sub1.c`,
`weapon.c`, `message.c`. Kein Code daraus kopiert oder generiert; die native
Umsetzung liegt in `native/inventory.cpp/.h` auf dem einzigen Bestandsbesitzer
`recvx::Inventory`. Kein zweiter SYS-/Player-/Inventory-Work, kein Copy-in/
Copy-out, keine globale Schattenverwaltung.

## Bestand

| Original | Nativer Besitzer | Bedeutung |
|---|---|---|
| `sys->itm[384]` | `Inventory::items` | gesamter Bestand |
| `sys->itm[ply_id*16 + k]` = `st->pip[k]` | `Inventory::slots()[k]` (`player`) | Spielerbestand |
| `pip[0]` | `slots()[0]` | **Ausrüstungsindex** (Slotnummer der ausgerüsteten Waffe, 0 = keine) |
| `pip[1]` | `slots()[1]` | Sonderslot (Messer/Spezialwaffe, z. B. Item 55) |
| `pip[2..11]` | `slots()[2..11]` | Taschenslots; 8 bzw. 10 nutzbar |
| `sys->itm[64..191]` | `items[64..191]` | Itembox (128) |
| `sys->itm[192..223]`, `[224..255]` | `items[192..]`, `[224..]` | Sonderboxen (je 32) |
| `sys->itm[224]` (Low-Byte) | `items[224]` | Batterie-/Zwischenwert für Item 4 (Use_05, GetItem) |
| `sys->itm[383]` | `items[383]` | Dateibesitz-Bitmaske |

**Packformat eines Eintrags:** `(id | flags) << 16 | menge`
- `id` Bits 16..23 (`Inventory::id`), `menge` Bits 0..15 (`Inventory::quantity`),
  im Original `unsigned short` — Zähler in `bhCountBullet` als `short` (signiert).
- Flags in Bits 24..31 des Eintrags (`>>16 & 0xFF00`):
  - `0x800` (Eintrag `0x08000000`): unendlich / nicht verbrauchbar
  - `0x1000/0x2000/0x4000` (Eintrag `0x10000000..`): Munitionssorte für Item 6/7
- Ein Eintrag ist frei, wenn `id == 0`.

**Breite Items:** `itemdata[id].type & 0x100` belegen zwei Kapazitätseinheiten;
`ItemSort` stellt sie vor alle schmalen. `sakai` = erster Slot ohne breites Item
(Cursor-Grenze im Menü; Host: `StatusMenu`).

## Systemflags (Host-Abbildung in `EventVm`)

| Original | Host | Im Inventarpaket benutzte Bits |
|---|---|---|
| `sys->cb_flg` | `EventVm::flags[10][0]` | `0x10` GetItem-Anforderung, `0x200` Bodentrigger aktiv, `0x400` ItemUse läuft, `0x800` Aufnahme bestätigt, `0x1000` Antwort liegt vor, `0x4000` Skriptgeschenk (ohne Frage), `0x8000` Sonderaufnahme Item 9/3, `0x1000000` Spezialslot pip[1] |
| `sys->st_flg` | `EventVm::flags[5][0]` | `0x200` Meldung aktiv, `0x4000` Antwortende |
| `sys->gm_flg` | `EventVm::flags[11][0]` | `0x8000000` Seitentasche (10 Slots), `0x40000` Magazin leer (bhCountBullet) |
| `sys->gm_mode` | `Inventory::game_mode` | Schwierigkeit, Spalte in `getbulletmax` |
| `sys->ply_id` | `Inventory::player` | Bestandsblock |
| `sys->sb_id` | `EventVm::values[8]` | aktuelles Aufnahme-/Gebrauchsitem |
| `sys->stg_no`, `rom_no` | `stage`, `room` (Parameter) | `roomno = rom_no + stg_no*100` |
| `sys->mes_sel` | Antwort `100`/`101` | Ja/Nein |
| `sys->spray_ct` | `Inventory::spray_count` | |
| `plp->hp` | `Inventory::hp` | Grenzen 160 / 320 (`gm_mode 2`) |
| `plp->stflg 0x80000`/`0x200000` | `Inventory::poisoned`/`infected` | |
| `rom->flrp[flr_idx].prm0..3` | `parameters` (`std::array<unsigned,4>`) | Bodentrigger |
| `swork.statusflg 0x100` | Rückgabe `PickupResult::granted` | Aufnahme vollzogen |
| `st->testmode` (GetItem) | `PickupResult::mode` | 1 Frage, 2 Bestätigt, 3 Abschluss, 4 Kraut-Frage, 5 Ende |
| `bhSetMessage(1,n)` | `message` | Systemmeldungsnummer |
| `st->abc`/`st->hp` | `ItemUseResult::condition` | Zustand 0..3 (Gift 4 bleibt) |

Die Seitentasche ist in der Inventory bereits als `side_pack` gespiegelt und
wird über `EventVm::set_flag(11,…)` synchron gehalten; Kapazität 8/10.

## Offene Anschlüsse außerhalb des Schreibbereichs

- `events.cpp` (Integrator): D06-Anschluss-Patch, siehe
  `archived/work/native_port_D06_events.patch`.
- `ui.cpp`/`box_menu.cpp`: nutzen `combine`, `use_item`, `box_change` bereits
  über die bestehende Schnittstelle; Rückgabewerte werden jetzt mit Originalwirkung
  gefüllt.
- Kampfbesitzer (`combat.cpp`): ruft `reload()`/`count_bullet()` über den
  bestehenden kleinen Vertrag (D02c).

## Disassembly-Klärungen (Original-ELF `data/files/SLUS_201.84`)

### `bhSearchBullet` (0x002A7820, Decomp nur 93,13 % matching)

- Seitentaschen-Grenze liest `lw $a1, 0x5C($v0)` → **`sys->gm_flg & 0x8000000`**
  (SYS_WORK-Offset 0x5C), nicht `st_flg` wie in der Decomp. Damit gilt dieselbe
  8/10-Grenze wie in GetItem/ItemSort.
- Nach dem ersten Treffer mit Munitionsklasse `combidata[it+2] & 0xF000` ruft
  sie `Combi_99`, verringert `pip[0]` nur bei Ergebnis 1 und wenn
  `i+2 < pip[0]`, sortiert und verlässt die Schleife (`$s5 != 0`).
- Rückgabe 1 genau dann, wenn die Schleife vor `max` endete.

### `ItemBoxChange` (0x002A53E0, Decomp nur 67,97 % matching)

Die Decomp ist an tragenden Stellen falsch. Belegter Ablauf aus dem
Maschinencode. Register beim Eintritt: `$t9 = lt_c = listcsr_0`,
`$t3 = pip[lt_c]`, `$t4 = bxp[ibcsr]` (itemid3), `$s4 = pip[0]`,
`$t0 = lt_c3 = 0`, `$a3 = lt_c2 = lt_c`, `$t2 = chk2 = 0`,
`$s2 = id1`, `$s3 = id2`, `$t1 = arms` (ID am Ausrüstungsslot).

1. **Restriktionen (nur `ply_id == 0`, 0x2A5474):** `type(id2)&0x40` in Raum
   9/26 → Meldung 151; `type(id1)&0x40` und id1 ∈ {105,106,121} → 150;
   `!(type(id1)&0x400)` und id1 und Raum 0/9 → 200. Jeweils
   `statusflg &= ~0x100000`, Rückkehr. Entspricht der Decomp.
2. **Kapazität (0x2A55D8):** `max = gm_flg&0x8000000 ? 10 : 8`, `bmax`
   wie Decomp.
3. **Boxitem breit (0x2A569C):**
   - `lt_c == 1` (Sonderslot), `bmax >= 2`, **und id2 ist eine Grundwaffe
     `standard[ply_id]`** (0x2A56C4–0x2A56F4): `lt_c3 = max+1`, `$v1 = itemid3`,
     `lt_c2 = lt_c` bzw. bei Treffer auf `standard[0]` `lt_c2 = lt_c`,
     **`itemid3 = 0`**, `chk2 = 1`. Nicht-Grundwaffe: ebenfalls `lt_c3 = max+1`,
     `$v1 = itemid3`, `lt_c2 = $s5` (= lt_c), `itemid3 = 0`, `chk2 = 1`.
     `bmax < 2` → `chk2` bleibt 0.
   - `lt_c != 1`: Taschenitem breit → `chk2 = 1`; sonst mit id1 → `chk2 = 1`
     bei `bmax >= 1`; leer → `chk2 = 1` bei `bmax >= 2`.
4. **Boxitem schmal (0x2A57A8):**
   - `lt_c == 1`: Grundwaffe → `chk2 = 1` (normaler Tausch). Keine
     Grundwaffe: bei `bmax >= 1` `lt_c3 = max+1`, `$v1 = itemid3`, `itemid3 = 0`,
     `chk2 = 1`; bei `bmax == 0` nur wenn **id2 == 0** (`bnez $s3`) `chk2 = 1`.
   - `lt_c != 1`: `chk2 = 1`; ist id2 eine Grundwaffe (0x2A5834):
     **`lt_c3 = lt_c`, `lt_c2 = 1`, `$v1 = 0`** — `chk2` bleibt 1 (die Decomp
     setzt es fälschlich auf 0).
5. **Stapelprüfung (0x2A5870):** `type(id2)&0x200`: id1 == id2 → `chk2 = 2`,
   Ziel `lt_c`; id1 == 0 → erstes `pip[x+2]` mit id2 und Menge < 999 → `chk2 = 2`.
6. **Schreibblock chk2 == 1 (0x2A592C):**
   - Ist `pip[0] == lt_c`: `parts_14b->atr &= ~0x20`, `pip[0] = 0`,
     `plp->mode3 = 0` (sb 0xF), `plp->wpnr_no = 0` (sw 0x3E4),
     `sys->mn_mode0 = 3` (sys+0x1B0F4).
   - `bxp[ibcsr] = pip[lt_c]` (`$t3`), `pip[lt_c2] = itemid3` (`$t4`),
     **falls `lt_c3`: `pip[lt_c3] = $v1`** — das ist der ursprüngliche Boxwert
     bzw. 0, **nicht die Konstante 2** der Decomp. Beim Sonderslot landet das
     Boxitem so in `pip[max+1]` (letzter Taschenslot), `pip[1]` wird 0; die
     anschließende Sortierung holt es nach vorn.
7. **`pip[0]`-Nachführung (0x2A59C0), nur wenn `pip[0] >= 2`:**
   - `lt_c == 1` → nur: id2 breit und `pip[pip[0]]` nicht breit → `pip[0] += 1`
     (0x2A5BD4–0x2A5C54).
   - id2 == 0 oder Grundwaffe: `lt_c < pip[0]` → `pip[0] -= 1`.
   - sonst mit id1: id2 breit → (id1 nicht breit, arms nicht breit,
     `pip[0] > lt_c`) → `+1`; id2 schmal → (id1 breit, arms breit,
     `lt_c > pip[0]`) → `−1`.
   - sonst ohne id1: id2 breit und arms nicht breit → `+1`.
8. **`listcsr_0`-Korrektur (0x2A5C5C), nur `lt_c != 1`:** wenn
   `listcsr_0 > sakai` oder (`listcsr_2 & 1` und `listcsr_0 == sakai-1`):
   id1 breit und id2 schmal → `listcsr_0 += 1`; id2 breit und id1 schmal →
   `listcsr_0 -= 1`. (Host-Cursor, `StatusMenu`.)
9. **Stapeln (0x2A5D7C):** `cmb` von id2; erste Regel mit `combidata[x] == id1`
   → `Combi_01(x, &bxp[ibcsr], &pip[ziel])`.
10. **Sonst (0x2A5E44):** wenn `!(st_flg & 0x200)`: `bhSetMessage(1,159)`,
    `statusflg &= ~0x100000`.
