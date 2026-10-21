# Inventarpaket: positive Wirkungstests (Teil 1) und Behebung (Teil 2)

Stand 2026-10-09. Plattform macOS arm64. **Windows x64 ungeprüft und in dieser
Sitzung nicht gebaut** (Begründung unten). Kein Commit, kein Paketstand.

Semantikreferenz: `src/ps2/veronica/prog/sub1.c`, `weapon.c`, `event.c`,
`include/.../types.h` sowie die reinen Datentabellen. Kein Originalkern, keine
Quarantänedatei eingebunden, keine PS2-Nachbildung. `python3
tools/check_no_original_core.py` meldet PASS.

## Ergebnis

CTest macOS arm64: **42/42 bestanden** (vorher 37/42).

Laufzeitabläufe mit echten Daten:

| Ablauf | Ergebnis |
|---|---|
| `verify_events` | bestanden — Steves M93R-Geschenk über das echte Raumskript, 12 Schuss, Ausrüstung, Quittung, Skip, RM_0070-Regalmunition |
| `verify_pickup_slide` | bestanden — die Frage ignoriert Bestätigen während der Einfahrt und nimmt sie danach an |
| `verify_progression` | bestanden — Ansage, Übergabe, Einlagern, Entnehmen **und der belegte STANDARD-Zellen-Tausch** |
| `scan_event_rooms` (205 Räume) | bestanden — 0 blockierte Befehle; **keiner der sieben D06-Befehle wird noch als ungebunden gemeldet** |
| `verify_snapshot` | bestanden |
| `verify_pickups` | scheitert vor GetItem: der Spieler erreicht die Calico-Position nicht (Kollision, offene Abhängigkeit) |
| `verify_ammunition` | scheitert am leeren Abzug (Spielerkampf, offene Abhängigkeit) |
| `verify_item_catalog`, `verify_file_pickup` | scheitern an den Dateitabellen (`inventory_engine.inc` Quarantäne); die Runtime meldet das wörtlich |
| `verify_detector` | scheitert an den Effektzuständen (`effect_sprites.inc` Quarantäne) |
| `verify_continuity` | scheitert an der Treppenmotion (`player_stairs.inc` Quarantäne) |
| `verify_combat` | scheitert am Spielerangriff (`player_combat.inc` Quarantäne) |

Teilnachweise mit direkt gestarteter Aufnahme (Kollision umgangen, daher **kein
vollständiger Nachweis**): Calico in RM_0070 — Frage 157, Einfügung, 158 einen
Durchlauf später, Sortierung vor die schmalen Items, pip[0] 3 → 4, Raumflag 136.
Regalmunition in RM_0070 — 157, 12×15 in pip[3], Flag 135, Schließen.

## Behobene Ursachen (Teil 2)

**1. D06 ist integriert.** Die sieben Eventbefehle laufen im produktiven Pfad in
`native/events.cpp` auf `Inventory::script_item_*`: `0x31` bhPlItemLost, `0xb2`
bhItemGetGet, `0xbf` bhPlItemLostEx, `0xc1` bhArmsItemSet, `0xc2`
bhItemGetGetEx, `0xc7` bhPlItemChangeM, `0xc9` bhPlItemTamaSet. Der
Anschluss-Patch `archived/work/native_port_D06_events.patch` ist damit erledigt; die
Peer-Session wurde über die Übernahme der Datei informiert.

**2. WeaponSet-Vorzustand.** `weapon_set` bekommt `plp->wpnr_no` von **vor** der
Mutation als Parameter; die Aufrufer nehmen es mit `weapon()` ab, bevor sie
Slots verändern. Das Original ruft WeaponSet erst nach dem Tausch auf, wo
`plp->wpnr_no` noch auf der alten Waffe steht. Kein zweiter Spielerzustand in
der Inventory. Folge: Combi_00 (Teil 19 + Bogen 9 → 10, ok 129) und Combi_02
(Munitionswechsel am Werfer, ok 128) lassen die Waffe ausgerüstet.

**3. EraseItem.** Liegt der gelöschte Eintrag auf oder hinter dem ausgerüsteten
Slot — bei `pip[0] == 0` immer —, endet die Routine ohne pip[0]-Änderung und
**ohne ItemSort**. Nur davor sinkt pip[0] und es wird sortiert.

**4. Absolute Indexsemantik.** bhPlItemLost/bhPlItemLostEx vergleichen die
Nachführung mit dem Slotindex, das Lösen aber mit dem absoluten itm-Index
(`ply_id*16 + slot`). Für Spieler 0 sind beide gleich; für 1..3 trifft der
zweite Vergleich nie, pip[0] sinkt dann nur um eins. Kein Sonderfall pro
Spieler, sondern die eine originale Rechnung.

**5. Meldung 158 im folgenden Durchlauf.** `pickup()` kennt jetzt testmode 2:
Ja setzt cb_flg 0x800 und fügt im selben Durchlauf ein, 158 und testmode 3
kommen im nächsten. `EventVm::service_pickup()` ruft GetItem wie StatusMain
einmal pro Durchlauf. Die Wartebedingung des Originals (`statusflg 0x40` /
`st_flg 0x200`) liegt als `busy`-Parameter vor; die Runtime bindet sie über
`EventVm::status_busy` an die Einfahrt des Aufnahmebildschirms. Mit der Antwort
löscht die VM die Frage, wie GetItem `st_flg 0x200` löscht.

**6. Kein erfundener Kapazitätscheck mehr.** Die zweite testmode-0-Entscheidung
nach „Ja" ist entfernt. Der Debugschutz steht getrennt in
`EventVm::cancel_item_request()`: eine Bestandsänderung aus dem Debugmenü oder
ein Snapshot-Laden verwirft eine offene Aufnahme. GetItem selbst entscheidet
unverändert einmal.

**7. Besitzeroperand.** Das Original rechnet `itm[owner*16 + k]` ohne Prüfung.
Besitzer 4..23 liegen damit in den Kisten-/Sonderbereichen von `itm[384]` und
wirken dort — das übernimmt der Port jetzt, statt sie still zu ignorieren. Ab
Besitzer 24 verlässt der Zugriff `itm`; `sys->ply_wno` hat vier Einträge, die
Partnerwaffe gilt also nur für 0..3. Beide Grenzfälle meldet die VM als
blockierten Befehl, statt Speicher zu überschreiben. Tests decken Besitzer 4,
23, 24 und Item 34 bei Besitzer 4 ab.

Ein statisches Abschreiten aller 205 Raumskripte nach echten Besitzerwerten
ist **nicht** gelungen: die Operandenlängen aus der dekompilierten `event.c`
sind für `bhSet`, `bhCk`, `bhLoadWork`, `bhWhile`, `bhSleep`, `bhElseCk`,
`bhNext`, `bhDieCk` nicht fest, der Lauf bricht dort ab. Ein reiner Bytescan
liefert nur eine verrauschte Obermenge und beweist nichts. Der 205-Raum-Scan
zeigt 0 blockierte Befehle, also kam in diesem Lauf kein Besitzer ≥ 24 vor;
das ist eine Untergrenze, kein Beweis.

**8. `verify_progression` korrigiert, nicht gestrichen.** Der Fall prüft jetzt
den belegten Tausch: Die Grundwaffe (Feuerzeug 55) aus der Kiste auf eine
Taschenzelle läuft über pip[1], das Taschenitem (Pistole) geht in die Kiste,
die Zelle wird leer. Dazu Erhaltung aller drei Items und die Freigabe der
Skriptflags nach dem EXIT-Druck. Die dekompilierte Fassung lehnt den Fall
dagegen ab (`chk2 = 0`, Meldung 159) — sie ist an dieser Stelle falsch.

## Kräuterfälle: Widerspruch aufgelöst

Mein Teil-1-Bericht behauptete, das Verhalten sei unbestimmbar. **Das war
falsch**; `native/inventory.cpp` und die Notiz in AGENTS.md hatten recht.

Beleg (nur als Semantiknachweis, nichts davon ist portiert): Die dekompilierte
`Use_00` lässt `herbuse` in zwei Zweigen unbelegt — rotes Kraut allein (keine
Heil-/Giftwirkung) und blaues Kraut bei bereits gesetztem zweitem Giftzustand.
Im Original liefert die Routine dort den Wert, den der Statusbildschirm vorher
hinterlegt hat, und `StatusMain` setzt ihn beim Eintritt auf 0 — unbedingt, auf
jedem Pfad. Auf dem Weg zum Aufruf (`StatusMain` → `ItemCommand` → `ItemUse` →
`Use_00`) verändert ihn nichts; das gilt auch für alle davor aufgerufenen
Routinen. `GetItem` benutzt den Rückgabewert nicht.

Folge, nativ als Verhalten umgesetzt: **beide Kräuter werden nicht
verbraucht.** Rotes Kraut allein meldet 161, blaues Kraut bei zweitem
Giftzustand meldet 198 und löscht den ersten Giftzustand; der Slot bleibt in
beiden Fällen belegt. Die wirksamen Heil- und Gegengiftzweige sind unverändert:
ein grünes Kraut heilt und leert seinen Slot. Tests sichern beide Richtungen.

Zur Methode: Die Prüfung lief in einem Wegwerfskript außerhalb des Repos; es
ist gelöscht. Im Projekt steht kein Registerzustand, keine Instruktion und kein
Interpreter. Der Prüfer meldet PASS.

## Policy-Befund

`tools/generate_item_use.py` liegt noch im Arbeitsbaum. Es ist ein verbotener
Funktionskörper-Generator (extrahiert `ItemUse`/`Use_00..05` aus `sub1.c`). Er
ist in `tools/check_no_original_core.py` unter `BODY_GENERATORS` **gelistet**
und wird vom Build nicht aufgerufen, ist also kein aktiver Verstoß — nur ein
Rest aus dem Rückbau. Ich habe ihn nicht angetastet; das Löschen ist eine
Nutzerentscheidung.

## Offen

- **Windows x64: nicht gebaut.** Der Link bricht am projekteigenen
  Symbol-Audit ab: die von zig erzeugte PE enthält keine Symboltabelle, und der
  Prüfer lehnt ein nicht auditierbares Binary ab („stripped binary is not
  evidence of absence"). Entweder müssen Symbole erhalten bleiben oder der
  Audit muss die Objektdateien prüfen — eine Policy-Entscheidung, die ich nicht
  eigenmächtig getroffen habe. Zusätzlich blockiert die Sandbox den
  Standard-Zig-Cache in `~/.cache/zig`; mit Cache im Temp-Verzeichnis läuft der
  Build bis zum Audit durch.
- Kollision und Spielerkampf bleiben offene Abhängigkeiten: `verify_pickups`
  und `verify_ammunition` erreichen ihre Aufnahmestellen nicht von selbst.
- Untersuchung, Dateitabellen, Effekte, Treppenmotion bleiben Lücken außerhalb
  dieses Pakets.
- GetItem testmode 5 (StatusCancel-Endzustand) ist nicht als eigener Zustand
  nachgebildet; der Host schließt den Bildschirm bei testmode 3.
- Besitzerwerte der Skripte statisch nicht vollständig belegt (siehe Punkt 7).
