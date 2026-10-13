#ifndef RECVX_NATIVE_EN21_H
#define RECVX_NATIVE_EN21_H
#include "enemy_work.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 21 — Albinoid Infant. Nativ eigenstaendig bestimmt.
   Semantikreferenz: en21.c (:54 Mode0[6], :67 MoveMode2[9], :88 bhEne21,
   :155 Init, :256 SearchPlayer, :288 BR00, :325 Move, :344..:735 MV00..MV08,
   :744 NG00, :840 Damage, :885 DD00, :931 PlayerControl, :1021
   AllWayWallCheck, :1085 SetSparkEff, :1168 bhEff_E21_Spark, :1299 SetMarkEff,
   :1324 bhEff_E21_Mark), zonzon1.c (bhEne_DirTarget :433, bhEne_CallSE :828,
   bhEne_SetBloodEffect2 :139, bhEne_BloodPool :391), pwksub.c:133 bhAddSpeed,
   rutchk.c:5 bhCheckRoute, eneset.c:906 bhEne_CallocWork, effect.c:436
   bhSetEffectTb und :644 bhSetShadow, Motion.c bhSetMotion, MdlPut.c:240
   bhCalcModel, hitchk.c bhCheckWall/bhCheckPlayer/bhCheckWallType.
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   EXP0-Block (en21.c:159 bhEne_CallocWork(0x18, 8)), Original-Byteoffsets:
     +0x00 float          Abstand zum Spieler (:263; Init 999.0f)
     +0x04 int            Winkel zum Spieler relativ ay, (short) (:269)
     +0x08 float          Spielerposition x (:264; MV01 ueberschreibt mit dem
                          Routenpunkt :419/:424)
     +0x0C float          Spielerposition y (:265)
     +0x10 float          Spielerposition z (:266 / :420 / :425)
     +0x14 unsigned char  Spieler im Sichtfeld (|Winkel| < 100 Grad, :271)
     +0x15 unsigned char  Griff-Sperrzeit; wird je Pass heruntergezaehlt
                          (:148-151) und von NG00 :820 auf 35 gesetzt
     +0x16 unsigned char  naechste Sterbemotion 4..6 (:182/:894-909); DD00
                          schreibt sie in ALLE Works der Identitaet 21
     +0x17 unsigned char  1 = dieser Work steuert gerade den Spieler (:754)

   Die Griffsequenz (NG00) schreibt Felder des Spielerworks. Alles, was
   RECVX_ENEMY_PLAYER fuehrt, laeuft ueber player_get/player_set; plp->way,
   plp->djnt_no und plp->dpx..dpz haben eigene Besitzer-Dienste (unten bzw.
   RECVX_ENEMY_EXT.player_hit_set). */
typedef struct {
    float         player_dist;    /* +0x00 EXP0_F(0x0)  */
    int32_t       player_ang;     /* +0x04 EXP0_I(0x4)  */
    float         target_x;       /* +0x08 EXP0_F(0x8)  */
    float         target_y;       /* +0x0C EXP0_F(0xC)  */
    float         target_z;       /* +0x10 EXP0_F(0x10) */
    unsigned char in_view;        /* +0x14 EXP0_UC(0x14) */
    unsigned char grab_wait;      /* +0x15 EXP0_UC(0x15) */
    unsigned char death_mtn;      /* +0x16 EXP0_UC(0x16) */
    unsigned char controlling;    /* +0x17 EXP0_UC(0x17) */
} RECVX_EN21_EXP0;

/* Dienste, die nur EN21 braucht und der gemeinsame Vertrag nicht fuehrt.
   Alles andere (Wandabfrage, Schatten, Motion, Blutpfuetze, Effekte,
   Spielersicht, Gegnerliste) laeuft ueber RECVX_ENEMY_SERVICES bzw.
   RECVX_ENEMY_EXT. Ein fehlender Dienst wird namentlich als Luecke gemeldet. */
typedef struct {
    /* rutchk.c:5 bhCheckRoute(ps0, ps1, ps2): Rueckgabe ist das ROHE
       unsigned char des Originals (0xFF = kein Weg bzw. gleiche Zone); `out`
       ist nur bei != 0xFF gueltig. Anbieter: dieselbe Funktion, die
       world_enemies.cpp als RECVX_EN01_SERVICES.route_next bindet. */
    int (*check_route)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to,
                       RECVX_ENEMY_VEC* out);
    /* sys->gm_mode (en21.c:210), 1 = vorhanden. */
    int (*game_mode)(int* out);
    /* plp->way = epw->ay (en21.c:796; Blutrichtung, vom Spielerbesitzer
       gefuehrt, RECVX_ENEMY_PLAYER fuehrt es nicht). */
    void (*player_set_way)(int16_t way);
} RECVX_EN21_SERVICES;

void recvx_en21_bind(const RECVX_EN21_SERVICES* sv);
void recvx_en21(RECVX_ENEMY_WORK* w);

/* AllWayWallCheck (en21.c:1021): Bits 0x1 voraus, 0x2 links, 0x4 rechts,
   0x8 hinten, je ueber wall_type. Oeffentlich, weil der Test sie einzeln
   prueft. */
unsigned char recvx_en21_all_way_wall_check(RECVX_ENEMY_WORK* w, float dist);

/* Die beiden Effektschritte des Originals (bhEff_E21_Spark :1168 und
   bhEff_E21_Mark :1324) auf den O_WRK-Feldern, die sie lesen/schreiben.
   Der Effektbesitzer (native/effect_sprites.cpp) bindet sie an die Effekt-IDs
   399 bzw. 396 und reiht bei Rueckgabe 1 den Slot in ef_trs ein
   (sys->ef_trs[sys->ef_trsn++] = op, nur bei ef_trsn < 512, :1290/:1386).
   Rueckgabe 0 = nicht einreihen (flg wurde ggf. auf 0 gesetzt). Zufall kommt
   aus random_next, NULL = der gemeinsame EE-Strom. */
typedef struct {
    uint32_t flg, stflg;
    uint16_t type;
    uint8_t  mode0;
    uint32_t bl_src, bl_dst;
    int32_t  ct0, ct1, ct2;
    float    px, py, pz;
    int32_t  ay;
    float    sx, sy, sxb, syb, yn;
    struct { uint32_t col; float u, v; } tv[4];
    int32_t  (*random_next)(void);
} RECVX_EN21_EFFECT;
int recvx_en21_effect_spark(RECVX_EN21_EFFECT* op);
int recvx_en21_effect_mark(RECVX_EN21_EFFECT* op);

#ifdef __cplusplus
}
#endif
#endif
