/* VU0-Port: Makromodus-Transfers und die Mikroprogramme der Vertexpipeline.
 *
 * Quelle ist archived/ps2/vsm/ps2_vu0.vsm. Dieser Dump ist byteweise gleich
 * dem Original-ELF (SLUS_201.84, Symbol ps2_vu0sub0 0x00350880, 491 Rohworte,
 * geprueft mit tools/check_asm_blocks.py). Jede Instruktion steht hier mit
 * ihrem .vsm-Offset und ihrer Mnemonik im Kommentar; uebersetzt ist Befehl fuer
 * Befehl, nicht die Wirkung.
 *
 * Zeitverhalten: Ein Befehlspaar ist ein Zyklus. Ergebnisse der FMAC-Einheit
 * stehen dem Rechenwerk sofort zur Verfuegung (das Original haelt die
 * Abhaengigkeitsabstaende mit NOPs selbst ein), die MAC-Flags dagegen erst vier
 * Zyklen spaeter: FMAND sieht die Flags des FMAC-Befehls von vor vier Zyklen.
 * Deshalb zaehlt jeder portierte Befehl einen Zyklus (nop(v, n) fuer NOPs) und
 * die Flags laufen durch eine Pipeline. Ebenso ist der Verzweigungs-Delay-Slot
 * des VU beruecksichtigt: der Befehl hinter einem genommenen Sprung wird noch
 * ausgefuehrt.
 *
 * Nicht portiert: Ueberlauf-/Unterlaufflags (kein Originalpfad liest sie; die
 * Masken sind 0xF0 = Vorzeichen und 0x0F = Null) und das Verhalten von DIV bei
 * Division durch Null (das Original liefert dort den Maximalwert mit
 * Divisionsflag, hier entsteht inf/nan). Beides ist als Luecke vermerkt. */
#include "core_vu0.h"
#include <math.h>
#include <string.h>

/* Feldmasken: Bit 0..3 = x,y,z,w. */
#define MX    0x1
#define MY    0x2
#define MZ    0x4
#define MW    0x8
#define MXY   (MX | MY)
#define MXYZ  (MX | MY | MZ)
#define MXYZW (MX | MY | MZ | MW)

static RECVX_VU0 vu0;

RECVX_VU0* recvx_vu0(void) { return &vu0; }

/* MAC-Flags: Bit 7..4 = Vorzeichen x,y,z,w; Bit 3..0 = Null x,y,z,w.
 * Belegt durch die Originalmasken: vi09 = 240 maskiert die Vorzeichen, und
 * LIGHT0..3 pruefen damit 128/64/32/16, also je ein Licht in x,y,z,w
 * (ps2_vu0.vsm VU0_CALCPOINT_INIT und LIGHT0_POINT). Komponenten ausserhalb
 * der Schreibmaske liefern keine Flags. */
static unsigned int mac_flags(int mask, const float* dst)
{
    unsigned int f = 0;
    int c;
    for (c = 0; c < 4; ++c)
    {
        if (!(mask & (1 << c))) continue;
        if (dst[c] < 0.0f) f |= 0x80u >> c;
        if (dst[c] == 0.0f) f |= 0x08u >> c;
    }
    return f;
}

/* Ein Zyklus: die sichtbaren MAC-Flags ruecken um eine Stufe nach, danach
 * belegt dieser Befehl die erste Stufe (ohne FMAC bleibt der alte Wert). */
static void tick(RECVX_VU0* v, int has_flags, unsigned int flags)
{
    v->mac = v->mac_pipe[3];
    v->mac_pipe[3] = v->mac_pipe[2];
    v->mac_pipe[2] = v->mac_pipe[1];
    v->mac_pipe[1] = v->mac_pipe[0];
    v->mac_pipe[0] = has_flags ? flags : v->mac_pipe[1];
}

static void nop(RECVX_VU0* v, int cycles)
{
    while (cycles-- > 0) tick(v, 0, 0);
}

/* FMAC-Kern. op: '*' '+' '-' 'M' (MAX) 'm' (MINI) '=' (MOVE).
 * bc < 0: komponentenweise, sonst Broadcast dieses Feldes von b.
 * acc != 0: Ergebnis ist ACC + a*b (MADD-Formen). */
static void fmac(RECVX_VU0* v, float* dst, int mask, const float* a,
                 const float* b, int bc, char op, int acc)
{
    float out[4];
    int c;
    memcpy(out, dst, sizeof(out));
    for (c = 0; c < 4; ++c)
    {
        float x, y;
        if (!(mask & (1 << c))) continue;
        x = a[c];
        y = b ? b[bc < 0 ? c : bc] : 0.0f;
        switch (op)
        {
        case '*': out[c] = acc ? v->acc[c] + x * y : x * y; break;
        case '+': out[c] = x + y; break;
        case '-': out[c] = x - y; break;
        case 'M': out[c] = x > y ? x : y; break;
        case 'm': out[c] = x < y ? x : y; break;
        default:  out[c] = x; break;
        }
    }
    memcpy(dst, out, sizeof(out));
    tick(v, 1, mac_flags(mask, dst));
}

/* Die Namen folgen den Mnemoniken: A = Ziel ACC, D = MADD (ACC + a*b). */
static void MUL(RECVX_VU0* v, int d, int mask, int s, int t, int bc)
{ fmac(v, v->vf[d], mask, v->vf[s], v->vf[t], bc, '*', 0); }
static void MULA(RECVX_VU0* v, int mask, int s, int t, int bc)
{ fmac(v, v->acc, mask, v->vf[s], v->vf[t], bc, '*', 0); }
static void MADD(RECVX_VU0* v, int d, int mask, int s, int t, int bc)
{ fmac(v, v->vf[d], mask, v->vf[s], v->vf[t], bc, '*', 1); }
static void MADDA(RECVX_VU0* v, int mask, int s, int t, int bc)
{ fmac(v, v->acc, mask, v->vf[s], v->vf[t], bc, '*', 1); }
static void ADD(RECVX_VU0* v, int d, int mask, int s, int t, int bc)
{ fmac(v, v->vf[d], mask, v->vf[s], v->vf[t], bc, '+', 0); }
static void SUB(RECVX_VU0* v, int d, int mask, int s, int t, int bc)
{ fmac(v, v->vf[d], mask, v->vf[s], v->vf[t], bc, '-', 0); }
static void MAXF(RECVX_VU0* v, int d, int mask, int s, int t, int bc)
{ fmac(v, v->vf[d], mask, v->vf[s], v->vf[t], bc, 'M', 0); }
static void MOVEF(RECVX_VU0* v, int d, int mask, int s)
{ fmac(v, v->vf[d], mask, v->vf[s], 0, -1, '=', 0); }
/* Skalaroperanden Q/ACC-frei: dst[c] = a[c] * Q. */
static void MULQ(RECVX_VU0* v, int d, int mask, int s)
{ float q[4]; q[0] = q[1] = q[2] = q[3] = v->q; fmac(v, v->vf[d], mask, v->vf[s], q, -1, '*', 0); }
static void ADDQ(RECVX_VU0* v, int d, int mask, int s)
{ float q[4]; q[0] = q[1] = q[2] = q[3] = v->q; fmac(v, v->vf[d], mask, v->vf[s], q, -1, '+', 0); }

/* DIV/RSQRT belegen Q. Das Original haelt die Latenz (7 bzw. 13 Zyklen) mit
 * NOPs bzw. WAITQ ein; der Port rechnet sofort und zaehlt den Zyklus. */
static void DIV(RECVX_VU0* v, int s, int sf, int t, int tf)
{ v->q = v->vf[s][sf] / v->vf[t][tf]; tick(v, 0, 0); }
static void RSQRT(RECVX_VU0* v, int s, int sf, int t, int tf)
{ v->q = v->vf[s][sf] / (float)sqrt((double)v->vf[t][tf]); tick(v, 0, 0); }

/* CLIPw: sechs Bits in das 24-Bit-Schieberegister. Bitbelegung belegt durch
 * InitScissorPlane (ps2_Vu1Strip.c:241): die fuenf benutzten Ebenen sind die
 * Bits 0,1,2,3,5 - also x>+w, x<-w, y>+w, y<-w, z<-w; Bit 4 (z>+w) bleibt
 * unbenutzt, die Fernebene wird nicht geschnitten. */
static void CLIPW(RECVX_VU0* v, int s, int t)
{
    const float w = (float)fabs((double)v->vf[t][3]);
    unsigned int f = 0;
    int c;
    for (c = 0; c < 3; ++c)
    {
        if (v->vf[s][c] > +w) f |= 1u << (c * 2);
        if (v->vf[s][c] < -w) f |= 1u << (c * 2 + 1);
    }
    v->clip = ((v->clip << 6) | f) & 0xFFFFFFu;
    tick(v, 0, 0);
}

static void FMAND(RECVX_VU0* v, int d, int s)
{ v->vi[d] = (unsigned short)(v->mac & v->vi[s]); tick(v, 0, 0); }
static void FCGET(RECVX_VU0* v, int d)
{ v->vi[d] = (unsigned short)v->clip; tick(v, 0, 0); }

static void LQ(RECVX_VU0* v, int d, int mask, int addr)
{
    int c;
    for (c = 0; c < 4; ++c) if (mask & (1 << c)) v->vf[d][c] = v->mem[addr][c];
    tick(v, 0, 0);
}
static void SQ(RECVX_VU0* v, int s, int mask, int addr)
{
    int c;
    for (c = 0; c < 4; ++c) if (mask & (1 << c)) v->mem[addr][c] = v->vf[s][c];
    tick(v, 0, 0);
}
static void ILW(RECVX_VU0* v, int d, int addr, int field)
{
    unsigned int raw;
    memcpy(&raw, &v->mem[addr][field], sizeof(raw));
    v->vi[d] = (unsigned short)raw;
    tick(v, 0, 0);
}
static void ISW(RECVX_VU0* v, int s, int addr, int field)
{
    unsigned int raw = v->vi[s];
    memcpy(&v->mem[addr][field], &raw, sizeof(raw));
    tick(v, 0, 0);
}
static void IADDIU(RECVX_VU0* v, int d, int s, int imm)
{ v->vi[d] = (unsigned short)(v->vi[s] + imm); tick(v, 0, 0); }
static void IADD(RECVX_VU0* v, int d, int s, int t)
{ v->vi[d] = (unsigned short)(v->vi[s] + v->vi[t]); tick(v, 0, 0); }
static void IAND(RECVX_VU0* v, int d, int s, int t)
{ v->vi[d] = (unsigned short)(v->vi[s] & v->vi[t]); tick(v, 0, 0); }
static void IOR(RECVX_VU0* v, int d, int s, int t)
{ v->vi[d] = (unsigned short)(v->vi[s] | v->vi[t]); tick(v, 0, 0); }

/* ---------------------------------------------------------------- Makromodus */

void recvx_vu0_lqc2(int vf, const void* addr)
{ if (vf) memcpy(vu0.vf[vf], addr, 16); }

void recvx_vu0_sqc2(int vf, void* addr)
{ memcpy(addr, vu0.vf[vf], 16); }

void recvx_vu0_qmfc2(int vf, void* addr)
{ memcpy(addr, vu0.vf[vf], 16); }

void recvx_vu0_qmtc2(int vf, const void* addr)
{ if (vf) memcpy(vu0.vf[vf], addr, 16); }

float recvx_vu0_getf(int vf, int field)
{ return vu0.vf[vf][field]; }

void recvx_vu0_setf(int vf, int field, float value)
{ if (vf) vu0.vf[vf][field] = value; }

unsigned int recvx_vu0_cfc2(int vi)
{
    /* vi18 ist das CLIP-Register, vi16/vi17 Status/MAC (cfc2 liest die
     * Steuerregister ab 16; die Decomp nutzt vi18 und vi2). */
    if (vi == 18) return vu0.clip;
    if (vi == 17) return vu0.mac;
    return vi < 16 ? vu0.vi[vi] : 0u;
}

void recvx_vu0_ctc2(int vi, unsigned int value)
{
    if (vi == 18) { vu0.clip = value & 0xFFFFFFu; return; }
    if (vi < 16 && vi) vu0.vi[vi] = (unsigned short)value;
}

/* ------------------------------------------------------- Mikroprogramme */

/* VU0_CALCPOINT_INIT, ps2_vu0.vsm 0x0050..0x0160.
 * Sichert die Aufruferregister in den VU-Speicher und laedt daraus die
 * Lichtdaten: der EE-Code hat vf04..vf14 mit v[0..10] belegt
 * (ps2_NinjaCnk.c:1295 ff.). vf24..vf27 enthalten beim Aufruf die Clipmatrix
 * (_Make_ClipMatrix, ps2_NaView.c:233), vf28..vf31 die Welt-Sicht-Matrix
 * (njSetMatrixCN, ps2_NaMatrix.c:352). */
static void calcpoint_init(RECVX_VU0* v)
{
    SQ(v,  4, MXYZW, 64);   /* 0050  SQ vf04, 64  - Licht-X          */
    SQ(v,  5, MXYZW, 65);   /* 0058  SQ vf05, 65  - Licht-Y          */
    SQ(v,  6, MXYZW, 66);   /* 0060  SQ vf06, 66  - Licht-Z          */
    SQ(v,  7, MXYZW, 67);   /* 0068  SQ vf07, 67  - Zustands/Modusbits */
    SQ(v,  8, MXYZW, 68);   /* 0070  SQ vf08, 68  - fNearRR          */
    SQ(v,  9, MXYZW, 69);   /* 0078  SQ vf09, 69  - fFarRR           */
    SQ(v, 10, MXYZW, 70);   /* 0080  SQ vf10, 70  - fI               */
    SQ(v, 11, MXYZW, 71);   /* 0088  SQ vf11, 71  - Farbe Licht 0    */
    SQ(v, 12, MXYZW, 72);   /* 0090  SQ vf12, 72  - Farbe Licht 1    */
    SQ(v, 13, MXYZW, 73);   /* 0098  SQ vf13, 73  - Farbe Licht 2    */
    SQ(v, 14, MXYZW, 74);   /* 00A0  SQ vf14, 74  - Farbe Licht 3    */
    SQ(v, 17, MXYZW, 49);   /* 00A8  SQ vf17, 49  - Bildmassstab     */
    SQ(v, 24, MXYZW, 56);   /* 00B0  SQ vf24, 56  - Clipmatrix 0     */
    SQ(v, 25, MXYZW, 57);   /* 00B8  SQ vf25, 57                     */
    MUL(v, 17, MXY, 17, 23, 2);
    SQ(v, 26, MXYZW, 58);   /* 00C0  MULz.xy vf17, vf17, vf23z | SQ vf26, 58 */
    SQ(v, 27, MXYZW, 59);   /* 00C8  SQ vf27, 59                     */
    SQ(v, 28, MXYZW, 60);   /* 00D0  SQ vf28, 60  - Welt-Sicht 0     */
    SQ(v, 29, MXYZW, 61);   /* 00D8  SQ vf29, 61                     */
    SQ(v, 30, MXYZW, 62);   /* 00E0  SQ vf30, 62                     */
    SQ(v, 31, MXYZW, 63);   /* 00E8  SQ vf31, 63                     */
    LQ(v, 24, MXYZW, 64);   /* 00F0  LQ vf24, 64  - Licht-X          */
    LQ(v, 25, MXYZW, 65);   /* 00F8  LQ vf25, 65  - Licht-Y          */
    LQ(v, 26, MXYZW, 66);   /* 0100  LQ vf26, 66  - Licht-Z          */
    LQ(v, 12, MXYZW, 68);   /* 0108  LQ vf12, 68  - fNearRR          */
    LQ(v, 13, MXYZW, 69);   /* 0110  LQ vf13, 69  - fFarRR           */
    LQ(v, 27, MXYZW, 70);   /* 0118  LQ vf27, 70  - fI               */
    ILW(v, 10, 67, 0);      /* 0120  ILW.x vi10, 67 - Lichtbitmaske  */
    IADDIU(v, 4, 0, 16);    /* 0128  vi04 = 16   - Zustand Licht 3   */
    IADDIU(v, 5, 0, 32);    /* 0130  vi05 = 32   - Zustand Licht 2   */
    IADDIU(v, 6, 0, 64);    /* 0138  vi06 = 64   - Zustand Licht 1   */
    IADDIU(v, 7, 0, 128);   /* 0140  vi07 = 128  - Zustand Licht 0   */
    IADDIU(v, 8, 0, 8);     /* 0148  vi08 = 8    - Modus Licht 0     */
    IADDIU(v, 9, 0, 240);   /* 0150  vi09 = 240  - Vorzeichenmaske   */
    ISW(v, 0, 71, 3);       /* 0158  ISW.w vi00, 71 (Ende-Bit)       */
    IADDIU(v, 11, 0, 63);   /* 0160  vi11 = 63   - Clipakkumulator   */
}

/* VU0_CALCPOINT_END, ps2_vu0.vsm 0x0168..0x0198: stellt die Aufruferregister
 * wieder her (vf17 ohne die Projektionsskalierung, vf24..27 Clipmatrix). */
static void calcpoint_end(RECVX_VU0* v)
{
    LQ(v, 17, MXYZW, 49);   /* 0168 */
    LQ(v, 24, MXYZW, 56);   /* 0170 */
    LQ(v, 25, MXYZW, 57);   /* 0178 */
    LQ(v, 26, MXYZW, 58);   /* 0180 */
    LQ(v, 27, MXYZW, 59);   /* 0188 */
    nop(v, 2);              /* 0190 NOP[E], 0198 NOP */
}

/* Ein Licht aus LIGHT0..LIGHT3 (ps2_vu0.vsm 0x0288..0x0600). Die vier Blöcke
 * sind bis auf das Feld (x,y,z,w = Licht 0..3) und die Zustandsmaske identisch;
 * `c` ist das Feld, `state_mask` vi07/vi06/vi05/vi04 fuer die Vorzeichenflags. */
static void calcpoint_light(RECVX_VU0* v, int c, int state_mask)
{
    int apply, attenuate;

    /* 0288  MULx.xyz vf10, vf19, vf27<c> | RSQRT Q, vf00w, vf05<c> */
    MUL(v, 10, MXYZ, 19, 27, c);
    RSQRT(v, 0, 3, 5, c);
    /* 0290  IAND vi01, vi14, vi07   - Zustandsbit des Lichts */
    IAND(v, 1, 14, 7);
    /* 0298  IAND vi15, vi14, vi08   - Modusbit des Lichts */
    IAND(v, 15, 14, 8);
    /* 02A0  IBEQ vi01, vi00, LIGHTn+1 ; Delay-Slot 02A8 IADD vi14, vi14, vi14 */
    apply = v->vi[1] != 0;
    nop(v, 1);
    IADD(v, 14, 14, 14);
    if (!apply) return;             /* Beitrag bleibt 0 (vf08 wurde geloescht) */
    /* 02B0  IBNE vi15, vi00, LIGHTn_POINT ; Delay-Slot 02B8 NOP */
    nop(v, 2);
    if (v->vi[15] == 0)
    {
        /* LIGHT0_PARA 02C0..02F0: vf08<c> = dot(Lichtrichtung, Normale * fI),
         * dann MAXx auf 0. Die Normale ist in vf10 schon mit fI skaliert. */
        MULA(v, 1 << c, 24, 10, 0);   /* 02C0  MULAx.<c>  ACC, vf24, vf10x */
        MADDA(v, 1 << c, 25, 10, 1);  /* 02C8  MADDAy.<c> ACC, vf25, vf10y */
        MADD(v, 8, 1 << c, 26, 10, 2);/* 02D0  MADDz.<c>  vf08, vf26, vf10z */
        nop(v, 2);                    /* 02D8, 02E0 */
        nop(v, 1);                    /* 02E8  IBEQ vi00, vi00, LIGHTn+1 */
        MAXF(v, 8, 1 << c, 8, 0, 0);  /* 02F0  MAXx.<c> vf08, vf08, vf00x (Delay-Slot) */
        return;
    }
    /* LIGHT0_POINT 02F8..0360. vi13 traegt "Licht faellt weg"
     * (Vorzeichen von dot ODER von fFarRR - r2), vi12 "daempfen"
     * (Vorzeichen von fNearRR - r2); siehe die FMAND-Stellen unten. */
    IAND(v, 1, 13, state_mask);       /* 02F8  IAND vi01, vi13, vi0x */
    IAND(v, 15, 12, state_mask);      /* 0300  IAND vi15, vi12, vi0x */
    /* 0308  IBNE vi01, vi00, LIGHTn+1 ; Delay-Slot 0310 NOP */
    apply = v->vi[1] == 0;
    nop(v, 2);
    if (!apply) return;
    nop(v, 1);                        /* 0318 */
    /* 0320  IBEQ vi15, vi00, LIGHTn+1 ; Delay-Slot 0328 */
    attenuate = v->vi[15] != 0;
    nop(v, 1);
    MULQ(v, 8, 1 << c, 4);            /* 0328  MULq.<c> vf08, vf04, Q (Delay-Slot) */
    DIV(v, 12, c, 5, c);              /*       | DIV Q, vf12<c>, vf05<c> */
    if (!attenuate) return;
    nop(v, 6);                        /* 0330..0358 */
    MULQ(v, 8, 1 << c, 8);            /* 0360  MULq.<c> vf08, vf08, Q */
}

/* VU0_CALCPOINT, ps2_vu0.vsm 0x01A0..0x0678.
 * Eingang: vf04 = Position (xyz), vf05 = Normale (xyz) des Chunk-Vertex.
 * Ausgang: vf07 = Lichtfarbe, vf18 = Sichtraumposition, vf06 = (Sx, Sy, 1/z,
 * Nz), vf11.x = 1/z fuer njCalcFogPowerEx, vi11 = Clipakkumulator. */
static void calcpoint(RECVX_VU0* v)
{
    MULA(v, MXYZW, 28, 4, 0);       /* 01A0  MULAx.xyzw  ACC, vf28, vf04x | MOVE.z vf06, vf01 */
    MOVEF(v, 6, MZ, 1);
    MADDA(v, MXYZW, 29, 4, 1);      /* 01A8  MADDAy.xyzw ACC, vf29, vf04y */
    MADDA(v, MXYZW, 30, 4, 2);      /* 01B0  MADDAz.xyzw ACC, vf30, vf04z */
    MADD(v, 18, MXYZW, 31, 0, 3);   /* 01B8  MADDw.xyzw  vf18, vf31, vf00w  -> Sichtraum */
    MULA(v, MXYZ, 28, 5, 0);        /* 01C0  MULAx.xyz   ACC, vf28, vf05x */
    MADDA(v, MXYZ, 29, 5, 1);       /* 01C8  MADDAy.xyz  ACC, vf29, vf05y */
    MADD(v, 19, MXYZ, 30, 5, 2);    /* 01D0  MADDz.xyz   vf19, vf30, vf05z  -> Normale */
    MUL(v, 6, MXY, 18, 17, -1);     /* 01D8  MUL.xy vf06, vf18, vf17 | DIV Q, vf00w, vf18z */
    DIV(v, 0, 3, 18, 2);
    SUB(v, 8, MXYZW, 8, 8, -1);     /* 01E0  SUB.xyzw vf08, vf08, vf08  -> Beitraege auf 0 */
    SUB(v, 9, MXYZW, 24, 18, 0);    /* 01E8  SUBx.xyzw vf09, vf24, vf18x | LQ vf28, 56 */
    LQ(v, 28, MXYZW, 56);
    SUB(v, 10, MXYZW, 25, 18, 1);   /* 01F0  SUBy.xyzw vf10, vf25, vf18y | LQ vf29, 57 */
    LQ(v, 29, MXYZW, 57);
    SUB(v, 11, MXYZW, 26, 18, 2);   /* 01F8  SUBz.xyzw vf11, vf26, vf18z | LQ vf30, 58 */
    LQ(v, 30, MXYZW, 58);
    LQ(v, 31, MXYZW, 59);           /* 0200  LQ vf31, 59  -> Clipmatrix in vf28..31 */
    MUL(v, 6, MW, 0, 19, 2);        /* 0208  MULz.w vf06, vf00, vf19z  -> Nz */
    MULQ(v, 6, MXYZ, 6);            /* 0210  MULq.xyz vf06, vf06, Q    -> Sx, Sy, 1/z */
    MULA(v, MXYZW, 9, 9, -1);       /* 0218  MULA.xyzw  ACC, vf09, vf09 */
    MADDA(v, MXYZW, 10, 10, -1);    /* 0220  MADDA.xyzw ACC, vf10, vf10 */
    MADD(v, 5, MXYZW, 11, 11, -1);  /* 0228  MADD.xyzw  vf05, vf11, vf11 -> r² je Licht */
    MULA(v, MXYZW, 9, 19, 0);       /* 0230  MULAx.xyzw  ACC, vf09, vf19x */
    MADDA(v, MXYZW, 10, 19, 1);     /* 0238  MADDAy.xyzw ACC, vf10, vf19y */
    MADD(v, 4, MXYZW, 11, 19, 2);   /* 0240  MADDz.xyzw  vf04, vf11, vf19z -> dot je Licht */
    SUB(v, 0, MXYZW, 12, 5, -1);    /* 0248  SUB.xyzw vf00, vf12, vf05  -> Flags fNearRR - r² */
    SUB(v, 0, MXYZW, 13, 5, -1);    /* 0250  SUB.xyzw vf00, vf13, vf05  -> Flags fFarRR - r² */
    nop(v, 1);                      /* 0258 */
    FMAND(v, 13, 9);                /* 0260  FMAND vi13, vi09 - Flags von 0240: dot < 0 */
    FMAND(v, 12, 9);                /* 0268  FMAND vi12, vi09 - Flags von 0248: r² > fNearRR */
    FMAND(v, 1, 9);                 /* 0270  FMAND vi01, vi09 - Flags von 0250: r² > fFarRR */
    IOR(v, 13, 13, 1);              /* 0278  IOR vi13, vi13, vi01 -> "Licht faellt weg" */
    IADD(v, 14, 10, 0);             /* 0280  IADD vi14, vi10, vi00 -> Arbeitskopie der Bits */

    calcpoint_light(v, 0, 7);       /* LIGHT0 ... LIGHT0_POINT */
    calcpoint_light(v, 1, 6);       /* LIGHT1 */
    calcpoint_light(v, 2, 5);       /* LIGHT2 */
    calcpoint_light(v, 3, 4);       /* LIGHT3 */

    /* LIGHT4 0608..0678: Clipprobe mit der Clipmatrix und Farbsumme. */
    MULA(v, MXYZW, 28, 18, 0);      /* 0608  MULAx.xyzw  ACC, vf28, vf18x | LQ.xyz vf07, 71 */
    LQ(v, 7, MXYZ, 71);
    MADDA(v, MXYZW, 29, 18, 1);     /* 0610  MADDAy.xyzw ACC, vf29, vf18y | LQ.xyz vf09, 72 */
    LQ(v, 9, MXYZ, 72);
    MADDA(v, MXYZW, 30, 18, 2);     /* 0618  MADDAz.xyzw ACC, vf30, vf18z | LQ.xyz vf10, 73 */
    LQ(v, 10, MXYZ, 73);
    MADD(v, 5, MXYZW, 31, 0, 3);    /* 0620  MADDw.xyzw  vf05, vf31, vf00w | LQ.xyz vf11, 74 */
    LQ(v, 11, MXYZ, 74);
    MULA(v, MXYZ, 7, 8, 0);         /* 0628  MULAx.xyz   ACC, vf07, vf08x | LQ vf28, 60 */
    LQ(v, 28, MXYZW, 60);
    MADDA(v, MXYZ, 9, 8, 1);        /* 0630  MADDAy.xyz  ACC, vf09, vf08y | LQ vf29, 61 */
    LQ(v, 29, MXYZW, 61);
    MADDA(v, MXYZ, 10, 8, 2);       /* 0638  MADDAz.xyz  ACC, vf10, vf08z | LQ vf30, 62 */
    LQ(v, 30, MXYZW, 62);
    CLIPW(v, 5, 5);                 /* 0640  CLIPw.xyz   vf05, vf05w      | LQ vf31, 63 */
    LQ(v, 31, MXYZW, 63);
    MADD(v, 7, MXYZ, 11, 8, 3);     /* 0648  MADDw.xyz   vf07, vf11, vf08w -> Lichtfarbe */
    ADD(v, 11, MX, 0, 6, 2);        /* 0650  ADDz.x      vf11, vf00, vf06z -> 1/z fuer Fog */
    nop(v, 1);                      /* 0658 */
    FCGET(v, 1);                    /* 0660  FCGET vi01 */
    IAND(v, 11, 11, 1);             /* 0668  IAND vi11, vi11, vi01 */
    nop(v, 2);                      /* 0670 NOP[E], 0678 NOP */
}

void recvx_vu0_vcallms(unsigned int address)
{
    switch (address)
    {
    case RECVX_VU0_CALCPOINT_INIT: calcpoint_init(&vu0); return;
    case RECVX_VU0_CALCPOINT:      calcpoint(&vu0);      return;
    case RECVX_VU0_CALCPOINT_END:  calcpoint_end(&vu0);  return;
    default:
        /* Nicht portiertes Mikroprogramm: als Luecke melden, nicht stillschweigend
         * ueberspringen (VU0_MINMAX, VU0_WAVE_*, VU0_CLIP_VIEW_VOLUME*,
         * VU0_LOAD_SCISSOR_WORK*, VU0_SET_NODE_ARRAY, VU0_CALC_COLOR, ...). */
        vu0.unsupported++;
        vu0.last_unsupported = address;
        return;
    }
}

/* ------------------------------------------- Makromodus-Bloecke der Decomp */

/* ps2_NinjaCnk.c:1351 (njCnkCvVnPs2): laedt die elf Lichtzeilen nach vf04..vf14.
 *   lqc2 vf4, 0(v1) ... lqc2 vf14, 0xA0(v1) */
void recvx_vu0_cnk_load_lights(const float v[11][4])
{
    int i;
    for (i = 0; i < 11; ++i) memcpy(vu0.vf[4 + i], v[i], 16);
}

/* ps2_NinjaCnk.c:1832 (_CVV), ELF 0x002D1CD0: transformiert einen Punkt mit der
 * Clipmatrix in vf24..vf27 und liefert das CLIP-Register.
 *   lqc2 vf4, 0(a0) / vmulax.xyzw ACC, vf24, vf4x / vmadday ... vmaddaz ...
 *   vmaddw.xyzw vf5, vf27, vf0w / vclipw.xyz vf5, vf5w / 5x vnop / cfc2 vi18 */
int recvx_vu0_cvv(const float* v0)
{
    RECVX_VU0* v = &vu0;
    recvx_vu0_lqc2(4, v0);
    MULA(v, MXYZW, 24, 4, 0);
    MADDA(v, MXYZW, 25, 4, 1);
    MADDA(v, MXYZW, 26, 4, 2);
    MADD(v, 5, MXYZW, 27, 0, 3);
    CLIPW(v, 5, 5);
    nop(v, 5);
    return (int)v->clip;
}
