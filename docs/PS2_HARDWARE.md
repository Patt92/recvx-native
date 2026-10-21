# Hardware boundary: VU microcode and GS registers

This page provides a reference for porting the Chunk renderer, including the
enemy and character model draw path. It identifies which parts of the original
are in VU code or GS registers, which parts were disassembled and checked
against the original ELF, and the authoritative original source for each.
[AGENTS.md](../AGENTS.md) remains binding; the verification method is in
[QUALITY.md](../QUALITY.md). Hardware details are semantic references, not a
request to build a PS2 emulator or link the original core.

## Tools

| Tool | Purpose |
|---|---|
| `tools/disasm_ps2.py` | Disassembles functions from the original ELF (R5900, including VU0 macro mode), with addresses, raw words, and symbol names. |
| `tools/check_asm_blocks.py` | Checks `.vsm` microcode byte for byte and decomp inline-assembly blocks (COP2 instruction sequence, fields, VU registers) against the ELF. |

Both need the original executable and, for VU decoding, `rabbitizer`. These
are in the archive (`archived/ps2/config/SLUS_201.84`,
`archived/ps2/config/symbol_addrs.txt`, `archived/ps2/venv`); the archive is
a reference, not a productive data source. Invocation:

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

## Verification of handwritten hardware sections

The original ELF is not stripped, so symbols and function sizes are known.
Every handwritten location can therefore be checked.

- **VU microcode:** `archived/ps2/vsm/ps2_vu0.vsm` (491 raw words) and
  `ps2_vu1.vsm` (838 raw words) are **byte-exact** extracts from the ELF
  (`ps2_vu0sub0` 0x00350880, `ps2_vu1sub0` 0x00351800). No differences.
- **Inline assembly:** 69 functions in `ps2_NinjaCnk.c`, `ps2_Vu1Strip.c`,
  `ps2_Vu1Scissor2.c`, `ps2_dummy.c`, `ps2_NaMatrix.c`, `ps2_NaMath.c`,
  `ps2_NaView.c`, `njplus.c`, `face_bh.c`, `objitm.c`, `expand.c`, and
  `ps2_loadtim2.c` were checked. In 68, COP2 instruction sequence, fields,
  and VU registers match exactly. One difference: `expand.c:163` writes
  `vnop` in the delay slot; the ELF has `nop` there (0x002CAFE4). Both have
  no effect.

Thus, decomp inline assembly can inform the port. EE registers, `%0`
placeholders, and source fields of `vdiv`/`vrsqrt` (not named by the decomp)
cannot be compared that way; disassembly remains authoritative for them.

## Chunk-model draw path

```
njCnkDrawModelLocal / ...Mod            ps2_NinjaCnk.c:674 / 777
  └─ pCnkFuncTbl[76]                    ps2_NinjaCnk.c:27  (Chunk type -> routine)
       ├─ Material/texture chunks       njCnkCm*/njCnkCtTid  -> GS state (ulType)
       ├─ Vertex chunks
       │    ├─ njCnkCvVn                :1123  pure C
       │    └─ njCnkCvVnPs2             :1280  VU0 program VU0_CALCPOINT
       │         -> pNaCnkVerBufTop[] (VU1_STRIP_BUF)
       └─ Strip chunks njCnkCs/CsUvn/CsUvh  :1720 / 2292 / 1860
            ├─ VU1 path: VIF1 packet, MSCAL 0 (DoubleMain) or 700 (SingleMain)
            │            -> VU1 transforms, clips, XGKICK -> GS
            └─ EE path:  pCnkCsVu1FuncTbl[32] -> vu1DrawTriangleStrip* (ps2_Vu1Strip.c)
                         -> VU1_PRIM_BUF -> Ps2AddPrim3DEx / Ps2AddOT
                         -> GIF packet -> loadImage (DMA channel 2) -> GS
```

Selection is in `ps2_NinjaCnk.c:1927` (same at :2359): the VU1 path runs
when `ulDrawGeneralPurposeWater != 0` or the selected function is **not**
`vu1DrawTriangleStripTransDouble`/`...DoubleI`. Transparent double-buffer
cases therefore use the EE version with an ordering table; all others use VU1.

## Key finding: C versions exist for both microprograms

The original contains each microcode part **twice**: as a VU program and as a
readable routine in C or VU0 macro mode, with the same names. The port should
translate the C algorithm and cross-check it against microcode; it does not
need a VU interpreter or microcode back-translation.

| Microprogram | C version | Evidence |
|---|---|---|
| `VU0_CALCPOINT` (ps2_vu0.vsm:99) | `njCnkCvVn` | ps2_NinjaCnk.c:1123 |
| `DoubleMain`/`SingleMain` (ps2_vu1.vsm:20/894) | `vu1DrawTriangleStripOpaqueDouble`/`...Single` | ps2_Vu1Strip.c:967/846 |
| `SCISSOR`/`PLANE_START`/`CLIP_INTER`/`INTERPOLATE` | `ScissorTriangle` | ps2_Vu1Strip.c:364 |
| `DrawScissorPolygon` | `DrawScissorPolygonOpaque` | ps2_Vu1Strip.c:777 |
| `_Double_DisplayAreaPoint_False` | `_Check_DisplayAreaPoint` | ps2_Vu1Strip.c:766 |
| `vu1GetVertexColor*` (11 labels) | `vu1GetVertexColor*` (11 functions) | ps2_Vu1Strip.c:1565 onward |

VU1 labels carry the C function names, and the instruction sequences match.
Example `vu1GetVertexColorDif` (ps2_vu1.vsm:690 versus
ps2_Vu1Strip.c:1631):

```
VU1:  MINIw.xyz vf01, vf01, vf00w   EE:  vminiw.xyz vf4, vf4, vf0w    /* to 1.0 */
      MAXx.xyz  vf01, vf01, vf00x        vmaxx.xyz  vf4, vf4, vf0x    /* to 0.0 */
      MUL.xyz   vf01, vf01, vf19         vmul.xyz   vf4, vf4, vf5     /* * Diffuse */
      MUL.w     vf01, vf00, vf21         vmulx.w    vf4, vf0, vf6x    /* w = AlphaRatio */
```

VU1-to-EE register map: `vf19` = `vu1Diffuse`, `vf20` = `vu1Specula`,
`vf21` = `fVu1AlphaRatio`. The EE side scales to the GS range (128 = 1.0):
Diffuse × 128, Specula × 31, Alpha × 128
(`ps2_Vu1Strip.c:130/157/196`).

## VU0_CALCPOINT vertex pipeline

`njCnkCvVnPs2` loads lighting data into vf04..vf14, calls
`VU0_CALCPOINT_INIT`, then calls `VU0_CALCPOINT` per vertex with vf04 as
position and vf05 as normal, retrieving results through `qmfc2`
(ps2_NinjaCnk.c:1385 onward).

Register and memory layout (ps2_vu0.vsm:51 onward):

| Location | Content | Source |
|---|---|---|
| vf28..vf31 | World-view matrix | `njSetMatrixCN` (ps2_NaMatrix.c:352), saved in VU mem 60..63 |
| VU mem 56..59 | Clip matrix `ClipMatrix2` | `_Make_ClipMatrix` (ps2_NaView.c:233) |
| vf24/vf25/vf26 | Light X/Y/Z for four lights | `v[0..2]`, ps2_NinjaCnk.c:1295 |
| vi10 | Light bit mask: bits 7..4 = `ulState` for lights 0..3; bits 3..0 = `ulMode`, masked by `mask[lLightMax]` | ps2_NinjaCnk.c:1310 |
| vf12/vf13 | `fNearRR`/`fFarRR` per light | `v[4]`, `v[5]` |
| vf27 | `fI` per light | `v[6]` |
| VU mem 71..74 | Light color (r,g,b,i) per light | `v[7..10]` |
| vf17 | (AspectW, AspectH) × projection | `vu1SetScreenAspect`, `MULz.xy vf17, vf17, vf23z` |
| vf23 | (NearClip, FarClip, projection) | `vu1SetNearFarClip`/`vu1SetScreenProjection` |
| vf01 | (1,1,1,1) | `njInitMatrix`, ps2_NaMatrix.c:36 |

Per-vertex output to `VU1_STRIP_BUF` (ps2_NinjaCnk.c:1397 onward):

| Register | Target | Content |
|---|---|---|
| vf07 | +0x10 `fIr,fIg,fIb,fA` | Accumulated light color |
| vf18 | +0x20 `fVx,fVy,fVz,fFog` | View-space position |
| vf06 | +0x30 `fSx,fSy,fIz,fNz` | Screen X/Y, 1/z, normal Z |
| vf11.x | -> `njCalcFogPowerEx` -> `fFog` | 1/z (original parameter name: `INVZ`) |
| vi11 | `Vu0ClipFlag` | Accumulated `CLIPw` flags against `ClipMatrix2` |

Semantics (also in `njCnkCvVn`, ps2_NinjaCnk.c:1182–1265):

```
vs      = M * pos                     /* view space */
iz      = 1 / vs.z
fSx     = aspectW * vs.x * dist * iz  /* EE adds screen offset later */
fSy     = aspectH * vs.y * dist * iz
fIz     = iz
fNz     = (M * normal).z              /* back-face test */
fFog    = njCalcFogPower(vs.z)        /* PS2 version: njCalcFogPowerEx(iz) */
Color: for each light with ulState != 0
  ulMode == 0 (parallel): d = dot(n, light direction); if d > 0: d *= fI
  otherwise (point):    v = light position - vs; r2 = |v|²
                        if fFarRR > r2: d = dot(n, v/|v|)
                          if d > 0 and fNearRR < r2: d *= fNearRR / r2
  contribution: fIr/fIg/fIb += d * light color
```

**Open:** In the VU0 program, MAC flags (`FMAND vi13/vi12`,
ps2_vu0.vsm:124–126, offset 0x0260) control near/far point-light branches;
the mapping of flag to branch depends on the VU flag pipeline. After
`IBNE vi01`, a light would disappear entirely (vf08 was zeroed earlier,
ps2_vu0.vsm:0x01E0), whereas the C version only applies attenuation there.
Clarify the flag pipeline before claiming equivalence. Drawn models use
`njCnkCvVnPs2`, hence VU0, as their original reference.

## VU1 path: layout from the VIF packet

The constant packet (`njCnkCsUvh`, ps2_NinjaCnk.c:1959 onward) unpacks 15
quadwords through `UNPACK V4_32` to VU1 address 884, with `BASE 0`/
`OFFSET 422` for the double buffer:

| Packet bytes | Content |
|---|---|
| 32 / 48 / 64 | `vu1Diffuse` / `vu1Ambient` / `vu1Specula` |
| 80..92 | NearClip, FarClip, projection, AlphaRatio |
| 96..144 | `ClipMatrix2[0..3]` |
| 160/164 | Screen offset X/Y |
| 168/172 | 0 / 4095 (screen limits) |
| 176/180 | AspectW / AspectH |
| 184/188 | 0 / 65535 (Z limits) |
| 192..240 | `ClipScreenMatrix[0..3]` |

Each strip sends vertex data (up to 60 vertices, followed by 58 new vertices)
to VU1 address 32768, preceded by a GIF tag (`REGLIST ST, RGBAQ, XYZF2`).
`Ps2_chunk_buff_flip` alternates scratchpad buffers 0x70002000 and
0x70002F40 (ps2_NinjaCnk.c:2011/2205). VU1 ends each strip with `XGKICK`
(ps2_vu1.vsm:0x0488).

## Ps2AddPrim3DEx: GIF packet and GS registers

`ps2_dummy.c:657`, ELF 0x002CBFB0 (size 0x314), inner loop from 0x002CC1CC.
The inline block's instruction sequence was checked against the ELF. Nearby,
`Ps2AddPrim` (`ps2_dummy.c:309`) has the same calculation in pure C. It is a
readable reference but differs at two boundaries (below).

Packet header:

```
DMA tag        qwc = num*3 + 3,  0x70000000 (scratchpad source)
GIFtag         PACKED, NREG 1, AD      -> TEST_1
TEST_1         ATE=1 ATST=GREATER AREF=0 AFAIL=KEEP ZTE=1 ZTST=GEQUAL
GIFtag         PACKED, NREG 3, NLOOP=num, PRIM from `prim`
Register list  ST, RGBAQ, XYZF2
```

Correction recorded 2026-10-12: the decomp labels this tag REGLIST, but the
ELF contradicts it. `Ps2AddPrim3DEx` 0x002CC12C–0x002CC160 and
`Ps2AddPrim3DMod` 0x002CC6E4–0x002CC724 build base
`0x3000400000008000`: FLG bits 58–59 are 0 (PACKED), NREG is 3. The
register descriptor is `0x412`; each vertex uses three quadwords, not three
64-bit words.

Per vertex (source `VU1_PRIM_BUF`, 48 bytes):

```
qw0 = {fS, fT, fQ, ulKick}   -> unchanged ST
qw1 = {fR, fG, fB, fA}       -> vftoi0 (truncate) -> RGBAQ
qw2 = {fX, fY, fZ, fF}       -> Z = -zbuff_a - zbuff_b * fQ
                                clamp(Z, 0, 65534)
                                vftoi4 (×16) -> PACKED XYZF2
                                (X,Y 12.4; Z bits 4..27; F bits 36..43)
ADC (bit 15 of fourth word) = ulKick | guard-band hit
```

Guard band: `vclipw.xyz` compares against ±2047, with
`x' = X - 2048`, `y' = Y - 2048`, `z' = 1.0625019 * fZ - 2048.0625`.
The CLIP register is cleared only **before** the loop; the mask is 0x3FFFF,
covering three iterations. A hit by any of the current triangle's three
vertices therefore marks its ADC bit. Derived limits: X,Y outside [1, 4095]
and `fZ` outside [1, 3854.18].

Pure-C `Ps2AddPrim` instead checks X,Y against [0, 4095] and Z against
[1, 65535]. These two original versions really differ. Ex callers must use
the disassembled version. The ×1.0625 Z term and resulting far-clip limit
around 3854 are evidenced by ELF 0x002CC1DC/0x002CC1E0, not inferred.

Ordering-table sort key: `vf09.w` sums clamped Z values, then multiplies by
`1/num` after the loop (`vdiv Q, vf0w, vf10w` with `vitof0.w` on `num`). The
mean is in the quadword after the last vertex and passed to `Ps2AddOT` as
`((float*)p)[12*num+3]`. `Ps2AddPrim` instead passes the last vertex's `sz`.

Destination: without bit 0x20000000000000, call `SyncPath();
loadImage(0xF0000000)` directly (DMA channel 2 = GIF,
`ps2_loadtim2.c:624`); otherwise add an ordering-table entry.

## Buffers

```c
VU1_STRIP_BUF (0x40)                     VU1_PRIM_BUF (0x30)
 0x00 fU, fV, fPad0, fPad1                0x00 fS, fT, fQ, ulKick
 0x10 fIr, fIg, fIb, fA                   0x10 fR, fG, fB, fA
 0x20 fVx, fVy, fVz, fFog                 0x20 fX, fY, fZ, fF
 0x30 fSx, fSy, fIz, fNz
```

(`include/ps2/veronica/prog/types.h:1024/1045`)

## Still unresolved

- VU0 point-light near/far branches (above).
- Which VU1 sections go beyond their C counterparts: double-buffer bookkeeping
  (`DOUBLE_START`/`DOUBLE_LOOP`, `PushStackIFRegs`), `XGKICK` timing, and
  `DOUBLE_XYZ3`/`XYZ3_ONLY` (kick without drawing) are organized differently
  in the EE version and need individual comparison.
- Other strip-chunk branches (`njCnkCs`, `njCnkCsUvn`) have not been checked
  line by line against `njCnkCsUvh`.
- GS side: `native/core_gs.cpp` implements PRIM kick, ADC, alpha/Z tests,
  blending, TEX1, and scissor. Which of these registers are set differently
  in the Chunk path than in the status/inventory path remains open.
