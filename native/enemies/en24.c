/* Identitaet 24 — Baby Black Widow. Nativ eigenstaendig bestimmt; kein
   Originalkoerper, kein Generator, keine zweite Workwelt.
   Fundstellen und Eigenheiten stehen in en24.h. */
#include "en24.h"
#include "en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
   EXP0-Block (72 Byte, en24.c:135 bhEne_CallocWork(72, 8)).
   Die Offsets sind die der Originalmakros EXP0_F/EXP0_I/EXP0_UC (macros.h:37/
   40/41) sowie EXP0_P3_X/Y/Z (macros.h:73-75, Schrittweite 12 ab 0x28) und
   EXP0_ANG (macros.h:76, Schrittweite 4 ab 0x40).
   --------------------------------------------------------------------------- */
typedef struct {
    /* +0x00 EXP0_F(0): Abstand zum Spieler (en24.c:163). Init schreibt den
       Wert als int-Bitmuster 1148829696 = 0x44800000 = 1024.0f
       (en24.c:138) — darum die Unionsicht, damit dasselbe Bitmuster
       entsteht und kein Wert geraten wird. */
    union { float f; uint32_t u; } dist;
    /* +0x04..+0x10 EXP0_I(0x4/0x8/0xC/0x10): Init nullt sie (en24.c:139-142);
       keine andere Stelle in en24.c liest oder schreibt sie. */
    int32_t unused_04, unused_08, unused_0c, unused_10;
    /* +0x14/+0x18/+0x1C EXP0_F(0x14..0x1C): Wurfgeschwindigkeit von MV04
       (en24.c:403-405). MV04 uebergibt &EXP0_UC(0x14) als Vektorzeiger
       (en24.c:438), also genau diese drei Floats. */
    float vx, vy, vz;
    /* +0x20 EXP0_UC(0x20): Sichtkontakt aus bhSearchPlayer (en24.c:165-172). */
    uint8_t sight;
    uint8_t pad0[3];          /* +0x21..+0x23 im Original unbelegt            */
    /* +0x24 EXP0_I(0x24): Wartezaehler vor dem naechsten Sprung
       (en24.c:151/179-182/199). */
    int32_t brain_wait;
    /* +0x28/+0x34 EXP0_P3(0..1): letzte Weltlage der beiden Spielergelenke
       aus bhEne24_DeadCheck (en24.c:553-569). */
    RECVX_ENEMY_VEC p3[2];
    /* +0x40/+0x44 EXP0_ANG(0..1): letzter Gelenkwinkel derselben zwei
       Gelenke (en24.c:561/571). */
    int32_t ang[2];
} RECVX_EN24_EXP0;

_Static_assert(sizeof(RECVX_EN24_EXP0) == 0x48u, "EXP0-Block ist 72 Byte (en24.c:135)");
_Static_assert(offsetof(RECVX_EN24_EXP0, vx)         == 0x14u, "EXP0_F(0x14)");
_Static_assert(offsetof(RECVX_EN24_EXP0, sight)      == 0x20u, "EXP0_UC(0x20)");
_Static_assert(offsetof(RECVX_EN24_EXP0, brain_wait) == 0x24u, "EXP0_I(0x24)");
_Static_assert(offsetof(RECVX_EN24_EXP0, p3)         == 0x28u, "EXP0_P3_X(0)");
_Static_assert(offsetof(RECVX_EN24_EXP0, ang)        == 0x40u, "EXP0_ANG(0)");

/* en24.c:15 DeadRate[19] — Wahrscheinlichkeit, dass ein Treffer mit der
   Waffennummer `wpnr_no` sofort toetet. */
static const float en24_dead_rate[19] = {
    0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    0.3f, 1.0f, 0.0f, 0.3f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f
};

/* en24.c:38 CapColTab[3] (CPCL). */
static const RECVX_ENEMY_CPCL en24_cap_col_tab[3] = {
    { 0, 0, 5 },
    { 0, 2, 0 },
    { 0, 0, 0 }
};

static void en24_init(RECVX_ENEMY_WORK* w);
static void en24_move(RECVX_ENEMY_WORK* w);
static void en24_nage(RECVX_ENEMY_WORK* w);
static void en24_damage(RECVX_ENEMY_WORK* w);
static void en24_die(RECVX_ENEMY_WORK* w);
static void en24_event(RECVX_ENEMY_WORK* w);
static void en24_dummy(RECVX_ENEMY_WORK* w);
static void en24_brain(RECVX_ENEMY_WORK* w);
static void en24_br00(RECVX_ENEMY_WORK* w);
static void en24_mv00(RECVX_ENEMY_WORK* w);
static void en24_mv01(RECVX_ENEMY_WORK* w);
static void en24_mv02(RECVX_ENEMY_WORK* w);
static void en24_mv03(RECVX_ENEMY_WORK* w);
static void en24_mv04(RECVX_ENEMY_WORK* w);
static void en24_mv05(RECVX_ENEMY_WORK* w);
static void en24_collision_walls(RECVX_ENEMY_WORK* w);
static void en24_collision_line(RECVX_ENEMY_WORK* w);
static int  en24_dead_check(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN24_PROC)(RECVX_ENEMY_WORK*);

/* en24.c:49 bhEne24_Mode0[7]. Index 5 ist bhEne_Event (subpl.c:143),
   Index 6 bhEne24_Dummy (en24.c:503, leer). */
static const RECVX_EN24_PROC en24_mode0[7] = {
    en24_init, en24_move, en24_nage, en24_damage, en24_die,
    en24_event, en24_dummy
};

/* en24.c:60 bhEne24_BrainType[1]. */
static const RECVX_EN24_PROC en24_brain_type[1] = { en24_br00 };

/* en24.c:65 bhEne24_MoveMode2[6]. */
static const RECVX_EN24_PROC en24_move_mode2[6] = {
    en24_mv00, en24_mv01, en24_mv02, en24_mv03, en24_mv04, en24_mv05
};

static void en24_gap(RECVX_ENEMY_WORK* w, const char* what)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(what);
}

/* macros.h:93/105: njRandom() = (Float)((Float)-rand()/(Float)(RAND_MAX+1)),
   RAND_MAX 0x7fffffff, (RAND_MAX+1) laeuft als int auf INT_MIN ueber. Beide
   Vorzeichen heben sich auf, der Wert liegt in [0,1]. */
static float en24_rand01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->random_next) return (float)sv->random_next() / 2147483648.0f;
    en24_gap(w, "rand");
    return 0.0f;   /* kein erfundener Zufallswert */
}

/* pwksub.c:133 bhAddSpeed(pp, r). */
static void en24_add_speed(RECVX_ENEMY_WORK* w, int32_t r)
{
    const int32_t angle = (w->ay + r) & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* zonzon1.c:864 bhArcTan2(y, x) — asinf-Form mit (short)-Umlauf im x<0-Zweig;
   10430.381 = 65536/(2*PI). */
static int32_t en24_arc_tan2(float y, float x)
{
    float len;
    int32_t ang;

    len = njSqrt((x * x) + (y * y));
    if (len == 0.0f) return 0;

    ang = (int32_t)(10430.381f * asinf(y / len));
    if (x < 0.0f) ang = (int16_t)(32768 - ang);
    return ang;
}

/* zonzon1.c:433 bhEne_DirTarget(epw, x, z, w). */
static int32_t en24_dir_target(RECVX_ENEMY_WORK* w, float x, float z, int32_t limit)
{
    int32_t ang;

    ang = en24_arc_tan2(w->px - x, w->pz - z);
    ang = (int16_t)(ang - w->ay);

    if (limit < abs(ang)) {
        ang = (ang > 0) ? limit : -limit;
    }
    return ang;
}

/* ps2_NaColi.c:65 njDistanceP2P. */
static float en24_distance_p2p(const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b)
{
    const float dx = b->x - a->x;
    const float dy = b->y - a->y;
    const float dz = b->z - a->z;
    return njSqrt((dx * dx) + (dy * dy) + (dz * dz));
}

/* zonzon1.c:828 bhEne_CallSE(epw, pos, se) mit dem Gate !(flg & 0x10000). */
static void en24_call_se(RECVX_ENEMY_WORK* w, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos;
    if (w->flg & 0x10000u) return;
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    if (sv && sv->enemy_se && sv->enemy_now) sv->enemy_se(sv->enemy_now(), &pos, se);
    else en24_gap(w, "bhEne_CallSE(RequestEnemySe(sys->enow))");
}

static int en24_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->player_get && sv->player_get(out)) return 1;
    en24_gap(w, "plp (player_get)");
    return 0;
}

/* ---------------------------------------------------------------------------
   pwksub.c:664 bhSearchPlayer(pp, r) — Sichtkegel des Works gegen den Spieler.
   Die Matrixfolge des Originals ist KUMULATIV: nach njRotateY(NULL, pp->ay)
   wird njRotateY(NULL, r) auf dieselbe Matrix gelegt, vc2 ist also die um
   ay+r gedrehte Blickrichtung (pwksub.c:679-685). Nativ steht dafuer eine
   lokale NJS_MATRIX statt der globalen Ninja-Matrix; die Rechnung ist
   dieselbe. Rueckgabe: -1 kein Sichtkontakt, sonst der Zielwinkel (mit der
   originalen -1 -> -2 Korrektur, pwksub.c:704-707).
   --------------------------------------------------------------------------- */
static int32_t en24_search_player(RECVX_ENEMY_WORK* w, int32_t r)
{
    RECVX_ENEMY_PLAYER plp;
    NJS_MATRIX m;
    NJS_VECTOR vc0, vc1, vc2;
    float a, er;
    int32_t yr;

    if (!en24_player(w, &plp)) return -1;

    if (r == 0) r = 32768;

    vc1.x = 0.0f;
    vc1.y = 0.0f;
    vc1.z = -1.0f;

    njUnitMatrix(&m);

    njRotateY(&m, w->ay);
    njCalcVector(&m, &vc1, &vc0);

    njRotateY(&m, r);
    njCalcVector(&m, &vc1, &vc2);

    er = -njInnerProduct(&vc0, &vc2);

    vc1.x = w->px - plp.pos.x;
    vc1.y = w->py - plp.pos.y;
    vc1.z = w->pz - plp.pos.z;

    njUnitVector(&vc1);

    a = njInnerProduct(&vc0, &vc1);

    if (a < er) {
        vc0.x = plp.pos.x - w->px;
        vc0.z = plp.pos.z - w->pz;

        yr = (int32_t)(10430.381f * atan2f(-vc0.x, -vc0.z));

        if (yr == -1) {
            yr = -2;
        }
    } else {
        yr = -1;
    }

    return yr;
}

static const RECVX_EN24_SERVICES* en24_sv;
void recvx_en24_bind(const RECVX_EN24_SERVICES* services) { en24_sv = services; }

/* ---------------------------------------------------------------------------
   zonzon1.c:611 bhEne_AttackHitCheck(plp, &epw->px, 2.0f) (en24.c:422): dieselbe
   portierte Routine wie bei EN02/EN03 (recvx_t3_attack_hit_check, en03_lib.c),
   die plp->watr, das naechste Spielergelenk und plp->djnt_no/dv/dp fuehrt.
   --------------------------------------------------------------------------- */
static int en24_attack_hit_check(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, float ar)
{
    (void)w;
    return recvx_t3_attack_hit_check(pos, ar);
}

/* en24.c:76 bhEne24. */
void recvx_en24(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_WORK* ep;

    if (!w) return;
    sv = w->sv;

    /* en24.c:80: mode0 == 6 (Dummy) laesst den GANZEN Pass aus — nicht nur
       die Mode0-Routine. bhEne24_Init stellt genau diesen Wert ein. */
    if (w->mode0 != 6u) {
        if (w->mode0 < 7u) {
            en24_mode0[w->mode0](w);
        } else {
            en24_gap(w, "bhEne24_Mode0 (mode0 ausserhalb 0..6)");
        }

        en24_collision_line(w);

        if (w->flg & 0x10u) {
            en24_collision_walls(w);
        }

        if (sv && sv->calc_model) sv->calc_model(w);
        else en24_gap(w, "bhCalcModel");

        /* en24.c:90-98: Waffenkapsel des Works, jeden Pass neu. */
        w->watr.c1.x = w->px;
        w->watr.c1.y = w->py;
        w->watr.c1.z = w->pz;

        w->watr.c2.x = w->px;
        w->watr.c2.y = w->py;
        w->watr.c2.z = 1.0f + w->pz;

        w->watr.r = 2.0f;

        /* en24.c:100-109: haengt der Work an einem anderen (lkwkp) und ist
           flg 0x80000 gesetzt, dann waehlt frm_no als schlichter Zaehler 0..3
           das Modell mdl[frm_no + 2] DES VERKNUEPFTEN Works. Diese Identitaet
           ruft kein bhSetMotion, frm_no ist hier also kein 16.16-Frame. */
        ep = w->lkwkp;
        if (ep != NULL && (w->flg & 0x80000u)) {
            if (3 < w->frm_no) {
                w->frm_no = 0;
            }
            /* en24.c:107 `epw->mlwP = &ep->mdl[frm_no + 2]`: der Modellsatz
               gehoert dem VERKNUEPFTEN Work. */
            if (!sv || !sv->link_model || !sv->link_model(w, ep, (uint32_t)(w->frm_no + 2)))
                en24_gap(w, "mlwP = &lkwkp->mdl[frm_no+2] (en24.c:107)");
            w->frm_no++;
        }
    }
}

/* en24.c:114 bhEne24_Init. */
static void en24_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN24_EXP0* e;

    w->flg |= 0x8010u;
    w->flg &= ~0x26u;

    w->mdflg |= 5u;

    w->ar = 1.2f;
    w->ah = 1.2f;

    w->car = 1.2f;
    w->cah = 1.2f;

    w->hp = 2;

    w->flg2 |= 0x10u;

    /* en24.c:131: ruhend, bis ein fremder Mechanismus mode0 umstellt. */
    w->mode0 = 6;

    if (w->exp0 == 0) {
        /* en24.c:135 bhEne_CallocWork(72, 8); der Vertragsdienst kennt nur
           die Groesse (enemy_work.h:565). */
        if (!sv || !sv->calloc_work) {
            en24_gap(w, "bhEne_CallocWork(72)");
            return;
        }
        w->exp0 = sv->calloc_work(0x48u);
        if (w->exp0 == 0) {
            en24_gap(w, "bhEne_CallocWork(72): Budget erschoepft");
            return;
        }
    }
    e = (RECVX_EN24_EXP0*)w->exp0;

    e->sight = 0;                 /* en24.c:137 EXP0_UC(0x20) = 0            */
    e->dist.u = 1148829696u;      /* en24.c:138 EXP0_I(0) — 0x44800000       */
    e->unused_04 = 0;             /* en24.c:139                              */
    e->unused_08 = 0;             /* en24.c:140                              */
    e->unused_0c = 0;             /* en24.c:141                              */
    e->unused_10 = 0;             /* en24.c:142                              */

    w->clp_jno[0] = 0;
    w->clp_jno[1] = -1;

    w->mdflg &= ~0x20u;

    w->cpcl = en24_cap_col_tab;

    /* en24.c:151. */
    e->brain_wait = (int32_t)(30.0f * en24_rand01(w)) + 30;
}

/* en24.c:155 bhEne24_Brain. */
static void en24_brain(RECVX_ENEMY_WORK* w)
{
    /* Original indexiert bhEne24_BrainType[epw->type] ohne Pruefung; die
       Tabelle hat genau einen Eintrag (en24.c:60). */
    if (w->type < 1u) {
        en24_brain_type[w->type](w);
    } else {
        en24_gap(w, "bhEne24_BrainType[type] (en24.c:157, type > 0)");
    }
}

/* en24.c:161 bhEne24_BR00. */
static void en24_br00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN24_EXP0* e = (RECVX_EN24_EXP0*)w->exp0;
    RECVX_ENEMY_PLAYER plp;
    RECVX_ENEMY_VEC self;

    if (!e) return;
    if (!en24_player(w, &plp)) return;

    self.x = w->px; self.y = w->py; self.z = w->pz;
    e->dist.f = en24_distance_p2p(&self, &plp.pos);   /* en24.c:163 */

    /* en24.c:165-172: 10922 = 60 Grad Halboeffnung des Sichtkegels. */
    if (en24_search_player(w, 10922) == -1) {
        e->sight = 0;
    } else {
        e->sight = 1;
    }

    if ((plp.stflg & 0x80000000u) || (plp.flg & 0x4u) || (w->flg & 0x4u)) {
        return;
    }

    if (e->brain_wait) {
        e->brain_wait--;
        return;
    }

    if (e->sight == 0u) {
        return;
    }

    if (e->dist.f >= 5.0f) {
        return;
    }

    w->mode1 = 0;
    w->mode2 = 4;
    w->mode3 = 0;

    e->brain_wait = (int32_t)(30.0f * en24_rand01(w)) + 30;
}

/* en24.c:203 bhEne24_Move. */
static void en24_move(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;

    if (w->mode1 == 1u) {
        en24_brain(w);
    }

    /* en24.c:212: der Spieler tritt die Spinne zu Tode. */
    if (en24_player(w, &plp)) {
        if ((plp.mode0 == 1u) && (w->flg & 0x20u) && en24_dead_check(w)) {
            w->flg |= 2u;
        }
    }

    /* en24.c:217-232: Treffer verarbeiten. */
    if (w->flg & 4u) {
        w->flg &= ~4u;
        if (w->hp > 0) {
            /* en24.c:222 njRandom() < DeadRate[epw->wpnr_no] ohne Bereichspruefung;
               bhCheckBombAtari (weapon.c:1453, Explosion bhEff016) setzt wpnr_no = 20.
               Im ELF liegt DeadRate bei 0x31F650 (76 Byte): Index 19 ist die Fuellung
               0x31F69C, Index 20 CapColTab[0] = {jnt_a 0, jnt_b 0} (0x31F6A0, nur ueber
               epw->cpcl gelesen, nirgends beschrieben). Beide Bitmuster sind 0 = 0.0f,
               der Wert ist also bestimmt und harmlos: kein Defekt nach QUALITY.md, treu
               portiert - njRandom() wird gezogen, der Vergleich ist nie wahr. */
            {
                float rate = 0.0f;
                if (w->wpnr_no >= 0 && w->wpnr_no < 19) rate = en24_dead_rate[w->wpnr_no];
                else if (w->wpnr_no != 19 && w->wpnr_no != 20)
                    en24_gap(w, "DeadRate[wpnr_no] (en24.c:222): wpnr_no ausserhalb 0..20");
                if (en24_rand01(w) < rate) {
                    w->flg |= 2u;
                }
            }
            w->hp -= 1;
        } else {
            w->flg |= 2u;
        }
    }

    if (w->flg & 2u) {
        RECVX_ENEMY_VEC pos;

        en24_call_se(w, 74497);          /* en24.c:236 */

        w->dvx = 0.0f;                   /* en24.c:238-240 */
        w->dvy = 1.0f;
        w->dvz = 0.0f;

        pos.x = w->px;                   /* en24.c:242-244 */
        pos.y = 1.0f + w->py;
        pos.z = w->pz;

        if (sv && sv->blood_effect4) {
            RECVX_ENEMY_VEC vec;
            vec.x = w->dvx; vec.y = w->dvy; vec.z = w->dvz;
            sv->blood_effect4(&pos, &vec, 1, 0, 0);
        } else {
            en24_gap(w, "bhEne_SetBloodEffect4 (en24.c:246)");
        }

        if (en24_sv && en24_sv->effect_tb) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz }, size = { 2.0f, 2.0f, 2.0f };
            (void)en24_sv->effect_tb(303u, 1u, 1u, &at, &size, 0, w->ay);   /* en24.c:249-263 */
        } else {
            en24_gap(w, "bhSetEffectTb(&sys->ef, 303) (en24.c:263)");
        }

        w->mdflg |= 1u;                  /* en24.c:265 */

        w->mode0 = 4;                    /* en24.c:267-269 */
        w->mode2 = 0;
        w->mode3 = 0;
    } else {
        if (w->mode2 < 6u) {
            en24_move_mode2[w->mode2](w);
        } else {
            en24_gap(w, "bhEne24_MoveMode2 (mode2 ausserhalb 0..5)");
        }
    }
}

/* en24.c:279 bhEne24_MV00 — leer, Signatur `void (void)`. */
static void en24_mv00(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en24.c:285 bhEne24_MV01 — kurze Pause, danach MV02 oder MV03.
   Beide Faelle laufen im Original ohne break durch. */
static void en24_mv01(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x80000u;
        w->flg |= 0x20u;
        w->ct0 = (int32_t)(20.0f * en24_rand01(w)) + 10;
        w->mode3++;
        /* Fallthrough */

    case 1:
        if (w->ct0-- == 0) {
            w->mode1 = 1;
            w->mode2 = (en24_rand01(w) > 0.5f) ? 2u : 3u;
            w->mode3 = 0;
        }
        /* Fallthrough (Original hat hier kein break; case 1 ist der letzte) */
        break;

    default:
        en24_gap(w, "bhEne24_MV01 (mode3 ausserhalb 0..1)");
        break;
    }
}

/* en24.c:307 bhEne24_MV02 — Verfolgen. Der ct2-Nachlauf (en24.c:361-366)
   steht AUSSERHALB des switch und laeuft in jedem mode3. */
static void en24_mv02(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN24_EXP0* e = (RECVX_EN24_EXP0*)w->exp0;

    if (!e) return;

    switch (w->mode3) {
    case 0:
        w->ct0 = (int32_t)(30.0f * en24_rand01(w)) + 60;

        /* en24.c:316: ct1 wird gesetzt, in en24.c aber nie gelesen. */
        w->ct1 = (en24_rand01(w) < 0.5f) ? -1 : 1;

        w->ct2 = (int32_t)(20.0f + (20.0f * en24_rand01(w)) - (20.0f * en24_rand01(w)));

        w->spd = 0.3f;

        w->flg |= 0x80000u;

        w->way = (int16_t)((en24_rand01(w) > 0.5f) ? 273 : -273);

        w->flg |= 0x20u;
        w->mode3++;
        /* Fallthrough */

    case 1: {
        RECVX_ENEMY_VEC pos;

        en24_add_speed(w, 0);

        if (e->sight == 1u) {
            RECVX_ENEMY_PLAYER plp;
            if (en24_player(w, &plp)) {
                w->ay += en24_dir_target(w, plp.pos.x, plp.pos.z, 455);
            }
        }

        pos.x = w->px - (5.0f * njSin(w->ay));
        pos.y = w->py;
        pos.z = w->pz - (5.0f * njCos(w->ay));

        if (sv && sv->wall_type) {
            if (sv->wall_type(&pos, 0u, 1.0f, 1.0f)) {
                w->ay += w->way;
            }
        } else {
            en24_gap(w, "bhCheckWallType (en24.c:341)");
        }

        if (w->ct0-- == 0) {
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
        }

        /* en24.c:353: der Boden liegt unter dem Work -> es faellt (MV04). */
        if (sv && sv->ground) {
            RECVX_ENEMY_VEC self;
            self.x = w->px; self.y = w->py; self.z = w->pz;
            if (w->py > sv->ground(&self)) {
                w->mode1 = 0;
                w->mode2 = 4;
                w->mode3 = 0;
            }
        } else {
            en24_gap(w, "bhGetGroundPosition (en24.c:353)");
        }
        break;
    }

    default:
        en24_gap(w, "bhEne24_MV02 (mode3 ausserhalb 0..1)");
        break;
    }

    /* en24.c:361-366: Laufgeraeusch in zufaelligem Abstand. */
    w->ct2--;
    if (w->ct2 < 0) {
        en24_call_se(w, 74496);
        w->ct2 = (int32_t)(40.0f + (40.0f * en24_rand01(w)));
    }
}

/* en24.c:370 bhEne24_MV03 — auf der Stelle drehen, danach MV02. */
static void en24_mv03(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->way = (int16_t)((en24_rand01(w) > 0.5f) ? 273 : -273);

        w->ct0 = (int32_t)(45.0f * en24_rand01(w)) + 15;

        w->flg |= 0x80000u;

        w->mode3++;
        /* Fallthrough */

    case 1:
        w->ay += w->way;

        if (w->ct0-- == 0) {
            w->mode1 = 1;
            w->mode2 = 2;
            w->mode3 = 0;
        }
        break;

    default:
        en24_gap(w, "bhEne24_MV03 (mode3 ausserhalb 0..1)");
        break;
    }
}

/* en24.c:396 bhEne24_MV04 — Sprung auf den Spieler. */
static void en24_mv04(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN24_EXP0* e = (RECVX_EN24_EXP0*)w->exp0;

    if (!e) return;

    switch (w->mode3) {
    case 0:
        w->flg |= 0x100000u;

        e->vx = 0.6f * -njSin(w->ay);    /* en24.c:403-405 */
        e->vy = 2.5f;
        e->vz = 0.6f * -njCos(w->ay);

        w->flg &= ~0x80000u;
        w->flg &= ~0x20u;

        w->mode3++;
        /* Fallthrough */

    case 1:
        if (w->flg & 0x100000u) {
            RECVX_ENEMY_PLAYER plp;

            w->px += e->vx;
            w->py += e->vy;
            w->pz += e->vz;
            e->vy -= 0.33f;

            if (!en24_player(w, &plp)) break;

            if (!(plp.flg & 4u)) {
                RECVX_ENEMY_VEC self;
                int hit;

                self.x = w->px; self.y = w->py; self.z = w->pz;
                hit = ((plp.stflg & 0x30u) == 0u) && ((plp.stflg & 0x80000000u) == 0u)
                      && (plp.hp >= 0)
                      ? en24_attack_hit_check(w, &self, 2.0f)
                      : 0;

                if (hit) {
                    RECVX_ENEMY_VEC vec;

                    plp.flg |= 0x10004u;     /* en24.c:424 */
                    plp.stflg |= 0x10000u;   /* en24.c:425 */

                    plp.hp -= 1;             /* en24.c:427-431 */
                    if (plp.hp < 0) {
                        plp.hp = 0;
                    }
                    plp.mode0 = 2;           /* en24.c:432-435 */
                    plp.mode1 = 0;
                    plp.mode2 = 0;
                    plp.mode3 = 0;

                    if (sv && sv->player_set) sv->player_set(&plp);
                    else en24_gap(w, "player_set (en24.c:424-435)");

                    en24_call_se(w, 74498);  /* en24.c:437 */

                    /* en24.c:438: &EXP0_UC(0x14) ist der Vektor vx/vy/vz. */
                    if (sv && sv->blood_effect4) {
                        vec.x = e->vx; vec.y = e->vy; vec.z = e->vz;
                        sv->blood_effect4(&self, &vec, 0, 9, 1);
                    } else {
                        en24_gap(w, "bhEne_SetBloodEffect4 (en24.c:438)");
                    }

                    e->vx *= -1.0f;          /* en24.c:439/440 */
                    e->vz *= -1.0f;
                    w->ay += 32768;          /* en24.c:441 */
                    break;
                }
            }
        } else {
            w->mode1 = 0;                    /* en24.c:448-452 */
            w->mode2 = 2;
            w->mode3 = 0;

            w->flg |= 0x20u;
        }
        break;

    default:
        en24_gap(w, "bhEne24_MV04 (mode3 ausserhalb 0..1)");
        break;
    }
}

/* en24.c:458 bhEne24_MV05 — freier Fall mit xn/yn/zn (von einem fremden
   Setzer gefuellt; en24.c schreibt diese drei Felder nicht). */
static void en24_mv05(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->flg |= 0x100000u;
        w->flg &= ~0x80000u;
        w->flg &= ~0x20u;
        w->mode3++;
        /* Fallthrough */

    case 1:
        if (w->flg & 0x100000u) {
            w->px += w->xn;
            w->py += w->yn;
            w->pz += w->zn;
            w->yn -= 0.33f;
            break;
        }
        w->flg |= 0x20u;
        w->mode1 = 0;
        w->mode2 = 2;
        w->mode3 = 0;
        break;

    default:
        en24_gap(w, "bhEne24_MV05 (mode3 ausserhalb 0..1)");
        break;
    }
}

/* en24.c:485/491/497/503 — alle vier leer, Signatur `void (void)`. */
static void en24_nage(RECVX_ENEMY_WORK* w)   { (void)w; }
static void en24_damage(RECVX_ENEMY_WORK* w) { (void)w; }
static void en24_die(RECVX_ENEMY_WORK* w)    { (void)w; }
static void en24_dummy(RECVX_ENEMY_WORK* w)  { (void)w; }

/* en24.c:509 bhEne24_CollisionWalls — die Pruefung laeuft um ar angehoben. */
static void en24_collision_walls(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;

    w->py += w->ar;
    if (sv && sv->collision_works) sv->collision_works(w);
    else en24_gap(w, "bhEne03_Collision (en03.c:5220)");
    w->py -= w->ar;
}

/* en24.c:517 bhEne24_CollisionLine — Flugbahn pxb..px gegen die Raumwaende.
   bhCollisionCheckLine bekommt &epw->px als zweiten Punkt und klemmt ihn im
   Trefferfall auf den Schnittpunkt (hitchkl.c:12/39 bhCollisionCheckLineMain);
   deshalb wird `to` hier zurueck in px/py/pz geschrieben. Variante 1 kennt
   weder flg noch flr_no (hitchkl.h:6). */
static void en24_collision_line(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC from, to, normal;
    NJS_VECTOR n;

    if (!(w->flg & 0x100000u)) return;

    if (!sv || !sv->line) {
        en24_gap(w, "bhCollisionCheckLine (hitchkl.c:12)");
        return;
    }

    from.x = w->pxb; from.y = w->pyb; from.z = w->pzb;
    to.x = w->px;    to.y = w->py;    to.z = w->pz;
    normal.x = normal.y = normal.z = 0.0f;

    if (!sv->line(1u, &from, &to, &normal, 0u, 0, 0, 0)) return;

    w->px = to.x;
    w->py = to.y;
    w->pz = to.z;

    /* en24.c:524: bhGetHitCollisionNormal(&n) — die Normale der LETZTEN
       getroffenen Flaeche, nicht die aus dem line-Ergebnis. */
    if (sv->hit_normal) {
        RECVX_ENEMY_VEC hn;
        if (!sv->hit_normal(&hn)) return;
        n.x = hn.x; n.y = hn.y; n.z = hn.z;
    } else {
        en24_gap(w, "bhGetHitCollisionNormal (en24.c:524)");
        return;
    }

    njUnitVector(&n);
    if (n.y > 0.99f) {
        w->flg &= ~0x100000u;
    }
}

/* ---------------------------------------------------------------------------
   en24.c:534 bhEne24_DeadCheck — der Spieler zertritt die Spinne: die beiden
   Fussgelenke 17 und 21 des SPIELERMODELLS muessen sich zwischen zwei Paessen
   genug bewegt oder gedreht haben und naeher als 2.0 am Work liegen.
   --------------------------------------------------------------------------- */
static int en24_dead_check(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN24_EXP0* e = (RECVX_EN24_EXP0*)w->exp0;
    static const uint32_t joint[2] = { 17u, 21u };
    int i;

    if (!e) return 0;

    if (!sv || !sv->player_joint_matrix) {
        en24_gap(w, "plp->mlwP->owP[17/21].mtx (en24.c:547)");
        return 0;
    }

    for (i = 0; i < 2; i++) {
        float mtx[16];
        RECVX_ENEMY_VEC pos, self;
        NJS_VECTOR vec;
        int32_t ang, jang;
        float mov;

        if (!sv->player_joint_matrix(joint[i], mtx)) {
            en24_gap(w, "plp->mlwP->owP[17/21].mtx (en24.c:547)");
            return 0;
        }

        pos.x = mtx[12];
        pos.y = mtx[13];
        pos.z = mtx[14];

        vec.x = e->p3[i].x - pos.x;
        vec.y = e->p3[i].y - pos.y;
        vec.z = e->p3[i].z - pos.z;

        mov = njScalor(&vec);

        jang = en24_arc_tan2(mtx[8], mtx[10]);

        ang = (int16_t)(jang - e->ang[i]);
        if (ang < 0) {
            ang = -ang;
        }

        e->p3[i] = pos;
        e->ang[i] = jang;

        self.x = w->px; self.y = w->py; self.z = w->pz;

        if ((mov > 0.05f || 182 < ang) && (en24_distance_p2p(&self, &pos) < 2.0f)) {
            return 1;
        }
    }
    return 0;
}

/* en24.c:56 Mode0-Index 5 — bhEne_Event (subpl.c:143). Gemeinsamer
   Ereignisfall; Besitzer ist der Begleitfiguren-Mechanismus. */
static void en24_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    en24_gap(w, "bhEne_Event(subpl_mv01_tbl)");
}
