#ifndef RECVX_NATIVE_EN05_H
#define RECVX_NATIVE_EN05_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Hunter/Sweeper (ID 5) und sein Kindwork bhEne05s (ID 31). Eigenstaendige
   native Umsetzung auf dem WorldEntity-Work; Semantikreferenz
   src/ps2/veronica/prog/en05.c:901-6186 und en05sub.c:22-239. Reine Daten
   aus en05.c:23-818 (hu_*-Translationen, HITPOINT, joint_tree, flip_tree,
   SdwTab, BloodParam, DmgReact, BloodTblF/R, CombWepTbl, Dmg/DeadSwitchTbl,
   Wasserspritzer- und Kapseltabellen). Kein Originalkoerper, kein
   Generator, keine zweite Spieler-/Gegnerwelt. */

typedef struct RECVX_EN05_SERVICES {
    /* eneset.c:170 bhSetEnemy(&ene05_child) ohne mlwP (en05.c:1015). */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent, const RECVX_ENEMY_TEMPLATE* tmpl);
    /* rutchk.c:5 bhCheckRoute: now oder 0xFF; -1 = Raumdaten fehlen. */
    int (*route_points)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to, RECVX_ENEMY_VEC* out);
    int (*game_mode)(int* out);
    /* pwksub.c:664 bhSearchPlayer(pp, r): Rueckgabe 1 = Spieler gebunden, *out Winkel oder -1. */
    int (*search_player)(const RECVX_ENEMY_WORK* w, int32_t r, int32_t* out);
    /* pwksub.c:133 bhAddSpeed(pp, r). */
    void (*add_speed)(RECVX_ENEMY_WORK* w, int r);
    /* Motion.c bhCalcFixOffset(epw, chain, NULL, out); chain -1-terminiert. */
    int (*fix_offset)(RECVX_ENEMY_WORK* w, const int8_t* chain, RECVX_ENEMY_VEC* out);
    /* hitchk.c:5532 bhCheckWater(pos). */
    int (*check_water)(const RECVX_ENEMY_VEC* pos);
    /* effect.c bhSetWaterSplash2(epw, pos, n, sx, sy, sz). */
    void (*water_splash)(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int n, float sx, float sy, float sz);
    /* sys->ef id/type/flg/px..pz/sx..sz schreiben (uebrige Vorlagenfelder bleiben),
       bhSetEffectTb(&sys->ef, NULL, NULL, lkono). Slot oder -1. */
    int (*effect_tb)(uint32_t id, uint32_t type, uint32_t flg, const RECVX_ENEMY_VEC* pos,
                     const RECVX_ENEMY_VEC* scale, int lkono, int set_type);
    /* eff[slot].stflg |= 0x20; txp[0] = mlwP->texP; tex_id (en05.c:5592-5594). */
    void (*effect_own_texture)(int slot, const RECVX_ENEMY_WORK* w, int tex_id);
    /* npSetAllMatColor(mlwP->objP, obj_num, rgba). */
    void (*material_color)(RECVX_ENEMY_WORK* w, uint32_t rgba);
    /* rom->etcp[index].attr: Rueckgabe 1 = vorhanden; *attr gelesen, danach |= set, &= ~clear. */
    int (*etc_attr)(uint32_t index, uint32_t set, uint32_t clear, uint32_t* attr);
    /* plp->mlwP[plp->mdl_no].owP[node].flg &= ~bits (en05.c:1066-1068). */
    void (*player_owp_clear)(uint32_t node, uint32_t bits);
    /* plp->mlwP->texP = plp->txp[k] (en05.c:1979/2115). 0 = kein Zeichenbesitzer. */
    int (*player_texture)(uint32_t k);
    /* plp->stflg |= 0x80000 (Gift; Besitzer Inventar). */
    void (*player_poison)(void);
    /* EXP_WORK des Spielers, EXP0_I(5) = dmlvl (PEXP0_I(0x14), en05.c:5627). */
    int (*player_dmlvl)(int32_t* out);
    /* objP[node].ang lesen/schreiben (en05.c:4090, :5321). */
    int (*node_angles)(RECVX_ENEMY_WORK* w, uint32_t node, int32_t* ang3, int write);
    /* owP[node].flg = (flg & and) | or. */
    int (*owp_flags)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t and_flg, uint32_t or_flg);
    /* rom->h (hitchk.c, Typ-0-Flaeche ohne h). */
    float (*room_height)(void);
} RECVX_EN05_SERVICES;

/* exp0 (bhEne_CallocWork(132), en05.c:1013) mit benannten Feldern. */
typedef struct RECVX_EN05_STATE {
    float    dist;                   /* EXP0_F(0x00)                          */
    int32_t  search;                 /* EXP0_I(0x04)                          */
    RECVX_ENEMY_VEC target;          /* EXP0_F(0x08..0x10)                    */
    uint8_t  found;                  /* EXP0_UC(0x14)                         */
    RECVX_ENEMY_VEC vel;             /* EXP0_F(0x1C..0x24)                    */
    float    ar_body, ar_leg;        /* EXP0_F(0x34/0x38)                     */
    float    ar_body_t, ar_leg_t;    /* EXP0_F(0x3C/0x40)                     */
    float    car_t;                  /* EXP0_F(0x44)                          */
    RECVX_ENEMY_VEC plane_p, plane_n;/* EXP0_F(0x48..0x5C)                    */
    RECVX_ENEMY_WORK* partner;       /* EXP0_I(0x60)                          */
    RECVX_ENEMY_WORK* child;         /* EXP0_I(0x64)                          */
    RECVX_ENEMY_VEC jump;            /* EXP0_F(0x68..0x70)                    */
    int32_t  atr_valid;              /* EXP0_I(0x74) != NULL                  */
    RECVX_ENEMY_ATR atr;             /* *(ATR_WORK*)EXP0_I(0x74)              */
    int32_t  timer;                  /* EXP0_I(0x78)                          */
    int32_t  fix_chain;              /* EXP0_I(0x7C): joint_tree-Zeile, -1 = NULL */
    int8_t   chain;                  /* EXP0_C(0x80)                          */
} RECVX_EN05_STATE;

void recvx_en05_bind(const RECVX_EN05_SERVICES* services);
void recvx_en05(RECVX_ENEMY_WORK* w);
void recvx_en05s(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
