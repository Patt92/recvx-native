/* Identitaet 55 — Gepanzerter Ritter B (Lanze und grosser Schild).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en55.c (bhEne55 :45,
   bhEne55_MainLoop :67, bhEne55_SetMtn :75, bhEne55_CollCheck :94,
   bhEne55_CalcEnemy :134, bhEne55_Init :160, bhEne55_Move :208,
   bhEne55_Damage :214, bhEne55_MVType00 :220, bhEne55_MV00 :226,
   bhEne55_DGType00 :242, bhEne55_DG00 :248 und die fuenf Tabellen
   :18/:27/:31/:35/:39), zonzon.c:1154 (bhEne_ChgMtn) und subpl.c:143
   (bhEne_Event, Ereignisfall der Mode0-Tabelle).

   Der Unterschied zu Identitaet 54 liegt ausschliesslich in der
   Gelenknummer der Rumpfkapsel: c1 sitzt hier auf Gelenk 12 (en55.c:140),
   dort auf Gelenk 5 (en54.c:140). Kein Originalkoerper, kein Generator,
   keine zweite Workwelt. */
#include "en55.h"
#include "en17.h"

/* Geteilter Mechanismus beim en01-Besitzer (native/en01_work.h:336). */
extern int recvx_enemy_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no,
                               int32_t frm, int32_t rate);

static void en55_init(RECVX_ENEMY_WORK* w);
static void en55_move(RECVX_ENEMY_WORK* w);
static void en55_damage(RECVX_ENEMY_WORK* w);
static void en55_event(RECVX_ENEMY_WORK* w);
static void en55_set_mtn(RECVX_ENEMY_WORK* w);
static void en55_calc_enemy(RECVX_ENEMY_WORK* w);
static void en55_coll_check(RECVX_ENEMY_WORK* w);
static void en55_mv_type00(RECVX_ENEMY_WORK* w);
static void en55_mv00(RECVX_ENEMY_WORK* w);
static void en55_dg_type00(RECVX_ENEMY_WORK* w);
static void en55_dg00(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN55_PROC)(RECVX_ENEMY_WORK*);

/* en55.c:18 bhEne55_Mode0[6]; Index 2 und 4 sind im Original NULL. */
static const RECVX_EN55_PROC en55_mode0[6] = {
    en55_init, en55_move, 0, en55_damage, 0, en55_event
};
/* en55.c:27/:31/:35/:39 — die vier Typ-/Modus-Tabellen mit je einer Zeile. */
static const RECVX_EN55_PROC en55_move_type[1]    = { en55_mv_type00 };
static const RECVX_EN55_PROC en55_move_mode2[1]   = { en55_mv00 };
static const RECVX_EN55_PROC en55_damage_type[1]  = { en55_dg_type00 };
static const RECVX_EN55_PROC en55_damage_mode2[1] = { en55_dg00 };

/* en55.c:45 bhEne55. */
void recvx_en55(RECVX_ENEMY_WORK* w)
{
    int i;
    if (!w) return;

    /* bhEne55_MainLoop (en55.c:67). */
    if (w->mode0 < 6u && en55_mode0[w->mode0]) {
        en55_mode0[w->mode0](w);
    } else if (w->sv && w->sv->gap) {
        w->sv->gap("bhEne55_Mode0");
    }
    en55_set_mtn(w);

    /* Redundanz zur anschliessenden bhEne_InitDamage; im Original so. */
    if (w->flg & 0x4u) {
        for (i = 0; i < (int)RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;
        w->flg &= ~0x4u;
    }

    en55_calc_enemy(w);
    en55_coll_check(w);
}

/* en55.c:75 bhEne55_SetMtn. */
static void en55_set_mtn(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->set_motion) {
        if (sv->set_motion(w, w->mtn_add, w->mtn_md) != 0)
            w->flg |= 0x2000000u;
        else
            w->flg &= ~0x2000000u;
    } else if (sv && sv->gap) {
        sv->gap("bhSetMotion");
    }
}

/* en55.c:94 bhEne55_CollCheck. */
static void en55_coll_check(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    uint32_t i, n;
    if (!sv) return;
    n = sv->enemy_count ? sv->enemy_count() : 0u;
    for (i = 0; i < n; ++i) {
        RECVX_ENEMY_WORK* epp = sv->enemy_at ? sv->enemy_at(i) : 0;
        if (epp && epp->id == 17u && w->mode0 == 1u) {
            RECVX_ENEMY_VEC center;
            if (!recvx_en17_blade_tip(epp, &center)) {
                if (sv->gap) sv->gap("bhEne55:((O_WRK*)epp->exp2)->mlwP->owP[1]");
            } else if (!sv->sphere_capsule) {
                if (sv->gap) sv->gap("npCollisionCheckSC");
            } else if (sv->sphere_capsule(&center, 4.0f,
                                          &w->watr.c1, &w->watr.c2, w->watr.r) != 0) {
                recvx_en17_blade_hit(epp);
                w->mode0 = 3;
                w->mode1 = 0;
                w->mode2 = 0;
                w->mode3 = 0;
            }
            break;
        }
    }
}

/* en55.c:134 bhEne55_CalcEnemy. c1 auf Gelenk 12, c2 auf der Wurzel. */
static void en55_calc_enemy(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    float mtx[16];
    if (!sv) return;
    if (!sv->calc_model) {
        if (sv->gap) sv->gap("bhCalcModel");
        return;
    }
    sv->calc_model(w);

    if (!sv->joint_matrix) {
        if (sv->gap) sv->gap("bhEne55:mlwP->owP[12].mtx");
        return;
    }
    if (sv->joint_matrix(w, 12u, mtx)) {
        w->cah = w->ah = mtx[13] - w->py;
        w->watr.c1.x = mtx[12];
        w->watr.c1.y = mtx[13];
        w->watr.c1.z = mtx[14];
    } else if (sv->gap) {
        sv->gap("bhEne55:mlwP->owP[12].mtx");
    }
    if (sv->joint_matrix(w, 0u, mtx)) {
        w->watr.c2.x = mtx[12];
        w->watr.c2.y = mtx[13];
        w->watr.c2.z = mtx[14];
    } else if (sv->gap) {
        sv->gap("bhEne55:mlwP->owP[0].mtx");
    }
    w->watr.r = 3.0f;
}

/* en55.c:160 bhEne55_Init. */
static void en55_init(RECVX_ENEMY_WORK* w)
{
    int i;

    w->ar = 6.0f;
    w->ah = 20.0f;
    w->aw = 0.0f;
    w->ad = 0.0f;
    w->car = 6.0f;
    w->cah = 20.0f;

    w->hp = 250;

    w->stflg = 0;

    for (i = 0; i < (int)RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;

    w->hokan_rate = 0;
    w->hokan_count = 0;

    w->mtn_no = 0;
    w->mtn_add = 0;
    w->frm_no = 0;

    w->mtn_tp = 0;
    w->mtn_md = 0;

    w->flg |= 0x8000u;
    w->flg &= ~0x2u;

    w->clp_jno[0] = -1;

    w->mdflg |= 0x20u;

    w->mode0 = 1;
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
}

/* en55.c:208 bhEne55_Move. */
static void en55_move(RECVX_ENEMY_WORK* w)
{
    if (w->type < 1u) en55_move_type[w->type](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne55_MoveType");
}

/* en55.c:214 bhEne55_Damage. */
static void en55_damage(RECVX_ENEMY_WORK* w)
{
    if (w->type < 1u) en55_damage_type[w->type](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne55_DamageType");
}

/* en55.c:220 bhEne55_MVType00. */
static void en55_mv_type00(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 1u) en55_move_mode2[w->mode2](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne55_MoveMode2");
}

/* en55.c:226 bhEne55_MV00. ORIGINALER DURCHFALL von case 0 nach case 1. */
static void en55_mv00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 0, 0, 0);
        w->mtn_add = 0;
        w->mode3++;
        /* fall through */
    case 1:
        break;
    }
}

/* en55.c:242 bhEne55_DGType00. */
static void en55_dg_type00(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 1u) en55_damage_mode2[w->mode2](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne55_DamageMode2");
}

/* en55.c:248 bhEne55_DG00. */
static void en55_dg00(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    switch (w->mode3) {
    case 0:
        if (w->pz < 20.0f)
            recvx_enemy_chg_mtn(w, 1, 0, 0);
        else
            recvx_enemy_chg_mtn(w, 0, 0, 0);
        w->mode3++;
        /* fall through */
    case 1:
        if (sv && sv->motion_frames) {
            /* en55.c:264: (frm_no / 65536) == mnwP[mtn_no].frm_num - 1 */
            if ((w->frm_no / 65536) == (int32_t)sv->motion_frames(w, w->mtn_no) - 1) {
                w->mtn_add = 0;
                w->mode3++;
            }
        } else if (sv && sv->gap) {
            sv->gap("bhEne55:mnwP[mtn_no].frm_num");
        }
        break;
    case 2:
        break;
    }
}

/* en55.c:25 Mode0-Index 5 — bhEne_Event (subpl.c:143). Siehe en54.c: der
   gemeinsame Ereignisfall gehoert dem Begleitfiguren-Mechanismus; der
   Vertragsdienst `event` (enemy_work.h:489-495) wird gerufen, sobald er
   gebunden ist, sonst gemeldete Luecke statt Ersatz. */
static void en55_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    if (w->sv && w->sv->gap) w->sv->gap("bhEne_Event(subpl_mv01_tbl)");
}
