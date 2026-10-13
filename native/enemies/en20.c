/* Identitaet 20 — Ameise B. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en20.c (:10 Mode0[6], :20 MoveMode2[8], :33 bhEne20, :41 bhEne20_Init,
   :85 bhEne20_Move, :94 MV00, :140 MV01, :196 MV02, :228 MV03, :260 MV04,
   :292 MV05, :324 MV06, :366 MV07, :408/414/420 leere Nage/Damage/Die),
   pwksub.c:133 (bhAddSpeed), eneset.c:906 (bhEne_CallocWork), Motion.c
   (bhSetMotion), MdlPut.c:240 (bhCalcModel) und subpl.c:143 (bhEne_Event).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */
#include "en20.h"
#include "../ninja_math.h"
#include "../core_random.h"
#include <math.h>
#include <stddef.h>

/* Der Zufall ist hier der rohe `rand() % n` des Originals
   (en20.c:109/119/155/168), nicht njRandom. random_next (enemy_work.h:309)
   ist derselbe EE-libc-Strom. */
static int32_t en20_rand(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->random_next) return sv->random_next();
    return recvx_core_random_next(); /* vorhandener gemeinsamer EE-Strom */
}

/* pwksub.c:133 bhAddSpeed(pp, r): Bewegung in Blickrichtung. Alle Aufrufer
   dieser Identitaet uebergeben r = 0. */
static void en20_add_speed(RECVX_ENEMY_WORK* w, int r)
{
    const int32_t angle = (w->ay + r) & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* epw->mnwP[epw->mtn_no].frm_num (en20.c:273/305/337/348/379/390). */
static int32_t en20_frm_num(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->motion_frames) return (int32_t)sv->motion_frames(w, w->mtn_no);
    if (sv && sv->gap) sv->gap("mnwP[mtn_no].frm_num");
    return 0;
}

static void en20_init(RECVX_ENEMY_WORK* w);
static void en20_move(RECVX_ENEMY_WORK* w);
static void en20_nage(RECVX_ENEMY_WORK* w);
static void en20_damage(RECVX_ENEMY_WORK* w);
static void en20_die(RECVX_ENEMY_WORK* w);
static void en20_event(RECVX_ENEMY_WORK* w);
static void en20_mv00(RECVX_ENEMY_WORK* w);
static void en20_mv01(RECVX_ENEMY_WORK* w);
static void en20_mv02(RECVX_ENEMY_WORK* w);
static void en20_mv03(RECVX_ENEMY_WORK* w);
static void en20_mv04(RECVX_ENEMY_WORK* w);
static void en20_mv05(RECVX_ENEMY_WORK* w);
static void en20_mv06(RECVX_ENEMY_WORK* w);
static void en20_mv07(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN20_PROC)(RECVX_ENEMY_WORK*);

/* en20.c:10 bhEne20_Mode0[6] — kein Eintrag ist NULL. */
static const RECVX_EN20_PROC en20_mode0[6] = {
    en20_init, en20_move, en20_nage, en20_damage, en20_die, en20_event
};
/* en20.c:20 bhEne20_MoveMode2[8]. */
static const RECVX_EN20_PROC en20_move_mode2[8] = {
    en20_mv00, en20_mv01, en20_mv02, en20_mv03,
    en20_mv04, en20_mv05, en20_mv06, en20_mv07
};

/* en20.c:33 bhEne20. */
void recvx_en20(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u) {
        en20_mode0[w->mode0](w);
    } else if (sv && sv->gap) {
        sv->gap("bhEne20_Mode0");
    }

    /* en20.c:36. Der vierte Originalparameter mtn_tp von bhSetMotion hat im
       Vertrag keinen Platz (enemy_work.h:375); en20 setzt ihn nie. */
    if (sv && sv->set_motion) sv->set_motion(w, (int)w->mtn_add, w->mtn_md);
    else if (sv && sv->gap)   sv->gap("bhSetMotion");

    if (sv && sv->calc_model) sv->calc_model(w);
    else if (sv && sv->gap)   sv->gap("bhCalcModel");
}

/* en20.c:41 bhEne20_Init. */
static void en20_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    float dx, dz;

    if (w->exp0 == NULL) {
        /* en20.c:47 bhEne_CallocWork(0x30, 8). Der Block wird von dieser
           Identitaet nie gelesen oder geschrieben — en20.c benutzt kein
           EXP0_*-Makro; er wird nur belegt. Der Vertragsdienst
           (enemy_work.h:537) nimmt keine Ausrichtung; NULL bleibt NULL. */
        if (sv && sv->calloc_work) w->exp0 = sv->calloc_work(0x30u);
        else if (sv && sv->gap)    sv->gap("bhEne_CallocWork(0x30, 8)");
    }

    w->flg |= 1u;         /* en20.c:50 */
    w->flg |= 0x8000u;    /* :51 */
    w->flg &= ~6u;        /* :52 */

    w->ar = 0.0f;
    w->ah = 0.0f;
    w->aw = 0.0f;
    w->ad = 0.0f;

    w->hp = 0x64;         /* :57 */

    if (w->type == 0u) {  /* :59-67 */
        dx = w->px - 70.0f;
        dz = w->pz - 70.0f;
        w->ax = 0x4000;
        /* :66 ist ein direkter atan2f, NICHT bhArcTan2 (kein (short)-Umlauf,
           kein njSqrt-Vorzeichenzweig); 10430.381 = 65536/2PI. */
        w->ay = (int32_t)(10430.381f * atan2f(dx, dz));
    }

    w->mtn_no = 0;
    w->frm_no = 0;
    w->hokan_count = 0;
    w->hokan_rate = 0x10000;
    w->mtn_add = 0x10000;
    w->mtn_md = 0;

    w->mode0 = 1;
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;

    w->clp_jno[0] = 0;
    w->clp_jno[1] = -1;

    w->mdflg &= ~0x20u;
}

/* en20.c:85 bhEne20_Move. Das Original prueft hier mode0 (nicht mode1); da
   Move nur aus mode0 == 1 heraus gerufen wird, ist die Bedingung immer
   erfuellt — sie bleibt so stehen. */
static void en20_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode0 == 1u) {
        if (w->mode2 < 8u) en20_move_mode2[w->mode2](w);
        else if (w->sv && w->sv->gap) w->sv->gap("bhEne20_MoveMode2");
    }
}

/* en20.c:94 bhEne20_MV00 — Ruhezustand, Motion 0.
   ORIGINALER DURCHFALL von case 0 nach case 1; case 1 hat kein break (Ende
   des switch). */
static void en20_mv00(RECVX_ENEMY_WORK* w)
{
    uint32_t rnd;

    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 0u) {
            w->mtn_no = 0;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = (en20_rand(w) % 10) + 10;    /* :109 */
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {                  /* :114, Postdekrement */
            w->mode0 = 1;
            w->mode1 = 0;

            rnd = (uint32_t)(en20_rand(w) % 6);   /* :119 */
            if (rnd == 0u)      w->mode2 = 6;
            else if (rnd == 1u) w->mode2 = 7;
            else                w->mode2 = 0;

            w->mode3 = 0;
        }
    }
}

/* en20.c:140 bhEne20_MV01 — Laufen, Motion 7. */
static void en20_mv01(RECVX_ENEMY_WORK* w)
{
    uint32_t rnd;

    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 7u) {
            w->mtn_no = 7;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = (en20_rand(w) % 10) + 10;    /* :155 */
        w->spd = 0.0f;
        w->mode3++;
        /* fall through */
    case 1:
        en20_add_speed(w, 0);                 /* :161 */

        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 0;

            rnd = (uint32_t)(en20_rand(w) % 10);  /* :168 */
            if (rnd < 2u) {
                w->mode2 = 0;
            } else {
                if (rnd == 2u)      w->mode2 = 3;
                else if (rnd == 3u) w->mode2 = 2;
                else                w->mode2 = 1;
            }

            w->mode3 = 0;
        }
    }
}

/* en20.c:196 bhEne20_MV02 — kurzer Laufabschnitt, danach immer MV01. */
static void en20_mv02(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 7u) {
            w->mtn_no = 7;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = (en20_rand(w) % 10) + 0xA;   /* :209 */
        w->spd = 0.0f;
        w->mode3++;
        /* fall through */
    case 1:
        en20_add_speed(w, 0);

        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 0;
            w->mode2 = 1;
            w->mode3 = 0;
        }
    }
}

/* en20.c:228 bhEne20_MV03 — gleicher Koerper wie MV02; im Original eine
   eigene Funktion und ein eigener Tabelleneintrag. */
static void en20_mv03(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 7u) {
            w->mtn_no = 7;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = (en20_rand(w) % 10) + 0xA;   /* :241 */
        w->spd = 0.0f;
        w->mode3++;
        /* fall through */
    case 1:
        en20_add_speed(w, 0);

        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 0;
            w->mode2 = 1;
            w->mode3 = 0;
        }
    }
}

/* en20.c:260 bhEne20_MV04 — Motion 5 einmal abspielen, danach MV01. Kein
   Zustand dieser Identitaet setzt mode2 auf 4; der Zustand ist nur ueber
   einen fremden Setzer erreichbar. */
static void en20_mv04(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 5u) {
            w->mtn_no = 5;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = en20_frm_num(w);             /* :273 */
        w->spd = 0.0f;
        w->mode3++;
        /* fall through */
    case 1:
        en20_add_speed(w, 0);

        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 0;
            w->mode2 = 1;
            w->mode3 = 0;
        }
    }
}

/* en20.c:292 bhEne20_MV05 — wie MV04, aber Motion 6. Ebenfalls nur ueber
   einen fremden Setzer erreichbar. */
static void en20_mv05(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 6u) {
            w->mtn_no = 6;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = en20_frm_num(w);             /* :305 */
        w->spd = 0.0f;
        w->mode3++;
        /* fall through */
    case 1:
        en20_add_speed(w, 0);

        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 0;
            w->mode2 = 1;
            w->mode3 = 0;
        }
    }
}

/* en20.c:324 bhEne20_MV06 — Motionfolge 3 -> 4 (vierte Motion doppelt so
   lang), danach MV00. case 1 hat hier ein break. */
static void en20_mv06(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 3u) {
            w->mtn_no = 3;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = en20_frm_num(w);             /* :337 */
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            w->mtn_no = 4;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
            w->ct0 = en20_frm_num(w) * 2;     /* :348 */
            w->mode3++;
        }
        break;

    case 2:
        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 0;
            w->mode2 = 0;
            w->mode3 = 0;
        }
    }
}

/* en20.c:366 bhEne20_MV07 — Motionfolge 2 -> 1, danach MV00. */
static void en20_mv07(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 2u) {
            w->mtn_no = 2;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
        }
        w->ct0 = en20_frm_num(w);             /* :379 */
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            w->mtn_no = 1;
            w->frm_no = 0;
            w->hokan_count = 3;
            w->hokan_rate = 0x8000;
            w->ct0 = en20_frm_num(w);         /* :390 */
            w->mode3++;
        }
        break;

    case 2:
        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 0;
            w->mode2 = 0;
            w->mode3 = 0;
        }
    }
}

/* en20.c:408 — leer (Originalsignatur `void bhEne20_Nage(void)`). */
static void en20_nage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en20.c:414 — leer. */
static void en20_damage(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en20.c:420 — leer. */
static void en20_die(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en20.c:17 Mode0-Index 5 — bhEne_Event (subpl.c:143) = subpl_mv01_tbl[ct2].
   Diesen Zustand besitzt der Begleitfiguren-Mechanismus; der Vertragsdienst
   `event` (enemy_work.h:489-495) wird gerufen, sobald er gebunden ist. */
static void en20_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    if (w->sv && w->sv->gap) w->sv->gap("bhEne_Event(subpl_mv01_tbl)");
}
