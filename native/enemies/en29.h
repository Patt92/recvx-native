#ifndef RECVX_NATIVE_EN29_H
#define RECVX_NATIVE_EN29_H
#include "enemy_work.h"
#include "en03_lib.h"
#include "en19.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 29 — Tentakel. Eigenstaendige native Zustandsmaschine auf dem
   WorldEntity-Work. Semantikreferenz src/ps2/veronica/prog/en29.c:164-1818 und
   include/ps2/veronica/prog/en29.h. Kein Originalkoerper, kein Generator,
   keine zweite Workwelt. Die Aktionstabelle hat das EA_WORK-Layout von EN19. */

struct RECVX_EN29_STATE;
typedef void (*RECVX_EN29_MV)(RECVX_ENEMY_WORK* w, struct RECVX_EN29_STATE* fw, int count);
typedef void (*RECVX_EN29_PLY)(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, struct RECVX_EN29_STATE* fw);

/* en29.h:85 e19_dmg_typ (0x14): Ring der Schadenskapsel-Kindwerke. */
typedef struct RECVX_EN29_DMG {
    struct RECVX_EN29_DMG* nextP;
    RECVX_ENEMY_WORK* ewP;
    int32_t atr_top, atr_end;
    float atr_rad;
} RECVX_EN29_DMG;

/* en29.h:95 eaw_typ (0x48). */
typedef struct {
    int32_t act_flg;
    RECVX_EN29_MV prgP;
    int32_t act_frm, act_now, mtn_rte, chg_rte, act_cnt;
    int32_t b_act_flg;
    RECVX_EN29_MV b_prgP;
    int32_t b_act_frm, b_act_now, b_mtn_rte, b_chg_rte, b_act_cnt;
    int32_t b_mtn_md, dummy;
    const RECVX_EN19_EA* act_tblP;
    int32_t tbl_num;
} RECVX_EN29_EAW;

/* en19.h:120 PAW_WORK (0x38); ene_mnwP/p_paramP sind nativ der Work selbst. */
typedef struct {
    int32_t p_status, p_mode, p_act_flg;
    RECVX_EN29_PLY p_prgP;
    int32_t p_act_frm, p_chg_frm, p_mtn_rte, p_act_jmp, p_frm_num, p_act_now;
    const RECVX_EN19_ACT* act_tblP;
    int32_t tbl_num;
} RECVX_EN29_PAW;

/* en29.h:120 en29_freework (0x1A0). bhEne29_Init nullt ab `status` 252 Byte
   (npSetMemory(&fwP->status, 252, 0)); dmg_eneP und DmgWrk bleiben. */
typedef struct RECVX_EN29_STATE {
    RECVX_EN29_DMG* dmg_eneP;
    RECVX_EN29_DMG DmgWrk[8];
    /* --- ab hier genullt (0xA4) --- */
    int32_t status, mode0_bak, br_mde0, br_mde1, br_rnd, br_tim, br_fir;
    RECVX_ENEMY_VEC tgt_pos;
    int32_t dir_dlt, bas_obj, bas_ax, bas_ay, atk_obj0, atk_obj1;
    float atk_rng0, atk_rng1;
    int32_t dmg_obj, dmg_lvl, eff_wit;
    /* O_WRK* sdw_owkP (0xF8): eff[]-Platz des rySetShadow; -1 = NULL. Wie im
       Original mit dem Rest genullt; nach einem zweiten Init bleibt er NULL. */
    int32_t sdw_owk;
    /* ATR_WORK* hit_tabP (0xFC): sys->mwalp-Index; -1 = NULL. */
    int32_t hit_tab;
    int32_t atr_obj, p_status, p_br_mde0, p_br_mde1;
    RECVX_ENEMY_VEC ply_dmg;
    int32_t dmg_dir;
    RECVX_EN29_EAW ene_act;
    RECVX_EN29_PAW ply_act;
    /* --- nativ, ausserhalb des Originalblocks: ewP->exp1 = htP (en29.c:205)
       haelt den Flaechenindex ueber das Nullen hinweg (Original: der Zeiger in
       exp1). -1 = keine Flaeche. */
    int32_t exp1_wall;
} RECVX_EN29_STATE;

typedef struct RECVX_EN29_SERVICES {
    /* sys->rm_flg |= set; &= ~clear (en29.c:305). */
    void (*room_flags)(uint32_t set, uint32_t clear);
    /* effsub3.c:1640 rySetShadow -> eff[]-Platz oder -1. */
    int  (*shadow)(RECVX_ENEMY_WORK* w, int obj0, int obj1, int obj2, float off_a, float off_b);
    /* oP->mdlver / oP->type / oP->sy nach rySetShadow (en29.c:294-298). */
    void (*shadow_fields)(int slot, uint32_t mdlver, uint32_t type, float sy);
    /* oP->stflg Bit 0x1000000 setzen (1) bzw. loeschen (0) (en29.c:316, 382). */
    void (*shadow_hidden)(int slot, int hidden);
    /* eneset.c:170 bhSetEnemy(&En29DmgDat, 0): Kindwerk ohne mlwP. */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent, const RECVX_ENEMY_TEMPLATE* tmpl);
    /* enP->mlwP = ewP->mlwP (en29.c:1750): das Kind teilt Modell und owP. */
    int  (*share_model)(RECVX_ENEMY_WORK* child, const RECVX_ENEMY_WORK* parent);
    /* Wie EN19: bhSetEffectTb, rySetEffBlood/2, plp->mnwP-Quelle. */
    int  (*effect_tb)(uint32_t mask, const RECVX_EN19_EF* ef, const RECVX_ENEMY_VEC* ofs,
                      RECVX_ENEMY_WORK* link, int joint);
    int  (*blood)(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* dir, int typ_no);
    int  (*blood2)(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* dir, int typ_no);
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
} RECVX_EN29_SERVICES;

void recvx_en29_bind(const RECVX_EN29_SERVICES* services);
/* Einstieg bhEne29 (en29.c:164) fuer bhJumpEnemy[29]. */
void recvx_en29(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
