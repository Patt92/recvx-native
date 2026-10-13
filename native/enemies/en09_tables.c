/* EN09 (Bandersnatch) — Datentabellen. Werte woertlich aus den
   Originalfeldern; Fundstelle steht an jeder Tabelle. Keine Funktionskoerper.
   Die sechs Armlaengenfelder stehen in en09_arm_tables.c. */
#include "en09_tables.h"
#include <stddef.h>

/* --- en09.c:40 en09_mtn_tbl[28] -------------------------------------------
   Fussfixierung je Motion: bis zu drei (type, s_frm, e_frm)-Abschnitte.
   bhEne09_MtnTblPlay (en09.c:7576-7644) laeuft die Tabelle bis `no == -1`,
   vergleicht `no` mit mtn_no und nimmt den ERSTEN Abschnitt, in dessen
   [s_frm, e_frm] der Frame liegt (`break` nach dem Treffer).

   ORIGINALER QUIRK (Zeilen 52/53 und 55/56): bei no 0x20 und 0x3E beginnt der
   dritte Abschnitt bei 0x0D und liegt damit INNERHALB des zweiten (0x0C..0x14).
   Weil der erste Treffer gewinnt, ist der dritte Abschnitt nur fuer die Frames
   0x15..0x23 erreichbar. Unveraendert uebernommen.

   Der Quirk gilt auch fuer no 0x21/0x3F: zweiter Abschnitt 0x0E..0x30 beginnt
   auf demselben Frame, auf dem der erste endet (0x00..0x0E) — Frame 0x0E
   trifft daher immer den ersten. */
const RECVX_EN09_MTN_FMTN recvx_en09_mtn_tbl[RECVX_EN09_MTN_TBL_COUNT] = {
    { 0x00, { {  8, 0x00, 0x13 }, {  7, 0x14, 0x31 }, {  8, 0x32, 0x3B } } },
    { 0x01, { {  0, 0x00, 0x3B }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x02, { {  1, 0x00, 0x3B }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x2D, { {  0, 0x00, 0x05 }, {  1, 0x06, 0x2D }, { -1, 0x00, 0x00 } } },
    { 0x31, { {  8, 0x00, 0x28 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x39, { {  8, 0x00, 0x09 }, {  7, 0x0A, 0x5A }, { -1, 0x00, 0x00 } } },
    { 0x3A, { {  0, 0x00, 0x21 }, {  8, 0x22, 0x3B }, {  0, 0x3C, 0x4B } } },
    { 0x45, { {  8, 0x00, 0x23 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x3B, { {  8, 0x00, 0x1F }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x1D, { {  1, 0x21, 0x45 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x20, { {  1, 0x00, 0x0B }, {  0, 0x0C, 0x14 }, {  1, 0x0D, 0x23 } } },
    { 0x3E, { {  1, 0x00, 0x0B }, {  0, 0x0C, 0x14 }, {  1, 0x0D, 0x23 } } },
    { 0x21, { {  0, 0x00, 0x0E }, {  1, 0x0E, 0x30 }, { -1, 0x00, 0x00 } } },
    { 0x3F, { {  0, 0x00, 0x0E }, {  1, 0x0E, 0x30 }, { -1, 0x00, 0x00 } } },
    { 0x22, { {  1, 0x00, 0x0A }, {  0, 0x0B, 0x18 }, {  1, 0x19, 0x1E } } },
    { 0x23, { {  0, 0x00, 0x15 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x16, { {  1, 0x00, 0x17 }, {  0, 0x18, 0x19 }, { -1, 0x00, 0x00 } } },
    { 0x17, { {  0, 0x00, 0x19 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x3D, { {  0, 0x00, 0x19 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x06, { {  0, 0x00, 0x12 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x15, { {  0, 0x00, 0x06 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x09, { {  6, 0x00, 0x06 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x0C, { {  6, 0x00, 0x04 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x07, { {  1, 0x00, 0x12 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x0A, { {  6, 0x00, 0x04 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x0E, { {  6, 0x00, 0x0A }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    { 0x10, { {  1, 0x00, 0x34 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
    {   -1, { { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 }, { -1, 0x00, 0x00 } } },
};

/* --- en09.c:72 en09_mtn_tbl2[24] -----------------------------------------
   SE je Frame: bhEne09_MtnTblPlay ruft bhEne09_SePlay(epw, act), sobald der
   Frame genau `frm` ist (en09.c:7648-7666). `frm == -1` beendet die Liste
   eines Eintrags, `no == -1` die Tabelle. `act` ist die vollstaendige
   SE-Nummer, die ueber RequestEnemySe geht (en09.c:7698). */
const RECVX_EN09_MTN_ATB recvx_en09_mtn_tbl2[RECVX_EN09_MTN_TBL2_COUNT] = {
    {  0, { { 19, 0x00012300 }, { 49, 0x00012300 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 45, { {  0, 0x0101130C }, { 10, 0x00012304 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 49, { {  5, 0x0101130C }, { 13, 0x00012304 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 22, { { 29, 0x0000230D }, { 33, 0x0000230D }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 23, { { 13, 0x0000230D }, { 18, 0x0000230D }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 16, { { 14, 0x00012304 }, { 17, 0x00012305 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    {  6, { { 13, 0x00012304 }, { 15, 0x00012305 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    {  7, { { 14, 0x00012304 }, { 17, 0x00012305 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 43, { { 12, 0x00012304 }, { 19, 0x00012305 }, { 27, 0x00012301 }, { 75, 0x00012302 }, { -1, 0 } } },
    { 44, { { 12, 0x00012304 }, { 16, 0x00012306 }, { 25, 0x00012301 }, { 47, 0x00012302 }, { -1, 0 } } },
    { 17, { { 12, 0x00012304 }, { 15, 0x00011309 }, { 51, 0x0001130A }, { 83, 0x0001130B }, { -1, 0 } } },
    { 48, { { 59, 0x0000230D }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 21, { {  0, 0x00012306 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    {  9, { {  0, 0x00012301 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 12, { {  9, 0x00012302 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 19, { {  0, 0x00012306 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 10, { {  0, 0x00012301 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 14, { {  3, 0x00012303 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 15, { { 10, 0x00012302 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 50, { {  0, 0x0101130C }, { 10, 0x00012304 }, { 15, 0x00011309 }, { 40, 0x0001130A }, { 82, 0x0001130B } } },
    { 57, { {  0, 0x0101130C }, { 10, 0x00012304 }, { 15, 0x00011309 }, { 27, 0x0001130A }, { 59, 0x0001130A } } },
    { 58, { { 23, 0x01002310 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 59, { {  0, 0x00011309 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { -1, { { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
};

/* --- en09.c:315 en09_mtn_tbl3[5] -----------------------------------------
   Vibration je Frame, nur solange mode0 < 5 (en09.c:7668); `act` geht an
   bhEne_SetVibration (en09.c:7680).

   ORIGINALER QUIRK (en09.c:321): im Abschlusseintrag ist das FUENFTE Paar
   { 0, 0 } statt { -1, 0 }. Es ist unerreichbar, weil `no == -1` die
   aeussere Schleife vorher beendet. Unveraendert uebernommen. */
const RECVX_EN09_MTN_ATB recvx_en09_mtn_tbl3[RECVX_EN09_MTN_TBL3_COUNT] = {
    { 0x32, { { 0x0F, 0x01 }, { 0x2E, 0x00 }, { 0x52, 0x01 }, { -1, 0 }, { -1, 0 } } },
    { 0x39, { { 0x0F, 0x01 }, { 0x1C, 0x00 }, { 0x2C, 0x00 }, { 0x40, 0x00 }, { -1, 0 } } },
    { 0x3A, { { 0x17, 0x01 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 0x3B, { { 0x00, 0x01 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    {   -1, { {   -1, 0x00 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { 0x00, 0 } } },
};

/* --- en09.c:324 mtn_chg_tbl[17] (CHK_TBL_WORK) ----------------------------
   Motionwechsel-Bedingungen: je (mtn_no, s_frm, e_frm) vier CHG_TBL_WORK-
   Zeilen (mtn_no, type, frm_no, h_cnt, mode). Der Verbraucher ist noch nicht
   portiert; die Werte stehen hier vollstaendig und unveraendert.
   `mtn_no == -1` beendet die Tabelle. frm_no ist 16.16 wie im Original
   (2883584 = 44<<16, 1441792 = 22<<16, 1310720 = 20<<16, 327680 = 5<<16). */
const RECVX_EN09_CHK_TBL recvx_en09_chg_mtn_tbl[RECVX_EN09_CHG_TBL_COUNT] = {
    { 22,  0, 39, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 23,  0, 34, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 25,  0, 24, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 53,  0, 57, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 60,  0, 39, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 61,  0, 34, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 39,  0, 43, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 56,  0, 39, { { -1, 0, 0, 0u, 0x000A0003 }, { -1, 0, 0, 0u, 0x000A0003 },
                    { -1, 0, 0, 0u, 0xFFFFFFFF }, { -1, 0, 0, 0u, 0xFFFFFFFF } } },
    { 43, 28, 69, { { -1, 0, 0, 0u, 0x00080003 }, { -1, 0, 0, 0u, 0x00080003 },
                    { -1, 0, 0, 0u, 0x00080003 }, { -1, 0, 0, 0u, 0x00080003 } } },
    { 43, 70, 90, { { -1, 0, 0, 0u, 0x00030003 }, { -1, 0, 0, 0u, 0x00040003 },
                    { -1, 0, 0, 0u, 0x00030003 }, { -1, 0, 0, 0u, 0x00040003 } } },
    { 44,  0, 30, { { -1, 0, 0, 0u, 0x00030003 }, { -1, 0, 0, 0u, 0x00040003 },
                    { -1, 0, 0, 0u, 0x00030003 }, { -1, 0, 0, 0u, 0x00040003 } } },
    { 44, 31, 65, { { -1, 0, 0, 0u, 0x00090003 }, { -1, 0, 0, 0u, 0x00090003 },
                    { -1, 0, 0, 0u, 0x00090003 }, { -1, 0, 0, 0u, 0x00090003 } } },
    { 54,  0, 60, { { 53, 1, 2883584, 10u, 0x020A0003 }, { 53, 1, 2883584, 10u, 0x020A0003 },
                    { 53, 1, 2883584, 10u, 0x01040003 }, { 53, 1, 2883584, 10u, 0x01040003 } } },
    { 54, 61, 79, { { 53, 1, 1441792, 10u, 0x01040003 }, { 53, 1, 1441792, 10u, 0x01040003 },
                    { 53, 1, 1441792, 10u, 0x01040003 }, { 53, 1, 1441792, 10u, 0x01040003 } } },
    { 29,  0, 39, { { 23, 1, 1310720, 10u, 0x020A0003 }, { 23, 1, 1310720, 10u, 0x020A0003 },
                    { 23, 1, 1310720, 10u, 0x01030003 }, { 23, 1, 1310720, 10u, 0x01030003 } } },
    { 29, 40, 59, { { 23, 1, 327680, 10u, 0x01030003 }, { 22, 0, 327680, 10u, 0x01040003 },
                    { 23, 1, 327680, 10u, 0x01030003 }, { 22, 0, 327680, 10u, 0x01040003 } } },
    { -1,  0,  0, { {  0, 0, 0, 0u, 0x00000000 }, { 0, 0, 0, 0u, 0x00000000 },
                    {  0, 0, 0, 0u, 0x00000000 }, { 0, 0, 0, 0u, 0x00000000 } } },
};

/* --- en09.c:464 En09_WpnDamageTbl[22] (WPNDAMAGE_WORK, types.h:1931) ------
   Je Waffennummer: flg, nm_act, nm_blood, cb_act, cb_blood. */
const RECVX_EN09_WPNDAMAGE recvx_en09_wpn_damage[RECVX_EN09_WPNDAMAGE_COUNT] = {
    { 0x00, 0, 0, 0, 0 },
    { 0x00, 0, 0, 0, 0 },
    { 0x0A, 4, 3, 3, 3 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x0A, 2, 7, 3, 3 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x02, 2, 9, 3, 9 },
    { 0x0A, 4, 7, 3, 3 },
    { 0x02, 2, 3, 3, 3 },
    { 0x12, 2, 1, 3, 3 },
    { 0xC2, 2, 1, 3, 3 },
    { 0x52, 2, 1, 3, 3 },
    { 0x02, 2, 1, 3, 3 },
    { 0x60, 2, 1, 3, 3 },
    { 0x12, 2, 1, 3, 3 },
    { 0x60, 2, 1, 3, 3 },
    { 0x0A, 2, 1, 3, 3 },
};

/* --- en09.c:490 CombWepTbl[21] (COMBWEP_WORK, zonzon1.h:28) ---------------
   crit, pt[3], timeout, bonus — Eingabe von bhEne_CalcDamage. */
const RECVX_EN09_COMBWEP recvx_en09_comb_wep[RECVX_EN09_COMBWEP_COUNT] = {
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   5, { 0, 0, 0 }, 120, 5 },
    {   5, { 0, 0, 0 },  60, 0 },
    {   5, { 0, 0, 0 },  60, 0 },
    {   5, { 0, 0, 0 },  60, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   5, { 0, 0, 0 },  60, 0 },
    { 180, { 0, 0, 0 },  30, 0 },
    {   5, { 0, 0, 0 },   0, 0 },
    {   5, { 0, 0, 0 },  60, 0 },
    {   4, { 0, 0, 0 },  60, 0 },
    { 100, { 0, 0, 0 },  30, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
    {   0, { 0, 0, 0 },   0, 0 },
};

/* --- en09.c:515 CombJointTbl[24] (COMBJOINT_WORK, zonzon1.h:21) -----------
   Im Original vollstaendig mit `= { 0 }` initialisiert, also 24 Nullzeilen.
   Das ist kein fehlender Datensatz, sondern der Originalinhalt. */
const RECVX_EN09_COMBJOINT recvx_en09_comb_joint[RECVX_EN09_COMBJOINT_COUNT] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
};

/* --- en09.c:517 Ene09CapColTab[19] (CPCL) ---------------------------------
   (Knoten1, Knoten2, Radius) der Trefferkapseln; bhEne09_Init haengt sie an
   epw->cpcl (en09.c:2428). Die Laenge steht beim Verbraucher
   (weapon.c bhCheckCapCol2Capsule), nicht als Terminator. */
const RECVX_ENEMY_CPCL recvx_en09_cap_col[RECVX_EN09_CAPCOL_COUNT] = {
    {   1,  2, 16 },
    {   3,  3, 22 },
    {   0,  8,  0 },
    {   5,  5, 22 },
    { -12, 20,  0 },
    {  17, 17, 22 },
    {  20, 23,  0 },
    {   4,  4, 12 },
    {   0, 11, -2 },
    {  10, 14, 11 },
    {   6,  7, 14 },
    {   7,  8, 10 },
    {   8,  9, 10 },
    {   9, 10, 10 },
    {  18, 19,  7 },
    {  19, 20,  6 },
    {  21, 22, 10 },
    {  22, 23,  6 },
    {   0,  0,  0 },
};

/* --- en09.c:540 en09_BldTbl (BP_WORK, types.h:2210) ----------------------- */
const RECVX_ENEMY_BP_WORK recvx_en09_bld_tbl = {
    { 0.0f, 0.1f, 0.0f },
    0,
    0.0f,
    0.06f,
    { 0.5f, 0.7f, 0.4f, 1.0f, 1.0f },
    { 1, 3, 8, 11, 15 }
};

/* --- en09.c:550 En09SdwTab[6] --------------------------------------------
   Gelenkkette des Schattens, im Original `char` mit 0xFF als Abschluss;
   nativ als int8_t mit -1, wie es `set_shadow` (enemy_work.h) erwartet.
   bhEne09_Init ruft bhSetShadow(En09SdwTab, epw, 1, 5.0f, 4.0f, 4.0f)
   (en09.c:2432). */
const int8_t recvx_en09_sdw_tab[6] = { 0x04, 0x0E, 0x11, 0x14, 0x17, -1 };

/* --- en09.c:555 en09prt_blood_tbl[24] (BT_WORK, zonzon.h:14) -------------- */
const RECVX_EN09_BT recvx_en09_prt_blood_tbl[RECVX_EN09_BLOOD_TBL_COUNT] = {
    {  0, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    {  1, 0.0f, -1.0f, 1.8f, 1.0f, 1.0f, 2.5f, 0.8f },
    {  2, 0.0f,  1.0f, 2.0f, 1.0f, 1.0f, 2.5f, 0.8f },
    {  3, 0.0f,  2.0f, 2.0f, 1.0f, 1.0f, 2.5f, 0.8f },
    {  4, 0.0f,  1.0f, 1.0f, 0.5f, 0.5f, 2.0f, 0.8f },
    {  5, 0.0f,  0.0f, 1.2f, 0.0f, 0.5f, 1.0f, 1.0f },
    {  6, 0.0f,  0.0f, 2.0f, 1.0f, 0.5f, 2.5f, 0.8f },
    {  7, 0.0f,  0.8f, 0.5f, 0.5f, 0.0f, 2.0f, 0.8f },
    {  8, 0.0f,  0.8f, 0.5f, 0.5f, 0.0f, 2.0f, 0.5f },
    {  9, 0.0f,  1.0f, 0.5f, 0.5f, 0.0f, 2.0f, 0.5f },
    { 10, 0.0f,  0.8f, 0.5f, 0.5f, 0.0f, 2.0f, 0.5f },
    { 11, 0.0f,  0.0f, 0.5f, 0.2f, 0.5f, 1.0f, 0.0f },
    { 12, 0.0f,  0.0f, 0.5f, 0.2f, 0.5f, 1.0f, 0.0f },
    { 13, 0.0f,  0.0f, 0.5f, 0.2f, 0.5f, 1.0f, 0.0f },
    { 14, 0.0f,  0.0f, 0.5f, 0.2f, 0.5f, 1.0f, 0.0f },
    { 15, 0.0f,  0.0f, 0.5f, 0.2f, 0.5f, 1.0f, 0.0f },
    { 16, 0.0f,  0.0f, 0.5f, 0.2f, 0.5f, 1.0f, 0.0f },
    { 17, 0.0f,  0.0f, 1.2f, 0.5f, 0.0f, 1.0f, 1.0f },
    { 18, 0.0f, -2.0f, 1.0f, 0.2f, 1.0f, 2.0f, 0.8f },
    { 19, 0.0f, -1.0f, 1.0f, 0.2f, 1.0f, 1.5f, 0.5f },
    { 20, 0.0f,  0.0f, 1.0f, 0.2f, 0.5f, 1.5f, 0.5f },
    { 21, 0.0f, -1.0f, 1.0f, 0.2f, 1.0f, 2.0f, 0.8f },
    { 22, 0.0f, -2.0f, 1.0f, 0.2f, 1.0f, 2.5f, 0.5f },
    { 23, 0.0f,  0.0f, 1.0f, 0.2f, 0.5f, 1.5f, 0.5f },
};

/* --- en09.c:583 en09_mtn23[35] ------------------------------------------- */
const float recvx_en09_mtn23[RECVX_EN09_MTN23_COUNT] = {
    0.0f,
    0.8460479975f,
    0.9654740095f,
    0.8990900517f,
    0.7489609718f,
    0.6820127964f,
    0.6796393394f,
    0.7418408394f,
    0.8686180115f,
    0.9701399803f,
    1.013323784f,
    1.071469307f,
    1.144568443f,
    1.232629776f,
    1.335645676f,
    0.2317342758f,
    0.207318306f,
    0.1867198944f,
    0.1699371338f,
    0.1569738388f,
    0.1478261948f,
    0.142496109f,
    0.1409835815f,
    0.1288032532f,
    0.1071634293f,
    0.09115505219f,
    0.08077430725f,
    0.07602500916f,
    0.07888698578f,
    0.08192253113f,
    0.07942676544f,
    0.07140254974f,
    0.05784797668f,
    0.03876399994f,
    0.01414966583f,
};

/* --- en09.c:7383 en09_mogmog[40] -----------------------------------------
   Formkurve: bhEne09_SetMtn setzt shp_ct = mogmog[(++EXP0_UC(0x13) & 0xFF) % 40]
   (en09.c:7505). Die letzten zehn Eintraege sind 0. */
const float recvx_en09_mogmog[RECVX_EN09_MOGMOG_COUNT] = {
     100.0f,  200.0f,  300.0f,  400.0f,  500.0f,
     400.0f,  300.0f,  200.0f,  100.0f,    0.0f,
     100.0f,  200.0f,  300.0f,  400.0f,  500.0f,
     600.0f,  700.0f,  800.0f,  900.0f, 1000.0f,
     900.0f,  800.0f,  700.0f,  600.0f,  500.0f,
     400.0f,  300.0f,  200.0f,  100.0f,    0.0f,
       0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
       0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
};

/* --- en09.c:2016 en09_tree[4][16] ----------------------------------------
   Knotenketten fuer bhCalcFixOffset/bhEne_CalcPartsPos. Im Original `char`
   mit 0xFF als Abschluss; nativ int8_t mit -1, wie es die T3-Helfer
   (en03_lib.h, `const int8_t*`) erwarten. Die Nullen hinter dem Abschluss
   sind Fuellwerte des Originalfeldes und bleiben stehen.
     0 -> rechtes Bein (1, 0x15, 0x16, 0x17)
     1 -> linkes Bein  (1, 0x12, 0x13, 0x14)
     2 -> Rumpfkette   (1, 2, 3, 5..0x0A, 0x0D)
     3 -> Armkette     (1, 2, 3, 4)
   Die Zuordnung folgt den Aufrufstellen en09.c:2239/2240 (beide Beine),
   :2280 (Arm) und :2299 (Rumpf). */
const int8_t recvx_en09_tree[4][16] = {
    { 0x00, 0x01, 0x15, 0x16, 0x17, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x00, 0x01, 0x12, 0x13, 0x14, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0D, -1, 0, 0, 0, 0 },
    { 0x00, 0x01, 0x02, 0x03, 0x04, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* --- en09.c:2024 en09_hp_tbl[2][16] --------------------------------------
   bhEne09_Init waehlt hp = hp_tbl[gm_mode == 2 ? 1 : 0][rand() % 16]
   (en09.c:2352-2359). */
const int32_t recvx_en09_hp_tbl[2][16] = {
    { 105, 105, 105, 120, 120, 120, 120, 135, 135, 135, 135, 135, 150, 150, 150, 150 },
    {  75,  75,  75,  90,  90,  90,  90, 105, 105, 105, 105, 105, 120, 120, 120, 120 },
};

/* --- en09.c:32 En09_PlyMtn_OffsetTbl[4] ----------------------------------
   Basis-MTN je Spielerfigur (0x64, 0x6E, 0x78, 0x6E). Eintrag 3 ist derselbe
   wie Eintrag 1 — unveraendert uebernommen. */
const int32_t recvx_en09_ply_mtn_offset[4] = { 0x64, 0x6E, 0x78, 0x6E };

/* --- en09.c:2440-2447 clp_jno[0..7] --------------------------------------
   bhEne09_Init setzt diese acht Clip-Gelenke und lok_jno = 4 (en09.c:2452).
   Sie ueberschreiben die Werte, die bhSetEnemy vorher gesetzt hat
   (eneset.c:302). Keine eigene Originaltabelle, sondern acht Zuweisungen —
   hier als Feld, damit der Mechanismus sie nicht einzeln einstreut. */
const int8_t recvx_en09_clp_jno[8] = { 4, 0x11, 0x0F, 7, 8, 9, 0x14, 0x17 };

/* ---------------------------------------------------------------------------
   Datensatz
   --------------------------------------------------------------------------- */
static const RECVX_EN09_DATA* en09_data_bound;

RECVX_EN09_DATA recvx_en09_default_data(void)
{
    RECVX_EN09_DATA d;
    d.hp_tbl         = recvx_en09_hp_tbl;
    d.mtn_tbl        = recvx_en09_mtn_tbl;
    d.mtn_tbl2       = recvx_en09_mtn_tbl2;
    d.mtn_tbl3       = recvx_en09_mtn_tbl3;
    d.chg_mtn_tbl    = recvx_en09_chg_mtn_tbl;
    d.wpn_damage     = recvx_en09_wpn_damage;
    d.comb_wep       = recvx_en09_comb_wep;
    d.comb_joint     = recvx_en09_comb_joint;
    d.cap_col        = recvx_en09_cap_col;
    d.bld_tbl        = &recvx_en09_bld_tbl;
    d.sdw_tab        = recvx_en09_sdw_tab;
    d.prt_blood_tbl  = recvx_en09_prt_blood_tbl;
    d.mtn23          = recvx_en09_mtn23;
    d.mogmog         = recvx_en09_mogmog;
    d.tree           = recvx_en09_tree;
    d.ply_mtn_offset = recvx_en09_ply_mtn_offset;
    d.arm_pos        = recvx_en09_arm_pos;
    d.arm_pos2       = recvx_en09_arm_pos2;
    d.arm_pos3       = recvx_en09_arm_pos3;
    d.arm_pos4       = recvx_en09_arm_pos4;
    d.arm_pos5       = recvx_en09_arm_pos5;
    d.arm_pos6       = recvx_en09_arm_pos6;
    return d;
}

void recvx_en09_set_data(const RECVX_EN09_DATA* data) { en09_data_bound = data; }
const RECVX_EN09_DATA* recvx_en09_data(void) { return en09_data_bound; }
