/* Identitaet 26 — Anatomist Zombie (en26.c:23).
   ===========================================================================
   Nativ eigenstaendig bestimmt. Semantikreferenz: en26.c (Fundstelle an jeder
   Stelle als :<zeile>) und die geteilten Originalhelfer, die en26.c ruft
   (zonzon.c, pwksub.c, en01.c:4078/8008, en01b.c:849). Kein Originalkoerper
   wird kopiert, generiert, kompiliert, gelinkt oder aufgerufen. Zustands-
   besitzer ist allein RECVX_ENEMY_WORK samt dem exp0-Block (en26.h); die
   Dienste sind die vorhandenen des Gegnerbesitzers (RECVX_ENEMY_SERVICES),
   der EN01-Satz (dieselben zonzon.c-/effect.c-Anbieter) und RECVX_EN26_SERVICES.
   Ein nicht gebundener Dienst meldet die Originalroutine ueber gap und der
   betroffene Zweig bricht ab; es wird kein Ersatzwert eingesetzt.

   Die Tabellen dieser Datei sind reine Originaldaten (en26.c:25-559) bzw. die
   lokalen Datentabellen der Routinen (rot_tbl :1119, fire_tbl :800,
   ply_ofs_pos :2717/:2729); Werte woertlich, Zeilen in Originalreihenfolge.
   =========================================================================== */
#include "en26.h"
#include "en01_core.h"
#include "en01_tables.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

/* --- exp0-Lage (en26.h) ------------------------------------------------- */
#define EN26_PP        ((size_t)9 * (sizeof(void*) - 4u))
#define EN26_OFF(f)    ((size_t)offsetof(RECVX_EN26_EXP0, f))
_Static_assert(EN26_OFF(search)          == 0x28 + EN26_PP, "en26 exp0 0x28");
_Static_assert(EN26_OFF(flinch_step)     == 0x2C + EN26_PP, "en26 exp0 0x2C");
_Static_assert(EN26_OFF(eat_wait)        == 0x2F + EN26_PP, "en26 exp0 0x2F");
_Static_assert(EN26_OFF(blood_wait)      == 0x30 + EN26_PP, "en26 exp0 0x30");
_Static_assert(EN26_OFF(fire_mask)       == 0x38 + EN26_PP, "en26 exp0 0x38");
_Static_assert(EN26_OFF(mat_color)       == 0x3C + EN26_PP, "en26 exp0 0x3C");
_Static_assert(EN26_OFF(flags)           == 0x40 + EN26_PP, "en26 exp0 0x40");
_Static_assert(EN26_OFF(flags2)          == 0x44 + EN26_PP, "en26 exp0 0x44");
_Static_assert(EN26_OFF(neck_angle)      == 0x50 + EN26_PP, "en26 exp0 0x50");
_Static_assert(EN26_OFF(player_distance) == 0x54 + EN26_PP, "en26 exp0 0x54");
_Static_assert(EN26_OFF(ikou_target)     == 0x58 + EN26_PP, "en26 exp0 0x58");
_Static_assert(EN26_OFF(grab_point)      == 0x64 + EN26_PP, "en26 exp0 0x64");
_Static_assert(EN26_OFF(line_hit)        == 0x70 + EN26_PP, "en26 exp0 0x70");
_Static_assert(EN26_OFF(head_damage)     == 0x78 + EN26_PP, "en26 exp0 0x78");
_Static_assert(EN26_OFF(overlay_frame)   == 0x80 + EN26_PP, "en26 exp0 0x80");
_Static_assert(EN26_OFF(word88)          == 0x88 + EN26_PP, "en26 exp0 0x88");
_Static_assert(sizeof(RECVX_EN26_EXP0)   >= 0xA0 + EN26_PP, "en26 exp0 160");
/* en01.c:4114/8029/8064 lesen EXP0_I(0x40) an id-26-Works ueber das
   en01-Blockbild: dieselbe Lage ist Pflicht. */
_Static_assert(EN26_OFF(flags) == offsetof(RECVX_EN01_EXP0, flags), "en26/en01 0x40");

/* --- Originaldaten ------------------------------------------------------ */
/* En26_PlyMtn_OffsetTbl[4] (en26.c:25). */
static const int32_t en26_ply_mtn_offset[4] = { 100, 100, 100, 100 };

/* en26_flipTree[21] (:30), 255 als int8 -1. */
static const int8_t en26_flip_tree[21] = {
    0, 1, 6, 7, 8, 9, 2, 3, 4, 5,
    10, 11, 12, 13, 17, 18, 19, 14, 15, 16,
    -1
};

/* en26_mtn_tbl[16] (:37). */
static const RECVX_EN01_MTN en26_mtn_tbl[16] = {
    { 25, { { 8, 0, 10 }, { 7, 11, 43 }, { 8, 44, 59 } } },
    { 16, { { 8, 0, 12 }, { 7, 13, 28 }, { 8, 29, 35 } } },
    { 17, { { 0, 0, 15 }, { 8, 16, 26 }, { 0, 27, 60 } } },
    {  0, { { 1, 0, 69 }, { -1, 0, 0 }, { -1, 0, 0 } } },
    { 15, { { 1, 0, 69 }, { -1, 0, 0 }, { -1, 0, 0 } } },
    {  3, { { 7, 0, 14 }, { 7, 15, 33 }, { -1, 0, 0 } } },
    {  4, { { 1, 0, 16 }, { -1, 0, 0 }, { -1, 0, 0 } } },
    {  5, { { 0, 0, 7 }, { 8, 8, 18 }, { 0, 19, 40 } } },
    { 27, { { 0, 0, 7 }, { 8, 8, 18 }, { 0, 19, 40 } } },
    {  6, { { 1, 0, 11 }, { 0, 12, 17 }, { 1, 18, 39 } } },
    { 28, { { 1, 0, 11 }, { 0, 12, 17 }, { 1, 18, 39 } } },
    {  7, { { 1, 0, 26 }, { -1, 0, 0 }, { -1, 0, 0 } } },
    {  8, { { 1, 0, 37 }, { -1, 0, 0 }, { -1, 0, 0 } } },
    {  1, { { 0, 0, 17 }, { -1, 0, 0 }, { -1, 0, 0 } } },
    { 26, { { 0, 0, 6 }, { 1, 7, 15 }, { 0, 16, 40 } } },
    { -1, { { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 } } }
};

/* en26_mtn_tbl2[18] (:169) — SE je Motion und Frame. */
static const RECVX_EN01_MTN2 en26_mtn_tbl2[18] = {
    { 15, { { 0, 16847639 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 25, { { 10, 70414 }, { 12, 70415 }, { 43, 70414 }, { 45, 70415 } } },
    { 16, { { 12, 70414 }, { 14, 70415 }, { 28, 70414 }, { 30, 70415 } } },
    {  3, { { 16, 8963 }, { 35, 8963 }, { -1, 0 }, { -1, 0 } } },
    {  4, { { 17, 8963 }, { 26, 8963 }, { -1, 0 }, { -1, 0 } } },
    {  5, { { 9, 70414 }, { 11, 70415 }, { 16, 70414 }, { 18, 70415 } } },
    { 27, { { 9, 70414 }, { 11, 70415 }, { 16, 70414 }, { 18, 70415 } } },
    {  6, { { 12, 70414 }, { 14, 70415 }, { 20, 70414 }, { 22, 70415 } } },
    { 28, { { 12, 70414 }, { 14, 70415 }, { 20, 70414 }, { 22, 70415 } } },
    { 13, { { 5, 70414 }, { 16, 70414 }, { -1, 0 }, { -1, 0 } } },
    { 17, { { 9, 70414 }, { 11, 70415 }, { 40, 70415 }, { -1, 0 } } },
    {  2, { { 2, 16782102 }, { 24, 8960 }, { 38, 8960 }, { 60, 8960 } } },
    { 12, { { 24, 8960 }, { 38, 8960 }, { 60, 8960 }, { -1, 0 } } },
    {  1, { { 17, 16786196 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    { 26, { { 5, 70414 }, { 15, 70414 }, { -1, 0 }, { -1, 0 } } },
    {  9, { { 25, 8960 }, { 47, 8960 }, { 66, 8960 }, { -1, 0 } } },
    { 14, { { 8, 8960 }, { 30, 8960 }, { -1, 0 }, { -1, 0 } } },
    { -1, { { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } }
};

/* en26_mtn_tbl3[6] (:335) — Vibration je Motion und Frame. */
static const RECVX_EN01_MTN2 en26_mtn_tbl3[6] = {
    {  2, { { 8, 0 }, { 24, 0 }, { 60, 0 }, { -1, 0 } } },
    { 12, { { 7, 0 }, { 24, 0 }, { 60, 0 }, { -1, 0 } } },
    {  1, { { 13, 1 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } },
    {  9, { { 25, 0 }, { 47, 0 }, { 66, 0 }, { -1, 0 } } },
    { 14, { { 7, 0 }, { 30, 0 }, { -1, 0 }, { -1, 0 } } },
    { -1, { { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 } } }
};

/* en26_tree[8][8] (:393), 255 als int8 -1. */
static const int8_t en26_tree[8][8] = {
    { 0, 1, 6, 7, 8, -1, 0, 0 },
    { 0, 1, 2, 3, 4, -1, 0, 0 },
    { 0, 1, 10, 11, 17, 18, 19, -1 },
    { 0, 1, 10, 11, 14, 15, 16, -1 },
    { 10, 11, 17, 18, 19, -1, 0, 0 },
    { 10, 11, 14, 15, 16, -1, 0, 0 },
    { 0, 1, 6, 7, -1, 0, 0, 0 },
    { 0, 1, 10, 11, 12, -1, 0, 0 }
};

/* EN26_WPNDAMAGE_WORK (en26.h:7-17). */
typedef struct {
    int32_t flg, nm_act, nm_blood, cb_act, cb_blood, dw_act, dw_blood;
} EN26_WPNDAMAGE;

/* En26_WpnDamageTbl[22] (:405). */
static const EN26_WPNDAMAGE en26_wpn_damage[22] = {
    { 0x0000, 0, 0, 0, 0, 0, 0 },
    { 0x0000, 0, 0, 0, 0, 0, 0 },
    { 0x0C02, 0, 3, 2, 3, 5, 3 },
    { 0x0FC2, 0, 7, 2, 3, 3, 3 },
    { 0x0FC2, 0, 7, 2, 3, 3, 3 },
    { 0x0FC2, 0, 7, 2, 3, 3, 3 },
    { 0x0112, 2, 7, 2, 3, 3, 3 },
    { 0x0112, 0, 1, 2, 3, 3, 3 },
    { 0x0EC2, 0, 7, 2, 3, 5, 3 },
    { 0x0FC2, 0, 7, 2, 3, 3, 3 },
    { 0x0FC2, 4, 7, 2, 3, 3, 3 },
    { 0x0039, 2, 9, 2, 9, 3, 9 },
    { 0x0EC2, 0, 7, 2, 3, 5, 3 },
    { 0x011D, 2, 3, 2, 3, 3, 3 },
    { 0x111C, 2, 1, 2, 3, 3, 3 },
    { 0xC100, 0, 1, 2, 3, 3, 3 },
    { 0x511C, 0, 1, 2, 3, 3, 3 },
    { 0x4000, 0, 1, 2, 3, 3, 3 },
    { 0x6000, 2, 1, 2, 3, 3, 3 },
    { 0x111C, 2, 1, 2, 3, 3, 3 },
    { 0x6000, 2, 1, 2, 3, 3, 3 },
    { 0x0FC2, 0, 1, 2, 3, 3, 3 }
};

/* CombWepTbl[21] (:431) / CombJointTbl[20] (:456). */
static const RECVX_EN01_COMBWEP en26_comb_wep[21] = {
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 90, { 10, 0, 0 }, 90, 5 },
    { 25, { 9, 5, 4 }, 60, 0 },
    { 25, { 9, 5, 4 }, 60, 0 },
    { 25, { 9, 5, 4 }, 60, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 25, { 9, 5, 4 }, 60, 0 },
    { 180, { 0, 0, 0 }, 0, 0 },
    { 25, { 9, 5, 4 }, 0, 0 },
    { 40, { 8, 5, 4 }, 40, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 100, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 }
};
static const RECVX_EN01_COMBJOINT en26_comb_joint[20] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }
};

/* en26_fire_tbl[14] (:484). */
static const RECVX_EN01_FIRE en26_fire_tbl[14] = {
    { 11, 2.5f, 1.0f, {  0.5f,  3.0f, -1.0f } },
    { 11, 1.5f, 1.0f, { -0.5f,  1.0f, -1.0f } },
    { 11, 4.0f, 1.0f, {  0.5f,  2.5f,  1.0f } },
    { 11, 2.0f, 1.0f, { -0.5f,  1.0f,  1.0f } },
    { 14, 2.5f, 1.0f, {  0.0f,  0.0f,  0.0f } },
    { 15, 2.0f, 1.0f, {  0.0f,  0.0f,  0.0f } },
    { 17, 2.0f, 1.0f, {  0.5f, -0.5f,  0.0f } },
    { 17, 2.5f, 1.0f, { -1.0f,  0.0f,  0.0f } },
    {  2, 1.5f, 2.0f, {  0.0f, -0.5f,  0.0f } },
    {  3, 2.5f, 1.5f, {  0.0f,  0.0f,  0.0f } },
    {  4, 2.0f, 1.5f, {  0.0f,  0.0f,  0.0f } },
    {  6, 3.0f, 1.0f, {  0.0f, -3.0f, -1.0f } },
    {  8, 2.5f, 1.5f, {  0.0f,  0.0f,  0.0f } },
    {  9, 3.0f, 1.0f, {  0.0f, -2.0f,  0.5f } }
};

/* Ene26CapColTab[17] (:502). */
static const RECVX_ENEMY_CPCL en26_cap_col[17] = {
    {  1, 11, 16 }, { 11, 11, 18 }, {  0, 16,  0 }, { 11, 11, 18 },
    {  0, 30,  0 }, { 12, 13,  6 }, { 13, 13, 11 }, {  0,  9, -2 },
    { 14, 15,  6 }, { 15, 16,  5 }, { 17, 18,  6 }, { 18, 19,  5 },
    {  2,  3,  9 }, {  3,  4,  8 }, {  6,  7,  9 }, {  7,  8,  8 },
    {  0,  0,  0 }
};

/* En26SdwTab[5] (:523), 255 als int8 -1. */
static const int8_t en26_sdw_tab[5] = { 0, 3, 4, 12, -1 };

/* en26prt_blood_tbl[20] (:527). */
static const RECVX_EN01_BT en26_prt_blood_tbl[20] = {
    {  0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 11, 0.0f, 2.0f, 1.8f, 2.0f, 2.0f, 5.0f, 1.5f },
    {  2, 0.0f, 0.0f, 1.8f, 2.0f, 2.0f, 5.0f, 1.5f },
    {  3, 0.0f, 2.0f, 1.0f, 1.0f, 2.0f, 5.0f, 1.5f },
    {  4, 0.0f, 2.0f, 1.0f, 1.0f, 2.0f, 5.0f, 1.5f },
    {  3, 0.0f, 2.0f, 1.0f, 1.0f, 2.0f, 5.0f, 1.5f },
    {  6, 0.0f, 0.0f, 1.8f, 2.0f, 2.0f, 5.0f, 1.5f },
    {  7, 0.0f, 2.0f, 1.0f, 1.0f, 2.0f, 5.0f, 1.5f },
    {  8, 0.0f, 2.0f, 1.0f, 1.0f, 2.0f, 5.0f, 1.5f },
    {  7, 0.0f, 2.0f, 1.0f, 1.0f, 2.0f, 5.0f, 1.5f },
    {  0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 11, 0.0f, 3.0f, 1.8f, 2.0f, 2.0f, 5.0f, 1.5f },
    { 11, 0.0f, 3.5f, 1.8f, 2.0f, 1.0f, 5.0f, 1.5f },
    { 11, 0.0f, 3.5f, 1.8f, 2.0f, 1.0f, 5.0f, 1.5f },
    { 14, 0.0f, 0.0f, 0.5f, 0.2f, 0.5f, 5.0f, 1.5f },
    { 15, 0.0f, 1.0f, 0.3f, 0.2f, 1.0f, 5.0f, 1.5f },
    { 16, 0.0f, 1.0f, 0.3f, 0.2f, 1.0f, 5.0f, 1.5f },
    { 17, 0.0f, 0.0f, 0.5f, 0.2f, 0.5f, 5.0f, 1.5f },
    { 18, 0.0f, 1.0f, 0.3f, 0.2f, 1.0f, 5.0f, 1.5f },
    { 19, 0.0f, 1.0f, 0.3f, 0.2f, 1.0f, 5.0f, 1.5f }
};

/* en26_keff_tbl[5] (:552). */
static const RECVX_EN01_KAMI en26_keff_tbl[5] = {
    {  2, { 28, 38, 62, -1 } },
    { 12, { 24, 38, 60, -1 } },
    {  9, { 25, 47, 66, -1 } },
    { 14, {  9, 32, -1, -1 } },
    { -1, { -1, -1, -1, -1 } }
};

/* NJM_DEG_ANG (macros.h:104) und njArcTan2 (NJM_RAD_ANG(atan2f), macros.h:102
   mit PI_2 = 3.141592f * 2, macros.h:4/5). */
#define EN26_PI          3.141592f
#define EN26_DEG_ANG(n)  ((int32_t)(((n) * 65536.0f) / 360.0f))
#define EN26_ARC_TAN2(y, x) ((int32_t)(atan2f((float)(y), (float)(x)) * (65536.0f / (EN26_PI * 2.0f))))

/* --- Dienste ------------------------------------------------------------- */
static const RECVX_EN26_SERVICES* en26_extra;

void recvx_en26_bind(const RECVX_EN26_SERVICES* services) { en26_extra = services; }
const RECVX_EN26_SERVICES* recvx_en26_bound(void) { return en26_extra; }

RECVX_EN26_EXP0* recvx_en26_exp0(RECVX_ENEMY_WORK* w)
{
    return w ? (RECVX_EN26_EXP0*)w->exp0 : NULL;
}

static void en26_gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}

static const RECVX_EN01_SERVICES* en26_en(void)
{
    const RECVX_EN01_CONTEXT* c = recvx_en01_context();
    return c ? c->en01 : NULL;
}

static int32_t en26_rand(const RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->random_next) return w->sv->random_next();
    en26_gap(w, "rand (en26)");
    return 0;
}

static int en26_player(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl, const char* where)
{
    if (w->sv && w->sv->player_get && w->sv->player_get(pl)) return 1;
    en26_gap(w, where);
    return 0;
}

static void en26_player_set(const RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* pl)
{
    if (w->sv && w->sv->player_set) w->sv->player_set(pl);
    else en26_gap(w, "plp (en26 Schreibzugriff)");
}

static int en26_joint_mtx(const RECVX_ENEMY_WORK* w, uint32_t joint, float* out16)
{
    if (w->sv && w->sv->joint_matrix && w->sv->joint_matrix(w, joint, out16)) return 1;
    en26_gap(w, "mlwP->owP[joint].mtx (en26)");
    return 0;
}

/* owk->mtx[12..14] als NJS_POINT3 (en26.c:1998 u. a. `&owk->mtx[12]`). */
static int en26_joint_pos(const RECVX_ENEMY_WORK* w, uint32_t joint, RECVX_ENEMY_VEC* out)
{
    float m[16];
    if (!en26_joint_mtx(w, joint, m)) return 0;
    out->x = m[12]; out->y = m[13]; out->z = m[14];
    return 1;
}

/* plp->mlwP->owP[joint].mtx[12..14]. */
static int en26_player_joint_pos(const RECVX_ENEMY_WORK* w, uint32_t joint, RECVX_ENEMY_VEC* out)
{
    float m[16];
    if (w->sv && w->sv->player_joint_matrix && w->sv->player_joint_matrix(joint, m)) {
        out->x = m[12]; out->y = m[13]; out->z = m[14];
        return 1;
    }
    en26_gap(w, "plp->mlwP->owP[joint].mtx (en26)");
    return 0;
}

static int en26_set_motion(RECVX_ENEMY_WORK* w, int add)
{
    if (w->sv && w->sv->set_motion) return w->sv->set_motion(w, add, w->mtn_md);
    en26_gap(w, "bhSetMotion (en26)");
    return 0;
}

static uint32_t en26_motion_frames(const RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    if (w->sv && w->sv->motion_frames) return w->sv->motion_frames(w, mtn);
    en26_gap(w, "mnwP[mtn_no].frm_num (en26)");
    return 0u;
}

static void en26_calc_model(RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->calc_model) { w->sv->calc_model(w); return; }
    en26_gap(w, "bhCalcModel (en26)");
}

static void en26_node_angle_set(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t axis, int32_t ang)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->node_angle_set) { en->node_angle_set(w, node, axis, ang); return; }
    en26_gap(w, "mlwP->objP[n].ang (en26)");
}

static int en26_node_angle_get(const RECVX_ENEMY_WORK* w, uint32_t node, uint32_t axis, int32_t* out)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->node_angle_get) { *out = en->node_angle_get(w, node, axis); return 1; }
    en26_gap(w, "mlwP->objP[n].ang (en26)");
    return 0;
}

static void en26_set_material_color(RECVX_ENEMY_WORK* w, uint32_t color)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->set_material_color) { en->set_material_color(w, color); return; }
    en26_gap(w, "npSetAllMatColor (en26)");
}

/* bhSearchPlayer2 (en01b.c:922) ueber den Dienst; 0 = kein Spielerbesitzer,
   dann wird kein -1 angenommen. */
static int en26_search_player2(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                               int32_t ay, int32_t r, int32_t* out)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->search_player && en->search_player(w, pos, ay, r, out)) return 1;
    en26_gap(w, "bhSearchPlayer2 (en01b.c:922)");
    return 0;
}

/* bhEne_PlayerSePlay (zonzon.c:1367): CallPlayerVoice bei !(flg & 0x10000). */
static void en26_player_se_play(RECVX_ENEMY_WORK* w, int no)
{
    if (w->flg & 0x10000u) return;
    if (w->sv && w->sv->player_voice) w->sv->player_voice(no);
    else en26_gap(w, "CallPlayerVoice (zonzon.c:1371)");
}

static void en26_clear_pad_on(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->pad_on_clear) en->pad_on_clear(0xFu);
    else en26_gap(w, "sys->pad_on &= ~0xF (en26)");
}

static int en26_player_way(RECVX_ENEMY_WORK* w, int16_t* out)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->player_way && en->player_way(out)) return 1;
    en26_gap(w, "plp->way (en26)");
    return 0;
}

static void en26_set_blood2(RECVX_ENEMY_WORK* w, uint8_t type, const RECVX_ENEMY_VEC* ofp, int16_t ry)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->set_blood2) { en->set_blood2(w, type, ofp, ry); return; }
    en26_gap(w, "bhEne_SetBlood2 (zonzon.c:466)");
}

static void en26_set_blood4(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs,
                            int tex_id, int type)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    if (en && en->set_blood4) { en->set_blood4(w, lnk_obj, ofs, tex_id, type); return; }
    en26_gap(w, "bhEne_SetBlood4 (zonzon.c:683)");
}

static void en26_mtn_flags_clear(RECVX_EN26_EXP0* p)
{
    p->flags &= ~0x3000000u;
}

static int en26_mode2_low(const RECVX_ENEMY_WORK* w)
{
    return w->mode2 == 0 || w->mode2 == 1 || w->mode2 == 2;
}

/* bhEne26_SePlay (en26.c:3434). */
static void en26_se_play(RECVX_ENEMY_WORK* w, int no)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos;
    uint8_t low = (uint8_t)no;

    if (w->flg & 0x10000u) return;                                   /* :3436 */
    if (!sv || !sv->enemy_se || !sv->enemy_now) { en26_gap(w, "RequestEnemySe (en26.c:3449)"); return; }

    if (low >= 20u && (low == 20u || low == 21u)) {                  /* :3441 */
        int playing;
        if (!sv->enemy_se_playing) { en26_gap(w, "ChechPlayEnemySe (en26.c:3443)"); return; }
        playing = sv->enemy_se_playing(sv->enemy_now(), no);
        if (playing || !(en26_rand(w) % 2)) return;                  /* :3443-3446 */
    }
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    sv->enemy_se(sv->enemy_now(), &pos, no);                         /* :3449 */
}

/* --- Vorwaertsdeklarationen -------------------------------------------- */
static void en26_main_loop(RECVX_ENEMY_WORK* w);
static void en26_player_control(RECVX_ENEMY_WORK* w);
static void en26_search_neck(RECVX_ENEMY_WORK* w);
static void en26_coll_check(RECVX_ENEMY_WORK* w);
static void en26_coll_check_wall(RECVX_ENEMY_WORK* w);
static void en26_player_link(RECVX_ENEMY_WORK* w);
static void en26_calc_enemy(RECVX_ENEMY_WORK* w);
static void en26_check_mtn_tbl(RECVX_ENEMY_WORK* w, int32_t frm);
static void en26_damage_add(RECVX_ENEMY_WORK* w);
static void en26_link_fire_effect(RECVX_ENEMY_WORK* w, int type);
static void en26_dmg_check_type00(RECVX_ENEMY_WORK* w);
static int  en26_check_exp_head(RECVX_ENEMY_WORK* w);
static void en26_set_blood(RECVX_ENEMY_WORK* w);
static void en26_move(RECVX_ENEMY_WORK* w);
static void en26_nage(RECVX_ENEMY_WORK* w);
static void en26_damage(RECVX_ENEMY_WORK* w);
static void en26_die(RECVX_ENEMY_WORK* w);
static void en26_ply_dg00(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl);
static int  en26_eat_check(RECVX_ENEMY_WORK* w, int rng, float dist, int mode);
static void en26_exp_head_effect(RECVX_ENEMY_WORK* w);
static void en26_neck_blood_effect(RECVX_ENEMY_WORK* w, int type);
static void en26_kami_effect(RECVX_ENEMY_WORK* w, int32_t frm);

/* ===========================================================================
   bhEne26 (en26.c:628)
   =========================================================================== */
void recvx_en26(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p;

    if (!w) return;
    recvx_en26_dmg_check(w);                                         /* :630 */
    en26_main_loop(w);                                               /* :631 */

    if (w->flg & 4u) w->flg &= ~4u;                                  /* :633-636 */

    p = recvx_en26_exp0(w);
    if (!p) { en26_gap(w, "bhEne_CallocWork(160, 8) (en26.c:1803)"); return; }
    if (p->eat_wait > 0u) p->eat_wait--;                             /* :638-641 */
    if (p->blood_wait > 0) p->blood_wait--;                          /* :643-646 */
    if (p->san_wait > 0) p->san_wait--;                              /* :648-651 */

    en26_coll_check(w);                                              /* :652 */

    if (p->flags & 0x80000u) en26_player_link(w);                    /* :654-657 */

    en26_calc_enemy(w);                                              /* :659 */
}

/* --- bhEne26_DmgCheck (en26.c:663) ------------------------------------- */
void recvx_en26_dmg_check(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    RECVX_EN26_EXP0* p;
    int32_t ang;

    if (!w) return;
    if (!(w->flg & 0x4u) || (w->flg & 0x2u)) return;                /* :667 */

    if (!en || !en->calc_damage) { en26_gap(w, "bhEne_CalcDamage (zonzon1.c:725)"); return; }
    en->calc_damage(w, en26_comb_wep, en26_comb_joint);              /* :669 */

    if (w->total_dam == 0) return;                                   /* :671 */

    p = recvx_en26_exp0(w);
    if (!p) { en26_gap(w, "bhEne26_DmgCheck: exp0"); return; }

    if (p->flags2 & 0x40u) {                                         /* :673 */
        RECVX_ENEMY_PLAYER pl;
        uint32_t wflg = 0;
        if (!en26_player(w, &pl, "plp (en26.c:674)")) return;
        if (w->flr_no <= pl.flr_no) {
            if (!en->weapon_flg || !en->weapon_flg((uint32_t)w->wpnr_no, &wflg)) {
                en26_gap(w, "WpnTab[wpnr_no].flg (en26.c:675)");
                return;
            }
            if (!(wflg & 0x20u) && !(wflg & 0x20000000u) && (pl.at_flg & 0x2u)) {
                return;                                              /* :679 */
            }
        }
    }

    en26_damage_add(w);                                              /* :682 */

    if ((w->mode0 >= 3) || (p->flags & 0x80u) ||
        ((p->flags & 0x40000u) && w->mode0 == 1 && w->mode3 == 0)) { /* :684-686 */
        return;                                                      /* :688 */
    }

    p->flags |= 0x200400u;                                           /* :691 */
    p->counter48 = 0;                                                /* :692 */

    if (w->comb_flg & 4u) p->flags |= 0x2000u;                       /* :694-701 */
    else                  p->flags &= ~0x2000u;

    ang = EN26_ARC_TAN2(w->dvx, w->dvz);                             /* :703 */
    if ((uint16_t)(ang - w->ay) <= (uint16_t)EN26_DEG_ANG(180.0f)) { /* :705 */
        p->flags2 |= 0x20u;                                          /* :707 */
    } else {
        p->flags2 &= ~0x20u;                                         /* :711 */
    }

    en26_dmg_check_type00(w);                                        /* :714 */

    if (p->head_damage != -1) {                                      /* :716 */
        en26_se_play(w, (w->hp < 0) ? 16786197 : 16786196);          /* :720/:724 */
    }
    en26_set_blood(w);                                               /* :727 */
}

/* --- bhEne26_DamageAdd (en26.c:732) ------------------------------------ */
static void en26_damage_add(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();
    const EN26_WPNDAMAGE* wp;
    int low_flg = 0;
    uint32_t n, i;

    if (w->hp >= 0) w->hp -= w->total_dam;                           /* :739-742 */

    if (w->comb_wep >= 22u) { en26_gap(w, "En26_WpnDamageTbl[comb_wep] ausserhalb (en26.c:744)"); return; }
    wp = &en26_wpn_damage[w->comb_wep];                              /* :744 */

    if (!w->sv || !w->sv->obj_num) { en26_gap(w, "mlwP->obj_num (en26.c:746)"); return; }
    n = w->sv->obj_num(w);
    if (n > RECVX_ENEMY_DAM) n = RECVX_ENEMY_DAM;
    for (i = 1u; i < n; i++) {                                       /* :746 */
        if (w->dam[i] > 0) {                                         /* :748 */
            /* Das Original prueft hier djnt_no, nicht den Laufindex
               (en26.c:750/755); bhEne_CalcDamage setzt djnt_no nicht. */
            if (w->djnt_no == 3 || w->djnt_no == 4 ||
                w->djnt_no == 7 || w->djnt_no == 8) {               /* :750-751 */
                low_flg = 1;                                         /* :753 */
            }
            if (w->djnt_no == 13) p->head_damage += w->dam[i];       /* :755-758 */
        }
    }

    if ((wp->flg & 0x8000) && p->san_wait <= 0) {                    /* :762 */
        if (en && en->san_effect) en->san_effect(w, (int)w->djnt_no, en26_prt_blood_tbl); /* :764 */
        else en26_gap(w, "bhEne_SetSanEffect (en26.c:764)");
        p->san_wait = 10;                                            /* :765 */
    }

    if (wp->flg & 0x1000) {                                          /* :768 */
        if (low_flg == 0) en26_link_fire_effect(w, (en26_rand(w) % 4) + 4);  /* :772 */
        else              en26_link_fire_effect(w, (en26_rand(w) % 4) + 8);  /* :776 */
        p->flags |= 0x10000u;                                        /* :778 */
    } else if (wp->flg & 0x2000) {                                   /* :780 */
        en26_link_fire_effect(w, (en26_rand(w) % 4) + 4);            /* :782 */
        p->flags |= 0x10000u;                                        /* :783 */
        p->mat_color = 0xFF000000u;                                  /* :784 */
        w->mdflg |= 0x400u;                                          /* :785 */
        en26_set_material_color(w, p->mat_color);                    /* :786 */
    }
}

/* --- bhEne26_LinkFireEffect (en26.c:791) -------------------------------- */
static void en26_link_fire_effect(RECVX_ENEMY_WORK* w, int type)
{
    /* fire_tbl (en26.c:800-814), lokale Datentabelle; 255 terminiert. */
    static const uint8_t fire_tbl[12][8] = {
        {  0,  1,  2,  3,  4,  6,   8, 255 },
        {  0,  1,  2,  3,  5,  6,   9, 255 },
        {  0,  1,  2,  3,  4,  7,  11, 255 },
        {  0,  1,  2,  3,  5,  7,  12, 255 },
        {  0,  1,  2,  3,  4,  6, 255, 255 },
        {  0,  1,  2,  3,  5,  6, 255, 255 },
        {  0,  1,  2,  3,  4,  7, 255, 255 },
        {  0,  1,  2,  3,  5,  7, 255, 255 },
        { 11, 10, 13, 255, 255, 255, 255, 255 },
        {  8, 10, 13, 255, 255, 255, 255, 255 },
        {  9, 10, 13, 255, 255, 255, 255, 255 },
        { 10, 12, 13, 255, 255, 255, 255, 255 }
    };
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN26_SERVICES* x = en26_extra;
    RECVX_EN26_EF ef;
    int i;

    if (type < 0 || type >= 12) { en26_gap(w, "bhEne26_LinkFireEffect: type ausserhalb (en26.c:825)"); return; }
    if (!x || !x->effect_tb || !x->effect_zn_ct3) { en26_gap(w, "bhSetEffectTb (en26.c:837)"); return; }

    memset(&ef, 0, sizeof(ef));
    ef.id = 352;                                                     /* :816 */
    ef.type = 0;                                                     /* :817 */
    ef.flg = 1u;                                                     /* :818 */
    ef.sz = 0.0f;                                                    /* :819 */
    ef.px = 0.0f; ef.py = 0.0f; ef.pz = 0.0f;                        /* :820-822 */
    ef.ay = 0;                                                       /* :823 */

    for (i = 0; i < 8; i++) {                                        /* :825 */
        const RECVX_EN01_FIRE* fp;
        uint8_t idx = fire_tbl[type][i];
        uint32_t flg;
        int32_t time;
        int eno;

        /* Das Original bildet eff_p vor der -1-Pruefung (:827/:828); bei 255
           wird die Adresse nicht benutzt. */
        if (idx == 255u) continue;                                   /* :828 */
        fp = &en26_fire_tbl[idx];
        flg = 1u << idx;                                             /* :830 */
        if (p->fire_mask & (int32_t)flg) continue;                   /* :831 */
        p->fire_mask |= (int32_t)flg;                                /* :833 */
        time = en26_rand(w) % 5;                                     /* :834 */
        ef.sy = ef.sx = fp->size;                                    /* :835 */
        ef.type = 0;                                                 /* :836 */
        eno = x->effect_tb(&ef, &fp->ofs, w, fp->obj);               /* :837 */
        if (eno != -1) x->effect_zn_ct3(eno, fp->len, time);          /* :838-842 */
        ef.type = 1;                                                 /* :843 */
        eno = x->effect_tb(&ef, &fp->ofs, w, fp->obj);               /* :844 */
        if (eno != -1) x->effect_zn_ct3(eno, fp->len, time);          /* :845-849 */
    }
}

/* --- bhEne26_DmgCheckType00 (en26.c:856) ------------------------------- */
static void en26_dmg_check_type00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const EN26_WPNDAMAGE* wp;
    int32_t act;

    if (w->wpnr_no < 0 || w->wpnr_no >= 22) { en26_gap(w, "En26_WpnDamageTbl[wpnr_no] ausserhalb (en26.c:863)"); return; }
    wp = &en26_wpn_damage[w->wpnr_no];                               /* :863 */
    act = wp->nm_act;                                                /* :864 */
    if ((wp->flg & 0x10) && !(p->flags & 0x440000u) &&
        en26_check_exp_head(w) != 0) {                               /* :865 */
        w->hp = -1;                                                  /* :867 */
        en26_exp_head_effect(w);                                     /* :868 */
        en26_neck_blood_effect(w, 0);                                /* :869 */
        p->head_damage = -1;                                         /* :870 */
        en26_se_play(w, 4869);                                       /* :871 */
    }

    if (w->hp < 0) {                                                 /* :874 */
        w->mode0 = 3; w->mode1 = 0; w->mode3 = 0;                    /* :876-878 */
        act = wp->dw_act;                                            /* :879 */
    } else if (w->comb_flg & 1u) {                                   /* :881 */
        w->comb_pnt = 0;                                             /* :883 */
        w->mode0 = 3; w->mode1 = 0; w->mode3 = 0;                    /* :884-886 */
        act = wp->cb_act;                                            /* :887 */
    }

    if (act == 0) {                                                  /* :890 */
        if (!(p->flags & 0x8000000u)) {                              /* :892 */
            p->flags |= 0x8000000u;                                  /* :894 */
            p->flinch_step = 0;                                      /* :895 */
        }
    } else if (act == 1) {                                           /* :898 */
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;      /* :900-903 */
    } else if (act == 2) {                                           /* :905 */
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 2; w->mode3 = 0;      /* :907-910 */
    } else if (act == 5) {                                           /* :912 */
        w->mode0 = 3; w->mode1 = 0; w->mode2 = 3; w->mode3 = 0;      /* :914-917 */
    } else if (act == 3) {                                           /* :919 */
        w->mode0 = 3; w->mode1 = 0; w->mode3 = 0;                    /* :921-923 */
        if ((wp->flg & 0x100) && w->hp > 0) {                        /* :925 */
            if (en26_rand(w) % 10 >= 3) {                            /* :927 */
                w->mode2 = 2;                                        /* :929 */
                return;                                              /* :930 */
            }
        }
        if (w->comb_flg & 4u) {                                      /* :934 */
            if (wp->flg & 0x200) w->mode2 = (en26_rand(w) % 3 == 0) ? 3 : 4; /* :936-939 */
            else                 w->mode2 = 4;                       /* :942 */
        } else {
            w->mode2 = 3;                                            /* :947 */
        }
    }
}

/* --- bhEne26_CheckExpHead (en26.c:954) --------------------------------- */
static int en26_check_exp_head(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;

    if (p->head_damage > 0) return 1;                                /* :956-959 */

    switch (w->wpnr_no) {                                            /* :961 */
    case 11:
    case 14:
        if (!en26_player(w, &pl, "plp (en26.c:965)")) return 0;
        if (w->flr_no == pl.flr_no && p->player_distance < 10.0f && (pl.at_flg & 4u)) return 1; /* :965 */
        break;
    case 6:
        if (!en26_player(w, &pl, "plp (en26.c:972)")) return 0;
        if (w->flr_no == pl.flr_no && (w->comb_flg & 0x10u) && (pl.at_flg & 6u) &&
            (w->djnt_no == 1 || w->djnt_no == 10 || w->djnt_no == 11 ||
             w->djnt_no == 12 || w->djnt_no == 13)) return 1;         /* :972-975 */
        break;
    case 19:
        if ((w->djnt_no >= 10 && w->djnt_no <= 19) || w->djnt_no == 1) return 1; /* :982 */
        break;
    default:
        break;
    }
    return 0;                                                        /* :987 */
}

/* --- bhEne26_SetBlood (en26.c:991) ------------------------------------- */
static void en26_set_blood(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();
    const EN26_WPNDAMAGE* wp;
    /* `ofp` ist im Original ohne Startwert (en26.c:994); der Zweig
       nm_blood == 9 (:1055) uebergibt ihn an bhEne_SetBlood2, bevor er dort
       beschrieben wird, wenn djnt_no nicht 11/1/10 war. Wie im EN01-Port
       (en01_core.c recvx_en01_set_blood) startet er bei 0: der Stackwert des
       Originals ist nicht bestimmbar (Originaldefekt). */
    RECVX_ENEMY_VEC ofp = { 0.0f, 0.0f, 0.0f };
    int obj_no;
    int16_t way;

    if (w->wpnr_no < 0 || w->wpnr_no >= 22) { en26_gap(w, "En26_WpnDamageTbl[wpnr_no] ausserhalb (en26.c:1000)"); return; }
    wp = &en26_wpn_damage[w->wpnr_no];                               /* :1000 */

    if (!(wp->flg & 0x4000)) {                                       /* :1001 */
        if (w->djnt_no == 11 || w->djnt_no == 1 || w->djnt_no == 10) { /* :1003 */
            ofp.x = -w->dvx; ofp.y = -w->dvy; ofp.z = -w->dvz;        /* :1005-1007 */
            if (en && en->link_blood) en->link_blood(w, w->djnt_no, &ofp, 0.5f); /* :1008 */
            else en26_gap(w, "bhLinkBlood (effect.c:736)");
        }

        if ((w->comb_flg & 1u) || w->hp < 0) {                       /* :1011 */
            if (p->blood_wait <= 0 && p->head_damage != -1) {        /* :1013 */
                p->blood_wait = 20;                                  /* :1015 */
                if (w->djnt_no > 1 && w->djnt_no < 10) {             /* :1017 */
                    ofp.x = ofp.z = 0.0f; ofp.y = -2.0f; obj_no = 1; /* :1019-1021 */
                } else {
                    ofp.x = ofp.z = 0.0f; ofp.y = 2.5f; obj_no = 11; /* :1025-1027 */
                }
                if (wp->cb_blood == 9) {                             /* :1030 */
                    en26_set_blood4(w, obj_no, &ofp, 8, (w->comb_flg & 0x40u) ? 3 : 1); /* :1034/:1038 */
                    ofp.x = ofp.z = 0.0f; ofp.y = 2.5f;              /* :1041-1042 */
                    if (en26_player_way(w, &way)) en26_set_blood2(w, 9, &ofp, way); /* :1043 */
                } else {
                    en26_set_blood4(w, obj_no, &ofp, 8, 0);          /* :1047 */
                }
            }
        } else {
            if (wp->nm_blood == 9) {                                 /* :1053 */
                if (en26_player_way(w, &way)) en26_set_blood2(w, 9, &ofp, way); /* :1055 */
                if (w->djnt_no > 1 && w->djnt_no < 10) {             /* :1057 */
                    ofp.x = ofp.z = 0.0f; ofp.y = -2.0f; obj_no = 1; /* :1059-1061 */
                } else {
                    ofp.x = ofp.z = 0.0f; ofp.y = 2.5f; obj_no = 11; /* :1065-1067 */
                }
                en26_set_blood4(w, obj_no, &ofp, 8, (w->comb_flg & 0x40u) ? 3 : 0); /* :1072/:1076 */
            } else {
                if (en && en->set_blood) en->set_blood(w, (uint8_t)wp->nm_blood, en26_prt_blood_tbl); /* :1081 */
                else en26_gap(w, "bhEne_SetBlood (zonzon.c:415)");
            }
        }
    }
    if ((wp->flg & 1) && !(w->comb_flg & 0x40u) && p->head_damage != -1 && w->hp < 0) { /* :1085 */
        RECVX_ENEMY_VEC pos;
        if (en26_joint_pos(w, (uint32_t)w->djnt_no, &pos)) {          /* :1087-1090 */
            if (en && en->nikuhen_effect2) en->nikuhen_effect2(w, 0, &pos, 4, 7); /* :1091 */
            else en26_gap(w, "bhEne_SetNikuhenEffect2 (zonzon.c:803)");
        }
    }
}

/* --- bhEne26_MainLoop (en26.c:1096) ------------------------------------ */
static void en26_main_loop(RECVX_ENEMY_WORK* w)
{
    if (w->mode0 != 6) {                                             /* :1098 */
        switch (w->mode0) {                                          /* bhEne26_Mode0 :561 */
        case 0: recvx_en26_init(w); break;
        case 1: en26_move(w);       break;
        case 2: en26_nage(w);       break;
        case 3: en26_damage(w);     break;
        case 4: en26_die(w);        break;
        case 5:
            if (w->sv && w->sv->event) w->sv->event(w);              /* bhEne_Event */
            else en26_gap(w, "bhEne_Event (subpl.c:143)");
            break;
        default:
            /* mode0 7 (Common_controll Selektor 0, event.c:11474) liest im
               Original hinter bhEne26_Mode0[6]. */
            en26_gap(w, "bhEne26_Mode0[mode0] ausserhalb (en26.c:1100)");
            break;
        }
    }
    if (!w->exp0) return;   /* gemeldeter Allokationsfehler, kein Ersatzblock */
    en26_player_control(w);                                          /* :1102 */
    recvx_en26_set_mtn(w);                                           /* :1103 */
    en26_search_neck(w);                                             /* :1104 */
}

/* --- bhEne26_PlayerControl (en26.c:1108) ------------------------------- */
static void en26_player_control(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;

    if (!(p->flags & 0x20000u)) return;                              /* :1110 */
    if (!en26_player(w, &pl, "plp (en26.c:1110)")) return;
    if (pl.mode0 != 4u && pl.mode0 != 6u) return;
    if (pl.mode2 != 0u) {                                            /* bhEne26_PlyDmgMode[1] :622 */
        en26_gap(w, "bhEne26_PlyDmgMode[plp->mode2] ausserhalb (en26.c:1112)");
        return;
    }
    en26_ply_dg00(w, &pl);                                           /* :1112 */
    en26_player_set(w, &pl);
}

/* --- bhEne26_SetMtn (en26.c:1117) -------------------------------------- */
int recvx_en26_set_mtn(RECVX_ENEMY_WORK* w)
{
    /* rot_tbl[4][8] (en26.c:1119), lokale Datentabelle. */
    static const int32_t rot_tbl[4][8] = {
        {  0x800,  0xA00,  0x1000,  0xE00,  0xC00,  0x800,  0x400,  0x200 },
        { -0x800, -0xA00, -0x1000, -0xE00, -0xC00, -0x800, -0x400, -0x200 },
        {  0x800,  0xA00,  0x1000,  0xE00,  0xC00,  0x800,  0x400,  0x200 },
        { -0x800, -0xA00, -0x1000, -0xE00, -0xC00, -0x800, -0x400, -0x200 }
    };
    RECVX_EN26_EXP0* p;
    const RECVX_EN01_SERVICES* en = en26_en();
    int ret;
    int32_t frm;

    if (!w) return 0;
    p = recvx_en26_exp0(w);
    if (!p) { en26_gap(w, "bhEne26_SetMtn: exp0"); return 0; }

    if (p->flags & 0x10u) return 0;                                  /* :1132-1135 */

    frm = w->frm_no / 65536;                                         /* :1137 */
    if (p->flags & 0x8000000u) {                                     /* :1138 */
        uint32_t c = p->flinch_step;
        int32_t a0, a1;
        if (c >= 8u) { en26_gap(w, "rot_tbl[.][EXP0_UC(0x2C)] ausserhalb (en26.c:1143)"); c = 0u; }
        if (p->flags & 0x2000u) {                                    /* :1141 */
            a0 = rot_tbl[0][c];                                      /* :1143 */
            a1 = (p->flags2 & 0x20u) ? rot_tbl[2][c] : rot_tbl[3][c];/* :1146/:1150 */
        } else {
            a0 = rot_tbl[1][c];                                      /* :1155 */
            a1 = (p->flags2 & 0x20u) ? rot_tbl[3][c] : rot_tbl[2][c];/* :1158/:1162 */
        }
        en26_node_angle_set(w, 10u, 0u, a0);                         /* objP[10].ang[0] */
        en26_node_angle_set(w, 10u, 1u, a1);                         /* objP[10].ang[1] */
        p->flinch_step++;                                            /* :1166 */
        if (p->flinch_step >= 8u) {                                  /* :1167 */
            p->flags &= ~0x8000000u;                                 /* :1169 */
            p->flinch_step = 0;                                      /* :1170 */
        }
    }

    ret = en26_set_motion(w, (int)w->mtn_add);                       /* :1174 */

    if ((p->flags2 & 0x80u) && w->mtn_no == 25u) {                   /* :1176 */
        /* Lauf-Overlay: MTN 23 auf dem Oberkoerper ab objP[10] mit eigener
           Uhr in EXP0_I(0x80) (en26.c:1178-1192). */
        const RECVX_EN26_SERVICES* x = en26_extra;
        uint32_t h_count = w->hokan_count;                           /* :1178 */
        int32_t frm_no = w->frm_no;                                  /* :1179 */
        if (!x || !x->shift_object_window || !x->shift_object_window(w, 10)) { /* :1180-1182 */
            en26_gap(w, "mlwP->objP += 10 (en26.c:1180)");
        } else {
            w->mtn_no = 23u;                                         /* :1183 */
            w->frm_no = p->overlay_frame;                            /* :1184 */
            ret = en26_set_motion(w, 65536);                         /* :1185 */
            p->overlay_frame = w->frm_no;                            /* :1186 */
            w->mtn_no = 25u;                                         /* :1187 */
            (void)x->shift_object_window(w, -10);                    /* :1188-1190 */
            w->hokan_count = h_count;                                /* :1191 */
            w->frm_no = frm_no;                                      /* :1192 */
        }
    }

    if (p->flags & 0x1000000u) {                                     /* :1195 */
        if (en && en->node_local_clear_xz) en->node_local_clear_xz(w, 0u); /* :1197-1198 */
        else en26_gap(w, "mlwP->objP[0].pos[0]/[2] = 0 (en26.c:1198)");
        if (en && en->translate_motion) en->translate_motion(w, 1u, frm);  /* :1199 */
        else en26_gap(w, "bhEne_GetTranslateMtn (zonzon.c:209)");
    } else if (p->flags & 0x2000000u) {                              /* :1201 */
        if (en && en->node_local_clear_xz) en->node_local_clear_xz(w, 0u); /* :1203-1204 */
        else en26_gap(w, "mlwP->objP[0].pos[0]/[2] = 0 (en26.c:1204)");
        if (en && en->translate_motion) en->translate_motion(w, 2u, frm);  /* :1205 */
        else en26_gap(w, "bhEne_GetTranslateMtn2 (zonzon.c:251)");
        if (en && en->add_speed) en->add_speed(w, 0);                /* :1206 */
        else en26_gap(w, "bhAddSpeed (pwksub.c:133)");
    }

    if (ret != 0) w->flg |= 0x2000000u;                              /* :1209-1216 */
    else          w->flg &= ~0x2000000u;

    if (w->mode0 < 5u) en26_kami_effect(w, frm);                     /* :1218-1221 */

    if (p->flags & 0x10000000u) {                                    /* :1223 */
        if (w->mtn_no == 2u) {                                       /* :1226 */
            if (frm < 0 || frm >= 71) en26_gap(w, "en01_kamikami[frm] ausserhalb (en26.c:1229)");
            else { w->mdflg |= 2u; w->shp_ct = recvx_en01_kamikami[frm]; } /* :1228-1229 */
        } else if (w->mtn_no == 12u) {                               /* :1231 */
            if (frm < 0 || frm >= 70) en26_gap(w, "en01_kamikami2[frm] ausserhalb (en26.c:1234)");
            else { w->mdflg |= 2u; w->shp_ct = recvx_en01_kamikami2[frm]; } /* :1233-1234 */
        } else if (w->mtn_no == 25u || w->mtn_no == 16u) {           /* :1236 */
            w->mdflg |= 2u;                                          /* :1238 */
            w->shp_ct = recvx_en01_mogmog[frm % 40];                 /* :1239 */
        } else {
            w->mdflg &= ~2u;                                         /* :1243 */
            w->shp_ct = 0.0f;                                        /* :1244 */
        }
    }
    en26_check_mtn_tbl(w, frm);                                      /* :1247 */
    return ret;                                                      /* :1248 */
}

/* --- bhEne26_CheckMtnTbl (en26.c:1252) --------------------------------- */
static void en26_check_mtn_tbl(RECVX_ENEMY_WORK* w, int32_t frm)
{
    static const RECVX_ENEMY_VEC vec1 = { 0.0f, 0.0f, 0.0f };        /* :1265 */
    static const RECVX_ENEMY_VEC vec2 = { 0.0f, -1.0f, -1.9f };      /* :1266 */
    const RECVX_EN01_SERVICES* en = en26_en();
    const RECVX_EN01_MTN* mtbl = en26_mtn_tbl;
    const RECVX_EN01_MTN2* mtbl2 = en26_mtn_tbl2;
    const RECVX_EN01_MTN2* mtbl3 = en26_mtn_tbl3;
    RECVX_ENEMY_VEC vec = { 0.0f, 0.0f, 0.0f };                      /* :1264 */
    int i;

    /* sys->rmthp != epw->mnwP (:1268) */
    if (w->mtn_room) return;

    if (w->flg & 0x40000u) {                                         /* :1270 */
        while (mtbl->no != -1) {                                     /* :1272 */
            if (w->mtn_no == 0u) {                                   /* :1274 */
                if (w->flg & 0x40000u) {                             /* :1276 */
                    const int8_t* tree = (w->flg & 0x80000u) ? en26_tree[0] : en26_tree[1]; /* :1278/:1287 */
                    if (en && en->calc_fix_offset) {
                        en->calc_fix_offset(w, tree, &vec1, &vec);
                        w->px -= vec.x;                              /* :1281/:1288 */
                        w->pz -= vec.z;
                    } else {
                        en26_gap(w, "bhCalcFixOffset (en26.c:1280)");
                    }
                }
                break;                                               /* :1283/:1290/:1293 */
            }
            if (mtbl->no == (int32_t)w->mtn_no) {                    /* :1297 */
                for (i = 0; i < 3; i++) {                            /* :1299 */
                    const RECVX_EN01_FMTN* f = &mtbl->fmtn[i];
                    if (f->type != -1 && frm >= f->s_frm && frm <= f->e_frm) { /* :1301-1303 */
                        const int8_t* tree = NULL;
                        const RECVX_ENEMY_VEC* local = &vec1;
                        switch (f->type) {                           /* :1305 */
                        case 0: tree = (w->mtn_md & 2u) ? en26_tree[1] : en26_tree[0]; break;
                        case 1: tree = (w->mtn_md & 2u) ? en26_tree[0] : en26_tree[1]; break;
                        case 7: tree = (w->mtn_md & 2u) ? en26_tree[1] : en26_tree[0]; local = &vec2; break;
                        case 8: tree = (w->mtn_md & 2u) ? en26_tree[0] : en26_tree[1]; local = &vec2; break;
                        default: break;
                        }
                        if (tree) {
                            if (en && en->calc_fix_offset) en->calc_fix_offset(w, tree, local, &vec);
                            else en26_gap(w, "bhCalcFixOffset (en26.c:1310)");
                        }
                        if (w->mtn_no == 16u) {                      /* :1351 */
                            vec.x *= 1.2f;                           /* :1353 */
                            vec.z *= 1.2f;                           /* :1354 */
                        }
                        w->px -= vec.x;                              /* :1357 */
                        w->pz -= vec.z;                              /* :1358 */
                        break;                                       /* :1360 */
                    }
                }
            }
            mtbl++;                                                  /* :1364 */
        }
    }

    while (mtbl2->no != -1) {                                        /* :1368 */
        if (mtbl2->no == (int32_t)w->mtn_no) {                       /* :1370 */
            for (i = 0; i < 4; i++) {                                /* :1372 */
                if (mtbl2->atb[i].frm == -1) break;                  /* :1374/:1381 */
                if (mtbl2->atb[i].frm == frm) en26_se_play(w, (int)mtbl2->atb[i].act); /* :1376-1379 */
            }
        }
        mtbl2++;                                                     /* :1384 */
    }

    if (w->mode0 < 5u) {                                             /* :1387 */
        while (mtbl3->no != -1) {                                    /* :1389 */
            if (mtbl3->no == (int32_t)w->mtn_no) {                   /* :1391 */
                for (i = 0; i < 4; i++) {                            /* :1393 */
                    if (mtbl3->atb[i].frm == -1) break;              /* :1395/:1402 */
                    if (mtbl3->atb[i].frm == frm) recvx_en01_set_vibration(w, (int32_t)mtbl3->atb[i].act); /* :1397-1400 */
                }
            }
            mtbl3++;                                                 /* :1405 */
        }
    }
}

/* --- bhEne26_SearchNeck (en26.c:1413) ---------------------------------- */
static void en26_search_neck(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);

    if (w->mode0 >= 5u) return;                                      /* :1417 */
    if (p->flags & 0x1000u) {                                        /* :1419 */
        en26_node_angle_set(w, 10u, 1u, (int32_t)w->wax);            /* :1421-1422 */
        en26_node_angle_set(w, 13u, 1u, p->neck_angle);              /* :1423-1424 */
    } else if ((p->flags & 0x800u) && (p->search & 0x40u) && p->player_distance < 35.0f) { /* :1428 */
        int32_t back;
        en26_node_angle_set(w, 13u, 1u, p->neck_angle);              /* :1430-1431 */
        recvx_en01_rot_neck(w, 13, 5);                               /* :1432 */
        if (en26_node_angle_get(w, 13u, 1u, &back)) p->neck_angle = back; /* :1433 */
    } else {
        if (p->neck_angle != 0) {                                    /* :1438 */
            if (p->neck_angle < 32768) {                             /* :1440 */
                p->neck_angle -= 910;                                /* :1442 */
                if (p->neck_angle < 0) p->neck_angle = 0;            /* :1443-1446 */
            } else {
                p->neck_angle += 910;                                /* :1450 */
                if (p->neck_angle >= 65536) p->neck_angle = 0;       /* :1451-1454 */
            }
        }
        en26_node_angle_set(w, 13u, 1u, p->neck_angle);              /* :1437/:1457 */
    }
}

/* --- bhEne26_CollCheck (en26.c:1464) ----------------------------------- */
static void en26_coll_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const RECVX_EN01_SERVICES* en = en26_en();
    uint32_t st_flg;

    if (!(w->flg & 2u)) {                                            /* :1469 */
        if (w->flg & 8u) {                                           /* :1471 */
            if (p->flags & 0x100u) {                                 /* :1473 */
                recvx_en01_coll_check_push(w, (sv && sv->enemy_at) ? sv->enemy_at(0) : NULL); /* :1475 */
            } else {
                if (w->flg & 0x80u) {                                /* :1479 */
                    float m[16];
                    if (en26_joint_mtx(w, 0, m)) {                   /* :1481-1484 */
                        w->aox = m[12] - w->px;
                        w->aoz = m[14] - w->pz;
                        w->aoy = 0.0f;
                    }
                } else {
                    w->aox = 0.0f; w->aoy = 0.0f; w->aoz = 0.0f;     /* :1488-1490 */
                }
                if (!(p->flags & 0x40u)) {                           /* :1492 */
                    if (sv && sv->check_player) sv->check_player(w);   /* :1494 */
                    else en26_gap(w, "bhCheckPlayer (hitchk.c:5712)");
                    if (sv && sv->check_enemies) sv->check_enemies(w); /* :1495 */
                    else en26_gap(w, "bhCheckEnemies (hitchk.c:5788)");
                }
            }
        }
        en26_coll_check_wall(w);                                     /* :1499 */
    }

    if (!sv || !sv->system_flags) { en26_gap(w, "sys->st_flg (en26.c:1502)"); return; }
    st_flg = sv->system_flags(2u);
    if (st_flg & 0x100u) {                                           /* :1502 */
        RECVX_ENEMY_ATR rec;
        w->mdflg &= ~0x40u;                                          /* :1504 */
        if (!en || !en->floor_effect) {
            en26_gap(w, "bhCheckFloorEffect (hitchk.c:5503)");
        } else if (en->floor_effect(w->flr_no, w->px, w->pz, &rec) && rec.prm0 == 2) { /* :1505-1506 */
            w->mdflg |= 0x40u;                                       /* :1508 */
        }
    }
}

/* --- bhEne26_CollCheckWall (en26.c:1514) ------------------------------- */
static void en26_coll_check_wall(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ps, ops, pd, ps2, ops2;
    float lcm[16];         /* die Streumatrix lcmat des Originals (ps2_dummy.h:150) */
    float m[16];
    uint32_t t;

    if (!en26_player(w, &pl, "plp (en26.c:1527)")) return;
    ps.x = pl.pos.x; ps.y = pl.pos.y; ps.z = pl.pos.z;               /* :1527-1529 */
    if (sv && sv->line) {                                            /* :1531 */
        RECVX_ENEMY_VEC from, to, normal;
        uint32_t id = 0;
        from.x = w->px; from.y = w->py; from.z = w->pz;
        to = ps;
        normal.x = normal.y = normal.z = 0.0f;
        p->line_hit = sv->line(1u, &from, &to, &normal, w->flg, w->flr_no, NULL, &id) ? (int32_t)id : 0;
    } else {
        en26_gap(w, "bhCollisionCheckLine (hitchkl.c:12)");
    }

    t = p->flags & 0xFu;                                             /* :1533 */

    if (t == 1u && (w->flg & 0x10u)) {                               /* :1535 */
        ps.x = w->px + w->aox; ps.y = w->py + w->aoy; ps.z = w->pz + w->aoz; /* :1537-1539 */
        ops.x = w->pxb + w->aox; ops.y = w->pyb + w->aoy; ops.z = w->pzb + w->aoz; /* :1541-1543 */
        if (sv && sv->check_dansa) sv->check_dansa(w);               /* :1545 */
        else en26_gap(w, "bhCheckDansa (hitchk.c:5391)");
        if (sv && sv->wall_move) sv->wall_move(w, &ps, &ops, w->ar, w->ah, w->flg); /* :1546 */
        else en26_gap(w, "bhCheckWallEx (hitchk.c)");
        w->px += (ps.x - (w->px + w->aox));                          /* :1548-1550 */
        w->py += (ps.y - (w->py + w->aoy));
        w->pz += (ps.z - (w->pz + w->aoz));
        if (!(p->flags & 0x8000000u)) {                              /* :1551 */
            if (!en26_joint_mtx(w, 12, m)) return;                   /* :1553-1555 */
            ops.x = m[12]; ops.z = m[14];
            ops.y = w->pyb;                                          /* :1556 */
            if (!recvx_en01_calc_parts_pos(w, lcm, &ps, en26_tree[7], 5, 1)) return; /* :1558 */
            ps.y = w->py;                                            /* :1560 */
            recvx_en01_collision_wall2(w, &ps, &ops, &pd, 2.0f, 2.0f); /* :1562 */
            w->px += pd.x; w->py += pd.y; w->pz += pd.z;             /* :1564-1566 */
        }
    } else if (t == 2u) {                                            /* :1569 */
        ps.x = w->px + w->aox; ps.y = w->py + w->aoy; ps.z = w->pz + w->aoz; /* :1571-1573 */
        ops.x = w->pxb + w->aox; ops.y = w->pyb + w->aoy; ops.z = w->pzb + w->aoz; /* :1575-1577 */
        if (sv && sv->check_dansa) sv->check_dansa(w);               /* :1578 */
        else en26_gap(w, "bhCheckDansa (hitchk.c:5391)");
        if (sv && sv->wall_move) sv->wall_move(w, &ps, &ops, w->ar, w->ah, w->flg); /* :1579 */
        else en26_gap(w, "bhCheckWallEx (hitchk.c)");
        w->px += (ps.x - (w->px + w->aox));                          /* :1581-1583 */
        w->py += (ps.y - (w->py + w->aoy));
        w->pz += (ps.z - (w->pz + w->aoz));
        if (!en26_joint_mtx(w, 16, m)) return;                       /* :1585-1587 */
        ops.x = m[12]; ops.z = m[14];
        if (!en26_joint_mtx(w, 19, m)) return;                       /* :1589-1591 */
        ops.x = (ops.x + m[12]) / 2.0f;
        ops.z = (ops.z + m[14]) / 2.0f;
        ops.y = w->pyb;                                              /* :1592 */
        if (!recvx_en01_calc_parts_pos(w, lcm, &ps, en26_tree[2], 7, 1)) return; /* :1594 */
        if (!recvx_en01_calc_parts_pos(w, lcm, &pd, en26_tree[3], 7, 1)) return; /* :1595 */
        ps.x = (ps.x + pd.x) / 2.0f;                                 /* :1597-1599 */
        ps.z = (ps.z + pd.z) / 2.0f;
        ps.y = w->py;
        recvx_en01_collision_wall2(w, &ps, &ops, &pd, 3.0f, 2.0f);   /* :1601 */
        w->px += pd.x; w->py += pd.y; w->pz += pd.z;                 /* :1603-1605 */
    } else if (t == 3u) {                                            /* :1607 */
        if (!en26_joint_mtx(w, 1, m)) return;                        /* :1609-1612 */
        ops.x = m[12]; ops.z = m[14]; ops.y = w->pyb;
        if (!recvx_en01_calc_parts_pos(w, lcm, &ps, en26_tree[7], 2, 1)) return; /* :1614 */
        ps.y = w->py;                                                /* :1616 */
        recvx_en01_collision_wall2(w, &ps, &ops, &pd, 1.5f, 2.0f);   /* :1618 */
        w->px += pd.x; w->py += pd.y; w->pz += pd.z;                 /* :1620-1622 */
    } else if (t == 5u) {                                            /* :1625 */
        if (!en26_joint_mtx(w, 0, m)) return;                        /* :1627-1630 */
        ops.x = m[12]; ops.z = m[14]; ops.y = w->pyb;
        if (!recvx_en01_calc_parts_pos(w, lcm, &ps, en26_tree[7], 1, 1)) return; /* :1632 */
        ps.y = w->py;                                                /* :1634 */
        recvx_en01_collision_wall2(w, &ps, &ops, &pd, w->ar, w->ah); /* :1636 */
        w->px += pd.x; w->py += pd.y; w->pz += pd.z;                 /* :1637-1639 */
    } else if (t == 6u) {                                            /* :1642 */
        NJS_POINT3 lp, lp2, o;
        if (!recvx_en01_calc_parts_pos(w, lcm, &ps, en26_tree[7], 2, 1)) return; /* :1644 */
        if (!en26_joint_mtx(w, 1, m)) return;                        /* :1645 */
        lp.x = 0.0f; lp.y = -5.0f; lp.z = 0.0f;                      /* :1646-1648 */
        njCalcPoint((NJS_MATRIX*)lcm, &lp, &o); ps.x = o.x; ps.y = o.y; ps.z = o.z;   /* :1649 */
        njCalcPoint((NJS_MATRIX*)m, &lp, &o); ops.x = o.x; ops.y = o.y; ops.z = o.z;  /* :1650 */
        lp2.x = 0.0f; lp2.y = 3.5f; lp2.z = 0.0f;                    /* :1652-1654 */
        njCalcPoint((NJS_MATRIX*)lcm, &lp2, &o); ps2.x = o.x; ps2.y = o.y; ps2.z = o.z;  /* :1655 */
        njCalcPoint((NJS_MATRIX*)m, &lp2, &o); ops2.x = o.x; ops2.y = o.y; ops2.z = o.z; /* :1656 */
        ps.y = ps2.y = w->py;                                        /* :1658 */
        ops.y = ops2.y = w->pyb;                                     /* :1659 */
        recvx_en01_collision_wall2(w, &ps, &ops, &pd, w->ar, w->ah); /* :1661 */
        w->px += pd.x; w->py += pd.y; w->pz += pd.z;                 /* :1663-1665 */
        recvx_en01_collision_wall2(w, &ps2, &ops2, &pd, w->ar, w->ah); /* :1667 */
        w->px += pd.x; w->py += pd.y; w->pz += pd.z;                 /* :1669-1671 */
    }
}

/* --- bhEne26_PlayerLink (en26.c:1676) ----------------------------------
   Spielerlage aus dem Griffpunkt exp0+0x64 (:1704-1710), bei plp->flg 0x1000
   zusaetzlich plp->mtx aus der XZ-Tabelle der Spielerfigur (:1711-1743).
   Anders als bhEne01_PlayerLink hat EN26 KEINEN Wandteil und verschiebt den
   Gegner nicht: die Rueckgabe out_delta des gemeinsamen Spielerdienstes
   (en01_work.h player_link_pose, en01.c:3093) wird deshalb verworfen. */
static void en26_player_link(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_XZ* const ply_pos[16] = {                       /* :1684-1702 */
        recvx_en01_cl_mtn01_xz, recvx_en01_cl_mtn02_xz, recvx_en01_cl_mtn04_xz, recvx_en01_cl_mtn05_xz,
        recvx_en01_ch_mtn01_xz, recvx_en01_ch_mtn02_xz, recvx_en01_ch_mtn04_xz, recvx_en01_ch_mtn05_xz,
        recvx_en01_st_mtn01_xz, recvx_en01_st_mtn02_xz, recvx_en01_st_mtn04_xz, recvx_en01_st_mtn05_xz,
        recvx_en01_ch_mtn01_xz, recvx_en01_ch_mtn02_xz, recvx_en01_ch_mtn04_xz, recvx_en01_ch_mtn05_xz
    };
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();
    const RECVX_EN01_XZ* offset = NULL;
    RECVX_ENEMY_PLAYER pl;
    NJS_POINT3 src, dst;
    RECVX_ENEMY_VEC pd, delta;
    int32_t ay;

    njUnitMatrix(NULL);                                              /* :1704 */
    njTranslate(NULL, w->px, w->py, w->pz);                          /* :1705 */
    njRotateXYZ(NULL, w->ax, w->ay, w->az);                          /* :1706 */
    src.x = p->grab_point.x; src.y = p->grab_point.y; src.z = p->grab_point.z;
    njCalcPoint(NULL, &src, &dst);                                   /* :1707 */
    pd.x = dst.x; pd.y = dst.y; pd.z = dst.z;
    ay = (int32_t)(uint16_t)(w->ay + w->waxp);                       /* :1710 */

    if (!en26_player(w, &pl, "plp (en26.c:1708)")) return;
    if (pl.flg & 0x1000u) {                                          /* :1711 */
        int32_t base, mtn_no, frm;
        uint32_t len;
        if (pl.ply_id >= 4u) { en26_gap(w, "En26_PlyMtn_OffsetTbl[ply_id] ausserhalb (en26.c:1713)"); return; }
        base = en26_ply_mtn_offset[pl.ply_id];
        if      ((int32_t)pl.mtn_no == base + 1) mtn_no = 0;          /* :1713-1716 */
        else if ((int32_t)pl.mtn_no == base + 2) mtn_no = 1;          /* :1717-1720 */
        else if ((int32_t)pl.mtn_no == base + 4) mtn_no = 2;          /* :1721-1724 */
        else if ((int32_t)pl.mtn_no == base + 5) mtn_no = 3;          /* :1725-1728 */
        else {
            /* Kein else im Original: mtn_no bleibt die nicht initialisierte
               Stackvariable (en26.c:1682/:1730). */
            en26_gap(w, "bhEne26_PlayerLink: mtn_no unbestimmt (en26.c:1730)");
            return;
        }
        frm = pl.frm_no / 65536;                                     /* :1731 */
        len = (mtn_no < 2) ? 60u : 50u;
        if (frm < 0 || (uint32_t)frm >= len) {
            en26_gap(w, "ply_pos[...][frm_no / 65536] ausserhalb (en26.c:1731)");
            return;
        }
        offset = &ply_pos[pl.ply_id * 4u + (uint32_t)mtn_no][frm];   /* :1730-1735 */
    }
    if (!en || !en->player_link_pose) { en26_gap(w, "bhEne26_PlayerLink (en26.c:1676)"); return; }
    if (!en->player_link_pose(w, &pd, ay, offset, &delta)) {         /* :1708-1742 */
        en26_gap(w, "bhEne26_PlayerLink: Spielerposition/-matrix");
    }
}

/* --- bhEne26_CalcEnemy (en26.c:1747) ----------------------------------- */
static void en26_calc_enemy(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC a, b;
    float m[16];

    en26_calc_model(w);                                              /* :1751 */
    if (!en26_joint_pos(w, 12, &a)) return;                          /* :1752-1755 */
    w->watr.c1 = a;
    if (!en26_joint_pos(w, 4, &a)) return;                           /* :1757-1760 */
    if (!en26_joint_pos(w, 8, &b)) return;                           /* :1762 */
    w->watr.c2.x = (a.x + b.x) / 2.0f;                               /* :1763-1765 */
    w->watr.c2.y = (a.y + b.y) / 2.0f;
    w->watr.c2.z = (a.z + b.z) / 2.0f;
    w->watr.r = 2.0f;                                                /* :1767 */
    if (!en26_joint_mtx(w, 12, m)) return;
    w->cah = w->ah = m[13] - w->py;                                  /* :1768 */
}

/* --- bhEne26_Init (en26.c:1772) ---------------------------------------- */
void recvx_en26_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    RECVX_EN26_EXP0* p;
    RECVX_ENEMY_PLAYER pl;
    uint32_t i;

    if (!w) return;
    if (en26_player(w, &pl, "plp->flg &= ~4 (en26.c:1778)")) {       /* :1778 */
        pl.flg &= ~4u;
        en26_player_set(w, &pl);
    }
    w->ar = 3.8f;                                                    /* :1779-1784 */
    w->ah = 18.2f;
    w->aw = 0.0f;
    w->ad = 0.0f;
    w->car = 2.5f;
    w->cah = 18.2f;
    w->hp = 200;                                                     /* :1785 */
    w->stflg = 0;                                                    /* :1786 */
    for (i = 0; i < RECVX_ENEMY_DAM; i++) w->dam[i] = 0;             /* :1788-1791 */
    w->hokan_rate = 0;                                               /* :1793 */
    w->hokan_count = 0;
    w->frm_no = 0;
    w->mtn_add = 65536;
    w->mtn_no = 0;
    w->mtn_tp = en26_flip_tree;                                      /* :1798 */
    w->mtn_md = 0;

    if (w->exp0 == NULL) {                                           /* :1801 */
        if (w->sv && w->sv->calloc_work) w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN26_EXP0)); /* :1803 */
        if (!w->exp0) { en26_gap(w, "bhEne_CallocWork(160, 8) (en26.c:1803)"); return; }
        w->exp1 = NULL;                                              /* :1804 */
    } else {
        memset(w->exp0, 0, sizeof(RECVX_EN26_EXP0));                 /* :1808-1813 */
    }
    p = recvx_en26_exp0(w);

    p->flags |= 1u;                                                  /* :1816 */
    w->flg |= 0x178u;                                                /* :1817 */
    w->flg &= ~2u;                                                   /* :1818 */
    w->cpcl = en26_cap_col;                                          /* :1819 */
    if (en && en->select_morph_pair) en->select_morph_pair(w);       /* :1820/:1822-1823 */
    else en26_gap(w, "mlwP->objP = mbp[0] (en26.c:1820)");
    w->mdflg = 0;                                                    /* :1821 */
    p->flags |= 0x10000000u;                                         /* :1825 */

    if (!(w->flg & 0x800u)) {                                        /* :1827 */
        /* bhSetShadow(En26SdwTab, epw, 1.0f, 6.0f, 5.0f, 5.0f): der dritte
           Parameter ist lkono (int), 1.0f wird zu 1 (effect.c:644). */
        if (w->sv && w->sv->set_shadow) (void)w->sv->set_shadow(en26_sdw_tab, w, 1, 6.0f, 5.0f, 5.0f); /* :1829 */
        else en26_gap(w, "bhSetShadow (effect.c:644)");
        w->flg |= 0x800u;                                            /* :1830 */
    }

    if ((p->mat_color & 0xFFFFFFu) == 0u) {                          /* :1833 */
        w->mdflg &= ~0x400u;                                         /* :1835 */
        p->mat_color = 0xFFB2B2B2u;                                  /* :1836 */
        en26_set_material_color(w, p->mat_color);                    /* :1837 */
    }

    w->clp_jno[0] = 13;                                              /* :1840-1845 */
    w->clp_jno[1] = 16;
    w->clp_jno[2] = 19;
    w->clp_jno[3] = 4;
    w->clp_jno[4] = 8;
    w->clp_jno[5] = -1;
    w->mdflg |= 0x20u;                                               /* :1846 */
    w->lok_jno = 12;                                                 /* :1847 */
    w->comb_flg |= 2u;                                               /* :1848 */
    /* bhClrUseKaidanFlag(epw) (:1849; hitchk.c:5005) wirkt nur ueber
       epw->kdnp. Dessen einziger Schreiber bhSetUseKaidanFlag (hitchk.c:4965)
       wird nur aus en01.c, en04.c, hitchk.c (Spieler) und player.c gerufen,
       nie fuer einen id-26-Work; bhSetEnemy nullt den Work. kdnp ist hier also
       immer NULL und der Aufruf ohne Wirkung. */
    (void)en26_set_motion(w, (int)w->mtn_add);                       /* :1850 */
    en26_calc_enemy(w);                                              /* :1851 */
    w->mode0 = 1;                                                    /* :1852-1855 */
    w->mode1 = 0;
    w->mode2 = 0;
    w->mode3 = 0;
    if (en && en->joint_flags_or) en->joint_flags_or(w, 10, 2u);     /* :1856 */
    else en26_gap(w, "mlwP->owP[10].flg |= 2 (en26.c:1856)");
}

/* --- bhEne26_EneSearch (en26.c:1910) ----------------------------------- */
static void en26_ene_search(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_VEC pos;
    int32_t neck_ry = 0, seen = -1;

    if (!en26_joint_pos(w, 13, &pos)) return;                        /* :1916-1921 */
    pos.y = 0.0f;
    if (!en26_node_angle_get(w, 13u, 1u, &neck_ry)) return;          /* :1917 */

    if ((p->search & 0x1Fu) < 31u) {                                 /* :1923 */
        if (!en26_search_player2(w, &pos, w->ay + neck_ry, 14563, &seen)) return; /* :1925 */
        if (seen != -1) p->search |= 0x20u;                          /* :1925-1928 */
        if (p->search & 0x20u) {                                     /* :1930 */
            p->search |= 0x40u;                                      /* :1932 */
            p->search &= (uint8_t)~0x20u;                            /* :1933 */
        } else if ((p->search & 0x1Fu) == 30u) {                     /* :1935 */
            p->search &= (uint8_t)~0x40u;                            /* :1937 */
        }
    }
    p->search++;                                                     /* :1941 */
    if ((p->search & 0x1Fu) >= 31u) p->search &= (uint8_t)~0x1Fu;    /* :1942-1945 */

    if (p->search & 0x40u) {                                         /* :1947 */
        p->flags |= 0x400u;                                          /* :1949 */
        p->search_hold = 15u;                                        /* :1950 */
    } else if (p->flags & 0x400u) {                                  /* :1952 */
        /* `--EXP0_UC(0x29) < 0` (:1954): ein vorzeichenloses Byte, der
           Vergleich ist nie wahr; 0x400 faellt hier also nie. Wie im
           EN01-Port (bhEne01_EneSearch en01.c:4595) unveraendert. */
        p->search_hold--;
    }
}

/* --- bhEne26_Brain00 (en26.c:1962) ------------------------------------- */
static void en26_brain00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC self, pp, pos;
    uint8_t rid;

    en26_ene_search(w);                                              /* :1967 */
    if (!(p->flags & 0x400u)) return;                                /* :1968 */
    if (!en26_player(w, &pl, "plp (en26.c:1970)")) return;
    if (!en || !en->route_id || !en->route_next) { en26_gap(w, "bhCheckRouteID / bhCheckRoute (rutchk.c:5/47)"); return; }
    self.x = w->px; self.y = w->py; self.z = w->pz;
    pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;

    if (p->player_distance < 50.0f && p->line_hit == 0 && w->flr_no == pl.flr_no) { /* :1970 */
        p->ikou_target.x = pl.pos.x;                                 /* :1972 */
        p->ikou_target.z = pl.pos.z;                                 /* :1973 */
        p->route_id = (uint8_t)en->route_id(&self);                  /* :1974 */
        p->route_ct = 0;                                             /* :1975 */
        return;
    }
    rid = (uint8_t)en->route_id(&self);                              /* :1979 */
    p->route_ct++;                                                   /* :1980 */
    if (p->route_id == rid || p->route_ct >= 255u) {                 /* :1981 */
        /* bhCheckRoute (rutchk.c:5) schreibt `pos` nur, wenn beide Punkte
           in einer Wegezone liegen; sonst liest das Original danach den
           uninitialisierten Stack (en26.c:1984-1985). Dieser Fall wird
           gemeldet und das Ziel nicht aus einem erfundenen Wert gesetzt. */
        const int both = en->route_id(&self) != 0xFF && en->route_id(&pp) != 0xFF;
        rid = (uint8_t)en->route_next(&self, &pp, &pos);             /* :1983 */
        if (both) {
            p->ikou_target.x = pos.x;                                /* :1984 */
            p->ikou_target.z = pos.z;                                /* :1985 */
        } else {
            en26_gap(w, "bhEne26_Brain00: bhCheckRoute ohne Wegezone, Original liest uninitialisiertes pos (en26.c:1984)");
        }
        p->route_id = rid;                                           /* :1986 */
        p->route_ct = 0;                                             /* :1987 */
    }
}

/* --- bhEne26_EatCheck (en26.c:3267) ------------------------------------ */
static int en26_eat_check(RECVX_ENEMY_WORK* w, int rng, float dist, int mode)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ps, eps;
    float ds;

    if (!en26_player(w, &pl, "plp (en26.c:3275)")) return 0;
    /* debug_attack_disabled: der Host-Schalter "Gegner-Angriff" sperrt den
       Angriffsbeginn wie bei EN01 (en01_core.c recvx_en01_eat_check); er
       ist kein Originalflag und wirkt wie plp->flg 0x2. */
    if ((p->flags & 0x20000000u) || pl.debug_attack_disabled || (pl.flg & 2u) ||
        (pl.stflg & 0x80000000u)) return 0;                          /* :3275-3278 */

    if (!en26_player_joint_pos(w, 3, &ps)) return 0;                 /* :3280-3283 */
    ps.y = pl.pos.y;
    if (!en26_joint_pos(w, 12, &eps)) return 0;                      /* :3284-3287 */
    eps.y = pl.pos.y;
    ds = recvx_en01_distance_p2p(&ps, eps.x, eps.y, eps.z);          /* :3289 */

    switch (mode) {                                                  /* :3291 */
    case 0: {
        RECVX_ENEMY_VEC self;
        int32_t seen;
        self.x = w->px; self.y = w->py; self.z = w->pz;
        if (!en26_search_player2(w, &self, w->ay, rng, &seen)) return 0; /* :3294 */
        if (seen == -1 || ds >= dist || pl.pos.y != w->py || pl.hp < 0) break; /* :3294-3300 */
        return 1;                                                    /* :3302 */
    }
    case 1:
        if (pl.pos.y != w->py || pl.hp < 0) break;                   /* :3305-3308 */
        return 1;                                                    /* :3310 */
    case 2: {
        float m[16];
        NJS_POINT3 lp, c;
        RECVX_ENEMY_VEC cc;
        RECVX_ENEMY_CAPSULE cap;
        if (!en26_joint_mtx(w, 12, m)) return 0;                     /* :3313 */
        lp.x = 0.0f; lp.y = 0.0f; lp.z = -4.0f;                      /* :3314-3316 */
        njCalcPoint((NJS_MATRIX*)m, &lp, &c);                        /* :3317 */
        cc.x = c.x; cc.y = c.y; cc.z = c.z;
        if (!en || !en->player_capsule || !en->player_capsule(&cap)) { en26_gap(w, "plp->watr (en26.c:3320)"); return 0; }
        if (!w->sv || !w->sv->sphere_capsule) { en26_gap(w, "npCollisionCheckSC (njplus.c)"); return 0; }
        if (!w->sv->sphere_capsule(&cc, 1.0f, &cap.c1, &cap.c2, cap.r) || /* :3318-3320 */
            pl.pos.y != w->py || pl.debug_attack_disabled || (pl.flg & 2u) ||
            (pl.stflg & 0x80000000u) || pl.hp < 0) break;            /* :3321-3324 */
        return 1;                                                    /* :3328 */
    }
    default:
        break;
    }
    return 0;                                                        /* :3331 */
}

/* --- bhEne26_ActionModeCheck (en26.c:1993) -----------------------------
   Das Original endet ohne return (int-Funktion); der einzige Aufrufer
   (:1900) wertet den Wert nicht aus. */
static int en26_action_mode_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC proot;

    if (!en26_player(w, &pl, "plp (en26.c:1997)")) return 0;
    if (!en26_player_joint_pos(w, 0, &proot)) return 0;              /* plp->mlwP->owP->mtx[12] */

    if (en26_mode2_low(w) && recvx_en01_ikou3(w, &proot, 4096) == 0 &&
        p->player_distance < 10.0f && pl.mode0 == 6u && pl.hp < 0) { /* :1997-1999 */
        w->mode1 = 0; w->mode2 = 16; w->mode3 = 0;                   /* :2001-2003 */
        return 1;                                                    /* :2004 */
    }

    /* debug_attack_disabled wie plp->flg 0x2 (siehe en26_eat_check). */
    if ((pl.flg & 4u) || pl.debug_attack_disabled || (pl.flg & 2u) ||
        (pl.stflg & 0x80000000u)) {                                  /* :2007 */
        RECVX_ENEMY_VEC self;
        int32_t seen;
        self.x = w->px; self.y = w->py; self.z = w->pz;
        if (!en26_search_player2(w, &self, w->ay, 5461, &seen)) return 0; /* :2009 */
        if (seen != -1) {
            if (p->player_distance < 7.0f && pl.flr_no == w->flr_no && en26_mode2_low(w)) { /* :2011-2013 */
                w->mode1 = 0; w->mode2 = 6; w->mode3 = 0;            /* :2015-2017 */
                return 1;                                            /* :2018 */
            }
        }
        return 0;                                                    /* :2021 */
    }

    if (en26_mode2_low(w)) {                                         /* :2025 */
        if (en26_eat_check(w, 8192, 5.5f, 0) && p->eat_wait == 0u && w->mtn_no == 16u) { /* :2028 */
            w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;  /* :2030-2033 */
            w->flg &= ~0x20u;                                        /* :2034 */
            if (en26_player(w, &pl, "plp->flg (en26.c:2035)")) {
                pl.flg |= 0x10000u;                                  /* :2035 */
                pl.flg |= 4u;                                        /* :2036 */
                en26_player_set(w, &pl);
            }
            return 1;                                                /* :2037 */
        }
        if (recvx_en01_cdir_check((int32_t)w->ay, pl.ay) == 0 && pl.mode2 == 6u &&
            (en26_rand(w) % 2) == 0 && en26_eat_check(w, 5461, 11.0f, 0)) { /* :2040-2043 */
            w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;                /* :2045-2047 */
            return 1;                                                /* :2048 */
        }
        if (p->player_distance < 11.0f && w->flr_no == pl.flr_no && pl.hp >= 0 &&
            recvx_en01_ikou3(w, &proot, 16384) && !(p->flags & 0x20000000u)) { /* :2051-2055 */
            w->mode1 = 0; w->mode2 = 3; w->mode3 = 0;                /* :2057-2059 */
            return 1;                                                /* :2060 */
        }
        if ((w->flg & 4u) && w->flr_no == pl.flr_no && !(p->player_distance <= 15.0f) &&
            recvx_en01_ikou3(w, &proot, 24576)) {                    /* :2063-2066 */
            w->mode1 = 0; w->mode2 = 7; w->mode3 = 0;                /* :2068-2070 */
            return 1;                                                /* :2071 */
        }
    }
    return 0;
}

/* --- MoveMode2 (en26.c:571) -------------------------------------------- */
static void en26_mv_idle_voice(RECVX_ENEMY_WORK* w)
{
    if (--w->ct3 < 0) {                                              /* :2181/:2267 */
        en26_se_play(w, 16847639);                                   /* :2183/:2269 */
        w->ct3 = (en26_rand(w) % 120) + 180;                         /* :2184/:2270 */
    }
}

/* bhEne26_MV00 (en26.c:2079). */
static void en26_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    int32_t frm_no;

    switch (w->mode3) {
    case 0: {
        uint32_t frames = en26_motion_frames(w, w->mtn_no);          /* :2086 */
        if (frames == 0u) { en26_gap(w, "mnwP[mtn_no].frm_num == 0 (en26.c:2086)"); return; }
        frm_no = (int32_t)((uint32_t)en26_rand(w) % frames) * 65536; /* :2086 */
        recvx_enemy_chg_mtn(w, 0u, 0, 15);                           /* :2087 */
        en26_mtn_flags_clear(p);                                     /* :2088 */
        w->frm_no = frm_no;                                          /* :2089 */
        w->flg |= 0x40000u;                                          /* :2090 */
        w->ct0 = ((en26_rand(w) % 10) * 20) + 15;                    /* :2091 */
        p->flags &= ~0x400u;                                         /* :2092 */
        p->flags &= ~0xFu;                                           /* :2093 */
        p->flags |= 1u;                                              /* :2094 */
        w->mode1 = 1;                                                /* :2095 */
        w->mode3++;                                                  /* :2096 */
        en26_se_play(w, 16847639);                                   /* :2097 */
    }
    /* FALLTHROUGH — kein break (:2097/:2099) */
    case 1:
        w->ct0 -= 1;                                                 /* :2100 */
        if (w->ct0 <= 0 || (p->flags & 0x400u)) {                    /* :2101 */
            w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;                /* :2103-2105 */
        }
        break;
    default:
        break;
    }
}

/* bhEne26_MV01 (en26.c:2114). */
static void en26_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    int hit;

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 25u, 0, 10);                          /* :2121 */
        en26_mtn_flags_clear(p);                                     /* :2122 */
        w->way = 512;                                                /* :2123 */
        w->flg |= 0x40000u;                                          /* :2124 */
        p->flags &= ~0xFu;                                           /* :2125 */
        p->flags |= 2u;                                              /* :2126 */
        w->ct0 = (en26_rand(w) % 128) + 200;                         /* :2127 */
        w->ct1 = 19;                                                 /* :2128 */
        w->ct3 = en26_rand(w) % 120;                                 /* :2129 */
        w->mode3 = 1;                                                /* :2130 */
    /* FALLTHROUGH (:2131) */
    case 1:
        if (p->player_distance < 30.0f || (p->flags & 0x400u)) {     /* :2133 */
            w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;                /* :2135-2137 */
            en26_se_play(w, 16847639);                               /* :2138 */
            p->flags |= 0x400u;                                      /* :2139 */
        } else if (recvx_en01_check_dir_wall(w, 0, 8.0f)) {          /* :2143 */
            hit = recvx_en01_check_side_wall(w, 8.0f, 0);            /* :2145 */
            if (hit == 0) {                                          /* :2147 */
                if (!en26_player(w, &pl, "plp (en26.c:2149)")) break;
                w->ayp = recvx_en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z); /* :2149 */
                w->ayp = (int32_t)(uint16_t)(w->ayp - w->ay);        /* :2150 */
                w->way = (w->ayp < 32769) ? 256 : -256;              /* :2151-2158 */
            } else {
                w->way = (int16_t)(hit * 256);                       /* :2162 */
            }
            w->mode3 = 2;                                            /* :2164 */
        }
        break;
    case 2:
        if (!recvx_en01_check_dir_wall(w, 0, 8.0f)) w->mode3 = 1;    /* :2170-2173 */
        else w->ay += w->way;                                        /* :2176 */
        break;
    default:
        break;
    }
    en26_mv_idle_voice(w);                                           /* :2181-2185 */
}

/* bhEne26_MV02 (en26.c:2189). */
static void en26_mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 25u, 0, 10);                          /* :2196 */
        en26_mtn_flags_clear(p);                                     /* :2197 */
        w->way = 0x100;                                              /* :2198 */
        w->flg |= 0x40000u;                                          /* :2199 */
        w->ct0 = 0;                                                  /* :2200 */
        w->ct1 = 0;                                                  /* :2201 */
        w->ct2 = (en26_rand(w) % 120) + 120;                         /* :2202 */
        p->word84 = 0;                                               /* :2203 */
        p->word88 = 0;                                               /* :2204 */
        w->ct3 = en26_rand(w) % 120;                                 /* :2205 */
        p->flags2 &= ~0x80u;                                         /* :2206 */
        p->flags &= ~0xFu;                                           /* :2207 */
        p->flags |= 2u;                                              /* :2208 */
        w->mode3 = 2;                                                /* :2209 */
        break;
    case 1:
        recvx_enemy_chg_mtn(w, 16u, 0, 10);                          /* :2212 */
        en26_mtn_flags_clear(p);                                     /* :2213 */
        w->flg |= 0x40000u;                                          /* :2214 */
        w->way = 0x200;                                              /* :2215 */
        w->ct0 = 0;                                                  /* :2216 */
        w->ct1 = 0;                                                  /* :2217 */
        w->ct2 = (en26_rand(w) % 120) + 120;                         /* :2218 */
        p->word84 = 0;                                               /* :2219 */
        p->word88 = 0;                                               /* :2220 */
        w->ct3 = en26_rand(w) % 120;                                 /* :2221 */
        p->flags &= ~0xFu;                                           /* :2222 */
        p->flags |= 2u;                                              /* :2223 */
        p->flags |= 0x400u;                                          /* :2224 */
        w->mode3 = 2;                                                /* :2225 */
        break;
    case 2:
        if (w->mtn_no == 25u) {                                      /* :2228 */
            if (!(p->flags2 & 0x80u)) {                              /* :2230 */
                p->word7C = 10;                                      /* :2232 */
                p->overlay_frame = 0;                                /* :2233 */
                if (w->hokan_count < 10u) w->hokan_count = 10u;      /* :2234-2237 */
                w->ct1 = 19;                                         /* :2238 */
                p->flags2 |= 0x80u;                                  /* :2239 */
            } else if (--w->ct1 < 0) {                               /* :2243 */
                int32_t frm;
                if (!en || !en->frame_num) { en26_gap(w, "bhGetFrameNum (pwksub.c:144)"); break; }
                frm = en->frame_num(en26_motion_frames(w, w->mtn_no), en26_motion_frames(w, 16u), w->frm_no); /* :2245 */
                recvx_enemy_chg_mtn(w, 16u, frm, 10);                /* :2246 */
                en26_mtn_flags_clear(p);                             /* :2247 */
                w->flg |= 0x40000u;                                  /* :2248 */
                w->way = 0x200;                                      /* :2249 */
            }
        }
        recvx_en01_ikou(w, &p->ikou_target, w->way);                 /* :2253 */
        break;
    case 3:
        if (!recvx_en01_check_dir_wall(w, 0, 8.0f)) w->mode3 = 2;    /* :2256-2259 */
        else w->ay += w->way;                                        /* :2262 */
        break;
    default:
        break;
    }
    en26_mv_idle_voice(w);                                           /* :2267-2271 */
}

/* bhEne26_MV03 (en26.c:2275). */
static void en26_mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC owk;
    int32_t rot;

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 13u, 0, 8);                           /* :2283 */
        en26_mtn_flags_clear(p);                                     /* :2284 */
        w->mtn_add = 0;                                              /* :2285 */
        w->ct0 = 6;                                                  /* :2286 */
        w->mode3++;                                                  /* :2287 */
    /* FALLTHROUGH (:2288) */
    case 1:
        if (!en26_player_joint_pos(w, 0, &owk)) break;               /* :2290 */
        rot = recvx_en01_ikou3(w, &owk, 4096);                       /* :2291 */
        w->ct0--;                                                    /* :2292 */
        if (w->ct0 <= 0 || rot == 0) {                               /* :2293 */
            if (!en26_player(w, &pl, "plp (en26.c:2295)")) break;
            if (!recvx_en01_ikou3(w, &owk, 4096) && p->player_distance < 10.0f &&
                pl.mode0 == 6u && pl.hp < 0) {                       /* :2295 */
                w->mode1 = 0; w->mode2 = 16; w->mode3 = 0;           /* :2297-2299 */
            } else {
                w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;            /* :2303-2305 */
            }
        }
        w->ay += rot;                                                /* :2308 */
        break;
    default:
        break;
    }
}

/* bhEne26_MV05 (en26.c:2314). */
static void en26_mv05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC proot;
    float hit;
    int32_t frm;

    switch (w->mode3) {
    case 0:
        /* Beide sys->ply_id-Zweige sind im Original gleich (:2324-2335). */
        recvx_enemy_chg_mtn(w, 13u, 0, 8);                           /* :2326/:2332 */
        p->flags &= ~0x2000000u;                                     /* :2327/:2333 */
        p->flags |= 0x1000000u;                                      /* :2328/:2334 */
        if (!en26_player(w, &pl, "plp (en26.c:2336)")) return;
        w->ay = recvx_en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z); /* :2336 */
        p->grab_point.y = 0.0f;                                      /* :2337 */
        p->grab_point.x = w->px;                                     /* :2338 */
        p->grab_point.z = w->pz;                                     /* :2339 */
        en26_se_play(w, 16782102);                                   /* :2340 */
        w->mode3++;                                                  /* :2341 */
    /* FALLTHROUGH (:2342) */
    case 1:
        frm = w->frm_no / 65536;                                     /* :2344 */
        if (frm < 13) {                                              /* :2346 */
            if (en26_player_joint_pos(w, 0, &proot)) w->ay += recvx_en01_ikou3(w, &proot, 1536); /* :2348 */
        }
        hit = 0.0f;                                                  /* :2351 */
        if (frm >= 8) {                                              /* :2352 */
            if (!en26_player(w, &pl, "plp (en26.c:2354)")) break;
            if (frm < 0xD && !(pl.flg & 4u) && en26_eat_check(w, 3640, 5.5f, 0) != 0) { /* :2354 */
                if (!en26_player(w, &pl, "plp (en26.c:2356)")) break;
                pl.flg |= 4u;                                        /* :2356 */
                pl.flg |= 0x10000u;                                  /* :2357 */
                w->flg &= ~0x20u;                                    /* :2358 */
                w->flg &= ~0x40u;                                    /* :2359 */
                /* *(int*)&pl->mode0 = 4 (:2361): mode0 4, mode1..3 0. */
                pl.mode0 = 4; pl.mode1 = 0; pl.mode2 = 0; pl.mode3 = 0;
                pl.mode3 = 4;                                        /* :2362 */
                p->flags |= 0x20000u;                                /* :2364 */
                if (recvx_en01_cdir_check(pl.ay, (int32_t)w->ay) == 0) p->flags |= 0x4000u; /* :2365-2368 */
                else                                                   p->flags &= ~0x4000u; /* :2371 */
                en26_player_set(w, &pl);
                recvx_enemy_chg_mtn(w, 12u, 0, 5);                   /* :2376/:2381 */
                en26_mtn_flags_clear(p);                             /* :2377/:2382 */
                p->flags &= ~0xFu;                                   /* :2385 */
                p->flags |= 5u;                                      /* :2386 */
                w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 2; /* :2387-2390 */
                w->ct0 = 75;                                         /* :2391 */
                hit = 1.0f;                                          /* :2392 */
            }
        }
        if (frm == 12 && hit == 0.0f) w->mode3++;                    /* :2395-2398 */
        break;
    case 2:
        if ((w->frm_no / 65536) == (int32_t)en26_motion_frames(w, w->mtn_no) - 1) { /* :2401 */
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;  /* :2403-2406 */
        }
        break;
    default:
        break;
    }
}

/* bhEne26_MV06 (en26.c:2412). */
static void en26_mv06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pp;

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 15u, 2949120, 15);                    /* :2417 */
        en26_mtn_flags_clear(p);                                     /* :2418 */
        w->flg |= 0x40000u;                                          /* :2419 */
        w->ct0 = (en26_rand(w) % 15) + 15;                           /* :2420 */
        w->mode1 = 1;                                                /* :2421 */
        w->mode3++;                                                  /* :2422 */
    /* FALLTHROUGH (:2423) */
    case 1:
        if (!en26_player(w, &pl, "plp (en26.c:2425)")) break;
        pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;
        w->ay += recvx_en01_ikou3(w, &pp, 910);                      /* :2425 */
        if (!(pl.flg & 4u)) {                                        /* :2426 */
            if (en26_eat_check(w, 4551, 5.5f, 0)) {                  /* :2428 */
                w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0; /* :2430-2433 */
                if (en26_player(w, &pl, "plp->flg (en26.c:2435)")) {
                    pl.flg |= 4u;                                    /* :2435 */
                    pl.flg |= 0x10000u;                              /* :2436 */
                    en26_player_set(w, &pl);
                }
                w->flg &= ~0x20u;                                    /* :2437 */
                break;                                               /* :2438 */
            }
            w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;                /* :2441-2443 */
        }
        break;
    default:
        break;
    }
}

/* bhEne26_MV07 (en26.c:2449). */
static void en26_mv07(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    int32_t frm;

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 17u, 0, 8);                           /* :2456 */
        en26_mtn_flags_clear(p);                                     /* :2457 */
        if (!en26_player(w, &pl, "plp (en26.c:2458)")) return;
        w->ayp = recvx_en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z); /* :2458 */
        w->ayp = (int32_t)(uint16_t)(w->ayp - w->ay);                /* :2459 */
        if (w->ayp < 32769) {                                        /* :2461 */
            w->ayp /= 22;                                            /* :2463 */
        } else {
            w->ayp = (-(65536 - w->ayp) / 22);                       /* :2467 */
            w->mtn_md |= 2u;                                         /* :2468 */
        }
        w->flg |= 0x40000u;                                          /* :2470 */
        w->mode3++;                                                  /* :2471 */
    /* FALLTHROUGH (:2472) */
    case 1:
        frm = w->frm_no / 65536;                                     /* :2474 */
        if (frm >= 17 && frm < 40) w->ay += w->ayp;                  /* :2475-2478 */
        if ((w->frm_no / 65536) == 40) {                             /* :2480 */
            w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;                /* :2482-2484 */
        }
        break;
    default:
        break;
    }
}

/* bhEne26_MV16 (en26.c:2491). */
static void en26_mv16(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_VEC proot;

    switch (w->mode3) {
    case 0:
        if (en26_player_joint_pos(w, 0, &proot)) w->ay += recvx_en01_ikou3(w, &proot, 910); /* :2496 */
        recvx_enemy_chg_mtn(w, 18u, 0, 5);                           /* :2497 */
        p->flags &= ~0x2000000u;                                     /* :2498 */
        p->flags |= 0x1000000u;                                      /* :2499 */
        w->mode3++;                                                  /* :2500 */
    /* FALLTHROUGH (:2501) */
    case 1:
        if (w->flg & 0x2000000u) {                                   /* :2503 */
            recvx_enemy_chg_mtn(w, 14u, 0, 0);                       /* :2505 */
            en26_mtn_flags_clear(p);                                 /* :2506 */
            w->mode3++;                                              /* :2507 */
        }
    /* FALLTHROUGH (:2509) */
    case 2:
        break;
    default:
        break;
    }
}

/* --- bhEne26_Move / bhEne26_MVType (en26.c:1860/:1882) ---------------- */
static void en26_move(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC pos;

    if (!p) { en26_gap(w, "bhEne26_Move: exp0"); return; }
    p->flags &= ~0x800u;                                             /* :1865 */
    if (!en26_player(w, &pl, "plp (en26.c:1867)")) return;
    if (!en26_player_joint_pos(w, 0, &pos)) return;                  /* :1867-1870 */
    pos.y = pl.pos.y;
    p->player_distance = recvx_en01_distance_p2p(&pos, w->px, w->py, w->pz); /* :1872 */
    if (w->mode2 != 9u) p->flags |= 0x800u;                          /* :1873-1876 */

    /* bhEne26_MVType (:1882) */
    {
        RECVX_ENEMY_VEC pp;
        pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;
        if (!en || !en->enemy_atari) {
            en26_gap(w, "bhEne_EnemyAtariCheck (zonzon.c:373)");
        } else if (en->enemy_atari(&pp, pl.flr_no, (uint8_t)w->id, 0)) { /* :1884 */
            p->flags |= 0x20000000u;                                 /* :1886 */
        } else {
            p->flags &= ~0x20000000u;                                /* :1890 */
        }
    }
    if (w->mode1 == 1u) en26_brain00(w);                             /* :1893-1896 */
    if (p->line_hit == 0) (void)en26_action_mode_check(w);           /* :1898-1901 */
    if (w->mode0 == 1u) {                                            /* :1903 */
        switch (w->mode2) {                                          /* bhEne26_MoveMode2 :571 */
        case 0:  en26_mv00(w); break;
        case 1:  en26_mv01(w); break;
        case 2:  en26_mv02(w); break;
        case 3:  en26_mv03(w); break;
        case 5:  en26_mv05(w); break;
        case 6:  en26_mv06(w); break;
        case 7:  en26_mv07(w); break;
        case 16: en26_mv16(w); break;
        default:
            /* 4 und 8..15 sind NULL (en26.c:577/581-588); >16 liegt hinter
               der Tabelle. */
            en26_gap(w, "bhEne26_MoveMode2[mode2] NULL/ausserhalb (en26.c:1905)");
            break;
        }
    }
}

/* --- bhEne26_Nage / NGType / NG00 (en26.c:2516/:2523/:2540) ----------- */
static void en26_ng00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    int32_t frm, ang;
    NJS_POINT3 ps, out;

    switch (w->mode3) {
    case 0:
        if (!en26_player(w, &pl, "plp (en26.c:2548)")) return;
        /* Beide sys->ply_id-Zweige sind im Original gleich (:2553-2562). */
        recvx_enemy_chg_mtn(w, 2u, 0, 0);                            /* :2555/:2560 */
        en26_mtn_flags_clear(p);                                     /* :2556/:2561 */
        w->ay = recvx_en01_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z); /* :2564 */
        w->flg &= ~0x40u;                                            /* :2565 */
        p->flags &= ~0xFu;                                           /* :2566 */
        p->flags |= 5u;                                              /* :2567 */
        p->flags &= ~0x4000000u;                                     /* :2568 */
        pl.mode0 = 4; pl.mode1 = 0; pl.mode2 = 0; pl.mode3 = 0;      /* :2569-2572 */
        en26_player_set(w, &pl);
        w->flg2 |= 1u;                                               /* :2573 */
        ps.x = 0.0f; ps.y = 0.0f;                                    /* :2574-2575 */
        if (p->flags & 0x4000u) {                                    /* :2577 */
            ps.z = -6.442887f;                                       /* :2579 */
            ang = (int32_t)(uint16_t)(w->ay + EN26_DEG_ANG(180.0f));  /* :2580 */
        } else {
            ps.z = 6.290813f;                                        /* :2584 */
            ang = w->ay;                                             /* :2585 */
        }
        njUnitMatrix(NULL);                                          /* :2588 */
        njTranslate(NULL, pl.pos.x, pl.pos.y, pl.pos.z);             /* :2589 */
        njRotateY(NULL, ang);                                        /* :2590 */
        njCalcPoint(NULL, &ps, &out);                                /* :2591 */
        w->px = out.x; w->py = out.y; w->pz = out.z;
        p->flags |= 0x20000u;                                        /* :2592 */
        w->mode3++;                                                  /* :2593 */
    /* FALLTHROUGH (:2594) */
    case 1:
        if ((w->frm_no / 65536) == 20) {                             /* :2596 */
            w->ct0 = 60;                                             /* :2598 */
            w->mode3++;                                              /* :2599 */
        }
        break;
    case 2:
        if (!en26_player(w, &pl, "plp (en26.c:2607)")) return;
        frm = w->frm_no / 65536;                                     /* :2604 */
        if (frm == 25 || frm == 60) {                                /* :2605 */
            pl.hp -= 30;                                             /* :2607 */
            if (pl.hp < 0) pl.stflg |= 0x40000u;                     /* :2608-2611 */
            en26_player_set(w, &pl);
        }
        if (!w->sv || !w->sv->lever_check) { en26_gap(w, "bhEne_LeverCheck (zonzon1.c)"); return; }
        w->ct0 -= w->sv->lever_check() + 1;                          /* :2613 */
        if (w->ct0 <= 0 || (w->flg & 0x2000000u)) {                  /* :2614 */
            if (pl.hp < 0) {                                         /* :2616 */
                recvx_enemy_chg_mtn(w, 9u, 0, 0);                    /* :2618 */
                en26_mtn_flags_clear(p);                             /* :2619 */
                p->flags &= ~0xFu;                                   /* :2620 */
                p->flags |= 5u;                                      /* :2621 */
                w->mode3 = 5;                                        /* :2622 */
                pl.mode0 = 6;                                        /* :2623 */
                pl.mode3 = 6;                                        /* :2624 */
            } else {
                recvx_enemy_chg_mtn(w, 1u, 0, 5);                    /* :2628 */
                en26_mtn_flags_clear(p);                             /* :2629 */
                w->flg |= 0x40000u;                                  /* :2630 */
                w->mode3++;                                          /* :2631 */
                pl.mode3 = 2;                                        /* :2632 */
                pl.ct1 = 0;                                          /* :2633 */
                p->flags &= ~0xFu;                                   /* :2634 */
                p->flags |= 6u;                                      /* :2635 */
                w->ct0 = 0;                                          /* :2636 */
                w->flg2 &= ~1u;                                      /* :2637 */
            }
            en26_player_set(w, &pl);
            w->flg |= 0x20u;                                         /* :2639 */
        }
        break;
    case 3: {
        const RECVX_EN01_SERVICES* en = en26_en();
        frm = w->frm_no / 65536;                                     /* :2644 */
        if (17 < frm && frm < 31) {                                  /* :2645 */
            w->spd = (float)(1.5f - w->ct0 * 0.14);                  /* :2647 */
            if (w->spd < 0.0f) w->spd = 0.05f;                       /* :2648-2651 */
            if (en && en->add_speed) en->add_speed(w, 32768);        /* :2652 */
            else en26_gap(w, "bhAddSpeed (pwksub.c:133)");
            w->ct0++;                                                /* :2653 */
        }
        if (frm == 20) p->flags |= 0x100u;                           /* :2656-2659 */
        if ((w->frm_no / 65536) == (int32_t)en26_motion_frames(w, w->mtn_no) - 1) { /* :2661 */
            recvx_enemy_chg_mtn(w, 26u, 0, 0);                       /* :2663 */
            en26_mtn_flags_clear(p);                                 /* :2664 */
            w->flg |= 0x40000u;                                      /* :2665 */
            p->flags &= ~0x80u;                                      /* :2666 */
            w->flg |= 0x40u;                                         /* :2667 */
            p->flags &= ~0xFu;                                       /* :2668 */
            p->flags |= 6u;                                          /* :2669 */
            w->mode3++;                                              /* :2670 */
        }
        break;
    }
    case 4:
        frm = w->frm_no / 65536;                                     /* :2675 */
        if (frm == 10) p->flags &= ~0x100u;                          /* :2676-2679 */
        if (frm == (int32_t)en26_motion_frames(w, w->mtn_no) - 1) {  /* :2681 */
            w->flg2 &= ~1u;                                          /* :2683 */
            p->flags &= ~0xFu;                                       /* :2684 */
            p->flags |= 1u;                                          /* :2685 */
            w->mtn_add = 0;                                          /* :2686 */
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;  /* :2687-2690 */
        }
        break;
    case 5:
        if (w->flg & 0x2000000u) {                                   /* :2695 */
            float m[16];
            if (!en26_joint_mtx(w, 0, m)) return;                    /* :2697 */
            w->px = m[12];                                           /* :2698 */
            w->pz = m[14];                                           /* :2699 */
            p->flags &= ~0x80000u;                                   /* :2700 */
            recvx_enemy_chg_mtn(w, 14u, 0, 0);                       /* :2701 */
            en26_mtn_flags_clear(p);                                 /* :2702 */
            p->flags &= ~0xFu;                                       /* :2703 */
            w->mode3++;                                              /* :2704 */
        }
        break;
    case 6:
        break;
    default:
        break;
    }
}

static void en26_nage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    if (!p) { en26_gap(w, "bhEne26_Nage: exp0"); return; }
    p->flags &= ~0x800u;                                             /* :2518 */
    if (w->mode3 == 0u) {                                            /* NGType :2525 */
        RECVX_ENEMY_PLAYER pl;
        if (!en26_player(w, &pl, "plp->ay (en26.c:2527)")) return;
        if (recvx_en01_cdir_check(pl.ay, (int32_t)w->ay) == 0) p->flags |= 0x4000u;  /* :2527-2530 */
        else                                                     p->flags &= ~0x4000u; /* :2533 */
    }
    if (w->mode2 == 0u) en26_ng00(w);                                /* NageMode2[1] :2536 */
    else en26_gap(w, "bhEne26_NageMode2[mode2] ausserhalb (en26.c:2536)");
}

/* --- bhEne26_PlyDG00 (en26.c:2715) -------------------------------------
   Nativ: w ist der greifende Gegner (epw), pl die Sicht auf den Spieler-
   besitzer (plp); der Aufrufer schreibt pl zurueck. */
static void en26_ply_dg00(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    static const RECVX_ENEMY_VEC ply_ofs_pos[8] = {                  /* :2717 */
        { 0.0f,      0.0f, -6.326351f  },
        { 0.0f,      0.0f, -6.326351f  },
        { 0.0f,      0.0f, -5.0f       },
        { 0.0f,      0.0f, -6.019392f  },
        { 0.167357f, 0.0f, -5.7556629f },
        { 0.0f,      0.0f, -5.9214029f },
        { 0.0f,      0.0f, -5.0f       },
        { 0.0f,      0.0f, -6.019392f  }
    };
    static const RECVX_ENEMY_VEC ply_ofs_pos2[8] = {                 /* :2729 */
        { 0.469302f, 0.0f, -5.826981f  },
        { 0.469231f, 0.0f, -5.499186f  },
        { 0.2f,      0.0f, -4.5f       },
        { 0.4f,      0.0f, -5.1999998f },
        { 0.469302f, 0.0f, -5.0f       },
        { 0.469231f, 0.0f, -4.9f       },
        { 0.2f,      0.0f, -4.5f       },
        { 0.4f,      0.0f, -5.1999998f }
    };
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    uint32_t frm, row;
    int32_t base;

    if (pl->ply_id >= 4u) { en26_gap(w, "En26_PlyMtn_OffsetTbl[ply_id] ausserhalb (en26.c:2801)"); return; }
    row = pl->ply_id * 2u;
    base = en26_ply_mtn_offset[pl->ply_id];

    switch (pl->mode3) {
    case 0:
        pl->flg &= ~0x40000u;                                        /* :2746 */
        pl->flg |= 0x10000u;                                         /* :2747 */
        pl->flg &= ~8u;                                              /* :2748 */
        pl->exp1_flags |= 0x1E0u;                                    /* :2749 */
        pl->exp1_flags &= ~4u;                                       /* :2750 */
        if (p->flags & 0x4000u) {                                    /* :2751 */
            pl->ayp = (int32_t)(uint16_t)((w->ay + 32768) - pl->ay); /* :2753 */
            w->waxp = (int16_t)-32768;                               /* :2754 */
        } else {
            pl->ayp = (int32_t)(uint16_t)(w->ay - pl->ay);           /* :2758 */
            w->waxp = 0;                                             /* :2759 */
        }
        if (32768 < pl->ayp) pl->ayp = (pl->ayp - 32768) - 32768;    /* :2762-2765 */
        w->waxp = (int16_t)(w->waxp - pl->ayp);                      /* :2766 */
        pl->ayp /= 5;                                                /* :2767 */
        pl->ct0 = 0;                                                 /* :2768 */
        pl->mode3++;                                                 /* :2769 */
    /* FALLTHROUGH (:2770) */
    case 1:
        if (pl->ct0 < 5) {                                           /* :2772 */
            w->waxp = (int16_t)(w->waxp + pl->ayp);                  /* :2774 */
        } else if (pl->ct0 == 5) {                                   /* :2776 */
            w->waxp = (p->flags & 0x4000u) ? (int16_t)-32768 : 0;    /* :2778-2785 */
        }
        pl->ct0++;                                                   /* :2788 */
        frm = (uint32_t)(w->frm_no / 65536);                         /* :2789 */
        if (frm == 1u) {                                             /* :2790 */
            pl->mtn_enemy = 1;                                       /* pl->mnwP = epw->mnwP :2792 */
            pl->mtn_enemy_owner = w->id;
            pl->mtn_enemy_work = w;
            pl->frm_no = 0;                                          /* :2793 */
            pl->hokan_count = 5;                                     /* :2794 */
            pl->hokan_rate = 42598;                                  /* :2795 */
            pl->mtn_add = 65536;                                     /* :2796 */
            pl->mtn_md = 0;                                          /* :2797 */
            p->flags |= 0x80u;                                       /* :2798 */
            if (p->flags & 0x4000u) {                                /* :2799 */
                pl->mtn_no = (uint32_t)(base + 6);                   /* :2801 */
                p->grab_point = ply_ofs_pos[row];                    /* :2802-2804 */
                w->waxp = (int16_t)-32768;                           /* :2805 */
                p->flags |= 0x80000u;                                /* :2806 */
                break;                                               /* :2807 */
            }
            pl->mtn_no = (uint32_t)(base + 7);                       /* :2809 */
            p->grab_point = ply_ofs_pos[row + 1u];                   /* :2810-2812 */
            w->waxp = 0;                                             /* :2813 */
            p->flags |= 0x80000u;                                    /* :2814 */
        } else if (frm == 12u) {                                     /* :2816 */
            en26_player_se_play(w, 1026);                            /* :2818 */
        }
        break;
    case 2:
        pl->exp1_flags |= 4u;                                        /* :2823 */
        pl->frm_no = 0;                                              /* :2824 */
        pl->hokan_count = 0;                                         /* :2825 */
        pl->hokan_rate = 32768;                                      /* :2826 */
        pl->mtn_add = 65536;                                         /* :2827 */
        pl->mtn_no = (uint32_t)(base + ((p->flags & 0x4000u) ? 10 : 11)); /* :2830/:2834 */
        p->flags &= ~0x80000u;                                       /* :2837 */
        pl->mode3++;                                                 /* :2838 */
    /* FALLTHROUGH (:2839) */
    case 3:
        frm = (uint32_t)(pl->frm_no / 65536);                        /* :2841 */
        if (frm == 14u)      recvx_en01_enemy_push_chk(w, 1);        /* :2844 */
        else if (frm == 17u) recvx_en01_enemy_push_chk(w, 0);        /* :2848 */
        {
            uint32_t frames = 0;
            if (w->sv && w->sv->player_motion_frames) frames = w->sv->player_motion_frames(pl->mtn_no, pl->mtn_enemy ? 1 : 0);
            else { en26_gap(w, "plp->mnwP[mtn_no].frm_num (en26.c:2851)"); break; }
            /* frm_no / 65536 gegen das vorzeichenlose frm_num - 1 (:2851). */
            if ((uint32_t)(pl->frm_no / 65536) >= frames - 1u) {        /* :2851 */
                en26_clear_pad_on(w);                                /* :2853 */
                pl->flg &= ~0x10000u;                                /* :2854 */
                pl->flg |= 8u;                                       /* :2855 */
                pl->stflg &= ~0x10000u;                              /* :2856 */
                pl->at_flg = 0;                                      /* :2857 */
                pl->mtn_enemy = 0;                                   /* pl->mnwP = pl->mnwPb :2858 */
                pl->mtn_enemy_owner = 0;
                pl->mtn_enemy_work = NULL;
                pl->mode0 = 1; pl->mode1 = 0; pl->mode2 = 0; pl->mode3 = 0; /* *(int*)&pl->mode0 = 1 :2859 */
                pl->flg &= ~4u;                                      /* plp->flg &= ~4 :2860 */
                p->flags &= ~0x4000u;                                /* :2861 */
                p->flags &= ~0x20000u;                               /* :2862 */
            }
        }
        break;
    case 4:
        pl->flg &= ~0x40000u;                                        /* :2867 */
        pl->flg |= 0x10000u;                                         /* :2868 */
        pl->flg &= ~8u;                                              /* :2869 */
        pl->exp1_flags |= 0x1E0u;                                    /* :2870 */
        pl->exp1_flags &= ~4u;                                       /* :2871 */
        if (p->flags & 0x4000u) {                                    /* :2872 */
            pl->mtn_no = (uint32_t)(base + 8);                       /* :2874 */
            p->grab_point = ply_ofs_pos2[row];                       /* :2875-2877 */
            p->flags |= 0x80000u;                                    /* :2878 */
            pl->ayp = (int32_t)(uint16_t)((w->ay + 32768) - pl->ay); /* :2879 */
            w->waxp = (int16_t)-32768;                               /* :2880 */
        } else {
            pl->mtn_no = (uint32_t)(base + 9);                       /* :2884 */
            p->grab_point = ply_ofs_pos2[row + 1u];                  /* :2885-2887 */
            w->waxp = 0;                                             /* :2888 */
            p->flags |= 0x80000u;                                    /* :2889 */
            pl->ayp = (int32_t)(uint16_t)(w->ay - pl->ay);           /* :2890 */
            w->waxp = 0;                                             /* :2891 */
        }
        pl->mtn_enemy = 1;                                           /* pl->mnwP = epw->mnwP :2894 */
        pl->mtn_enemy_owner = w->id;
        pl->mtn_enemy_work = w;
        pl->frm_no = 0;                                              /* :2895 */
        pl->hokan_count = 5;                                         /* :2896 */
        pl->hokan_rate = 42598;                                      /* :2897 */
        pl->mtn_add = 65536;                                         /* :2898 */
        pl->mtn_md = 0;                                              /* :2899 */
        p->flags |= 0x80u;                                           /* :2900 */
        en26_player_se_play(w, 1026);                                /* :2901 */
        if (32768 < pl->ayp) pl->ayp = (pl->ayp - 32768) - 32768;    /* :2903-2906 */
        w->waxp = (int16_t)(w->waxp - pl->ayp);                      /* :2907 */
        pl->ayp /= 5;                                                /* :2908 */
        pl->ct0 = 0;                                                 /* :2909 */
        pl->mode3++;                                                 /* :2910 */
    /* FALLTHROUGH (:2911) */
    case 5:
        if (pl->ct0 < 5) {                                           /* :2913 */
            w->waxp = (int16_t)(w->waxp + pl->ayp);                  /* :2915 */
        } else if (pl->ct0 == 5) {                                   /* :2917 */
            w->waxp = (p->flags & 0x4000u) ? (int16_t)-32768 : 0;    /* :2919-2926 */
        }
        pl->ct0++;                                                   /* :2928 */
        break;
    case 6:
        pl->frm_no = 0;                                              /* :2932 */
        pl->hokan_count = 0;                                         /* :2933 */
        pl->hokan_rate = 32768;                                      /* :2934 */
        pl->mtn_add = 65536;                                         /* :2935 */
        pl->flg &= ~0x10u;                                           /* :2936 */
        pl->mtn_no = (uint32_t)(base + ((p->flags & 0x4000u) ? 12 : 13)); /* :2939/:2943 */
        en26_player_se_play(w, 1025);                                /* :2945 */
        pl->mode3++;                                                 /* :2946 */
    /* FALLTHROUGH (:2947) */
    case 7:
        frm = (uint32_t)(pl->frm_no / 65536);                        /* :2949 */
        if (p->flags & 0x4000u) {                                    /* :2951 */
            if (frm == 22u || frm == 30u) en26_se_play(w, 8963);     /* :2953-2956 */
        } else {
            if (frm == 16u || frm == 22u) en26_se_play(w, 8963);     /* :2960-2963 */
        }
        {
            uint32_t frames = 0;
            if (w->sv && w->sv->player_motion_frames) frames = w->sv->player_motion_frames(pl->mtn_no, pl->mtn_enemy ? 1 : 0);
            else { en26_gap(w, "plp->mnwP[mtn_no].frm_num (en26.c:2966)"); break; }
            if (frm >= frames - 1u) {                                /* :2966, vorzeichenlos */
                pl->mtn_add = 0;                                     /* plp->mtn_add = 0 :2968 */
                en26_clear_pad_on(w);                                /* :2969 */
                pl->flg &= ~0x10000u;                                /* :2970 */
                pl->stflg &= ~0x10000u;                              /* :2971 */
                pl->at_flg = 0;                                      /* :2972 */
                pl->flg |= 2u;                                       /* :2973 */
                p->flags &= ~0x4000u;                                /* :2974 */
                p->flags &= ~0x20000u;                               /* :2975 */
                p->flags &= ~0x80000u;                               /* :2976 */
                pl->flg &= ~4u;                                      /* plp->flg &= ~4 :2977 */
            }
        }
        break;
    default:
        en26_gap(w, "bhEne26_PlyDG00: mode3 ausserhalb (en26.c:2743)");
        break;
    }
}

/* --- bhEne26_Damage / DGType / DG00..DG15 (en26.c:2984-3192) ---------- */
static void en26_dg02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    int32_t frm;

    switch (w->mode3) {
    case 0:
        if (p->flags & 0x2000u) {                                    /* :3016 */
            recvx_enemy_chg_mtn(w, (w->mtn_no == 0x10u) ? 27u : 5u, 0, 5); /* :3020/:3025 */
        } else {
            recvx_enemy_chg_mtn(w, (w->mtn_no == 16u) ? 28u : 6u, 0, 5);   /* :3033/:3038 */
        }
        en26_mtn_flags_clear(p);                                     /* :3021/:3026/:3034/:3039 */
        w->flg |= 0x40000u;                                          /* :3044 */
        p->flags &= ~0xFu;                                           /* :3045 */
        p->flags |= 0x1u;                                            /* :3046 */
        w->mode3++;                                                  /* :3047 */
    /* FALLTHROUGH (:3048) */
    case 1:
        frm = w->frm_no / 65536;                                     /* :3050 */
        if ((frm == 28 && w->mtn_no == 5u) || (frm == 28 && w->mtn_no == 27u) ||
            (frm == 30 && w->mtn_no == 6u) || (frm == 30 && w->mtn_no == 28u)) { /* :3052-3055 */
            RECVX_ENEMY_PLAYER pl;
            RECVX_ENEMY_VEC pp;
            if (!en26_player(w, &pl, "plp (en26.c:3057)")) break;
            pp.x = pl.pos.x; pp.y = pl.pos.y; pp.z = pl.pos.z;
            if (w->flr_no == pl.flr_no && p->player_distance > 15.0f &&
                recvx_en01_ikou3(w, &pp, 24576)) {                   /* :3057-3059 */
                w->mode0 = 1; w->mode1 = 0; w->mode2 = 7; w->mode3 = 0; /* :3061-3064 */
            } else {
                w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; /* :3068-3071 */
            }
            p->flags &= ~0x200u;                                     /* :3074 */
            p->flags &= ~0xFu;                                       /* :3075 */
            p->flags |= 0x1u;                                        /* :3076 */
        }
        break;
    default:
        break;
    }
}

static void en26_dg03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);

    switch (w->mode3) {
    case 0:
        p->flags &= ~0xFu;                                           /* :3088 */
        p->flags |= 6u;                                              /* :3089 */
        w->spd = 0.0f;                                               /* :3090 */
        recvx_enemy_chg_mtn(w, 3u, 0, 10);                           /* :3091 */
        en26_mtn_flags_clear(p);                                     /* :3092 */
        w->flg |= 0x40000u;                                          /* :3093 */
        w->mode3++;                                                  /* :3094 */
    /* FALLTHROUGH (:3095) */
    case 1:
        if ((w->frm_no / 65536) == (int32_t)en26_motion_frames(w, w->mtn_no) - 1) { /* :3097 */
            p->flags &= ~0xFu;                                       /* :3098 */
            p->flags |= 6u;                                          /* :3099 */
            w->mtn_add = 0;                                          /* :3100 */
            p->flags &= ~0x8000u;                                    /* :3101 */
            w->mode0 = 4; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;  /* :3102-3105 */
        }
        break;
    default:
        break;
    }
}

static void en26_dg04(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();
    int32_t frm, ang;

    switch (w->mode3) {
    case 0:
        p->flags &= ~0xFu;                                           /* :3120 */
        p->flags |= 6u;                                              /* :3121 */
        recvx_enemy_chg_mtn(w, 4u, 0, 10);                           /* :3122 */
        en26_mtn_flags_clear(p);                                     /* :3123 */
        w->flg |= 0x40000u;                                          /* :3124 */
        if (!(p->flags & 0x200u)) {                                  /* :3125 */
            w->ayp = (int32_t)(10430.381f * atan2f(w->dvx, w->dvz)); /* :3127 */
        }
        w->mode3++;                                                  /* :3129 */
    /* FALLTHROUGH (:3130) */
    case 1:
        frm = w->frm_no / 65536;                                     /* :3133 */
        if (!(p->flags & 0x200u)) {                                  /* :3134 */
            if (frm < 10) {                                          /* :3136 */
                ang = (int32_t)(uint16_t)(w->ayp - w->ay);           /* :3138 */
                if (ang > EN26_DEG_ANG(180.0f)) ang -= EN26_DEG_ANG(360.0f); /* :3139-3142 */
                w->ay += (ang / 2);                                  /* :3143 */
            }
        }
        if (frm == 10) p->flags &= ~0x100u;                          /* :3147-3150 */
        if (16 <= frm && frm < 21) {                                 /* :3152 */
            w->spd = 0.49733332f;                                    /* :3154 */
            if (en && en->add_speed) en->add_speed(w, 32768);        /* :3155 */
            else en26_gap(w, "bhAddSpeed (pwksub.c:133)");
        }
        if (frm == (int32_t)en26_motion_frames(w, w->mtn_no) - 1) {  /* :3158 */
            p->flags &= ~0xFu;                                       /* :3160 */
            p->flags |= 6u;                                          /* :3161 */
            w->mtn_add = 0;                                          /* :3162 */
            p->flags |= 0x8000u;                                     /* :3163 */
            w->mode0 = 4; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;  /* :3164-3167 */
        }
        break;
    default:
        break;
    }
}

static void en26_dg15(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);

    switch (w->mode3) {
    case 0:
        recvx_enemy_chg_mtn(w, 16u, 0, 10);                          /* :3179 */
        en26_mtn_flags_clear(p);                                     /* :3180 */
        w->flg |= 0x40000u;                                          /* :3181 */
        w->mode3++;                                                  /* :3182 */
    /* FALLTHROUGH (:3183) */
    case 1:
        if (w->flg & 0x2000000u) {                                   /* :3185 */
            w->mode2 = 3;                                            /* :3187 */
            w->mode3 = 0;                                            /* :3188 */
        }
        break;
    default:
        break;
    }
}

static void en26_damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    if (!p) { en26_gap(w, "bhEne26_Damage: exp0"); return; }
    p->flags &= ~0x800u;                                             /* :2986 */
    switch (w->mode2) {                                              /* bhEne26_DamageMode2[16] :597 */
    case 0:                                                          /* bhEne26_DG00 :2997, leer */
    case 1:                                                          /* bhEne26_DG01 :3003, leer */
        break;
    case 2:  en26_dg02(w); break;
    case 3:  en26_dg03(w); break;
    case 4:  en26_dg04(w); break;
    case 15: en26_dg15(w); break;
    default:
        /* 5..14 sind NULL (en26.c:604-613); >15 liegt hinter der Tabelle. */
        en26_gap(w, "bhEne26_DamageMode2[mode2] NULL/ausserhalb (en26.c:2993)");
        break;
    }
}

/* --- bhEne26_Die / DDType / DD00 (en26.c:3195/:3210/:3216) ------------ */
static void en26_dd00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    const RECVX_EN01_SERVICES* en = en26_en();

    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_VEC pos;
        recvx_enemy_chg_mtn(w, (p->flags & 0x8000u) ? 11u : 10u, 0, 0); /* :3221-3229 */
        en26_mtn_flags_clear(p);                                     /* :3224/:3229 */
        w->mtn_add = 0;                                              /* :3232 */
        w->flg |= 2u;                                                /* :3233 */
        w->flg &= ~0x128u;                                           /* :3234 */
        w->ct0 = 0;                                                  /* :3235 */
        w->ct1 = (en26_rand(w) % 5) + 10;                            /* :3236 */
        pos.x = w->px; pos.y = w->py; pos.z = w->pz;
        if (en && en->blood_pool_lnk) (void)en->blood_pool_lnk(w, &pos, w->ay, &recvx_en01_bld_tbl, 0); /* :3237 */
        else en26_gap(w, "bhSetBloodPoolLnk (effsub3.c:145)");
        w->mode3++;                                                  /* :3238 */
    }
    /* FALLTHROUGH (:3239) */
    case 1:
        if (--w->ct0 < 0) {                                          /* :3241 */
            w->mtn_add = 65536;                                      /* :3243 */
            w->mode3++;                                              /* :3244 */
        }
        break;
    case 2:
        if ((w->frm_no / 65536) == (int32_t)en26_motion_frames(w, w->mtn_no) - 1) { /* :3248 */
            w->mtn_add = 0;                                          /* :3250 */
            if (w->ct1-- > 0) {                                      /* :3251 */
                w->ct0 = (en26_rand(w) % 50) + 10;                   /* :3253 */
                w->mode3 = 1;                                        /* :3254 */
            } else {
                w->mode3++;                                          /* :3258 */
            }
        }
    /* FALLTHROUGH (:3261) */
    case 3:
        break;
    default:
        break;
    }
}

static void en26_die(RECVX_ENEMY_WORK* w)
{
    RECVX_EN26_EXP0* p = recvx_en26_exp0(w);
    if (!p) { en26_gap(w, "bhEne26_Die: exp0"); return; }
    p->flags &= ~0x800u;                                             /* :3197 */
    if (p->fire_mask != 0) {                                         /* :3198 */
        if ((p->mat_color & 0x00FFFFFFu) > 0u) p->mat_color += 0xFFFEFEFFu; /* :3199-3202 */
        w->mdflg |= 0x400u;                                          /* :3203 */
        en26_set_material_color(w, p->mat_color);                    /* :3204 */
    }
    if (w->mode2 == 0u) en26_dd00(w);                                /* bhEne26_DieMode2[1] :3212 */
    else en26_gap(w, "bhEne26_DieMode2[mode2] ausserhalb (en26.c:3212)");
}

/* --- Effekte (en26.c:3335/:3361/:3390) --------------------------------- */
static void en26_exp_head_effect(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    float m[16];
    NJS_POINT3 ps, pd;
    RECVX_ENEMY_VEC pdv, v;
    int32_t ang1;
    int i;

    if (!en26_joint_mtx(w, 12, m)) return;                           /* :3344 */
    ps.x = 0.0f; ps.y = 0.5f; ps.z = -1.0f;                          /* :3345-3347 */
    njCalcPoint((NJS_MATRIX*)m, &ps, &pd);                           /* :3348 */
    pdv.x = pd.x; pdv.y = pd.y; pdv.z = pd.z;
    for (i = 0; i < 4; i++) {                                        /* :3350 */
        /* Das Original addiert hier epw->py (float), nicht ay (:3352). */
        ang1 = (int32_t)(((float)EN26_DEG_ANG(45.0f) + w->py) + (float)(i * EN26_DEG_ANG(90.0f)));
        v.x = -njSin(ang1) * njCos(EN26_DEG_ANG(22.5f));             /* :3353 */
        v.y = njSin(EN26_DEG_ANG(22.5f));                            /* :3354 */
        v.z = -njCos(ang1) * njCos(EN26_DEG_ANG(22.5f));             /* :3355 */
        if (en && en->nikuhen_effect) en->nikuhen_effect(w, 0, &pdv, &v, 7); /* :3356 */
        else en26_gap(w, "bhEne_SetNikuhenEffect (zonzon.c:768)");
    }
}

static void en26_neck_blood_effect(RECVX_ENEMY_WORK* w, int type)
{
    const RECVX_EN01_SERVICES* en = en26_en();
    RECVX_ENEMY_VEC ofs = { 0.0f, 1.0f, 0.0f };                      /* :3363 */
    RECVX_ENEMY_VEC v;
    int32_t ang;
    int i;

    for (i = 0, ang = EN26_DEG_ANG(45.0f); i < 4; ang += EN26_DEG_ANG(90.0f), i++) { /* :3369 */
        ofs.y = -1.0f;                                               /* :3371 */
        if (type != 0) continue;                                     /* :3372 */
        ofs.y = 0.0f;                                                /* :3374 */
        v.x = -(njSin(ang) * njCos(EN26_DEG_ANG(56.25f)));           /* :3376-3378 */
        v.y = njSin(EN26_DEG_ANG(56.25f));
        v.z = -(njCos(ang) * njCos(EN26_DEG_ANG(56.25f)));
        if (en && en->set_blood3) en->set_blood3(w, 12, &ofs, &v, 6, 8, 1, i); /* :3379 */
        else { en26_gap(w, "bhEne_SetBlood3 (zonzon.c:634)"); return; }
        v.x = -(njSin(ang) * njCos(EN26_DEG_ANG(22.5f)));            /* :3381-3383 */
        v.y = njSin(EN26_DEG_ANG(22.5f));
        v.z = -(njCos(ang) * njCos(EN26_DEG_ANG(22.5f)));
        en->set_blood3(w, 12, &ofs, &v, 3, 8, 3, i);                 /* :3384 */
    }
}

static void en26_kami_effect(RECVX_ENEMY_WORK* w, int32_t frm)
{
    const RECVX_EN01_KAMI* keff;
    RECVX_ENEMY_VEC pd;
    int j;

    for (keff = en26_keff_tbl; keff->mtn_no != -1; keff++) {         /* :3400-3405 */
        if ((int32_t)w->mtn_no != keff->mtn_no) continue;            /* :3407 */
        for (j = 0; j < 4; j++) {                                    /* :3409 */
            if (frm != keff->frm[j]) continue;                       /* :3411 */
            pd.x = 0.0f; pd.y = 0.0f; pd.z = -1.8f;                  /* :3413-3415 */
            w->djnt_no = 13;                                         /* :3417 */
            en26_set_blood2(w, 1u, &pd, 0);                          /* :3418 */
            if (j == 0) {                                            /* :3420 */
                w->djnt_no = 13;                                     /* :3422 */
                en26_set_blood2(w, 4u, &pd, (int16_t)w->ay);         /* :3423 */
            }
            return;                                                  /* :3426 */
        }
    }
}
