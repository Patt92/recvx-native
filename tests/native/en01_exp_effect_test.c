/* EN01 ExpLegEffect/ExpWormEffect -> njRandom() (macros.h:105).
   ===========================================================================
   njRandom() ist (Float)-rand() / (Float)(RAND_MAX + 1); RAND_MAX + 1 laeuft
   als int auf -2^31 ueber. Die Disassembly belegt das an allen vier
   EN01-Stellen: bhEne01_ExpLegEffect 0x1882C8 (:11055) und 0x1883D0
   (:11066), bhEne01_ExpWormEffect 0x1885E8 (:11114) und 0x1886CC (:11124)
   rechnen `jal rand` -> `negu` -> `cvt.s.w` -> `div.s` durch 0xCF000000
   (-2^31), danach `mul.s` mit 2.0 und `add.s` mit 1.0 bzw. 2.0. Der Wert ist
   also POSITIV in [0, 1], nicht negativ.
   Erwartungen: rand() = 2^30 -> njRandom 0.5; rand() = RAND_MAX -> genau
   1.0f (cvt.s.w rundet 0x7FFFFFFF auf 2^31); rand() = 0 -> 0.
   Die Dienste sind TESTAUFSTELLUNGEN (feste rand()-Werte, Einheitsmatrix,
   Protokoll von bhSetEffectTb und eff[eno].spd). */
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
    if (got != want) {
        ++g_failed;
        printf("FEHLER: %s -- erwartet %.9g, erhalten %.9g\n", what, want, got);
    }
}

#define LOG_MAX 64

typedef struct TbCall { int32_t id, type; float sx, sy, sz; } TbCall;
typedef struct SlotCall { int slot; uint32_t mask; float spd; } SlotCall;

static TbCall   g_tb[LOG_MAX];
static int      g_tb_n;
static SlotCall g_slot[LOG_MAX];
static int      g_slot_n;
static int32_t  g_rand_value;
static int      g_rand_calls;

static void t_gap(const char* name) { (void)name; }
static int32_t t_rand(void) { ++g_rand_calls; return g_rand_value; }

static int t_joint_matrix(const RECVX_ENEMY_WORK* w, uint32_t joint, float* out16)
{
    int i;
    (void)w; (void)joint;
    for (i = 0; i < 16; ++i) out16[i] = 0.0f;
    out16[0] = out16[5] = out16[10] = out16[15] = 1.0f;
    return 1;
}

static int t_set_effect_tb(const RECVX_EN01_EF* tmpl, const RECVX_ENEMY_VEC* off,
                           struct RECVX_ENEMY_WORK* link, int lkono)
{
    (void)off; (void)link; (void)lkono;
    if (g_tb_n >= LOG_MAX) return -1;
    g_tb[g_tb_n].id = tmpl->id;
    g_tb[g_tb_n].type = tmpl->type;
    g_tb[g_tb_n].sx = tmpl->sx;
    g_tb[g_tb_n].sy = tmpl->sy;
    g_tb[g_tb_n].sz = tmpl->sz;
    return g_tb_n++;
}

static void t_effect_slot_set(int slot, uint32_t mask, const RECVX_EN01_EF_SLOT* in)
{
    if (g_slot_n >= LOG_MAX) return;
    g_slot[g_slot_n].slot = slot;
    g_slot[g_slot_n].mask = mask;
    g_slot[g_slot_n].spd = in->spd;
    ++g_slot_n;
}

static RECVX_ENEMY_SERVICES g_sv;
static RECVX_EN01_SERVICES  g_en;
static RECVX_EN01_CONTEXT   g_ctx;

static void bind(int32_t rand_value)
{
    memset(&g_sv, 0, sizeof(g_sv));
    memset(&g_en, 0, sizeof(g_en));
    g_sv.gap = t_gap;
    g_sv.random_next = t_rand;
    g_sv.joint_matrix = t_joint_matrix;
    g_en.set_effect_tb = t_set_effect_tb;
    g_en.effect_slot_set = t_effect_slot_set;
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.services = &g_sv;
    g_ctx.en01 = &g_en;
    recvx_en01_set_context(&g_ctx);
    g_tb_n = g_slot_n = 0;
    g_rand_value = rand_value;
    g_rand_calls = 0;
}

static void work(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p)
{
    memset(w, 0, sizeof(*w));
    memset(p, 0, sizeof(*p));
    w->sv = &g_sv;
    w->exp0 = p;
}

/* Alle Vorlagen mit id 363 tragen sx = sy = want_sx, sz = 1; jeder Slot mit
   RECVX_EN01_EFS_SPD traegt want_spd. */
static void check_scatter(int want_n, float want_sx, float want_spd, const char* what)
{
    char msg[160];
    int i, n_tb = 0, n_spd = 0;

    for (i = 0; i < g_tb_n; ++i) {
        if (g_tb[i].id != 363) continue;
        ++n_tb;
        snprintf(msg, sizeof(msg), "%s: Vorlage %d sx = 1/2 + k*njRandom()", what, i);
        check_f(g_tb[i].sx, want_sx, msg);
        check_f(g_tb[i].sy, want_sx, msg);
        check_f(g_tb[i].sz, 1.0f, msg);
    }
    for (i = 0; i < g_slot_n; ++i) {
        if (!(g_slot[i].mask & RECVX_EN01_EFS_SPD)) continue;
        ++n_spd;
        snprintf(msg, sizeof(msg), "%s: Slot %d spd = 2 + 2*njRandom()", what, g_slot[i].slot);
        check_f(g_slot[i].spd, want_spd, msg);
    }
    snprintf(msg, sizeof(msg), "%s: %d Splitter-Vorlagen", what, want_n);
    check(n_tb == want_n, msg);
    snprintf(msg, sizeof(msg), "%s: %d Splitter-Geschwindigkeiten", what, want_n);
    check(n_spd == want_n, msg);
}

/* en01.c:11052-11076: 3 x 8 Splitter, tmp = 1 + 2*njRandom(),
   spd = 2 + 2*njRandom(); davor 4 x rand() % 2 fuer die Nikuhen. */
static void test_exp_leg(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_EN01_EXP0 p;

    bind(0x40000000);
    work(&w, &p);
    recvx_en01_exp_leg_effect(&w, 3);
    check_scatter(24, 2.0f, 3.0f, "ExpLeg rand 2^30");
    check(g_rand_calls == 4 + 48, "ExpLeg: 4 + 24*2 rand()-Aufrufe");

    bind(0x7FFFFFFF);
    work(&w, &p);
    recvx_en01_exp_leg_effect(&w, 3);
    check_scatter(24, 3.0f, 4.0f, "ExpLeg rand RAND_MAX");

    bind(0);
    work(&w, &p);
    recvx_en01_exp_leg_effect(&w, 3);
    check_scatter(24, 1.0f, 2.0f, "ExpLeg rand 0");
}

/* en01.c:11112-11131: 2 x 8 Splitter, tmp = 2 + njRandom(),
   spd = 2 + 2*njRandom(). */
static void test_exp_worm(void)
{
    RECVX_ENEMY_WORK w, parent;
    RECVX_EN01_EXP0 p, pp;

    bind(0x40000000);
    work(&w, &p);
    work(&parent, &pp);
    w.lkwkp = &parent;
    recvx_en01_exp_worm_effect(&w);
    check_scatter(16, 2.5f, 3.0f, "ExpWorm rand 2^30");
    check(g_rand_calls == 32, "ExpWorm: 16*2 rand()-Aufrufe");

    bind(0x7FFFFFFF);
    work(&w, &p);
    work(&parent, &pp);
    w.lkwkp = &parent;
    recvx_en01_exp_worm_effect(&w);
    check_scatter(16, 3.0f, 4.0f, "ExpWorm rand RAND_MAX");
}

int main(void)
{
    _Make_SinTable();
    test_exp_leg();
    test_exp_worm();
    printf("en01_exp_effect_test: %d Pruefungen, %d Fehler\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
