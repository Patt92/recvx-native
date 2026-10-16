#ifndef RECVX_NATIVE_EN15_H
#define RECVX_NATIVE_EN15_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Nosferatu (ID 15). Eigenstaendige native Umsetzung auf dem WorldEntity-Work;
   Semantikreferenz src/ps2/veronica/prog/en15.c:265-3347 (der Kern bhEne53
   :3350-3745 liegt in native/enemies/en53.c). Reine Daten aus en15.c:27-241,
   :317-348, :1824-1854, :2214-2326. Kein Originalkoerper, kein Generator,
   keine zweite Workwelt. */

/* Die Felder des Spielerworks, die Nosferatu waehrend des Griffs liest und
   schreibt und die RECVX_ENEMY_PLAYER nicht fuehrt (BH_PWORK dvx/dvy/dvz,
   dax/day, spd, pxb/pyb/pzb). pos_b ist nur lesbar. */
typedef struct RECVX_EN15_PLAYER_DYN {
    RECVX_ENEMY_VEC dv;     /* plp->dvx/dvy/dvz                              */
    int32_t dax, day;       /* plp->dax/day                                  */
    float   spd;            /* plp->spd                                      */
    RECVX_ENEMY_VEC pos_b;  /* plp->pxb/pyb/pzb (bhControlPlayer, player.c:1366) */
} RECVX_EN15_PLAYER_DYN;

/* sys->ef-Teilschreiben (en15.c:2335-2344 SpecialAttack, :2364-2419 SetPoison):
   nur die maskierten Felder der persistenten Vorlage werden geschrieben. */
#define RECVX_EN15_EF_FLG    0x001u
#define RECVX_EN15_EF_ID     0x002u
#define RECVX_EN15_EF_TYPE   0x004u
#define RECVX_EN15_EF_MDLVER 0x008u
#define RECVX_EN15_EF_POS    0x020u
#define RECVX_EN15_EF_SX     0x040u
#define RECVX_EN15_EF_SY     0x080u
#define RECVX_EN15_EF_SZ     0x100u
#define RECVX_EN15_EF_AX     0x200u
#define RECVX_EN15_EF_AY     0x400u
typedef struct RECVX_EN15_EF {
    uint32_t flg, id, type, mdlver;
    RECVX_ENEMY_VEC pos;
    float sx, sy, sz;
    int32_t ax, ay;
} RECVX_EN15_EF;

/* Nachfelder von eff[eno] nach bhSetEffectTb (en15.c:2349-2353/2376-2407). */
#define RECVX_EN15_P_STFLG  0x01u  /* stflg |= 0x20                           */
#define RECVX_EN15_P_TEXP   0x02u  /* txp[0] = epw->mlwP->texP                */
#define RECVX_EN15_P_TEXID  0x04u
#define RECVX_EN15_P_MODE3  0x08u
#define RECVX_EN15_P_OWNER  0x10u  /* exp0 = epw                              */
#define RECVX_EN15_P_VEC    0x20u  /* xn/yn/zn                                */
#define RECVX_EN15_P_TVXY   0x40u  /* tv[0..3].x/y                            */
typedef struct RECVX_EN15_EFFPATCH {
    uint32_t fields;
    int32_t  tex_id;
    uint32_t mode3;
    RECVX_ENEMY_VEC vec;
    float    tv[4][2];
} RECVX_EN15_EFFPATCH;

typedef struct RECVX_EN15_SERVICES {
    int  (*player_dyn_get)(RECVX_EN15_PLAYER_DYN* out);
    void (*player_dyn_set)(const RECVX_EN15_PLAYER_DYN* in);
    /* bhSetMotion(plp, add, plp->mtn_md, plp->mtn_tp) aus dem Gegnerpass
       (en15.c:1977 SetPlyMtn): legt `p` am Spielerbesitzer ab, laesst die
       Spieler-Motionuhr einmal laufen und liest `p` zurueck. Rueckgabe wie
       bhSetMotion (-1/0/1), -2 = Clip fehlt (gemeldet). */
    int  (*player_set_motion)(RECVX_ENEMY_PLAYER* p, int add);
    /* plp->mnwP: 0 = plp->mnwPb, 1 = epw->mnwP. */
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
    /* Motion.c:868 bhCalcFixOffset (bhFixPosition, Motion.c:857). */
    void (*calc_fix_offset)(RECVX_ENEMY_WORK* w, const int8_t* tree,
                            const RECVX_ENEMY_VEC* local, RECVX_ENEMY_VEC* out);
    /* rom->posp[i] (en15.c:2871 AbleToFall); 0 = Index fehlt. */
    int  (*room_point)(uint32_t i, RECVX_ENEMY_VEC* out);
    /* mlwP->objP = mbp[model] / obj_a, obj_b = mbp[a], mbp[b] (en15.c:475-477). */
    int  (*model_root)(RECVX_ENEMY_WORK* w, uint32_t model);
    int  (*morph_pair)(RECVX_ENEMY_WORK* w, uint32_t a, uint32_t b);
    /* sys->gm_mode. */
    int  (*game_mode)(int* out);
    /* SYS_WORK-Flagbaenke: 0 gm_flg, 1 st_flg, 2 pt_flg, 3 ts_flg. */
    uint32_t (*sys_flags)(uint32_t which);
    void (*sys_flags_set)(uint32_t which, uint32_t set, uint32_t clear);
    /* bhStFlg/bhCrFlg(sys->ev_flg, no). */
    void (*event_flag_set)(uint32_t no, int on);
    /* sys->ef teilweise schreiben, dann bhSetEffectTb(&sys->ef, ofs, link, joint). */
    int  (*effect_tb)(uint32_t mask, const RECVX_EN15_EF* ef, const RECVX_ENEMY_VEC* ofs,
                      RECVX_ENEMY_WORK* link, int joint);
    void (*effect_patch)(int slot, RECVX_ENEMY_WORK* owner, const RECVX_EN15_EFFPATCH* p);
} RECVX_EN15_SERVICES;

/* exp0 (bhGetFreeMemory(100, 32), en15.c:429): sieben Punkte, das Drehziel
   EXP0_I(0x54), die angeforderte Motion EXP0_S(0x58), die Haltebits
   EXP0_S(0x5A) (Bit 0: der Spielerwork wird von Ply_func gefuehrt) und der
   Giftzaehler EXP0_S(0x5C), den bhEff_E15_Poison hochzaehlt. */
typedef struct RECVX_EN15_STATE {
    RECVX_ENEMY_VEC pts[7];  /* 0x00: (NJS_POINT3*)exp0 + 0..6               */
    int32_t goal;            /* 0x54                                         */
    int16_t req_mtn;         /* 0x58                                         */
    int16_t hold;            /* 0x5A                                         */
    int16_t poison;          /* 0x5C                                         */
    int16_t pad;
} RECVX_EN15_STATE;

void recvx_en15_bind(const RECVX_EN15_SERVICES* services);
void recvx_en15(RECVX_ENEMY_WORK* w);
/* en15.c:24 static char poison_attack_wait: von bhEne15 heruntergezaehlt und
   von bhEff_E15_Poison (PoisonAttack, :2593) gesetzt — ein Zaehler fuer alle. */
signed char* recvx_en15_poison_attack_wait(void);
/* en15.c:2836 bhEne15_RotChar auf einem beliebigen ay/flg-Paar. */
void recvx_en15_rot_char(int32_t* ay, uint32_t flg, int goal, int add_ang);

#ifdef __cplusplus
}
#endif
#endif
