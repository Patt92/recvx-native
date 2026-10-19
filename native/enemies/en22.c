/* Identitaet 22 — Albinoid Adult (en22.c). Eigenstaendige native Fassung der
   Zustandsmaschine mit den Diensten aus enemy_work.h, en01_work.h (EN01-Satz
   ueber recvx_en01_context), en03_lib.h (zonzon/zonzon1-Helfer) und
   en22.h (Lichtplatz 2, Effektplaetze, Materialfarben). Jede Funktion nennt
   ihre Originalzeilen. */
#include "en22.h"
#include "en01_core.h"
#include "en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(RECVX_EN22_EXP0) == 0xB0u, "bhEne_CallocWork(0xB0, 8)");
_Static_assert(offsetof(RECVX_EN22_EXP0, flags) == 0x08u, "EXP0_I(0x8)");
_Static_assert(offsetof(RECVX_EN22_EXP0, dist) == 0x20u, "EXP0_F(0x20)");
_Static_assert(offsetof(RECVX_EN22_EXP0, trg_z) == 0x2Cu, "EXP0_F(0x2C)");
_Static_assert(offsetof(RECVX_EN22_EXP0, key_z) == 0x44u, "EXP0_F(0x44)");
_Static_assert(offsetof(RECVX_EN22_EXP0, atr17) == 0x48u, "EXP0_ATR(17) = 4 + 4*17");
_Static_assert(offsetof(RECVX_EN22_EXP0, light_effect) == 0x4Cu, "exp0 + 0x4C");
_Static_assert(offsetof(RECVX_EN22_EXP0, reach) == 0x50u, "EXP0_F(0x50)");
_Static_assert(offsetof(RECVX_EN22_EXP0, word5C) == 0x5Cu, "EXP0_I(0x5C)");
_Static_assert(offsetof(RECVX_EN22_EXP0, argb) == 0x60u, "exp0 + 0x60");

/* --- Tabellen (en22.c:15-505), reine Daten --------------------------------- */
static const int8_t en22_flip_tree[43] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    19, 20, 21, 22, 23, 24, 25,
    12, 13, 14, 15, 16, 17, 18,
    34, 35, 36, 37, 38, 39, 40, 41,
    26, 27, 28, 29, 30, 31, 32, 33,
    -1
};
typedef struct { int32_t frm, act; } EN22_MTN_ATB;
typedef struct { int32_t no; EN22_MTN_ATB atb[4]; } EN22_MTN_WORK;
static const EN22_MTN_WORK en22_mtn_tbl[12] = {                          /* :24 */
    { 2, {{ 0,    74504}, {38,    74504}, {-1, 0}, {-1, 0}}},
    { 5, {{ 9,    74496}, {-1,        0}, {-1, 0}, {-1, 0}}},
    { 6, {{ 0,    74504}, {36,    74504}, {-1, 0}, {-1, 0}}},
    { 7, {{ 4,    74504}, {-1,        0}, {-1, 0}, {-1, 0}}},
    { 8, {{ 0,    74504}, {-1,        0}, {-1, 0}, {-1, 0}}},
    { 9, {{10, 16851713}, {-1,        0}, {-1, 0}, {-1, 0}}},
    {10, {{ 0, 16786180}, {20,    74505}, {-1, 0}, {-1, 0}}},
    {11, {{ 0, 16786180}, {-1,        0}, {-1, 0}, {-1, 0}}},
    {13, {{ 0, 16786186}, {-1,        0}, {-1, 0}, {-1, 0}}},
    {19, {{ 0,    74505}, {85, 16786181}, {-1, 0}, {-1, 0}}},
    {20, {{ 0, 16786187}, {-1,        0}, {-1, 0}, {-1, 0}}},
    {-1, {{ 0,        0}, { 0,        0}, { 0, 0}, { 0, 0}}}
};
/* WPNDAMAGE_WORK (en22.c:38): flg, nm_act, cb_act, nm_blood, cb_blood (Lesestellen :641-650, :719-725). */
typedef struct { int32_t flg, nm_act, cb_act, nm_blood, cb_blood; } EN22_WPNDAMAGE;
static const EN22_WPNDAMAGE en22_wpn_damage[22] = {
    { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 4, 3, 3, 3}, { 0, 4, 7, 3, 3}, { 0, 4, 7, 3, 3}, { 0, 4, 7, 3, 3},
    { 0, 4, 7, 3, 3}, { 0, 4, 7, 3, 3}, { 0, 4, 7, 3, 3}, { 0, 4, 7, 3, 3}, { 0, 4, 7, 3, 3}, { 0, 0, 5, 3, 5},
    { 0, 4, 7, 3, 3}, { 0, 4, 3, 3, 3}, { 0, 0, 1, 3, 3}, {12, 0, 1, 3, 3}, { 4, 0, 1, 3, 3}, { 0, 0, 1, 3, 3},
    { 0, 0, 1, 3, 3}, { 0, 0, 1, 3, 3}, { 0, 0, 1, 3, 3}, { 0, 0, 1, 3, 3}
};
static const RECVX_T3_COMBWEP en22_comb_wep[21] = {                      /* :63 */
    {  0, { 0, 0, 0}, 0, 0}, {  0, { 0, 0, 0}, 0, 0}, { 40, {10, 8, 5}, 0, 0}, { 40, {10, 8, 5}, 0, 0},
    { 40, {10, 8, 5}, 0, 0}, { 90, {10, 8, 5}, 0, 0}, {  0, { 0, 0, 0}, 0, 0}, { 40, {10, 8, 5}, 0, 0},
    {160, {10, 8, 5}, 0, 0}, { 40, {10, 8, 5}, 0, 0}, { 80, {10, 8, 0}, 0, 0}, {  0, { 0, 0, 0}, 0, 0},
    {160, {10, 8, 5}, 0, 0}, {  0, { 0, 0, 0}, 0, 0}, {  0, { 0, 0, 0}, 0, 0}, {  0, { 0, 0, 0}, 0, 0},
    {  0, { 0, 0, 0}, 0, 0}, {  0, { 0, 0, 0}, 0, 0}, {  0, { 0, 0, 0}, 0, 0}, {  0, { 0, 0, 0}, 0, 0},
    {  0, { 0, 0, 0}, 0, 0}
};
static const RECVX_T3_COMBJOINT en22_comb_joint[42] = {{0,0}};             /* :87 alle {0,0} */
static const RECVX_ENEMY_CPCL en22_cap_col[27] = {                         /* :131 */
    {  2,  2, 20}, {  0,  0, 20}, {  2,  2, 23}, {-25,  0, 20}, {  2,  2, 23}, { 25,  0, 20},
    {  3,  4, 30}, {  4,  5, 22}, {  5,  6, 15}, {  6,  7, 18}, {  7,  8, 12}, {  8,  9, 11},
    {  9, 10, 11}, { 10, 11, 10}, { 19, 20, 11}, { 20, 21,  9}, { 12, 13, 11}, { 13, 14,  9},
    { 34, 34, 20}, {  8,  0,  0}, { 35, 36,  8}, { 36, 37,  8}, { 26, 26, 20}, { -8,  0,  0},
    { 27, 28,  8}, { 28, 29,  8}, {  0,  0,  0}
};
/* BT_WORK (en22.c:160): {lnk_obj, x, y, z, xlen, ylen, size, len}; Zeile i hat lnk_obj i, ab 2 size 5. */
#define EN22_BT(i) { (i), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 0.0f }
static const RECVX_T3_BT en22_prt_blood_tbl[42] = {
    { 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, { 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    EN22_BT(2), EN22_BT(3), EN22_BT(4), EN22_BT(5), EN22_BT(6), EN22_BT(7), EN22_BT(8), EN22_BT(9),
    EN22_BT(10), EN22_BT(11), EN22_BT(12), EN22_BT(13), EN22_BT(14), EN22_BT(15), EN22_BT(16), EN22_BT(17),
    EN22_BT(18), EN22_BT(19), EN22_BT(20), EN22_BT(21), EN22_BT(22), EN22_BT(23), EN22_BT(24), EN22_BT(25),
    EN22_BT(26), EN22_BT(27), EN22_BT(28), EN22_BT(29), EN22_BT(30), EN22_BT(31), EN22_BT(32), EN22_BT(33),
    EN22_BT(34), EN22_BT(35), EN22_BT(36), EN22_BT(37), EN22_BT(38), EN22_BT(39), EN22_BT(40), EN22_BT(41)
};
static const int8_t en22_sdw_tab[7] = { 2, 7, 13, 20, 28, 36, -1 };      /* :205 */
typedef struct { int32_t mtn, frm, obj; } EN22_WSP_WORK;                  /* :208 */
static const EN22_WSP_WORK en22_wsp_tbl[57] = {
    {13,  5, 37}, {13,  6, 35}, {13,  6, 29}, {13,  6,  6}, {13,  7,  2}, {13,  8,  4}, {13,  8,  9}, {13, 10, 13},
    {13, 10, 20}, {10,  0,  2}, {10,  3, 11}, {10,  5, 29}, {10,  5,  9}, {10,  7,  7}, {10, 10, 37}, {10, 12,  0},
    {10, 17,  0}, {11,  3, 28}, {11,  4,  0}, {11,  5, 13}, {11,  6,  7}, {11,  9, 36}, {11, 10, 20}, {11, 11, 10},
    {11, 13, 11}, {19,  5,  2}, {19, 10, 10}, {19, 11,  4}, {19, 12, 29}, {19, 12, 11}, {19, 16, 37}, {19, 25,  0},
    {19, 38,  0}, {19, 51,  0}, {19, 65,  0}, {19, 77, 29}, {19, 79, 37}, {19, 83,  4}, {19, 88,  2}, {20,  3, 13},
    {20,  3,  4}, {20,  4, 20}, {20,  5, 28}, {20,  5, 36}, {20,  6,  2}, {20,  6, 10}, {20, 24,  2}, {20, 27, 28},
    {20, 29, 36}, {20, 29,  6}, {20, 31,  9}, {20, 45,  3}, {20, 50,  0}, {20, 52,  9}, {20, 52, 28}, {20, 52, 36},
    {-1,  0,  0}
};
typedef struct { int32_t mtn, frm, obj; } EN22_WEFF_WORK;                 /* :268 */
static const EN22_WEFF_WORK en22_weff_tbl[14] = {
    { 2,  0, 28}, { 2, 34, 36}, { 5, 17,  2}, { 6, 23,  2}, { 6, 10,  2}, { 9, 37,  4}, {10, 20,  4},
    {11, 13,  2}, {13,  8,  2}, {19,  8,  4}, {19, 88,  4}, {20, 16,  3}, {20, 36,  3}, {-1,  0,  0}
};
typedef struct { float px, pz; } EN22_POINT2_XZ;                           /* :284 */
static const EN22_POINT2_XZ ply_mtn42b_pos[20] = {
    { 0.0f, -3.220037f }, { 0.0f, -1.4619832f }, { 0.0f, -1.447103f }, { 0.0f, -1.4290719f }, { 0.0f, -1.4078922f },
    { 0.0f, -1.383564f }, { 0.0f, -1.3560877f }, { 0.0f, -1.3254604f }, { 0.0f, -1.2916822f }, { 0.0f, -1.2547579f },
    { 0.0f, -1.2146845f }, { 0.0f, -1.1714611f }, { 0.0f, -1.1250877f }, { 0.000066f, -0.9541111f },
    { 0.00015499999f, -0.6899109f }, { 0.000176f, -0.46962357f }, { 0.00013200002f, -0.29325867f },
    { 0.000021999993f, -0.16081047f }, {-0.00015400001f, -0.07228279f }, {-0.000397f, -0.027669907f }
};
static const EN22_POINT2_XZ ply_mtn43b_pos[20] = {
    {0.0f, 5.8285589f }, {0.0f, 2.0167189f }, {0.0f, 1.8550768f }, {0.0f, 1.7206335f }, {0.0f, 1.6133881f },
    {0.0f, 1.5333433f }, {0.0f, 1.4804945f }, {0.0f, 1.4548454f }, {0.0f, 1.4563942f }, {0.0f, 0.95617104f},
    {0.0f, 0.57528687f}, {0.0f, 0.7033367f }, {0.0f, 0.6761093f }, {0.0f, 0.61144829f}, {0.0f, 0.49389648f},
    {0.0f, 0.34840584f}, {0.0f, 0.27017784f}, {0.0f, 0.24227142f}, {0.0f, 0.18896675f}, {0.0f, 0.1102581f }
};
static const EN22_POINT2_XZ ply_mtn44b_pos[50] = {
    {-0.00059f, -0.0079956055f}, {-0.00068f, 0.014802933f }, {-0.00075900008f, 0.035778046f }, {-0.000825f, 0.054922104f },
    {-0.00087899994f, 0.072244644f }, {-0.00091999979f, 0.08773613f }, {-0.00095000025f, 0.10140228f }, {-0.00096700015f, 0.11324501f },
    {-0.00097199995f, 0.12325668f }, {-0.00096500013f, 0.13144493f }, {-0.00094499998f, 0.13780403f }, {-0.00091299973f, 0.1423397f },
    {-0.00086900033f, 0.14504623f }, {-0.0008129999f, 0.14592743f }, {-0.00074400008f, 0.14580154f }, {-0.00066300016f, 0.16204071f },
    {-0.0005699992f, 0.18796539f }, {-0.00046500005f, 0.21744347f }, {-0.00034700055f, 0.25047493f }, {-0.00021700002f, 0.28705788f },
    {-0.000075999647f, 0.32719803f }, { 0.000049999915f, 0.3674984f }, { 0.00014299992f, 0.34771347f }, { 0.00023099966f, 0.30610847f },
    { 0.0003120005f, 0.26638985f }, { 0.00038500037f, 0.22855759f }, { 0.00045299996f, 0.19260406f }, { 0.00051299948f, 0.15853691f },
    { 0.00056700036f, 0.12635422f }, { 0.00061399955f, 0.09605789f }, { 0.0006550001f, 0.067640305f }, { 0.00068799965f, 0.041110992f },
    { 0.00071500055f, 0.01646614f }, { 0.00073499978f, -0.006298065f }, { 0.0007480001f, -0.027175903f }, { 0.00075600017f, -0.04616928f },
    { 0.0007549999f, -0.063278198f }, { 0.00074899988f, -0.07850647f }, { 0.00073500024f, -0.09184837f }, { 0.0007149996f, -0.10330391f },
    { 0.0006880001f, -0.11288071f }, { 0.00065400009f, -0.12056732f }, { 0.000614f, -0.1263752f }, { 0.00056699989f, -0.1302967f },
    { 0.00051400007f, -0.13233185f }, { 0.00045199995f, -0.13248825f }, { 0.00038600003f, -0.13075447f }, { 0.000311f, -0.12714195f },
    { 0.000231f, -0.12164116f }, { 0.000144f, -0.11425972f }
};
static const EN22_POINT2_XZ ply_mtn45b_pos[50] = {
    { 0.0051699998f, 0.051181793f }, { 0.014990001f, 0.03083229f }, { 0.02403f, 0.013719559f }, { 0.032290999f, -0.00015449524f},
    { 0.039773002f, -0.010793686f }, { 0.046472996f, -0.018190384f }, { 0.052395999f, -0.022354126f }, { 0.057539016f, -0.023277283f },
    { 0.061900973f, -0.020963669f }, { 0.065484017f, -0.027778625f }, { 0.06828898f, -0.049476624f }, { 0.07031202f, -0.076560974f },
    { 0.07155597f, -0.109041214f }, { 0.072021008f, -0.1469059f }, { 0.07833004f, -0.17226791f }, { 0.08751994f, -0.18546867f },
    { 0.09148502f, -0.20457649f }, { 0.090228975f, -0.22959328f }, { 0.083749056f, -0.2605896f }, { 0.072044015f, -0.29907799f },
    { 0.055117965f, -0.32930183f }, { 0.03296697f, -0.34487152f }, { 0.009256005f, -0.34578705f }, { 0.0026700497f, -0.33204842f },
    {-0.0010420084f, -0.30365753f }, {-0.0048240423f, -0.26327515f }, {-0.0086729527f, -0.22501564f }, {-0.012591004f, -0.19028473f },
    {-0.016576052f, -0.15877151f }, {-0.020630002f, -0.13047409f }, {-0.02475202f, -0.10539436f }, {-0.028941989f, -0.08353424f },
    {-0.033199906f, -0.064891815f }, {-0.03752601f, -0.049465179f }, {-0.04192102f, -0.037258148f }, {-0.04638207f, -0.028268814f },
    {-0.050912976f, -0.022497177f }, {-0.055512965f, -0.01994133f }, {-0.06017804f, -0.020606995f }, {-0.063638985f, -0.0031776428f },
    {-0.065679014f, 0.030195236f }, {-0.067460954f, 0.057128906f }, {-0.068989038f, 0.077625275f }, {-0.070258975f, 0.09168434f },
    {-0.071274996f, 0.099300385f }, {-0.072033018f, 0.10048103f }, {-0.072537005f, 0.095220566f }, {-0.07278399f, 0.083524704f },
    {-0.07277499f, 0.065385818f }, {-0.072509006f, 0.040813446f }
};
static const uint32_t ene22_eff_col[20] = {                               /* :2297 */
    0x60B2B2FFu, 0xC0B2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu,
    0xFFB2B2FFu, 0xFFB2B2FFu, 0xFFB2B2FFu, 0xE0B2B2FFu, 0xC0B2B2FFu, 0xAFB2B2FFu, 0x80B2B2FFu, 0x60B2B2FFu, 0x40B2B2FFu, 0x20B2B2FFu
};
typedef struct { int32_t obj, wait; RECVX_ENEMY_VEC ofs; } EN22_DEN_WORK;  /* :2700 */
static const EN22_DEN_WORK en22_den_tbl[20] = {
    { 2,  0, {0.0f, 0.0f, 1.0f}}, { 3,  0, {0.0f, 1.0f, 0.0f}}, { 4,  0, {0.0f, 1.0f, 0.0f}}, { 5,  0, {0.0f, 0.0f, 0.0f}},
    { 6,  0, {0.0f, 0.0f, 0.0f}}, {26,  4, {0.0f, 0.0f, 0.0f}}, {27, 12, {0.0f, 0.0f, 0.0f}}, {28, 20, {0.0f, 0.0f, 0.0f}},
    {29, 28, {0.0f, 0.0f, 0.0f}}, {34,  6, {0.0f, 0.0f, 0.0f}}, {35, 14, {0.0f, 0.0f, 0.0f}}, {36, 22, {0.0f, 0.0f, 0.0f}},
    {37, 30, {0.0f, 0.0f, 0.0f}}, {12,  6, {0.0f, 0.0f, 0.0f}}, {13, 14, {0.0f, 0.0f, 0.0f}}, {14, 20, {0.0f, 0.0f, 0.0f}},
    {19,  4, {0.0f, 0.0f, 0.0f}}, {20, 12, {0.0f, 0.0f, 0.0f}}, {21, 20, {0.0f, 0.0f, 0.0f}}, {-1,  0, {0.0f, 0.0f, 0.0f}}
};

/* --- Dienste --------------------------------------------------------------- */
static const RECVX_EN22_SERVICES* en22_extra;
void recvx_en22_bind(const RECVX_EN22_SERVICES* services) { en22_extra = services; }
const RECVX_EN22_SERVICES* recvx_en22_bound(void) { return en22_extra; }
RECVX_EN22_EXP0* recvx_en22_exp0(RECVX_ENEMY_WORK* w) { return w ? (RECVX_EN22_EXP0*)w->exp0 : NULL; }
#define NJM_DEG_ANG(d) ((int32_t)((d) * 182.04445f))

static void en22_gap(const RECVX_ENEMY_WORK* w, const char* name) { if (w && w->sv && w->sv->gap) w->sv->gap(name); }
static const RECVX_EN01_SERVICES* en22_en(void) { const RECVX_EN01_CONTEXT* c = recvx_en01_context(); return c ? c->en01 : NULL; }
static int32_t en22_rand(const RECVX_ENEMY_WORK* w)
{
    if (w->sv && w->sv->random_next) return w->sv->random_next();
    en22_gap(w, "rand (en22)"); return 0;
}
/* macros.h:105 njRandom() = (float)-rand() / (float)(RAND_MAX + 1) auf demselben Strom. */
static float en22_njrandom(const RECVX_ENEMY_WORK* w) { return (float)en22_rand(w) / 2147483648.0f; }
static int en22_player(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl, const char* where)
{
    if (w->sv && w->sv->player_get && w->sv->player_get(pl)) return 1;
    en22_gap(w, where); return 0;
}
static void en22_player_set(const RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* pl)
{
    if (w->sv && w->sv->player_set) w->sv->player_set(pl); else en22_gap(w, "plp (en22 Schreibzugriff)");
}
static int en22_joint_mtx(const RECVX_ENEMY_WORK* w, uint32_t joint, float* out16)
{
    if (w->sv && w->sv->joint_matrix && w->sv->joint_matrix(w, joint, out16)) return 1;
    en22_gap(w, "mlwP->owP[joint].mtx (en22)"); return 0;
}
static int en22_joint_pos(const RECVX_ENEMY_WORK* w, uint32_t joint, RECVX_ENEMY_VEC* out)
{
    float m[16]; if (!en22_joint_mtx(w, joint, m)) return 0;
    out->x = m[12]; out->y = m[13]; out->z = m[14]; return 1;
}
static int en22_player_joint_pos(const RECVX_ENEMY_WORK* w, uint32_t joint, RECVX_ENEMY_VEC* out)
{
    float m[16];
    if (w->sv && w->sv->player_joint_matrix && w->sv->player_joint_matrix(joint, m)) { out->x = m[12]; out->y = m[13]; out->z = m[14]; return 1; }
    en22_gap(w, "plp->mlwP->owP[joint].mtx (en22)"); return 0;
}
static uint32_t en22_motion_frames(const RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    if (w->sv && w->sv->motion_frames) return w->sv->motion_frames(w, mtn);
    en22_gap(w, "mnwP[mtn_no].frm_num (en22)"); return 0u;
}
static int en22_last_frame(const RECVX_ENEMY_WORK* w)
{
    return (w->frm_no / 65536) == (int32_t)en22_motion_frames(w, w->mtn_no) - 1;
}
static void en22_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no, int frm, int rate) { (void)recvx_t3_chg_mtn(w, no, frm, rate); }
static void en22_add_speed(RECVX_ENEMY_WORK* w, int rot)
{
    const RECVX_EN01_SERVICES* en = en22_en();
    if (en && en->add_speed) { en->add_speed(w, rot); return; }
    en22_gap(w, "bhAddSpeed (en22)");
}
static void en22_ikou(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add) { recvx_t3_ikou(w, pos, add); }
static int en22_ikou3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add) { return recvx_t3_ikou3(w, pos, add); }
/* bhEne22_SePlay (en22.c:2605): flg 0x10000 unterdrueckt RequestEnemySe(sys->enow, ps, no). */
static void en22_se_play(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps, int no)
{
    if (w->flg & 0x10000u) return;
    if (!w->sv || !w->sv->enemy_se || !w->sv->enemy_now) { en22_gap(w, "RequestEnemySe (en22.c:2609)"); return; }
    w->sv->enemy_se(w->sv->enemy_now(), ps, no);
}
static RECVX_ENEMY_VEC en22_pos(const RECVX_ENEMY_WORK* w) { RECVX_ENEMY_VEC p = { w->px, w->py, w->pz }; return p; }
static int en22_light_get(const RECVX_ENEMY_WORK* w, RECVX_EN22_LIGHT* out)
{
    if (en22_extra && en22_extra->light_get && en22_extra->light_get(out)) return 1;
    en22_gap(w, "rom->lgtp[2] (en22)"); return 0;
}
static void en22_light_set(const RECVX_ENEMY_WORK* w, const RECVX_EN22_LIGHT* in)
{
    if (en22_extra && en22_extra->light_set) { en22_extra->light_set(in); return; }
    en22_gap(w, "rom->lgtp[2] (en22 Schreibzugriff)");
}
/* rom->lgtp[2].flg &= ~0x3 (en22.c:664/:956/:1720). */
static void en22_light_off(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_LIGHT l; if (!en22_light_get(w, &l)) return;
    l.flg &= ~0x3u; en22_light_set(w, &l);
}
/* (*(O_WRK**)(exp0 + 0x4C)) ist das 353 aus bhEne22_SetElectricLightEffect: flg != 0 && id == 353 && lkwkp == epw -> mode0 = 4. */
static void en22_light_effect_off(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    if (p->light_effect < 0) { en22_gap(w, "(*(O_WRK**)(exp0 + 0x4C))->flg bei NULL (en22.c:661)"); return; }
    if (en22_extra && en22_extra->effect_light_off) en22_extra->effect_light_off(p->light_effect, w);
    else en22_gap(w, "eff[].mode0 = 4 (en22 Blitzlicht)");
}

/* --- Vorwaertsdeklarationen -------------------------------------------- */
static void en22_main_loop(RECVX_ENEMY_WORK* w);
static void en22_chg_dmg_mode(RECVX_ENEMY_WORK* w);
static void en22_damage_add(RECVX_ENEMY_WORK* w);
static void en22_coll_check(RECVX_ENEMY_WORK* w);
static void en22_coll_check_wall(RECVX_ENEMY_WORK* w);
static void en22_move(RECVX_ENEMY_WORK* w);
static void en22_damage(RECVX_ENEMY_WORK* w);
static void en22_die(RECVX_ENEMY_WORK* w);
static void en22_player_hit_check(RECVX_ENEMY_WORK* w);
static void en22_brain(RECVX_ENEMY_WORK* w);
static void en22_mv00(RECVX_ENEMY_WORK* w);
static void en22_mv01(RECVX_ENEMY_WORK* w);
static void en22_mv02(RECVX_ENEMY_WORK* w);
static void en22_mv04(RECVX_ENEMY_WORK* w);
static void en22_mv06(RECVX_ENEMY_WORK* w);
static void en22_dg00(RECVX_ENEMY_WORK* w);
static void en22_dg01(RECVX_ENEMY_WORK* w);
static void en22_dd00(RECVX_ENEMY_WORK* w);
static void en22_ply_dg00(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl);
static void en22_ply_dg01(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl);
static int  en22_set_mtn(RECVX_ENEMY_WORK* w);
static void en22_check_mtn_tbl(RECVX_ENEMY_WORK* w, int frm);
static void en22_get_translate_mtn(RECVX_ENEMY_WORK* w, int frm);
static void en22_spark_effect(RECVX_ENEMY_WORK* w, int frm);
static void en22_set_water_effect(RECVX_ENEMY_WORK* w, int mtn_no, int frm);
static int  en22_area_check(float ene_x, float ene_z, float ply_x, float ply_z);
static int  en22_set_trg_pos(RECVX_ENEMY_WORK* w);
static int  en22_set_dengeki_effect(RECVX_ENEMY_WORK* w, int obj, const RECVX_ENEMY_VEC* ofs, float size, int on_player);
static void en22_set_dengeki_effect2(RECVX_ENEMY_WORK* w);
static void en22_set_electric_shock_effect(RECVX_ENEMY_WORK* w, int type);
static void en22_set_electric_shock_effect2(RECVX_ENEMY_WORK* w);
static int  en22_set_electric_light_effect(RECVX_ENEMY_WORK* w);
static void en22_set_light(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs, int type);

/* --- bhEne22 (en22.c:512) ------------------------------------------------- */
void recvx_en22(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p;
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC j;
    int i;
    if (!w) return;
    en22_main_loop(w);                                                  /* :517 */
    p = recvx_en22_exp0(w);
    if (!p) return;                                                     /* vor Init (mode0 0 legt exp0 an) */
    if (en22_player(w, &pl, "plp (en22 PlyDG)")) {
        if (pl.mode0 == 4 || pl.mode0 == 6) {                           /* :518 */
            if (pl.mode2 == 0) en22_ply_dg00(w, &pl); else en22_ply_dg01(w, &pl);
        }
    }
    if (w->flg & 0x4u) {                                                /* :529 */
        for (i = 0; i < 64; i++) w->dam[i] = 0;
        w->flg &= ~0x4u;
    }
    en22_coll_check(w);                                                 /* :537 */
    if (w->sv && w->sv->calc_model) w->sv->calc_model(w); else en22_gap(w, "bhCalcModel (en22)");
    if (en22_joint_pos(w, 4, &j)) { w->aox = j.x - w->px; w->aoz = j.z - w->pz; }   /* :539-542 */
    if (en22_joint_pos(w, 3, &j)) { w->watr.c1.x = j.x; w->watr.c1.y = w->py + 2.0f; w->watr.c1.z = j.z; }
    if (en22_joint_pos(w, 9, &j)) { w->watr.c2.x = j.x; w->watr.c2.y = w->py + 2.0f; w->watr.c2.z = j.z; }
    w->watr.r = 5.5f;                                                   /* :551 */
    if (p->wait0C > 0) p->wait0C--;
    if (p->fire_wait > 0) p->fire_wait--;
    if (p->shock_wait > 0) p->shock_wait--;
    if (p->hit_window > 0) p->hit_window--;
    recvx_en22_ctr_light(w);                                            /* :564 */
}
/* bhEne22_MainLoop (:582). */
static void en22_main_loop(RECVX_ENEMY_WORK* w)
{
    recvx_en22_dmg_chk(w);
    switch (w->mode0) {                                                 /* bhEne22_Mode0[6] (:378) */
    case 0: recvx_en22_init(w); break;
    case 1: en22_move(w); break;
    case 2: break;                                                      /* bhEne22_Nage */
    case 3: en22_damage(w); break;
    case 4: en22_die(w); break;
    case 5: if (w->sv && w->sv->event) w->sv->event(w); else en22_gap(w, "bhEne_Event (subpl.c:143)"); break;
    default: en22_gap(w, "bhEne22_Mode0[mode0] ausserhalb der Tabelle"); break;
    }
    en22_set_mtn(w);
}
/* bhEne22_DmgChk (:592). */
int recvx_en22_dmg_chk(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    int houkou;
    if (!p) return 0;
    if ((w->flg & 0x4u) && !(w->flg & 0x2u) && !(p->flags & 0x20000u)) {
        RECVX_ENEMY_VEC dv;
        recvx_t3_calc_damage(w, en22_comb_wep, en22_comb_joint);        /* :597 */
        if (w->total_dam == 0) return 1;
        en22_damage_add(w);
        if (w->mode0 != 1) return 1;
        dv.x = w->dvx; dv.y = w->dvy; dv.z = w->dvz;
        houkou = recvx_t3_dgc_dir_check3(&dv, w->ay);                   /* :607 */
        p->flags &= ~0x30u;
        if (houkou == 1) p->flags |= 0x10u;
        else if (houkou == 2) p->flags |= 0x20u;
        else if (houkou == 3) p->flags |= 0x30u;
        en22_chg_dmg_mode(w);
        return 1;
    }
    return 0;
}
/* bhEne22_ChgDmgMode (:638). */
static void en22_chg_dmg_mode(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    const EN22_WPNDAMAGE* wp = &en22_wpn_damage[w->wpnr_no < 22 ? w->wpnr_no : 0];
    int act;
    if (w->wpnr_no >= 22) en22_gap(w, "En22_WpnDamageTbl[wpnr_no] ausserhalb der Tabelle");
    act = wp->nm_act;
    if (w->hp < 0) { w->comb_flg |= 0x1u; w->comb_timeout = 0; w->comb_pnt = 0; }
    if (w->comb_flg & 0x1u) act = wp->cb_act;
    if ((uint32_t)act < 4u) {
        if (p->flags & 0x1000u) {
            en22_light_effect_off(w);                                   /* :657-661 */
            if (p->flags & 0x80000u) { en22_light_off(w); p->flags &= ~0x80000u; }
            p->flags &= ~0x1000u;
        }
        if (w->hp < 0) { w->mode0 = 4; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0; }
        else {
            w->mode0 = 3; w->mode1 = 0; w->mode3 = 0;
            w->mode2 = (p->flags & 0x100u) ? 0 : 1;
        }
    }
}
/* bhEne22_DamageAdd (:703). */
static void en22_damage_add(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    const EN22_WPNDAMAGE* wp = &en22_wpn_damage[0];
    uint32_t obj_num = (w->sv && w->sv->obj_num) ? w->sv->obj_num(w) : 0u;
    int i;
    if (w->hp >= 0) {
        w->hp -= w->total_dam;
        wp = &en22_wpn_damage[w->comb_wep < 22u ? w->comb_wep : 0u];
        if (w->comb_wep >= 22u) en22_gap(w, "En22_WpnDamageTbl[comb_wep] ausserhalb der Tabelle");
        for (i = 2; i < (int)obj_num && i < 64; i++) {
            if (w->dam[i] <= 0) continue;
            w->djnt_no = i;
            if (wp->flg & 0x4) continue;
            if ((w->comb_flg & 0x1u) || w->hp < 0) recvx_t3_set_blood(w, wp->cb_blood, en22_prt_blood_tbl);
            else recvx_t3_set_blood(w, wp->nm_blood, en22_prt_blood_tbl);
        }
    }
    if (((wp->flg & 0x1) || (wp->flg & 0x2)) && p->fire_wait <= 0) {
        p->fire_wait = 10;
        (void)recvx_t3_set_dfire_effect(w, w->djnt_no, en22_prt_blood_tbl, (wp->flg & 0x2) ? 2 : 1);
    }
    if ((wp->flg & 0x8) && p->fire_wait <= 0) {
        p->fire_wait = 10;
        (void)recvx_t3_set_san_effect(w, w->djnt_no, en22_prt_blood_tbl);
    }
}
/* bhEne22_CollCheck (:756). */
static void en22_coll_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    if (w->flg & 0x2u) return;
    if ((w->flg & 0x8u) && (p->flags & 0x40u)) {
        if (w->sv && w->sv->check_player) w->sv->check_player(w); else en22_gap(w, "bhCheckPlayer (en22)");
    }
    en22_coll_check_wall(w);
}
/* bhEne22_CollCheckWall (:770): Wasserbit, Wandtyp, dann die Gelenkproben. */
static void en22_wall_probe(RECVX_ENEMY_WORK* w, uint32_t joint, RECVX_ENEMY_VEC* pd, float r, int add_pd)
{
    RECVX_ENEMY_VEC ps, j;
    if (!en22_joint_pos(w, joint, &j)) return;
    ps.x = j.x + (add_pd ? pd->x : 0.0f);
    ps.z = j.z + (add_pd ? pd->z : 0.0f);
    ps.y = w->py;
    if (recvx_t3_collision_check_wall(w, &ps, pd, r, w->ah)) { w->px += pd->x; w->py += pd->y; w->pz += pd->z; }
}
static void en22_coll_check_wall(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    const RECVX_EN01_SERVICES* en = en22_en();
    RECVX_ENEMY_VEC ps, pd = { 0.0f, 0.0f, 0.0f }, pos = en22_pos(w);
    uint32_t st_flg = (w->sv && w->sv->system_flags) ? w->sv->system_flags(2u) : 0u;
    if ((st_flg & 0x40u) && !(w->stflg & 0x100000u)) {                  /* :775 */
        if (en && en->check_water) { if (en->check_water(&pos)) w->stflg |= 0x100000u; }
        else en22_gap(w, "bhCheckWater (en22)");
    }
    if (w->sv && w->sv->wall_type) p->atr17 = w->sv->wall_type(&pos, w->flg, w->ar, w->ah);  /* :779 */
    else en22_gap(w, "bhCheckWallType (en22)");
    if ((p->flags & 0xFu) == 0x1u && (w->flg & 0x10u)) {               /* :780 */
        ps.x = w->px + w->aox; ps.z = w->pz + w->aoz; ps.y = w->py;
        if (recvx_t3_collision_check_wall(w, &ps, &pd, w->ar, w->ah)) { w->px += pd.x; w->py += pd.y; w->pz += pd.z; }
        en22_wall_probe(w, 28, &pd, 2.0f, 0);
        en22_wall_probe(w, 29, &pd, 2.0f, 1);
        en22_wall_probe(w, 36, &pd, 2.0f, 1);
        en22_wall_probe(w, 37, &pd, 2.0f, 1);
        en22_wall_probe(w, 13, &pd, 2.0f, 1);
        en22_wall_probe(w, 14, &pd, 2.0f, 1);
        en22_wall_probe(w, 20, &pd, 2.0f, 1);
        en22_wall_probe(w, 21, &pd, 2.0f, 1);
    } else if ((p->flags & 0xFu) == 2u) {                               /* :882 */
        if (w->sv && w->sv->check_dansa) w->sv->check_dansa(w); else en22_gap(w, "bhCheckDansa (en22)");
        en22_wall_probe(w, 21, &pd, 2.0f, 0);
        en22_wall_probe(w, 14, &pd, 2.0f, 1);
    }
}
/* bhEne22_Init (:911). */
void recvx_en22_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p;
    const RECVX_EN01_CONTEXT* ctx = recvx_en01_context();
    int i;
    w->ar = 7.0f; w->ah = 6.0f; w->aw = 0.0f; w->ad = 0.0f; w->car = 7.0f; w->cah = 6.0f;
    w->stflg = 0;
    if (!ctx) { en22_gap(w, "sys->gm_mode (en22.c:921)"); w->hp = 250; }
    else w->hp = (ctx->game_mode != 2u) ? 250 : 160;
    for (i = 0; i < 64; i++) w->dam[i] = 0;
    w->hokan_rate = 0; w->hokan_count = 0; w->mtn_no = 0; w->mtn_add = 0; w->frm_no = 0;
    w->mtn_tp = en22_flip_tree; w->mtn_md = 0;
    if (!w->exp0) {
        if (w->sv && w->sv->calloc_work) w->exp0 = w->sv->calloc_work(0xB0u);
        if (!w->exp0) { en22_gap(w, "bhEne_CallocWork(0xB0, 8) (en22)"); return; }
        p = recvx_en22_exp0(w);
    } else {
        p = recvx_en22_exp0(w);
        if (p->flags & 0x80000u) { en22_light_off(w); p->flags &= ~0x80000u; }
        memset(p, 0, 0xB0u);
    }
    p->light_effect = -1;                                               /* NULL */
    p->flags |= 0x41u;
    w->flg |= 0x78u; w->flg &= ~0x2u;
    if (!(w->flg & 0x800u)) {
        if (w->sv && w->sv->set_shadow) (void)w->sv->set_shadow(en22_sdw_tab, w, 4, 4.0f, 4.0f, 8.0f);
        else en22_gap(w, "bhSetShadow (en22)");
        w->flg |= 0x800u;
    }
    w->clp_jno[0] = -1;
    w->mdflg |= 0x20u;
    w->mode0 = 1; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
    w->lok_jno = 2;
    w->cpcl = en22_cap_col;
}
/* bhEne22_Move (:998). */
static void en22_move(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_VEC pos, epos;
    if (!en22_player_joint_pos(w, 0, &pos)) return;
    pos.y = w->py;
    epos.x = w->px + w->aox; epos.z = w->pz + w->aoz; epos.y = w->py;
    p->dist = sqrtf((pos.x - epos.x) * (pos.x - epos.x) + (pos.y - epos.y) * (pos.y - epos.y) + (pos.z - epos.z) * (pos.z - epos.z));
    /* bhEne22_MVType00 (:1264) */
    if (w->mode1 == 1) en22_brain(w);
    switch (w->mode2) {
    case 0: en22_mv00(w); break;
    case 1: en22_mv01(w); break;
    case 2: en22_mv02(w); break;
    case 3: break;
    case 4: en22_mv04(w); break;
    case 5: break;
    case 6: en22_mv06(w); break;
    default: en22_gap(w, "bhEne22_MoveMode2[mode2] ausserhalb der Tabelle"); break;
    }
    if (p->shock_wait == 0 && w->mode2 != 4) {                          /* :1012 */
        RECVX_ENEMY_VEC lo = { 0.0f, 5.0f, 0.0f };
        en22_set_electric_shock_effect(w, 3);
        p->shock_wait = en22_rand(w) % 30 + 60;
        p->hit_window = 15;
        en22_set_light(w, 4, &lo, 0);
        pos = en22_pos(w);
        en22_se_play(w, &pos, 0x1012302);
    }
    en22_player_hit_check(w);
}
static void en22_damage(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) { case 0: en22_dg00(w); break; case 1: en22_dg01(w); break; case 2: break;
    default: en22_gap(w, "bhEne22_DamageMode2[mode2] ausserhalb der Tabelle"); break; }
}
static void en22_die(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) { case 0: en22_dd00(w); break; case 1: break;
    default: en22_gap(w, "bhEne22_DieMode2[mode2] ausserhalb der Tabelle"); break; }
}
/* bhEne22_PlyerHitCheck (:1053). */
static void en22_player_hit_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC ps, plpos;
    if (!en22_player(w, &pl, "plp (en22 PlyerHitCheck)")) return;
    plpos = pl.pos;
    if (p->hit_window > 0 && p->dist <= 20.0f && pl.flr_no == w->flr_no && !(pl.flg & 0x2u) && !(pl.flg & 0x4u) && !(pl.stflg & 0x80000000u)) {
        p->flags |= 0x40000u;
        pl.mode0 = 4; pl.mode1 = 0; pl.mode2 = 0; pl.mode3 = 0;
        pl.flg |= 0x4u; pl.hp -= 30;
        en22_player_set(w, &pl);
        en22_set_electric_shock_effect2(w);
        en22_se_play(w, &plpos, 0x1012302);
        ps.x = plpos.x; ps.y = 10.0f; ps.z = plpos.z;
        en22_set_light(w, -1, &ps, 0);
        p->hit_window = 0;
        if (!en22_player(w, &pl, "plp (en22 PlyerHitCheck)")) return;
    }
    if (p->dist <= 10.0f && en22_ikou3(w, &plpos, NJM_DEG_ANG(30.0f)) == 0 && pl.flr_no == w->flr_no
        && !(pl.flg & 0x2u) && !(pl.flg & 0x4u) && !(pl.stflg & 0x80000000u)) {
        p->flags |= 0x40000u;
        if (w->mtn_no == 5u || (w->mtn_no - 6u) < 2u) { pl.mode0 = 4; pl.mode1 = 0; pl.mode2 = 1; pl.mode3 = 0; }
        else { pl.mode0 = 4; pl.mode1 = 0; pl.mode2 = 0; pl.mode3 = 0; }
        pl.flg |= 0x4u; pl.hp -= 30;
        en22_player_set(w, &pl);
        en22_set_electric_shock_effect2(w);
        en22_set_electric_shock_effect(w, 1);
        en22_se_play(w, &plpos, 0x1012302);
        ps.x = plpos.x; ps.y = 10.0f; ps.z = plpos.z;
        en22_set_light(w, -1, &ps, 0);
    }
}
/* bhEne22_EneSearch (:1125) mit bhSearchPlayer(epw, 18204) ueber den EN01-Dienst (pwksub.c:664). */
static void en22_ene_search(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    const RECVX_EN01_SERVICES* en = en22_en();
    p->search |= 0x80u;
    if ((p->search & 0x1Fu) < 0x4u) {
        int32_t yr = -1;
        RECVX_ENEMY_VEC pos = en22_pos(w);
        if (en && en->search_player) { if (en->search_player(w, &pos, w->ay, 18204, &yr) == 0) yr = -1; }
        else en22_gap(w, "bhSearchPlayer (en22)");
        if (yr != -1) p->search |= 0x20u;
        if ((p->search & 0x1Fu) == 3u) {
            if (p->search & 0x20u) p->search |= 0x40u; else p->search &= ~0x40u;
            p->search &= ~0xA0u;
        }
    }
    p->search++;
    if ((p->search & 0x1Fu) > 0xFu) p->search &= ~0x1Fu;
}
/* bhEne22_Brain (:1160) und Brain00/01/02/04. */
static void en22_brain(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    if (!(p->search & 0x40u)) en22_ene_search(w);
    if (!en22_player(w, &pl, "plp (en22 Brain)")) return;
    switch (w->mode2) {
    case 0:                                                             /* :1171 */
        if (w->mode3 == 0) return;
        if (pl.flr_no == 0) {
            if (p->dist <= 40.0f) { w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; }
            else if (!(pl.stflg & 0x80000000u) && p->dist > 40.0f) { w->mode1 = 1; w->mode2 = 4; w->mode3 = 0; }
            w->ct0 = en22_rand(w) % 60 + 120;
        } else {
            w->ct0--;
            if (en22_area_check(w->px, w->pz, pl.pos.x, pl.pos.z) || w->ct0 < 0 || p->dist < 42.0f) {
                en22_set_trg_pos(w);
                if (w->ct0 < 0) w->mode2 = (en22_rand(w) % 2) ? 1 : 2; else w->mode2 = 2;
                w->mode1 = 1; w->mode3 = 0;
            }
        }
        break;
    case 1:                                                             /* :1222 */
        if (en22_area_check(p->trg_x, p->trg_z, pl.pos.x, pl.pos.z)) en22_set_trg_pos(w);
        if (pl.flr_no == 0) { w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        break;
    case 2:                                                             /* :1238 */
        if (pl.flr_no == 0) { p->trg_x = pl.pos.x; p->trg_z = pl.pos.z; }
        else if (en22_area_check(p->trg_x, p->trg_z, pl.pos.x, pl.pos.z)) en22_set_trg_pos(w);
        break;
    case 4:                                                             /* :1255 */
        if (p->dist < 20.0f) p->flags |= 0x10000u;
        break;
    default: break;                                                     /* DmmyBrain */
    }
}
/* bhEne22_MV00 (:1275). */
static void en22_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    if (w->mode3 == 0) {
        en22_chg_mtn(w, 0, 0, 7);
        if (p->search & 0x40u) w->ct0 = en22_rand(w) % 60 + 30; else w->ct0 = en22_rand(w) % 60 + 120;
        if (p->flags & 0x4000u) { p->flags &= ~0x4000u; w->ct0 = en22_rand(w) % 60 + 90; w->ct1 = 1; }
        else w->ct1 = 0;
        w->ct2 = 0; w->mode1 = 1; w->mode3++;
    }
    if ((w->flg & 0x2000000u) && (en22_rand(w) % 2)) { RECVX_ENEMY_VEC pos = en22_pos(w); en22_se_play(w, &pos, 0x1012306); }
}
/* bhEne22_MV01 (:1317). */
static void en22_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_VEC trg = { p->trg_x, 0.0f, p->trg_z };
    int hit;
    switch (w->mode3) {
    case 0:
        en22_chg_mtn(w, 1, 0, 5);
        w->way = 384; w->wax = 0; w->ct0 = 0; w->ct1 = 0; w->spd = 0.0f; w->mode3++;
        /* fallthrough */
    case 1:
        if (w->flg & 0x2000000u) { en22_chg_mtn(w, 2, 0, 0); w->ct1 = en22_rand(w) % 30 + 40; w->mode3++; }
        w->spd += 0.02f; if (w->spd >= 0.4f) w->spd = 0.4f;
        en22_ikou(w, &trg, 384); en22_add_speed(w, 0);
        break;
    case 2:
        if (recvx_t3_check_dir_wall2(w, 0, 18.0f) != 0) {
            hit = recvx_t3_check_side_wall2(w, 18.0f, 0);
            if (hit == 0) w->way = (en22_rand(w) % 2) ? 384 : -384; else w->way = (int16_t)(hit * 384);
            w->wax = (w->way < 0) ? -256 : 256;
            p->flags |= 0x2000u;
            en22_chg_mtn(w, 2, 0, 5);
            w->ct1 = 42; w->mode3++;
        } else {
            if ((w->flg & 0x2000000u) && (((fabsf(p->trg_x - w->px) < 10.0f) && (fabsf(p->trg_z - w->pz) < 10.0f)) || (p->flags & 0x8000u))) {
                en22_chg_mtn(w, 4, 0, 5); w->ct1 = 0; w->mode3 = 4; p->flags &= ~0x8000u;
            }
            en22_ikou(w, &trg, 384); en22_add_speed(w, 0);
        }
        break;
    case 3:
        w->ay += w->way;
        en22_add_speed(w, -w->way);
        if (--w->ct1 < 0) { w->ct1 = en22_rand(w) % 30 + 40; w->mode3 = 2; }
        break;
    case 4:
        if (en22_last_frame(w)) { w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        w->spd -= 0.02f; if (w->spd < 0.0f) w->spd = 0.0f;
        en22_add_speed(w, 0);
        break;
    default: break;
    }
}
/* bhEne22_MV02 (:1422). */
static void en22_mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    RECVX_ENEMY_VEC trg, pos;
    int frm, hit;
    if (!en22_player(w, &pl, "plp (en22 MV02)")) return;
    trg.x = p->trg_x; trg.y = 0.0f; trg.z = p->trg_z;
    switch (w->mode3) {
    case 0:
        w->way = 768; w->wax = 0; w->ct0 = 0; w->ct1 = 0; w->ct2 = 0; w->ct3 = 0;
        p->flags |= 0x100u;
        if (p->flags & 0x200u) { en22_chg_mtn(w, 6, 0, 5); p->flags &= ~0x200u; w->ct0 = 0; w->spd = 1.2f; w->mode3 = 2; return; }
        en22_chg_mtn(w, 5, 0, 5);
        p->trg_x = pl.pos.x; p->trg_z = pl.pos.z; trg.x = p->trg_x; trg.z = p->trg_z;
        w->spd = 0.0f; w->mode3++;
        /* fallthrough */
    case 1:
        if (w->flg & 0x2000000u) { en22_chg_mtn(w, 6, 0, 0); w->ct0 = 0; w->ct2 = 300; w->mode3++; }
        w->spd += 0.05f; if (w->spd >= 1.2f) w->spd = 1.2f;
        en22_ikou(w, &trg, 768); en22_add_speed(w, 0);
        break;
    case 2:
        w->spd = 1.2f - (float)w->ct0 * 0.01f;
        w->ct0++;
        if (w->spd < 0.8f) {
            w->ct0 = 0; w->spd = 0.8f;
            if (en22_ikou3(w, &pl.pos, NJM_DEG_ANG(112.5f)) && p->dist > 20.0f) { w->mode3 = 6; return; }
        }
        if (p->flags & 0x40000u) {
            p->flags &= ~0x40000u;
            w->way = (en22_rand(w) % 2) ? 768 : -768;
            p->flags |= 0x2000u;
            en22_chg_mtn(w, 7, 0, 5);
            if (w->way > 0) w->mtn_md |= 0x2u;
            w->ct1 = 32; w->mode3 = 4; w->ct2 = 0;
        } else {
            hit = recvx_t3_check_dir_wall2(w, 0, 18.0f);
            if (hit != 0) {
                pos.x = w->px + 18.0f * -njSin(w->ay); pos.z = w->pz + 18.0f * -njCos(w->ay); pos.y = w->py;
                hit = recvx_t3_check_side_wall3(w, &pos, 18.0f, 0);
                if (hit == 0) w->way = (en22_rand(w) % 2) ? 768 : -768; else w->way = (int16_t)(hit * 768);
                w->wax = (w->way < 0) ? -1024 : 1024;
                p->flags |= 0x2000u;
                en22_chg_mtn(w, 7, 0, 5);
                if (w->wax > 0) w->mtn_md |= 0x2u;
                w->ct1 = 21; w->mode3 = 4;
            } else {
                if (pl.flr_no == 0 && p->dist >= 40.0f) { en22_chg_mtn(w, 8, 0, 5); w->mode3 = 3; }
                else {
                    if (fabsf(p->trg_x - w->px) < 5.0f && fabsf(p->trg_z - w->pz) < 5.0f) { en22_chg_mtn(w, 8, 0, 5); w->mode3 = 3; }
                    en22_ikou(w, &trg, 384); en22_add_speed(w, 0);
                }
            }
        }
        break;
    case 3:
        if (en22_last_frame(w)) { p->flags &= ~0x100u; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        w->spd -= 0.05f; if (w->spd < 0.0f) w->spd = 0.0f;
        en22_add_speed(w, 0);
        break;
    case 4:
        frm = w->frm_no / 65536;
        if (frm == 20) w->mtn_add = 0;
        if (w->spd > 0.5f) { w->spd -= 0.02f; w->ct0++; } else w->spd = 0.5f;
        hit = recvx_t3_check_dir_wall2(w, 0, 18.0f);
        if (hit == 0 && frm >= 16) { w->mtn_add = 65536; w->mode3++; }
        else w->ay += w->way;
        en22_add_speed(w, -w->way);
        break;
    case 5:
        frm = w->frm_no / 65536;
        w->spd += 0.05f; if (w->spd >= 1.2f) w->spd = 1.2f;
        w->ay += w->way;
        if (frm == 31) {
            w->ct1 = en22_rand(w) % 30 + 40; w->ct0 = 0; w->mode3 = 2;
            if (w->mtn_md & 0x2u) en22_chg_mtn(w, 6, 0, 5); else { en22_chg_mtn(w, 6, 0, 5); w->mtn_md |= 0x2u; }
        }
        en22_add_speed(w, 0);
        break;
    case 6:
        en22_chg_mtn(w, 7, 0, 5);
        w->ayp = recvx_t3_niten_dir(w->px, w->pz, pl.pos.x, pl.pos.z);
        w->ayp = (w->ayp - w->ay) & 0xFFFF;
        if (w->ayp <= NJM_DEG_ANG(180.0f)) { w->ayp /= 32; w->mtn_md |= 0x2u; }
        else w->ayp = -(NJM_DEG_ANG(360.0f) - w->ayp) / 32;
        w->ct0 = 32; w->mode3++;
        /* fallthrough */
    case 7:
        if (w->spd > 0.5f) w->spd -= 0.02f; else w->spd = 0.5f;
        w->ay += w->ayp;
        if (--w->ct0 < 0) {
            w->ct1 = en22_rand(w) % 30 + 40; w->ct0 = 0; w->mode3 = 2;
            if (w->mtn_md & 0x2u) en22_chg_mtn(w, 6, 0, 5); else { en22_chg_mtn(w, 6, 0, 5); w->mtn_md |= 0x2u; }
        }
        en22_add_speed(w, -w->way);
        break;
    default: break;
    }
}
/* bhEne22_MV04 (:1694): der Stromstoss (Motion 9). */
static void en22_mv04(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    int frm;
    switch (w->mode3) {
    case 0:
        en22_chg_mtn(w, 9, 0, 5);
        w->ct0 = 30; w->ct1 = 0; w->ct2 = 0; p->word5C = 0; w->mode3++;
        /* fallthrough */
    case 1:
        frm = w->frm_no / 65536;
        if (p->flags & 0x10000u) {
            p->flags &= ~0x10000u;
            if (frm <= 20) {
                p->flags &= ~0x1000u;
                w->mode1 = 1; w->mode2 = 2; w->mode3 = 0;
                en22_light_effect_off(w);
                if (p->flags & 0x80000u) { en22_light_off(w); p->flags &= ~0x80000u; }
                break;
            }
        }
        if (w->ct0 <= 0) w->ct1++; else w->ct0--;
        p->reach = 4.0f * (float)w->ct1 + 24.0f;
        if (frm >= 21 && frm <= 60) {
            p->flags |= 0x20000u;
            if (en22_player(w, &pl, "plp (en22 MV04)") && p->dist <= p->reach && p->dist >= p->reach - 8.0f
                && !(pl.flg & 0x2u) && !(pl.flg & 0x4u) && !(pl.stflg & 0x80000000u)) {
                RECVX_ENEMY_VEC plpos = pl.pos;
                pl.flg |= 0x4u; pl.mode0 = 4; pl.mode1 = 0; pl.mode2 = 0; pl.mode3 = 0; pl.hp -= 30;
                en22_player_set(w, &pl);
                en22_set_electric_shock_effect2(w);
                en22_se_play(w, &plpos, 0x1012302);
            }
        } else p->flags &= ~0x20000u;
        if (en22_last_frame(w)) { p->flags &= ~0x1000u; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        break;
    default: break;
    }
}
/* bhEne22_MV06 (:1790). */
static void en22_mv06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    int frm;
    switch (w->mode3) {
    case 0: en22_chg_mtn(w, 2, 0, 7); w->ct1 = 0; w->mode3++;
        /* fallthrough */
    case 1:
        frm = w->frm_no / 65536;
        if (frm >= 0 && frm <= 30) w->ay -= NJM_DEG_ANG(3.0f);
        if (en22_last_frame(w)) { p->flags |= 0x200u; w->ct0 = 0; w->ct1 = 0; w->ct3 = 0; w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; }
        break;
    default: break;
    }
}
/* bhEne22_DG00 (:1830): alle vier Richtungen waehlen Motion 13. */
static void en22_dg00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_VEC trg = { p->trg_x, 0.0f, p->trg_z };
    switch (w->mode3) {
    case 0: en22_chg_mtn(w, 13, 0, 5); w->mode3++;
        /* fallthrough */
    case 1:
        if (en22_last_frame(w)) { p->flags |= 0x200u; w->mode0 = 1; w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; }
        w->spd = 0.6f;
        en22_ikou(w, &trg, 384); en22_add_speed(w, 0);
        break;
    default: break;
    }
}
/* bhEne22_DG01 (:1869). */
static void en22_dg01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    switch (w->mode3) {
    case 0:
        if ((p->flags & 0x30u) == 0) { en22_chg_mtn(w, 10, 0, 5); if ((en22_rand(w) % 2) == 0) w->mtn_md |= 0x2u; }
        else if ((p->flags & 0x30u) == 0x10u) en22_chg_mtn(w, 10, 0, 5);
        else if ((p->flags & 0x30u) == 0x20u) { en22_chg_mtn(w, 11, 0, 5); if ((en22_rand(w) % 2) == 0) w->mtn_md |= 0x2u; }
        else en22_chg_mtn(w, 10, 0, 5);
        w->mode3++;
        /* fallthrough */
    case 1:
        if (en22_last_frame(w)) { w->mode0 = 1; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        break;
    default: break;
    }
}
/* bhEne22_DD00 (:1925). */
static void en22_dd00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    switch (w->mode3) {
    case 0:
        en22_chg_mtn(w, (p->flags & 0x100u) ? 20u : 19u, 0, 5);
        w->spd = 0.8f; w->ct0 = 0; w->mode3++;
        /* fallthrough */
    case 1:
        if (w->mtn_no == 20u) { w->spd -= 0.01f; w->ct0++; if (w->spd < 0.0f) w->spd = 0.0f; en22_add_speed(w, 0); }
        if (w->flg & 0x2000000u) { w->flg |= 0x2u; w->flg &= ~0x28u; en22_chg_mtn(w, 22, 0, 0); w->mode3++; }
        break;
    default: break;
    }
}
/* bhEne22_PlyDG00 (:1975): Stromschlag ohne Griff (plp mode2 0). */
static void en22_ply_dg00(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    const RECVX_EN01_SERVICES* en = en22_en();
    RECVX_ENEMY_VEC dv;
    switch (pl->mode3) {
    case 0:
        pl->flg &= ~0x40000u; pl->flg |= 0x10000u; pl->flg |= 0x200000u;
        pl->frm_no = 65536; pl->hokan_count = 0; pl->hokan_rate = 49152; pl->mtn_add = 65536; pl->mtn_md = 0;
        pl->ct0 = 2; pl->ct1 = 0;
        dv.x = w->px - pl->pos.x; dv.y = w->py - pl->pos.y; dv.z = w->pz - pl->pos.z;
        pl->mtn_no = recvx_t3_dgc_dir_check(&dv, pl->ay) ? 73u : 74u;
        recvx_t3_set_vibration(1);
        recvx_t3_player_se_play(w, 1026);
        pl->mode3++;
        en22_player_set(w, pl);
        break;
    case 1:
        if ((pl->frm_no / 65536) == 0) { pl->ct0--; if (pl->ct0 != 0) recvx_t3_set_vibration(1); }
        if (pl->ct0 == 0) {
            pl->flg &= ~0x200000u; pl->flg &= ~0x4u;
            if (en && en->pad_on_clear) en->pad_on_clear(0xFu); else en22_gap(w, "sys->pad_on &= ~0xF (en22)");
            pl->flg &= ~0x10000u; pl->flg |= 0x8u; pl->stflg &= ~0x10000u; pl->at_flg = 0;
            pl->mode0 = 1; pl->mode1 = 0; pl->mode2 = 0; pl->mode3 = 0;     /* *(int*)&plp->mode0 = 1 */
        }
        en22_player_set(w, pl);
        break;
    default: break;
    }
}
/* bhEne22_PlyDG01 (:2040): Griff mit den Spielermotionen 30..33 aus der Gegnerbibliothek. */
static void en22_ply_dg01(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl)
{
    const RECVX_EN01_SERVICES* en = en22_en();
    RECVX_ENEMY_VEC dv, jpos;
    int rot, frm;
    switch (pl->mode3) {
    case 0:
        pl->flg &= ~0x40000u; pl->flg |= 0x10000u; pl->flg |= 0x200000u;
        pl->mtn_enemy = 1; pl->mtn_enemy_work = w; pl->mtn_enemy_owner = (uint32_t)w->idx_ct;   /* pl->mnwP = epw->mnwP */
        pl->frm_no = 65536; pl->hokan_count = 0; pl->hokan_rate = 49152; pl->mtn_add = 65536; pl->mtn_md = 0;
        pl->ct0 = 0; pl->ct1 = 0;
        dv.x = w->px - pl->pos.x; dv.y = w->py - pl->pos.y; dv.z = w->pz - pl->pos.z;
        if (recvx_t3_dgc_dir_check(&dv, pl->ay)) { pl->mtn_no = 31; pl->ayp = (int32_t)(10430.381f * atan2f(-dv.x, -dv.z)); }
        else { pl->mtn_no = 30; pl->ayp = (int32_t)(10430.381f * atan2f(dv.x, dv.z)); pl->ct0 = 1; }
        recvx_t3_player_se_play(w, 1026);
        recvx_t3_set_vibration(1);
        pl->mode3++;
        en22_player_set(w, pl);
        break;
    case 1:
        frm = pl->frm_no / 65536;
        if (frm < 10) {
            rot = (pl->ayp - pl->ay) & 0xFFFF;
            if (rot > NJM_DEG_ANG(180.0f)) rot -= NJM_DEG_ANG(360.0f);
            pl->ay += rot / 2;
        }
        if (frm >= 6 && frm <= 18 && (frm % 3) == 0) {
            if (en22_player_joint_pos(w, 1, &jpos)) {
                if (w->sv && w->sv->set_effect) (void)w->sv->set_effect(108, &jpos, NULL, NULL, 15);
                else en22_gap(w, "bhSetEffect(108) (en22)");
                if (en22_extra && en22_extra->player_water_splash) en22_extra->player_water_splash(&jpos, 1, 2.0f, 2.0f, 2.0f);
                else en22_gap(w, "bhSetWaterSplash2(plp) (en22)");
            }
        }
        if (frm == 0) { pl->mtn_no = (pl->mtn_no == 31u) ? 33u : 32u; pl->mode3++; }
        en22_player_set(w, pl);
        break;
    case 2:
        frm = pl->frm_no / 65536;
        if (frm == 0) {
            if (en && en->player_node_clear_xz) en->player_node_clear_xz(0); else en22_gap(w, "plp->mlwP->objP->pos[0/2] = 0 (en22)");
            pl->flg &= ~0x200000u;
            if (en && en->pad_on_clear) en->pad_on_clear(0xFu); else en22_gap(w, "sys->pad_on &= ~0xF (en22)");
            pl->flg &= ~0x10000u; pl->flg |= 0x8u; pl->stflg &= ~0x10000u; pl->at_flg = 0;
            pl->mtn_enemy = 0; pl->mtn_enemy_work = NULL;                 /* pl->mnwP = pl->mnwPb */
            pl->mode0 = 1; pl->mode1 = 0; pl->mode2 = 0; pl->mode3 = 0;
            pl->flg &= ~0x4u;
        }
        en22_player_set(w, pl);
        break;
    default: break;
    }
    if (pl->mode0 == 4 || pl->mode0 == 6) {                             /* :2143-2163 Wurzelversatz aus ply_mtn4xb_pos */
        const EN22_POINT2_XZ* pos_p;
        NJS_MATRIX m;
        NJS_POINT3 key, out;
        uint32_t row = pl->mtn_no - 30u;
        if (row >= 4u) { en22_gap(w, "mtn_pos[mtn_no - 30] ausserhalb (en22.c:2145)"); return; }
        frm = pl->frm_no / 65536;
        if ((row < 2u && (frm < 0 || frm >= 20)) || (row >= 2u && (frm < 0 || frm >= 50))) { en22_gap(w, "ply_mtn4xb_pos[frm] ausserhalb (en22.c:2148)"); return; }
        pos_p = (row == 0) ? ply_mtn42b_pos : (row == 1) ? ply_mtn43b_pos : (row == 2) ? ply_mtn44b_pos : ply_mtn45b_pos;
        pos_p += frm;
        key.x = pos_p->px; key.y = 0.0f; key.z = pos_p->pz;
        njUnitMatrix(&m);
        njTranslate(&m, pl->pos.x, pl->pos.y, pl->pos.z);
        njRotateXYZ(&m, pl->ax, pl->ay, pl->az);
        njCalcPoint(&m, &key, &out);
        pl->pos.x = out.x; pl->pos.y = out.y; pl->pos.z = out.z;
        en22_player_set(w, pl);
    }
}
/* bhEne22_SetMtn (:2166). */
static int en22_set_mtn(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    int frm, ret;
    if (!p || (p->flags & 0x80u)) return 0;
    frm = w->frm_no / 65536;
    if (w->sv && w->sv->set_motion) ret = w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    else { en22_gap(w, "bhSetMotion (en22)"); ret = 0; }
    if (ret != 0) w->flg |= 0x2000000u; else w->flg &= ~0x2000000u;
    if (p->flags & 0x20000000u) {
        const RECVX_EN01_SERVICES* en = en22_en();
        if (en && en->node_local_clear_xz) en->node_local_clear_xz(w, 0u); else en22_gap(w, "mlwP->objP->pos[0/2] = 0 (en22)");
        en22_get_translate_mtn(w, frm);
    }
    if (w->mode0 < 5 && (w->stflg & 0x100000u)) en22_set_water_effect(w, (int)w->mtn_no, frm);
    if (w->mtn_no == 9u) en22_spark_effect(w, frm);
    en22_check_mtn_tbl(w, frm);
    return ret;
}
/* bhEne22_CheckMtnTbl (:2212). */
static void en22_check_mtn_tbl(RECVX_ENEMY_WORK* w, int frm)
{
    const EN22_MTN_WORK* mtbl = en22_mtn_tbl;
    RECVX_ENEMY_VEC pos = en22_pos(w);
    int i;
    if (w->mtn_room) return;                                            /* sys->rmthp == epw->mnwP */
    while (mtbl->no != -1) {
        if (mtbl->no == (int32_t)w->mtn_no) {
            for (i = 0; i < 4; i++) {
                if (mtbl->atb[i].frm == -1) break;
                if (mtbl->atb[i].frm == frm) en22_se_play(w, &pos, mtbl->atb[i].act);
            }
        }
        mtbl++;
    }
}
/* bhEne22_GetTranslateMtn (:2244): Wurzelkey der Motion auf frm, Differenz zum gemerkten Key. */
static void en22_get_translate_mtn(RECVX_ENEMY_WORK* w, int frm)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_VEC key, pos;
    NJS_MATRIX m;
    NJS_POINT3 in, out;
    if (!(w->sv && w->sv->motion_root_position && w->sv->motion_root_position(w, w->mtn_no, (uint32_t)frm, &key))) {
        en22_gap(w, "mnwP[mtn_no].md2P->p[0] (en22 GetTranslateMtn)"); return;
    }
    if (w->mtn_add != 0 || (w->frm_no / 65536) != frm) {
        if (frm == 0) {
            if (p->flags & 0x40000000u) { pos.x = key.x - p->key_x; pos.y = 0.0f; pos.z = key.z - p->key_z; }
            else { pos.x = key.x; pos.y = 0.0f; pos.z = key.z; }
        } else { pos.x = key.x - p->key_x; pos.y = 0.0f; pos.z = key.z - p->key_z; }
        if (w->mtn_md & 0x2u) pos.x *= -1.0f;
        p->key_x = key.x; p->key_y = key.y; p->key_z = key.z;
        njUnitMatrix(&m);
        njTranslate(&m, w->px, w->py, w->pz);
        njRotateXYZ(&m, w->ax, w->ay, w->az);
        in.x = pos.x; in.y = pos.y; in.z = pos.z;
        njCalcPoint(&m, &in, &out);
        w->px = out.x; w->py = out.y; w->pz = out.z;
    }
}
/* bhEne22_GetDengekiColorAddr (:2416) / ChgDengekiColor (:2993): gemeinsamer plist-Lauf.
   Der Kopf ist das niedrige Byte des Wortes; 64..66 und 56..58 tragen ein Groessenwort,
   8 ein Zusatzwort, 17..23 sind die gezaehlten Materialchunks, 255 endet. */
int recvx_en22_material_chunk(const uint8_t* plist, size_t size, int no, size_t* color_at, int* head)
{
    size_t at = 0;
    int max = 0;
    while (at + 2u <= size) {
        const int h = plist[at];
        at += 2u;
        if ((h >= 64 && h < 67) || (h >= 56 && h <= 58)) {
            int16_t offset; if (at + 2u > size) return 0;
            offset = (int16_t)(plist[at] | (plist[at + 1u] << 8)); at += 2u + 2u * (size_t)(int)offset;
        } else if (h == 8) {
            at += 2u;
        } else if (h >= 17 && h < 24) {
            int16_t offset; if (at + 2u > size) return 0;
            if (no == max) { *color_at = at + 2u; *head = h; return 1; }
            max++;
            offset = (int16_t)(plist[at] | (plist[at + 1u] << 8)); at += 2u + 2u * (size_t)(int)offset;
        } else if (h == 255) {
            return 0;
        }
    }
    return 0;
}
static uint8_t* en22_material(RECVX_ENEMY_WORK* w, int mdl, int no, int* head, size_t* bytes)
{
    uint8_t* plist = NULL; size_t size = 0, at = 0;
    if (!en22_extra || !en22_extra->model_plist || !en22_extra->model_plist(w, mdl, 1, &plist, &size)) { en22_gap(w, "epw->mdl[j + 1].objP[1].model->plist (en22)"); return NULL; }
    if (!recvx_en22_material_chunk(plist, size, no, &at, head)) return NULL;
    *bytes = size - at;
    return plist + at;
}
/* bhEne22_ChgDengekiColor (:2993): 17/21 schreiben B,G,R,A einmal, 19/23 zweimal; 18/20/22 nichts. */
static void en22_chg_dengeki_color(RECVX_ENEMY_WORK* w, int mdl, int no, uint32_t argb)
{
    const uint8_t c[4] = { (uint8_t)argb, (uint8_t)(argb >> 8), (uint8_t)(argb >> 16), (uint8_t)(argb >> 24) };
    int head = 0; size_t bytes = 0;
    uint8_t* mat = en22_material(w, mdl, no, &head, &bytes);
    if (!mat) return;                                                   /* Endchunk: return (:3064) */
    if ((head == 17 || head == 21) && bytes >= 4u) memcpy(mat, c, 4);
    else if ((head == 19 || head == 23) && bytes >= 8u) { memcpy(mat, c, 4); memcpy(mat + 4, c, 4); }
}
/* *bhEne22_GetDengekiColorAddr(obj, no) = argb: ein unsigned-int-Wort (EE little endian). */
static void en22_put_dengeki_color(RECVX_ENEMY_WORK* w, int mdl, int no, uint32_t argb)
{
    const uint8_t c[4] = { (uint8_t)argb, (uint8_t)(argb >> 8), (uint8_t)(argb >> 16), (uint8_t)(argb >> 24) };
    int head = 0; size_t bytes = 0;
    uint8_t* mat = en22_material(w, mdl, no, &head, &bytes);
    if (!mat || bytes < 4u) { en22_gap(w, "bhEne22_GetDengekiColorAddr: kein Materialchunk no (Rueckgabe undefiniert, :2458)"); return; }
    memcpy(mat, c, 4);
}
/* bhEne22_SparkEffect (:2294): Materialfarben der Blitzmeshes mdl[1..6], Blitzeffekte, Lichtsatz. */
static void en22_spark_effect(RECVX_ENEMY_WORK* w, int frm)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    int i, j, no;
    if (frm == 0) {
        for (j = 0; j < 6; j++) {
            for (i = 0; i < 20; i++) {
                p->argb[i] = 0xB2B2B2u;
                en22_chg_dengeki_color(w, j + 1, 4 * i, p->argb[i]);
                en22_chg_dengeki_color(w, j + 1, 4 * i + 1, p->argb[i]);
            }
        }
        p->spark_step = 0;
        p->light_effect = -1;
    } else if (frm >= 30 && frm < 70) {
        for (i = 0; i < 20; i++) {
            if (i <= p->spark_step) p->argb[i] = (p->spark_step - i < 20) ? ene22_eff_col[p->spark_step - i] : 0xB2B2FFu;
            else p->argb[i] = 0xB2B2FFu;
        }
        p->spark_step++;
        for (j = 0; j < 6; j++) {
            for (i = 0; i < 20; i++) {
                no = (2 * i + 20) % 40;
                /* *ulpMatAdrTbl[j][no][0/1] = *argb (:2386-2387): ein 32-Bit-Wort an der
                   ersten Farbe, die bhEne22_GetDengekiColorAddr bei frm 0 ermittelt hat. */
                en22_put_dengeki_color(w, j + 1, no, p->argb[i]);
                en22_put_dengeki_color(w, j + 1, no + 1, p->argb[i]);
            }
        }
        en22_set_dengeki_effect2(w);
    }
    if (frm >= 11 && frm < 55) {
        if ((frm % 2) == 0) en22_set_electric_shock_effect(w, 0);
        if (frm == 21) en22_set_electric_shock_effect(w, 2);
    }
    if (frm == 11) {
        RECVX_ENEMY_VEC ps = { 0.0f, 10.0f, 0.0f };
        p->light_effect = en22_set_electric_light_effect(w);
        en22_set_light(w, 4, &ps, 1);
        p->flags |= 0x1000u;
    }
}
/* bhEne22_SetWaterEffect (:2464): Effekt 108 an den Tabellengelenken, Wasserspritzer 2 + njRandom. */
static void en22_set_water_effect(RECVX_ENEMY_WORK* w, int mtn_no, int frm)
{
    const RECVX_EN01_SERVICES* en = en22_en();
    const EN22_WEFF_WORK* eff = en22_weff_tbl;
    const EN22_WSP_WORK* eff2 = en22_wsp_tbl;
    RECVX_ENEMY_VEC j;
    float size;
    while (eff->mtn != -1) {
        if (eff->mtn == mtn_no && eff->frm == frm && en22_joint_pos(w, (uint32_t)eff->obj, &j)) {
            if (w->sv && w->sv->set_effect) (void)w->sv->set_effect(108, &j, NULL, NULL, 15);
            else en22_gap(w, "bhSetEffect(108) (en22)");
        }
        eff++;
    }
    while (eff2->mtn != -1) {
        if (eff2->mtn == mtn_no && eff2->frm == frm) {
            size = 2.0f + en22_njrandom(w);
            if (en22_joint_pos(w, (uint32_t)eff2->obj, &j)) {
                if (en && en->water_splash) en->water_splash(w, &j, 1, size, size, size);
                else en22_gap(w, "bhSetWaterSplash2 (en22)");
            }
        }
        eff2++;
    }
}
/* bhEne22_GetAreaNo (:2512) / AreaCheck (:2536) / SetTrgPos (:2547). */
int recvx_en22_get_area_no(float px, float pz)
{
    static const float trg_atari[4][4] = { {0.0f, 0.0f, 70.0f, 70.0f}, {70.0f, 0.0f, 70.0f, 70.0f}, {0.0f, 70.0f, 70.0f, 70.0f}, {70.0f, 70.0f, 70.0f, 70.0f} };
    int i;
    for (i = 0; i < 4; i++) if (recvx_t3_pos_check(px, pz, trg_atari[i][0], trg_atari[i][1], trg_atari[i][2], trg_atari[i][3]) != 0) return i;
    return -1;
}
static int en22_area_check(float ene_x, float ene_z, float ply_x, float ply_z)
{
    return (recvx_en22_get_area_no(ene_x, ene_z) == recvx_en22_get_area_no(ply_x, ply_z)) ? 1 : 0;
}
static int en22_set_trg_pos(RECVX_ENEMY_WORK* w)
{
    static const float trg_atari[4][4] = { {30.0f, 30.0f, 40.0f, 40.0f}, {70.0f, 30.0f, 40.0f, 40.0f}, {30.0f, 70.0f, 40.0f, 40.0f}, {70.0f, 70.0f, 40.0f, 40.0f} };
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_ENEMY_PLAYER pl;
    float near_dist = 0.0f, dist, x = 0.0f, z = 0.0f;
    int ene_at, ply_at, i;
    if (!en22_player(w, &pl, "plp (en22 SetTrgPos)")) return 0;
    ene_at = recvx_en22_get_area_no(w->px, w->pz);
    ply_at = recvx_en22_get_area_no(pl.pos.x, pl.pos.z);
    if (ene_at == -1) return 0;
    for (i = 0; i < 4; i++) {
        if (i != ene_at && i != ply_at) {
            const float cx = trg_atari[i][0] + trg_atari[i][2] / 2.0f, cz = trg_atari[i][1] + trg_atari[i][3] / 2.0f;
            dist = sqrtf((cx - w->px) * (cx - w->px) + (cz - w->pz) * (cz - w->pz));
            if (near_dist > dist || near_dist == 0.0f) { near_dist = dist; x = cx; z = cz; }
        }
    }
    if (near_dist != -1.0f) { p->trg_x = x; p->trg_z = z; return 1; }
    return 0;
}
/* bhEne22_SetDengekiEffect (:2621): Effekt 354 (Blitzbogen) mit tex_id 6, zn 1.5. */
static int en22_set_dengeki_effect(RECVX_ENEMY_WORK* w, int obj, const RECVX_ENEMY_VEC* ofs, float size, int on_player)
{
    RECVX_EN22_EF ef; RECVX_EN22_EFSLOT sl; int eno;
    if (!en22_extra || !en22_extra->effect_tb || !en22_extra->effect_slot) { en22_gap(w, "bhSetEffectTb (en22 Dengeki)"); return -1; }
    memset(&ef, 0, sizeof ef);
    ef.id = 354; ef.flg = 0x1u; ef.type = 0; ef.sx = size; ef.sy = size; ef.sz = size; ef.ay = 0;
    if (obj == -1) { ef.px = ofs->x; ef.py = ofs->y; ef.pz = ofs->z; eno = en22_extra->effect_tb(&ef, NULL, NULL, 0, 0); }
    else { ef.px = 0.0f; ef.py = 0.0f; ef.pz = 0.0f; eno = en22_extra->effect_tb(&ef, ofs, on_player ? NULL : w, on_player, obj); }
    if (eno == -1) return -1;
    memset(&sl, 0, sizeof sl);
    sl.stflg_or = 0x20u; sl.tex_from = w; sl.tex_id = 6; sl.ct3 = 0; sl.zn = 1.5f;
    en22_extra->effect_slot(eno, RECVX_EN22_EFS_STFLG | RECVX_EN22_EFS_TEX | RECVX_EN22_EFS_CT3 | RECVX_EN22_EFS_ZN, &sl);
    return eno;
}
static void en22_effect_ct3(RECVX_ENEMY_WORK* w, int eno, int32_t ct3)
{
    RECVX_EN22_EFSLOT sl;
    if (eno < 0) return;
    memset(&sl, 0, sizeof sl); sl.ct3 = ct3;
    if (en22_extra && en22_extra->effect_slot) en22_extra->effect_slot(eno, RECVX_EN22_EFS_CT3, &sl);
    else en22_gap(w, "eff[eno].ct3 (en22)");
}
/* bhEne22_SetDengekiEffect2 (:2658): acht 364 um das Werk mit zufaelligem Blitzmodell mdl[1..6]. */
static void en22_set_dengeki_effect2(RECVX_ENEMY_WORK* w)
{
    RECVX_EN22_EF ef; RECVX_EN22_EFSLOT sl; int eno, i, ang;
    if (!en22_extra || !en22_extra->effect_tb || !en22_extra->effect_slot) { en22_gap(w, "bhSetEffectTb (en22 Dengeki2)"); return; }
    memset(&ef, 0, sizeof ef);
    ef.id = 364; ef.type = 1; ef.flg = 0x1u; ef.sx = 1.0f; ef.sy = 1.0f; ef.sz = 1.0f; ef.ax = 0; ef.ay = 0;
    ef.px = w->px + w->aox; ef.pz = w->pz + w->aoz; ef.py = w->py + 4.5f;
    ef.mdlver = 1; ef.mask = RECVX_EN22_EF_AX | RECVX_EN22_EF_MDLVER;
    ang = w->ay + (en22_rand(w) % 2) * NJM_DEG_ANG(22.5f);
    for (i = 0; i < 8; i++) {
        eno = en22_extra->effect_tb(&ef, NULL, NULL, 0, 0);
        /* (eno != -1) || (eno < 512) (:2678) ist immer wahr: auch bei vollem Pool laufen
           rand() und der Winkelschritt, die Schreibzugriffe treffen eff[-1]. */
        memset(&sl, 0, sizeof sl);
        sl.stflg_or = 0x20u; sl.tex_id = 0; sl.model = (en22_rand(w) % 6) + 1; sl.model_of = w; sl.tex_from = NULL;
        sl.flr_no = 0; sl.ax = 0; sl.ay = ang & 0xFFFF; sl.az = 0;
        if (eno != -1) en22_extra->effect_slot(eno, RECVX_EN22_EFS_STFLG | RECVX_EN22_EFS_TEX | RECVX_EN22_EFS_MODEL | RECVX_EN22_EFS_FLR | RECVX_EN22_EFS_ANG, &sl);
        else en22_gap(w, "eff[-1] (en22.c:2680 bhEne22_SetDengekiEffect2 bei vollem Effektpool)");
        ang += NJM_DEG_ANG(45.0f);
    }
}
/* bhEne22_SetElectricShockEffect (:2698). */
static void en22_set_electric_shock_effect(RECVX_ENEMY_WORK* w, int type)
{
    const EN22_DEN_WORK* t;
    RECVX_ENEMY_VEC ps;
    int i, op;
    if (type == 0) {
        t = &en22_den_tbl[en22_rand(w) % 5];
        ps.x = (float)((double)t->ofs.x + 6.0 * (double)en22_njrandom(w) - 3.0);  /* double wie im Original */ ps.y = t->ofs.y; ps.z = t->ofs.z;
        (void)en22_set_dengeki_effect(w, t->obj, &ps, 2.0f * en22_njrandom(w) + 6.0f, 0);
    } else if (type == 1) {
        for (i = 0; i < 4; i++) {
            t = &en22_den_tbl[en22_rand(w) % 2];
            ps.x = (float)((double)t->ofs.x + 6.0 * (double)en22_njrandom(w) - 3.0);  /* double wie im Original */ ps.y = t->ofs.y; ps.z = t->ofs.z;
            op = en22_set_dengeki_effect(w, t->obj, &ps, 2.0f * en22_njrandom(w) + 6.0f, 0);
            en22_effect_ct3(w, op, 2 * i);
        }
    } else if (type == 2) {
        t = &en22_den_tbl[5];
        while (t->obj != -1) {
            op = en22_set_dengeki_effect(w, t->obj, &t->ofs, 5.0f, 0);
            en22_effect_ct3(w, op, t->wait);
            t++;
        }
    } else {
        for (i = 0; i < 5; i++) {
            t = &en22_den_tbl[en22_rand(w) % 5];
            ps.x = (float)((double)t->ofs.x + 6.0 * (double)en22_njrandom(w) - 3.0);  /* double wie im Original */ ps.y = t->ofs.y; ps.z = t->ofs.z;
            op = en22_set_dengeki_effect(w, t->obj, &ps, 2.0f * en22_njrandom(w) + 6.0f, 0);
            en22_effect_ct3(w, op, 2 * i);
        }
    }
}
/* bhEne22_SetElectricShockEffect2 (:2795): vier 354 am Spieler (Gelenke 1,1,2,3), Textur des Gegners. */
static void en22_set_electric_shock_effect2(RECVX_ENEMY_WORK* w)
{
    static const int obj[4] = { 1, 1, 2, 3 };
    static const float ply_ofs[4] = { -3.0f, 0.0f, 0.0f, 1.0f };
    RECVX_ENEMY_VEC ps;
    int i, op;
    for (i = 0; i < 4; i++) {
        ps.y = ply_ofs[i];
        ps.x = (float)((double)en22_njrandom(w) - 0.5);
        ps.z = (float)((double)en22_njrandom(w) - 0.5);
        op = en22_set_dengeki_effect(w, obj[i], &ps, 7.0f, 1);
        if (op != -1) {
            RECVX_EN22_EFSLOT sl; memset(&sl, 0, sizeof sl);
            sl.tex_from = w; sl.tex_id = 6; sl.ct3 = 4 * i;                /* txp[0] = epw->mlwP->texP, ct3 */
            en22_extra->effect_slot(op, RECVX_EN22_EFS_TEX | RECVX_EN22_EFS_CT3, &sl);
        }
    }
}
/* bhEne22_SetElectricLightEffect (:2795): 353 am Wurzelgelenk, tex_id 5. */
static int en22_set_electric_light_effect(RECVX_ENEMY_WORK* w)
{
    static const RECVX_ENEMY_VEC ofs = { 0.0f, 0.1f, -5.0f };
    RECVX_EN22_EF ef; RECVX_EN22_EFSLOT sl; int eno;
    if (!en22_extra || !en22_extra->effect_tb || !en22_extra->effect_slot) { en22_gap(w, "bhSetEffectTb (en22 Blitzlicht)"); return -1; }
    memset(&ef, 0, sizeof ef);
    ef.id = 353; ef.flg = 0x1u; ef.type = 0; ef.sx = 25.0f; ef.sy = 25.0f; ef.sz = 25.0f; ef.ay = 0; ef.px = 0.0f; ef.py = 0.0f; ef.pz = 0.0f;
    eno = en22_extra->effect_tb(&ef, &ofs, w, 0, 0);
    if (eno == -1) return -1;
    memset(&sl, 0, sizeof sl); sl.stflg_or = 0x20u; sl.tex_from = w; sl.tex_id = 5; sl.ct3 = 0;
    en22_extra->effect_slot(eno, RECVX_EN22_EFS_STFLG | RECVX_EN22_EFS_TEX | RECVX_EN22_EFS_CT3, &sl);
    return eno;
}
/* bhEne22_SetLight (:2857): rom->lgtp[2] neu beschreiben. */
static void en22_set_light(RECVX_ENEMY_WORK* w, int lnk_obj, const RECVX_ENEMY_VEC* ofs, int type)
{
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_EN22_LIGHT l;
    if (!en22_light_get(w, &l)) return;
    p->flags |= 0x80000u;
    l.flg = 0x3u; l.aspd = 4; l.lsrc = 4; l.type = 0;
    l.r = 0.0f; l.g = 0.0f; l.b = 0.0f; l.nr = 20.0f; l.fr = 40.0f;
    if (lnk_obj == -1) { l.lkflg = 0; l.lkno = 0; l.lkono = 0; l.px = ofs->x; l.py = ofs->y; l.pz = ofs->z; }
    else { l.lkflg = 0x2; l.lkno = w->idx_ct; l.lkono = 3; l.lx = ofs->x; l.ly = ofs->y; l.lz = ofs->z; l.px = 0.0f; l.py = 0.0f; l.pz = 0.0f; }
    l.mode = (type == 0) ? 0 : 2;
    en22_light_set(w, &l);
}
/* bhEne22_CtrLight (:2905). */
void recvx_en22_ctr_light(RECVX_ENEMY_WORK* w)
{
    static const float rgb[3] = { 1.0f, 1.0f, 6.375f };
    RECVX_EN22_EXP0* p = recvx_en22_exp0(w);
    RECVX_EN22_LIGHT l;
    float fl;
    if (!p || !(p->flags & 0x80000u)) return;
    if (!en22_light_get(w, &l)) return;
    if (!((l.flg & 0x1u) && (l.flg & 0x2u))) return;
    switch (l.mode) {
    case 0: l.ct0 = 0; l.ct1 = 16; l.aspd = 8; l.mode++;
        /* fallthrough */
    case 1:
        fl = njSin(l.ct0); l.r = fl * rgb[0]; l.g = fl * rgb[1]; l.b = fl * rgb[2];
        l.ct0 = (l.ct0 + 256 * l.aspd) & 0x7FFF;
        if (--l.ct1 < 0) { l.flg &= ~0x3u; p->flags &= ~0x80000u; }
        break;
    case 2: l.aspd = 4; l.ct0 = 0; l.ct1 = 16; l.mode++;
        /* fallthrough */
    case 3:
        fl = njSin(l.ct0); l.r = fl * rgb[0]; l.g = fl * rgb[1]; l.b = fl * rgb[2];
        l.ct0 = (l.ct0 + 256 * l.aspd) & 0x7FFF;
        if (--l.ct1 < 0) { l.mode++; l.ct1 = 4; }
        break;
    case 4:
        if (--l.ct1 < 0) { l.mode++; l.ct1 = 22; }
        /* fallthrough */
    case 5:
        l.nr += 0.8f; l.fr += 4.0f;
        if (--l.ct1 < 0) { l.mode++; l.ct1 = 15; }
        break;
    case 6:
        fl = njSin(l.ct0); l.r = fl * rgb[0]; l.g = fl * rgb[1]; l.b = fl * rgb[2];
        l.ct0 = (l.ct0 + 256 * l.aspd) & 0x7FFF;
        if (--l.ct1 < 0) { l.flg &= ~0x3u; p->flags &= ~0x80000u; }
        break;
    default: break;
    }
    en22_light_set(w, &l);
}
