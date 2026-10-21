/* Identitaet 16 — Alfred Ashford (Scharfschuetze). Nativ eigenstaendig bestimmt.
   Semantikreferenz: en16.c (Mode0[6] :15, BrainType[1] :25 = { bhEne16_BR00 },
   MoveMode2[3] :30, bhEne16 :38, Init :53, Brain :100, BR00 :106, Move :126,
   MV00 :136, MV01 :145, MV02 :154, GetEffNum :387, GetEffPos :393,
   AddEffPos :399), effsub2.c:2353 (bhEff_E16_LaserSight) und :2390 (…Draw),
   effect.c:825 (Sprungtabelle bhJumpEffect2[id-250]: 270 = Eintrag 20),
   zonzon1.c:828 (bhEne_CallSE, Gate flg&0x10000), :846 (bhEne_CallEffectSE =
   RequestEnemySe(127,…)), :243 (bhEne_SetBloodEffectBurst), :495
   (bhEne_DirCheck) und :515 (bhEne_DGDirCheck(epw) = DirCheck(epw->ay,
   &epw->dvx)), MdlPut.c:240 (bhCalcModel), subpl.c:143 (bhEne_Event),
   hitchk.c:5474 (bhCheckFloorEnemy) und :5881 (bhCheckWallAttrB89).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Traegermechanismus ist der Laserstrahl-Effekt 270 (nativ step_e16_laser,
   effect_sprites.cpp); Spawn, Feldsicht und Modus laufen ueber
   RECVX_EN16_SERVICES (Bindung world_enemies.cpp). Fehlt ein Dienst, meldet
   Init den Spawn ueber `gap`, und weil `EXP0_I(0)` danach wie im Original bei
   eff_id < 0 exakt 0 ist, wertet der Rest der Datei die Originalbedingungen
   `if (EXP0_I(0) != 0)` wahrheitsgemaess mit 0 aus. Nur wo ein Effektfeld
   hinter einem erfuellten `EXP0_I(0) != 0` gelesen wuerde, meldet die Datei
   die fehlende Sicht namentlich.

   hp setzt diese Identitaet nicht (wie en08): die Trefferzahl steht im
   ETTY_WORK und bhSetEnemy (eneset.c:183-241) kopiert sie nicht in den Work.

   Dispatch-Eintrag 0x10 der Tabelle (eneset.c:51) gehoert dem
   Dispatcher-Besitzer und wird hier nur beschrieben, nicht angelegt. */
#include "en16.h"
#include "en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
   EXP0-Block: 104 Byte (en16.c:74 bhEne_CallocWork(104, 8)).
   EXP0_I(o) ist ein Byte-Offset in den Block (macros.h), EXP0_P(i) liegt bei
   4 + 12*i, EXP0_I(0x64) bei 0x64.
   --------------------------------------------------------------------------- */
typedef struct {
    int32_t slot;            /* +0x00 EXP0_I(0)     &eff[eff_id] (en16.c:89); nativ eff_id + 1, 0 = NULL */
    RECVX_ENEMY_VEC pts[8];  /* +0x04 EXP0_P(0..7)  macros.h:80                  */
    int32_t pts_num;         /* +0x64 EXP0_I(0x64)  Anzahl gueltiger Punkte      */
} RECVX_EN16_EXP0;

#define RECVX_EN16_EXP0_SIZE ((uint32_t)sizeof(RECVX_EN16_EXP0))

_Static_assert(sizeof(RECVX_EN16_EXP0) == 0x68u,
               "EXP0-Block ist 104 Byte (en16.c:74)");
_Static_assert(offsetof(RECVX_EN16_EXP0, pts) == 0x4u,
               "EXP0_P(0) liegt bei +0x4 (macros.h:80)");
_Static_assert(offsetof(RECVX_EN16_EXP0, pts) + 7u * sizeof(RECVX_ENEMY_VEC) == 0x58u,
               "der achte Punkt liegt bei +0x58 (Ringpuffer, en16.c:405)");
_Static_assert(offsetof(RECVX_EN16_EXP0, pts_num) == 0x64u,
               "EXP0_I(0x64) liegt bei +0x64 (en16.c:389)");

/* Felder des Laserstrahl-Effekts (BH_PWORK/O_WRK-Kopf), die en16 benutzt.
   Der Decompiler zeigt an zwei Stellen EXP0_I(0x3D4) und EXP0_I(0x2F0), also
   scheinbare Zugriffe weit hinter den 104-Byte-Block; die Disassembly von
   bhEne16_MV02 laedt EXP0_I(0) und indexiert DANN +0x3D4 bzw. +0x2F0 — das
   sind die BH_PWORK-Felder ct0 (0x3D4) und exp0 (0x2F0). en16.c:259
   (`(*(BH_PWORK**)epw->exp0)->lox`) belegt dieselbe Dereferenzkette fuer die
   uebrigen Felder. */
/* RECVX_EN16_EFF (en16.h) traegt genau diese Felder. */
static const RECVX_EN16_SERVICES* en16_extra;
void recvx_en16_bind(const RECVX_EN16_SERVICES* services) { en16_extra = services; }

/* ---------------------------------------------------------------------------
   Originalbausteine
   --------------------------------------------------------------------------- */

/* macros.h:93/105: njRandom() = (Float)((Float)-rand()/(Float)(RAND_MAX+1))
   mit RAND_MAX 0x7fffffff. Disassembly von bhEne16_MV02 (0x1EA994):
   `jal rand` -> `negu $v0,$v0` -> `cvt.s.w` -> `div.s` durch 0xCF000000
   = -2^31. Die Vorzeichen heben sich auf; der Wert liegt in [0,1). Der
   Vertragsdienst random_next (enemy_work.h:309) liefert rand(). */
static float en16_rand01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->random_next) return (float)sv->random_next() / 2147483648.0f;
    if (sv && sv->gap) sv->gap("rand");
    return 0.0f;   /* kein erfundener Zufallswert */
}

/* zonzon1.c bhArcTan2(y,x) — Koerper woertlich (asinf-Form, (short)-Umlauf
   des x<0-Zweigs; 10430.381 = 65536/2PI, macros.h:102). */
static int32_t en16_arc_tan2(float y, float x)
{
    float len;
    int32_t ang;

    len = njSqrt((x * x) + (y * y));
    if (len == 0.0f) return 0;

    ang = (int32_t)(10430.381f * asinf(y / len));
    if (x < 0.0f) ang = (int16_t)(32768 - ang);
    return ang;
}

/* zonzon1.c:495 bhEne_DirCheck(ay, vec) — identischer Koerper. 0 bei
   v.z >= 0, sonst 1. zonzon1.c:515 bhEne_DGDirCheck setzt ay aus dem Werk
   und den Vektor aus dvx/dvy/dvz; der Aufrufer hier uebergibt beide direkt. */
static int32_t en16_dir_check(int32_t ay, const RECVX_ENEMY_VEC* vec)
{
    NJS_VECTOR in, v;
    in.x = vec->x; in.y = vec->y; in.z = vec->z;
    njUnitMatrix(NULL);
    njRotateY(NULL, -ay);
    njCalcVector(NULL, &in, &v);
    return (v.z >= 0.0f) ? 0 : 1;
}

/* Sicht auf den vorhandenen Spielerbesitzer (enemy_work.h:463). */
static int en16_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->player_get && sv->player_get(out)) return 1;
    if (sv && sv->gap) sv->gap("player_get (Spielerbesitzer)");
    return 0;
}

/* EXP0_I(0) ist &eff[slot]; nativ steht dort slot + 1, 0 bleibt NULL. */
static int en16_eff_read(RECVX_ENEMY_WORK* w, RECVX_EN16_EFF* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const RECVX_EN16_EXP0* e = (const RECVX_EN16_EXP0*)w->exp0;
    if (!e || e->slot == 0) return 0;
    if (en16_extra && en16_extra->effect_get && en16_extra->effect_get(e->slot - 1, out)) return 1;
    if (sv && sv->gap)
        sv->gap("Effektfeld-Sicht eff[mode0,ct0,gpx,lox,exp0] (effsub2.c:2390)");
    return 0;
}

/* ((char*)EXP0_I(0))[0xC] = mode0 (en16.c:46/140/149/174/315). */
static void en16_eff_mode(RECVX_ENEMY_WORK* w, int32_t mode0)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const RECVX_EN16_EXP0* e = (const RECVX_EN16_EXP0*)w->exp0;
    if (e && e->slot != 0 && en16_extra && en16_extra->effect_mode) {
        en16_extra->effect_mode(e->slot - 1, mode0);
        return;
    }
    if (sv && sv->gap) sv->gap("eff[slot].mode0 (O_WRK+0xC)");
}

/* zonzon1.c:846 bhEne_CallEffectSE(pos, se) = RequestEnemySe(127, pos, se). */
static void en16_call_effect_se(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->enemy_se) sv->enemy_se(127u, pos, se);
    else if (sv && sv->gap) sv->gap("bhEne_CallEffectSE(RequestEnemySe(127))");
}

/* zonzon1.c:828 bhEne_CallSE(epw, pos, se): Gate !(flg & 0x10000), dann
   RequestEnemySe(sys->enow, pos, se); sys->enow ist enemy_now
   (enemy_work.h:406). */
static void en16_call_se(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (w->flg & 0x10000u) return;
    if (sv && sv->enemy_se && sv->enemy_now) sv->enemy_se(sv->enemy_now(), pos, se);
    else if (sv && sv->gap) sv->gap("bhEne_CallSE(RequestEnemySe(sys->enow))");
}

static RECVX_EN16_EXP0* en16_exp0(RECVX_ENEMY_WORK* w)
{
    return (RECVX_EN16_EXP0*)w->exp0;
}

/* ---------------------------------------------------------------------------
   Tabellen und Vorwaertsdeklarationen (en16.c:15/25/30)
   --------------------------------------------------------------------------- */
static void en16_init(RECVX_ENEMY_WORK* w);
static void en16_move(RECVX_ENEMY_WORK* w);
static void en16_event(RECVX_ENEMY_WORK* w);
static void en16_br00(RECVX_ENEMY_WORK* w);
static void en16_mv00(RECVX_ENEMY_WORK* w);
static void en16_mv01(RECVX_ENEMY_WORK* w);
static void en16_mv02(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN16_PROC)(RECVX_ENEMY_WORK*);

/* en16.c:15 bhEne16_Mode0[6]; Index 2, 3 und 4 sind im Original NULL. */
static const RECVX_EN16_PROC en16_mode0[6] = {
    en16_init, en16_move, 0, 0, 0, en16_event
};
/* en16.c:25 bhEne16_BrainType[1]. */
static const RECVX_EN16_PROC en16_brain_type[1] = { en16_br00 };
/* en16.c:30 bhEne16_MoveMode2[3]. */
static const RECVX_EN16_PROC en16_move_mode2[3] = { en16_mv00, en16_mv01, en16_mv02 };

/* en16.c:38 bhEne16. */
void recvx_en16(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_EN16_EXP0* e;
    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u && en16_mode0[w->mode0]) {
        en16_mode0[w->mode0](w);
    } else if (sv && sv->gap) {
        sv->gap("bhEne16_Mode0");
    }

    /* en16.c:41-48: nur type 0 laeuft mit Motion; danach bekommt der Effekt
       Modus 2. Der vierte Originalparameter mtn_tp von bhSetMotion hat im
       Vertrag keinen Platz (enemy_work.h:375); en16 setzt ihn nie, er ist 0. */
    if (w->type == 0u) {
        if (sv && sv->set_motion) sv->set_motion(w, (int)w->mtn_add, w->mtn_md);
        else if (sv && sv->gap)   sv->gap("bhSetMotion");
        e = en16_exp0(w);
        if (e && e->slot != 0) en16_eff_mode(w, 2);
    }

    if (sv && sv->calc_model) sv->calc_model(w);
    else if (sv && sv->gap)   sv->gap("bhCalcModel");
}

/* en16.c:53 bhEne16_Init. */
static void en16_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    int eff_id = -1;

    if (w->type == 0u) {
        w->mode0 = 1; w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
    } else {
        w->mode0 = 1; w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;
    }

    if (w->exp0 == NULL) {
        /* en16.c:74 bhEne_CallocWork(104, 8). Der Vertragsdienst
           (enemy_work.h:537) nimmt keine Ausrichtung; kein statischer
           Ersatzblock, NULL wird als Original-Fehlschlag behandelt. */
        if (sv && sv->calloc_work) w->exp0 = sv->calloc_work(RECVX_EN16_EXP0_SIZE);
        else if (sv && sv->gap)    sv->gap("bhEne_CallocWork(104, 8)");

        if (w->exp0 == NULL) {
            if (sv && sv->gap) sv->gap("bhEne_CallocWork(104, 8) lieferte NULL");
        } else {
            RECVX_EN16_EXP0* e = en16_exp0(w);
            e->slot = 0;
            e->pts_num = 0;

            /* en16.c:75-87: EF-Vorlage {flg 1, id 270, type 0, mdlver 0,
               px/py/pz 0, sx/sy/sz 0, ay 0} und
               bhSetEffectTb(&sys->ef, NULL, (unsigned char*)epw, 0). */
            if (en16_extra && en16_extra->effect_tb) eff_id = en16_extra->effect_tb(w);
            else {
                if (sv && sv->gap)
                    sv->gap("bhSetEffectTb(EF{flg=1,id=270,type=0,scale=0,ay=0})");
                eff_id = -1;
            }

            if (eff_id >= 0) {
                e->slot = eff_id + 1;
                en16_eff_mode(w, 0);   /* en16.c:90 */
                /* en16.c:91 bhEff_E16_LaserSight((O_WRK*)EXP0_I(0))
                   (effsub2.c:2353; case 0 setzt func + mode0++). */
                if (en16_extra && en16_extra->effect_step) en16_extra->effect_step(eff_id);
                else if (sv && sv->gap) sv->gap("bhEff_E16_LaserSight(eff)");
            }
        }
    }

    w->mdflg |= 1u;        /* en16.c:94 */
    w->flg |= 0x8000u;     /* en16.c:95 */
    w->ct0 = 45;           /* en16.c:96 */
}

/* en16.c:100 bhEne16_Brain — der Tabelleneintrag 0 ist bhEne16_BR00. */
static void en16_brain(RECVX_ENEMY_WORK* w)
{
    en16_brain_type[0](w);
}

/* en16.c:106 bhEne16_BR00. */
static void en16_br00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER ply;
    int hit = 0;
    int32_t prm0 = 0;

    if (!en16_player(w, &ply)) return;   /* plp->flr_no/px/pz unbekannt */

    /* hitchk.c:5474 bhCheckFloorEnemy(plp->flr_no, plp->px, plp->pz): erste
       Bodenflaeche (rom->flrp + sys->mflrp) mit flg&1, type 2, px..px+w /
       pz..pz+d und passendem flr_no; gelesen wird nur prm0. */
    if (en16_extra && en16_extra->floor_enemy)
        hit = en16_extra->floor_enemy(ply.flr_no, ply.pos.x, ply.pos.z, &prm0);
    else if (sv && sv->gap)
        sv->gap("bhCheckFloorEnemy(plp->flr_no,plp->px,plp->pz) (Ergebnis "
                "unbekannt, Zweig wie NULL)");

    if (hit != 0 && prm0 == 16) {          /* en16.c:112-117 */
        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;
    } else {                               /* en16.c:118-122 */
        w->mode2 = 0;
    }
}

/* en16.c:126 bhEne16_Move. */
static void en16_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1u) {                  /* en16.c:128-131 */
        en16_brain(w);
    }
    if (w->mode2 < 3u) en16_move_mode2[w->mode2](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne16_MoveMode2");
}

/* en16.c:136 bhEne16_MV00. */
static void en16_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN16_EXP0* e = en16_exp0(w);
    if (e && e->slot != 0) en16_eff_mode(w, 1);
}

/* en16.c:145 bhEne16_MV01. */
static void en16_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN16_EXP0* e = en16_exp0(w);
    if (e && e->slot != 0) en16_eff_mode(w, 2);
}

/* en16.c:154 bhEne16_MV02. */
static void en16_mv02(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN16_EXP0* e = en16_exp0(w);
    float mtx[16];
    float dx, dy, dz, spd, mov;
    NJS_VECTOR v;
    RECVX_ENEMY_VEC pos;
    int32_t ax, ay;

    switch (w->mode3) {
    case 0:                                            /* en16.c:171-188 */
        if (e && e->slot != 0) en16_eff_mode(w, 2);    /* :172-175 */
        w->ct0 = 30;
        w->ct1 = 7;
        w->flg |= 0x80000u;
        /* :179-182: Ziel 20 Einheiten vor Spielergelenk 3. */
        if (sv && sv->player_joint_matrix && sv->player_joint_matrix(3u, mtx)) {
            w->xn = mtx[12] + 20.0f * mtx[8];
            w->yn = mtx[13] + 20.0f * mtx[9];
            w->zn = mtx[14] + 20.0f * mtx[10];
        } else if (sv && sv->gap) {
            sv->gap("bhEne16_MV02:plp->mlwP->owP[3].mtx");
        }
        w->aox = 0.0f;
        w->aoy = 0.0f;
        w->aoz = 0.0f;
        if (e) e->pts_num = 0;                         /* :186 EXP0_I(0x64) */
        else if (sv && sv->gap) sv->gap("EXP0-Block (en16.c:186)");
        w->mode3++;
        break;

    case 1:                                            /* :189-194, Postdekrement */
        if (w->ct0-- == 0) w->mode3++;
        break;

    case 2:                                            /* :195-204 */
        v.x = w->aox; v.y = w->aoy; v.z = w->aoz;
        if (njScalor(&v) < 0.5f) {
            if (e && e->slot != 0) {                   /* :198 */
                RECVX_EN16_EFF eff;
                if (en16_eff_read(w, &eff) && eff.ct0 > 0) {
                    w->ct0 = 30;
                    w->mode3++;
                }
            }
        }
        break;

    case 3:                                            /* :205-210, Postdekrement */
        if (w->ct0-- == 0) w->mode3++;
        break;

    case 4:                                            /* :211-278 */
        if (w->ct1 != 0) {
            w->ct1--;
            pos.x = w->px; pos.y = w->py; pos.z = w->pz;
            en16_call_effect_se(w, &pos, 74497);       /* :215 */

            /* :217-267: Spielertreffer, sonst Wandtreffer. */
            if (e && e->slot != 0) {
                RECVX_EN16_EFF eff;
                if (en16_eff_read(w, &eff)) {
                    RECVX_ENEMY_PLAYER ply;
                    int player_ok = en16_player(w, &ply);
                    if (eff.ct0 > 0 && player_ok && !(ply.stflg & 0x30u)) {
                        RECVX_ENEMY_VEC dir;
                        /* :219-221: dv = Zielpunkt - Position. Der
                           Originalvektor liegt in plp->dvx/dvy/dvz und wird
                           von bhEne_DGDirCheck(plp) (zonzon1.c:515) wieder
                           gelesen; RECVX_ENEMY_PLAYER hat kein dv, der
                           Rueckstossvektor wird darum gemeldet und die
                           Richtungspruefung mit demselben Vektor gerechnet. */
                        dir.x = w->xn - w->px;
                        dir.y = w->yn - w->py;
                        dir.z = w->zn - w->pz;
                        /* :219-223: plp->dv = Zielpunkt - Position, danach
                           bhEne_SetBloodEffectBurst(plp, 2, eff->ct0, &eff->gpx, 1)
                           (zonzon1.c:243) am Spielerbesitzer. */
                        {
                            int djnt = 0; RECVX_ENEMY_VEC dv0, dp;
                            const RECVX_T3_LINK plink = { NULL, 1 };
                            if (recvx_t3_player_hit_get(&djnt, &dv0, &dp)) {
                                recvx_t3_player_hit_set(djnt, &dir, &dp);
                                recvx_t3_set_blood_effect_burst(plink, 2, eff.ct0, &eff.gpx, 1);
                            }
                        }
                        /* :224-236 */
                        if (150 < ply.hp)      ply.hp -= ply.hp / 2;
                        else if (50 < ply.hp)  ply.hp -= ply.hp / 3;
                        else                   ply.hp = -1;
                        /* :237-248 */
                        if (ply.hp >= 0) {
                            ply.flg |= 4u;
                            ply.mode0 = 2;
                            ply.mode1 = (uint8_t)en16_dir_check(ply.ay, &dir);
                            ply.mode2 = 1;
                            ply.mode3 = 0;
                        } else {
                            w->ct1 = 0;
                        }
                        if (sv && sv->player_set) sv->player_set(&ply);
                        else if (sv && sv->gap) sv->gap("player_set (Spielerbesitzer)");
                    } else if (eff.atr != NULL) {
                        /* :252-266 Wandtreffer: hp = eff->exp0 (Disassembly
                           0x1EA388 lw $s0, 0x2F0($v1)), bei bhCheckWallAttrB89(hp)
                           v = Position - Zielpunkt, pos = eff->lox/loy/loz und
                           Funken 308. */
                        int32_t prm0 = 0;
                        if (en16_extra && en16_extra->wall_attr_b89 &&
                            en16_extra->wall_attr_b89(eff.atr, &prm0)) {
                            RECVX_ENEMY_VEC v, at;
                            v.x = w->px - w->xn; v.y = w->py - w->yn; v.z = w->pz - w->zn;
                            at = eff.lox;
                            /* en16.c:262 bhSetEffGunSpark(&pos, &v, 0xFFF8F8A0, 0x4CE04010,
                               WpnTab[13].hiteff - 4); ELF 0x3560D6 (WpnTab 0x355B30
                               + 13*0x68 + 0x5E) = 5, also Eff308Prm[1]. */
                            if (en16_extra->gun_spark)
                                en16_extra->gun_spark(&at, &v, 0xFFF8F8A0u, 0x4CE04010u, en16_extra->hiteff13 - 4);
                            else if (sv && sv->gap) sv->gap("bhSetEffGunSpark (en16.c:262)");
                            /* en16.c:263-265 gap.wpn_no = 0xD; gap.ay = bhArcTan2(v.x, v.z);
                               bhSetGunSplash(&gap, &pos, hp->prm0). bhSetGunSplash
                               (weapon.c:1636-1640, Disassembly 0x28DA58-0x28DA80) liest
                               gap->vx/vy/vz (GA_WORK +0xC/+0x10/+0x14) als Spritzrichtung
                               fuer lox (Faelle 0/1/2/3) bzw. px/pz (Faelle 4/5). bhEne16_MV02
                               legt gap bei sp+0x50 an und schreibt nur wpn_no (sp+0x78) und
                               ay (sp+0x90); sp+0x5C..0x64 beschreibt die Funktion nirgends
                               (alle Stackstores: 0x0..0x8 Register, 0x78, 0x90, 0xA0..0xB8
                               fuer v und pos). Die Richtung ist im Original also
                               uninitialisierter Stackinhalt, nicht v. Ohne belegbaren Wert
                               wird kein Spritzer erzeugt; benannte Luecke statt Ersatz. */
                            if (sv && sv->gap)
                                sv->gap("bhSetGunSplash (en16.c:265): GA_WORK vx/vy/vz im Original uninitialisiert");
                        } else if (!en16_extra || !en16_extra->wall_attr_b89) {
                            if (sv && sv->gap) sv->gap("bhCheckWallAttrB89 (en16.c:253)");
                        }
                    }
                }
            }

            /* :268-276 */
            if (sv && sv->joint_matrix && sv->joint_matrix(w, 0u, mtx)) {
                w->aox += 1.5f * mtx[4];
                w->aoy += 1.5f * mtx[5];
                w->aoz += 1.5f * mtx[6];
            } else if (sv && sv->gap) {
                sv->gap("bhEne16_MV02:epw->mlwP->owP->mtx[4..6]");
            }
            w->dpx = w->xn;
            w->dpy = w->yn;
            w->dpz = w->zn;
            w->flg &= ~0x80000u;
            w->mode3++;
            w->ct0 = 0;
        }
        break;

    case 5:                                            /* :279-321 */
        w->ct0++;
        if (w->ct0 < 3) {
            w->aox *= 0.7f;
            w->aoy *= 0.7f;
            w->aoz *= 0.7f;
        } else if (w->ct0 == 3) {
            if (sv && sv->joint_matrix && sv->joint_matrix(w, 0u, mtx)) {
                w->xn += 15.0f * mtx[4];
                w->yn += 15.0f * mtx[5];
                w->zn += 15.0f * mtx[6];
            } else if (sv && sv->gap) {
                sv->gap("bhEne16_MV02:epw->mlwP->owP->mtx[4..6]");
            }
        } else if (w->ct0 < 30) {
            w->aox += (w->dpx - w->xn) / 32.0f;
            w->aoy += (w->dpy - w->yn) / 32.0f;
            w->aoz += (w->dpz - w->zn) / 32.0f;
            v.x = w->aox; v.y = w->aoy; v.z = w->aoz;
            spd = njScalor(&v);
            mov = spd + (0.1f * (0.6f - spd));
            njUnitVector(&v);
            w->aox = v.x * mov;
            w->aoy = v.y * mov;
            w->aoz = v.z * mov;
        } else if (w->ct1 != 0) {
            w->flg |= 0x80000u;
            w->mode3 = 2;
        } else {
            if (e && e->slot != 0) en16_eff_mode(w, 1);    /* :313-316 */
            w->mode3 = 6;
            w->ct0 = 0x5A;
            pos.x = w->px; pos.y = w->py; pos.z = w->pz;
            en16_call_se(w, &pos, 74498);              /* :319 */
        }
        break;

    case 6:                                            /* :322-328, Postdekrement */
        if (w->ct0-- == 0) {
            w->mode1 = 1;
            w->mode2 = 0;
        }
        break;

    default:
        if (sv && sv->gap) sv->gap("bhEne16_MV02:mode3");
        break;
    }

    /* :330-371: Solange flg 0x80000 steht, folgt der Koerper dem Ziel. */
    if (w->flg & 0x80000u) {
        if (sv && sv->player_joint_matrix && sv->player_joint_matrix(3u, mtx)) {
            w->aox += (mtx[12] - w->xn) / 32.0f;
            w->aoy += ((1.5f + mtx[13]) - w->yn) / 32.0f;
            w->aoz += (mtx[14] - w->zn) / 32.0f;

            dx = mtx[12] - w->xn;
            dz = mtx[14] - w->zn;
            if (njSqrt(dx * dx + dz * dz) < 30.0f) {   /* :340-363 */
                RECVX_ENEMY_PLAYER ply;
                if (en16_player(w, &ply)) {
                    if (ply.hp < 30)       spd = 0.6f;
                    else if (ply.hp < 120) spd = 0.65f;
                    else                   spd = 0.7f;
                    v.x = w->aox; v.y = w->aoy; v.z = w->aoz;
                    mov = njScalor(&v);
                    if (mov > spd) {
                        mov = mov + (0.1f * (spd - mov));
                        njUnitVector(&v);
                        w->aox = v.x * mov;
                        w->aoy = v.y * mov;
                        w->aoz = v.z * mov;
                    }
                }
            }
        } else if (sv && sv->gap) {
            sv->gap("bhEne16_MV02:plp->mlwP->owP[3].mtx");
        }

        if (e && e->slot != 0) {                       /* :365-370 */
            RECVX_EN16_EFF eff;
            if (en16_eff_read(w, &eff) && eff.ct0 > 0) {
                w->aox *= 0.7f;
                w->aoy *= 0.7f;
                w->aoz *= 0.7f;
            }
        }
    }

    /* :372-374: xn/yn/zn += aox + 0.3*njRandom - 0.15 (drei getrennte
       Aufrufe, siehe en16_rand01). */
    w->xn += (w->aox + (0.3f * en16_rand01(w))) - 0.15f;
    w->yn += (w->aoy + (0.3f * en16_rand01(w))) - 0.15f;
    w->zn += (w->aoz + (0.3f * en16_rand01(w))) - 0.15f;

    /* :376-383: Der Modellwurzelknoten zeigt auf das Ziel. Nativ traegt der
       Besitzer die Wurzelwinkel in ax/ay; objP->ang[0]/[1] hat dort keine
       eigene Flaeche, der Zeichenpfad muss ax/ay als Wurzelwinkel anwenden. */
    dx = w->xn - w->px;
    dy = w->yn - w->py;
    dz = w->zn - w->pz;
    ay = en16_arc_tan2(-dx, -dz);                          /* objP->ang[1] */
    ax = en16_arc_tan2(dy, njSqrt((dx * dx) + (dz * dz))); /* objP->ang[0] */
    w->ay = ay;
    w->ax = ax;
}

/* en16.c:15 Mode0-Index 5 — bhEne_Event (subpl.c:143) = subpl_mv01_tbl[ct2].
   Diesen Zustand besitzt der Begleitfiguren-Mechanismus; der Vertragsdienst
   `event` (enemy_work.h:489-495) wird gerufen, sobald er gebunden ist. */
static void en16_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    if (w->sv && w->sv->gap) w->sv->gap("bhEne_Event(subpl_mv01_tbl)");
}

/* ---------------------------------------------------------------------------
   en16.c:387/393/399 — Effektpunkte des Laserstrahls. Aufrufer ist die
   Zeichenroutine des Effekts (effsub2.c:2390) ueber op->lkwkp.
   --------------------------------------------------------------------------- */
int recvx_en16_eff_num(const RECVX_ENEMY_WORK* w)
{
    const RECVX_EN16_EXP0* e;
    if (!w) return 0;
    e = (const RECVX_EN16_EXP0*)w->exp0;
    if (!e) return 0;                  /* Original liest EXP0_I(0x64) blind */
    return e->pts_num;
}

const RECVX_ENEMY_VEC* recvx_en16_eff_pos(const RECVX_ENEMY_WORK* w)
{
    const RECVX_EN16_EXP0* e;
    if (!w) return NULL;
    e = (const RECVX_EN16_EXP0*)w->exp0;
    if (!e) return NULL;
    return &e->pts[0];                 /* en16.c:395 &EXP0_P(0) */
}

void recvx_en16_add_eff_pos(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos)
{
    RECVX_EN16_EXP0* e;
    int32_t i;
    if (!w) return;
    e = (RECVX_EN16_EXP0*)w->exp0;
    if (!e) {
        if (w->sv && w->sv->gap) w->sv->gap("EXP0-Block (en16.c:399)");
        return;
    }
    if (!pos) {
        /* en16.c:408 kopiert ohne Pruefung; der Original-Aufrufer uebergibt
           immer einen Punkt. NULL meldet den Aufruferfehler, statt fremden
           Speicher zu lesen. */
        if (w->sv && w->sv->gap) w->sv->gap("bhEne16_AddEffPos(NULL)");
        return;
    }

    for (i = e->pts_num; i > 0; i--) { /* en16.c:403-406 */
        e->pts[i] = e->pts[i - 1];
    }
    e->pts[0] = *pos;                  /* en16.c:408 */

    if (e->pts_num < 7) {              /* en16.c:410-413 */
        e->pts_num++;
    }
}
