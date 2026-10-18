/* EN01-Sterbezustaende -> bhSetBloodPoolLnk (effsub3.c:145).
   ===========================================================================
   Erwartungen aus src/ps2/veronica/prog/en01.c (bhEne01_DD00 :9973-9995,
   bhEne01_DD01 :10120-10123, bhEne01_DD02 :10143-10150) und en01b.c
   (bhEne01_DD02B :749-763): auf trockenem Boden (stflg ohne 0x100000) ruft
   jeder Zustand bhSetBloodPoolLnk genau einmal mit en01_BldTbl bzw.
   en01_BldTbl2 und pal_bnk 0; im Wasser nie. lkwkp ist epw, in DD02B NULL.
   Die Dienste sind TESTAUFSTELLUNGEN (Protokoll, feste Gelenkmatrix); die
   Host-Bindung prueft tests/native/zombie_blood_pool_check.py. */
#include "en01_core.h"
#include "ninja_math.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failed, g_checks;

static void check(int ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failed; printf("FEHLER: %s\n", what); }
}

static void check_f(float got, float want, const char* what)
{
    ++g_checks;
    if (fabsf(got - want) > 1e-5f) {
        ++g_failed;
        printf("FEHLER: %s -- erwartet %.6f, erhalten %.6f\n", what, want, got);
    }
}

typedef struct PoolCall {
    RECVX_ENEMY_WORK* ew;
    RECVX_ENEMY_VEC pos;
    int32_t ay;
    const RECVX_ENEMY_BP_WORK* tab;
    int pal_bnk;
} PoolCall;

static PoolCall g_calls[8];
static int g_call_n;
static int g_pool_gap;

static void t_gap(const char* name)
{
    if (strstr(name, "bhSetBloodPoolLnk")) ++g_pool_gap;
}
static int32_t t_rand(void) { return 0; }

static int t_pool(RECVX_ENEMY_WORK* ew, const RECVX_ENEMY_VEC* pos, int32_t ay,
                  const RECVX_ENEMY_BP_WORK* tab, int pal_bnk)
{
    if (g_call_n < 8) {
        PoolCall* c = &g_calls[g_call_n];
        c->ew = ew; c->pos = *pos; c->ay = ay; c->tab = tab; c->pal_bnk = pal_bnk;
    }
    ++g_call_n;
    return 1;
}

/* owP[joint].mtx: Einheitsmatrix mit Translation (10+joint, 20+joint, 30+joint). */
static void joint_mtx(uint32_t joint, float* m)
{
    int i;
    for (i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    m[12] = 10.0f + (float)joint; m[13] = 20.0f + (float)joint; m[14] = 30.0f + (float)joint;
}
static int t_joint_matrix(const RECVX_ENEMY_WORK* w, uint32_t joint, float* out16)
{
    (void)w; joint_mtx(joint, out16); return 1;
}
static int t_joint_point(const RECVX_ENEMY_WORK* w, uint32_t joint,
                         const RECVX_ENEMY_VEC* local, RECVX_ENEMY_VEC* out)
{
    float m[16];
    (void)w; joint_mtx(joint, m);
    out->x = local->x + m[12]; out->y = local->y + m[13]; out->z = local->z + m[14];
    return 1;
}

static RECVX_ENEMY_SERVICES g_sv;
static RECVX_EN01_SERVICES  g_en;
static RECVX_EN01_CONTEXT   g_ctx;

static void bind(int with_pool)
{
    memset(&g_sv, 0, sizeof(g_sv));
    memset(&g_en, 0, sizeof(g_en));
    g_sv.gap = t_gap;
    g_sv.random_next = t_rand;
    g_sv.joint_matrix = t_joint_matrix;
    g_sv.joint_point = t_joint_point;
    if (with_pool) g_en.blood_pool_lnk = t_pool;
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.services = &g_sv;
    g_ctx.en01 = &g_en;
    recvx_en01_set_context(&g_ctx);
    g_call_n = 0; g_pool_gap = 0;
    memset(g_calls, 0, sizeof(g_calls));
}

static void work(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p, uint32_t stflg)
{
    memset(w, 0, sizeof(*w));
    memset(p, 0, sizeof(*p));
    w->sv = &g_sv;
    w->exp0 = p;
    w->px = 1.5f; w->py = -2.0f; w->pz = 7.25f;
    w->ay = 12288;
    w->stflg = stflg;
}

static void same_pos(const RECVX_ENEMY_VEC* got, float x, float y, float z, const char* what)
{
    check_f(got->x, x, what); check_f(got->y, y, what); check_f(got->z, z, what);
}

/* en01.c:10120-10122 */
static void test_dd01(void)
{
    RECVX_ENEMY_WORK w; RECVX_EN01_EXP0 p;
    bind(1); work(&w, &p, 0u);
    recvx_en01_dd01(&w);
    check(g_call_n == 1, "DD01 trocken: genau ein bhSetBloodPoolLnk");
    check(g_calls[0].ew == &w, "DD01: lkwkp = epw");
    same_pos(&g_calls[0].pos, 1.5f, -2.0f, 7.25f, "DD01: posP = &epw->px");
    check(g_calls[0].ay == 12288, "DD01: ay = epw->ay");
    check(g_calls[0].tab == &recvx_en01_bld_tbl, "DD01: en01_BldTbl");
    check(g_calls[0].pal_bnk == 0, "DD01: pal_bnk 0");
    check(w.mode3 == 1, "DD01: mode3++ (en01.c:10124)");
    recvx_en01_dd01(&w);
    check(g_call_n == 1, "DD01 case 1 legt keine zweite Lache an");

    bind(1); work(&w, &p, 0x100000u);
    recvx_en01_dd01(&w);
    check(g_call_n == 0, "DD01 im Wasser (stflg 0x100000): keine Lache");

    bind(0); work(&w, &p, 0u);
    recvx_en01_dd01(&w);
    check(g_pool_gap == 1, "DD01 ohne Dienst meldet bhSetBloodPoolLnk als Luecke");
}

/* en01.c:9958/9973-9995: mode1 3 waehlt mtn 54 (mtn_tbl[3][0]). */
static void test_dd00(void)
{
    RECVX_ENEMY_WORK w; RECVX_EN01_EXP0 p;
    int32_t ang;

    bind(1); work(&w, &p, 0u);
    w.mode1 = 3;
    recvx_en01_dd00(&w);
    check(w.mtn_no == 54u, "DD00 mode1 3: mtn_tbl[3][0] = 54");
    check(g_call_n == 1, "DD00 mtn 54: genau ein bhSetBloodPoolLnk");
    ang = (int32_t)(uint16_t)(12288 + 32768);           /* EXP0 0x8000 nicht gesetzt (:9983) */
    check(g_calls[0].ay == ang, "DD00 mtn 54: ang = (u16)(ay + 180 Grad)");
    same_pos(&g_calls[0].pos, 1.5f - njSin(ang), -2.0f, 7.25f - njCos(ang),
             "DD00 mtn 54: gpos = px - njSin(ang), py, pz - njCos(ang)");
    check(g_calls[0].tab == &recvx_en01_bld_tbl2, "DD00 mtn 54: en01_BldTbl2");
    check(g_calls[0].ew == &w && g_calls[0].pal_bnk == 0, "DD00: lkwkp epw, pal_bnk 0");

    bind(1); work(&w, &p, 0u);
    w.mode1 = 3; p.flags = 0x8000u;
    recvx_en01_dd00(&w);
    check(g_call_n == 1 && g_calls[0].ay == 12288, "DD00 EXP0 0x8000: ang = epw->ay (:9979)");

    bind(1); work(&w, &p, 0u);
    w.mode1 = 0;
    recvx_en01_dd00(&w);
    check(w.mtn_no == 37u, "DD00 mode1 0: mtn_tbl[0][0] = 37");
    check(g_call_n == 1, "DD00 sonst: genau ein bhSetBloodPoolLnk (:9994)");
    same_pos(&g_calls[0].pos, 1.5f, -2.0f, 7.25f, "DD00 sonst: posP = &epw->px");
    check(g_calls[0].ay == 12288 && g_calls[0].tab == &recvx_en01_bld_tbl,
          "DD00 sonst: epw->ay und en01_BldTbl");
}

/* en01.c:10143-10150: gpos.xz aus owP[1].mtx, gpos.y = epw->py. */
static void test_dd02(void)
{
    RECVX_ENEMY_WORK w; RECVX_EN01_EXP0 p;
    bind(1); work(&w, &p, 0u);
    recvx_en01_dd02(&w);
    check(g_call_n == 1, "DD02 trocken: genau ein bhSetBloodPoolLnk");
    same_pos(&g_calls[0].pos, 11.0f, -2.0f, 31.0f, "DD02: gpos = owP[1].mtx[12]/py/mtx[14]");
    check(g_calls[0].ew == &w && g_calls[0].ay == 12288 && g_calls[0].tab == &recvx_en01_bld_tbl
          && g_calls[0].pal_bnk == 0, "DD02: epw, epw->ay, en01_BldTbl, 0");
}

/* en01b.c:749-763: pd = njCalcPoint(owP[10].mtx, (0,0,3)), pd.y = py, lkwkp NULL. */
static void test_dd02_b(void)
{
    RECVX_ENEMY_WORK w; RECVX_EN01_EXP0 p;
    bind(1); work(&w, &p, 0u);
    recvx_en01_dd02_b(&w);
    check(g_call_n == 1, "DD02B trocken: genau ein bhSetBloodPoolLnk");
    check(g_calls[0].ew == NULL, "DD02B: lkwkp NULL (en01b.c:763)");
    same_pos(&g_calls[0].pos, 20.0f, -2.0f, 43.0f, "DD02B: pd aus owP[10] mit z 3.0, pd.y = py");
    check(g_calls[0].ay == 12288 && g_calls[0].tab == &recvx_en01_bld_tbl && g_calls[0].pal_bnk == 0,
          "DD02B: epw->ay, en01_BldTbl, 0");

    bind(1); work(&w, &p, 0x100000u);
    recvx_en01_dd02_b(&w);
    check(g_call_n == 0, "DD02B im Wasser: keine Lache");
}

int main(void)
{
    _Make_SinTable();
    test_dd01();
    test_dd00();
    test_dd02();
    test_dd02_b();
    printf("en01_blood_pool_test: %d Pruefungen, %d Fehler\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
