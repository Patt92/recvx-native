#ifndef RECVX_NATIVE_EN17_H
#define RECVX_NATIVE_EN17_H
#include "enemy_work.h"
#include "enemy_dispatch.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 17 — Monster Steve. Nativ eigenstaendig bestimmt aus der
   Semantikreferenz en17.c/en17sub.c. Kein Originalkoerper, kein Generator,
   keine zweite Workwelt.

   Kettenwerke: en17.c:826/:828 legt bhEne17_SetLinkWork ueber bhSetEnemy mit
   einer 9-Byte-Vorlage (flg 0x80A1, id 0x1F bzw. 0x20) zwei EIGENE Works an
   (Modell mdl[4] bzw. mdl[3], gemeinsame Motionquelle), die per
   bhEne_SetCallFunc auf bhEne17LArm/bhEne17RArm umgehaengt werden. Der
   Zeiger steht im Original in exp2/exp3; nativ zeigt er auf den Kettenwork. */

/* EXP0: Originalallokation 0x54 Byte (en17.c:821).
   Die unbekannten Bereiche bleiben unbenannte Bytes, keine geratenen Felder.
   wall_atr bildet den ATR_WORK*-Speicherplatz bei Originaloffset 0x40 ab;
   der native Zeiger ist auf arm64/x64 breiter. sizeof dieses nativen Blocks
   ist deshalb NICHT die originale Allokationsgroesse. */
typedef struct {
    uint8_t brain_flags;             /* 0x00, en17.c:931 */
    uint8_t damage_toggle;           /* 0x01, en17.c:433-437 */
    uint8_t unknown_02[2];
    int32_t wait_count;              /* 0x04, en17.c:1002/1351-1357 */
    uint32_t flags;                  /* 0x08, auch en54.c/en55.c:120 */
    int32_t timer_0c;                /* 0x0C, en17.c:377-378 */
    int32_t effect_timer;            /* 0x10, en17.c:513-526 */
    float player_distance;           /* 0x14, en17.c:413 */
    union {
        uint32_t bits;               /* 0x18, en17.c:591/596/601/606 */
        float amplitude;            /* derselbe Speicher, :611/1675-1677 */
    } camera_shake;
    RECVX_ENEMY_VEC route_target;    /* 0x1C, x/z :934-938; MV01 :1033 */
    uint8_t unknown_28[24];
    const RECVX_ENEMY_ATR* wall_atr;  /* Original 0x40, en17.c:754 */
    uint8_t unknown_44[16];          /* Original 0x44..0x53 */
} RECVX_EN17_EXP0;

/* EXP1: 0x270 Byte = 52 Punkte, en17.c:822 / en17sub.c:148-201.
   Zwei zusammenhaengende Bereiche mit je sechs Historienpunkten und
   zwanzig Ausgabepunkten. Ausgabe[0] ist eine direkte Kopie, danach
   njBezierSpline mit sechs Schluesseln und frame = 0.05f * i, i=1..19.
   Globale Ausgabeindizes: 6..25 und 32..51, NICHT 6..26/32..52.
   LArm_Init nullt jeweils nur die ersten fuenf Historienpunkte (:73)
   und alle zwanzig Ausgabepunkte (:82); Historienpunkt 5 bleibt erhalten.
   PlayerDGCheck liest nur Ausgabeindizes 0..4 beider Bereiche (:1584-1659). */
typedef struct {
    RECVX_ENEMY_VEC tip_history[6];   /* 0x000, globale Indizes 0..5 */
    RECVX_ENEMY_VEC tip_curve[20];    /* 0x048, globale Indizes 6..25 */
    RECVX_ENEMY_VEC root_history[6];  /* 0x138, globale Indizes 26..31 */
    RECVX_ENEMY_VEC root_curve[20];   /* 0x180, globale Indizes 32..51 */
} RECVX_EN17_EXP1;

/* Zusatzdienste, die weder der Work-Vertrag noch RECVX_ENEMY_EXT anbietet.
   Ein ungebundener Dienst bricht den Zweig ab und meldet die Originalroutine. */
typedef struct {
    uint32_t flg;
    int32_t  aspd, lsrc, type;
    float    r, g, b, nr, fr;
    int32_t  lkflg, lkno, lkono;
    float    px, py, pz;
    int32_t  ct0, mode;
} RECVX_EN17_LIGHT;

typedef struct {
    /* en17.c:879 bhEne17_SetLinkWork: bhSetEnemy mit der 9-Wort-Vorlage (flg 0x80A1,
       id), lkwkp = parent, lkono = lnk_obj, lox/loy/loz = 0, epp->mdl[0] = epw->mdl[mdl_no],
       mlwP = &epp->mdl[0], mnwP = epw->mnwP. NULL bei voller Tabelle oder fehlendem Modell. */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent, uint32_t lnk_obj, uint32_t mdl_no, uint32_t id);
    /* eneset.c:933 bhEne_SetCallFunc. */
    void (*set_call_func)(uint32_t id, RECVX_ENEMY_PROC proc);
    /* en17.c:861-863: mlwP->objP = *mbp; obj_a = mbp[0]; obj_b = mbp[1]. */
    void (*select_morph_pair)(RECVX_ENEMY_WORK* w);
    /* bhCheckRoute(&epw->px, &plp->px, &out): 255 = kein Weg. */
    int  (*check_route)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to,
                        RECVX_ENEMY_VEC* out);
    /* bhCalcFixOffset(epw, tree, &local, &out); `tree` ist -1-terminiert. */
    void (*calc_fix_offset)(RECVX_ENEMY_WORK* w, const int8_t* tree,
                            const RECVX_ENEMY_VEC* local, RECVX_ENEMY_VEC* out);
    /* mlwP->objP->pos[0] = mlwP->objP->pos[2] = 0 (Gegner bzw. Spieler). */
    void (*model_root_zero_xz)(RECVX_ENEMY_WORK* w);
    void (*player_root_zero_xz)(void);
    /* plp->axp / plp->spd (nicht in RECVX_ENEMY_PLAYER); bhAddSpeed(plp, axp). */
    int  (*player_extra_get)(int32_t* axp, float* spd);
    void (*player_extra_set)(int32_t axp, float spd);
    void (*player_add_speed)(int mode);
    /* bhEne_SetBlood2(plp, type, ofs, ry). */
    void (*player_blood2)(int type, RECVX_ENEMY_VEC* ofs, int ry);
    /* bhEff_SetPtcl(epw,pos,kind) / bhEff_SetPtcl2(epw,pos,mtx16) /
       bhEff_SetPtcl2V(plp,pos,v,kind). `v` ist nicht const: bhEff_SetPtcl2V ruft
       njUnitVector(dv) auf dem Vektor DES AUFRUFERS (effsub4.c:260), die acht
       Aufrufe in PlayerDGCheck bekommen ihn deshalb ab dem zweiten normiert. */
    void (*eff_ptcl)(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int kind);
    void (*eff_ptcl2)(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, const float* mtx16);
    void (*eff_ptcl2v_player)(const RECVX_ENEMY_VEC* pos, RECVX_ENEMY_VEC* v, int kind);
    /* bhEne17_AfterimageAxEffect: Effekt Id 364, mlwP = &epw->mdl[5],
       Winkel aus der Matrix, flr_no 1, tv[0].col = argb. */
    void (*afterimage)(RECVX_ENEMY_WORK* w, const float* mtx16, const RECVX_ENEMY_VEC* ofs,
                       uint32_t argb);
    /* cam.ofy = y bzw. cam.ofx/ofy/ofz = 0. */
    void (*camera_ofy)(float y);
    void (*camera_clear)(void);
    /* rom->lgtp[index] mit lp->light = NULL. */
    void (*set_light)(uint32_t index, const RECVX_EN17_LIGHT* light);
    /* npCollisionCheckCC(a, b). */
    int  (*capsule_capsule)(const RECVX_ENEMY_CAPSULE* a, const RECVX_ENEMY_CAPSULE* b);
    /* plp->mnwP: 1 = ewP->mnwP (en17.c:1463), 0 = plp->mnwPb (en17.c:1432). */
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
} RECVX_EN17_SERVICES;

void recvx_en17_bind(const RECVX_EN17_SERVICES* services);

/* Einstieg bhEne17 (en17.c:347) und die Kettenwerke bhEne17LArm/RArm (en17sub.c:23/101). */
void recvx_en17(RECVX_ENEMY_WORK* w);
void recvx_en17_larm(RECVX_ENEMY_WORK* w);
void recvx_en17_rarm(RECVX_ENEMY_WORK* w);

/* Klingenspitze des linken Arms: njCalcPoint(arm->mlwP->owP[1].mtx,
   (0,0,-17.5)) (en54.c/en55.c CollCheck). 0, wenn Kettenwerk oder Dienst fehlen. */
int recvx_en17_blade_tip(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out);

/* ((unsigned int*)epw->exp0)[2] |= 0x20000000; en17.c:385 liest, :397 loescht. */
void recvx_en17_blade_hit(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
