/* EN29 Tentakel — eigenstaendige native Zustandsmaschine auf dem einzigen
   WorldEntity-Work. Semantikreferenz (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en29.c:164 bhEne29, :178 Init, :309 Move, :362 Die,
   :368 Damage, :374 Event, :392 Br00, :594 Br01, :825-889 Mv00..Mv20,
   :892 ActionInit, :902 ActionSearch, :943 ActionChange, :997 ActionMain,
   :1017 TargetAnalyze, :1104 CalcEnemy, :1119 DmgCheck, :1217 AttackHitCheck,
   :1262 PlySetDamage, :1330 PlyMoveMain, :1384-1472 PlyAction*, :1475-1523
   PlyDmg117/118/Rtn, :1526 SetDmgEffect, :1697 SetDmgEne, :1729 CalcDmgEne,
   :1755 CheckDmgEne. Datentabellen en29.c:36-158 als reine Daten. Ungebundene
   Dienste melden den Originalnamen und erfinden keinen Wert. */
#include "en29.h"
#include "en03_collision.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const RECVX_EN29_SERVICES* en29_sv;
void recvx_en29_bind(const RECVX_EN29_SERVICES* services) { en29_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
#define SV(w, f, name)    do { if (!en29_sv || !en29_sv->f) { gap(w, name); return; } } while (0)
#define BASE(w, f, name)  do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return; } } while (0)
#define BASEI(w, f, name) do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return 0; } } while (0)

/* TC_BR_MODE0/1 (en29.h:36-48), TC_ACT_NO (en29.h:6). */
enum { BR0_NORMAL = 0, BR0_ATTACK = 1, BR0_DAMAGE = 2, BR0_DIE = 3, BR0_WAIT = 4 };
enum { BR1_FIRST = 0, BR1_LOOP0 = 1, BR1_LOOP1 = 2 };
enum { ACT_000 = 0, ACT_001 = 1, ACT_002 = 2, ACT_003 = 3, ACT_004 = 4, ACT_010 = 10, ACT_011 = 11,
       ACT_012 = 12, ACT_013 = 13, ACT_014 = 14, ACT_020 = 20, ACT_022 = 22, ACT_ALL = -1 };

static RECVX_EN29_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN29_STATE*)w->exp0; }

/* --- Datentabellen (en29.c:36-158) ---------------------------------------- */
/* En29ActTbl[40]: prg 0 Mv00, 1 Mv01, 2 Mv02, 3 Mv03, 4 Mv04, 5 Mv20. */
static const RECVX_EN19_EA en29_act_tbl[40] = {
    {   256,  0, 0, 0, 36, 1.0f, 0x20, 0 }, {   514,  2, 0, 0,  0, 1.0f,    0, 2 },
    {   768,  0, 0, 0, 24, 1.0f, 0x20, 0 }, {  1541,  5, 0, 0, 36, 1.0f, 0x20, 0 },
    {  1799,  7, 0, 0,  0, 1.0f,    0, 2 }, {  2053,  5, 0, 0, 24, 1.0f, 0x20, 0 },
    {  2826, 10, 0, 0, 36, 1.0f, 0x20, 0 }, {  3084, 12, 0, 0,  0, 1.0f,    0, 2 },
    {  3338, 10, 0, 0, 24, 1.0f, 0x20, 0 }, {  4111, 15, 0, 0, 36, 1.0f, 0x20, 0 },
    {  4369, 17, 0, 0,  0, 1.0f,    0, 2 }, {  4623, 15, 0, 0, 24, 1.0f, 0x20, 0 },
    {  7680,  0, 0, 0,  0, 1.0f,    0, 0 }, {  7685,  5, 0, 0,  0, 1.0f,    0, 0 },
    {  7690, 10, 0, 0,  0, 1.0f,    0, 0 }, {  7695, 15, 0, 0,  0, 1.0f,    0, 0 },
    { 65280,  0, 0, 0,  8, 1.0f, 0x20, 0 }, { 65281,  1, 0, 0,  8, 1.0f, 0x20, 1 },
    { 65282,  2, 0, 0,  8, 1.0f, 0x20, 2 }, { 65283,  3, 0, 0,  8, 1.0f, 0x20, 3 },
    { 65284,  4, 0, 0,  8, 1.0f, 0x20, 4 }, { 65285,  5, 0, 0,  8, 1.0f, 0x20, 0 },
    { 65286,  6, 0, 0,  8, 1.0f, 0x20, 1 }, { 65287,  7, 0, 0,  8, 1.0f, 0x20, 2 },
    { 65288,  8, 0, 0,  8, 1.0f, 0x20, 3 }, { 65289,  9, 0, 0,  8, 1.0f, 0x20, 4 },
    { 65290, 10, 0, 0,  8, 1.0f, 0x20, 0 }, { 65291, 11, 0, 0,  8, 1.0f, 0x20, 1 },
    { 65292, 12, 0, 0,  8, 1.0f, 0x20, 2 }, { 65293, 13, 0, 0,  8, 1.0f, 0x20, 3 },
    { 65294, 14, 0, 0,  8, 1.0f, 0x20, 4 }, { 65295, 15, 0, 0,  8, 1.0f, 0x20, 0 },
    { 65296, 16, 0, 0,  8, 1.0f, 0x20, 1 }, { 65297, 17, 0, 0,  8, 1.0f, 0x20, 2 },
    { 65298, 18, 0, 0,  8, 1.0f, 0x20, 3 }, { 65299, 19, 0, 0,  8, 1.0f, 0x20, 4 },
    { 65300, 20, 0, 0,  8, 1.0f,    0, 5 }, { 65301, 21, 0, 0,  8, 1.0f,    0, 5 },
    { 65302, 22, 0, 0,  8, 1.0f, 0x20, 0 }, { 65303, 23, 0, 0,  8, 1.0f, 0x20, 0 }
};
static const RECVX_T3_COMBWEP en29_comb_wep[21] = {
    {  0, {  0,  0,  0 },   0, 0 }, {  0, {  0,  0,  0 },   0, 0 }, { 32, {  8,  0,  0 }, 160, 0 },
    { 32, { 11, 10,  9 },  90, 0 }, { 32, { 11, 10,  9 },  90, 0 }, { 32, { 11, 10,  9 },  90, 0 },
    { 32, { 11, 10,  9 },  90, 0 }, {  0, {  0,  0,  0 },  30, 0 }, { 32, {  4,  3,  2 },  30, 0 },
    { 32, { 11, 10,  9 },  90, 0 }, { 32, { 11, 10,  9 },  60, 0 }, { 32, { 16, 15, 14 },  80, 0 },
    { 32, {  4,  3,  2 },  30, 0 }, { 32, {  9,  8,  7 },  90, 0 }, {  1, {  1,  1,  1 },   0, 0 },
    {  1, {  1,  1,  1 },   0, 0 }, {  1, {  1,  1,  1 },   0, 0 }, {  1, {  1,  1,  1 },   0, 0 },
    {  1, {  1,  1,  1 },   0, 0 }, { 24, { 11, 10,  9 },  60, 0 }, {  1, {  1,  1,  1 },   0, 0 }
};
static const RECVX_T3_COMBJOINT en29_comb_joint[21] = { { 0u, 0 } };
/* TypPrm[4] (ET_WORK): bas_obj, atr_obj, atr_siz, atk_obj0, atk_rng0, atk_obj1, atk_rng1;
   TC_OBJ_BODYnn = nn + 1 (en29.h:50). */
typedef struct { int32_t bas_obj, atr_obj; RECVX_ENEMY_VEC atr_siz; int32_t atk_obj0; float atk_rng0; int32_t atk_obj1; float atk_rng1; } EN29_ET;
static const EN29_ET en29_typ_prm[4] = {
    { 1, 4, { 3.0f, 0.0f, 3.0f }, 12, 6.5f, 18, 6.5f },
    { 1, 4, { 3.0f, 0.0f, 3.0f }, 12, 6.0f, 17, 7.9f },
    { 1, 4, { 3.0f, 0.0f, 3.0f },  9, 9.0f, 17, 8.0f },
    { 1, 4, { 3.0f, 0.0f, 3.0f },  9, 9.0f, 17, 8.0f }
};
static const int32_t en29_eff_tbl[9][4] = {
    { 0, 3, 0, 0 }, { 0, 4, 0, 0 }, { 0, 5, 0, 0 }, { 1, 3, 0, 0 }, { 1, 4, 0, 0 },
    { 1, 5, 0, 0 }, { 2, 1, 2, 306 }, { 3, 2, 5, 301 }, { 4, 131072, 40, 298 }
};
static const int8_t en29_flp_tbl[20] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
static const RECVX_ENEMY_CPCL en29_cap_col[15] = {
    {  1,  4, 20 }, {  4,  6, 20 }, {  6,  8, 18 }, {  8, 10, 18 }, {  9, 10, 16 },
    { 10, 11, 16 }, { 11, 12, 14 }, { 12, 13, 14 }, { 13, 14, 12 }, { 14, 15, 12 },
    { 15, 16, 10 }, { 16, 17, 10 }, { 17, 18,  8 }, { 18, 19,  8 }, {  0,  0,  0 }
};
/* En29PlyActTbl[2]: prg 0 = PlyDmg117, 1 = PlyDmg118. */
static const RECVX_EN19_ACT en29_ply_act_tbl[2] = {
    { 30, 0, 0, 8, 255, -1, 0x20, 0 },
    { 31, 0, 0, 8, 255, -1, 0x20, 1 }
};
/* En29DmgDat (ETTY_WORK): flg 0x8021, id 29, type 8. */
static const RECVX_ENEMY_TEMPLATE en29_dmg_dat = { 0x8021u, 29, 8, 0, 0, 0.0f, 0.0f, 0.0f, 0 };
/* E29DmgSet[5] (DS_WORK): atr_top, atr_end, atr_rad, or_flg2. */
typedef struct { int32_t atr_top, atr_end; float atr_rad; int32_t or_flg2; } EN29_DS;
static const EN29_DS en29_dmg_set[5] = { { 1, 4, 2.0f, 16 }, { 5, 8, 2.0f, 0 }, { 9, 12, 2.0f, 16 }, { 13, 16, 2.0f, 16 }, { 17, 19, 2.0f, 16 } };

/* --- Hilfen ---------------------------------------------------------------- */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en29)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;   /* (-rand() / -2.1474836E9f) */
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en29)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (w->sv && w->sv->player_set) w->sv->player_set(p);
    else gap(w, "plp schreiben (en29)");
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    BASEI(w, joint_matrix, "mlwP->owP[n].mtx (en29)");
    if (!w->sv->joint_matrix(w, j, (float*)m)) { gap(w, "mlwP->owP[n].mtx ausserhalb des Modells (en29)"); return 0; }
    return 1;
}
static int jpos(RECVX_ENEMY_WORK* w, uint32_t j, NJS_POINT3* out)
{
    NJS_MATRIX m;
    if (!joint(w, j, &m)) return 0;
    out->x = m[12]; out->y = m[13]; out->z = m[14];
    return 1;
}
static RECVX_ENEMY_VEC vec_of(const NJS_POINT3* p) { RECVX_ENEMY_VEC v = { p->x, p->y, p->z }; return v; }
static int effect_tb(RECVX_ENEMY_WORK* w, uint32_t mask, const RECVX_EN19_EF* ef,
                     const NJS_POINT3* ofs, RECVX_ENEMY_WORK* link, int jnt)
{
    RECVX_ENEMY_VEC o;
    if (!en29_sv || !en29_sv->effect_tb) { gap(w, "bhSetEffectTb (en29)"); return -1; }
    if (ofs) { o.x = ofs->x; o.y = ofs->y; o.z = ofs->z; }
    return en29_sv->effect_tb(mask, ef, ofs ? &o : NULL, link, jnt);
}
/* sys->mwalp[hit_tab].flg |= or; &= and (Lesen-Aendern-Schreiben der Flaeche). */
static void hit_flags(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, uint32_t and_flg, uint32_t or_flg)
{
    RECVX_ENEMY_ATR a;
    if (fw->hit_tab < 0) { gap(w, "fwP->hit_tabP NULL (en29)"); return; }
    BASE(w, mobile_wall_get, "sys->mwalp (en29)");
    BASE(w, mobile_wall_set, "sys->mwalp (en29)");
    if (!w->sv->mobile_wall_get(fw->hit_tab, &a)) { gap(w, "sys->mwalp[hit_tab] ohne Zuteilung (en29)"); return; }
    a.flg = (a.flg & and_flg) | or_flg;
    w->sv->mobile_wall_set(fw->hit_tab, &a);
}
static void shadow_hidden(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int hidden)
{
    if (fw->sdw_owk < 0) { gap(w, "fwP->sdw_owkP NULL (en29.c:316/382)"); return; }
    SV(w, shadow_hidden, "sdw_owkP->stflg (en29)");
    en29_sv->shadow_hidden(fw->sdw_owk, hidden);
}
static void enemy_se(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int se)
{
    NJS_POINT3 p;
    RECVX_ENEMY_VEC v;
    BASE(w, enemy_se, "RequestEnemySe (en29)");
    BASE(w, enemy_now, "sys->enow (en29)");
    if (!jpos(w, (uint32_t)fw->bas_obj, &p)) return;
    v = vec_of(&p);
    w->sv->enemy_se(w->sv->enemy_now(), &v, se);
}

/* ps2_NaColi.c:446 njCollisionCheckSC(sphere, capsule): Fusspunkt auf der
   unendlichen Geraden, auf die Segmentenden geklemmt, dann Abstandsvergleich
   (dieselbe Pruefung wie en19.c bhEne19_AttackHitCheck). */
static int nj_collision_check_sc(const NJS_POINT3* c, float r, const RECVX_ENEMY_CAPSULE* cap)
{
    RECVX_EN03_LINE line;
    RECVX_ENEMY_VEC point, sc = { c->x, c->y, c->z };
    float len;

    line.v.x = cap->c2.x - cap->c1.x; line.v.y = cap->c2.y - cap->c1.y; line.v.z = cap->c2.z - cap->c1.z;
    line.p = cap->c1;
    len = cap->r + r;
    if (len <= recvx_en03_distance_p2l(&sc, &line, &point)) return 0;
    len = recvx_en03_distance_p2p(&cap->c1, &cap->c2);
    if (len < recvx_en03_distance_p2p(&point, &cap->c1)) point = cap->c2;
    else if (len < recvx_en03_distance_p2p(&point, &cap->c2)) point = cap->c1;
    if (recvx_en03_distance_p2p(&sc, &point) < (cap->r + r)) return 1;
    return 0;
}

/* --- Vorwaertsdeklarationen -------------------------------------------------- */
static void en29_init(RECVX_ENEMY_WORK* w);
static void en29_move(RECVX_ENEMY_WORK* w);
static void en29_damage(RECVX_ENEMY_WORK* w);
static void en29_die(RECVX_ENEMY_WORK* w);
static void en29_event(RECVX_ENEMY_WORK* w);
static void en29_br00(RECVX_ENEMY_WORK* w);
static void en29_br01(RECVX_ENEMY_WORK* w);
static void mv00(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count);
static void mv01(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count);
static void mv02(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count);
static void mv03(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count);
static void mv04(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count);
static void mv20(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count);
static void action_init(RECVX_EN29_EAW* eaw, const RECVX_EN19_EA* tbl, int n);
static int  action_change(RECVX_ENEMY_WORK* w, RECVX_EN29_EAW* eaw, int act_dst);
static int  action_main(RECVX_ENEMY_WORK* w, RECVX_EN29_EAW* eaw);
static void target_analyze(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static void calc_enemy(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static void dmg_check(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static int  attack_hit_check(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static int  ply_set_damage(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN29_STATE* fw, int dmg_mde);
static void ply_move_main(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static void ply_action_init(RECVX_EN29_PAW* paw, const RECVX_EN19_ACT* tbl, int n);
static void ply_action_main(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN29_STATE* fw);
static int  ply_action_change(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN29_STATE* fw, int act_no);
static void ply_dmg117(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static void ply_dmg118(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static void ply_dmg_rtn(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static void set_dmg_effect(RECVX_ENEMY_WORK* w, int eff_typ);
static void set_dmg_ene(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, const EN29_DS* ds, int n);
static void calc_dmg_ene(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);
static void check_dmg_ene(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw);

/* en29.c:19 bhEne29_Mode0[6] (Index 4 ist NULL), :28 BrainType[4]. */
typedef void (*en29_mode_fn)(RECVX_ENEMY_WORK*);
static const en29_mode_fn en29_mode0[6] = { en29_init, en29_move, en29_damage, en29_die, NULL, en29_event };
static const en29_mode_fn en29_brain[4] = { en29_br00, en29_br00, en29_br01, en29_br01 };
static const RECVX_EN29_MV en29_mv[6] = { mv00, mv01, mv02, mv03, mv04, mv20 };
static const RECVX_EN29_PLY en29_ply[2] = { ply_dmg117, ply_dmg118 };

/* en29.c:164 bhEne29: Typ 8 sind die Schadenskapseln (SetDmgEne), sie tun nichts. */
void recvx_en29(RECVX_ENEMY_WORK* w)
{
    if (!w || w->type == 8) return;
    if (w->mode0 >= 6u || !en29_mode0[w->mode0]) { gap(w, "bhEne29_Mode0[mode0] ohne Routine (en29.c:19)"); return; }
    en29_mode0[w->mode0](w);
    if (w->mode0 != 0 && st(w)) st(w)->mode0_bak = w->mode0;          /* :170 */
}

/* en29.c:178 bhEne29_Init */
static void en29_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN29_STATE* fw = st(w);
    const EN29_ET* etP;
    RECVX_ENEMY_ATR a;

    if (fw != NULL) {
        /* :187 npSetMemory(&fwP->status, 252, 0): ab status bis zum Blockende. */
        memset(&fw->status, 0, offsetof(RECVX_EN29_STATE, exp1_wall) - offsetof(RECVX_EN29_STATE, status));
        fw->sdw_owk = -1; fw->hit_tab = -1;                          /* genullte Zeiger = NULL */
    } else {
        BASE(w, calloc_work, "bhEne_CallocWork (en29)");
        w->exp0 = w->sv->calloc_work(sizeof(RECVX_EN29_STATE));      /* :191 */
        if (!w->exp0) { gap(w, "bhEne_CallocWork: Budget erschoepft (en29.c:191)"); return; }
        fw = st(w);
        fw->sdw_owk = -1; fw->hit_tab = -1; fw->exp1_wall = -1;
        /* :193 htP = &sys->mwalp[sys->mwal_n++] */
        BASE(w, mobile_wall_alloc, "sys->mwalp (en29.c:193)");
        fw->exp1_wall = w->sv->mobile_wall_alloc();
        if (fw->exp1_wall < 0) { gap(w, "sys->mwalp erschoepft (en29.c:193)"); }
        else if (w->sv->mobile_wall_get && w->sv->mobile_wall_set && w->sv->mobile_wall_get(fw->exp1_wall, &a)) {
            a.flg |= 0x81u; a.type = 1; a.flr_no = 0; a.attr = 2;   /* :195-203 */
            a.prm0 = a.prm1 = a.prm2 = a.prm3 = 0;
            w->sv->mobile_wall_set(fw->exp1_wall, &a);
        } else gap(w, "sys->mwalp lesen/schreiben (en29.c:195)");
        set_dmg_ene(w, fw, en29_dmg_set, 5);                         /* :209 */
    }

    w->mode0 = 1; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;          /* :214 */
    w->ar = 3.0f; w->ah = 4.0f; w->aw = 0.0f; w->ad = 0.0f;
    w->car = 6.0f; w->cah = 4.0f;
    w->hp = 160;
    w->cpcl = en29_cap_col;
    w->mtn_no = 0; w->frm_no = 0;
    w->hokan_count = 0; w->hokan_rate = 0;
    w->mtn_add = 0; w->mtn_md = 0;
    w->mtn_tp = en29_flp_tbl;
    w->flg |= 0x158u; w->flg &= ~0x2u;                               /* :241 */
    w->flg2 |= 0x1u;
    w->clp_jno[0] = 9; w->clp_jno[1] = 15; w->clp_jno[2] = 19; w->clp_jno[3] = -1;
    w->mdflg &= ~0x20u;
    w->lok_jno = 16;
    fw->hit_tab = fw->exp1_wall;                                     /* :255 hit_tabP = exp1 */

    if (w->type > 3u) { gap(w, "TypPrm[type] ausserhalb (en29.c:263)"); return; }
    etP = &en29_typ_prm[w->type];
    fw->bas_obj = etP->bas_obj; fw->atr_obj = etP->atr_obj;
    fw->atk_obj0 = etP->atk_obj0; fw->atk_obj1 = etP->atk_obj1;
    fw->atk_rng0 = etP->atk_rng0; fw->atk_rng1 = etP->atk_rng1;
    if (fw->hit_tab < 0) gap(w, "htP NULL (en29.c:276)");
    else if (w->sv->mobile_wall_get && w->sv->mobile_wall_set && w->sv->mobile_wall_get(fw->hit_tab, &a)) {
        a.flg |= 0x1u;                                               /* :276 */
        a.w = etP->atr_siz.x; a.h = etP->atr_siz.y; a.d = etP->atr_siz.z;
        a.px = w->px; a.py = w->py; a.pz = w->pz;                    /* :284 */
        w->sv->mobile_wall_set(fw->hit_tab, &a);
    } else gap(w, "sys->mwalp lesen/schreiben (en29.c:276)");

    if (!(w->flg & 0x800u)) {                                        /* :286 */
        w->flg |= 0x800u;
        if (!en29_sv || !en29_sv->shadow) gap(w, "rySetShadow bhEff304 (en29.c:290)");
        else {
            fw->sdw_owk = en29_sv->shadow(w, 1, 3, 5, 2.0f, 2.0f);
            /* :294 oP->mdlver = 1 ohne Pruefung: bei vollem eff[]-Pool schreibt das Original durch NULL. */
            if (fw->sdw_owk < 0) gap(w, "rySetShadow: eff-Pool voll, oP NULL (en29.c:294)");
            else if (en29_sv->shadow_fields) en29_sv->shadow_fields(fw->sdw_owk, 1u, 1u, 6.0f);
            else gap(w, "oP->mdlver/type/sy (en29.c:294)");
        }
    }

    action_init(&fw->ene_act, en29_act_tbl, 40);                     /* :302 */
    ply_action_init(&fw->ply_act, en29_ply_act_tbl, 2);              /* :303 */
    SV(w, room_flags, "sys->rm_flg (en29.c:305)");
    en29_sv->room_flags(0u, 0x3u);                                   /* :305 sys->rm_flg &= ~0x3 */
}

/* en29.c:309 bhEne29_Move */
static void en29_move(RECVX_ENEMY_WORK* w)
{
    RECVX_EN29_STATE* fw = st(w);
    uint32_t rm_flg;

    hit_flags(w, fw, ~0u, 0x1u);                                     /* :315 */
    shadow_hidden(w, fw, 0);                                         /* :316 */

    if (!(fw->status & 0x40)) {                                      /* :318 */
        fw->status |= 0x40;
        fw->br_mde0 = BR0_NORMAL;
        fw->br_mde1 = BR1_FIRST;
        if (!w->sv || !w->sv->system_flags) { gap(w, "sys->rm_flg lesen (en29.c:325)"); return; }
        rm_flg = w->sv->system_flags(1);
        if (w->type == 2 && !(rm_flg & 0x1u)) fw->br_mde0 = BR0_WAIT;
        if (w->type == 3 && !(rm_flg & 0x2u)) fw->br_mde0 = BR0_WAIT;
    }

    dmg_check(w, fw);                                                /* :336 */
    if (w->type > 3u) { gap(w, "bhEne29_BrainType[type] ausserhalb (en29.c:338)"); return; }
    en29_brain[w->type](w);
    if (!(fw->status & 0x10)) action_main(w, &fw->ene_act);          /* :340 */
    calc_enemy(w, fw);                                               /* :350 */
    if (fw->status & 0x20) hit_flags(w, fw, ~0x1u, 0u);              /* :353 */
    ply_move_main(w, fw);                                            /* :358 */
}

/* en29.c:362/368 bhEne29_Die/Damage: leer. */
static void en29_die(RECVX_ENEMY_WORK* w) { (void)w; }
static void en29_damage(RECVX_ENEMY_WORK* w) { (void)w; }

/* en29.c:374 bhEne29_Event */
static void en29_event(RECVX_ENEMY_WORK* w)
{
    RECVX_EN29_STATE* fw = st(w);
    hit_flags(w, fw, ~0x1u, 0u);
    shadow_hidden(w, fw, 1);
    BASE(w, event, "bhEne_Event (en29.c:384)");
    w->sv->event(w);
    BASE(w, set_motion, "bhSetMotion (en29.c:386)");
    (void)w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    calc_enemy(w, fw);
}

/* Gemeinsamer Schluss von Br00/Br01 (en29.c:578-590, :809-821). */
static void brain_act(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int act, int limit, int typ)
{
    if (act == ACT_ALL) return;
    if (act < limit) act += typ * 5; else act += typ;
    action_change(w, &fw->ene_act, act);
}

/* en29.c:392 bhEne29_Br00 (Typ 0/1) */
static void en29_br00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN29_STATE* fw = st(w);
    RECVX_EN29_EAW* eawP = &fw->ene_act;
    int32_t* stsP = &fw->status;
    RECVX_ENEMY_PLAYER p;
    int typ = (w->type != 0) ? 1 : 0;
    int act = ACT_ALL, lop = 0, sts, rte;

    if (!player(w, &p)) return;
    fw->tgt_pos = p.pos;                                             /* :412 */
    *stsP |= 0x1;
    target_analyze(w, fw);
    sts = *stsP;
    rte = eawP->mtn_rte;

    do {
        if (fw->dmg_lvl != 0 && fw->br_mde0 != BR0_DIE) {            /* :431 */
            if (fw->br_mde0 != BR0_DAMAGE) {
                fw->br_mde0 = (fw->dmg_lvl < 3) ? BR0_DAMAGE : BR0_DIE;
                fw->br_mde1 = BR1_FIRST;
            } else if (fw->dmg_lvl >= 3) { fw->br_mde0 = BR0_DIE; fw->br_mde1 = BR1_FIRST; }
        }
        switch (fw->br_mde0) {
        case BR0_NORMAL:
            if (fw->br_tim > 0) { fw->br_tim--; act = ACT_000; }
            else if ((sts & 0x200) && !(p.flg & 0x4u)) { fw->br_mde0 = BR0_ATTACK; fw->br_mde1 = BR1_FIRST; lop = 1; }
            else act = ACT_000;
            if (act == ACT_000 && eawP->act_now == typ * 5) act = ACT_ALL;   /* :474 */
            break;
        case BR0_ATTACK:
            switch (fw->br_mde1) {
            case BR1_FIRST: fw->br_mde1 = BR1_LOOP0; act = ACT_001; break;
            case BR1_LOOP0: if (!(sts & 0x200)) { act = ACT_022; fw->br_mde1 = BR1_LOOP1; } break;
            case BR1_LOOP1: if (rte >= eawP->chg_rte) { fw->br_mde0 = BR0_NORMAL; fw->br_tim = 0; } break;
            }
            break;
        case BR0_DAMAGE:
            switch (fw->br_mde1) {
            case BR1_FIRST:
                if (fw->br_fir > 0) { fw->br_mde1 = BR1_LOOP1; act = ACT_003; }
                else { act = (fw->dmg_lvl <= 1) ? ACT_002 : ACT_003; fw->br_mde1 = BR1_LOOP0; }
                break;
            case BR1_LOOP0: if (rte >= 32768) fw->br_mde0 = BR0_NORMAL; break;
            case BR1_LOOP1: if (fw->br_fir <= 0) fw->br_mde0 = BR0_NORMAL; break;
            }
            break;
        case BR0_DIE:
            switch (fw->br_mde1) {
            case BR1_FIRST: act = ACT_004; fw->br_mde1 = BR1_LOOP0; break;
            case BR1_LOOP0: if (rte >= eawP->chg_rte) { w->flg |= 0x2u; w->mtn_add = 0; fw->br_mde1 = BR1_LOOP1; } break;
            case BR1_LOOP1: break;
            }
            break;
        }
    } while (lop-- != 0);
    brain_act(w, fw, act, ACT_022, typ);                             /* :578 */
}

/* en29.c:594 bhEne29_Br01 (Typ 2/3) */
static void en29_br01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN29_STATE* fw = st(w);
    RECVX_EN29_EAW* eawP = &fw->ene_act;
    int32_t* stsP = &fw->status;
    RECVX_ENEMY_PLAYER p;
    int typ = (w->type == 2) ? 0 : 1;
    int act = ACT_ALL, lop = 0, sts, rte;
    uint32_t rm_flg;

    if (!player(w, &p)) return;
    fw->tgt_pos = p.pos;                                             /* :614 */
    *stsP |= 0x1;
    target_analyze(w, fw);
    rte = eawP->mtn_rte;
    sts = *stsP;

    do {
        if (fw->dmg_lvl != 0 && fw->br_mde0 != BR0_DIE) {            /* :632 */
            if (fw->br_mde0 != BR0_DAMAGE) {
                fw->br_mde0 = (fw->dmg_lvl < 3) ? BR0_DAMAGE : BR0_DIE;
                fw->br_mde1 = BR1_FIRST;
            } else if (fw->dmg_lvl >= 3) { fw->br_mde0 = BR0_DIE; fw->br_mde1 = BR1_FIRST; }
        }
        switch (fw->br_mde0) {
        case BR0_NORMAL:
            if (fw->br_tim > 0) { fw->br_tim--; act = ACT_010; }
            else if ((sts & 0x200) && !(p.flg & 0x4u)) { fw->br_mde0 = BR0_ATTACK; fw->br_mde1 = BR1_FIRST; lop = 1; }
            else act = ACT_010;
            if (act == ACT_010 && eawP->act_now == typ * 5 + 10) act = ACT_ALL;   /* :675 */
            break;
        case BR0_ATTACK:
            switch (fw->br_mde1) {
            case BR1_FIRST: act = ACT_011; fw->br_mde1 = BR1_LOOP0; break;
            case BR1_LOOP0: if (!(sts & 0x200)) { fw->br_mde1 = BR1_LOOP1; fw->br_tim = 48; } break;
            case BR1_LOOP1: if (fw->br_tim-- == 0) { fw->br_mde0 = BR0_NORMAL; fw->br_tim = 16; } break;
            }
            break;
        case BR0_DAMAGE:
            switch (fw->br_mde1) {
            case BR1_FIRST:
                if (fw->br_fir > 0) { act = ACT_013; fw->br_mde1 = BR1_LOOP1; }
                else { act = (fw->dmg_lvl <= 1) ? ACT_012 : ACT_013; fw->br_mde1 = BR1_LOOP0; }
                break;
            case BR1_LOOP0: if (rte >= 32768) fw->br_mde0 = BR0_NORMAL; break;
            case BR1_LOOP1: if (fw->br_fir <= 0) fw->br_mde0 = BR0_NORMAL; break;
            }
            break;
        case BR0_DIE:
            switch (fw->br_mde1) {
            case BR1_FIRST: act = ACT_014; fw->br_mde1 = BR1_LOOP0; break;
            case BR1_LOOP0: if (rte >= eawP->chg_rte) { w->flg |= 0x2u; w->mtn_add = 0; fw->br_mde1 = BR1_LOOP1; } break;
            case BR1_LOOP1: break;
            }
            break;
        case BR0_WAIT:
            switch (fw->br_mde1) {
            case BR1_FIRST: act = ACT_020; fw->br_mde1 = BR1_LOOP0; break;
            case BR1_LOOP0:
                if (!w->sv || !w->sv->system_flags) { gap(w, "sys->rm_flg lesen (en29.c:785)"); return; }
                rm_flg = w->sv->system_flags(1);
                if (rm_flg & (1u << typ)) { fw->br_mde1 = BR1_LOOP1; *stsP &= ~0x10; }
                else *stsP |= 0x10;
                break;
            case BR1_LOOP1: if (rte >= eawP->chg_rte) fw->br_mde0 = BR0_NORMAL; break;
            }
            break;
        }
    } while (lop-- != 0);
    brain_act(w, fw, act, ACT_020, typ);                             /* :809 */
}

/* en29.c:825-889 Mv00..Mv20 */
static void mv00(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count) { (void)w; (void)fw; (void)count; }
static void mv_hit(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    RECVX_ENEMY_PLAYER p;
    if (attack_hit_check(w, fw) != -1) {
        if (!player(w, &p)) return;
        ply_set_damage(w, &p, fw, 2);
        put_player(w, &p);
        fw->status |= 0x8;
    }
}
static void mv01(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count)
{
    if (count == 0) enemy_se(w, fw, 74498);
    mv_hit(w, fw);
}
static void mv02(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count)
{
    if (count == 0) enemy_se(w, fw, 74499);
}
static void mv03(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count)
{
    if (count == 0) enemy_se(w, fw, 74499);
    mv_hit(w, fw);
}
static void mv04(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count)
{
    (void)count;
    if (fw->ene_act.mtn_rte < 65536) {                               /* :874 ChechPlayEnemySe */
        BASE(w, enemy_se_playing, "ChechPlayEnemySe (en29.c:874)");
        BASE(w, enemy_now, "sys->enow (en29.c:874)");
        if (w->sv->enemy_se_playing(w->sv->enemy_now(), 74499) == 0) enemy_se(w, fw, 74499);
    }
}
static void mv20(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, int count)
{
    (void)count;
    mv_hit(w, fw);
}

/* en29.c:892 bhEne29_ActionInit */
static void action_init(RECVX_EN29_EAW* eaw, const RECVX_EN19_EA* tbl, int n)
{
    eaw->act_tblP = tbl;
    eaw->tbl_num = n;
    eaw->act_now = 30;
}

/* en29.c:902 bhEne29_ActionSearch: Binaersuche; high beginnt bei tbl_num (hinter
   dem Ende). Liegt der Schluessel ueber jedem Eintrag, liest das Original
   act_tblP[tbl_num]; nativ gemeldet. */
static const RECVX_EN19_EA* action_search(RECVX_ENEMY_WORK* w, const RECVX_EN29_EAW* eaw, int act_nw, int act_no)
{
    int key = ((int)(uint8_t)act_nw << 8) | (int)(uint8_t)act_no;
    int high = eaw->tbl_num, low = 0, middle, val;
    while (low <= high) {
        middle = (low + high) / 2;
        if (middle >= eaw->tbl_num) { gap(w, "bhEne29_ActionSearch: act_tblP[tbl_num] ausserhalb (en29.c:920)"); return NULL; }
        val = eaw->act_tblP[middle].label;
        if (key != val) { if (key < val) high = middle - 1; else if (key > val) low = middle + 1; }
        else return &eaw->act_tblP[middle];
    }
    return NULL;
}

/* en29.c:943 bhEne29_ActionChange */
static int action_change(RECVX_ENEMY_WORK* w, RECVX_EN29_EAW* eaw, int act_dst)
{
    const RECVX_EN19_EA* eaP;
    if ((eaP = action_search(w, eaw, eaw->act_now, act_dst)) == NULL && eaw->act_now != act_dst)
        eaP = action_search(w, eaw, -1, act_dst);
    if (eaP != NULL) {
        /* :954 njMemCopy4(&b_act_flg, &act_flg, 7): act_flg..act_cnt. */
        eaw->b_act_flg = eaw->act_flg; eaw->b_prgP = eaw->prgP; eaw->b_act_frm = eaw->act_frm;
        eaw->b_act_now = eaw->act_now; eaw->b_mtn_rte = eaw->mtn_rte; eaw->b_chg_rte = eaw->chg_rte;
        eaw->b_act_cnt = eaw->act_cnt;
        eaw->b_mtn_md = (int32_t)w->mtn_md;
        eaw->act_flg = 0;
        eaw->prgP = (eaP->prg >= 0 && eaP->prg < 6) ? en29_mv[eaP->prg] : NULL;
        eaw->act_cnt = 0;
        eaw->act_frm = eaP->frm_no;
        w->mtn_no = eaP->mtn_no;
        w->frm_no = (int32_t)eaP->frm_no * 65536;
        w->hokan_rate = (int32_t)((double)eaP->hkn_lvl * (65536.0 / 255.0));   /* :968 */
        w->hokan_count = eaP->hkn_cnt;
        if (eaP->flag & 0x20000) w->mtn_add = 0;
        else if (eaP->flag & 0x40000) w->mtn_add = -65536;
        else w->mtn_add = 65536;
        w->mtn_md = (uint16_t)eaP->flag;
        eaw->chg_rte = (int32_t)(eaP->chg_rte * 65536.0f);
        eaw->act_now = act_dst;
        return 1;
    }
    return 0;
}

/* en29.c:997 bhEne29_ActionMain */
static int action_main(RECVX_ENEMY_WORK* w, RECVX_EN29_EAW* eaw)
{
    int ret;
    int16_t hi, lo;
    uint32_t frm_num;

    if (eaw->prgP != NULL) eaw->prgP(w, st(w), eaw->act_cnt++);      /* :1001 */
    /* :1006-1009 s16P = (short*)&act_frm: s16P[1] += bhSetMotion, s16P[0] = frm_no / 65536. */
    BASEI(w, set_motion, "bhSetMotion (en29.c:1008)");
    ret = w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    hi = (int16_t)((uint32_t)eaw->act_frm >> 16);
    hi = (int16_t)(hi + ret);
    lo = (int16_t)(w->frm_no / 65536);
    eaw->act_frm = (int32_t)(((uint32_t)(uint16_t)hi << 16) | (uint16_t)lo);
    BASEI(w, motion_frames, "epw->mnwP[mtn_no].frm_num (en29.c:1011)");
    frm_num = w->sv->motion_frames(w, w->mtn_no);
    if (!frm_num) { gap(w, "epw->mnwP[mtn_no] fehlt (en29.c:1011)"); return 0; }
    if (frm_num <= 1u) { gap(w, "bhEne29_ActionMain: frm_num - 1 == 0 (en29.c:1011)"); return 0; }
    eaw->mtn_rte = w->frm_no / (int32_t)(frm_num - 1u);
    return 0;
}

/* en29.c:1017 bhEne29_TargetAnalyze */
static void target_analyze(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    int32_t* stsP = &fw->status;
    RECVX_ENEMY_PLAYER p;
    NJS_MATRIX m;
    NJS_POINT3 a;
    float dx, dz, dst;
    int ad, dlt;

    if (!player(w, &p)) return;
    if ((*stsP & 0x1) && (p.hp < 0 || (p.flg & 0x2u))) *stsP |= 0x20;   /* :1024 */

    if (!joint(w, (uint32_t)fw->bas_obj, &m)) return;
    fw->bas_ax = (int32_t)(10430.381f * asinf(m[9]));                /* :1031 mtx[8..10] */
    fw->bas_ay = (int32_t)(10430.381f * atan2f(m[8], m[10]));

    dx = m[12] - fw->tgt_pos.x; dz = m[14] - fw->tgt_pos.z;          /* :1038 njSubVector */
    ad = (int)(10430.381f * atan2f(dx, dz)) - fw->bas_ay;
    *stsP &= ~0xC00;
    if ((int16_t)ad < 0) *stsP |= 0x800; else *stsP |= 0x400;

    dlt = (uint16_t)(p.ay - fw->bas_ay);                             /* :1059 */
    if (dlt & 0x8000) dlt = (int)((uint32_t)(65536 - dlt) | 0x80000000u);
    fw->dir_dlt = dlt;

    *stsP &= ~0x200;                                                 /* :1069 */
    if (!jpos(w, (uint32_t)fw->atk_obj0, &a)) return;
    dx = a.x - fw->tgt_pos.x; dz = a.z - fw->tgt_pos.z;
    dst = dx * dx + dz * dz;
    if (dst <= fw->atk_rng0 * fw->atk_rng0) *stsP |= 0x200;
    else {
        if (!jpos(w, (uint32_t)fw->atk_obj1, &a)) return;
        dx = a.x - fw->tgt_pos.x; dz = a.z - fw->tgt_pos.z;
        dst = dx * dx + dz * dz;
        if (dst <= fw->atk_rng1 * fw->atk_rng1) *stsP |= 0x200;
    }
}

/* en29.c:1104 bhEne29_CalcEnemy */
static void calc_enemy(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    NJS_POINT3 a;
    RECVX_ENEMY_ATR h;
    BASE(w, calc_model, "bhCalcModel (en29.c:1108)");
    w->sv->calc_model(w);
    calc_dmg_ene(w, fw);
    if (fw->hit_tab < 0) { gap(w, "fwP->hit_tabP NULL (en29.c:1114)"); return; }
    if (!jpos(w, (uint32_t)fw->atr_obj, &a)) return;
    BASE(w, mobile_wall_get, "sys->mwalp (en29.c:1114)");
    BASE(w, mobile_wall_set, "sys->mwalp (en29.c:1114)");
    if (!w->sv->mobile_wall_get(fw->hit_tab, &h)) { gap(w, "sys->mwalp[hit_tab] ohne Zuteilung (en29.c:1114)"); return; }
    h.px = a.x; h.pz = a.z;                                          /* :1114 */
    w->sv->mobile_wall_set(fw->hit_tab, &h);
}

/* en29.c:1119 bhEne29_DmgCheck */
static void dmg_check(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    /* :1121 DmgDat[21] (DD_WRK): eff_nml_p, eff_nml_s, eff_cmb_p, eff_cmb_s. */
    static const int32_t DmgDat[21][4] = {
        { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 3, 1, 4 }, { 0, 3, 1, 4 },
        { 0, 3, 1, 4 }, { 0, 3, 1, 4 }, { 1, 4, 2, 5 }, { 0, 3, 1, 4 },
        { 0, 3, 0, 4 }, { 0, 3, 1, 4 }, { 0, 3, 1, 4 }, { 1, 4, 2, 5 },
        { 0, 3, 1, 4 }, { 2, 4, 2, 5 }, { 2, 4, 2, 5 }, { 0, 8, 1, 8 },
        { 0, 6, 1, 6 }, { 2, 4, 2, 5 }, { 2, 5, 2, 5 }, { 1, 3, 2, 4 },
        { 2, 5, 2, 5 }
    };
    const int32_t* ddP;

    check_dmg_ene(w, fw);                                            /* :1133 */
    if (w->flg & 0x4u) {
        w->flg &= ~0x4u;
        recvx_t3_calc_damage(w, en29_comb_wep, en29_comb_joint);    /* :1139 */
        w->hp -= w->total_dam;
        fw->dmg_obj = w->djnt_no;
        if (fw->eff_wit == 0) {
            if (w->comb_wep >= 21u) { gap(w, "DmgDat[comb_wep] ausserhalb (en29.c:1149)"); return; }
            ddP = DmgDat[w->comb_wep];
            if (w->comb_flg & 0x1u) {
                set_dmg_effect(w, ddP[2]);
                set_dmg_effect(w, ddP[3]);
                set_dmg_effect(w, 7);
            } else {
                if (w->type == 2 || w->type == 3) set_dmg_effect(w, ddP[0]);
                set_dmg_effect(w, ddP[1]);
            }
        }
        if (w->comb_wep == 16u) { fw->br_fir = 128; fw->eff_wit = 24; }
        else if (w->comb_wep == 15u) { fw->br_fir = 64; fw->eff_wit = 24; }
        fw->dmg_lvl = (w->comb_flg & 0x1u) ? 2 : 1;
    } else fw->dmg_lvl = 0;

    if (fw->br_fir > 0) {                                            /* :1195 */
        fw->br_fir--;
        if (!(fw->br_fir & 0x3)) w->hp--;
    }
    if (fw->eff_wit != 0) fw->eff_wit--;
    if (w->hp < 0) fw->dmg_lvl = 3;
}

/* en29.c:1217 bhEne29_AttackHitCheck: Kugeln an sechs Gelenken gegen plp->watr. */
static int attack_hit_check(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    static const struct { uint32_t obj_no; float chk_siz; } E29Cll[6] = {
        { 18, 3.0f }, { 17, 3.0f }, { 15, 3.5f }, { 13, 4.0f }, { 11, 4.5f }, { 9, 4.5f } };
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    RECVX_ENEMY_CAPSULE cap;
    RECVX_ENEMY_PLAYER p;
    NJS_POINT3 c;
    int i, ang;

    if (!player(w, &p)) return -1;
    if (!ext || !ext->player_watr || !ext->player_watr(&cap)) { gap(w, "plp->watr (en29.c:1230)"); return -1; }
    for (i = 0; i < 6; i++) {
        if (!jpos(w, E29Cll[i].obj_no, &c)) return -1;
        if (!(p.flg & 0x4u) && nj_collision_check_sc(&c, E29Cll[i].chk_siz, &cap) != 0) {
            ang = fw->bas_ay;
            if (fw->status & 0x400) ang += 16384; else ang -= 16384;
            fw->ply_dmg.x = 1.0f * -njSin((Angle)ang);               /* :1244 */
            fw->ply_dmg.y = 0;
            fw->ply_dmg.z = 1.0f * -njCos((Angle)ang);
            fw->dmg_dir = ang;
            return i;
        }
    }
    return -1;
}

/* en29.c:1262 bhEne29_PlySetDamage */
static int ply_set_damage(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN29_STATE* fw, int dmg_mde)
{
    static const int PlyDmgTbl[4] = { 5, 5, 5, 5 };
    int dir_mde;
    if (dmg_mde > 3) dmg_mde = 3;
    if (dmg_mde < 0) { gap(w, "PlyDmgTbl[dmg_mde] ausserhalb (en29.c:1274)"); return 0; }
    if (!(p->flg & 0x4u)) {
        p->hp -= PlyDmgTbl[dmg_mde];
        if ((uint32_t)fw->dir_dlt & 0x80000000u) dir_mde = (fw->status & 0x400) ? 0 : 1;
        else dir_mde = (fw->status & 0x400) ? 1 : 0;
        if (p->hp > 0) {
            if (dmg_mde < 3) {
                p->flg |= 0x210004u; p->flg &= ~0x40000u;
                p->mode0 = 2; p->mode1 = (uint8_t)dir_mde; p->mode2 = (uint8_t)dmg_mde; p->mode3 = 0;
                fw->p_status = 1;
                fw->p_br_mde0 = 2; fw->p_br_mde1 = 0;
            } else {
                BASEI(w, player_voice, "CallPlayerVoice (en29.c:1304)");
                w->sv->player_voice(1027);
                p->flg |= 0x210004u; p->flg &= ~0x40000u;
                p->mode0 = 5; p->mode1 = 0; p->mode2 = 0; p->mode3 = 0;
                fw->ply_act.p_act_flg = 0;
                fw->p_status = 1;
                fw->p_br_mde0 = dir_mde; fw->p_br_mde1 = 0;
                return 1;
            }
        }
    }
    return 0;
}

/* en29.c:1330 bhEne29_PlyMoveMain */
static void ply_move_main(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    int32_t* stsP = &fw->p_status;
    RECVX_ENEMY_PLAYER p;
    int act = ACT_ALL, lop = 0;

    if (!(*stsP & 0x1)) return;
    if (!player(w, &p)) return;
    do {
        switch (fw->p_br_mde0) {
        case BR0_NORMAL: act = (fw->p_br_mde1 != BR1_FIRST) ? act : ACT_000; break;
        case BR0_ATTACK: act = (fw->p_br_mde1 != BR1_FIRST) ? act : ACT_001; break;
        case BR0_DAMAGE:
            p.pos.x += fw->ply_dmg.x;                                /* :1359 */
            p.pos.z += fw->ply_dmg.z;
            fw->ply_dmg.x *= 0.8f;
            fw->ply_dmg.z *= 0.8f;
            if (fabsf(fw->ply_dmg.x + fw->ply_dmg.z) < 0.01f) *stsP &= ~0x1;
            break;
        }
    } while (lop-- != 0);
    if (act != ACT_ALL) ply_action_change(w, &p, fw, act);
    ply_action_main(w, &p, fw);
    put_player(w, &p);
}

/* en29.c:1384 bhEne29_PlyActionInit (ene_mnwP/p_paramP nativ der Work). */
static void ply_action_init(RECVX_EN29_PAW* paw, const RECVX_EN19_ACT* tbl, int n)
{
    paw->act_tblP = tbl;
    paw->tbl_num = n;
    paw->p_act_now = -1;
}

/* en29.c:1397 bhEne29_PlyActionMain */
static void ply_action_main(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN29_STATE* fw)
{
    RECVX_EN29_PAW* paw = &fw->ply_act;
    if ((paw->p_act_flg & 0x7) == 2) {
        paw->p_act_flg &= ~0x2;
        ply_action_change(w, p, fw, paw->p_act_jmp);
    }
    if (paw->p_frm_num - 1 == 0) { gap(w, "bhEne29_PlyActionMain: p_frm_num - 1 == 0 (en29.c:1406)"); return; }
    paw->p_mtn_rte = p->frm_no / (paw->p_frm_num - 1);              /* :1406 */
    if (paw->p_prgP != NULL) paw->p_prgP(p, w, fw);
    paw->p_act_frm = p->frm_no / 65536;
    if (paw->p_act_frm == paw->p_chg_frm) paw->p_act_flg &= ~0x1;
}

/* en29.c:1422 bhEne29_PlyActionChange */
static int ply_action_change(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN29_STATE* fw, int act_no)
{
    RECVX_EN29_PAW* paw = &fw->ply_act;
    const RECVX_EN19_ACT* paP;
    uint32_t n;

    if (act_no >= paw->tbl_num) { gap(w, "act_tblP[act_no] ausserhalb (en29.c:1426)"); return 0; }
    paP = act_no >= 0 ? &paw->act_tblP[act_no] : NULL;
    if (!(paw->p_act_flg & 0x7) && paw->p_act_now != act_no) {
        paw->p_prgP = (paP && paP->prg >= 0 && paP->prg < 2) ? en29_ply[paP->prg] : NULL;
        paw->p_act_now = act_no;
        paw->p_act_flg = 1;
        if (act_no >= 0) {
            /* :1437 plP->mnwP = pawP->ene_mnwP (= ewP->mnwP) */
            p->mtn_enemy = 1; p->mtn_enemy_owner = w->id; p->mtn_enemy_work = w;
            if (en29_sv && en29_sv->player_library) en29_sv->player_library(w, 1u);
            else { gap(w, "plp->mnwP = ewP->mnwP (en29.c:1437)"); return 0; }
            p->mtn_no = (uint32_t)paP->mtn_no;
            p->frm_no = (int32_t)paP->frm_no * 65536;
            p->hokan_count = paP->hkn_cnt;
            p->hokan_rate = (int32_t)((double)paP->hkn_lvl * (65536.0 / 255.0));
            p->mtn_add = 65536;
            p->mtn_md = (uint16_t)paP->flag;
            BASEI(w, player_motion_frames, "plp->mnwP[mtn_no].frm_num (en29.c:1448)");
            n = w->sv->player_motion_frames(p->mtn_no, 1);
            if (!n) { gap(w, "plp->mnwP[mtn_no] fehlt in der Tentakel-Bibliothek (en29.c:1448)"); return 0; }
            paw->p_frm_num = (int32_t)n;
            paw->p_act_frm = paP->frm_no;
            paw->p_act_jmp = paP->act_jmp;
            if (paw->p_act_jmp != -1) paw->p_act_flg |= 0x2;
            if (paP->chg_frm != 0xFF) paw->p_chg_frm = paP->chg_frm;
            else paw->p_chg_frm = paw->p_frm_num - 1;
        }
        return 1;
    }
    return 0;
}

/* en29.c:1475 bhEne29_PlyDmg117 */
static void ply_dmg117(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    int dlt;
    p->pos.x += fw->ply_dmg.x;
    p->pos.z += fw->ply_dmg.z;
    fw->ply_dmg.x *= 0.9f;
    fw->ply_dmg.z *= 0.9f;
    if (fw->ply_act.p_mtn_rte <= 49152) {
        dlt = fw->dmg_dir - p->ay;
        if (njCos((Angle)dlt) < 0) dlt += 32768;
        p->ay += (int16_t)dlt / 32;                                  /* :1494 (short)dlt / 32 */
    }
    if (fw->ply_act.p_mtn_rte >= 65536) ply_dmg_rtn(p, w, fw);
}
/* en29.c:1504 bhEne29_PlyDmg118 */
static void ply_dmg118(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw) { ply_dmg117(p, w, fw); }
/* en29.c:1510 bhEne29_PlyDmgRtn */
static void ply_dmg_rtn(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    p->mtn_enemy = 0; p->mtn_enemy_work = NULL;
    if (en29_sv && en29_sv->player_library) en29_sv->player_library(w, 0u);
    else gap(w, "plp->mnwP = plp->mnwPb (en29.c:1512)");
    p->flg &= ~0x10004u;
    p->stflg &= ~0x10000u;
    p->mode0 = 1; p->mode1 = 0; p->mode2 = 0; p->mode3 = 0;
    fw->p_status &= ~0x1;
}

/* en29.c:1526 bhEne29_SetDmgEffect */
static void set_dmg_effect(RECVX_ENEMY_WORK* w, int eff_typ)
{
    const int32_t* effP;
    RECVX_EN19_EF ef;
    float r;

    if (eff_typ < 0 || eff_typ >= 9) { gap(w, "E29EffTbl[eff_typ] ausserhalb (en29.c:1531)"); return; }
    effP = en29_eff_tbl[eff_typ];
    switch (effP[0]) {
    case 0: case 1: {                                                /* :1535/1546 rySetEffBlood2 / rySetEffBlood */
        RECVX_ENEMY_VEC pos = { w->dpx, w->dpy, w->dpz }, dir = { -w->dvx, -w->dvy, -w->dvz };
        RECVX_T3_LINK lk = { w, 0 };
        if (effP[0] == 0) {
            if (en29_sv && en29_sv->blood2) en29_sv->blood2(lk, w->djnt_no, &pos, &dir, effP[1]);
            else gap(w, "rySetEffBlood2 bhEff311 (en29.c:1543)");
        } else {
            if (en29_sv && en29_sv->blood) en29_sv->blood(lk, w->djnt_no, &pos, &dir, effP[1]);
            else gap(w, "rySetEffBlood bhEff309 (en29.c:1554)");
        }
        break;
    }
    case 2: {                                                        /* :1557 Splitter an djnt_no-1..+1 */
        NJS_VECTOR vct = { w->dvx, w->dvy, w->dvz }, dir;
        NJS_POINT3 jp, off;
        int i, djnt_no;
        njUnitVector(&vct);
        ef.flg = 1;
        ef.id = (uint32_t)effP[3];
        ef.ax = (int32_t)(10430.381f * asinf(vct.y));
        ef.ay = (int32_t)(10430.381f * atan2f(-vct.x, -vct.z));
        ef.sx = ef.sy = ef.sz = 1.0f;
        ef.mdlver = (uint32_t)effP[1];
        ef.type = 0;
        if (!jpos(w, (uint32_t)w->djnt_no, &jp)) return;
        ef.pos = vec_of(&jp);
        djnt_no = w->djnt_no;
        dir.x = -w->dvx; dir.y = -w->dvy; dir.z = -w->dvz;           /* :1586 (Richtung wird nicht weiter gelesen) */
        njUnitVector(&dir);
        const uint32_t mask = RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AX | RECVX_EN19_EF_AY |
                              RECVX_EN19_EF_SX | RECVX_EN19_EF_SY | RECVX_EN19_EF_SZ | RECVX_EN19_EF_MDLVER |
                              RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS;
        for (i = effP[2]; i > 0; i--) {
            if (djnt_no != 1) {
                if (!rnd(w, &r)) return; off.x = 3.0f * (r - 0.5f);
                if (!rnd(w, &r)) return; off.y = 3.0f * (r - 0.5f);
                if (!rnd(w, &r)) return; off.z = 3.0f * r;
                effect_tb(w, mask, &ef, &off, w, djnt_no - 1);
            }
            if (!rnd(w, &r)) return; off.x = 3.0f * (r - 0.5f);
            if (!rnd(w, &r)) return; off.y = 3.0f * (r - 0.5f);
            if (!rnd(w, &r)) return; off.z = 2.0f * r;
            effect_tb(w, mask, &ef, &off, w, djnt_no);
            if (!rnd(w, &r)) return; off.x = 3.0f * (r - 0.5f);
            if (!rnd(w, &r)) return; off.y = 3.0f * (r - 0.5f);
            if (!rnd(w, &r)) return; off.z = 1.0f + 2.0f * r;
            effect_tb(w, mask, &ef, &off, w, djnt_no);
            if (djnt_no != 19) {
                if (!rnd(w, &r)) return; off.x = 3.0f * (r - 0.5f);
                if (!rnd(w, &r)) return; off.y = 3.0f * (r - 0.5f);
                if (!rnd(w, &r)) return; off.z = 3.0f * r;
                effect_tb(w, mask, &ef, &off, w, djnt_no + 1);
            }
        }
        break;
    }
    case 3: {                                                        /* :1627 Gelenk-Z-Achse, ohne Link */
        NJS_MATRIX m;
        float px, py, pz;
        int i;
        if (!joint(w, (uint32_t)w->djnt_no, &m)) return;
        px = -m[8]; py = -m[9]; pz = -m[10];                         /* :1632 mtx[8..10] */
        ef.flg = 1;
        ef.id = (uint32_t)effP[3];
        ef.ax = (int32_t)(10430.381f * asinf(py));
        ef.ay = (int32_t)(10430.381f * atan2f(-px, -pz));
        ef.mdlver = (uint32_t)effP[1];
        ef.sx = ef.sy = ef.sz = 1.0f;
        ef.type = 6;
        ef.pos.x = w->dpx; ef.pos.y = w->dpy; ef.pos.z = w->dpz;
        for (i = effP[2]; i > 0; i--)
            effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AX | RECVX_EN19_EF_AY | RECVX_EN19_EF_MDLVER |
                         RECVX_EN19_EF_SX | RECVX_EN19_EF_SY | RECVX_EN19_EF_SZ | RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS,
                      &ef, NULL, NULL, 0);
        break;
    }
    case 4: {                                                        /* :1660 skalierter Satz am Trefferpunkt */
        NJS_MATRIX inv;
        NJS_POINT3 dp = { w->dpx, w->dpy, w->dpz }, off;
        float scl = 0.000015258789f * (float)effP[1];
        if (!joint(w, (uint32_t)w->djnt_no, &inv)) return;
        njInvertMatrix(&inv);
        njCalcPoint(&inv, &dp, &off);
        ef.flg = 1;
        ef.id = (uint32_t)effP[3];
        ef.ax = ef.ay = 0;
        ef.mdlver = 0;
        ef.sx = ef.sy = ef.sz = scl;
        ef.type = (uint32_t)effP[2];
        ef.pos.x = ef.pos.y = ef.pos.z = 0;
        effect_tb(w, RECVX_EN19_EF_FLG | RECVX_EN19_EF_ID | RECVX_EN19_EF_AX | RECVX_EN19_EF_AY | RECVX_EN19_EF_MDLVER |
                     RECVX_EN19_EF_SX | RECVX_EN19_EF_SY | RECVX_EN19_EF_SZ | RECVX_EN19_EF_TYPE | RECVX_EN19_EF_POS,
                  &ef, &off, w, w->djnt_no);
        break;
    }
    }
}

/* en29.c:1697 SetDmgEne: Ring aus Kopf DmgWrk[0] und den Kapselwerken; jedes neue
   Glied wird hinter den Kopf gehaengt (Durchlaufreihenfolge also rueckwaerts). */
static void set_dmg_ene(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw, const EN29_DS* ds, int n)
{
    RECVX_EN29_DMG* hedP = &fw->DmgWrk[0];
    RECVX_EN29_DMG* edP = hedP + 1;
    fw->dmg_eneP = hedP;
    hedP->nextP = hedP;
    for (; n > 0; n--, ds++, edP++) {
        RECVX_ENEMY_WORK* c;
        if (!en29_sv || !en29_sv->create_child) { gap(w, "eneset.c:170 Kindwerk-Dienst (en29.c:1713)"); return; }
        c = en29_sv->create_child(w, &en29_dmg_dat);
        /* :1715 ewP->flg2 ohne Pruefung: bei voller Gegnertabelle schreibt das Original durch NULL. */
        if (!c) { gap(w, "eneset.c:170 Kindwerk: Gegnertabelle voll (en29.c:1715)"); return; }
        c->flg2 |= (uint32_t)ds->or_flg2;
        edP->atr_top = ds->atr_top;
        edP->atr_end = ds->atr_end;
        edP->atr_rad = ds->atr_rad;
        edP->ewP = c;
        edP->nextP = hedP->nextP;
        hedP->nextP = edP;
    }
}

/* en29.c:1729 CalcDmgEne */
static void calc_dmg_ene(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    RECVX_EN29_DMG* hedP = fw->dmg_eneP;
    RECVX_EN29_DMG* edP;
    NJS_POINT3 a, b;
    if (!hedP) { gap(w, "fwP->dmg_eneP NULL (en29.c:1735)"); return; }
    for (edP = hedP->nextP; edP != hedP; edP = edP->nextP) {
        RECVX_ENEMY_WORK* enP = edP->ewP;
        enP->watr.r = edP->atr_rad;
        if (!jpos(w, (uint32_t)edP->atr_top, &a) || !jpos(w, (uint32_t)edP->atr_end, &b)) return;
        enP->watr.c1 = vec_of(&a);
        enP->watr.c2 = vec_of(&b);
        enP->cpcl = w->cpcl;
        if (!en29_sv || !en29_sv->share_model || !en29_sv->share_model(enP, w)) gap(w, "enP->mlwP = ewP->mlwP (en29.c:1750)");
    }
}

/* en29.c:1755 CheckDmgEne: der zuletzt durchlaufene getroffene Kapselwerk gibt
   seinen Treffer an den Koerper weiter. */
static void check_dmg_ene(RECVX_ENEMY_WORK* w, RECVX_EN29_STATE* fw)
{
    RECVX_EN29_DMG* hedP = fw->dmg_eneP;
    RECVX_EN29_DMG* edP;
    RECVX_ENEMY_WORK* dmgP = NULL;
    RECVX_ENEMY_PLAYER p;
    int i;

    if (!hedP) { gap(w, "fwP->dmg_eneP NULL (en29.c:1764)"); return; }
    for (edP = hedP->nextP; edP != hedP; edP = edP->nextP) {
        RECVX_ENEMY_WORK* enP = edP->ewP;
        if (enP->flg & 0x4u) {
            float dx, dy, dz;
            enP->flg &= ~0x4u;
            if (!player(w, &p)) return;
            dx = enP->dpx - p.pos.x; dy = enP->dpy - p.pos.y; dz = enP->dpz - p.pos.z;   /* njDistanceP2P */
            if (10000.0f > njSqrt(dx * dx + dy * dy + dz * dz)) dmgP = enP;
        }
    }
    if (dmgP != NULL && !(w->flg & 0x4u)) {                          /* :1784 */
        w->flg |= 0x4u;
        w->djnt_no = dmgP->djnt_no;
        for (i = 0; i < (int)RECVX_ENEMY_DAM; i++) w->dam[i] = dmgP->dam[i];
        w->dpx = dmgP->dpx; w->dpy = dmgP->dpy; w->dpz = dmgP->dpz;
        w->dvx = dmgP->dvx; w->dvy = dmgP->dvy; w->dvz = dmgP->dvz;
        w->dax = dmgP->dax; w->day = dmgP->day;
        w->wpnr_no = dmgP->wpnr_no;
        w->comb_flg |= dmgP->comb_flg & 0x70u;
    }
}
