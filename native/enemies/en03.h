#ifndef RECVX_NATIVE_EN03_H
#define RECVX_NATIVE_EN03_H
#include "enemy_work.h"
#include "en03_spider.h"
#include "ninja_matrix.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Schwarze Witwe (ID 3). Eigenstaendige native Umsetzung auf dem
   WorldEntity-Work; Semantikreferenz src/ps2/veronica/prog/en03.c:478-7432.
   Reine Daten aus en03.c:21-406 (spm_016/023/026/028/029, spmh_013/015/026,
   ENE03_HITPOINT, spider_tree, SdwTab, flip_tree, BrokenParts, BloodParam,
   BloodTbl, DmgReact, CombWepTbl, CombJointTbl, CapColTabA/B).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */

typedef struct RECVX_EN03_ID_SERVICES {
    /* eneset.c:170 bhSetEnemy fuer die 8 Beinkinder (ene03_child, flg 0x8001,
       id 31), den Anfuehrer (ene03_leader, flg 0x4008001, id 3) und die 15
       EN24-Kinder. `base` ist der Modellfenster-Index; ~0u laesst mlwP NULL
       (so wie bhSetEnemy es tut). */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent,
                                      const RECVX_ENEMY_TEMPLATE* tmpl, uint32_t base);
    /* rutchk.c:6 bhCheckRoute: Rueckgabe now oder 0xFF; -1 = Raumdaten fehlen. */
    int (*route_points)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to,
                        RECVX_ENEMY_VEC* out);
    /* sys->gm_mode (en03.c:767). */
    int (*game_mode)(int* out);
    /* plp->mnwP: 0 = plp->mnwPb, 1 = epw->mnwP (en03.c:6825/6895/6974). */
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
    /* eff[slot].id (en03.c:848: der Schatten wird 258). */
    void (*effect_id)(int slot, uint32_t id);
    /* sys->ef.id/type/flg/px..pz/sx..sz schreiben und danach
       bhSetEffectTb(&sys->ef, NULL, NULL, 0); Slot oder -1 (en03.c:7306-7323). */
    int (*effect_tb)(uint32_t id, uint32_t type, uint32_t flg,
                     const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* scale);
    /* npSetAllMatColor(mlwP->objP, mlwP->obj_num, rgba) (en03.c:7428). */
    void (*material_color)(RECVX_ENEMY_WORK* w, uint32_t rgba);
    /* hitchk.c:12 bhCheckWall(plp) (en03.c:7011): der Spielerwork gegen die
       Raumwaende mit ar/ah; schreibt p->pos. */
    int (*player_wall)(RECVX_ENEMY_PLAYER* p, float ar, float ah);
    /* plp->dvx/dvy/dvz (en03.c:2856/2916/3109/3190): der Rueckstoss des
       Spielerworks. RECVX_ENEMY_PLAYER fuehrt diese drei Felder nicht; der
       native Spielerbesitzer nimmt sie ueber diesen Dienst entgegen. */
    void (*player_dv_set)(const RECVX_ENEMY_VEC* dv);
    /* eff[slot].px/py/pz nach dem Anlegen (en03.c:7345-7347, Acid). Derselbe
       Anbieter wie RECVX_EN23_SERVICES.effect_position. */
    int (*effect_position)(int slot, RECVX_ENEMY_VEC* pos, int write);
} RECVX_EN03_ID_SERVICES;

/* exp1 (en03.c:683 bhEne_CallocWork(12, 8); gelesen werden nur 0 und 8). */
typedef struct RECVX_EN03_EXP1 {
    int32_t flg;      /* EPW_EXP1_I(0)                                       */
    int32_t pad;      /* Offset 4: im Original vorhanden, ohne Leser         */
    int32_t timer;    /* EPW_EXP1_I(8)                                       */
} RECVX_EN03_EXP1;

/* exp0-Kopf (en03.c 0x00..0x6B). Deckungsgleich mit RECVX_EN03_BODY
   (en03_spider.h) bis auf 0x6A: dort liegt EXP0_C(106), den EN03 als
   Flaechenart des Ziels fuehrt und das EN23 nie liest. Ab 0x6C folgt der
   Richtungsvektor EXP0_F(108/112/116); RECVX_EN03_BODY hat dort sein ungenutztes
   `ar`-Feld, weshalb der Kopf hier eigenstaendig steht. Die gemeinsamen
   Bausteine bekommen `(RECVX_EN03_BODY*)&s->h` — sie lesen nur m[] und
   surface (0x69), die hier identisch liegen. */
typedef struct RECVX_EN03_HEAD {
    float   m[16];          /* 0x00  EXP0 0x00..0x3F (NJS_MATRIX)          */
    float   dist;           /* 0x40  EXP0_F(64)                            */
    int32_t ang;            /* 0x44  EXP0_I(68)                            */
    RECVX_ENEMY_VEC target; /* 0x48  EXP0_F(72/76/80)                      */
    RECVX_ENEMY_VEC vel;    /* 0x54  EXP0_F(84/88/92)                      */
    int32_t wall;           /* 0x60  EXP0_I(96), ATR-Index oder -1         */
    int32_t wall2;          /* 0x64  EXP0_I(100)                           */
    uint8_t seen;           /* 0x68  EXP0_UC(104)                          */
    int8_t  surface;        /* 0x69  EXP0_C(105): 0 Boden, 1 Decke, 2..5   */
    uint8_t surface2;       /* 0x6A  EXP0_C(106)                           */
    uint8_t pad;            /* 0x6B                                        */
} RECVX_EN03_HEAD;

/* exp0 des Elternworks (en03.c:669 bhEne_CallocWork(240, 8)). Das Original
   haelt hier rohe Zeiger und Bytes; nativ sind es benannte Felder (gleiche
   Datenrepraesentation wie in RECVX_EN23_STATE). */
typedef struct RECVX_EN03_STATE {
    RECVX_EN03_HEAD   h;
    RECVX_ENEMY_VEC   dir;          /* 0x6C EXP0_F(108/112/116)              */
    int32_t           hp2;          /* 0x7C EXP0_I(124)                      */
    RECVX_ENEMY_WORK* legs[8];      /* 0x80 EXP0_C(128): die 8 bhEne03s-Works */
    uint8_t           broken[8];   /* 0xA0                                  */
    RECVX_ENEMY_WORK* babies[15];   /* 0xA8 EXP0_C(168): die 15 EN24-Works   */
    RECVX_EN03S_LINK  leg_link[8];  /* exp1 der 8 Beinkinder (O_WORK-Verweis) */
    /* exp2 des Originalworks: der versteckte Anfuehrerwork (flg 0x4008001,
       id 3). Er traegt dieselbe exp1 und laeuft ueber bhEne03 nur den
       PlayerControl-Zweig (en03.c:692-701). */
    RECVX_ENEMY_WORK* leader;
    int32_t           timer;        /* 0xE4 EXP0_I(228)                      */
    int32_t           climb_timer;  /* 0xE8 EXP0_I(232)                      */
} RECVX_EN03_STATE;

void recvx_en03_id_bind(const RECVX_EN03_ID_SERVICES* services);
void recvx_en03(RECVX_ENEMY_WORK* w);
/* en03.c:6335 bhEne03_GetWallDir: Flaechenart aus einer Normalen (0 Decke,
   1 Boden, 2/4 x-Wand, 3/5 z-Wand). Kein Aufrufer in en03.c. */
int recvx_en03_get_wall_dir(RECVX_ENEMY_WORK* w, const NJS_VECTOR* v);

#ifdef __cplusplus
}
#endif
#endif
