# Hardwaregrenze: VU-Mikrocode und GS-Register

Diese Seite ist die Portierungsgrundlage für den Chunk-Renderer, also für den
Zeichenpfad der Gegner- und Figurenmodelle. Sie beschreibt, welche Teile des
Originals in VU-Code bzw. GS-Registern liegen, was davon disassembliert und
gegen das Original-ELF geprüft ist und welche Originalquelle für jeden Teil
maßgeblich ist. Verbindlich bleibt [AGENTS.md](../AGENTS.md); Prüfmethode
[QUALITY.md](../QUALITY.md).

## Werkzeuge

| Werkzeug | Zweck |
|---|---|
| `tools/disasm_ps2.py` | Disassembliert Funktionen aus dem Original-ELF (R5900 inkl. VU0-Makromodus), mit Adressen, Rohwort und Symbolnamen. |
| `tools/check_asm_blocks.py` | Prüft `.vsm`-Mikrocode byteweise und die Inline-Assembler-Blöcke der Decomp (COP2-Befehlsfolge, Felder, VU-Register) gegen das ELF. |

Beide brauchen das Original-Executable und, für VU-Dekodierung, `rabbitizer`.
Beides liegt im Archiv (`archived/ps2/config/SLUS_201.84`,
`archived/ps2/config/symbol_addrs.txt`, `archived/ps2/venv`); das Archiv ist
Referenz, keine produktive Datenquelle. Aufruf:

```sh
archived/ps2/venv/bin/python -I tools/disasm_ps2.py \
    --elf archived/ps2/config/SLUS_201.84 \
    --symbols archived/ps2/config/symbol_addrs.txt Ps2AddPrim3DEx

archived/ps2/venv/bin/python -I tools/check_asm_blocks.py \
    --elf archived/ps2/config/SLUS_201.84 \
    --symbols archived/ps2/config/symbol_addrs.txt \
    --vsm archived/ps2/vsm/ps2_vu0.vsm \
    --inline src/ps2/veronica/prog/ps2_NinjaCnk.c
```

## Prüfstand der handgeschriebenen Hardwareteile

Das Original-ELF ist nicht gestrippt, Symbole und Funktionsgrößen sind also
bekannt. Damit ist jede handgeschriebene Stelle nachprüfbar.

- **VU-Mikrocode:** `archived/ps2/vsm/ps2_vu0.vsm` (491 Rohworte) und
  `ps2_vu1.vsm` (838 Rohworte) sind **byte-exakte** Auszüge aus dem ELF
  (`ps2_vu0sub0` 0x00350880, `ps2_vu1sub0` 0x00351800). Keine Abweichung.
- **Inline-Assembler:** 69 Funktionen in `ps2_NinjaCnk.c`, `ps2_Vu1Strip.c`,
  `ps2_Vu1Scissor2.c`, `ps2_dummy.c`, `ps2_NaMatrix.c`, `ps2_NaMath.c`,
  `ps2_NaView.c`, `njplus.c`, `face_bh.c`, `objitm.c`, `expand.c`,
  `ps2_loadtim2.c` geprüft. 68 stimmen in der COP2-Befehlsfolge mit Feldern und
  VU-Registern exakt; eine Abweichung: `expand.c:163` schreibt `vnop` im
  Delay-Slot, das ELF hat dort `nop` (0x002CAFE4) – beides wirkungslos.

Folge: Der Inline-Assembler der Decomp darf als Quelle für die Portierung
genommen werden. Nicht vergleichbar sind EE-Register, `%0`-Platzhalter und die
Quellfelder von `vdiv`/`vrsqrt` (die Decomp nennt sie nicht); dafür bleibt die
Disassembly maßgeblich.

## Zeichenpfad der Chunk-Modelle

```
njCnkDrawModelLocal / ...Mod            ps2_NinjaCnk.c:674 / 777
  └─ pCnkFuncTbl[76]                    ps2_NinjaCnk.c:27  (Chunk-Typ -> Routine)
       ├─ Material/Textur-Chunks        njCnkCm*/njCnkCtTid  -> GS-Zustand (ulType)
       ├─ Vertex-Chunks
       │    ├─ njCnkCvVn                :1123  reines C
       │    └─ njCnkCvVnPs2             :1280  VU0-Mikroprogramm VU0_CALCPOINT
       │         -> pNaCnkVerBufTop[] (VU1_STRIP_BUF)
       └─ Strip-Chunks njCnkCs/CsUvn/CsUvh  :1720 / 2292 / 1860
            ├─ VU1-Pfad: VIF1-Paket, MSCAL 0 (DoubleMain) bzw. 700 (SingleMain)
            │            -> VU1 transformiert, clippt, XGKICK -> GS
            └─ EE-Pfad:  pCnkCsVu1FuncTbl[32] -> vu1DrawTriangleStrip*  (ps2_Vu1Strip.c)
                         -> VU1_PRIM_BUF -> Ps2AddPrim3DEx / Ps2AddOT
                         -> GIF-Paket -> loadImage (DMA-Kanal 2) -> GS
```

Die Auswahl steht in `ps2_NinjaCnk.c:1927` (gleichlautend :2359): der VU1-Pfad
läuft, wenn `ulDrawGeneralPurposeWater != 0` oder die gewählte Funktion **nicht**
`vu1DrawTriangleStripTransDouble`/`...DoubleI` ist. Die durchsichtigen
Doppelpuffer-Fälle laufen also über die EE-Fassung mit Ordering Table, alles
andere über VU1.

## Wichtigster Befund: zu beiden Mikroprogrammen gibt es eine C-Fassung

Das Original enthält jeden Mikrocode-Teil **zweimal**: als VU-Programm und als
lesbare Routine in C bzw. VU0-Makromodus, mit denselben Namen. Für die
Portierung heißt das: kein VU-Interpreter und kein Rückübersetzen von
Mikrocode, sondern Portieren der C-Fassung und Gegenprüfen am Mikrocode.

| Mikroprogramm | C-Fassung | Beleg |
|---|---|---|
| `VU0_CALCPOINT` (ps2_vu0.vsm:99) | `njCnkCvVn` | ps2_NinjaCnk.c:1123 |
| `DoubleMain`/`SingleMain` (ps2_vu1.vsm:20/894) | `vu1DrawTriangleStripOpaqueDouble`/`...Single` | ps2_Vu1Strip.c:967/846 |
| `SCISSOR`/`PLANE_START`/`CLIP_INTER`/`INTERPOLATE` | `ScissorTriangle` | ps2_Vu1Strip.c:364 |
| `DrawScissorPolygon` | `DrawScissorPolygonOpaque` | ps2_Vu1Strip.c:777 |
| `_Double_DisplayAreaPoint_False` | `_Check_DisplayAreaPoint` | ps2_Vu1Strip.c:766 |
| `vu1GetVertexColor*` (11 Label) | `vu1GetVertexColor*` (11 Funktionen) | ps2_Vu1Strip.c:1565 ff. |

Die VU1-Label tragen die Namen der C-Funktionen, und die Befehlsfolgen stimmen.
Beispiel `vu1GetVertexColorDif` (ps2_vu1.vsm:690 gegen ps2_Vu1Strip.c:1631):

```
VU1:  MINIw.xyz vf01, vf01, vf00w   EE:  vminiw.xyz vf4, vf4, vf0w    /* auf 1.0 */
      MAXx.xyz  vf01, vf01, vf00x        vmaxx.xyz  vf4, vf4, vf0x    /* auf 0.0 */
      MUL.xyz   vf01, vf01, vf19         vmul.xyz   vf4, vf4, vf5     /* * Diffuse */
      MUL.w     vf01, vf00, vf21         vmulx.w    vf4, vf0, vf6x    /* w = AlphaRatio */
```

Registerzuordnung VU1 -> EE: `vf19` = `vu1Diffuse`, `vf20` = `vu1Specula`,
`vf21` = `fVu1AlphaRatio`. Die Skalierung auf den GS-Bereich (128 = 1,0) macht
die EE-Seite: Diffuse × 128, Specula × 31, Alpha × 128
(`ps2_Vu1Strip.c:130/157/196`).

## VU0_CALCPOINT – Vertexpipeline

Aufruf: `njCnkCvVnPs2` lädt die Lichtdaten in vf04..vf14, ruft
`VU0_CALCPOINT_INIT`, dann je Vertex `VU0_CALCPOINT` mit vf04 = Position,
vf05 = Normale, und holt die Ergebnisse mit `qmfc2` ab (ps2_NinjaCnk.c:1385 ff.).

Register- und Speicherbelegung (ps2_vu0.vsm:51 ff.):

| Ort | Inhalt | Herkunft |
|---|---|---|
| vf28..vf31 | Welt-Sicht-Matrix | `njSetMatrixCN` (ps2_NaMatrix.c:352), gesichert in VU-Mem 60..63 |
| VU-Mem 56..59 | Clipmatrix `ClipMatrix2` | `_Make_ClipMatrix` (ps2_NaView.c:233) |
| vf24/vf25/vf26 | Licht-X/-Y/-Z der vier Lichter | `v[0..2]`, ps2_NinjaCnk.c:1295 |
| vi10 | Licht-Bitmaske: Bit 7..4 = `ulState` Licht 0..3, Bit 3..0 = `ulMode`, maskiert mit `mask[lLightMax]` | ps2_NinjaCnk.c:1310 |
| vf12/vf13 | `fNearRR`/`fFarRR` je Licht | `v[4]`, `v[5]` |
| vf27 | `fI` je Licht | `v[6]` |
| VU-Mem 71..74 | Lichtfarbe (r,g,b,i) je Licht | `v[7..10]` |
| vf17 | (AspectW, AspectH) × Projektion | `vu1SetScreenAspect`, `MULz.xy vf17, vf17, vf23z` |
| vf23 | (NearClip, FarClip, Projektion) | `vu1SetNearFarClip`/`vu1SetScreenProjection` |
| vf01 | (1,1,1,1) | `njInitMatrix`, ps2_NaMatrix.c:36 |

Ausgabe je Vertex nach `VU1_STRIP_BUF` (ps2_NinjaCnk.c:1397 ff.):

| Register | Ziel | Inhalt |
|---|---|---|
| vf07 | +0x10 `fIr,fIg,fIb,fA` | aufsummierte Lichtfarbe |
| vf18 | +0x20 `fVx,fVy,fVz,fFog` | Sichtraumposition |
| vf06 | +0x30 `fSx,fSy,fIz,fNz` | Bildschirm-X/Y, 1/z, Normalen-Z |
| vf11.x | -> `njCalcFogPowerEx` -> `fFog` | 1/z (Parametername im Original: `INVZ`) |
| vi11 | `Vu0ClipFlag` | akkumulierte `CLIPw`-Flags gegen `ClipMatrix2` |

Semantik (identisch in `njCnkCvVn`, ps2_NinjaCnk.c:1182–1265):

```
vs      = M * pos                     /* Sichtraum */
iz      = 1 / vs.z
fSx     = aspectW * vs.x * dist * iz  /* Bildschirmoffset kommt erst auf der EE-Seite dazu */
fSy     = aspectH * vs.y * dist * iz
fIz     = iz
fNz     = (M * normal).z              /* Rückseitentest */
fFog    = njCalcFogPower(vs.z)        /* Ps2-Fassung: njCalcFogPowerEx(iz) */
Farbe: je Licht mit ulState != 0
  ulMode == 0 (parallel): d = dot(n, lichtrichtung); wenn d > 0: d *= fI
  sonst (Punktlicht):     v = lichtpos - vs; r2 = |v|²
                          wenn fFarRR > r2: d = dot(n, v/|v|)
                            wenn d > 0 und fNearRR < r2: d *= fNearRR / r2
  Beitrag: fIr/fIg/fIb += d * Lichtfarbe
```

**Offen:** Im VU0-Programm steuern die MAC-Flags (`FMAND vi13/vi12`,
ps2_vu0.vsm:124–126, Offset 0x0260) die Near-/Far-Zweige der Punktlichter, und die Zuordnung
Flag → Zweig hängt an der VU-Flag-Pipeline. Nach dem Zweig `IBNE vi01` würde ein
Licht vollständig entfallen (vf08 ist zuvor auf 0 gesetzt, ps2_vu0.vsm:0x01E0),
während die C-Fassung in diesem Fall nur die Dämpfung anwendet. Vor einer
Gleichheitsaussage beider Fassungen ist das an der Flag-Pipeline zu klären;
maßgeblich für gezeichnete Modelle ist `njCnkCvVnPs2`, also VU0.

## VU1-Pfad – Speicherbelegung aus dem VIF-Paket

Das Konstantenpaket (`njCnkCsUvh`, ps2_NinjaCnk.c:1959 ff.) entpackt per
`UNPACK V4_32` 15 Quadwords nach VU1-Adresse 884, mit `BASE 0`/`OFFSET 422` für
den Doppelpuffer:

| Byte im Paket | Inhalt |
|---|---|
| 32 / 48 / 64 | `vu1Diffuse` / `vu1Ambient` / `vu1Specula` |
| 80..92 | NearClip, FarClip, Projektion, AlphaRatio |
| 96..144 | `ClipMatrix2[0..3]` |
| 160/164 | Bildschirmoffset X/Y |
| 168/172 | 0 / 4095 (Bildschirmgrenzen) |
| 176/180 | AspectW / AspectH |
| 184/188 | 0 / 65535 (Z-Grenzen) |
| 192..240 | `ClipScreenMatrix[0..3]` |

Vertexdaten gehen je Streifen (höchstens 60 Vertices, danach Fortsetzung mit 58
neuen) nach VU1-Adresse 32768 mit vorangestelltem GIF-Tag
(`REGLIST ST, RGBAQ, XYZF2`); `Ps2_chunk_buff_flip` wechselt zwischen den
Scratchpad-Puffern 0x70002000 und 0x70002F40 (ps2_NinjaCnk.c:2011/2205). VU1
beendet jeden Streifen mit `XGKICK` (ps2_vu1.vsm:0x0488).

## Ps2AddPrim3DEx – GIF-Paket und GS-Register

`ps2_dummy.c:657`, ELF 0x002CBFB0 (Größe 0x314), innere Schleife ab
0x002CC1CC. Die Befehlsfolge des Inline-Blocks ist gegen das ELF geprüft.
Daneben liegt mit `Ps2AddPrim` (`ps2_dummy.c:309`) dieselbe Rechnung in reinem
C; sie ist die lesbare Referenz, weicht aber in zwei Grenzen ab (siehe unten).

Kopf des Pakets:

```
DMA-Tag        qwc = num*3 + 3,  0x70000000 (Scratchpad-Quelle)
GIFtag         PACKED, NREG 1, AD      -> TEST_1
TEST_1         ATE=1 ATST=GREATER AREF=0 AFAIL=KEEP ZTE=1 ZTST=GEQUAL
GIFtag         PACKED, NREG 3, NLOOP=num, PRIM aus `prim`
Registerliste  ST, RGBAQ, XYZF2
```

Nachtrag 2026-10-12: Die Decomp beschriftet diesen Tag als REGLIST; das ELF
widerlegt das. `Ps2AddPrim3DEx` 0x002CC12C–0x002CC160 und `Ps2AddPrim3DMod`
0x002CC6E4–0x002CC724 bauen die Basis `0x3000400000008000`: FLG-Bits 58–59
sind 0 (PACKED), NREG ist 3. Die Registerbeschreibung ist `0x412`, die
Vertexfolge besteht tatsächlich aus drei Quadwords, nicht drei 64-Bit-Worten.

Je Vertex (Quelle `VU1_PRIM_BUF`, 48 Byte):

```
qw0 = {fS, fT, fQ, ulKick}   -> unverändert als ST übernommen
qw1 = {fR, fG, fB, fA}       -> vftoi0 (Abschneiden) -> RGBAQ
qw2 = {fX, fY, fZ, fF}       -> Z = -zbuff_a - zbuff_b * fQ
                                clamp(Z, 0, 65534)
                                vftoi4 (×16) -> PACKED XYZF2
                                (X,Y 12.4; Z in Bit 4..27; F in Bit 36..43)
ADC (Bit 15 des vierten Worts) = ulKick | Guard-Band-Treffer
```

Guard-Band: `vclipw.xyz` vergleicht gegen ±2047, mit
`x' = X - 2048`, `y' = Y - 2048`, `z' = 1,0625019 * fZ - 2048,0625`.
Das CLIP-Register wird nur **vor** der Schleife gelöscht, die Maske ist 0x3FFFF,
also drei Durchläufe – damit markiert ein Treffer bei einem der drei Vertices
des aktuellen Dreiecks dessen ADC-Bit. Grenzen daraus: X,Y außerhalb [1, 4095]
und `fZ` außerhalb [1, 3854,18].

`Ps2AddPrim` (reines C) prüft stattdessen X,Y gegen [0, 4095] und Z gegen
[1, 65535]. Die beiden Fassungen des Originals unterscheiden sich hier also
tatsächlich; für die Ex-Aufrufer gilt die disassemblierte Fassung. Die ×1,0625
im Z-Term und die daraus folgende Fernclipgrenze von ~3854 sind im ELF belegt
(0x002CC1DC/0x002CC1E0) und nicht interpretiert.

Sortierschlüssel für die Ordering Table: `vf09.w` summiert die geklammerten
Z-Werte, nach der Schleife `× 1/num` (`vdiv Q, vf0w, vf10w` mit
`vitof0.w` auf `num`); der Mittelwert steht im Quadword hinter dem letzten
Vertex und geht als `((float*)p)[12*num+3]` an `Ps2AddOT`. `Ps2AddPrim`
übergibt dagegen das `sz` des letzten Vertex.

Zielpfad: ohne Bit 0x20000000000000 direkt `SyncPath(); loadImage(0xF0000000)`
(DMA-Kanal 2 = GIF, `ps2_loadtim2.c:624`), sonst Eintrag in die Ordering Table.

## Puffer

```c
VU1_STRIP_BUF (0x40)                     VU1_PRIM_BUF (0x30)
 0x00 fU, fV, fPad0, fPad1                0x00 fS, fT, fQ, ulKick
 0x10 fIr, fIg, fIb, fA                   0x10 fR, fG, fB, fA
 0x20 fVx, fVy, fVz, fFog                 0x20 fX, fY, fZ, fF
 0x30 fSx, fSy, fIz, fNz
```
(`include/ps2/veronica/prog/types.h:1024/1045`)

## Was damit noch nicht geklärt ist

- Near-/Far-Zweig der Punktlichter in VU0 (siehe oben).
- Welche Teile des VU1-Programms über die C-Zwillinge hinausgehen: die
  Doppelpuffer-Buchführung (`DOUBLE_START`/`DOUBLE_LOOP`, `PushStackIFRegs`),
  `XGKICK`-Zeitpunkt und `DOUBLE_XYZ3`/`XYZ3_ONLY` (Kick ohne Zeichnen) sind in
  der EE-Fassung anders organisiert und einzeln zu vergleichen.
- Die übrigen Strip-Chunk-Zweige (`njCnkCs`, `njCnkCsUvn`) sind nicht Zeile für
  Zeile gegen `njCnkCsUvh` geprüft.
- GS-Seite: `native/core_gs.cpp` setzt PRIM-Kick, ADC, Alpha-/Z-Test, Blending,
  TEX1 und Scissor um; welche der hier genannten Register im Chunk-Pfad
  tatsächlich anders gesetzt werden als im Status-/Inventarpfad, ist offen.
