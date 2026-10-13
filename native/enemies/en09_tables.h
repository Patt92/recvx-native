#ifndef RECVX_NATIVE_EN09_TABLES_H
#define RECVX_NATIVE_EN09_TABLES_H
#include <stdint.h>
#include "enemy_work.h"
#include "en09.h"

/* ===========================================================================
   EN09 (Bandersnatch) — Datentabellen
   ---------------------------------------------------------------------------
   Reine Daten-/Layouttabellen, keine Funktionskoerper (AGENTS.md 2026-10-09:
   "Reine Daten-/Layouttabellen sind keine generierten Funktionskoerper").
   Werte woertlich aus den Originalfeldern, ohne Umrechnung oder Auswahl;
   Struktur- und Feldreihenfolge wie im Original. Jede Tabelle traegt ihre
   Fundstelle an der Definition in en09_tables.c.

   Gleiche Aufteilung wie EN01 (native/enemies/en01_tables.h): die Tabellen
   stehen hier, der Mechanismus liest sie ueber einen gebundenen Datensatz
   (RECVX_EN09_DATA). Ein Satz mit NULL-Zeigern bleibt moeglich (Tests); die
   betroffenen Mechanismen melden dann ihre Luecke und setzen KEINEN
   Ersatzwert ein.
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
   Zeilentypen, die nicht schon in en09.h oder im gemeinsamen Vertrag stehen.
   Feldbilder woertlich aus types.h:1931 (WPNDAMAGE_WORK), zonzon1.h:28
   (COMBWEP_WORK), zonzon1.h:21 (COMBJOINT_WORK), zonzon.h:14 (BT_WORK),
   types.h:2210 (BP_WORK = RECVX_ENEMY_BP_WORK), CPCL = RECVX_ENEMY_CPCL.
   Die Belegung ist dieselbe, die native/enemies/en01_tables.h:164-168 und
   native/enemies/en03_lib.h:79/180 bereits aus denselben Kopfzeilen fuehren.
   --------------------------------------------------------------------------- */
typedef struct { int32_t flg, nm_act, nm_blood, cb_act, cb_blood; } RECVX_EN09_WPNDAMAGE;
typedef struct { int32_t crit; int32_t pt[3]; int32_t timeout; int32_t bonus; } RECVX_EN09_COMBWEP;
typedef struct { int32_t flg; int32_t correct; } RECVX_EN09_COMBJOINT;
typedef struct { int32_t lnk_obj; float x, y, z, xlen, ylen, size, len; } RECVX_EN09_BT;

/* ---------------------------------------------------------------------------
   Tabellenlaengen. Sie stehen hier als Konstanten, weil mehrere Mechanismen
   dieselbe Laenge brauchen und das Original die Listen teils ueber einen
   Terminator (-1) und teils ueber die Feldlaenge beendet.
   --------------------------------------------------------------------------- */
#define RECVX_EN09_MTN_TBL_COUNT      28u  /* en09.c:40,  -1 terminiert       */
#define RECVX_EN09_MTN_TBL2_COUNT     24u  /* en09.c:72,  -1 terminiert       */
#define RECVX_EN09_MTN_TBL3_COUNT      5u  /* en09.c:315, -1 terminiert       */
#define RECVX_EN09_CHG_TBL_COUNT      17u  /* en09.c:324, -1 terminiert       */
#define RECVX_EN09_WPNDAMAGE_COUNT    22u  /* en09.c:464                      */
#define RECVX_EN09_COMBWEP_COUNT      21u  /* en09.c:490                      */
#define RECVX_EN09_COMBJOINT_COUNT    24u  /* en09.c:515, im Original alle 0  */
#define RECVX_EN09_CAPCOL_COUNT       19u  /* en09.c:517                      */
#define RECVX_EN09_BLOOD_TBL_COUNT    24u  /* en09.c:555                      */
#define RECVX_EN09_MTN23_COUNT        35u  /* en09.c:583                      */
#define RECVX_EN09_MOGMOG_COUNT       40u  /* en09.c:7383                     */
#define RECVX_EN09_ARM_POS_COUNT      92u  /* en09.c:622                      */
#define RECVX_EN09_ARM_POS2_COUNT     67u  /* en09.c:718                      */
#define RECVX_EN09_ARM_POS3_COUNT    143u  /* en09.c:789,  mtn_no 2           */
#define RECVX_EN09_ARM_POS4_COUNT    128u  /* en09.c:936,  mtn_no 3           */
#define RECVX_EN09_ARM_POS5_COUNT    654u  /* en09.c:1068, mtn_no 4           */
#define RECVX_EN09_ARM_POS6_COUNT    185u  /* en09.c:1726, mtn_no 5           */

/* Dispatchbreiten (en09.c:1915-2014). Nur die Laengen und die Indexquelle,
   nicht die Funktionszeiger: der Port bestimmt die Zustaende selbst.
     bhEne09_Mode0[6]        Index epw->mode0  (5 = bhEne_Event)
     bhEne09_MoveType[11]    Index epw->type   (0..8 gleich, 9 und 10 eigen)
     bhEne09_DamageType[1] / NageType[1] / DieType[1]  Index fest 0
     bhEne09_BrainMode2[12]  Index epw->mode2  (0/1 Brain00, 2..11 DmmyBrain)
     bhEne09_MoveMode2[15]   Index epw->mode2  (MV00..MV14)
     bhEne09_NageMode2[2]    Index epw->mode2
     bhEne09_DamageMode2[11] Index epw->mode2  (DG00..DG10)
     bhEne09_DieMode2[1]     Index epw->mode2  (DD00) */
#define RECVX_EN09_MODE0_COUNT        6u
#define RECVX_EN09_MOVE_TYPE_COUNT   11u
#define RECVX_EN09_BRAIN_MODE2_COUNT 12u
#define RECVX_EN09_MOVE_MODE2_COUNT  15u
#define RECVX_EN09_NAGE_MODE2_COUNT   2u
#define RECVX_EN09_DMG_MODE2_COUNT   11u
#define RECVX_EN09_DIE_MODE2_COUNT    1u

/* bhEne09_BrainMode2[2..11] ist bhEne09_DmmyBrain, eine leere Originalfunktion
   (en09.c:2031). Nur mode2 0 und 1 denken; der Rest tut im Original nichts. */
#define RECVX_EN09_BRAIN_ACTIVE       2u

/* ---------------------------------------------------------------------------
   Die Felder selbst (definiert in en09_tables.c)
   --------------------------------------------------------------------------- */
extern const RECVX_EN09_MTN_FMTN   recvx_en09_mtn_tbl[RECVX_EN09_MTN_TBL_COUNT];
extern const RECVX_EN09_MTN_ATB    recvx_en09_mtn_tbl2[RECVX_EN09_MTN_TBL2_COUNT];
extern const RECVX_EN09_MTN_ATB    recvx_en09_mtn_tbl3[RECVX_EN09_MTN_TBL3_COUNT];
extern const RECVX_EN09_CHK_TBL    recvx_en09_chg_mtn_tbl[RECVX_EN09_CHG_TBL_COUNT];
extern const RECVX_EN09_WPNDAMAGE  recvx_en09_wpn_damage[RECVX_EN09_WPNDAMAGE_COUNT];
extern const RECVX_EN09_COMBWEP    recvx_en09_comb_wep[RECVX_EN09_COMBWEP_COUNT];
extern const RECVX_EN09_COMBJOINT  recvx_en09_comb_joint[RECVX_EN09_COMBJOINT_COUNT];
extern const RECVX_ENEMY_CPCL      recvx_en09_cap_col[RECVX_EN09_CAPCOL_COUNT];
extern const RECVX_ENEMY_BP_WORK   recvx_en09_bld_tbl;
extern const int8_t                recvx_en09_sdw_tab[6];
extern const RECVX_EN09_BT         recvx_en09_prt_blood_tbl[RECVX_EN09_BLOOD_TBL_COUNT];
extern const float                 recvx_en09_mtn23[RECVX_EN09_MTN23_COUNT];
extern const float                 recvx_en09_mogmog[RECVX_EN09_MOGMOG_COUNT];
extern const int8_t                recvx_en09_tree[4][16];
extern const int32_t               recvx_en09_hp_tbl[2][16];
extern const int32_t               recvx_en09_ply_mtn_offset[4];
extern const int8_t                recvx_en09_clp_jno[8];

/* Die sechs Armlaengenfelder stehen in en09_arm_tables.c, weil sie zusammen
   1269 Eintraege haben und die uebrigen Tabellen sonst darin untergehen. */
extern const RECVX_EN09_ARM_POS    recvx_en09_arm_pos[RECVX_EN09_ARM_POS_COUNT];
extern const RECVX_EN09_ARM_POS    recvx_en09_arm_pos2[RECVX_EN09_ARM_POS2_COUNT];
extern const RECVX_EN09_ARM_POS    recvx_en09_arm_pos3[RECVX_EN09_ARM_POS3_COUNT];
extern const RECVX_EN09_ARM_POS    recvx_en09_arm_pos4[RECVX_EN09_ARM_POS4_COUNT];
extern const RECVX_EN09_ARM_POS    recvx_en09_arm_pos5[RECVX_EN09_ARM_POS5_COUNT];
extern const RECVX_EN09_ARM_POS    recvx_en09_arm_pos6[RECVX_EN09_ARM_POS6_COUNT];

/* ---------------------------------------------------------------------------
   Datensatz, den der Mechanismus liest (gleiches Muster wie
   RECVX_EN01_DATA, native/enemies/en01_tables.h:280).
   --------------------------------------------------------------------------- */
typedef struct RECVX_EN09_DATA {
    /* en09.c:2024 en09_hp_tbl[2][16]; gelesen :2354/:2358
       (Zeile 0 = sys->gm_mode != 2, Zeile 1 = gm_mode == 2). */
    const int32_t (*hp_tbl)[16];
    const RECVX_EN09_MTN_FMTN*  mtn_tbl;        /*  28  en09.c:40   */
    const RECVX_EN09_MTN_ATB*   mtn_tbl2;       /*  24  en09.c:72   */
    const RECVX_EN09_MTN_ATB*   mtn_tbl3;       /*   5  en09.c:315  */
    const RECVX_EN09_CHK_TBL*   chg_mtn_tbl;    /*  17  en09.c:324  */
    const RECVX_EN09_WPNDAMAGE* wpn_damage;     /*  22  en09.c:464  */
    const RECVX_EN09_COMBWEP*   comb_wep;       /*  21  en09.c:490  */
    const RECVX_EN09_COMBJOINT* comb_joint;     /*  24  en09.c:515  */
    const RECVX_ENEMY_CPCL*     cap_col;        /*  19  en09.c:517  */
    const RECVX_ENEMY_BP_WORK*  bld_tbl;        /*      en09.c:540  */
    const int8_t*               sdw_tab;        /*   6  en09.c:550  */
    const RECVX_EN09_BT*        prt_blood_tbl;  /*  24  en09.c:555  */
    const float*                mtn23;          /*  35  en09.c:583  */
    const float*                mogmog;         /*  40  en09.c:7383 */
    const int8_t              (*tree)[16];      /* 4x16 en09.c:2016 */
    const int32_t*              ply_mtn_offset; /*   4  en09.c:32   */
    /* bhEne09_SetMtn waehlt nach mtn_no: 2 -> arm_pos3, 3 -> arm_pos4,
       4 -> arm_pos5, sonst (5) -> arm_pos6 (en09.c:7435-7450). arm_pos und
       arm_pos2 gehoeren zu noch nicht portierten Zweigen und sind hier
       bereits gefuehrt, damit der Satz vollstaendig bleibt. */
    const RECVX_EN09_ARM_POS*   arm_pos;        /*  92  en09.c:622  */
    const RECVX_EN09_ARM_POS*   arm_pos2;       /*  67  en09.c:718  */
    const RECVX_EN09_ARM_POS*   arm_pos3;       /* 143  en09.c:789  */
    const RECVX_EN09_ARM_POS*   arm_pos4;       /* 128  en09.c:936  */
    const RECVX_EN09_ARM_POS*   arm_pos5;       /* 654  en09.c:1068 */
    const RECVX_EN09_ARM_POS*   arm_pos6;       /* 185  en09.c:1726 */
} RECVX_EN09_DATA;

/* Vollstaendiger Satz aus den Feldern dieser Datei. */
RECVX_EN09_DATA recvx_en09_default_data(void);
/* Bindung durch den Besitzer; NULL loest die Lueckenmeldungen aus. */
void recvx_en09_set_data(const RECVX_EN09_DATA* data);
const RECVX_EN09_DATA* recvx_en09_data(void);

#ifdef __cplusplus
}
#endif
#endif
