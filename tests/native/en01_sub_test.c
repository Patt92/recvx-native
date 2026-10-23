/* Komponentenpruefung der EN01-Kindfamilie (native/enemies/en01_sub.c).
   ===========================================================================
   Jede Erwartung ist aus src/ps2/veronica/prog/en01sub.c abgeleitet (Fund-
   stelle an der Pruefung), nicht aus dem neuen Code. Alle Dienste sind
   ausdrueckliche TESTAUFSTELLUNGEN: sie protokollieren Anforderungen und
   liefern feste Eingaben (Modellbaum, Boden, Spieler). Das ist KEIN Runtime-
   oder Gameplaynachweis; die Host-Bindung (native/world_enemies.cpp) ist
   nicht Teil dieser Pruefung.
   =========================================================================== */
#include "en01_core.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failed, g_checks;

static void check(int ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failed; printf("FEHLER: %s\n", what); }
}

static void check_eq(long long got, long long want, const char* what)
{
    ++g_checks;
    if (got != want) {
        ++g_failed;
        printf("FEHLER: %s -- erwartet %lld, erhalten %lld\n", what, want, got);
    }
}

static void check_f(float got, float want, const char* what)
{
    ++g_checks;
    if (fabsf(got - want) > 1e-4f) {
        ++g_failed;
        printf("FEHLER: %s -- erwartet %.6f, erhalten %.6f\n", what, want, got);
    }
}

/* ---------------------------------------------------------------------------
   Testaufstellung
   --------------------------------------------------------------------------- */
#define LOG_MAX 64
static char g_gap[LOG_MAX][128];
static int  g_gap_n;

static void t_gap(const char* name)
{
    if (g_gap_n < LOG_MAX) {
        snprintf(g_gap[g_gap_n], sizeof(g_gap[0]), "%s", name ? name : "(null)");
        ++g_gap_n;
    }
}

static int gap_seen(const char* needle)
{
    int i;
    for (i = 0; i < g_gap_n; ++i) if (strstr(g_gap[i], needle)) return 1;
    return 0;
}

/* Allokationen (bhEne_CallocWork). */
static void*    g_alloc[LOG_MAX];
static uint32_t g_alloc_size[LOG_MAX];
static int      g_alloc_n;

static void* t_calloc(uint32_t size)
{
    void* p;
    if (g_alloc_n >= LOG_MAX) return NULL;
    p = calloc(1, size);
    g_alloc[g_alloc_n] = p;
    g_alloc_size[g_alloc_n] = size;
    ++g_alloc_n;
    return p;
}

/* Modellbaeume: je Work ein eigener objP/owP-Satz. */
#define NODES 16
typedef struct {
    RECVX_ENEMY_WORK* owner;
    float    owk[NODES][16];
    uint32_t owflg[NODES];
    RECVX_ENEMY_VEC pos[NODES];
    int32_t  ang[NODES][3];
} FAKE_MODEL;
static FAKE_MODEL g_model[8];
static int        g_model_n;

static FAKE_MODEL* model_of(const RECVX_ENEMY_WORK* w)
{
    int i;
    for (i = 0; i < g_model_n; ++i) if (g_model[i].owner == w) return &g_model[i];
    return NULL;
}

static FAKE_MODEL* model_add(RECVX_ENEMY_WORK* w)
{
    FAKE_MODEL* m = &g_model[g_model_n++];
    int i;
    memset(m, 0, sizeof(*m));
    m->owner = w;
    for (i = 0; i < NODES; ++i) {
        m->owk[i][0] = m->owk[i][5] = m->owk[i][10] = m->owk[i][15] = 1.0f;
    }
    return m;
}

static int t_joint_matrix(const RECVX_ENEMY_WORK* w, uint32_t j, float* out)
{
    FAKE_MODEL* m = model_of(w);
    if (!m || j >= NODES) return 0;
    memcpy(out, m->owk[j], sizeof(float) * 16);
    return 1;
}

static int t_joint_matrix_set(RECVX_ENEMY_WORK* w, uint32_t j, const float* in)
{
    FAKE_MODEL* m = model_of(w);
    if (!m || j >= NODES) return 0;
    memcpy(m->owk[j], in, sizeof(float) * 16);
    return 1;
}

static void t_joint_flags_or(RECVX_ENEMY_WORK* w, uint32_t j, uint32_t bits)
{
    FAKE_MODEL* m = model_of(w);
    if (m && j < NODES) m->owflg[j] |= bits;
}

static int t_node_local(const RECVX_ENEMY_WORK* w, uint32_t n, RECVX_ENEMY_VEC* pos,
                        int32_t* ang3)
{
    FAKE_MODEL* m = model_of(w);
    if (!m || n >= NODES) return 0;
    *pos = m->pos[n];
    ang3[0] = m->ang[n][0]; ang3[1] = m->ang[n][1]; ang3[2] = m->ang[n][2];
    return 1;
}

static void t_node_angle_set(RECVX_ENEMY_WORK* w, uint32_t n, uint32_t axis, int32_t a)
{
    FAKE_MODEL* m = model_of(w);
    if (m && n < NODES && axis < 3) m->ang[n][axis] = a;
}

static int g_clear_xz_node = -1;
static void t_node_local_clear_xz(RECVX_ENEMY_WORK* w, uint32_t n)
{
    (void)w;
    g_clear_xz_node = (int)n;
}

static int g_calc_model, g_set_motion;
static void t_calc_model(RECVX_ENEMY_WORK* w) { (void)w; ++g_calc_model; }
static int  t_set_motion(RECVX_ENEMY_WORK* w, int add, uint32_t md)
{
    (void)w; (void)add; (void)md; ++g_set_motion; return 0;
}

static uint32_t g_frames = 30u;
static uint32_t t_motion_frames(const RECVX_ENEMY_WORK* w, uint32_t mtn)
{
    (void)w; (void)mtn; return g_frames;
}

static float g_grand = 0.0f;
static float t_grand(uint32_t i) { (void)i; return g_grand; }
static uint32_t g_st_flg;
static uint32_t t_system_flags(uint32_t which) { return which == 2u ? g_st_flg : 0u; }

static int g_se[LOG_MAX], g_se_n;
static void t_enemy_se(uint32_t e, const RECVX_ENEMY_VEC* pos, int se)
{
    (void)e; (void)pos;
    if (g_se_n < LOG_MAX) g_se[g_se_n++] = se;
}
static uint32_t t_enemy_now(void) { return 0u; }

static RECVX_ENEMY_VEC g_explode;
static int g_explode_n;
static void t_explode(const RECVX_ENEMY_VEC* p) { g_explode = *p; ++g_explode_n; }

static int g_blood2_n, g_blood2_djnt[LOG_MAX];
static void t_blood2(RECVX_ENEMY_WORK* w, uint8_t type, const RECVX_ENEMY_VEC* ofp, int16_t ry)
{
    (void)type; (void)ofp; (void)ry;
    if (g_blood2_n < LOG_MAX) g_blood2_djnt[g_blood2_n] = w->djnt_no;
    ++g_blood2_n;
}

static int g_blood3_n, g_blood3_type[LOG_MAX];
static void t_blood3(RECVX_ENEMY_WORK* w, int lnk, const RECVX_ENEMY_VEC* ofs,
                     const RECVX_ENEMY_VEC* dv, int n, int tex, int type, int wcnt)
{
    (void)w; (void)lnk; (void)ofs; (void)dv; (void)n; (void)tex; (void)wcnt;
    if (g_blood3_n < LOG_MAX) g_blood3_type[g_blood3_n] = type;
    ++g_blood3_n;
}

static int g_wall_hit;
static uint32_t g_wall_type;
static int t_wall_ref_angle(RECVX_ENEMY_VEC* pos, uint32_t flg, float ar, float ah,
                            int32_t* ay, RECVX_ENEMY_ATR* rec)
{
    (void)pos; (void)flg; (void)ar; (void)ah; (void)ay;
    if (rec) rec->type = g_wall_type;
    return g_wall_hit;
}
static int t_wall_type_atr(const RECVX_ENEMY_VEC* pos, uint32_t flg, float ar, float ah,
                           RECVX_ENEMY_ATR* rec)
{
    (void)pos; (void)flg; (void)ar; (void)ah;
    if (rec) rec->type = g_wall_type;
    return g_wall_hit;
}

static int t_check_water(const RECVX_ENEMY_VEC* pos) { (void)pos; return 0; }

static float g_work_mtx[16];
static int t_work_matrix(const RECVX_ENEMY_WORK* w, float* out)
{
    (void)w; memcpy(out, g_work_mtx, sizeof(g_work_mtx)); return 1;
}

static RECVX_EN01_EF g_ef[4];
static int g_ef_n;
static int t_set_effect_tb(const RECVX_EN01_EF* t, const RECVX_ENEMY_VEC* off,
                           RECVX_ENEMY_WORK* link, int lkono)
{
    (void)off; (void)link; (void)lkono;
    if (g_ef_n < 4) g_ef[g_ef_n] = *t;
    return 100 + g_ef_n++;
}
static uint32_t g_slot_mask[4];
static RECVX_EN01_EF_SLOT g_slot[4];
static int g_slot_n;
static void t_effect_slot_set(int slot, uint32_t mask, const RECVX_EN01_EF_SLOT* in)
{
    (void)slot;
    if (g_slot_n < 4) { g_slot_mask[g_slot_n] = mask; g_slot[g_slot_n] = *in; }
    ++g_slot_n;
}

/* Spieler. */
static RECVX_ENEMY_PLAYER g_player;
static int t_player_get(RECVX_ENEMY_PLAYER* out) { *out = g_player; return 1; }
static void t_player_set(const RECVX_ENEMY_PLAYER* in) { g_player = *in; }
static int t_player_matrix(float* out)
{
    int i;
    for (i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = out[5] = out[10] = out[15] = 1.0f;
    out[12] = g_player.pos.x; out[13] = g_player.pos.y; out[14] = g_player.pos.z;
    return 1;
}
static int g_lever;
static int t_lever(void) { return g_lever; }
static int g_link_calls[2];
static int t_link_player(RECVX_ENEMY_WORK* w, int link)
{
    (void)w; ++g_link_calls[link ? 1 : 0]; return 1;
}
static uint32_t g_pad_clear;
static void t_pad_on_clear(uint32_t bits) { g_pad_clear |= bits; }

static RECVX_ENEMY_SERVICES g_sv;
static RECVX_EN01_SERVICES  g_en;
static RECVX_EN01_CONTEXT   g_ctx;
static RECVX_ENEMY_TABLE    g_table;

static void setup(void)
{
    memset(&g_sv, 0, sizeof(g_sv));
    memset(&g_en, 0, sizeof(g_en));
    memset(&g_ctx, 0, sizeof(g_ctx));
    memset(&g_table, 0, sizeof(g_table));
    memset(&g_player, 0, sizeof(g_player));
    g_gap_n = g_se_n = g_explode_n = g_blood2_n = g_blood3_n = 0;
    g_ef_n = g_slot_n = 0;
    g_calc_model = g_set_motion = 0;
    g_link_calls[0] = g_link_calls[1] = 0;
    g_clear_xz_node = -1;
    g_pad_clear = 0u;
    g_model_n = 0;
    g_wall_hit = 0; g_wall_type = 0u;
    g_grand = 0.0f; g_st_flg = 0u; g_frames = 30u; g_lever = 0;

    g_sv.gap = t_gap;
    g_sv.calloc_work = t_calloc;
    g_sv.joint_matrix = t_joint_matrix;
    g_sv.calc_model = t_calc_model;
    g_sv.set_motion = t_set_motion;
    g_sv.motion_frames = t_motion_frames;
    g_sv.grand = t_grand;
    g_sv.system_flags = t_system_flags;
    g_sv.enemy_se = t_enemy_se;
    g_sv.enemy_now = t_enemy_now;
    g_sv.explode = t_explode;
    g_sv.player_get = t_player_get;
    g_sv.player_set = t_player_set;
    g_sv.lever_check = t_lever;

    g_en.joint_matrix_set = t_joint_matrix_set;
    g_en.joint_flags_or = t_joint_flags_or;
    g_en.node_local = t_node_local;
    g_en.node_angle_set = t_node_angle_set;
    g_en.node_local_clear_xz = t_node_local_clear_xz;
    g_en.set_blood2 = t_blood2;
    g_en.set_blood3 = t_blood3;
    g_en.wall_ref_angle = t_wall_ref_angle;
    g_en.wall_type_atr = t_wall_type_atr;
    g_en.check_water = t_check_water;
    g_en.work_matrix = t_work_matrix;
    g_en.set_effect_tb = t_set_effect_tb;
    g_en.effect_slot_set = t_effect_slot_set;
    g_en.player_matrix = t_player_matrix;
    g_en.link_player = t_link_player;
    g_en.pad_on_clear = t_pad_on_clear;

    g_ctx.table = &g_table;
    g_ctx.services = &g_sv;
    g_ctx.en01 = &g_en;
    recvx_en01_set_context(&g_ctx);
}

static void teardown(void)
{
    int i;
    for (i = 0; i < g_alloc_n; ++i) free(g_alloc[i]);
    g_alloc_n = 0;
    recvx_en01_set_context(NULL);
}

static void work(RECVX_ENEMY_WORK* w, uint16_t id)
{
    memset(w, 0, sizeof(*w));
    w->sv = &g_sv;
    w->id = id;
    w->flg = 0x1u;
}

/* Ein aktiver Haupt-Zombie (Filter en01sub.c:131/:188). */
static void zombie(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p, float dist, int8_t flr)
{
    work(w, 1);
    memset(p, 0, sizeof(*p));
    w->exp0 = p;
    w->flr_no = flr;
    p->player_distance = dist;
}

/* ===========================================================================
   1 — Parent: Init und Move-Dispatch (en01sub.c:92-115)
   =========================================================================== */
static void test_parent_init(void)
{
    RECVX_ENEMY_WORK w;
    setup();
    work(&w, 0x26);
    w.stflg = 0x123u;
    w.mode1 = 7;
    recvx_en01_parent(&w);
    check_eq(w.stflg, 0, "Parent_Init stflg = 0 (:100)");
    check(w.flg & 0x8000u, "Parent_Init flg |= 0x8000 (:102)");
    check_eq(w.mode0, 1, "Parent_Init mode0 = 1 (:104)");
    check_eq(w.mode1, 0, "Parent_Init mode1 = 0 (:105)");
    w.mode0 = 2;
    recvx_en01_parent(&w);
    check(gap_seen("bhEne01Parent_Mode0"), "Mode0[2] liegt hinter der Tabelle (:16)");
    teardown();
}

/* ===========================================================================
   2 — Parent_NearestCheck (en01sub.c:118-150)
   =========================================================================== */
static void test_nearest(void)
{
    RECVX_ENEMY_WORK par, a, b, c, d, e, f;
    RECVX_EN01_EXP0 pa, pb, pc, pd, pe, pf;

    setup();
    work(&par, 0x26);
    zombie(&a, &pa, 30.0f, 0); pa.flags2 = 0x2u | 0x1u;   /* altes Bit 2 */
    zombie(&b, &pb, 10.0f, 0);
    zombie(&c, &pc, 20.0f, 0);
    zombie(&d, &pd, 1.0f, 0);  d.type = 10; pd.flags2 = 0x2u;  /* type 10 */
    zombie(&e, &pe, 0.5f, 0);  e.flg |= 0x80u;                 /* verknuepft */
    zombie(&f, &pf, 0.1f, 0);  f.flg |= 0x2u;                  /* sterbend */
    g_table.slot[0] = &par;
    g_table.slot[1] = &a;
    g_table.slot[2] = &b;
    g_table.slot[3] = &c;
    g_table.slot[4] = &d;
    g_table.slot[5] = &e;
    g_table.slot[6] = NULL;                                    /* Luecke */
    g_table.slot[7] = &f;
    g_table.ewk_n = 8;

    recvx_en01_parent_nearest_check(&par);
    check_eq(pb.flags2 & 0x2u, 0x2u, "naechster Zombie erhaelt EXP0_I(68) Bit 0x2 (:148)");
    check_eq(pa.flags2, 0x1u, "uebrige Kandidaten: nur Bit 0x2 geloescht (:133)");
    check_eq(pc.flags2 & 0x2u, 0, "Kandidat c ohne Bit 0x2");
    check_eq(pd.flags2, 0x2u, "type 10 ist kein Kandidat und bleibt unberuehrt (:131)");
    check_eq(pe.flags2, 0, "flg 0x80 ist kein Kandidat");
    check_eq(pf.flags2, 0, "flg 0x2 ist kein Kandidat");

    /* Gleichstand: `dist > ...` -> der fruehere Slot bleibt (:135). */
    pc.player_distance = 10.0f;
    recvx_en01_parent_nearest_check(&par);
    check_eq(pb.flags2 & 0x2u, 0x2u, "Gleichstand: frueherer Slot behaelt die Marke");
    check_eq(pc.flags2 & 0x2u, 0, "Gleichstand: spaeterer Slot ohne Marke");

    /* Uebernahme durch Move (:113). */
    par.mode0 = 1;
    pa.player_distance = 2.0f;
    recvx_en01_parent(&par);
    check_eq(pa.flags2 & 0x2u, 0x2u, "Parent_Move ruft NearestCheck (:113)");
    check_eq(pb.flags2 & 0x2u, 0, "Parent_Move: alte Marke geloescht");
    teardown();
}

/* ===========================================================================
   3 — Parent_NoKaidanCheck (en01sub.c:153-210)
   =========================================================================== */
static void test_nokaidan(void)
{
    RECVX_ENEMY_WORK par, a, b, c, d, e;
    RECVX_EN01_EXP0 pa, pb, pc, pd, pe;

    setup();
    work(&par, 0x26);
    zombie(&a, &pa, 0.0f, 2); a.flg |= 0x200000u;  /* auf Etage 2, Altmarke */
    zombie(&b, &pb, 0.0f, 2);
    zombie(&c, &pc, 0.0f, 1);
    zombie(&d, &pd, 0.0f, 0);
    zombie(&e, &pe, 0.0f, 1); e.type = 10; e.flg |= 0x200000u; /* kein Kandidat */
    g_table.slot[0] = &par;
    g_table.slot[1] = &a;
    g_table.slot[2] = &b;
    g_table.slot[3] = &c;
    g_table.slot[4] = &d;
    g_table.slot[5] = &e;
    g_table.ewk_n = 6;

    /* Ohne Stage-Satz: Luecke, keine Aenderung. */
    recvx_en01_parent_nokaidan_check(&par);
    check(gap_seen("sys->stg_no"), "stage_valid 0 -> Luecke statt Vergleich mit 0");
    check(a.flg & 0x200000u, "ohne Stage-Satz bleibt flg unberuehrt");

    /* ene_stg_tbl[0] = {stg 0, rom 6, flr 2, ene_max 2} (:157). Stage 0. */
    g_ctx.stg_no = 0; g_ctx.rom_no = 3; g_ctx.stage_valid = 1u;
    recvx_en01_set_context(&g_ctx);
    recvx_en01_parent_nokaidan_check(&par);
    check_eq(a.flg & 0x200000u, 0, "Kandidat auf Etage 2: flg 0x200000 geloescht (:190)");
    check_eq(b.flg & 0x200000u, 0, "Kandidat auf Etage 2 ohne Marke");
    check_eq(c.flg & 0x200000u, 0x200000u, "n >= ene_max: Kandidat ausserhalb Etage 2 markiert (:207)");
    check_eq(d.flg & 0x200000u, 0x200000u, "n >= ene_max: zweiter Kandidat ausserhalb markiert");
    check_eq(e.flg & 0x200000u, 0x200000u, "type 10 bleibt unberuehrt (:188)");

    /* n = 1 < ene_max 2: niemand wird markiert, alle Marken geloescht. */
    b.flr_no = 1;
    recvx_en01_parent_nokaidan_check(&par);
    check_eq(c.flg & 0x200000u, 0, "n < ene_max: Marke geloescht");
    check_eq(b.flg & 0x200000u, 0, "n < ene_max: keine neue Marke");

    /* Raum 6 einer anderen Stage passt wegen `||` ebenfalls (:177). */
    b.flr_no = 2;
    g_ctx.stg_no = 1; g_ctx.rom_no = 6;
    recvx_en01_set_context(&g_ctx);
    recvx_en01_parent_nokaidan_check(&par);
    check_eq(c.flg & 0x200000u, 0x200000u, "Stage 1 / Raum 6 passt ueber || (:177)");

    /* Stage 1 / Raum 5: Tabellenende -1, Rueckkehr ohne Aenderung (:172). */
    g_ctx.stg_no = 1; g_ctx.rom_no = 5;
    recvx_en01_set_context(&g_ctx);
    c.flg |= 0x200000u; a.flg |= 0x200000u;
    recvx_en01_parent_nokaidan_check(&par);
    check(a.flg & 0x200000u, "kein Tabelleneintrag: auch Etage-2-Kandidat behaelt Marke");
    check(c.flg & 0x200000u, "kein Tabelleneintrag: flg unveraendert");
    teardown();
}

/* ===========================================================================
   4 — Init/Move aller Kindfamilien
   =========================================================================== */
static int alloc_size_of(void* p)
{
    int i;
    for (i = 0; i < g_alloc_n; ++i) if (g_alloc[i] == p) return (int)g_alloc_size[i];
    return -1;
}

static void test_arm_leg_head_init_move(void)
{
    RECVX_ENEMY_WORK w, before;
    RECVX_EN01_SUB_EXP0* x;
    int fam;

    for (fam = 0; fam < 3; ++fam) {
        setup();
        work(&w, (uint16_t)(fam == 0 ? 0x20 : fam == 1 ? 0x21 : 0x1F));
        w.stflg = 0x55u;
        if (fam == 0) recvx_en01_arm(&w);
        else if (fam == 1) recvx_en01_leg(&w);
        else recvx_en01_head(&w);
        x = (RECVX_EN01_SUB_EXP0*)w.exp0;
        check(x != NULL, "Init legt exp0 an (:227/:671/:809)");
        if (!x) { teardown(); continue; }
        check_eq(alloc_size_of(w.exp0),
                 fam == 2 ? (long long)sizeof(RECVX_EN01_SUB_EXP0)
                          : (long long)sizeof(RECVX_EN01_SUB_EXP0_MTX),
                 "Blockgroesse: Kopf 32, Arm/Bein 112 Byte (nativ + Zeigerzuwachs)");
        check_eq(x->node_num, fam == 2 ? 0 : 2, "EXP0_I(8): Arm/Bein 2, Kopf 0 (:232/:676/:814)");
        check(x->obj.work == NULL && x->owk.work == NULL, "EXP0_I(0)/(4) = 0");
        check_eq(w.stflg, 0, "Init stflg = 0");
        check(w.flg & 0x8000u, "Init flg |= 0x8000");
        check_eq(w.mode0, 1, "Init mode0 = 1");

        /* Move = MV00, leer (:249/:693/:831). */
        before = w;
        if (fam == 0) recvx_en01_arm(&w);
        else if (fam == 1) recvx_en01_leg(&w);
        else recvx_en01_head(&w);
        check(memcmp(&before, &w, sizeof(w)) == 0, "Move/MV00 aendert nichts");
        check_eq(g_gap_n, 0, "Init/Move ohne Luecke");

        /* Mode0[2] ist NULL (:25/:37/:48). */
        w.mode0 = 2;
        if (fam == 0) recvx_en01_arm(&w);
        else if (fam == 1) recvx_en01_leg(&w);
        else recvx_en01_head(&w);
        check(gap_seen("Mode0[mode0] NULL"), "Mode0[2] NULL wird gemeldet");

        /* Zweiter Init behaelt den Block (exp0 != NULL, :225). */
        {
            void* old = w.exp0;
            w.mode0 = 0;
            if (fam == 0) recvx_en01_arm(&w);
            else if (fam == 1) recvx_en01_leg(&w);
            else recvx_en01_head(&w);
            check(w.exp0 == old, "zweiter Init allokiert nicht neu");
        }
        teardown();
    }
}

static void test_cap_init_move(void)
{
    RECVX_ENEMY_WORK par, w;
    RECVX_EN01_EXP0 pp;

    setup();
    zombie(&par, &pp, 0.0f, 0);
    par.stflg = 0x1000000u;
    work(&w, 0x22);
    w.lkwkp = &par;
    w.px = 5.0f; w.ay = 77;
    recvx_en01_cap(&w);
    check(alloc_size_of(w.exp0) == (int)sizeof(RECVX_EN01_SUB_EXP0_MTX), "Cap: 112-Byte-Block (:1009)");
    check(w.flg & 0x80u, "Cap_Init ohne flg 0x80: setzt 0x80 (:1014)");
    check_f(w.px, 0.0f, "Cap_Init nullt px (:1016)");
    check_eq(w.ay, 0, "Cap_Init nullt ay (:1021)");
    /* Spiegelung VOR Init (:986), Init setzt stflg = 0 (:1003). */
    check_eq(w.stflg, 0, "Cap: Init nach der Spiegelung loescht stflg");
    check_eq(g_calc_model, 1, "Cap ruft bhCalcModel je Pass (:997)");

    recvx_en01_cap(&w);                                      /* Move/MV00 */
    check_eq(w.stflg & 0x1000000u, 0x1000000u, "Cap spiegelt lkwkp->stflg 0x1000000 (:988)");
    check_eq(g_calc_model, 2, "Cap: zweiter Pass bhCalcModel");
    par.stflg = 0;
    recvx_en01_cap(&w);
    check_eq(w.stflg & 0x1000000u, 0, "Cap loescht die Spiegelung (:992)");

    /* flg 0x80 vorhanden: Lage bleibt (:1012). */
    work(&w, 0x22);
    w.lkwkp = &par;
    w.flg |= 0x80u; w.px = 5.0f;
    recvx_en01_cap(&w);
    check_f(w.px, 5.0f, "Cap_Init mit flg 0x80 behaelt px");
    teardown();
}

static void test_worm_init_move(void)
{
    RECVX_ENEMY_WORK par, w;
    RECVX_EN01_EXP0 pp;
    float m[16];

    setup();
    zombie(&par, &pp, 0.0f, 0);
    work(&w, 0x23);
    w.exp1 = &par;
    w.lkwkp = &par;
    w.flg |= 0x100000u | 0x2u;
    recvx_en01_worm(&w);
    check_eq(w.mtn_no, 168, "Worm_Init mtn_no 168 (:1296)");
    check_eq(w.mtn_add, 65536, "Worm_Init mtn_add 65536 (:1295)");
    check(alloc_size_of(w.exp0) == (int)sizeof(RECVX_EN01_SUB_EXP0_MTX), "Worm: 112-Byte-Block (:1302)");
    check_eq(w.flg & 0x100000u, 0, "Worm_Init loescht flg 0x100000 (:1288)");
    check_eq(w.flg & 0x2u, 0, "Worm_Init loescht flg 0x2 (:1329)");
    check(w.flg & 0x80u, "Worm_Init: flg 0x80 (:1307)");
    check(w.mdflg & 0x1u, "Worm_Init: mdflg 0x1 (:1308)");
    check_eq(g_set_motion, 1, "Worm ruft bhSetMotion je Pass (:1266)");
    check_eq(g_calc_model, 1, "Worm: ohne mdflg 4 bhCalcModel (:1278)");

    /* MV00 (mode2 0) leer; mdflg 4 unterdrueckt bhCalcModel. */
    w.mdflg |= 0x4u;
    recvx_en01_worm(&w);
    check_eq(g_calc_model, 1, "Worm: mdflg 0x4 unterdrueckt bhCalcModel (:1276)");
    check_eq(g_set_motion, 2, "Worm: bhSetMotion auch bei mdflg 4");

    /* mtn_no 159 nullt objP[0].pos x/z (:1268-1273). */
    w.mtn_no = 159;
    recvx_en01_worm(&w);
    check_eq(g_clear_xz_node, 0, "mtn 159: objP[0].pos[0/2] = 0");

    /* Zweiter Init mit Spielerbezug (lkwkp != exp1, :1318): Bezug lösen. */
    w.mode0 = 0;
    w.lkwkp = NULL;              /* nativ: am Spieler (link_player) */
    recvx_en01_worm(&w);
    check(w.lkwkp == &par, "Worm_Init: lkwkp = exp1 (:1322)");
    check_eq(g_link_calls[0], 1, "Spielerbezug wird beim Besitzer aufgehoben");
    check_f(w.loy, 1.0f, "Worm_Init: loy = 1.0 (:1325)");

    /* MV01 mode3 0: Motion nach mode1, Richtung, SE (:1356-1383). */
    model_add(&w);
    w.mode0 = 1; w.mode2 = 1; w.mode1 = 2; w.mode3 = 0;
    w.mdflg = 0; w.flg |= 0x80u;
    w.px = 0.0f; w.pz = 0.0f; w.ay = 0;
    g_player.pos.x = 10.0f; g_player.pos.z = 0.0f;
    g_se_n = 0;
    recvx_en01_worm(&w);
    check_eq(w.mtn_no, 156, "MV01 mode1 2 -> MTN 156 (:1372)");
    check_eq(w.flg & 0x80u, 0, "MV01 loest flg 0x80 (:1359)");
    check_eq(w.mode3, 1, "MV01 mode3 -> 1 (:1383)");
    check(g_se_n == 1 && g_se[0] == 74560, "MV01 SE 74560 am Elternwork (:1381)");
    check_eq(w.ay, recvx_en01_niten_dir(0.0f, 0.0f, 10.0f, 0.0f) , "MV01 ay = NitenDir_ck (:1379)");

    /* MV01 mode3 2: Abstand < 5 -> Spielergriff (:1410-1445). */
    w.mode3 = 2; w.px = 12.0f; w.pz = 0.0f; g_player.pos.x = 10.0f;
    recvx_en01_worm(&w);
    check_eq(w.mtn_no, 159, "MV01 Griff: MTN 159 (:1412)");
    check_eq(g_link_calls[1], 1, "MV01 Griff: lkwkp = plp (:1417)");
    check(w.lkwkp == NULL && (w.flg & 0x80u), "MV01 Griff: am Spieler, flg 0x80");
    check_f(w.loz, 1.870168f, "MV01 Griff: loz (:1422)");
    check_eq(g_player.mtn_no, 42, "MV01 Griff: plp->mtn_no 42 (:1430)");
    check_eq(g_player.mode0, 4, "MV01 Griff: plp->mode0 4 (:1441)");
    check_eq(w.mode3, 3, "MV01 Griff: mode3 3");

    /* MV02 mode3 0/1: Hebel (:1576-1582). */
    w.mode2 = 2; w.mode3 = 0;
    g_player.flg = 0; g_player.hp = 50; g_lever = 3;
    recvx_en01_worm(&w);
    check_eq(w.ct0, 89, "MV02: ct0 90, dann --ct0 (:1576/:1621)");
    check_eq(w.ct2, 57, "MV02: ct2 60 - LeverCheck (:1578/:1582)");
    check_eq(w.mode3, 1, "MV02: bleibt in mode3 1");

    /* Ungueltiger MoveMode-Index. */
    w.mode2 = 3;
    recvx_en01_worm(&w);
    check(gap_seen("Worm_MoveMode"), "MoveMode[3] ausserhalb wird gemeldet");
    (void)m;
    teardown();
}

static void test_bom(void)
{
    RECVX_ENEMY_WORK par, w;
    RECVX_EN01_EXP0 pp;
    FAKE_MODEL* mdl;

    setup();
    zombie(&par, &pp, 0.0f, 0);
    par.type = 13;
    work(&w, 0x24);
    w.lkwkp = &par;
    w.mdflg = 0x1u;
    mdl = model_add(&w);
    mdl->owk[0][12] = 5.0f; mdl->owk[0][13] = 6.0f; mdl->owk[0][14] = 7.0f;

    recvx_en01_bom(&w);
    check_eq(w.hp, 1, "Bom_Init hp 1 (:1839)");
    check_eq(w.flg & 0x8020u, 0x8020u, "Bom_Init flg |= 0x8020 (:1846)");
    check(w.cpcl && w.cpcl[0].z == 14 && w.cpcl[1].z == 0, "Bom_Init cpcl = Ene01BomCapColTab (:1813)");
    check_eq(w.mdflg & 0x1u, 0, "Bom_Init: Elterntyp 13 -> mdflg 0x1 geloescht (:1857)");
    check_f(w.ar, 3.0f, "Bom_Init ar 3.0");
    check_f(w.watr.c1.y, 7.0f, "Bom watr.c1 = owP[0] * (0,1,0) (:1804)");
    check_f(w.watr.c2.y, 5.0f, "Bom watr.c2 = owP[0] * (0,-1,0) (:1808)");
    check_f(w.watr.r, 2.0f, "Bom watr.r 2.0 (:1810)");

    /* Move: Treffer mit Waffe 2 loest nichts aus (:1876). */
    w.flg |= 0x4u; w.wpnr_no = 2;
    recvx_en01_bom(&w);
    check_eq(g_explode_n, 0, "Waffe 2 zuendet nicht");
    check_eq(w.flg & 0x4u, 0, "flg 0x4 nach dem Pass geloescht (:1793)");
    /* Waffe 17 nur mit flg2 0x4. */
    w.flg |= 0x4u; w.wpnr_no = 17;
    recvx_en01_bom(&w);
    check_eq(g_explode_n, 0, "Waffe 17 ohne flg2 0x4 zuendet nicht");
    w.flg |= 0x4u; w.wpnr_no = 17; w.flg2 = 0x4u;
    g_grand = 10.0f;
    g_se_n = 0;
    recvx_en01_bom(&w);
    check_eq(g_explode_n, 1, "Waffe 17 mit flg2 0x4 zuendet (:1885)");
    check_f(g_explode.z, 9.0f, "Explosionspunkt owP[0] * (0,0,2) (:1878)");
    check_f(g_explode.y, 11.0f, "unter grand -> y = 1.0 + grand (:1882)");
    check(g_se_n == 1 && g_se[0] == 4936, "SE 4936 am Elternwork (:1887)");
    check_eq(w.mdflg & 0x1u, 0x1u, "nach Explosion mdflg 0x1 (:1889)");
    check_eq(w.flg & 0x20u, 0, "nach Explosion flg 0x20 geloescht (:1891)");
    check_eq(w.mode3, 1, "Bom mode3 1 (:1893)");

    /* Elternwork stirbt: flg 0x20 weg (:1784). */
    w.flg |= 0x20u; par.flg |= 0x2u;
    recvx_en01_bom(&w);
    check_eq(w.flg & 0x20u, 0, "Eltern flg 0x2 -> Bom flg 0x20 geloescht");
    teardown();
}

static void test_scope(void)
{
    RECVX_ENEMY_WORK par, w;
    RECVX_EN01_EXP0 pp;
    int i;

    setup();
    zombie(&par, &pp, 0.0f, 0);
    par.type = 14;
    work(&w, 0x25);
    w.lkwkp = &par;
    w.mdflg = 0x1u;
    for (i = 0; i < 16; ++i) g_work_mtx[i] = 0.0f;
    g_work_mtx[0] = g_work_mtx[5] = g_work_mtx[10] = g_work_mtx[15] = 1.0f;
    g_work_mtx[12] = 1.0f; g_work_mtx[13] = 2.0f; g_work_mtx[14] = 3.0f;

    recvx_en01_scope(&w);
    check_eq(w.mdflg & 0x1u, 0, "Scope_Init: Elterntyp 14 -> mdflg 0x1 geloescht (:1924)");
    check(w.flg & 0x8000u, "Scope_Init flg |= 0x8000");
    check_eq(w.mode0, 1, "Scope_Init mode0 1");
    check_eq(g_calc_model, 1, "Scope bhCalcModel (:1914)");
    check_eq(g_ef_n, 2, "Scope_Effect: zwei Effekte (:1977)");
    check_eq(g_ef[0].id, 355, "Effekt 355 (:1966)");
    check_f(g_ef[0].sx, 0.18f, "Skala 0.18 (:1971)");
    check_f(g_ef[0].px, 1.3f, "scope_ofs[0] auf epw->mtx (:1979)");
    check_f(g_ef[1].px, 0.7f, "scope_ofs[1] auf epw->mtx");
    check_f(g_ef[0].pz, 1.9f, "scope_ofs z -1.1 + 3");
    check_eq(g_ef[0].keep, RECVX_EN01_EF_KEEP_AY, "sys->ef.ay wird nicht gesetzt (keep)");
    check_eq(g_slot_n, 2, "zwei eff[eno]-Nachschriften");
    check_eq(g_slot_mask[0], RECVX_EN01_EFS_ANG | RECVX_EN01_EFS_AZ |
                             RECVX_EN01_EFS_CT3 | RECVX_EN01_EFS_EXP0,
             "eff: ax/ay/az, ct3, exp0 (:1985-1991)");
    check(g_slot[0].exp0 == &par, "eff[eno].exp0 = lkwkp (:1991)");
    check_eq(g_slot[0].ax, 0, "Einheitsmatrix -> ax 0");

    /* Elternwork tot: kein Effekt (:1961). */
    g_ef_n = 0;
    par.flg |= 0x2u;
    recvx_en01_scope(&w);
    check_eq(g_ef_n, 0, "Eltern flg 0x2 -> kein Effekt");
    /* Elterntyp != 14: mdflg 0x1 (:1928), damit kein Effekt. */
    par.flg &= ~0x2u; par.type = 0;
    w.mode0 = 0;
    recvx_en01_scope(&w);
    check_eq(w.mdflg & 0x1u, 0x1u, "Scope_Init: anderer Typ -> mdflg 0x1");
    check_eq(g_ef_n, 0, "mdflg 0x1 -> kein Effekt");
    teardown();
}

/* ===========================================================================
   5 — Schadenszweige mit Knotenreferenzen
   =========================================================================== */
static void test_head_dg00(void)
{
    RECVX_ENEMY_WORK par, w;
    RECVX_EN01_EXP0 pp;
    RECVX_EN01_SUB_EXP0* x;
    FAKE_MODEL* mdl;
    float psx, psy;
    NJS_MATRIX r;

    setup();
    zombie(&par, &pp, 0.0f, 0);
    par.mdlver = 3;
    mdl = model_add(&par);
    mdl->owk[11][13] = 10.0f;
    work(&w, 0x1F);
    w.lkwkp = &par;
    recvx_en01_head(&w);                                    /* Init */
    x = (RECVX_EN01_SUB_EXP0*)w.exp0;
    /* bhEne01_NG03B (en01b.c:328-333) */
    x->obj.work = &par; x->obj.node = 11;
    x->owk.work = &par; x->owk.node = 11;
    w.mode0 = 3; w.mode2 = 0; w.mode3 = 0; w.ay = 0;

    recvx_en01_head(&w);
    check_f(w.px, 2.0f, "Head_DG00 px 2.0 (:860)");
    check_eq(w.ax, 4096, "Head_DG00 ax 4096 (:866)");
    check_eq(w.az, 7424, "Head_DG00 az 7424 (:867)");
    check_eq(w.ct0, 1, "ct0++ im Blutzweig (:871)");
    check_eq(w.ct1, 1, "ct1++ (:916)");
    check_eq(g_blood3_n, 5, "ein SetBlood3 Typ 2 + vier Typ 5 (:887/:907)");
    check(g_blood3_type[0] == 2 && g_blood3_type[4] == 5, "Bluttypen 2 dann 5");
    check_eq(par.djnt_no, 11, "ep->djnt_no = 11 (:877)");

    psx = 2.0f * njCos(7424);
    psy = 2.0f * njSin(7424) - 0.0f;
    njUnitMatrix(&r);
    njRotateX(&r, 4096);
    check_f(mdl->owk[11][13], 10.0f + psy, "owP[11].mtx[13] += ps.y (:921)");
    check_f(mdl->owk[11][14], -psx, "owP[11].mtx[14] -= ps.x * cos(ay) (:922)");
    check_f(mdl->owk[11][5], r[5], "njRotateX(owk, ax) (:918)");
    check_eq(w.mode3, 1, "ohne Wandtreffer bleibt mode3 1");

    /* Wandtreffer nach zwei Abprallern: Knoten verstecken (:964-970). */
    w.ct2 = 2; g_wall_hit = 1; g_wall_type = 0u;
    recvx_en01_head(&w);
    check_eq(mdl->owflg[11] & 0x100u, 0x100u, "owk->flg |= 0x100 (:968)");
    check_eq(w.mode3, 3, "mode3++ und unter -10 erneut ++ (:970/:975)");
    teardown();
}

static void test_arm_leg_dg(void)
{
    RECVX_ENEMY_WORK par, w;
    RECVX_EN01_EXP0 pp;
    RECVX_EN01_SUB_EXP0_MTX* xm;
    FAKE_MODEL* mdl;
    NJS_MATRIX e;

    /* Arm_DG00 mode3 0 -> 1 (en01sub.c:277-333) */
    setup();
    zombie(&par, &pp, 0.0f, 0);
    mdl = model_add(&par);
    mdl->owk[4][13] = 20.0f;
    mdl->pos[5].y = 1.5f;
    work(&w, 0x20);
    w.lkwkp = &par;
    recvx_en01_arm(&w);
    xm = (RECVX_EN01_SUB_EXP0_MTX*)w.exp0;
    xm->base.obj.work = &par; xm->base.obj.node = 4;
    xm->base.owk.work = &par; xm->base.owk.node = 4;
    xm->base.djnt_no = 7;
    w.mode0 = 3; w.mode2 = 0; w.mode3 = 0;
    w.dvx = 1.0f; w.dvz = -2.0f; w.ct3 = 9;
    g_grand = -100.0f;
    recvx_en01_arm(&w);
    check_eq(g_blood2_n, 2, "Arm_DG00: zwei SetBlood2 bei ct0 % 8 == 0 (:289/:292)");
    check(g_blood2_djnt[0] == 7 && g_blood2_djnt[1] == 9, "djnt_no EXP0_I(12), dann ct3");
    check_f(mdl->owk[4][12], 0.1f, "owk[0].mtx[12] += dvx / 10 (:295)");
    check_f(mdl->owk[4][14], -0.2f, "owk[0].mtx[14] += dvz / 10 (:297)");
    check_f(mdl->owk[5][13], 21.5f, "owk[1] = owk[0] * T(obj[1].pos) (:301-304)");
    check_eq(w.ct0, 1, "ct0++ (:310)");
    check_eq(w.mode3, 1, "pd.y ueber grand: mode3 bleibt 1");
    /* Boden erreicht: exp0-Matrix und EXP0_F(100) = grand (:320-330). */
    g_grand = 100.0f;
    recvx_en01_arm(&w);
    check_eq(w.mode3, 2, "pd.y < grand -> mode3 2");
    check_f(xm->mtx[13], 100.0f, "EXP0_F(100) ist mtx[13] der exp0-Matrix (:326)");
    teardown();

    /* Arm ohne Knotenzeiger (Original-Nullzeiger): gemeldet. */
    setup();
    zombie(&par, &pp, 0.0f, 0);
    work(&w, 0x20);
    w.lkwkp = &par;
    recvx_en01_arm(&w);
    w.mode0 = 3; w.mode2 = 0; w.mode3 = 1; w.ct0 = 1;
    recvx_en01_arm(&w);
    check(gap_seen("O_WORK* exp0+4 NULL"), "Arm_DG00 mit EXP0_I(4) == 0 wird gemeldet");
    teardown();

    /* Leg_DG00 mode3 0 (:717-731): Winkel genullt, exp0-Matrix gesetzt. */
    setup();
    zombie(&par, &pp, 0.0f, 0);
    mdl = model_add(&par);
    mdl->ang[3][1] = 1234; mdl->ang[4][2] = 99;
    mdl->owk[4][13] = 5.0f;
    work(&w, 0x21);
    w.lkwkp = &par;
    recvx_en01_leg(&w);
    xm = (RECVX_EN01_SUB_EXP0_MTX*)w.exp0;
    xm->base.obj.work = &par; xm->base.obj.node = 3;
    xm->base.owk.work = &par; xm->base.owk.node = 3;
    w.mode0 = 3; w.mode2 = 0; w.mode3 = 0;
    g_wall_hit = 1; g_wall_type = 7u;                       /* Wassertyp 7 */
    recvx_en01_leg(&w);
    check_eq(mdl->ang[3][1], 0, "obj[0].ang = 0 (:722)");
    check_eq(mdl->ang[4][2], 0, "obj[1].ang = 0 (:723)");
    memcpy(e, mdl->owk[4], sizeof(e));
    check_f(xm->mtx[13], 5.0f - 1.0f, "exp0-Matrix = owk[1] * T(0,-1,-2.2) (:727-729)");
    check_eq(w.mode3, 2, "hp->type 7: Bein faellt ab, mode3 2 (:765-777)");
    check_f(mdl->owk[3][13], -100.0f, "owk[0].mtx[13] += -100 (:767)");
    teardown();
}

static void test_cap_dg01_gap(void)
{
    RECVX_ENEMY_WORK par, w;
    RECVX_EN01_EXP0 pp;
    FAKE_MODEL* mdl;

    setup();
    zombie(&par, &pp, 0.0f, 0);
    work(&w, 0x22);
    w.lkwkp = &par;
    mdl = model_add(&w);
    mdl->owk[0][13] = 4.0f;
    recvx_en01_cap(&w);                                     /* Init */
    w.mode0 = 3; w.mode2 = 1; w.mode3 = 0;
    recvx_en01_cap(&w);
    /* ORIGINAL-FIX (QUALITY.md): das uninitialisierte pd (:1208) gilt als "keine Wand", keine Luecke mehr. */
    check(!gap_seen("pd uninitialisiert"), "Cap_DG01: pd-Fix meldet noch eine Luecke (:1208)");
    check_f(w.dpx, 0.16f, "Cap_DG01 dpx 0.16 (:1172)");
    check_eq(w.waz, 0, "Cap_DG01 waz zuletzt 0 (:1187)");
    teardown();
}

int main(void)
{
    test_parent_init();
    test_nearest();
    test_nokaidan();
    test_arm_leg_head_init_move();
    test_cap_init_move();
    test_worm_init_move();
    test_bom();
    test_scope();
    test_head_dg00();
    test_arm_leg_dg();
    test_cap_dg01_gap();
    printf("en01_sub: %d Pruefungen, %d Fehler\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
