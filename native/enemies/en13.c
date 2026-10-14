/* EN13 Alexia II und EN13B — eigenstaendige native Zustandsmaschinen auf dem
   einzigen WorldEntity-Work. Semantikreferenzen (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en13.c:50 bhEne13, :63 Init, :246-441 Brain,
   :444-604 Move/MV00-03, :613-687 Damage/DG00, :690 Die, :729 InitDamage,
   :857-1030 Finish/ScaleModel/Store/Restore, :1033-1156 Kinderdienste,
   Kamera, SelectTentacle, :1161 PlayerControl; en13sub.c:33-462 bhEne13B.
   Reine Daten: en13.c:1163-1174 und en02.c:23-226 (Spielertranslationen).
   Ungebundene Dienste melden den Originalnamen und erfinden keinen Wert. */
#include "en13.h"
#include "en03_lib.h"
#include "en14.h"
#include "en18.h"
#include "ninja_matrix.h"
#include <stddef.h>
#include <math.h>

static const RECVX_EN13_SERVICES* en13_sv;
void recvx_en13_bind(const RECVX_EN13_SERVICES* services) { en13_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
#define SV(w, f, name) do { if (!en13_sv || !en13_sv->f) { gap(w, name); return; } } while (0)
#define BASE(w, f, name) do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return; } } while (0)

static RECVX_EN13_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN13_STATE*)w->exp0; }
static RECVX_EN13B_STATE* stb(RECVX_ENEMY_WORK* w) { return (RECVX_EN13B_STATE*)w->exp0; }

/* macros.h:93/105 njRandom() = -rand()/-(RAND_MAX+1), Wert in [0,1). */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "njRandom (en13)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en13)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_set) { gap(w, "plp schreiben (en13)"); return; }
    w->sv->player_set(p);
}
static void call_se(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se)
{
    recvx_t3_call_se(w, pos, se);
}
static uint32_t frames(RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    uint32_t n;
    if (!w->sv || !w->sv->motion_frames) { gap(w, "epw->mnwP[mtn_no].frm_num (en13)"); return 0; }
    n = w->sv->motion_frames(w, mtn);
    if (!n) gap(w, "epw->mnwP[mtn_no] fehlt (en13)");
    return n;
}
static uint32_t obj_num(RECVX_ENEMY_WORK* w)
{
    if (!w->sv || !w->sv->obj_num) { gap(w, "mlwP->obj_num (en13)"); return 0; }
    return w->sv->obj_num(w);
}
static int node_pos(RECVX_ENEMY_WORK* w, uint32_t node, RECVX_ENEMY_VEC* out)
{
    if (!en13_sv || !en13_sv->node_position || !en13_sv->node_position(w, node, out)) {
        gap(w, "mlwP->owP[n].mtx[12..14] (en13)"); return 0;
    }
    return 1;
}
static void set_modes(RECVX_ENEMY_WORK* w, uint8_t m0, uint8_t m1, uint8_t m2, uint8_t m3)
{
    w->mode0 = m0; w->mode1 = m1; w->mode2 = m2; w->mode3 = m3;
}
static float dist_xz(const RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    return njSqrt(((w->px - p->pos.x) * (w->px - p->pos.x)) + ((w->pz - p->pos.z) * (w->pz - p->pos.z)));
}
/* exp0[8 + 4j]: die Koerperkinder Typ 1..3. */
static RECVX_ENEMY_WORK* body(RECVX_EN13_STATE* s, int j) { return s->child[2 + j]; }
/* *(BH_PWORK**)&exp0[8] + EXP0_I(128) (en13.c:510/588) ist Zeigerarithmetik auf
   ene[]: der Work idx_ct(Typ1) + hatch_no, nicht ein exp0-Eintrag. */
static RECVX_ENEMY_WORK* body_by_slot(RECVX_ENEMY_WORK* w, RECVX_EN13_STATE* s)
{
    RECVX_ENEMY_WORK* ep;
    if (!w->sv || !w->sv->enemy_at) { gap(w, "ene[] (en13 exp0[8] + EXP0_I(128))"); return NULL; }
    ep = w->sv->enemy_at((uint32_t)(s->child[2]->idx_ct + s->hatch_no));
    if (!ep) gap(w, "ene[idx_ct + EXP0_I(128)] ausserhalb (en13)");
    return ep;
}

/* --- Kinderdienste (en13.c:1033-1102) --------------------------------------- */
void recvx_en13_put_attacker(RECVX_ENEMY_WORK* w, int32_t no)
{
    static const RECVX_ENEMY_VEC pos[3] = { {0.0f, 0.0f, -25.0f}, {-20.0f, 0.0f, -13.0f}, {20.0f, 0.0f, -13.0f} };
    static const int32_t ang[3] = { 0, 8192, (int32_t)0xFFFFE000 };
    RECVX_EN13_STATE* s;
    int32_t i;
    if (!w || !w->exp0) { gap(w, "bhEne13_PutAttacker: exp0"); return; }
    if (no < 0 || no > 2) { gap(w, "bhEne13_PutAttacker: pos[no] ausserhalb"); return; }
    s = st(w);
    for (i = 0; i < s->hatch_n; i++) {
        RECVX_ENEMY_WORK* ep = s->hatch[i];
        if (ep->mode0 == 1 && ep->mode2 == 0) {
            NJS_MATRIX m; NJS_VECTOR in = { pos[no].x, pos[no].y, pos[no].z }, wp;
            ep->mode1 = 0; ep->mode2 = 6; ep->mode3 = 0;
            njUnitMatrix(&m); njRotateY(&m, w->ay); njCalcVector(&m, &in, &wp);
            ep->px = ep->pxb = w->px + wp.x;
            ep->py = ep->pyb = w->py + wp.y;
            ep->pz = ep->pzb = w->pz + wp.z;
            ep->ay = w->ay + ang[no];
            {
                const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
                call_se(w, &at, 74507);
            }
            break;
        }
    }
}
void recvx_en13_tentacle(RECVX_ENEMY_WORK* w, int32_t no)
{
    RECVX_ENEMY_WORK* t;
    if (!w || !w->exp0 || !st(w)->tentacle) { gap(w, "bhEne13_Tentacle: exp0[976]"); return; }
    t = st(w)->tentacle;
    t->mode0 = 1; t->mode1 = 0; t->mode2 = 1; t->mode3 = 0;
    t->type = (uint16_t)no;
}
int recvx_en13_hatch_no(const RECVX_ENEMY_WORK* w, int32_t* out)
{
    if (!w || !w->exp0) return 0;
    *out = ((const RECVX_EN13_STATE*)w->exp0)->hatch_no;
    return 1;
}
int recvx_en13_tenta_no(const RECVX_ENEMY_WORK* w, int32_t* out)
{
    if (!w || !w->exp0) return 0;
    *out = ((const RECVX_EN13_STATE*)w->exp0)->tenta_no;
    return 1;
}

/* --- Init (en13.c:63-243) --------------------------------------------------- */
static RECVX_ENEMY_WORK* child(RECVX_ENEMY_WORK* w, uint16_t id, uint16_t type, uint32_t base,
                               RECVX_ENEMY_WORK* share)
{
    RECVX_ENEMY_TEMPLATE t = {0};
    RECVX_ENEMY_WORK* c;
    t.flg = 0x8081u; t.id = id;                         /* ene18 / ene13B */
    c = en13_sv->create_child(w, &t, base, share);
    if (!c) { gap(w, "bhEne13_Init: eneset.c:170 ene[] voll oder mdl fehlt"); return NULL; }
    c->type = type;
    c->lkwkp = w; c->lkono = 0;
    c->lox = 0; c->loy = 0; c->loz = 0;
    return c;
}
static void init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s;
    uint32_t i, n;
    int gm;
    w->flg |= 0x8018u; w->flg &= ~0x6u;
    w->flg2 |= 0x1u;
    w->mdflg |= 0x20u;
    w->ar = 5.0f; w->ah = 1.0f; w->car = 3.0f;
    SV(w, game_mode, "bhEne13_Init: sys->gm_mode");
    if (!en13_sv->game_mode(&gm)) { gap(w, "bhEne13_Init: sys->gm_mode"); return; }
    w->hp = (gm != 2) ? 700 : 400;
    set_modes(w, 1, 1, 0, 0);
    w->hokan_rate = 65536; w->hokan_count = 0;
    w->mtn_no = 0; w->mtn_md = 0; w->mtn_add = 65536;
    w->frm_no = 0;
    if (!w->exp0) {
        SV(w, create_child, "bhEne13_Init: eneset.c:170 Kindwork");
        SV(w, share_motion, "bhEne13_Init: mnwP/mnwPb/skp[13]");
        BASE(w, calloc_work, "bhEne_CallocWork (en13)");
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN13_STATE));
        if (!w->exp0) { gap(w, "bhEne13_Init: EXP-Budget erschoepft"); return; }
        s = st(w);
        /* Reihenfolge der bhSetEnemy-Aufrufe: exp0[8], [12], [16], [4], [20..40], [976]. */
        if (!(s->child[2] = child(w, 18, 1, 1, NULL))) return;
        if (!(s->child[3] = child(w, 18, 2, 4, NULL))) return;
        if (!(s->child[4] = child(w, 18, 3, 7, NULL))) return;
        if (!(s->child[1] = child(w, 18, 0, 10, NULL))) return;
        /* exp0[92..104] = objP der Koerperkinder: nativ deren Wurzel 0 (select_first_root). */
        for (i = 0; i < 6; i++)
            if (!(s->child[5 + i] = child(w, 18, (uint16_t)(i + 4), 10, s->child[1]))) return;
        if (!(s->tentacle = child(w, 31, 0, 13, NULL))) return;
        s->tentacle->mdl_no = 0;
        en13_sv->share_motion(s->tentacle, w);
        if (!recvx_enemy_ext() || !recvx_enemy_ext()->set_call_func) { gap(w, "bhEne_SetCallFunc(bhEne13B, 31)"); return; }
        recvx_enemy_ext()->set_call_func(31u, recvx_en13b);
        /* EXP0_I(932..948) und bhEne13_StoreObject: der Speicher ist der
           unveraenderte Modellzustand beim ersten Init (restore_objects). */
        s->store_index[0] = 0;
    }
    s = st(w);
    SV(w, select_first_root, "bhEne13_Init: mlwP->objP = exp0[92..104]");
    SV(w, restore_objects, "bhEne13_RestoreObject");
    for (i = 1; i <= 4; i++)
        if (!en13_sv->select_first_root(s->child[i])) { gap(w, "bhEne13_Init: exp0[92..104] Wurzel"); return; }
    if (!en13_sv->restore_objects(w)) { gap(w, "bhEne13_RestoreObject(epw)"); return; }
    for (i = 1; i <= 4; i++)
        if (!en13_sv->restore_objects(s->child[i])) { gap(w, "bhEne13_RestoreObject(Kind)"); return; }
    s->hatch_n = 0;
    BASE(w, enemy_count, "sys->ewk_n (en13)");
    BASE(w, enemy_at, "ene[] (en13)");
    n = w->sv->enemy_count();
    for (i = 0; i < n; i++) {
        RECVX_ENEMY_WORK* ep = w->sv->enemy_at(i);
        if (!ep) continue;
        if ((ep->flg & 0x1u) && ep->id == 30) {
            /* exp0[44 + 4k]: ab k = 12 ueberschriebe das Original exp0[92..]. */
            if (s->hatch_n >= 12) { gap(w, "bhEne13_Init: mehr als 12 id-30-Works ueberschreiben exp0[92]"); return; }
            s->hatch[s->hatch_n] = ep;
            s->hatch_n++;
            ep->lkwkp = w;
        }
        if ((ep->flg & 0x1u) && ep->id == 14) {
            s->en14 = ep;
            ep->lkwkp = w;
        }
    }
    s->timer[0] = 15; s->timer[1] = 30; s->timer[2] = 45;
    s->unused116[0] = s->unused116[1] = s->unused116[2] = 100;
}

/* --- Brain (en13.c:246-441) ------------------------------------------------- */
static int any_attacking(RECVX_EN13_STATE* s)
{
    int j;
    for (j = 0; j < 3; j++) if (body(s, j)->mode2 == 4 || body(s, j)->mode2 == 5) return 1;
    return 0;
}
static int select_tentacle(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p, int* out)
{
    int ang;
    float r;
    ang = (int16_t)(recvx_t3_arc_tan2(w->px - p->pos.x, w->pz - p->pos.z) - w->ay);
    if (!rnd(w, &r)) return 0;
    *out = 0;
    if (r < 0.9f) {
        if (ang > 5461) *out = 1;                /* NJM_DEG_ANG(30.0f) */
        else if (ang < -5461) *out = 2;
    }
    return 1;
}
static void br00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float dist, r;
    int i, j;
    if (w->hp < 0) return;
    if (!player(w, &p)) return;
    dist = dist_xz(w, &p);
    if (s->timer[0] != 0) s->timer[0]--;
    if (s->timer[1] != 0) s->timer[1]--;
    if (s->timer[2] != 0) s->timer[2]--;
    if (s->timer[2] == 0 && dist > 40.0f) {
        w->mode1 = 0; w->mode2 = 2; w->mode3 = 0;
        s->timer[2] = dist > 60.0f ? 10 : 45;
        return;
    }
    if (dist < 60.0f && s->timer[1] == 0 && !any_attacking(s)) {
        if (!select_tentacle(w, &p, &j)) return;
        if (body(s, j)->mode2 == 0) {
            s->hatch_no = j;
            if (!rnd(w, &r)) return;
            s->tenta_no = r < 0.4f ? s->hatch_no : s->hatch_no + 3;
            w->mode1 = 0; w->mode2 = 3; w->mode3 = 0;
            s->timer[1] = 30;
            if (s->timer[0] != 0) s->timer[0]--;
            else w->type = 0;
            return;
        }
    }
    if (s->timer[0] == 0) {
        if (!rnd(w, &r)) return;
        j = (int)(3.0f * r);
        s->hatch_no = -1;
        /* Float32 kann njRandom() == 1.0 liefern; j == 3 liest wie das Original
           exp0[20], das erste Kollisionskind. */
        for (i = 0; i < 3; i++) {
            if (body(s, j)->mode2 == 0) { s->hatch_no = j; break; }
            if (++j > 2) j = 0;
        }
        if (s->hatch_no != -1) {
            for (i = 0; i < s->hatch_n; i++) {
                RECVX_ENEMY_WORK* ep = s->hatch[i];
                if (ep->mode0 == 1 && ep->mode2 == 0) {
                    w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
                    s->timer[0] = 15;
                    break;
                }
            }
        }
    }
}
static void br01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float dist, r;
    int j;
    if (w->hp < 0) return;
    if (!player(w, &p)) return;
    dist = dist_xz(w, &p);
    if (s->timer[1] != 0) s->timer[1]--;
    if (s->timer[2] != 0) s->timer[2]--;
    if (s->timer[2] == 0 && dist > 60.0f) {
        w->mode1 = 0; w->mode2 = 2; w->mode3 = 0;
        s->timer[2] = 45;
        return;
    }
    if (s->timer[1] == 0 && !any_attacking(s)) {
        if (!select_tentacle(w, &p, &j)) return;
        if (body(s, j)->mode2 == 0) {
            s->hatch_no = j;
            if (!rnd(w, &r)) return;
            s->tenta_no = r > 0.7f ? s->hatch_no : s->hatch_no + 3;
            w->mode1 = 0; w->mode2 = 3; w->mode3 = 0;
            s->timer[1] = 30;
            if (s->timer[0] != 0) s->timer[0]--;
            else { w->type = 0; return; }
        }
    }
}

/* --- Move (en13.c:444-604) ------------------------------------------------- */
static void en14_mode(RECVX_EN13_STATE* s, uint8_t mode2)
{
    if (s->en14->type == 0) { s->en14->mode2 = mode2; s->en14->mode3 = 0; }
}
static void mv00(RECVX_ENEMY_WORK* w)
{
    if (w->mode3 == 0) {
        w->mtn_no = 0; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 45875;
        w->mode3++;
    }
}
static void mv01_03(RECVX_ENEMY_WORK* w, int attack)
{
    RECVX_EN13_STATE* s = st(w);
    RECVX_ENEMY_WORK* ep;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 1; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 45875;
        if (!s->en14) { gap(w, "bhEne13_MV01: exp0[112] (EN14) NULL"); return; }
        en14_mode(s, 1);
        w->ct0 = 10;
        w->mode3++;
        break;
    case 1:
        if (w->frm_no == 0) { w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        if (w->ct0-- == 0) {
            if (!(ep = body_by_slot(w, s))) return;
            ep->mode3 = 0;
            if (!attack) ep->mode2 = 2;
            else ep->mode2 = s->tenta_no >= 3 ? 4 : 5;
        }
        break;
    }
}
static void mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    uint32_t n;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 2; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 45875;
        if (!s->en14) { gap(w, "bhEne13_MV02: exp0[112] (EN14) NULL"); return; }
        en14_mode(s, 2);
        if (!(n = frames(w, w->mtn_no))) return;
        w->ct0 = (int32_t)n - 1;
        w->mode3++;
        break;
    case 1:
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        break;
    }
}
static int child_hit(RECVX_EN13_STATE* s)
{
    int i;
    for (i = 0; i < 6; i++) if (s->child[5 + i]->flg & 0x4u) return 1;
    return s->en14 && s->en14->type == 0 && (s->en14->flg & 0x4u);
}
static void init_damage(RECVX_ENEMY_WORK* w);
static void move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1) {
        if (w->type == 0) br00(w);
        else if (w->type == 1) br01(w);
        else { gap(w, "bhEne13_BrainType[type]"); return; }
    }
    switch (w->mode2) {
    case 0: mv00(w); break;
    case 1: mv01_03(w, 0); break;
    case 2: mv02(w); break;
    case 3: mv01_03(w, 1); break;
    default: gap(w, "bhEne13_MoveMode2[mode2]"); return;
    }
    if (child_hit(st(w))) init_damage(w);
}

/* --- Schaden (en13.c:613-854) ---------------------------------------------- */
static int en18_hit(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* c, int* dam)
{
    *dam = recvx_en18_hitmark(c);
    if (*dam < 0) { gap(w, "bhEne18_HitMark (en13)"); return 0; }
    return 1;
}
static int en14_hit(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* c, int* dam)
{
    *dam = recvx_en14_hitmark(c);
    if (*dam < 0) { gap(w, "bhEne14_HitMark (en13)"); return 0; }
    return 1;
}
static void dg00(RECVX_ENEMY_WORK* w)
{
    uint32_t n;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 3; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 45875;
        if (!(n = frames(w, w->mtn_no))) return;
        w->ct0 = (int32_t)n - 1;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            set_modes(w, 1, 1, 0, 0);
            w->flg &= ~0x4u;
        }
        if (w->frm_no == 3932160 && w->hp < 0) w->flg |= 0x2u;
    }
}
static void damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    int dam, i, max_dam;
    if (child_hit(s)) {
        max_dam = 0;
        for (i = 0; i < 6; i++) {
            RECVX_ENEMY_WORK* c = s->child[5 + i];
            if (c->flg & 0x4u) {
                c->flg &= ~0x4u;
                if (!en18_hit(w, c, &dam)) return;
                if (max_dam < dam) max_dam = dam;
            }
        }
        if (!s->en14) { gap(w, "bhEne13_Damage: exp0[112] (EN14) NULL"); return; }
        if (s->en14->type == 0 && (s->en14->flg & 0x4u)) {
            s->en14->flg &= ~0x4u;
            if (!en14_hit(w, s->en14, &dam)) return;
            if (max_dam < dam) max_dam = dam;
        }
        w->hp -= max_dam;
    }
    if (w->mode2 != 0) { gap(w, "bhEne13_DamageMode2[mode2]"); return; }
    dg00(w);
}
static void init_damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    int dam, max_dam = 0, i, flg;
    int32_t wep_no = 0;
    uint32_t dflg = 0;
    for (i = 0; i < 6; i++) {
        RECVX_ENEMY_WORK* c = s->child[5 + i];
        if (c->flg & 0x4u) {
            c->flg &= ~0x4u;
            if (!en18_hit(w, c, &dam)) return;
            if (max_dam < dam) { max_dam = dam; wep_no = c->wpnr_no; dflg = c->flg2 & 0x4u; }
        }
    }
    if (!s->en14) { gap(w, "bhEne13_InitDamage: exp0[112] (EN14) NULL"); return; }
    if (s->en14->type == 0 && (s->en14->flg & 0x4u)) {
        s->en14->flg &= ~0x4u;
        if (!en14_hit(w, s->en14, &dam)) return;
        if (max_dam < dam) {
            max_dam = dam;
            wep_no = s->en14->wpnr_no;
            /* en13.c:769 liest exp0[20 + 4*i] mit i == 6 nach der Schleife:
               exp0[44], den ersten id-30-Work. */
            if (!s->hatch_n) { gap(w, "bhEne13_InitDamage: exp0[44] NULL (en13.c:769)"); return; }
            dflg = s->hatch[0]->flg2 & 0x4u;
        }
    }
    if (max_dam != 0) {
        const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
        flg = 0;
        if (!((wep_no == 15 || wep_no == 16 || wep_no == 17) && dflg == 0)) call_se(w, &at, 8961);
        if (w->hp > 600) { w->hp -= max_dam; if (w->hp < 600) flg = 1; }
        else if (w->hp > 400) { w->hp -= max_dam; if (w->hp < 400) flg = 1; }
        else if (w->hp > 200) { w->hp -= max_dam; if (w->hp < 200) flg = 1; }
        else { w->hp -= max_dam; if (w->hp < 0) flg = 1; }
        if (flg != 0) {
            set_modes(w, 3, 0, 0, 0);
            if (s->en14->type == 0) {
                if (w->hp < 0) set_modes(s->en14, 3, 0, 1, 0);
                else set_modes(s->en14, 3, 0, 0, 0);
            }
        }
    }
}

/* --- Tod (en13.c:690-894) --------------------------------------------------- */
static void finish(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    uint32_t i, ono, n;
    const float spd = w->spd;
    SV(w, scale_object, "bhEne13_Finish: objP/Vertexdaten (beschreibbarer Modellbesitzer)");
    for (i = 1; i <= 4; i++) {
        const RECVX_ENEMY_VEC pos = { 1.0f, spd, 1.0f }, v = { 1.0f, spd, 1.0f };
        n = obj_num(s->child[i]);
        for (ono = 0; ono < n; ono++)
            if (!en13_sv->scale_object(s->child[i], ono, &pos, &v)) { gap(w, "bhEne13_ScaleModel (Kind)"); return; }
    }
    n = obj_num(w);
    for (ono = 0; ono < n; ono++) {
        const int y_only = ono == 1 || ono == 4 || ono == 7 || ono == 11 || ono == 15 || ono == 19;
        const RECVX_ENEMY_VEC pos = { y_only ? 1.0f : spd, spd, y_only ? 1.0f : spd }, v = { spd, spd, spd };
        if (!en13_sv->scale_object(w, ono, &pos, &v)) { gap(w, "bhEne13_ScaleModel"); return; }
    }
}
static void die(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    int i;
    switch (w->mode3) {
    case 0:
        set_modes(s->tentacle, 4, 0, 0, 0);
        for (i = 0; i < 4; i++) set_modes(s->child[1 + i], 4, 0, 0, 0);
        for (i = 0; i < 6; i++) set_modes(s->child[5 + i], 4, 0, 0, 0);
        w->mode3++;
        break;
    case 1:
        w->spd = 0.97f + (w->mode2 / 50.0f);
        finish(w);
        break;
    }
}

/* --- Kamera (en13.c:1105-1134) ---------------------------------------------- */
static void camera_control(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13_STATE* s = st(w);
    float r[3];
    int i;
    if (w->mode0 == 5) return;
    SV(w, camera_offset, "cam.ofx/ofy/ofz (en13)");
    if ((w->flg & 0x80000u) || s->camera > 0.01f) {
        for (i = 0; i < 3; i++) if (!rnd(w, &r[i])) return;
        en13_sv->camera_offset((s->camera * r[0]) - (s->camera / 2.0f),
                               (s->camera * r[1]) - (s->camera / 2.0f),
                               (s->camera * r[2]) - (s->camera / 2.0f));
        if (!(w->flg & 0x80000u)) s->camera *= 0.9f;
    } else {
        en13_sv->camera_offset(0, 0, 0);
    }
}

/* --- Spielersteuerung (en13.c:1161-1387) ---------------------------------- */
/* Reine Daten: en02.c:23/45/67/119/141/163 (cler_042/043/045, cher_060/061/063). */
#include "en02_trans_tables.h"
static const int32_t pc_mtn[3][8] = {
    { 10, 11, 12, 13, 15, 14, 0, 0 },
    { 16, 17, 18, 19, 21, 20, 0, 0 },
    { 16, 17, 18, 19, 21, 20, 0, 0 }
};
static const RECVX_ENEMY_VEC* pc_trans(uint32_t ply_id, int k)
{
    static const RECVX_ENEMY_VEC* const t[3][3] = {
        { recvx_cler_042, recvx_cler_043, recvx_cler_045 },
        { recvx_cher_060, recvx_cher_061, recvx_cher_063 },
        { recvx_cher_060, recvx_cher_061, recvx_cher_063 }
    };
    return t[ply_id][k];
}
static void library(RECVX_ENEMY_WORK* w, uint32_t source)
{
    if (!en13_sv || !en13_sv->player_library) { gap(w, "bhEne13_PlayerControl: plp->mnwP"); return; }
    en13_sv->player_library(w, source);
}
static void null_trans_dir(int ay, const RECVX_ENEMY_VEC* table)
{
    const RECVX_T3_LINK lk = { NULL, 1 };
    recvx_t3_add_null_trans_dir(lk, ay, table);
}
static void player_control(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    uint32_t n;
    int k;
    if (!player(w, &p)) return;
    if (p.mode0 != 4 && p.mode0 != 6) return;
    if (p.ply_id > 2) { gap(w, "bhEne13_PlayerControl: mtn[sys->ply_id] ausserhalb"); return; }
    if (p.mode2 == 0 || p.mode2 == 1) {
        const int death = p.mode0 == 6;
        switch (p.mode3) {
        case 0:
            p.flg &= ~0x40000u; p.flg2 |= 0x1u;
            p.mtn_enemy = 1; p.mtn_enemy_owner = w->id; p.mtn_enemy_work = w;
            /* mode0 6: `plp->mode2 == 2` ist innerhalb case 0/1 nie wahr. */
            p.mtn_no = (uint32_t)pc_mtn[p.ply_id][(!death && p.mode2 == 0) ? 0 : 1];
            p.frm_no = 0;
            p.hokan_count = 3; p.hokan_rate = 32768;
            p.mtn_add = 65536;
            p.mode3++;
            p.ct0 = 8;
            put_player(w, &p); library(w, 1);
            recvx_t3_call_player_voice(death ? 1 : 2);
            BASE(w, vibration, "StartVibrationEx (en13)");
            w->sv->vibration(1, 11);
            if (!player(w, &p)) return;
            break;
        case 1:
            k = (!death && p.mode2 == 0) ? 0 : 1;
            null_trans_dir(p.ayp, pc_trans(p.ply_id, k));
            if (!player(w, &p)) return;
            if (p.ct0 != 0) { p.ay += (int16_t)(p.ayp - p.ay) / p.ct0; p.ct0--; }
            if (p.frm_no == 0) {
                if (!death) {
                    if (p.mode2 == 0) { p.mtn_no = (uint32_t)pc_mtn[p.ply_id][2]; p.flg |= 0xC0000u; }
                    else p.mtn_no = (uint32_t)pc_mtn[p.ply_id][3];
                } else {
                    p.mtn_no = (uint32_t)pc_mtn[p.ply_id][5];
                    BASE(w, player_motion_frames, "bhEne13_PlayerControl: plp->mnwP[mtn_no].frm_num");
                    if (!(n = w->sv->player_motion_frames(p.mtn_no, 1))) { gap(w, "bhEne13_PlayerControl: mnwP[mtn_no] fehlt"); return; }
                    p.ct0 = (int32_t)n - 2;
                }
                p.mode3++;
            }
            break;
        case 2:
            if (!death) {
                if (p.mode2 == 0) {
                    if ((p.frm_no / 65536) == 19) p.flg &= ~0x80000u;
                    if (p.ply_id == 0 && (p.frm_no / 65536) == 41) p.flg |= 0x80000u;
                } else if (p.mtn_no == (uint32_t)pc_mtn[p.ply_id][3]) {
                    put_player(w, &p);
                    {
                        const RECVX_T3_LINK lk = { NULL, 1 };
                        recvx_t3_add_null_trans(lk, pc_trans(p.ply_id, 2));
                    }
                    if (!player(w, &p)) return;
                }
                if (p.frm_no == 0) {
                    p.mtn_enemy = 0; p.mtn_enemy_work = NULL;
                    p.flg &= ~0x10004u; p.flg2 &= ~0x1u; p.flg |= 0x8u;
                    p.at_flg = 0;
                    p.stflg &= ~0x10000u;
                    p.mode0 = 1; p.mode1 = 0; p.mode2 = 0; p.mode3 = 0;
                    put_player(w, &p); library(w, 0);
                    if (!player(w, &p)) return;
                }
            } else if (p.ct0-- == 0) {
                p.mtn_add = 0; p.ct0 = 45; p.mode3++;
            }
            break;
        case 3:
            if (death && p.ct0-- == 0) p.flg |= 0x2u;
            break;
        }
    }
    p.flg |= 0x200000u;
    put_player(w, &p);
}

void recvx_en13(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    if (w->mode0 != 0 && !w->exp0) { gap(w, "bhEne13: exp0 fehlt"); return; }
    switch (w->mode0) {
    case 0: init(w); break;
    case 1: move(w); break;
    case 2: break;                          /* bhEne13_Nage ist original leer */
    case 3: damage(w); break;
    case 4: die(w); break;
    case 5:
        if (w->sv && w->sv->event) w->sv->event(w);
        else gap(w, "bhEne_Event (en13)");
        break;
    default: gap(w, "bhEne13_Mode0[mode0]"); return;
    }
    if (!w->exp0) return;
    BASE(w, set_motion, "bhSetMotion (en13)");
    w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    BASE(w, calc_model, "bhCalcModel (en13)");
    w->sv->calc_model(w);
    camera_control(w);
    player_control(w);
}

/* --- EN13B (en13sub.c) ----------------------------------------------------- */
static int wall_get(RECVX_ENEMY_WORK* w, uint32_t i, RECVX_ENEMY_ATR* a)
{
    if (!w->sv || !w->sv->mobile_wall_get || !w->sv->mobile_wall_get(stb(w)->wall[i], a)) {
        gap(w, "bhEne13B: EXP0_ATR(i) lesen"); return 0;
    }
    return 1;
}
static void wall_put(RECVX_ENEMY_WORK* w, uint32_t i, const RECVX_ENEMY_ATR* a)
{
    if (!w->sv || !w->sv->mobile_wall_set) { gap(w, "bhEne13B: EXP0_ATR(i) schreiben"); return; }
    w->sv->mobile_wall_set(stb(w)->wall[i], a);
}
static void walls_off(RECVX_ENEMY_WORK* w, uint32_t n)
{
    RECVX_ENEMY_ATR a;
    uint32_t i;
    for (i = 0; i < n; i++) { if (!wall_get(w, i, &a)) return; a.flg = 0; wall_put(w, i, &a); }
}
static void b_init(RECVX_ENEMY_WORK* w)
{
    uint32_t i, n;
    RECVX_ENEMY_VEC root;
    w->flg |= 0x8018u; w->flg &= ~0x6u;
    w->flg2 |= 0x1u; w->flg2 |= 0x10u;
    w->mdflg |= 0x20u;
    w->ar = 5.0f; w->ah = 1.0f; w->car = 3.0f;
    set_modes(w, 1, 0, 0, 0);
    w->hokan_rate = 65536; w->hokan_count = 0;
    w->mtn_no = 4; w->mtn_md = 0; w->mtn_add = 0;
    w->frm_no = 0;
    BASE(w, set_motion, "bhSetMotion (en13B)");
    w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    BASE(w, calc_model, "bhCalcModel (en13B)");
    w->sv->calc_model(w);
    if (!w->exp0) {
        BASE(w, calloc_work, "bhEne_CallocWork (en13B)");
        BASE(w, mobile_wall_alloc, "sys->mwal_n++ (en13B)");
        n = obj_num(w);
        if (n > RECVX_EN13B_NODES) { gap(w, "bhEne13B_Init: obj_num > 32 sprengt exp0[520]"); return; }
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN13B_STATE));
        if (!w->exp0) { gap(w, "bhEne13B_Init: EXP-Budget erschoepft"); return; }
        if (!node_pos(w, 0, &root)) return;
        for (i = 0; i < n; i++) {
            RECVX_ENEMY_ATR a;
            stb(w)->wall[i] = w->sv->mobile_wall_alloc();
            if (!wall_get(w, i, &a)) return;
            a.flg = 0; a.type = 3;
            a.w = 2.0f; a.h = 1.0f; a.d = 0;
            a.attr = 0;
            a.px = root.x; a.py = root.y; a.pz = root.z;   /* owP->mtx: immer owP[0] */
            wall_put(w, i, &a);
        }
    }
}
static int frame_is(const RECVX_ENEMY_WORK* w, uint32_t mtn, int32_t frm) { return w->mtn_no == mtn && w->frm_no == frm; }
static void b_player_hit(RECVX_ENEMY_WORK* w, uint32_t i)
{
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC p1, pi, dv, dp;
    int djnt;
    float d, s;
    if (!node_pos(w, 0, &p1) || !node_pos(w, i, &pi)) return;
    if (!recvx_t3_player_hit_get(&djnt, &dv, &dp)) return;
    dv.x = stb(w)->prev[i].x - pi.x;
    dv.y = stb(w)->prev[i].y - pi.y;
    dv.z = stb(w)->prev[i].z - pi.z;
    if (!player(w, &p)) return;
    d = njSqrt((p1.x - p.pos.x) * (p1.x - p.pos.x) + (p1.y - p.pos.y) * (p1.y - p.pos.y) + (p1.z - p.pos.z) * (p1.z - p.pos.z));
    s = njSqrt(dv.x * dv.x + dv.y * dv.y + dv.z * dv.z);
    if (d < 15.0f || s < 5.0f) { dv.x = p1.x - p.pos.x; dv.y = p1.y - p.pos.y; dv.z = p1.z - p.pos.z; }
    if (!recvx_t3_player_hit_set(djnt, &dv, &dp)) return;
    p.hp -= (w->ct0 == 1) ? 25 : 60;
    p.flg |= 0x10004u;
    p.stflg |= p.hp < 0 ? 0x50000u : 0x10000u;
    p.mode0 = p.hp < 0 ? 6 : 4;
    p.mode1 = 0;
    if (recvx_t3_dir_check(p.ay, &dv) != 0) { p.ayp = recvx_t3_arc_tan2(-dv.x, -dv.z); p.mode2 = 1; }
    else { p.ayp = recvx_t3_arc_tan2(dv.x, dv.z); p.mode2 = 0; }
    p.mode3 = 0;
    put_player(w, &p);
    player_control(w->lkwkp);
    w->ct0 = 0;
}
static void b_mv01(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn_no[6] = { 6, 4, 8, 7, 5, 9 };
    RECVX_ENEMY_VEC at;
    switch (w->mode3) {
    case 0:
        if (w->type > 5) { gap(w, "bhEne13B_MV01: mtn_no[type] ausserhalb"); return; }
        w->mtn_no = mtn_no[w->type]; w->frm_no = 0;
        w->mtn_add = 65536;
        w->hokan_count = 0;
        w->ct0 = 1;
        w->flg &= ~0x80000u;
        at.x = w->px; at.y = w->py; at.z = w->pz;
        call_se(w, &at, 74507);
        w->mode3++;
        break;
    case 1: {
        uint32_t n, i;
        if (frame_is(w, 5, 3276800) || frame_is(w, 7, 3801088) || frame_is(w, 9, 3276800)) {
            if (!w->lkwkp || !w->lkwkp->exp0) { gap(w, "bhEne13_SetCamera: lkwkp"); return; }
            ((RECVX_EN13_STATE*)w->lkwkp->exp0)->camera = 3.2f;
            if (!node_pos(w, 18, &at)) return;
            call_se(w, &at, 74512);
        }
        if (frame_is(w, 4, 1507328) || frame_is(w, 6, 1507328) || frame_is(w, 8, 1507328)) {
            if (!node_pos(w, 18, &at)) return;
            call_se(w, &at, 74511);
        }
        switch (w->mtn_no) {
        case 4: case 8:
            if (w->frm_no >= 3538944) { w->flg |= 0x80000u; w->ct0 = 0; }
            break;
        case 6:
            if (w->frm_no >= 2883584) { w->flg |= 0x80000u; w->ct0 = 0; }
            break;
        case 5: case 9:
            if (w->frm_no == 851968) { w->flg |= 0x80000u; w->ct0 = 0; }
            if (w->frm_no == 3080192) { w->flg &= ~0x80000u; w->ct0 = 2; }
            if (w->frm_no == 3473408) { w->flg |= 0x80000u; w->ct0 = 0; }
            break;
        case 7:
            if (w->frm_no == 1245184) { w->flg |= 0x80000u; w->ct0 = 0; }
            if (w->frm_no == 3604480) { w->flg &= ~0x80000u; w->ct0 = 2; }
            if (w->frm_no == 3932160) { w->flg |= 0x80000u; w->ct0 = 0; }
            break;
        }
        if (w->ct0 != 0) {
            n = obj_num(w);
            for (i = n - 1; i > 0 && n > 0; i--) {
                RECVX_ENEMY_PLAYER p;
                RECVX_EN13_STATE* ps;
                int32_t j;
                if (!node_pos(w, i, &at)) return;
                if (!player(w, &p)) return;
                if (w->ct0 != 0 && p.mode0 != 3 && p.mode0 != 4 && p.mode0 != 6 && recvx_t3_attack_hit_check(&at, 4.0f))
                    b_player_hit(w, i);
                if (!w->lkwkp || !w->lkwkp->exp0) { gap(w, "bhEne13_GetChild: lkwkp"); return; }
                ps = (RECVX_EN13_STATE*)w->lkwkp->exp0;
                SV(w, attack_hit_work, "bhEne_AttackHitCheck(*ep) (en13B)");
                for (j = 0; j < ps->hatch_n; j++) {
                    RECVX_ENEMY_WORK* ep = ps->hatch[j];
                    if ((ep->flg & 0x20u) && en13_sv->attack_hit_work(ep, &at, 4.0f)) {
                        ep->flg |= 0x4u;
                        ep->dam[0] = 100;
                    }
                }
            }
        }
        if (w->frm_no == 0) { w->mode1 = 0; w->mode2 = 0; w->mtn_add = 0; w->mode3 = 0; }
        break;
    }
    }
}
static void b_die(RECVX_ENEMY_WORK* w)
{
    w->stflg |= 0x1000000u;
    w->flg |= 0x2u;
    walls_off(w, obj_num(w));
    w->flg &= ~0x80000u;
}
static void b_set_hittab(RECVX_ENEMY_WORK* w)
{
    RECVX_EN13B_STATE* s = stb(w);
    RECVX_ENEMY_ATR a;
    RECVX_ENEMY_VEC p;
    int32_t i;
    if (w->flg & 0x80000u) {
        s->hit_n++;
        if (s->hit_n > 15) s->hit_n = 15;
        for (i = 0; i < s->hit_n; i++) {
            if (!node_pos(w, (uint32_t)i, &p) || !wall_get(w, (uint32_t)i, &a)) return;
            a.flg = 1; a.px = p.x; a.py = p.y; a.pz = p.z;
            wall_put(w, (uint32_t)i, &a);
        }
    } else {
        walls_off(w, obj_num(w));
        s->hit_n = 0;
    }
}
void recvx_en13b(RECVX_ENEMY_WORK* w)
{
    uint32_t i, n;
    if (!w) return;
    if (w->mode0 != 0 && !w->exp0) { gap(w, "bhEne13B: exp0 fehlt"); return; }
    switch (w->mode0) {
    case 0: b_init(w); break;
    case 1:
        if (w->mode2 == 0) break;               /* bhEne13B_MV00 ist original leer */
        if (w->mode2 == 1) b_mv01(w);
        else { gap(w, "bhEne13B_MoveMode2[mode2]"); return; }
        break;
    case 2: case 3: break;                      /* Nage/Damage original leer */
    case 4: b_die(w); break;
    case 5:
        if (w->sv && w->sv->event) w->sv->event(w);
        else gap(w, "bhEne_Event (en13B)");
        break;
    default: gap(w, "bhEne13B_Mode0[mode0]"); return;
    }
    if (!w->exp0) return;
    if (!w->lkwkp) { gap(w, "bhEne13B: lkwkp fehlt"); return; }
    n = obj_num(w);
    if (w->lkwkp->stflg & 0x1000000u) {
        w->stflg |= 0x1000000u;
        walls_off(w, n);
    }
    for (i = 0; i < n; i++)
        if (!node_pos(w, i, &stb(w)->prev[i])) return;
    BASE(w, set_motion, "bhSetMotion (en13B)");
    w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    BASE(w, calc_model, "bhCalcModel (en13B)");
    w->sv->calc_model(w);
    b_set_hittab(w);
}
