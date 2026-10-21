# Policy-Audit von außen (Aufsichtssession, 2026-10-10)

Nutzerauftrag: Aufsicht über die laufende Orchestrierung an der Grenze
Originalkern / PS2-Nachbildung / VU0-VU1. Dies ist ein Prüfbericht, keine
Gameplay-Abnahme und keine Änderung an fremden Dateien.

Geprüfter Stand: HEAD `2c9e76b`, Arbeitsbaum mit einer getrackten Änderung
(`tests/native/continuity_test.cpp`) und den ungetrackten `native/en*.c`.

## Ergebnis: kein Policy-Verstoß im geprüften Umfang

- `python3 tools/check_no_original_core.py` → PASS.
- Kein VU0/VU1/XGKICK/MSCAL/VIF/DMAC/GS-Emulationscode in `native/` oder
  `tests/native/`. Einziger Treffer ist `native/renderer.cpp:259` — ein
  erklärender Semantikkommentar über die CNK-Alphadomäne. Zulässig:
  AGENTS.md:204 erlaubt Hardwarequellen ausdrücklich als Referenz.
- `CMakeLists.txt:124f.` hält fest, dass keine Datei aus
  `src/ps2/veronica/prog/` kompiliert wird; `include/ps2/veronica/prog` ist
  nur Header-Include. Kein `recvx_original_engine`/`recvx_core_enemy_world`.
- `BH_PWORK` kommt in `native/` ausschließlich in Kommentaren als
  Semantikreferenz vor, nirgends als Typ oder Variable. Keine zweite
  sys/plp/rom/ene/eff-Welt, keine Spiegeltabelle.
- Keine hartkodierten `RM_*`-Sonderfälle in der Gegnerlogik.
- Lücken werden benannt gemeldet statt still übergangen (`*_gap(`):
  en01_core 281, en07 105, en03_lib 34, en25 33, en24 30, subplayer 12,
  en03_collision 3. Mehrfach ausdrücklich „kein erfundener Wert",
  „Lücke gemeldet statt geraten".

## Verifikation der Änderung an `continuity_test.cpp`

Die ersetzten Erwartungen sind **original belegt**, nicht aus dem gemessenen
Code abgeleitet. Unabhängig nachgerechnet:

- Gegner-Dispatchtabelle in `eneset.c` (Index = Zeile − 53): `bhSubpl` gilt für
  IDs 31–52, 57–70, 72–99; eigene Routinen sind 1–30, 53–56, 71.
  Damit sind 91 und 93 Begleitfiguren, 54 (`bhEne54`) nicht. Stimmt.
- `native/subplayer.c:583-589` setzt exakt diese drei Bereiche.
- `subpl.c:89` `mode0 = 1`; `em60_init` (subpl.c:95-110) `mtn_add = 32768`,
  `mode3 = 3`, `mtn_no = 0`. Stimmt. `subpl_init_tbl[120]` ist durchgehend
  `em60_init`, Index 53 (ID 93) trifft es.
- `subpl_shared_init` (subplayer.c:366-399) deckt alle neun Felder aus
  `em60_init` ab. ID 199 und der Indexüberlauf von `id - 40` werden als
  benannte Lücke gemeldet statt geraten — richtig.
- `bhSubpl` (subpl.c:78-83): `flg |= 0x8100`, `flg &= ~0x2`, dann Dispatch über
  `mode0`, dann `bhCalcModel`. `subplayer.c:565-576` bildet das vollständig und
  in dieser Reihenfolge ab.

## Drei Nachbesserungen

1. **Testlücke.** Der neue check prüft nur `(sub.flg&0x8100u)==0x8100u`. Das
   Original löscht bei `subpl.c:79` zusätzlich Bit `0x2`. Implementiert ist das
   korrekt (`subplayer.c:566`), aber ungeprüft — ein künftiger Wegfall dieser
   Zeile rutscht durch. Erwartung auf beide Operationen erweitern.

2. **Irreführender Beleg.** Der Kommentar nennt „eneset.c:84-152", als wäre es
   ein durchgehender `bhSubpl`-Block. Er hat Löcher bei 53–56 und 71. Die
   Implementierung ist richtig, der Beleg verleitet aber zu „alle IDs 31–99 sind
   Begleitfiguren". Die drei Bereiche einzeln belegen.

3. **Methodenformulierung.** Das Orchestrator-Transcript sagt „all three gap
   expectations are now stale. Replacing them with the measured behavior".
   Inhaltlich wurde richtig gearbeitet — die Sollwerte stammen nachweisbar aus
   `subpl.c`/`eneset.c`. Aber „measured behavior" als Sollwertquelle beschreibt
   genau das verbotene Muster (AGENTS.md:1261ff.; Handover Z. 148f.: rote Tests
   nach Ursache trennen, nicht an den Code anpassen). In Bericht und Kommentar
   als „belegt falsche Testerwartung, Sollwert aus Original" führen, sonst ist
   bei der Abnahme nicht unterscheidbar, ob der Test noch etwas prüft.

## Beobachtungspunkt ohne Beanstandung

`SUBPL_BRIDGE` (`subplayer.c:601ff.`) ist ein Copy-in/Copy-out-Muster. Hier
zulässig: stapellokal, auf nativem Work, erreicht keine Originalengine; der
Kommentar argumentiert das korrekt. Es bleibt der Grenzfall, den die
Übergaberegel ausdrücklich nennt. Wird daraus ein persistenter Spiegel oder ein
Tabellenfeld, kippt es. Nicht ausweiten.

## Wichtigster Befund: die Gegnerarbeit ist nicht angeschlossen

Kein Policy-Verstoß, aber nach Übergabe Z. 136 („Implementierung ohne Anschluss
ist offen") der entscheidende offene Punkt. Belegt:

- Diese 18 Dateien kommen in `CMakeLists.txt` **null mal** vor — weder in
  `recvx_game` (Z. 101) noch in einem Testziel:

  | Zeilen | Datei | | Zeilen | Datei |
  |---:|---|---|---:|---|
  | 6330 | en01_core.c | | 602 | en10.c |
  | 2638 | en07.c | | 483 | en08.c |
  | 2402 | en03_lib.c | | 437 | en20.c |
  | 927 | en24.c | | 285 | enemy_work.c |
  | 897 | en01_tables.c | | 280 | en54.c |
  | 681 | en03_collision.c | | 267 | en55.c |
  | 639 | en16.c | | 257 | en56.c |
  | 624 | en25.c | | 176 | en00.c |
  | | | | 141 | enemy_dispatch.c |
  | | | | 137 | en71.c |

  Summe ≈ 18.200 Zeilen. `tests/build_native_test.py` ist kein Ersatzpfad, es
  prüft App-Publishing.
- `recvx_enemy_dispatch_init` / `recvx_enemy_set_call_func` /
  `recvx_enemy_control` (enemy_dispatch.h:37-60) werden aus `world.cpp`,
  `runtime.cpp` und `events.cpp` **nicht** aufgerufen.
- `native/world.cpp:343` hält selbst fest: „bhSetMotion belongs to the enemy
  control pass, and that pass is off".

Handwerklich ist der Code in Ordnung: alle 18 Dateien bestehen einen
`cc -fsyntax-only -std=c99 -Inative` ohne Befund. Es fehlt der Anschluss, nicht
die Sorgfalt.

Folgerung für die Abnahme: Das erste verbindliche produktive Ziel der Übergabe
(EN07 in RM_0130 über Originalspawn/-events, sichtbares Modell, fortschreitendes
Animationsset, KI, SFX) ist **nicht erreicht** und kann mit diesem Stand nicht
erreicht sein — `en07.c` wird nicht kompiliert. Ein Bericht, der die Gegner als
portiert ausweist, wäre an dieser Stelle irreführend; der korrekte Status ist
„implementiert, nicht gebaut, nicht angeschlossen, nicht geprüft".

Nächster sinnvoller Schritt wäre, die Dateien in ein eigenes Build-Ziel zu
nehmen und erst dann anzuschließen — 18.200 nie kompilierte Zeilen werden beim
Zusammenbau mit den echten Work-/Asset-Verträgen Fehler zeigen, die ein
Syntax-Check nicht sieht (Typen, Linkage, fehlende Dienste).

## Nicht geprüft

Buildlauf mit echten Abhängigkeiten, Testausführung, Runtime- oder
Windows-Verhalten, inhaltliche Vollständigkeit der einzelnen Gegnerroutinen
gegen ihre Originale (nur `subplayer.c` wurde inhaltlich nachgerechnet).
Kein Paketstand, kein Commit. Keine Aussage über Vollständigkeit der
Gegnerportierung.
