/* Identitaet 10 — Ameise A. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en10.c (:12 Mode0[6], :22 MoveMode2[3], :30 bhEne10, :94 Init,
   :141 CheckPos, :159 Brain, :188 Move, :253 MV00 (leer), :259 MV01,
   :297 MV02, :354 Nage, :417/423 leere Damage/Die), zonzon1.c:433
   (bhEne_DirTarget), :828 (bhEne_CallSE), :175 (bhEne_SetBloodEffect4),
   pwksub.c:133 (bhAddSpeed), ps2_NaColi.c:65 (njDistanceP2P), macros.h:109
   (njArcTan2), hitchk.c (bhCheckWall, bhCheckPlayer), effect.c:436
   (bhSetEffectTb), vibman.c (StartVibrationEx), eneset.c:906
   (bhEne_CallocWork), Motion.c (bhSetMotion), MdlPut.c:240 (bhCalcModel)
   und subpl.c:143 (bhEne_Event).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */
#include "en10.h"
#include "ninja_math.h"
#include <math.h>
#include <stdlib.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
   EXP0-Block: 0x24 Byte (en10.c:100 bhEne_CallocWork(0x24, 8)). Die
   Original-Byteoffsets stehen in en10.h; die Felder hier liegen exakt dort.
   --------------------------------------------------------------------------- */
typedef struct {
    unsigned char mdl_tick;      /* +0x00 EXP0_UC(0x0)  */
    unsigned char mdl_sel;       /* +0x01 EXP0_UC(0x1)  */
    unsigned char pad02[2];      /* +0x02 Auffuellung des int bei +0x4 */
    int32_t       turn;          /* +0x04 EXP0_I(0x4)   */
    float         home_x;        /* +0x08 EXP0_F(0x8)   */
    float         home_y;        /* +0x0C EXP0_F(0xC)   */
    float         home_z;        /* +0x10 EXP0_F(0x10)  */
    float         home_dist;     /* +0x14 EXP0_F(0x14)  */
    float         ply_dist;      /* +0x18 EXP0_F(0x18)  */
    int32_t       ply_ang;       /* +0x1C EXP0_I(0x1C)  */
    signed char   bite_wait;     /* +0x20 EXP0_C(0x20)  */
    signed char   unused21;      /* +0x21 EXP0_C(0x21)  */
    unsigned char pad22[2];      /* +0x22 Rest des 0x24-Blocks */
} RECVX_EN10_EXP0;

_Static_assert(sizeof(RECVX_EN10_EXP0) == 0x24u,
               "EXP0-Block ist 0x24 Byte (en10.c:100)");
_Static_assert(offsetof(RECVX_EN10_EXP0, turn) == 0x4u, "EXP0_I(0x4)");
_Static_assert(offsetof(RECVX_EN10_EXP0, home_x) == 0x8u, "EXP0_F(0x8)");
_Static_assert(offsetof(RECVX_EN10_EXP0, home_z) == 0x10u, "EXP0_F(0x10)");
_Static_assert(offsetof(RECVX_EN10_EXP0, home_dist) == 0x14u, "EXP0_F(0x14)");
_Static_assert(offsetof(RECVX_EN10_EXP0, ply_dist) == 0x18u, "EXP0_F(0x18)");
_Static_assert(offsetof(RECVX_EN10_EXP0, ply_ang) == 0x1Cu, "EXP0_I(0x1C)");
_Static_assert(offsetof(RECVX_EN10_EXP0, bite_wait) == 0x20u, "EXP0_C(0x20)");

static RECVX_EN10_EXP0* en10_exp0(RECVX_ENEMY_WORK* w)
{
    return (RECVX_EN10_EXP0*)w->exp0;
}

/* rand() des Originals (en10.c:129/131/266/282/284/338/340/401/403). */
static int32_t en10_rand(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->random_next) return sv->random_next();
    if (sv && sv->gap) sv->gap("rand");
    return 0;
}

/* en10.c:129/282/338/401: `rand() % 5 * 1.5 * 182.04444885253906` bzw. ohne
   den Faktor 1.5 (:338/:401) — beide Formen bleiben getrennt. Das Ergebnis
   wird im Original in einen int gelegt, also abgeschnitten. Danach entscheidet
   ein zweiter rand()%2 ueber das Vorzeichen. */
static int32_t en10_rand_turn(RECVX_ENEMY_WORK* w, int with_half)
{
    int32_t ang;
    if (with_half)
        ang = (int32_t)((double)(en10_rand(w) % 5) * 1.5 * 182.04444885253906);
    else
        ang = (int32_t)((double)(en10_rand(w) % 5) * 182.04444885253906);
    return ((en10_rand(w) % 2) != 0) ? ang : -ang;
}

/* pwksub.c:133 bhAddSpeed(pp, r); alle Aufrufer hier uebergeben r = 0. */
static void en10_add_speed(RECVX_ENEMY_WORK* w, int r)
{
    const int32_t angle = (w->ay + r) & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* macros.h:109 njArcTan2(y,x) = NJM_RAD_ANG(atan2f(y,x)). */
static int32_t en10_arc_tan2(float y, float x)
{
    return (int32_t)(atan2f(y, x) * (65536.0f / (3.141592f * 2.0f)));
}

/* zonzon1.c bhArcTan2 — die eigene Form mit asinf und (short)-Umlauf, von
   bhEne_DirTarget benutzt (zonzon1.c:437). */
static int32_t en10_bh_arc_tan2(float y, float x)
{
    float len;
    int32_t ang;

    len = njSqrt((x * x) + (y * y));
    if (len == 0.0f) return 0;

    ang = (int32_t)(10430.381f * asinf(y / len));
    if (x < 0.0f) ang = (int16_t)(32768 - ang);
    return ang;
}

/* zonzon1.c:433 bhEne_DirTarget(epw, x, z, w) — Koerper woertlich. */
static int32_t en10_dir_target(RECVX_ENEMY_WORK* w, float x, float z, int32_t limit)
{
    int32_t ang;

    ang = en10_bh_arc_tan2(w->px - x, w->pz - z);
    ang = (int16_t)(ang - w->ay);

    if (limit < abs(ang)) {
        ang = (ang > 0) ? limit : -limit;
    }
    return ang;
}

/* ps2_NaColi.c:65 njDistanceP2P. */
static float en10_distance_p2p(float ax, float ay, float az,
                               float bx, float by, float bz)
{
    const float dx = bx - ax;
    const float dy = by - ay;
    const float dz = bz - az;
    return njSqrt((dx * dx) + (dy * dy) + (dz * dz));
}

/* zonzon1.c:828 bhEne_CallSE(epw, pos, se) mit dem Gate !(flg & 0x10000). */
static void en10_call_se(RECVX_ENEMY_WORK* w, int se)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos;
    if (w->flg & 0x10000u) return;
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    if (sv && sv->enemy_se && sv->enemy_now) sv->enemy_se(sv->enemy_now(), &pos, se);
    else if (sv && sv->gap) sv->gap("bhEne_CallSE(RequestEnemySe(sys->enow))");
}

static int en10_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->player_get && sv->player_get(out)) return 1;
    if (sv && sv->gap) sv->gap("player_get (Spielerbesitzer)");
    return 0;
}

static void en10_init(RECVX_ENEMY_WORK* w);
static void en10_move(RECVX_ENEMY_WORK* w);
static void en10_nage(RECVX_ENEMY_WORK* w);
static void en10_damage(RECVX_ENEMY_WORK* w);
static void en10_die(RECVX_ENEMY_WORK* w);
static void en10_event(RECVX_ENEMY_WORK* w);
static void en10_mv00(RECVX_ENEMY_WORK* w);
static void en10_mv01(RECVX_ENEMY_WORK* w);
static void en10_mv02(RECVX_ENEMY_WORK* w);
static void en10_check_pos(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN10_PROC)(RECVX_ENEMY_WORK*);

/* en10.c:12 bhEne10_Mode0[6] — kein Eintrag ist NULL. */
static const RECVX_EN10_PROC en10_mode0[6] = {
    en10_init, en10_move, en10_nage, en10_damage, en10_die, en10_event
};
/* en10.c:22 bhEne10_MoveMode2[3]. */
static const RECVX_EN10_PROC en10_move_mode2[3] = {
    en10_mv00, en10_mv01, en10_mv02
};

/* en10.c:30 bhEne10. */
void recvx_en10(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_EN10_EXP0* e;
    float mtx[16];
    int i;

    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u) {
        en10_mode0[w->mode0](w);
    } else if (sv && sv->gap) {
        sv->gap("bhEne10_Mode0");
    }

    e = en10_exp0(w);
    if (!e) {
        /* Das Original liest EXP0_UC(0x0) hier ungeschuetzt; ohne Block ist
           weder Modellwechsel noch CheckPos moeglich. */
        if (sv && sv->gap) sv->gap("EXP0-Block (en10.c:37)");
        return;
    }

    /* en10.c:37-49: jeder dritte Pass schaltet das Modell um 0..3 weiter. */
    e->mdl_tick++;
    if (e->mdl_tick > 2u) {
        e->mdl_tick = 0;
        e->mdl_sel = (unsigned char)(e->mdl_sel + 1u);
        if (e->mdl_sel > 3u) e->mdl_sel = 0;
    }

    /* en10.c:51 epw->mlwP = &epw->mdl[EXP0_UC(0x1)]. Nativ ist mdl_no die
       Auswahl der Modellwurzel; der Zeichenpfad muss sie auswerten. */
    w->mdl_no = e->mdl_sel;

    if (w->flg & 4u) {                       /* en10.c:53-61 */
        for (i = 0; i < (int)RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;
        w->flg &= ~4u;
    }

    if (w->flg & 8u) {                       /* en10.c:63-67 */
        if (sv && sv->check_wall) sv->check_wall(w);
        else if (sv && sv->gap)   sv->gap("bhCheckWall");
        if (sv && sv->check_player) sv->check_player(w);
        else if (sv && sv->gap)     sv->gap("bhCheckPlayer");
    }

    if (sv && sv->calc_model) sv->calc_model(w);
    else if (sv && sv->gap)   sv->gap("bhCalcModel");

    /* en10.c:71-83: beide Kapselenden sitzen auf Gelenk 1; c1 liegt 0.2 ueber
       c2. Das 0.2 ist im Original ein double-Literal. */
    if (sv && sv->joint_matrix && sv->joint_matrix(w, 1u, mtx)) {
        w->watr.c1.x = mtx[12];
        w->watr.c1.y = (float)((double)mtx[13] + 0.2);
        w->watr.c1.z = mtx[14];
        w->watr.c2.x = mtx[12];
        w->watr.c2.y = mtx[13];
        w->watr.c2.z = mtx[14];
    } else if (sv && sv->gap) {
        sv->gap("bhEne10:mlwP->owP[1].mtx");
    }
    w->watr.r = 0.8f;

    if (e->bite_wait != 0) {                 /* en10.c:85-88 */
        e->bite_wait--;
    }

    en10_check_pos(w);                       /* en10.c:90 */
}

/* en10.c:94 bhEne10_Init. */
static void en10_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN10_EXP0* e;

    if (w->exp0 == NULL) {
        if (sv && sv->calloc_work) w->exp0 = sv->calloc_work(0x24u);
        else if (sv && sv->gap)    sv->gap("bhEne_CallocWork(0x24, 8)");
    }

    w->flg |= 0x138u;        /* en10.c:103 */
    w->flg2 &= ~1u;          /* :104 */
    w->flg &= 0xFFFF7FF9u;   /* :105 */
    w->mdflg &= ~1u;         /* :106 */

    w->ar = 1.0f;
    w->ah = 1.0f;
    w->car = 1.0f;
    w->cah = 1.0f;

    w->hp = 1;               /* :111 */

    e = en10_exp0(w);
    if (!e) {
        /* Ohne Block kann der Heimatpunkt nicht gemerkt werden; das Original
           schreibt hier ungeschuetzt. */
        if (sv && sv->gap) sv->gap("EXP0-Block (en10.c:113)");
    } else {
        e->home_x = w->px;   /* :113-115 */
        e->home_y = w->py;
        e->home_z = w->pz;
    }

    w->spd = 0.14f;          /* :117 */

    if (e) {
        e->ply_dist = 999.0f;   /* :119 */
        /* :120 schreibt EXP0_F(0x1C) = 0, also dasselbe Feld, das :155 als
           int beschreibt. Beide Zugriffe gehoeren zum Original; der Nullwert
           ist in beiden Deutungen 0. */
        e->ply_ang = 0;
        e->bite_wait = 0;       /* :121 */
        e->unused21 = 0;        /* :122 */
    }

    w->mode0 = 1;
    w->mode1 = 1;
    w->mode2 = 1;
    w->mode3 = 0;

    /* :129-133 */
    if (e) e->turn = en10_rand_turn(w, 1);
    else   (void)en10_rand_turn(w, 1);   /* der Zufallsstrom laeuft trotzdem */

    w->clp_jno[0] = 0;
    w->clp_jno[1] = -1;
    w->mdflg &= ~0x20u;
}

/* en10.c:141 bhEne10_CheckPos. */
static void en10_check_pos(RECVX_ENEMY_WORK* w)
{
    RECVX_EN10_EXP0* e = en10_exp0(w);
    RECVX_ENEMY_PLAYER ply;
    float dx, dz;

    if (!e) return;

    dx = w->px - e->home_x;      /* :146-149 */
    dz = w->pz - e->home_z;
    e->home_dist = njSqrt(dx * dx + dz * dz);

    if (!en10_player(w, &ply)) return;

    dx = w->px - ply.pos.x;      /* :151-155 */
    dz = w->pz - ply.pos.z;
    e->ply_dist = njSqrt(dx * dx + dz * dz);
    e->ply_ang = (int16_t)(en10_arc_tan2(dx, dz) - w->ay);
}

/* en10.c:159 bhEne10_Brain. */
static void en10_brain(RECVX_ENEMY_WORK* w)
{
    RECVX_EN10_EXP0* e = en10_exp0(w);
    RECVX_ENEMY_PLAYER ply;

    if (!e) return;

    if (en10_player(w, &ply)) {
        if (!(ply.stflg & 0x80000000u)) {                   /* :161 */
            if (!(ply.flg & 4u) && !(ply.flg & 2u) && !(w->flg & 4u)) {   /* :163 */
                if ((e->bite_wait == 0) && (e->ply_dist < 3.0f)
                    && (abs(e->ply_ang) < 0xE38)) {         /* :165 */
                    w->mode0 = 2;
                    w->mode1 = 0;
                    w->mode2 = 0;
                    w->mode3 = 0;
                    en10_nage(w);                           /* :171 */
                    return;
                }
            }
        }
    }

    if (!(e->home_dist <= 10.0f) && (w->mode2 != 2u)) {     /* :177-184 */
        w->mode0 = 1;
        w->mode1 = 1;
        w->mode2 = 2;
        w->mode3 = 0;
        w->spd = 0.042000003f;
    }
}

/* en10.c:188 bhEne10_Move. */
static void en10_move(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER ply;
    RECVX_ENEMY_VEC pos;

    if (w->mode1 == 1u) {                    /* :192-195 */
        en10_brain(w);
    }

    /* :197-215: ein Spieler in Nahkampfbewegung (mode0 1, mode2 1..11)
       innerhalb 1.5 toetet die Ameise. */
    if (en10_player(w, &ply)) {
        if ((ply.mode0 == 1u)
            && (en10_distance_p2p(w->px, w->py, w->pz,
                                  ply.pos.x, ply.pos.y, ply.pos.z) < 1.5f)) {
            switch (ply.mode2) {
            case 1: case 2: case 3: case 4: case 5: case 6:
            case 7: case 8: case 9: case 10: case 11:
                w->flg |= 2u;
                break;
            default:
                break;
            }
        }
    }

    if (w->flg & 4u) {                       /* :217-220 */
        w->flg |= 2u;
    }

    if (w->flg & 2u) {                       /* :222-244 */
        en10_call_se(w, 0x2303);

        pos.x = w->px;
        pos.y = w->py;
        pos.z = w->pz;

        w->dvx = 0.0f;
        w->dvy = 1.0f;
        w->dvz = 0.0f;

        if (sv && sv->blood_effect4) {
            RECVX_ENEMY_VEC vec;
            vec.x = w->dvx; vec.y = w->dvy; vec.z = w->dvz;
            sv->blood_effect4(&pos, &vec, 1, 1, 0);
        } else if (sv && sv->gap) {
            sv->gap("bhEne_SetBloodEffect4");
        }

        w->mdflg |= 1u;
        w->mode0 = 4;
        w->mode1 = 0;
        w->mode2 = 0;
        w->mode3 = 0;
        w->hp = -1;
        return;
    }

    if (w->mode0 == 1u) {                    /* :246-249 */
        if (w->mode2 < 3u) en10_move_mode2[w->mode2](w);
        else if (sv && sv->gap) sv->gap("bhEne10_MoveMode2");
    }
}

/* en10.c:253 bhEne10_MV00 — leer (Originalsignatur `void (void)`). */
static void en10_mv00(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en10.c:259 bhEne10_MV01 — Umherlaufen mit festem Drehschritt.
   ORIGINALER DURCHFALL von case 0 nach case 1. */
static void en10_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN10_EXP0* e = en10_exp0(w);

    switch (w->mode3) {
    case 0:
        w->ct0 = (en10_rand(w) % 10) + 0xA;    /* :266 */
        w->mode3++;
        /* fall through */
    case 1:
        if (e) w->ay += e->turn;                /* :271 */
        else if (w->sv && w->sv->gap) w->sv->gap("EXP0-Block (en10.c:271)");

        en10_add_speed(w, 0);                   /* :273 */

        if (w->ct0 == 0x13) {                   /* :275-278 */
            en10_call_se(w, 0x12300);
        }

        if (w->ct0-- == 0) {                    /* :280, Postdekrement */
            /* :282-286 */
            if (e) e->turn = en10_rand_turn(w, 1);
            else   (void)en10_rand_turn(w, 1);

            w->mode0 = 1;
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
        }
    }
}

/* en10.c:297 bhEne10_MV02 — Rueckweg zum Heimatpunkt. */
static void en10_mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN10_EXP0* e = en10_exp0(w);
    int32_t ay;

    if (!e) {
        if (w->sv && w->sv->gap) w->sv->gap("EXP0-Block (en10.c:304)");
        return;
    }

    switch (w->mode3) {
    case 0:
        ay = en10_dir_target(w, e->home_x, e->home_z, 0x5B0);   /* :304 */
        w->ay += ay;
        en10_add_speed(w, 0);

        /* :309: das Original rechnet 0.005493164f * ay als float, legt es in
           einen int (abgeschnitten) und vergleicht diesen gegen 1.0f. Der
           Zweig ist damit erst ab |ay| >= 183 gesperrt. */
        if (abs((int)(0.005493164f * (float)ay)) < 1.0f) {
            w->mode3++;
            w->spd = 0.14f;
        }
        break;

    case 1:
        w->ay += en10_dir_target(w, e->home_x, e->home_z, 0x38E);  /* :319 */
        en10_add_speed(w, 0);

        if (w->ct0 == 0x12) {                   /* :323-326 */
            en10_call_se(w, 0x12300);
        }

        /* :329: ebenfalls int-Abschnitt vor dem Vergleich gegen 1.0f. */
        if ((abs((int)(w->px - e->home_x)) < 1.0f)
            && (abs((int)(w->pz - e->home_z)) < 1.0f)) {
            w->mode3 += 1;
        }
        break;

    case 2:
        /* :338-342: hier OHNE den Faktor 1.5. */
        e->turn = en10_rand_turn(w, 0);

        w->mode0 = 1;
        w->mode1 = 1;
        w->mode2 = 1;
        w->mode3 = 0;
        w->spd = 0.14f;
    }
}

/* en10.c:354 bhEne10_Nage — der Biss. ORIGINALER DURCHFALL von case 0
   nach case 1. */
static void en10_nage(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN10_EXP0* e = en10_exp0(w);
    RECVX_ENEMY_PLAYER ply;

    switch (w->mode3) {
    case 0:
        if (en10_player(w, &ply)) {             /* :361-372 */
            ply.flg |= 4u;
            ply.hp--;
            if (ply.hp < 0) ply.hp = 0;
            ply.mode0 = 2;
            ply.mode1 = 0;
            ply.mode2 = 0;
            ply.mode3 = 0;
            if (sv && sv->player_set) sv->player_set(&ply);
            else if (sv && sv->gap) sv->gap("player_set (Spielerbesitzer)");
        }
        w->ct0 = 0xA;                           /* :373 */

        en10_call_se(w, 0x12302);               /* :375 */

        if (sv && sv->vibration) sv->vibration(1, 0xA);   /* :376 */
        else if (sv && sv->gap)  sv->gap("StartVibrationEx(1, 0xA)");

        /* :378-390: EF-Vorlage {flg 1, id 5, type 0, mdlver 0,
           px = px-0.5, py = py+0.3, pz = pz-0.5, sx/sy/sz 2, ay = epw->ay}
           und bhSetEffectTb(&sys->ef, NULL, NULL, 0) (effect.c:436) — also
           OHNE Verknuepfung. set_effect (enemy_work.h:447) kennt nur
           id/pos/off/link/param: Skala 2 und der Winkel ay haetten dort
           keinen Platz, der Spawn wuerde mit Skala 1 und Winkel 0 entstehen.
           Deshalb kein Ersatzspawn, sondern die gemeldete Luecke. */
        if (sv && sv->gap)
            sv->gap("bhSetEffectTb(EF{flg=1,id=5,type=0,scale=2,ay=epw->ay})");

        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {                    /* :395 */
            /* :397: ay + 0x7FFF + 1 = Kehrtwende. */
            w->ay = w->ay + 0x7FFF + 1;

            if (e) {
                e->bite_wait = 0x1E;            /* :399 */
                /* :401-405: ohne den Faktor 1.5. */
                e->turn = en10_rand_turn(w, 0);
            } else {
                (void)en10_rand_turn(w, 0);
                if (sv && sv->gap) sv->gap("EXP0-Block (en10.c:399)");
            }

            w->mode0 = 1;
            w->mode1 = 1;
            w->mode2 = 1;
            w->mode3 = 0;
            w->spd = 0.14f;
        }
    }
}

/* en10.c:417 — leer (Originalsignatur `void bhEne10_Damage(void)`). */
static void en10_damage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en10.c:423 — leer. Mode0 4 (Tod) laeuft damit nur noch die Rumpfarbeit von
   bhEne10 (Modellwechsel, Kapsel, CheckPos) ab; genau so im Original. */
static void en10_die(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en10.c:19 Mode0-Index 5 — bhEne_Event (subpl.c:143) = subpl_mv01_tbl[ct2].
   Diesen Zustand besitzt der Begleitfiguren-Mechanismus; der Vertragsdienst
   `event` (enemy_work.h:489-495) wird gerufen, sobald er gebunden ist. */
static void en10_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    if (w->sv && w->sv->gap) w->sv->gap("bhEne_Event(subpl_mv01_tbl)");
}
