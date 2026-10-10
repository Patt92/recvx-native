/* Identitaet 54 — Gepanzerter Ritter A (Schwert und mittlerer Schild).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en54.c (bhEne54 :45,
   bhEne54_MainLoop :67, bhEne54_SetMtn :75, bhEne54_CollCheck :94,
   bhEne54_CalcEnemy :134, bhEne54_Init :160, bhEne54_Move :208,
   bhEne54_Damage :214, bhEne54_MVType00 :220, bhEne54_MV00 :226,
   bhEne54_DGType00 :242, bhEne54_DG00 :248 und die fuenf Tabellen
   :18/:27/:31/:35/:39), zonzon.c:1154 (bhEne_ChgMtn) und subpl.c:143
   (bhEne_Event, Ereignisfall der Mode0-Tabelle).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt. */
#include "en54.h"
#include "en17.h"

/* Der Motionwechsel-Helfer bhEne_ChgMtn (zonzon.c:1154) ist reine
   Work-Feld-Logik und liegt als geteilter Mechanismus beim en01-Besitzer
   (native/en01_work.h:336). Er wird hier benutzt, nicht nachgebaut. */
extern int recvx_enemy_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no,
                               int32_t frm, int32_t rate);

static void en54_init(RECVX_ENEMY_WORK* w);
static void en54_move(RECVX_ENEMY_WORK* w);
static void en54_damage(RECVX_ENEMY_WORK* w);
static void en54_event(RECVX_ENEMY_WORK* w);
static void en54_set_mtn(RECVX_ENEMY_WORK* w);
static void en54_calc_enemy(RECVX_ENEMY_WORK* w);
static void en54_coll_check(RECVX_ENEMY_WORK* w);
static void en54_mv_type00(RECVX_ENEMY_WORK* w);
static void en54_mv00(RECVX_ENEMY_WORK* w);
static void en54_dg_type00(RECVX_ENEMY_WORK* w);
static void en54_dg00(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN54_PROC)(RECVX_ENEMY_WORK*);

/* en54.c:18 bhEne54_Mode0[6]; Index 2 und 4 sind im Original NULL. */
static const RECVX_EN54_PROC en54_mode0[6] = {
    en54_init, en54_move, 0, en54_damage, 0, en54_event
};
/* en54.c:27/:31/:35/:39 — die vier Typ-/Modus-Tabellen mit je einer Zeile.
   `type` kommt aus dem Raum-Datensatz; ein Wert ausserhalb der Tabellengroesse
   laeuft im Original aus der Tabelle heraus und wird hier als Luecke gemeldet. */
static const RECVX_EN54_PROC en54_move_type[1]    = { en54_mv_type00 };
static const RECVX_EN54_PROC en54_move_mode2[1]   = { en54_mv00 };
static const RECVX_EN54_PROC en54_damage_type[1]  = { en54_dg_type00 };
static const RECVX_EN54_PROC en54_damage_mode2[1] = { en54_dg00 };

/* en54.c:45 bhEne54.
   Reihenfolge: MainLoop (Mode0-Tabelle + SetMtn), Redundanz-Loeschung der
   Trefferliste bei flg 0x4, CalcEnemy, CollCheck. */
void recvx_en54(RECVX_ENEMY_WORK* w)
{
    int i;
    if (!w) return;

    /* bhEne54_MainLoop (en54.c:67). */
    if (w->mode0 < 6u && en54_mode0[w->mode0]) {
        en54_mode0[w->mode0](w);
    } else if (w->sv && w->sv->gap) {
        w->sv->gap("bhEne54_Mode0");
    }
    en54_set_mtn(w);

    /* Dieselbe dam-Loeschung, die anschliessend auch bhEne_InitDamage
       ausfuehrt; die Redundanz steht so im Original und bleibt erhalten. */
    if (w->flg & 0x4u) {
        for (i = 0; i < (int)RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;
        w->flg &= ~0x4u;
    }

    en54_calc_enemy(w);
    en54_coll_check(w);
}

/* en54.c:75 bhEne54_SetMtn (auch von MainLoop gerufen). */
static void en54_set_mtn(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (sv && sv->set_motion) {
        if (sv->set_motion(w, w->mtn_add, w->mtn_md) != 0)
            w->flg |= 0x2000000u;
        else
            w->flg &= ~0x2000000u;
    } else if (sv && sv->gap) {
        /* Ohne Dienst ist die Umlaufrueckgabe unbekannt; flg 0x2000000 bleibt
           unveraendert und die Luecke wird gemeldet. */
        sv->gap("bhSetMotion");
    }
}

/* en54.c:94 bhEne54_CollCheck — die Klingenspitze des linken Arms von
   Gegner 17 gegen die Rumpfkapsel dieses Ritters. Der Abbruch steht im
   Original IM if (en54.c:128): bei erfuellter Bedingung endet die Suche
   nach dem ersten id-17-Work. */
static void en54_coll_check(RECVX_ENEMY_WORK* w)
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
                if (sv->gap) sv->gap("bhEne54:((O_WRK*)epp->exp2)->mlwP->owP[1]");
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

/* en54.c:134 bhEne54_CalcEnemy. c1 sitzt auf Gelenk 5, c2 auf der Wurzel
   (Index 0); cach/cah ist die Hoehe des Gelenks 5 ueber py. */
static void en54_calc_enemy(RECVX_ENEMY_WORK* w)
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
        if (sv->gap) sv->gap("bhEne54:mlwP->owP[5].mtx");
        return;
    }
    if (sv->joint_matrix(w, 5u, mtx)) {
        w->cah = w->ah = mtx[13] - w->py;
        w->watr.c1.x = mtx[12];
        w->watr.c1.y = mtx[13];
        w->watr.c1.z = mtx[14];
    } else if (sv->gap) {
        sv->gap("bhEne54:mlwP->owP[5].mtx");
    }
    if (sv->joint_matrix(w, 0u, mtx)) {
        w->watr.c2.x = mtx[12];
        w->watr.c2.y = mtx[13];
        w->watr.c2.z = mtx[14];
    } else if (sv->gap) {
        sv->gap("bhEne54:mlwP->owP[0].mtx");
    }
    w->watr.r = 3.0f;
}

/* en54.c:160 bhEne54_Init. */
static void en54_init(RECVX_ENEMY_WORK* w)
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

/* en54.c:208 bhEne54_Move. */
static void en54_move(RECVX_ENEMY_WORK* w)
{
    if (w->type < 1u) en54_move_type[w->type](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne54_MoveType");
}

/* en54.c:214 bhEne54_Damage. */
static void en54_damage(RECVX_ENEMY_WORK* w)
{
    if (w->type < 1u) en54_damage_type[w->type](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne54_DamageType");
}

/* en54.c:220 bhEne54_MVType00. */
static void en54_mv_type00(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 1u) en54_move_mode2[w->mode2](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne54_MoveMode2");
}

/* en54.c:226 bhEne54_MV00. ORIGINALER DURCHFALL von case 0 nach case 1. */
static void en54_mv00(RECVX_ENEMY_WORK* w)
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

/* en54.c:242 bhEne54_DGType00. */
static void en54_dg_type00(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 < 1u) en54_damage_mode2[w->mode2](w);
    else if (w->sv && w->sv->gap) w->sv->gap("bhEne54_DamageMode2");
}

/* en54.c:248 bhEne54_DG00. Beide switch-Zweige fallen durch. */
static void en54_dg00(RECVX_ENEMY_WORK* w)
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
            /* en54.c:264: (frm_no / 65536) == mnwP[mtn_no].frm_num - 1 */
            if ((w->frm_no / 65536) == (int32_t)sv->motion_frames(w, w->mtn_no) - 1) {
                w->mtn_add = 0;
                w->mode3++;
            }
        } else if (sv && sv->gap) {
            sv->gap("bhEne54:mnwP[mtn_no].frm_num");
        }
        break;
    case 2:
        break;
    }
}

/* en54.c:25 Mode0-Index 5 — bhEne_Event (subpl.c:143) verteilt auf
   subpl_mv01_tbl[ct2], den gemeinsamen Ereignisfall aller Gegner. Diesen
   Zustand besitzt der Begleitfiguren-Mechanismus; native/subplayer.* fuehrt
   ihn derzeit ausdruecklich als Luecke (fremder Besitzer, eigener Vertrag).
   Der Anschluss ist ein Patch fuer den Integrator — hier wird die Luecke
   namentlich gemeldet und nichts erfunden. Der Vertragsdienst `event`
   (enemy_work.h:489-495) wird gerufen, sobald der Subplayer-Besitzer ihn
   bindet. */
static void en54_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    if (w->sv && w->sv->gap) w->sv->gap("bhEne_Event(subpl_mv01_tbl)");
}
