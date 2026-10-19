#ifndef RECVX_NATIVE_EN06_H
#define RECVX_NATIVE_EN06_H
#include <stdint.h>
#include "enemy_work.h"
#include "enemy_dispatch.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Identitaet 6 — Moth / Riesenmotte (en06.c), RM_6030. Nativ eigenstaendig
   bestimmt. Semantikreferenz: en06.c (:229 bhEne06 bis :2269 DeadCheck) und
   en06sub.c (:33 bhEne06s, die vierzehn Fluegelfragmente auf den owP-Knoten
   des Elternworks). Kein Originalkoerper, kein Generator, keine zweite
   Workwelt. Zustandsbesitzer ist RECVX_ENEMY_WORK samt den exp-Bloecken unten.

   Drei Workarten mit id 6/31:
     - die Motte selbst (Raum-ETTY, id 6);
     - der verdeckte Anfuehrer (ene06_leader, flg 0x01008001, id 6): er laeuft
       nur bhEne06_PlayerControl, solange EXP1 Bit 1 steht und plp->mode0 4 ist;
     - vierzehn Fragmente (ene06_child, flg 0x8001, id 31, bhEne06s), deren
       epw->mtx auf lkwkp->mlwP->owP[BrokenParts[lkono]].mtx zeigt. */

/* exp0 der Motte (bhEne_CallocWork(176, 8), en06.c:315). Offsets wie die
   EXP0_*-Zugriffe; ab 0x34 haelt das Original Zeiger, nativ benannte Felder. */
typedef struct RECVX_EN06_EXP0 {
    float    dist;               /* 0x00 EXP0_F(0) Spielerabstand xz        */
    uint8_t  pad04[0x12];
    int8_t   searched;           /* 0x16 EXP0_C(0x16)                       */
    uint8_t  pad17;
    int32_t  atr;                /* 0x18 ATR_WORK* (nur gespeichert): 1 = Treffer */
    float    vel[3];             /* 0x1C/0x20/0x24                          */
    float    lean[3];            /* 0x28/0x2C/0x30 Anlehnpunkt an der Wand  */
    struct RECVX_ENEMY_WORK* parts[14]; /* 0x34 die vierzehn bhEne06s-Works */
    float    ar_target;          /* 0x6C                                    */
    float    car_target;         /* 0x70                                    */
    int32_t  rinpun;             /* 0x74 Schuppenstaub-Takt                 */
    float    floor_n[3];         /* 0x78/0x7C/0x80 Bodennormale             */
    int32_t  shadow;             /* 0x84 O_WRK* des Schattens, nativ Slot   */
    float    dead_pos[2][3];     /* 0x88 DeadCheck: Fusslagen des Spielers  */
    int32_t  dead_ang[2];        /* 0xA0                                    */
} RECVX_EN06_EXP0;
/* exp1 (bhEne_CallocWork(12, 8)), geteilt von allen Motten einer Kette und
   ihrem Anfuehrer: Bit 1 = ein Griff laeuft. */
typedef struct RECVX_EN06_EXP1 { int32_t flags; int32_t pad[2]; } RECVX_EN06_EXP1;
/* exp0 eines Fragments (bhEne_CallocWork(68, 8), en06sub.c:69). */
typedef struct RECVX_EN06S_EXP0 {
    uint16_t flags;              /* 0x00 EXP0_US(0) Bit 1 = faellt          */
    uint16_t pad02;
    float    vel[3];             /* 0x04/0x08/0x0C                          */
    float    n[3];               /* 0x10/0x14/0x18 Trefferflaechennormale   */
    float    pts[3][3];          /* 0x1C CollisionOffset in Welt            */
    int32_t  hit;                /* 0x40 EXP0_I(64) getroffener Punkt       */
} RECVX_EN06S_EXP0;

typedef struct RECVX_EN06_SERVICES {
    /* eneset.c:170 bhSetEnemy(tmpl) ohne Modell (mlwP NULL): Anfuehrer,
       Fragmente und das Mottenei ene27 (en06.c:337/351/1182). NULL = voll. */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent, const RECVX_ENEMY_TEMPLATE* tmpl);
    /* eneset.c:933 bhEne_SetCallFunc(bhEne06s, 31). */
    void (*set_call_func)(uint32_t id, RECVX_ENEMY_PROC proc);
    /* owP[i].flg = (flg & and) | or fuer i < mnwP->obj_num (en06.c:307-311). */
    int  (*model_flags)(RECVX_ENEMY_WORK* w, uint32_t and_flg, uint32_t or_flg);
    /* owP[node].flg |= bits (en06.c:1287/:1740). */
    int  (*owp_flag_or)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t bits);
    /* owP[node].mtx lesen/schreiben (en06sub.c: epw->mtx des Fragments). */
    int  (*owp_matrix)(const RECVX_ENEMY_WORK* w, uint32_t node, float* out16);
    int  (*owp_matrix_set)(RECVX_ENEMY_WORK* w, uint32_t node, const float* in16);
    /* eff[slot].sy (en06.c:559/:900 ueber exp0 + 0x84). */
    int  (*effect_sy)(int slot, float* sy, int write);
    /* bhSetEffectTb(&sys->ef, NULL, NULL, 0) mit den in en06.c:2183-2193
       geschriebenen Feldern id/flg/type/sx/sy/sz/ay/px/py/pz; Slot oder -1. */
    int  (*effect_tb)(uint32_t id, uint32_t flg, uint32_t type, const RECVX_ENEMY_VEC* pos,
                      const RECVX_ENEMY_VEC* scale, int32_t ay);
    /* eff[slot].stflg |= 0x20, txp[0] = epw->mdl[mdl].texP, tex_id (en06.c:2197-2199). */
    void (*effect_texture)(int slot, RECVX_ENEMY_WORK* w, uint32_t mdl, int32_t tex_id);
    /* hitchk.c:12 bhCheckWall(plp) mit plp->ar/ah. */
    int  (*player_wall)(RECVX_ENEMY_PLAYER* p, float ar, float ah);
    /* npSetAllMatColor(mlwP->objP, mlwP->obj_num, rgba) (en06.c:2263). */
    void (*material_color)(RECVX_ENEMY_WORK* w, uint32_t rgba);
} RECVX_EN06_SERVICES;
void recvx_en06_bind(const RECVX_EN06_SERVICES* services);
/* bhJumpEnemy[6] (en06.c:229) und bhEne_SetCallFunc(bhEne06s, 31) (en06sub.c:33). */
void recvx_en06(RECVX_ENEMY_WORK* w);
void recvx_en06s(RECVX_ENEMY_WORK* w);
RECVX_EN06_EXP0* recvx_en06_exp0(RECVX_ENEMY_WORK* w);
/* BrokenParts (en06.c:42): owP-Knoten der zwoelf Fragmente. */
extern const int8_t recvx_en06_broken_parts[12];
#ifdef __cplusplus
}
#endif
#endif /* RECVX_NATIVE_EN06_H */
