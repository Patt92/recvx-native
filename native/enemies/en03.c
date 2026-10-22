/* EN03 Schwarze Witwe — eigenstaendige native Zustandsmaschine auf dem
   einzigen WorldEntity-Work. Semantikreferenzen (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en03.c:478 bhEne03, :625 Init, :886-1142 Brain
   BR00..02, :1145-2698 Move MV00..19, :2701-3211 Nage NG00/01, :3216-4224
   Damage DG00..12, :4229-4822 Die DD00..05, :4825-4950 SearchPlayer/DirTarget/
   GoAHead/GetPartsPos, :5064 CollisionWalls, :6074 CollisionBoxEdge2, :6199/
   :6216 CollisionLine/2, :6278 DiveSpace, :6335 GetWallDir, :6366 DamageInit,
   :6715 CheckJumpSpace, :6798 PlayerControl, :7117 CallSE, :7275 Acid,
   :7353 HitMark. Die gemeinsamen Bausteine (MakeMatrix, HideParts, GetWall,
   SetModelFlg, AddNullTrans, DGDirCheck, DirTarget, GoAHead, GetPartsPos,
   Collision-Familie) liegen in native/enemies/en03_spider.c und
   en03_collision.c.
   Reihenfolge der Zufallsaufrufe und der njRotateXYZ-Schluessel gegen die
   Disassembly geprueft. Ungebundene Dienste melden den Originalnamen und
   erfinden keinen Wert. */
#include "en03.h"
#include "en03_lib.h"
#include "en03_collision.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const RECVX_EN03_ID_SERVICES* en03_sv;
void recvx_en03_id_bind(const RECVX_EN03_ID_SERVICES* services) { en03_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}

static RECVX_EN03_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN03_STATE*)w->exp0; }
static const RECVX_EN03_STATE* cst(const RECVX_ENEMY_WORK* w) { return (const RECVX_EN03_STATE*)w->exp0; }
static RECVX_EN03_BODY* bd(RECVX_ENEMY_WORK* w) { return (RECVX_EN03_BODY*)&st(w)->h; }
static RECVX_EN03_EXP1* xp1(RECVX_ENEMY_WORK* w) { return (RECVX_EN03_EXP1*)w->exp1; }
static NJS_MATRIX* bm(RECVX_ENEMY_WORK* w) { return (NJS_MATRIX*)st(w)->h.m; }
static void set_modes(RECVX_ENEMY_WORK* w, uint8_t m0, uint8_t m1, uint8_t m2, uint8_t m3)
{
    w->mode0 = m0; w->mode1 = m1; w->mode2 = m2; w->mode3 = m3;
}
/* -rand() / -2.1474836E9f — beide Vorzeichen heben sich auf; der Wert ist
   (float)rand() / 2147483648.0f. */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en03)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static int rnd_mod(RECVX_ENEMY_WORK* w, int n, int* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en03)"); return 0; }
    *out = (int)((uint32_t)w->sv->random_next() % (uint32_t)n);
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en03)"); return 0; }
    return 1;
}
/* plp->dvx/dvy/dvz: der Rueckstoss des Spielerworks (en03.c:2856/2916/3109/
   3190). RECVX_ENEMY_PLAYER fuehrt diese drei Felder nicht; der native
   Spielerbesitzer nimmt sie ueber player_dv_set (world.cpp h.player_dv)
   entgegen. Ohne Bindung wird die Luecke namentlich gemeldet. */
static void player_set_dv(RECVX_ENEMY_WORK* w, float x, float y, float z)
{
    RECVX_ENEMY_VEC v;
    if (!en03_sv || !en03_sv->player_dv_set) {
        gap(w, "plp->dvx/dvy/dvz ohne Spielerbesitzer (en03)"); return;
    }
    v.x = x; v.y = y; v.z = z;
    en03_sv->player_dv_set(&v);
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_set) { gap(w, "plp schreiben (en03)"); return; }
    w->sv->player_set(p);
}
/* epw->mnwP[mtn_no].frm_num. */
static uint32_t frames(RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    uint32_t n;
    if (!w->sv || !w->sv->motion_frames) { gap(w, "epw->mnwP[mtn_no].frm_num (en03)"); return 0; }
    n = w->sv->motion_frames(w, mtn);
    if (!n) gap(w, "epw->mnwP[mtn_no] fehlt (en03)");
    return n;
}
/* mkfP = epw->mnwP[mtn].md2P->p[0]; mkfP += frame; */
static int root_pos(RECVX_ENEMY_WORK* w, uint32_t mtn, uint32_t frame, NJS_VECTOR* out)
{
    RECVX_ENEMY_VEC p;
    if (!w->sv || !w->sv->motion_root_position || !w->sv->motion_root_position(w, mtn, frame, &p)) {
        gap(w, "mnwP[mtn].md2P->p[0] (en03)"); return 0;
    }
    out->x = p.x; out->y = p.y; out->z = p.z;
    return 1;
}
/* mkaP = epw->mnwP[mtn].md2P->p[1]; mkaP += frame; */
static int root_ang(RECVX_ENEMY_WORK* w, uint32_t mtn, uint32_t frame, int32_t* a)
{
    if (!w->sv || !w->sv->motion_root_angles || !w->sv->motion_root_angles(w, mtn, frame, a)) {
        gap(w, "mnwP[mtn].md2P->p[1] (en03)"); return 0;
    }
    return 1;
}
static int root_ang_frm(RECVX_ENEMY_WORK* w, uint32_t mtn, int32_t frm, int32_t* a)
{
    if (frm < 0) { gap(w, "mnwP[mtn].md2P->p[1] (en03): frm_no negativ"); return 0; }
    return root_ang(w, mtn, (uint32_t)(frm >> 16), a);
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    if (!w->sv || !w->sv->joint_matrix || !w->sv->joint_matrix(w, j, (float*)m)) {
        gap(w, "mlwP->owP[n].mtx (en03)"); return 0;
    }
    return 1;
}
static void call_se(RECVX_ENEMY_WORK* w, int se)
{
    const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
    recvx_t3_call_se(w, &at, se);
}
static int route(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b,
                 RECVX_ENEMY_VEC* out, int* r)
{
    if (!en03_sv || !en03_sv->route_points) { gap(w, "bhCheckRoute (en03)"); return 0; }
    *r = en03_sv->route_points(a, b, out);
    if (*r < 0) { gap(w, "bhCheckRoute (en03): rom->rutp"); return 0; }
    return 1;
}
/* bhCheckWallType(pos, flg, r, h): 1 = Treffer. */
static int wall_type(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* p, float r, float h, int* hit)
{
    if (!w->sv || !w->sv->wall_type) { gap(w, "bhCheckWallType (en03)"); return 0; }
    *hit = w->sv->wall_type(p, 0, r, h);
    return 1;
}
static float ground_pos(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* p, int* ok)
{
    if (!w->sv || !w->sv->ground) { gap(w, "bhGetGroundPosition (en03)"); *ok = 0; return 0.0f; }
    *ok = 1;
    return w->sv->ground(p);
}
/* ((ATR_WORK*)EXP0_I(96))->... : der Originalzeiger als Index im gemeinsamen
   wand-Durchlauf (en03_spider.c recvx_en03_get_wall liefert id - 1). */
static int wall_record(RECVX_ENEMY_WORK* w, int index, RECVX_T3_ATR* out)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    if (index < 0 || !x || !x->wal_count || !x->wal_at || x->wal_count() <= index ||
        !x->wal_at((uint32_t)index, out)) {
        gap(w, "((ATR_WORK*)EXP0_I(96)) (en03)"); return 0;
    }
    return 1;
}
/* rom->h */
static int room_h(RECVX_ENEMY_WORK* w, float* out)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    if (!x || !x->room_height) { gap(w, "rom->h (en03)"); return 0; }
    *out = x->room_height();
    return 1;
}
/* en03.c:595 epw->mnwP != epw->mnwPb: die Motion kommt aus der RAUM-MTN-Liste
   statt aus der eingebauten Gegnerbibliothek (enemy_work.h:212 mtn_room). */
static int room_motion(const RECVX_ENEMY_WORK* w) { return w->mtn_room != 0; }
static void get_wall(RECVX_ENEMY_WORK* w) { st(w)->h.wall = recvx_en03_get_wall(w, bd(w)); }
static void do_calc_model(RECVX_ENEMY_WORK* w)
{
    if (!w->sv || !w->sv->calc_model) { gap(w, "bhCalcModel (en03)"); return; }
    w->sv->calc_model(w);
}

/* --- Reine Daten (en03.c:21-406) ------------------------------------------- */
static const RECVX_ENEMY_VEC spm_016[45] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, -0.218743995f, 0.0f }, { 0.0f, -0.26177001f, 0.0f },
    { 0.0f, -0.283394009f, 0.0f }, { 0.0f, -0.283618003f, 0.0f }, { 0.0f, -0.262441009f, 0.0f },
    { 0.0f, -0.219705999f, 0.0f }, { 0.0f, 0.537151992f, -0.0567060001f },
    { 0.0f, 0.950820029f, -0.0633549988f }, { 0.0f, 0.462606013f, -0.0712549984f },
    { 0.0f, 0.289056987f, -0.0788789988f }, { 0.0f, 0.259640008f, -0.0861949995f },
    { 0.0f, 0.227210999f, -0.0931779966f }, { 0.0f, 0.180557996f, -0.0997989997f },
    { 0.0f, 0.126130998f, -0.106032997f }, { 0.0f, 0.128854007f, -0.111854002f },
    { 0.0f, 0.181151003f, -0.117241003f }, { 0.0f, 0.132466003f, -0.122172996f },
    { 0.0f, 0.0634640008f, -0.126628995f }, { 0.0f, 0.0191810001f, -0.130595997f },
    { 0.0f, -0.000353999989f, -0.134051993f }, { 0.0f, 0.00609100005f, -0.136989996f },
    { 0.0f, 0.0735360011f, -0.139393002f }, { 0.0f, 0.0920139998f, -0.141257003f },
    { 0.0f, 0.0102890003f, -0.142570004f }, { 0.0f, -0.0898019969f, -0.143329993f },
    { 0.0f, -0.125242993f, -0.206928998f }, { 0.0f, -0.160959005f, -0.470672011f },
    { 0.0f, -0.197161004f, -0.469761014f }, { 0.0f, -0.233820006f, -0.468297005f },
    { 0.0f, -0.291278005f, -0.46628499f }, { 0.0f, -0.333431005f, -0.463735998f },
    { 0.0f, -0.308402002f, -0.460655004f }, { 0.0f, -0.25276199f, -0.457060993f },
    { 0.0f, -0.226135999f, -0.452955991f }, { 0.0f, -0.215074003f, -0.44837001f },
    { 0.0f, -0.266965002f, -0.501981974f }, { 0.0f, -0.387537003f, -0.263983011f },
    { 0.0f, -0.568105996f, -0.222800002f }, { 0.0f, -0.956220984f, -0.176549003f },
    { 0.0f, -1.12237203f, -0.126333997f }, { 0.0f, -0.834299982f, -0.0733570009f },
    { 0.0f, -0.609296978f, -0.0188759994f }, { 0.0f, -0.450051993f, 0.0358089991f },
    { 0.0f, -0.356626987f, 0.0893910006f }
};
static const RECVX_ENEMY_VEC spm_023[25] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.224094003f, -0.125722006f }, { 0.0f, 0.981598973f, -0.273948997f },
    { 0.0f, 2.12025094f, -0.204677001f }, { 0.0f, 2.59201503f, 0.103661999f },
    { 0.0f, 1.87202299f, 0.563848972f }, { 0.0f, 0.737565994f, 0.995473027f },
    { 0.0f, 0.00721199997f, 1.204216f }, { 0.0f, -0.587921977f, 1.12663901f },
    { 0.0f, -0.884258986f, 0.848743975f }, { 0.0f, -0.827269971f, 0.509276986f },
    { 0.0f, -0.459913999f, 0.189154997f }, { 0.0f, -0.119617f, 0.0278840009f },
    { 0.0f, -0.0057359999f, 0.0189759992f }, { 0.0f, 0.0569400005f, 0.0112309996f },
    { 0.0f, 0.0684129968f, 0.00464700023f }, { 0.0f, 0.185518995f, -0.000774000015f },
    { 0.0f, 0.156975999f, -0.00503499992f }, { 0.0f, -0.0356190018f, -0.00813299976f },
    { 0.0f, -0.0849379972f, -0.0100689996f }, { 0.0f, -0.101379998f, -0.0108430004f },
    { 0.0f, -0.0849379972f, -0.0104569998f }, { 0.0f, -0.0356200002f, -0.0089069996f },
    { 0.0f, 0.0f, -0.00619600015f }, { 0.0f, 0.0f, -0.002324f }
};
static const RECVX_ENEMY_VEC spm_026[16] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.30459401f, -1.17761803f }, { 0.0f, 0.844596028f, -1.61228299f },
    { 0.0f, 1.18923604f, -2.095191f }, { 0.0f, 1.14049196f, -2.58198094f },
    { 0.0f, 0.903213024f, -2.77993989f }, { 0.0f, -0.0704019964f, -1.82634902f },
    { 0.0f, -0.180464f, -0.808885992f }, { 0.0f, 0.594169021f, -0.379321009f },
    { 0.0f, -0.137284994f, -0.686769009f }, { 0.0f, 0.586520016f, -2.03712106f },
    { 0.0f, -1.14873302f, -2.89061999f }, { 0.0f, -1.01687205f, -3.97540689f },
    { 0.0f, -1.09641302f, -3.57847905f }, { 0.0f, -1.72937405f, -0.468986988f },
    { 0.0f, -0.183275998f, -0.220272005f }
};
static const RECVX_ENEMY_VEC spm_028[13] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, -0.151141003f, 0.0439810008f }, { 0.0f, -0.316551f, 0.0733029991f },
    { 0.0f, -0.276656002f, 0.0146599999f }, { 0.0f, -0.0314540006f, -0.131944001f },
    { 0.0f, 0.250993013f, -0.27502501f }, { 0.0f, 0.392113f, -0.341735989f },
    { 0.0f, 0.470683008f, -0.387134999f }, { 0.0f, 0.486701995f, -0.411226004f },
    { 0.0f, 0.440171003f, -0.414005011f }, { 0.0f, 0.331088006f, -0.395476013f },
    { 0.0f, 0.159455001f, -0.355636001f }, { 0.0f, -0.0862509981f, -0.348071992f }
};
static const RECVX_ENEMY_VEC spm_029[20] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, -0.393936992f, -0.350028008f }, { 0.0f, -0.526373029f, -0.362201005f },
    { 0.0f, -0.617738008f, -0.370819002f }, { 0.0f, -0.668031991f, -0.375883013f },
    { 0.0f, -0.677253008f, -0.377393007f }, { 0.0f, -0.645403981f, -0.375348002f },
    { 0.0f, -0.57248199f, -0.369747996f }, { 0.0f, -0.458487004f, -0.360596001f },
    { 0.0f, -0.303422004f, -0.347887009f }, { 0.0f, -0.148374006f, -0.331624001f },
    { 0.0f, -0.0359250009f, -0.311807007f }, { 0.0f, 0.0537150018f, -0.288435996f },
    { 0.0f, 0.120548002f, -0.261510015f }, { 0.0f, 0.164574996f, -0.231029004f },
    { 0.0f, 0.185794994f, -0.196994007f }, { 0.0f, 0.184208006f, -0.159407005f },
    { 0.0f, 0.159814f, -0.118261002f }, { 0.0f, 0.112613998f, -0.0735640004f },
    { 0.0f, 0.0426059999f, -0.0253110006f }
};
static const RECVX_ENEMY_VEC spmh_026[16] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.203879997f, 1.05717099f }, { 0.0f, 0.607263982f, 1.96484005f },
    { 0.0f, 0.976207018f, 1.15935194f }, { 0.0f, 1.159971f, 2.03937006f },
    { 0.0f, 0.946618974f, 3.05633402f }, { 0.0f, 0.392645001f, 2.22015095f },
    { 0.0f, 0.428660005f, 1.87996101f }, { 0.0f, -0.874442995f, 0.907414019f },
    { 0.0f, 0.206529006f, 1.272668f }, { 0.0f, 0.258655995f, 1.68754995f },
    { 0.0f, -0.430880994f, 3.26340199f }, { 0.0f, -1.18263304f, -1.137707f },
    { 0.0f, -1.22665501f, -0.229169995f }, { 0.0f, -1.02153397f, 0.262127012f },
    { 0.0f, -0.444285005f, -0.108080998f }
};
static const RECVX_ENEMY_VEC spmh_013[10] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, -0.242614999f, -0.224384993f }, { 0.0f, -0.404422998f, -0.304396003f },
    { 0.0f, -0.0810950026f, -0.374191999f }, { 0.0f, 0.72736901f, -0.433775991f },
    { 0.0f, 1.38212895f, -0.483148009f }, { 0.0f, 1.51324797f, -0.522302985f },
    { 0.0f, 1.57458794f, -0.530400991f }, { 0.0f, 1.56615698f, -0.546994984f },
    { 0.0f, 1.48794401f, -0.618399024f }
};
static const RECVX_ENEMY_VEC spmh_015[30] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.12219799f, -0.925270021f }, { 0.0f, 0.860957026f, -1.11558104f },
    { 0.0f, 0.615898013f, -1.17758f }, { 0.0f, 0.389692008f, -1.01981294f },
    { 0.0f, 0.125657007f, -0.875569999f }, { 0.0f, -0.19438f, -0.744898021f },
    { 0.0f, -0.510737002f, -0.627790987f }, { 0.0f, -0.775402009f, -0.524253011f },
    { 0.0f, -0.988370001f, -0.434282988f }, { 0.0f, -1.14964795f, -0.357879996f },
    { 0.0f, -1.25923097f, -0.295046002f }, { 0.0f, -1.31712306f, -0.245778993f },
    { 0.0f, -1.32331705f, -0.210078999f }, { 0.0f, -3.11279607f, -0.187948003f },
    { 0.0f, -2.46084905f, -0.163435996f }, { 0.0f, 0.801108003f, -0.129717007f },
    { 0.0f, 0.968285978f, -0.099326998f }, { 0.0f, 0.263590008f, -0.0722640008f },
    { 0.0f, 0.144869998f, -0.0485250019f }, { 0.0f, 0.0444109999f, -0.0281119999f },
    { 0.0f, -0.0377870016f, -0.0110309999f }, { 0.0f, -0.101727001f, 0.00272800005f },
    { 0.0f, -0.147406995f, 0.0131569998f }, { 0.0f, -0.174825996f, 0.0202600006f },
    { 0.0f, -0.183984995f, 0.024038f }, { 0.0f, -0.174886003f, 0.0244859997f },
    { 0.0f, -0.147525996f, 0.0216090009f }, { 0.0f, -0.101906002f, 0.0154050002f },
    { 0.0f, -0.0380269997f, 0.0058749998f }
};
static const int32_t ENE03_HITPOINT[4][16] = {
    { 130, 130, 130, 130, 130, 150, 150, 150, 150, 150, 150, 180, 180, 180, 180, 180 },
    {  50,  50,  50,  50,  50,  70,  70,  70,  70,  70,  70,  90,  90,  90,  90,  90 },
    {  90,  90,  90,  90,  90, 110, 110, 110, 130, 130, 130, 130, 130, 150, 150, 150 },
    {  30,  30,  30,  30,  30,  50,  50,  50,  60,  60,  60,  60,  60,  70,  70,  70 }
};
static const int8_t spider_tree[5][6] = {
    {  0, -1,  0,  0,  0,  0 },
    {  0,  1,  7,  8,  9, -1 },
    {  0,  1, 25, 26, 27, -1 },
    {  0,  1, 10, 11, 12, -1 },
    {  0,  1, 28, 29, 30, -1 }
};
static const int8_t SdwTab[6] = { 1, 12, 30, 9, 27, -1 };
static const uint8_t flip_tree[37] = {
     0,  1,  2,  5,  6,  3,  4, 10,
    11, 12,  7,  8,  9, 16, 17, 18,
    13, 14, 15, 22, 23, 24, 19, 20,
    21, 28, 29, 30, 25, 26, 27, 33,
    34, 31, 32, 36, 35
};
static const uint8_t BrokenParts[8] = { 10, 16, 22, 28, 7, 13, 19, 25 };
static const RECVX_ENEMY_BP_WORK BloodParam = {
    { 0.0f, 0.1f, 0.0f }, 0, 0.0f, 0.07f, { 0.4f, 0.1f, 0.4f, 0.3f, 0.2f }, { 0, 3, 6, 9, 12 }
};
typedef struct { uint8_t flg; RECVX_ENEMY_VEC ofp; float rx, ry, rz; } EN03_BLOOD;
static const EN03_BLOOD BloodTbl[37] = {
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 3.0f, 0.0f }, 2.0f, 0.0f, 2.0f },
    { 1, { 0.0f, 6.0f, 8.0f }, 3.0f, 0.0f, 4.0f },
    { 1, { 0.0f, 6.0f, -8.0f }, 6.0f, 3.0f, 8.0f },
    { 1, { 0.0f, 6.0f, -8.0f }, 6.0f, 3.0f, 8.0f },
    { 1, { 0.0f, 6.0f, -8.0f }, 6.0f, 3.0f, 8.0f },
    { 1, { 0.0f, 6.0f, -8.0f }, 6.0f, 3.0f, 8.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { -1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 0, { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f },
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }
};
typedef struct { int8_t type[3]; uint8_t bloodstain[3]; uint8_t exef; } EN03_REACT;
static const EN03_REACT DmgReact[21] = {
    { {  0,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  0,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  0,  0,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  0,  0,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  0,  0,  0 }, { 0, 0, 0 }, 0 },
    { {  2,  1,  0 }, { 1, 0, 0 }, 0 },
    { {  0,  0,  0 }, { 0, 0, 0 }, 0 },
    { {  1,  1,  0 }, { 0, 0, 0 }, 0 },
    { {  2,  1,  0 }, { 1, 1, 1 }, 1 },
    { { -1, -1, -1 }, { 0, 0, 0 }, 2 },
    { { -1, -1, -1 }, { 0, 0, 0 }, 1 },
    { { -1, -1, -1 }, { 0, 0, 0 }, 0 },
    { {  2,  2,  2 }, { 1, 1, 1 }, 5 },
    { {  2,  2,  2 }, { 0, 0, 0 }, 1 },
    { {  2,  2,  2 }, { 1, 1, 1 }, 5 }
};
static const RECVX_T3_COMBWEP CombWepTbl[21] = {
    {  0, { 0, 0, 0 },  0,  0 },
    {  0, { 0, 0, 0 },  0,  0 },
    {  4, { 1, 0, 0 }, 30, 20 },
    { 10, { 4, 3, 1 }, 20, 10 },
    { 10, { 4, 3, 1 }, 20, 10 },
    { 10, { 4, 3, 1 }, 10,  0 },
    {  0, { 0, 0, 0 }, 25,  0 },
    {  0, { 0, 0, 0 }, 25,  0 },
    { 25, { 5, 3, 1 },  5,  0 },
    {  0, { 0, 0, 0 }, 60,  0 },
    {  0, { 0, 0, 0 }, 10,  0 },
    {  0, { 0, 0, 0 }, 30,  0 },
    { 25, { 5, 4, 2 }, 10,  0 },
    {  0, { 0, 0, 0 }, 60,  0 },
    {  0, { 0, 0, 0 },  0,  0 },
    {  0, { 0, 0, 0 },  0,  0 },
    { 15, { 1, 1, 1 },  5,  0 },
    {  0, { 0, 0, 0 },  0,  0 },
    {  0, { 0, 0, 0 },  0,  0 },
    {  0, { 0, 0, 0 },  0,  0 },
    {  0, { 0, 0, 0 },  0,  0 }
};
static const RECVX_T3_COMBJOINT CombJointTbl[37] = { { 0, 0 } };
static const RECVX_ENEMY_CPCL CapColTabA[23] = {
    {  1,  1,  16 }, {  0,  0, -20 }, {  1,  1,  11 }, {  0,  0, -38 }, {  2,  2,  30 },
    {  0, 15,  20 }, {  7,  8,   3 }, {  8,  9,   3 }, { 10, 11,   3 }, { 11, 12,   3 },
    { 13, 14,   3 }, { 14, 15,   3 }, { 16, 17,   3 }, { 17, 18,   3 }, { 19, 20,   3 },
    { 20, 21,   3 }, { 22, 23,   3 }, { 23, 24,   3 }, { 25, 26,   3 }, { 26, 27,   3 },
    { 28, 29,   3 }, { 29, 30,   3 }, {  0,  0,   0 }
};
static const RECVX_ENEMY_CPCL CapColTabB[21] = {
    {  1,  1,  16 }, {  0,  0, -20 }, {  1,  1,  11 }, {  0,  0, -38 }, {  7,  8,   3 },
    {  8,  9,   3 }, { 10, 11,   3 }, { 11, 12,   3 }, { 13, 14,   3 }, { 14, 15,   3 },
    { 16, 17,   3 }, { 17, 18,   3 }, { 19, 20,   3 }, { 20, 21,   3 }, { 22, 23,   3 },
    { 23, 24,   3 }, { 25, 26,   3 }, { 26, 27,   3 }, { 28, 29,   3 }, { 29, 30,   3 },
    {  0,  0,   0 }
};

/* --- Zufallstabelle der Aktionsauswahl (en03.c:1166) ----------------------- */
static const uint8_t mv00_action[2][6] = {
    { 1,  1,  2, 2, 0, 0 },
    { 1, 17, 17, 2, 0, 0 }
};

/* en03.c:4922 bhEne03_GetPartsPos(epw, spider_tree[k], p). */
static int parts(RECVX_ENEMY_WORK* w, int k, RECVX_ENEMY_VEC* out)
{
    return recvx_en03_parts_pos(w, bd(w), spider_tree[k], out);
}

static int parts(RECVX_ENEMY_WORK* w, int k, RECVX_ENEMY_VEC* out);

/* --- Vorwaertsdeklarationen ------------------------------------------------- */
static void en03_init(RECVX_ENEMY_WORK*);
static void en03_move(RECVX_ENEMY_WORK*);
static void en03_nage(RECVX_ENEMY_WORK*);
static void en03_damage(RECVX_ENEMY_WORK*);
static void en03_die(RECVX_ENEMY_WORK*);
static void br00(RECVX_ENEMY_WORK*);
static void br01(RECVX_ENEMY_WORK*);
static void br02(RECVX_ENEMY_WORK*);
static void mv00(RECVX_ENEMY_WORK*);
static void mv01(RECVX_ENEMY_WORK*);
static void mv02(RECVX_ENEMY_WORK*);
static void mv03(RECVX_ENEMY_WORK*);
static void mv04(RECVX_ENEMY_WORK*);
static void mv05(RECVX_ENEMY_WORK*);
static void mv06(RECVX_ENEMY_WORK*);
static void mv07(RECVX_ENEMY_WORK*);
static void mv08(RECVX_ENEMY_WORK*);
static void mv09(RECVX_ENEMY_WORK*);
static void mv10(RECVX_ENEMY_WORK*);
static void mv11(RECVX_ENEMY_WORK*);
static void mv12(RECVX_ENEMY_WORK*);
static void mv13(RECVX_ENEMY_WORK*);
static void mv14(RECVX_ENEMY_WORK*);
static void mv15(RECVX_ENEMY_WORK*);
static void mv16(RECVX_ENEMY_WORK*);
static void mv17(RECVX_ENEMY_WORK*);
static void mv18(RECVX_ENEMY_WORK*);
static void mv19(RECVX_ENEMY_WORK*);
static void ng00(RECVX_ENEMY_WORK*);
static void ng01(RECVX_ENEMY_WORK*);
static void dg01(RECVX_ENEMY_WORK*);
static void dg02(RECVX_ENEMY_WORK*);
static void dg03(RECVX_ENEMY_WORK*);
static void dg04(RECVX_ENEMY_WORK*);
static void dg06(RECVX_ENEMY_WORK*);
static void dg07(RECVX_ENEMY_WORK*);
static void dg08(RECVX_ENEMY_WORK*);
static void dg09(RECVX_ENEMY_WORK*);
static void dg10(RECVX_ENEMY_WORK*);
static void dg11(RECVX_ENEMY_WORK*);
static void dg12(RECVX_ENEMY_WORK*);
static void dd00(RECVX_ENEMY_WORK*);
static void dd01(RECVX_ENEMY_WORK*);
static void dd02(RECVX_ENEMY_WORK*);
static void dd03(RECVX_ENEMY_WORK*);
static void dd04(RECVX_ENEMY_WORK*);
static void dd05(RECVX_ENEMY_WORK*);
static void damage_init(RECVX_ENEMY_WORK*);
static void player_control(RECVX_ENEMY_WORK*);
static void en03_call_se(RECVX_ENEMY_WORK*);
static void en03_acid(RECVX_ENEMY_WORK*, int type);
static void en03_hit_mark(RECVX_ENEMY_WORK*);
static int  en03_dive_space(RECVX_ENEMY_WORK*);
static int  en03_check_jump_space(RECVX_ENEMY_WORK*);
static void en03_search_player(RECVX_ENEMY_WORK*, int ang);
static void en03_collision_line(RECVX_ENEMY_WORK*);
static void en03_collision_line2(RECVX_ENEMY_WORK*);
static int  en03_collision_walls(RECVX_ENEMY_WORK*);
static void en03_collision_box_edge2(RECVX_ENEMY_WORK*);
static void nage(RECVX_ENEMY_WORK*);

/* en03.c:260/266/272 — die drei Originale haben leere Parameterlisten und
   geben unbedingt 0 zurueck; MV01/MV03/MV17 rufen sie mit Argumenten und
   werten nur diesen Wert aus. */
static int check_climb_wall(void) { return 0; }
static int check_climb_down_wall(void) { return 0; }
static int avoid_wall(void) { return 0; }

/* bhCollisionCheckLine(p1, p2) != NULL. */
static int line_hit(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b)
{
    RECVX_ENEMY_VEC from = *a, to = *b, n;
    if (!w->sv || !w->sv->line) { gap(w, "bhCollisionCheckLine (en03)"); return 0; }
    return w->sv->line(1, &from, &to, &n, 0, 0, NULL, NULL);
}

/* --- Init (en03.c:625-883) ------------------------------------------------- */
static void en03_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s;
    RECVX_EN03_EXP1* e1;
    int i, n;

    w->flg |= 0x1078u;
    w->flg &= ~0x8006u;
    w->flg = (uint16_t)w->flg;          /* Original schneidet auf 16 Bit ab */
    w->mdflg |= 0x4u;
    w->ar = 6.0f; w->ah = 6.0f;
    w->car = 6.0f; w->cah = 6.0f;
    w->aox = 0.0f; w->aoy = 0.0f; w->aoz = 0.0f;
    w->hokan_rate = 65536; w->hokan_count = 0;
    w->mtn_no = 0; w->mtn_md = 0; w->mtn_add = 65536;
    w->frm_no = 0;
    w->mtn_tp = flip_tree;
    w->mtn_md &= ~0x2u;

    do_calc_model(w);

    set_modes(w, 1, 1, 0, 0);

    if (!w->sv->calloc_work) { gap(w, "bhEne_CallocWork (en03)"); return; }

    if (!w->exp0) {
        RECVX_ENEMY_TEMPLATE tmpl;
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN03_STATE));
        if (!w->exp0) { gap(w, "bhEne03_Init: EXP-Budget erschoepft"); return; }
        s = st(w);
        /* en03.c:671-706: der erste Work mit flg&1 und id==3 liefert exp1 und
           exp2. Findet er sich selbst, legt er beide neu an. */
        {
            uint32_t k;
            RECVX_ENEMY_WORK* found = NULL;
            const uint32_t en = (w->sv->enemy_count) ? w->sv->enemy_count() : 0u;
            for (k = 0; k < en; ++k) {
                RECVX_ENEMY_WORK* ep = (w->sv->enemy_at) ? w->sv->enemy_at(k) : NULL;
                if (ep && (ep->flg & 0x1u) && ep->id == 3u) { found = ep; break; }
            }
            if (!found) { gap(w, "bhEne03_Init: kein Work mit id 3 in der ene-Liste"); return; }
            if (found->exp1) {
                w->exp1 = found->exp1;
            } else {
                w->exp1 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN03_EXP1));
                if (!w->exp1) { gap(w, "bhEne03_Init: exp1-Budget erschoepft"); return; }
            }
            e1 = xp1(w);
            (void)e1;
            if (found->exp2) {
                s->leader = (RECVX_ENEMY_WORK*)found->exp2;
            } else {
                if (!en03_sv || !en03_sv->create_child) { gap(w, "eneset.c:170 Kindwork (Anfuehrer) fehlt (en03)"); return; }
                memset(&tmpl, 0, sizeof tmpl);
                tmpl.flg = 0x4008001u; tmpl.id = 3; tmpl.type = 0;
                s->leader = en03_sv->create_child(w, &tmpl, ~0u);
                if (!s->leader) { gap(w, "bhEne03_Init: ene[] voll (Anfuehrer)"); return; }
                s->leader->mdflg |= 0x1u;
                s->leader->mdflg |= 0x20u;
                s->leader->exp1 = w->exp1;
                /* ep_leader->mnwP = epw->mnwP: bhEne03_PlayerControl, der
                   einzige Zweig des Anfuehrerworks, liest mnwP nicht; nativ
                   bleibt mtn_room deshalb 0. */
            }
        }
        /* en03.c:688/694: weitere Spinnen lesen den gemeinsamen Anfuehrer
           aus dem ersten aktiven EN03-Work, nicht aus dessen EXP0-Zustand. */
        w->exp2 = s->leader;
        /* en03.c:708-734: acht Beinkinder auf die owP-Knoten BrokenParts[i]. */
        {
            if (!en03_sv || !en03_sv->create_child) { gap(w, "eneset.c:170 Kindwork (Bein) fehlt (en03)"); return; }
            memset(&tmpl, 0, sizeof tmpl);
            tmpl.flg = 0x8001u; tmpl.id = 31; tmpl.type = 0;
            for (i = 0; i < 8; ++i) {
                RECVX_ENEMY_WORK* c = en03_sv->create_child(w, &tmpl, ~0u);
                if (!c) { gap(w, "bhEne03_Init: ene[] voll (Bein)"); return; }
                s->legs[i] = c;
                c->type = 0;
                c->lkwkp = w;
                c->lkono = (i >= 4) ? 1 : 0;
                c->lox = 0.0f; c->loy = 0.0f; c->loz = 0.0f;
                c->mdflg |= 0x1u;
                c->mdflg |= 0x20u;
                /* owk = &epw->mlwP->owP[BrokenParts[i]]; mtx = &owk->mtx;
                   exp1 = owk. */
                s->leg_link[i].owner = w;
                s->leg_link[i].node = BrokenParts[i];
                c->exp1 = &s->leg_link[i];
            }
            if (!recvx_enemy_ext() || !recvx_enemy_ext()->set_call_func) {
                gap(w, "bhEne_SetCallFunc(bhEne03s, 31)"); return;
            }
            recvx_enemy_ext()->set_call_func(31u, recvx_en03s);
        }
        /* en03.c:735-749: fuenfzehn EN24-Kinder auf mdl[1]. */
        {
            if (!en03_sv || !en03_sv->create_child) { gap(w, "eneset.c:170 Kindwork (EN24) fehlt (en03)"); return; }
            memset(&tmpl, 0, sizeof tmpl);
            tmpl.flg = 0x8001u; tmpl.id = 24; tmpl.type = 0;
            for (i = 0; i < 15; ++i) {
                RECVX_ENEMY_WORK* c = en03_sv->create_child(w, &tmpl, 1u);
                if (!c) { gap(w, "bhEne03_Init: ene[] voll (EN24)"); return; }
                s->babies[i] = c;
                c->lkwkp = w;
                c->lkono = 0;
                c->lox = 0.0f; c->loy = 0.0f; c->loz = 0.0f;
            }
        }
    }

    s = st(w);
    e1 = xp1(w);
    if (!e1) { gap(w, "bhEne03_Init: exp1 fehlt"); return; }

    s->h.seen = 0;
    s->h.dist = 999.0f;
    s->h.ang = 0;
    s->h.target.x = 0.0f; s->h.target.y = 0.0f; s->h.target.z = 0.0f;

    if (!rnd_mod(w, 90, &n)) return;
    s->timer = n + 60;
    s->climb_timer = 0;

    if (!rnd_mod(w, 16, &i)) return;
    if (!en03_sv || !en03_sv->game_mode) { gap(w, "sys->gm_mode (en03)"); return; }
    {
        int gm = 0;
        if (!en03_sv->game_mode(&gm)) return;
        if (gm != 2) {
            w->hp = ENE03_HITPOINT[0][i];
            s->hp2 = ENE03_HITPOINT[1][i];
        } else {
            w->hp = ENE03_HITPOINT[2][i];
            s->hp2 = ENE03_HITPOINT[3][i];
        }
    }

    for (i = 0; i < 8; ++i) s->broken[i] = 0;

    for (i = 0; i < 15; ++i) if (s->babies[i]) s->babies[i]->mdflg |= 0x1u;

    e1->flg = 0;
    if (!rnd_mod(w, 30, &n)) return;
    e1->timer = n + 60;

    njUnitMatrix(bm(w));
    njRotateY(bm(w), (Angle)w->ay);

    s->h.wall = -1;

    if ((w->type & 0x2u)) {
        RECVX_ENEMY_VEC a, b, nor;
        uint32_t id = 0;
        a.x = w->px; a.y = w->py; a.z = w->pz;
        b.x = w->px; b.y = w->py + 999.0f; b.z = w->pz;
        if (!w->sv->line) { gap(w, "bhCollisionCheckLine (en03 Init)"); return; }
        if (w->sv->line(1, &a, &b, &nor, 0, 0, NULL, &id)) {
            s->h.wall = id ? (int)id - 1 : -1;
            s->h.surface = 1;
            recvx_en03_make_matrix(bd(w));
            w->py = b.y;
        }
        w->type &= (uint16_t)~0x2u;
    }

    if (s->h.wall < 0) {
        s->h.surface = 0;
        get_wall(w);
    }

    w->pxb = w->px; w->pyb = w->py; w->pzb = w->pz;

    w->clp_jno[0] = 1; w->clp_jno[1] = 5; w->clp_jno[2] = 12; w->clp_jno[3] = 30;
    w->clp_jno[4] = 9; w->clp_jno[5] = 27; w->clp_jno[6] = 32; w->clp_jno[7] = 34;

    w->mdflg &= ~0x20u;

    if (!(w->flg & 0x800u)) {
        int sdw;
        if (!w->sv->set_shadow) { gap(w, "bhSetShadow (en03)"); return; }
        sdw = w->sv->set_shadow(SdwTab, w, 0, 8.0f, 8.0f, 9.0f);
        if (sdw < 0) { gap(w, "bhEne03_Init: eff[] voll (Schatten)"); return; }
        if (!en03_sv || !en03_sv->effect_id) { gap(w, "eff[sdw].id (en03)"); return; }
        en03_sv->effect_id(sdw, 258u);
        w->flg |= 0x800u;
    }

    w->stflg &= ~0x8u;

    w->cpcl = (w->type & 0x1u) ? CapColTabB : CapColTabA;

    recvx_en03_hide_parts(w, 1u, 0);
    if ((w->type & 0x1u)) recvx_en03_hide_parts(w, 2u, 1);

    recvx_en03_set_model_flg(w, (uint32_t)-4, 0u);

    if ((w->type & 0x4u)) {
        w->type &= (uint16_t)~0x4u;
        w->flg |= 0x2000000u;
        w->stflg |= 0x8u;
        w->flg &= ~0x28u;
    }
}

/* --- Brain (en03.c:886-1142) ----------------------------------------------- */
static void brain(RECVX_ENEMY_WORK* w)
{
    if ((w->flg & 0x2000000u)) { br02(w); return; }
    switch (w->type) {
    case 0: br00(w); return;
    case 1: br01(w); return;
    default: gap(w, "bhEne03_BrainType[epw->type]"); return;
    }
}

/* en03.c:899 BR00 — Verfolgung am Boden. */
static void br00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float dist;
    if (!player(w, &p)) return;
    s->h.dist = recvx_en03_distance_p2p(&(RECVX_ENEMY_VEC){ w->px, w->py, w->pz }, &p.pos);
    en03_search_player(w, 21845);
    if ((s->h.seen == 0) && (w->mode2 != 4) && (s->h.dist < 20.0f)) {
        set_modes(w, 1, 0, 4, 0);
    }
    if (p.hp < 0) return;
    if (s->h.surface == 0) {
        if ((s->h.seen == 1) && (s->h.dist > 12.0f) && (w->mode2 != 3)) {
            w->mode0 = 1; w->mode2 = 3; w->mode3 = 0;
        }
        if ((p.stflg & 0x80000000u) || (p.flg & 0x4u) || (w->flg & 0x4u)) return;
        if ((recvx_t3_check_event_atr(p.flr_no, p.pos.x, p.pos.z, 3, 0, 0) == 0) && (xp1(w) && xp1(w)->timer == 0)) {
            if ((s->h.dist < 20.0f) && (abs(s->h.ang) < 5461) && (fabsf(w->py - p.pos.y) < 0.01f)) {
                int r;
                w->mode1 = 0; w->mode2 = 19; w->mode3 = 0;
                if (!rnd_mod(w, 30, &r)) return;
                xp1(w)->timer = r + 60;
                return;
            }
        }
        if (s->timer == 0) {
            if ((s->h.dist < 25.0f) && (abs(s->h.ang) < 3640)) {
                int r;
                set_modes(w, 1, 0, 9, 0);
                if (!rnd_mod(w, 90, &r)) return;
                s->timer = r + 60;
            }
        } else {
            s->timer--;
        }
    } else if (s->h.surface == 1) {
        dist = njSqrt(((p.pos.x - w->px) * (p.pos.x - w->px)) + ((p.pos.z - w->pz) * (p.pos.z - w->pz)));
        if ((w->py > p.pos.y) && (dist < 40.0f) && (abs(s->h.ang) > 16384) && (en03_dive_space(w) != 0)) {
            set_modes(w, 1, 0, 11, 0);
        }
        if (s->timer == 0) {
            if ((w->py > p.pos.y) && (dist < 25.0f) && (abs(s->h.ang) < 3640)) {
                int r;
                set_modes(w, 1, 0, 9, 0);
                if (!rnd_mod(w, 90, &r)) return;
                s->timer = r + 60;
            }
        } else {
            s->timer--;
        }
    } else {
        dist = njSqrt(((p.pos.x - w->px) * (p.pos.x - w->px)) + ((p.pos.z - w->pz) * (p.pos.z - w->pz)));
        if ((s->h.seen == 1) && (s->h.dist > 12.0f) && (w->mode2 != 3)) {
            w->mode0 = 1; w->mode2 = 3; w->mode3 = 0;
        }
    }
}

/* en03.c:1005 BR01 — Verfolgung an Decke/Wand. */
static void br01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float dx, dy, dz;
    if (!player(w, &p)) return;
    dx = w->px - p.pos.x;
    dy = w->py - p.pos.y;
    dz = w->pz - p.pos.z;
    switch (s->h.surface) {
    case 0: case 1: s->h.dist = njSqrt((dx * dx) + (dz * dz)); break;
    case 2: case 4: s->h.dist = njSqrt((dy * dy) + (dz * dz)); break;
    case 3: case 5: s->h.dist = njSqrt((dx * dx) + (dy * dy)); break;
    default: gap(w, "bhEne03_BR01: EXP0_C(105) ausserhalb"); break;
    }
    en03_search_player(w, 21845);
    if (p.hp < 0) return;
    if (s->h.seen == 1) {
        if ((s->h.dist > 12.0f) && (w->mode2 != 3)) { w->mode0 = 1; w->mode2 = 3; w->mode3 = 0; }
        if ((w->mode2 != 4) && (s->h.dist < 20.0f) && (abs(s->h.ang) > 3640)) {
            set_modes(w, 1, 0, 4, 0);
        }
    }
    if (s->h.surface == 0) {
        if ((s->h.seen == 1) && (s->h.dist > 20.0f) && (en03_check_jump_space(w) != 0)) {
            set_modes(w, 1, 0, 14, 0);
        }
        if ((p.stflg & 0x80000000u) || (p.flg & 0x4u) || (w->flg & 0x4u)) return;
        if (recvx_t3_check_event_atr(p.flr_no, p.pos.x, p.pos.z, 3, 0, 0) != 0) {
            if (s->timer == 0) {
                if ((s->h.dist < 25.0f) && (abs(s->h.ang) < 3640)) {
                    set_modes(w, 1, 0, 9, 0);
                    s->timer = 20;
                }
            } else {
                s->timer--;
            }
        } else if (xp1(w) && xp1(w)->timer == 0) {
            if ((s->h.dist < 30.0f) && (abs(s->h.ang) <= 7280) && (fabsf(w->py - p.pos.y) < 0.01f)) {
                int r;
                w->mode1 = 0; w->mode2 = 19; w->mode3 = 0;
                if (!rnd_mod(w, 30, &r)) return;
                xp1(w)->timer = r + 60;
            }
        }
    } else if (s->h.surface == 1) {
        if ((w->py > p.pos.y) && (s->h.dist < 40.0f) && (en03_dive_space(w) != 0)) {
            set_modes(w, 1, 0, 11, 0);
        }
    } else if ((s->h.seen == 1) && (s->h.m[9] > 0.0f) && (en03_check_jump_space(w) != 0)) {
        set_modes(w, 1, 0, 15, 0);
    }
}

/* en03.c:1118 BR02 — Endkampfzweig. */
static void br02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    if (!player(w, &p)) return;
    s->h.dist = recvx_en03_distance_p2p(&(RECVX_ENEMY_VEC){ w->px, w->py, w->pz }, &p.pos);
    en03_search_player(w, 21845);
    s->h.seen = 1;
    if (s->timer == 0) {
        if ((s->h.dist < 25.0f) && (abs(s->h.ang) < 3640)) {
            int r;
            set_modes(w, 1, 0, 10, 0);
            if (!rnd_mod(w, 90, &r)) return;
            s->timer = r + 60;
        }
    } else {
        s->timer--;
    }
}

/* en03.c:1145 Move. */
static void en03_move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1) brain(w);
    if ((w->flg & 0x4u) && !(w->flg & 0x2u)) damage_init(w);
    if (w->mode0 != 1) return;
    switch (w->mode2) {
    case  0: mv00(w); break;
    case  1: mv01(w); break;
    case  2: mv02(w); break;
    case  3: mv03(w); break;
    case  4: mv04(w); break;
    case  5: mv05(w); break;
    case  6: mv06(w); break;
    case  7: mv07(w); break;
    case  8: mv08(w); break;
    case  9: mv09(w); break;
    case 10: mv10(w); break;
    case 11: mv11(w); break;
    case 12: mv12(w); break;
    case 13: mv13(w); break;
    case 14: mv14(w); break;
    case 15: mv15(w); break;
    case 16: mv16(w); break;
    case 17: mv17(w); break;
    case 18: mv18(w); break;
    case 19: mv19(w); break;
    default: gap(w, "bhEne03_MoveMode2[mode2]"); break;
    }
}

/* --- Move (en03.c:1164-2698) ------------------------------------------------ */

/* en03.c:1164 MV00 — Warten und Aktionswahl. */
static void mv00(RECVX_ENEMY_WORK* w)
{
    int r;
    switch (w->mode3) {
    case 0:
        if (w->type == 0) {
            if (w->mtn_no != 0) {
                w->mtn_no = 0; w->frm_no = 0;
                w->hokan_count = 10; w->hokan_rate = 32768;
            }
        } else if (w->mtn_no != 30) {
            w->mtn_no = 30; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
        }
        if (!rnd_mod(w, 63, &r)) return;
        w->ct0 = r + 40;
        w->mode3++;
        /* faellt durch (Original ohne break) */
    case 1:
        if (w->ct0-- == 0) {
            if (!rnd_mod(w, 4, &r)) return;
            w->mode1 = 1;
            w->mode2 = mv00_action[w->type & 1u][r];
            w->mode3 = 0;
        }
        break;
    default: gap(w, "bhEne03_MV00: mode3 ausserhalb"); break;
    }
}

/* en03.c:1211 MV01 — Vorwaertsgang mit Zielwahl. */
static void mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    int r;
    switch (w->mode3) {
    case 0:
        if (w->type == 0) {
            if (w->mtn_no != 1) {
                if (w->mtn_no != 6 && w->mtn_no != 7) w->frm_no = 0;
                w->mtn_no = 1;
                w->hokan_count = 10; w->hokan_rate = 32768;
            }
        } else if (w->mtn_no != 31) {
            w->mtn_no = 31; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
        }
        if (!rnd_mod(w, 47, &r)) return;
        w->ct0 = r;
        if (s->h.seen == 1) {
            w->ct1 = (s->h.ang < 0) ? -1 : 1;
        } else {
            float f;
            if (!rnd(w, &f)) return;
            w->ct1 = (f < 0.5f) ? -1 : 1;
        }
        w->spd = 0.4f;
        w->mode3++;
        /* faellt durch */
    case 1: {
        RECVX_ENEMY_VEC from, pl, out;
        int now;
        recvx_en03_go_ahead(w, bd(w));
        if (s->climb_timer == 0) {
            if (check_climb_wall() != 0) { w->mode1 = 0; w->mode2 = 6; w->mode3 = 0; break; }
            if (check_climb_down_wall() != 0) { w->mode1 = 0; w->mode2 = 8; w->mode3 = 0; break; }
        } else {
            s->climb_timer--;
        }
        avoid_wall();
        if (!player(w, &p)) return;
        from.x = w->px; from.y = w->py; from.z = w->pz;
        pl = p.pos;
        if (!route(w, &from, &pl, &out, &now)) return;
        if (now != 0xFF) { s->h.target.x = out.x; s->h.target.z = out.z; }
        else { s->h.target.x = p.pos.x; s->h.target.z = p.pos.z; }
        if (s->h.seen == 1) {
            w->ayp = recvx_en03_dir_target_body(w, bd(w), &s->h.target, 819);
            njRotateY(bm(w), (Angle)(int16_t)w->ayp);
        }
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        break;
    }
    default: gap(w, "bhEne03_MV01: mode3 ausserhalb"); break;
    }
}

/* en03.c:1314 MV02 — Drehung auf der Stelle. */
static void mv02(RECVX_ENEMY_WORK* w)
{
    int dir, timer;
    if (!rnd_mod(w, 63, &dir)) return;
    dir &= 1;
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != (uint32_t)(dir + 6)) {
            w->mtn_no = (uint32_t)(dir + 6); w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
        }
        /* en03.c:1332-1334: der zweite RNG-Zug setzt nur ct0;
           das Vorzeichen bleibt vom ersten Zug (dir) abhaengig. */
        if (!rnd_mod(w, 63, &timer)) return;
        w->ct0 = timer;
        w->ayp = ((dir != 0) ? 1 : -1) * 819;
        w->mode3++;
        /* faellt durch */
    case 1:
        njRotateY(bm(w), (Angle)(int16_t)w->ayp);
        if (w->ct0-- == 0) { w->mode1 = 0; w->mode2 = 1; w->mode3 = 0; }
        break;
    default: gap(w, "bhEne03_MV02: mode3 ausserhalb"); break;
    }
}

/* en03.c:1352 MV03 — Verfolgungsgang mit Wegsuche. */
static void mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    int r;
    switch (w->mode3) {
    case 0:
        if (w->type == 0) {
            if (w->mtn_no != 4) {
                w->mtn_no = 4; w->frm_no = 0;
                w->hokan_count = 10; w->hokan_rate = 32768;
                w->spd = 0.6f;
            }
            if (!rnd_mod(w, 60, &r)) return;
            w->ct0 = r + 30;
        } else {
            if (w->mtn_no != 40) {
                w->mtn_no = 40; w->frm_no = 0;
                w->hokan_count = 10; w->hokan_rate = 32768;
                w->spd = 0.8f;
            }
            if (!rnd_mod(w, 60, &r)) return;
            w->ct0 = r + 90;
        }
        w->ct1 = (s->h.ang < 0) ? -1 : 1;
        w->mode3++;
        /* faellt durch */
    case 1: {
        RECVX_ENEMY_VEC from, pl, out;
        int now;
        recvx_en03_go_ahead(w, bd(w));
        if (s->climb_timer == 0) {
            if (check_climb_wall() != 0) { w->mode1 = 0; w->mode2 = 6; w->mode3 = 0; break; }
            if (check_climb_down_wall() != 0) { w->mode1 = 0; w->mode2 = 8; w->mode3 = 0; break; }
        } else {
            s->climb_timer--;
        }
        if (!player(w, &p)) return;
        from.x = w->px; from.y = w->py; from.z = w->pz;
        pl = p.pos;
        if (!route(w, &from, &pl, &out, &now)) return;
        if (now != 0xFF) {
            s->h.target.x = out.x; s->h.target.z = out.z;
            w->ayp = recvx_en03_dir_target_body(w, bd(w), &s->h.target, 819);
            njRotateY(bm(w), (Angle)(int16_t)w->ayp);
        } else {
            s->h.target.x = p.pos.x; s->h.target.z = p.pos.z;
            if (avoid_wall() == 0) {
                w->ayp = recvx_en03_dir_target_body(w, bd(w), &s->h.target, 819);
                njRotateY(bm(w), (Angle)(int16_t)w->ayp);
            }
        }
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
        return;
    }
    default: gap(w, "bhEne03_MV03: mode3 ausserhalb"); break;
    }
}

/* en03.c:1453 MV04 — Eindrehen auf den Spieler. */
static void mv04(RECVX_ENEMY_WORK* w)
{
    int r;
    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_PLAYER p;
        int wang;
        if (!player(w, &p)) return;
        wang = recvx_en03_dir_target_body(w, bd(w), &p.pos, 32768) < 0;
        w->mtn_no = (uint32_t)(wang + 6);
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        if (!rnd_mod(w, 31, &r)) return;
        w->ct0 = r;
        w->mode3++;
        /* faellt durch */
    }
    case 1: {
        RECVX_ENEMY_PLAYER p;
        if (!player(w, &p)) return;
        w->ayp = recvx_en03_dir_target_body(w, bd(w), &p.pos, 819);
        njRotateY(bm(w), (Angle)(int16_t)w->ayp);
        if ((w->ct0-- == 0) || (abs(w->ayp) < 273)) { w->mode1 = 1; w->mode2 = 3; w->mode3 = 0; }
        break;
    }
    default: gap(w, "bhEne03_MV04: mode3 ausserhalb"); break;
    }
}

/* en03.c:1488 MV05 — Anlegen an die Flaeche EXP0_I(100). */
static void mv05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = (w->type == 0) ? 2u : 32u;
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->flg &= ~0x10u;
        w->flg |= 0x1800000u;
        w->flg |= 0x300000u;
        w->flg2 |= 0x1u;
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) {
            RECVX_ENEMY_VEC pos;
            RECVX_T3_ATR hp;
            float h;
            if (!parts(w, 0, &pos)) return;
            w->px = pos.x; w->py = pos.y; w->pz = pos.z;
            njRotateX(bm(w), 16384);
            if (!wall_record(w, s->h.wall2, &hp)) return;
            switch (s->h.surface2) {
            case 0:
                if (hp.type == 0) {
                    h = hp.h;
                    if (h == 0) { if (!room_h(w, &h)) return; }
                    w->py = hp.py + h;
                } else {
                    w->py = hp.py;
                }
                break;
            case 1:
                if (hp.type == 0) w->py = hp.py;
                else w->py = hp.py + hp.h;
                break;
            case 5: w->pz = hp.pz + hp.d; break;
            case 3: w->pz = hp.pz; break;
            case 4: w->px = hp.px; break;
            case 2: w->px = hp.px + hp.w; break;
            default: gap(w, "bhEne03_MV05: EXP0_C(106) ausserhalb"); break;
            }
            s->h.surface = s->h.surface2;
            recvx_en03_make_matrix(bd(w));
            w->flg &= ~0x1800000u;
            w->flg &= ~0x300000u;
            w->mode1 = 1; w->mode2 = 1; w->mode3 = 1;
            w->mtn_no = 1;
            w->ct0 = 10;
            w->spd = 0.4f;
            recvx_en03_go_ahead(w, bd(w));
            w->flg |= 0x30u;
            if (xp1(w)) xp1(w)->flg &= ~0x2;
            w->flg &= ~0x80000u;
            s->h.wall = s->h.wall2;
            if (s->h.surface == 0) w->flg2 &= ~0x1u;
            else w->flg2 |= 0x1u;
            s->climb_timer = 20;
        }
        break;
    default: gap(w, "bhEne03_MV05: mode3 ausserhalb"); break;
    }
}

/* en03.c:1619 MV06 — Sprung auf die Flaeche EXP0_I(100). */
static void mv06(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    NJS_VECTOR up, forward, ov;
    float out;
    up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
    /* en03.c:1646: Kreuzprodukt mit EXP0_F(32..40), nicht der
       Hochachse EXP0_F(16..24), die erst das Vorzeichen bestimmt. */
    forward.x = s->h.m[8]; forward.y = s->h.m[9]; forward.z = s->h.m[10];
    switch (w->mode3) {
    case 0:
        if (xp1(w)) xp1(w)->flg |= 0x2;
        w->flg |= 0x80000u;
        if (w->mtn_no != 1) {
            w->mtn_no = 1; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
        }
        w->ct0 = 12;
        w->mode3++;
        /* faellt durch */
    case 1: {
        NJS_VECTOR dir;
        dir.x = s->dir.x; dir.y = s->dir.y; dir.z = s->dir.z;
        if (w->ct0 != 0) {
            out = njOuterProduct(&forward, &dir, &ov);
            if (njInnerProduct(&up, &ov) > 0) w->ayp = (int)(10430.381f * asinf(out)) / w->ct0;
            else w->ayp = -(int)(10430.381f * asinf(out)) / w->ct0;
            njRotateY(bm(w), (Angle)(int16_t)w->ayp);
            w->px += (s->h.target.x - w->px) / (float)w->ct0;
            w->py += (s->h.target.y - w->py) / (float)w->ct0;
            w->pz += (s->h.target.z - w->pz) / (float)w->ct0;
            w->ct0--;
            break;
        }
        w->mode1 = 0;
        w->mode2 = 5;
        w->mode3 = 0;
        if ((s->h.surface == 0) && (w->type == 1)) w->mode2 = 18;
        break;
    }
    default: gap(w, "bhEne03_MV06: mode3 ausserhalb"); break;
    }
}

/* en03.c:1683 MV07 — Anlegen an die Flaeche (Gegenstueck zu MV05). */
static void mv07(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = (w->type == 0) ? 3u : 33u;
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->flg &= ~0x10u;
        w->flg |= 0x1800000u;
        w->flg |= 0x300000u;
        w->flg2 |= 0x1u;
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) {
            RECVX_ENEMY_VEC pos;
            RECVX_T3_ATR hp;
            float h;
            if (!parts(w, 0, &pos)) return;
            w->px = pos.x; w->py = pos.y; w->pz = pos.z;
            njRotateX(bm(w), -16384);
            if (!wall_record(w, s->h.wall2, &hp)) return;
            switch (s->h.surface2) {
            case 0:
                if (hp.type == 0) {
                    h = hp.h;
                    if (h == 0) { if (!room_h(w, &h)) return; }
                    w->py = hp.py + h;
                } else {
                    w->py = hp.py;
                }
                break;
            case 1:
                if (hp.type == 0) w->py = hp.py;
                else w->py = hp.py + hp.h;
                break;
            case 5: w->pz = hp.pz + hp.d; break;
            case 3: w->pz = hp.pz; break;
            case 4: w->px = hp.px; break;
            case 2: w->px = hp.px + hp.w; break;
            default: gap(w, "bhEne03_MV07: EXP0_C(106) ausserhalb"); break;
            }
            w->pxb = w->px; w->pyb = w->py; w->pzb = w->pz;
            s->h.surface = s->h.surface2;
            recvx_en03_make_matrix(bd(w));
            w->flg &= ~0x1800000u;
            w->flg &= ~0x300000u;
            w->mode1 = 1; w->mode2 = 1; w->mode3 = 1;
            w->mtn_no = 1;
            w->ct0 = 10;
            w->spd = 0.4f;
            recvx_en03_go_ahead(w, bd(w));
            w->flg |= 0x30u;
            if (xp1(w)) xp1(w)->flg &= ~0x2;
            w->flg &= ~0x80000u;
            s->h.wall = s->h.wall2;
            if (s->h.surface == 0) w->flg2 &= ~0x1u;
            else w->flg2 |= 0x1u;
            s->climb_timer = 20;
        }
        break;
    default: gap(w, "bhEne03_MV07: mode3 ausserhalb"); break;
    }
}

/* en03.c:1818 MV08 — Sprung auf die Flaeche. */
static void mv08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    NJS_VECTOR vd, ov, up;
    float out;
    switch (w->mode3) {
    case 0:
        if (xp1(w)) xp1(w)->flg |= 0x2;
        w->flg |= 0x80000u;
        if (w->mtn_no != 1) {
            w->mtn_no = 1; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
        }
        w->ct0 = 12;
        w->mode3++;
        /* faellt durch */
    case 1:
        up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
        if (w->ct0 != 0) {
            NJS_VECTOR dir;
            vd.x = -s->h.m[8]; vd.y = -s->h.m[9]; vd.z = -s->h.m[10];
            dir.x = s->dir.x; dir.y = s->dir.y; dir.z = s->dir.z;
            out = njOuterProduct(&vd, &dir, &ov);
            if (njInnerProduct(&up, &ov) > 0) w->ayp = (int)(10430.381f * asinf(out)) / w->ct0;
            else w->ayp = -(int)(10430.381f * asinf(out)) / w->ct0;
            njRotateY(bm(w), (Angle)(int16_t)w->ayp);
            w->px += (s->h.target.x - w->px) / (float)w->ct0;
            w->py += (s->h.target.y - w->py) / (float)w->ct0;
            w->pz += (s->h.target.z - w->pz) / (float)w->ct0;
            w->ct0--;
            break;
        }
        w->mode1 = 0;
        w->mode2 = 7;
        w->mode3 = 0;
        break;
    default: gap(w, "bhEne03_MV08: mode3 ausserhalb"); break;
    }
}

/* en03.c:1880 MV09 — Angriff aus der Entfernung. */
static void mv09(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = (w->type == 0) ? 11u : 57u;
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) {
            if (w->type == 0) { w->mode0 = 1; w->mode1 = 1; w->mode2 = 3; w->mode3 = 0; }
            else { w->mode0 = 1; w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        }
        if (w->type == 0) {
            if (w->frm_no >= 2818048 && w->frm_no <= 3014656) en03_acid(w, 0);
        } else if ((w->frm_no >= 983040 && w->frm_no <= 1179648) ||
                   (w->frm_no >= 1441792 && w->frm_no <= 1638400) ||
                   (w->frm_no >= 1835008 && w->frm_no <= 1966080)) {
            en03_acid(w, 0);
        }
        break;
    default: gap(w, "bhEne03_MV09: mode3 ausserhalb"); break;
    }
}

/* en03.c:1938 MV10 — zweiter Angriffszweig. */
static void mv10(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 12; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) { w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
        if (w->frm_no >= 3407872 && w->frm_no <= 3604480) en03_acid(w, 1);
        break;
    default: gap(w, "bhEne03_MV10: mode3 ausserhalb"); break;
    }
}

/* en03.c:1971 MV11 — Sturzangriff. */
static void mv11(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_VEC pos;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 13; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        s->h.vel.x = 0.0f; s->h.vel.y = -5.0f; s->h.vel.z = 0.0f;
        w->flg |= 0x1800000u;
        if (xp1(w)) xp1(w)->flg |= 0x1;
        w->flg &= ~0x20u;
        s->h.wall = -1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) {
            if (!parts(w, 0, &pos)) return;
            w->px = pos.x; w->py = pos.y; w->pz = pos.z;
            w->mtn_no = 14; w->frm_no = 0;
            w->hokan_count = 0;
            w->mode3++;
            s->h.surface = 0;
            njRotateY(bm(w), 32768);
            recvx_en03_make_matrix(bd(w));
            w->flg |= 0x20u;
        }
        break;
    case 2:
        if (s->h.wall >= 0) {
            RECVX_T3_ATR hp;
            float h;
            w->flg &= ~0x1800000u;
            w->mtn_no = 15; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            w->mode3++;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
            if (!wall_record(w, s->h.wall, &hp)) return;
            if (hp.type == 7) { w->py = hp.py; break; }
            h = hp.h;
            if (h == 0) { if (!room_h(w, &h)) return; }
            w->py = hp.py + h;
            break;
        }
        w->py += s->h.vel.y;
        s->h.vel.y -= 0.33f;
        break;
    case 3:
        if (w->ct0-- == 0) {
            if (xp1(w)) xp1(w)->flg &= ~0x1;
            w->mtn_no = 0; w->frm_no = 0;
            w->mode1 = 1; w->mode2 = 3; w->mode3 = 0;
            w->flg2 &= ~0x1u;
        }
        break;
    default: gap(w, "bhEne03_MV11: mode3 ausserhalb"); break;
    }
}

/* en03.c:2087/2093 MV12 und MV13 sind leere Originale. */
static void mv12(RECVX_ENEMY_WORK* w) { (void)w; }
static void mv13(RECVX_ENEMY_WORK* w) { (void)w; }

/* en03.c:2099 MV14 — Absprung mit Parabel. */
static void mv14(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    NJS_VECTOR v = { 0.0f, 3.0f, -1.3f };
    switch (w->mode3) {
    case 0:
        w->mtn_no = 41; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
        w->mode3++;
        /* faellt durch */
    case 1:
        w->ct0--;
        if ((w->frm_no / 65536) == 12) {
            njCalcVector(bm(w), &v, (NJS_VECTOR*)&s->h.vel);
            s->h.wall = -1;
            w->mode3++;
        }
        break;
    case 2:
        if (w->ct0-- == 0) { w->mtn_add = 0; w->mode3++; }
        /* faellt durch */
    case 3:
        if (s->h.vel.y < 0.0f) {
            w->mtn_no = 49; w->frm_no = 0;
            w->hokan_count = 8; w->hokan_rate = 45875;
            w->mtn_add = 0;
            w->mode3++;
        }
        /* faellt durch */
    case 4:
        if (s->h.wall >= 0) {
            RECVX_T3_ATR hp;
            float h;
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->mtn_no = 49; w->frm_no = 0;
            w->hokan_count = 3; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            if (!wall_record(w, s->h.wall, &hp)) return;
            if (hp.type == 7) {
                w->py = hp.py;
            } else {
                h = hp.h;
                if (h == 0) { if (!room_h(w, &h)) return; }
                w->py = hp.py + h;
            }
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3 = 5;
            break;
        }
        w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
        s->h.vel.y -= 0.33f;
        break;
    case 5:
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 3; w->mode3 = 0; }
        break;
    default: gap(w, "bhEne03_MV14: mode3 ausserhalb"); break;
    }
}

/* en03.c:2208 MV15 — Absprung mit Aufrichten am Ziel. */
static void mv15(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    NJS_VECTOR vwork = { 0.0f, 0.8f, 0.0f }, ov, up, dir;
    float out;
    up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
    switch (w->mode3) {
    case 0:
        w->mtn_no = 41; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if ((w->frm_no / 65536) == 12) {
            njCalcVector(bm(w), &vwork, (NJS_VECTOR*)&s->h.vel);
            s->h.wall = -1;
            s->dir.x = 0.0f; s->dir.y = 1.0f; s->dir.z = 0.0f;
            w->mode3++;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 48; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            w->mode3++;
        }
        /* faellt durch */
    case 3:
        dir.x = s->dir.x; dir.y = s->dir.y; dir.z = s->dir.z;
        out = njOuterProduct(&up, &dir, &ov);
        if (out > 0.0f) {
            NJS_MATRIX r;
            njUnitVector(&ov);
            njUnitMatrix(&r);
            njRotate(&r, &ov, (Angle)((int)(10430.381f * asinf(out)) / 8));
            njMultiMatrix(&r, bm(w));
            memcpy(bm(w), r, sizeof r);
        }
        if (s->h.wall >= 0) {
            RECVX_T3_ATR hp;
            float h;
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->mtn_no = 49; w->frm_no = 0;
            w->hokan_count = 3; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            if (!wall_record(w, s->h.wall, &hp)) return;
            if (hp.type == 7) w->py = hp.py;
            else {
                h = hp.h;
                if (h == 0) { if (!room_h(w, &h)) return; }
                w->py = hp.py + h;
            }
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3 = 4;
            break;
        }
        w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
        s->h.vel.y -= 0.33f;
        break;
    case 4:
        if (w->ct0-- == 0) { w->mode1 = 1; w->mode2 = 3; w->mode3 = 0; w->flg2 &= ~0x1u; }
        break;
    default: gap(w, "bhEne03_MV15: mode3 ausserhalb"); break;
    }
}

/* en03.c:2327 MV16 — Rueckwaertssturz. */
static void mv16(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = 47; w->frm_no = 0;
        w->hokan_count = 3; w->hokan_rate = 32768;
        s->h.vel.x = 0.0f; s->h.vel.y = -1.0f; s->h.vel.z = 0.0f;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->ct1 = 8;
        s->h.wall = -1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) {
            w->mtn_add = 0;
            w->frm_no = (int32_t)(65536.0f * (float)((int32_t)frames(w, w->mtn_no) - 1));
        }
        if (w->ct1 > 0) {
            w->ct1--;
            njRotateX(bm(w), 4004);
        }
        if (s->h.wall >= 0) {
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
            w->mtn_add = 65536;
            w->flg2 &= ~0x1u;
            break;
        }
        w->py += s->h.vel.y;
        s->h.vel.y -= 0.33f;
        break;
    default: gap(w, "bhEne03_MV16: mode3 ausserhalb"); break;
    }
}

/* en03.c:2387 MV17 — Verfolgung mit erhoehtem Eindrehwinkel. */
static void mv17(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    int r;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 37; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        if (!rnd_mod(w, 47, &r)) return;
        w->ct0 = r;
        w->mode3++;
        w->spd = 0.6f;
        if (s->h.seen == 1) {
            w->ct1 = (s->h.ang < 0) ? -1 : 1;
        } else {
            float f;
            if (!rnd(w, &f)) return;
            w->ct1 = (f < 0.5f) ? -1 : 1;
        }
        /* faellt durch */
    case 1: {
        RECVX_ENEMY_VEC from, pl, out;
        RECVX_ENEMY_PLAYER p;
        int now;
        recvx_en03_go_ahead(w, bd(w));
        if (check_climb_wall() != 0) { w->mode1 = 0; w->mode2 = 6; w->mode3 = 0; break; }
        if (check_climb_down_wall() != 0) { w->mode1 = 0; w->mode2 = 8; w->mode3 = 0; break; }
        if (!player(w, &p)) return;
        from.x = w->px; from.y = w->py; from.z = w->pz;
        pl = p.pos;
        if (!route(w, &from, &pl, &out, &now)) return;
        if (now != 0xFF) { s->h.target.x = out.x; s->h.target.z = out.z; }
        else { s->h.target.x = p.pos.x; s->h.target.z = p.pos.z; }
        if ((avoid_wall() == 0) && (s->h.seen == 1)) {
            w->ayp = recvx_en03_dir_target_body(w, bd(w), &s->h.target, 1547);
            njRotateY(bm(w), (Angle)(int16_t)w->ayp);
        }
        if (w->ct0-- == 0) { w->mode1 = 0; w->mode2 = 1; w->mode3 = 0; }
        break;
    }
    default: gap(w, "bhEne03_MV17: mode3 ausserhalb"); break;
    }
}

/* en03.c:2463 MV18 — Rueckzug auf die Flaeche EXP0_I(100). */
static void mv18(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    NJS_VECTOR v, ov, up, dir;
    float out;
    up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
    switch (w->mode3) {
    case 0:
        w->mtn_no = 41; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->flg2 |= 0x1u;
        w->mode3++;
        break;
    case 1:
        if ((w->frm_no / 65536) == 12) {
            v.x = 0.0f; v.y = 3.0f; v.z = -0.9f;
            njCalcVector(bm(w), &v, (NJS_VECTOR*)&s->h.vel);
            w->flg &= ~0x10u;
            s->h.wall = -1;
            w->mode3++;
        }
        break;
    case 2:
        if (w->frm_no == 0) {
            w->mtn_no = 48;
            w->hokan_count = 10; w->hokan_rate = 32768;
            w->ct0 = 9;
            w->mode3++;
        }
        break;
    case 3:
        dir.x = s->dir.x; dir.y = s->dir.y; dir.z = s->dir.z;
        out = njOuterProduct(&up, &dir, &ov);
        if (out > 0.0f) {
            NJS_MATRIX r;
            njUnitVector(&ov);
            njUnitMatrix(&r);
            njRotate(&r, &ov, (Angle)((int)(10430.381f * asinf(out)) / 8));
            njMultiMatrix(&r, bm(w));
            memcpy(bm(w), r, sizeof r);
        }
        if (w->ct0-- == 0) {
            s->h.wall = s->h.wall2;
            s->h.surface = s->h.surface2;
            recvx_en03_make_matrix(bd(w));
            w->mtn_no = 49; w->frm_no = 0;
            w->hokan_count = 3; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3 = 4;
            break;
        }
        w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
        s->h.vel.y -= 0.33f;
        break;
    case 4:
        if (w->ct0-- == 0) {
            w->flg |= 0x30u;
            if (xp1(w)) xp1(w)->flg &= ~0x2;
            w->flg &= ~0x80000u;
            w->mode1 = 1; w->mode2 = 3; w->mode3 = 0;
            s->climb_timer = 20;
        }
        break;
    default: gap(w, "bhEne03_MV18: mode3 ausserhalb"); break;
    }
}

/* en03.c:2576 MV19 — Anschleichen und Nage-Uebergang. */
static void mv19(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    float dist;
    RECVX_ENEMY_VEC p1;
    NJS_POINT3 p2;
    switch (w->mode3) {
    case 0:
        w->mtn_no = (w->type == 0) ? 28u : 43u;
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->ar = 0.1f;
        w->mode3++;
        break;
    case 1: {
        RECVX_ENEMY_PLAYER p;
        if (w->type == 0) recvx_en03_add_null_trans(w, bd(w), spm_028, 13);
        else recvx_en03_add_null_trans(w, bd(w), spmh_013, 10);
        if (!player(w, &p)) return;
        if (w->frm_no == 0) {
            dist = njSqrt(((p.pos.x - w->px) * (p.pos.x - w->px)) +
                          ((p.pos.z - w->pz) * (p.pos.z - w->pz)));
            w->ay = recvx_t3_arc_tan2(-s->h.m[2], s->h.m[0]);
            if (!parts(w, 0, &p1)) return;
            p2.x = p.pos.x; p2.y = 10.0f + p.pos.y; p2.z = p.pos.z;
            if (w->type == 0) {
                if (recvx_t3_check_event_atr(p.flr_no, p.pos.x, p.pos.z, 3, 0, 0) == 0 &&
                    dist < 10.0f &&
                    recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 9102) != 0 &&
                    fabsf(w->py - p.pos.y) < 0.01f &&
                    line_hit(w, &p1,
                             &(RECVX_ENEMY_VEC){ p2.x, p2.y, p2.z }) == 0) {
                    set_modes(w, 2, 0, 0, 0);
                    nage(w);
                    break;
                }
                w->mtn_no = 29;
                w->mode3++;
                break;
            }
            if (recvx_t3_check_event_atr(p.flr_no, p.pos.x, p.pos.z, 3, 0, 0) == 0 &&
                dist < 20.0f &&
                recvx_t3_check_dir_target(w, p.pos.x, p.pos.z, 5461) != 0 &&
                fabsf(w->py - p.pos.y) < 0.01f &&
                line_hit(w, &p1,
                         &(RECVX_ENEMY_VEC){ p2.x, p2.y, p2.z }) == 0) {
                set_modes(w, 2, 0, 1, 0);
                nage(w);
                break;
            }
            w->mtn_no = 45;
            s->h.wall = -1;
            w->mode3++;
        }
        break;
    }
    case 2:
        if (w->type == 0) recvx_en03_add_null_trans(w, bd(w), spm_029, 20);
        else recvx_en03_add_null_trans(w, bd(w), spmh_015, 30);
        w->ar += (6.0f - w->ar) / 16.0f;
        if (w->frm_no == 0) {
            w->mtn_no = 0; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            w->mode1 = 1; w->mode2 = 0; w->mode3 = 0;
            w->ar = 6.0f;
            if (w->type == 1) get_wall(w);
        }
        break;
    default: gap(w, "bhEne03_MV19: mode3 ausserhalb"); break;
    }
}

/* --- Nage (en03.c:2701-3211) ------------------------------------------------ */
/* Die beiden trg_pos[4][2] des Originals liegen zusammenhaengend; ein zweiter
   Index > 1 liest die naechste Zeile (en03.c:2873/:2944 mit `& 1` bleibt
   unkritisch, :2931/:3105/:3142/:3205 mit mode2 bzw. mode2-2 bis 3 nicht).
   Hinter flat 8 liegt fremder Stapelspeicher; das wird als belegte
   Original-UB gemeldet und nicht nachgebildet. */
static const RECVX_ENEMY_VEC ng00_trg[8] = {
    { 2.1e-05f, -2.618032f, -6.719222f }, { 2.1e-05f, -2.61543f, -6.687998f },
    { 2.1e-05f, -2.618032f, -6.719222f }, { 2.1e-05f, -2.61543f, -6.687998f },
    { 2.1e-05f, -2.618032f, -6.719222f }, { 2.1e-05f, -2.61543f, -6.687998f },
    { 2.1e-05f, -2.618032f, -6.719222f }, { 2.1e-05f, -2.61543f, -6.687998f }
};
static const RECVX_ENEMY_VEC ng01_trg[8] = {
    { 0.063656f, -0.090291f, -12.650927f }, { 0.063656f, -0.090291f, -11.614147f },
    { 0.063656f, -0.090291f, -13.150927f }, { 0.063656f, -0.090291f, -12.114147f },
    { 0.063656f, -0.090291f, -12.650927f }, { 0.063656f, -0.090291f, -11.614147f },
    { 0.063656f, -0.090291f, -13.150927f }, { 0.063656f, -0.090291f, -12.114147f }
};
static int trg_at(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* tab, int row, int col,
                  RECVX_ENEMY_VEC* out)
{
    const int flat = (row * 2) + col;
    if (row < 0 || row > 3 || flat < 0 || flat >= 8) {
        gap(w, "bhEne03_NG00/NG01: trg_pos[...] hinter der Tabelle (Original liest fremden Speicher)");
        return 0;
    }
    *out = tab[flat];
    return 1;
}
/* (NJS_MATRIX)epw->exp0 auf einen Punkt dieses Works. */
static void local_point(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* in, RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m;
    NJS_POINT3 p, q;
    memcpy(m, bm(w), sizeof m);
    p.x = in->x; p.y = in->y; p.z = in->z;
    njCalcPoint(&m, &p, &q);
    out->x = q.x; out->y = q.y; out->z = q.z;
}
/* njUnitMatrix(NULL); njRotateY(NULL, ayp); njCalcVector(NULL, v, out). */
static void ayp_rotate_vector(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* v, RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m;
    NJS_VECTOR a, b;
    njUnitMatrix(&m);
    njRotateY(&m, (Angle)(int16_t)w->ayp);
    a.x = v->x; a.y = v->y; a.z = v->z;
    njCalcVector(&m, &a, &b);
    out->x = b.x; out->y = b.y; out->z = b.z;
}
static void nage(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) {
    case 0: ng00(w); break;
    case 1: ng01(w); break;
    default: gap(w, "bhEne03_NageMode2[mode2]"); break;
    }
}

/* en03.c:2709 NG00 — Griff am Boden. */
static void ng00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC pos;
    switch (w->mode3) {
    case 0: {
        if (!player(w, &p)) return;
        w->ayp = recvx_t3_arc_tan2(w->px - p.pos.x, w->pz - p.pos.z);
        if (xp1(w)) xp1(w)->flg |= 0x4;
        p.flg |= 0x10004u;
        p.stflg |= 0x10000u;
        p.mode0 = 4; p.mode3 = 0;
        if (abs((int16_t)(w->ayp - p.ay)) < 16384) {
            p.mode2 = 1;
            p.ayp = w->ayp;
        } else {
            p.mode2 = 0;
            p.ayp = (int16_t)(w->ayp + 32768);
        }
        put_player(w, &p);
        w->mtn_no = 8; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->flg &= ~0x60u;
        w->flg |= 0x100000u;
        w->flg &= ~0x200000u;
        w->flg2 |= 0x1u;
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        w->ct1 = 0;
        w->ct2 = 6;
        w->mode3++;
        /* faellt durch */
    }
    case 1:
        w->ct1 += recvx_t3_lever_check(w);
        if ((w->ct0-- == 0) || (w->ct1 > 30)) {
            w->mtn_no = 9; w->frm_no = 0;
            w->hokan_count = 0;
            w->ct0 = 0;
            w->mode3++;
        }
        break;
    case 2:
        w->ct1 += recvx_t3_lever_check(w);
        if (!player(w, &p)) return;
        if (w->frm_no == 0) {
            if (p.hp < 0) {
                p.mode0 = 6;
                p.mode2 = (p.mode2 == 0) ? 0 : 1;
                p.mode3 = 0;
                put_player(w, &p);
                w->mtn_no = 25; w->frm_no = 0;
                w->hokan_count = 0;
                w->mode3 = 4;
                return;
            }
            w->ct0++;
            if ((w->ct0 == 3) || (w->ct1 > 30)) {
                p.mode2 = (p.mode2 == 0) ? 4 : 5;
                p.mode3 = 0;
                put_player(w, &p);
                w->mtn_no = 10; w->frm_no = 0;
                w->hokan_count = 0;
                w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
                w->mode3++;
                return;
            }
        }
        if ((w->frm_no / 65536) == 19) {
            NJS_MATRIX m;
            RECVX_ENEMY_VEC at;
            p.hp -= 12;
            if (p.hp < 0) p.stflg |= 0x40000u;
            player_set_dv(w, -njSin((Angle)w->ay), -1.0f, -njCos((Angle)w->ay));
            put_player(w, &p);
            if (!joint(w, 35, &m)) return;
            at.x = m[12]; at.y = m[13]; at.z = m[14];
            if (w->sv->blood_effect_burst) w->sv->blood_effect_burst(w, 4, 4, &at, 1);
            else gap(w, "bhEne_SetBloodEffectBurst (en03)");
            if (w->sv->vibration) w->sv->vibration(1, 9);
            else gap(w, "StartVibrationEx (en03)");
        }
        break;
    case 3:
        if (!player(w, &p)) return;
        w->ar += (6.0f - w->ar) / 16.0f;
        if (!trg_at(w, ng00_trg, (int)p.ply_id, (int)(p.mode2 & 0x1u), &pos)) return;
        ayp_rotate_vector(w, &pos, &pos);
        w->px = p.pos.x - pos.x;
        w->pz = p.pos.z - pos.z;
        if (w->ct0-- == 0) {
            w->flg |= 0x20u;
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
            w->mtn_no = 0; w->frm_no = 0;
            w->hokan_count = 0;
            if (!parts(w, 0, &pos)) return;
            w->px = pos.x;
            w->pz = pos.z;
            w->flg |= 0x60u;
            w->flg2 &= ~0x1u;
            w->ar = 6.0f;
            if (xp1(w)) xp1(w)->timer += 30;
        }
        break;
    case 4:
        if (!player(w, &p)) return;
        if (w->frm_no == 0) {
            w->frm_no = 2031616;
            p.flg |= 0x2u;
            put_player(w, &p);
        }
        if (((w->frm_no / 65536) == 32) || ((w->frm_no / 65536) == 51)) {
            NJS_MATRIX m;
            RECVX_ENEMY_VEC at;
            player_set_dv(w, njSin((Angle)w->ay), -2.0f, njCos((Angle)w->ay));
            put_player(w, &p);
            if (!joint(w, 35, &m)) return;
            at.x = m[12]; at.y = m[13]; at.z = m[14];
            if (w->sv->blood_effect_burst) w->sv->blood_effect_burst(w, 4, 4, &at, 1);
            else gap(w, "bhEne_SetBloodEffectBurst (en03)");
            if (w->sv->vibration) w->sv->vibration(1, 9);
            else gap(w, "StartVibrationEx (en03)");
        }
        w->ar += (6.0f - w->ar) / 16.0f;
        if (!trg_at(w, ng00_trg, (int)p.ply_id, (int)p.mode2, &pos)) return;
        ayp_rotate_vector(w, &pos, &pos);
        p.pos.x = w->px + pos.x;
        p.pos.z = w->pz + pos.z;
        put_player(w, &p);
        break;
    default: gap(w, "bhEne03_NG00: mode3 ausserhalb"); break;
    }

    if (w->mode3 == 1 || w->mode3 == 2) {
        RECVX_ENEMY_VEC pos2;
        if (!player(w, &p)) return;
        if (!trg_at(w, ng00_trg, (int)p.ply_id, (int)(p.mode2 & 0x1u), &pos2)) return;
        ayp_rotate_vector(w, &pos2, &pos2);
        w->xn = p.pos.x - pos2.x;
        w->zn = p.pos.z - pos2.z;
        if (w->ct2 > 0) {
            w->px += (w->xn - w->px) / (float)w->ct2;
            w->pz += (w->zn - w->pz) / (float)w->ct2;
            w->ay = recvx_t3_arc_tan2(-s->h.m[2], s->h.m[0]);
            njRotateY(bm(w), (Angle)(int16_t)((int16_t)(w->ayp - w->ay) / w->ct2));
            w->ct2--;
        } else {
            w->px = w->xn;
            w->pz = w->zn;
        }
    }
}

/* en03.c:2969 NG01 — Griff an Decke/Wand. */
static void ng01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC pos;
    switch (w->mode3) {
    case 0:
        if (!player(w, &p)) return;
        w->ayp = recvx_t3_arc_tan2(w->px - p.pos.x, w->pz - p.pos.z);
        if (xp1(w)) xp1(w)->flg |= 0x4;
        p.flg |= 0x10004u;
        p.stflg |= 0x10000u;
        p.mode0 = 4; p.mode3 = 0;
        if (abs((int16_t)(w->ayp - p.ay)) < 16384) {
            p.mode2 = 3;
            p.ayp = w->ayp;
        } else {
            p.mode2 = 2;
            p.ayp = (int16_t)(w->ayp + 32768);
        }
        put_player(w, &p);
        w->mtn_no = 44; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->flg &= ~0x60u;
        w->flg |= 0x100000u;
        w->flg &= ~0x200000u;
        w->flg2 |= 0x1u;
        w->ct0 = (int32_t)frames(w, w->mtn_no);
        w->ct1 = 0;
        w->ct2 = 6;
        w->stflg |= 0x8u;
        w->mode3++;
        /* faellt durch */
    case 1:
        w->ct1 += recvx_t3_lever_check(w);
        if (w->ct0-- == 0) {
            w->stflg &= ~0x8u;
            if (!player(w, &p)) return;
            if (p.hp < 0) {
                p.mode0 = 6;
                p.mode2 = (p.mode2 == 2) ? 2 : 3;
                p.mode3 = 0;
                put_player(w, &p);
                w->mtn_no = 55; w->frm_no = 0;
                w->hokan_count = 0;
                w->mode3 = 3;
            } else {
                w->mtn_no = 46; w->frm_no = 0;
                w->hokan_count = 0;
                w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
                w->mode3++;
            }
            return;
        }
        if (!player(w, &p)) return;
        if (!trg_at(w, ng01_trg, (int)p.ply_id, (int)(p.mode2 - 2), &pos)) return;
        ayp_rotate_vector(w, &pos, &pos);
        w->xn = p.pos.x - pos.x;
        w->zn = p.pos.z - pos.z;
        if (w->ct2 > 0) {
            w->px += (w->xn - w->px) / (float)w->ct2;
            w->pz += (w->zn - w->pz) / (float)w->ct2;
            w->ay = recvx_t3_arc_tan2(-s->h.m[2], s->h.m[0]);
            njRotateY(bm(w), (Angle)(int16_t)((int16_t)(w->ayp - w->ay) / w->ct2));
            w->ct2--;
        } else {
            w->px = w->xn;
            w->pz = w->zn;
        }
        if ((w->frm_no == 720896) || (w->frm_no == 1900544) || (w->frm_no == 3342336)) {
            NJS_MATRIX m;
            RECVX_ENEMY_VEC at;
            player_set_dv(w, -njSin((Angle)w->ay), -0.5f, -njCos((Angle)w->ay));
            if (w->ct1 > 30) {
                p.hp -= 5;
                put_player(w, &p);
                if (!joint(w, 35, &m)) return;
                at.x = m[12]; at.y = m[13]; at.z = m[14];
                if (w->sv->blood_effect_burst) w->sv->blood_effect_burst(w, 5, 4, &at, 1);
                else gap(w, "bhEne_SetBloodEffectBurst (en03)");
            } else {
                p.hp -= 12;
                put_player(w, &p);
                if (!joint(w, 35, &m)) return;
                at.x = m[12]; at.y = m[13]; at.z = m[14];
                if (w->sv->blood_effect_burst) w->sv->blood_effect_burst(w, 4, 4, &at, 1);
                else gap(w, "bhEne_SetBloodEffectBurst (en03)");
            }
            if (!player(w, &p)) return;
            if (p.hp < 0) {
                p.stflg |= 0x40000u;
                put_player(w, &p);
            }
            if (w->sv->vibration) w->sv->vibration(1, 9);
            else gap(w, "StartVibrationEx (en03)");
        }
        break;
    case 2: {
        RECVX_ENEMY_VEC pos2;
        if (!player(w, &p)) return;
        w->ar += (6.0f - w->ar) / 16.0f;
        if (!trg_at(w, ng01_trg, (int)p.ply_id, (int)(p.mode2 - 2), &pos2)) return;
        ayp_rotate_vector(w, &pos2, &pos2);
        w->xn = p.pos.x - pos2.x;
        w->zn = p.pos.z - pos2.z;
        if (w->ct0-- == 0) {
            w->flg |= 0x20u;
            w->mode0 = 1; w->mode1 = 1; w->mode2 = 1; w->mode3 = 0;
            if (!parts(w, 0, &pos)) return;
            w->px = pos.x;
            w->pz = pos.z;
            w->mtn_no = 30; w->frm_no = 0;
            w->hokan_count = 0;
            w->flg |= 0x60u;
            w->flg2 &= ~0x1u;
            w->ar = 6.0f;
            w->flg &= ~0x100000u;
            if (xp1(w)) xp1(w)->timer += 20;
        }
        break;
    }
    case 3: {
        RECVX_ENEMY_VEC pos2;
        if (!player(w, &p)) return;
        if (w->frm_no == 0) w->frm_no = 11206656;
        if (w->frm_no == 3932160) {
            p.flg |= 0x2u;
            put_player(w, &p);
        }
        if (((w->frm_no / 65536) == 183) || ((w->frm_no / 65536) == 194) ||
            ((w->frm_no / 65536) == 210)) {
            NJS_MATRIX m;
            RECVX_ENEMY_VEC at;
            player_set_dv(w, njSin((Angle)w->ay), -2.0f, njCos((Angle)w->ay));
            put_player(w, &p);
            if (!joint(w, 35, &m)) return;
            at.x = m[12]; at.y = m[13]; at.z = m[14];
            if (w->sv->blood_effect_burst) w->sv->blood_effect_burst(w, 4, 4, &at, 1);
            else gap(w, "bhEne_SetBloodEffectBurst (en03)");
            if (w->sv->vibration) w->sv->vibration(1, 9);
            else gap(w, "StartVibrationEx (en03)");
        }
        if (!player(w, &p)) return;
        w->ar += (6.0f - w->ar) / 16.0f;
        if (!trg_at(w, ng01_trg, (int)p.ply_id, (int)(p.mode2 - 2), &pos2)) return;
        ayp_rotate_vector(w, &pos2, &pos2);
        p.pos.x = w->px + pos2.x;
        p.pos.z = w->pz + pos2.z;
        put_player(w, &p);
        break;
    }
    default: gap(w, "bhEne03_NG01: mode3 ausserhalb"); break;
    }
}

/* --- Damage (en03.c:3216-4224) ---------------------------------------------- */
/* en03.c:3216 Damage — Schadensabwicklung und Weitergabe an DamageMode2. */
static void en03_damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    if ((w->flg & 0x4u)) {
        w->flg &= ~0x4u;
        recvx_t3_calc_damage(w, CombWepTbl, CombJointTbl);
        w->comb_flg &= ~0xCu;
        if (recvx_en03_dg_dir_check(w, bd(w)) != 0) w->comb_flg |= 0x8u;
        else w->comb_flg |= 0x4u;
        if ((w->wpnr_no != 16) || (w->flg2 & 0x4u) || (w->comb_pnt == 1)) {
            if (w->type == 0) {
                if ((w->comb_flg & 0x8u)) {
                    s->hp2 -= w->total_dam;
                } else {
                    RECVX_ENEMY_PLAYER p;
                    if (!player(w, &p)) return;
                    if ((s->h.surface == 0) && (!(p.at_flg & 0x8u))) s->hp2 -= w->total_dam;
                    w->hp -= w->total_dam;
                }
            } else {
                w->hp -= w->total_dam;
            }
            if ((w->wpnr_no != 17) || (w->flg2 & 0x4u)) en03_hit_mark(w);
        }
    }
    switch (w->mode2) {
    case  0: break;                                        /* DG00 ist leer */
    case  1: dg01(w); break;
    case  2: dg02(w); break;
    case  3: dg03(w); break;
    case  4: dg04(w); break;
    case  5: break;                                        /* DG05 ist leer */
    case  6: dg06(w); break;
    case  7: dg07(w); break;
    case  8: dg08(w); break;
    case  9: dg09(w); break;
    case 10: dg10(w); break;
    case 11: dg11(w); break;
    case 12: dg12(w); break;
    default: gap(w, "bhEne03_DamageMode2[mode2]"); break;
    }
}

/* en03.c:3275 DG01. */
static void dg01(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 20; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 3, 0); w->flg &= ~0x4u; }
        break;
    default: gap(w, "bhEne03_DG01: mode3 ausserhalb"); break;
    }
}

/* en03.c:3307 DG02 — Rueckstoss und Umkippen. */
static void dg02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0: {
        NJS_VECTOR trans, k;
        int32_t ang[3];
        w->mtn_no = 17; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 45875;
        w->mtn_add = 65536;
        s->h.vel.x = 0.0f; s->h.vel.y = -1.0f; s->h.vel.z = 0.0f;
        s->h.surface = 0;
        recvx_en03_make_matrix(bd(w));
        njRotateY(bm(w), 32768);
        if (!root_pos(w, w->mtn_no, 0, &k)) return;
        njCalcVector(bm(w), &k, &trans);
        w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
        w->mtn_md |= 0x100u;
        if (!root_ang(w, w->mtn_no, 0, ang)) return;
        if (!recvx_en03_bind_root_angles(w, ang)) return;
        s->h.wall = -1;
        w->ar = 0.1f;
        w->mode3++;
        /* faellt durch */
    }
    case 1:
        if (s->h.wall >= 0) {
            w->mtn_no = 18; w->frm_no = 0;
            w->hokan_count = 0; w->hokan_rate = 0;
            w->mtn_md &= ~0x100u;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
            w->flg2 &= ~0x1u;
            w->mode3++;
        } else {
            w->py += s->h.vel.y;
            s->h.vel.y -= 0.33f;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 16; w->frm_no = 0;
            w->hokan_count = 0; w->hokan_rate = 0;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3++;
        }
        break;
    case 3:
        recvx_en03_add_null_trans(w, bd(w), spm_016, 45);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 3, 0); w->flg &= ~0x4u; }
        break;
    default: gap(w, "bhEne03_DG02: mode3 ausserhalb"); break;
    }
    w->ar += (6.0f - w->ar) / 16.0f;
}

/* en03.c:3419 DG03. */
static void dg03(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 19; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 3, 0); w->flg &= ~0x4u; }
        break;
    default: gap(w, "bhEne03_DG03: mode3 ausserhalb"); break;
    }
}

/* en03.c:3451 DG04 — Bein bricht. */
static void dg04(RECVX_ENEMY_WORK* w)
{
    NJS_POINT3 pos, q;
    int i;
    switch (w->mode3) {
    case 0: {
        NJS_MATRIX m;
        w->mtn_no = 22; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->ct1 = 0;
        w->mode3++;
        if (!joint(w, 2, &m)) return;
        pos.x = 0.0f; pos.y = 3.0f; pos.z = 4.0f;
        njCalcPoint(&m, &pos, &q);
        if (w->sv->blood_effect4) {
            const RECVX_ENEMY_VEC v = { q.x, q.y, q.z };
            w->sv->blood_effect4(&v, &v, 1, 9, 2);
            w->sv->blood_effect4(&v, &v, 1, 9, 1);
        } else {
            gap(w, "bhEne_SetBloodEffect4 (en03)");
        }
        recvx_t3_set_bloodstain(w, 1, 2, NULL);
        for (i = 0; i < 6; ++i) {
            float a, b, c;
            if (!joint(w, 2, &m)) return;
            if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return;
            pos.x = (4.0f * a) - 2.0f;
            pos.y = (3.0f + (4.0f * b)) - 2.0f;
            pos.z = (4.0f + (6.0f * c)) - 3.0f;
            njCalcPoint(&m, &pos, &q);
            w->dpx = q.x; w->dpy = q.y; w->dpz = q.z;
            recvx_t3_set_mince_effect2(w, 258, 0.3f, 1);
            recvx_t3_set_mince_effect2(w, 259, 0.3f, 1);
        }
        w->type = 1;
        w->cpcl = CapColTabB;
        recvx_en03_hide_parts(w, 2u, 1);
        recvx_t3_erase_arrow(w, 2);
        /* faellt durch */
    }
    case 1:
        if (w->ct1 < 4) {
            NJS_MATRIX m;
            float a, b, c;
            if (!joint(w, 2, &m)) return;
            if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return;
            pos.x = (6.0f * a) - 3.0f;
            pos.y = (3.0f + (4.0f * b)) - 2.0f;
            pos.z = (4.0f + (4.0f * c)) - 2.0f;
            njCalcPoint(&m, &pos, &q);
            if (w->sv->blood_effect4) {
                const RECVX_ENEMY_VEC v = { q.x, q.y, q.z };
                w->sv->blood_effect4(&v, &v, 1, 9, 2);
            } else {
                gap(w, "bhEne_SetBloodEffect4 (en03)");
            }
            w->ct1++;
        }
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 3, 0); w->flg &= ~0x4u; }
        break;
    default: gap(w, "bhEne03_DG04: mode3 ausserhalb"); break;
    }
}

/* en03.c:3541 DG06. */
static void dg06(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 52; w->frm_no = 0;
        w->hokan_count = 4; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 0, 0); w->flg &= ~0x4u; }
        break;
    default: gap(w, "bhEne03_DG06: mode3 ausserhalb"); break;
    }
}

/* en03.c:3573 DG07 — Rueckstoss entlang der Hochachse. */
static void dg07(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = 50; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 0;
        s->h.vel.x = s->h.m[4]; s->h.vel.y = s->h.m[5]; s->h.vel.z = s->h.m[6];
        s->h.wall = -1;
        w->ct0 = 8;
        w->mode3++;
        /* faellt durch */
    case 1:
        if (w->ct0 > 0) {
            NJS_MATRIX r;
            w->ct0--;
            njUnitMatrix(&r);
            switch (s->h.surface) {
            case 5: njRotateX(&r, -2048); break;
            case 3: njRotateX(&r, 2048); break;
            case 4: njRotateZ(&r, -2048); break;
            case 2: njRotateZ(&r, 2048); break;
            case 1: njRotateX(&r, 4096); break;
            default: break;
            }
            njMultiMatrix(&r, bm(w));
            memcpy(bm(w), r, sizeof r);
        }
        if (s->h.wall >= 0) {
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->mtn_no = 49; w->frm_no = 0;
            w->hokan_count = 3; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->flg2 &= ~0x1u;
            w->mode3++;
            break;
        }
        w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
        s->h.vel.y -= 0.33f;
        break;
    case 2:
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 3, 0); w->flg &= ~0x4u; }
        break;
    default: gap(w, "bhEne03_DG07: mode3 ausserhalb"); break;
    }
}

/* en03.c:3670 DG08. */
static void dg08(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = 26; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        recvx_en03_add_null_trans(w, bd(w), spm_026, 16);
        if (w->ct0-- == 0) {
            w->mtn_no = 30; w->frm_no = 0;
            set_modes(w, 1, 0, 3, 0);
            w->flg &= ~0x4u;
        }
        break;
    default: gap(w, "bhEne03_DG08: mode3 ausserhalb"); break;
    }
    (void)s;
}

/* en03.c:3707 DG09. */
static void dg09(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = 56; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        recvx_en03_add_null_trans(w, bd(w), spmh_026, 16);
        if (w->ct0-- == 0) {
            w->mtn_no = 30; w->frm_no = 0;
            set_modes(w, 1, 0, 3, 0);
            w->flg &= ~0x4u;
        }
        break;
    default: gap(w, "bhEne03_DG09: mode3 ausserhalb"); break;
    }
    (void)s;
}

/* en03.c:3746 DG10 — Rueckstoss mit Aufrichten. */
static void dg10(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    NJS_VECTOR v, ov;
    float out;
    int ang;
    switch (w->mode3) {
    case 0:
        w->mtn_no = 56; w->frm_no = 0;
        w->mtn_add = 65536;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
        if ((w->flg & 0x80000u)) {
            if ((s->h.surface == 0) || (s->h.surface == 1)) {
                s->h.vel.x = s->dir.x; s->h.vel.y = s->dir.y; s->h.vel.z = s->dir.z;
            } else {
                s->h.vel.x = s->h.m[4]; s->h.vel.y = s->h.m[5]; s->h.vel.z = s->h.m[6];
            }
            w->flg |= 0x30u;
            w->flg &= ~0x80000u;
            w->flg &= ~0x1800000u;
            w->flg &= ~0x300000u;
        } else {
            s->h.vel.x = w->dvx; s->h.vel.y = w->dvy; s->h.vel.z = w->dvz;
            {
                NJS_VECTOR u = { s->h.vel.x, s->h.vel.y, s->h.vel.z };
                njUnitVector(&u);
                s->h.vel.x = u.x; s->h.vel.y = u.y; s->h.vel.z = u.z;
            }
            s->h.vel.x *= 2.0f;
            s->h.vel.z *= 2.0f;
        }
        w->ct1 = 8;
        w->mode3++;
        break;
    case 1: {
        NJS_VECTOR up;
        up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
        if (w->ct1 > 0) {
            v.x = 0.0f; v.y = 1.0f; v.z = 0.0f;
            out = njOuterProduct(&up, &v, &ov);
            if (out > 0.0f) {
                NJS_MATRIX r;
                njUnitVector(&ov);
                njUnitMatrix(&r);
                ang = (int)(10430.381f * asinf(out)) / w->ct1;
                njRotate(&r, &ov, (Angle)ang);
                njMultiMatrix(&r, bm(w));
                memcpy(bm(w), r, sizeof r);
            }
            w->ct1--;
        }
        if (w->ct0 == 0) w->mtn_add = 0;
        else w->ct0--;
        if (s->h.wall >= 0) {
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->flg2 &= ~0x1u;
            w->mode3++;
            break;
        }
        w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
        s->h.vel.y -= 0.33f;
        break;
    }
    case 2:
        recvx_en03_add_null_trans(w, bd(w), spmh_026, 16);
        if (w->ct0-- == 0) {
            set_modes(w, 1, 1, 3, 0);
            w->flg &= ~0x4u;
            w->mtn_no = 4; w->frm_no = 0;
            w->hokan_count = 5; w->hokan_rate = 32768;
            w->mtn_add = 65536;
        }
        break;
    default: gap(w, "bhEne03_DG10: mode3 ausserhalb"); break;
    }
}

/* en03.c:3881 DG11. */
static void dg11(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_VEC pos;
        int32_t ang[3];
        NJS_MATRIX m;
        if (!parts(w, 0, &pos)) return;
        w->px = pos.x; w->py = pos.y; w->pz = pos.z;
        if (!root_ang_frm(w, w->mtn_no, w->frm_no, ang)) return;
        njRotateXYZ(bm(w), (Angle)ang[0], (Angle)ang[1], (Angle)ang[2]);
        w->mtn_md |= 0x100u;
        w->mtn_no = 50; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 45875;
        w->mtn_add = 0;
        w->mtn_md &= ~0x2u;
        {
            int32_t a2[3];
            if (!root_ang(w, w->mtn_no, 0, a2)) return;
            if (!recvx_en03_bind_root_angles(w, a2)) return;
        }
        if ((s->h.surface == 0) || (s->h.surface == 1)) {
            s->h.vel.x = s->dir.x; s->h.vel.y = s->dir.y; s->h.vel.z = s->dir.z;
        } else {
            s->h.vel.x = s->h.m[4]; s->h.vel.y = s->h.m[5]; s->h.vel.z = s->h.m[6];
        }
        s->h.wall = -1;
        w->flg |= 0x30u;
        w->flg &= ~0x80000u;
        w->flg &= ~0x1800000u;
        w->flg &= ~0x300000u;
        w->ar = 3.0f;
        w->ct0 = 8;
        w->mode3++;
        (void)m;
        /* faellt durch */
    }
    case 1: {
        NJS_VECTOR up, v, ov;
        float out;
        int ang;
        up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
        if (w->ct0 > 0) {
            v.x = 0.0f; v.y = 1.0f; v.z = 0.0f;
            out = njOuterProduct(&up, &v, &ov);
            if (out > 0.0f) {
                NJS_MATRIX r;
                njUnitVector(&ov);
                njUnitMatrix(&r);
                ang = (int)(10430.381f * asinf(out));
                if (s->h.m[5] < 0.0f) ang = 32768 - ang;
                njRotate(&r, &ov, (Angle)(ang / w->ct0));
                njMultiMatrix(&r, bm(w));
                memcpy(bm(w), r, sizeof r);
            } else {
                NJS_MATRIX r;
                njUnitMatrix(&r);
                njRotateX(&r, (Angle)(32768 / w->ct0));
                njMultiMatrix(&r, bm(w));
                memcpy(bm(w), r, sizeof r);
            }
            w->ct0--;
        }
        if (s->h.wall >= 0) {
            RECVX_T3_ATR hp;
            float h;
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            if (!wall_record(w, s->h.wall, &hp)) return;
            if (hp.type == 7) w->py = hp.py;
            else {
                h = hp.h;
                if (h == 0) { if (!room_h(w, &h)) return; }
                w->py = hp.py + h;
            }
            w->mtn_no = 49; w->frm_no = 0;
            w->hokan_count = 3; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            w->mtn_md &= ~0x100u;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->flg2 &= ~0x1u;
            w->mode3++;
        } else {
            w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
            s->h.vel.y -= 0.33f;
        }
        break;
    }
    case 2:
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 3, 0); w->flg &= ~0x4u; w->ar = 6.0f; }
        break;
    default: gap(w, "bhEne03_DG11: mode3 ausserhalb"); break;
    }
    w->ar += (6.0f - w->ar) / 16.0f;
}

/* en03.c:4060 DG12. */
static void dg12(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_VEC pos;
        int32_t ang[3], a2[3];
        NJS_VECTOR trans, k;
        if (!parts(w, 0, &pos)) return;
        w->px = pos.x; w->py = pos.y; w->pz = pos.z;
        if (!root_ang_frm(w, w->mtn_no, w->frm_no, ang)) return;
        if ((w->mtn_md & 0x2u)) {
            njRotateXYZ(bm(w), (Angle)ang[0], (Angle)(-ang[1]), (Angle)(-ang[2]));
        } else {
            njRotateXYZ(bm(w), (Angle)ang[0], (Angle)ang[1], (Angle)ang[2]);
        }
        if ((w->flg & 0x80000u)) {
            s->h.vel.x = s->h.m[4]; s->h.vel.y = 0.0f; s->h.vel.z = s->h.m[6];
        } else {
            s->h.vel.x = 0.0f; s->h.vel.y = 0.0f; s->h.vel.z = 0.0f;
        }
        njRotateX(bm(w), 32768);
        w->mtn_md |= 0x100u;
        w->mtn_no = 18; w->frm_no = 0;
        w->mtn_add = 0;
        w->mtn_md &= ~0x2u;
        w->hokan_count = 30; w->hokan_rate = 45875;
        if (!root_ang(w, w->mtn_no, 0, a2)) return;
        if (!recvx_en03_bind_root_angles(w, a2)) return;
        if (!root_pos(w, w->mtn_no, 0, &k)) return;
        njCalcVector(bm(w), &k, &trans);
        w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
        s->h.wall = -1;
        w->flg |= 0x30u;
        w->flg &= ~0x80000u;
        w->flg &= ~0x1800000u;
        w->flg &= ~0x300000u;
        w->ct0 = 8;
        w->mode3++;
        /* faellt durch */
    }
    case 1: {
        NJS_VECTOR up, v, ov;
        float out;
        up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
        if (w->ct0 > 0) {
            v.x = 0.0f; v.y = 1.0f; v.z = 0.0f;
            out = njOuterProduct(&up, &v, &ov);
            if (out > 0.0f) {
                NJS_MATRIX r;
                njUnitVector(&ov);
                njUnitMatrix(&r);
                njRotate(&r, &ov, (Angle)((int)(10430.381f * asinf(out)) / w->ct0));
                njMultiMatrix(&r, bm(w));
                memcpy(bm(w), r, sizeof r);
            } else {
                NJS_MATRIX r;
                njUnitMatrix(&r);
                njRotateX(&r, (Angle)(32768 / w->ct0));
                njMultiMatrix(&r, bm(w));
                memcpy(bm(w), r, sizeof r);
            }
            w->ct0--;
        }
        if (s->h.wall >= 0) {
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->mtn_add = 65536;
            w->frm_no = 0;
            w->mtn_md &= ~0x100u;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->flg2 &= ~0x1u;
            w->mode3++;
            break;
        }
        w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
        s->h.vel.y -= 0.33f;
        break;
    }
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = 16; w->frm_no = 0;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3++;
        }
        break;
    case 3:
        recvx_en03_add_null_trans(w, bd(w), spm_016, 45);
        if (w->ct0-- == 0) { set_modes(w, 1, 1, 1, 0); w->flg &= ~0x4u; }
        break;
    default: gap(w, "bhEne03_DG12: mode3 ausserhalb"); break;
    }
}

/* --- Die (en03.c:4229-4822) ------------------------------------------------ */
static void en03_die(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) {
    case 0: dd00(w); break;
    case 1: dd01(w); break;
    case 2: dd02(w); break;
    case 3: dd03(w); break;
    case 4: dd04(w); break;
    case 5: dd05(w); break;
    default: gap(w, "bhEne03_DeadMode2[mode2]"); break;
    }
}

/* en03.c:4235 DD00 — Zusammenbrechen. */
static void dd00(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 23; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
        w->mode3++;
        /* faellt durch */
    case 1:
        recvx_en03_add_null_trans(w, bd(w), spm_023, 25);
        if (w->ct0-- == 0) {
            w->mtn_no = 24; w->frm_no = 0;
            w->hokan_count = 0;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
            w->mode3++;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            w->frm_no = (int32_t)(65536.0f * (float)((int32_t)frames(w, w->mtn_no) - 1));
            w->mtn_add = 0;
            w->mode3++;
            w->flg &= ~0x8u;
            w->flg2 |= 0x1u;
            w->ay = recvx_t3_arc_tan2(-st(w)->h.m[2], st(w)->h.m[0]);
            if (w->sv->blood_pool) w->sv->blood_pool(w, &at, w->ay, &BloodParam);
            else gap(w, "bhEne_BloodPool (en03)");
        }
        break;
    default: gap(w, "bhEne03_DD00: mode3 ausserhalb"); break;
    }
}

/* en03.c:4289 DD01 — Rueckstoss im Sterben. */
static void dd01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_VEC pos;
        NJS_VECTOR trans, k;
        int32_t ang[3], a2[3];
        if ((w->flg & 0x80000u)) {
            w->flg |= 0x30u;
            w->flg &= ~0x1800000u;
            w->flg &= ~0x300000u;
            if (!parts(w, 0, &pos)) return;
            w->px = pos.x; w->py = pos.y; w->pz = pos.z;
            if (!root_ang_frm(w, w->mtn_no, w->frm_no, ang)) return;
            njRotateXYZ(bm(w), (Angle)ang[0], (Angle)ang[1], (Angle)ang[2]);
        }
        w->mtn_no = 17; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 45875;
        w->mtn_md |= 0x100u;
        if (!root_ang(w, w->mtn_no, 0, a2)) return;
        if (!recvx_en03_bind_root_angles(w, a2)) return;
        s->h.vel.x = s->h.m[4] / 2.0f;
        s->h.vel.y = s->h.m[5] / 2.0f;
        s->h.vel.z = s->h.m[6] / 2.0f;
        njRotateX(bm(w), 32768);
        if (!root_pos(w, w->mtn_no, 0, &k)) return;
        njCalcVector(bm(w), &k, &trans);
        w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
        if ((w->flg & 0x80000u)) {
            w->flg &= ~0x80000u;
            w->pxb = w->px; w->pyb = w->py; w->pzb = w->pz;
        }
        s->h.wall = -1;
        w->ar = 0.1f;
        w->flg &= ~0x8u;
        w->flg2 |= 0x1u;
        w->ct0 = 8;
        w->mode3++;
        /* faellt durch */
    }
    case 1: {
        NJS_VECTOR up, v, ov;
        float out;
        int ang;
        up.x = s->h.m[4]; up.y = s->h.m[5]; up.z = s->h.m[6];
        if (w->ct0 > 0) {
            v.x = 0.0f; v.y = 1.0f; v.z = 0.0f;
            out = njOuterProduct(&up, &v, &ov);
            if (out > 0.0f) {
                NJS_MATRIX r;
                njUnitVector(&ov);
                njUnitMatrix(&r);
                ang = (int)(10430.381f * asinf(out));
                if (s->h.m[5] < 0.0f) ang = 32768 - ang;
                njRotate(&r, &ov, (Angle)(ang / w->ct0));
                njMultiMatrix(&r, bm(w));
                memcpy(bm(w), r, sizeof r);
            }
            w->ct0--;
        }
        if (s->h.wall >= 0) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->mtn_no = 24; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 45875;
            w->mtn_add = 65536;
            w->mtn_md &= ~0x100u;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3++;
            (void)at;
        } else {
            w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
            s->h.vel.y -= 0.33f;
        }
        break;
    }
    case 2:
        if (w->ct0-- == 0) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            w->mtn_add = 0;
            w->mode3++;
            w->ay = recvx_t3_arc_tan2(-s->h.m[2], s->h.m[0]);
            if (w->sv->blood_pool) w->sv->blood_pool(w, &at, w->ay, &BloodParam);
            else gap(w, "bhEne_BloodPool (en03)");
        }
        break;
    default: gap(w, "bhEne03_DD01: mode3 ausserhalb"); break;
    }
    w->ar += (6.0f - w->ar) / 16.0f;
}

/* en03.c:4456 DD02. */
static void dd02(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0: {
        NJS_VECTOR trans, k;
        int32_t ang[3];
        w->mtn_no = 17; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 45875;
        s->h.vel.x = 0.0f; s->h.vel.z = 0.0f; s->h.vel.y = -1.0f;
        njRotateX(bm(w), 32768);
        if (!root_pos(w, w->mtn_no, 0, &k)) return;
        njCalcVector(bm(w), &k, &trans);
        w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
        w->mtn_md |= 0x100u;
        if (!root_ang(w, w->mtn_no, 0, ang)) return;
        if (!recvx_en03_bind_root_angles(w, ang)) return;
        s->h.wall = -1;
        w->flg &= ~0x8u;
        w->flg2 |= 0x1u;
        w->ar = 0.1f;
        w->mode3++;
        /* faellt durch */
    }
    case 1:
        if (s->h.wall >= 0) {
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            w->mtn_no = 24; w->frm_no = 0;
            w->hokan_count = 0;
            w->mtn_add = 65536;
            w->mtn_md &= ~0x100u;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 1;
            w->mode3++;
        } else {
            w->py += s->h.vel.y;
            s->h.vel.y -= 0.33f;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            w->frm_no = (int32_t)((float)((int32_t)frames(w, w->mtn_no) - 1) * 65536.0f);
            w->hokan_count = 0;
            w->mtn_add = 0;
            w->mode3++;
            w->ay = recvx_t3_arc_tan2(-s->h.m[2], s->h.m[0]);
            if (w->sv->blood_pool) w->sv->blood_pool(w, &at, w->ay, &BloodParam);
            else gap(w, "bhEne_BloodPool (en03)");
        }
        break;
    case 3: break;
    default: gap(w, "bhEne03_DD02: mode3 ausserhalb"); break;
    }
    w->ar += (6.0f - w->ar) / 16.0f;
}

/* en03.c:4558 DD03 — Zerfall in die acht Beine. */
static void dd03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    unsigned i;
    (void)s;
    switch (w->mode3) {
    case 0: {
        const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
        w->flg &= ~0x8u;
        w->flg2 |= 0x1u;
        w->flg |= 0x8000u;
        recvx_t3_set_mince_effect2(w, 2, 0.5f, 6);
        recvx_t3_set_mince_effect2(w, 259, 0.3f, 8);
        if (w->sv->blood_effect4) {
            w->sv->blood_effect4(&at, &at, 1, 9, 1);
            w->sv->blood_effect4(&at, &at, 1, 9, 2);
        } else {
            gap(w, "bhEne_SetBloodEffect4 (en03)");
        }
        for (i = 0; i < 8; ++i) {
            if (s->broken[i] == 0) {
                recvx_en03_owp_flag_or(w, (uint32_t)BrokenParts[i], 0x3u);
                recvx_en03_owp_flag_or(w, (uint32_t)BrokenParts[i] + 1u, 0x2u);
                recvx_en03_owp_flag_or(w, (uint32_t)BrokenParts[i] + 2u, 0x2u);
                if (s->legs[i]) recvx_en03s_leg_mark(s->legs[i]);
                s->broken[i] = 1;
                /* EXP0_INT(i, 980) = EXP0_C(105): schreibt 768 Byte hinter den
                   212-Byte-Block des Kindes (bhEne_CallocWork(0xD4, 8)) und
                   damit in den Block eines spaeteren Kindes. Belegte Original-UB,
                   kein Leser im Original; wird gemeldet, nicht nachgebildet. */
                gap(w, "bhEne03_DD03: EXP0_INT(i,980) schreibt hinter den Kindblock (Original schreibt fremden Speicher)");
            }
        }
        w->mdflg |= 0x20u;
        w->stflg |= 0x8u;
        if (!w->sv->model_node_hidden) { gap(w, "objP[n].evalflags (en03)"); return; }
        w->sv->model_node_hidden(w, 1u, 1);
        w->sv->model_node_hidden(w, 2u, 1);
        w->sv->model_node_hidden(w, 31u, 1);
        w->sv->model_node_hidden(w, 32u, 1);
        w->sv->model_node_hidden(w, 33u, 1);
        w->sv->model_node_hidden(w, 34u, 1);
        w->sv->model_node_hidden(w, 35u, 1);
        w->sv->model_node_hidden(w, 36u, 1);
        recvx_t3_erase_arrow(w, 1);
        recvx_t3_erase_arrow(w, 2);
        recvx_t3_erase_arrow(w, 31);
        recvx_t3_erase_arrow(w, 32);
        recvx_t3_erase_arrow(w, 33);
        recvx_t3_erase_arrow(w, 34);
        recvx_t3_erase_arrow(w, 35);
        recvx_t3_erase_arrow(w, 36);
        w->mtn_no = 0;
        w->mtn_add = 0;
        w->frm_no = 0;
        w->mode3++;
        break;
    }
    default: gap(w, "bhEne03_DD03: mode3 ausserhalb"); break;
    }
}

/* en03.c:4630 DD04. */
static void dd04(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_VEC pos;
        int32_t ang[3];
        if (!parts(w, 0, &pos)) return;
        w->px = pos.x; w->py = pos.y; w->pz = pos.z;
        w->mtn_md |= 0x100u;
        w->mtn_no = 50; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 52428;
        w->mtn_add = 0;
        w->mtn_md &= ~0x2u;
        if ((s->h.surface == 0) || (s->h.surface == 1)) {
            s->h.vel.x = s->dir.x; s->h.vel.y = s->dir.y; s->h.vel.z = s->dir.z;
        } else {
            s->h.vel.x = s->h.m[4]; s->h.vel.y = s->h.m[5]; s->h.vel.z = s->h.m[6];
        }
        if (!root_ang(w, w->mtn_no, 0, ang)) return;
        if (!recvx_en03_bind_root_angles(w, ang)) return;
        s->h.wall = -1;
        w->flg |= 0x30u;
        w->flg &= ~0x80000u;
        w->flg &= ~0x1800000u;
        w->flg &= ~0x300000u;
        w->ar = 3.0f;
        w->ct0 = 8;
        w->mode3++;
        /* faellt durch */
    }
    case 1:
        if (w->ct0 > 0) {
            w->ct0--;
            {
                NJS_MATRIX r;
                njUnitMatrix(&r);
                switch (s->h.surface) {
                case 5: njRotateX(&r, -2048); break;
                case 3: njRotateX(&r, 2048); break;
                case 4: njRotateZ(&r, -2048); break;
                case 2: njRotateZ(&r, 2048); break;
                case 1: njRotateX(&r, 4096); break;
                default: break;
                }
                njMultiMatrix(&r, bm(w));
                memcpy(bm(w), r, sizeof r);
            }
        }
        if (s->h.wall >= 0) {
            RECVX_T3_ATR hp;
            float h;
            s->h.surface = 0;
            recvx_en03_make_matrix(bd(w));
            if (!wall_record(w, s->h.wall, &hp)) return;
            if (hp.type == 7) w->py = hp.py;
            else {
                h = hp.h;
                if (h == 0) { if (!room_h(w, &h)) return; }
                w->py = hp.py + h;
            }
            w->mtn_no = 49; w->frm_no = 0;
            w->hokan_count = 3; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            w->mtn_md &= ~0x100u;
            w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
            w->mode3++;
        } else {
            w->px += s->h.vel.x; w->py += s->h.vel.y; w->pz += s->h.vel.z;
            s->h.vel.y -= 0.33f;
        }
        break;
    case 2:
        if (w->ct0-- == 0) {
            set_modes(w, 4, 0, 5, 0);
            w->flg &= ~0x8u;
            w->flg2 |= 0x1u;
            w->ar = 6.0f;
        }
        break;
    default: gap(w, "bhEne03_DD04: mode3 ausserhalb"); break;
    }
    w->ar += (6.0f - w->ar) / 16.0f;
}

/* en03.c:4790 DD05. */
static void dd05(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = 27; w->frm_no = 0;
        w->hokan_count = 8; w->hokan_rate = 32768;
        w->ct0 = (int32_t)frames(w, w->mtn_no) - 2;
        w->mode3++;
        break;
    case 1:
        if (w->ct0-- == 0) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            w->mtn_add = 0;
            w->mode3++;
            w->flg &= ~0x8u;
            w->flg2 |= 0x1u;
            w->ay = recvx_t3_arc_tan2(-s->h.m[2], s->h.m[0]);
            if (w->sv->blood_pool) w->sv->blood_pool(w, &at, w->ay, &BloodParam);
            else gap(w, "bhEne_BloodPool (en03)");
        }
        break;
    default: gap(w, "bhEne03_DD05: mode3 ausserhalb"); break;
    }
}

/* --- Gemeinsame Helfer (en03.c:4825-4950, 6074-6797) ----------------------- */

/* en03.c:4842 bhEne03_SearchPlayer. */
static void en03_search_player(RECVX_ENEMY_WORK* w, int ang)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC dist, p1, p2, n;
    if (!player(w, &p)) return;
    dist.x = w->px - p.pos.x;
    dist.y = w->py - p.pos.y;
    dist.z = w->pz - p.pos.z;
    local_point(w, &dist, &dist);
    s->h.ang = recvx_t3_arc_tan2(dist.x, dist.z);
    if (abs(s->h.ang) < ang) {
        p1.x = w->px + (5.0f * s->h.m[4]);
        p1.y = w->py + (5.0f * s->h.m[5]);
        p1.z = w->pz + (5.0f * s->h.m[6]);
        p2.x = p.pos.x;
        p2.y = 10.0f + p.pos.y;
        p2.z = p.pos.z;
        if (!w->sv || !w->sv->line) { gap(w, "bhCollisionCheckLine2 (en03)"); return; }
        if (w->sv->line(2, &p1, &p2, &n, 0x4400u, -1, NULL, NULL) == 0) {
            s->h.seen = 1;
            return;
        }
    }
    if (s->h.seen != 0) {
        s->h.seen++;
        if (s->h.seen > 90) s->h.seen = 0;
    }
}

/* en03.c:6199 bhEne03_CollisionLine. */
static void en03_collision_line(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_VEC from, to, n;
    uint32_t id = 0;
    from.x = w->pxb; from.y = w->pyb; from.z = w->pzb;
    to.x = w->px; to.y = w->py; to.z = w->pz;
    if (!w->sv || !w->sv->line) { gap(w, "bhEne03_CollisionLine: bhCollisionCheckLine"); return; }
    if (w->sv->line(1, &from, &to, &n, 0, 0, NULL, &id)) {
        if (s->h.wall < 0 && n.y > 0.0f) s->h.wall = id ? (int)id - 1 : -1;
    }
}

/* en03.c:6216 bhEne03_CollisionLine2: derselbe Lauf, um 0,01 Hochachse
   verschoben; das Ergebnis wird verworfen. */
static void en03_collision_line2(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_VEC from, to, n;
    if (!w->sv || !w->sv->line) { gap(w, "bhEne03_CollisionLine2: bhCollisionCheckLine"); return; }
    from.x = w->pxb + (0.01f * s->h.m[4]);
    from.y = w->pyb + (0.01f * s->h.m[5]);
    from.z = w->pzb + (0.01f * s->h.m[6]);
    to.x = w->px + (0.01f * s->h.m[4]);
    to.y = w->py + (0.01f * s->h.m[5]);
    to.z = w->pz + (0.01f * s->h.m[6]);
    w->sv->line(1, &from, &to, &n, 0, 0, NULL, NULL);
}

/* en03.c:6074 bhEne03_CollisionBoxEdge2. */
static void en03_collision_box_edge2(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_VEC p[4], p2[4];
    const float tmp = 999.0f;
    float dx = 0.0f, dy = 0.0f, dz = 0.0f;
    int i;
    switch (s->h.surface) {
    case 0:
        dx = 0.0f; dy = -tmp; dz = 0.0f;
        p[0].x = w->px - tmp; p[0].z = w->pz - tmp;
        p[1].x = w->px - tmp; p[1].z = w->pz + tmp;
        p[2].x = w->px + tmp; p[2].z = w->pz - tmp;
        p[3].x = w->px + tmp; p[3].z = w->pz + tmp;
        p[0].y = p[1].y = p[2].y = p[3].y = w->py;
        break;
    case 1:
        dx = 0.0f; dy = tmp; dz = 0.0f;
        p[0].x = w->px - tmp; p[0].z = w->pz - tmp;
        p[1].x = w->px - tmp; p[1].z = w->pz + tmp;
        p[2].x = w->px + tmp; p[2].z = w->pz - tmp;
        p[3].x = w->px + tmp; p[3].z = w->pz + tmp;
        p[0].y = p[1].y = p[2].y = p[3].y = w->py;
        break;
    case 4:
        dx = tmp; dy = 0.0f; dz = 0.0f;
        p[0].y = w->py - tmp; p[0].z = w->pz - tmp;
        p[1].y = w->py - tmp; p[1].z = w->pz + tmp;
        p[2].y = w->py + tmp; p[2].z = w->pz - tmp;
        p[3].y = w->py + tmp; p[3].z = w->pz + tmp;
        p[0].x = p[1].x = p[2].x = p[3].x = w->px;
        break;
    case 2:
        dx = -tmp; dy = 0.0f; dz = 0.0f;
        p[0].y = w->py - tmp; p[0].z = w->pz - tmp;
        p[1].y = w->py - tmp; p[1].z = w->pz + tmp;
        p[2].y = w->py + tmp; p[2].z = w->pz - tmp;
        p[3].y = w->py + tmp; p[3].z = w->pz + tmp;
        p[0].x = p[1].x = p[2].x = p[3].x = w->px;
        break;
    case 3:
        dx = 0.0f; dy = 0.0f; dz = tmp;
        p[0].x = w->px - tmp; p[0].y = w->py - tmp;
        p[1].x = w->px - tmp; p[1].y = w->py + tmp;
        p[2].x = w->px + tmp; p[2].y = w->py - tmp;
        p[3].x = w->px + tmp; p[3].y = w->py + tmp;
        p[0].z = p[1].z = p[2].z = p[3].z = w->pz;
        break;
    case 5:
        dx = 0.0f; dy = 0.0f; dz = -tmp;
        p[0].x = w->px - tmp; p[0].y = w->py - tmp;
        p[1].x = w->px - tmp; p[1].y = w->py + tmp;
        p[2].x = w->px + tmp; p[2].y = w->py - tmp;
        p[3].x = w->px + tmp; p[3].y = w->py + tmp;
        p[0].z = p[1].z = p[2].z = p[3].z = w->pz;
        break;
    default:
        gap(w, "bhEne03_CollisionBoxEdge2: EXP0_C(0x69) ausserhalb");
        return;
    }
    for (i = 0; i < 4; ++i) {
        RECVX_T3_ATR hp;
        RECVX_ENEMY_VEC at;
        p2[i].x = dx + p[i].x;
        p2[i].y = dy + p[i].y;
        p2[i].z = dz + p[i].z;
        if (line_hit(w, &p[i], &p2[i]) != 0) continue;
        get_wall(w);
        if (s->h.wall < 0) continue;
        if (!wall_record(w, s->h.wall, &hp)) return;
        at.x = w->px; at.y = w->py; at.z = w->pz;
        recvx_en03_collision_box_edge(&hp, &at, w->ar);
        break;
    }
}

/* en03.c:5064 bhEne03_CollisionWalls. */
static int en03_collision_walls(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    NJS_MATRIX m;
    NJS_VECTOR body, trans;
    RECVX_ENEMY_VEC op;
    float px, py, pz;
    int ret = 0;
    px = w->px; py = w->py; pz = w->pz;
    body.x = 0.0f; body.y = w->ar; body.z = 0.0f;
    memcpy(m, bm(w), sizeof m);
    if ((w->flg & 0x1000000u)) {
        int32_t ang[3];
        if (!root_ang_frm(w, w->mtn_no, w->frm_no, ang)) return 0;
        njRotateXYZ(&m, (Angle)ang[0], (Angle)ang[1], (Angle)ang[2]);
    }
    njCalcVector(&m, &body, &body);
    if (!w->sv || !w->sv->model_root_position || !w->sv->model_root_position(w, &op)) {
        gap(w, "bhEne03_CollisionWalls: mlwP->objP->pos"); return 0;
    }
    trans.x = op.x; trans.y = op.y; trans.z = op.z;
    if (!(w->flg & 0x800000u)) trans.y = 0.0f;
    njCalcVector(bm(w), &trans, &trans);
    w->px += trans.x; w->py += trans.y; w->pz += trans.z;
    if (s->h.wall >= 0) {
        int hit;
        w->px += body.x; w->py += body.y; w->pz += body.z;
        w->px -= 3.0f * s->h.m[8]; w->py -= 3.0f * s->h.m[9]; w->pz -= 3.0f * s->h.m[10];
        if (recvx_en03_collision2(w, s->h.wall, &hit)) ret = 1;
        w->px += 3.0f * s->h.m[8]; w->py += 3.0f * s->h.m[9]; w->pz += 3.0f * s->h.m[10];
        if (recvx_en03_collision2(w, s->h.wall, &hit)) ret = 1;
        if (w->type == 0) {
            w->px += 2.0f * s->h.m[8]; w->py += 2.0f * s->h.m[9]; w->pz += 2.0f * s->h.m[10];
            if (recvx_en03_collision2(w, s->h.wall, &hit)) ret = 1;
            w->px -= 2.0f * s->h.m[8]; w->py -= 2.0f * s->h.m[9]; w->pz -= 2.0f * s->h.m[10];
        }
        if ((s->h.surface != 0) && (s->h.surface != 1)) en03_collision_box_edge2(w);
        w->px -= body.x; w->py -= body.y; w->pz -= body.z;
    } else {
        int hit;
        w->px += body.x; w->py += body.y; w->pz += body.z;
        if (recvx_en03_collision(w, &hit)) ret = 1;
        w->px -= body.x; w->py -= body.y; w->pz -= body.z;
    }
    w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
    if (s->h.wall >= 0) {
        switch (s->h.surface) {
        case 0: case 1: w->py = py; break;
        case 2: case 4: w->px = px; break;
        case 3: case 5: w->pz = pz; break;
        default: break;
        }
    }
    return ret;
}

/* en03.c:6278 bhEne03_DiveSpace. */
static int en03_dive_space(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    NJS_VECTOR pos;
    NJS_VECTOR k;
    float dist;
    uint32_t i, n;
    int ok = 0, hit = 0;
    if (xp1(w) && (xp1(w)->flg & 0x3)) return 0;
    if (!player(w, &p)) return 0;
    {
        const uint32_t f = frames(w, 13);
        if (!f) return 0;
        if (!root_pos(w, 13, f - 1, &k)) return 0;
    }
    njCalcVector(bm(w), &k, &pos);
    pos.x += w->px; pos.y += w->py; pos.z += w->pz;
    dist = ((pos.x - p.pos.x) * (pos.x - p.pos.x)) + ((pos.z - p.pos.z) * (pos.z - p.pos.z));
    if (dist < 25.0f) return 0;
    n = (w->sv && w->sv->enemy_count) ? w->sv->enemy_count() : 0u;
    for (i = 0; i < n; ++i) {
        RECVX_ENEMY_WORK* ep = (w->sv->enemy_at) ? w->sv->enemy_at(i) : NULL;
        if (ep && (ep->flg & 0x1u) && ep->id == 3u && ep != w) {
            dist = ((pos.x - ep->px) * (pos.x - ep->px)) + ((pos.z - ep->pz) * (pos.z - ep->pz));
            if (dist < 10.0f) return 0;
        }
    }
    pos.y = ground_pos(w, &(RECVX_ENEMY_VEC){ pos.x, pos.y, pos.z }, &ok);
    if (!ok) return 1;                 /* Dienst fehlt: Luecke gemeldet, kein Treffer */
    if (!wall_type(w, &(RECVX_ENEMY_VEC){ pos.x, pos.y, pos.z }, 5.0f,
                   (w->py - pos.y) - 10.0f, &hit)) return 1;
    return hit ? 0 : 1;
}

/* en03.c:6335 bhEne03_GetWallDir. Kein Aufrufer in en03.c; der Originalkoerper
   faellt fuer |y| < 0.9 und |x|,|z| < 0.9 ohne Rueckgabe durch und liefert
   damit einen unbestimmten Wert. Das wird gemeldet, nicht geraten. */
int recvx_en03_get_wall_dir(RECVX_ENEMY_WORK* w, const NJS_VECTOR* v)
{
    if (v->y >= 0.9f) return 0;
    if (v->y <= -0.9f) return 1;
    if (v->x >= 0.9f) return 2;
    if (v->x <= -0.9f) return 4;
    if (v->z >= 0.9f) return 5;
    if (v->z <= -0.9f) return 3;
    gap(w, "bhEne03_GetWallDir: kein Zweig trifft (Original faellt ohne Rueckgabe durch)");
    return 0;
}

/* en03.c:6715 bhEne03_CheckJumpSpace. */
static int en03_check_jump_space(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    NJS_POINT3 p1, p2;
    float dist;
    uint32_t i, n;
    int hit = 0, ok = 0;
    if (!player(w, &p)) return 0;
    if (s->h.surface == 0) {
        p1.x = w->px - (20.0f * s->h.m[8]);
        p1.y = w->py - (20.0f * s->h.m[9]);
        p1.z = w->pz - (20.0f * s->h.m[10]);
        p2.x = w->px; p2.y = w->py; p2.z = w->pz;
        for (i = 0; i < 5; ++i) {
            p2.x -= 4.0f * s->h.m[8];
            p2.z -= 4.0f * s->h.m[10];
            if (!wall_type(w, &(RECVX_ENEMY_VEC){ p2.x, p2.y, p2.z }, 5.0f, 15.0f, &hit)) return 0;
            if (hit) return 0;
        }
    } else {
        p1.x = w->px + (10.0f * s->h.m[4]);
        p1.y = w->py + (10.0f * s->h.m[5]);
        p1.z = w->pz + (10.0f * s->h.m[6]);
        p1.y = ground_pos(w, &(RECVX_ENEMY_VEC){ p1.x, p1.y, p1.z }, &ok);
        if (!ok) return 1;
        if (!wall_type(w, &(RECVX_ENEMY_VEC){ p1.x, p1.y, p1.z }, 5.0f, 10.0f, &hit)) return 1;
        if (hit) return 0;
        p1.y += 1.0f;
        p2.x = w->px; p2.y = w->py + 1.0f; p2.z = w->pz;
        if (line_hit(w, &(RECVX_ENEMY_VEC){ p2.x, p2.y, p2.z },
                     &(RECVX_ENEMY_VEC){ p1.x, p1.y, p1.z })) return 0;
    }
    if ((((18.0f + p.pos.y) + w->ar) > p1.y) && ((p.pos.y - w->ar) < p1.y)) {
        dist = njSqrt(((p1.x - p.pos.x) * (p1.x - p.pos.x)) + ((p1.z - p.pos.z) * (p1.z - p.pos.z)));
        if (dist < 10.0f) return 0;
    }
    n = (w->sv && w->sv->enemy_count) ? w->sv->enemy_count() : 0u;
    for (i = 0; i < n; ++i) {
        RECVX_ENEMY_WORK* ep = (w->sv->enemy_at) ? w->sv->enemy_at(i) : NULL;
        if (ep && (ep->flg & 0x1u) && ep->id == 3u && ep != w) {
            const RECVX_ENEMY_VEC a = { p1.x, p1.y, p1.z };
            const RECVX_ENEMY_VEC b = { ep->px, ep->py, ep->pz };
            if (recvx_en03_distance_p2p(&a, &b) < 10.0f) return 0;
        }
    }
    return 1;
}

/* plp->mnwP[mtn_no].frm_num. `from_enemy` waehlt die Quelle: 1 = epw->mnwP,
   0 = plp->mnwPb (enemy_work.h:522). */
static uint32_t player_frames(RECVX_ENEMY_WORK* w, uint32_t mtn, int from_enemy)
{
    if (!w->sv || !w->sv->player_motion_frames) {
        gap(w, "plp->mnwP[mtn_no].frm_num (en03)"); return 0;
    }
    return w->sv->player_motion_frames(mtn, from_enemy);
}
static void player_library(RECVX_ENEMY_WORK* w, uint32_t source)
{
    if (!en03_sv || !en03_sv->player_library) {
        gap(w, source ? "plp->mnwP = epw->mnwP (en03)" : "plp->mnwP = plp->mnwPb (en03)");
        return;
    }
    en03_sv->player_library(w, source);
}
static void player_voice(RECVX_ENEMY_WORK* w, int se)
{
    if (!w->sv || !w->sv->player_voice) { gap(w, "bhEne_CallPlayerVoice (en03)"); return; }
    w->sv->player_voice(se);
}
static void vibration(RECVX_ENEMY_WORK* w, int motor, int power)
{
    if (!w->sv || !w->sv->vibration) { gap(w, "StartVibrationEx (en03)"); return; }
    w->sv->vibration(motor, power);
}

/* en03.c:6798 bhEne03_PlayerControl — der Spielerwork waehrend des Griffs.
   Der Anfuehrerwork (flg 0x4000000) laeuft ausschliesslich hier durch. */
static void player_control(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    float px, pz;
    NJS_MATRIX m;
    if (!player(w, &p)) return;
    if (p.mode0 == 4) {
        switch (p.mode2) {
        case 0:
        case 1:
            switch (p.mode3) {
            case 0:
                p.flg &= ~0x40000u;
                p.flg2 |= 0x1u;
                p.ct1 = 8;
                p.mtn_no = (p.ply_id * 14u) + ((p.mode2 == 0) ? 62u : 65u);
                player_library(w, 1);
                p.frm_no = 0;
                p.hokan_count = 3;
                p.hokan_rate = 32768;
                p.mtn_add = 65536;
                p.ct0 = (int32_t)player_frames(w, p.mtn_no, 1);
                player_voice(w, 2);
                vibration(w, 1, 11);
                p.mode3++;
                /* faellt durch */
            case 1:
                if (p.ct1 != 0) p.ay += (int16_t)(p.ayp - p.ay) / p.ct1;
                if (p.ct0-- == 0) {
                    p.mtn_no = (p.ply_id * 14u) + ((p.mode2 == 0) ? 63u : 66u);
                    p.frm_no = 0;
                    p.mode3++;
                }
                break;
            case 2:
                if (p.ct1 != 0) p.ay += (int16_t)(p.ayp - p.ay) / p.ct1;
                break;
            default:
                gap(w, "bhEne03_PlayerControl: plp->mode3 ausserhalb (mode2 0/1)");
                break;
            }
            break;
        case 2:
        case 3:
            switch (p.mode3) {
            case 0:
                p.flg &= ~0x40000u;
                p.flg |= 0x10000u;
                p.flg2 |= 0x1u;
                p.ct0 = 8;
                p.mtn_no = (p.ply_id * 14u) + ((p.mode2 == 2) ? 58u : 59u);
                player_library(w, 1);
                p.frm_no = 0;
                p.hokan_count = 3;
                p.hokan_rate = 32768;
                p.mtn_add = 65536;
                p.mode3++;
                break;
            case 1:
                if (p.ct0 != 0) p.ay += (int16_t)(p.ayp - p.ay) / p.ct0;
                if (p.frm_no == 0) {
                    p.mtn_no = (p.ply_id * 14u) + ((p.mode2 == 2) ? 60u : 61u);
                    p.frm_no = 0;
                    p.mode3++;
                }
                if ((p.frm_no / 65536) == 4) {
                    player_voice(w, 2);
                    vibration(w, 1, 11);
                }
                break;
            case 2:
                if (p.frm_no == 0) {
                    player_library(w, 0);
                    p.flg &= ~0x10004u;
                    p.flg2 &= ~0x1u;
                    p.flg |= 0x8u;
                    p.at_flg = 0;
                    p.stflg &= ~0x10000u;
                    p.mode0 = 1; p.mode1 = 0; p.mode2 = 0; p.mode3 = 0;
                }
                break;
            default:
                gap(w, "bhEne03_PlayerControl: plp->mode3 ausserhalb (mode2 2/3)");
                break;
            }
            break;
        case 4:
        case 5:
            switch (p.mode3) {
            case 0:
                p.flg &= ~0x40000u;
                p.flg |= 0x10000u;
                p.mtn_no = (p.ply_id * 14u) + ((p.mode2 == 4) ? 64u : 67u);
                player_library(w, 1);
                p.frm_no = 0;
                p.mode3++;
                break;
            case 1:
                if (p.frm_no == 0) {
                    player_library(w, 0);
                    p.flg &= ~0x10004u;
                    p.flg2 &= ~0x1u;
                    p.flg |= 0x8u;
                    p.at_flg = 0;
                    p.stflg &= ~0x10000u;
                    p.mode0 = 1; p.mode1 = 0; p.mode2 = 0; p.mode3 = 0;
                    if (xp1(w)) xp1(w)->flg &= ~0x4;
                }
                break;
            default:
                gap(w, "bhEne03_PlayerControl: plp->mode3 ausserhalb (mode2 4/5)");
                break;
            }
            break;
        default:
            gap(w, "bhEne03_PlayerControl: plp->mode2 ausserhalb (mode0 4)");
            break;
        }
        if (!w->sv || !w->sv->player_joint_matrix || !w->sv->player_joint_matrix(5u, (float*)m)) {
            gap(w, "plp->mlwP->owP[5].mtx (en03)"); return;
        }
        px = m[12] - p.pos.x;
        pz = m[14] - p.pos.z;
        p.pos.x += px;
        p.pos.z += pz;
        /* hitchk.c:12 bhCheckWall(plp) mit plp->ar = PlyInfo[ply_id][0] = car + 1
           und plp->ah = PlyInfo[ply_id][1] = cah + 1 (player.c:705-709). */
        if (en03_sv && en03_sv->player_wall) en03_sv->player_wall(&p, p.car + 1.0f, p.cah + 1.0f);
        else gap(w, "bhCheckWall(plp) (en03.c:7011)");
        p.pos.x -= px;
        p.pos.z -= pz;
    } else if (p.mode0 == 6) {
        switch (p.mode2) {
        case 0:
        case 1:
            switch (p.mode3) {
            case 0:
                p.flg &= ~0x40000u;
                p.flg |= 0x10000u;
                p.ct1 = 10;
                p.mtn_no = (p.ply_id * 14u) + ((p.mode2 == 0) ? 71u : 70u);
                player_library(w, 1);
                p.frm_no = 0;
                p.hokan_count = 3;
                p.hokan_rate = 32768;
                p.mtn_add = 65536;
                p.ct0 = (int32_t)player_frames(w, p.mtn_no, 1) - 1;
                player_voice(w, 1);
                vibration(w, 1, 11);
                p.mode3++;
                /* faellt durch */
            case 1:
                if (p.ct0-- == 0) p.mtn_add = 0;
                break;
            default:
                gap(w, "bhEne03_PlayerControl: plp->mode3 ausserhalb (mode0 6, mode2 0/1)");
                break;
            }
            break;
        case 2:
        case 3:
            switch (p.mode3) {
            case 0:
                p.flg &= ~0x40000u;
                p.flg |= 0x10000u;
                p.ct1 = 10;
                p.mtn_no = (p.ply_id * 14u) + ((p.mode2 == 2) ? 68u : 69u);
                player_library(w, 1);
                p.frm_no = 0;
                p.hokan_count = 3;
                p.hokan_rate = 32768;
                p.mtn_add = 65536;
                p.ct0 = (int32_t)player_frames(w, p.mtn_no, 1) - 1;
                player_voice(w, 1);
                vibration(w, 1, 11);
                p.mode3++;
                /* faellt durch */
            case 1:
                if (p.ct0-- == 0) p.mtn_add = 0;
                break;
            default:
                gap(w, "bhEne03_PlayerControl: plp->mode3 ausserhalb (mode0 6, mode2 2/3)");
                break;
            }
            break;
        default:
            gap(w, "bhEne03_PlayerControl: plp->mode2 ausserhalb (mode0 6)");
            break;
        }
    }
    put_player(w, &p);
}

/* en03.c:7117 bhEne03_CallSE. */
static void en03_call_se(RECVX_ENEMY_WORK* w)
{
    uint32_t fno;
    if (room_motion(w)) return;
    fno = (uint32_t)(w->frm_no / 65536);
    switch (w->mtn_no) {
    case 1: case 31:
        if (fno == 0 || fno == 9 || fno == 14) call_se(w, 74499);
        break;
    case 2: case 3: case 32: case 33:
        if (fno == 8 || fno == 20 || fno == 28 || fno == 34) call_se(w, 74499);
        break;
    case 6: case 7:
        if (fno == 7 || fno == 13 || fno == 18) call_se(w, 74499);
        break;
    case 4:
        if (fno == 0 || fno == 7) call_se(w, 74499);
        break;
    case 37:
        if (fno == 0 || fno == 7) call_se(w, 74499);
        break;
    case 40:
        if (fno == 2 || fno == 6) call_se(w, 74499);
        break;
    case 8:
        if (fno == 10) call_se(w, 74500);
        break;
    case 9:
        if (fno == 20) call_se(w, 74501);
        break;
    case 10:
        if (fno == 30) call_se(w, 74502);
        break;
    case 15:
        if (fno == 0) call_se(w, 74510);
        break;
    case 18:
        if (fno == 0) call_se(w, 8978);
        break;
    case 23:
        if (fno == 10) call_se(w, 8978);
        break;
    case 25:
        if (fno == 35 || fno == 50) call_se(w, 74501);
        break;
    case 22:
        if (fno == 0) call_se(w, 8980);
        break;
    case 41:
        if (fno == 13) call_se(w, 74517);
        break;
    case 49:
        if (fno == 1) call_se(w, 74518);
        break;
    case 44:
        if (fno == 15) call_se(w, 74519);
        if (fno == 43) call_se(w, 74501);
        break;
    case 46:
        if (fno == 20) call_se(w, 74502);
        break;
    case 55:
        if (fno == 185 || fno == 195 || fno == 210) call_se(w, 74501);
        break;
    default: break;
    }
}

/* en03.c:7275 bhEne03_Acid — acht Saeurespritzer zwischen den Kopfknoten
   35 und 36. */
static void en03_acid(RECVX_ENEMY_WORK* w, int type)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    RECVX_EN03_STATE* s = st(w);
    NJS_MATRIX m35, m36;
    RECVX_ENEMY_VEC pos1, pos2, scale;
    int i, eno;
    if (!joint(w, 36, &m36)) return;
    if (!joint(w, 35, &m35)) return;
    pos1.x = (m36[12] + m35[12]) * 0.5f;
    pos1.y = (m36[13] + m35[13]) * 0.5f;
    pos1.z = (m36[14] + m35[14]) * 0.5f;
    if (type == 0) {
        pos2.x = -2.5f * s->h.m[8];
        pos2.y = -2.5f * s->h.m[10];
        pos2.z = (s->h.surface == 0) ? 0.5f : 0.0f;
    } else {
        pos2.x = -0.5f * s->h.m[8];
        pos2.y = -0.5f * s->h.m[10];
        pos2.z = 2.5f;
    }
    if (!en03_sv || !en03_sv->effect_tb) { gap(w, "bhSetEffectTb (en03 Acid)"); return; }
    if (!x || !x->effect_patch || !x->model_texp_index) {
        gap(w, "eff[eno] (en03 Acid)"); return;
    }
    if (!en03_sv || !en03_sv->effect_position) { gap(w, "eff[eno].px (en03 Acid)"); return; }
    for (i = 0; i < 8; ++i) {
        float dt, a, b, c;
        NJS_MATRIX r;
        NJS_VECTOR v;
        RECVX_T3_EFFPATCH patch;
        RECVX_ENEMY_VEC at;
        if (!rnd(w, &dt)) return;
        scale.x = scale.y = scale.z = 0.5f + dt;
        eno = en03_sv->effect_tb(256u, (type != 0) ? 5u : 1u, 1u, &pos1, &scale);
        if (eno == -1) continue;
        if (!rnd(w, &a) || !rnd(w, &b)) return;
        njUnitMatrix(&r);
        njRotateY(&r, (Angle)(int)((1820.0f * a) - 910.0f));
        njRotateX(&r, (Angle)(int)(1820.0f * b));
        v.x = pos2.x; v.y = pos2.z; v.z = pos2.y;
        njCalcVector(&r, &v, &v);
        if (!rnd(w, &c)) return;
        memset(&patch, 0, sizeof patch);
        patch.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP |
                       RECVX_T3_EFF_TEX_ID | RECVX_T3_EFF_VECTOR;
        patch.stflg_or = 0x20u;
        patch.texp = x->model_texp_index(w, 1);
        patch.tex_id = 1;
        patch.xn = v.x;
        patch.yn = v.y;
        patch.zn = v.z;
        x->effect_patch(eno, &patch);
        at.x = pos1.x + (c * v.x);
        at.y = pos1.y + (c * v.y);
        at.z = pos1.z + (c * v.z);
        en03_sv->effect_position(eno, &at, 1);
    }
}

/* en03.c:7353 bhEne03_HitMark. */
static void en03_hit_mark(RECVX_ENEMY_WORK* w)
{
    int i, range;
    RECVX_ENEMY_VEC ofp;
    const EN03_BLOOD* blp;
    if (w->djnt_no < 0 || w->djnt_no >= 37) {
        gap(w, "bhEne03_HitMark: BloodTbl[epw->djnt_no] hinter der Tabelle");
        return;
    }
    if (w->wpnr_no < 0 || w->wpnr_no >= 21) {
        gap(w, "bhEne03_HitMark: DmgReact[epw->wpnr_no] hinter der Tabelle");
        return;
    }
    blp = &BloodTbl[w->djnt_no];
    range = 0;
    if ((w->comb_flg & 0x10u)) range = 0;
    if ((w->comb_flg & 0x20u)) range = 1;
    if ((w->comb_flg & 0x40u)) range = 2;
    if (DmgReact[w->wpnr_no].type[range] >= 0) {
        float a, b, c;
        ofp = blp->ofp;
        if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return;
        ofp.x += (blp->rx * a) - (blp->rx / 2.0f);
        ofp.y += (blp->ry * b) - (blp->ry / 2.0f);
        ofp.z += (blp->rz * c) - (blp->rz / 2.0f);
        if (w->sv->blood_effect_burst)
            w->sv->blood_effect_burst(w, DmgReact[w->wpnr_no].type[range], w->djnt_no, &ofp, 0);
        else gap(w, "bhEne_SetBloodEffectBurst (en03)");
        if (DmgReact[w->wpnr_no].bloodstain[range] != 0)
            recvx_t3_set_bloodstain(w, 0, w->djnt_no, &ofp);
    }
    if ((DmgReact[w->wpnr_no].exef & 0x1u) && (blp->flg == 0)) {
        for (i = 0; i < 4; ++i) {
            float a, b, c, s;
            int len;
            ofp = blp->ofp;
            if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return;
            ofp.x += (blp->rx * a) - (blp->rx / 2.0f);
            ofp.y += (blp->ry * b) - (blp->ry / 2.0f);
            ofp.z += (blp->rz * c) - (blp->rz / 2.0f);
            if (!rnd(w, &s)) return;
            if (!rnd_mod(w, 40, &len)) return;
            if (w->sv->fire_effect)
                w->sv->fire_effect(w, w->djnt_no, &ofp, 0.5f + (0.5f * s), len + 20);
            else gap(w, "bhEne_SetFireEffect (en03)");
        }
    }
    if ((DmgReact[w->wpnr_no].exef & 0x2u)) {
        float a, b, c;
        ofp = blp->ofp;
        if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return;
        ofp.x += (blp->rx * a) - (blp->rx / 2.0f);
        ofp.y += (blp->ry * b) - (blp->ry / 2.0f);
        ofp.z += (blp->rz * c) - (blp->rz / 2.0f);
        if (w->sv->acid_effect) w->sv->acid_effect(w, w->djnt_no, &ofp, 2.0f);
        else gap(w, "bhEne_SetAcidEffect (en03)");
    }
    if ((DmgReact[w->wpnr_no].exef & 0x4u)) {
        if (!en03_sv || !en03_sv->material_color) { gap(w, "npSetAllMatColor (en03)"); return; }
        en03_sv->material_color(w, 0xFF201010u);
        w->mdflg |= 0x400u;
    }
}

/* en03.c:6366 bhEne03_DamageInit. */
static void damage_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    int i, pno, num, flg;
    w->flg &= ~0x4u;
    recvx_t3_calc_damage(w, CombWepTbl, CombJointTbl);
    w->comb_flg &= ~0xCu;
    if (recvx_en03_dg_dir_check(w, bd(w)) != 0) w->comb_flg |= 0x8u;
    else w->comb_flg |= 0x4u;

    if ((w->wpnr_no == 16) && !(w->flg2 & 0x4u) && (w->comb_pnt != 1)) return;

    if (w->type == 0) {
        if (!player(w, &p)) return;
        if ((w->comb_flg & 0x8u)) {
            s->hp2 -= w->total_dam;
        } else {
            if ((s->h.surface == 0) && (!(p.at_flg & 0x8u))) s->hp2 -= w->total_dam;
            w->hp -= w->total_dam;
        }
    } else {
        w->hp -= w->total_dam;
    }

    if (w->wpnr_no == 20) w->hp = -1;
    if ((w->wpnr_no == 17) && !(w->flg2 & 0x4u)) return;

    en03_hit_mark(w);

    if (w->total_dam > 15) {
        float r;
        if (!rnd(w, &r)) return;
        if (r < 0.3f) {
            switch (w->wpnr_no) {
            case 11: case 20: case 14: case 15: case 16:
                if (!rnd_mod(w, 8, &pno)) return;
                num = 0;
                if (pno < 4) { for (i = 0; i < 4; ++i) num += s->broken[i]; }
                else { for (i = 4; i < 8; ++i) num += s->broken[i]; }
                if ((num == 0) && (s->broken[pno] == 0) &&
                    (((pno != 0) && (pno != 4)) ||
                     ((pno == 0) && (s->broken[4] == 0)) ||
                     ((pno == 4) && (s->broken[0] == 0)))) {
                    s->broken[pno] = 1;
                    recvx_en03_owp_flag_or(w, (uint32_t)BrokenParts[pno], 0x3u);
                    recvx_en03_owp_flag_or(w, (uint32_t)BrokenParts[pno] + 1u, 0x2u);
                    recvx_en03_owp_flag_or(w, (uint32_t)BrokenParts[pno] + 2u, 0x2u);
                    if (s->legs[pno]) recvx_en03s_leg_mark(s->legs[pno]);
                    /* EXP0_INT(pno, 980) = EXP0_C(105) liegt 768 Byte hinter
                       dem 212-Byte-Block (bhEne_CallocWork(0xD4, 8)); belegte
                       Original-UB, kein Leser im Original. */
                    gap(w, "bhEne03_DamageInit: EXP0_INT(pno,980) schreibt hinter den Kindblock (Original schreibt fremden Speicher)");
                    w->mdflg |= 0x20u;
                    if (w->sv->mince_effect) {
                        w->sv->mince_effect(w, 2, 3);
                        w->sv->mince_effect(w, 259, 2);
                    } else gap(w, "bhEne_SetMinceEffect (en03)");
                    recvx_t3_set_bloodstain(w, 2, BrokenParts[pno], NULL);
                }
                break;
            default: break;
            }
        }
    }

    if (w->hp < 0) {
        w->flg |= 0x2u;
        w->flg &= ~0x20u;
        call_se(w, 8977);
        w->mode0 = 4;
        w->mode1 = 0;
        w->mode3 = 0;
        if (w->type == 0) {
            if (w->wpnr_no == 20) {
                w->mode2 = 3;
            } else if ((w->flg & 0x80000u)) {
                w->mode2 = ((s->h.surface == 0) || (s->h.surface2 == 0)) ? 4 : 1;
            } else {
                switch (s->h.surface) {
                case 0:
                    switch (w->wpnr_no) {
                    case 5: case 6: case 11: case 20: case 14: case 15: case 16:
                        w->mode2 = 0;
                        break;
                    default: w->mode2 = 5; break;
                    }
                    break;
                case 1: w->mode2 = 2; break;
                default: w->mode2 = 1; break;
                }
            }
        } else {
            if (w->total_dam > 25) {
                w->mode2 = 3;
            } else if ((w->flg & 0x80000u)) {
                w->mode2 = ((s->h.surface == 0) || (s->h.surface2 == 0)) ? 4 : 1;
            } else {
                switch (s->h.surface) {
                case 0:
                    w->mode2 = (s->h.wall >= 0) ? 5 : 4;
                    break;
                case 1: w->mode2 = 2; break;
                default: w->mode2 = 1; break;
                }
            }
        }
        if ((w->flg & 0x80000u)) {
            if (xp1(w)) xp1(w)->flg &= 0x2;
        }
        switch (w->mode2) {
        case 0: dd00(w); break;
        case 1: dd01(w); break;
        case 2: dd02(w); break;
        case 3: dd03(w); break;
        case 4: dd04(w); break;
        case 5: dd05(w); break;
        default: gap(w, "bhEne03_DeadMode2[mode2]"); break;
        }
        return;
    }

    if (w->type == 0) {
        flg = 0;
        if (s->hp2 < 0) {
            switch (w->wpnr_no) {
            case 8: case 9: case 12:
                if (w->comb_pnt > 5) { flg = 1; w->mode2 = 4; }
                break;
            case 5: case 6: case 10: case 11: case 13:
            case 14: case 15: case 16: case 20:
                flg = 1;
                w->mode2 = 4;
                break;
            default: break;
            }
        }
        if (flg == 0) {
            if (w->total_dam < 30) return;
            if (w->total_dam < 50) w->mode2 = 1;
            else w->mode2 = (s->h.surface == 1) ? 2 : 3;
        }
        call_se(w, 8976);
        if ((w->flg & 0x80000u)) {
            NJS_MATRIX m0;
            if (!joint(w, 0, &m0)) return;
            w->mode2 = (m0[5] > 0.0f) ? 11 : 12;
        }
        if (s->h.wall < 0) w->mode2 = 10;
    } else {
        if (w->total_dam < 30) return;
        if (w->total_dam < 50) {
            w->mode2 = 6;
        } else {
            if (s->h.surface == 0) {
                w->mode2 = (w->comb_flg & 0x4u) ? 9 : 8;
            } else {
                w->mode2 = 7;
            }
        }
        if ((w->flg & 0x80000u)) w->mode2 = 11;
        if (s->h.wall < 0) w->mode2 = 10;
    }

    w->mode0 = 3;
    w->mode1 = 0;
    w->mode3 = 0;

    if ((w->flg & 0x80000u)) {
        if (xp1(w)) xp1(w)->flg &= 0x2;
    }

    switch (w->mode2) {
    case  0: break;
    case  1: dg01(w); break;
    case  2: dg02(w); break;
    case  3: dg03(w); break;
    case  4: dg04(w); break;
    case  5: break;
    case  6: dg06(w); break;
    case  7: dg07(w); break;
    case  8: dg08(w); break;
    case  9: dg09(w); break;
    case 10: dg10(w); break;
    case 11: dg11(w); break;
    case 12: dg12(w); break;
    default: gap(w, "bhEne03_DamageMode2[mode2]"); break;
    }
}

/* --- bhEne03 (en03.c:478-622) ---------------------------------------------- */
void recvx_en03(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03_STATE* s;
    uint32_t flg;
    float px, py, pz;
    if (!w) return;
    if ((w->flg & 0x4000000u)) {
        RECVX_EN03_EXP1* e1 = xp1(w);
        if (!e1) { gap(w, "bhEne03: exp1 fehlt"); return; }
        if ((e1->flg & 0x4)) player_control(w);
        if (e1->timer != 0) e1->timer--;
        return;
    }
    switch (w->mode0) {
    case 0: en03_init(w); break;
    case 1: en03_move(w); break;
    case 2: nage(w); break;
    case 3: en03_damage(w); break;
    case 4: en03_die(w); break;
    case 5:
        if (!w->sv || !w->sv->event) { gap(w, "bhEne_Event"); return; }
        w->sv->event(w);
        break;
    default:
        gap(w, "bhEne03_Mode0[mode0]");
        return;
    }

    /* bhEne03_Init legt exp0 an; erst danach lesen die uebrigen Zweige ihn. */
    if (!w->exp0) { gap(w, "bhEne03: exp0 fehlt"); return; }
    s = st(w);

    en03_call_se(w);
    if (!w->sv || !w->sv->set_motion) { gap(w, "bhSetMotion (en03)"); return; }
    w->sv->set_motion(w, w->mtn_add, w->mtn_md);

    px = w->px; py = w->py; pz = w->pz;

    if ((w->flg & 0x100000u)) {
        RECVX_ENEMY_VEC pos;
        if (!parts(w, 0, &pos)) return;
        w->aox = pos.x - w->px;
        w->aoy = pos.y - w->py;
        w->aoz = pos.z - w->pz;
        if (!(w->flg & 0x200000u)) w->aoy = 0.0f;
    } else {
        w->aox = 0.0f; w->aoy = 0.0f; w->aoz = 0.0f;
    }

    switch (s->h.surface) {
    case 0: w->car = 6.0f; w->cah = 6.0f; break;
    case 1: w->car = 6.0f; w->cah = 6.0f; w->aoy -= 6.0f; break;
    default: w->car = 6.0f; w->cah = 12.0f; w->aoy -= 6.0f; break;
    }

    if ((w->flg & 0x8u)) {
        if (!w->sv || !w->sv->check_player) { gap(w, "bhCheckPlayer (en03)"); return; }
        w->sv->check_player(w);
        recvx_t3_check_enemies_ball(w);
    }

    if (s->h.wall >= 0) {
        switch (s->h.surface) {
        case 0: case 1: w->py = py; break;
        case 2: case 4: w->px = px; break;
        case 3: case 5: w->pz = pz; break;
        default: break;
        }
    }

    if (s->h.wall < 0) {
        en03_collision_line(w);
        if ((w->flg & 0x10u) && en03_collision_walls(w) != 0) en03_collision_line(w);
    } else if ((w->flg & 0x10u)) {
        en03_collision_walls(w);
        en03_collision_line2(w);
    }

    /* en03.c:589-593 baut die Wurzelmatrix des Works aus Lage und
       Koerpermatrix zusammen; das unmittelbar folgende bhCalcModel
       ueberschreibt sie wieder (toter Schreibvorgang des Originals). */
    njUnitMatrix((NJS_MATRIX*)&w->root_mtx);
    njTranslate((NJS_MATRIX*)&w->root_mtx, w->px, w->py, w->pz);
    njMultiMatrix((NJS_MATRIX*)&w->root_mtx, bm(w));

    if (room_motion(w)) {
        flg = w->flg;
        w->flg &= ~0x1000u;
        do_calc_model(w);
        w->flg = flg;
        w->mdflg &= ~0x4u;
    } else {
        w->mdflg |= 0x4u;
    }

    if ((w->type & 0x1u)) {
        if (w->sv->weapon_atr) w->sv->weapon_atr(w, 35, 1, 5.0f);
        else gap(w, "bhEne_SetWeponAtr (en03)");
    } else {
        if (w->sv->weapon_atr) w->sv->weapon_atr(w, 35, 6, 5.0f);
        else gap(w, "bhEne_SetWeponAtr (en03)");
        return;
    }
}
