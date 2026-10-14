/* EN23 Riesen-Schwarze-Witwe — eigenstaendige native Zustandsmaschine auf
   dem einzigen WorldEntity-Work. Semantikreferenzen (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en23.c:361 bhEne23, :447 Init, :670-840 Brain,
   :843-1827 Move MV00-12, :1830 Nage, :1842-2516 Damage DG00-07,
   :2521-2956 Die DD00-03, :2961 CollisionWalls, :3094 CollisionLine,
   :3115 CheckClimbWall, :3224 CheckDiving, :3266 DamageInit, :3435 LegBreak,
   :3466 InitChild, :3518 PlayerControl, :3732 Acid, :3810 SearchPlayer,
   :3830 Shape, :3853 CallSE, :3951 HitMark. Argumentreihenfolgen der
   Zufallsaufrufe und der njRotateXYZ-Schluessel gegen die Disassembly
   (SLUS_201.84 bhEne23_DG06 0x2036F0, bhEne23_HitMark 0x2071B4) geprueft.
   Ungebundene Dienste melden den Originalnamen und erfinden keinen Wert. */
#include "en23.h"
#include "en03_lib.h"
#include "en03_collision.h"
#include "en02_trans_tables.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static const RECVX_EN23_SERVICES* en23_sv;
void recvx_en23_bind(const RECVX_EN23_SERVICES* services) { en23_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
#define SV(w, f, name) do { if (!en23_sv || !en23_sv->f) { gap(w, name); return; } } while (0)
#define BASE(w, f, name) do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return; } } while (0)

/* --- Reine Daten (en23.c:22-300) ------------------------------------------- */
static const RECVX_ENEMY_VEC spl_051[80] = {
    { 0.0f, 0.0f, 0.0f }, { 0.0f, -0.043997999f, 0.0f }, { 0.0f, -0.123427004f, 0.0f },
    { 0.0f, -0.190007001f, 0.0f }, { 0.0f, -0.243738994f, 0.0f }, { 0.0f, -0.284621f, 0.0f },
    { 0.0f, -0.312653005f, 0.0f }, { 0.0f, 0.131259993f, 0.000005f }, { 0.0f, 0.103832997f, 0.000002f },
    { 0.0f, 0.068269f, 0.000003f }, { 0.0f, 0.024565f, 0.000001f }
};
static const RECVX_ENEMY_VEC spl_052[21] = {
    { 0.0f, 0.0f, 0.0f }, { 0.323074013f, 1.16421604f, -0.000003f }, { 0.862697005f, 1.27816403f, -0.000003f },
    { 1.17025101f, 1.04479098f, -0.000004f }, { 1.121099f, 0.545728981f, -0.000004f },
    { 0.680630982f, 0.022164f, -0.000004f }, { 0.010563f, -0.280977994f, -0.000005f },
    { -0.589628994f, -0.296923995f, -0.000004f }, { -0.868438005f, -0.179964006f, -0.000004f },
    { -0.754297972f, -0.167448997f, -0.000004f }, { -0.488382012f, -0.711363971f, -0.000004f },
    { -0.407911003f, -1.540488f, -0.000004f }, { -0.329908013f, -2.07722092f, -0.000004f },
    { -0.255604f, -2.31096101f, -0.000003f }, { -0.188733995f, -2.23500705f, -0.000004f },
    { -0.131156996f, -1.84400296f, -0.000003f }, { -0.083714001f, -1.29996598f, -0.000002f },
    { -0.046722f, -0.543478012f, -0.000002f }, { -0.020282f, 0.497803003f, -0.000002f },
    { -0.004415f, 0.938849986f, -0.000001f }, { 0.000878f, 0.506936014f, 0.0f }
};
static const int8_t joint_tree[5][6] = {
    { 0, -1, 0, 0, 0, 0 }, { 0, 1, 4, 5, 6, -1 }, { 0, 1, 13, 14, 15, -1 },
    { 0, 1, 25, 26, 27, -1 }, { 0, 1, 28, 29, 30, -1 }
};
static const uint8_t flip_tree[37] = {
    0, 1, 23, 24, 25, 26, 27, 22, 8, 10, 9, 12, 11, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 7, 2, 3, 4, 5, 6, 13, 14, 15, 16, 17, 18, 19, 20, 21
};
static const int8_t SdwTab[6] = { 1, 27, 36, 6, 21, -1 };
/* BrokenParts[2][2] liegt im Original zusammenhaengend; ein Spaltenindex 2
   (njRandom()==1.0 in Float32) liest das naechste Element. */
static const uint8_t BrokenParts[4] = { 28, 31, 13, 16 };
static const RECVX_ENEMY_BP_WORK BloodParam = {
    { 0.0f, 0.1f, 0.0f }, 0, 0.0f, 0.2f, { 0.5f, 0.1f, 0.6f, 0.3f, 0.5f }, { 0, 3, 6, 9, 12 }
};
typedef struct { uint8_t flg; RECVX_ENEMY_VEC ofp; float rx, ry, rz; } EN23_BLOOD;
static const EN23_BLOOD BloodTbl[37] = {
    { 1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f }, { 0, { 0.0f, 3.0f, -5.0f }, 3.0f, 0.0f, 4.0f },
    { 1, { 0.0f, 3.0f, 5.0f }, 3.0f, 2.0f, 5.0f }, { 1, { 0.0f, 3.0f, 5.0f }, 3.0f, 2.0f, 5.0f },
    { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 1, { 0.0f, 3.0f, 5.0f }, 3.0f, 2.0f, 5.0f },
    { 1, { 0.0f, 5.0f, 8.0f }, 4.0f, 3.0f, 6.0f }, { 1, { 0.0f, 5.0f, -8.0f }, 4.0f, 3.0f, 6.0f },
    { 1, { 0.0f, 5.0f, -8.0f }, 4.0f, 3.0f, 6.0f }, { 1, { 0.0f, 5.0f, -8.0f }, 4.0f, 3.0f, 6.0f },
    { 1, { 0.0f, 5.0f, -8.0f }, 4.0f, 3.0f, 6.0f }, { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { -3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 1, { 0.0f, 3.0f, 5.0f }, 3.0f, 2.0f, 5.0f }, { 1, { 0.0f, 3.0f, 5.0f }, 3.0f, 2.0f, 5.0f },
    { 1, { 0.0f, 3.0f, 5.0f }, 3.0f, 2.0f, 5.0f }, { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }, { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f },
    { 0, { 3.0f, 1.0f, 0.0f }, 3.0f, 0.0f, 1.0f }
};
static const RECVX_ENEMY_CPCL CapColTabA[25] = {
    { 1, 1, 32 }, { 0, 0, -40 }, { 1, 1, 30 }, { 0, 0, -75 }, { 1, 1, 25 }, { 0, 0, -100 },
    { 8, 8, 75 }, { 0, 30, 65 }, { 4, 5, 6 }, { 5, 6, 6 }, { 25, 26, 6 }, { 26, 27, 6 },
    { 13, 14, 6 }, { 14, 15, 6 }, { 28, 29, 6 }, { 29, 30, 6 }, { 16, 17, 6 }, { 17, 18, 6 },
    { 31, 32, 6 }, { 32, 33, 6 }, { 19, 20, 6 }, { 20, 21, 6 }, { 34, 35, 6 }, { 35, 36, 6 },
    { 0, 0, 0 }
};
static const RECVX_ENEMY_CPCL CapColTabB[23] = {
    { 1, 1, 32 }, { 0, 0, -40 }, { 1, 1, 30 }, { 0, 0, -75 }, { 1, 1, 25 }, { 0, 0, -100 },
    { 4, 5, 6 }, { 5, 6, 6 }, { 25, 26, 6 }, { 26, 27, 6 }, { 13, 14, 6 }, { 14, 15, 6 },
    { 28, 29, 6 }, { 29, 30, 6 }, { 16, 17, 6 }, { 17, 18, 6 }, { 31, 32, 6 }, { 32, 33, 6 },
    { 19, 20, 6 }, { 20, 21, 6 }, { 34, 35, 6 }, { 35, 36, 6 }, { 0, 0, 0 }
};
typedef struct { int8_t type[3]; uint8_t bloodstain[3]; uint8_t exef; } EN23_REACT;
static const EN23_REACT DmgReact[21] = {
    { { 0, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 0, 0 }, { 0, 0, 0 }, 0 },
    { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 },
    { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 0, 0 }, { 0, 0, 0 }, 0 },
    { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 0, 0, 0 }, { 0, 0, 0 }, 0 }, { { 2, 1, 0 }, { 1, 0, 0 }, 0 },
    { { 0, 0, 0 }, { 0, 0, 0 }, 0 }, { { 1, 1, 0 }, { 0, 0, 0 }, 0 }, { { 2, 1, 0 }, { 1, 1, 1 }, 1 },
    { { -1, -1, -1 }, { 0, 0, 0 }, 2 }, { { -1, -1, -1 }, { 0, 0, 0 }, 1 }, { { -1, -1, -1 }, { 0, 0, 0 }, 0 },
    { { 2, 2, 2 }, { 1, 1, 1 }, 5 }, { { 2, 2, 2 }, { 0, 0, 0 }, 1 }, { { 2, 2, 2 }, { 1, 1, 1 }, 5 }
};
static const RECVX_T3_COMBWEP CombWepTbl[21] = {
    { 0, { 0, 0, 0 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 }, { 4, { 1, 0, 0 }, 30, 20 },
    { 10, { 4, 3, 1 }, 20, 10 }, { 10, { 4, 3, 1 }, 20, 10 }, { 10, { 4, 3, 1 }, 10, 0 },
    { 0, { 0, 0, 0 }, 25, 0 }, { 0, { 0, 0, 0 }, 25, 0 }, { 25, { 5, 3, 1 }, 5, 0 },
    { 0, { 0, 0, 0 }, 0, 0 }, { 0, { 0, 0, 0 }, 10, 0 }, { 0, { 0, 0, 0 }, 30, 0 },
    { 25, { 5, 4, 2 }, 10, 0 }, { 0, { 0, 0, 0 }, 60, 0 }, { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 }, { 15, { 1, 1, 1 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 },
    { 0, { 0, 0, 0 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 }, { 0, { 0, 0, 0 }, 0, 0 }
};
static const RECVX_T3_COMBJOINT CombJointTbl[37] = { { 0, 0 } };

/* --- Grundzugriffe --------------------------------------------------------- */
static RECVX_EN23_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN23_STATE*)w->exp0; }
static RECVX_EN03_BODY* bd(RECVX_ENEMY_WORK* w) { return &st(w)->body; }
static NJS_MATRIX* bm(RECVX_ENEMY_WORK* w) { return (NJS_MATRIX*)st(w)->body.m; }
static void set_modes(RECVX_ENEMY_WORK* w, uint8_t m0, uint8_t m1, uint8_t m2, uint8_t m3)
{
    w->mode0 = m0; w->mode1 = m1; w->mode2 = m2; w->mode3 = m3;
}
/* -rand() / -2147483648.0f */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en23)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en23)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_set) { gap(w, "plp schreiben (en23)"); return; }
    w->sv->player_set(p);
}
static uint32_t frames(RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    uint32_t n;
    if (!w->sv || !w->sv->motion_frames) { gap(w, "epw->mnwP[mtn_no].frm_num (en23)"); return 0; }
    n = w->sv->motion_frames(w, mtn);
    if (!n) gap(w, "epw->mnwP[mtn_no] fehlt (en23)");
    return n;
}
static int frames_ct(RECVX_ENEMY_WORK* w, int sub, int32_t* ct)
{
    const uint32_t n = frames(w, w->mtn_no);
    if (!n) return 0;
    *ct = (int32_t)n - sub;
    return 1;
}
static int root_pos(RECVX_ENEMY_WORK* w, uint32_t mtn, uint32_t key, NJS_VECTOR* out)
{
    RECVX_ENEMY_VEC p;
    if (!w->sv || !w->sv->motion_root_position || !w->sv->motion_root_position(w, mtn, key, &p)) {
        gap(w, "mnwP[mtn].md2P->p[0] (en23)"); return 0;
    }
    out->x = p.x; out->y = p.y; out->z = p.z;
    return 1;
}
static int root_ang(RECVX_ENEMY_WORK* w, uint32_t mtn, int32_t frm, int32_t* a)
{
    if (frm < 0 || !w->sv || !w->sv->motion_root_angles || !w->sv->motion_root_angles(w, mtn, (uint32_t)(frm >> 16), a)) {
        gap(w, "mnwP[mtn].md2P->p[1] (en23)"); return 0;
    }
    return 1;
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t j, NJS_MATRIX* m)
{
    if (!w->sv || !w->sv->joint_matrix || !w->sv->joint_matrix(w, j, (float*)m)) {
        gap(w, "mlwP->owP[n].mtx (en23)"); return 0;
    }
    return 1;
}
static void call_se(RECVX_ENEMY_WORK* w, int se)
{
    const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
    recvx_t3_call_se(w, &at, se);
}
static int route(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b, RECVX_ENEMY_VEC* out, int* r)
{
    if (!en23_sv || !en23_sv->route_points) { gap(w, "bhCheckRoute (en23)"); return 0; }
    *r = en23_sv->route_points(a, b, out);
    if (*r < 0) { gap(w, "bhCheckRoute (en23): rom->rutp"); return 0; }
    return 1;
}
static int wall_type(RECVX_ENEMY_WORK* w, const NJS_POINT3* p, float r, float h, int* hit)
{
    const RECVX_ENEMY_VEC v = { p->x, p->y, p->z };
    if (!w->sv || !w->sv->wall_type) { gap(w, "bhCheckWallType (en23)"); return 0; }
    *hit = w->sv->wall_type(&v, 0, r, h);
    return 1;
}
/* bhCollisionCheckLine(p1, p2): Wandindex oder -1. */
static int line1(RECVX_ENEMY_WORK* w, const NJS_POINT3* a, const NJS_POINT3* b)
{
    RECVX_ENEMY_VEC p1 = { a->x, a->y, a->z }, p2 = { b->x, b->y, b->z }, n;
    uint32_t id = 0;
    if (!w->sv || !w->sv->line) { gap(w, "bhCollisionCheckLine (en23)"); return -1; }
    if (!w->sv->line(1, &p1, &p2, &n, 0, 0, NULL, &id)) return -1;
    return id ? (int)id - 1 : -1;
}
static int search_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p, int ang)
{
    NJS_MATRIX m;
    NJS_POINT3 d = { w->px - p->pos.x, w->py - p->pos.y, w->pz - p->pos.z };
    memcpy(m, bd(w)->m, sizeof m);
    njInvertMatrix(&m);
    njCalcPoint(&m, &d, &d);
    bd(w)->ang = recvx_t3_arc_tan2(d.x, d.z);
    return abs(bd(w)->ang) < ang ? 1 : 0;
}
static int heading(RECVX_ENEMY_WORK* w) { return recvx_t3_arc_tan2(-bd(w)->m[2], bd(w)->m[0]); }
static void rotate_y(RECVX_ENEMY_WORK* w, int ang) { njRotateY(bm(w), (Angle)(int16_t)ang); }
static int parts(RECVX_ENEMY_WORK* w, int k, RECVX_ENEMY_VEC* out) { return recvx_en03_parts_pos(w, bd(w), joint_tree[k], out); }
static void get_wall(RECVX_ENEMY_WORK* w) { bd(w)->wall = recvx_en03_get_wall(w, bd(w)); }
static void cap_table(RECVX_ENEMY_WORK* w) { w->cpcl = (w->type & 0x1) ? CapColTabB : CapColTabA; }
/* *(int*)&mode0 bleibt bei Fehltreffer; die Flaechenliste rom->flrp + sys->mflrp. */
static int floor_at(RECVX_ENEMY_WORK* w, uint32_t i, RECVX_ENEMY_ATR* out)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    if (!x || !x->flr_at) { gap(w, "rom->flrp/sys->mflrp (en23)"); return 0; }
    return x->flr_at(i, out);
}
static int floor_count(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    if (!x || !x->flr_count) { gap(w, "rom->flr_n + sys->mflr_n (en23)"); return -1; }
    return x->flr_count();
}

/* --- Init (en23.c:447-667) ------------------------------------------------- */
static void init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s;
    float r;
    int i, gm, n;
    w->flg |= 0x1078u; w->flg &= ~0x6u;
    w->mdflg |= 0x4u;
    w->ar = 14.0f; w->ah = 20.0f; w->car = 15.0f; w->cah = 8.0f;
    w->hokan_rate = 65536; w->hokan_count = 0;
    w->mtn_no = 0; w->mtn_md = 0; w->mtn_add = 65536;
    w->frm_no = 0;
    w->mtn_tp = flip_tree;
    BASE(w, calc_model, "bhCalcModel (en23)");
    w->sv->calc_model(w);
    set_modes(w, 1, 1, 1, 0);
    if (!w->exp0) {
        RECVX_ENEMY_TEMPLATE t = {0};
        BASE(w, calloc_work, "bhEne_CallocWork (en23)");
        SV(w, create_child, "bhEne23_Init: eneset.c:170 Kindwork");
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN23_STATE));
        if (!w->exp0) { gap(w, "bhEne23_Init: EXP-Budget erschoepft"); return; }
        s = st(w);
        t.flg = 0x1u; t.id = 31;                                 /* ene23_child */
        for (i = 0; i < 2; i++) {
            RECVX_ENEMY_WORK* c = en23_sv->create_child(w, &t);
            if (!c) { gap(w, "bhEne23_Init: ene[] voll"); return; }
            s->legs[i] = c;
            c->type = 1;
            c->lkwkp = w; c->lkono = i;
            c->lox = 0; c->loy = 0; c->loz = 0;
            c->mdflg |= 0x1u;
            if (!rnd(w, &r)) return;
            {
                const int k = i * 2 + (int)(2.0f * r);
                if (k > 3) { gap(w, "bhEne23_Init: BrokenParts[1][2] liest hinter die Tabelle"); return; }
                s->broken_node[i] = BrokenParts[k];
            }
            s->leg_link[i].owner = w;
            s->leg_link[i].node = s->broken_node[i];
            c->exp1 = &s->leg_link[i];                          /* mtx = &owk->mtx; exp1 = owk */
        }
        if (!recvx_enemy_ext() || !recvx_enemy_ext()->set_call_func) { gap(w, "bhEne_SetCallFunc(bhEne03s, 31)"); return; }
        recvx_enemy_ext()->set_call_func(31u, recvx_en03s);
    }
    s = st(w);
    s->body.ar = 14.0f;
    if (!rnd(w, &r)) return;
    s->timer = (int)(30.0f * r) + 30;
    s->wall_ofs = 10.4f;
    SV(w, game_mode, "bhEne23_Init: sys->gm_mode");
    if (!en23_sv->game_mode(&gm)) { gap(w, "bhEne23_Init: sys->gm_mode"); return; }
    if (w->type & 0x1) w->hp = ((gm != 2) ? 250 : 160) - ((gm != 2) ? 100 : 50);
    else { w->hp = (gm != 2) ? 250 : 160; s->hp2 = (gm != 2) ? 100 : 50; }
    s->babies_n = 0;
    BASE(w, enemy_count, "sys->ewk_n (en23)");
    BASE(w, enemy_at, "ene[] (en23)");
    BASE(w, link_model, "epw->mlwP = &ep->mdl[k] (en23)");
    n = (int)w->sv->enemy_count();
    for (i = 0; i < n; i++) {
        RECVX_ENEMY_WORK* ep = w->sv->enemy_at((uint32_t)i);
        if (!ep || !(ep->flg & 0x1u) || ep->id != 24) continue;
        if (s->babies_n >= (int32_t)RECVX_EN23_BABIES) { gap(w, "bhEne23_Init: mehr als 30 id-24-Works ueberschreiben exp0[248]"); return; }
        s->babies[s->babies_n++] = ep;
        ep->lkwkp = w; ep->lkono = 0;
        ep->lox = 0; ep->loy = 0; ep->loz = 0;
        if (!w->sv->link_model(ep, w, 2)) gap(w, "bhEne23_Init: ep->mlwP = &epw->mdl[2]");
    }
    s->leg_broken[0] = s->leg_broken[1] = 0;                    /* exp0[258..265] = 0 */
    s->floor_valid = 0; s->climb = 0;
    njUnitMatrix(bm(w));
    njRotateY(bm(w), (Angle)w->ay);
    w->flg &= ~0x4000000u;
    if (w->type & 0x2) {
        RECVX_ENEMY_VEC a = { w->px, w->py, w->pz }, b = { w->px, w->py + 999.0f, w->pz }, nrm;
        BASE(w, line, "bhCollisionCheckLine (en23)");
        if (w->sv->line(1, &a, &b, &nrm, 0, 0, NULL, NULL)) {
            s->body.surface = 1;
            recvx_en03_make_matrix(&s->body);
            w->py = b.y;
            w->flg |= 0x4000000u;
        }
        w->type &= (uint16_t)~0x2u;
    }
    if (!(w->flg & 0x4000000u)) { s->body.surface = 0; w->flg |= 0x4000000u; }
    w->pxb = w->px; w->pyb = w->py; w->pzb = w->pz;
    get_wall(w);
    w->clp_jno[0] = 1; w->clp_jno[1] = 10; w->clp_jno[2] = 27; w->clp_jno[3] = 36;
    w->clp_jno[4] = 6; w->clp_jno[5] = 21; w->clp_jno[6] = 24; w->clp_jno[7] = 3;
    w->mdflg &= ~0x20u;
    if (!(w->flg & 0x800u)) {
        int sdw;
        BASE(w, set_shadow, "bhSetShadow (en23)");
        SV(w, effect_id, "bhEne23_Init: eff[sdw].id = 258");
        sdw = w->sv->set_shadow(SdwTab, w, 0, 16.0f, 18.0f, 18.0f);
        if (sdw < 0) { gap(w, "bhEne23_Init: bhSetShadow -1 (eff[-1].id)"); return; }
        en23_sv->effect_id(sdw, 258);
        w->flg |= 0x800u;
    }
    w->stflg &= ~0x8u;
    if (w->type & 0x4) {
        w->type &= (uint16_t)~0x4u;
        w->flg &= ~0x60u; w->flg |= 0x8000000u;
        w->stflg |= 0x8u;
        w->ar = 13.0f;
    } else w->flg &= ~0x8000000u;
    cap_table(w);
    recvx_en03_hide_parts(w, 1, 0);
    if (w->type & 0x1) recvx_en03_hide_parts(w, 8, 1);
    recvx_en03_set_model_flg(w, (uint32_t)-4, 0);
    SV(w, room_flags, "sys->rm_flg (en23)");
    en23_sv->room_flags(0, 0x1u);
    if (!(w->type & 0x1)) {
        /* mlwP = mdl; obj_a = mdl[0].objP; obj_b = mdl[1].objP; mdflg |= 2 */
        BASE(w, link_model, "epw->mlwP = epw->mdl (en23)");
        if (!w->sv->link_model(w, w, 0)) gap(w, "bhEne23_Init: mlwP = mdl");
        SV(w, morph_sources, "bhEne23_Init: obj_a/obj_b");
        en23_sv->morph_sources(w, 0, 1);
        w->mdflg |= 0x2u;
        w->shp_ct = 0;
    }
}

/* --- Brain (en23.c:670-840) ------------------------------------------------ */
static void timer_rand(RECVX_ENEMY_WORK* w, float mul, int add)
{
    float r;
    if (!rnd(w, &r)) return;
    st(w)->timer = (int)(mul * r) + add;
}
static void br(RECVX_ENEMY_WORK* w, int type)
{
    RECVX_EN23_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    if (!player(w, &p)) return;
    s->body.dist = njSqrt(((w->px - p.pos.x) * (w->px - p.pos.x)) + ((w->pz - p.pos.z) * (w->pz - p.pos.z)));
    search_player(w, &p, 8192);
    if (type == 0) {
        if (w->flg & 0x8000000u) return;
        if (s->timer != 0) s->timer--;
    } else if (!(w->flg & 0x8000000u) && s->timer != 0) s->timer--;
    if (s->body.surface == 0) {
        if (w->flr_no != p.flr_no && !(p.stflg & 0x20u)) { w->mode1 = 0; w->mode2 = 2; w->mode3 = 0; return; }
        if ((p.stflg & 0x80000000u) || (w->flg & 0x4u)) return;
        if (s->timer != 0) return;
        if (s->body.dist < 30.0f && search_player(w, &p, type == 0 ? 5461 : 3640) && fabsf(w->py - p.pos.y) < 5.0f) {
            set_modes(w, 1, 0, 10, 0);
            timer_rand(w, 20.0f, 10);
            return;
        }
        if (type == 0 && s->body.dist > 35.0f && search_player(w, &p, 5461)) {
            set_modes(w, 1, 0, 9, 0);
            timer_rand(w, 20.0f, 10);
        }
    } else if (s->body.surface == 1) {
        if (p.flr_no == 0 && !(p.stflg & 0x20u)) { set_modes(w, 1, 0, 3, 0); return; }
        if (s->timer != 0) return;
        if (s->body.dist < 28.0f && fabsf(p.pos.y - w->py) < 45.0f && search_player(w, &p, 3640)) {
            set_modes(w, 1, 0, 10, 0);
            timer_rand(w, 20.0f, 40);
            return;
        }
        if (type == 0 && s->body.dist < 35.0f && search_player(w, &p, 10922)) {
            set_modes(w, 1, 0, 9, 0);
            timer_rand(w, 20.0f, 20);
        }
    }
}

/* --- Bewegung (en23.c:862-1827) ------------------------------------------- */
static void walk_entry(RECVX_ENEMY_WORK* w, uint32_t mno)
{
    if (w->mtn_no != mno) {
        w->frm_no = 0; w->mtn_no = mno;
        w->hokan_count = 10; w->hokan_rate = 32768;
        if (w->mtn_md & 0x2u) { w->frm_no = 589824; w->mtn_md &= ~0x2u; }
    }
}
static void mv00(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2] = { 0, 53 };
    RECVX_ENEMY_PLAYER p;
    float r, dist;
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != mtn[w->type]) {
            w->mtn_no = mtn[w->type]; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
        }
        w->mtn_add = 65536;
        w->flg |= 0x10000000u;
        if (!rnd(w, &r)) return;
        w->ct0 = (int)(30.0f * r) + 20;
        w->mode3++;
        /* fall through */
    case 1:
        st(w)->wall_ofs = 0;
        if (w->ct0 != 0) w->ct0--;
        else {
            if (!player(w, &p)) return;
            dist = njSqrt(((p.pos.x - w->px) * (p.pos.x - w->px)) + ((p.pos.z - w->pz) * (p.pos.z - w->pz)));
            if (st(w)->body.surface == 0) dist -= 30.0f;
            if (st(w)->body.surface == 1) dist -= 28.0f;
            if (dist > 0 || search_player(w, &p, 1820) == 0) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
        }
        break;
    }
}
static void mv01(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2] = { 2, 22 };
    static const float spd[2] = { 0.7f, 1.0f };
    RECVX_EN23_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC pos;
    float r, dist;
    int rt;
    switch (w->mode3) {
    case 0:
        walk_entry(w, mtn[w->type]);
        w->mtn_add = 65536;
        w->flg |= 0x10000000u;
        if (!rnd(w, &r)) return;
        w->ct0 = (int32_t)(47.0f * r);
        if (!rnd(w, &r)) return;
        w->ct1 = r < 0.5f ? -1 : 1;
        w->spd = spd[w->type];
        s->body.ar = s->body.surface == 0 ? 14.0f : 20.0f;
        if (w->flg & 0x8000000u) {
            w->mtn_no = 56; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            w->spd = 0.35f;
        }
        if (!rnd(w, &r)) return;
        w->ct0 = (int)(30.0f * r) + 20;
        w->mode3++;
        /* fall through */
    case 1:
        recvx_en03_go_ahead(w, bd(w));
        s->wall_ofs = 0;
        if (!player(w, &p)) return;
        if (w->flg & 0x8000000u) {
            const RECVX_ENEMY_VEC sp = { w->px, 0, w->pz }, dp = { p.pos.x, 0, p.pos.z };
            if (!route(w, &sp, &dp, &pos, &rt)) return;
            if ((rt & 0xFF) != 0xFF) { w->xn += (pos.x - w->xn) / 32.0f; w->zn += (pos.z - w->zn) / 32.0f; }
            else { w->xn = p.pos.x; w->zn = p.pos.z; }
            { const RECVX_ENEMY_VEC t = { w->xn, w->yn, w->zn }; w->way = (int16_t)recvx_en03_dir_target_body(w, bd(w), &t, 227); }
        } else {
            const RECVX_ENEMY_VEC a = { w->px, w->py, w->pz };
            if (!route(w, &a, &p.pos, &pos, &rt)) return;
            if ((rt & 0xFF) != 0xFF) { w->xn += (pos.x - w->xn) / 16.0f; w->zn += (pos.z - w->zn) / 16.0f; }
            else { w->xn = p.pos.x; w->zn = p.pos.z; }
            dist = njSqrt(((w->xn - w->px) * (w->xn - w->px)) + ((w->zn - w->pz) * (w->zn - w->pz)));
            { const RECVX_ENEMY_VEC t = { w->xn, w->yn, w->zn }; w->way = (int16_t)recvx_en03_dir_target_body(w, bd(w), &t, dist > 30.0f ? 455 : 728); }
        }
        rotate_y(w, w->way);
        if (w->ct0 != 0) { w->ct0--; return; }
        w->mode1 = 1;
        dist = njSqrt(((p.pos.x - w->px) * (p.pos.x - w->px)) + ((p.pos.z - w->pz) * (p.pos.z - w->pz)));
        if (s->body.surface == 0) dist -= 32.0f;
        if (s->body.surface == 1) dist -= 30.0f;
        if (dist < 0 && search_player(w, &p, 3640)) { w->mode1 = 1; w->mode2 = 0; w->mode3 = 0; }
        break;
    }
}
static int check_climb_wall(RECVX_ENEMY_WORK* w, int flg);
static int check_diving(RECVX_ENEMY_WORK* w);
/* MV02/MV03: naechste Flaeche type 2, prm0 23 (prm1 < 4 bzw. == 4) in rom->flrp + sys->mflrp. */
static int pick_floor(RECVX_ENEMY_WORK* w, int ceiling)
{
    RECVX_EN23_STATE* s = st(w);
    float min = 9999.0f;
    int i, n = floor_count(w);
    if (n < 0) return 0;
    for (i = 0; i < n; i++) {
        RECVX_ENEMY_ATR fp;
        float dx, dz, dist;
        if (!floor_at(w, (uint32_t)i, &fp)) continue;
        if (!((fp.flg & 0x1u) && fp.type == 2 && fp.prm0 == 23 && (ceiling ? fp.prm1 == 4 : fp.prm1 < 4))) continue;
        dx = (fp.px + (fp.w / 2.0f)) - w->px;
        dz = (fp.pz + (fp.d / 2.0f)) - w->pz;
        dist = (dx * dx) + (dz * dz);
        if (dist < min) {
            min = dist;
            s->body.target.x = fp.px + (fp.w / 2.0f);
            s->body.target.y = ceiling ? fp.py + (fp.h / 2.0f) : w->py;
            s->body.target.z = fp.pz + (fp.d / 2.0f);
        }
    }
    return 1;
}
static void mv02_03(RECVX_ENEMY_WORK* w, int ceiling)
{
    static const uint32_t mtn[2] = { 2, 22 };
    static const float spd[2] = { 0.7f, 1.0f };
    RECVX_EN23_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float r;
    int rt;
    switch (w->mode3) {
    case 0:
        walk_entry(w, mtn[w->type]);
        w->mtn_add = 65536;
        w->flg |= 0x10000000u;
        if (!rnd(w, &r)) return;
        w->ct0 = (int)(47.0f * r) + 30;
        if (!rnd(w, &r)) return;
        w->ct1 = r < 0.5f ? -1 : 1;
        w->spd = spd[w->type];
        if (!pick_floor(w, ceiling)) return;
        w->mode3++;
        /* fall through */
    case 1: {
        RECVX_ENEMY_VEC out;
        recvx_en03_go_ahead(w, bd(w));
        if (!ceiling) {
            const int fno = w->frm_no / 65536;
            if (fno >= 0 && fno <= 10) s->climb = check_climb_wall(w, 1);
            else s->climb = -check_climb_wall(w, -1);
            if (s->climb != 0) { w->mode1 = 0; w->mode2 = 6; w->mode3 = 0; break; }
            {
                const RECVX_ENEMY_VEC a = { w->px, w->py, w->pz };
                out.x = w->xn; out.y = w->yn; out.z = w->zn;
                if (!route(w, &a, &s->body.target, &out, &rt)) return;
                w->xn = out.x; w->yn = out.y; w->zn = out.z;
                if ((rt & 0xFF) == 0xFF) { w->xn = s->body.target.x; w->zn = s->body.target.z; }
            }
        } else {
            if (!player(w, &p)) return;
            {
                const RECVX_ENEMY_VEC a = { w->px, p.pos.y, w->pz };
                out.x = w->xn; out.y = w->yn; out.z = w->zn;
                if (!route(w, &a, &s->body.target, &out, &rt)) return;
                w->xn = out.x; w->yn = out.y; w->zn = out.z;
                if ((rt & 0xFF) == 0xFF) { w->xn = s->body.target.x; w->zn = s->body.target.z; }
            }
        }
        { const RECVX_ENEMY_VEC t = { w->xn, w->yn, w->zn }; w->way = (int16_t)recvx_en03_dir_target_body(w, bd(w), &t, 455); }
        rotate_y(w, w->way);
        if (ceiling && check_diving(w)) set_modes(w, 1, 0, 11, 0);
        if (w->ct0 == 0) {
            w->mode1 = 1;
            if (!player(w, &p)) return;
            if (!ceiling ? p.flr_no == w->flr_no : p.flr_no != 0) { w->mode1 = 1; w->mode2 = 1; w->mode3 = 0; }
        } else w->ct0--;
        break;
    }
    }
}
static void mv05(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2] = { 3, 24 };
    RECVX_EN23_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->flg |= 0x400000u;
        if (s->climb > 0) w->mtn_no = (uint32_t)((s->climb + (int)mtn[w->type]) - 1);
        else { w->mtn_no = (uint32_t)(((int)mtn[w->type] - s->climb) - 1); w->mtn_md |= 0x2u; }
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg &= ~0x18u; w->flg |= 0x180000u; w->flg |= 0x10000000u;
        w->flg2 |= 0x1u;
        w->flg |= 0x1800000u;
        if (!frames_ct(w, 0, &w->ct0)) return;
        w->mode3++;
        break;
    case 1:
        if (w->frm_no == 0) {
            RECVX_ENEMY_VEC p;
            NJS_MATRIX o;
            if (!parts(w, 0, &p)) return;
            w->px = p.x; w->py = p.y; w->pz = p.z;
            if (!joint(w, 0, &o)) return;
            memcpy(s->body.m, o, sizeof o);
            s->body.surface = 1;
            recvx_en03_make_matrix(&s->body);
            w->flg &= ~0x180000u;
            w->mode1 = 1; w->mode2 = 1; w->mode3 = 1;
            if (w->type == 0) { w->mtn_no = 2; w->frm_no = 0; w->ct0 = 10; w->spd = 0.7f; }
            else { w->mtn_no = 22; w->frm_no = 0; w->ct0 = 10; w->spd = 1.0f; }
            recvx_en03_go_ahead(w, bd(w));
            w->flg |= 0x18u; w->flg &= ~0x1800000u;
            w->flg |= 0x4000000u; w->flg &= ~0x400000u;
            get_wall(w);
            s->body.ar = 20.0f;
        }
        break;
    }
}
static void mv06(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2] = { 2, 22 };
    RECVX_EN23_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        if (w->mtn_no != mtn[w->type]) {
            w->frm_no = 0; w->mtn_no = mtn[w->type];
            w->hokan_count = 10; w->hokan_rate = 32768;
        }
        w->flg |= 0x10000000u;
        w->mtn_add = 65536;
        if (!s->floor_valid) { gap(w, "bhEne23_MV06: EXP0_I(260) NULL"); return; }
        w->ayp = (int16_t)((s->floor.prm1 & 0x3) * -16384);
        w->ct0 = 8;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0 > 0) {
            w->ay = heading(w);
            rotate_y(w, (int16_t)(w->ayp - w->ay) / w->ct0);
            w->px += (w->xn - w->px) / w->ct0;
            w->pz += (w->zn - w->pz) / w->ct0;
            w->ct0--;
        } else { w->mode1 = 0; w->mode2 = 5; w->mode3 = 0; }
        break;
    }
}
static void acid(RECVX_ENEMY_WORK* w);
static void mv09(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mtn_no = 11; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg |= 0x10000000u;
        if (!frames_ct(w, 1, &w->ct0)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) set_modes(w, 1, 1, 1, 0);
        if (w->frm_no > 1835008 && w->frm_no < 3014656) acid(w);
        break;
    }
}
static int hit_player(RECVX_ENEMY_WORK* w, const NJS_POINT3* pos)
{
    const RECVX_ENEMY_VEC v = { pos->x, pos->y, pos->z };
    return recvx_t3_attack_hit_check(&v, 5.0f);
}
static void mv10(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2][2] = { { 10, 12 }, { 27, 28 } };
    RECVX_EN23_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    switch (w->mode3) {
    case 0:
        w->mtn_no = mtn[w->type][s->body.surface == 0 ? 0 : 1];
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg &= ~0x10000000u;
        if (!frames_ct(w, 1, &w->ct0)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            if (w->ct1 != 0) set_modes(w, 1, 1, 0, 0);
            else set_modes(w, 1, 1, 1, 0);
        }
        break;
    }
    s->wall_ofs = 0;
    if (!player(w, &p)) return;
    if (!(p.flg & 0x4u) && !(p.stflg & 0x80000000u) && (p.stflg & 0x30u) == 0) {
        if ((s->body.surface == 0 && w->frm_no >= 589824 && w->frm_no <= 720896) ||
            (s->body.surface == 1 && w->frm_no >= 327680 && w->frm_no <= 589824)) {
            NJS_MATRIX m;
            NJS_POINT3 pos;
            RECVX_ENEMY_VEC pp;
            pos.x = -7.0f; pos.y = 0; pos.z = 0;
            if (!joint(w, 6, &m)) return;
            njCalcPoint(&m, &pos, &pos);
            w->ct1 = hit_player(w, &pos);
            if (!parts(w, 1, &pp)) return;
            pos.x = pp.x; pos.y = pp.y; pos.z = pp.z;
            w->ct1 |= hit_player(w, &pos);
            pos.x = 7.0f; pos.y = 0; pos.z = 0;
            if (!joint(w, 27, &m)) return;
            njCalcPoint(&m, &pos, &pos);
            w->ct1 |= hit_player(w, &pos);
            if (!parts(w, 3, &pp)) return;
            pos.x = pp.x; pos.y = pp.y; pos.z = pp.z;
            w->ct1 |= hit_player(w, &pos);
            if (w->ct1 != 0) {
                const RECVX_T3_LINK lk = { NULL, 1 };
                s->timer += 20;
                recvx_t3_set_blood_effect5(lk, 9, 2);
                if (!player(w, &p)) return;
                if (!(p.flg & 0x80000000u)) p.hp -= 40;
                p.flg |= 0x10004u;
                p.stflg |= 0x10000u;
                if (p.hp >= 0) {
                    const int far = abs((int16_t)(w->ay = heading(w), w->ay - p.ay)) > 16384;
                    if (s->body.surface == 0) { p.mode0 = 2; p.mode2 = 1; p.mode3 = 0; p.mode1 = far ? 0 : 1; }
                    else { p.mode0 = 4; p.mode1 = 0; p.mode3 = 0; p.mode2 = far ? 0 : 1; }
                }
                put_player(w, &p);
            }
        }
    }
}
static void mv11(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2][2] = { { 37, 0 }, { 42, 53 } };
    RECVX_EN23_STATE* s = st(w);
    switch (w->mode3) {
    case 0:
        w->mtn_no = mtn[w->type][0]; w->frm_no = 0;
        w->mtn_add = 65536;
        s->body.ar = 14.0f; w->ar = 14.0f;
        w->flg &= ~0x20u; w->flg &= ~0x4100000u;
        w->flg |= 0x10000000u;
        w->mode3++;
        break;
    case 1:
        if (w->frm_no == 0) {
            RECVX_ENEMY_VEC p;
            NJS_VECTOR k;
            RECVX_T3_ATR a;
            set_modes(w, 1, 1, 0, 0);
            if (!parts(w, 0, &p)) return;
            w->px = p.x; w->py = p.y; w->pz = p.z;
            w->mtn_no = mtn[w->type][1]; w->frm_no = 0;
            if (!root_pos(w, w->mtn_no, 0, &k)) return;
            njCalcVector(bm(w), &k, &k);
            w->px -= k.x; w->py -= k.y; w->pz -= k.z;
            s->body.surface = 0;
            if (!(w->type & 0x1)) njRotateY(bm(w), 32768);
            recvx_en03_make_matrix(&s->body);
            s->wall_ofs = 0;
            w->py += 1.0f;
            get_wall(w);
            if (s->body.wall < 0 || !recvx_enemy_ext() || !recvx_enemy_ext()->wal_count || !recvx_enemy_ext()->wal_at ||
                recvx_enemy_ext()->wal_count() <= s->body.wall || !recvx_enemy_ext()->wal_at((uint32_t)s->body.wall, &a)) {
                gap(w, "bhEne23_MV11: ((ATR_WORK*)EXP0_I(96))->py (NULL)"); return;
            }
            w->py = a.py;
            w->flg |= 0x20u; w->flg |= 0x4000000u;
            w->flg2 &= ~0x1u;
        }
        break;
    }
}
static void mv12(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2] = { 0, 53 };
    RECVX_EN23_STATE* s = st(w);
    RECVX_ENEMY_VEC p;
    NJS_VECTOR k;
    NJS_MATRIX o;
    w->flg |= 0x1078u; w->flg &= ~0x6u; w->flg &= ~0x8000000u;
    w->stflg &= ~0x8u; w->mdflg |= 0x4u;
    w->flg |= 0x4000000u;
    njUnitMatrix(bm(w));
    if (!parts(w, 0, &p)) return;
    w->px = p.x; w->py = p.y; w->pz = p.z;
    w->type &= 0x1;
    w->mtn_no = mtn[w->type]; w->frm_no = 0;
    w->mtn_add = 65536;
    w->mtn_md &= ~0x20u;
    if (!root_pos(w, w->mtn_no, 0, &k)) return;
    njCalcVector(bm(w), &k, &k);
    w->px -= k.x; w->py -= k.y; w->pz -= k.z;
    w->pxb = w->px; w->pyb = w->py; w->pzb = w->pz;
    if (!joint(w, 0, &o)) return;
    s->body.surface = o[5] > 0 ? 0 : 1;
    recvx_en03_make_matrix(&s->body);
    get_wall(w);
    set_modes(w, 1, 1, 1, 0);
    if (w->type & 0x4) {
        w->type &= (uint16_t)~0x4u;
        w->flg |= 0x8000000u; w->flg &= ~0x60u;
        w->stflg |= 0x8u;
        w->ar = 13.0f;
    } else w->flg &= ~0x8000000u;
    cap_table(w);
    recvx_en03_hide_parts(w, 1, 0);
    if (w->type & 0x1) recvx_en03_hide_parts(w, 8, 1);
}
static void damage_init(RECVX_ENEMY_WORK* w);
static void move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1) {
        if (w->type == 0) br(w, 0);
        else if (w->type == 1) br(w, 1);
        else { gap(w, "bhEne23_BrainType[type]"); return; }
    }
    if (w->mode0 == 1) {
        switch (w->mode2) {
        case 0: mv00(w); break;
        case 1: mv01(w); break;
        case 2: mv02_03(w, 0); break;
        case 3: mv02_03(w, 1); break;
        case 4: case 7: case 8: break;          /* MV04/07/08 original leer */
        case 5: mv05(w); break;
        case 6: mv06(w); break;
        case 9: mv09(w); break;
        case 10: mv10(w); break;
        case 11: mv11(w); break;
        case 12: mv12(w); break;
        default: gap(w, "bhEne23_MoveMode2[mode2]"); return;
        }
    }
    if ((w->flg & 0x4u) && !(w->flg & 0x2u)) damage_init(w);
}

/* --- Schaden (en23.c:1842-2516) ------------------------------------------ */
static void hit_mark(RECVX_ENEMY_WORK* w);
static void timer_back(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    s->timer -= 20;
    if (s->timer < 0) s->timer = 0;
}
static void dg01_03(RECVX_ENEMY_WORK* w, int dg03)
{
    static const uint32_t mtn01[2][2] = { { 14, 18 }, { 29, 33 } };
    static const uint32_t mtn03[2] = { 16, 31 };
    switch (w->mode3) {
    case 0:
        if (dg03) w->mtn_no = mtn03[w->type] + (uint32_t)recvx_en03_dg_dir_check(w, bd(w));
        else w->mtn_no = mtn01[w->type][st(w)->body.surface == 0 ? 0 : 1] + (uint32_t)recvx_en03_dg_dir_check(w, bd(w));
        w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg |= 0x10000000u;
        if (!frames_ct(w, 1, &w->ct0)) return;
        w->mode3++;
        /* fall through */
    case 1:
        if (w->ct0-- == 0) {
            set_modes(w, 1, 1, 1, 0);
            w->flg &= ~0x4u;
            timer_back(w);
        }
        break;
    }
    st(w)->wall_ofs = 0;
}
static void init_child(RECVX_ENEMY_WORK* w);
static int leg_blood_pos(RECVX_ENEMY_WORK* w, float sx, float ox, float sy, float oy, float sz, float oz, NJS_POINT3* out)
{
    NJS_MATRIX m;
    float a, b, c;
    if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return 0;
    out->x = (sx * a) - ox;
    out->y = (8.0f + (sy * b)) - oy;
    out->z = (8.0f + (sz * c)) - oz;
    if (!joint(w, 8, &m)) return 0;
    njCalcPoint(&m, out, out);
    return 1;
}
static void dg04(RECVX_ENEMY_WORK* w)
{
    NJS_POINT3 pos;
    NJS_MATRIX m;
    int i;
    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_VEC p;
        w->mtn_no = 17; w->frm_no = 0;
        w->hokan_count = 10; w->hokan_rate = 32768;
        w->mtn_add = 65536;
        w->flg |= 0x10000000u;
        if (!frames_ct(w, 1, &w->ct0)) return;
        w->ct1 = 0;
        w->mode3++;
        recvx_en03_hide_parts(w, 8, 1);
        recvx_t3_erase_arrow(w, 8);
        w->cpcl = CapColTabB;
        w->type = 1;
        pos.x = 0; pos.y = 6.0f; pos.z = 8.0f;
        if (!joint(w, 8, &m)) return;
        njCalcPoint(&m, &pos, &pos);
        p.x = pos.x; p.y = pos.y; p.z = pos.z;
        recvx_t3_set_blood_effect4(&p, &p, 1, 9, 2);
        recvx_t3_set_blood_effect4(&p, &p, 1, 9, 1);
        recvx_t3_set_bloodstain(w, 0, 8, NULL);
        recvx_t3_set_bloodstain(w, 0, 8, NULL);
        recvx_t3_set_bloodstain(w, 0, 8, NULL);
        for (i = 0; i < 6; i++) {
            NJS_POINT3 d;
            if (!leg_blood_pos(w, 8.0f, 4.0f, 8.0f, 4.0f, 12.0f, 6.0f, &d)) return;
            w->dpx = d.x; w->dpy = d.y; w->dpz = d.z;
            recvx_t3_set_mince_effect2(w, 258, 0.5f, 1);
            recvx_t3_set_mince_effect2(w, 259, 0.5f, 1);
        }
        init_child(w);
        SV(w, room_flags, "sys->rm_flg (en23)");
        en23_sv->room_flags(0x1u, 0);
        call_se(w, 74516);
    }
        /* fall through */
    case 1:
        if (w->ct1 == 0) {
            RECVX_ENEMY_VEC p;
            if (!leg_blood_pos(w, 8.0f, 4.0f, 8.0f, 4.0f, 12.0f, 6.0f, &pos)) return;
            p.x = pos.x; p.y = pos.y; p.z = pos.z;
            recvx_t3_set_blood_effect4(&p, &p, 1, 9, 2);
            {
                float a, b, c;
                if (!rnd(w, &a) || !rnd(w, &b) || !rnd(w, &c)) return;
                pos.x = (12.0f * a) - 6.0f;
                pos.y = (8.0f + (12.0f * b)) - 8.0f;
                pos.z = (8.0f + (16.0f * c)) - 8.0f;
                if (!joint(w, 8, &m)) return;
                njCalcPoint(&m, &pos, &pos);
                p.x = pos.x; p.y = pos.y; p.z = pos.z;
                recvx_t3_set_blood_effect4(&p, &p, 1, 9, 2);
            }
            w->ct1 = 3;
        } else w->ct1--;
        if (w->ct0-- == 0) {
            set_modes(w, 1, 1, 1, 0);
            w->flg &= ~0x4u;
            timer_back(w);
        }
        break;
    }
    st(w)->wall_ofs = 0;
}
/* DG06/DG07/DD01/DD03 Kopf: Lage auf die Wurzel, Schluesselwinkel in die
   Lagematrix (Spiegelung mit mtn_md 0x2), Absprungvektor, neue Motion. */
static int fall_head(RECVX_ENEMY_WORK* w, uint32_t next, int rot_x, int vel_mode, int keep_wall)
{
    RECVX_EN23_STATE* s = st(w);
    RECVX_ENEMY_VEC p;
    int32_t a[3];
    NJS_VECTOR k;
    if (!parts(w, 0, &p)) return 0;
    w->px = p.x; w->py = p.y; w->pz = p.z;
    if (!root_ang(w, w->mtn_no, w->frm_no, a)) return 0;
    if (w->mtn_md & 0x2u) njRotateXYZ(bm(w), (Angle)a[0], (Angle)-a[1], (Angle)-a[2]);
    else njRotateXYZ(bm(w), (Angle)a[0], (Angle)a[1], (Angle)a[2]);
    if (vel_mode == 2 || (vel_mode == 1 && (w->flg & 0x400000u))) {
        s->body.vel.x = 3.0f * s->body.m[4];
        s->body.vel.y = vel_mode == 2 ? 3.0f * s->body.m[5] : 0;
        s->body.vel.z = 3.0f * s->body.m[6];
    } else if (vel_mode == 3) {
        s->body.vel.x = 3.0f * s->body.m[4]; s->body.vel.y = 0; s->body.vel.z = 3.0f * s->body.m[6];
    } else { s->body.vel.x = s->body.vel.y = s->body.vel.z = 0; }
    if (rot_x) njRotateX(bm(w), 32768);
    w->mtn_md |= 0x100u;
    w->mtn_no = next; w->frm_no = 0;
    w->mtn_add = 0;
    w->mtn_md &= ~0x2u;
    w->hokan_count = 30; w->hokan_rate = 45875;
    if (!root_ang(w, w->mtn_no, 0, a)) return 0;
    if (!recvx_en03_bind_root_angles(w, a)) return 0;
    if (!root_pos(w, w->mtn_no, 0, &k)) return 0;
    njCalcVector(bm(w), &k, &k);
    w->px -= k.x; w->py -= k.y; w->pz -= k.z;
    w->flg &= ~0x4000000u;
    if (keep_wall && (w->flg & 0x400000u)) s->body.wall = s->body.wall2;
    w->flg |= 0x30u;
    w->flg &= ~0x400000u; w->flg &= ~0x180000u; w->flg &= ~0x1800000u;
    s->body.ar = 14.0f;
    w->flg |= 0x10000000u;
    w->ct0 = 8;
    w->mode3++;
    return 1;
}
/* Gemeinsamer Fall: Lagematrix in ct0 Schritten aufrichten (njOuterProduct mit
   der Hochachse), bis bhEne23_CollisionLine flg 0x4000000 setzt. */
static void fall_step(RECVX_ENEMY_WORK* w, int dg06_ang, int32_t landed_frm, int clear_flg2)
{
    RECVX_EN23_STATE* s = st(w);
    if (w->ct0 > 0) {
        NJS_VECTOR v = { 0, 1.0f, 0 }, ov, up = { s->body.m[4], s->body.m[5], s->body.m[6] };
        NJS_MATRIX r;
        const float out = njOuterProduct(&up, &v, &ov);
        njUnitMatrix(&r);
        if (out > 0) {
            int ang;
            njUnitVector(&ov);
            if (dg06_ang) {
                ang = (int)(10430.381f * asinf(out));
                if (s->body.m[5] < 0) ang = 32768 - ang;
                njRotate(&r, &ov, (Angle)(ang / w->ct0));
            } else {
                ang = (int)(10430.381f * asinf(out)) / w->ct0;
                njRotate(&r, &ov, (Angle)ang);
            }
        } else njRotateX(&r, (Angle)(32768 / w->ct0));
        njMultiMatrix(&r, bm(w));
        memcpy(s->body.m, r, sizeof r);
        w->ct0--;
    }
    if (w->flg & 0x4000000u) {
        s->body.surface = 0;
        recvx_en03_make_matrix(&s->body);
        w->mtn_add = 65536;
        w->frm_no = landed_frm;
        w->mtn_md &= ~0x100u;
        if (!frames_ct(w, 2, &w->ct0)) return;
        if (clear_flg2) w->flg2 &= ~0x1u;
        w->mode3++;
    } else {
        w->px += s->body.vel.x; w->py += s->body.vel.y; w->pz += s->body.vel.z;
        s->body.vel.y -= 0.6f;
    }
}
static void land_end(RECVX_ENEMY_WORK* w)
{
    set_modes(w, 1, 1, 1, 0);
    w->flg &= ~0x4u;
    w->ar = 14.0f;
    timer_back(w);
}
static void dg06(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2][2] = { { 40, 51 }, { 45, 52 } };
    switch (w->mode3) {
    case 0:
        if (!fall_head(w, mtn[w->type][0], 1, 1, 1)) return;
        w->ar = 1.0f;
        /* fall through */
    case 1: fall_step(w, 1, 0, 1); break;
    case 2:
        if (w->ct0-- == 0) {
            w->mtn_no = mtn[w->type][1]; w->frm_no = 0;
            if (!frames_ct(w, 2, &w->ct0)) return;
            w->mode3++;
            if (w->type & 0x1) {
                int32_t a[3];
                njRotateY(bm(w), 32768);
                if (!root_ang(w, w->mtn_no, 0, a)) return;
                recvx_en03_bind_root_angles(w, a);
            }
        }
        break;
    case 3:
        recvx_en03_add_null_trans(w, bd(w), w->type ? spl_052 : spl_051, w->type ? 21u : 80u);
        if (w->ct0-- == 0) land_end(w);
        break;
    }
    st(w)->wall_ofs = 0;
}
static void dg07(RECVX_ENEMY_WORK* w)
{
    static const uint32_t mtn[2] = { 54, 55 };
    switch (w->mode3) {
    case 0:
        if (!fall_head(w, mtn[w->type], 0, 3, 0)) return;
        w->ar = 14.0f;
        /* fall through */
    case 1: fall_step(w, 0, 0, 1); break;
    case 2:
        if (w->ct0-- == 0) land_end(w);
        break;
    }
    st(w)->wall_ofs = 0;
}
static void damage(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    if (w->flg & 0x4u) {
        w->flg &= ~0x4u;
        w->comb_flg &= ~0xCu;
        w->comb_flg |= recvx_en03_dg_dir_check(w, bd(w)) ? 0x8u : 0x4u;
        recvx_t3_calc_damage(w, CombWepTbl, CombJointTbl);
        if (w->wpnr_no != 16 || (w->flg2 & 0x4u) || w->comb_pnt == 1) {
            if (w->total_dam != 0) {
                if (w->type == 0 && (w->comb_flg & 0x8u)) { s->hp2 -= w->total_dam; w->hp -= w->total_dam; }
                else w->hp -= w->total_dam;
            }
            if (w->wpnr_no != 17 || (w->flg2 & 0x4u)) hit_mark(w);
        }
    }
    switch (w->mode2) {
    case 0: case 2: case 5: break;              /* DG00/02/05 original leer */
    case 1: dg01_03(w, 0); break;
    case 3: dg01_03(w, 1); break;
    case 4: dg04(w); break;
    case 6: dg06(w); break;
    case 7: dg07(w); break;
    default: gap(w, "bhEne23_DamageMode2[mode2]"); break;
    }
}

/* --- Tod (en23.c:2521-2956) ------------------------------------------------ */
static void blood_pool(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
    w->ay = heading(w);
    if (!w->sv || !w->sv->blood_pool) { gap(w, "bhEne_BloodPool (en23)"); return; }
    w->sv->blood_pool(w, &at, w->ay, &BloodParam);
}
static void corpse(RECVX_ENEMY_WORK* w)
{
    uint32_t n = frames(w, w->mtn_no);
    if (!n) return;
    w->frm_no = (int32_t)(65536.0f * (float)(n - 1));
    w->hokan_count = 0;
    w->mtn_add = 0;
    w->flg &= ~0x40u;
    w->mode3++;
    w->flg &= ~0x8u; w->flg2 |= 0x1u;
    blood_pool(w);
}
static void die(RECVX_ENEMY_WORK* w)
{
    static const uint32_t dd00[2] = { 48, 50 };
    static const uint32_t dd01[2][2] = { { 40, 47 }, { 45, 49 } };
    static const uint32_t dd03[2][2] = { { 54, 48 }, { 55, 50 } };
    switch (w->mode2) {
    case 0:
        switch (w->mode3) {
        case 0:
            w->mtn_no = dd00[w->type]; w->frm_no = 0;
            w->hokan_count = 10; w->hokan_rate = 32768;
            w->mtn_add = 65536;
            w->flg |= 0x10000000u;
            if (!frames_ct(w, 1, &w->ct0)) return;
            w->mode3++;
            /* fall through */
        case 1:
            if (w->ct0-- == 0) {
                w->mtn_add = 0;
                w->mode3++;
                w->flg &= ~0x8u; w->flg2 |= 0x1u;
                blood_pool(w);
            }
            break;
        }
        break;
    case 1: case 3: {
        const int dd03_ = w->mode2 == 3;
        switch (w->mode3) {
        case 0:
            if (!fall_head(w, dd03_ ? dd03[w->type][0] : dd01[w->type][0], dd03_ ? 0 : 1, dd03_ ? 2 : 1, dd03_ ? 0 : 1)) return;
            w->ar = 14.0f;
            /* fall through */
        case 1: fall_step(w, 0, 65536, 0); break;
        case 2:
            if (w->ct0-- == 0) {
                w->mtn_no = dd03_ ? dd03[w->type][1] : dd01[w->type][1];
                w->frm_no = 0;
                if (dd03_) { w->hokan_count = 0; w->mtn_add = 65536; }
                if (!frames_ct(w, dd03_ ? 1 : 2, &w->ct0)) return;
                w->mode3++;
            }
            break;
        case 3:
            if (w->ct0-- == 0) corpse(w);
            break;
        }
        break;
    }
    case 2: break;                               /* DD02 original leer */
    default: gap(w, "bhEne23_DeadMode2[mode2]"); return;
    }
    st(w)->wall_ofs = 0;
}

/* --- Kollision (en23.c:2961-3263) ----------------------------------------- */
static void collision_walls(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    NJS_MATRIX m;
    NJS_VECTOR body = { 0, w->ar, 0 }, trans = { 0, 0, 0 };
    const float py = w->py;
    memcpy(m, s->body.m, sizeof m);
    if (w->flg & 0x100000u) {
        int32_t a[3];
        if (!root_ang(w, w->mtn_no, w->frm_no, a)) return;
        njRotateXYZ(&m, (Angle)a[0], (Angle)a[1], (Angle)a[2]);
    }
    njCalcVector(&m, &body, &body);
    if (w->flg & 0x10000000u) {
        RECVX_ENEMY_VEC r;
        if (!w->sv || !w->sv->model_root_position || !w->sv->model_root_position(w, &r)) { gap(w, "bhEne23_CollisionWalls: objP->pos"); return; }
        trans.x = r.x; trans.y = r.y; trans.z = r.z;
        if (!(w->flg & 0x80000u)) trans.y = 0;
        njCalcVector(bm(w), &trans, &trans);
        w->px += trans.x; w->py += trans.y; w->pz += trans.z;
    }
    if ((w->flg & 0x4000000u) && s->body.surface == 0) {
        const float dx = s->body.m[8] * -s->wall_ofs, dz = s->body.m[10] * -s->wall_ofs;
        const float ar = w->ar, ah = w->ah;
        w->px += dx; w->py += 5.0f; w->pz += dz;
        w->ah = 20.0f; w->ar = 12.0f;
        if (w->sv && w->sv->check_wall) w->sv->check_wall(w);
        else gap(w, "bhCheckWall (en23)");
        w->px -= dx; w->py -= 5.0f; w->pz -= dz;
        w->ar = ar; w->ah = ah;
        s->wall_ofs += (10.4f - s->wall_ofs) / 16.0f;
    }
    if (!(w->flg & 0x4000000u)) {
        w->px += body.x; w->py += body.y; w->pz += body.z;
        recvx_en03_collision2(w, s->body.wall, NULL);
        w->px -= body.x; w->py -= body.y; w->pz -= body.z;
    } else if (s->body.surface == 0 || (w->flg & 0x8000000u)) {
        w->px += body.x; w->py += body.y; w->pz += body.z;
        recvx_en03_collision(w, NULL);
        w->px -= body.x; w->py -= body.y; w->pz -= body.z;
    } else recvx_en03_collision2(w, s->body.wall, NULL);
    w->px -= trans.x; w->py -= trans.y; w->pz -= trans.z;
    if (w->flg & 0x4000000u) w->py = py;
    if (!(w->flg & 0x8000000u)) w->ar += (s->body.ar - w->ar) / 8.0f;
}
static void collision_line(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC from = { w->pxb, w->pyb, w->pzb }, to = { w->px, w->py, w->pz }, n;
    RECVX_ENEMY_ATR rec;
    if (!w->sv || !w->sv->line) { gap(w, "bhCollisionCheckLine2 (en23)"); return; }
    if (!w->sv->line(2, &from, &to, &n, 0x4400u, -1, &rec, NULL)) return;
    w->px = to.x; w->py = to.y; w->pz = to.z;          /* Line2 kuerzt &epw->px */
    if (rec.type == 7 && !(w->flg & 0x4000000u) && n.y > 0) {
        w->flg |= 0x4000000u;
        get_wall(w);
    }
}
static int check_climb_wall(RECVX_ENEMY_WORK* w, int flg)
{
    static const uint32_t mtn[2] = { 3, 24 };
    RECVX_EN23_STATE* s = st(w);
    NJS_POINT3 pos;
    RECVX_ENEMY_ATR hp;
    int ang, ang2, i, root, n, found = 0;
    float r;
    if (s->body.surface != 0) return 0;
    w->ay = heading(w);
    if (w->way > 910) ang = (int16_t)((w->ay + 16384) & ~0x3FFF);
    else if (w->way < -910) ang = (int16_t)(w->ay & ~0x3FFF);
    else ang = (int16_t)((w->ay + 8192) & ~0x3FFF);
    ang2 = (int16_t)(ang - w->ay);
    if (ang2 < -5461 || ang2 > 5461) return 0;
    pos.x = w->px - (27.0f * njSin((Angle)ang));
    pos.z = w->pz - (27.0f * njCos((Angle)ang));
    /* hitchk.c:5474 bhCheckFloorEnemy(flr_no, x, z): erste type-2-Flaeche. */
    n = floor_count(w);
    for (i = 0; i < n; i++) {
        if (!floor_at(w, (uint32_t)i, &hp)) continue;
        if ((hp.flg & 0x1u) && hp.type == 2 && hp.px <= pos.x && hp.px + hp.w >= pos.x &&
            hp.pz <= pos.z && hp.pz + hp.d >= pos.z && hp.flr_no == w->flr_no) { found = 1; break; }
    }
    if (!(found && hp.prm0 == 23 && hp.prm1 < 4 && ang == (int16_t)((-(int)(hp.prm1 & 0x3)) * 16384))) return 0;
    s->floor_valid = 1; s->floor = hp;
    w->xn = w->px; w->yn = w->py; w->zn = w->pz;
    switch (hp.prm1) {
    case 0: w->zn = (hp.pz + hp.d) + 22.4f; break;
    case 1: w->xn = hp.px - 22.4f; break;
    case 2: w->zn = hp.pz - 22.4f; break;
    case 3: w->xn = (hp.px + hp.w) + 22.4f; break;
    }
    if (!rnd(w, &r)) return 0;
    root = (int)(2.0f * r);
    for (i = 0; i < 2; i++) {
        NJS_MATRIX m;
        NJS_VECTOR k;
        uint32_t nf;
        int hit;
        if (!(nf = frames(w, (uint32_t)root + mtn[w->type]))) return 0;
        if (!root_pos(w, (uint32_t)root + mtn[w->type], nf - 1, &k)) return 0;
        njUnitMatrix(&m);
        njRotateY(&m, (Angle)ang);
        k.x *= (float)flg;
        njCalcVector(&m, &k, &k);
        pos.x = k.x + w->xn; pos.y = k.y + w->yn; pos.z = k.z + w->zn;
        pos.y -= 23.0f;
        if (!wall_type(w, &pos, 18.199999f, 20.0f, &hit)) return 0;
        if (!hit) {
            const NJS_POINT3 p2 = { pos.x, pos.y + 999.0f, pos.z };
            s->body.wall2 = line1(w, &pos, &p2);
            return root + 1;
        }
        if (++root > 1) root = 0;
    }
    return 0;
}
static int check_diving(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    NJS_VECTOR k;
    NJS_POINT3 pos;
    uint32_t n;
    int hit;
    float dist;
    if (!player(w, &p)) return 0;
    if (p.flr_no != 0) return 0;
    if (w->flg & 0x8000000u) return 0;
    if (!(n = frames(w, 37))) return 0;
    if (!root_pos(w, 37, n - 1, &k)) return 0;
    njCalcVector(bm(w), &k, &k);
    pos.x = k.x + w->px; pos.z = k.z + w->pz;
    pos.y = p.pos.y;
    if (!wall_type(w, &pos, 14.0f, 20.0f, &hit)) return 0;
    if (hit) return 0;
    dist = njSqrt(((pos.x - p.pos.x) * (pos.x - p.pos.x)) + ((pos.z - p.pos.z) * (pos.z - p.pos.z)));
    if (dist < 15.0f) return 0;
    return 1;
}

/* --- Treffer (en23.c:3266-3515) ------------------------------------------- */
static void leg_break(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    float r;
    int i;
    if (!rnd(w, &r)) return;
    i = (int)(2.0f * r);
    if (i > 1) { gap(w, "bhEne23_LegBreak: exp0[258+2] liest hinter die Beinflags"); return; }
    if (s->leg_broken[i] == 0) {
        const uint32_t node = s->broken_node[i];
        s->leg_broken[i] = 1;
        if (!recvx_en03_owp_flag_or(w, node, 0x3u) || !recvx_en03_owp_flag_or(w, node + 1, 0x2u) ||
            !recvx_en03_owp_flag_or(w, node + 2, 0x2u)) return;
        set_modes(s->legs[i], 1, s->legs[i]->mode1, 0, 0);
        w->mdflg |= 0x20u;
        recvx_t3_set_bloodstain(w, 0, (int)node, NULL);
        recvx_t3_set_mince_effect(w, 2, 3);
        recvx_t3_set_mince_effect(w, 3, 2);
    }
}
static void damage_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    RECVX_ENEMY_PLAYER p;
    float r;
    int flg;
    w->flg &= ~0x4u;
    recvx_t3_calc_damage(w, CombWepTbl, CombJointTbl);
    w->comb_flg &= ~0xCu;
    w->comb_flg |= recvx_en03_dg_dir_check(w, bd(w)) ? 0x8u : 0x4u;
    if (!(w->wpnr_no != 16 || (w->flg2 & 0x4u) || w->comb_pnt == 1)) return;
    if (w->total_dam != 0) {
        if (w->type == 0) {
            if (w->comb_flg & 0x8u) s->hp2 -= w->total_dam;
            else {
                if (!player(w, &p)) return;
                if (s->body.surface == 0 && !(p.at_flg & 0x8u)) s->hp2 -= w->total_dam;
                w->hp -= w->total_dam;
            }
        } else w->hp -= w->total_dam;
    }
    if (w->wpnr_no == 17 && !(w->flg2 & 0x4u)) return;
    hit_mark(w);
    if (s->body.surface == 0 && !(w->flg & 0x400000u) && w->total_dam > 50) {
        if (!rnd(w, &r)) return;
        if (r < 0.3f) leg_break(w);
    }
    if (w->hp < 0) {
        NJS_MATRIX o;
        w->mode0 = 4; w->mode1 = 0; w->mode3 = 0;
        if (s->body.surface == 0) w->mode2 = 0;
        else if (s->body.surface == 1) w->mode2 = 1;
        if (w->flg & 0x400000u) {
            if (!joint(w, 0, &o)) return;
            w->mode2 = o[5] < 0 ? 1 : 3;
        }
        w->flg |= 0x2u; w->flg &= ~0x20u;
        return;
    }
    flg = 0;
    if (w->flg & 0x400000u) {
        if (w->total_dam < 40) return;
        if (!rnd(w, &r)) return;
        if (r > 0.5f) return;
    }
    if (w->type == 0 && s->hp2 < 0) {
        switch (w->wpnr_no) {
        case 8: case 9: case 12:
            if (w->comb_pnt > 5) { flg = 1; w->mode2 = 4; }
            break;
        case 5: case 6: case 10: case 11: case 13: case 14: case 15: case 16: case 20:
            flg = 1; w->mode2 = 4;
            break;
        }
    }
    if (flg == 0) {
        if (w->total_dam > 40) w->mode2 = 3;
        else {
            if (w->total_dam <= 20) return;
            w->mode2 = 1;
        }
    }
    if (s->body.surface == 1) {
        if (!rnd(w, &r)) return;
        if (r > 0.5f) w->mode2 = 6;
    }
    if (w->flg & 0x400000u) {
        NJS_MATRIX o;
        if (!joint(w, 0, &o)) return;
        w->mode2 = o[5] < 0 ? 6 : 7;
    }
    w->mode0 = 3; w->mode1 = 0; w->mode3 = 0;
}
static void init_child(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    int i;
    for (i = 0; i < s->babies_n; i++) {
        RECVX_ENEMY_WORK* e = s->babies[i];
        float r, spd, px, py, pz;
        int ang;
        NJS_VECTOR v;
        if (!((e->flg & 0x1u) && e->id == 24)) continue;
        set_modes(e, 1, 1, 5, 0);
        e->mdflg &= ~0x1u;
        if (!rnd(w, &r)) return; ang = (int)(65536.0f * r);
        if (!rnd(w, &r)) return; spd = 0.5f + (1.2f * r);
        e->ay = ang;
        e->xn = spd * -njSin((Angle)ang);
        if (!rnd(w, &r)) return; e->yn = 0.5f + (1.5f * r);
        e->zn = spd * -njCos((Angle)ang);
        if (!rnd(w, &r)) return;
        v.x = 0; v.y = 5.0f + (10.0f * r); v.z = 10.0f;
        njCalcVector(bm(w), &v, &v);
        px = w->px + v.x; py = w->py + v.y; pz = w->pz + v.z;
        if (!rnd(w, &r)) return; spd = 3.0f + (3.0f * r);
        e->px = e->pxb = px - (spd * njSin((Angle)ang));
        e->py = e->pyb = py;
        e->pz = e->pzb = pz - (spd * njCos((Angle)ang));
    }
}

/* --- Spieler (en23.c:3518-3729) ------------------------------------------- */
static const int32_t pc_mtn[3][8] = {
    { 60, 61, 62, 63, 65, 64, 0, 0 }, { 66, 67, 68, 69, 71, 70, 0, 0 }, { 66, 67, 68, 69, 71, 70, 0, 0 }
};
static const RECVX_ENEMY_VEC* pc_trans(uint32_t id, int k)
{
    static const RECVX_ENEMY_VEC* const t[3][3] = {
        { recvx_cler_042, recvx_cler_043, recvx_cler_045 },
        { recvx_cher_060, recvx_cher_061, recvx_cher_063 },
        { recvx_cher_060, recvx_cher_061, recvx_cher_063 }
    };
    return t[id][k];
}
static void library(RECVX_ENEMY_WORK* w, uint32_t source)
{
    if (!en23_sv || !en23_sv->player_library) { gap(w, "bhEne23_PlayerControl: plp->mnwP"); return; }
    en23_sv->player_library(w, source);
}
static void null_trans(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, int k)
{
    const RECVX_T3_LINK lk = { NULL, 1 };
    put_player(w, p);
    recvx_t3_add_null_trans(lk, pc_trans(p->ply_id, k));
    player(w, p);
}
static void player_control(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p;
    uint32_t n;
    if (!player(w, &p)) return;
    if (p.mode0 != 4 && p.mode0 != 6) return;
    if (p.ply_id > 2) { gap(w, "bhEne23_PlayerControl: mtn[sys->ply_id] ausserhalb"); return; }
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
            put_player(w, &p); library(w, 1);
            recvx_t3_call_player_voice(death ? 1 : 2);
            BASE(w, vibration, "StartVibrationEx (en23)");
            w->sv->vibration(1, 11);
            if (!player(w, &p)) return;
            break;
        case 1:
            null_trans(w, &p, (!death && p.mode2 == 0) ? 0 : 1);
            if (p.frm_no == 0) {
                if (!death) {
                    if (p.mode2 == 0) { p.mtn_no = (uint32_t)pc_mtn[p.ply_id][2]; p.flg |= 0xC0000u; }
                    else p.mtn_no = (uint32_t)pc_mtn[p.ply_id][3];
                } else {
                    p.mtn_no = (uint32_t)pc_mtn[p.ply_id][5];
                    BASE(w, player_motion_frames, "bhEne23_PlayerControl: plp->mnwP[mtn_no].frm_num");
                    if (!(n = w->sv->player_motion_frames(p.mtn_no, 1))) { gap(w, "bhEne23_PlayerControl: mnwP[mtn_no] fehlt"); return; }
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
                } else if (p.mtn_no == (uint32_t)pc_mtn[p.ply_id][3]) null_trans(w, &p, 2);
                if (p.frm_no == 0) {
                    p.mtn_enemy = 0; p.mtn_enemy_work = NULL;
                    p.flg &= ~0x10004u; p.flg2 &= ~0x1u; p.flg |= 0x8u;
                    p.at_flg = 0;
                    p.stflg &= ~0x10000u;
                    p.mode0 = 1; p.mode1 = 0; p.mode2 = 0; p.mode3 = 0;
                    put_player(w, &p); library(w, 0);
                    if (!player(w, &p)) return;
                }
            } else if (p.ct0-- == 0) { p.mtn_add = 0; p.ct0 = 45; p.mode3++; }
            break;
        case 3:
            if (death && p.ct0-- == 0) p.flg |= 0x2u;
            break;
        }
    }
    p.flg |= 0x200000u;
    put_player(w, &p);
}

/* --- Saeure, Form, Klang, Trefferbild (en23.c:3732-4028) ----------------- */
static void acid(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    NJS_MATRIX m7, m22;
    RECVX_ENEMY_VEC pos1, pos2;
    int i;
    if (!joint(w, 7, &m7) || !joint(w, 22, &m22)) return;
    pos1.x = (m7[12] + m22[12]) / 2.0f;
    pos1.y = (m7[13] + m22[13]) / 2.0f;
    pos1.z = (m7[14] + m22[14]) / 2.0f;
    if (s->body.surface == 0) { pos2.x = -3.5f * s->body.m[8]; pos2.y = 0; pos2.z = -3.5f * s->body.m[10]; }
    else { pos2.x = -4.5f * s->body.m[8]; pos2.y = 1.0f; pos2.z = -4.5f * s->body.m[10]; }
    SV(w, effect_tb, "bhSetEffectTb(256) (en23)");
    SV(w, model_texture, "epw->mdl[6].texP (en23)");
    SV(w, effect_position, "eff[eno].px (en23)");
    if (!x || !x->effect_patch) { gap(w, "eff[eno]-Nachfelder (en23)"); return; }
    for (i = 0; i < 2; i++) {
        float dt, a, b;
        int eno;
        if (!rnd(w, &dt)) return;
        dt = 1.5f + dt;
        {
            const RECVX_ENEMY_VEC sc = { dt, dt, dt };
            eno = en23_sv->effect_tb(256, 2, 1, &pos1, &sc);
        }
        if (eno != -1) {
            RECVX_T3_EFFPATCH patch;
            NJS_MATRIX m;
            NJS_VECTOR v = { pos2.x, pos2.y, pos2.z };
            RECVX_ENEMY_VEC at;
            memset(&patch, 0, sizeof patch);
            patch.fields = RECVX_T3_EFF_STFLG_OR | RECVX_T3_EFF_TEXP | RECVX_T3_EFF_TEX_ID;
            patch.stflg_or = 0x20u;
            patch.texp = en23_sv->model_texture(w, 6);
            patch.tex_id = 1;
            if (!rnd(w, &a) || !rnd(w, &b)) return;
            njUnitMatrix(&m);
            njRotateY(&m, (Angle)((1820.0f * a) - 910.0f));
            njRotateX(&m, (Angle)(1820.0f * b));
            njCalcVector(&m, &v, &v);
            patch.fields |= RECVX_T3_EFF_VECTOR;
            patch.xn = v.x; patch.yn = v.y; patch.zn = v.z;
            x->effect_patch(eno, &patch);
            if (!rnd(w, &dt)) return;
            if (!en23_sv->effect_position(eno, &at, 0)) { gap(w, "eff[eno].px (en23)"); return; }
            at.x += dt * v.x; at.y += dt * v.y; at.z += dt * v.z;
            en23_sv->effect_position(eno, &at, 1);
        }
    }
}
static void shape(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s = st(w);
    if (w->mdflg & 0x2u) {
        if (w->type != 0) w->mdflg &= ~0x2u;
        s->shape += (w->flg & 0x2u) ? 91 : 1456;
        w->shp_ct = 500.0f + (500.0f * njSin((Angle)s->shape));
    }
}
static void call_se_frames(RECVX_ENEMY_WORK* w)
{
    int fno;
    if (w->mtn_room) return;                       /* mnwP != mnwPb */
    fno = w->frm_no / 65536;
    switch (w->mtn_no) {
    case 2:
        if (fno == 0 || fno == 9 || fno == 14 || fno == 23) call_se(w, 74499);
        break;
    case 22:
        if (fno == 0 || fno == 6) call_se(w, 74499);
        break;
    case 3: case 4: case 24: case 25:
        fno %= 34;
        if (fno == 0 || fno == 9 || fno == 14 || fno == 23) call_se(w, 74499);
        break;
    case 11:
        if (fno == 29 || fno == 44) call_se(w, 74503);
        break;
    case 14: case 15: case 16: case 17: case 18: case 19:
    case 29: case 30: case 31: case 32: case 33: case 34:
        if (fno == 0) call_se(w, 8976);
        break;
    case 37:
        if (fno == 13) call_se(w, 74510);
        break;
    case 42:
        if (fno == 20) call_se(w, 74510);
        break;
    case 40: case 45: case 54: case 55:
        if (fno == 1) call_se(w, 74510);
        break;
    case 48: case 50:
        if (fno == 26) call_se(w, 8978);
        break;
    }
}
static int blood_ofs(RECVX_ENEMY_WORK* w, const EN23_BLOOD* b, RECVX_ENEMY_VEC* o)
{
    float a, c, d;
    o->x = b->ofp.x; o->y = b->ofp.y; o->z = b->ofp.z;
    if (!rnd(w, &a)) return 0; o->x += (b->rx * a) - (b->rx / 2.0f);
    if (!rnd(w, &c)) return 0; o->y += (b->ry * c) - (b->ry / 2.0f);
    if (!rnd(w, &d)) return 0; o->z += (b->rz * d) - (b->rz / 2.0f);
    return 1;
}
static void hit_mark(RECVX_ENEMY_WORK* w)
{
    const EN23_BLOOD* b;
    const EN23_REACT* r;
    RECVX_ENEMY_VEC ofp;
    int i, range = 0;
    float size, len;
    if (w->djnt_no < 0 || w->djnt_no >= 37) { gap(w, "bhEne23_HitMark: BloodTbl[djnt_no] ausserhalb"); return; }
    if (w->wpnr_no < 0 || w->wpnr_no >= 21) { gap(w, "bhEne23_HitMark: DmgReact[wpnr_no] ausserhalb"); return; }
    b = &BloodTbl[w->djnt_no];
    r = &DmgReact[w->wpnr_no];
    if (w->comb_flg & 0x10u) range = 0;
    if (w->comb_flg & 0x20u) range = 1;
    if (w->comb_flg & 0x40u) range = 2;
    if (r->type[range] >= 0) {
        const RECVX_T3_LINK lk = { w, 0 };
        if (!blood_ofs(w, b, &ofp)) return;
        recvx_t3_set_blood_effect_burst(lk, r->type[range], w->djnt_no, &ofp, 0);
        if (r->bloodstain[range] != 0) recvx_t3_set_bloodstain(w, 0, w->djnt_no, &ofp);
    }
    if ((r->exef & 0x1u) && b->flg == 0) {
        for (i = 0; i < 4; i++) {
            if (!blood_ofs(w, b, &ofp)) return;
            if (!rnd(w, &size) || !rnd(w, &len)) return;
            recvx_t3_set_fire_effect(w, w->djnt_no, &ofp, 0.5f + (0.5f * size), (int)(40.0f * len) + 20);
        }
    }
    if ((r->exef & 0x2u) && b->flg == 0) {
        if (!blood_ofs(w, b, &ofp)) return;
        recvx_t3_set_acid_effect(w, w->djnt_no, &ofp, 2.0f);
    }
    if (r->exef & 0x4u) {
        SV(w, material_color, "npSetAllMatColor (en23)");
        en23_sv->material_color(w, 0xFF201010u);
    }
}

/* --- Rahmen (en23.c:361-444) ---------------------------------------------- */
void recvx_en23(RECVX_ENEMY_WORK* w)
{
    RECVX_EN23_STATE* s;
    if (!w) return;
    if (w->mode0 != 0 && !w->exp0) { gap(w, "bhEne23: exp0 fehlt"); return; }
    w->flg &= ~0x100u;
    switch (w->mode0) {
    case 0: init(w); break;
    case 1: move(w); break;
    case 2: break;                                 /* NG00 original leer */
    case 3: damage(w); break;
    case 4: die(w); break;
    case 5:
        if (w->sv && w->sv->event) w->sv->event(w);
        else gap(w, "bhEne_Event (en23)");
        break;
    default: gap(w, "bhEne23_Mode0[mode0]"); return;
    }
    if (!w->exp0) return;
    s = st(w);
    BASE(w, set_motion, "bhSetMotion (en23)");
    w->sv->set_motion(w, w->mtn_add, w->mtn_md);
    call_se_frames(w);
    if (w->flg & 0x800000u) {
        RECVX_ENEMY_VEC pos;
        if (!parts(w, 0, &pos)) return;
        w->aox = pos.x - w->px; w->aoy = pos.y - w->py; w->aoz = pos.z - w->pz;
        if (!(w->flg & 0x1000000u)) w->aoy = 0;
    } else { w->aox = 0; w->aoy = 0; w->aoz = 0; }
    if (!(w->flg & 0x400000u)) {
        if (w->sv->check_player) w->sv->check_player(w);
        else gap(w, "bhCheckPlayer (en23)");
    }
    if (!(w->flg & 0x4000000u)) collision_line(w);
    if (w->flg & 0x10u) collision_walls(w);
    {
        NJS_MATRIX root, o;
        njUnitMatrix(&root);
        njTranslate(&root, w->px, w->py, w->pz);
        memcpy(o, s->body.m, sizeof o);
        njMultiMatrix(&root, &o);
        memcpy(w->root_mtx, root, sizeof root);   /* epw->mtx */
    }
    if (w->mtn_room) {                             /* mnwP != mnwPb */
        const uint32_t flg = w->flg;
        w->flg &= ~0x1000u;
        BASE(w, calc_model, "bhCalcModel (en23)");
        w->sv->calc_model(w);
        w->flg = flg;
        w->mdflg &= ~0x4u;
    } else w->mdflg |= 0x4u;
    BASE(w, weapon_atr, "bhEne_SetWeponAtr (en23)");
    if (w->type & 0x1) w->sv->weapon_atr(w, 22, 1, 8.0f);
    else w->sv->weapon_atr(w, 22, 12, 8.0f);
    shape(w);
    player_control(w);
}
