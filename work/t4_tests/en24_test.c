/* Komponentenpruefung EN24 (T4). Prueft originale Zustandsuebergaenge, nicht
   den neuen Code: jeder Fall ist so gebaut, dass eine falsche Bindung
   (fehlender Fallthrough, vertauschte Flagbits, fehlende Positionsklemmung,
   fehlende Kumulierung der Sichtmatrix) auffaellt.
   Build/Lauf steht im Bericht. */
#include "en24.h"
#include "ninja_math.h"
#include "core_random.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static int failures;
static int checks;

static void ok(int cond, const char* what)
{
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}

static void okf(float got, float want, float eps, const char* what)
{
    checks++;
    if (fabsf(got - want) > eps) {
        failures++;
        printf("FAIL: %s (got %.6f want %.6f)\n", what, (double)got, (double)want);
    }
}

/* ------------------------------------------------------------------ Dienste */
#define GAPS 64
static char gap_log[GAPS][160];
static int  gap_n;
static int  calc_model_calls;
static int  set_motion_calls;
static int  collision_works_calls;
static int  se_last, se_n;
static int  blood4_n, blood4_col, blood4_t1, blood4_t2;
static RECVX_ENEMY_VEC blood4_pos, blood4_vec;
static int  wall_type_hit;
static float ground_value;
static int  line_hit;
static RECVX_ENEMY_VEC line_to, line_normal, hit_normal_v;
static RECVX_ENEMY_PLAYER player;
static int  player_bound = 1;
static float joint_mtx[32][16];
static int  joint_available = 1;
static unsigned char arena[4096];
static unsigned arena_used;

static void t_gap(const char* name)
{
    if (gap_n < GAPS) { strncpy(gap_log[gap_n], name, 159); gap_log[gap_n][159] = 0; }
    gap_n++;
}
static int gap_seen(const char* needle)
{
    int i;
    for (i = 0; i < gap_n && i < GAPS; i++) if (strstr(gap_log[i], needle)) return 1;
    return 0;
}
static int32_t t_random(void) { return recvx_core_random_next(); }
static void t_calc_model(RECVX_ENEMY_WORK* w) { (void)w; calc_model_calls++; }
static int  t_set_motion(RECVX_ENEMY_WORK* w, int add, uint32_t md)
{ (void)w; (void)add; (void)md; set_motion_calls++; return 0; }
static int  t_collision_works(RECVX_ENEMY_WORK* w) { (void)w; collision_works_calls++; return 0; }
static void t_enemy_se(uint32_t e, const RECVX_ENEMY_VEC* p, int se)
{ (void)e; (void)p; se_last = se; se_n++; }
static uint32_t t_enemy_now(void) { return 0u; }
static int  t_wall_type(const RECVX_ENEMY_VEC* p, uint32_t f, float r, float h)
{ (void)p; (void)f; (void)r; (void)h; return wall_type_hit; }
static float t_ground(const RECVX_ENEMY_VEC* p) { (void)p; return ground_value; }
static int  t_line(uint32_t v, const RECVX_ENEMY_VEC* from, RECVX_ENEMY_VEC* to,
                   RECVX_ENEMY_VEC* n, uint32_t f, int fl,
                   RECVX_ENEMY_ATR* rec, uint32_t* id)
{
    (void)v; (void)from; (void)f; (void)fl; (void)rec; (void)id;
    if (!line_hit) return 0;
    *to = line_to;
    *n = line_normal;
    return 1;
}
static int t_hit_normal(RECVX_ENEMY_VEC* n) { *n = hit_normal_v; return 1; }
static void t_blood4(const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* vec,
                     int col, int t1, int t2)
{ blood4_pos = *pos; blood4_vec = *vec; blood4_col = col; blood4_t1 = t1; blood4_t2 = t2; blood4_n++; }
static int t_player_get(RECVX_ENEMY_PLAYER* out)
{ if (!player_bound) return 0; *out = player; return 1; }
static void t_player_set(const RECVX_ENEMY_PLAYER* in) { player = *in; }
static int t_player_joint_matrix(uint32_t j, float* out16)
{
    if (!joint_available || j >= 32u) return 0;
    memcpy(out16, joint_mtx[j], sizeof(float) * 16);
    return 1;
}
static void* t_calloc(uint32_t size)
{
    void* p;
    if (arena_used + size > sizeof(arena)) return 0;
    p = &arena[arena_used];
    arena_used += (size + 15u) & ~15u;
    memset(p, 0, size);
    return p;
}

static RECVX_ENEMY_SERVICES services;

static void services_init(void)
{
    memset(&services, 0, sizeof(services));
    services.gap = t_gap;
    services.random_next = t_random;
    services.calc_model = t_calc_model;
    services.set_motion = t_set_motion;
    services.collision_works = t_collision_works;
    services.enemy_se = t_enemy_se;
    services.enemy_now = t_enemy_now;
    services.wall_type = t_wall_type;
    services.ground = t_ground;
    services.line = t_line;
    services.hit_normal = t_hit_normal;
    services.blood_effect4 = t_blood4;
    services.player_get = t_player_get;
    services.player_set = t_player_set;
    services.player_joint_matrix = t_player_joint_matrix;
    services.calloc_work = t_calloc;
}

static void reset(RECVX_ENEMY_WORK* w)
{
    memset(w, 0, sizeof(*w));
    w->sv = &services;
    gap_n = 0;
    calc_model_calls = set_motion_calls = collision_works_calls = 0;
    se_n = 0; se_last = 0; blood4_n = 0;
    wall_type_hit = 0; ground_value = -1000.0f; line_hit = 0;
    joint_available = 1; player_bound = 1;
    arena_used = 0;
    memset(&player, 0, sizeof(player));
    memset(joint_mtx, 0, sizeof(joint_mtx));
    recvx_core_random_seed(1u);
}

/* Der EXP0-Block liegt im Arena-Puffer; die Offsets sind die des Originals. */
static float exp0_f(const RECVX_ENEMY_WORK* w, unsigned o)
{ float v; memcpy(&v, (const char*)w->exp0 + o, 4); return v; }
static int32_t exp0_i(const RECVX_ENEMY_WORK* w, unsigned o)
{ int32_t v; memcpy(&v, (const char*)w->exp0 + o, 4); return v; }
static unsigned char exp0_uc(const RECVX_ENEMY_WORK* w, unsigned o)
{ return *((const unsigned char*)w->exp0 + o); }

/* =========================================================== 1. Init ===== */
static void test_init(void)
{
    RECVX_ENEMY_WORK w;
    reset(&w);
    w.mode0 = 0;
    w.flg = 0x26u;          /* alle drei Bits, die Init loeschen muss */
    w.mdflg = 0x20u;        /* Bit, das Init am Ende loeschen muss   */

    recvx_en24(&w);

    /* en24.c:116/117 */
    ok((w.flg & 0x8010u) == 0x8010u, "Init setzt flg 0x8010");
    ok((w.flg & 0x26u) == 0u, "Init loescht flg 0x26");
    /* en24.c:119 gefolgt von :147 */
    ok((w.mdflg & 5u) == 5u, "Init setzt mdflg 5");
    ok((w.mdflg & 0x20u) == 0u, "Init loescht mdflg 0x20 nach dem Setzen");

    okf(w.ar, 1.2f, 1e-6f, "ar");
    okf(w.ah, 1.2f, 1e-6f, "ah");
    okf(w.car, 1.2f, 1e-6f, "car");
    okf(w.cah, 1.2f, 1e-6f, "cah");
    ok(w.hp == 2, "hp 2");
    ok((w.flg2 & 0x10u) == 0x10u, "flg2 0x10");

    /* en24.c:131 — ruhend */
    ok(w.mode0 == 6u, "Init setzt mode0 = 6 (Dummy)");

    ok(w.exp0 != 0, "EXP0 belegt");
    okf(exp0_f(&w, 0), 1024.0f, 0.0f, "EXP0_F(0) == 1024.0 aus Bitmuster 0x44800000");
    ok(exp0_i(&w, 4) == 0 && exp0_i(&w, 8) == 0 &&
       exp0_i(&w, 0xC) == 0 && exp0_i(&w, 0x10) == 0, "EXP0_I(4..0x10) genullt");
    ok(exp0_uc(&w, 0x20) == 0, "EXP0_UC(0x20) genullt");
    {
        int32_t wait = exp0_i(&w, 0x24);
        ok(wait >= 30 && wait <= 60, "brain_wait in 30..60");
    }

    ok(w.clp_jno[0] == 0 && w.clp_jno[1] == -1, "clp_jno {0,-1}");
    ok(w.cpcl != 0 && w.cpcl[0].z == 5 && w.cpcl[1].y == 2, "cpcl = CapColTab");

    /* en24.c:88 — Init laeuft noch im selben Pass durch bhCalcModel */
    ok(calc_model_calls == 1, "bhCalcModel im Init-Pass");
    /* EN24 ruft nie bhSetMotion (en24.c:76-111) */
    ok(set_motion_calls == 0, "kein bhSetMotion in dieser Identitaet");
}

/* ================================================= 2. Ruhen bei mode0 6 === */
static void test_dormant(void)
{
    RECVX_ENEMY_WORK w;
    reset(&w);
    w.mode0 = 6;
    w.flg = 0x10u;          /* wuerde CollisionWalls ausloesen, wenn der
                               Pass liefe */
    recvx_en24(&w);
    ok(calc_model_calls == 0, "mode0 6 laesst den ganzen Pass aus");
    ok(collision_works_calls == 0, "mode0 6: kein bhEne03_Collision");
}

/* ============================================ 3. MV01 Fallthrough ========= */
static void test_mv01_fallthrough(void)
{
    RECVX_ENEMY_WORK w;
    int32_t ct_after;

    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 1; w.mode3 = 0;
    w.flg = 0x80000u;

    recvx_en24(&w);

    /* case 0: flg 0x80000 weg, 0x20 gesetzt, ct0 = 10..29, mode3 = 1.
       case 1 laeuft im GLEICHEN Pass (kein break im Original) und zieht ct0
       um eins ab. Ohne Fallthrough bliebe ct0 unveraendert. */
    ok((w.flg & 0x80000u) == 0u, "MV01 loescht flg 0x80000");
    ok((w.flg & 0x20u) == 0x20u, "MV01 setzt flg 0x20");
    ok(w.mode3 == 1u, "MV01 mode3 = 1");
    ct_after = w.ct0;
    ok(ct_after >= 9 && ct_after <= 28, "MV01 case 1 lief im selben Pass (ct0 abgezogen)");

    /* Ablauf bis ct0 == -1: dann mode1 1, mode2 2 oder 3, mode3 0. */
    {
        int guard = 0;
        while (w.mode3 == 1u && guard++ < 100) recvx_en24(&w);
        ok(w.mode1 == 1u, "MV01 Ende: mode1 = 1");
        ok(w.mode2 == 2u || w.mode2 == 3u, "MV01 Ende: mode2 2 oder 3");
        ok(w.mode3 == 0u, "MV01 Ende: mode3 = 0");
    }
}

/* ============================================ 4. MV02 Verfolgen/Fall ====== */
static void test_mv02(void)
{
    RECVX_ENEMY_WORK w;
    float ay_before;

    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 2; w.mode3 = 0;
    w.py = 0.0f;
    ground_value = -1000.0f;      /* Boden weit unten -> py > ground -> Fall */

    recvx_en24(&w);
    okf(w.spd, 0.3f, 1e-6f, "MV02 spd 0.3");
    ok((w.flg & 0x80000u) == 0x80000u, "MV02 flg 0x80000");
    ok((w.flg & 0x20u) == 0x20u, "MV02 flg 0x20");
    ok(w.way == 273 || w.way == -273, "MV02 way +/-273");
    /* case 0 fliesst in case 1: bei py > ground schaltet derselbe Pass auf
       MV04 um (en24.c:353-358). */
    ok(w.mode2 == 4u && w.mode1 == 0u && w.mode3 == 0u,
       "MV02 case 1 im selben Pass: Boden unter dem Work -> MV04");

    /* Jetzt ohne Fall: Wandtreffer dreht um way. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 2; w.mode3 = 1;
    w.way = 273; w.spd = 0.0f; w.ct0 = 50; w.ct2 = 50;
    w.py = 0.0f; ground_value = 10.0f;    /* Boden oberhalb -> kein Fall */
    wall_type_hit = 1;
    ay_before = (float)w.ay;
    recvx_en24(&w);
    ok((float)w.ay == ay_before + 273.0f, "MV02 Wandtreffer dreht um way");
    ok(w.mode2 == 2u, "MV02 bleibt bei Boden oberhalb");

    /* ct2-Nachlauf laeuft ausserhalb des switch (en24.c:361). */
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 2; w.mode3 = 1;
    w.ct0 = 50; w.ct2 = 0; ground_value = 10.0f;
    recvx_en24(&w);
    ok(se_n == 1 && se_last == 74496, "MV02 ct2 < 0 spielt SE 74496");
    ok(w.ct2 >= 40 && w.ct2 <= 80, "MV02 ct2 neu in 40..80");
}

/* ============================================ 5. MV03 Drehen ============== */
static void test_mv03(void)
{
    RECVX_ENEMY_WORK w;
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 3; w.mode3 = 0;

    recvx_en24(&w);
    ok((w.flg & 0x80000u) == 0x80000u, "MV03 flg 0x80000");
    ok(w.way == 273 || w.way == -273, "MV03 way +/-273");
    ok(w.mode3 == 1u, "MV03 mode3 = 1");
    /* Fallthrough: ay wurde im selben Pass schon um way gedreht. */
    ok(w.ay == w.way, "MV03 case 1 im selben Pass: ay += way");
    ok(w.ct0 >= 14 && w.ct0 <= 59, "MV03 ct0 in 14..59");
}

/* ============================================ 6. MV04 Sprung ============== */
static void test_mv04(void)
{
    RECVX_ENEMY_WORK w;
    float vy0;

    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 4; w.mode3 = 0;
    w.ay = 0;                       /* njSin(0)=0, njCos(0)=1 */
    w.flg = 0x80000u | 0x20u;
    player.flg = 4u;                /* Spieler ausgenommen -> kein Angriff  */

    recvx_en24(&w);
    ok((w.flg & 0x100000u) == 0x100000u, "MV04 setzt flg 0x100000");
    ok((w.flg & 0x80000u) == 0u, "MV04 loescht flg 0x80000");
    ok((w.flg & 0x20u) == 0u, "MV04 loescht flg 0x20");
    okf(exp0_f(&w, 0x14), 0.0f, 1e-5f, "MV04 vx = 0.6 * -sin(0)");
    okf(exp0_f(&w, 0x18), 2.5f - 0.33f, 1e-5f, "MV04 vy nach einem Pass");
    okf(exp0_f(&w, 0x1C), -0.6f, 1e-5f, "MV04 vz = 0.6 * -cos(0)");
    /* Fallthrough: case 1 verschob die Lage im selben Pass. */
    okf(w.py, 2.5f, 1e-5f, "MV04 case 1 im selben Pass: py += vy");
    okf(w.pz, -0.6f, 1e-5f, "MV04 case 1 im selben Pass: pz += vz");

    vy0 = exp0_f(&w, 0x18);
    recvx_en24(&w);
    okf(exp0_f(&w, 0x18), vy0 - 0.33f, 1e-5f, "MV04 Schwerkraft 0.33 je Pass");

    /* Landung: flg 0x100000 geloescht -> zurueck nach MV02. */
    w.flg &= ~0x100000u;
    recvx_en24(&w);
    ok(w.mode2 == 2u && w.mode1 == 0u && w.mode3 == 0u, "MV04 Landung -> MV02");
    ok((w.flg & 0x20u) == 0x20u, "MV04 Landung setzt flg 0x20");

    /* Angriffszweig: mit erlaubtem Spieler ist das Praedikat ungebunden und
       muss als Luecke gemeldet werden, nicht stillschweigend 0 liefern. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 4; w.mode3 = 1;
    w.flg = 0x100000u;
    player.flg = 0u; player.stflg = 0u; player.hp = 5;
    recvx_en24(&w);
    ok(gap_seen("bhEne_AttackHitCheck"), "MV04 meldet bhEne_AttackHitCheck als Luecke");
    ok(player.hp == 5, "MV04 ohne gebundenes Praedikat kein Spielerschaden");
}

/* ============================================ 7. MV05 freier Fall ========= */
static void test_mv05(void)
{
    RECVX_ENEMY_WORK w;
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 5; w.mode3 = 0;
    w.xn = 1.0f; w.yn = 2.0f; w.zn = 3.0f;

    recvx_en24(&w);
    ok((w.flg & 0x100000u) == 0x100000u, "MV05 flg 0x100000");
    okf(w.px, 1.0f, 1e-6f, "MV05 px += xn im selben Pass");
    okf(w.py, 2.0f, 1e-6f, "MV05 py += yn");
    okf(w.pz, 3.0f, 1e-6f, "MV05 pz += zn");
    okf(w.yn, 2.0f - 0.33f, 1e-6f, "MV05 yn -= 0.33");

    w.flg &= ~0x100000u;
    recvx_en24(&w);
    ok(w.mode2 == 2u && (w.flg & 0x20u) == 0x20u, "MV05 Ende -> MV02 mit flg 0x20");
}

/* ============================================ 8. Trefferverarbeitung ====== */
static void test_damage(void)
{
    RECVX_ENEMY_WORK w;

    /* wpnr_no 2: DeadRate 1.0 -> njRandom() < 1.0 ist fast immer wahr,
       also sofortiger Tod. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    w.hp = 2; w.flg = 4u; w.wpnr_no = 2;
    recvx_en24(&w);
    ok((w.flg & 4u) == 0u, "Treffer loescht flg 4");
    ok(w.hp == 1, "Treffer zieht 1 HP ab");
    ok(w.mode0 == 4u, "Sofortiger Tod -> mode0 4");
    ok((w.mdflg & 1u) == 1u, "Tod setzt mdflg 1");
    ok(se_last == 74497, "Tod spielt SE 74497");
    ok(blood4_n == 1 && blood4_col == 1 && blood4_t1 == 0 && blood4_t2 == 0,
       "Tod ruft bhEne_SetBloodEffect4(col 1, 0, 0)");
    okf(blood4_pos.y, 1.0f, 1e-6f, "Blutquelle 1.0 ueber dem Work");
    ok(gap_seen("bhSetEffectTb"), "Tod meldet bhSetEffectTb(id 303) als Luecke");

    /* wpnr_no 0: DeadRate 0.0 -> njRandom() < 0.0 niemals -> nur HP ab. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    w.hp = 2; w.flg = 4u; w.wpnr_no = 0;
    recvx_en24(&w);
    ok(w.hp == 1 && w.mode0 == 1u, "DeadRate 0.0: kein sofortiger Tod");

    /* hp == 0 -> Tod ohne Wuerfeln. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    w.hp = 0; w.flg = 4u; w.wpnr_no = 0;
    recvx_en24(&w);
    ok(w.mode0 == 4u, "hp 0 -> Tod");

    /* wpnr_no 20 (weapon.c:1453) liegt hinter DeadRate[19]. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    w.hp = 2; w.flg = 4u; w.wpnr_no = 20;
    recvx_en24(&w);
    ok(gap_seen("DeadRate[wpnr_no]"), "wpnr_no 20 wird als Luecke gemeldet");
    ok(w.hp == 1, "wpnr_no 20 zieht dennoch 1 HP ab");
}

/* ============================================ 9. DeadCheck (Zertreten) ==== */
static void set_joint(unsigned j, float x, float y, float z, float m8, float m10)
{
    memset(joint_mtx[j], 0, sizeof(float) * 16);
    joint_mtx[j][12] = x; joint_mtx[j][13] = y; joint_mtx[j][14] = z;
    joint_mtx[j][8] = m8; joint_mtx[j][10] = m10;
    joint_mtx[j][15] = 1.0f;
}

static void test_dead_check(void)
{
    RECVX_ENEMY_WORK w;

    reset(&w);
    w.mode0 = 1; w.mode1 = 0; w.mode2 = 0; w.mode3 = 0;
    w.flg = 0x20u;                /* en24.c:212 verlangt flg 0x20 */
    w.hp = 2;
    player.mode0 = 1;             /* en24.c:212 verlangt plp->mode0 == 1 */
    /* exp0 anlegen, ohne den Treffer schon auszuloesen: Gelenke weit weg. */
    set_joint(17, 100.0f, 0.0f, 100.0f, 0.0f, 1.0f);
    set_joint(21, 100.0f, 0.0f, 100.0f, 0.0f, 1.0f);
    w.exp0 = t_calloc(0x48u);
    recvx_en24(&w);
    ok(w.mode0 == 1u, "DeadCheck: weit entferntes Gelenk toetet nicht");

    /* Jetzt Gelenk 17 nahe am Work und um mehr als 0.05 bewegt. */
    set_joint(17, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    recvx_en24(&w);          /* erster Pass merkt die Lage (mov ist gross) */
    ok(w.mode0 == 4u, "DeadCheck: bewegtes Gelenk < 2.0 toetet (mode0 4)");

    /* Gegenprobe: dieselbe Lage ohne Bewegung und ohne Drehung -> kein Tod. */
    reset(&w);
    w.mode0 = 1; w.flg = 0x20u; w.hp = 2;
    player.mode0 = 1;
    set_joint(17, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    set_joint(21, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    w.exp0 = t_calloc(0x48u);
    recvx_en24(&w);          /* Pass 1: merkt die Lage, mov = 0 (exp0 genullt) */
    ok(w.mode0 == 1u, "DeadCheck Pass 1 ohne Vorlage: mov 0 toetet nicht");
    recvx_en24(&w);          /* Pass 2: unveraendert -> weiterhin kein Tod */
    ok(w.mode0 == 1u, "DeadCheck Pass 2 ohne Bewegung: kein Tod");

    /* Nur Drehung ueber 182 Einheiten toetet ebenfalls. */
    set_joint(17, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);   /* 90 Grad gedreht */
    recvx_en24(&w);
    ok(w.mode0 == 4u, "DeadCheck: Drehung > 182 toetet");

    /* Ohne Gelenkdienst muss die Luecke gemeldet werden. */
    reset(&w);
    w.mode0 = 1; w.flg = 0x20u; w.hp = 2;
    player.mode0 = 1;
    joint_available = 0;
    w.exp0 = t_calloc(0x48u);
    recvx_en24(&w);
    ok(gap_seen("owP[17/21].mtx"), "DeadCheck meldet fehlende Spielergelenke");
    ok(w.mode0 == 1u, "DeadCheck ohne Gelenke toetet nicht");
}

/* ============================================ 10. CollisionLine =========== */
static void test_collision_line(void)
{
    RECVX_ENEMY_WORK w;

    reset(&w);
    w.mode0 = 2;                 /* Nage ist leer, nur der Rahmen laeuft */
    w.flg = 0x100000u;
    w.pxb = 0.0f; w.pyb = 10.0f; w.pzb = 0.0f;
    w.px = 0.0f;  w.py = -5.0f;  w.pz = 0.0f;
    line_hit = 1;
    line_to.x = 0.0f; line_to.y = 0.0f; line_to.z = 0.0f;
    line_normal.x = 0.0f; line_normal.y = 1.0f; line_normal.z = 0.0f;
    hit_normal_v = line_normal;

    recvx_en24(&w);
    okf(w.py, 0.0f, 1e-6f, "CollisionLine klemmt py auf den Schnittpunkt");
    ok((w.flg & 0x100000u) == 0u, "Bodennormale y > 0.99 beendet den Flug");

    /* Schraege Normale beendet den Flug NICHT. */
    reset(&w);
    w.mode0 = 2;
    w.flg = 0x100000u;
    w.pxb = 0.0f; w.pyb = 10.0f; w.pzb = 0.0f;
    w.px = 0.0f; w.py = -5.0f; w.pz = 0.0f;
    line_hit = 1;
    line_to.x = 0.0f; line_to.y = 0.0f; line_to.z = 0.0f;
    hit_normal_v.x = 1.0f; hit_normal_v.y = 0.5f; hit_normal_v.z = 0.0f;
    line_normal = hit_normal_v;
    recvx_en24(&w);
    ok((w.flg & 0x100000u) == 0x100000u, "Wandnormale beendet den Flug nicht");

    /* Ohne flg 0x100000 wird die Linie gar nicht geprueft. */
    reset(&w);
    w.mode0 = 2;
    w.px = 7.0f;
    line_hit = 1;
    line_to.x = 0.0f; line_to.y = 0.0f; line_to.z = 0.0f;
    recvx_en24(&w);
    okf(w.px, 7.0f, 1e-6f, "ohne flg 0x100000 keine Linienpruefung");
}

/* ============================================ 11. CollisionWalls ========== */
static void test_collision_walls(void)
{
    RECVX_ENEMY_WORK w;
    reset(&w);
    w.mode0 = 2;
    w.flg = 0x10u;
    w.ar = 1.2f;
    w.py = 0.0f;
    recvx_en24(&w);
    ok(collision_works_calls == 1, "flg 0x10 ruft bhEne03_Collision");
    okf(w.py, 0.0f, 1e-6f, "py nach der Anhebung wieder zurueckgesetzt");

    reset(&w);
    w.mode0 = 2;
    recvx_en24(&w);
    ok(collision_works_calls == 0, "ohne flg 0x10 kein bhEne03_Collision");
}

/* ============================================ 12. Waffenkapsel ============ */
static void test_watr(void)
{
    RECVX_ENEMY_WORK w;
    reset(&w);
    w.mode0 = 2;
    w.px = 1.0f; w.py = 2.0f; w.pz = 3.0f;
    recvx_en24(&w);
    okf(w.watr.c1.x, 1.0f, 1e-6f, "watr.c1.x");
    okf(w.watr.c1.z, 3.0f, 1e-6f, "watr.c1.z");
    okf(w.watr.c2.z, 4.0f, 1e-6f, "watr.c2.z = pz + 1.0");
    okf(w.watr.r, 2.0f, 1e-6f, "watr.r 2.0");
}

/* ============================================ 13. Verknuepfter Modellsatz = */
static void test_linked_model(void)
{
    RECVX_ENEMY_WORK w, parent;
    reset(&w);
    memset(&parent, 0, sizeof(parent));
    w.mode0 = 2;
    w.flg = 0x80000u;
    w.lkwkp = &parent;
    w.frm_no = 0;

    recvx_en24(&w);
    ok(w.mdl_no == 2u, "frm_no 0 -> mdl_no 2");
    ok(w.frm_no == 1, "frm_no zaehlt hoch");
    ok(gap_seen("lkwkp->mdl[frm_no+2]"), "fremder Modellsatz wird gemeldet");

    w.frm_no = 4;
    recvx_en24(&w);
    ok(w.mdl_no == 2u, "frm_no 4 wird auf 0 zurueckgesetzt (3 < frm_no)");
    ok(w.frm_no == 1, "nach dem Ruecksetzen wieder 1");

    /* Ohne flg 0x80000 bleibt der Modellsatz unberuehrt. */
    reset(&w);
    w.mode0 = 2;
    w.lkwkp = &parent;
    w.frm_no = 0;
    recvx_en24(&w);
    ok(w.frm_no == 0, "ohne flg 0x80000 kein Modellwechsel");
}

/* ============================================ 14. Brain / Sichtkegel ====== */
static void test_brain(void)
{
    RECVX_ENEMY_WORK w;

    /* Spieler direkt vor dem Work (ay 0 blickt nach -z), 3.0 entfernt. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 0; w.mode3 = 0;
    w.ay = 0;
    w.px = 0.0f; w.py = 0.0f; w.pz = 0.0f;
    player.pos.x = 0.0f; player.pos.y = 0.0f; player.pos.z = -3.0f;
    w.exp0 = t_calloc(0x48u);
    recvx_en24(&w);
    ok(exp0_uc(&w, 0x20) == 1, "Sichtkegel: Spieler vorn ist sichtbar");
    okf(exp0_f(&w, 0), 3.0f, 1e-5f, "Abstand 3.0 in EXP0_F(0)");
    /* brain_wait war 0 (frischer Block), Abstand < 5 -> Sprung. */
    ok(w.mode2 == 4u && w.mode1 == 0u, "Brain loest den Sprung aus (mode2 4)");

    /* Spieler hinter dem Work ist nicht sichtbar. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 0; w.mode3 = 0;
    w.ay = 0;
    player.pos.z = 3.0f;
    w.exp0 = t_calloc(0x48u);
    recvx_en24(&w);
    ok(exp0_uc(&w, 0x20) == 0, "Sichtkegel: Spieler hinten ist unsichtbar");
    ok(w.mode2 == 0u, "ohne Sicht kein Sprung");

    /* Sichtbar, aber zu weit (>= 5.0). */
    reset(&w);
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 0; w.mode3 = 0;
    w.ay = 0;
    player.pos.z = -8.0f;
    w.exp0 = t_calloc(0x48u);
    recvx_en24(&w);
    ok(exp0_uc(&w, 0x20) == 1, "weit entfernter Spieler bleibt sichtbar");
    ok(w.mode2 == 0u, "Abstand >= 5.0 loest keinen Sprung aus");

    /* brain_wait blockiert und zaehlt herunter. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 0; w.mode3 = 0;
    w.ay = 0;
    player.pos.z = -3.0f;
    w.exp0 = t_calloc(0x48u);
    memset((char*)w.exp0 + 0x24, 0, 4);
    { int32_t v = 5; memcpy((char*)w.exp0 + 0x24, &v, 4); }
    recvx_en24(&w);
    ok(exp0_i(&w, 0x24) == 4, "brain_wait zaehlt herunter");
    ok(w.mode2 == 0u, "brain_wait blockiert den Sprung");

    /* plp->flg 4 bzw. stflg 0x80000000 unterbrechen vor dem Zaehler. */
    reset(&w);
    w.mode0 = 1; w.mode1 = 1; w.mode2 = 0; w.mode3 = 0;
    player.pos.z = -3.0f;
    player.flg = 4u;
    w.exp0 = t_calloc(0x48u);
    { int32_t v = 5; memcpy((char*)w.exp0 + 0x24, &v, 4); }
    recvx_en24(&w);
    ok(exp0_i(&w, 0x24) == 5, "plp->flg 4 kehrt vor dem Zaehler zurueck");
    ok(w.mode2 == 0u, "plp->flg 4 verhindert den Sprung");

    /* type > 0 hat keinen BrainType-Eintrag (Tabelle hat genau einen). */
    reset(&w);
    w.mode0 = 1; w.mode1 = 1; w.type = 1;
    w.exp0 = t_calloc(0x48u);
    recvx_en24(&w);
    ok(gap_seen("bhEne24_BrainType[type]"), "type > 0 wird gemeldet");
}

/* ============================================ 15. Ungebundene Dienste ===== */
static void test_unbound(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_SERVICES bare;

    memset(&bare, 0, sizeof(bare));
    bare.gap = t_gap;
    reset(&w);
    w.sv = &bare;
    w.mode0 = 0;
    recvx_en24(&w);
    ok(gap_seen("bhEne_CallocWork(72)"), "fehlender Allokator wird gemeldet");
    ok(w.exp0 == 0, "ohne Allokator kein Ersatzblock");

    reset(&w);
    w.sv = &bare;
    w.mode0 = 2;
    recvx_en24(&w);
    ok(gap_seen("bhCalcModel"), "fehlendes bhCalcModel wird gemeldet");

    reset(&w);
    w.mode0 = 1; w.mode0 = 1; w.mode2 = 7;
    recvx_en24(&w);
    ok(gap_seen("bhEne24_MoveMode2"), "mode2 ausserhalb 0..5 wird gemeldet");
}

int main(void)
{
    _Make_SinTable();
    services_init();

    test_init();
    test_dormant();
    test_mv01_fallthrough();
    test_mv02();
    test_mv03();
    test_mv04();
    test_mv05();
    test_damage();
    test_dead_check();
    test_collision_line();
    test_collision_walls();
    test_watr();
    test_linked_model();
    test_brain();
    test_unbound();

    printf("EN24: %d Pruefungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
