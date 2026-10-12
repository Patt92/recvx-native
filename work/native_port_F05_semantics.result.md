# F05: reziproke Z- und Akkumulator-Semantik

Stand 2026-10-12. Nutzerauftrag: „mach semantik“, keine Sichtbarkeits-Heuristik.

## Ergebnis

**Die bisher ungeklärte Vorzeichen-/Akkumulatorfrage ist am Original-ELF geklärt. Sichtbare Schatten sind NICHT fertig.** Der produktive `SoftwareRenderer::draw_view` konsumiert `Scene::shadows` weiterhin nicht. Das ist ein fehlender Zeichenmechanismus, nicht der Beleg für ein falsches Tiefenvorzeichen.

Keine Combat-, World-, Runtime-, gemeinsamen Math- oder Audioänderungen. Keine Quarantäne benutzt, kein Originalkern gebaut, kein Interpreter oder zweiter Workbesitzer. Kein Commit/Push, kein Starter-/Paketwechsel.

## Originalbelege

Referenzdateien außerhalb der Quarantäne:
- `ps2_NaView.c:142–162`: njClipZ hält signierte negative Near/Far-Werte.
- `ps2_NaDraw2D.c:10–14`, ELF **0x002DF1F0–0x002DF240**: `a=(-65536*n)/(f-n)`, `b=((65536*n)*f)/(f-n)`. Symboladressen: a=0x01E12F48, b=0x01E12F40, n=0x00339C60, f=0x00339C68.
- `ps2_NinjaCnk.c:1518–1541`, ELF **0x002D1DF8 / 0x002D1E30**: `q=1/view_z`, positiv für akzeptierte Modifiervertices. `njCnkCoP3` reicht q unverändert als fQ weiter (CNK_PRIM_BUF +8, also vf4.z).
- `Ps2AddPrim3DMod`, ELF **0x002CC658–0x002CC678**: beide Koeffizienten werden negiert und als vf10.x=-b / vf10.z=-a geladen.
- **0x002CC7AC**: `VADDA.z` ersetzt ACC.z mit `vf0.z+vf10.z=-a`. Die vorherige Guard-Rechnung trägt NICHT in die Tiefe über.
- **0x002CC7B0**: `VMADDX.z` schreibt den Vertexwert `ACC.z+vf4.z*vf10.x = -a-b*q`. Kein Update des ACC durch diese Instruktion.
- **0x002CC7B4 / 0x002CC7BC / 0x002CC7C4**: untere Grenze 0, obere 65534, danach vftoi4. PACKED XYZF2 entnimmt Lane-Z Bits 4–27; die Vertex-Z-Fraktion wird verworfen.
- **0x002CC6E4–0x002CC724**: Tagbasis `0x3000400000008000`, FLG=0 = **PACKED**, NREG=3, Registerliste 0x412. Die REGLIST-Beschriftung der Decomp ist falsch. Normaler EE-Ex-Tag bei 0x002CC12C–0x002CC160 ist ebenfalls PACKED; `docs/PS2_HARDWARE.md` korrigiert.

Die alternative Formel `a+b/z` gehört zum **skalaren Ps2AddPrim**, nicht zu einem wählbaren Modifierzweig. Die entsprechende Kommentarvermutung ist entfernt.

Reproduzierbarer statischer Nachweis: `python3 -I tools/verify_shadow_semantics.py data/files/SLUS_201.84` prüft **38 Instruktionsworte und 3 Floatvektoren** gegen das ELF. Dieser Prüfer führt keinen Originalcode aus. Disassembly gesichert unter `build/f05-semantics-claude-20261012/original-disassembly.txt`.

## Native Änderungen

- `native/shadow_depth.h`: belegter positiver q-Vertrag, ACC-Überschreiben dokumentiert, getrennte Multiplikation/Addition gegen native FMA-Kontraktion; explizites GEQUAL für bereits transportierte Tiefen. Keine Vorzeichenkorrektur zu einer vertrauten Near/Far-Rampe.
- `native/shadow_transport.h`: eigenständiger nativer Vertextransport aus Mod. 12.4-XY, transportierte Vertextiefe, Guard `X/Y-2048` gegen ±2047, Z-Guard `(z-2048.0625)+z*0.06250190734863281`. Drei Vertexprüfungen werden als geometrische Dreiecksunterdrückung abgebildet: der letzte Kick erhält ADC, sobald eine der drei Prüfungen fehlschlägt. Keine VU-Registersimulation.
- `native/shadow_raster.h`: **erst Vertex-Z clampen/konvertieren, dann screen-linear interpolieren**. Der Callback erhält keine rohe reziproke Tiefe mehr. Er muss den vollständigen Receiver-/Fragmentvertrag liefern; keine angenommene Integerkonversion eingebaut.
- Tests prüfen echte 2/20000-Clipwerte, Quantisierungsgleichstände, Sättigung, Guard-Grenzen/-Unabhängigkeit und Quantisierung vor Interpolation. Der Volumentest nutzt transportierte konstante Receiver und GEQUAL statt eines diagnostischen strikten q-Vergleichs.

Beispiel n=-2/f=-20000 mit Float32-Zwischenschritten:

| view_z | transportierte Vertex-Z |
|---:|---:|
| 1 | 65534 |
| 2 | 65534 |
| 4 | 32777 |
| 10 | 13115 |
| 100 | 1317 |
| 1000 | 137 |
| 20000 | 13 |

Die Sättigung bei der alten Diagnose n=-1/f=-2 bleibt geprüft, ist aber kein Beleg für pauschale Sättigung realer Clipbereiche.

## Verifikation

### macOS arm64
Eigener Release-Build `build/f05-semantics-claude-20261012`.
- Alle neun bestehenden Schattenprogramme gebaut; neues `shadow_elf_semantics` registriert.
- **9/10 Schatten-CTestgruppen bestanden.** `shadow_modifier_pixels` bleibt rot: Mitte weiterhin `ff808080` statt `ff757575`, Tiefe 0.1.
- Echter Vorhervergleich mit den bestehenden, nicht neu gebauten Programmen in `build/f05-shadow-20261012`: **8/9**, exakt derselbe Pixelfehler. Logs `ctest-before.log` / `ctest-final.log` im eigenen Build.
- Runtime neu gebaut/gelinkt; Quellen-/Build-/Runtime-Binary-Kernpolicy **PASS**.
- Keine Runtime-Pixel-/Durchspiel-/Hardwareabnahme behauptet. Weltprüfung belegt die fünf natürlichen RM_0130-Schattenallokationen und Submission, nicht ihre sichtbare Darstellung.

Apple-Werkzeuge meldeten blockierte xcrun-Cache-Schreibzugriffe im vom Werkzeug gewählten `/var/folders/.../T`; Compile/Link und Tests wurden trotzdem erfolgreich ausgeführt. Keine Sandbox-Eskalation. Das ist separat von der Testfehlermeldung zu verstehen.

### Windows x64
Eigener Ninja-Crossbuild `build/f05-semantics-windows-20261012` mit vorhandener Windows-SDL und Zig-Toolchain.
- Geänderte Depth-/Raster-/Volume-Testprogramme kompiliert und gelinkt.
- Quellen-/Build-/Depth-Binary-Kernpolicy **PASS** mit dem bereitgestellten projektlokalen Zig-Cache.
- Erste Configure-Auswahl nahm die macOS-SDL (fehlende IMPORTED_IMPLIB); auf vorhandenen Windows-SDL-Pfad korrigiert. Erster Audit wollte außerhalb des bereitgestellten Bereichs in ~/.cache/zig schreiben; danach explizit projektlokaler Cache. Keine Sandbox-Eskalation.
- Nicht ausgeführt; kein Windows-Runtime-/Hardware-/Pixelnachweis.

`tools/port_check.py --domain Effects` ausgeführt, Ergebnis separat im eigenen Build; Ledger nur **partial**, keine F05-Fertigmeldung. `git diff --check` bestanden.

## Tatsächliche Restlücken für sichtbare Schatten

1. **Receiver-/Fragmenttiefe:** jetziger Renderer hält reziproke Float-Tiefe. Konvertieren dieses fertigen Puffers ist NICHT äquivalent zur ursprünglichen Vertexkonversion vor Interpolation. Receiver-Transport, integerne Fragmentinterpolation/-konversion, Alpha-/Depthwrite-Akzeptanz und identische quantisierte Coverage müssen noch gebunden und belegt werden.
2. **Fog-Besitzer:** Scene/Camera/World tragen keinen installierten Fogzustand. `sync.c:143–257` (bhControlEOR) besitzt Flags, fog_ct und Installationszeitpunkt; `ps2_NaFog.c:55–152/213–275` besitzt Tabelle/Dichte/Sampling. 255 gilt nur bei original deaktiviertem Fog; kein pauschaler Ersatzwert.
3. **Drawphase/Maskenlebensdauer:** die originale Modifierphase, Komposition je Work, Rectangle-/UV-Quantisierung und verbleibende Maske nach abgewiesener Komposition fehlen im flachen nativen Zeichenvertrag. Nicht als finaler Darkening-Overlay ersetzen. `runtime.cpp` compose leert `scene.shadows` derzeit auch nicht; vor einem Verbraucher muss der Framebesitz repariert werden.
4. **Projektionsreihenfolge:** bestehendes `project_shadow` transformiert Model und View getrennt. Original komponiert erst und wertet CvVMod translation-first aus. Die Operationen einschließlich Distanz/Aspekt dürfen nicht bloß algebraisch umgruppiert werden. In dieser Änderung NICHT umgebaut.
5. **Arithmetikgrenze:** belegte Instruktions-/Akkumulatorsemantik und erzwungene Float32-Zwischenschritte sind kein vollständiger bitgenauer Nachweis aller EE-/VU-FPU-Rundungsfälle. Keine solche Behauptung.
6. **Spieler-/Subplayer-Schatten:** Eventproduzenten bhInitSetKage/bhKageSwSet und ihr produktiver Lebenszyklus bleiben außerhalb dieser geklärten Mod-Transportfrage.

**F05 bleibt partial — aber nicht mehr wegen ungeklärtem q-Vorzeichen oder einem vermeintlich fortgetragenen Guard-Akkumulator.** Der nächste zulässige Anschluss ist Receiver-Tiefen-/Fog-/Drawphasenport, nicht ein Vorzeichen- oder Sichtbarkeitsfix.
