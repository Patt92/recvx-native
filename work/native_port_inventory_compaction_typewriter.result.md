# Bericht: Inventar-Nachrutschen (ItemSort) und Schreibmaschinen-Speichern

Thema: `inventory_compaction_typewriter` (Subagent `inventory-save`, 2026-10-14).
Arbeitsbaum geteilt; nur eigene Mechanismen geändert. Kein Commit/Push/PR,
keine Paketersetzung, kein Originalkern.

## 1. Items rutschen im Inventar nicht nach

### Originalmechanismus
`sub1.c` `ItemSort()` (Zeile 2266, disassembliert 0x0029BFD0): arbeitet auf
`pip[2..11]` (der Decompile-Ausdruck `((NJS_TEXNAME*)&st->pip[n])->texaddr`
liest ueber den NJS_TEXNAME-Offset 0x8 = `pip[n+2]`), legt breite Eintraege
(`itemdata[].type & 0x100`, Zwei-Slot-Waffen) vor die uebrigen, erhaelt deren
Reihenfolge und laesst `pip[0]` (Ausrueckungsindex) unveraendert; zum Schluss
setzt es `sakai`.

Die drei Original-Aufrufstellen (vollstaendig per grep belegt):
- `sub1.c:8296` — `StatusMain() -> SpriteOnOff()` ruft ItemSort in **jedem**
  Frame des Statusbildschirms (`if (st->subcsr != 3)`; subcsr 3 = kurzer
  Verschmelzschritt einer Kombination, sub1.c:5901).
- `sub1.c:8917` — `bhSearchBullet()` (Nachladen aus dem Bestand).
- `sub1.c:9044` — `EraseItem()` (nur wenn der geloeschte Slot vor `pip[0]`
  liegt, dann `pip[0]--` und ItemSort).

Der Dateiablage-Zweig `fileview.c:322-336` (Handbuch 0x910001 wird per
192er-Schleife ohne ItemSort geloescht) ruft bewusst **kein** ItemSort; der
Statusbildschirm schliesst die Luecke im naechsten Frame. Genau das fehlte
nativ: es gab ItemSort nur an einigen Mutationsstellen, aber **nicht** im
per-Frame-Pfad — daher blieb der leere Slot sichtbar, bis ein Ereignis mit
eigenem Sortieraufruf (z. B. Aufnehmen) ihn nebenbei fuellte.

### Aenderung
- `native/inventory.h` / `native/inventory.cpp`: neue oeffentliche
  `Inventory::sort()` = Original-ItemSort (ruft das bestehende `compact()`).
- `native/ui.cpp` `StatusMenu::tick()`: `if(active)inventory.sort();`
  entspricht `sub1.c:8296`. Damit ruckt der Bestand in jedem Statusframe nach,
  auch nach der Dateiablage (Handbuch) und nach ItemUse/ItemBox-Entnahmen,
  deren Originalpfad selbst nicht sortiert. `erase()` (EraseItem) und
  `reload()` (bhSearchBullet) bleiben die beiden direkten Aufrufstellen.
- Die bereits vorhandenen Streu-`compact()`-Aufrufe (in HEAD, nicht von mir)
  bleiben: sie erzeugen denselben Endzustand im Statusbildschirm. Fuer exakte
  Zwischenbild-Treue koennten sie spaeter auf die per-Frame-Sortierung
  reduziert werden (Vorschlag, kein Muss).

### Pruefung
- `tests/native/inventory_sort_test.cpp` (neu, `inventory_item_sort`): Handbuch-
  Ablage (`erase_read_manual(false)`) hinterlaesst zunaechst die Original-Luecke
  und wird im naechsten Statusframe gefuellt; EraseItem senkt `pip[0]` nur vor
  dem ausgeruesteten Slot und sortiert, danach zieht der Statusframe nach;
  Verbrauch (Erste-Hilfe-Spray 20 auf 0) zieht nach; Zwei-Slot-Waffe (Item 34,
  `type&0x100`) landet vorne, zaehlt zwei Kapazitaetseinheiten und verschiebt
  die `sakai`-Grenze. **Passt.**
- Bestehende Suiten gruen: `recvx-inventory-tests`, `recvx-inventory-engine-tests`
  (dort auch "ItemSearch/EraseItem und ItemSort passed"), `recvx-snapshot-tests`.

## 2. Schreibmaschine zum Speichern

### Originalmechanismus
- Aufgabenverteilung: `main.c` `bhSysTaskJumpTab[15] = bhSysCallTypewriter`;
  `njUserMain` (0x0012B430) ruft Task 15, wenn `tk_flg & 0x8000` gesetzt und
  `ts_flg & 0x8000` **nicht** gesetzt ist. `bhFirstGameStart` (system.c:453,
  disassembliert 0x0013294C/58) setzt `tk_flg=0x73DFC0` und `ts_flg=0x3DF80`
  (Bit 15 als Sperre).
- `system.c:1224` `bhSysCallTypewriter -> ControlTypewriter()`; `bup_00.c:52`
  `TypewriterMode[sys->typ_md0]()` = Init/Main/Exit. `ss_flg & 0x200` schaltet
  zwischen Speicher- (0x200 aus) und Ladebildschirm (0x200 an)
  (`bup_00.c:343-415`). Titel-Returncode 3 (LOAD GAME) setzt in
  `bhSysCallFirstmovie` case 3 `ss_flg |= 0x200; tk_flg=0x308040; typ_md0=0`
  (system.c:374-378).
- Memory Card: Host-Datei je Slot als Plattformanpassung; Datenformat die
  originale SAVEFILE-Struktur (`snapshot.h SaveFile`, 0x838 Byte, exakt der
  `sys->version .. save_end`-Bereich, den `bhPushGameData` (room.c:1016)
  kopiert und `bhPopGameData` (room.c:1037) zurueckschreibt).
- Das Handbuch (Item 145) wird ueber den Bestand gelesen; die Ablage nach FILE
  laeuft ueber `inventory.erase_read_manual` (bereits vorhanden).

### Aenderung
- `native/user_paths.{h,cpp}` (neu): EINE Pfadaufloesung,
  Prioritaet expliziter Pfad > Dev-Baum (Git-Wurzel, erkannt am `.git`/
  `CLAUDE.md` neben `data/`) > Plattform-Benutzerdatenverzeichnis
  (`%APPDATA%\RECVX`, `~/Library/Application Support/RECVX`, XDG). Kein SDL,
  um keine zweite Abhaengigkeit zu ziehen.
- `native/save_slots.{h,cpp}` (neu): 15 Slots (`bup_00.c SelectFileInfo[15]`),
  `save-N.rcvxsav` je Slot mit dem SAVEFILE-Datenformat (via `snapshot.h`).
- `native/typewriter.{h,cpp}` (neu): Slotauswahl (Init/Main/Exit-Aequivalent),
  Cursor auf erstem freien (Speichern) bzw. belegten (Laden) Slot, Laden aus
  leerem Slot abgelehnt.
- `native/runtime.cpp`:
  - `ts_flg` Bit 15 wird bei jedem Raumaufbau gesetzt (Ladesperre der
    Schreibmaschinenaufgabe, `bhFirstGameStart`/system.c:453).
  - Beim Loeschen von Bit 15 durch das Raumskript (`typewriter_was_blocked`)
    startet der Speicherbildschirm; `typewriter_step()` schreibt den SAVEFILE-
    Hoststand in den gewaehlten Slot, setzt den Speicherzaehler fort und
    verbraucht ein Farbband (Item 31, nur wenn vorhanden), dann wird Bit 15
    wieder gesetzt (`TypewriterExit`, bup_00.c:445).
  - `--user-dir` / `--saves` als CLI; Standard `<Benutzerdaten>/saves`.
  - Titel: `frontend.has_saves` macht LOAD GAME (Cursor 0) waehlbar; Auswahl
    laeuft ueber Titel-Returncode 3 in den Ladebildschirm; Laden nutzt denselben
    `bhPopGameData`-Pfad wie der Snapshot (Raum/Position/Inventar/Flags).

### Pruefung
- `tests/native/save_slots_test.cpp` (neu, `typewriter_save_slots`): 15 Slots,
  Roundtrip, Slotgrenzen, Speicher-/Ladecursor, Ablehnung leerer Slots,
  Benutzerpfad-Prioritaet. **Passt.**
- `tools/verify_typewriter_save.py` (neu): erzeugt an einer echten Position
  einen Originalstand und laedt ihn aus dem **Titel ueber LOAD GAME** (Cursor 0)
  zurueck; Raum, Position, Inventar, Ausruestung und HP stimmen mit dem
  gespeicherten Stand ueberein. **Passt** (gegen `build/macos-arm64/recvx-runtime`).

### Offen (ehrlich benannt)
- Der **raumseitige Ausloeser** des Speicherbildschirms (welcher Opcode/Objekt-
  handler `ts_flg` Bit 15 loescht) wurde nicht gefunden: kein Raum-SCD enthaelt
  `05 0c 0f 00 .. 01` (bhSetFlag Matrix 12/Bit 15), und ein automatischer
  Trigger trat in einem Scan der 17 INK-RIBBON-Raeume nicht auf. Der native
  Einstieg ist der scheduler-korrekte (Bit 15), aber der **Speicher-Replay am
  Raum** konnte damit in dieser Sitzung nicht gefahren werden; das ist die
  zentrale verbleibende Luecke fuer Bug 2. Naechster Schritt: den Objekt-/
  Skriptpfad der Schreibmaschine in den Originaldaten bestimmen (Interaktion
  RM_0010 Nr. 1, `type=3`, `attr=0xb400`, `pos=(1,0,-29)`, `params[1]=0` =
  Nachricht 0 "USE AN INK RIBBON") und dort ansetzen.
- Der Speicherbildschirm ist eine funktionale Host-Slotliste (Host-Font), keine
  pixelgenaue Portierung von `ps2_SaveScreen.c` (Slot-Icons/Fehlermeldungen).
- INIs/Snapshots wurden NICHT in das Release-Benutzerverzeichnis umgezogen
  (Risiko/Vorgabe); `--snapshots` bleibt unveraendert. Naechster Schritt:
  Release-Launcher (`tools/package_native.py`) ohne `--controls` und die
  Standardpfade auf `user_paths` umstellen, wenn risikofrei.

## Geaenderte Dateien
- `native/inventory.h`, `native/inventory.cpp`, `native/ui.cpp`
- `native/user_paths.h/.cpp`, `native/save_slots.h/.cpp`, `native/typewriter.h/.cpp` (neu)
- `native/frontend.h`, `native/frontend.cpp`, `native/runtime.cpp`
- `tests/native/inventory_sort_test.cpp`, `tests/native/save_slots_test.cpp` (neu)
- `tools/verify_typewriter_save.py` (neu)
- `CMakeLists.txt` (drei neue Ziele)

## Ledger-Vorschlaege (TAB)
```
sub1.c:ItemSort	done	sub1.c:2266; drei Aufrufstellen sub1.c:8296 (SpriteOnOff je Frame), 8917 (bhSearchBullet), 9044 (EraseItem). native/inventory.cpp sort(); ui.cpp StatusMenu::tick.
fileview.c:FileExit01	done	Handbuch 145: Statuspfad ItemSearch+EraseItem (300-302) und Ablageschleife ohne ItemSort (322-336); native inventory.erase_read_manual.
bup_00.c:ControlTypewriter	partial	bup_00.c:52/343/388/419; system.c:1224; main.c Task 15. Native native/typewriter.* + runtime.ts_flg-Bit-15-Einstieg; Speicher-/Ladebildschirm als Host-Slotliste. Raumseitiger Bit-15-Ausloeser offen.
room.c:bhPushGameData/bhPopGameData	done	room.c:1016/1037; SAVEFILE 0x838, snapshot.h SaveFile; LOAD GAME nutzt den Pfad.
```
