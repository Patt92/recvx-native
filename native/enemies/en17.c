/* EN17 Monster Steve — eigenstaendige native Zustandsmaschine auf dem einzigen
   WorldEntity-Work. Semantikreferenz (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en17.c:347 bhEne17, :404 EneToPlyDist, :417 MainLoop,
   :425 DmgChk, :454 ChgDmgMode, :487 DamageAdd, :532 SetMtn, :671 MtnTblPlay,
   :740 CollCheck, :763 CalcEnemy, :789 Init, :879 SetLinkWork, :905-:1280
   Move/Damage/Brain/MV00..MV06, :1287 DG00, :1364 PlyDG00, :1440 PlyDG01,
   :1565 PlayerDGCheck, :1664 SePlay, :1672 CameraControl, :1689 Afterimage,
   :1724-:1849 SmokeEffect/2/3, :1852 SetLight; en17sub.c:23-203 LArm/RArm/AxsCheck.
   Tabellen en17.c:13-338 als reine Daten. Ungebundene Dienste melden den
   Originalnamen und erfinden keinen Wert. */
#include "en17.h"
#include "en03_lib.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const RECVX_EN17_SERVICES* en17_sv;
void recvx_en17_bind(const RECVX_EN17_SERVICES* services) { en17_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
#define SV(w, f, name)    do { if (!en17_sv || !en17_sv->f) { gap(w, name); return; } } while (0)
#define BASE(w, f, name)  do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return; } } while (0)
#define BASEI(w, f, name) do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return 0; } } while (0)

static RECVX_EN17_EXP0* x0(RECVX_ENEMY_WORK* w) { return (RECVX_EN17_EXP0*)w->exp0; }
static RECVX_EN17_EXP1* x1(RECVX_ENEMY_WORK* w) { return (RECVX_EN17_EXP1*)w->exp1; }

/* --- Datentabellen (en17.c:13-338) ------------------------------------------- */
static const int8_t En17_SdwTab[7] = { 2, 5, 15, 20, 9, 13, -1 };
/* WPNDAMAGE_WORK: flg, nm_act, nm_blood, cb_act, cb_blood. */
static const int32_t En17_WpnDamageTbl[22][5] = {
    {  0,  0,  0,  0,  0 }, {  0,  0,  0,  0,  0 }, {  0,  4,  3,  3,  3 }, {  0,  4, 10,  3, 10 },
    {  0,  4, 10,  3, 10 }, {  0,  4, 10,  3, 10 }, {  0,  2, 10,  3, 10 }, {  0,  4, 10,  3, 10 },
    {  0,  4, 10,  3, 10 }, {  0,  4, 10,  3, 10 }, {  0,  4,  7,  3,  7 }, {  0,  2,  5,  3,  5 },
    {  0,  4, 10,  3, 10 }, {  0,  2,  3,  3,  3 }, {  0,  2, 10,  3, 10 }, { 12,  2,  1,  3,  3 },
    {  5,  2,  1,  3,  3 }, {  0,  2,  1,  3,  3 }, {  6,  2,  1,  3,  3 }, {  0,  2,  1,  3,  3 },
    {  6,  2,  1,  3,  3 }, {  0,  2,  1,  3,  3 }
};
static const RECVX_T3_COMBWEP en17_comb_wep[21] = {
    {  0, {  0,  0,  0 }, 0, 0 }, {  0, {  0,  0,  0 }, 0, 0 }, { 60, { 10,  0,  0 }, 0, 0 },
    { 60, { 20, 20, 20 }, 0, 0 }, { 60, { 20, 20, 20 }, 0, 0 }, { 60, { 20, 20, 20 }, 0, 0 },
    { 60, { 60, 60, 60 }, 0, 0 }, { 60, { 20, 20, 20 }, 0, 0 }, { 60, {  4,  4,  4 }, 0, 0 },
    { 60, { 20, 20, 20 }, 0, 0 }, { 60, { 20, 10, 10 }, 0, 0 }, { 60, { 60, 60, 30 }, 0, 0 },
    { 60, {  8,  8,  8 }, 0, 0 }, { 60, { 60, 60, 60 }, 0, 0 }, { 60, { 60, 60,  0 }, 0, 0 },
    { 60, { 60, 60,  0 }, 0, 0 }, { 60, { 60, 60, 20 }, 0, 0 }, { 60, { 10, 10,  0 }, 0, 0 },
    { 60, { 60, 60, 60 }, 0, 0 }, { 60, { 60, 60, 60 }, 0, 0 }, { 60, { 60, 60, 60 }, 0, 0 }
};
static const RECVX_T3_COMBJOINT en17_comb_joint[24] = { { 0u, 0 } };
static const RECVX_ENEMY_CPCL en17_cap_col[25] = {
    {   1,   2,  20 }, {   3,   3,  25 }, {   0,  10,   0 }, {   3,   3,  30 }, {   0,  40,  16 },
    {   6,   6,  23 }, { -30,  24,   0 }, {  10,  10,  23 }, {  30,  24,   0 }, {   4,   5,   8 },
    {   5,   5,  13 }, {   0,  10,  -5 }, {   7,   8,  10 }, {   8,   9,   7 }, {  11,  12,  10 },
    {  12,  13,   7 }, {  14,  15,  14 }, {  15,  16,  10 }, {  16,  17,   7 }, {  17,  18,  10 },
    {  19,  20,  14 }, {  20,  21,  10 }, {  21,  22,   7 }, {  22,  23,  10 }, {   0,   0,   0 }
};
/* en17prt_blood_tbl[24] (BT_WORK): lnk_obj, x, y, xlen, ylen, size, len — wie RECVX_T3_BT. */
static const RECVX_T3_BT en17prt_blood_tbl[24] = {
    {  0,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f }, {  1,  0.0f, -1.0f,  1.0f,  1.5f,  1.0f,  3.0f,  1.0f },
    {  2,  0.0f,  1.0f,  1.5f,  1.0f,  0.5f,  2.0f,  1.0f }, {  3,  0.0f,  2.5f,  2.5f,  1.0f,  1.0f,  3.0f,  1.0f },
    {  4,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  4.0f,  1.0f }, {  5,  0.0f,  1.0f,  2.0f,  0.5f,  0.5f,  2.0f,  0.5f },
    {  6,  0.3f,  0.0f,  2.0f,  0.0f,  0.5f,  2.0f,  1.0f }, {  7,  0.0f,  0.0f,  1.5f,  0.5f,  0.5f,  4.0f,  0.5f },
    {  8,  0.0f,  0.0f,  0.0f,  0.5f,  0.5f,  4.0f,  1.0f }, {  9,  0.0f,  0.0f,  0.0f,  0.5f,  0.5f,  3.0f,  0.5f },
    { 10, -0.3f,  0.0f,  2.0f,  0.0f,  0.5f,  2.0f,  1.0f }, { 11,  0.0f,  0.0f,  1.5f,  0.5f,  0.5f,  4.0f,  0.5f },
    { 12,  0.0f,  0.0f,  0.0f,  0.5f,  0.5f,  4.0f,  1.0f }, { 13,  0.0f,  0.0f,  0.0f,  0.5f,  0.5f,  3.0f,  0.5f },
    { 14,  0.0f, -3.0f,  1.5f,  0.5f,  1.5f,  5.0f,  1.0f }, { 15,  0.0f,  0.0f,  0.5f,  0.5f,  0.0f,  3.0f,  1.0f },
    { 16,  0.0f, -1.0f,  0.5f,  0.5f,  1.0f,  3.0f,  1.0f }, { 17,  0.0f,  0.0f,  0.5f,  0.5f,  0.0f,  2.0f,  1.0f },
    { 18,  0.0f,  0.0f,  0.5f,  0.5f,  0.0f,  4.0f,  1.0f }, { 19,  0.0f, -3.0f,  1.5f,  0.5f,  1.5f,  5.0f,  1.0f },
    { 20,  0.0f,  0.0f,  0.5f,  0.5f,  0.0f,  3.0f,  1.0f }, { 21,  0.0f, -1.0f,  0.5f,  0.5f,  1.0f,  3.0f,  1.0f },
    { 22,  0.0f,  0.0f,  0.5f,  0.5f,  0.0f,  2.0f,  1.0f }, { 23,  0.0f,  0.0f,  0.5f,  0.5f,  0.0f,  4.0f,  1.0f }
};
/* MTBL_WRK en17_mtn_tbl[10]: no, fmtn[4]{type,s,e}. Die se-Spalte ist unbenutzt (en17.c:671). */
typedef struct { int32_t no; int32_t type[4], s_frm[4], e_frm[4]; } EN17_MTBL;
static const EN17_MTBL en17_mtn_tbl[10] = {
    {  3, {  1, 0, 1, 0 }, {  0, 12,  25,  36 }, {  11,  24,  35,  49 } },
    { 15, {  1, 0, 1, 0 }, {  0, 48, 100, 144 }, {  44,  96, 140, 195 } },
    {  6, {  0,-1,-1,-1 }, {  0,  0,   0,   0 }, {  45,   0,   0,   0 } },
    {  1, {  0, 1,-1,-1 }, {  0, 30,   0,   0 }, {  29,  59,   0,   0 } },
    {  2, {  0,-1,-1,-1 }, {  0,  0,   0,   0 }, {  59,   0,   0,   0 } },
    {  5, {  0,-1,-1,-1 }, {  0,  0,   0,   0 }, {  79,   0,   0,   0 } },
    { 16, {  2,-1,-1,-1 }, {  0,  0,   0,   0 }, { 319,   0,   0,   0 } },
    { 11, {  0,-1,-1,-1 }, {  0,  0,   0,   0 }, {  44,   0,   0,   0 } },
    { 12, {  1, 0,-1, 0 }, {  0, 22,   0,  -1 }, {  21,  70,  -1,   0 } },
    { -1, { -1,-1,-1,-1 }, {  0,  0,   0,   0 }, {   0,   0,  -1,   0 } }
};
/* MTBL_WORK en17_mtn_tbl2[7]: no, atb[6]{frm, act}. */
typedef struct { int32_t no; int32_t frm[6]; uint32_t act[6]; } EN17_MTBL2;
static const EN17_MTBL2 en17_mtn_tbl2[7] = {
    { 15, {   0,  48,  72, 100, 140, 144 }, { 74496, 74496, 74500, 74496, 74500, 74496 } },
    {  1, {   0,  30,  -1,  -1,  -1,  -1 }, { 74496, 74496,     0,     0,     0,     0 } },
    {  2, {  32,  35,  -1,  -1,  -1,  -1 }, { 74499, 16786185,   0,     0,     0,     0 } },
    { 16, { 132, 160,  -1,  -1,  -1,  -1 }, { 74499, 16786185,   0,     0,     0,     0 } },
    { 11, {  19,  22,  -1,  -1,  -1,  -1 }, { 74503, 74499,     0,     0,     0,     0 } },
    { 17, {  25,  -1,  -1,  -1,  -1,  -1 }, { 74499,     0,     0,     0,     0,     0 } },
    { -1, {  -1,  -1,  -1,  -1,  -1,  -1 }, {     0,     0,     0,     0,     0,     0 } }
};
/* POS_WORK ply_mtn42..45_pos (Spielerwurzelversatz je Frame). */
typedef struct { float px, pz; } EN17_POS;
static const EN17_POS ply_mtn42_pos[20] = {
    { 0.0f,  0.0f }, { 0.0f, -2.12480998f }, { 0.0f, -2.18794179f }, { 0.0f, -2.072721f }, { 0.0f, -1.78017569f },
    { 0.0f, -1.31060696f }, { 0.0f, -1.02873039f }, { 0.0f, -1.01653862f }, { 0.0f, -1.00815964f }, { 0.0f, -1.0035944f },
    { 0.0f, -1.00283909f }, { 0.0f, -1.00589752f }, { 0.0f, -1.0127697f }, { 0.0f, -0.807430267f }, { 0.0f, -0.420703888f },
    { 0.0f, -0.267501831f }, { 0.0f, -0.218467712f }, { 0.0f, -0.169967651f }, { 0.0f, -0.121238708f }, { 0.0f, -0.0722846985f }
};
static const EN17_POS ply_mtn43_pos[20] = {
    { 0.0f, 5.57899809f }, { 0.0f, 1.89403009f }, { 0.0f, 1.89615822f }, { 0.0f, 1.89056587f }, { 0.0f, 1.87725449f },
    { 0.0f, 1.85622406f }, { 0.0f, 1.82747269f }, { 0.0f, 1.79100037f }, { 0.0f, 1.45895195f }, { 0.0f, 0.900272369f },
    { 0.0f, 0.492113113f }, { 0.0f, 0.234746933f }, { 0.0f, 0.128166199f }, { 0.0f, 0.172372818f }, { 0.0f, 0.339530945f },
    { 0.0f, 0.408201218f }, { 0.0f, 0.416120529f }, { 0.0f, 0.38740921f }, { 0.0f, 0.322071075f }, { 0.0f, 0.220104218f }
};
static const EN17_POS ply_mtn44_pos[51] = {
    { 0.0f, -2.6063652f }, { -0.00109499996f, -0.0548400879f }, { -0.00294700009f, -0.0579090118f }, { -0.0042940001f, -0.0601291656f },
    { -0.00513700023f, -0.0614967346f }, { -0.00547399931f, -0.0620174408f }, { -0.00530499965f, -0.0616855621f }, { -0.0046310015f, -0.0605049133f },
    { -0.00345199741f, -0.058473587f }, { -0.00176900253f, -0.0555915833f }, { 0.000421002507f, -0.0518627167f }, { 0.00311599858f, -0.0472793579f },
    { 0.00631500036f, -0.0418510437f }, { 0.0100209992f, -0.0355682373f }, { 0.0142310001f, -0.0284366608f }, { 0.0187749993f, -0.0231723785f },
    { 0.0229530018f, -0.0203113556f }, { 0.0265810005f, -0.0173988342f }, { 0.0296639949f, -0.0144405365f }, { 0.0321990028f, -0.0114307404f },
    { 0.0341860056f, -0.0083732605f }, { 0.0356269926f, -0.00526809692f }, { 0.0365200043f, -0.00211334229f }, { 0.0368660092f, 0.00109100342f },
    { 0.0366629958f, 0.00434303284f }, { 0.0359149873f, 0.00764274597f }, { 0.0346190035f, 0.0109920502f }, { 0.0327759981f, 0.0143909454f },
    { 0.0303840041f, 0.0178394318f }, { 0.0274469852f, 0.0213336945f }, { 0.0239610076f, 0.0248775482f }, { 0.0188489854f, 0.0315227509f },
    { 0.0127390027f, 0.0407295227f }, { 0.00702399015f, 0.0491752625f }, { 0.00170201063f, 0.0568599701f }, { -0.00322598219f, 0.0637836456f },
    { -0.00775802135f, 0.0699443817f }, { -0.0118969679f, 0.0753479004f }, { -0.0156410038f, 0.0799865723f }, { -0.0189909935f, 0.0838661194f },
    { -0.021946013f, 0.0869846344f }, { -0.0245069861f, 0.0893440247f }, { -0.0266750157f, 0.0909385681f }, { -0.0303269923f, 0.106515884f },
    { -0.0353450179f, 0.130146027f }, { -0.0397799909f, 0.144119263f }, { -0.0436370075f, 0.148435593f }, { -0.0469129831f, 0.143100739f },
    { -0.0496090055f, 0.128105164f }, { -0.0517240018f, 0.103460312f }, { -0.0532590002f, 0.0691566467f }
};
static const EN17_POS ply_mtn45_pos[51] = {
    { 0.0f, 0.0815086365f }, { 0.000188999998f, 0.0247268677f }, { 0.000134999995f, 0.0163879395f }, { -0.00057199999f, 0.00716018677f },
    { -0.00242500007f, -0.00296020508f }, { -0.00393699994f, -0.013967514f }, { -0.0065960004f, -0.0258655548f }, { -0.00990600046f, -0.0386543274f },
    { -0.0138679985f, -0.0523319244f }, { -0.0184790008f, -0.0669002533f }, { -0.0237429962f, -0.0823554993f }, { -0.0296570063f, -0.0987052917f },
    { -0.0419429988f, -0.115940094f }, { -0.0572379977f, -0.134067535f }, { -0.0681459904f, -0.156576157f }, { -0.07466501f, -0.18034935f },
    { -0.0767939985f, -0.200340271f }, { -0.0745350122f, -0.216550827f }, { -0.0678870082f, -0.228973389f }, { -0.0568509698f, -0.237615585f },
    { -0.041424036f, -0.24247551f }, { -0.0216109753f, -0.243551254f }, { -0.00201100111f, -0.240844727f }, { 0.0130140185f, -0.234354019f },
    { 0.0258889794f, -0.224081039f }, { 0.0366160274f, -0.21002388f }, { 0.0451929569f, -0.200519562f }, { 0.0516210198f, -0.19443512f },
    { 0.0558989942f, -0.18286705f }, { 0.0580269992f, -0.165821075f }, { 0.0580070019f, -0.143297195f }, { 0.0557470024f, -0.115289688f },
    { 0.0508189946f, -0.0818042755f }, { 0.0454149991f, -0.0428390503f }, { 0.04004999995f, -0.0143680573f }, { 0.0347240046f, -0.000867843628f },
    { 0.0294359997f, 0.0113964081f }, { 0.0241890028f, 0.0224246979f }, { 0.0189800002f, 0.0322189331f }, { 0.0138109997f, 0.0407733917f },
    { 0.00868099928f, 0.0480957031f }, { 0.00565600023f, 0.0541801453f }, { 0.00475900061f, 0.0590305328f }, { 0.00393799972f, 0.0626430511f },
    { 0.00319300033f, 0.0650215149f }, { 0.00252299989f, 0.0661621094f }, { 0.00192700000f, 0.0660686493f }, { 0.00140899990f, 0.0647392273f },
    { 0.000964000006f, 0.0621738434f }, { 0.000594999990f, 0.0583705902f }, { 0.000302000000f, 0.0533351898f }
};
/* en17.c:338/339: flipTree und die beiden Armketten (255-terminiert). */
static const int8_t en17_flip_tree[25] = { 0, 1, 2, 3, 4, 5, 10, 11, 12, 13, 6, 7, 8, 9, 19, 20, 21, 22, 23, 14, 15, 16, 17, 18, -1 };
static const int8_t en17_tree[2][8] = { { 0, 1, 19, 20, 21, 22, 23, -1 }, { 0, 1, 14, 15, 16, 17, 18, -1 } };

/* --- Hilfen ---------------------------------------------------------------- */
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en17)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (w->sv && w->sv->player_set) w->sv->player_set(p);
    else gap(w, "plp schreiben (en17)");
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    BASEI(w, joint_matrix, "mlwP->owP[n].mtx (en17)");
    if (!w->sv->joint_matrix(w, j, (float*)m)) { gap(w, "mlwP->owP[n].mtx ausserhalb des Modells (en17)"); return 0; }
    return 1;
}
static int pjoint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    BASEI(w, player_joint_matrix, "plp->mlwP->owP[n].mtx (en17)");
    if (!w->sv->player_joint_matrix(j, (float*)m)) { gap(w, "plp->mlwP->owP[n].mtx fehlt (en17)"); return 0; }
    return 1;
}
static RECVX_ENEMY_VEC vec_of(const NJS_POINT3* p) { RECVX_ENEMY_VEC v = { p->x, p->y, p->z }; return v; }
/* (-rand() / -2.1474836E9f): rand()/2^31 als float, gemeinsamer EE-Strom. */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en17)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static void se_play(RECVX_ENEMY_WORK* w, int no)
{
    RECVX_ENEMY_VEC pos;
    if (w->flg & 0x10000u) return;                                   /* en17.c:1666 */
    BASE(w, enemy_se, "RequestEnemySe (en17.c:1667)");
    BASE(w, enemy_now, "sys->enow (en17.c:1667)");
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    w->sv->enemy_se(w->sv->enemy_now(), &pos, no);
}

/* --- Rauch- und Partikelanforderungen (effsub4/en17.c:1724-1849) -------------- */
/* sys->ef-Felder, die en17 vor bhSetEffectTb schreibt; die Anforderung geht ueber
   den vorhandenen effect_tb-Dienst. */
static int smoke_effect(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int type, float scale,
                        int ay, float xn, int ct3)
{
    RECVX_T3_EFF ef;
    RECVX_T3_EFFPATCH patch;
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    int slot;
    memset(&ef, 0, sizeof ef);
    memset(&patch, 0, sizeof patch);
    if (!ext || !ext->effect_tb || !ext->effect_patch || !ext->model_texp) { gap(w, "bhEff369 Rauch (en17.c:1732)"); return -1; }
    ef.flg = 1; ef.id = 369; ef.type = type; ef.mdlver = 0;
    ef.sx = ef.sy = ef.sz = scale;
    ef.ax = 0; ef.ay = ay;
    ef.px = pos->x; ef.py = pos->y; ef.pz = pos->z;
    {
        RECVX_T3_LINK none = { NULL, 0 };
        slot = ext->effect_tb(&ef, NULL, none, 0);
    }
    if (slot == -1) return -1;
    patch.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP | RECVX_T3_EFF_TEX_ID | RECVX_T3_EFF_VECTOR | RECVX_T3_EFF_CT3;
    patch.stflg_or = 0x20;
    patch.texp = ext->model_texp(w);
    patch.tex_id = 7;
    patch.xn = xn; patch.yn = 0.05f; patch.zn = 0.9f;
    patch.ct3 = ct3;
    ext->effect_patch(slot, &patch);
    return slot;
}
/* en17.c:1724 bhEne17_SetSmokeEffect: drei Reihen zu je vier Wolken hinter dem Gelenk. */
static void set_smoke(RECVX_ENEMY_WORK* w, int lnk_obj, const NJS_POINT3* ofs)
{
    NJS_MATRIX m;
    NJS_POINT3 ps = *ofs, p;
    RECVX_ENEMY_VEC v;
    float r;
    int i, j;
    if (!joint(w, (uint32_t)lnk_obj, &m)) return;
    for (j = 0; j < 3; j++) {
        ps.z -= 2.0f;
        njCalcPoint(&m, &ps, &p);
        p.y = 1.0f;
        for (i = 0; i < 4; i++) {
            float dx, dz, xn;
            if (!rnd(w, &r)) return; dx = 2.0f * r - 1.0f; p.x += dx;
            if (!rnd(w, &r)) return; dz = 2.0f * r - 1.0f; p.z += dz;
            if (!rnd(w, &r)) return; xn = 0.4f + 0.2f * r;
            v = vec_of(&p);
            smoke_effect(w, &v, 0, 2.0f, w->ay, xn, 0);
        }
    }
}
/* en17.c:1771 bhEne17_SetSmokeEffect2: vier Wolken in Drehrichtung rot, ct3 0/2/4/6. */
static void set_smoke2(RECVX_ENEMY_WORK* w, const NJS_POINT3* ofs, int rot)
{
    float r;
    int i, j = 0;
    for (i = 0; i < 4; i++) {
        RECVX_ENEMY_VEC v;
        float xn;
        v.y = ofs->y;
        if (!rnd(w, &r)) return; v.x = (ofs->x + 2.0f * r) - 1.0f;
        if (!rnd(w, &r)) return; v.z = (ofs->z + 2.0f * r) - 1.0f;
        if (!rnd(w, &r)) return; xn = 1.2f + 0.2f * r;
        smoke_effect(w, &v, 1, 2.0f, rot, xn, j);
        j += 2;
    }
}
/* en17.c:1817 bhEne17_SetSmokeEffect3: eine Wolke. */
static void set_smoke3(RECVX_ENEMY_WORK* w, const NJS_POINT3* ofs, int rot)
{
    RECVX_ENEMY_VEC v;
    float r, xn;
    v.y = ofs->y;
    if (!rnd(w, &r)) return; v.x = ofs->x + (2.0f * r - 1.0f);
    if (!rnd(w, &r)) return; v.z = ofs->z + (2.0f * r - 1.0f);
    if (!rnd(w, &r)) return; xn = 0.1f + 0.2f * r;
    smoke_effect(w, &v, 0, 2.0f, rot, xn, 0);
}
static void eff_ptcl(RECVX_ENEMY_WORK* w, const NJS_POINT3* pos, int kind)
{
    RECVX_ENEMY_VEC v = vec_of(pos);
    SV(w, eff_ptcl, "bhEff_SetPtcl (en17.c:627)");
    en17_sv->eff_ptcl(w, &v, kind);
}
static void eff_ptcl2(RECVX_ENEMY_WORK* w, const NJS_POINT3* pos, const NJS_MATRIX* m)
{
    RECVX_ENEMY_VEC v = vec_of(pos);
    SV(w, eff_ptcl2, "bhEff_SetPtcl2 (en17.c:392)");
    en17_sv->eff_ptcl2(w, &v, (const float*)m);
}

/* --- Vorwaertsdeklarationen -------------------------------------------------- */
static void en17_init(RECVX_ENEMY_WORK* w);
static void en17_move(RECVX_ENEMY_WORK* w);
static void en17_nage(RECVX_ENEMY_WORK* w);
static void en17_damage(RECVX_ENEMY_WORK* w);
static void en17_die(RECVX_ENEMY_WORK* w);
static void en17_event(RECVX_ENEMY_WORK* w);
static void brain(RECVX_ENEMY_WORK* w);
static void brain00(RECVX_ENEMY_WORK* w);
static void dmmy_brain(RECVX_ENEMY_WORK* w);
static void mv00(RECVX_ENEMY_WORK* w);
static void mv01(RECVX_ENEMY_WORK* w);
static void mv02(RECVX_ENEMY_WORK* w);
static void mv03(RECVX_ENEMY_WORK* w);
static void mv04(RECVX_ENEMY_WORK* w);
static void mv05(RECVX_ENEMY_WORK* w);
static void mv06(RECVX_ENEMY_WORK* w);
static void dg00(RECVX_ENEMY_WORK* w);
static int  player_dg_check(RECVX_ENEMY_WORK* w);
static void ply_dg00(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w);
static void ply_dg01(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w);

typedef void (*en17_fn)(RECVX_ENEMY_WORK*);
/* en17.c:298 bhEne17_Mode0[6]; :309 BrainMode2[7]; :319 MoveMode2[7]. */
static const en17_fn en17_mode0[6] = { en17_init, en17_move, en17_nage, en17_damage, en17_die, en17_event };
static const en17_fn en17_brain_mode2[7] = { brain00, dmmy_brain, dmmy_brain, dmmy_brain, dmmy_brain, dmmy_brain, brain00 };
static const en17_fn en17_move_mode2[7] = { mv00, mv01, mv02, mv03, mv04, mv05, mv06 };

/* --- ikou-Familie (Dienst en03_lib, zonzon.c:10/45) ---------------------------- */
static void ikou(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add) { recvx_t3_ikou(w, pos, add); }
static int ikou3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int add) { return recvx_t3_ikou3(w, pos, add); }

/* ===========================================================================
   en17.c:347 bhEne17
   =========================================================================== */
void recvx_en17(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    RECVX_EN17_EXP0* e;
    int i;

    if (!w) return;
    if (!w->exp0 && w->mode0 != 0) { gap(w, "bhEne17: exp0 NULL bei mode0 != 0 (en17.c:377)"); return; }

    /* bhEne17_MainLoop (:417): DmgChk, Mode0[mode0], SetMtn. */
    if (w->mode0 >= 6u || !en17_mode0[w->mode0]) { gap(w, "bhEne17_Mode0[mode0] ausserhalb (en17.c:298)"); return; }

    /* :425 bhEne17_DmgChk */
    if ((w->flg & 4u) && !(w->flg & 2u)) {
        if (!w->exp0) { gap(w, "bhEne17_DmgChk: exp0 NULL (en17.c:433)"); return; }
        e = x0(w);
        recvx_t3_calc_damage(w, en17_comb_wep, en17_comb_joint);
        if (w->total_dam != 0) {
            /* :487 bhEne17_DamageAdd */
            const int32_t* wp;
            if (w->comb_wep >= 22u) { gap(w, "En17_WpnDamageTbl[comb_wep] ausserhalb (en17.c:495)"); return; }
            wp = En17_WpnDamageTbl[w->comb_wep];
            if (w->hp >= 0) {
                int n = 0;
                se_play(w, 0x01002302);
                BASE(w, obj_num, "mlwP->obj_num (en17.c:498)");
                n = (int)w->sv->obj_num(w);
                for (i = 1; i < n; i++) {
                    if (w->dam[i] > 0) {
                        w->djnt_no = i;
                        if (!(wp[0] & 4)) {
                            int bt = ((w->comb_flg & 1u) || w->hp < 0) ? wp[4] : wp[2];
                            if (bt < 0 || bt >= 24) { gap(w, "en17prt_blood_tbl[] ausserhalb (en17.c:503)"); return; }
                            recvx_t3_set_blood(w, bt, en17prt_blood_tbl);
                        }
                    }
                }
            }
            if ((wp[0] & 1) || (wp[0] & 2)) {                         /* :512 */
                if (e->effect_timer <= 0) {
                    e->effect_timer = 10;
                    recvx_t3_set_dfire_effect(w, w->djnt_no, en17prt_blood_tbl, (wp[0] & 2) ? 2 : 1);
                }
            }
            if (wp[0] & 8) {                                          /* :523 */
                if (e->effect_timer <= 0) {
                    e->effect_timer = 10;
                    recvx_t3_set_san_effect(w, w->djnt_no, en17prt_blood_tbl);
                }
            }
            if (e->damage_toggle != 0) { e->flags |= 0x200u; e->damage_toggle = 0; }
            else e->damage_toggle = 1;
            if (w->mode0 == 1) {
                if (w->comb_flg & 4u) e->flags |= 0x80u; else e->flags &= ~0x80u;
                {                                                     /* :454 bhEne17_ChgDmgMode */
                    int act;
                    if (w->wpnr_no < 0 || w->wpnr_no >= 22) { gap(w, "En17_WpnDamageTbl[wpnr_no] ausserhalb (en17.c:459)"); return; }
                    wp = En17_WpnDamageTbl[w->wpnr_no];
                    act = wp[1];
                    if (w->hp < 0) { w->comb_flg |= 1u; w->comb_timeout = 0; w->comb_pnt = 0; }
                    if (w->comb_flg & 1u) act = wp[3];
                    if (!((unsigned)act >= 4u) && !((w->wpnr_no == 17 || w->wpnr_no == 16) && !(w->flg2 & 4u))) {
                        w->mode0 = 3; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
                    }
                }
            }
        }
    }

    en17_mode0[w->mode0](w);
    {                                                                 /* :421 bhEne17_SetMtn */
        RECVX_EN17_EXP0* e0;
        int frm, lnk_obj = -1, ret;
        int32_t sfrm_no;
        if (!w->exp0) return;
        e0 = x0(w);
        if (e0->flags & 0x20u) goto after_set_mtn;
        sfrm_no = w->frm_no;
        frm = w->frm_no / 65536;
        if (!w->mtn_room) {                                           /* :557 sys->rmthp != epw->mnwP */
            if (w->type == 0 && (w->mtn_no == 16 || w->mtn_no == 15) && frm >= 8) {
                /* :563 sieben Nachbilder, danach Rueckstellung auf sfrm_no - 4. */
                static const float ofs_z = -16.15572f;
                NJS_MATRIX am;
                for (i = 0; i < 7; i++) {
                    uint32_t argb = ((uint32_t)i << 28) + 0x20FFFFFFu;
                    RECVX_ENEMY_VEC ofs = { 0.0f, 0.0f, ofs_z };
                    w->frm_no = sfrm_no - (7 - i) * 65536;
                    BASE(w, set_motion, "bhSetMotion (en17.c:566)");
                    (void)w->sv->set_motion(w, 0, w->mtn_md);
                    BASE(w, calc_model, "bhCalcModel (en17.c:567)");
                    w->sv->calc_model(w);
                    if (!w->exp2) { gap(w, "bhEne17: exp2 (linker Arm) NULL (en17.c:569)"); w->frm_no = sfrm_no; goto after_set_mtn; }
                    w->sv->calc_model((RECVX_ENEMY_WORK*)w->exp2);
                    if (!joint((RECVX_ENEMY_WORK*)w->exp2, 1, &am)) { w->frm_no = sfrm_no; goto after_set_mtn; }
                    if (en17_sv && en17_sv->afterimage) en17_sv->afterimage(w, (const float*)am, &ofs, argb);
                    else gap(w, "bhEne17_AfterimageAxEffect (en17.c:578)");
                }
                w->frm_no = sfrm_no - 4 * 65536;                      /* :581 sfrm_no + 0xFFFC0000 */
                BASE(w, set_motion, "bhSetMotion (en17.c:582)");
                (void)w->sv->set_motion(w, 0, w->mtn_md);
                w->sv->calc_model(w);
                w->frm_no = sfrm_no;
            }
            if (w->mtn_no == 16) { if (frm == 168) { e0->flags |= 0x800u; e0->camera_shake.bits = 0x40000000u; } }
            else if (w->mtn_no == 11) { if (frm == 25) { e0->flags |= 0x800u; e0->camera_shake.bits = 0x40000000u; } }
            else if (w->mtn_no == 5) { if (frm == 42) { e0->flags |= 0x800u; e0->camera_shake.bits = 0x40000000u; } }
            else if (w->mtn_no == 2) { if (frm == 35) { e0->flags |= 0x800u; e0->camera_shake.bits = 0x40000000u; } }
            else if (w->mtn_no == 15) { if (frm == 0 || frm == 48 || frm == 100 || frm == 144) { e0->flags |= 0x800u; e0->camera_shake.amplitude = 1.0f; } }
            if (w->mtn_no == 15) {
                if (frm == 0 || frm == 100) lnk_obj = 17;
                else if (frm == 48 || frm == 144) lnk_obj = 22;
                if (frm == 0 || frm == 25) {
                    for (i = 0; i < 4; i++) {
                        NJS_POINT3 ps;
                        float r;
                        if (!rnd(w, &r)) goto after_set_mtn; ps.x = w->px + ((80.0f * r) - 40.0f);
                        ps.y = 45.0f;
                        if (!rnd(w, &r)) goto after_set_mtn; ps.z = w->pz + ((40.0f * r) - 20.0f);
                        eff_ptcl(w, &ps, 8);
                    }
                }
            } else if (w->mtn_no == 1) {
                if (frm == 0) lnk_obj = 23; else if (frm == 30) lnk_obj = 18;
            }
            if (lnk_obj != -1) {                                      /* :639 */
                NJS_MATRIX m;
                NJS_POINT3 ps;
                if (!joint(w, (uint32_t)lnk_obj, &m)) goto after_set_mtn;
                ps.x = m[12]; ps.y = 1.0f; ps.z = m[14];
                set_smoke3(w, &ps, w->ay + 32768);
            }
        }
        BASE(w, set_motion, "bhSetMotion (en17.c:651)");
        ret = w->sv->set_motion(w, w->mtn_add, w->mtn_md);
        if (e0->flags & 0x40000000u) {                                /* :653 */
            recvx_t3_get_translate_mtn(w, frm, 0);
            if (en17_sv && en17_sv->model_root_zero_xz) en17_sv->model_root_zero_xz(w);
            else gap(w, "mlwP->objP->pos[0/2] = 0 (en17.c:655)");
        }
        if (!w->mtn_room) {                                           /* :671 bhEne17_MtnTblPlay */
            if (w->flg & 0x40000u) {
                const EN17_MTBL* t;
                for (t = en17_mtn_tbl; t->no != -1; ++t) {
                    if (t->no == (int32_t)w->mtn_no) {
                        for (i = 0; i < 4; i++) {
                            if (t->type[i] != -1 && frm >= t->s_frm[i] && frm <= t->e_frm[i]) {
                                static const RECVX_ENEMY_VEC vec1 = { 0.0f, 0.0f, 0.0f };
                                static const RECVX_ENEMY_VEC vec2 = { 0.0f, -0.8f, -2.5f };
                                RECVX_ENEMY_VEC out = { 0, 0, 0 };
                                const RECVX_ENEMY_VEC* local = (t->type[i] <= 1) ? &vec2 : &vec1;
                                const int8_t* chain = en17_tree[t->type[i] & 1];
                                SV(w, calc_fix_offset, "bhCalcFixOffset (en17.c:691)");
                                en17_sv->calc_fix_offset(w, chain, local, &out);
                                if (w->mtn_no == 15) { w->px -= 1.2f * out.x; w->pz -= 1.2f * out.z; }
                                else { w->px -= out.x; w->pz -= out.z; }
                                break;
                            }
                        }
                    }
                }
            }
            {
                const EN17_MTBL2* t2;
                for (t2 = en17_mtn_tbl2; t2->no != -1; ++t2) {
                    if (t2->no == (int32_t)w->mtn_no) {
                        for (i = 0; i < 6; i++) {
                            if (t2->frm[i] == -1) break;
                            if (t2->frm[i] == frm) se_play(w, (int)t2->act[i]);
                        }
                    }
                }
            }
        }
        if (ret != 0) w->flg |= 0x2000000u; else w->flg &= ~0x2000000u;
    }
after_set_mtn:
    if (!w->exp0) return;

    /* Spielerreaktion beim Griff (en17.c:355-361). */
    if (player(w, &p)) {
        if (p.mode0 == 4 || p.mode0 == 6) {
            if (p.mode2 == 0) ply_dg00(&p, w); else ply_dg01(&p, w);
            put_player(w, &p);
        }
    }
    e = x0(w);
    {                                                                 /* :404 EneToPlyDist */
        NJS_MATRIX pm;
        if (pjoint(w, 0, &pm)) {
            RECVX_ENEMY_VEC pos = { pm[12], w->py, pm[14] };
            const float dx = pos.x - w->px, dy = pos.y - w->py, dz = pos.z - w->pz;
            e->player_distance = njSqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    if (w->flg & 4u) {                                                /* :364 */
        for (i = 0; i < 64; i++) w->dam[i] = 0;
        w->flg &= ~4u;
    }
    /* :740 bhEne17_CollCheck */
    if (!(w->flg & 2u)) {
        if ((w->flg & 8u) && (e->flags & 0x10u)) { BASE(w, check_player, "bhCheckPlayer (en17.c:745)"); w->sv->check_player(w); }
        BASE(w, wall_type, "bhCheckWallType (en17.c:754)");
        {
            RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz };
            e->wall_atr = (const RECVX_ENEMY_ATR*)(intptr_t)w->sv->wall_type(&pos, w->flg, w->ar, w->ah);
        }
        if ((e->flags & 0xFu) == 1u && (w->flg & 0x10u)) {
            if (w->sv->check_dansa) w->sv->check_dansa(w); else gap(w, "bhCheckDansa (en17.c:757)");
            BASE(w, check_wall, "bhCheckWall (en17.c:758)");
            w->sv->check_wall(w);
        }
    }
    /* :763 bhEne17_CalcEnemy */
    {
        NJS_MATRIX m, m16, m21;
        BASE(w, calc_model, "bhCalcModel (en17.c:767)");
        w->sv->calc_model(w);
        if (joint(w, 5, &m) && joint(w, 16, &m16) && joint(w, 21, &m21)) {
            w->cah = w->ah = m[13] - w->py;
            w->watr.c1.x = m[12]; w->watr.c1.y = m[13] - 5.0f; w->watr.c1.z = m[14];
            w->watr.c2.x = (m16[12] + m21[12]) / 2.0f;
            w->watr.c2.y = (m16[13] + m21[13]) / 2.0f;
            w->watr.c2.z = (m16[14] + m21[14]) / 2.0f;
            w->watr.r = 4.0f;
        }
    }
    if (w->mode0 < 5) {                                               /* :373 bhEne17_CameraControl */
        if (e->flags & 0x800u) {
            if (!(e->camera_shake.amplitude <= 0.01f)) {
                float r;
                if (rnd(w, &r)) {
                    if (en17_sv && en17_sv->camera_ofy) en17_sv->camera_ofy(e->camera_shake.amplitude * r - e->camera_shake.amplitude / 2.0f);
                    else gap(w, "cam.ofy (en17.c:1676)");
                    e->camera_shake.amplitude *= 0.8f;
                }
            } else {
                if (en17_sv && en17_sv->camera_clear) en17_sv->camera_clear(); else gap(w, "cam.ofx/ofy/ofz (en17.c:1681)");
                e->flags &= ~0x800u;
            }
        }
    }
    if (e->timer_0c > 0) e->timer_0c--;
    if (e->effect_timer > 0) e->effect_timer--;
    if (e->flags & 0x20000000u) {                                     /* :385 Klingentreffer an den Spieler */
        RECVX_ENEMY_VEC tip;
        if (recvx_en17_blade_tip(w, &tip)) {
            NJS_MATRIX am;
            NJS_POINT3 pd = { tip.x, tip.y, tip.z };
            if (w->exp2 && joint((RECVX_ENEMY_WORK*)w->exp2, 1, &am)) {
                for (i = 0; i < 4; i++) eff_ptcl2(w, &pd, &am);
                e->flags &= ~0x20000000u;
                if (en17_sv && en17_sv->set_light) {                  /* :398 bhEne17_SetLight */
                    RECVX_EN17_LIGHT l;
                    memset(&l, 0, sizeof l);
                    l.flg = 3; l.aspd = 8; l.lsrc = 4; l.type = 101;
                    l.r = l.g = l.b = 6.0f; l.nr = 10.0f; l.fr = 80.0f;
                    l.px = pd.x; l.py = pd.y; l.pz = pd.z;
                    en17_sv->set_light(2, &l);
                } else gap(w, "rom->lgtp[2] (en17.c:1854)");
                se_play(w, 0x01002309);
            }
        } else gap(w, "((O_WRK*)epw->exp2)->mlwP->owP[1] (en17.c:386)");
    }
}

/* en17.c:1664 SePlay ist oben; en17.c:21 Arm-Stuetzfunktionen: */
int recvx_en17_blade_tip(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m;
    NJS_POINT3 ps = { 0.0f, 0.0f, -17.5f }, pd;
    RECVX_ENEMY_WORK* arm;
    if (!w || !w->exp2) return 0;
    arm = (RECVX_ENEMY_WORK*)w->exp2;
    if (!arm->sv || !arm->sv->joint_matrix || !arm->sv->joint_matrix(arm, 1u, (float*)m)) return 0;
    njCalcPoint(&m, &ps, &pd);
    out->x = pd.x; out->y = pd.y; out->z = pd.z;
    return 1;
}
void recvx_en17_blade_hit(RECVX_ENEMY_WORK* w)
{
    if (w && w->exp0) x0(w)->flags |= 0x20000000u;
}

/* ===========================================================================
   en17.c:789 bhEne17_Init
   =========================================================================== */
static void en17_init(RECVX_ENEMY_WORK* w)
{
    int i;
    RECVX_EN17_EXP0* e;

    w->ar = 8.0f; w->ah = 25.0f; w->aw = 0; w->ad = 0;               /* :795 */
    w->car = 10.0f; w->cah = 25.0f;
    w->hp = 250;
    w->stflg = 0;
    for (i = 0; i < 64; i++) w->dam[i] = 0;
    w->hokan_rate = 0; w->hokan_count = 0;
    w->mtn_no = 0; w->mtn_add = 0; w->frm_no = 0;
    w->mtn_tp = en17_flip_tree;
    w->mtn_md = 0;

    if (w->exp0 == NULL) {                                           /* :819 */
        BASE(w, calloc_work, "bhEne_CallocWork (en17.c:821)");
        w->exp0 = w->sv->calloc_work(sizeof(RECVX_EN17_EXP0));
        w->exp1 = w->sv->calloc_work(sizeof(RECVX_EN17_EXP1));
        if (!w->exp0 || !w->exp1) { gap(w, "bhEne_CallocWork: Budget erschoepft (en17.c:821)"); return; }
        if (!(w->flg & 0x80u)) {                                     /* :824 zwei Kettenwerke (Arme) */
            SV(w, create_child, "bhEne17_SetLinkWork (en17.c:826)");
            w->exp2 = en17_sv->create_child(w, 0xD, 4, 0x1F);
            if (w->exp2) en17_sv->set_call_func(0x1Fu, recvx_en17_larm); else gap(w, "bhEne17_SetLinkWork: Gegnertabelle voll (en17.c:826)");
            w->exp3 = en17_sv->create_child(w, 9, 3, 0x20);
            if (w->exp3) en17_sv->set_call_func(0x20u, recvx_en17_rarm); else gap(w, "bhEne17_SetLinkWork: Gegnertabelle voll (en17.c:828)");
        }
    } else {
        memset(w->exp0, 0, 84);                                      /* :832 i = 84 Bytes */
    }
    e = x0(w);
    if (!e) return;
    e->flags |= 0x11u;                                               /* :840 */
    w->flg |= 0x78u; w->flg &= ~2u;

    if (!(w->flg & 0x800u)) {                                        /* :844 bhSetShadow(En17_SdwTab, epw, 1, 10, 5, 10) */
        BASE(w, set_shadow, "bhSetShadow (en17.c:846)");
        (void)w->sv->set_shadow(En17_SdwTab, w, 1, 10.0f, 5.0f, 10.0f);
        w->flg |= 0x800u;
    }
    w->clp_jno[0] = 5; w->clp_jno[1] = 1; w->clp_jno[2] = 12;        /* :850 */
    w->clp_jno[3] = 8; w->clp_jno[4] = 21; w->clp_jno[5] = 16; w->clp_jno[6] = -1;
    w->mode0 = 1; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
    w->lok_jno = 4;
    SV(w, select_morph_pair, "mlwP->objP = *mbp; obj_a/obj_b (en17.c:862)");
    en17_sv->select_morph_pair(w);
    w->mdflg = 0;                                                    /* :863 */
    w->cpcl = en17_cap_col;
    /* :868 InitType00 ist leer; :869 bhSetMotion und CalcEnemy. */
    BASE(w, set_motion, "bhSetMotion (en17.c:869)");
    (void)w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    BASE(w, calc_model, "bhCalcModel (en17.c:767)");
    w->sv->calc_model(w);
    {
        NJS_MATRIX m, m16, m21;
        if (joint(w, 5, &m) && joint(w, 16, &m16) && joint(w, 21, &m21)) {
            w->cah = w->ah = m[13] - w->py;
            w->watr.c1.x = m[12]; w->watr.c1.y = m[13] - 5.0f; w->watr.c1.z = m[14];
            w->watr.c2.x = (m16[12] + m21[12]) / 2.0f;
            w->watr.c2.y = (m16[13] + m21[13]) / 2.0f;
            w->watr.c2.z = (m16[14] + m21[14]) / 2.0f;
            w->watr.r = 4.0f;
        }
    }
}

/* en17.c:905-922 Move/Damage/Nage/Die/Event */
static void en17_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1) brain(w);                                     /* :978 MVType00 */
    if (w->mode2 >= 7u) { gap(w, "bhEne17_MoveMode2[mode2] ausserhalb (en17.c:319)"); return; }
    en17_move_mode2[w->mode2](w);
}
static void en17_damage(RECVX_ENEMY_WORK* w)
{
    if (w->mode2 != 0) { gap(w, "bhEne17_DamageMode2[mode2] ausserhalb (en17.c:330)"); return; }
    dg00(w);                                                         /* :1281 DGType00 -> DG00 */
}
static void en17_nage(RECVX_ENEMY_WORK* w) { (void)w; }
static void en17_die(RECVX_ENEMY_WORK* w) { (void)w; }
static void en17_event(RECVX_ENEMY_WORK* w)
{
    BASE(w, event, "bhEne_Event (en17.c:305)");
    w->sv->event(w);
}
static void dmmy_brain(RECVX_ENEMY_WORK* w) { (void)w; }

/* en17.c:927 bhEne17_Brain */
static void brain(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC pos, from;
    int r;
    e->brain_flags = 64;                                             /* :931 */
    if (!(e->brain_flags & 0x40)) return;
    if (!player(w, &p)) return;
    from.x = w->px; from.y = w->py; from.z = w->pz;
    SV(w, check_route, "bhCheckRoute (en17.c:933)");
    r = en17_sv->check_route(&from, &p.pos, &pos);
    if (r != 255) { e->route_target.x = pos.x; e->route_target.z = pos.z; }
    else { e->route_target.x = p.pos.x; e->route_target.z = p.pos.z; }
    if (w->mode2 >= 7u) { gap(w, "bhEne17_BrainMode2[mode2] ausserhalb (en17.c:309)"); return; }
    en17_brain_mode2[w->mode2](w);
}
/* en17.c:946 bhEne17_Brain00 */
static void brain00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    if (w->mode3 == 0) return;
    if (!player(w, &p)) return;
    if (!(p.flg & 2u) && !(p.flg & 4u)) {
        if (!(e->flags & 0x400u)) {
            if (e->player_distance < 24.0f && ikou3(w, &p.pos, 0x2000) == 0) {
                if (e->player_distance < 15.5f && (p.stflg & 0x400u)) { w->mode1 = 0; w->mode2 = 4; w->mode3 = 0; }
                else { w->mode1 = 0; w->mode2 = 3; w->mode3 = 0; }
            } else { w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; }
        }
    }
}

/* en17.c:986 bhEne17_MV00 */
static void mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    switch (w->mode3) {
    case 0:
        (void)recvx_t3_chg_mtn(w, 0, 0, 7);
        e->flags &= ~0x40000000u;
        w->mode1 = 1;
        w->ct0 = (e->player_distance < 24.0f) ? 10 : 1;
        e->flags |= 0x400u;
        w->ct0 += e->wait_count;
        w->ct1 = 0;
        e->wait_count = 0;
        w->mode3 += 1;
        /* fall through */
    case 1:
        if (--w->ct0 < 0) e->flags &= ~0x400u;
        if (!player(w, &p)) return;
        if (w->wpnr_no != 14 && w->wpnr_no != 15 && w->wpnr_no != 16 && w->wpnr_no != 17 &&
            (e->flags & 0x400u) && (p.stflg & 0x400u)) {
            if (++w->ct1 > 10) e->flags &= ~0x400u;
        }
        break;
    }
}
/* en17.c:1023 bhEne17_MV01 */
static void mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    switch (w->mode3) {
    case 0:
        (void)recvx_t3_chg_mtn(w, 1, 1966080, 5);
        w->flg |= 0x40000u;
        w->ct0 = 0;
        w->mode3++;
        /* fall through */
    case 1:
        ikou(w, &e->route_target, 512);                              /* :1033 &exp0[28] (x/z) */
        if (!player(w, &p)) return;
        if (!(p.flg & 2u) && !(p.flg & 4u) && e->player_distance < 22.0f && !ikou3(w, &p.pos, 8192)) {
            w->mode1 = 0; w->mode2 = 5; w->mode3 = 0;
        }
        break;
    }
}
/* en17.c:1047 bhEne17_MV02 */
static void mv02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    int frm;
    switch (w->mode3) {
    case 0:
        (void)recvx_t3_chg_mtn(w, 15, 12320768, 56);
        w->mtn_add = 262144;
        w->flg |= 0x40000u;
        e->damage_toggle = 0;
        w->ct0 = 0;
        w->mode3 += 1;
        /* fall through */
    case 1:
        if (!player(w, &p)) return;
        ikou(w, &p.pos, 728);
        frm = w->frm_no / 65536;
        if (frm >= 84 && frm < 177 && player_dg_check(w) != 0) w->ct0 = 1;
        if (w->ct0 == 1 && ((w->flg & 0x02000000u) || frm == 136)) { w->mode1 = 0; w->mode2 = 0; w->mode3 = 0; }
        break;
    }
}
/* en17.c:1078 bhEne17_MV03 */
static void mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    int frm, i;
    switch (w->mode3) {
    case 0:
        (void)recvx_t3_chg_mtn(w, 16, 0, 80);
        w->mtn_add = 262144;
        w->flg |= 0x40000u;
        w->flg2 |= 1u;
        e->damage_toggle = 0;
        w->mode3 += 1;
        /* fall through */
    case 1:
        if (!player(w, &p)) return;
        frm = w->frm_no / 65536;
        if (frm >= 72 && frm < 121) ikou(w, &p.pos, 728);
        if (frm == 156) {
            NJS_POINT3 ofs = { -4.5f, 0.0f, -15.0f };
            NJS_MATRIX m;
            set_smoke(w, 0, &ofs);
            if (!joint(w, 1, &m)) return;
            ofs.x = m[12]; ofs.z = m[14]; ofs.y = 1.0f;
            for (i = 0; i < 4; i++) set_smoke2(w, &ofs, w->ay + 20480 + i * 8192);
        }
        if (frm >= 156 && frm < 169) {
            for (i = 0; i < 2; i++) {
                NJS_POINT3 ofs;
                float r;
                if (!rnd(w, &r)) return; ofs.x = w->px + ((80.0f * r) - 40.0f);
                ofs.y = 45.0f;
                if (!rnd(w, &r)) return; ofs.z = w->pz + ((40.0f * r) - 20.0f);
                eff_ptcl(w, &ofs, 8);
            }
        }
        if (frm >= 144 && frm < 169) (void)player_dg_check(w);
        if (!(e->player_distance < 24.0f) && p.hp > 0) {
            if (frm < 148 || frm > 220) {
                w->mode1 = 1; w->mode2 = 2; w->mode3 = 0; w->flg2 &= ~1u;
                return;
            }
        }
        if ((w->frm_no / 65536) >= 312) { w->mode1 = 0; w->mode2 = 0; w->mode3 = 0; w->flg2 &= ~1u; }
        break;
    }
}
/* en17.c:1158 bhEne17_MV04 */
static void mv04(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    int frm;
    uint32_t n;
    switch (w->mode3) {
    case 0:
        (void)recvx_t3_chg_mtn(w, 11, 0, 10);
        w->flg |= 0x40000u;
        w->flg2 |= 1u;
        e->damage_toggle = 0;
        w->mode3++;
        /* fall through */
    case 1:
        frm = w->frm_no / 65536;
        if (frm >= 21 && frm < 29) (void)player_dg_check(w);
        BASE(w, motion_frames, "epw->mnwP[mtn_no].frm_num (en17.c:1177)");
        n = w->sv->motion_frames(w, w->mtn_no);
        if ((w->frm_no / 65536) == (int)n - 1) {
            w->mode1 = 1; w->mode2 = 6; w->mode3 = 0;
            e->wait_count = 0;
            w->flg2 &= ~1u;
        }
        break;
    }
}
/* en17.c:1188 bhEne17_MV05 */
static void mv05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    int frm, i;
    switch (w->mode3) {
    case 0:
        (void)recvx_t3_chg_mtn(w, 2, 0, 10);
        w->flg |= 0x40000u;
        e->damage_toggle = 0;
        w->mode3++;
        /* fall through */
    case 1:
        if (!player(w, &p)) return;
        frm = w->frm_no / 65536;
        if (frm >= 0 && frm < 23) ikou(w, &p.pos, 728);
        if (frm >= 31 && frm < 37) (void)player_dg_check(w);
        if (frm == 32) {
            NJS_POINT3 ofs = { 0.0f, 0.0f, -15.0f };
            NJS_MATRIX m;
            set_smoke(w, 0, &ofs);
            if (!joint(w, 1, &m)) return;
            ofs.x = m[12]; ofs.z = m[14]; ofs.y = 1.0f;
            for (i = 0; i < 4; i++) set_smoke2(w, &ofs, w->ay + 20480 + i * 8192);
        }
        if (frm >= 36 && frm <= 40) {
            for (i = 0; i < 2; i++) {
                NJS_POINT3 ofs;
                float r;
                if (!rnd(w, &r)) return; ofs.x = w->px + ((80.0f * r) - 40.0f);
                ofs.y = 45.0f;
                if (!rnd(w, &r)) return; ofs.z = w->pz + ((40.0f * r) - 20.0f);
                eff_ptcl(w, &ofs, 8);
            }
        }
        break;
    }
}
/* en17.c:1246 bhEne17_MV06 */
static void mv06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    switch (w->mode3) {
    case 0:
        (void)recvx_t3_chg_mtn(w, 17, 0, 7);
        e->flags &= ~0x40000000u;
        w->mode1 = 1;
        w->ct0 = (e->player_distance < 24.0f) ? 10 : 1;
        e->flags |= 0x400u;
        w->ct0 += e->wait_count;
        w->ct1 = 0;
        e->wait_count = 0;
        w->mode3++;
        /* fall through */
    case 1:
        if (--w->ct0 < 0) e->flags &= ~0x400u;
        if (!player(w, &p)) return;
        if ((e->flags & 0x400u) && (p.stflg & 0x400u)) {
            if (++w->ct1 > 10) e->flags &= ~0x400u;
        }
        break;
    }
}

/* en17.c:1287 bhEne17_DG00 */
static void dg00(RECVX_ENEMY_WORK* w)
{
    static const int wcnt_tbl[4] = { 0, 5, 20, 35 };
    RECVX_EN17_EXP0* e = x0(w);
    RECVX_ENEMY_PLAYER p;
    int frm, ang;
    switch (w->mode3) {
    case 0:
        if (w->mtn_no == 15) {
            frm = w->frm_no / 65536;
            if ((frm >= 0 && frm <= 11) || (frm >= 27 && frm <= 37)) (void)recvx_t3_chg_mtn(w, 6, 0, 6);
            else (void)recvx_t3_chg_mtn(w, 12, 0, 6);
        } else (void)recvx_t3_chg_mtn(w, 6, 0, 6);
        w->ayp = (int32_t)(10430.381f * atan2f(w->dvx, w->dvz));
        e->flags &= ~0x100u;
        w->flg |= 0x40000u;
        w->flg2 &= ~1u;
        w->mode3 += 1;
        /* fall through */
    case 1:
        frm = w->frm_no / 65536;
        if (frm < 10) {
            ang = (uint16_t)(w->ayp - w->ay);
            if (32768 < ang) ang -= 65536;
            w->ay += ang / 2;
        }
        if (w->wpnr_no != 14) {
            if (!player(w, &p)) return;
            if ((e->flags & 0x200u) && frm >= 20 && frm < 31 && !(p.flg & 2u) && !(p.flg & 4u) &&
                e->player_distance < 24.0f && ikou3(w, &p.pos, 0x2000) == 0) {
                w->mode0 = 1; w->mode1 = 0; w->mode2 = 3; w->mode3 = 0;
            }
        }
        if (frm != 31) return;
        w->mode0 = 1; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
        if (w->wpnr_no == 19) e->wait_count = wcnt_tbl[0];
        else if (w->wpnr_no >= 14 && w->wpnr_no < 18) e->wait_count = wcnt_tbl[3];
        else if ((unsigned)(w->wpnr_no - 5) < 2u) e->wait_count = wcnt_tbl[1];
        else e->wait_count = wcnt_tbl[2];
        break;
    }
}

/* en17.c:1565 bhEne17_PlayerDGCheck: fuenf Kapseln entlang der Klinge gegen plp->watr. */
static int player_dg_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN17_EXP1* x = x1(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_CAPSULE pcap;
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    int i;
    if (!player(w, &p)) return 0;
    if ((p.flg & 4u) || (p.flg & 2u) || (p.stflg & 0x01000000u) || (p.stflg & 0x80000000u)) return 0;
    if (!ext || !ext->player_watr || !ext->player_watr(&pcap)) { gap(w, "plp->watr (en17.c:1593)"); return 0; }
    if (!en17_sv || !en17_sv->capsule_capsule) { gap(w, "npCollisionCheckCC (en17.c:1593)"); return 0; }
    for (i = 0; i < 5; i++) {
        RECVX_ENEMY_CAPSULE cap;
        cap.c1 = x->tip_curve[i + 1];                                /* exp1 + 72 + 12*i: Ausgabepunkt 6+i = tip_curve[i] */
        cap.c1 = x->tip_curve[i];
        cap.c2 = x->root_curve[i];
        cap.r = 1.0f;
        if (en17_sv->capsule_capsule(&cap, &pcap) != 0) {
            RECVX_ENEMY_VEC v, pd, ps = { 0, 0, 0 };
            NJS_MATRIX m3;
            int j;
            se_play(w, 0x01002308);
            if (!w->sv->vibration) { gap(w, "bhEne_SetVibration (en17.c:1599)"); return 0; }
            w->sv->vibration(1, 11);
            if (w->mtn_no == 11) {
                p.mode0 = 4; p.mode1 = 0; p.mode2 = 1; p.mode3 = 0;
                p.flg |= 4u;
                p.hp -= 30;
                if (p.hp < 0) p.hp = 0;
                put_player(w, &p);
                return 1;
            }
            if (w->mtn_no == 2) p.hp = -1;
            else {
                if (p.hp < 120) p.hp = -1;
                else { p.mode0 = 4; p.mode1 = 0; p.mode3 = 0; p.mode2 = 0; p.hp -= 120; }
                p.flg |= 4u;
            }
            if (!pjoint(w, 3, &m3)) { put_player(w, &p); return 1; }
            pd.x = m3[12]; pd.y = m3[13]; pd.z = m3[14];
            v.x = w->px - p.pos.x; v.y = 15.0f; v.z = w->pz - p.pos.z;
            if (!en17_sv || !en17_sv->eff_ptcl2v_player) gap(w, "bhEff_SetPtcl2V (en17.c:1644)");
            else for (j = 0; j < 8; j++) en17_sv->eff_ptcl2v_player(&pd, &v, j / 2);
            put_player(w, &p);
            if (en17_sv->player_blood2) en17_sv->player_blood2(2, &ps, 0);
            else gap(w, "bhEne_SetBlood2 (en17.c:1654)");
            return 1;
        }
    }
    return 0;
}

/* en17.c:1364 bhEne17_PlyDG00: Wurf des Spielers (Motion 75/76). */
static void ply_dg00(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC dv;
    int ang;
    switch (p->mode3) {
    case 0:
        p->flg &= ~0x40000u; p->flg |= 0x10000u; p->flg |= 0x200000u;
        p->frm_no = 65536; p->hokan_count = 0; p->hokan_rate = 49152; p->mtn_add = 65536; p->mtn_md = 0;
        p->ct0 = 0; p->ct1 = 0;
        dv.x = w->px - p->pos.x; dv.y = w->py - p->pos.y; dv.z = w->pz - p->pos.z;
        if (recvx_t3_dgc_dir_check(&dv, p->ay) != 0) { p->mtn_no = 75; p->ayp = (int32_t)(10430.381f * atan2f(-dv.x, -dv.z)); }
        else { p->mtn_no = 76; p->ayp = (int32_t)(10430.381f * atan2f(dv.x, dv.z)); }
        if (en17_sv && en17_sv->player_extra_set) en17_sv->player_extra_set(p->mtn_no == 75 ? 32768 : 0, 2.0f);
        else gap(w, "plp->axp / plp->spd (en17.c:1388)");
        recvx_t3_player_se_play(w, 0x402);
        p->mode3 += 1;
        break;
    case 1: {
        int32_t axp = 0; float spd = 0;
        if (p->frm_no / 65536 < 10) {
            ang = (uint16_t)(p->ayp - p->ay);
            if (32768 < ang) ang -= 65536;
            p->ay += ang / 2;
        }
        if (w->mtn_no != 16) {
            NJS_MATRIX m;
            NJS_POINT3 ps;
            if (!en17_sv || !en17_sv->player_extra_get || !en17_sv->player_extra_get(&axp, &spd)) { gap(w, "plp->axp / plp->spd (en17.c:1411)"); break; }
            spd *= 0.8f;
            en17_sv->player_extra_set(axp, spd);
            { const int a = (p->ay + axp) & 0xFFFF; p->pos.x -= spd * njSin(a); p->pos.z -= spd * njCos(a); }   /* bhAddSpeed */
            if (pjoint(w, 17, &m)) { ps.x = m[12]; ps.y = 1.0f; ps.z = m[14]; set_smoke3(w, &ps, p->ayp); }
            if (pjoint(w, 21, &m)) { ps.x = m[12]; ps.z = m[14]; set_smoke3(w, &ps, p->ayp); }
        }
        if (p->frm_no / 65536 == 0) {
            p->flg &= ~0x200000u; p->flg &= ~4u;
            { const RECVX_ENEMY_EXT* x = recvx_enemy_ext(); if (x && x->pad_on_clear) x->pad_on_clear(0xF); else gap(w, "sys->pad_on (en17.c:1427)"); }
            p->flg &= ~0x10000u; p->flg |= 8u; p->stflg &= ~0x10000u; p->at_flg = 0;
            p->mtn_enemy = 0; p->mtn_enemy_work = NULL;
            if (en17_sv && en17_sv->player_library) en17_sv->player_library(w, 0u); else gap(w, "plp->mnwP = plp->mnwPb (en17.c:1432)");
            p->mode0 = 1;
        }
        break;
    }
    }
}
/* en17.c:1440 bhEne17_PlyDG01: Griffreaktion mit den Wurzelversaetzen mtn42..45. */
static void ply_dg01(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w)
{
    static const EN17_POS* const mtn_pos[4] = { ply_mtn42_pos, ply_mtn43_pos, ply_mtn44_pos, ply_mtn45_pos };
    RECVX_ENEMY_VEC dv;
    int frm, i, rot, ang;
    switch (p->mode3) {
    case 0:
        p->flg &= ~0x40000u; p->flg |= 0x10000u; p->flg |= 0x200000u;
        p->mtn_enemy = 1; p->mtn_enemy_owner = w->id; p->mtn_enemy_work = w;
        if (en17_sv && en17_sv->player_library) en17_sv->player_library(w, 1u); else { gap(w, "plp->mnwP = epw->mnwP (en17.c:1463)"); return; }
        p->frm_no = 65536; p->hokan_count = 0; p->hokan_rate = 49152; p->mtn_add = 65536; p->mtn_md = 0;
        p->ct0 = 0; p->ct1 = 0;
        dv.x = w->px - p->pos.x; dv.y = w->py - p->pos.y; dv.z = w->pz - p->pos.z;
        if (recvx_t3_dgc_dir_check(&dv, p->ay) != 0) { p->mtn_no = 21; p->ayp = (int32_t)(10430.381f * atan2f(-dv.x, -dv.z)); }
        else { p->ayp = (int32_t)(10430.381f * atan2f(dv.x, dv.z)); p->mtn_no = 20; p->ct0 = 1; }
        recvx_t3_player_se_play(w, 0x402);
        p->mode3 += 1;
        break;
    case 1:
        frm = p->frm_no / 65536;
        if (frm < 10) {
            ang = (uint16_t)(p->ayp - p->ay);
            if (32768 < ang) ang -= 65536;
            p->ay += ang / 2;
        }
        if (frm == 7) {
            NJS_MATRIX m;
            NJS_POINT3 key;
            if (!pjoint(w, 1, &m)) return;
            key.x = m[12]; key.z = m[14]; key.y = 1.0f;
            rot = (p->ct0 == 0) ? p->ay + 16384 : p->ay - 16384;
            for (i = 0; i < 5; i++) { set_smoke2(w, &key, rot); rot += 8192; }
        }
        if (frm == 0) { p->mtn_no = (p->mtn_no == 21) ? 23 : 22; p->mode3 += 1; }
        break;
    case 2:
        if (p->frm_no / 65536 == 0) {
            if (en17_sv && en17_sv->player_root_zero_xz) en17_sv->player_root_zero_xz(); else gap(w, "plp->mlwP->objP->pos[0/2] = 0 (en17.c:1533)");
            p->flg &= ~0x200000u;
            { const RECVX_ENEMY_EXT* x = recvx_enemy_ext(); if (x && x->pad_on_clear) x->pad_on_clear(0xF); else gap(w, "sys->pad_on (en17.c:1427)"); }
            p->flg &= ~0x10000u; p->flg |= 8u; p->stflg &= ~0x10000u; p->at_flg = 0;
            p->mtn_enemy = 0; p->mtn_enemy_work = NULL;
            if (en17_sv && en17_sv->player_library) en17_sv->player_library(w, 0u); else gap(w, "plp->mnwP = plp->mnwPb (en17.c:1432)");
            p->mode0 = 1;
            p->flg &= ~4u;
        }
        break;
    }
    if (p->mode0 == 4 || p->mode0 == 6) {                            /* :1548 */
        const int idx = (int)p->mtn_no - 20;
        if (idx < 0 || idx > 3) { gap(w, "mtn_pos[plp->mtn_no - 20] ausserhalb (en17.c:1549)"); return; }
        frm = p->frm_no / 65536;
        if (frm < 0 || frm >= ((idx < 2) ? 20 : 51)) { gap(w, "ply_mtn_pos[frm] ausserhalb (en17.c:1550)"); return; }
        {
            NJS_MATRIX m;
            NJS_POINT3 key = { mtn_pos[idx][frm].px, 0.0f, mtn_pos[idx][frm].pz }, out;
            njUnitMatrix(&m);
            njTranslate(&m, p->pos.x, p->pos.y, p->pos.z);
            njRotateXYZ(&m, p->ax, p->ay, p->az);
            njCalcPoint(&m, &key, &out);
            p->pos.x = out.x; p->pos.y = out.y; p->pos.z = out.z;
        }
    }
}

/* ===========================================================================
   en17sub.c: Kettenwerke (linker/rechter Arm)
   =========================================================================== */
static void axs_check(RECVX_ENEMY_WORK* arm)
{
    RECVX_ENEMY_WORK* epp = arm->lkwkp;
    RECVX_EN17_EXP1* x;
    NJS_MATRIX m;
    NJS_POINT3 ps = { 0.0f, 0.0f, -19.5f }, pd;
    float idata[6][3];
    NJS_SPLINE attr;
    int iparam = 6;
    float fparam = 0;
    int i;
    if (!epp || !epp->exp1) { gap(arm, "bhEne17_AxsCheck: epw->lkwkp/exp1 NULL (en17sub.c:138)"); return; }
    x = x1(epp);
    if (!joint(arm, 1, &m)) return;
    njCalcPoint(&m, &ps, &pd);
    for (i = 5; i > 0; i--) {                                         /* :148 Historie weiterreichen */
        x->tip_history[i] = x->tip_history[i - 1];
        x->root_history[i] = x->root_history[i - 1];
    }
    x->tip_history[0].x = pd.x; x->tip_history[0].y = pd.y; x->tip_history[0].z = pd.z;   /* :159 i == 0 */
    x->root_history[0].x = m[12]; x->root_history[0].y = m[13]; x->root_history[0].z = m[14];
    attr.keys = 0; attr.iparam = &iparam; attr.fparam = &fparam;
    for (i = 0; i < 6; i++) { idata[i][0] = x->tip_history[i].x; idata[i][1] = x->tip_history[i].y; idata[i][2] = x->tip_history[i].z; }
    /* :179 exp1[6] = exp1[0] ist Ausgabepunkt 0 = tip_curve[0]; Punkte 1..19 laufen von :183 an. */
    x->tip_curve[0] = x->tip_history[0];
    for (i = 1; i < 20; i++) njBezierSpline(&idata[0][0], &x->tip_curve[i].x, &attr, 0.05f * (float)i);
    for (i = 0; i < 6; i++) { idata[i][0] = x->root_history[i].x; idata[i][1] = x->root_history[i].y; idata[i][2] = x->root_history[i].z; }
    x->root_curve[0] = x->root_history[0];                           /* :195 */
    for (i = 1; i < 20; i++) njBezierSpline(&idata[0][0], &x->root_curve[i].x, &attr, 0.05f * (float)i);
}
void recvx_en17_larm(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* epp = w->lkwkp;
    RECVX_EN17_EXP1* x;
    int i;
    if (!epp) { gap(w, "bhEne17LArm: lkwkp NULL (en17sub.c:28)"); return; }
    if (w->mode0 == 0) {                                              /* :58 LArm_Init */
        w->stflg = 0; w->flg |= 0x8000u; w->mdflg |= 0x20u;
        if (epp->exp1) {
            x = x1(epp);
            for (i = 0; i < 5; i++) { memset(&x->tip_history[i], 0, sizeof x->tip_history[i]); memset(&x->root_history[i], 0, sizeof x->root_history[i]); }
            memset(x->tip_curve, 0, sizeof x->tip_curve);
            memset(x->root_curve, 0, sizeof x->root_curve);
        }
        w->mode0 = 1; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
    }
    BASE(w, calc_model, "bhCalcModel (en17sub.c:31)");
    w->sv->calc_model(w);
    if (epp->flg & 0x80000000u) { if (epp->flg & 0x40000000u) axs_check(w); }
    else axs_check(w);
    if (w->sv->model_node_hidden) w->sv->model_node_hidden(w, 1u, epp->type == 1u ? 1 : 0);
    else gap(w, "mlwP->objP[1].evalflags (en17sub.c:45)");
}
void recvx_en17_rarm(RECVX_ENEMY_WORK* w)
{
    if (w->mode0 == 0) {                                              /* :108 RArm_Init */
        w->stflg = 0; w->flg |= 0x8000u; w->mdflg |= 0x20u;
        w->mode0 = 1; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
    }
    BASE(w, calc_model, "bhCalcModel (en17sub.c:104)");
    w->sv->calc_model(w);
}
