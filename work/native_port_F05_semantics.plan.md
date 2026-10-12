# F05 – reziproke Z-/Akkumulator-Semantik

## Kontext
Nutzerauftrag 2026-10-12: die bisher ungeklärte reziproke-Z-Konvention und VU0-Akkumulator-Semantik anhand des Originals klären, nicht sichtbare Schatten durch Vorzeichen-/Overlay-Heuristiken erzwingen. Der bestehende Renderer konsumiert Scene::shadows überhaupt nicht; Vorher-Teststand build/f05-shadow-20261012: 8/9 Schattenprüfungen grün, shadow_modifier_pixels rot.

## Vorgehen
1. ELF-Belege für CalcPs2ZbuffAB, njCnkCvVMod/CoP3 und Ps2AddPrim3DMod sichern: positives q=1/view_z, ACC.z wird ersetzt, D=-a-b*q; PACKED statt der REGLIST-Beschriftung in der Decomp.
2. Vorhandenen nativen Tiefenvertrag in native/shadow_depth.h vervollständigen: getrennte Rechenschritte, ursprüngliche Clamp-/Konversionsfolge, GEQUAL und transportierte Vertexwerte. Keine alternative Branchwahl für den Modifier erfinden.
3. Transport-Guard aus der disassemblierten Mod-Routine als geometrische Dreiecksentscheidung nativ bestimmen, ohne VU-/GS-Interpreter. Vorhandene Geometrie und Raster-/Projektionskomponenten beibehalten.
4. Unterscheidende Tests für reale Clipbereiche, Grenzen, Akkumulator-Unabhängigkeit, Quantisierung-vor-Interpolation und Guard ergänzen. Keine bestehende rote Pixelerwartung abschwächen.
5. Sichtbaren Anschluss erst mit vollständigem Vertrag für Receiver-Tiefe, ursprüngliche Drawphase und installierten Fog realisieren. Falls diese Abhängigkeiten in dieser Änderung nicht geklärt sind, ausdrücklich offen dokumentieren; keinen geratenen Verbraucher installieren.

## Verifikation
Eigener neuer arm64-Release-Build, mandatory tools/check_no_original_core.py vor Build und danach mit Binary; gezielte Schattenprüfungen, gemessener Vorher-Vergleich. Windows-x64 betroffene Tests crossbauen, sofern vorhandene Toolchain nutzbar; kein Windows-Lauf behaupten. Bericht mit Quellen-/ELF-Adressen und verbleibenden Bindungen, Ledger nur mit tatsächlich belegtem Umfang. Kein Commit/Push oder stilles Starter-Ersetzen; keine Combat-Änderungen.
