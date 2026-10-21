# Windows x64: Linkeingangs- und Binärnachweis statt Symbolaudit

Stand 2026-10-09, macOS-Host, Toolchain `zig 0.16.0` (`x86_64-windows-gnu`,
LLD im MinGW-Modus). **Der Prüfer wurde nicht gelockert.** Das fehlende
Symbolverzeichnis ist keine Ausnahme, sondern wird durch einen vollständigen
Eingangs- und Vergleichsnachweis ersetzt.

## Warum kein Symbolaudit möglich ist (gemessen)

| Versuch | Ergebnis |
|---|---|
| `llvm-nm recvx-runtime.exe` | `no symbols` |
| `llvm-objdump --syms` | leere `SYMBOL TABLE` |
| `llvm-objdump --section-headers` | `.text .rdata .buildid .data .pdata .tls .reloc` — **kein** `.debug_*` |
| `-g` im Toolchainfile, kompletter Rebuild | PE unverändert ohne Symbole |
| `-O2 -g -gdwarf-4` | linkt, `llvm-nm` findet nichts |
| `-fno-strip` | `Unknown Clang option` |
| `-Wl,--no-strip`, `-Wl,-Map=…`, `-Wl,--no-insert-timestamp` | `unsupported linker arg` |
| `SOURCE_DATE_EPOCH` | wirkungslos; LLD schreibt einen hashbasierten Zeitstempel |

Der `-g`-Versuch ist zurückgenommen. Eine Linkkarte als Ersatznachweis gibt es
nicht.

## Umgesetzter Nachweis (`tools/windows_link_audit.py`)

Der Prüfer ruft das Modul automatisch, sobald ein `.exe` keine Symbole liefert
(`tools/check_no_original_core.py`, `check_windows_binary`). Drei Stufen, alle
fail-closed:

**Stufe 1 — tatsächlicher Linkbefehl.** Aus dem Buildsystem (`ninja -t
commands <ziel>`) wird genau der Teilbefehl gewählt, der dieses Binary mit `-o`
erzeugt. CMake hängt Nachlaufschritte mit `&&` an dieselbe Zeile; die Teile
werden getrennt, sonst würde der Prüferaufruf selbst als Link gelten. `@`-
Response-Dateien werden aufgelöst. Ist kein solcher Befehl auffindbar oder
fehlt eine Response-Datei, bricht das Audit ab.

**Stufe 2 — alle Eingaben.** Jede im Befehl genannte `.obj`/`.o`/`.a`/`.lib`
wird aufgelöst; eine fehlende Datei ist ein Fehler. Dazu kommen die impliziten
Laufzeitartefakte des Werkzeugsatzes, die zig nicht im Befehl nennt: der
gesamte Werkzeugsatz-Cache wird als **Obermenge** geprüft (aktuell 980
Artefakte). Enthält keines davon ein verbotenes Symbol, gilt das auch für die
tatsächlich gelinkte Teilmenge. Archive müssen ihr Inhaltsverzeichnis
preisgeben (`llvm-nm --print-armap`), sonst sind sie kein Nachweis. Die
Symbolprüfung ist dieselbe Verbotsliste wie für native Binaries.

**Stufe 3 — Vergleichslink.** Derselbe Befehl läuft erneut, Ausgabe in ein
eigenes Verzeichnis unter demselben Dateinamen (damit der im RSDS-Satz
hinterlegte PDB-Name identisch bleibt). Das Ergebnis wird Byte für Byte gegen
das auszuliefernde Binary gestellt. Abweichen dürfen nur Felder, die aus den
PE-Strukturen **geparst** sind:

- COFF `TimeDateStamp` (4 Byte, Offset aus `e_lfanew`),
- `TimeDateStamp` jedes Eintrags im Debug-Verzeichnis (4 Byte, Offset über
  DataDirectory[6] und die Abschnittstabelle),
- GUID und Age des `RSDS`-Satzes (20 Byte, Offset aus dem Debug-Eintrag).

Kein Abschnitt und kein freier Bytebereich ist ausgenommen; der PDB-Pfad im
RSDS-Satz bleibt verbindlich. Jede andere Abweichung lässt das Audit scheitern.

Gemessen an zwei Links desselben Codes: 21 abweichende Bytes, davon 20 in den
drei genannten Feldern und das 21. im PDB-Namen — letzteres entfällt, weil der
Vergleichslink denselben Dateinamen benutzt.

## Negativprüfungen (`tests/windows_audit_test.py`, 9 Tests)

| Test | Erwartung |
|---|---|
| identischer Vergleichslink | besteht |
| gekipptes Codebyte in `.text` | scheitert mit „outside the parsed metadata fields" |
| gekipptes Datenbyte am Dateiende | scheitert |
| Metadatenfelder | jedes Feld ≤ 20 Byte, keine Abschnittsausnahme |
| fehlende Bibliothek im Linkbefehl | wird als fehlende Eingabe gemeldet |
| Archivmitglied mit verbotenem Symbol | wird mit Symbolnamen gemeldet |
| saubere Eingabe | besteht |
| unlesbares Archiv | scheitert |
| fehlender Werkzeugsatz-Cache | scheitert |

Die Fixtures für verbotene Symbole entstehen mit dem **Hostcompiler**, damit
nie ein verbotenes Objekt in den Werkzeugsatz-Cache gelangt. Der Test ist in
CTest registriert, sobald der Zig-Werkzeugsatz vorliegt (`windows_link_audit`).

Während der Entwicklung hat das Audit genau das geleistet: ein früher Testlauf
hatte ein `sys`-Objekt im Zig-Cache hinterlassen, und die Obermengenprüfung hat
es gefunden und den Windows-Build blockiert.

## Status

**Das Windows-x64-Binary ist erzeugt und hat das Audit bestanden**
(`build/windows-x64/recvx-runtime.exe`, 2 175 488 Byte; Build mit
`tools/build_windows.py`). Audit bestanden heißt: Linkeingaben vollständig
erfasst und symbolgeprüft, Vergleichslink byteidentisch außerhalb der drei
geparsten Metadatenfelder.

**Nicht enthalten und ausdrücklich offen:** jeder tatsächliche Windows-Lauf.
Das Binary ist nicht auf Windows ausgeführt worden; Spielabläufe, Eingabe,
Audio und Darstellung unter Windows sind ein eigener Nachweis. Grüne Lade- und
Komponententests auf macOS sagen darüber nichts aus.
