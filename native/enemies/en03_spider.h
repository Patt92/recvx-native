#ifndef RECVX_NATIVE_EN03_SPIDER_H
#define RECVX_NATIVE_EN03_SPIDER_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
   Gemeinsame Spinnenbausteine der Familie EN03/EN23 — eigenstaendige native
   Bestimmung. Semantikreferenz (nicht kompiliert/kopiert):
     en03.c:4825 bhEne03_DGDirCheck, :4888 DirTarget, :4914 GoAHead,
     :4922 GetPartsPos, :4951 MakeMatrix, :4990-5061 HideParts(Sub0/Sub1),
     :5203 GetWall, :6234 SetModelFlg, :6249 AddNullTrans;
     en03sub.c:18-484 bhEne03s (abfallendes Bein auf dem owP des Elternworks).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.
   =========================================================================== */

/* Kopf des EN03/EN23-exp0 (en03.c/en23.c EXP0-Offsets 0x00..0x6F): die
   Lagematrix des Koerpers und die Felder, die die Bausteine lesen. */
typedef struct RECVX_EN03_BODY {
    float    m[16];            /* 0x00 NJS_MATRIX; EXP0_F(16..24) Hochachse, (32..40) Laufachse */
    float    dist;             /* 0x40 EXP0_F(64)                          */
    int32_t  ang;              /* 0x44 EXP0_I(68)                          */
    RECVX_ENEMY_VEC target;    /* 0x48 EXP0_F(72/76/80)                    */
    RECVX_ENEMY_VEC vel;       /* 0x54 EXP0_F(84/88/92)                    */
    int32_t  wall;             /* 0x60 EXP0_I(96): ATR-Zeiger als Wandindex, -1 = NULL */
    int32_t  wall2;            /* 0x64 EXP0_I(100)                         */
    uint8_t  seen;             /* 0x68 EXP0_UC(104)                        */
    int8_t   surface;          /* 0x69 EXP0_C(105): 0 Boden, 1 Decke, 2..5 Waende */
    float    ar;               /* 0x6C EXP0_F(108)                         */
} RECVX_EN03_BODY;

typedef struct RECVX_EN03_SERVICES {
    /* en03.c:5037 bhEne03_HideParts: objP[node].evalflags 0x8 setzen/loeschen
       und dieselbe Aenderung auf child und rekursiv auf dessen child/sibling. */
    int (*hide_parts)(RECVX_ENEMY_WORK* w, uint32_t node, int hide);
    /* en03.c:6234: owP[i].flg = (flg & and) | or fuer i < mnwP->obj_num. */
    int (*model_flags)(RECVX_ENEMY_WORK* w, uint32_t and_flg, uint32_t or_flg);
    /* owP[node].flg |= bits (en23.c:3448-3450). */
    int (*owp_flag_or)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t bits);
    /* owP[node].mtx des Baums, den w liest/schreibt (en03sub.c: epw->mtx). */
    int (*owp_matrix)(const RECVX_ENEMY_WORK* w, uint32_t node, float* out16);
    int (*owp_matrix_set)(RECVX_ENEMY_WORK* w, uint32_t node, const float* m16);
    /* mlwP->objP->ang[0..2] schreiben (en23.c:2187). */
    int (*root_angles_set)(RECVX_ENEMY_WORK* w, const int32_t* ang3);
} RECVX_EN03_SERVICES;
void recvx_en03_bind(const RECVX_EN03_SERVICES* services);

/* Bausteine; `b` ist der exp0-Kopf des Works. Rueckgabe 0 = Dienst fehlt (gemeldet). */
int  recvx_en03_dg_dir_check(const RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b);
int  recvx_en03_dir_target_body(const RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b,
                                const RECVX_ENEMY_VEC* pos, int lim);
void recvx_en03_go_ahead(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b);
int  recvx_en03_parts_pos(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b,
                          const int8_t* parts, RECVX_ENEMY_VEC* p);
void recvx_en03_make_matrix(RECVX_EN03_BODY* b);
/* en03.c:5203: Linie von p + Hochachse 999 nach unten; Wandindex oder -1. */
int  recvx_en03_get_wall(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b);
int  recvx_en03_hide_parts(RECVX_ENEMY_WORK* w, uint32_t node, int hide);
int  recvx_en03_set_model_flg(RECVX_ENEMY_WORK* w, uint32_t and_flg, uint32_t or_flg);
/* owP[node].flg |= bits und mlwP->objP->ang = a (en23.c:3448, :2187). */
int  recvx_en03_owp_flag_or(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t bits);
int  recvx_en03_bind_root_angles(RECVX_ENEMY_WORK* w, const int32_t* ang3);
/* en03.c:6249: mtn[frm_no/65536] ueber die Lagematrix, nur x/z. `n` ist die
   Tabellenlaenge (das Original prueft sie nicht; ein Index dahinter wird
   gemeldet). */
void recvx_en03_add_null_trans(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b,
                               const RECVX_ENEMY_VEC* mtn, uint32_t n);

/* bhEne03s: exp1 des Kindes ist sein O_WORK-Verweis (en23.c:509-511
   `mtx = &owk->mtx; exp1 = owk`). Nativ: Besitzer und Knoten. */
typedef struct RECVX_EN03S_LINK {
    RECVX_ENEMY_WORK* owner;
    uint32_t node;
} RECVX_EN03S_LINK;
void recvx_en03s(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
