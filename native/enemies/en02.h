#ifndef RECVX_NATIVE_EN02_H
#define RECVX_NATIVE_EN02_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 2 — Gulp Worm. Eigenstaendige native Zustandsmaschine auf dem
   WorldEntity-Work. Semantikreferenz src/ps2/veronica/prog/en02.c:714-3384
   und en02sub.c (Kindwork Identitaet 31: Maul/Ring/Felsen). Kein
   Originalkoerper, kein Generator, keine zweite Workwelt. */

/* sys->ef-Felder, die eine EN02-Routine VOR bhSetEffectTb schreibt. Die
   Vorlage ist persistent (effect.c sys->ef); nur maskierte Felder werden
   gesetzt, die uebrigen bleiben stehen (bhEne02_SetSandParticle laesst
   sx/sy/sz/mdlver absichtlich vom vorigen Aufruf, en02.c:3007-3016). */
#define RECVX_EN02_EF_ID     0x01u
#define RECVX_EN02_EF_TYPE   0x02u
#define RECVX_EN02_EF_FLG    0x04u
#define RECVX_EN02_EF_POS    0x08u
#define RECVX_EN02_EF_SCALE  0x10u
#define RECVX_EN02_EF_MDLVER 0x20u
typedef struct {
    uint32_t id, type, flg, mdlver;
    RECVX_ENEMY_VEC pos;
    float sx, sy, sz;
} RECVX_EN02_EFF;

/* eff[eno]-Nachfelder (en02.c:2535-2537 SandSpr, :3026-3038 SandParticle). */
#define RECVX_EN02_EFS_VEC    0x01u   /* xn/yn/zn        */
#define RECVX_EN02_EFS_SCALE  0x02u   /* sx/sy/sz        */
#define RECVX_EN02_EFS_SCALEB 0x04u   /* sxb/syb/szb     */
typedef struct {
    float xn, yn, zn;
    float sx, sy, sz;
    float sxb, syb, szb;
} RECVX_EN02_EFFSET;

typedef struct RECVX_EN02_SERVICES {
    /* eneset.c:170 bhSetEnemy(&child) mit epp->mlwP = &epw->mdl[base]
       (en02.c:777-803); obj_a/obj_b setzt der Aufrufer ueber morph_sources. */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent, const RECVX_ENEMY_TEMPLATE* tmpl, uint32_t base);
    /* epp->mlwP = &epw->mdl[base] nachtraeglich (en02.c:3159 FallingRock). 1 = gesetzt. */
    int  (*child_model)(RECVX_ENEMY_WORK* child, RECVX_ENEMY_WORK* parent, uint32_t base);
    /* obj_a = mdl[base+a].objP, obj_b = mdl[base+b].objP (en02.c:781-782). */
    void (*morph_sources)(RECVX_ENEMY_WORK* w, uint32_t a, uint32_t b);
    /* sys->gm_mode (en02.c:764). */
    int  (*game_mode)(int* out);
    /* sys->rm_flg |= set; &= ~clear (en02.c:975/1070/1367). */
    void (*room_flags)(uint32_t set, uint32_t clear);
    /* cam.ofx/ofy/ofz (en02.c:3074 CameraControl). */
    void (*camera_offset)(float x, float y, float z);
    /* plp->mnwP: 1 = epw->mnwP, 0 = plp->mnwPb (en02.c:2685/2778/2827). */
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
    /* plp->mdflg &= ~bits (en02.c:844). Bit 0x1 ist die Zeichensperre des
       Spielers (game.c:361); andere Bits haben nativ keinen Besitzer. 1 = getan. */
    int  (*player_mdflg_clear)(uint32_t bits);
    /* hitchk.c:5474 bhCheckFloorEnemy(flr_no, px, pz): erste Bodenflaeche type 2
       aus rom->flrp + sys->mflrp. `index` ist die Listenposition — die Original-
       routine vergleicht den ATR_WORK-ZEIGER (en02.c:3108), nativ die Position. */
    int  (*floor_enemy)(int flr_no, float px, float pz, RECVX_ENEMY_ATR* out, int32_t* index);
    /* zonzon1.c:852 bhEne_SetSEPan: RequestEnemySeBasic(sys->enow, pos, se, 0, 0). */
    void (*se_pan)(uint32_t enemy, const RECVX_ENEMY_VEC* pos, int se);
    /* pwksub.c bhAddSpeed(epw, mode) — en02sub.c:252 am Kindwork. */
    void (*add_speed)(RECVX_ENEMY_WORK* w, int mode);
    /* sys->ef maskiert schreiben und bhSetEffectTb(&sys->ef, NULL, NULL, 0);
       Slot oder -1 (effect.c:436). */
    int  (*effect_tb)(uint32_t mask, const RECVX_EN02_EFF* ef);
    /* eff[slot]-Nachfelder. */
    void (*effect_set)(int slot, uint32_t mask, const RECVX_EN02_EFFSET* v);
    /* hitchk.c:12 bhCheckWall(&dmy_ene) auf einem Stapel-Dummy (en02.c:1946-
       1962): gesetzt sind flg, stflg, px/py/pz, ar, ah; flr_no bleibt im
       Original uninitialisiert und wird nur fuer Waende mit attr 0x1 gelesen
       (hitchk.c:87). Rueckgabe 1 = Probe gelaufen, *stflg und *pos wie
       bhCheckWall geschrieben; 0 = nicht entscheidbar, Luecke gemeldet. */
    int  (*wall_probe)(uint32_t flg, uint32_t* stflg, RECVX_ENEMY_VEC* pos, float ar, float ah);
} RECVX_EN02_SERVICES;

/* exp0 = bhEne_CallocWork(144, 8) (en02.c:775) mit benannten Feldern. */
#define RECVX_EN02_ROCKS 16u
typedef struct RECVX_EN02_STATE {
    float shake;                        /* EXP0_F(36): Kamerawackel-Amplitude      */
    RECVX_ENEMY_WORK* mouth;            /* exp0+40: Kind type 0, mlwP = &mdl[5]    */
    RECVX_ENEMY_WORK* ring;             /* exp0+44: Kind type 1, mlwP = &mdl[4]    */
    RECVX_ENEMY_WORK* rocks[RECVX_EN02_ROCKS]; /* exp0+48..111: type 2, mlwP = &mdl[1] */
    int32_t attack_ct;                  /* EXP0_I(112)                             */
    int32_t warp_floor;                 /* exp0+116: ATR_WORK* als Listenindex, -1 = NULL */
    float car_target;                   /* EXP0_F(120)                             */
    int32_t wall_index;                 /* exp0+124: &sys->mwalp[n] als Index      */
    /* plp->xn / plp->zn (en02.c:1134-1135 schreiben, :1306-1307 lesen): das
       Schluckziel des Spielers. Im gesamten Original hat dieses Paar keinen
       anderen Leser oder Schreiber (grep plp->xn / plp->zn: nur en02.c), es
       ist Zwischenspeicher dieser einen Routine. Nativ liegt es deshalb hier
       — gleiche Werte, gleiche Lebensdauer, kein zweiter Spielerwork. */
    float swallow_x, swallow_z;
} RECVX_EN02_STATE;

void recvx_en02_bind(const RECVX_EN02_SERVICES* services);
/* Einstieg bhEne02 (en02.c:714) fuer bhJumpEnemy[2]. */
void recvx_en02(RECVX_ENEMY_WORK* w);
/* bhEne02sub (en02sub.c:40): Kindroutine, per bhEne_SetCallFunc auf Slot 31. */
void recvx_en02sub(RECVX_ENEMY_WORK* w);
/* en02.c:2252 bhEne02_SetSandEffect(epw, pos, type) — auch Skriptbefehl 0x98
   (event.c:4214 bhEffectSandSet). */
void recvx_en02_set_sand_effect(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int type);
/* en02.c:2258 bhEne02_SetSandEffectEV(eno, type1, type2) — Skriptbefehl 0xC3
   (event.c:8595 bhEffectSandSetMatsumoto); `ene` ist ene[eno]. */
void recvx_en02_set_sand_effect_ev(RECVX_ENEMY_WORK* ene, int type1, int type2);

#ifdef __cplusplus
}
#endif
#endif
