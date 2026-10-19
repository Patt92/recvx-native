#ifndef RECVX_NATIVE_EN22_H
#define RECVX_NATIVE_EN22_H
#include <stddef.h>
#include <stdint.h>
#include "enemy_work.h"
#include "en01_work.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Identitaet 22 — Albinoid Adult (en22.c:14), der elektrische Wasserbewohner
   von RM_7100 (Gegner 0). Nativ eigenstaendig bestimmt. Semantikreferenz:
   en22.c (:506 bhEne22_DmmyBrain bis :2993 bhEne22_ChgDengekiColor) und die
   geteilten Helfer aus zonzon.c/zonzon1.c/pwksub.c, die en22.c ruft. Kein
   Originalkoerper, kein Generator, keine zweite Workwelt. Zustandsbesitzer
   ist allein RECVX_ENEMY_WORK samt dem exp0-Block unten. Ein ungebundener
   Dienst meldet die Originalroutine ueber gap. */
/* exp0-Block — bhEne_CallocWork(0xB0, 8) (en22.c:945). Offsets wie die
   EXP0_*-Zugriffe des Originals; EXP0_ATR(n) liegt bei 4 + 4n (en07.c:52),
   EXP0_ATR(17) also bei 0x48. */
typedef struct RECVX_EN22_EXP0 {
    /* 0x00 */ uint8_t  search;            /* EXP0_UC(0): EneSearch :1125     */
    /* 0x01 */ uint8_t  pad01[7];
    /* 0x08 */ uint32_t flags;             /* EXP0_I(0x8) Hauptflagwort       */
    /* 0x0C */ int32_t  wait0C;            /* :548 nur dekrementiert          */
    /* 0x10 */ int32_t  fire_wait;         /* :552, DamageAdd :735/:748       */
    /* 0x14 */ int32_t  shock_wait;        /* :556, Move :1012                */
    /* 0x18 */ int32_t  spark_step;        /* SparkEffect :2340/:2358         */
    /* 0x1C */ int32_t  hit_window;        /* :560, Move :1013, PlyerHitCheck */
    /* 0x20 */ float    dist;              /* EXP0_F(0x20) Spielerabstand     */
    /* 0x24 */ float    trg_x;             /* Zielpunkt x                     */
    /* 0x28 */ float    unused28;
    /* 0x2C */ float    trg_z;             /* Zielpunkt z                     */
    /* 0x30 */ uint8_t  unused30[0xC];
    /* 0x3C */ float    key_x;             /* GetTranslateMtn :2244           */
    /* 0x40 */ float    key_y;
    /* 0x44 */ float    key_z;
    /* 0x48 */ int32_t  atr17;             /* EXP0_ATR(17) = bhCheckWallType, nie gelesen */
    /* 0x4C */ int32_t  light_effect;      /* O_WRK* des 353 (:2446); nativ Slot, -1 = NULL */
    /* 0x50 */ float    reach;             /* EXP0_F(0x50) MV04 :1736         */
    /* 0x54 */ uint8_t  unused54[8];
    /* 0x5C */ int32_t  word5C;            /* MV04 :1705, nie gelesen         */
    /* 0x60 */ uint32_t argb[20];          /* SparkEffect Materialfarben      */
} RECVX_EN22_EXP0;
/* sys->ef-Felder, die en22.c schreibt (bhEne22_SetDengekiEffect :2621-2628,
   SetDengekiEffect2 :2664-2675, SetElectricLightEffect :2802-2811). `mask`
   nennt die Felder ueber den gemeinsamen Satz hinaus (ax, mdlver). */
#define RECVX_EN22_EF_AX      0x1u
#define RECVX_EN22_EF_MDLVER  0x2u
typedef struct {
    uint32_t flg;
    int32_t  id, type;
    float    px, py, pz;
    float    sx, sy, sz;
    int32_t  ax, ay;
    uint8_t  mdlver;
    uint32_t mask;
} RECVX_EN22_EF;
/* Nachfelder am eff[]-Platz (:2637-2641, :2680-2688, :2814-2816). */
#define RECVX_EN22_EFS_STFLG  0x01u  /* stflg |= stflg_or                 */
#define RECVX_EN22_EFS_TEX    0x02u  /* tex_id; txp[0] = tex_from->mlwP->texP, wenn tex_from */
#define RECVX_EN22_EFS_CT3    0x04u  /* ct3                               */
#define RECVX_EN22_EFS_ZN     0x08u  /* zn                                */
#define RECVX_EN22_EFS_ANG    0x10u  /* ax/ay/az                          */
#define RECVX_EN22_EFS_FLR    0x20u  /* flr_no                            */
#define RECVX_EN22_EFS_MODEL  0x40u  /* mlwP = &model_of->mdl[model], txp[0] = mlwP->texP (nach TEX) */
typedef struct {
    uint32_t stflg_or;
    struct RECVX_ENEMY_WORK* tex_from;
    int32_t  tex_id;
    int32_t  ct3;
    float    zn;
    int32_t  ax, ay, az;
    int32_t  flr_no;
    struct RECVX_ENEMY_WORK* model_of;
    int32_t  model;
} RECVX_EN22_EFSLOT;
/* rom->lgtp[2] (LGT_WORK), wie bhEne22_SetLight/CtrLight es schreibt (:2857-2991). */
typedef struct {
    uint32_t flg;
    int32_t  aspd, lsrc, type;
    float    r, g, b, nr, fr;
    int32_t  lkflg, lkno, lkono;
    float    lx, ly, lz, px, py, pz;
    int32_t  mode, ct0, ct1;
} RECVX_EN22_LIGHT;
typedef struct RECVX_EN22_SERVICES {
    /* effect.c:436 bhSetEffectTb(&sys->ef, ofs, link, lkono) auf der persistenten
       Vorlage; link_player != 0 verknuepft mit plp (:2779). Rueckgabe Slot/-1. */
    int  (*effect_tb)(const RECVX_EN22_EF* ef, const RECVX_ENEMY_VEC* ofs,
                      RECVX_ENEMY_WORK* link, int link_player, int lkono);
    void (*effect_slot)(int slot, uint32_t mask, const RECVX_EN22_EFSLOT* in);
    /* :1711-1715 / :657-661: (op->flg != 0 && op->id == 353 && op->lkwkp == epw) -> op->mode0 = 4. */
    void (*effect_light_off)(int slot, RECVX_ENEMY_WORK* w);
    /* rom->lgtp[2] lesen/schreiben. */
    int  (*light_get)(RECVX_EN22_LIGHT* out);
    void (*light_set)(const RECVX_EN22_LIGHT* in);
    /* epw->mdl[mdl].objP[obj].model->plist als beschreibbarer Strom des Werks
       (bhEne22_ChgDengekiColor :2993 / GetDengekiColorAddr :2416 schreiben dort).
       Rueckgabe 0 -> gap. */
    int  (*model_plist)(RECVX_ENEMY_WORK* w, int mdl, int obj, uint8_t** plist, size_t* size);
    /* bhSetWaterSplash2(plp, pos, type, sx, sy, sz) (:2127) am Spielerbesitzer. */
    void (*player_water_splash)(const RECVX_ENEMY_VEC* pos, int type, float sx, float sy, float sz);
} RECVX_EN22_SERVICES;
void recvx_en22_bind(const RECVX_EN22_SERVICES* services);
const RECVX_EN22_SERVICES* recvx_en22_bound(void);
/* bhJumpEnemy[22] (eneset.c): bhEne22 (en22.c:512). */
void recvx_en22(RECVX_ENEMY_WORK* w);
void recvx_en22_init(RECVX_ENEMY_WORK* w);               /* :911  */
int  recvx_en22_dmg_chk(RECVX_ENEMY_WORK* w);            /* :592  */
void recvx_en22_ctr_light(RECVX_ENEMY_WORK* w);          /* :2905 */
int  recvx_en22_get_area_no(float px, float pz);         /* :2512 */
RECVX_EN22_EXP0* recvx_en22_exp0(RECVX_ENEMY_WORK* w);
/* Materialchunk `no` im plist (:2416-2460): Byteversatz der ersten Farbe
   hinter dem Groessenwort und der Chunkkopf. 0 = Endchunk 255 erreicht. */
int  recvx_en22_material_chunk(const uint8_t* plist, size_t size, int no, size_t* color_at, int* head);
#ifdef __cplusplus
}
#endif
#endif /* RECVX_NATIVE_EN22_H */
