#ifndef RECVX_NATIVE_EN26_H
#define RECVX_NATIVE_EN26_H

#include <stdint.h>
#include "enemy_work.h"
#include "en01_work.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 26 — Anatomist Zombie (en26.c:23), der schnelle Ereigniszombie
   (RM_0110 Gegner 4). Nativ eigenstaendig bestimmt. Semantikreferenz:
   en26.c (:561 Mode0[6], :571 MoveMode2[17], :592 NageMode2[1],
   :597 DamageMode2[16], :617 DieMode2[1], :622 PlyDmgMode[1], :628 bhEne26
   bis :3434 bhEne26_SePlay) und die geteilten Helfer aus zonzon.c/pwksub.c/
   en01.c/en01b.c, die en26.c ruft (bhEne01_RotNeck, bhEne01_CollCheckPush,
   bhEne01_EnemyPushChk). Kein Originalkoerper, kein Generator, keine zweite
   Workwelt. Zustandsbesitzer ist allein RECVX_ENEMY_WORK samt dem exp0-Block
   unten. Ein ungebundener Dienst meldet die Originalroutine ueber gap. */

/* ---------------------------------------------------------------------------
   exp0-Block — bhEne_CallocWork(160, 8) (en26.c:1803)
   ---------------------------------------------------------------------------
   en26.c adressiert den Block nur ab 0x28. en01.c:4114 (bhEne01_CollCheckPush)
   und en01.c:8029 (bhEne01_EnemyPushChk) lesen und schreiben EP_EXP0_I(0x40)
   auch an fremden id-26-Works (en01.c:4107/8021 `ep->id == 1 || ep->id == 26`)
   und am greifenden id-26-Work selbst (EXP0_I(0x40) & 0x4000, en01.c:8064).
   Beide Bloecke haben das Flagwort also am selben Originaloffset 0x40. Nativ
   liegt es deshalb am selben Offset wie RECVX_EN01_EXP0.flags: der von en26.c
   nie adressierte Anfang 0x00..0x27 hat die native Breite des en01-Anfangs
   (neun Zeiger und ein int, en01_work.h:91-106). Die Asserts in en26.c belegen
   Reihenfolge und Gleichlage. Datenrepraesentation, keine andere Entscheidung. */
typedef struct RECVX_EN26_EXP0 {
    /* 0x00 */ void*    unused00[9];        /* von en26.c nicht adressiert     */
    /* 0x24 */ int32_t  unused24;
    /* 0x28 */ uint8_t  search;             /* EneSearch-Zaehler/Sicht :1923-1958 */
    /* 0x29 */ uint8_t  search_hold;        /* :1950/:1954                    */
    /* 0x2A */ uint8_t  route_id;           /* Brain00 :1974/:1986            */
    /* 0x2B */ uint8_t  route_ct;           /* Brain00 :1975/:1980/:1987      */
    /* 0x2C */ uint8_t  flinch_step;        /* SetMtn rot_tbl-Spalte :1143-1171 */
    /* 0x2D */ uint8_t  unused2D;
    /* 0x2E */ uint8_t  unused2E;
    /* 0x2F */ uint8_t  eat_wait;           /* bhEne26 :638, ActionModeCheck :2028 */
    /* 0x30 */ int32_t  blood_wait;         /* bhEne26 :643, SetBlood :1013   */
    /* 0x34 */ int32_t  san_wait;           /* bhEne26 :648, DamageAdd :762   */
    /* 0x38 */ int32_t  fire_mask;          /* LinkFireEffect :831, Die :1198 */
    /* 0x3C */ uint32_t mat_color;          /* Init :1833, DamageAdd :784, Die :1199 */
    /* 0x40 */ uint32_t flags;              /* Hauptflagwort                  */
    /* 0x44 */ uint32_t flags2;             /* 0x20 Trefferseite, 0x40, 0x80 Lauf-Overlay */
    /* 0x48 */ int32_t  counter48;          /* DmgCheck :692                  */
    /* 0x4C */ int32_t  unused4C;
    /* 0x50 */ int32_t  neck_angle;         /* SearchNeck :1424-1457          */
    /* 0x54 */ float    player_distance;    /* Move :1872                     */
    /* 0x58 */ RECVX_ENEMY_VEC ikou_target; /* Brain00 :1972/:1984, MV02 :2253 */
    /* 0x64 */ RECVX_ENEMY_VEC grab_point;  /* PlyDG00 :2802, PlayerLink :1707, MV05 :2337 */
    /* 0x70 Im Original ATR_WORK* aus bhCollisionCheckLine (:1531); nativ die
       stabile Flaechenidentitaet des Dienstes `line`, 0 = kein Treffer
       (NULL). Gelesen nur als == 0 (:1898, :1970). */
    /* 0x70 */ int32_t  line_hit;
    /* 0x74 */ int32_t  unused74;
    /* 0x78 */ int32_t  head_damage;        /* DamageAdd :757, -1 = Kopf geplatzt :870 */
    /* 0x7C */ int32_t  word7C;             /* MV02 :2232, nie gelesen        */
    /* 0x80 */ int32_t  overlay_frame;      /* MTN-23-Uhr des Lauf-Overlays :1184-1186 */
    /* 0x84 */ int32_t  word84;             /* MV02 :2203/:2219, nie gelesen  */
    /* 0x88 */ int32_t  word88;             /* MV02 :2204/:2220, nie gelesen  */
    /* 0x8C */ uint8_t  tail8C[0x14];       /* bis 0xA0, nie adressiert       */
} RECVX_EN26_EXP0;

/* sys->ef-Felder, die bhEne26_LinkFireEffect setzt (en26.c:816-823, :835). */
typedef struct {
    uint32_t flg;
    int32_t  id, type;
    float    px, py, pz;
    float    sx, sy, sz;
    int32_t  ay;
} RECVX_EN26_EF;

/* Dienste, die EN26 zusaetzlich zu RECVX_ENEMY_SERVICES und dem EN01-Satz
   (recvx_en01_context()->en01) braucht. NULL -> gap. */
typedef struct RECVX_EN26_SERVICES {
    /* effect.c:436 bhSetEffectTb(&sys->ef, ofs, epw, obj) auf der
       PERSISTENTEN Vorlage sys->ef: nur flg/id/type/sz/px/py/pz/ay
       (en26.c:816-823) und sx/sy (:835) werden geschrieben; mdlver, flr_no,
       ax und die uebrigen Vorlagenfelder bleiben vom letzten Schreiber.
       Rueckgabe Slot oder -1. */
    int  (*effect_tb)(const RECVX_EN26_EF* ef, const RECVX_ENEMY_VEC* ofs,
                      RECVX_ENEMY_WORK* link, int lkono);
    /* eff[eno].zn = len; eff[eno].ct3 = time (en26.c:840-841/:847-848). */
    void (*effect_zn_ct3)(int slot, float zn, int32_t ct3);
    /* mlwP->objP += n, owP += n, obj_num -= n um bhSetMotion herum
       (en26.c:1180-1190, n = 10); `nodes` < 0 nimmt die Verschiebung
       zurueck. Rueckgabe 0 = nicht ausfuehrbar. */
    int  (*shift_object_window)(RECVX_ENEMY_WORK* w, int32_t nodes);
} RECVX_EN26_SERVICES;

void recvx_en26_bind(const RECVX_EN26_SERVICES* services);
const RECVX_EN26_SERVICES* recvx_en26_bound(void);

/* bhJumpEnemy[26] (eneset.c:51): bhEne26 (en26.c:628). */
void recvx_en26(RECVX_ENEMY_WORK* w);

/* Einzelzustaende fuer Tests und Dispatchpruefung. */
void recvx_en26_init(RECVX_ENEMY_WORK* w);            /* en26.c:1772 */
int  recvx_en26_set_mtn(RECVX_ENEMY_WORK* w);         /* :1117 */
void recvx_en26_dmg_check(RECVX_ENEMY_WORK* w);       /* :663  */
RECVX_EN26_EXP0* recvx_en26_exp0(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif

#endif /* RECVX_NATIVE_EN26_H */
