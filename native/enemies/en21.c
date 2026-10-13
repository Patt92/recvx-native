/* Identitaet 21 — Albinoid Infant. Nativ eigenstaendig bestimmt.
   Semantikreferenz: en21.c (siehe en21.h). Kein Originalkoerper, kein
   Generator, keine zweite Workwelt. */
#include "en21.h"
#include "en03_lib.h"
#include "../core_random.h"
#include "../ninja_math.h"
#include <math.h>
#include <stdlib.h>
#include <stddef.h>

_Static_assert(sizeof(RECVX_EN21_EXP0) == 0x18u, "EXP0-Block ist 0x18 Byte (en21.c:159)");
_Static_assert(offsetof(RECVX_EN21_EXP0, player_ang) == 0x4u, "EXP0_I(0x4)");
_Static_assert(offsetof(RECVX_EN21_EXP0, target_x) == 0x8u, "EXP0_F(0x8)");
_Static_assert(offsetof(RECVX_EN21_EXP0, target_z) == 0x10u, "EXP0_F(0x10)");
_Static_assert(offsetof(RECVX_EN21_EXP0, in_view) == 0x14u, "EXP0_UC(0x14)");
_Static_assert(offsetof(RECVX_EN21_EXP0, grab_wait) == 0x15u, "EXP0_UC(0x15)");
_Static_assert(offsetof(RECVX_EN21_EXP0, death_mtn) == 0x16u, "EXP0_UC(0x16)");
_Static_assert(offsetof(RECVX_EN21_EXP0, controlling) == 0x17u, "EXP0_UC(0x17)");

/* macros.h:104 NJM_DEG_ANG(n) = (Angle)((n * 65536.0f) / 360.0f). */
#define EN21_DEG_ANG(n) ((int32_t)(((n) * 65536.0f) / 360.0f))

static const RECVX_EN21_SERVICES* en21_services;
void recvx_en21_bind(const RECVX_EN21_SERVICES* sv) { en21_services = sv; }

/* en21.c:17 BloodParam. */
static const RECVX_ENEMY_BP_WORK en21_blood_param = {
    {0.0f, 0.1f, 0.0f}, 0, 0.0f, 0.1f, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, {0, 0, 0, 0, 0}
};

/* en21.c:26 En21SdwTab. */
static const int8_t en21_sdw_tab[2] = {1, -1};

/* en21.c:28 CapColTab[9] (CPCL: jnt_a, jnt_b, cap_r). */
static const RECVX_ENEMY_CPCL en21_cap_col_tab[9] = {
    {1, 4, 10}, {2, 2, 8}, {-1, 2, 3}, {3, 3, 8}, {1, 2, 3},
    {5, 10, 5}, {10, 12, 5}, {12, 14, 4}, {0, 0, 0}
};

/* en21.c:40 En21SpdAddTbl[19]. */
static const float en21_spd_add_tbl[19] = {
    -0.2f, -0.25f, -0.25f, -0.2f,
    -0.15f, -0.1f, -0.0f, -0.1f,
    -0.15f, -0.2f, -0.25f, -0.25f,
    -0.2f, -0.15f, -0.1f, -0.0f,
    -0.0f, -0.1f, -0.15f
};

/* en21.c:48 En21SparkTbl[15]. NG00 ruft SetSparkEff in jedem Durchlauf mit
   ofy = ct0 auf, auch in dem Durchlauf mit ct0 == 15 (:781 liegt vor dem
   Test :804); der Index 15 liest im Original das Fuellbyte hinter der
   15-Byte-Tabelle. Gemessen in SLUS_201.84 (Dateioffset 0x21CA30..0x21CA3F:
   11 15 10 14 0F 13 0E 12 01 02 03 06 0A 04 05 00) ist es 0. */
static const unsigned char en21_spark_tbl[16] = {
    17, 21, 16, 20, 15,
    19, 14, 18, 1, 2,
    3, 6, 10, 4, 5, 0
};

static void en21_gap(RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
    else recvx_t3_gap(name);
}

static int32_t en21_rand(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->random_next) return sv->random_next();
    return recvx_core_random_next();
}

static RECVX_EN21_EXP0* en21_exp0(RECVX_ENEMY_WORK* w)
{
    return (RECVX_EN21_EXP0*)w->exp0;
}

static int en21_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->player_get && sv->player_get(out)) return 1;
    en21_gap(w, "player_get (Spielerbesitzer)");
    return 0;
}

static void en21_player_put(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* in)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->player_set) sv->player_set(in);
    else en21_gap(w, "player_set (Spielerbesitzer)");
}

/* zonzon1.c:828 bhEne_CallSE(epw, &epw->px, se). */
static void en21_call_se(RECVX_ENEMY_WORK* w, int se)
{
    RECVX_ENEMY_VEC pos;
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    recvx_t3_call_se(w, &pos, se);
}

/* pwksub.c:133 bhAddSpeed(pp, 0). */
static void en21_add_speed(RECVX_ENEMY_WORK* w)
{
    const int32_t angle = w->ay & 0xFFFF;
    w->px -= w->spd * njSin(angle);
    w->pz -= w->spd * njCos(angle);
}

/* sys->ef_trs-Einreihung gehoert dem Effektbesitzer; hier nur die Frames. */
static unsigned en21_frame_count(RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->motion_frames) {
        const unsigned n = sv->motion_frames(w, mtn);
        if (n) return n;
    }
    en21_gap(w, "mnwP[mtn_no].frm_num");
    return 0;
}

static void en21_init(RECVX_ENEMY_WORK* w);
static void en21_move(RECVX_ENEMY_WORK* w);
static void en21_nage(RECVX_ENEMY_WORK* w);
static void en21_damage(RECVX_ENEMY_WORK* w);
static void en21_die(RECVX_ENEMY_WORK* w);
static void en21_event(RECVX_ENEMY_WORK* w);
static void en21_mv00(RECVX_ENEMY_WORK* w);
static void en21_mv01(RECVX_ENEMY_WORK* w);
static void en21_mv02(RECVX_ENEMY_WORK* w);
static void en21_mv03(RECVX_ENEMY_WORK* w);
static void en21_mv04(RECVX_ENEMY_WORK* w);
static void en21_mv05(RECVX_ENEMY_WORK* w);
static void en21_mv06(RECVX_ENEMY_WORK* w);
static void en21_mv07(RECVX_ENEMY_WORK* w);
static void en21_mv08(RECVX_ENEMY_WORK* w);
static void en21_player_control(RECVX_ENEMY_WORK* w);
static void en21_set_mark_eff(RECVX_ENEMY_WORK* w);
static void en21_set_spark_eff(RECVX_ENEMY_WORK* w, int num, int flg, uint32_t ofy);

typedef void (*RECVX_EN21_PROC)(RECVX_ENEMY_WORK*);

/* en21.c:54 bhEne21_Mode0[6]. */
static const RECVX_EN21_PROC en21_mode0[6] = {
    en21_init, en21_move, en21_nage, en21_damage, en21_die, en21_event
};
/* en21.c:67 bhEne21_MoveMode2[9]. */
static const RECVX_EN21_PROC en21_move_mode2[9] = {
    en21_mv00, en21_mv01, en21_mv02, en21_mv03, en21_mv04,
    en21_mv05, en21_mv06, en21_mv07, en21_mv08
};

/* en21.c:88 bhEne21. */
void recvx_en21(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_EN21_EXP0* e;
    float mtx[16];
    int i;

    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u) en21_mode0[w->mode0](w);
    else en21_gap(w, "bhEne21_Mode0");

    if (sv && sv->set_motion) sv->set_motion(w, w->mtn_add, w->mtn_md);   /* :95 */
    else en21_gap(w, "bhSetMotion");

    if ((w->mode0 == 5u) && (w->type == 2u) && (en21_rand(w) % 3 == 0)) {   /* :97 */
        en21_set_mark_eff(w);
    }

    if (w->flg & 0x4u) {                                                    /* :102 */
        for (i = 0; i < (int)RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;
        w->flg &= ~0x4u;
    }

    if ((w->flg & 0x8u) && (w->mode0 != 5u)) {                              /* :112 */
        if (w->flg & 0x10u) {
            if (sv && sv->check_wall) sv->check_wall(w);
            else en21_gap(w, "bhCheckWall");
        }
        if (w->flg & 0x40u) {
            if (sv && sv->check_player) sv->check_player(w);
            else en21_gap(w, "bhCheckPlayer");
        }
        recvx_t3_check_enemies_ball(w);                                     /* :124 */
    }

    if (sv && sv->calc_model) sv->calc_model(w);                            /* :127 */
    else en21_gap(w, "bhCalcModel");

    if (w->flg & 0x20u) {                                                   /* :129 */
        if (sv && sv->joint_matrix && sv->joint_matrix(w, 1u, mtx)) {
            w->watr.c1.x = mtx[12]; w->watr.c1.y = mtx[13]; w->watr.c1.z = mtx[14];
        } else en21_gap(w, "bhEne21:mlwP->owP[1].mtx");
        if (sv && sv->joint_matrix && sv->joint_matrix(w, 6u, mtx)) {
            w->watr.c2.x = mtx[12]; w->watr.c2.y = mtx[13]; w->watr.c2.z = mtx[14];
        } else en21_gap(w, "bhEne21:mlwP->owP[6].mtx");
        w->watr.r = 1.2f;
    }

    e = en21_exp0(w);
    if (!e) {
        /* Das Original liest EXP0_UC(0x17)/(0x15) hier ungeschuetzt. */
        en21_gap(w, "EXP0-Block (en21.c:143)");
        return;
    }
    if (e->controlling == 1u) en21_player_control(w);                       /* :143 */
    if (e->grab_wait != 0u) e->grab_wait--;                                 /* :148 */
}

/* en21.c:155 bhEne21_Init. */
static void en21_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN21_EXP0* e;

    if (w->exp0 == NULL) {
        if (sv && sv->calloc_work) w->exp0 = sv->calloc_work(0x18u);
        else en21_gap(w, "bhEne_CallocWork(0x18, 8)");
    }
    e = en21_exp0(w);

    if (w->type == 1u) {
        w->flg |= 0x8000u;
        w->flg &= ~0x178u;
        w->stflg |= 0x8u;
        w->flg2 &= ~1u;
        w->flg &= ~0x6u;
        w->ar = 0.0f;
        w->ah = 0.0f;
        w->car = 0.0f;
        w->cah = 0.0f;
        w->hp = 80;

        if (e) {
            e->in_view = 0;
            e->player_dist = 999.0f;
            e->player_ang = 0;
            e->target_x = 0; e->target_y = 0; e->target_z = 0;
            e->grab_wait = 0;
            e->death_mtn = 4;
            e->controlling = 0;
        } else en21_gap(w, "EXP0-Block (en21.c:175)");

        w->mtn_no = 7;
        w->frm_no = 0;
        w->hokan_count = 0;
        w->hokan_rate = 0;
        w->mtn_add = 0;
        w->mtn_md = 0;
        w->mode0 = 1;
        w->mode1 = 0;
        w->mode2 = 7;
        w->mode3 = 0;
        return;
    }

    w->flg |= 0x178u;
    w->flg2 |= 0x40u;
    w->stflg &= ~0x8u;
    w->flg &= ~0x8006u;
    w->ar = 4.0f;
    w->ah = 1.0f;
    w->aox = 0.0f;
    w->aoy = 0.0f;
    w->aoz = 0.8f;
    w->car = 2.0f;
    w->cah = 1.0f;

    {
        int gm = 0;
        const RECVX_EN21_SERVICES* x = en21_services;
        if (x && x->game_mode && x->game_mode(&gm)) {
            w->hp = (gm == 0) ? 80 : 50;                                    /* :210-217 */
        } else {
            en21_gap(w, "sys->gm_mode (en21.c:210)");
        }
    }

    if (e) {
        e->in_view = 0;
        e->player_dist = 999.0f;
        e->player_ang = 0;
        e->target_x = 0; e->target_y = 0; e->target_z = 0;
        e->grab_wait = 0;
        e->death_mtn = 4;
        e->controlling = 0;
    } else en21_gap(w, "EXP0-Block (en21.c:219)");

    w->lok_jno = 4;
    w->mtn_no = 7;
    /* :231 schreibt rand() % 19 ROH in frm_no (nicht 16.16); die spaeteren
       Lesestellen teilen durch 65536 und sehen daher bis zum ersten
       Motionvorschub 0. */
    w->frm_no = en21_rand(w) % 19;
    w->hokan_count = 0;
    w->hokan_rate = 65536;
    w->mtn_add = 65536;
    w->mtn_md = 0;
    w->mode0 = 1;
    w->mode1 = 1;
    w->mode2 = 1;
    w->mode3 = 0;
    w->cpcl = en21_cap_col_tab;
    w->clp_jno[0] = 0;
    w->clp_jno[1] = -1;
    w->mdflg &= ~0x20u;

    if (w->flg & 0x800u) return;                                            /* :245 */

    if (sv && sv->set_shadow) sv->set_shadow(en21_sdw_tab, w, 4, 3.0f, 1.0f, 4.0f);
    else en21_gap(w, "bhSetShadow");
    w->flg |= 0x800u;
}

/* en21.c:256 bhEne21_SearchPlayer. */
static void en21_search_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* ply)
{
    RECVX_EN21_EXP0* e = en21_exp0(w);
    float dx, dz;
    int32_t ang;

    dx = w->px - ply->pos.x;
    dz = w->pz - ply->pos.z;
    e->player_dist = njSqrt(dx * dx + dz * dz);
    e->target_x = ply->pos.x;
    e->target_y = ply->pos.y;
    e->target_z = ply->pos.z;

    ang = (int32_t)(10430.381f * atan2f(dx, dz));
    e->player_ang = (int16_t)(ang - w->ay);
    ang = abs(e->player_ang);
    e->in_view = (ang < EN21_DEG_ANG(100.0f)) ? 1u : 0u;
}

/* en21.c:288 bhEne21_BR00 (:282 Brain ruft nur diese). */
static void en21_brain(RECVX_ENEMY_WORK* w)
{
    RECVX_EN21_EXP0* e = en21_exp0(w);
    RECVX_ENEMY_PLAYER ply;

    if (!e) { en21_gap(w, "EXP0-Block (en21.c:297)"); return; }
    if (!en21_player(w, &ply)) return;

    /* :290; debug_attack_disabled ist der Host-Schalter "Gegner-Angriff" und
       wird wie bei en01/en04/en10 gleich flg & 2 behandelt. */
    if ((ply.stflg & 0x80000000u) || (ply.debug_attack_disabled || (ply.flg & 0x2u))
        || (ply.flg & 0x4u) || (w->flg & 0x4u)) {
        return;
    }

    en21_search_player(w, &ply);

    if ((e->in_view != 0u) && (e->grab_wait == 0u) && (e->player_dist < 5.5f)) {
        if (abs(e->player_ang) < EN21_DEG_ANG(5.0f)) {
            w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
            en21_nage(w);
        } else if (e->player_ang > 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 5; w->mode3 = 0;
        } else {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 4; w->mode3 = 0;
        }
    }
}

/* en21.c:325 bhEne21_Move. */
static void en21_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1u) en21_brain(w);

    if (((w->flg & 0x4u) != 0u) && ((w->flg & 0x2u) == 0u)) en21_damage(w);

    if (w->mode0 == 1u) {
        if (w->mode2 < 9u) en21_move_mode2[w->mode2](w);
        else en21_gap(w, "bhEne21_MoveMode2");
    }
}

/* Gemeinsamer Kopf der Laufzustaende MV00..MV05 (:349-356 usw.): bei
   mtn_no != 7 wird die Motion 7 mit zufaelligem Startbild gestartet. */
static void en21_start_walk_motion(RECVX_ENEMY_WORK* w)
{
    if (w->mtn_no != 7u) {
        w->mtn_no = 7;
        w->frm_no = en21_rand(w) % 19;
        w->hokan_count = 3;
        w->hokan_rate = 32768;
    }
}

/* Schrittweite aus En21SpdAddTbl[frm_no / 65536]. */
static float en21_spd_add(RECVX_ENEMY_WORK* w)
{
    const int32_t idx = w->frm_no / 65536;
    if (idx < 0 || idx >= 19) {
        en21_gap(w, "En21SpdAddTbl[frm_no / 65536] ausserhalb");
        return 0.0f;
    }
    return en21_spd_add_tbl[idx];
}

/* en21.c:344 bhEne21_MV00. ORIGINALER DURCHFALL case 0 -> case 1. */
static void en21_mv00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        en21_start_walk_motion(w);
        w->ct0 = (en21_rand(w) % 10) + 10;
        w->spd = 0.05f;
        w->mode3++;
        /* fall through */
    case 1:
        en21_add_speed(w);

        if (w->ct0 == 8) {
            en21_set_spark_eff(w, 1, 3, (uint32_t)(w->frm_no / 65536));
            en21_call_se(w, 0x12301);
        }

        if (w->ct0-- == 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
        }
        break;
    }
}

/* en21.c:380 bhEne21_MV01 — Verfolgung. ORIGINALER DURCHFALL. */
static void en21_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN21_EXP0* e = en21_exp0(w);
    RECVX_ENEMY_PLAYER ply;
    RECVX_ENEMY_VEC pos, from, to;
    uint32_t rnd;

    switch (w->mode3) {
    case 0:
        en21_start_walk_motion(w);
        w->ct0 = (en21_rand(w) % 10) + 10;
        w->spd = 0.5f;
        w->mode3++;
        /* fall through */
    case 1:
        if (!e) { en21_gap(w, "EXP0-Block (en21.c:415)"); return; }

        w->spd = 0.5f;
        w->spd += en21_spd_add(w);
        en21_add_speed(w);

        if (w->ct0 == 17) en21_call_se(w, 0x12300);

        if ((en21_rand(w) % 3) == 0) en21_set_mark_eff(w);

        if (e->in_view != 0u) {
            const RECVX_EN21_SERVICES* x = en21_services;
            if (!en21_player(w, &ply)) return;
            from.x = w->px; from.y = w->py; from.z = w->pz;
            to = ply.pos;
            if (x && x->check_route) {
                if (x->check_route(&from, &to, &pos) != 0xFF) {            /* :417 */
                    e->target_x = pos.x;
                    e->target_z = pos.z;
                } else {
                    e->target_x = ply.pos.x;
                    e->target_z = ply.pos.z;
                }
                w->ay += recvx_t3_dir_target(w, e->target_x, e->target_z,
                                             EN21_DEG_ANG(4.0f));
            } else {
                en21_gap(w, "bhCheckRoute (rutchk.c:5)");
            }
        } else {
            w->mode0 = 1;
            w->mode1 = 1;
            w->mode2 = (en21_rand(w) % 2) ? 3 : 2;
            w->mode3 = 0;
            return;                                                         /* :435 */
        }

        if (w->ct0-- == 0) {
            w->mode0 = 1;
            w->mode1 = 1;
            rnd = (uint32_t)(en21_rand(w) % 10);
            if (rnd == 0) w->mode2 = 0;
            else if (rnd == 1) w->mode2 = 3;
            else if (rnd == 2) w->mode2 = 2;
            else w->mode2 = 1;
            w->mode3 = 0;
        }
        break;
    }
}

/* en21.c:466 bhEne21_MV02 — Linkskurve. */
static void en21_mv02(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        en21_start_walk_motion(w);
        w->ct0 = (en21_rand(w) % 10) + 10;
        w->spd = 0.5f;
        w->mode3++;
        /* fall through */
    case 1:
        w->ay -= EN21_DEG_ANG(4.0f);
        w->spd = 0.5f;
        w->spd += en21_spd_add(w);
        en21_add_speed(w);

        if (w->ct0 == 17) en21_call_se(w, 0x12300);

        if (en21_rand(w) % 3 == 0) en21_set_mark_eff(w);

        if (w->ct0-- == 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
        }
        break;
    }
}

/* en21.c:510 bhEne21_MV03 — Rechtskurve. */
static void en21_mv03(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        en21_start_walk_motion(w);
        w->ct0 = (en21_rand(w) % 10) + 10;
        w->spd = 0.5f;
        w->mode3++;
        /* fall through */
    case 1:
        w->ay += EN21_DEG_ANG(4.0f);
        w->spd = 0.5f;
        w->spd += en21_spd_add(w);
        en21_add_speed(w);

        if (w->ct0 == 17) en21_call_se(w, 0x12300);

        if (en21_rand(w) % 3 == 0) en21_set_mark_eff(w);

        if (w->ct0-- == 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
        }
        break;
    }
}

/* en21.c:554 bhEne21_MV04 — schnelle Linksdrehung ohne Schrittweitentabelle. */
static void en21_mv04(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        en21_start_walk_motion(w);
        w->ct0 = (en21_rand(w) % 10) + 10;
        w->spd = 0.05f;
        w->mode3++;
        /* fall through */
    case 1:
        w->ay -= EN21_DEG_ANG(7.0f);
        en21_add_speed(w);

        if (w->ct0 == 17) en21_call_se(w, 0x12300);

        if (w->ct0-- == 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
        }
        break;
    }
}

/* en21.c:591 bhEne21_MV05 — schnelle Rechtsdrehung. */
static void en21_mv05(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        en21_start_walk_motion(w);
        w->ct0 = (en21_rand(w) % 10) + 10;
        w->spd = 0.05f;
        w->mode3++;
        /* fall through */
    case 1:
        w->ay += EN21_DEG_ANG(7.0f);
        en21_add_speed(w);

        if (w->ct0 == 17) en21_call_se(w, 0x12300);

        if (w->ct0-- == 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
        }
        break;
    }
}

/* en21.c:1021 bhEne21_AllWayWallCheck. */
unsigned char recvx_en21_all_way_wall_check(RECVX_ENEMY_WORK* w, float dist)
{
    static const int32_t offs[4] = {0, -16384, 16384, 32768};   /* 0, -90, +90, 180 Grad */
    static const unsigned char bits[4] = {0x1, 0x2, 0x4, 0x8};
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    unsigned char ret = 0;
    int k;
    float i;

    if (!sv || !sv->wall_type) {
        en21_gap(w, "bhCheckWallType");
        return 0;
    }

    for (k = 0; k < 4; ++k) {
        const int32_t ang = (uint16_t)(w->ay + offs[k]);
        for (i = 1.0f; i <= dist; i = i + 2.0f) {
            RECVX_ENEMY_VEC pos;
            pos.x = w->px - i * njSin(ang);
            pos.z = w->pz - i * njCos(ang);
            pos.y = w->py;
            if (sv->wall_type(&pos, w->flg, w->ar, w->ah)) {
                ret |= bits[k];
                break;
            }
        }
    }
    return ret;
}

/* MV06: Kehrtwende/Quartkreis mit Wandabfrage (en21.c:628). */
static void en21_mv06(RECVX_ENEMY_WORK* w)
{
    unsigned char wall;
    unsigned frames;

    switch (w->mode3) {
    case 0:
        wall = recvx_en21_all_way_wall_check(w, 3.0f);
        if ((wall & 0x8u) == 0u) {
            w->mtn_no = (en21_rand(w) % 2) ? 2 : 1;
            w->mode3 = 2;
        } else {
            w->mode3 = 1;

            if (((wall & 0x2u) == 0u) && ((wall & 0x4u) == 0u)) {
                w->mtn_no = (en21_rand(w) % 2) ? 2 : 1;
            } else if ((wall & 0x2u) == 0u) {
                w->mtn_no = 1;
            } else if ((wall & 0x4u) == 0u) {
                w->mtn_no = 2;
            } else {
                w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
                break;
            }
        }
        frames = en21_frame_count(w, w->mtn_no);
        w->ct0 = (int32_t)frames;
        w->frm_no = 0;
        w->hokan_count = 3;
        w->hokan_rate = 32768;
        break;

    case 1:
    case 2: {
        /* :672 und :696 sind bis auf den Drehwinkel (90 bzw. 180 Grad) gleich. */
        const int32_t total = (w->mode3 == 1u) ? EN21_DEG_ANG(90.0f) : EN21_DEG_ANG(180.0f);
        frames = en21_frame_count(w, w->mtn_no);
        if (frames != 0u) {
            if (w->mtn_no == 1u) w->ay -= (int32_t)((uint32_t)total / frames);
            else w->ay += (int32_t)((uint32_t)total / frames);
        }

        if (w->ct0 == 17) en21_call_se(w, 0x12300);

        if (w->ct0-- == 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
        }
        break;
    }
    }
}

/* en21.c:723 bhEne21_MV07 — leer. */
static void en21_mv07(RECVX_ENEMY_WORK* w) { (void)w; }

/* en21.c:729 bhEne21_MV08. */
static void en21_mv08(RECVX_ENEMY_WORK* w)
{
    w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
}

/* en21.c:738 bhEne21_Nage -> NageMode2[1] = { NG00 }. */
static void en21_ng00(RECVX_ENEMY_WORK* w);
static void en21_nage(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 == 0u) en21_ng00(w);
    else en21_gap(w, "bhEne21_NageMode2");
}

/* en21.c:744 bhEne21_NG00 — Griff. */
static void en21_ng00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const RECVX_EN21_SERVICES* x = en21_services;
    RECVX_EN21_EXP0* e = en21_exp0(w);
    RECVX_ENEMY_PLAYER ply;
    uint32_t i, n;

    w->ct0++;

    switch (w->mode3) {
    case 0:
        if (!e) { en21_gap(w, "EXP0-Block (en21.c:754)"); return; }
        if (!en21_player(w, &ply)) return;

        e->controlling = 1;
        ply.flg |= 0x10004u;
        ply.stflg |= 0x10000u;
        ply.mode0 = 4;
        ply.mode1 = 0;
        if (abs((int16_t)(w->ay - ply.ay)) < EN21_DEG_ANG(90.0f)) {
            ply.mode2 = 1;
            ply.ayp = w->ay;
        } else {
            ply.mode2 = 0;
            ply.ayp = (int16_t)(w->ay + EN21_DEG_ANG(180.0f));
        }
        ply.mode3 = 0;
        en21_player_put(w, &ply);

        w->mtn_no = 7;
        w->frm_no = 0;
        w->hokan_count = 3;
        w->hokan_rate = 32768;
        w->ct0 = 0;
        en21_call_se(w, 0x12301);
        w->mode3++;
        /* fall through */
    case 1:
        if (((w->frm_no / 65536) % 2) == 0) {
            en21_set_spark_eff(w, 1, 1, (uint32_t)w->ct0);
        }

        if (w->ct0 == 4) {
            float mtx[16];
            RECVX_ENEMY_VEC dp;

            en21_call_se(w, 0x12301);
            if (!en21_player(w, &ply)) return;
            ply.hp -= 9;
            if (ply.hp < 0) {
                /* :788 — der Abzug bleibt im Spielerwork stehen. */
                en21_player_put(w, &ply);
                w->mode3 = 3;
                break;
            }
            ply.mode0 = 4;
            ply.mode1 = 1;
            ply.mode3 = 0;
            en21_player_put(w, &ply);

            /* plp->djnt_no = 4, plp->way, plp->dpx..dpz (:795-799). */
            if (sv && sv->joint_matrix && sv->joint_matrix(w, 0u, mtx)) {
                const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
                dp.x = mtx[12]; dp.y = mtx[13]; dp.z = mtx[14];
                if (ext && ext->player_hit_set) ext->player_hit_set(4, NULL, &dp);
                else en21_gap(w, "plp->djnt_no/dpx (player_hit_set)");
            } else {
                en21_gap(w, "bhEne21_NG00:mlwP->owP[0].mtx");
            }
            if (x && x->player_set_way) x->player_set_way((int16_t)w->ay);
            else en21_gap(w, "plp->way (en21.c:796)");

            recvx_t3_call_player_voice(0x402);                              /* :800 */
            if (sv && sv->vibration) sv->vibration(1, 10);                  /* :801 */
            else en21_gap(w, "StartVibrationEx(1, 10)");
        }

        if (w->ct0 == 15) w->mode3++;
        break;

    case 2:
        w->mode0 = 1;
        w->mode1 = 1;
        w->mode2 = 6;
        w->mode3 = 0;

        /* :815-822 — jeder lebende Work der Identitaet 21. */
        if (sv && sv->enemy_count && sv->enemy_at) {
            n = sv->enemy_count();
            for (i = 0; i < n; ++i) {
                RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
                if (ep && (ep->flg & 0x1u) && ep->id == 21u) {
                    RECVX_EN21_EXP0* ee = en21_exp0(ep);
                    if (ee) ee->grab_wait = 35;
                    else en21_gap(ep, "EXP0-Block (en21.c:820)");
                }
            }
        } else en21_gap(w, "ene-Liste/sys->ewk_n (en21.c:815)");

        if (en21_player(w, &ply)) {
            ply.mode0 = 4;
            ply.mode1 = 2;
            ply.mode2 = 0;
            ply.mode3 = 0;
            en21_player_put(w, &ply);
        }
        break;

    case 3:
        w->ct0 = 0;
        w->mode0 = 1;
        w->mode1 = 0;
        w->mode2 = 0;
        w->mode3 = 0;
        break;
    }
}

/* en21.c:840 bhEne21_Damage. */
static void en21_damage(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    uint32_t i, dam = 0, count = 0;

    if (sv && sv->motion_obj_num) count = sv->motion_obj_num(w, 0u);        /* mnwP->obj_num */
    if (count == 0u) en21_gap(w, "mnwP->obj_num");
    if (count > RECVX_ENEMY_DAM) count = RECVX_ENEMY_DAM;

    for (i = 0; i < count; ++i) {
        if (w->dam[i] != 0) {
            dam += (uint32_t)w->dam[i];
            w->dam[i] = 0;
        }
    }

    if (dam != 0u) {
        RECVX_ENEMY_VEC pos;
        pos.x = 0.0f; pos.y = 0.4f; pos.z = 0.0f;
        recvx_t3_set_blood_effect2(w, 0, 4, &pos);
        en21_call_se(w, 0x2302);
    }

    /* :864 vergleicht int gegen unsigned: ein negatives hp ist hier gross. */
    if ((uint32_t)w->hp < dam) {
        w->mode0 = 4; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
    } else {
        w->hp -= (int32_t)dam;
        w->spd = 0.7f;
    }
}

/* en21.c:885 bhEne21_DD00. ORIGINALER DURCHFALL case 0 -> case 1. */
static void en21_dd00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN21_EXP0* e = en21_exp0(w);
    uint32_t i, n;
    unsigned frames;

    switch (w->mode3) {
    case 0:
        if (!e) { en21_gap(w, "EXP0-Block (en21.c:894)"); return; }
        w->flg &= ~0x8u;
        w->mtn_no = e->death_mtn;
        w->frm_no = 0;
        w->hokan_count = 3;
        w->hokan_rate = 32768;
        frames = en21_frame_count(w, w->mtn_no);
        w->ct0 = (int32_t)frames - 1;
        e->death_mtn++;
        if (e->death_mtn > 6u) e->death_mtn = 4;
        if (sv && sv->enemy_count && sv->enemy_at) {
            n = sv->enemy_count();
            for (i = 0; i < n; ++i) {
                RECVX_ENEMY_WORK* ep = sv->enemy_at(i);
                if (ep && (ep->flg & 0x1u) && ep->id == 21u) {
                    RECVX_EN21_EXP0* ee = en21_exp0(ep);
                    if (ee) ee->death_mtn = e->death_mtn;
                    else en21_gap(ep, "EXP0-Block (en21.c:909)");
                }
            }
        } else en21_gap(w, "ene-Liste/sys->ewk_n (en21.c:905)");
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            RECVX_ENEMY_VEC pos;
            w->mtn_add = 0;
            pos.x = w->px; pos.y = w->py; pos.z = w->pz;
            if (sv && sv->blood_pool) sv->blood_pool(w, &pos, w->ay, &en21_blood_param);
            else en21_gap(w, "bhEne_BloodPool");
            w->mode3++;
        }
        break;

    case 2:
        w->hp = -1;
        w->flg |= 0x2u;
        break;
    }
}

/* en21.c:879 bhEne21_Die -> DieMode2[1] = { DD00 }. */
static void en21_die(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 == 0u) en21_dd00(w);
    else en21_gap(w, "bhEne21_DieMode2");
}

/* en21.c:19 Mode0-Index 5 — bhEne_Event (subpl.c:143). */
static void en21_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) { w->sv->event(w); return; }
    en21_gap(w, "bhEne_Event(subpl_mv01_tbl)");
}

/* en21.c:931 bhEne21_PlayerControl. */
static void en21_player_control(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN21_EXP0* e = en21_exp0(w);
    RECVX_ENEMY_PLAYER ply;
    int dirty = 0;
    uint32_t frames;

    if (!en21_player(w, &ply)) return;

    switch (ply.mode1) {
    case 0:
        if (ply.mode3 != 0u) break;

        ply.flg &= ~0x40000u;
        ply.flg |= 0x10004u;
        ply.flg2 |= 0x1u;
        ply.stflg |= 0x10000u;
        if (ply.hp < 30) ply.mtn_no = 44;
        else if (ply.hp < 120) ply.mtn_no = 43;
        else ply.mtn_no = 42;
        ply.frm_no = 0;
        ply.hokan_count = 5;
        ply.hokan_rate = 32768;
        ply.mode3++;
        dirty = 1;
        break;

    case 1:
        switch (ply.mode3) {
        case 0:
            ply.mtn_no = (ply.mode2 == 0u) ? 74u : 73u;
            ply.frm_no = 0;
            ply.hokan_count = 3;
            ply.hokan_rate = 32768;
            if (!sv || !sv->player_motion_frames
                || (frames = sv->player_motion_frames(ply.mtn_no, 0)) == 0u) {
                en21_gap(w, "plp->mnwP[plp->mtn_no].frm_num");
                break;
            }
            ply.ct0 = (int32_t)frames;
            ply.ct1 = (int32_t)frames - 3;
            ply.mode3++;
            dirty = 1;
            /* fall through */
        case 1:
            if (ply.ct1 > 0) {
                ply.ay += (int16_t)(ply.ayp - ply.ay) / ply.ct1;
                dirty = 1;
            }

            if (ply.ct0-- == 0) {
                if (ply.hp < 30) ply.mtn_no = 44;
                else if (ply.hp < 120) ply.mtn_no = 43;
                else ply.mtn_no = 42;
                ply.frm_no = 0;
                ply.hokan_count = 4;
                ply.hokan_rate = 32768;
                ply.mode3++;
            }
            dirty = 1;
            break;
        }
        break;

    case 2:
        ply.flg &= ~0x10004u;
        ply.flg2 &= ~0x1u;
        ply.flg |= 0x8u;
        ply.stflg &= ~0x10000u;
        ply.at_flg = 0;
        ply.mode0 = 1;
        if (e) e->controlling = 0;
        dirty = 1;
        break;
    }

    if (dirty) en21_player_put(w, &ply);
}

/* en21.c:1085 bhEne21_SetSparkEff. */
static void en21_set_spark_eff(RECVX_ENEMY_WORK* w, int num, int flg, uint32_t ofy)
{
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    int n;

    for (n = 0; n < num; ++n) {
        RECVX_T3_EFF ef = {0};
        RECVX_T3_LINK none = {NULL, 0};
        RECVX_ENEMY_PLAYER ply;
        float mtx[16];
        int slot;

        ef.id = 399;
        ef.flg = 1;
        ef.type = flg;
        ef.sx = 1.0f; ef.sy = 1.0f; ef.sz = 1.0f;

        switch (flg) {
        case 0:
            ef.px = w->px - 0.5f;
            ef.py = w->py + 0.7f;
            ef.pz = w->pz - 0.5f;
            ef.ay = w->ay;
            break;

        case 1: {
            float tmp;
            float mx, my, mz;
            if (ofy >= 16u) { en21_gap(w, "En21SparkTbl[ofy] ausserhalb"); return; }
            if (!ext || !ext->player_joint_matrix
                || !ext->player_joint_matrix(en21_spark_tbl[ofy], mtx)) {
                en21_gap(w, "plp->mlwP->owP[En21SparkTbl[ofy]].mtx");
                return;
            }
            mx = mtx[12]; my = mtx[13]; mz = mtx[14];

            tmp = ((en21_rand(w) % 2) ? 0.2f : -0.2f);
            ef.px = mx + (tmp * (en21_rand(w) % 3));
            tmp = ((en21_rand(w) % 2) ? 0.1f : -0.1f);
            ef.pz = mz + (tmp * (en21_rand(w) % 3));
            tmp = ((en21_rand(w) % 2) ? 0.1f : -0.1f);
            ef.py = my + (tmp * (en21_rand(w) % 3));
            break;
        }

        case 2: {
            float tmp;
            if (!en21_player(w, &ply)) return;
            tmp = ((en21_rand(w) % 2) ? 0.5f : -0.5f);
            ef.px = ply.pos.x + (tmp * ((en21_rand(w) % 6) + 1));
            tmp = ((en21_rand(w) % 2) ? 0.4f : -0.4f);
            ef.pz = ply.pos.z + (tmp * ((en21_rand(w) % 5) + 1));
            ef.py = ply.pos.y + (3.0f * ((en21_rand(w) % 5) + 1));
            break;
        }

        case 3: {
            float mx, my, mz;
            if (!w->sv || !w->sv->joint_matrix || !w->sv->joint_matrix(w, 1u, mtx)) {
                en21_gap(w, "bhEne21_SetSparkEff:mlwP->owP[1].mtx");
                return;
            }
            mx = mtx[12];
            my = mtx[13] + 0.7f;
            mz = mtx[14];
            ef.px = mx;
            ef.pz = mz;
            ef.py = my;
            ef.ay = w->ay + EN21_DEG_ANG(180.0f);
            break;
        }

        default:
            /* Im Original bleiben Lage und Winkel dann die von sys->ef. */
            en21_gap(w, "bhEne21_SetSparkEff: flg ausserhalb 0..3");
            return;
        }

        if (!ext || !ext->effect_tb || !ext->effect_patch || !ext->model_texp) {
            en21_gap(w, "bhSetEffectTb/eff[eno]/mlwP->texP (en21.c:1157)");
            return;
        }
        slot = ext->effect_tb(&ef, NULL, none, 0);
        if (slot != -1) {
            RECVX_T3_EFFPATCH p = {0};
            p.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP | RECVX_T3_EFF_TEX_ID;
            p.stflg_or = 0x20;
            p.texp = ext->model_texp(w);
            p.tex_id = 5;
            ext->effect_patch(slot, &p);
        }
    }
}

/* en21.c:1299 bhEne21_SetMarkEff. */
static void en21_set_mark_eff(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    RECVX_T3_EFF ef = {0};
    RECVX_T3_LINK none = {NULL, 0};
    float mtx[16];
    int slot;

    if (!w->sv || !w->sv->joint_matrix || !w->sv->joint_matrix(w, 0u, mtx)) {
        en21_gap(w, "bhEne21_SetMarkEff:mlwP->owP[0].mtx");
        return;
    }
    ef.id = 396;
    ef.flg = 1;
    ef.type = 0;
    ef.sx = 1.0f; ef.sy = 1.0f; ef.sz = 1.0f;
    ef.px = mtx[12];
    ef.py = mtx[13] + 0.1f;
    ef.pz = mtx[14];
    ef.ay = w->ay;
    ef.ax = EN21_DEG_ANG(90.0f);

    if (!ext || !ext->effect_tb || !ext->effect_patch || !ext->model_texp) {
        en21_gap(w, "bhSetEffectTb/eff[eno]/mlwP->texP (en21.c:1315)");
        return;
    }
    slot = ext->effect_tb(&ef, NULL, none, 0);
    if (slot != -1) {
        RECVX_T3_EFFPATCH p = {0};
        p.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP | RECVX_T3_EFF_TEX_ID;
        p.stflg_or = 0x20;
        p.texp = ext->model_texp(w);
        p.tex_id = 6;
        ext->effect_patch(slot, &p);
    }
}

/* ---------------------------------------------------------------------------
   Effektschritte (en21.c:1168 / :1324). Sie lesen/schreiben nur O_WRK-Felder.
   --------------------------------------------------------------------------- */
typedef struct { int32_t u, v, sx, sy; } RECVX_EN21_UV;   /* en21.h EN21UV */

static const RECVX_EN21_UV en21_spark_uv0[9] = {
    {80, 160, 40, 40}, {120, 160, 40, 40}, {160, 160, 40, 40}, {200, 160, 40, 40},
    {80, 200, 40, 40}, {120, 200, 40, 40}, {160, 200, 40, 40}, {200, 200, 40, 40},
    {-1, 0, 0, 0}
};
static const RECVX_EN21_UV en21_spark_uv1[8] = {
    {0, 0, 80, 80}, {80, 0, 80, 80}, {160, 0, 80, 80},
    {0, 80, 80, 80}, {80, 80, 80, 80}, {160, 80, 80, 80},
    {0, 160, 80, 80}, {-1, 0, 0, 0}
};
/* uvinfo2 und uvinfo3 sind im Original zwei getrennte, gleiche Tabellen. */
static const RECVX_EN21_UV* const en21_spark_uv[4] = {
    en21_spark_uv0, en21_spark_uv1, en21_spark_uv0, en21_spark_uv0
};
static const int8_t en21_spark_flm[4] = {10, 3, 2, 3};
static const int8_t en21_spark_pat[4] = {8, 7, 8, 8};

static const RECVX_EN21_UV en21_mark_uv0[12] = {
    {0, 0, 55, 55}, {55, 0, 55, 55}, {110, 0, 55, 55}, {165, 0, 55, 55},
    {0, 55, 55, 55}, {55, 55, 55, 55}, {110, 55, 55, 55}, {165, 55, 55, 55},
    {0, 110, 55, 55}, {55, 110, 55, 55}, {110, 110, 55, 55},
    {-1, 0, 0, 0}
};

static int32_t en21_effect_rand(RECVX_EN21_EFFECT* op)
{
    return op->random_next ? op->random_next() : recvx_core_random_next();
}

static void en21_effect_uv(RECVX_EN21_EFFECT* op, const RECVX_EN21_UV* uvp, uint32_t col)
{
    op->tv[0].col = col; op->tv[1].col = col; op->tv[2].col = col; op->tv[3].col = col;
    op->tv[0].u = uvp->u / 256.0f;
    op->tv[0].v = uvp->v / 256.0f;
    op->tv[1].u = (uvp->u + uvp->sx - 1) / 256.0f;
    op->tv[1].v = op->tv[0].v;
    op->tv[2].u = op->tv[0].u;
    op->tv[2].v = (uvp->v + uvp->sy - 1) / 256.0f;
    op->tv[3].u = op->tv[1].u;
    op->tv[3].v = op->tv[2].v;
    op->sx = (op->sxb * uvp->sx) / 32.0f;
    op->sy = (op->syb * uvp->sy) / 32.0f;
}

/* en21.c:1168 bhEff_E21_Spark. ORIGINALER DURCHFALL case 0 -> case 1. */
int recvx_en21_effect_spark(RECVX_EN21_EFFECT* op)
{
    const RECVX_EN21_UV* uvp;

    if (!op || op->type > 3u) return 0;

    switch (op->mode0) {
    case 0:
        op->bl_src = 8;
        op->bl_dst = 10;
        op->ct0 = 0;
        op->ct1 = en21_spark_pat[op->type];
        op->ct2 = en21_spark_flm[op->type];
        op->flg |= 0x180000u;
        op->stflg |= 0x20u;
        op->sxb = op->sx;
        op->syb = op->sy;
        op->yn = op->py;
        op->mode0 = 1;
        /* fall through */
    case 1:
        op->ct2--;
        if (op->ct2 < 0) {
            op->flg = 0;
            return 0;
        }
        uvp = &en21_spark_uv[op->type][en21_effect_rand(op) % op->ct1];

        switch (op->type) {
        case 0:
            op->px = op->px - njSin((uint16_t)op->ay) * 0.4f;
            op->pz = op->pz - njCos((uint16_t)op->ay) * 0.4f;
            break;
        case 1:
            op->py += 0.2f;
            break;
        case 2:
            break;
        case 3:
            op->px = op->px - njSin((uint16_t)op->ay) * 0.7f;
            op->pz = op->pz - njCos((uint16_t)op->ay) * 0.7f;
            break;
        }

        en21_effect_uv(op, uvp, 0xFFA0A0FFu);
        return 1;
    }
    return 0;
}

/* en21.c:1324 bhEff_E21_Mark. */
int recvx_en21_effect_mark(RECVX_EN21_EFFECT* op)
{
    const RECVX_EN21_UV* uvp;

    if (!op) return 0;

    switch (op->mode0) {
    case 0:
        op->bl_src = 11;
        op->bl_dst = 3;
        op->ct0 = 0;
        op->ct1 = 33;
        op->ct2 = 0;
        op->flg |= 0x80000u;
        op->stflg |= 0x20u;
        op->sxb = op->sx;
        op->syb = op->sy;
        op->mode0 = 1;
        /* fall through */
    case 1:
        op->ct1--;
        if (op->ct1 < 0) {
            op->flg = 0;
            return 0;
        }

        if ((op->ct1 % 3) == 0) op->ct2++;

        /* ct1 laeuft 32..0, ct2 erreicht hoechstens 11 (Abschlusseintrag). */
        if (op->ct2 < 0 || op->ct2 > 11) return 0;
        uvp = &en21_mark_uv0[op->ct2];

        en21_effect_uv(op, uvp, 0x40808080u);
        return 1;
    }
    return 0;
}
