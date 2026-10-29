/* Identitaet 25 — Deckenfalle (Ceiling Trap). Nativ eigenstaendig bestimmt.
   Semantikreferenz: en25.c (Zeilen siehe en25.h), subpl.c:143 (bhEne_Event)
   und eneset.c:906 (bhEne_CallocWork).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Besonderheit dieser Identitaet: Der Work veroeffentlicht EINE bewegliche
   Flaeche in sys->mwalp (Original: EXP0_ATR(0) = &sys->mwalp[sys->mwal_n++],
   en25.c:58). Nativ laeuft das ueber den Vertragsdienst mobile_wall_alloc/
   get/set (enemy_work.h:503-512); der Work haelt nur den Index.

   Die Falle schreibt ausserdem in den Spielerwork (plp->flg/flg2/mode0/mode3/
   py/hp, en25.c:208-212 und :367-379). Das geschieht ausschliesslich ueber
   die Spielersicht player_get/player_set (enemy_work.h:123-147/460-464) —
   kein zweiter Spielerwork. */
#include "en25.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
   EXP0-Block (12 Byte, en25.c:57). Die Offsets sind die der Originalmakros
   EXP0_UC(o) = *(u8*)(exp0 + o) (macros.h:37) und
   EXP0_ATR(n) = *(ATR_WORK**)(exp0 + n*4 + 0x4) (macros.h:78).
   --------------------------------------------------------------------------- */
typedef struct {
    uint8_t inside_close;   /* +0x0 EXP0_UC(0x0) 6.9er-Box (en25.c:126-133)  */
    uint8_t inside_tight;   /* +0x1 EXP0_UC(0x1) 6.8er-Box (en25.c:140-147)  */
    uint8_t pad0[2];        /* +0x2/+0x3 im Original unbelegt                  */
    int32_t mwal_index;     /* +0x4 EXP0_ATR(0): Index in sys->mwalp oder -1   */
    uint8_t caught;         /* +0x8 EXP0_UC(0x8)  (en25.c:82/209)              */
    uint8_t pad1[3];        /* +0x9..+0xB im Original unbelegt                 */
} RECVX_EN25_EXP0;

_Static_assert(sizeof(RECVX_EN25_EXP0) == 0xCu, "EXP0-Block ist 0xC Byte (en25.c:57)");
_Static_assert(offsetof(RECVX_EN25_EXP0, mwal_index) == 0x4, "EXP0_ATR(0) liegt bei +0x4");
_Static_assert(offsetof(RECVX_EN25_EXP0, caught) == 0x8, "EXP0_UC(0x8) liegt bei +0x8");

/* en25.c:13 BloodParam (BP_WORK, enemy_work.h:286). */
static const RECVX_ENEMY_BP_WORK en25_blood_param = {
    { 0.0f, 0.2f, 0.0f },
    0,
    0.0f,
    0.3f,
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 0, 0, 0, 0, 0 }
};

static void en25_init(RECVX_ENEMY_WORK* w);
static void en25_move(RECVX_ENEMY_WORK* w);
static void en25_nage(RECVX_ENEMY_WORK* w);
static void en25_damage(RECVX_ENEMY_WORK* w);
static void en25_die(RECVX_ENEMY_WORK* w);
static void en25_event(RECVX_ENEMY_WORK* w);
static void en25_brain(RECVX_ENEMY_WORK* w);
static void en25_mv00(RECVX_ENEMY_WORK* w);
static void en25_mv01(RECVX_ENEMY_WORK* w);
static void en25_mv02(RECVX_ENEMY_WORK* w);
static void en25_mv03(RECVX_ENEMY_WORK* w);
static void en25_set_effect(RECVX_ENEMY_WORK* w);
static void en25_player_control(RECVX_ENEMY_WORK* w);

typedef void (*RECVX_EN25_PROC)(RECVX_ENEMY_WORK*);

/* en25.c:22. */
static const RECVX_EN25_PROC en25_mode0[6] = {
    en25_init, en25_move, en25_nage, en25_damage, en25_die, en25_event
};

/* en25.c:31. */
static const RECVX_EN25_PROC en25_move_mode2[4] = {
    en25_mv00, en25_mv01, en25_mv02, en25_mv03
};

static void en25_gap(RECVX_ENEMY_WORK* w, const char* what)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(what);
}

/* ---------------------------------------------------------------------------
   Bewegliche Flaeche: Lesen-Aendern-Schreiben auf derselben Flaeche.
   --------------------------------------------------------------------------- */
static int en25_wall_get(RECVX_ENEMY_WORK* w, const RECVX_EN25_EXP0* e,
                         RECVX_ENEMY_ATR* out)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (!e || e->mwal_index < 0) return 0;
    if (!sv || !sv->mobile_wall_get) {
        en25_gap(w, "mobile_wall_get (sys->mwalp)");
        return 0;
    }
    return sv->mobile_wall_get(e->mwal_index, out);
}

static void en25_wall_set(RECVX_ENEMY_WORK* w, const RECVX_EN25_EXP0* e,
                          const RECVX_ENEMY_ATR* in)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    if (!e || e->mwal_index < 0) return;
    if (!sv || !sv->mobile_wall_set) {
        en25_gap(w, "mobile_wall_set (sys->mwalp)");
        return;
    }
    sv->mobile_wall_set(e->mwal_index, in);
}

/* ---------------------------------------------------------------------------
   mnwP[mtn_no].frm_num - 1 des Originals (en25.c:222/232/267). Liefert -1,
   wenn die Motionquelle fehlt; ein Vergleich mit dem ganzzahligen frm_no
   (>= 0) ist dann nie wahr, genau wie im Original bei frm_num == 0.
   --------------------------------------------------------------------------- */
static int32_t en25_last_frame(RECVX_ENEMY_WORK* w, uint32_t mtn_no)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    uint32_t n;
    if (!sv || !sv->motion_frames) {
        en25_gap(w, "motion_frames (mnwP[mtn_no].frm_num)");
        return -1;
    }
    n = sv->motion_frames(w, mtn_no);
    if (n == 0u) {
        en25_gap(w, "mnwP[mtn_no].frm_num (Motion fehlt)");
        return -1;
    }
    return (int32_t)n - 1;
}

/* epw->mlwP->owP[0].mtx[13] (en25.c:188/265): Y der Weltwurzel des Modells,
   ueber die Gelenkmatrix des Wurzelobjekts — enemy_work.h:379. */
static int en25_root_y(RECVX_ENEMY_WORK* w, float* out_y)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    float mtx[16];
    if (!sv || !sv->joint_matrix) {
        en25_gap(w, "joint_matrix(owP[0])");
        return 0;
    }
    if (!sv->joint_matrix(w, 0u, mtx)) return 0;
    *out_y = mtx[13];
    return 1;
}

/* ---------------------------------------------------------------------------
   en25.c:192-207: beim Zuschnappen endet ein laufendes Zielen wie in
   bhCPM0_nage (player.c:6596) und das Spielermodell wird ausgeblendet.
   --------------------------------------------------------------------------- */
static const RECVX_EN25_SERVICES* en25_sv;
void recvx_en25_bind(const RECVX_EN25_SERVICES* services) { en25_sv = services; }

static int en25_trap_flags(RECVX_ENEMY_WORK* w)
{
    if (!en25_sv || !en25_sv->sys_flags || !en25_sv->sys_flags_set) {
        en25_gap(w, "sys->gm_flg/st_flg/pt_flg (en25.c:192-207)");
        return 0;
    }
    if ((en25_sv->sys_flags(0) & 0x40u) && (en25_sv->sys_flags(1) & 0x800000u)) {  /* :192 */
        if (!(en25_sv->sys_flags(0) & 0x1000000u))                                  /* :194 */
            en25_sv->sys_flags_set(0, 0x800u, 0x40u | 0x80u);                       /* :196-198 */
        en25_sv->sys_flags_set(1, 0u, 0x800000u);                                   /* :200 */
        en25_sv->sys_flags_set(0, 0u, 0x80000u);                                    /* :201 */
        en25_sv->sys_flags_set(2, 0x1u, 0u);                                        /* :202 */
    }
    return 1;
}

/* en25.c:39 bhEne25. */
void recvx_en25(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_EN25_EXP0* e;

    if (!w) return;
    sv = w->sv;

    if (w->mode0 < 6u) {
        en25_mode0[w->mode0](w);
    } else {
        en25_gap(w, "bhEne25_Mode0 (mode0 ausserhalb 0..5)");
    }

    /* en25.c:43: bhSetMotion(epw, mtn_add, mtn_md, mtn_tp). */
    if (sv && sv->set_motion) sv->set_motion(w, w->mtn_add, w->mtn_md);
    else en25_gap(w, "bhSetMotion");

    /* en25.c:44. */
    if (sv && sv->calc_model) sv->calc_model(w);
    else en25_gap(w, "bhCalcModel");

    /* en25.c:46: if (EXP0_UC(0x8) == 1). Der Block ist nach dem ersten Pass
       garantiert vorhanden (mode0 startet 0 -> Init); fehlt er, hat Init den
       erschoepften Allokator bereits gemeldet. */
    e = (RECVX_EN25_EXP0*)w->exp0;
    if (e && e->caught == 1u) {
        en25_player_control(w);
    }
}

/* en25.c:53 bhEne25_Init. */
static void en25_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN25_EXP0* e = (RECVX_EN25_EXP0*)w->exp0;

    if (e == 0) {
        /* en25.c:57 bhEne_CallocWork(0xC, 0x8) — Groesse 0xC, Ausrichtung 8.
           Der Vertragsdienst kennt nur die Groesse (enemy_work.h:537). */
        if (!sv || !sv->calloc_work) {
            en25_gap(w, "bhEne_CallocWork(0xC)");
            return;
        }
        e = (RECVX_EN25_EXP0*)sv->calloc_work(0xCu);
        if (e == 0) {
            en25_gap(w, "bhEne_CallocWork(0xC): Budget erschoepft");
            return;
        }
        w->exp0 = e;
        e->pad0[0] = 0; e->pad0[1] = 0;
        e->pad1[0] = 0; e->pad1[1] = 0; e->pad1[2] = 0;
        e->mwal_index = -1;

        /* en25.c:58-63: EXP0_ATR(0) = &sys->mwalp[sys->mwal_n++]; Flaeche
           kennzeichnen (flg |= 0x81, type 0, flr_no 0, attr 2, prm0..3 0). */
        if (sv->mobile_wall_alloc) {
            RECVX_ENEMY_ATR atr;
            e->mwal_index = sv->mobile_wall_alloc();
            if (e->mwal_index < 0) {
                en25_gap(w, "sys->mwalp erschoepft (en25.c:58)");
            } else if (sv->mobile_wall_get && sv->mobile_wall_set) {
                if (sv->mobile_wall_get(e->mwal_index, &atr)) {
                    atr.flg |= 0x81u;
                    atr.type = 0u;
                    atr.flr_no = 0;
                    atr.attr = 2u;
                    atr.prm0 = atr.prm1 = atr.prm2 = atr.prm3 = 0u;
                    sv->mobile_wall_set(e->mwal_index, &atr);
                } else {
                    en25_gap(w, "mobile_wall_get (en25.c:58)");
                }
            } else {
                en25_gap(w, "mobile_wall_get/set (en25.c:58)");
            }
        } else {
            en25_gap(w, "mobile_wall_alloc (sys->mwalp)");
        }
    }

    /* en25.c:66-71: Geometrie in JEDEM Pass. */
    {
        RECVX_ENEMY_ATR atr;
        if (en25_wall_get(w, e, &atr)) {
            atr.h  = 4.0f;
            atr.py = w->py - -30.0f;   /* Original: epw->py - -30.0f */
            atr.w  = 14.0f;
            atr.d  = 14.0f;
            atr.px = w->px - 7.0f;
            atr.pz = w->pz - 7.0f;
            en25_wall_set(w, e, &atr);
        }
    }

    w->flg |= 0x1u;      /* en25.c:72 */
    w->flg |= 0x8000u;   /* en25.c:73 */
    w->flg &= ~0x6u;     /* en25.c:74 */

    w->ar = 0.0f;
    w->ah = 0.0f;
    w->aw = 0.0f;
    w->ad = 0.0f;

    w->hp = 100;        /* en25.c:79 — einzige Stelle, die hp dieser Identitaet setzt */

    e->inside_close = 0;   /* en25.c:80 EXP0_UC(0x0) */
    e->inside_tight = 0;   /* en25.c:81 EXP0_UC(0x1) */
    e->caught       = 0;   /* en25.c:82 EXP0_UC(0x8) */

    w->mtn_no = 0;
    w->frm_no = 0;
    w->hokan_count = 0;
    w->hokan_rate = 0;
    w->mtn_add = 0;

    if (w->type == 0u) {
        w->mode0 = 1;
        w->mode1 = 1;
        w->mode2 = 0;
        w->mode3 = 0;
    } else if (w->type == 1u) {
        w->mode0 = 4;   /* Die (leer, en25.c:313) */
        w->mode1 = 0;
        w->mode2 = 0;
        w->mode3 = 0;
    }

    w->mdflg |= 0x20u;
}

/* en25.c:108 — leer. */
static void en25_brain(RECVX_ENEMY_WORK* w)
{
    (void)w;
}

/* en25.c:114 bhEne25_Move. */
static void en25_move(RECVX_ENEMY_WORK* w)
{
    RECVX_EN25_EXP0* e = (RECVX_EN25_EXP0*)w->exp0;
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;
    int have_player;

    if (!e) return;

    have_player = (sv && sv->player_get) ? sv->player_get(&plp) : 0;
    if (!have_player) {
        en25_gap(w, "plp (en25.c:126/140)");
    } else {
        float tx, tz, bx, bz;

        /* en25.c:121-133: 6.9er-Box um den Fallenmittelpunkt. */
        tx = w->px - 6.9f;
        tz = w->pz - 6.9f;
        bx = w->px + 6.9f;
        bz = w->pz + 6.9f;
        if ((plp.pos.x > tx) && (plp.pos.z > tz) && (plp.pos.x < bx) && (plp.pos.z < bz)) {
            e->inside_close = 1;
        } else {
            e->inside_close = 0;
        }

        /* en25.c:135-147: 6.8er-Box. */
        tx = w->px - 6.8f;
        tz = w->pz - 6.8f;
        bx = w->px + 6.8f;
        bz = w->pz + 6.8f;
        if ((plp.pos.x > tx) && (plp.pos.z > tz) && (plp.pos.x < bx) && (plp.pos.z < bz)) {
            e->inside_tight = 1;
        } else {
            e->inside_tight = 0;
        }
    }

    if (w->mode1 == 1u) {
        en25_brain(w);
    }

    if (w->mode0 == 1u) {
        if (w->mode2 < 4u) {
            en25_move_mode2[w->mode2](w);
        } else {
            en25_gap(w, "bhEne25_MoveMode2 (mode2 ausserhalb 0..3)");
        }
    }
}

/* en25.c:161 bhEne25_MV00. */
static void en25_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN25_EXP0* e = (RECVX_EN25_EXP0*)w->exp0;
    if (!e) return;

    if ((w->mode1 == 1u) && (e->inside_close == 1u)) {
        w->mode2 = 1;
        w->mode3 = 0;
    }
}

/* en25.c:171 bhEne25_MV01 — Zuschnappen. */
static void en25_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN25_EXP0* e = (RECVX_EN25_EXP0*)w->exp0;
    const RECVX_ENEMY_SERVICES* sv = w->sv;

    if (!e) return;

    switch (w->mode3) {
    case 0:
        w->ct0 = 12;
        w->mode3++;
        /* Fallthrough */
    case 1:
        if (w->ct0-- == 0) {
            w->mtn_add = 0x10000;
            w->mode3++;
        }
        break;

    case 2: {
        RECVX_ENEMY_ATR atr;
        float root_y;

        /* en25.c:188: EXP0_ATR(0)->py = epw->mlwP->owP->mtx[13]. */
        if (en25_wall_get(w, e, &atr) && en25_root_y(w, &root_y)) {
            atr.py = root_y;
            en25_wall_set(w, e, &atr);
        }

        /* en25.c:190: Spieler steht in der 6.8er-Box, die Falle ist am
           6. Frame und hat noch nicht zugeschnappt. */
        if ((e->inside_tight == 1u) && ((w->frm_no >> 16) == 6) && (e->caught == 0u)) {
            RECVX_ENEMY_PLAYER plp;

            const int flags = en25_trap_flags(w);  /* en25.c:192-204 */

            w->flg |= 0x2u;              /* en25.c:206 */

            if (flags) en25_sv->sys_flags_set(2, 0u, 0x1u);   /* en25.c:207 sys->pt_flg &= ~0x1 */

            if (sv && sv->player_get && sv->player_set && sv->player_get(&plp)) {
                plp.flg &= ~0x18u;       /* en25.c:208 */
                e->caught = 1u;          /* en25.c:209 EXP0_UC(0x8) = 1 */
                plp.mode0 = 6;           /* en25.c:210 */
                plp.mode3 = 0;           /* en25.c:211 */
                plp.hp = -1;             /* en25.c:212 */
                sv->player_set(&plp);
            } else {
                en25_gap(w, "plp (en25.c:208-212)");
            }
        }

        /* en25.c:215-220: Wirkungspunkt der Falle am 9. Frame. */
        if ((w->frm_no >> 16) == 9) {
            en25_set_effect(w);
            if (sv && sv->vibration) sv->vibration(1, 0xB);
            else en25_gap(w, "StartVibrationEx(1,0xB)");
            /* CallSystemSe(0, 0x80000202) */
            if (sv && sv->system_se) sv->system_se(0x80000202);
            else en25_gap(w, "CallSystemSe(0x80000202)");
        }

        /* en25.c:222-228: Ende der Zuschnapp-Motion. */
        if ((w->frm_no >> 16) == en25_last_frame(w, w->mtn_no)) {
            if (sv && sv->vibration) sv->vibration(1, 0xB);
            else en25_gap(w, "StartVibrationEx(1,0xB)");
            w->mtn_no = 1;
            w->frm_no = 0;
            w->mode3++;
        }
        break;
    }

    case 3: {
        /* en25.c:231-238: Ende der zweiten Motion, dann 15 Ticks warten. */
        if ((w->frm_no >> 16) == en25_last_frame(w, w->mtn_no)) {
            w->mtn_add = 0;
            w->ct0 = 15;
            w->mode3++;
        }
        break;
    }

    case 4:
        /* en25.c:240-245: zurueck nach oben (MV02). */
        if (w->ct0-- == 0) {
            w->mode2 = 2;
            w->mode3 = 0;
        }
        break;

    default:
        en25_gap(w, "bhEne25_MV01 (mode3 ausserhalb 0..4)");
        break;
    }
}

/* en25.c:251 bhEne25_MV02 — Rueckzug nach oben. */
static void en25_mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN25_EXP0* e = (RECVX_EN25_EXP0*)w->exp0;
    const RECVX_ENEMY_SERVICES* sv = w->sv;

    if (!e) return;

    switch (w->mode3) {
    case 0:
        w->mtn_no = 2;
        w->frm_no = 0;
        w->mtn_add = 0x10000;

        /* CallSystemSe(0, 0x80000203) */
        if (sv && sv->system_se) sv->system_se(0x80000203);
        else en25_gap(w, "CallSystemSe(0x80000203)");

        w->mode3++;
        /* Fallthrough */
    case 1: {
        RECVX_ENEMY_ATR atr;
        float root_y;

        /* en25.c:265. */
        if (en25_wall_get(w, e, &atr) && en25_root_y(w, &root_y)) {
            atr.py = root_y;
            en25_wall_set(w, e, &atr);
        }

        /* en25.c:267-273: Ende der Rueckzug-Motion. */
        if ((w->frm_no >> 16) == en25_last_frame(w, w->mtn_no)) {
            if (en25_wall_get(w, e, &atr)) {
                atr.py = w->py - -30.0f;
                en25_wall_set(w, e, &atr);
            }
            w->mtn_add = 0;
            w->mode2 = 3;
            w->mode3 = 0;
        }
        break;
    }

    default:
        en25_gap(w, "bhEne25_MV02 (mode3 ausserhalb 0..1)");
        break;
    }
}

/* en25.c:278 bhEne25_MV03 — Ruhestellung. */
static void en25_mv03(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 0;
        w->frm_no = 0;
        w->hokan_count = 0;
        w->hokan_rate = 0;
        w->mtn_add = 0;
        w->ct0 = 20;
        w->mode3++;
        /* Fallthrough */
    case 1:
        if (w->ct0-- == 0) {
            w->mode2 = 0;
            w->mode3 = 0;
        }
        break;

    default:
        en25_gap(w, "bhEne25_MV03 (mode3 ausserhalb 0..1)");
        break;
    }
}

/* en25.c:301/307/313 — leer. */
static void en25_nage(RECVX_ENEMY_WORK* w)   { (void)w; }
static void en25_damage(RECVX_ENEMY_WORK* w) { (void)w; }
static void en25_die(RECVX_ENEMY_WORK* w)    { (void)w; }

/* en25.c:319 bhEne25_SetEffect.
   Original: 10 Durchlaeufe x 4 bhSetEffectTb(&sys->ef, NULL, NULL, 0) mit
   festem Satz
       id = 2, flg = 0x100001, type = 3, mdlver = 0, flr_no = 0, ax = ay = 0,
       sx = sy = sz = 2.0f
   an vier Kanten eines 15x15-Rasters um den Fallenmittelpunkt:
       (px-7.5+1.5i, 11.5, pz-7.5), (px-7.5, 11.5, pz-7.5+1.5i),
       (px+7.5-1.5i, 11.5, pz+7.5), (px+7.5, 11.5, pz+7.5-1.5i).
   Der Vertragsdienst set_effect (effect.c:331 bhSetEffect) traegt diesen Satz
   NICHT: er setzt flg 1/type 0/mdlver 0/sx=sy=sz 1.0 und bezieht die Position
   aus einem POINT mit lokalen Offsets. Genau dafuer deklariert T3 einen
   `effect_tb`-Dienst (en03_lib.h, bhSetEffectTb effect.c:436). Bis er
   gebunden ist, wird der Aufruf namentlich gemeldet und nicht ersetzt. */
static void en25_set_effect(RECVX_ENEMY_WORK* w)
{
    en25_gap(w, "bhSetEffectTb(&sys->ef,id 2,flg 0x100001,type 3,mdlver 0,"
                "sx=sy=sz 2.0) 40x (en25.c:338/343/348/353)");
}

/* en25.c:358 bhEne25_PlayerControl — der Spieler haengt in der Falle. */
static void en25_player_control(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER plp;

    if (!sv || !sv->player_get || !sv->player_set) {
        en25_gap(w, "plp (en25.c:358)");
        return;
    }
    if (!sv->player_get(&plp)) {
        en25_gap(w, "player_get (en25.c:358)");
        return;
    }

    if (plp.mode0 == 6u) {
        switch (plp.mode3) {
        case 0: {
            RECVX_ENEMY_VEC pos;

            plp.flg &= ~0x40018u;   /* en25.c:367 */
            plp.flg |= 0x10600u;    /* en25.c:368 */
            plp.flg2 |= 0x1u;       /* en25.c:369 */

            pos.x = w->px + 5.0f;   /* en25.c:371-373 */
            pos.y = plp.pos.y;
            pos.z = w->pz;
            if (sv->blood_pool) sv->blood_pool(w, &pos, w->ay, &en25_blood_param);
            else en25_gap(w, "bhEne_BloodPool (en25.c:374)");

            plp.mode3++;            /* en25.c:376 */
            /* Fallthrough */
        }
        case 1:
            plp.pos.y = -30.0f;     /* en25.c:379 */
            plp.mode3++;            /* en25.c:380 */
            break;

        case 2:
            break;

        default:
            en25_gap(w, "bhEne25_PlayerControl (plp->mode3 ausserhalb 0..2)");
            break;
        }
        sv->player_set(&plp);
    }
}

/* en25.c:23 Mode0-Index 5 — bhEne_Event (subpl.c:143). Gemeinsamer
   Ereignisfall; Besitzer ist der Begleitfiguren-Mechanismus. Anschluss als
   Patch, hier gemeldete Luecke statt Ersatz. */
static void en25_event(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->event) {
        w->sv->event(w);
        return;
    }
    en25_gap(w, "bhEne_Event(subpl_mv01_tbl)");
}
