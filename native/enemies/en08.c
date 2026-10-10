/* Identitaet 8 — Fisch. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en08.c (:18 Mode0[6], :28 MoveMode2[3], :36 bhEne08, :45 bhEne08_Init,
   :81 bhEne08_Move, :85 bhEne08_MV00, :168 bhEne08_MV01, :263 bhEne08_MV02,
   :310/315/320 leere Nage/Damage/Die), zonzon1.c:529
   (bhEne_CheckEnemiesBall), pwksub.c:133 (bhAddSpeed), Motion.c:21
   (bhSetMotion), MdlPut.c (bhCalcModel), macros.h:93-109
   (RAND_MAX/njRandom/bhRandomAngle/njArcSin/njArcTan2), hitchkl.h:6
   (bhCollisionCheckLine) und hitchk.h:10 (bhCheckWallType).

   Der Fisch setzt in Init KEIN hp (en08.c:45-79): im Original steht die
   Trefferzahl im ETTY_WORK, und bhSetEnemy (eneset.c:183-241) kopiert sie
   nicht — der Work startet mit hp 0. Genauso bleibt es hier; kein
   erfundener Startwert.

   bhEne03_Collision (en03.c:5220) gehoert zur Identitaet 3 und ist im
   gemeinsamen Vertrag nicht enthalten. Ihr Fehlen wird beim Aufruf
   namentlich gemeldet (kein stiller Ersatz, keine eigene Kollisionslogik).

   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */
#include "en08.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>

/* ---------------------------------------------------------------------------
   Winzige Originalbausteine (Makros bzw. Funktionskoerper), woertlich:
   --------------------------------------------------------------------------- */

/* macros.h:93/105: njRandom() = -rand()/(RAND_MAX+1), RAND_MAX 0x7fffffff.
   Der gemeinsame EE-libc-Strom des Vertrags (enemy_work.h:309) liefert
   rand(); alle vier Zufallsstellen dieser Identitaet brauchen den Wert in
   [0,1) — belegt durch den Tabellenindex in en07.c:153
   (int)(16.0f*njRandom()) auf eine 16er-Tabelle und den Muenzwurf
   en07.c:350. Genau diese Normierung 1/2^31 benutzt der vorhandene native
   Kern (native/en56.c:229, native/weapon_flash.cpp:66). */
static float en08_rand01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->random_next) return (float)sv->random_next() / 2147483648.0f;
    if (sv && sv->gap) sv->gap("rand");
    return 0.0f;   /* kein erfundener Zufallswert */
}

/* macros.h:106 bhRandomAngle(deg). */
static float en08_rand_angle(RECVX_ENEMY_WORK* w, int deg)
{
    /* NJM_DEG_ANG (macros.h:104). */
    const float u = (float)(int32_t)(((float)deg * 65536.0f) / 360.0f);
    return en08_rand01(w) * (u * 2.0f) - u;
}

/* macros.h:102/108/109: NJM_RAD_ANG, njArcSin, njArcTan2. */
static int32_t en08_arc_tan2(float y, float x)
{
    return (int32_t)(atan2f(y, x) * (65536.0f / (3.141592f * 2.0f)));
}
static int32_t en08_arc_sin(float n)
{
    return (int32_t)(asinf(n) * (65536.0f / (3.141592f * 2.0f)));
}

/* pwksub.c:133 bhAddSpeed(pp, r): Bewegung in Blickrichtung. r ist die
   Originalzusatzdrehung; alle Aufrufer dieses Gegners uebergeben 0. */
static void en08_add_speed(RECVX_ENEMY_WORK* w, int r)
{
    const int32_t angle = (w->ay + r) & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

static void en08_init(RECVX_ENEMY_WORK* w);
static void en08_move(RECVX_ENEMY_WORK* w);
static void en08_nage(RECVX_ENEMY_WORK* w);
static void en08_damage(RECVX_ENEMY_WORK* w);
static void en08_die(RECVX_ENEMY_WORK* w);
static void en08_event(RECVX_ENEMY_WORK* w);
static void en08_mv00(RECVX_ENEMY_WORK* w);
static void en08_mv01(RECVX_ENEMY_WORK* w);
static void en08_mv02(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN08_PROC)(RECVX_ENEMY_WORK*);

/* en08.c:18 bhEne08_Mode0[6]. */
static const RECVX_EN08_PROC en08_mode0[6] = {
    en08_init, en08_move, en08_nage, en08_damage, en08_die, en08_event
};
/* en08.c:28 bhEne08_MoveMode2[3]. */
static const RECVX_EN08_PROC en08_move_mode2[3] = {
    en08_mv00, en08_mv01, en08_mv02
};

/* en08.c:36 bhEne08. */
void recvx_en08(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u) {
        en08_mode0[w->mode0](w);
    } else if (sv && sv->gap) {
        sv->gap("bhEne08_Mode0");
    }

    if (sv && sv->set_motion) sv->set_motion(w, w->mtn_add, w->mtn_md);
    else if (sv && sv->gap)   sv->gap("bhSetMotion");

    w->ar = 5.0f;

    /* en08.c:41 bhEne03_Collision: Koerperkollision der Fischgroesse gegen
       die ATR-Flaechen des Raums (en03.c:5220). Besitzer ist Identitaet 3
       (T3). Ohne Anschluss wird die Luecke gemeldet — der Fisch bewegt sich
       dann ohne Wandkontakt weiter, es wird nichts erfunden. */
    if (sv && sv->gap) sv->gap("bhEne03_Collision(en03.c:5220)");

    /* en08.c:42 bhEne_CheckEnemiesBall (zonzon1.c:529); geteilte Fassung
       dieser Teilaufgabe siehe unten. */
    recvx_t4_enemies_ball(w);

    if (sv && sv->calc_model) sv->calc_model(w);
    else if (sv && sv->gap)   sv->gap("bhCalcModel");
}

/* en08.c:45 bhEne08_Init. */
static void en08_init(RECVX_ENEMY_WORK* w)
{
    w->flg |= 0x8018u;   /* 0x8000 Schadenszustand | 0x10 Wand | 0x8 Feindkontakt */
    w->flg &= ~0x6u;

    w->ar = 5.0f;
    w->ah = 1.0f;
    w->car = 3.0f;

    w->mode0 = 1;
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;

    w->hokan_rate = 65536;
    w->hokan_count = 0;

    w->mtn_no = 0;
    w->mtn_md = 0;
    w->mtn_add = 65536;

    w->frm_no = 0;

    w->clp_jno[0] = 0;
    w->clp_jno[1] = -1;

    w->mdflg &= ~0x20u;

    w->ct1 = (en08_rand01(w) > 0.5f) ? 1 : -1;
}

/* en08.c:81 bhEne08_Move: nur die mode2-Tabelle, KEINE type-Tabelle. */
static void en08_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 3u) en08_move_mode2[w->mode2](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne08_MoveMode2");
}

/* en08.c:85 bhEne08_MV00 — Kreisen dicht unter der Oberflaeche.
   case 0 faellt im Original in case 1 durch (en08.c:115). */
static void en08_mv00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos;
    float dy;
    NJS_VECTOR vec;

    switch (w->mode3) {
    case 0:
        w->mtn_no = 0;
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 45875;

        w->ct0 = (int32_t)(en08_rand01(w) * 180.0f) + 120;
        w->ct1 = (en08_rand01(w) > 0.5f) ? 1 : -1;
        w->spd = 0.01f;
        w->ayp = (int32_t)(w->ay + en08_rand_angle(w, 3));
        w->yn  = en08_rand01(w) * 2.0f + w->py - 1.0f;
        w->mode3++;
        /* fall through */
    case 1:
        en08_add_speed(w, 0);
        if (w->ct0-- == 0) {
            w->mode2 = 1;
            w->mode3 = 0;
        }
    }

    w->ay += (int32_t)(int16_t)(w->ayp - w->ay) / 8;

    /* en08.c:118-124: py wird im Original ZWEIMAL um dy angehoben; die
       Redundanz steht so im Original und bleibt erhalten. */
    w->py += (w->yn - w->py) / 100.0f;
    dy = (w->yn - w->py) / 100.0f;
    w->py += dy;

    w->axp = en08_arc_tan2(dy, 0.1f);
    w->ax += (int32_t)(int16_t)(w->axp - w->ax) / 16;

    pos.x = w->px - njSin(w->ayp) * 20.0f;
    pos.y = w->py;
    pos.z = w->pz - njCos(w->ayp) * 20.0f;

    if (sv && sv->wall_type) {
        if (sv->wall_type(&pos, 0u, 5.0f, 1.0f)) w->ayp += w->ct1 * 91;
    } else if (sv && sv->gap) {
        sv->gap("bhCheckWallType");
    }

    pos.x = w->px - njSin(w->ayp) * 7.0f;
    pos.y = w->py;
    pos.z = w->pz - njCos(w->ayp) * 7.0f;

    if (sv && sv->line) {
        RECVX_ENEMY_VEC from;
        RECVX_ENEMY_VEC norm;
        from.x = w->px; from.y = w->py; from.z = w->pz;
        if (sv->line(1u, &from, &pos, &norm, 0u, 0, 0, 0) != 0) {
            /* bhGetHitCollisionNormal liefert genau die Normale, die der
               Treffer von bhCollisionCheckLine hinterlassen hat. */
            vec.x = norm.x; vec.y = norm.y; vec.z = norm.z;
            njUnitVector(&vec);

            njUnitMatrix(0);
            njRotateY(0, -w->ay);
            njCalcVector(0, &vec, &vec);

            if (vec.x < 0.0f) w->ct1 = 1;
            else              w->ct1 = -1;

            w->ayp = en08_arc_sin(vec.z);

            w->mode2 = 2;
            w->mode3 = 0;
        }
    } else if (sv && sv->gap) {
        sv->gap("bhCollisionCheckLine");
    }
}

/* en08.c:168 bhEne08_MV01 — schneller Vorstoss, danach Auslaufen. */
static void en08_mv01(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos;
    float dy;
    NJS_VECTOR vec;

    switch (w->mode3) {
    case 0:
        w->mtn_no = 1;
        w->frm_no = 0;
        w->hokan_count = 8;
        w->hokan_rate = 45875;

        w->ct0 = (int32_t)(60.0f * en08_rand01(w)) + 90;
        w->spd = 0.2f + 0.2f * en08_rand01(w);
        w->ayp = (int32_t)(w->ay + en08_rand_angle(w, 10));
        w->yn  = w->py + 6.0f * en08_rand01(w) - 3.0f;

        w->mode3++;
        break;

    case 1:
        if (w->frm_no == 0) {
            w->mtn_no = 0;
            w->frm_no = 0;
            w->hokan_count = 8;
            w->hokan_rate = 45875;
        }

        if (w->ct0-- == 0) {
            w->mode2 = 0;
            w->mode3 = 0;
        }
    }
    en08_add_speed(w, 0);

    w->spd *= 0.96f;
    if (w->spd < 0.01f) w->spd = 0.01f;

    w->ay += (int32_t)(int16_t)(w->ayp - w->ay) / 8;

    dy = (w->yn - w->py) / 100.0f;
    w->py += dy;

    w->axp = en08_arc_tan2(dy, 0.1f);
    w->ax += (int32_t)(int16_t)(w->axp - w->ax) / 16;

    pos.x = w->px - njSin(w->ayp) * 20.0f;
    pos.y = w->py;
    pos.z = w->pz - njCos(w->ayp) * 20.0f;

    if (sv && sv->wall_type) {
        if (sv->wall_type(&pos, 0u, 5.0f, 1.0f)) w->ayp += w->ct1 * 91;
    } else if (sv && sv->gap) {
        sv->gap("bhCheckWallType");
    }

    pos.x = w->px - njSin(w->ayp) * 7.0f;
    pos.y = w->py;
    pos.z = w->pz - njCos(w->ayp) * 7.0f;

    if (sv && sv->line) {
        RECVX_ENEMY_VEC from;
        RECVX_ENEMY_VEC norm;
        from.x = w->px; from.y = w->py; from.z = w->pz;
        if (sv->line(1u, &from, &pos, &norm, 0u, 0, 0, 0) != 0) {
            vec.x = norm.x; vec.y = norm.y; vec.z = norm.z;
            njUnitVector(&vec);

            njUnitMatrix(0);
            njRotateY(0, -w->ay);
            njCalcVector(0, &vec, &vec);

            if (vec.x < 0.0f) w->ct1 = 1;
            else              w->ct1 = -1;

            w->ayp = en08_arc_sin(vec.z);

            w->mode2 = 2;
            w->mode3 = 0;
        }
    } else if (sv && sv->gap) {
        sv->gap("bhCollisionCheckLine");
    }
}

/* en08.c:263 bhEne08_MV02 — Ausweichen/Abdrehen. case 0 faellt in case 1
   durch (en08.c:290). */
static void en08_mv02(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        /* en08.c:266: ((-rand()) / -2.1474836e9f) ist die zweite Schreibweise
           desselben Zufallswerts in [0,1) (njRandom). */
        w->ayp = (int32_t)(w->ay + w->ct1 * (w->ayp + 3640.0f * en08_rand01(w)));
        if (w->ct1 > 0) w->mtn_no = 3;
        else            w->mtn_no = 2;

        w->frm_no = 0;

        w->hokan_count = 8;
        w->hokan_rate = 45875;

        w->ct0 = 30;
        w->spd = 0.1f;

        w->mode3++;
        /* fall through */
    case 1:
        en08_add_speed(w, 0);
        w->spd *= 0.96f;

        if (w->spd < 0.01f) w->spd = 0.01f;

        if (w->ct0-- == 0) {
            w->mode2 = 1;
            w->mode3 = 0;
        }
        break;
    }

    w->ax -= (int32_t)(int16_t)w->ax / 8;
    w->ay += (int32_t)(int16_t)(w->ayp - w->ay) / 12;
}

/* en08.c:310 — leer. */
static void en08_nage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en08.c:315 — leer. */
static void en08_damage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en08.c:320 — leer. */
static void en08_die(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en08.c:24 Mode0-Index 5 — bhEne_Event (subpl.c:143). Gemeinsamer
   Ereignisfall; Besitzer ist der Begleitfiguren-Mechanismus. Der Vertrag
   fuehrt ihn als Dienst `event` (enemy_work.h:489-495); ist er gebunden, wird
   er gerufen, sonst bleibt nur die gemeldete Luecke. */
static void en08_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    if (w->sv && w->sv->gap) w->sv->gap("bhEne_Event(subpl_mv01_tbl)");
}

/* ---------------------------------------------------------------------------
   zonzon1.c:529 bhEne_CheckEnemiesBall — geteilter Helfer dieser
   Teilaufgabe (Benutzer en08.c:42, en06.c:253, en21.c:124; im Original
   ausserdem en03.c:552).
   --------------------------------------------------------------------------- */
void recvx_t4_enemies_ball(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    float px, py, pz;
    uint32_t i, n;

    if (!w) return;
    sv = w->sv;

    /* zonzon1.c:541: flg2-Bit 0 sperrt den Abgleich (waehrend eines
       Angriffs). */
    if (w->flg2 & 0x1u) return;

    if (!sv) return;
    n = sv->enemy_count ? sv->enemy_count() : 0u;
    if (!sv->enemy_at) {
        if (sv->gap) sv->gap("bhEne_CheckEnemiesBall:ene");
        return;
    }

    px = w->px + w->aox;
    py = w->py + w->aoy;
    pz = w->pz + w->aoz;

    /* zonzon1.c:551 vergleicht den laufenden Work mit plp. Die drei
       nativen Aufrufer (en08.c:42, en06.c:253, en21.c:124) sind stets
       Gegnerworks: in dieser Portierung ist der Spieler kein
       RECVX_ENEMY_WORK, sondern eigener Besitzer mit Sichtstruktur
       (enemy_work.h:123-147). Damit ist `w == plp` an diesen Stellen exakt
       immer falsch, `(epw != plp) || (epp->flg & 0x40)` also immer wahr und
       der stflg-Zweig immer 0x2. Sobald ein anderer Besitzer diesen Helfer
       mit einem Spielerwork aufruft, muss er die Identitaet uebergeben. */
    {
        const int is_player = 0;

    for (i = 0; i < n; ++i) {
        RECVX_ENEMY_WORK* epp = sv->enemy_at(i);
        if (!epp) continue;
        /* zonzon1.c:551: nur lebende Works mit Feindkontaktbit, nicht der
           eigene; der Spielerwork nur gegen Gegner mit flg 0x40. */
        if ((epp->flg & 0x1u) && (epp->flg & 0x8u) && epp != w &&
            (!is_player || (epp->flg & 0x40u))) {
            const float ex0 = epp->px + epp->aox;
            const float ey0 = epp->py + epp->aoy;
            const float ez0 = epp->pz + epp->aoz;

            const float ex1 = px - ex0;
            const float ey1 = py - ey0;
            const float ez1 = pz - ez0;

            const float ln = njSqrt((ex1 * ex1) + (ey1 * ey1) + (ez1 * ez1));

            float r = w->car + epp->car;

            if (ln < r) {
                const float ex2 = ex0 - px;
                const float ey2 = ey0 - py;
                const float ez2 = ez0 - pz;

                r = (1.0f / ln) * r;

                w->px = (ex0 - (ex2 * r)) - w->aox;
                w->py = (ey0 - (ey2 * r)) - w->aoy;
                w->pz = (ez0 - (ez2 * r)) - w->aoz;

                if (is_player) {
                    epp->stflg |= 0x4u;
                } else {
                    epp->stflg |= 0x2u;
                }
            }
        }
    }
    }
}
