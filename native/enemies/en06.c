/* Identitaet 6 — Moth (en06.c) und ihre Fluegelfragmente bhEne06s (en06sub.c).
   Eigenstaendige native Fassung mit den Diensten aus enemy_work.h, en01_work.h
   (bhAddSpeed ueber recvx_en01_context), en03_lib.h (zonzon/zonzon1-Helfer)
   und en06.h. Jede Funktion nennt ihre Originalzeilen. */
#include "en06.h"
#include "en01_core.h"
#include "en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(offsetof(RECVX_EN06_EXP0, searched) == 0x16u, "EXP0_C(0x16)");
_Static_assert(offsetof(RECVX_EN06_EXP0, atr) == 0x18u, "EXP0 0x18");
_Static_assert(offsetof(RECVX_EN06_EXP0, vel) == 0x1Cu, "EXP0_F(0x1C)");
_Static_assert(offsetof(RECVX_EN06_EXP0, lean) == 0x28u, "EXP0_F(0x28)");
/* Ab 0x34 haelt das Original 4-Byte-Zeiger; nativ liegen Host-Zeiger dort (Ausrichtung). */
_Static_assert(sizeof(RECVX_EN06_EXP1) == 12u, "bhEne_CallocWork(12, 8)");
_Static_assert(offsetof(RECVX_EN06S_EXP0, vel) == 4u, "EXP0_F(4)");
_Static_assert(offsetof(RECVX_EN06S_EXP0, n) == 16u, "EXP0_F(16)");
_Static_assert(offsetof(RECVX_EN06S_EXP0, pts) == 28u, "exp0[28]");
_Static_assert(offsetof(RECVX_EN06S_EXP0, hit) == 64u, "EXP0_I(64)");
_Static_assert(sizeof(RECVX_EN06S_EXP0) == 68u, "bhEne_CallocWork(68, 8)");

/* --- Tabellen (en06.c:18-169, en06sub.c:24-30), reine Daten --------------- */
static const int32_t ene06_hitpoint[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const int8_t  junction_tree[6] = { 0x00, -1, 0x00, 0x00, 0x00, 0x00 };
static const int8_t  sdw_tab[2] = { 0x00, -1 };
const int8_t recvx_en06_broken_parts[12] = { 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
static const RECVX_ENEMY_BP_WORK blood_param = {
    { 0.0f, 0.01f, 0.0f }, 0, 0.0f, 0.05f, { 0.3f, 0.7f, 0.5f, 0.7f, 0.4f }, { 0, 3, 6, 9, 12 }
};
static const RECVX_ENEMY_CPCL cap_col_tab[8] = {
    { 1, 1, 5 }, { 0, 0, -6 }, { 1, 2, 5 }, { 2, 4, 6 }, { 4, 6, 7 }, { 6, 8, 7 }, { 8, 10, 6 }, { 0, 0, 0 }
};
typedef struct { int8_t type[3]; int8_t bloodstain[3]; int8_t exef; } EN06_DMG_REACT;
static const EN06_DMG_REACT dmg_react[21] = {
    { {  0,  1,  0 }, { 0, 0, 0 }, 0 }, { {  0,  1,  0 }, { 0, 0, 0 }, 0 }, { {  0,  0,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 }, { {  1,  1,  0 }, { 0, 0, 0 }, 0 }, { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 }, { {  1,  1,  0 }, { 0, 0, 0 }, 0 }, { {  0,  0,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 }, { {  0,  0,  0 }, { 0, 0, 0 }, 0 }, { {  2,  1,  0 }, { 1, 0, 0 }, 0 },
    { {  0,  0,  0 }, { 0, 0, 0 }, 0 }, { {  1,  1,  0 }, { 0, 0, 0 }, 0 }, { {  2,  1,  0 }, { 1, 1, 1 }, 1 },
    { { -1, -1, -1 }, { 0, 0, 0 }, 2 }, { { -1, -1, -1 }, { 0, 0, 0 }, 1 }, { { -1, -1, -1 }, { 0, 0, 0 }, 0 },
    { {  2,  2,  2 }, { 1, 1, 1 }, 5 }, { {  2,  2,  2 }, { 0, 0, 0 }, 1 }, { {  2,  2,  2 }, { 1, 1, 1 }, 5 }
};
typedef struct { int32_t flg; RECVX_ENEMY_VEC ofp; float rx, ry, rz; } EN06_BLOOD_TBL;
static const EN06_BLOOD_TBL blood_tbl[23] = {
    { 1, {  0.0f, 0.0f,  0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 1.0f, 1.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 1.0f, 1.0f, 1.0f }, { 0, { 2.0f, 0.0f, -1.0f }, 1.0f, 0.0f, 1.0f },
    { 0, { -2.0f, 0.0f, -1.0f }, 1.0f, 0.0f, 1.0f }, { 0, { 1.0f, 0.0f, 1.0f }, 1.0f, 0.0f, 1.0f },
    { 0, { -1.0f, 0.0f,  1.0f }, 1.0f, 0.0f, 1.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 0.0f, 0.0f, 0.0f }, { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, {  0.0f, 0.0f,  0.0f }, 0.0f, 0.0f, 0.0f }
};
static const NJS_POINT3 collision_offset[4][3] = {
    { { 0.0f, 0.0f, 0.0f }, {  5.0f, -0.5f, -2.5f }, {  3.5f, -0.5f, 1.5f } },
    { { 0.0f, 0.0f, 0.0f }, { -5.0f, -0.5f, -2.5f }, { -3.5f, -0.5f, 1.5f } },
    { { 0.0f, 0.0f, 0.0f }, {  3.0f, -0.5f,  2.0f }, {  0.5f, -0.3f, 5.0f } },
    { { 0.0f, 0.0f, 0.0f }, { -3.0f, -0.5f,  2.0f }, { -0.5f, -0.3f, 5.0f } }
};
#define NJM_DEG_ANG06(d) ((int32_t)((d) * 182.04445f))

/* --- Dienste --------------------------------------------------------------- */
static const RECVX_EN06_SERVICES* en06_sv;
void recvx_en06_bind(const RECVX_EN06_SERVICES* services) { en06_sv = services; }
RECVX_EN06_EXP0* recvx_en06_exp0(RECVX_ENEMY_WORK* w) { return w ? (RECVX_EN06_EXP0*)w->exp0 : NULL; }
static RECVX_EN06S_EXP0* s_exp0(RECVX_ENEMY_WORK* w) { return (RECVX_EN06S_EXP0*)w->exp0; }
static RECVX_EN06_EXP1* x1(RECVX_ENEMY_WORK* w) { return (RECVX_EN06_EXP1*)w->exp1; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name) { if (w && w->sv && w->sv->gap) w->sv->gap(name); }
static int32_t rnd(const RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->random_next) return w->sv->random_next();
    gap(w, "rand (en06)"); return 0;
}
/* macros.h:105 njRandom() = (float)-rand() / (float)(RAND_MAX + 1): [0, 1]. */
static float njrand(const RECVX_ENEMY_WORK* w) { return (float)rnd(w) / 2147483648.0f; }
/* en06sub.c:95 `-rand() / -2.1474836E9f`: derselbe Wert ueber den Divisor -2^31. */
static float subrand(const RECVX_ENEMY_WORK* w) { return (float)(-rnd(w)) / -2.1474836E9f; }
static int player(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl, const char* where)
{
    if (w->sv && w->sv->player_get && w->sv->player_get(pl)) return 1;
    gap(w, where); return 0;
}
static void player_set(const RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* pl)
{
    if (w->sv && w->sv->player_set) w->sv->player_set(pl); else gap(w, "plp (en06 Schreibzugriff)");
}
static uint32_t frames(const RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    if (w->sv && w->sv->motion_frames) return w->sv->motion_frames(w, mtn);
    gap(w, "mnwP[mtn_no].frm_num (en06)"); return 0u;
}
static void call_se(RECVX_ENEMY_WORK* w, int se) { RECVX_ENEMY_VEC p = { w->px, w->py, w->pz }; recvx_t3_call_se(w, &p, se); }
static void add_speed(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_CONTEXT* c = recvx_en01_context();
    if (c && c->en01 && c->en01->add_speed) { c->en01->add_speed(w, 0); return; }
    gap(w, "bhAddSpeed (en06)");
}
static void node_hidden(RECVX_ENEMY_WORK* w, uint32_t node, int hidden)
{
    if (w->sv && w->sv->model_node_hidden) w->sv->model_node_hidden(w, node, hidden);
    else gap(w, "mlwP->objP[n].evalflags (en06)");
}
/* bhCollisionCheckLine (hitchkl.c:12): `to` wird auf den Treffer gekuerzt. */
static int line1(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* from, RECVX_ENEMY_VEC* to,
                 RECVX_ENEMY_ATR* rec, RECVX_ENEMY_VEC* normal)
{
    RECVX_ENEMY_VEC n = { 0.0f, 0.0f, 0.0f };
    if (!(w->sv && w->sv->line)) { gap(w, "bhCollisionCheckLine (en06)"); return 0; }
    if (!w->sv->line(1u, from, to, &n, 0u, -1, rec, NULL)) return 0;
    if (normal) *normal = n;
    return 1;
}
static void unit(float* v)
{
    NJS_VECTOR t; t.x = v[0]; t.y = v[1]; t.z = v[2];
    njUnitVector(&t);
    v[0] = t.x; v[1] = t.y; v[2] = t.z;
}
static float shadow_sy(RECVX_ENEMY_WORK* w)
{
    float sy = 0.0f;
    const RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    if (!en06_sv || !en06_sv->effect_sy || p->shadow < 0 || !en06_sv->effect_sy(p->shadow, &sy, 0)) gap(w, "((BH_PWORK*)EXP0_I(0x84))->sy (en06)");
    return sy;
}
static void shadow_sy_set(RECVX_ENEMY_WORK* w, float sy)
{
    const RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    if (!en06_sv || !en06_sv->effect_sy || p->shadow < 0 || !en06_sv->effect_sy(p->shadow, &sy, 1)) gap(w, "((BH_PWORK*)EXP0_I(0x84))->sy (en06)");
}

static void en06_init(RECVX_ENEMY_WORK* w);
static void en06_move(RECVX_ENEMY_WORK* w);
static void en06_nage(RECVX_ENEMY_WORK* w);
static void en06_damage(RECVX_ENEMY_WORK* w);
static void en06_die(RECVX_ENEMY_WORK* w);
static void en06_player_control(RECVX_ENEMY_WORK* w);
static void en06_floor_collision(RECVX_ENEMY_WORK* w);
static void en06_collision_walls(RECVX_ENEMY_WORK* w);
static void en06_set_rinpun(RECVX_ENEMY_WORK* w, int num, int flg);

/* --- bhEne06 (en06.c:229) ---------------------------------------------------- */
void recvx_en06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p;
    if (!w) return;
    if (w->flg & 0x1000000u) {                                          /* Anfuehrer :231-239 */
        RECVX_ENEMY_PLAYER pl;
        if (!w->exp1) { gap(w, "*(int*)epw->exp1 (en06 Anfuehrer ohne exp1)"); return; }
        if ((x1(w)->flags & 1) && player(w, &pl, "plp (en06 Anfuehrer)") && pl.mode0 == 4u) en06_player_control(w);
        return;
    }
    switch (w->mode0) {                                                 /* bhEne06_Mode0[6] :178 */
    case 0: en06_init(w); break;
    case 1: en06_move(w); break;
    case 2: en06_nage(w); break;
    case 3: en06_damage(w); break;
    case 4: en06_die(w); break;
    case 5: if (w->sv && w->sv->event) w->sv->event(w); else gap(w, "bhEne_Event (subpl.c:143)"); break;
    default: gap(w, "bhEne06_Mode0[mode0] ausserhalb der Tabelle"); return;
    }
    p = recvx_en06_exp0(w);
    if (!p) return;
    if (w->sv && w->sv->set_motion) (void)w->sv->set_motion(w, w->mtn_add, w->mtn_md);   /* :245 */
    else gap(w, "bhSetMotion (en06)");
    w->car += (p->car_target - w->car) / 8.0f;                          /* :247 */
    if (w->sv && w->sv->check_player) (void)w->sv->check_player(w); else gap(w, "bhCheckPlayer (en06)");
    if (w->flg & 0x8u) recvx_t3_check_enemies_ball(w);                  /* :251 */
    en06_floor_collision(w);                                            /* :256 */
    if (w->flg & 0x10u) en06_collision_walls(w);
    if (w->sv && w->sv->calc_model) w->sv->calc_model(w); else gap(w, "bhCalcModel (en06)");
    if (w->sv && w->sv->weapon_atr) w->sv->weapon_atr(w, 10, 1, 2.0f); else gap(w, "bhEne_SetWeponAtr (en06)");
}

/* bhEne06_Init (en06.c:272). */
static void en06_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p;
    int i, hp_idx;
    w->flg |= 0x78u;
    w->flg &= ~0x9006u;
    w->flg &= ~0x800000u;
    w->ar = 5.0f; w->ah = 1.0f; w->car = 3.0f; w->cah = 3.0f;
    hp_idx = (int)(16.0f * njrand(w));                                  /* :293 */
    if (hp_idx < 16) w->hp = ene06_hitpoint[hp_idx];
    else gap(w, "ENE06_HITPOINT[16] (en06.c:293, njRandom() == 1.0 liest hinter die Tabelle)");
    w->hokan_rate = 65536; w->hokan_count = 0;
    w->mtn_no = 4; w->mtn_md = 0; w->mtn_add = 65536; w->frm_no = 0;
    w->mode0 = 1; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
    if (!en06_sv || !en06_sv->model_flags || !en06_sv->model_flags(w, ~3u, 0u)) gap(w, "owP[i].flg &= ~3 (en06.c:307)");
    if (!w->exp0) {                                                     /* :313 */
        RECVX_ENEMY_TEMPLATE tmpl;
        if (!(w->sv && w->sv->calloc_work)) { gap(w, "bhEne_CallocWork (en06)"); return; }
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN06_EXP0));
        if (!w->exp0) { gap(w, "bhEne_CallocWork(176, 8) (en06)"); return; }
        {   /* :316-345: der erste Work mit flg & 1 und id 6 liefert exp1/exp2. */
            const uint32_t n = (w->sv->enemy_count) ? w->sv->enemy_count() : 0u;
            uint32_t k;
            for (k = 0; k < n; ++k) {
                RECVX_ENEMY_WORK* ep = w->sv->enemy_at ? w->sv->enemy_at(k) : NULL;
                if (!(ep && (ep->flg & 1u) && ep->id == 6u)) continue;
                if (ep->exp1) w->exp1 = ep->exp1;
                else {
                    w->exp1 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN06_EXP1));
                    if (!w->exp1) { gap(w, "bhEne_CallocWork(12, 8) (en06)"); return; }
                }
                if (ep->exp2) w->exp2 = ep->exp2;
                else {
                    RECVX_ENEMY_WORK* leader;
                    if (!en06_sv || !en06_sv->create_child) { gap(w, "eneset.c:170 Kindwork (Anfuehrer) fehlt (en06)"); return; }
                    memset(&tmpl, 0, sizeof tmpl); tmpl.flg = 0x01008001u; tmpl.id = 6;
                    leader = en06_sv->create_child(w, &tmpl);
                    if (!leader) { gap(w, "bhEne06_Init: ene[] voll (Anfuehrer)"); return; }
                    w->exp2 = leader;
                    leader->mdflg |= 0x21u;
                    leader->exp1 = w->exp1;
                    /* ep_leader->mnwP = epw->mnwP: der Anfuehrer traegt id 6 und spielt
                       dieselbe Bibliothek emtp[6] (create_child, Raumform nicht im Raum). */
                }
                break;
            }
            if (k == n) { gap(w, "bhEne06_Init: kein Work mit id 6 in der ene-Liste"); return; }
        }
        p = recvx_en06_exp0(w);
        if (!en06_sv || !en06_sv->create_child) { gap(w, "eneset.c:170 Kindwork (Fragment) fehlt (en06)"); return; }
        memset(&tmpl, 0, sizeof tmpl); tmpl.flg = 0x8001u; tmpl.id = 31;
        for (i = 0; i < 14; i++) {                                      /* :347-359 */
            RECVX_ENEMY_WORK* c = en06_sv->create_child(w, &tmpl);
            if (!c) { gap(w, "bhEne06_Init: ene[] voll (Fragment)"); return; }
            p->parts[i] = c;
            c->lkwkp = w; c->lkono = i;
            c->lox = 0.0f; c->loy = 0.0f; c->loz = 0.0f;
            c->mdflg |= 1u;
            /* mtx = &epw->mlwP->owP[BrokenParts[i]].mtx: das Fragment liest und
               schreibt diese Matrix ueber lkwkp/lkono (recvx_en06s). BrokenParts hat
               zwoelf Eintraege; i 12/13 lesen dahinter, ihre Fragmente werden aber
               nie aktiviert (DD03 :1738 und DG00/01 :1287 nur i < 12). */
        }
        if (!en06_sv->set_call_func) { gap(w, "bhEne_SetCallFunc(bhEne06s, 31)"); return; }
        en06_sv->set_call_func(31u, recvx_en06s);
    }
    p = recvx_en06_exp0(w);
    p->ar_target = w->ar;                                               /* :363 */
    p->car_target = w->car;
    p->rinpun = (int)(30.0f * njrand(w)) + 20;
    if (!(w->flg & 0x800u)) {                                           /* :367 */
        int sdw = -1;
        if (w->sv && w->sv->set_shadow) sdw = w->sv->set_shadow(sdw_tab, w, 0, 5.0f, 4.0f, 4.0f);
        else gap(w, "bhSetShadow (en06)");
        if (sdw >= 0 && w->sv->effect_set_mdlver) w->sv->effect_set_mdlver(sdw, 1u);
        else gap(w, "eff[sdw].mdlver = 1 (en06.c:370)");
        w->flg |= 0x800u;
        p->shadow = sdw;
    }
    w->stflg &= ~8u;
    w->clp_jno[0] = 11; w->clp_jno[1] = 12; w->clp_jno[2] = 13; w->clp_jno[3] = 14;
    w->clp_jno[4] = 1; w->clp_jno[5] = 2; w->clp_jno[6] = -1;
    w->cpcl = cap_col_tab;
    w->flg |= 0x100000u;
    node_hidden(w, 1, 0);                                               /* :387-388 */
    node_hidden(w, 2, 0);
}

/* bhEne06_SearchPlayer (en06.c:1776). */
static void en06_search_player(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    float dx, dz;
    p->searched = 0;
    if (!player(w, &pl, "plp (en06 SearchPlayer)")) return;
    dx = w->px - pl.pos.x; dz = w->pz - pl.pos.z;
    p->dist = njSqrt(dx * dx + dz * dz);
    p->searched = 1;
}
/* bhEne06_BR00 (en06.c:398). */
static void en06_br00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    en06_search_player(w);
    if (!(w->flg & 0x400000u)) return;
    if (p->dist < 20.0f && w->mode2 != 3) { w->mode1 = 1; w->mode2 = 3; w->mode3 = 0; }
    if (!player(w, &pl, "plp (en06 BR00)")) return;
    if (!(!(pl.flg & 4u) && !(pl.stflg & 0x80000000u))) return;        /* :410 */
    if ((!(pl.stflg & 0x8000000u) || !(pl.stflg & 0x10000000u) || !(pl.stflg & 0x20000000u))
        && p->dist < 15.0f
        && fabsf((w->py - pl.pos.y) - 15.0f) < 8.0f
        && abs((int16_t)(w->ay - pl.ay)) < NJM_DEG_ANG06(90)
        && abs(recvx_t3_dir_target(w, pl.pos.x, pl.pos.z, NJM_DEG_ANG06(90))) < NJM_DEG_ANG06(90)) {
        w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
        pl.flg |= 0x10004u;
        pl.stflg |= 0x10000u;
        player_set(w, &pl);
    }
}
/* EXP0_I(0x74): Schuppenstaub waehrend des Flugs (MV01 :539, MV02 :658, MV03 :758). */
static void rinpun_tick(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* pl)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    if (pl->flg & 4u) { p->rinpun = (int)(30.0f * njrand(w)) + 60; return; }
    p->rinpun--;
    if (p->rinpun < 0) {
        en06_set_rinpun(w, 1, 0);
        if (p->rinpun < -5) p->rinpun = (int)(30.0f * njrand(w)) + 30;
    }
}
/* MV02/MV03/MV05 :593-604: Steigschritt EXP0_F(0x20) auf yn, begrenzt auf 0.4. */
static float climb(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    p->vel[1] += 0.1f * (w->yn - w->py);
    if (p->vel[1] > 0.4f) p->vel[1] = 0.4f;
    if (p->vel[1] < -0.4f) p->vel[1] = -0.4f;
    w->py += p->vel[1];
    return p->vel[1];
}
static int en06_avoid_wall(RECVX_ENEMY_WORK* w, int dir);
static int en06_check_leaning_wall(RECVX_ENEMY_WORK* w);

/* bhEne06_MV00 (en06.c:454). */
static void en06_mv00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 4;
        w->frm_no = (int32_t)((float)frames(w, w->mtn_no) * (32768.0f * njrand(w)));
        w->hokan_count = 0;
        w->flg |= 0x180000u; w->flg &= ~0x400000u;
        if (njrand(w) < 0.8f) w->ct0 = (int)(30.0f * njrand(w)) + 30;
        else w->ct0 = (int)(90.0f * njrand(w)) + 120;
        w->mode3++;
        /* fallthrough */
    case 1:
        w->ax += (int16_t)-w->ax / 8;
        if (w->ct0-- == 0) { w->mode1 = 0; w->mode2 = 1; w->mode3 = 0; }
        break;
    }
}
/* bhEne06_MV01 (en06.c:492). */
static void en06_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    switch (w->mode3) {
    case 0:
        w->ct1 = (njrand(w) > 0.5f);
        w->mtn_no = 0; w->frm_no = 0; w->hokan_count = 5; w->hokan_rate = 32768;
        w->flg |= 0x580000u;
        w->ar = 0.1f; p->ar_target = 5.0f; w->spd = 1.0f;
        w->ayp = w->ct1 ? w->ay + 16384 : w->ay - 16384;
        w->ct0 = 14; w->ct2 = 8;
        w->mode3++;
        break;
    case 1:
        if (w->ct0 != 0) {
            w->ay += (int16_t)(w->ayp - w->ay) / w->ct0;
            w->px -= w->spd * njSin(w->ayp);
            w->pz -= w->spd * njCos(w->ayp);
            w->ct0--;
        } else {
            w->mtn_no = 0; w->frm_no = 0;
            w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;
        }
        if (player(w, &pl, "plp (en06 MV01)")) rinpun_tick(w, &pl);
        if (w->ct2 != 0) {                                              /* :557 */
            const float sy = shadow_sy(w);
            shadow_sy_set(w, sy + (4.0f - sy) / (float)w->ct2);
            w->ct2--;
        }
        break;
    }
}
/* bhEne06_MV02 (en06.c:568). */
static void en06_mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER pl;
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 0) { w->mtn_no = 0; w->frm_no = 0; w->hokan_count = 5; w->hokan_rate = 32768; }
        w->flg |= 0x580000u; w->flg &= ~0x2180000u;
        w->spd = 1.0f;
        w->ct0 = (int)(128.0f * njrand(w)) + 90;
        w->ct1 = (njrand(w) > 0.5f) ? 1 : -1;
        w->ct2 = 20;
        w->ct3 = (int)(20.0f * njrand(w)) + 10;
        if (!player(w, &pl, "plp (en06 MV02)")) return;
        w->yn = 10.0f + pl.pos.y + (20.0f * njrand(w));
        w->ayp = (int32_t)((float)w->ay + ((7281.0f * njrand(w)) - 3640.0f));
        recvx_en06_exp0(w)->vel[1] = 0.0f;
        w->mode3++;
        /* fallthrough */
    case 1:
        if (climb(w) > 0.0f) w->ax += (int16_t)(-3640 - w->ax) / 16;
        else w->ax += (int16_t)(-10922 - w->ax) / 16;
        add_speed(w);
        if (w->ct0 > 0) {
            w->ct0--;
            if (en06_avoid_wall(w, w->ct1)) w->flg |= 0x2000000u;
            else if (!(w->flg & 0x2000000u)) w->ay += (int16_t)(w->ayp - w->ay) / 16;
        } else if (en06_check_leaning_wall(w)) {
            w->mode1 = 1; w->mode2 = 8; w->mode3 = 0;
        } else if (en06_avoid_wall(w, w->ct1)) {
            w->flg |= 0x2000000u;
        } else if (!(w->flg & 0x2000000u)) {
            w->ay += (int16_t)(w->ayp - w->ay) / 16;
        }
        if (!player(w, &pl, "plp (en06 MV02)")) return;
        if (w->ct3-- == 0) {
            w->yn = 15.0f + pl.pos.y + (10.0f * njrand(w));
            w->ayp = (int32_t)((float)w->ay + ((7281.0f * njrand(w)) - 3640.0f));
            w->ct3 = (int)(10.0f * njrand(w)) + 20;
            w->flg &= ~0x2000000u;
        }
        if (w->ct2-- == 0) { call_se(w, 74497); w->ct2 = (int)(5.0f * njrand(w)) + 20; }
        rinpun_tick(w, &pl);
        break;
    }
}
/* bhEne06_MV03 (en06.c:678). */
static void en06_mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER pl;
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != 0) { w->mtn_no = 0; w->frm_no = 0; w->hokan_count = 5; w->hokan_rate = 32768; }
        w->flg |= 0x580000u; w->flg &= ~0x2180000u;
        w->spd = 1.0f;
        w->ct0 = (int)(63.0f * njrand(w)) + 20;
        w->ct1 = (njrand(w) > 0.5f) ? 1 : -1;
        w->ct2 = 20;
        w->ct3 = (int)(20.0f * njrand(w)) + 10;
        if (!player(w, &pl, "plp (en06 MV03)")) return;
        w->yn = 15.0f + pl.pos.y + (10.0f * njrand(w));
        w->ayp = recvx_t3_arc_tan2(w->px - pl.pos.x, w->pz - pl.pos.z);
        recvx_en06_exp0(w)->vel[1] = 0.0f;
        w->mode3++;
        /* fallthrough */
    case 1:
        if (climb(w) > 0.0f) w->ax += (int16_t)(-w->ax) / 8;
        else w->ax += (int16_t)(-3640 - w->ax) / 8;
        add_speed(w);
        if (en06_avoid_wall(w, w->ct1)) w->flg |= 0x2000000u;
        else if (!(w->flg & 0x2000000u)) w->ay += (int16_t)(w->ayp - w->ay) / 16;
        if (!player(w, &pl, "plp (en06 MV03)")) return;
        if (w->ct3-- == 0) {
            w->yn = 10.0f + pl.pos.y + (10.0f * njrand(w));
            w->ayp = (int32_t)((float)recvx_t3_arc_tan2(w->px - pl.pos.x, w->pz - pl.pos.z) + ((7281.0f * njrand(w)) - 3640.0f));
            w->ct3 = (int)(10.0f * njrand(w)) + 20;
        }
        if (w->ct2-- == 0) { call_se(w, 74497); w->ct2 = 20; }
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; }
        rinpun_tick(w, &pl);
        break;
    }
}
/* bhEne06_MV05 (en06.c:786). */
static void en06_mv05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 0; w->frm_no = 0; w->hokan_count = 10; w->hokan_rate = 32768;
        w->mode3++;
        w->ct0 = (int)(8.0f * njrand(w)) + 14;
        w->ayp = w->ay + 32768;
        w->spd = 1.0f;
        w->flg |= 0x50u;
        w->car = 0.1f; p->car_target = 3.0f;
        w->ar = 0.1f; p->ar_target = 5.0f;
        if (!player(w, &pl, "plp (en06 MV05)")) return;
        w->yn = 15.0f + pl.pos.y + (20.0f * njrand(w));
        p->vel[1] = 0.0f;
        /* fallthrough */
    case 1:
        if (climb(w) > 0.0f) w->ax += (int16_t)(-w->ax) / 8;
        else w->ax += (int16_t)(-3640 - w->ax) / 8;
        if (w->ct0 != 0) {
            w->ay += (int16_t)(w->ayp - w->ay) / w->ct0;
            w->px -= w->spd * njSin(w->ayp);
            w->pz -= w->spd * njCos(w->ayp);
            w->ct0--;
            break;
        }
        w->car = 3.0f;
        w->mode1 = 0; w->mode2 = 2; w->mode3 = 0;
        w->ax = 0;
        break;
    }
}
/* bhEne06_MV08 (en06.c:871): an der Wand anlehnen. */
static void en06_mv08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    RECVX_ENEMY_VEC pos;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 3; w->frm_no = 0; w->hokan_count = 5; w->hokan_rate = 0x8000;
        w->flg |= 0x580000u; w->flg &= ~0x400000u;
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        w->ct1 = 8;
        w->mode3++;
        /* fallthrough */
    case 1:
        if (w->ct1 != 0) {
            w->ax += (int16_t)-w->ax / w->ct1;
            w->ay += (int16_t)(w->ayp - w->ay) / w->ct1;
            w->px = w->px + ((p->lean[0] - w->px) / (float)w->ct1);
            w->pz = w->pz + ((p->lean[2] - w->pz) / (float)w->ct1);
            { const float sy = shadow_sy(w); shadow_sy_set(w, sy + (2.0f - sy) / (float)w->ct1); }
            w->ct1--;
        }
        if (w->ct0-- == 0) {
            if (!recvx_t3_get_parts_pos(w, junction_tree, &pos)) return;
            w->px = pos.x; w->py = pos.y; w->pz = pos.z;
            w->ay = w->ayp;
            w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
            w->mtn_no = 4; w->frm_no = 0;
        }
        break;
    }
}
static int en06_dead_check(RECVX_ENEMY_WORK* w);
/* bhEne06_MV09/MV10 (en06.c:927/:998): Torkeln ohne Fluegel bis zum Tritt. */
static void en06_mv_crawl(RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != mtn) { w->mtn_no = mtn; w->frm_no = 0; w->hokan_count = 5; w->hokan_rate = 32768; }
        w->flg &= ~0x180000u; w->flg |= 0x800000u;
        p->vel[0] = 0.0f; p->vel[1] = 0.0f; p->vel[2] = 0.0f;
        w->flg |= 0x200000u;
        w->ct2 = (int)(20.0f * njrand(w)) + 10;
        w->mode3++;
        /* fallthrough */
    case 1:
        w->ax += (int16_t)(-w->ax) / 8;
        w->ay += (int)(1820.0f * njrand(w));
        w->ay -= 910;
        w->spd = 0.2f * njrand(w);
        add_speed(w);
        if (en06_dead_check(w)) {
            RECVX_ENEMY_VEC pos;
            const RECVX_T3_LINK lk = { w, 0 };
            w->frm_no = 0; w->mtn_add = 0; w->hokan_count = 5; w->hokan_rate = 32768;
            w->flg |= 2u; w->flg &= ~0x28u;
            w->dvx = 0.0f; w->dvy = -1.0f; w->dvz = 0.0f;
            recvx_t3_set_blood_effect_burst(lk, 3, 1, NULL, 0);
            pos.x = w->px; pos.y = w->py; pos.z = w->pz;
            if (w->sv && w->sv->blood_pool) w->sv->blood_pool(w, &pos, w->ay, &blood_param); else gap(w, "bhEne_BloodPool (en06)");
            call_se(w, 8965);
            w->mode3++;
        }
        if (!(w->flg & 0x200000u)) {
            p->vel[0] = 0.0f; p->vel[1] = 0.0f; p->vel[2] = 0.0f;
            w->flg |= 0x200000u;
            return;
        }
        w->px += p->vel[0]; w->py += p->vel[1]; w->pz += p->vel[2];
        p->vel[1] -= 0.2f;
        if (w->ct2-- == 0) { call_se(w, 74498); w->ct2 = (int)(20.0f * njrand(w)) + 10; }
        break;
    }
}
/* bhEne06_Move (en06.c:434). */
static void en06_move(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) {                                                 /* bhEne06_MoveMode2[11] :193 */
    case 0: en06_mv00(w); break;
    case 1: en06_mv01(w); break;
    case 2: en06_mv02(w); break;
    case 3: en06_mv03(w); break;
    case 4: case 6: case 7: break;                                      /* MV04/MV06/MV07 leer */
    case 5: en06_mv05(w); break;
    case 8: en06_mv08(w); break;
    case 9: en06_mv_crawl(w, 15); break;
    case 10: en06_mv_crawl(w, 16); break;
    default: gap(w, "bhEne06_MoveMode2[mode2] ausserhalb der Tabelle"); return;
    }
    if ((w->flg & 4u) && !(w->flg & 2u)) {
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
        w->flg &= ~4u;
        en06_damage(w);
        return;
    }
    if (w->mode1 == 1) {                                                /* bhEne06_Brain :392 */
        if (w->type == 0) en06_br00(w);
        else gap(w, "bhEne06_BrainType[type] ausserhalb der Tabelle");
    }
}

/* bhEne06_NG00 (en06.c:1079): Griff, Ablage des Mottenei. */
static void en06_nage(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER pl;
    if (w->mode2 != 0) { gap(w, "bhEne06_NageMode2[mode2] ausserhalb der Tabelle"); return; }
    switch (w->mode3) {
    case 0:
        w->mtn_no = 6; w->frm_no = 0; w->hokan_count = 10; w->hokan_rate = 32768;
        if (!w->exp1) { gap(w, "*(int*)epw->exp1 (en06 NG00)"); return; }
        x1(w)->flags |= 1;
        if (!player(w, &pl, "plp (en06 NG00)")) return;
        pl.mode0 = 4; pl.mode1 = 0; pl.mode2 = 0; pl.mode3 = 0;      /* *(int*)&plp->mode0 = 4 */
        pl.flg |= 0x10004u;
        pl.ayp = recvx_t3_arc_tan2(w->px - pl.pos.x, w->pz - pl.pos.z);
        player_set(w, &pl);
        w->flg &= ~0x70u; w->flg |= 0x580000u;
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        w->mode3++;
        w->ct2 = 0; w->ct3 = 12;
        /* fallthrough */
    case 1:
        w->ct2 += (w->sv && w->sv->lever_check) ? w->sv->lever_check() : recvx_t3_lever_check(w);
        if (w->frm_no == 655360) {                                      /* :1107 */
            if (w->sv && w->sv->vibration) w->sv->vibration(1, 11); else gap(w, "StartVibrationEx(1, 11) (en06)");
        }
        if (w->mtn_no == 6u) {
            float m[16];
            if (!(w->sv && w->sv->player_joint_matrix && w->sv->player_joint_matrix(3u, m))) { gap(w, "plp->mlwP->owP[3].mtx (en06 NG00)"); return; }
            {
                NJS_MATRIX mm; NJS_POINT3 offset = { 0.0f, 1.0f, 2.5f }, pos; NJS_VECTOR v = { 0.0f, 0.0f, -1.0f };
                memcpy(mm, m, sizeof mm);
                njCalcPoint(&mm, &offset, &pos);
                if (w->ct3 > 0) {
                    w->px = w->px + ((pos.x - w->px) / (float)w->ct3);
                    w->py = w->py + ((pos.y - w->py) / (float)w->ct3);
                    w->pz = w->pz + ((pos.z - w->pz) / (float)w->ct3);
                    njCalcVector(&mm, &v, &v);
                    w->ayp = recvx_t3_arc_tan2(-v.x, -v.z);
                    w->axp = recvx_t3_arc_tan2(v.y, njSqrt(v.x * v.x + v.z * v.z));
                    w->ax += (int16_t)(w->axp - w->ax) / w->ct3;
                    w->ay += (int16_t)(w->ayp - w->ay) / w->ct3;
                    w->ct3--;
                } else {
                    w->px = pos.x; w->py = pos.y; w->pz = pos.z;
                    w->flg |= 0x1000u;
                    memcpy(w->root_mtx, m, sizeof w->root_mtx);         /* njSetMatrix(epw->mtx, &owk->mtx) */
                    w->root_mtx[12] = w->px; w->root_mtx[13] = w->py; w->root_mtx[14] = w->pz;
                    njCalcVector(&mm, &v, &v);
                    w->ay = recvx_t3_arc_tan2(-v.x, -v.z);
                    w->ax = recvx_t3_arc_tan2(v.y, njSqrt(v.x * v.x + v.z * v.z));
                }
            }
        }
        if (w->ct0-- == 0) {
            w->flg |= 0x20u; w->flg &= ~0x1000u;
            w->mode0 = 1; w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;
        }
        if ((w->frm_no / 65536) == 29 && w->ct2 < 30) {                 /* :1161 */
            static const uint32_t flg[3] = { 0x8000000u, 0x10000000u, 0x20000000u };
            int type, i;
            call_se(w, 0x12303);
            type = (int)(3.0f * njrand(w));
            if (!player(w, &pl, "plp (en06 NG00 Ei)")) return;
            for (i = 0; i < 3; i++) {
                if (type > 2) { gap(w, "flg[type] (en06.c:1170, njRandom() == 1.0 liest hinter die Tabelle)"); return; }
                if (!(pl.stflg & flg[type])) { pl.stflg |= flg[type]; break; }
                type++;
                if (2 < type) type = 0;
            }
            player_set(w, &pl);
            {
                RECVX_ENEMY_TEMPLATE tmpl; RECVX_ENEMY_WORK* egg;
                memset(&tmpl, 0, sizeof tmpl); tmpl.flg = 0x8001u; tmpl.id = 0x1B;
                if (!en06_sv || !en06_sv->create_child) { gap(w, "eneset.c:170 Kindwork (ene27) fehlt (en06)"); return; }
                egg = en06_sv->create_child(w, &tmpl);
                if (!egg) { gap(w, "bhEne06_NG00: ene[] voll (ene27, en06.c:1182, Original schreibt NULL->type)"); return; }
                egg->type = (uint16_t)type;
                egg->mode2 = 1;
            }
        }
        break;
    }
}

/* bhEne06_HitMark (en06.c:2206). */
static void en06_hit_mark(RECVX_ENEMY_WORK* w)
{
    const EN06_BLOOD_TBL* blp;
    const EN06_DMG_REACT* dr;
    RECVX_ENEMY_VEC ofp;
    int i = 0, range;
    if (w->djnt_no < 0 || w->djnt_no >= 23) { gap(w, "BloodTbl[djnt_no] ausserhalb der Tabelle (en06)"); return; }
    if (w->wpnr_no < 0 || w->wpnr_no >= 21) { gap(w, "DmgReact[wpnr_no] ausserhalb der Tabelle (en06)"); return; }
    blp = &blood_tbl[w->djnt_no];
    dr = &dmg_react[w->wpnr_no];
    if (w->comb_flg & 0x10u) i = 0;
    if (w->comb_flg & 0x20u) i = 1;
    if (w->comb_flg & 0x40u) i = 2;
#define EN06_OFP() do { ofp = blp->ofp; \
        ofp.x += ((blp->rx * njrand(w)) - (blp->rx / 2.0f)); \
        ofp.y += (blp->ry * njrand(w)) - (blp->ry / 2.0f); \
        ofp.z += (blp->rz * njrand(w)) - (blp->rz / 2.0f); } while (0)
    if (dr->type[i] >= 0) {
        const RECVX_T3_LINK lk = { w, 0 };
        EN06_OFP();
        recvx_t3_set_blood_effect_burst(lk, dr->type[i], w->djnt_no, &ofp, 0);
        if (dr->bloodstain[i] != 0) recvx_t3_set_bloodstain(w, 0, w->djnt_no, &ofp);
    }
    if ((dr->exef & 1) && blp->flg == 0) {
        range = 0;
        do {
            float size; int len;
            EN06_OFP();
            size = 0.5f + (0.5f * njrand(w));                           /* ELF 0x1BEE88: Groesse vor Laenge */
            len = (int)(40.0f * njrand(w)) + 20;
            recvx_t3_set_fire_effect(w, w->djnt_no, &ofp, size, len);
            range++;
        } while (range < 4);
    }
    if (dr->exef & 2) {
        EN06_OFP();
        recvx_t3_set_acid_effect(w, w->djnt_no, &ofp, 2.0f);
    }
#undef EN06_OFP
    if (dr->exef & 4) {
        if (en06_sv && en06_sv->material_color) en06_sv->material_color(w, 0xFF201010u); else gap(w, "npSetAllMatColor (en06)");
        w->mdflg |= 0x400u;
    }
}
static void en06_dg(RECVX_ENEMY_WORK* w, uint32_t m_hit, uint32_t m_fall, uint32_t m_land, int next_mode2, int turn);
/* bhEne06_Damage (en06.c:1195). */
static void en06_damage(RECVX_ENEMY_WORK* w)
{
    uint32_t n = 0, i;
    int dam = 0, dir;
    if (w->sv && w->sv->motion_obj_num) n = w->sv->motion_obj_num(w, 0);
    else gap(w, "mnwP->obj_num (en06)");
    for (i = 0; i < n && i < RECVX_ENEMY_DAM; i++) {
        if (w->dam[i]) { dam += w->dam[i]; w->dam[i] = 0; }
    }
    w->hp -= dam;
    if (w->mode3 == 0) {
        en06_hit_mark(w);
        en06_set_rinpun(w, 8, 1);
        dir = recvx_t3_dg_dir_check(w);
        if (dam < 31 && (w->flg & 0x400000u) && njrand(w) < 0.4f) {
            w->mode2 = (dir != 0) ? 0 : 1;
        } else {
            w->mode0 = 4; w->mode1 = 0; w->mode3 = 0;
            if (w->flg & 0x800000u) w->mode2 = 4;
            else if (30 < dam) {
                w->mode2 = 3;
                w->dpx = w->px; w->dpy = w->py; w->dpz = w->pz;
                recvx_t3_set_mince_effect2(w, 2, 0.3f, 6);
                recvx_t3_set_mince_effect2(w, 259, 0.3f, 8);
            } else if (w->flg & 0x400000u) w->mode2 = (dir != 0) ? 1 : 2;
            else w->mode2 = 0;
            w->flg |= 2u; w->flg &= ~0x28u;
            en06_die(w);
            return;
        }
    }
    switch (w->mode2) {                                                 /* bhEne06_DamageMode2[2] */
    case 0: en06_dg(w, 10, 12, 14, 10, 0); break;
    case 1: en06_dg(w, 9, 11, 13, 9, 1); break;
    default: gap(w, "bhEne06_DamageMode2[mode2] ausserhalb der Tabelle"); break;
    }
}
/* Gemeinsamer Ablauf von DG00/DG01/DD00/DD01/DD02 (en06.c:1260-1726): Treffer-
   motion m_hit, dann Fall m_fall mit dem Wurzelversatz p[0], dann Landung m_land. */
static int fall_start(RECVX_ENEMY_WORK* w, uint32_t m_fall)
{
    RECVX_ENEMY_VEC pos, key;
    NJS_MATRIX m; NJS_VECTOR trans;
    if (!recvx_t3_get_parts_pos(w, junction_tree, &pos)) return 0;
    w->px = pos.x; w->py = pos.y; w->pz = pos.z;
    w->mtn_no = m_fall; w->frm_no = 0;
    w->flg &= ~0x80000u;
    if (!(w->sv && w->sv->motion_root_position && w->sv->motion_root_position(w, w->mtn_no, (uint32_t)(w->frm_no / 65536), &key))) {
        gap(w, "mnwP[mtn_no].md2P->p[0] (en06)"); return 0;
    }
    trans.x = key.x; trans.y = key.y; trans.z = key.z;
    njUnitMatrix(&m); njRotateY(&m, w->ay); njCalcVector(&m, &trans, &trans);
    w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
    return 1;
}
static void hit_start(RECVX_ENEMY_WORK* w, uint32_t m_hit, uint32_t clear)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    w->mtn_no = m_hit; w->frm_no = 0; w->hokan_count = 5; w->hokan_rate = 32768;
    w->flg |= 0x200000u; w->flg &= ~clear;
    p->vel[0] = w->dvx; p->vel[1] = w->dvy; p->vel[2] = w->dvz;
    unit(p->vel);
    p->vel[1] *= 0.5f;
}
/* DG00/DG01 (en06.c:1260/:1356): ein Fluegelstueck bricht ab, die Motte faellt und rappelt sich auf. */
static void en06_dg(RECVX_ENEMY_WORK* w, uint32_t m_hit, uint32_t m_fall, uint32_t m_land, int next_mode2, int turn)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    static const char parts[4] = { 0xB, 0xC, 0xD, 0xE };
    switch (w->mode3) {
    case 0: {
        int pno;
        hit_start(w, m_hit, 0x580000u);
        if (turn) p->car_target = 1.0f;                                 /* DG01 :1379 vor pno */
        pno = (int)(4.0f * njrand(w));
        if (pno > 3) { gap(w, "parts[pno] (en06 DG, njRandom() == 1.0)"); return; }
        if (!en06_sv || !en06_sv->owp_flag_or || !en06_sv->owp_flag_or(w, (uint32_t)parts[pno], 3u)) gap(w, "owk[parts[pno]].flg |= 3 (en06)");
        if (p->parts[pno]) { p->parts[pno]->mode0 = 1; p->parts[pno]->mode2 = 1; }
        else gap(w, "exp0 + 0x34 Fragment fehlt (en06 DG)");
        if (!turn) p->car_target = 1.0f;                                /* DG00 :1292 nach pno */
        call_se(w, 8964);
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        w->mode3++;
    }
        /* fallthrough */
    case 1:
        if (w->ct0-- == 0) {
            if (!fall_start(w, m_fall)) return;
            w->mode3++;
            w->flg |= 0x200000u;
        }
        /* fallthrough */
    case 2:
        if (!(w->flg & 0x200000u)) {
            w->mtn_no = m_land; w->frm_no = 0; w->hokan_count = 0;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3 = 3;
        } else {
            w->px += p->vel[0]; w->py += p->vel[1]; w->pz += p->vel[2];
            p->vel[1] -= 0.2f;
        }
        break;
    case 3:
        if (w->ct0-- == 0) {
            w->mode0 = 1; w->mode1 = 1; w->mode2 = (uint8_t)next_mode2; w->mode3 = 0;
            if (turn) { w->mtn_no = 15; w->frm_no = 0; w->hokan_count = 0; w->ay += 32768; }
            w->flg &= ~0x40u;
        }
        break;
    }
    w->ax += (int16_t)(-w->ax) / 8;
}
/* DD00/DD01/DD02 (en06.c:1458/:1551/:1640). */
static void en06_dd_fall(RECVX_ENEMY_WORK* w, uint32_t m_hit, uint32_t clear, uint32_t m_fall, uint32_t m_land, int dd00)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    switch (w->mode3) {
    case 0:
        hit_start(w, m_hit, clear);
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        if (dd00) { w->ar = 0.0f; p->ar_target = 5.0f; }
        call_se(w, 8964);
        w->mode3++;
        /* fallthrough */
    case 1:
        if (w->ct0-- == 0) {
            if (!fall_start(w, m_fall)) return;
            w->mode3++;
            w->flg |= 0x200000u;
        }
        /* fallthrough */
    case 2:
        if (!(w->flg & 0x200000u)) {
            w->mtn_no = m_land; w->frm_no = 0; w->hokan_count = 0;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3 = 3;
        } else {
            w->px += p->vel[0]; w->py += p->vel[1]; w->pz += p->vel[2];
            p->vel[1] -= 0.2f;
        }
        break;
    case 3:
        if (w->ct0-- == 0) {
            RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz };
            w->flg &= ~0x78u;
            w->mtn_add = 0;
            w->mode3++;
            if (w->sv && w->sv->blood_pool) w->sv->blood_pool(w, &pos, w->ay, &blood_param); else gap(w, "bhEne_BloodPool (en06)");
        }
        break;
    }
    w->ax += (int16_t)(-w->ax) / 8;
}
/* bhEne06_Die (en06.c:1452). */
static void en06_die(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    switch (w->mode2) {
    case 0: en06_dd_fall(w, 8, 0x180000u, 11, 13, 1); break;
    case 1: en06_dd_fall(w, 10, 0x580000u, 12, 14, 0); break;
    case 2: en06_dd_fall(w, 9, 0x580000u, 11, 13, 0); break;
    case 3:                                                             /* DD03 :1729 Zerfetzt */
        if (w->mode3 == 0) {
            uint32_t i;
            for (i = 0; i < 12u; i++) {
                if (!en06_sv || !en06_sv->owp_flag_or || !en06_sv->owp_flag_or(w, (uint32_t)recvx_en06_broken_parts[i], 3u)) gap(w, "owk[BrokenParts[i]].flg |= 3 (en06)");
                if (p->parts[i]) { p->parts[i]->mode0 = 1; p->parts[i]->mode2 = (uint8_t)(i < 4); }
                else gap(w, "exp0 + 0x34 Fragment fehlt (en06 DD03)");
            }
            node_hidden(w, 1, 1);
            node_hidden(w, 2, 1);
            w->stflg |= 8u;
            w->flg &= ~0x78u; w->flg |= 0x8000u;
            w->mtn_add = 0;
            call_se(w, 8960);
            w->mode3++;
        }
        break;
    case 4:                                                             /* DD04 :1759 */
        if (w->mode3 == 0) {
            RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz };
            w->frm_no = 0; w->mtn_add = 0; w->hokan_count = 5; w->hokan_rate = 32768;
            w->flg |= 2u; w->flg &= ~0x28u;
            if (w->sv && w->sv->blood_pool) w->sv->blood_pool(w, &pos, w->ay, &blood_param); else gap(w, "bhEne_BloodPool (en06)");
            w->mode3++;
        }
        break;
    default: gap(w, "bhEne06_DeadMode2[mode2] ausserhalb der Tabelle"); break;
    }
}

/* Wurzelversatz der Kollisionsproben (CollisionWalls :1796-1834, AvoidWall :1992-2025). */
static int probe_offsets(RECVX_ENEMY_WORK* w, NJS_VECTOR* ofp, NJS_VECTOR* trans)
{
    RECVX_ENEMY_VEC root;
    NJS_MATRIX m;
    if (w->flg & 0x400000u) { ofp->z = 0.0f; ofp->y = 0.0f; ofp->x = 0.0f; }
    else {
        ofp->z = 0.0f; ofp->x = 0.0f; ofp->y = w->ar;
        if (w->flg & 0x100000u) {
            int32_t a[3];
            if (!(w->sv && w->sv->motion_root_angles && w->sv->motion_root_angles(w, w->mtn_no, (uint32_t)(w->frm_no / 65536), a))) {
                gap(w, "mnwP[mtn_no].md2P->p[1] (en06)"); return 0;
            }
            njUnitMatrix(&m); njRotateY(&m, w->ay); njRotateXYZ(&m, a[0], a[1], a[2]);
            njCalcVector(&m, ofp, ofp);
        }
    }
    if (!(w->sv && w->sv->model_root_position && w->sv->model_root_position(w, &root))) { gap(w, "mlwP->objP->pos (en06)"); return 0; }
    trans->x = root.x; trans->y = root.y; trans->z = root.z;
    if (!(w->flg & 0x80000u)) trans->y = 0.0f;
    njUnitMatrix(&m); njRotateY(&m, w->ay); njCalcVector(&m, trans, trans);
    return 1;
}
/* bhEne06_CollisionWalls (en06.c:1789). */
static void en06_collision_walls(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    NJS_VECTOR ofp, trans;
    w->ar += (p->ar_target - w->ar) / 8.0f;
    if (!probe_offsets(w, &ofp, &trans)) return;
    w->px += trans.x; w->py += trans.y; w->pz += trans.z;
    w->px += ofp.x; w->py += ofp.y; w->pz += ofp.z;
    if (w->sv && w->sv->collision_works) p->atr = w->sv->collision_works(w); else gap(w, "bhEne03_Collision (en06)");
    w->px -= ofp.x; w->py -= ofp.y; w->pz -= ofp.z;
    w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
}
/* bhEne06_FloorCollision (en06.c:1852). */
static void en06_floor_collision(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    RECVX_ENEMY_VEC from = { w->pxb, w->pyb, w->pzb }, to = { w->px, w->py, w->pz }, n;
    RECVX_ENEMY_ATR hp;
    if (!line1(w, &from, &to, &hp, &n)) return;
    w->px = to.x; w->py = to.y; w->pz = to.z;                           /* p2 = &epw->px wird gekuerzt */
    p->floor_n[0] = n.x; p->floor_n[1] = n.y; p->floor_n[2] = n.z;
    unit(p->floor_n);
    p->atr = 1;
    if (p->floor_n[1] > 0.9f) {
        w->flg &= ~0x200000u;
        if (hp.type == 7u) { if (hp.py > w->py) w->py = hp.py; }
        else {
            float h = hp.h;
            if (h == 0.0f) {
                const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
                if (!(x && x->room_height)) { gap(w, "rom->h (en06.c:1877)"); return; }
                h = x->room_height();
            }
            if (hp.py + h > w->py) w->py = hp.py + h;
        }
    }
    if (p->floor_n[1] < -0.9f) p->vel[1] = 0.0f;                        /* EXP0_I(0x20) = 0 */
    if (fabsf(p->floor_n[1]) < 0.3f) { p->vel[0] = 0.0f; p->vel[2] = 0.0f; }
}
/* bhEne06_CheckLeaningWall (en06.c:1899). */
static int en06_check_leaning_wall(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    NJS_MATRIX m; NJS_VECTOR vec = { 0.0f, 0.0f, -10.0f };
    RECVX_ENEMY_VEC pos, from, n;
    RECVX_ENEMY_ATR hp;
    uint32_t i, cnt;
    njUnitMatrix(&m); njRotateY(&m, w->ay); njCalcVector(&m, &vec, &vec);
    pos.x = w->px + vec.x; pos.y = w->py; pos.z = w->pz + vec.z;
    cnt = (w->sv && w->sv->enemy_count) ? w->sv->enemy_count() : 0u;
    for (i = 0; i < cnt; i++) {
        const RECVX_ENEMY_WORK* ep = w->sv->enemy_at(i);
        if (ep && (ep->flg & 1u) && ep->id == 6u && ep != w) {
            const float dx = pos.x - ep->px, dy = pos.y - ep->py, dz = pos.z - ep->pz;
            if (njSqrt(dx * dx + dy * dy + dz * dz) < 10.0f) return 0;
        }
    }
    from.x = w->px; from.y = w->py; from.z = w->pz;
    if (line1(w, &from, &pos, &hp, &n) && !(hp.attr & 8u)) {
        float nv[3] = { n.x, n.y, n.z };
        unit(nv);
        w->ayp = recvx_t3_arc_tan2(nv[0], nv[2]);
        p->lean[0] = pos.x + nv[0]; p->lean[1] = pos.y + nv[1]; p->lean[2] = pos.z + nv[2];
        return 1;
    }
    return 0;
}
/* bhEne06_AvoidWall (en06.c:1952). */
static int en06_avoid_wall(RECVX_ENEMY_WORK* w, int dir)
{
    NJS_VECTOR ofp, trans;
    RECVX_ENEMY_VEC pos1, pos2, n;
    RECVX_ENEMY_ATR a1, a2;
    int hp1, hp2, ret = 0;
    const RECVX_ENEMY_WORK* ep2 = NULL;
    float wdist = 0.0f;
    uint32_t i, cnt = (w->sv && w->sv->enemy_count) ? w->sv->enemy_count() : 0u;
    for (i = 0; i < cnt; i++) {
        const RECVX_ENEMY_WORK* ep = w->sv->enemy_at(i);
        if (ep && (ep->flg & 1u) && ep->id == 6u && ep != w && !(ep->flg & 0x1000000u) && !(ep->flg & 2u)) {
            const float dist = njSqrt((w->px - ep->px) * (w->px - ep->px) + (w->pz - ep->pz) * (w->pz - ep->pz));
            if (ep2 == NULL || dist < wdist) { wdist = dist; ep2 = ep; }
        }
    }
    if (ep2 && fabsf(w->py - ep2->py) < 8.0f) {
        if (ep2->yn < w->py) w->yn += 3.0f; else w->yn -= 3.0f;
    }
    if (!probe_offsets(w, &ofp, &trans)) return 0;
    pos1.x = ofp.x + (w->px + trans.x); pos1.y = ofp.y + (w->py + trans.y); pos1.z = ofp.z + (w->pz + trans.z);
    pos2.x = pos1.x; pos2.y = pos1.y - 6.0f; pos2.z = pos1.z;
    hp1 = line1(w, &pos1, &pos2, &a1, &n);
    if (!hp1) {
        pos1.x = ofp.x + (w->px + trans.x);
        pos1.y = (ofp.y + (w->py + trans.y)) - 6.0f;
        pos1.z = ofp.z + (w->pz + trans.z);
        pos2.x = pos1.x - (10.0f * njSin(w->ay));
        pos2.y = ofp.y + (w->py + trans.y);
        pos2.z = pos1.z - (10.0f * njCos(w->ay));
        hp1 = line1(w, &pos1, &pos2, &a1, &n);
    }
    pos1.x = ofp.x + (w->px + trans.x); pos1.y = ofp.y + (w->py + trans.y); pos1.z = ofp.z + (w->pz + trans.z);
    pos2.x = pos1.x; pos2.y = pos1.y + 6.0f; pos2.z = pos1.z;
    hp2 = line1(w, &pos1, &pos2, &a2, &n);
    if (!hp2) {
        pos1.x = ofp.x + (w->px + trans.x);
        pos1.y = 6.0f + (ofp.y + (w->py + trans.y));
        pos1.z = ofp.z + (w->pz + trans.z);
        pos2.x = pos1.x - (10.0f * njSin(w->ay));
        pos2.y = ofp.y + (w->py + trans.y);
        pos2.z = pos1.z - (10.0f * njCos(w->ay));
        hp2 = line1(w, &pos1, &pos2, &a2, &n);
    }
    if (hp1 && hp2) { w->ay += dir * 1820; return 1; }
    if (hp1 && !hp2) {
        if (a1.type == 7u) w->yn = 6.0f + a1.py; else w->yn = 6.0f + (a1.py + a1.h);
        ret = 1;
        w->yn -= trans.y + ofp.y;
    }
    if (!hp1 && hp2) {
        if (a2.type == 7u) w->yn = (a2.py + a2.h) - 6.0f; else w->yn = a2.py - 6.0f;
        ret = 1;
        w->yn -= trans.y + ofp.y;
    }
    pos2.x = pos1.x - (3.0f * njSin(w->ay));
    pos2.y = (ofp.y + (w->py + trans.y)) - 5.0f;
    pos2.z = pos1.z - (3.0f * njCos(w->ay));
    if (!(w->sv && w->sv->wall_type)) { gap(w, "bhCheckWallType (en06)"); return ret; }
    if (w->sv->wall_type(&pos2, 0u, 2.0f, 10.0f)) { w->ay += dir * 1820; ret = 1; }
    return ret;
}
/* bhEne06_PlayerControl (en06.c:2112), im Anfuehrerwork. */
static void en06_player_control(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER pl;
    float m[16], dx, dz;
    if (!player(w, &pl, "plp (en06 PlayerControl)")) return;
    if (pl.mode2 != 0) return;
    switch (pl.mode3) {
    case 0:
        pl.flg &= ~0x40000u; pl.flg |= 0x10004u;
        pl.mtn_enemy = 1u; pl.mtn_enemy_owner = w->id; pl.mtn_enemy_work = w;   /* plp->mnwP = epw->mnwP */
        pl.mtn_no = 20; pl.frm_no = 0; pl.hokan_count = 5; pl.hokan_rate = 52428; pl.mtn_add = 65536;
        pl.ct1 = 8;
        pl.mode3++;
        player_set(w, &pl);
        break;
    case 1:
        if (pl.ct1 > 0) { pl.ay += (int16_t)(pl.ayp - pl.ay) / pl.ct1; pl.ct1--; }
        if (pl.frm_no == 0) {
            pl.mtn_enemy = 0u;                                          /* plp->mnwP = plp->mnwPb */
            pl.flg &= ~0x10004u; pl.flg |= 8u;
            pl.at_flg = 0;
            pl.stflg &= ~0x10000u;
            pl.mode0 = 1; pl.mode1 = 0; pl.mode2 = 0; pl.mode3 = 0;
            x1(w)->flags &= ~1;
        }
        if (!(w->sv && w->sv->player_joint_matrix && w->sv->player_joint_matrix(5u, m))) { gap(w, "plp->mlwP->owP[5].mtx (en06)"); player_set(w, &pl); return; }
        dx = m[12] - pl.pos.x; dz = m[14] - pl.pos.z;
        pl.pos.x += dx; pl.pos.z += dz;
        /* bhCheckWall(plp): plp->ar/ah = PlyInfo[ply_id] = car/cah + 1 (player.c:199/708). */
        if (en06_sv && en06_sv->player_wall) (void)en06_sv->player_wall(&pl, pl.car + 1.0f, pl.cah + 1.0f);
        else gap(w, "bhCheckWall(plp) (en06)");
        pl.pos.x -= dx; pl.pos.z -= dz;
        player_set(w, &pl);
        break;
    }
}
/* bhEne06_SetRinpunEffect (en06.c:2176): die while-Schleife bricht nach einem Satz ab. */
static void en06_set_rinpun(RECVX_ENEMY_WORK* w, int num, int flg)
{
    while (0 < num) {
        RECVX_ENEMY_VEC pos, scale;
        float size;
        int32_t ay;
        int eno;
        size = 0.6f + (0.4f * njrand(w));
        scale.x = size; scale.y = size; scale.z = 1.0f;
        ay = (int32_t)((float)(w->ay + 18204) + (29127.0f * njrand(w)));
        size = 8.0f * njrand(w);
        pos.x = w->px - (size * njSin(ay));
        pos.z = w->pz - (size * njCos(ay));
        pos.y = (w->py + (8.0f * njrand(w))) - 4.0f;
        if (!en06_sv || !en06_sv->effect_tb) { gap(w, "bhSetEffectTb (en06 Rinpun)"); break; }
        eno = en06_sv->effect_tb(260u, 1u, (uint32_t)flg, &pos, &scale, ay);
        if (eno != -1) {
            if (en06_sv->effect_texture) en06_sv->effect_texture(eno, w, 1u, 1);
            else gap(w, "eff[eno].txp[0] = epw->mdl[1].texP (en06)");
        }
        break;
    }
}
/* bhEne06_DeadCheck (en06.c:2269): Tritt des Spielers (Fussgelenke 17/21). */
static int en06_dead_check(RECVX_ENEMY_WORK* w)
{
    static const int joint[2] = { 17, 21 };
    RECVX_EN06_EXP0* p = recvx_en06_exp0(w);
    int i;
    for (i = 0; i < 2; i++) {
        float m[16], dx, dy, dz;
        NJS_VECTOR vec;
        float mov;
        int ang, wa;
        if (!(w->sv && w->sv->player_joint_matrix && w->sv->player_joint_matrix((uint32_t)joint[i], m))) { gap(w, "plp->mlwP->owP[17/21].mtx (en06)"); return 0; }
        vec.x = p->dead_pos[i][0] - m[12]; vec.y = p->dead_pos[i][1] - m[13]; vec.z = p->dead_pos[i][2] - m[14];
        mov = njScalor(&vec);
        wa = recvx_t3_arc_tan2(m[8], m[10]);
        ang = (int16_t)(wa - p->dead_ang[i]);
        if (ang < 0) ang = -ang;
        p->dead_pos[i][0] = m[12]; p->dead_pos[i][1] = m[13]; p->dead_pos[i][2] = m[14];
        p->dead_ang[i] = wa;
        dx = m[12] - w->px; dy = m[13] - w->py; dz = m[14] - w->pz;
        if ((mov > 0.1f || 182 < ang) && njSqrt(dx * dx + dy * dy + dz * dz) < 2.0f) return 1;
    }
    return 0;
}

/* ===========================================================================
   bhEne06s (en06sub.c:33) — Fluegelfragment auf owP[BrokenParts[lkono]] des
   Elternworks lkwkp. `epw->mtx` ist diese Knotenmatrix; gelesen/geschrieben
   ueber owp_matrix/owp_matrix_set.
   =========================================================================== */
static int s_node(RECVX_ENEMY_WORK* w, uint32_t* node)
{
    if (!w->lkwkp || w->lkono < 0 || w->lkono >= 12) { gap(w, "bhEne06s: epw->mtx ausserhalb BrokenParts[12] (en06.c:358)"); return 0; }
    *node = (uint32_t)recvx_en06_broken_parts[w->lkono];
    return 1;
}
static int mtx_get(RECVX_ENEMY_WORK* w, NJS_MATRIX* m)
{
    uint32_t node;
    if (!s_node(w, &node)) return 0;
    if (!en06_sv || !en06_sv->owp_matrix || !en06_sv->owp_matrix(w->lkwkp, node, (float*)m)) { gap(w, "bhEne06s: lkwkp->mlwP->owP[n].mtx"); return 0; }
    return 1;
}
static void mtx_put(RECVX_ENEMY_WORK* w, const NJS_MATRIX* m)
{
    uint32_t node;
    if (!s_node(w, &node)) return;
    if (!en06_sv || !en06_sv->owp_matrix_set || !en06_sv->owp_matrix_set(w->lkwkp, node, (const float*)m)) gap(w, "bhEne06s: lkwkp->mlwP->owP[n].mtx schreiben");
}
static void put_pos(RECVX_ENEMY_WORK* w)
{
    NJS_MATRIX m;
    if (!mtx_get(w, &m)) return;
    m[12] = w->px; m[13] = w->py; m[14] = w->pz;
    mtx_put(w, &m);
}
/* njRotate(NULL, &ov, ang / ct3); njMultiMatrix(NULL, epw->mtx); njGetMatrix(epw->mtx). */
static void s_turn(NJS_MATRIX* m, NJS_VECTOR* ov, int ang)
{
    NJS_MATRIX r;
    njUnitMatrix(&r);
    njRotate(&r, ov, ang);
    njMultiMatrix(&r, m);
    memcpy(*m, r, sizeof r);
}
/* bhEne06s_MV00 (en06sub.c:82): Rumpfstueck faellt und kippt auf den Boden. */
static void s_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06S_EXP0* p = s_exp0(w);
    NJS_MATRIX m;
    switch (w->mode3) {
    case 0:
        if (!mtx_get(w, &m)) return;
        w->px = w->pxb = m[12]; w->py = w->pyb = m[13]; w->pz = w->pzb = m[14];
        p->vel[0] = subrand(w) - 0.5f;
        p->vel[1] = subrand(w) - 0.2f;
        p->vel[2] = subrand(w) - 0.5f;
        p->flags |= 1u;
        w->flg |= 0x10u;
        w->ct3 = 8;
        w->mode3++;
        /* fallthrough */
    case 1:
        if (p->flags & 1u) {
            p->vel[1] -= 0.1f;
            w->px += p->vel[0]; w->py += p->vel[1]; w->pz += p->vel[2];
        } else w->mode3++;
        put_pos(w);
        break;
    case 2:
        if (!mtx_get(w, &m)) return;
        if (w->ct3 != 0) {
            NJS_VECTOR v, vd, ov;
            float out;
            int ang;
            v.x = p->n[0]; v.y = p->n[1]; v.z = p->n[2];
            njCalcVector(&m, &v, &vd);
            if (vd.y < 0) { vd.x *= -1.0f; vd.y *= -1.0f; vd.z *= -1.0f; }
            out = njOuterProduct(&vd, &v, &ov);
            njUnitVector(&ov);
            ang = njArcSin(out);
            s_turn(&m, &ov, ang / w->ct3);
            if (ang < 18) { w->flg &= ~0x10u; w->mode0 = 2; }
            w->ct3--;
        }
        m[12] = w->px; m[13] = w->py; m[14] = w->pz;
        mtx_put(w, &m);
        break;
    }
}
/* bhEne06s_MV01 (en06sub.c:170): Fluegel trudelt und legt sich auf die getroffene Kante. */
static void s_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06S_EXP0* p = s_exp0(w);
    NJS_MATRIX m;
    if (!mtx_get(w, &m)) return;
    switch (w->mode3) {
    case 0:
        w->px = w->pxb = m[12]; w->py = w->pyb = m[13]; w->pz = w->pzb = m[14];
        p->vel[0] = subrand(w) - 0.5f;
        p->vel[1] = subrand(w) - 0.2f;
        p->vel[2] = subrand(w) - 0.5f;
        p->flags |= 1u;
        w->flg |= 0x10u;
        njCalcPoints(&m, (NJS_POINT3*)collision_offset[w->lkono], (NJS_POINT3*)p->pts, 3);
        w->mode3++;
        /* fallthrough */
    case 1:
        if (p->flags & 1u) {
            NJS_VECTOR avec = { 0.0f, 0.0f, -0.1f };
            njCalcVector(&m, &avec, &avec);
            if (avec.y > 0) avec.y *= -1.0f;
            njRotateX(&m, 910); njRotateZ(&m, 364); njRotateY(&m, 546);
            p->vel[1] += avec.y - 0.01f;
            w->px += p->vel[0]; w->py += p->vel[1]; w->pz += p->vel[2];
        } else { w->mode3++; w->ct3 = 10; }
        break;
    case 2:
        if (w->ct3 > 0) {
            NJS_VECTOR vd, ov, nv;
            NJS_POINT3 pl[3];
            float out;
            int ang;
            if (p->hit < 0 || p->hit > 2) { gap(w, "bhEne06s: exp0[EXP0_I(64) * 12 + 28] ausserhalb"); return; }
            memcpy(pl, collision_offset[w->lkono], sizeof pl);
            njGetPlaneNormal(pl, &vd);
            njCalcVector(&m, &vd, &vd);
            if (vd.y < 0) { vd.x *= -1.0f; vd.y *= -1.0f; vd.z *= -1.0f; }
            njUnitVector(&vd);
            nv.x = p->n[0]; nv.y = p->n[1]; nv.z = p->n[2];
            out = njOuterProduct(&vd, &nv, &ov);
            njUnitVector(&ov);
            { NJS_VECTOR t; t.x = m[12]; t.y = m[13]; t.z = m[14]; njSubVector(&t, (NJS_VECTOR*)p->pts[p->hit]); m[12] = t.x; m[13] = t.y; m[14] = t.z; }
            ang = njArcSin(out);
            s_turn(&m, &ov, ang / w->ct3);
            { NJS_VECTOR t; t.x = m[12]; t.y = m[13]; t.z = m[14]; njAddVector(&t, (NJS_VECTOR*)p->pts[p->hit]); m[12] = t.x; m[13] = t.y; m[14] = t.z; }
            w->px = m[12]; w->py = m[13]; w->pz = m[14];
            w->ct3--;
        } else {
            w->mode0 = 2;
            w->flg &= ~0x10u;
        }
        break;
    }
    m[12] = w->px; m[13] = w->py; m[14] = w->pz;
    mtx_put(w, &m);
}
/* Trefferauswertung von FloorCollision/WallCheck (en06sub.c:301-319, :346-366). */
static void s_hit(RECVX_EN06S_EXP0* p, const RECVX_ENEMY_VEC* n, int i, int wall)
{
    p->n[0] = n->x; p->n[1] = n->y; p->n[2] = n->z;
    unit(p->n);
    if (p->n[1] > 0) { p->flags &= ~1u; if (wall) p->hit = i; }
    if (p->n[1] < 0) p->vel[1] = 0;
    if (p->n[1] == 0) { p->vel[0] = 0; p->vel[2] = 0; }
}
/* bhEne06s_FloorCollision (en06sub.c:291). */
static void s_floor_collision(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06S_EXP0* p = s_exp0(w);
    if (p->flags & 1u) {
        RECVX_ENEMY_VEC from = { w->pxb, w->pyb, w->pzb }, to = { w->px, w->py, w->pz }, n;
        if (line1(w, &from, &to, NULL, &n)) { w->px = to.x; w->py = to.y; w->pz = to.z; s_hit(p, &n, 0, 0); }
        put_pos(w);
    }
}
/* bhEne06s_WallCheck (en06sub.c:329). */
static void s_wall_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN06S_EXP0* p = s_exp0(w);
    NJS_MATRIX m;
    int i;
    if (!(p->flags & 1u)) return;
    if (!mtx_get(w, &m)) return;
    for (i = 0; i < 3; i++) {
        NJS_VECTOR vec;
        RECVX_ENEMY_VEC from, to, n;
        NJS_POINT3 o = collision_offset[w->lkono][i];
        njCalcVector(&m, &o, &vec);
        w->px += vec.x; w->py += vec.y; w->pz += vec.z;
        from.x = p->pts[i][0]; from.y = p->pts[i][1]; from.z = p->pts[i][2];
        to.x = w->px; to.y = w->py; to.z = w->pz;
        if (line1(w, &from, &to, NULL, &n)) { w->px = to.x; w->py = to.y; w->pz = to.z; s_hit(p, &n, i, 1); }
        if (w->sv && w->sv->collision_works) (void)w->sv->collision_works(w); else gap(w, "bhEne03_Collision (en06s)");
        w->px -= vec.x; w->py -= vec.y; w->pz -= vec.z;
    }
    m[12] = w->px; m[13] = w->py; m[14] = w->pz;
    mtx_put(w, &m);
    njCalcPoints(&m, (NJS_POINT3*)collision_offset[w->lkono], (NJS_POINT3*)p->pts, 3);
}
void recvx_en06s(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    switch (w->mode0) {                                                 /* bhEne06s_Mode0[3] */
    case 0:                                                             /* bhEne06s_Init :52 */
        w->flg &= ~0x10u;
        w->ar = 0.5f; w->ah = 0.3f; w->car = 0.5f; w->cah = 0.3f;
        w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
        if (!w->exp0) {
            if (w->sv && w->sv->calloc_work) w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN06S_EXP0));
            if (!w->exp0) { gap(w, "bhEne_CallocWork(68, 8) (en06s)"); return; }
        }
        break;
    case 1:
        if (!w->exp0) { gap(w, "bhEne06s: exp0 fehlt"); return; }
        if (w->mode2 == 0) s_mv00(w);
        else if (w->mode2 == 1) {
            if (w->lkono < 0 || w->lkono >= 4) { gap(w, "CollisionOffset[lkono] ausserhalb (en06sub.c:191)"); return; }
            s_mv01(w);
        } else { gap(w, "bhEne06s_MoveMode2[mode2] ausserhalb der Tabelle"); return; }
        break;
    case 2: break;                                                      /* bhEne06s_Dummy */
    default: gap(w, "bhEne06s_Mode0[mode0] ausserhalb der Tabelle"); return;
    }
    if (w->flg & 0x10u) {                                               /* :37 */
        if (w->lkono < 4) s_wall_check(w);
        else {
            s_floor_collision(w);
            if (w->sv && w->sv->collision_works) (void)w->sv->collision_works(w); else gap(w, "bhEne03_Collision (en06s)");
        }
    }
}
