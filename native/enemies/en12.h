#ifndef RECVX_NATIVE_EN12_H
#define RECVX_NATIVE_EN12_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Alexia I (ID 12). Eigenstaendiger nativer Mechanismus auf dem Work des
   WorldEntity-Besitzers. Referenz: src/ps2/veronica/prog/en12.c:478-2204.
   Keine Originalkoerper, Generatoren oder zweite Spieler-/Effektwelt.
   Alle Dienste unten sind BLATTZUGRIFFE auf vorhandene Besitzer. */
typedef struct {
    int32_t joint;
    RECVX_ENEMY_VEC offset;
    float size;
    int32_t life, delay;
} RECVX_EN12_FIRE;

typedef struct {
    uint32_t id, flg, type, mdlver;
    RECVX_ENEMY_VEC pos, scale;
    int32_t ay;
} RECVX_EN12_EFFECT_TEMPLATE;
#define RECVX_EN12_EF_ID       0x001u
#define RECVX_EN12_EF_FLG      0x002u
#define RECVX_EN12_EF_TYPE     0x004u
#define RECVX_EN12_EF_MDLVER   0x008u
#define RECVX_EN12_EF_POS      0x010u
#define RECVX_EN12_EF_SX       0x020u
#define RECVX_EN12_EF_SY       0x040u
#define RECVX_EN12_EF_SZ       0x080u
#define RECVX_EN12_EF_AY       0x100u

typedef struct {
    uint32_t id, flg, stflg;
    uint8_t mode0;
    int32_t ct0, ct1, ax, ay, tex_id;
    RECVX_ENEMY_VEC pos, velocity, normal;
    float ofy;
    int32_t exp1_slot; /* exp1 -> eff[slot]; -1 = NULL, nicht Poolindex 0 */
} RECVX_EN12_EFFECT_VIEW;

typedef struct RECVX_EN12_SERVICES {
    /* rutchk.c bhCheckRoute(from,to,out): out ist xn/yn/zn, nicht nur XZ. */
    int (*route_points)(const RECVX_ENEMY_VEC*, const RECVX_ENEMY_VEC*, RECVX_ENEMY_VEC*);
    /* objP-Knoten: nur lokale Pose lesen; joint_pose_set aendert genau ang
       und owP.mtx von Knoten 5 wie en12.c:1488-1495. */
    int (*local_node)(const RECVX_ENEMY_WORK*, uint32_t, RECVX_ENEMY_VEC*, int32_t*);
    int (*motion_angles)(const RECVX_ENEMY_WORK*, uint32_t, uint32_t, uint32_t, int32_t*);
    void (*joint_pose_set)(RECVX_ENEMY_WORK*, uint32_t, const int32_t*, const float*);
    /* Motion.c bhCalcFixOffset(w,chain,NULL,out); chain ist terminierter
       signed-byte Knotenpfad, Quelle dieselbe mutable objP/owP-Pose. */
    int (*fix_offset)(RECVX_ENEMY_WORK*, const int8_t*, RECVX_ENEMY_VEC*);
    /* mlwP=mdl[0], obj_a=mdl[0].objP, obj_b=mdl[1].objP (en12.c:800). */
    void (*morph_sources)(RECVX_ENEMY_WORK*, uint32_t, uint32_t);
    /* hitchk.c bhCheckFloorEnemy: Rueckgabe 0 = kein Treffer. */
    int (*floor_enemy)(int, float, float, RECVX_ENEMY_ATR*);
    /* rm_flg &= ~bits, vorhandener Event/Systembesitzer. */
    void (*system_flags_clear)(uint32_t which, uint32_t bits);
    /* EffectTb: zuerst NUR maskierte Felder der vorhandenen gemeinsamen
       Effektvorlage schreiben, dann Original-Poolallokation. Nicht genannte
       Felder bleiben erhalten (z.B. sz bei Feuerbinta). -1 = Pool voll.
       Get/set sind Views desselben Slots; nicht genannte Effektfelder bleiben
       unveraendert. Ein wiederverwendeter Slot behaelt seine Originalidentitaet. */
    int (*effect_tb)(const RECVX_EN12_EFFECT_TEMPLATE*, uint32_t mask,
                     RECVX_ENEMY_WORK*, const RECVX_ENEMY_VEC*, int joint);
    int (*effect_get)(int, RECVX_EN12_EFFECT_VIEW*);
    void (*effect_set)(int, const RECVX_EN12_EFFECT_VIEW*);
    void (*effect_texture)(int slot, const RECVX_ENEMY_WORK*, uint32_t model);
    /* plp besitzt diese Felder; sie werden NICHT in EN12 gespiegelt. */
    int (*player_extra_get)(int32_t* ct1, int32_t* ayp);
    void (*player_extra_set)(int32_t ct1, int32_t ayp);
    /* Quelle 0 = eigene plp->mnwPb; 1 = w->mnwP; 2 = w->mnwPb.
       en12.c:1683 stellt ausdruecklich epw->mnwPb ein, nicht plp->mnwPb. */
    void (*player_library)(const RECVX_ENEMY_WORK*, uint32_t source);
    int (*player_wall)(RECVX_ENEMY_PLAYER*);
    void (*player_texture_zero)(void);
    void (*player_material)(uint32_t rgba);
    void (*object_material)(uint32_t object, uint32_t count, uint32_t rgba);
    void (*player_fire)(int joint, const RECVX_ENEMY_VEC*, float size, int life);
} RECVX_EN12_SERVICES;

/* Typisierte exp0-Daten statt ILP32-Zeigerwoerter. Original 0xB0:
   +4..20 Countdown, +24 Kette, +28 Look-Interpolation, +32 eff262,
   +40..100 eff274-Zeiger, +160 letzte Handposition. Nicht verwendete
   Bytebereiche sind keine weitere Zustandsmaschine. */
typedef struct {
    int32_t wait[5];
    const int8_t* foot_chain;
    int32_t look_count;
    int32_t surface_slot;        /* -1 = NULL, -2 = original &eff[-1] */
    int32_t hand_slot[16];
    RECVX_ENEMY_VEC previous_hand;
    const RECVX_EN12_SERVICES* services;
    uint8_t original_allocated;
} RECVX_EN12_EXT;

/* Vor erstem Tick binden; Bindung alloziert ueber den vorhandenen
   calloc_work-Dienst und schreibt nur w->exp0. NULL/fehlender Dienst wird
   gemeldet. Keine globale Servicetabelle, keine zweite Workliste. */
int recvx_en12_bind(RECVX_ENEMY_WORK*, const RECVX_EN12_SERVICES*);
void recvx_en12(RECVX_ENEMY_WORK*);
/* en12.c:2089 bhEne12_SetFireBintaEffect; Aufrufer ausserhalb von EN12:
   effsub2.c:3678-3688 (bhEff_E12_BintaEffControl, Effekt 276). */
void recvx_en12_set_fire_binta_effect(RECVX_ENEMY_WORK*, int action);
#ifdef __cplusplus
}
#endif
#endif
