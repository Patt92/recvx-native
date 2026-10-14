#ifndef RECVX_NATIVE_EN23_H
#define RECVX_NATIVE_EN23_H
#include "enemy_work.h"
#include "en03_spider.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Riesen-Schwarze-Witwe (ID 23). Eigenstaendige native Umsetzung auf dem
   WorldEntity-Work; Semantikreferenz src/ps2/veronica/prog/en23.c:361-4028.
   Reine Daten aus en23.c:22-300 (spl_051/052, joint_tree, flip_tree, SdwTab,
   BrokenParts, BloodParam/BloodTbl, CapColTabA/B, DmgReact, CombWepTbl).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */

typedef struct RECVX_EN23_SERVICES {
    /* eneset.c:170 bhSetEnemy(&ene23_child) ohne mlwP (en23.c:492). */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent, const RECVX_ENEMY_TEMPLATE* tmpl);
    /* rutchk.c:5 bhCheckRoute: Rueckgabe now oder 0xFF; -1 = Raumdaten fehlen. */
    int (*route_points)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to, RECVX_ENEMY_VEC* out);
    /* sys->gm_mode, 1 = vorhanden. */
    int (*game_mode)(int* out);
    /* plp->mnwP: 0 = plp->mnwPb, 1 = epw->mnwP (en23.c:3545/3619). */
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
    /* eff[slot].id = id (en23.c:613: Schatten von bhSetShadow wird bhEff_E03_Shadow). */
    void (*effect_id)(int slot, uint32_t id);
    /* epw->mdl[model].texP fuer eff[].txp[0] (en23.c:3786); NULL = fehlt. */
    const void* (*model_texture)(const RECVX_ENEMY_WORK* w, uint32_t model);
    /* eff[slot].px/py/pz (en23.c:3802-3804). */
    int (*effect_position)(int slot, RECVX_ENEMY_VEC* pos, int write);
    /* npSetAllMatColor(mlwP->objP, obj_num, rgba) (en23.c:4026). */
    void (*material_color)(RECVX_ENEMY_WORK* w, uint32_t rgba);
    /* sys->rm_flg |= / &= ~bits. */
    void (*room_flags)(uint32_t set, uint32_t clear);
    /* sys->ef.id/type/flg/px..pz/sx..sz schreiben (uebrige Vorlagenfelder bleiben)
       und bhSetEffectTb(&sys->ef, NULL, NULL, 0); Slot oder -1 (en23.c:3763-3780). */
    int (*effect_tb)(uint32_t id, uint32_t type, uint32_t flg, const RECVX_ENEMY_VEC* pos,
                     const RECVX_ENEMY_VEC* scale);
    /* obj_a = mdl[a].objP, obj_b = mdl[b].objP (en23.c:660-661). */
    void (*morph_sources)(RECVX_ENEMY_WORK* w, uint32_t a, uint32_t b);
} RECVX_EN23_SERVICES;

/* exp0 (bhEne_CallocWork(304), en23.c:486) mit benannten Feldern. */
#define RECVX_EN23_BABIES 30u
typedef struct RECVX_EN23_STATE {
    RECVX_EN03_BODY body;                    /* 0x00..0x6F                     */
    RECVX_ENEMY_WORK* babies[RECVX_EN23_BABIES]; /* exp0[128 + 4i]: id-24-Works */
    RECVX_ENEMY_WORK* legs[2];               /* exp0[248/252]: bhEne03s-Kinder */
    RECVX_EN03S_LINK leg_link[2];            /* exp1 der Kinder (O_WORK-Verweis) */
    uint8_t  broken_node[2];                 /* exp0[256/257]                  */
    uint8_t  leg_broken[2];                  /* exp0[258/259]                  */
    int32_t  floor_valid;                    /* EXP0_I(260) != NULL            */
    RECVX_ENEMY_ATR floor;                   /* *(ATR_WORK*)EXP0_I(260)        */
    int32_t  climb;                          /* EXP0_I(264)                    */
    int32_t  babies_n;                       /* EXP0_I(268)                    */
    int32_t  timer;                          /* EXP0_I(272)                    */
    int32_t  hp2;                            /* EXP0_I(276)                    */
    float    wall_ofs;                       /* EXP0_F(280)                    */
    int32_t  shape;                          /* EXP0_I(284)                    */
} RECVX_EN23_STATE;

void recvx_en23_bind(const RECVX_EN23_SERVICES* services);
void recvx_en23(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
