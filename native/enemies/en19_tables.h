#ifndef RECVX_NATIVE_EN19_TABLES_H
#define RECVX_NATIVE_EN19_TABLES_H
#include "enemy_work.h"
#include "en03_lib.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Reine Datentabellen des Tyrant T-078 (en19.c:41-213 und die statischen
   Tabellen der Routinen). Layouts aus en19.h (EA_WORK 0x14, ACT_TBL_WORK 0x10,
   DD_WORK 0x1C, EO_WORK 0x10, PF_WORK 0x24) und effsub3.h (PMA_WORK 0x20);
   COMBWEP ist der Spiegel aus en03_lib.h. Werte unveraendert; keine
   Funktionskoerper. Die Funktionszeiger prgP sind als Index (prg) geführt:
   EA: 0..21 = Mv00, Mv01, Mv02, Mv03, Mv04a, Mv04b, Mv05, Mv06, Mv07, Mv08,
       Mv09, Mv10, Mv11, Mv12, Mv13, Mv14, Mv15, Mv16, Mv17, Mv18, Mv19, Mv20;
   ACT: 0 PlyDmgFal, 1 PlyDmg042, 2 PlyDmg043, 3 PlyDmg044, 4 PlyDmg045,
        5 PlyDmg046_047, 6 PlyDmg050_051, 7 PlyDmg052_053, 8 PlyDmg117_118,
        -1 NULL. */

/* en19.h:212 EA_WORK */
typedef struct {
    int32_t label;
    uint8_t mtn_no, frm_no, hkn_lvl, hkn_cnt;
    float   chg_rte;
    int32_t flag;
    int8_t  prg;
} RECVX_EN19_EA;
/* en19.h:107 ACT_TBL_WORK */
typedef struct {
    int16_t mtn_no, frm_no;
    uint8_t hkn_lvl, hkn_cnt, chg_frm;
    int8_t  act_jmp;
    int32_t flag;
    int8_t  prg;
} RECVX_EN19_ACT;
/* effsub3.h:394 PMA_WORK (Parameter des Kamerawackel-Effekts 310/bhEff30a) */
typedef struct {
    int32_t type, ang_fst, add_ax;
    float   x_rang, x_rate;
    int32_t add_ay;
    float   y_rang, y_rate;
} RECVX_EN19_PMA;
/* en19.h:254 DD_WORK */
typedef struct { int32_t eff_nml[3]; int32_t eff_cmb[3]; int32_t dmg_cdn; } RECVX_EN19_DD;
/* en19.h:246 EO_WORK */
typedef struct { float off_x, off_y, rnd_x, rnd_y; } RECVX_EN19_EO;
/* en19.h:237 PF_WORK */
typedef struct { RECVX_ENEMY_VEC vct_spd, vct_acl; float acl_rte, die_dst; int32_t mode; } RECVX_EN19_PF;

extern const RECVX_EN19_EA     recvx_en19_act_tbl[50];
extern const RECVX_T3_COMBWEP  recvx_en19_comb_wep[21];
extern const int8_t            recvx_en19_flp_tbl[26];
extern const RECVX_ENEMY_CPCL  recvx_en19_cap_col[28];
extern const RECVX_EN19_ACT    recvx_en19_ply_act_tbl[17];
extern const float             recvx_en19_ovl_tbl[3];
extern const int32_t           recvx_en19_hp_tbl[4][3];
extern const float             recvx_en19_atk_rng[3][4];
extern const RECVX_EN19_PMA    recvx_en19_eff30a_prm1[3];
extern const RECVX_EN19_PMA    recvx_en19_eff30a_prm2[3];
extern const RECVX_EN19_DD     recvx_en19_dmg_dat[21];
extern const RECVX_EN19_EO     recvx_en19_off_tbl[26];
extern const int32_t           recvx_en19_dmg_eff_tbl[12][4];
extern const int32_t           recvx_en19_snd_tbl[20];
extern const int32_t           recvx_en19_snd_eff_tbl[15];
extern const int32_t           recvx_en19_ply_dmg_tbl[4];
extern const RECVX_EN19_PF     recvx_en19_ply_fal[3];
extern const float             recvx_en19_blood_prm[2][3];

#ifdef __cplusplus
}
#endif
#endif
