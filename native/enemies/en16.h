#ifndef RECVX_NATIVE_EN16_H
#define RECVX_NATIVE_EN16_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 16 — Alfred Ashford (Scharfschuetze). Nativ eigenstaendig bestimmt.
   Semantikreferenz: en16.c (Mode0[6] :15, BrainType[1] :25, MoveMode2[3] :30,
   bhEne16 :38, Init :53, Brain :100, BR00 :106, Move :126, MV00 :136,
   MV01 :145, MV02 :154, GetEffNum :387, GetEffPos :393, AddEffPos :399),
   effsub2.c:2353 (bhEff_E16_LaserSight) und :2390 (…Draw), effect.c:825
   (Sprungtabelle bhJumpEffect2[id-250]: 270 = Eintrag 20 = E16_LaserSight),
   zonzon1.c:828 (bhEne_CallSE), :846 (bhEne_CallEffectSE), :243
   (bhEne_SetBloodEffectBurst), :505/:515 (bhEne_DirCheck/_DGDirCheck) und
   hitchk.c:5474 (bhCheckFloorEnemy). Kein Originalkoerper, kein Generator,
   keine zweite Workwelt.

   Traegermechanismus ist der Laserstrahl-Effekt 270. Init legt den 104 Byte
   grossen EXP0-Block an (en16.c:74 bhEne_CallocWork(104, 8)):
     +0x00 EXP0_I(0)      Zeiger auf eff[eff_id] (en16.c:89)
     +0x04 EXP0_P(0..7)   acht Punkte (EXP0_P(i) = exp0+4+12*i, macros.h:80);
                          benutzt werden 0..7 als Ringpuffer, GetEffNum
                          begrenzt den Zaehler auf 7 (en16.c:410)
     +0x64 EXP0_I(0x64)   Anzahl gueltiger Punkte (en16.c:186/389/410)
   AddEffPos schiebt P[0..n] um eins nach hinten und legt den neuen Punkt
   auf P[0]; GetEffPos/GetEffNum werden von bhEff_E16_LaserSightDraw
   (effsub2.c:2390) ueber `op->lkwkp` gelesen (nativ step_e16_laser in
   effect_sprites.cpp).

   hp setzt en16 NICHT (wie en08): im Original steht die Trefferzahl im
   ETTY_WORK und bhSetEnemy (eneset.c:183-241) kopiert sie nicht in den Work.

   Einstieg: bhEne16 (en16.c:38); der Dispatch-Eintrag 0x10 der Tabelle
   (eneset.c:51) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en16(RECVX_ENEMY_WORK* w);

/* Sicht auf den Laserstrahl-Effekt (BH_PWORK/O_WRK-Kopf), die en16 liest. */
typedef struct {
    int32_t mode0;           /* +0x0C */
    int32_t ct0;             /* +0x3D4 bhEff_E11_CheckCollisionPlayer */
    RECVX_ENEMY_VEC gpx;     /* +0x58 */
    RECVX_ENEMY_VEC lox;     /* +0x64 */
    void*   atr;             /* +0x2F0 getroffene Flaeche oder NULL */
} RECVX_EN16_EFF;

/* Anbieter des Effektbesitzers (eff[512] des Raums). Slots sind eff-Indizes;
   NULL-Dienst = benannte Luecke. */
typedef struct {
    /* en16.c:75-87 bhSetEffectTb(&sys->ef {flg 1, id 270, Skala 0}, NULL, epw, 0) */
    int  (*effect_tb)(RECVX_ENEMY_WORK* w);
    int  (*effect_get)(int slot, RECVX_EN16_EFF* out);
    void (*effect_mode)(int slot, int32_t mode0);
    /* en16.c:91 bhEff_E16_LaserSight(eff) unmittelbar nach dem Anlegen */
    void (*effect_step)(int slot);
    /* hitchk.c:5474 bhCheckFloorEnemy(flr_no, px, pz): 1 und prm0 bei Treffer */
    int  (*floor_enemy)(int flr_no, float px, float pz, int32_t* prm0);
    /* hitchk.c:5881 bhCheckWallAttrB89(hp) auf der Flaeche aus eff->exp0; prm0 dazu. */
    int  (*wall_attr_b89)(const void* atr, int32_t* prm0);
    /* effsub3.c:2401 bhSetEffGunSpark(pos, dir, src, dst, typ) */
    void (*gun_spark)(const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* dir, uint32_t src, uint32_t dst, int typ);
    int hiteff13; /* weapon.c WpnTab[13].hiteff */
    /* weapon.c:1632 bhSetGunSplash(gap, pos, wattr) mit gap->vx/vy/vz, wpn_no, ay. */
    void (*gun_splash)(const RECVX_ENEMY_VEC* gvec, const RECVX_ENEMY_VEC* pos, int wattr, int wpn_no, int ay);
    /* en16.c:265 bhSetGunSplash hat bewusst keinen Dienst: der uebergebene GA_WORK
       traegt in vx/vy/vz uninitialisierten Stackinhalt (siehe en16.c, MV02 mode3 4);
       en16 meldet den Aufruf als benannte Luecke. */
} RECVX_EN16_SERVICES;
void recvx_en16_bind(const RECVX_EN16_SERVICES* services);

/* en16.c:387/393/399 — Effektpunkte des Laserstrahls. Der Aufrufer ist die
   Zeichenroutine des Effekts (effsub2.c:2390) und uebergibt immer einen
   gueltigen Punkt; en16.c:408 kopiert ihn ohne Nullpruefung. Nativ meldet
   NULL diesen Aufruferfehler, statt fremden Speicher zu lesen. */
int  recvx_en16_eff_num(const RECVX_ENEMY_WORK* w);
const RECVX_ENEMY_VEC* recvx_en16_eff_pos(const RECVX_ENEMY_WORK* w);
void recvx_en16_add_eff_pos(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos);

#ifdef __cplusplus
}
#endif
#endif
