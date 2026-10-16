/* EN15 Nosferatu — eigenstaendige native Zustandsmaschine auf dem einzigen
   WorldEntity-Work. Semantikreferenzen (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en15.c:265 target_direction, :281 target_distance,
   :295 GetLocalEneNo, :315 SetMtnSE, :362 bhEne15, :423 Init, :519 Move,
   :540 Stand, :595 __attack, :810 CloseTurn, :854 Chase, :872-901 _goalAng,
   :904 KeepFar, :940 Attack, :1240 Throw, :1425 Damage, :1454 Die,
   :1499 NearestCapsule, :1564 CheckDamage, :1857 GetRelay, :1900 SetMtn,
   :1963 ReqMtn, :1969 SetPlyMtn, :1998 VacumeToPoint, :2021 LockLeg,
   :2062/2117/2172 bhEne15_AttackPlayerCC/BC/SS, :2199 SetSmoke,
   :2329 SpecialAttack, :2360 _bhEne_SetPoison, :2479 bhEne_SetPoison,
   :2836 bhEne15_RotChar, :2865 AbleToFall, :2879-3347 Ply_func.
   Die Effektprozedur bhEff_E15_Poison (:2616) mit PoisonAttack/AddWindForce/
   _bhEne_SetPoison2 liegt beim Effektbesitzer (effect_sprite_procedures.cpp).
   Ungebundene Dienste melden den Originalnamen und erfinden keinen Wert. */
#include "en15.h"
#include "en03_lib.h"
#include "ninja_collision.h"
#include "combat_geometry.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const RECVX_EN15_SERVICES* en15_sv;
void recvx_en15_bind(const RECVX_EN15_SERVICES* services) { en15_sv = services; }

/* en15.c:24/25 — modulweit, nicht je Work. */
static signed char poison_attack_wait;
static signed char poison_eff_wait;
signed char* recvx_en15_poison_attack_wait(void) { return &poison_attack_wait; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
static RECVX_EN15_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN15_STATE*)w->exp0; }

/* --- Reine Daten (en15.c:27-241) ------------------------------------------- */
typedef struct { int8_t dmg_type, blood_type, mince_type; } EN15_WPNDG;
static const EN15_WPNDG WpnDamageTbl[21] = {
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 2, 2 }, { 2, 2, 2 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 2, 1, 1 }, { 0, 0, 0 }, { 2, 1, 2 }, { 0, 1, 1 }, { 2, 0, 1 },
    { 2, 0, 0 }, { 2, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { 2, 2, 2 }, { 0, 0, 0 }, { 2, 2, 2 }
};
static const RECVX_T3_COMBWEP CombWepTbl[21] = {
    {   0, {  0, 0, 0 },  0, 0 }, {   0, {  0, 0, 0 },  0, 0 }, {  40, { 10, 0, 0 }, 30, 0 },
    {  70, { 10, 8, 0 }, 60, 0 }, {  70, { 10, 8, 0 }, 60, 0 }, {  40, { 10, 8, 0 }, 30, 0 },
    {   0, {  0, 0, 0 },  0, 0 }, {  70, { 10, 8, 0 }, 30, 0 }, { 160, { 10, 8, 0 }, 40, 0 },
    {   0, {  0, 0, 0 },  0, 0 }, {  80, { 10, 8, 0 }, 70, 0 }, {   0, {  0, 0, 0 },  0, 0 },
    {  60, { 10, 8, 0 }, 60, 0 }, {   0, {  0, 0, 0 },  0, 0 }, {   0, {  0, 0, 0 },  0, 0 },
    {   0, {  0, 0, 0 },  0, 0 }, {   0, {  0, 0, 0 },  0, 0 }, {   0, {  0, 0, 0 },  0, 0 },
    {   0, {  0, 0, 0 },  0, 0 }, {   0, {  0, 0, 0 },  0, 0 }, {   0, {  0, 0, 0 },  0, 0 }
};
static const RECVX_T3_COMBJOINT CombJointTbl[24] = { { 0, 0 } };
typedef struct { int8_t dmg_type[3]; int8_t blood_type[3]; } EN15_COMBO;
static const EN15_COMBO Combo_Eff[21] = {
    { { 0, 0, 0 }, { -1, -1, -1 } }, { { 0, 0, 0 }, { -1, -1, -1 } }, { { 1, 0, 0 }, { 1, 0, -1 } },
    { { 1, 0, 0 }, { 1, 0, -1 } },   { { 1, 0, 0 }, { 1, 0, -1 } },   { { 1, 0, 0 }, { 1, 0, -1 } },
    { { 0, 0, 0 }, { -1, -1, -1 } }, { { 1, 0, 0 }, { 1, 0, -1 } },   { { 1, 0, 0 }, { 1, 0, -1 } },
    { { 0, 0, 0 }, { -1, -1, -1 } }, { { 1, 0, 0 }, { 1, 0, -1 } },   { { 0, 0, 0 }, { -1, -1, -1 } },
    { { 1, 0, 0 }, { 1, 0, -1 } },   { { 0, 0, 0 }, { -1, -1, -1 } }, { { 0, 0, 0 }, { -1, -1, -1 } },
    { { 0, 0, 0 }, { -1, -1, -1 } }, { { 0, 0, 0 }, { -1, -1, -1 } }, { { 0, 0, 0 }, { -1, -1, -1 } },
    { { 0, 0, 0 }, { -1, -1, -1 } }, { { 0, 0, 0 }, { -1, -1, -1 } }, { { 0, 0, 0 }, { -1, -1, -1 } }
};
static const int8_t SdwTab[3] = { 20, 23, -1 };
static const RECVX_T3_BT prt_blood_tbl[24] = {
    {  0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, {  1, 0.0f, 2.0f, 1.8f, 2.0f, 5.0f, 1.0f, 5.0f },
    {  2, 0.0f, 2.0f, 1.8f, 2.0f, 5.0f, 1.0f, 5.0f }, {  3, 0.0f, 2.0f, 1.8f, 2.0f, 5.0f, 1.0f, 5.0f },
    {  4, 0.0f, 2.0f, 1.8f, 2.0f, 3.0f, 1.0f, 3.0f }, {  5, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f },
    {  6, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f }, {  7, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 3.0f },
    {  8, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f }, {  9, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f },
    { 10, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 3.0f }, { 11, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f },
    { 12, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f }, { 13, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 3.0f },
    { 14, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f }, { 15, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 4.0f },
    { 16, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 3.0f }, { 17, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 1.0f, 3.0f },
    { 18, 0.0f, 0.0f, 1.8f, 2.0f, 4.0f, 1.0f, 4.0f }, { 19, 0.0f, 2.0f, 1.0f, 1.0f, 4.0f, 1.0f, 4.0f },
    { 20, 0.0f, 2.0f, 1.0f, 1.0f, 3.0f, 1.0f, 3.0f }, { 21, 0.0f, 0.0f, 1.8f, 2.0f, 4.0f, 1.0f, 4.0f },
    { 22, 0.0f, 2.0f, 1.0f, 1.0f, 4.0f, 1.0f, 4.0f }, { 23, 0.0f, 2.0f, 1.0f, 1.0f, 3.0f, 1.0f, 3.0f }
};
static const int8_t rfoot_joint_tree[6] = { 0, 1, 18, 19, 20, -1 };
static const int8_t lfoot_joint_tree[6] = { 0, 1, 21, 22, 23, -1 };
typedef struct { int8_t start, end; } EN15_LEGLOCK;
static const EN15_LEGLOCK lrl_walk[2]  = { { 40, 74 }, { -1, -1 } };
static const EN15_LEGLOCK lrl_a1[2]    = { { 64, 93 }, { -1, -1 } };
static const EN15_LEGLOCK lrl_a2[2]    = { { 25, 47 }, { -1, -1 } };
static const EN15_LEGLOCK lrl_fldmg[2] = { { 18, 58 }, { -1, -1 } };
static const EN15_LEGLOCK lrl_bldmg[2] = { { 27, 92 }, { -1, -1 } };
/* char 118 als end: (char)118 bleibt positiv. */
static const EN15_LEGLOCK lrl_crdmg[2] = { { 25, 118 }, { -1, -1 } };
static const EN15_LEGLOCK lrl_dummy[1] = { { -1, -1 } };
typedef struct { int8_t mtn_no, default_lr; const EN15_LEGLOCK* list; } EN15_LEGTAB;
static const EN15_LEGTAB leglock_tab[11] = {
    {  1,  1, lrl_walk  }, {  2,  1, lrl_a1    }, {  3,  0, lrl_a2    }, {  6,  0, lrl_dummy },
    {  7,  1, lrl_dummy }, {  8,  1, lrl_fldmg }, {  9,  1, lrl_bldmg }, { 10,  0, lrl_crdmg },
    { 12,  0, lrl_dummy }, { 13,  1, lrl_dummy }, { -1, -1, NULL      }
};
static const int attack1_col_joint[5] = { 7, 8, 9, 10, -1 };
static const int attack2_col_joint[4] = { 8, 9, 10, -1 };
static const int attack3_col_joint[6] = { 6, 7, 8, 9, 10, -1 };
typedef struct {
    const int* obj_no; int start_frm, end_frm, damage, impact_ang;
    float spd, volume; int sp_start_frm, sp_end_frm;
} EN15_ATTACK;
static const EN15_ATTACK attack_col_tab[3] = {
    { attack1_col_joint, 31, 56, 50, 8192, 6.05f, 4.3f, 41, 52 },
    { attack2_col_joint, 29, 40, 50, 5461,  4.0f, 3.3f,  0,  0 },
    { attack3_col_joint, 24, 32, 30, 7281,  4.5f, 5.0f, 24, 30 }
};
static const RECVX_ENEMY_CPCL CapColTab[23] = {
    {   3,   3,   4 }, {   0,  12,  -8 }, {   4,   4,  12 }, {   0,   9,  -2 }, {   3,   3,  10 },
    {  18,  14,   6 }, {   3,   3,  10 }, {  18,   5,   7 }, {   3,   3,  10 }, {  18,  -4,   8 },
    {   3,   3,  10 }, { -18,  14,   6 }, {   3,   3,  10 }, { -18,   5,   7 }, {   3,   3,  10 },
    { -18,  -4,   8 }, {   4,   3,  12 }, {   3,   2,  15 }, {  18,  19,  10 }, {  19,  20,  10 },
    {  21,  22,  10 }, {  22,  23,  10 }, {   0,   0,   0 }
};
static const uint8_t flip_tree[24] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
};
typedef struct { uint32_t mtn_no, frm; int seno; } EN15_MTNSE;
static const EN15_MTNSE mtn_se_tbl[30] = {
    {  1,  38,    74496 }, {  1,  74,    74496 }, {  2,  33,    74498 }, {  2,  11,    74496 },
    {  2,  57,    74496 }, {  2,  96,    74496 }, {  3,  32,    74500 }, {  3,  15,    74496 },
    {  3,  47,    74496 }, {  3,  66,    74496 }, {  4,  24,    74498 }, {  4,  20,    74496 },
    {  4,  20,    74496 }, {  6,   1, 16851722 }, {  7,   1, 16851722 }, {  8,   9, 16851723 },
    {  8,  12,    74496 }, {  8,  47,    74496 }, {  9,  13, 16851723 }, {  9,  26,    74496 },
    {  9,  58,    74496 }, { 10,  33, 16851724 }, { 10,  26,    74496 }, { 10,  49,    74496 },
    { 10,  92,    74496 }, { 11,   8, 16851725 }, { 11,  91,     8974 }, { 11, 150,     8974 },
    { 12,  21,    74496 }, { (uint32_t)-1, 0,  0 }
};
typedef struct { int16_t old_mtn_no, next_mtn_no, from, to; } EN15_RELAY;
static const EN15_RELAY mtn_relay[21] = {
    {  1,  2, 55,  0 }, {  1,  3, 55,  0 }, {  1,  4, 55,  0 }, {  2,  1, -1,  0 },
    {  3,  1, -1,  0 }, {  4,  1, -1, 23 }, {  1,  5, 55,  0 }, {  1,  6, 55,  0 },
    {  1,  7, 29,  0 }, {  6,  1, -1, 31 }, {  7,  1, -1, 38 }, {  8,  1, -1, 23 },
    {  9,  1, -1,  0 }, { 10,  1, -1, 64 }, {  1, 12, 55,  0 }, { 12,  0, -1,  0 },
    {  0, 13,  1,  0 }, { 13,  1, -1, 23 }, {  5,  0, -1,  0 }, {  5,  1,  0, 56 },
    { -1,  0,  0,  0 }
};
typedef struct { int16_t old_mtn_no, next_mtn_no; const EN15_RELAY* relay; } EN15_RELAY2;
static const EN15_RELAY2 mtn_relay_relay[4] = {
    {  1,  0, &mtn_relay[14] }, {  0,  1, &mtn_relay[16] }, {  5,  1, &mtn_relay[18] },
    { -1,  0, NULL }
};
/* en15.c:2319 eff_info[type].texid (UV-Tabellen liegen beim Effektbesitzer). */
static const int32_t eff_info_texid[5] = { 7, 6, 6, 7, 6 };

/* MTN_NO_CHECK (macros.h:87). */
static int mtn_check(const RECVX_ENEMY_WORK* w) { return (w->mtn_no == 2) ? 0 : (w->mtn_no == 3) ? 1 : 2; }

/* --- Grundzugriffe --------------------------------------------------------- */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en15)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static int rnd_mod(RECVX_ENEMY_WORK* w, int n, int* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en15)"); return 0; }
    *out = (int)((uint32_t)w->sv->random_next() % (uint32_t)n);
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en15)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_set) { gap(w, "plp schreiben (en15)"); return; }
    w->sv->player_set(p);
}
static int dyn_get(RECVX_ENEMY_WORK* w, RECVX_EN15_PLAYER_DYN* d)
{
    if (!en15_sv || !en15_sv->player_dyn_get || !en15_sv->player_dyn_get(d)) {
        gap(w, "plp->dvx/dax/day/spd/pxb (en15)"); return 0;
    }
    return 1;
}
static void dyn_set(RECVX_ENEMY_WORK* w, const RECVX_EN15_PLAYER_DYN* d)
{
    if (!en15_sv || !en15_sv->player_dyn_set) { gap(w, "plp->dvx/dax/day/spd schreiben (en15)"); return; }
    en15_sv->player_dyn_set(d);
}
static uint32_t frames(RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    uint32_t n;
    if (!w->sv || !w->sv->motion_frames) { gap(w, "epw->mnwP[mtn_no].frm_num (en15)"); return 0; }
    n = w->sv->motion_frames(w, mtn);
    if (!n) gap(w, "epw->mnwP[mtn_no] fehlt (en15)");
    return n;
}
static uint32_t player_frames(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    uint32_t n;
    if (!w->sv || !w->sv->player_motion_frames) { gap(w, "plp->mnwP[mtn_no].frm_num (en15)"); return 0; }
    n = w->sv->player_motion_frames(p->mtn_no, p->mtn_enemy ? 1 : 0);
    if (!n) gap(w, "plp->mnwP[plp->mtn_no] fehlt (en15)");
    return n;
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    if (!w->sv || !w->sv->joint_matrix || !w->sv->joint_matrix(w, j, (float*)m)) {
        gap(w, "epw->mlwP->owP[n].mtx (en15)"); return 0;
    }
    return 1;
}
static int joint_pos(RECVX_ENEMY_WORK* w, uint32_t j, RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m;
    if (!joint(w, j, &m)) return 0;
    out->x = m[12]; out->y = m[13]; out->z = m[14];
    return 1;
}
/* njCalcPoint(&owP[j].mtx, local, out). */
static int joint_point(RECVX_ENEMY_WORK* w, uint32_t j, float lx, float ly, float lz, RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m;
    NJS_POINT3 a, b;
    if (!joint(w, j, &m)) return 0;
    a.x = lx; a.y = ly; a.z = lz;
    njCalcPoint(&m, &a, &b);
    out->x = b.x; out->y = b.y; out->z = b.z;
    return 1;
}
static int player_joint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    if (!x || !x->player_joint_matrix || !x->player_joint_matrix(j, (float*)m)) {
        gap(w, "plp->mlwP->owP[n].mtx (en15)"); return 0;
    }
    return 1;
}
static void enemy_se(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int se)
{
    /* RequestEnemySe(GetLocalEneNo(epw), pos, se): GetLocalEneNo (en15.c:295)
       ist der Platz von epw in ene[], also idx_ct (eneset.c:268). */
    if (!w->sv || !w->sv->enemy_se) { gap(w, "RequestEnemySe (en15)"); return; }
    w->sv->enemy_se((uint32_t)w->idx_ct, pos, se);
}
static void vibration(RECVX_ENEMY_WORK* w, int motor, int power)
{
    if (!w->sv || !w->sv->vibration) { gap(w, "StartVibrationEx (en15)"); return; }
    w->sv->vibration(motor, power);
}
static void player_voice(RECVX_ENEMY_WORK* w, int se)
{
    /* sdfunc.c CallPlayerVoice ohne Work-Gate (nicht bhEne_CallPlayerVoice). */
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    if (!x || !x->player_voice_free) { gap(w, "CallPlayerVoice (en15)"); return; }
    x->player_voice_free(se);
}
static int set_motion(RECVX_ENEMY_WORK* w, int add)
{
    if (!w->sv || !w->sv->set_motion) { gap(w, "bhSetMotion (en15)"); return 0; }
    return w->sv->set_motion(w, add, w->mtn_md);
}
static uint32_t sys_flags(RECVX_ENEMY_WORK* w, uint32_t which)
{
    if (!en15_sv || !en15_sv->sys_flags) { gap(w, "sys->gm_flg/st_flg/pt_flg/ts_flg (en15)"); return 0; }
    return en15_sv->sys_flags(which);
}
static void sys_flags_set(RECVX_ENEMY_WORK* w, uint32_t which, uint32_t set, uint32_t clear)
{
    if (!en15_sv || !en15_sv->sys_flags_set) { gap(w, "sys->gm_flg/st_flg/pt_flg/ts_flg schreiben (en15)"); return; }
    en15_sv->sys_flags_set(which, set, clear);
}
static void ev_flag(RECVX_ENEMY_WORK* w, uint32_t no, int on)
{
    if (!en15_sv || !en15_sv->event_flag_set) { gap(w, on ? "bhStFlg(sys->ev_flg) (en15)" : "bhCrFlg(sys->ev_flg) (en15)"); return; }
    en15_sv->event_flag_set(no, on);
}
/* NJM_RAD_ANG(atan2f(y, x)) = njArcTan2 (macros.h:102/109), PI_2 = 3.141592f*2. */
static int32_t nj_arc_tan2(float y, float x) { return (int32_t)(atan2f(y, x) * (65536.0f / (3.141592f * 2.0f))); }
static float dist3(const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b)
{
    const float dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
    return njSqrt(dx * dx + dy * dy + dz * dz);
}
static float scalor(const RECVX_ENEMY_VEC* v)
{
    NJS_VECTOR u;
    u.x = v->x; u.y = v->y; u.z = v->z;
    return njScalor(&u);
}

/* sys->gm_flg 0x40-Abschluss (en15.c:1477-1493 / :1778-1796, gleiches Muster in
   player.c bhCPM0_nage): Kampfmusik/Bosszustand beenden. */
static void end_boss_flags(RECVX_ENEMY_WORK* w)
{
    uint32_t gm = sys_flags(w, 0), stf;
    if (!(gm & 0x40u)) return;
    sys_flags_set(w, 0, 0, 0x40u);
    gm = sys_flags(w, 0);
    if (!(gm & 0x1000000u)) sys_flags_set(w, 0, 0, 0x80u);
    sys_flags_set(w, 0, 0x800u, 0);
    stf = sys_flags(w, 1);
    if (stf & 0x800000u) {
        sys_flags_set(w, 1, 0, 0x800000u);
        sys_flags_set(w, 0, 0, 0x80000u);
        sys_flags_set(w, 2, 0x1u, 0);
    }
}

/* --- en15.c:265-292 -------------------------------------------------------- */
static int target_direction(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    float ans = 0.005493164f * (float)(recvx_t3_niten_dir(w->px, w->pz, p->pos.x, p->pos.z) - w->ay);
    if (ans > 180.0f) ans = ans + -360.0f;
    else if (ans <= -180.0f) ans = ans + 360.0f;
    return (int)(182.04445f * ans);
}
static int target_distance(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p, float* out)
{
    RECVX_ENEMY_VEC e;
    if (!joint_pos(w, 1, &e)) return 0;
    e.y = 0.0f;
    *out = dist3(&p->pos, &e);
    return 1;
}

/* en15.c:315 SetMtnSE. */
static void set_mtn_se(RECVX_ENEMY_WORK* w)
{
    int i;
    for (i = 0; mtn_se_tbl[i].mtn_no != (uint32_t)-1; i++) {
        if (w->mtn_no == mtn_se_tbl[i].mtn_no && (uint32_t)(w->frm_no / 65536) == mtn_se_tbl[i].frm) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            enemy_se(w, &at, mtn_se_tbl[i].seno);
            return;
        }
    }
}

/* en15.c:1963 ReqMtn. */
static void req_mtn(RECVX_ENEMY_WORK* w, int mtn) { st(w)->req_mtn = (int16_t)mtn; }
static void set_modes01(RECVX_ENEMY_WORK* w, uint8_t m0, uint8_t m1, int way, int mtn)
{
    w->mode0 = m0; w->mode1 = m1; w->way = (int16_t)way; req_mtn(w, mtn);
}

/* en15.c:2836 bhEne15_RotChar. */
void recvx_en15_rot_char(int32_t* ay, uint32_t flg, int goal, int add_ang)
{
    int rot;
    if (flg & 0x80u) return;
    if (add_ang & 0x80000000) {
        add_ang = -add_ang;
        goal = (uint16_t)(goal + 32768);
    }
    rot = (uint16_t)(add_ang + (goal - *ay));
    if (rot < (add_ang + add_ang)) { *ay = goal; return; }
    *ay = *ay - add_ang;
    if (rot <= 32768) *ay += (add_ang + add_ang);
}

/* zonzon.c:10 ikou am Spielerwork (en15.c:3241/3250). */
static void player_ikou(RECVX_ENEMY_PLAYER* p, const RECVX_ENEMY_VEC* pos, int add_dir)
{
    int ang, rot;
    if (p->flg & 0x80u) return;
    ang = recvx_t3_niten_dir(p->pos.x, p->pos.z, pos->x, pos->z);
    if (add_dir & 0x80000000) { add_dir = -add_dir; ang = (ang + 0x8000) & 0xffff; }
    rot = (add_dir + (ang - p->ay)) & 0xffff;
    if (rot < (add_dir + add_dir)) p->ay = ang;
    else { p->ay -= add_dir; if (rot <= 0x8000) p->ay += (add_dir + add_dir); }
}

/* en15.c:1969 SetPlyMtn. `p` ist die aktuelle Spielersicht des Aufrufers. */
static int set_ply_mtn(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, uint32_t mtn_no)
{
    int r;
    p->hokan_rate = 13107;
    p->hokan_count = 10;
    p->frm_no = 0;
    p->mtn_add = 65536;
    p->mtn_no = mtn_no;
    if (!en15_sv || !en15_sv->player_set_motion) { gap(w, "bhSetMotion(plp) (en15.c:1977)"); return 0; }
    r = en15_sv->player_set_motion(p, 0);
    if (r == -2) return 0;
    if (r != 0) p->flg |= 0x2000000u;
    else p->flg &= ~0x2000000u;
    if (p->mtn_no == 26) p->pos.y -= 10.6722f;
    if (p->mtn_no == 25) p->pos.y -= 10.7582f;
    return 1;
}
/* plp->mnwP = epw->mnwP / plp->mnwPb. */
static void ply_library(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, int enemy)
{
    if (enemy) { p->mtn_enemy = 1; p->mtn_enemy_owner = w->id; p->mtn_enemy_work = w; }
    else { p->mtn_enemy = 0; p->mtn_enemy_owner = 0; p->mtn_enemy_work = NULL; }
    if (!en15_sv || !en15_sv->player_library) { gap(w, enemy ? "plp->mnwP = epw->mnwP (en15)" : "plp->mnwP = plp->mnwPb (en15)"); return; }
    en15_sv->player_library(w, enemy ? 1u : 0u);
}
/* Wiederkehrender Uebergang der Griffzweige (en15.c:1142-1160 u. a.):
   mnwP auf den Gegner, Haltebit, epw->mode3, plp->spd, SetPlyMtn, plp->mode0..3
   und die Spielerbits. */
static void grab_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d,
                        int lib, uint8_t mode3, float spd, uint32_t mtn, uint8_t pmode0)
{
    if (lib) ply_library(w, p, 1);
    st(w)->hold |= 0x1;
    w->mode3 = mode3;
    d->spd = spd;
    dyn_set(w, d);
    set_ply_mtn(w, p, mtn);
    p->mode0 = pmode0; p->mode1 = 0; p->mode2 = 0; p->mode3 = 0;
    p->flg |= 0x10004u;
    p->flg &= ~0x40000u;
    p->stflg |= 0x50000u;
}

/* --- Init (en15.c:423) ----------------------------------------------------- */
static void en15_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN15_STATE* s;
    int i, gm = 0;
    if (!w->exp0) {
        if (!w->sv->calloc_work) { gap(w, "bhGetFreeMemory (en15)"); return; }
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN15_STATE));
        /* en15.c:430-434: das Original meldet den Fehlschlag und schreibt danach
           ungeprueft EXP0_S(0x5A) — ein NULL-Zugriff. */
        if (!w->exp0) { gap(w, "bhEne15_Init: Can't Get WorkMemory (Original schreibt danach nach NULL)"); return; }
    }
    s = st(w);
    s->req_mtn = -1;
    s->poison = 0;
    w->flg |= 0x178u;
    w->flg &= ~6u;
    w->mdflg &= ~4u;
    s->hold = 0;
    ev_flag(w, 57, 0);
    w->aox = w->aoy = w->aoz = 0.0f;
    w->ar = 3.8f; w->ah = 20.0f; w->aw = 0.0f; w->ad = 0.0f;
    w->car = 2.5f; w->cah = 20.0f;
    w->cpcl = CapColTab;
    for (i = 0; i < 64; i++) w->dam[i] = 0;
    if (!en15_sv || !en15_sv->game_mode || !en15_sv->game_mode(&gm)) { gap(w, "sys->gm_mode (en15)"); return; }
    w->hp = (gm == 2) ? 360 : 600;
    recvx_enemy_init_damage(w);
    w->ct1 = 0; w->ct2 = 0; w->ct0 = 0;
    /* mlwP->objP = mbp[0]; obj_a = obj_b = mbp[0]; mdflg &= ~2 (keine Morphzeichnung). */
    if (!en15_sv || !en15_sv->model_root || !en15_sv->model_root(w, 0)) { gap(w, "mlwP->objP = mbp[0] (en15)"); return; }
    if (!en15_sv->morph_pair || !en15_sv->morph_pair(w, 0, 0)) { gap(w, "obj_a/obj_b = mbp[0] (en15)"); return; }
    w->mdflg &= ~2u;
    w->shp_ct = 0.0f;
    if (!(w->flg & 0x800u)) {
        if (!w->sv->set_shadow) { gap(w, "bhSetShadow (en15)"); return; }
        if (w->sv->set_shadow(SdwTab, w, 1, 6.0f, 5.0f, 5.0f) < 0) gap(w, "bhEne15_Init: eff[] voll (Schatten)");
        w->flg |= 0x800u;
    }
    w->clp_jno[0] = 4; w->clp_jno[1] = 20; w->clp_jno[2] = 23; w->clp_jno[3] = -1;
    w->mdflg |= 0x20u;
    w->lok_jno = 4;
    w->mtn_md = 0;
    w->mtn_no = (uint32_t)-1;
    w->mtn_tp = flip_tree;
    w->mtn_add = 65536;
    w->ct2 = 0;
    w->mode0 = 1;
    w->mode1 = 0;
    w->way = 0;
    req_mtn(w, 0);
    w->hokan_rate = 0;
    w->hokan_count = 0;
    w->frm_no = 0;
    w->mtn_add = 65536;
    w->mtn_no = (uint32_t)(int32_t)s->req_mtn;
    if (set_motion(w, 0) != 0) w->flg |= 0x2000000u;
    else w->flg &= ~0x2000000u;
    s->req_mtn = -1;
}

/* --- Move (en15.c:519-937) ------------------------------------------------- */
static void en15_attack_choose(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p);

static void stand(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    float d;
    int dir;
    if (!player(w, &p)) return;
    recvx_t3_ikou(w, &p.pos, w->way);
    if (!target_distance(w, &p, &d)) return;
    if (35.0f < d) { set_modes01(w, 1, 3, 145, 1); return; }
    if (25.0f < d) { set_modes01(w, 1, 2, 327, 1); return; }
    dir = target_direction(w, &p);
    if (dir >= -1820) {
        if (target_direction(w, &p) < 1820 && 7.0f > d) {
            if (p.flg & 2u) { w->ct2 = 30; set_modes01(w, 1, 0, 0, 0); return; }
            w->mode2 = 0; set_modes01(w, 2, 0, 1456, 5); return;
        }
    }
    w->ct2 = 30;
    set_modes01(w, 1, 1, 1456, 1);
}

/* en15.c:595 __attack. Die Bedingungsglieder rufen target_distance mehrfach;
   der Wert aendert sich innerhalb des Aufrufs nicht. */
static void en15_attack_choose(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    int ang, f;
    float d;
    if (w->mtn_no != 1) return;
    f = w->frm_no / 65536;
    if (f < 47 || f > 54) return;
    ang = target_direction(w, p);
    if (!target_distance(w, p, &d)) return;
#define EN15_GRAB_OR(m2, way_, mtn_) do { \
        if (p->flg & 0x2u) { w->ct2 = 30; set_modes01(w, 1, 0, 0, 0); } \
        else if (w->ct0 == 0) { w->mode2 = (m2); set_modes01(w, 1, 4, (way_), (mtn_)); } \
    } while (0)
    if (w->mode0 == 1 && w->mode1 == 2) {
        if (((ang >= -24576 && ang < -8192) || (ang >= 8192 && ang < 24576)) && (21.0f <= d && d < 30.0f)) {
            EN15_GRAB_OR(0, 3276, 3); return;
        }
    }
    if (w->mode0 == 1 && w->mode1 == 2) {
        if (((ang >= -24576 && ang < -8192) || (ang >= 8192 && ang < 24576)) && (17.0f <= d && d < 21.0f)) {
            EN15_GRAB_OR(0, 3276, 2); return;
        }
    }
    if ((ang >= -2730 && ang < 2730) && (21.0f <= d && d < 30.0f)) { EN15_GRAB_OR(0, 3276, 3); return; }
    if ((ang >= -10922 && ang < 10922) && (17.0f <= d && d < 22.0f)) { EN15_GRAB_OR(0, 0, 2); return; }
    if (((ang <= -16384 && ang >= -32768) || (ang <= 32768 && ang >= 16384)) && (0 <= d && d < 22.0f)) {
        EN15_GRAB_OR(0, 0, 4); return;
    }
    if ((ang >= -16384 && ang < 16384) && (22.0f <= d)) { w->ct3 = 0; EN15_GRAB_OR(1, 0, 2); return; }
    if (((ang <= -16384 && ang >= -32768) || (ang <= 32768 && ang >= 16384)) && (22.0f <= d)) {
        w->ct3 = 0; EN15_GRAB_OR(1, 0, 4); return;
    }
#undef EN15_GRAB_OR
}

static void close_turn(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    float d;
    if (!player(w, &p)) return;
    recvx_t3_ikou(w, &p.pos, w->way);
    if (!target_distance(w, &p, &d)) return;
    if (16.0f < d) { set_modes01(w, 1, 2, 327, 1); return; }
    if (target_direction(w, &p) >= -1820) {
        if (target_direction(w, &p) < 1820 && 7.0f > d) {
            if (p.flg & 2u) { w->ct2 = 30; set_modes01(w, 1, 0, 0, 0); }
            else { w->mode2 = 0; set_modes01(w, 2, 0, 1456, 5); }
            return;
        }
    }
    if (9.0f < d) en15_attack_choose(w, &p);
}

static void chase(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    float d;
    if (!player(w, &p)) return;
    recvx_t3_ikou(w, &p.pos, w->way);
    if (!target_distance(w, &p, &d)) return;
    if (25.0f > d) set_modes01(w, 1, 2, 327, 1);
    else en15_attack_choose(w, &p);
}

/* en15.c:872-901 __goalAng/_goalAng/_goalAng2. */
static void goal_ang_base(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p, NJS_VECTOR* vec, NJS_VECTOR* ans)
{
    NJS_VECTOR v = { 0.0f, 1.0f, 0.0f };
    vec->x = p->pos.x - w->px; vec->y = p->pos.y - w->py; vec->z = p->pos.z - w->pz;
    njOuterProduct(vec, &v, ans);
}
static int goal_ang(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    NJS_VECTOR vec, ans;
    goal_ang_base(w, p, &vec, &ans);
    return nj_arc_tan2(ans.x, ans.z);
}
static int goal_ang2(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    NJS_VECTOR vec, ans;
    goal_ang_base(w, p, &vec, &ans);
    njAddVector(&ans, &vec);
    return nj_arc_tan2(ans.x, ans.z);
}

static void keep_far(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    float d;
    if (!player(w, &p)) return;
    if (!target_distance(w, &p, &d)) return;
    if (9.0f > d) { w->ct2 = 30; set_modes01(w, 1, 1, 1456, 1); return; }
    if (16.0f > d) recvx_en15_rot_char(&w->ay, w->flg, goal_ang2(w, &p), w->way);
    else recvx_en15_rot_char(&w->ay, w->flg, goal_ang(w, &p), w->way);
    if (35.0f < d) set_modes01(w, 1, 3, 145, 1);
    else en15_attack_choose(w, &p);
}

/* --- Angriff (en15.c:940) -------------------------------------------------- */
static int nj_cc(const RECVX_CB_CAPSULE* a, const RECVX_CB_CAPSULE* b) { return recvx_nj_check_cc(a, b); }

/* Gemeinsamer Treffer von bhEne15_AttackPlayerCC/BC (en15.c:2073-2110). */
static int attack_player_hit(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, const RECVX_ENEMY_VEC* hit_p,
                             const NJS_VECTOR* av, int damage)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    RECVX_EN15_PLAYER_DYN d;
    uint32_t j, n;
    int kno = 0;
    float latest = 0.0f;
    if (!x || !x->player_obj_num || !x->player_hit_set) { gap(w, "plp->mlwP->obj_num/djnt_no (en15)"); return 0; }
    n = x->player_obj_num();
    for (j = 0; j < n; j++) {
        NJS_MATRIX m;
        RECVX_ENEMY_VEC t;
        float dd;
        if (!player_joint(w, j, &m)) return 0;
        t.x = m[12]; t.y = m[13]; t.z = m[14];
        dd = dist3(hit_p, &t);
        if (j == 0) { latest = dd; kno = 0; }
        else if (latest > dd) { latest = dd; kno = (int)j; }
    }
    if (!dyn_get(w, &d)) return 0;
    d.dax = nj_arc_tan2(av->y, av->z);
    d.day = nj_arc_tan2(av->x, av->z);
    d.dv.x = av->x; d.dv.y = av->y; d.dv.z = av->z;
    dyn_set(w, &d);
    x->player_hit_set(kno, &d.dv, hit_p);
    /* plp->dam[kno] = damage: weder player.c noch pwksub.c lesen plp->dam;
       einziger Folgezugriff ist bhEne_InitDamage(plp), das es loescht. */
    p->hp -= damage;
    p->flg |= 4u;
    return 1;
}
static int attack_player_cc(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, const RECVX_CB_CAPSULE* cap,
                            const NJS_VECTOR* av, int damage)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    RECVX_ENEMY_CAPSULE watr;
    RECVX_CB_CAPSULE body;
    RECVX_ENEMY_VEC hit;
    if (!x || !x->player_watr || !x->player_watr(&watr)) { gap(w, "plp->watr (en15)"); return 0; }
    body.c1.x = watr.c1.x; body.c1.y = watr.c1.y; body.c1.z = watr.c1.z;
    body.c2.x = watr.c2.x; body.c2.y = watr.c2.y; body.c2.z = watr.c2.z; body.r = watr.r;
    if (!nj_cc(cap, &body)) return 0;
    hit.x = (cap->c1.x + cap->c2.x) / 2.0f;
    hit.y = (cap->c1.y + cap->c2.y) / 2.0f;
    hit.z = (cap->c1.z + cap->c2.z) / 2.0f;
    return attack_player_hit(w, p, &hit, av, damage);
}
static int attack_player_bc(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, const RECVX_CB_VEC box[8],
                            const NJS_VECTOR* av, int damage)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    RECVX_ENEMY_CAPSULE watr;
    RECVX_CB_CAPSULE body;
    RECVX_ENEMY_VEC hit;
    if (!x || !x->player_watr || !x->player_watr(&watr)) { gap(w, "plp->watr (en15)"); return 0; }
    body.c1.x = watr.c1.x; body.c1.y = watr.c1.y; body.c1.z = watr.c1.z;
    body.c2.x = watr.c2.x; body.c2.y = watr.c2.y; body.c2.z = watr.c2.z; body.r = watr.r;
    if (!recvx_nj_check_bc2(box, &body)) return 0;
    hit.x = (box[6].x + (box[5].x + (box[1].x + box[2].x))) / 4.0f;
    hit.y = (box[6].y + (box[5].y + (box[1].y + box[2].y))) / 4.0f;
    hit.z = (box[6].z + (box[5].z + (box[1].z + box[2].z))) / 4.0f;
    return attack_player_hit(w, p, &hit, av, damage);
}

/* en15.c:2329 SpecialAttack. */
static void special_attack(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* splash_v)
{
    RECVX_EN15_EF ef;
    RECVX_EN15_EFFPATCH patch;
    int eno;
    if (w->ct3 != 0) return;
    memset(&ef, 0, sizeof ef);
    ef.flg = 1;
    if (!joint_point(w, 10, 0.0f, 8.0f, 0.0f, &ef.pos)) return;
    ef.ay = 0; ef.mdlver = 0; ef.id = 397; ef.type = 4;
    if (!en15_sv || !en15_sv->effect_tb || !en15_sv->effect_patch) { gap(w, "bhSetEffectTb (en15 SpecialAttack)"); return; }
    eno = en15_sv->effect_tb(RECVX_EN15_EF_FLG | RECVX_EN15_EF_POS | RECVX_EN15_EF_AY | RECVX_EN15_EF_MDLVER |
                             RECVX_EN15_EF_ID | RECVX_EN15_EF_TYPE, &ef, NULL, NULL, 0);
    if (eno == -1) return;
    memset(&patch, 0, sizeof patch);
    patch.fields = RECVX_EN15_P_STFLG | RECVX_EN15_P_TEXP | RECVX_EN15_P_TEXID | RECVX_EN15_P_MODE3 |
                   RECVX_EN15_P_OWNER | RECVX_EN15_P_VEC;
    patch.tex_id = eff_info_texid[4];   /* eff_info[sys->ef.type] mit type 4 */
    patch.mode3 = 0;
    patch.vec = *splash_v;
    en15_sv->effect_patch(eno, w, &patch);
}

static void attack(RECVX_ENEMY_WORK* w)
{
    RECVX_EN15_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    const EN15_ATTACK* at;
    int i, f, k;
    if (w->ct3 != 0) w->ct3--;
    if (!(w->mtn_no >= 2 && w->mtn_no < 5)) return;
    if (!player(w, &p)) return;
    recvx_t3_ikou(w, &p.pos, w->way);
    f = w->frm_no / 65536;
    if (f == 20) w->way = 0;
    k = mtn_check(w);
    at = &attack_col_tab[k];
    if ((at->start_frm <= f && f < at->end_frm) && !(s->hold & 0x1)) {
        for (i = 0; at->obj_no[i] != -1; i++) {
            RECVX_CB_CAPSULE cap;
            RECVX_CB_VEC box[8];
            NJS_VECTOR attack_v;
            int hit;
            memset(&cap, 0, sizeof cap);
            memset(box, 0, sizeof box);
            if (k == 1) {
                RECVX_ENEMY_VEC c;
                cap.r = at->volume;
                if (!joint_point(w, (uint32_t)at->obj_no[i], 0, 0, 0, &c)) return;
                cap.c1.x = c.x; cap.c1.y = c.y; cap.c1.z = c.z;
                if (at->obj_no[i + 1] != -1) {
                    if (!joint_point(w, (uint32_t)at->obj_no[i + 1], 0, 0, 0, &c)) return;
                } else {
                    if (!joint_point(w, 10, 0.0f, 8.0f, 0.0f, &c)) return;
                }
                cap.c2.x = c.x; cap.c2.y = c.y; cap.c2.z = c.z;
            } else {
                static const int8_t left_idx[4] = { 0, 4, 5, 1 }, right_idx[4] = { 3, 7, 6, 2 };
                float vane = 2.0f;
                const int8_t *f_idx, *b_idx;
                const float half = at->volume / 2.0f;
                RECVX_ENEMY_VEC c;
                if (k == 0) { b_idx = left_idx; f_idx = right_idx; }
                else { vane *= -1.0f; b_idx = right_idx; f_idx = left_idx; }
                /* *((NJS_POINT3*)exp0 + (obj_no - 5)) */
                {
                    const int pi = at->obj_no[i] - 5;
                    if (pi < 0 || pi > 6) { gap(w, "bhEne15_Attack: exp0-Punkt ausserhalb"); return; }
                    box[b_idx[0]].x = box[b_idx[1]].x = s->pts[pi].x;
                    box[b_idx[0]].y = box[b_idx[1]].y = s->pts[pi].y;
                    box[b_idx[0]].z = box[b_idx[1]].z = s->pts[pi].z;
                }
                box[b_idx[0]].y += half;
                box[b_idx[1]].y -= half;
                if (!joint_point(w, (uint32_t)at->obj_no[i], vane, 0, 0, &c)) return;
                box[b_idx[3]].x = c.x; box[b_idx[3]].y = c.y; box[b_idx[3]].z = c.z;
                box[b_idx[2]] = box[b_idx[3]];
                box[b_idx[3]].y += half;
                box[b_idx[2]].y -= half;
                if (at->obj_no[i + 1] != -1) {
                    const int pi = at->obj_no[i + 1] - 5;
                    if (pi < 0 || pi > 6) { gap(w, "bhEne15_Attack: exp0-Punkt ausserhalb"); return; }
                    box[f_idx[0]].x = box[f_idx[1]].x = s->pts[pi].x;
                    box[f_idx[0]].y = box[f_idx[1]].y = s->pts[pi].y;
                    box[f_idx[0]].z = box[f_idx[1]].z = s->pts[pi].z;
                    box[f_idx[0]].y += half;
                    box[f_idx[1]].y -= half;
                    if (!joint_point(w, (uint32_t)at->obj_no[i + 1], vane, 0, 0, &c)) return;
                } else {
                    const int pi = at->obj_no[i] - 4;
                    if (pi < 0 || pi > 6) { gap(w, "bhEne15_Attack: exp0-Punkt ausserhalb"); return; }
                    box[f_idx[0]].x = box[f_idx[1]].x = s->pts[pi].x;
                    box[f_idx[0]].y = box[f_idx[1]].y = s->pts[pi].y;
                    box[f_idx[0]].z = box[f_idx[1]].z = s->pts[pi].z;
                    box[f_idx[0]].y += half;
                    box[f_idx[1]].y -= half;
                    if (!joint_point(w, 10, vane, 9.8f, 0, &c)) return;
                }
                box[f_idx[3]].x = c.x; box[f_idx[3]].y = c.y; box[f_idx[3]].z = c.z;
                box[f_idx[2]] = box[f_idx[3]];
                box[f_idx[3]].y += half;
                box[f_idx[2]].y -= half;
            }
            if (k == 1) {
                NJS_MATRIX mat;
                NJS_VECTOR vec = { 0, 0, -1.0f };
                njUnitMatrix(&mat);
                njRotateY(&mat, (Angle)w->ay);
                njCalcVector(&mat, &vec, &attack_v);
            } else {
                NJS_VECTOR vec0, vec1;
                vec0.x = box[1].x; vec0.y = box[1].y; vec0.z = box[1].z;
                vec1.x = box[2].x; vec1.y = box[2].y; vec1.z = box[2].z;
                njSubVector(&vec1, &vec0);
                vec0.x = 0; vec0.y = 1.0f; vec0.z = 0;
                njOuterProduct(&vec0, &vec1, &attack_v);
            }
            njUnitVector(&attack_v);
            attack_v.x *= at->spd; attack_v.y *= at->spd; attack_v.z *= at->spd;
            {
                NJS_MATRIX mat;
                NJS_VECTOR vec = { 0, 1.0f, 0 }, axis;
                njOuterProduct(&attack_v, &vec, &axis);
                njUnitVector(&axis);
                vec = attack_v;
                njUnitMatrix(&mat);
                njRotate(&mat, &axis, (Angle)at->impact_ang);
                njCalcVector(&mat, &vec, &attack_v);
            }
            hit = (k == 1) ? attack_player_cc(w, &p, &cap, &attack_v, at->damage)
                           : attack_player_bc(w, &p, box, &attack_v, at->damage);
            if (hit) {
                RECVX_EN15_PLAYER_DYN d;
                const RECVX_T3_LINK lk = { NULL, 1 };
                const RECVX_ENEMY_VEC dp = { w->dpx, w->dpy, w->dpz };
                put_player(w, &p);
                recvx_t3_set_blood_effect(lk, 1);
                if (!player(w, &p)) return;
                enemy_se(w, &dp, (k == 1) ? 0x12305 : 0x12303);
                vibration(w, 1, 11);
                p.flg &= ~0x110u;
                if (!dyn_get(w, &d)) return;
                if (recvx_t3_dgc_dir_check(&d.dv, p.ay) != 0) {
                    d.day += 32768;
                    grab_player(w, &p, &d, 1, 0, 0.0f, 14, 5);
                } else {
                    grab_player(w, &p, &d, 1, 0, 0.0f, 15, 5);
                }
                put_player(w, &p);
                break;
            }
        }
    }
    if ((at->sp_start_frm <= f && f < at->sp_end_frm) && w->mode2 == 1) {
        RECVX_ENEMY_VEC splash_v;
        if (!joint_point(w, 10, 0.0f, 9.8f, 0.0f, &splash_v)) return;
        splash_v.x -= s->pts[6].x; splash_v.y -= s->pts[6].y; splash_v.z -= s->pts[6].z;
        special_attack(w, &splash_v);
    }
    if (k != 1) {
        for (i = 0; at->obj_no[i] != -1; i++) {
            const int pi = at->obj_no[i] - 5;
            if (pi < 0 || pi > 6) { gap(w, "bhEne15_Attack: exp0-Punkt ausserhalb"); return; }
            if (!joint_point(w, (uint32_t)at->obj_no[i], 0, 0, 0, &s->pts[pi])) return;
        }
        if (!joint_point(w, 10, 0.0f, 9.8f, 0.0f, &s->pts[6])) return;
    }
    {
        const uint32_t n = frames(w, w->mtn_no);
        if (n && f == (int)n - 1) {
            int r;
            if (!rnd_mod(w, 150, &r)) return;
            w->ct0 = r + 30;
            set_modes01(w, 1, 3, 145, 1);
        }
    }
}

static void en15_move(RECVX_ENEMY_WORK* w)
{
    switch (w->mode1) {
    case 0: stand(w); break;
    case 1: close_turn(w); break;
    case 2: keep_far(w); break;
    case 3: chase(w); break;
    case 4: attack(w); break;
    default: gap(w, "bhEne15 Move_func[mode1]"); break;
    }
    if (w->ct2 != 0) w->ct2--;
    if (w->ct0 != 0) w->ct0--;
    if (w->ct1 != 0) w->ct1--;
}

/* --- Wurf (en15.c:1240) ---------------------------------------------------- */
static void en15_throw(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    RECVX_EN15_PLAYER_DYN d;
    float dist;
    int f;
    if (!player(w, &p)) return;
    if (w->mode2 == 0 && (w->frm_no / 65536) < 22) recvx_t3_ikou(w, &p.pos, w->way);
    if (w->mtn_no != 5) return;
    f = w->frm_no / 65536;
    if (w->mode2 != 0) {
        if (f == 0) set_modes01(w, 1, 3, 145, 1);
        return;
    }
    if (f == 0) {
        int dir;
        if (!target_distance(w, &p, &dist)) return;
        dir = target_direction(w, &p);
        if (p.mode0 == 3 || p.mode0 == 6 || p.hp < 0 || dir < -2730 || dir >= 2730 || 9.0f < dist) {
            set_modes01(w, 1, 3, 145, 1);
            return;
        }
    }
    if (f == 22) {
        int dir;
        if (!target_distance(w, &p, &dist)) return;
        dir = target_direction(w, &p);
        if (p.mode0 != 3 && p.mode0 != 6 && p.hp >= 0 && dir >= -2730 && dir < 2730 && 9.0f >= dist) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            if (!dyn_get(w, &d)) return;
            if (recvx_t3_cdir_check(p.ay, w->ay) != 0) grab_player(w, &p, &d, 1, 5, 1.0f, 21, 4);
            else grab_player(w, &p, &d, 1, 5, 1.0f, 19, 4);
            put_player(w, &p);
            enemy_se(w, &at, 74502);
            vibration(w, 1, 9);
            w->flg &= ~0x40u;
        } else {
            w->mode2 = 1;
            w->mtn_add = -65536;
            return;
        }
    }
    if (f == 55) {
        RECVX_ENEMY_VEC pos;
        if (!joint_point(w, 10, 0.0f, 8.0f, 0.0f, &pos)) return;
        if (recvx_t3_attack_hit_check(&pos, 3.0f)) {
            const RECVX_T3_LINK lk = { NULL, 1 };
            recvx_t3_set_blood_effect(lk, 1);
        }
        enemy_se(w, &pos, 74503);
        if (!player(w, &p)) return;
        p.flg &= ~0x118u;
        st(w)->hold |= 0x1;
        w->mode3 = 6;
        if (!dyn_get(w, &d)) return;
        d.spd = 0.0f;
        dyn_set(w, &d);
        put_player(w, &p);
    }
    if (f == 78) {
        NJS_MATRIX mat;
        NJS_VECTOR attack_v, vec = { 0.0f, 0.0f, -1.0f };
        njUnitMatrix(&mat);
        njRotateY(&mat, (Angle)(w->ay + 32768));
        njCalcVector(&mat, &vec, &attack_v);
        njUnitVector(&attack_v);
        attack_v.x *= 3.0f; attack_v.y *= 3.0f; attack_v.z *= 3.0f;
        if (!player(w, &p)) return;
        if (!dyn_get(w, &d)) return;
        d.dax = nj_arc_tan2(attack_v.y, attack_v.z);
        d.day = nj_arc_tan2(attack_v.x, attack_v.z);
        d.dv.x = attack_v.x; d.dv.y = attack_v.y; d.dv.z = attack_v.z;
        enemy_se(w, &p.pos, 74504);
        p.flg &= ~0x100u;
        /* en15.c:1388-1403: beide Zweige von bhDGCdirCheck sind gleich. */
        (void)recvx_t3_dgc_dir_check(&d.dv, p.ay);
        st(w)->hold |= 0x1;
        w->mode3 = 0;
        d.spd = 0.0f;
        dyn_set(w, &d);
        put_player(w, &p);
        w->flg |= 0x40u;
    }
    {
        const uint32_t n = frames(w, w->mtn_no);
        if (n && f == (int)n - 1) {
            int r;
            if (!rnd_mod(w, 10, &r)) return;
            w->ct2 = (r * 20) + 15;
            set_modes01(w, 1, 0, 0, 0);
            return;
        }
    }
}

/* --- Schaden/Tod (en15.c:1425/1454) ---------------------------------------- */
static void en15_damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN15_STATE* s = st(w);
    const int f = w->frm_no / 65536;
    uint32_t n;
    if (!(w->mtn_no >= 6 && w->mtn_no < 11)) return;
    if (w->mtn_no == 10 && f == 1) s->goal = w->ay + 16384;
    n = frames(w, w->mtn_no);
    if (!n) return;
    if (w->mtn_no == 10) {
        if (f >= 29 && f < (int)n) recvx_en15_rot_char(&w->ay, w->flg, s->goal, 910);
    }
    if (f == (int)n - 1) {
        set_modes01(w, 1, 3, 145, 1);
        w->ct1 = 60;
    }
}

static void en15_die(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC v, j4;
    uint32_t n;
    if (w->mtn_no != 11) return;
    if (!joint_pos(w, 4, &j4)) return;
    v.x = w->px - j4.x; v.y = 0.0f; v.z = w->pz - j4.z;
    if (scalor(&v) > 3.8f) w->ar = scalor(&v);
    else w->ar = 3.8f;
    n = frames(w, w->mtn_no);
    if (n && (w->frm_no / 65536) == (int)n - 1) {
        w->flg &= ~0x40u;
        w->flg |= 2u;
        w->mtn_add = 0;
        if (w->mode1 == 0) end_boss_flags(w);
    }
}

/* en15.c:1499 NearestCapsule. */
static int nearest_capsule(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, RECVX_CB_CAPSULE* dest, int16_t* jnt)
{
    const RECVX_ENEMY_CPCL* ctab = w->cpcl;
    RECVX_CB_CAPSULE top, cap;
    int16_t topjnt = 0, _jnt;
    float topdis = 0.0f, dis;
    int notop = 1;
    memset(&top, 0, sizeof top);
    while (ctab->x != 0 && ctab->y != 0 && ctab->z != 0) {
        RECVX_ENEMY_VEC a, b, mid;
        if (ctab->x != ctab->y) {
            _jnt = ctab->x;
            if (!joint_pos(w, (uint32_t)ctab->x, &a) || !joint_pos(w, (uint32_t)ctab->y, &b)) return 0;
            cap.c1.x = a.x; cap.c1.y = a.y; cap.c1.z = a.z;
            cap.c2.x = b.x; cap.c2.y = b.y; cap.c2.z = b.z;
            cap.r = 0.1f * ctab->z;
        } else {
            _jnt = ctab->x;
            if (!joint_pos(w, (uint32_t)ctab->x, &a)) return 0;
            cap.c1.x = a.x; cap.c1.y = a.y; cap.c1.z = a.z;
            cap.r = 0.1f * ctab->z;
            ctab++;
            cap.c2.x = 0.1f * ctab->x;
            cap.c2.y = 0.1f * ctab->y;
            cap.c2.z = 0.1f * ctab->z;
            cap.c1.x += cap.c2.x; cap.c1.y += cap.c2.y; cap.c1.z += cap.c2.z;
            cap.c2 = cap.c1;
        }
        mid.x = (cap.c1.x + cap.c2.x) / 2.0f;
        mid.y = (cap.c1.y + cap.c2.y) / 2.0f;
        mid.z = (cap.c1.z + cap.c2.z) / 2.0f;
        dis = dist3(pos, &mid);
        if (notop) { topdis = dis; top = cap; topjnt = _jnt; notop = 0; }
        else if (dis < topdis) { topdis = dis; top = cap; topjnt = _jnt; }
        ctab++;
    }
    *dest = top;
    *jnt = topjnt;
    return !notop;
}

/* en15.c:2360 _bhEne_SetPoison. */
static void set_poison_at(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ofp, int16_t ry)
{
    static const float tvxy[4][2] = { { 2.0f, -1.0f }, { 0.0f, -1.0f }, { 2.0f, 1.0f }, { 0.0f, 1.0f } };
    const uint32_t mask = RECVX_EN15_EF_ID | RECVX_EN15_EF_TYPE | RECVX_EN15_EF_FLG | RECVX_EN15_EF_POS |
                          RECVX_EN15_EF_SX | RECVX_EN15_EF_SY | RECVX_EN15_EF_SZ | RECVX_EN15_EF_AY |
                          RECVX_EN15_EF_AX | RECVX_EN15_EF_MDLVER;
    RECVX_EN15_EF ef;
    RECVX_EN15_EFFPATCH patch;
    int eno, k;
    if (!en15_sv || !en15_sv->effect_tb || !en15_sv->effect_patch) { gap(w, "bhSetEffectTb (en15 SetPoison)"); return; }
    for (k = 0; k < 3; ++k) {
        memset(&ef, 0, sizeof ef);
        ef.id = 397; ef.flg = 1;
        ef.type = (k == 0) ? 1u : 0u;
        ef.sx = ef.sy = 1.0f; ef.sz = 0.0f;
        /* EF_WORK.ay/ax sind short (types.h:226/227): ry - NJM_DEG_ANG(90) wird gekuerzt. */
        ef.ay = (k == 0) ? (int32_t)ry : (int32_t)(int16_t)(ry - 16384);
        ef.ax = (k == 2) ? 16384 : 0;
        ef.mdlver = 0;
        eno = en15_sv->effect_tb(mask, &ef, ofp, w, w->djnt_no);
        if (eno == -1) continue;
        memset(&patch, 0, sizeof patch);
        patch.fields = RECVX_EN15_P_STFLG | RECVX_EN15_P_TEXP | RECVX_EN15_P_TEXID | RECVX_EN15_P_OWNER;
        patch.tex_id = eff_info_texid[ef.type];
        if (k == 0) { patch.fields |= RECVX_EN15_P_VEC; patch.vec.x = patch.vec.y = patch.vec.z = 0.0f; }
        else {
            if (k == 1) { patch.fields |= RECVX_EN15_P_MODE3; patch.mode3 = 0; }
            patch.fields |= RECVX_EN15_P_TVXY;
            memcpy(patch.tv, tvxy, sizeof tvxy);
        }
        en15_sv->effect_patch(eno, w, &patch);
    }
}

/* en15.c:2479 bhEne_SetPoison(epw, bt). */
static void set_poison(RECVX_ENEMY_WORK* w, const RECVX_T3_BT* bt)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    RECVX_ENEMY_VEC ofp;
    NJS_MATRIX m;
    int fhit = 0;
    int16_t way = 0;
    if (poison_eff_wait != 0) return;
    poison_eff_wait = 15;
    if (w->djnt_no < 0 || w->djnt_no >= 24) { gap(w, "bhEne_SetPoison: owP[djnt_no] ausserhalb"); return; }
    if (!joint(w, (uint32_t)w->djnt_no, &m)) return;
    if (bt == NULL || w->wpnr_no == 13 || w->wpnr_no == 10) {
        NJS_VECTOR ps, o;
        NJS_MATRIX inv;
        ps.x = w->dpx - m[12]; ps.y = w->dpy - m[13]; ps.z = w->dpz - m[14];
        memcpy(inv, m, sizeof inv);
        njInvertMatrix(&inv);
        njCalcVector(&inv, &ps, &o);
        ofp.x = o.x; ofp.y = o.y; ofp.z = o.z;
    } else {
        const RECVX_T3_BT* btp;
        const RECVX_ENEMY_VEC dv = { w->dvx, w->dvy, w->dvz };
        float r1, r2;
        if (recvx_t3_dgc_dir_check2(&dv, m) == 0) fhit = 1;
        btp = &bt[w->djnt_no];
        if (!rnd(w, &r1)) return;
        ofp.x = btp->x + (btp->xlen - (2.0f * btp->xlen * r1));
        if (!rnd(w, &r2)) return;
        ofp.y = btp->y + (btp->ylen - (2.0f * btp->ylen * r2));
        ofp.z = fhit ? -btp->z : btp->z;
        w->djnt_no = btp->lnk_obj;
    }
    if (x && x->player_way) way = (int16_t)x->player_way();
    else gap(w, "plp->way (en15)");
    set_poison_at(w, &ofp, way);
}

/* en15.c:1564 CheckDamage. */
static void check_damage(RECVX_ENEMY_WORK* w)
{
    int core = 0;
    int sel;
    if (!(w->flg & 0x4u)) return;
    recvx_t3_calc_damage(w, CombWepTbl, CombJointTbl);
    if (w->wpnr_no < 0 || w->wpnr_no >= 21) { gap(w, "bhEne15 CheckDamage: wpnr_no ausserhalb der Tabellen"); return; }
    if (!(w->comb_flg & 0x8u) && w->wpnr_no == 2) {
        RECVX_ENEMY_VEC pos;
        if (!joint_point(w, 3, 0.1f * CapColTab[1].x, 0.1f * CapColTab[1].y, 0.1f * CapColTab[1].z, &pos)) return;
        pos.x -= w->dpx; pos.y -= w->dpy; pos.z -= w->dpz;
        if (scalor(&pos) < (3.0f + (0.1f * CapColTab[0].z))) {
            core = 1;
            w->total_dam *= 20;
        }
    }
    if (!(w->comb_flg & 0x8u) && (w->wpnr_no == 13 && w->cpcl_no == 0)) {
        core = 1;
        w->total_dam *= 5;
    }
    w->hp -= w->total_dam;
    sel = (w->comb_flg & 0x10u) ? 0 : (w->comb_flg & 0x20u) ? 1 : 2;
    {
        const RECVX_T3_LINK lk = { w, 0 };
        if (core) {
            if (w->hp >= 0) recvx_t3_set_blood_effect5(lk, 0, 1);
        } else {
            if (w->wpnr_no != 16 || (w->flg2 & 0x4u)) {
                if ((w->comb_flg & 0x1u) && Combo_Eff[w->wpnr_no].blood_type[sel] != -1)
                    recvx_t3_set_blood_effect5(lk, 0, Combo_Eff[w->wpnr_no].blood_type[sel]);
                else if (WpnDamageTbl[w->wpnr_no].blood_type != -1)
                    recvx_t3_set_blood_effect5(lk, 0, WpnDamageTbl[w->wpnr_no].blood_type);
            }
            if (w->wpnr_no != 16 || (w->flg2 & 0x4u)) set_poison(w, prt_blood_tbl);
        }
    }
    {
        int fire = (w->wpnr_no == 18) || (w->wpnr_no == 16 && (w->flg2 & 0x4u));
        if (!fire && w->wpnr_no == 16) {
            float r;
            if (!rnd(w, &r)) return;
            fire = ((double)r < 0.3);
        }
        if (fire) {
            RECVX_CB_CAPSULE cap;
            RECVX_CB_VEC dp, o;
            RECVX_ENEMY_VEC jp, ofp;
            int16_t jnt = 0;
            const RECVX_ENEMY_VEC dpos = { w->dpx, w->dpy, w->dpz };
            nearest_capsule(w, &dpos, &cap, &jnt);
            dp.x = w->dpx; dp.y = w->dpy; dp.z = w->dpz;
            recvx_cb_distance_p2c(&dp, &cap, &o);
            if (!joint_pos(w, (uint32_t)jnt, &jp)) return;
            ofp.x = o.x - jp.x; ofp.y = o.y - jp.y; ofp.z = o.z - jp.z;
            if (w->sv->fire_effect) w->sv->fire_effect(w, jnt, &ofp, 2.0f, 90);
            else gap(w, "bhEne_SetFireEffect (en15)");
        }
    }
    if (w->hp >= 0) {
        const int f = w->frm_no / 65536;
        if (core) {
            set_modes01(w, 3, 4, 0, 10);
        } else if (w->comb_flg & 0x1u) {
            switch (Combo_Eff[w->wpnr_no].dmg_type[sel]) {
            case 1:
                if (w->comb_flg & 0x8u) set_modes01(w, 3, 1, 0, 7);
                else set_modes01(w, 3, 0, 0, 6);
                break;
            case 2:
                if (w->comb_flg & 0x8u) set_modes01(w, 3, 3, 0, 9);
                else set_modes01(w, 3, 2, 0, 8);
                break;
            default: break;
            }
        } else if (w->mode0 != 2 && w->mode0 != 3 && (w->mode0 != 1 || w->mode1 != 4)) {
            if (w->wpnr_no != 16 || (w->flg2 & 0x4u)) {
                switch (WpnDamageTbl[w->wpnr_no].dmg_type) {
                case 1:
                    if (w->comb_flg & 0x8u) {
                        if (w->ct1 == 0 && w->mtn_no == 1 && f >= 21 && f < 29) set_modes01(w, 3, 1, 0, 7);
                    } else {
                        if (w->ct1 == 0 && w->mtn_no == 1 && f >= 47 && f < 55) set_modes01(w, 3, 0, 0, 6);
                    }
                    break;
                case 2:
                    if (w->comb_flg & 0x8u) {
                        if (w->ct1 == 0 && w->mtn_no == 1 && f >= 62 && f < 74) set_modes01(w, 3, 3, 0, 9);
                    } else {
                        if (w->ct1 == 0 && w->mtn_no == 1 && f >= 13 && f < 25) set_modes01(w, 3, 2, 0, 8);
                    }
                    break;
                default: break;
                }
            }
        }
    } else if (w->mode0 != 4) {
        int gm = 0;
        if (!en15_sv || !en15_sv->game_mode || !en15_sv->game_mode(&gm)) { gap(w, "sys->gm_mode (en15)"); return; }
        if (gm != 3) {
            if (core) {
                w->flg |= 0x2u;
                ev_flag(w, 57, 1);
                end_boss_flags(w);
            } else {
                set_modes01(w, 4, 0, 0, 11);
            }
        } else {
            set_modes01(w, 4, 1, 0, 11);
        }
    }
    w->flg &= ~0x4u;
    recvx_enemy_init_damage(w);
}

/* --- Motionwahl (en15.c:1857-1960) ----------------------------------------- */
static int get_relay(RECVX_ENEMY_WORK* w, const EN15_RELAY** ret)
{
    const RECVX_EN15_STATE* s = st(w);
    const int f = w->frm_no / 65536;
    int i, found = 0;
    uint32_t n = 0;
    *ret = NULL;
    for (i = 0; mtn_relay[i].old_mtn_no != -1; i++) {
        if ((int32_t)w->mtn_no == mtn_relay[i].old_mtn_no && s->req_mtn == mtn_relay[i].next_mtn_no) {
            if (mtn_relay[i].from == -1) { n = frames(w, w->mtn_no); if (!n) return 0; }
            if (f == mtn_relay[i].from || (mtn_relay[i].from == -1 && f == (int)n - 1)) {
                *ret = &mtn_relay[i];
                return 1;
            }
            found = 1;
        }
    }
    for (i = 0; mtn_relay_relay[i].old_mtn_no != -1; i++) {
        if ((int32_t)w->mtn_no == mtn_relay_relay[i].old_mtn_no && s->req_mtn == mtn_relay_relay[i].next_mtn_no) {
            const EN15_RELAY* r = mtn_relay_relay[i].relay;
            if (r->from == -1) { n = frames(w, w->mtn_no); if (!n) return 0; }
            if (f == r->from || (r->from == -1 && f >= (int)n - 1)) {
                *ret = r;
                return 1;
            }
            found = 1;
        }
    }
    return found;
}

static void set_mtn(RECVX_ENEMY_WORK* w)
{
    RECVX_EN15_STATE* s = st(w);
    const EN15_RELAY* relay;
    if (s->req_mtn != -1) {
        if (get_relay(w, &relay) != 0) {
            if (relay != NULL) {
                w->hokan_rate = 19660;
                w->hokan_count = 30;
                w->frm_no = (int32_t)(65536.0f * (float)relay->to);
                w->mtn_add = 65536;
                w->mtn_no = (uint32_t)(int32_t)relay->next_mtn_no;
                if (set_motion(w, 0) != 0) w->flg |= 0x2000000u;
                else w->flg &= ~0x2000000u;
                if (s->req_mtn == (int32_t)w->mtn_no) s->req_mtn = -1;
                return;
            }
        } else if ((int32_t)w->mtn_no != s->req_mtn) {
            w->hokan_rate = 6553;
            w->hokan_count = 0;
            w->frm_no = 0;
            w->mtn_add = 65536;
            w->mtn_no = (uint32_t)(int32_t)s->req_mtn;
            if (set_motion(w, 0) != 0) w->flg |= 0x2000000u;
            else w->flg &= ~0x2000000u;
            s->req_mtn = -1;
            return;
        }
    }
    set_motion(w, w->mtn_add);
}

/* en15.c:2021 LockLeg. */
static void lock_leg(RECVX_ENEMY_WORK* w)
{
    const int f = w->frm_no / 65536;
    int i, j;
    for (i = 0; !(leglock_tab[i].mtn_no == -1 && leglock_tab[i].default_lr == -1); i++) {
        if ((int32_t)w->mtn_no == leglock_tab[i].mtn_no) {
            int8_t lock_leg_ = leglock_tab[i].default_lr;
            RECVX_ENEMY_VEC pos;
            for (j = 0; !(leglock_tab[i].list[j].start == -1 && leglock_tab[i].list[j].end == -1); j++) {
                if (leglock_tab[i].list[j].start <= f && leglock_tab[i].list[j].end > f)
                    lock_leg_ = (lock_leg_ == 0) ? 1 : 0;
            }
            if (!en15_sv || !en15_sv->calc_fix_offset) { gap(w, "bhFixPosition/bhCalcFixOffset (en15)"); return; }
            en15_sv->calc_fix_offset(w, (lock_leg_ == 0) ? rfoot_joint_tree : lfoot_joint_tree, NULL, &pos);
            w->px -= pos.x;
            w->pz -= pos.z;
            return;
        }
    }
}

/* --- Spielerfuehrung Ply_func (en15.c:2865-3347) --------------------------- */
static int able_to_fall(RECVX_ENEMY_WORK* w, int* out)
{
    NJS_MATRIX m;
    RECVX_ENEMY_VEC p1, p2, p3, p4;
    if (!player_joint(w, 1, &m)) return 0;
    if (!en15_sv || !en15_sv->room_point || !en15_sv->room_point(1, &p1) || !en15_sv->room_point(2, &p2) ||
        !en15_sv->room_point(3, &p3) || !en15_sv->room_point(4, &p4)) {
        gap(w, "rom->posp[1..4] (en15 AbleToFall)"); return 0;
    }
    *out = (m[12] <= p1.x || m[12] >= p4.x || m[14] <= p2.z || m[14] >= p3.z) ? 1 : 0;
    return 1;
}
/* en15.c:2199 SetSmoke: sys->ef id 257, flg 1, type 0, Lage, Skalierung 0.5. */
static void set_smoke(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos)
{
    RECVX_EN15_EF ef;
    memset(&ef, 0, sizeof ef);
    ef.id = 257; ef.flg = 1; ef.type = 0; ef.pos = *pos; ef.sx = ef.sy = ef.sz = 0.5f;
    if (!en15_sv || !en15_sv->effect_tb) { gap(w, "bhSetEffectTb (en15 SetSmoke)"); return; }
    en15_sv->effect_tb(RECVX_EN15_EF_ID | RECVX_EN15_EF_FLG | RECVX_EN15_EF_TYPE | RECVX_EN15_EF_POS |
                       RECVX_EN15_EF_SX | RECVX_EN15_EF_SY | RECVX_EN15_EF_SZ, &ef, NULL, NULL, 0);
}
/* Die Schalter ueber plp->mtn_no ohne default (en15.c:2953/3066/3134/3168):
   ein anderer Wert laesst _mtnno uninitialisiert. */
static int fall_mtn(RECVX_ENEMY_WORK* w, uint32_t mtn, int* out)
{
    switch (mtn) {
    case 15: *out = 26; return 1;
    case 14: *out = 25; return 1;
    case 22: *out = 26; return 1;
    case 20: *out = 25; return 1;
    default: gap(w, "bhEne15 Ply_func: _mtnno uninitialisiert (Original liest einen Stapelwert)"); return 0;
    }
}
static void end_of_player_motion(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    const uint32_t n = player_frames(w, p);
    if (n && (p->frm_no / 65536) == (int)n - 1) p->mtn_add = 0;
}

/* en15.c:2879 _DrivePlayer. */
static int drive_player_step(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d, int* ans)
{
    NJS_MATRIX m;
    float ofs;
    *ans = 0;
    if (!player_joint(w, 1, &m)) return 0;
    ofs = m[13] - p->pos.y;
    p->pos.x += d->dv.x; p->pos.y += d->dv.y; p->pos.z += d->dv.z;
    if (p->pos.y + ofs < 0.0f) {
        int fall;
        *ans = 3;
        if (!able_to_fall(w, &fall)) return 0;
        if (fall) {
            *ans = 2;
        } else {
            const float fp0 = njSqrt(powf(d->pos_b.x, 2.0f) + powf(d->pos_b.z, 2.0f));
            const float fp1 = njSqrt(powf(p->pos.x, 2.0f) + powf(p->pos.z, 2.0f));
            const float fp = fp0 + fp1;
            p->pos.y = -1.0f * (p->pos.y + ofs) - ofs;
            d->dv.y -= 1.3f * (fp0 / fp);
            d->dv.y *= -0.15f;
            d->dv.y -= 1.3f * (fp1 / fp);
        }
        if (fabsf(d->dv.y) < 0.3f) {
            p->pos.y = 0.0f;
            d->dv.y = 0.0f;
            d->spd = scalor(&d->dv);
            if (p->mtn_no == 15 || p->mtn_no == 19 || p->mtn_no == 20 || p->mtn_no == 21 || p->mtn_no == 22)
                d->spd *= -1.0f;
            *ans = 1;
        }
    } else {
        d->dv.y -= 1.3f;
    }
    d->dv.x *= 0.9f; d->dv.y *= 0.9f; d->dv.z *= 0.9f;
    return 1;
}

static void drive_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d)
{
    int ans, mtn;
    recvx_en15_rot_char(&p->ay, p->flg, d->day, 16384);
    if (!drive_player_step(w, p, d, &ans)) return;
    switch (ans) {
    case 2:
        if (p->mtn_no == 19 || p->mtn_no == 21) {
            st(w)->hold |= 0x1;
            w->mode3 = 3;
            d->spd = 0.0f;
            dyn_set(w, d);
            break;
        }
        if (!fall_mtn(w, p->mtn_no, &mtn)) return;
        grab_player(w, p, d, 1, 3, d->spd, (uint32_t)mtn, 5);
        break;
    case 3:
        dyn_set(w, d);
        if (p->mtn_no == 19 || p->mtn_no == 21) set_ply_mtn(w, p, p->mtn_no == 19 ? 22u : 20u);
        set_smoke(w, &p->pos);
        if (p->mode3 == 0) { player_voice(w, 519); vibration(w, 1, 11); p->mode3++; }
        break;
    case 1:
        dyn_set(w, d);
        if (p->mtn_no == 19 || p->mtn_no == 21) {
            set_ply_mtn(w, p, p->mtn_no == 19 ? 22u : 20u);
            p->hp -= 80;
        }
        if (p->mode3 == 0) { player_voice(w, 519); vibration(w, 1, 11); p->mode3++; }
        st(w)->hold |= 0x1;
        w->mode3 = 1;
        break;
    default:
        dyn_set(w, d);
        break;
    }
    end_of_player_motion(w, p);
}

static void falling_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d)
{
    recvx_en15_rot_char(&p->ay, p->flg, d->day, 16384);
    p->pos.x += d->dv.x; p->pos.y += d->dv.y; p->pos.z += d->dv.z;
    d->dv.y -= 1.3f;
    d->dv.x *= 0.9f; d->dv.y *= 0.9f; d->dv.z *= 0.9f;
    dyn_set(w, d);
    if (p->pos.y < -90.0f) {
        p->hp = -1;
        grab_player(w, p, d, 1, 4, d->spd, p->mtn_no, 6);
    }
    end_of_player_motion(w, p);
}

static void slide_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d)
{
    RECVX_ENEMY_VEC delta;
    int fall, mtn;
    float dl;
    if (!able_to_fall(w, &fall)) return;
    if (fall) {
        if (!fall_mtn(w, p->mtn_no, &mtn)) return;
        grab_player(w, p, d, 1, 3, d->spd, (uint32_t)mtn, 5);
        return;
    }
    recvx_en15_rot_char(&p->ay, p->flg, d->day, 16384);
    {   /* pwksub.c:133 bhAddSpeed(plp, 0) */
        const int a = p->ay & 0xFFFF;
        p->pos.x -= d->spd * njSin((Angle)a);
        p->pos.z -= d->spd * njCos((Angle)a);
    }
    end_of_player_motion(w, p);
    if (d->spd < 0.8f) {
        float r;
        if (!rnd(w, &r)) return;
        if ((double)r < 0.6) set_smoke(w, &p->pos);
    }
    d->spd *= 0.94f;
    delta.x = p->pos.x - d->pos_b.x; delta.y = p->pos.y - d->pos_b.y; delta.z = p->pos.z - d->pos_b.z;
    dl = scalor(&delta);
    if (dl < 0.33f) {
        d->spd = 0.0f;
        p->flg |= 0x100u;
        if (p->hp < 0) {
            switch (p->mtn_no) {
            case 15: case 22: mtn = 27; break;
            case 14: case 20: mtn = 28; break;
            default: gap(w, "bhEne15 SlidePlayer: _mtnno uninitialisiert (Original liest einen Stapelwert)"); return;
            }
            grab_player(w, p, d, 1, 7, 0.0f, (uint32_t)mtn, 6);
        } else {
            switch (p->mtn_no) {
            case 15: mtn = 17; break;
            case 14: mtn = 16; break;
            case 22: mtn = 23; break;
            case 20: mtn = 24; break;
            default: gap(w, "bhEne15 SlidePlayer: _mtnno uninitialisiert (Original liest einen Stapelwert)"); return;
            }
            st(w)->hold |= 0x1;
            w->mode3 = 2;
            d->spd = 0.0f;
            dyn_set(w, d);
            set_ply_mtn(w, p, (uint32_t)mtn);
        }
    } else {
        dyn_set(w, d);
    }
    if (dl < 0.8f) p->flg |= 0x10u;
}

static void standup_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d)
{
    uint32_t n;
    if (p->mtn_no == 17 || p->mtn_no == 16) p->flg |= 0xC0000u;
    n = player_frames(w, p);
    if (n && (p->frm_no / 65536) == (int)n - 1) {
        if (p->mtn_no == 22) set_ply_mtn(w, p, 23);
        else if (p->mtn_no == 20) set_ply_mtn(w, p, 24);
        else {
            ply_library(w, p, 0);
            p->mode0 = 1; p->mode3 = 0; p->mode2 = 0; p->mode1 = 0;
            p->flg &= ~0x310004u;
            p->flg |= 0x118u;
            p->stflg &= ~0x50480u;
            d->spd = 0.0f;
            dyn_set(w, d);
            st(w)->hold &= ~0x1;
        }
    }
}

static void hold_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d)
{
    RECVX_ENEMY_VEC v, j1;
    if (p->mtn_no == 19) {
        const RECVX_ENEMY_VEC e = { w->px, w->py, w->pz };
        player_ikou(p, &e, 8192);
    } else {
        v.x = p->pos.x - w->px + p->pos.x;
        v.y = p->pos.y - w->py + p->pos.y;
        v.z = p->pos.z - w->pz + p->pos.z;
        player_ikou(p, &v, 8192);
    }
    {
        NJS_VECTOR u;
        u.x = -njSin((Angle)w->ay); u.y = 0; u.z = -njCos((Angle)w->ay);
        njUnitVector(&u);
        u.x *= 2.0f; u.z *= 2.0f;
        if (!joint_pos(w, 1, &j1)) return;
        v.x = u.x + j1.x; v.y = u.y + 0.0f; v.z = u.z + j1.z;
    }
    {   /* en15.c:1998 VacumeToPoint(plp, &v) */
        RECVX_ENEMY_VEC dv;
        dv.x = v.x - p->pos.x; dv.y = v.y - p->pos.y; dv.z = v.z - p->pos.z;
        if (scalor(&dv) > d->spd) {
            NJS_VECTOR u;
            u.x = dv.x; u.y = dv.y; u.z = dv.z;
            njUnitVector(&u);
            p->pos.x += u.x * d->spd; p->pos.y += u.y * d->spd; p->pos.z += u.z * d->spd;
        } else {
            p->pos = v;
        }
    }
}

static void flying_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    RECVX_ENEMY_VEC pos1;
    NJS_MATRIX m;
    if (!joint_point(w, 10, 0.0f, 8.0f, 0.0f, &pos1)) return;
    if (!player_joint(w, 1, &m)) return;
    pos1.x += p->pos.x - m[12];
    pos1.y += p->pos.y - m[13];
    pos1.z += p->pos.z - m[14];
    p->pos = pos1;
}

static void fall_die_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, RECVX_EN15_PLAYER_DYN* d)
{
    switch (p->mode1) {
    case 0:
        ply_library(w, p, 0);
        p->flg &= ~0x310004u;
        p->flg &= ~0x118u;
        p->stflg &= ~0x10480u;
        p->stflg |= 8u;
        d->spd = 0.0f;
        dyn_set(w, d);
        p->mode1++;
        break;
    case 1:
        player_voice(w, 1025);
        vibration(w, 1, 11);
        p->mode1++;
        break;
    case 2:
        st(w)->hold &= ~0x1;
        p->flg |= 2u;
        sys_flags_set(w, 3, 0x4000u, 0);
        break;
    default: break;
    }
}

static void die_player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    uint32_t n;
    if (p->mode1 == 0) { player_voice(w, 1025); p->mode1++; }
    n = player_frames(w, p);
    if (n && (p->frm_no / 65536) == (int)n - 1) {
        st(w)->hold &= ~0x1;
        p->flg |= 2u;
        p->mtn_add = 0;
        sys_flags_set(w, 3, 0x4000u, 0);
    }
}

static void ply_func(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    RECVX_EN15_PLAYER_DYN d;
    if (!player(w, &p)) return;
    if (!dyn_get(w, &d)) return;
    p.flg |= 0x200000u;
    switch (w->mode3) {
    case 0: drive_player(w, &p, &d); break;
    case 1: slide_player(w, &p, &d); break;
    case 2: standup_player(w, &p, &d); break;
    case 3: falling_player(w, &p, &d); break;
    case 4: fall_die_player(w, &p, &d); break;
    case 5: hold_player(w, &p, &d); break;
    case 6: flying_player(w, &p); break;
    case 7: die_player(w, &p); break;
    default: gap(w, "bhEne15 Ply_func[mode3]"); break;
    }
    put_player(w, &p);
}

/* --- bhEne15 (en15.c:362) -------------------------------------------------- */
void recvx_en15(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    if (poison_attack_wait != 0) poison_attack_wait -= 1;
    if (poison_eff_wait != 0) poison_eff_wait -= 1;
    if (w->mode0 == 0) en15_init(w);
    if (!w->exp0) { gap(w, "bhEne15: exp0 fehlt"); return; }
    if (!w->sv || !w->sv->calc_model) { gap(w, "bhCalcModel (en15)"); return; }
    w->sv->calc_model(w);
    if (w->mode0 == 5) {
        if (!w->sv->event) { gap(w, "bhEne_Event"); return; }
        w->sv->event(w);
        set_motion(w, w->mtn_add);
        return;
    }
    if (!(w->flg & 2u) && !(w->mdflg & 1u)) {
        RECVX_ENEMY_VEC c4, c20, c23;
        check_damage(w);
        set_mtn(w);
        lock_leg(w);
        if (w->sv->check_wall) w->sv->check_wall(w);
        else gap(w, "bhCheckWall (en15)");
        set_mtn_se(w);
        switch (w->mode0) {
        case 0: break;
        case 1: en15_move(w); break;
        case 2: en15_throw(w); break;
        case 3: en15_damage(w); break;
        case 4: en15_die(w); break;
        case 5:
            if (!w->sv->event) { gap(w, "bhEne_Event"); return; }
            w->sv->event(w);
            break;
        default: gap(w, "bhEne15 Mode_func[mode0]"); return;
        }
        if (!joint_pos(w, 4, &c4) || !joint_pos(w, 20, &c20) || !joint_pos(w, 23, &c23)) return;
        w->watr.c1 = c4;
        w->watr.c1.y -= 1.0f;
        w->watr.c2.x = (c20.x + c23.x) / 2.0f;
        w->watr.c2.y = (c20.y + c23.y) / 2.0f;
        w->watr.c2.z = (c20.z + c23.z) / 2.0f;
        w->watr.r = 2.5f;
    }
    if (st(w)->hold & 1) ply_func(w);
}
