/* Komponentenpruefung der EN01-Zustandsmaschine (native/en01_core.c) mit den
   echten EN01-Datenfeldern aus native/en01_tables.c.
   ===========================================================================
   Die Pruefungen zielen auf originale Zustandsuebergaenge, Tabellenindizes
   und Reihenfolgen, nicht auf eine Nacherzaehlung des neuen Codes. Alle
   Dienste sind ausdrueckliche TESTAUFSTELLUNGEN: sie protokollieren, was der
   Mechanismus anfordert, und liefern feste Eingaben. Wo das Original einen
   Dienst braucht, der hier NICHT gebunden ist, muss die Pruefung die
   Lueckenmeldung sehen - eine abgeschwaechte Erwartung auf ein neutrales
   Ergebnis ist ausdruecklich nicht zulaessig.

   Dieses Programm ist noch nicht in CMakeLists.txt registriert, weil
   native/en01_core.c dort ebenfalls noch fehlt (Anschlussschritt beim
   Integrator, siehe work/native_port_T2_en01.result.md). Bauen und laufen:

     /usr/bin/cc -DRECVX_NATIVE=1 -I build/macos-arm64/generated \
       -I build-tools/ninja -I native -I include/ps2/veronica/prog \
       -O1 -DNDEBUG -std=gnu11 -arch arm64 -mmacosx-version-min=11.0 \
       -Wall -Wextra -o build/en01_core_test tests/native/en01_core_test.c \
       native/en01_core.c native/en01_tables.c build-tools/ninja/ninja_math.c \
       build-tools/ninja/ninja_matrix.c native/core_random.c
     ./build/en01_core_test
   =========================================================================== */
#include "en01_core.h"
#include "core_random.h"
#include "ninja_math.h"
#include <stdio.h>
#include <string.h>

static int g_failed;
static int g_checks;

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

/* ---------------------------------------------------------------------------
   Testaufstellung: protokollierende Dienste
   --------------------------------------------------------------------------- */
#define LOG_MAX 64
static char     g_gap[LOG_MAX][96];
static int      g_gap_n;
static int      g_se[LOG_MAX];
static int      g_se_n;
static int      g_vib[LOG_MAX];
static int      g_vib_n;
static const int8_t* g_fix_tree;
static int      g_fix_calls;
static const RECVX_EN01_XZ* g_link_offset;
static int      g_link_calls;
static int32_t  g_rand_seq[16];
static int      g_rand_n, g_rand_i;

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

static int32_t t_rand(void)
{
    if (g_rand_n > 0) {
        int32_t v = g_rand_seq[g_rand_i % g_rand_n];
        ++g_rand_i;
        return v;
    }
    return recvx_core_random_next();
}

static void t_enemy_se(uint32_t enemy, const RECVX_ENEMY_VEC* pos, int se)
{
    (void)enemy; (void)pos;
    if (g_se_n < LOG_MAX) g_se[g_se_n++] = se;
}

static int t_enemy_se_playing(uint32_t enemy, int se) { (void)enemy; (void)se; return 0; }
static uint32_t t_enemy_now(void) { return 0u; }

static void t_vibration(int motor, int power)
{
    (void)motor;
    if (g_vib_n < LOG_MAX) g_vib[g_vib_n++] = power;
}

/* bhCalcFixOffset: liefert eine feste Differenz, damit die Pruefung sieht,
   WELCHE Fusskette uebergeben wurde und dass px/pz davon abgezogen werden. */
static void t_calc_fix_offset(RECVX_ENEMY_WORK* w, const int8_t* tree,
                              const RECVX_ENEMY_VEC* local, RECVX_ENEMY_VEC* out)
{
    (void)w; (void)local;
    g_fix_tree = tree;
    ++g_fix_calls;
    out->x = 3.0f; out->y = 0.0f; out->z = -4.0f;
}

static RECVX_ENEMY_PLAYER g_player;
static int                g_player_bound;

static int t_player_get(RECVX_ENEMY_PLAYER* out)
{
    if (!g_player_bound) return 0;
    *out = g_player;
    return 1;
}
static void t_player_set(const RECVX_ENEMY_PLAYER* in) { g_player = *in; }

static int t_player_link_pose(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* wp,
                              int32_t ay, const RECVX_EN01_XZ* local_offset,
                              RECVX_ENEMY_VEC* out_delta)
{
    (void)w; (void)wp; (void)ay;
    g_link_offset = local_offset;
    ++g_link_calls;
    out_delta->x = 0.0f; out_delta->y = 0.0f; out_delta->z = 0.0f;
    return 1;
}

static RECVX_ENEMY_SERVICES g_sv;
static RECVX_EN01_SERVICES  g_en;
static RECVX_EN01_CONTEXT   g_ctx;
static RECVX_EN01_DATA      g_data;

static void reset_logs(void)
{
    g_gap_n = g_se_n = g_vib_n = 0;
    g_fix_tree = NULL; g_fix_calls = 0;
    g_link_offset = NULL; g_link_calls = 0;
    g_rand_n = g_rand_i = 0;
}

/* plp->mlwP->owP[joint].mtx — bhEne01_Move braucht owP[0], um den
   Spielerabstand zu bilden (en01.c:2840-2846). Einheitsmatrix mit der
   Spielerposition in der Translationsspalte. */
static int t_player_joint_matrix(uint32_t joint, float* out16)
{
    int i;
    (void)joint;
    if (!g_player_bound || !out16) return 0;
    for (i = 0; i < 16; ++i) out16[i] = 0.0f;
    out16[0] = out16[5] = out16[10] = out16[15] = 1.0f;
    out16[12] = g_player.pos.x;
    out16[13] = g_player.pos.y;
    out16[14] = g_player.pos.z;
    return 1;
}

static void bind_services(unsigned game_mode, int with_data)
{
    memset(&g_sv, 0, sizeof(g_sv));
    memset(&g_en, 0, sizeof(g_en));
    g_sv.gap               = t_gap;
    g_sv.random_next       = t_rand;
    g_sv.enemy_se          = t_enemy_se;
    g_sv.enemy_se_playing  = t_enemy_se_playing;
    g_sv.enemy_now         = t_enemy_now;
    g_sv.vibration         = t_vibration;
    g_sv.player_get        = t_player_get;
    g_sv.player_set        = t_player_set;
    g_sv.player_joint_matrix = t_player_joint_matrix;
    g_en.calc_fix_offset   = t_calc_fix_offset;
    g_en.player_link_pose  = t_player_link_pose;

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.services  = &g_sv;
    g_ctx.en01      = &g_en;
    g_ctx.game_mode = game_mode;
    recvx_en01_set_context(&g_ctx);

    if (with_data) {
        g_data = recvx_en01_default_data();
        recvx_en01_set_data(&g_data);
    } else {
        recvx_en01_set_data(NULL);
    }
}

static void work_init(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* p)
{
    memset(w, 0, sizeof(*w));
    memset(p, 0, sizeof(*p));
    w->sv   = &g_sv;
    w->exp0 = p;
}

/* ===========================================================================
   1 — ene01_typ_tbl (en01.c:2541) und die i < 30 Suchgrenze (en01.c:2696)
   =========================================================================== */
static void test_personal_type(void)
{
    /* Erwartungswerte unabhaengig aus den ersten 30 Zeilen des Originals
       abgeleitet: mdlver -> Subtyp. Nicht gelistete mdlver ergeben 0. */
    static const int pair[30][2] = {
        {0,0},{1,1},{31,2},{18,0},{2,3},{3,4},{4,5},{32,6},{33,7},{34,8},
        {21,9},{19,3},{5,10},{6,11},{35,12},{36,13},{22,14},{20,10},{7,15},
        {8,16},{37,17},{38,18},{24,15},{9,19},{10,20},{25,21},{39,22},
        {40,23},{41,24},{42,25}
    };
    int i;
    for (i = 0; i < 30; ++i) {
        check_eq(recvx_en01_personal_type((uint8_t)pair[i][0]), pair[i][1],
                 "ene01_typ_tbl: mdlver -> Subtyp");
    }
    /* Zeile 30 (mdlver 43 -> 26) liegt hinter der Suchgrenze i < 30 und ist
       im Original unerreichbar. Keine Korrektur, die Eigenheit bleibt. */
    check_eq(recvx_en01_personal_type(43), 0,
             "mdlver 43 bleibt wegen i < 30 unerreichbar");
    check_eq(recvx_en01_personal_type(11), 0, "nicht gelistete mdlver -> 0");
    check_eq(recvx_en01_personal_type(255), 0, "mdlver 255 -> 0");
}

/* ===========================================================================
   2 — bhEne01_Init: HP aus en01_hp_tbl + add_hp (en01.c:2703-2710)
   =========================================================================== */
static void test_init_hp(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_EN01_EXP0  p;

    /* gm_mode 0 -> Zeile 0; rand() % 16 == 5 -> en01_hp_tbl[0][5] == 90.
       mdlver 3 -> Subtyp 4, add_hp 10 -> 90 + 9 = 99. */
    bind_services(0u, 1);
    reset_logs();
    g_rand_seq[0] = 5; g_rand_n = 1;
    g_player_bound = 1; memset(&g_player, 0, sizeof(g_player));
    work_init(&w, &p);
    w.mdlver = 3; w.type = 0;
    recvx_en01_init_work(&w, &p);
    check_eq(p.personal_type, 4, "mdlver 3 -> Subtyp 4");
    check_eq(w.hp, 99, "gm_mode 0, rand%16=5: 90 + 10% = 99");
    check_eq(w.mode0, 1, "InitType00 setzt mode0 = 1");
    check_eq(w.mtn_no, 2, "InitType00 ohne flg 0x80: mtn_no = 2");

    /* gm_mode 2 -> Zeile 1; rand() % 16 == 5 -> en01_hp_tbl[1][5] == 40.
       mdlver 5 -> Subtyp 10, und en01_PersonalType[10] hat add_hp 0
       (en01.c:1072), also bleibt es bei 40. Nur Subtyp 9 traegt dort 20. */
    bind_services(2u, 1);
    reset_logs();
    g_rand_seq[0] = 5; g_rand_n = 1;
    work_init(&w, &p);
    w.mdlver = 5; w.type = 0;
    recvx_en01_init_work(&w, &p);
    check_eq(p.personal_type, 10, "mdlver 5 -> Subtyp 10");
    check_eq(w.hp, 40, "gm_mode 2, rand%16=5: 40, Subtyp 10 ohne add_hp");

    /* Negativer add_hp (Subtyp 19, -20 Prozent): mdlver 9, Zeile 0,
       rand%16 = 0 -> 60 - 12 = 48. Die Originalformel rundet zur Null. */
    bind_services(0u, 1);
    reset_logs();
    g_rand_seq[0] = 0; g_rand_n = 1;
    work_init(&w, &p);
    w.mdlver = 9; w.type = 0;
    recvx_en01_init_work(&w, &p);
    check_eq(p.personal_type, 19, "mdlver 9 -> Subtyp 19");
    check_eq(w.hp, 48, "add_hp -20 Prozent: 60 -> 48");

    /* Ohne gebundene Tabelle MUSS die Luecke gemeldet werden. */
    bind_services(0u, 0);
    reset_logs();
    work_init(&w, &p);
    w.mdlver = 3;
    recvx_en01_init_work(&w, &p);
    check(gap_seen("en01_hp_tbl"), "fehlende HP-Tabelle wird namentlich gemeldet");
}

/* ===========================================================================
   3 — bhEne01_InitType-Familien (en01.c:2217 / :4430-4548)
   =========================================================================== */
static void test_init_types(void)
{
    RECVX_ENEMY_WORK w, parent;
    RECVX_EN01_EXP0  p;

    bind_services(0u, 1);
    g_player_bound = 1;

    /* Typ 2 -> InitType02: hp wird halbiert, mtn_no 3, flags unteres Nibble
       auf genau 1 (en01.c:4446-4462). */
    reset_logs();
    g_rand_seq[0] = 0; g_rand_n = 1;          /* en01_hp_tbl[0][0] == 60 */
    work_init(&w, &p);
    w.mdlver = 0; w.type = 2;
    recvx_en01_init_work(&w, &p);
    check_eq(w.hp, 30, "InitType02 halbiert die HP (60 -> 30)");
    check_eq(w.mtn_no, 3, "InitType02 ohne flg 0x80: mtn_no = 3");
    check_eq(p.flags & 0xFu, 1, "InitType02 setzt das untere Nibble auf 1");

    /* Typ 2 MIT flg 0x80 (verknuepfter Oberkoerper): hp kommt vom Elternwork,
       mtn_no 203, unteres Nibble geloescht (en01.c:4453-4457). */
    reset_logs();
    g_rand_seq[0] = 0; g_rand_n = 1;
    memset(&parent, 0, sizeof(parent));
    parent.hp = 77; parent.type = 2;
    work_init(&w, &p);
    w.mdlver = 0; w.type = 2; w.flg = 0x80u; w.lkwkp = &parent;
    recvx_en01_init_work(&w, &p);
    check_eq(w.hp, 77, "InitType02 mit flg 0x80 uebernimmt lkwkp->hp");
    check_eq(w.mtn_no, 203, "InitType02 mit flg 0x80: mtn_no = 203");
    check_eq(p.flags & 0xFu, 0, "InitType02 mit flg 0x80 loescht das Nibble");
    check(p.flags & 0x80000000u, "flg 0x80 markiert den Oberkoerper");

    /* Typ 6 -> InitType06: mode2 = 4. Typ 15 -> InitType15: mode2 = 4,
       mtn_no 13, mtn_add 0, flags 0x40000 und flags2 0x10
       (en01.c:4467-4489 / :4529-4548). Der +10-Zweig in InitType06 gilt nur
       fuer type == 15, das aber InitType15 bekommt - er ist unerreichbar. */
    reset_logs();
    g_rand_seq[0] = 0; g_rand_n = 1;
    work_init(&w, &p);
    w.mdlver = 0; w.type = 6;
    recvx_en01_init_work(&w, &p);
    check_eq(w.mode2, 4, "InitType06 setzt mode2 = 4");
    check_eq(w.mtn_no, 3, "InitType06 ohne flg 0x80: mtn_no = 3");

    reset_logs();
    g_rand_seq[0] = 0; g_rand_n = 1;
    work_init(&w, &p);
    w.mdlver = 0; w.type = 15;
    recvx_en01_init_work(&w, &p);
    check_eq(w.mode2, 4, "InitType15 setzt mode2 = 4");
    check_eq(w.mtn_no, 13, "InitType15 ohne flg 0x80: mtn_no = 13");
    check_eq(w.mtn_add, 0, "InitType15 setzt mtn_add = 0");
    check(p.flags & 0x40000u, "InitType15 setzt flags 0x40000");
    check(p.flags2 & 0x10u, "InitType15 setzt flags2 0x10");

    /* Typ 10 -> InitType10 loescht flg 0x20 und flags 0x10000000; Typ 11 ->
       InitType11 setzt nur flags2 0x10 (en01.c:4492-4526). */
    reset_logs();
    g_rand_seq[0] = 0; g_rand_n = 1;
    work_init(&w, &p);
    w.mdlver = 0; w.type = 10;
    recvx_en01_init_work(&w, &p);
    check_eq(w.flg & 0x20u, 0, "InitType10 loescht flg 0x20");
    check_eq(p.flags & 0x10000000u, 0, "InitType10 loescht flags 0x10000000");
    check_eq(w.mtn_no, 2, "InitType10 ohne flg 0x80: mtn_no = 2");

    reset_logs();
    g_rand_seq[0] = 0; g_rand_n = 1;
    work_init(&w, &p);
    w.mdlver = 0; w.type = 11;
    recvx_en01_init_work(&w, &p);
    check(p.flags2 & 0x10u, "InitType11 setzt flags2 0x10");
    check_eq(w.mtn_no, 2, "InitType11 ohne flg 0x80: mtn_no = 2");

    /* Die Clip-Gelenke und lok_jno aus en01.c:2784-2794 ueberschreiben die
       Werte von bhSetEnemy. */
    check_eq(w.clp_jno[0], 0x0B, "clp_jno[0] = 0x0B (en01.c:2784)");
    check_eq(w.clp_jno[7], 0x01, "clp_jno[7] = 0x01 (en01.c:2791)");
    check_eq(w.lok_jno, 0x0A, "lok_jno = 10 (en01.c:2794)");
    check(w.comb_flg & 2u, "comb_flg Bit 1 (en01.c:2795)");
}

/* ===========================================================================
   4 — bhEne01_SePlay (en01.c:11367-11409)
   =========================================================================== */
static void test_se_play(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_EN01_EXP0  p;

    bind_services(0u, 1);

    /* Nummer < 20: kein add_seno. */
    reset_logs();
    work_init(&w, &p);
    recvx_en01_se_play(&w, 19);
    check_eq(g_se_n, 1, "SE 19 wird angefordert");
    check_eq(g_se[0], 19, "SE 19 bleibt unveraendert (kein add_seno)");

    /* Nummer 22 mit Subtyp 1 (add_seno 32) -> 54. Keine Stimmenpruefung,
       weil nur 20/21 durch ChechPlayEnemySe gehen. */
    reset_logs();
    work_init(&w, &p);
    p.personal_type = 1;
    recvx_en01_se_play(&w, 22);
    check_eq(g_se_n, 1, "SE 22 wird angefordert");
    check_eq(g_se[0], 54, "SE 22 + add_seno[1] = 54");

    /* Nummer 20 ist eine Stimme: bei rand() gerade bricht das Original ab
       (en01.c:11389), bei ungerade spielt es. */
    reset_logs();
    g_rand_seq[0] = 2; g_rand_n = 1;
    work_init(&w, &p);
    p.personal_type = 2;                       /* add_seno 16 -> 36 */
    recvx_en01_se_play(&w, 20);
    check_eq(g_se_n, 0, "Stimme 20 bei geradem rand() unterdrueckt");

    reset_logs();
    g_rand_seq[0] = 3; g_rand_n = 1;
    work_init(&w, &p);
    p.personal_type = 2;
    recvx_en01_se_play(&w, 20);
    check_eq(g_se_n, 1, "Stimme 20 bei ungeradem rand() gespielt");
    check_eq(g_se[0], 36, "Stimme 20 + add_seno[2] = 36");

    /* Die 20..63-Pruefung nimmt das NIEDRIGSTE Byte (en01.c:11383), nicht
       die ganze Zahl: 70415 & 0xFF == 15, also kein add_seno. Diese
       Eigenheit entscheidet ueber jeden Schrittklang aus en01_mtn_tbl2. */
    reset_logs();
    work_init(&w, &p);
    p.personal_type = 1;
    recvx_en01_se_play(&w, 70415);
    check_eq(g_se[0], 70415, "70415: unteres Byte 15 -> kein add_seno");

    /* stflg 0x100000 (Wasser): 70414/70415 werden um 2 erhoeht, 8963 fallen
       ganz weg (en01.c:11397-11406). */
    reset_logs();
    work_init(&w, &p);
    w.stflg = 0x100000u;
    recvx_en01_se_play(&w, 70415);
    check_eq(g_se[0], 70417, "stflg 0x100000: 70415 -> 70417");

    reset_logs();
    work_init(&w, &p);
    w.stflg = 0x100000u;
    recvx_en01_se_play(&w, 8963);
    check_eq(g_se_n, 0, "stflg 0x100000: 8963 wird unterdrueckt");

    /* flg 0x10000 unterdrueckt jeden SE (en01.c:11376). */
    reset_logs();
    work_init(&w, &p);
    w.flg = 0x10000u;
    recvx_en01_se_play(&w, 19);
    check_eq(g_se_n, 0, "flg 0x10000 unterdrueckt jeden SE");

    /* Subtyp ausserhalb add_seno[27] -> Luecke, kein Ersatzwert. */
    reset_logs();
    work_init(&w, &p);
    p.personal_type = 27;
    recvx_en01_se_play(&w, 20);
    check_eq(g_se_n, 0, "Subtyp 27 spielt nichts");
    check(gap_seen("add_seno"), "Subtyp ausserhalb add_seno wird gemeldet");
}

/* ===========================================================================
   5 — bhEne01_CheckMtnTbl (en01.c:10484-10735)
   =========================================================================== */
static void test_check_mtn_tbl(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_EN01_EXP0  p;

    bind_services(0u, 1);

    /* mtn_no 49, Frame 0: en01_mtn_tbl2 traegt dort act 16847639.
       16847639 & 0xFF == 23, also 20..63 -> add_seno[0] = 28 -> 16847667. */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 49u; w.mode0 = 1;
    recvx_en01_check_mtn_tbl(&w, 0);
    check_eq(g_se_n, 1, "MTN 49 Frame 0 loest genau einen SE aus");
    check_eq(g_se[0], 16847639 + 28, "MTN 49 Frame 0: act + add_seno[0]");

    /* Derselbe Clip auf einem Frame ohne Eintrag bleibt still. */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 49u; w.mode0 = 1;
    recvx_en01_check_mtn_tbl(&w, 1);
    check_eq(g_se_n, 0, "MTN 49 Frame 1 loest keinen SE aus");

    /* MTN 0 hat zwei Schrittframes (12 und 45) mit act 70415. */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 0u; w.mode0 = 1;
    recvx_en01_check_mtn_tbl(&w, 12);
    check_eq(g_se_n, 1, "MTN 0 Frame 12 loest den ersten Schritt aus");
    check_eq(g_se[0], 70415, "MTN 0 Frame 12: act 70415 unveraendert");
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 0u; w.mode0 = 1;
    recvx_en01_check_mtn_tbl(&w, 45);
    check_eq(g_se_n, 1, "MTN 0 Frame 45 loest den zweiten Schritt aus");

    /* Vibration aus en01_mtn_tbl3 nur fuer mode0 < 5 (en01.c:10709).
       Die Tabelle hat 15 Zeilen (8, 85, 120, 121, 9, 43, 86, 36, 6, 35, 5,
       7, 34, 46, 111); MTN 50 steht NICHT darin. MTN 7 hat genau einen
       Eintrag, Frame 13 mit act 1 (en01.c:797). */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 7u; w.mode0 = 4;
    recvx_en01_check_mtn_tbl(&w, 13);
    check(g_vib_n > 0, "mode0 = 4: Vibration aus en01_mtn_tbl3 laeuft");

    /* mode0 >= 5 sperrt genau diese Tabelle (en01.c:10709). */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 7u; w.mode0 = 5;
    recvx_en01_check_mtn_tbl(&w, 13);
    check_eq(g_vib_n, 0, "mode0 = 5: en01_mtn_tbl3 bleibt still");

    /* MTN 50 loest dort nichts aus, weil die Zeile fehlt. */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 50u; w.mode0 = 4;
    recvx_en01_check_mtn_tbl(&w, 0);
    check_eq(g_vib_n, 0, "MTN 50 steht nicht in en01_mtn_tbl3");
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 50u; w.mode0 = 5;
    recvx_en01_check_mtn_tbl(&w, 0);
    check_eq(g_vib_n, 0, "mode0 = 5: keine Vibration mehr");

    /* Fusskorrektur: mtn_no 2 verlaesst die Schleife sofort und nimmt je
       flg 0x80000 Kette 0 oder 1; px/pz werden um die Differenz GESENKT
       (en01.c:10502-10524). */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 2u; w.mode0 = 1; w.flg = 0x40000u;
    w.px = 10.0f; w.pz = 20.0f;
    recvx_en01_check_mtn_tbl(&w, 5);
    check_eq(g_fix_calls, 1, "MTN 2: genau ein bhCalcFixOffset");
    check(g_fix_tree == recvx_en01_tree[1], "MTN 2 ohne flg 0x80000: Kette 1");
    check(w.px == 7.0f && w.pz == 24.0f, "MTN 2: px/pz um die Differenz gesenkt");

    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 2u; w.mode0 = 1; w.flg = 0x40000u | 0x80000u;
    recvx_en01_check_mtn_tbl(&w, 5);
    check(g_fix_tree == recvx_en01_tree[0], "MTN 2 mit flg 0x80000: Kette 0");

    /* Ohne flg 0x40000 gilt die Fusskorrektur ueberhaupt nicht
       (en01.c:10498); SE und Vibration laufen trotzdem. */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 2u; w.mode0 = 1;
    recvx_en01_check_mtn_tbl(&w, 5);
    check_eq(g_fix_calls, 0, "ohne flg 0x40000 keine Fusskorrektur");

    /* Raum-MTN-Quelle schaltet alle drei Tabellen ab (en01.c:10496). */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 49u; w.mode0 = 1; w.mtn_room = 1;
    recvx_en01_check_mtn_tbl(&w, 0);
    check_eq(g_se_n, 0, "Raum-MTN-Quelle: keine Tabellenauswertung");

    /* Ohne gebundene Tabellen MUSS jede der drei Luecken gemeldet werden. */
    bind_services(0u, 0);
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 49u; w.mode0 = 1; w.flg = 0x40000u;
    recvx_en01_check_mtn_tbl(&w, 0);
    check(gap_seen("en01_mtn_tbl (en01.c:30)"), "fehlende mtn_tbl gemeldet");
    check(gap_seen("en01_mtn_tbl2"), "fehlende mtn_tbl2 gemeldet");
    check(gap_seen("en01_mtn_tbl3"), "fehlende mtn_tbl3 gemeldet");
}

/* ===========================================================================
   6 — bhEne01_PlayerLink: Tabellenauswahl (en01.c:3014-3074)
   =========================================================================== */
static void test_player_link_table(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_EN01_EXP0  p;

    bind_services(0u, 1);
    g_player_bound = 1;

    /* ply_id 2 -> En01_PlyMtn_OffsetTbl[2] == 450. mtn_no 454 == 450 + 4
       -> mtn_no-Index 2 -> ply_pos[2 * 4 + 2] == st_mtn04_xz. frm_no
       3 * 65536 -> Eintrag 3. */
    reset_logs();
    work_init(&w, &p);
    p.flags |= 0x80000u;
    memset(&g_player, 0, sizeof(g_player));
    g_player.flg = 0x1000u; g_player.ply_id = 2u;
    g_player.mtn_no = 454u; g_player.frm_no = 3 * 65536;
    recvx_en01_player_link(&w);
    check_eq(g_link_calls, 1, "PlayerLink ruft den Spielerteil");
    check(g_link_offset == &recvx_en01_st_mtn04_xz[3],
          "ply_id 2 / MTN +4 / Frame 3 -> st_mtn04_xz[3]");

    /* ply_id 3 benutzt im Original DIESELBEN ch_*-Felder wie ply_id 1
       (en01.c:3028-3031). Diese Doppelung bleibt erhalten. */
    reset_logs();
    work_init(&w, &p);
    p.flags |= 0x80000u;
    g_player.ply_id = 3u; g_player.mtn_no = 425u + 1u; g_player.frm_no = 0;
    recvx_en01_player_link(&w);
    check(g_link_offset == &recvx_en01_ch_mtn01_xz[0],
          "ply_id 3 greift auf ch_mtn01_xz zu");
    reset_logs();
    work_init(&w, &p);
    p.flags |= 0x80000u;
    g_player.ply_id = 1u; g_player.mtn_no = 425u + 1u; g_player.frm_no = 0;
    recvx_en01_player_link(&w);
    check(g_link_offset == &recvx_en01_ch_mtn01_xz[0],
          "ply_id 1 greift auf dasselbe Feld zu");

    /* ply->flg 0x1000 geloescht -> else-Zweig ohne Griffversatz
       (en01.c:3086-3091). */
    reset_logs();
    work_init(&w, &p);
    p.flags |= 0x80000u;
    g_player.flg = 0u; g_player.ply_id = 0u;
    recvx_en01_player_link(&w);
    check_eq(g_link_calls, 1, "ohne ply->flg 0x1000 laeuft der else-Zweig");
    check(g_link_offset == NULL, "else-Zweig ohne Griffversatz");

    /* Keiner der vier MTN-Vergleiche trifft: das Original liest mit einer
       nicht initialisierten mtn_no. Hier wird das gemeldet, kein Ersatz. */
    reset_logs();
    work_init(&w, &p);
    p.flags |= 0x80000u;
    g_player.flg = 0x1000u; g_player.ply_id = 0u; g_player.mtn_no = 9999u;
    recvx_en01_player_link(&w);
    check_eq(g_link_calls, 0, "unbestimmte mtn_no fuehrt keinen Griff aus");
    check(gap_seen("mtn_no unbestimmt"), "unbestimmte mtn_no wird gemeldet");

    /* Frameindex hinter dem Feldende: 50 Eintraege, Frame 50 ist daneben. */
    reset_logs();
    work_init(&w, &p);
    p.flags |= 0x80000u;
    g_player.flg = 0x1000u; g_player.ply_id = 0u;
    g_player.mtn_no = 404u;                   /* 400 + 4 -> Index 2, 50 lang */
    g_player.frm_no = 50 * 65536;
    recvx_en01_player_link(&w);
    check_eq(g_link_calls, 0, "Frame hinter dem Feldende wird nicht gelesen");
    check(gap_seen("frm_no / 65536"), "Frame hinter dem Feldende gemeldet");
}

/* ===========================================================================
   7 — Tabellenwerte, die ein Zustandsuebergang direkt braucht
   =========================================================================== */
static void test_table_shape(void)
{
    /* chg_mtn_tbl Zeile 9: mode == -1 liegt in einem `unsigned int`-Feld und
       ist damit 0xFFFFFFFF - genau wie im Original (en01.c:1213). */
    check_eq((long long)recvx_en01_chg_mtn_tbl[9].chg_tbl[0].mode,
             0xFFFFFFFFll, "chg_mtn_tbl[9]: mode -1 wird 0xFFFFFFFF");
    /* Zeile 0 traegt mode 655363 und clr_flg 0xFFFF7FFF (en01.c:1150). */
    check_eq((long long)recvx_en01_chg_mtn_tbl[0].chg_tbl[0].mode, 655363,
             "chg_mtn_tbl[0]: mode 655363");
    check_eq((long long)recvx_en01_chg_mtn_tbl[0].chg_tbl[0].clr_flg,
             0xFFFF7FFFll, "chg_mtn_tbl[0]: clr_flg 0xFFFF7FFF");
    /* Alle drei Motionstabellen enden auf no == -1, worauf die
       Suchschleifen des Originals bauen. */
    check_eq(recvx_en01_mtn_tbl[25].no, -1, "en01_mtn_tbl endet auf -1");
    check_eq(recvx_en01_mtn_tbl2[59].no, -1, "en01_mtn_tbl2 endet auf -1");
    check_eq(recvx_en01_mtn_tbl3[15].no, -1, "en01_mtn_tbl3 endet auf -1");
    check_eq(recvx_en01_keff_tbl[10].mtn_no, -1, "en01_keff_tbl endet auf -1");
    check_eq(recvx_en01_weff_tbl[17].mtn_no, -1, "en01_weff_tbl endet auf -1");
    /* Laufmotionspaare (en01.c:1118): Unterkoerper + 200 = Oberkoerper. */
    check_eq(recvx_en01_walk_mtn[1] - recvx_en01_walk_mtn[0], 200,
             "Laufpaar 0: Oberkoerper = Unterkoerper + 200");
    check_eq(recvx_en01_walk_mtn[3] - recvx_en01_walk_mtn[2], 200,
             "Laufpaar 1: Oberkoerper = Unterkoerper + 200");
    check_eq(recvx_en01_walk_mtn[5] - recvx_en01_walk_mtn[4], 200,
             "Laufpaar 2: Oberkoerper = Unterkoerper + 200");
    /* en01_hp_tbl ist wertgleich mit combat_tables_data.inc:615. */
    check_eq(recvx_en01_hp_tbl[0][0], 60, "en01_hp_tbl[0][0] = 60");
    check_eq(recvx_en01_hp_tbl[1][15], 60, "en01_hp_tbl[1][15] = 60");
}

/* ===========================================================================
   8 — Move-Verteiler: der NULL-Eintrag der Originaltabelle
   =========================================================================== */
static void test_move_dispatch_hole(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_EN01_EXP0  p;

    bind_services(0u, 1);
    g_player_bound = 1;
    memset(&g_player, 0, sizeof(g_player));

    /* bhEne01_MoveMode2W[13] ist NULL (en01.c:2270). Das Original springt
       dorthin und stuerzt ab; der Port meldet die Stelle namentlich.
       Typ 8 ist dafuer die einzige brauchbare Wahl: MoveTypeW[8] ist
       MVType00 (en01.c:2245), und MVType00 ueberspringt fuer type == 8
       Brain00 und ActionModeCheck (en01.c:4873). Bei jedem anderen Typ
       setzt ActionModeCheck mode2 vorher um und der NULL-Platz wird nie
       erreicht. */
    reset_logs();
    work_init(&w, &p);
    w.type = 8; w.mode0 = 1; w.mode2 = 13;
    recvx_en01_move(&w);
    check(gap_seen("MoveMode2W[13]"), "NULL-Eintrag 13 wird namentlich gemeldet");
}

/* ===========================================================================
   9 — Trefferreaktion: Motionwahl, Oberkoerperkopplung, Kappenwurf
   ===========================================================================
   bhEne01_DG00 (en01.c:8143), DG02 (:8255), DG16 (:9837). exp0 Bit 0x2000
   ist in allen drei die Weiche vordere/hintere Treffermotion. */
static void test_damage_states(void)
{
    RECVX_ENEMY_WORK w, cepw;
    RECVX_EN01_EXP0  p, cp;

    bind_services(0u, 1);
    g_player_bound = 1;
    memset(&g_player, 0, sizeof(g_player));

    /* DG00 vorne: MTN 20 am Hauptkoerper, 220 am Oberkoerper. */
    reset_logs();
    work_init(&w, &p);
    work_init(&cepw, &cp);
    w.exp1 = &cepw;
    p.flags = 0x2000u | 0x3000000u;
    cp.flags = 0x3000000u;
    recvx_en01_dg00(&w);
    check_eq((int)w.mtn_no, 20, "DG00 mit 0x2000: MTN 20");
    check_eq((int)cepw.mtn_no, 220, "DG00 mit 0x2000: Oberkoerper MTN 220");
    check_eq((int)(p.flags & 0x3000000u), 0, "DG00 loescht exp0 0x3000000");
    check_eq((int)(cp.flags & 0x3000000u), 0, "DG00 loescht es auch am Oberkoerper");
    check((w.flg & 0x40000u) != 0, "DG00 setzt flg 0x40000");
    check_eq((int)w.mode3, 1, "DG00 geht nach mode3 1");

    /* DG00 hinten: MTN 21/221. */
    reset_logs();
    work_init(&w, &p);
    work_init(&cepw, &cp);
    w.exp1 = &cepw;
    recvx_en01_dg00(&w);
    check_eq((int)w.mtn_no, 21, "DG00 ohne 0x2000: MTN 21");
    check_eq((int)cepw.mtn_no, 221, "DG00 ohne 0x2000: Oberkoerper MTN 221");

    /* DG00 ohne Oberkoerper darf nicht abbrechen (cepw == NULL, :8169). */
    reset_logs();
    work_init(&w, &p);
    recvx_en01_dg00(&w);
    check_eq((int)w.mtn_no, 21, "DG00 ohne Oberkoerper laeuft weiter");

    /* DG02 wirft die Kappe: ohne 0x2000 auf Frame 5 (:8303), mit 0x2000 auf
       Frame 2 (:8298). Ohne Kappen-Kindwork bleibt FlyingCap still
       (en01.c:11904), also pruefen wir die Modusgruppe aus :8290. */
    reset_logs();
    work_init(&w, &p);
    work_init(&cepw, &cp);
    w.exp1 = &cepw;
    recvx_en01_dg02(&w);
    check_eq((int)w.mtn_no, 17, "DG02 ohne 0x2000: MTN 17");
    check_eq((int)(p.flags & 0xFu), 1, "DG02 zwingt die Modusgruppe auf 1");

    /* DG16 benutzt den anderen Motionwechsel: nur 0x2000000 loeschen,
       0x1000000 setzen (en01.c:9864). */
    reset_logs();
    work_init(&w, &p);
    work_init(&cepw, &cp);
    w.exp1 = &cepw;
    p.flags = 0x2000000u;
    cp.flags = 0x2000000u;
    recvx_en01_dg16(&w);
    check_eq((int)w.mtn_no, 130, "DG16 ohne 0x2000: MTN 130");
    check_eq((int)(p.flags & 0x2000000u), 0, "DG16 loescht nur 0x2000000");
    check((p.flags & 0x1000000u) != 0, "DG16 setzt 0x1000000");
    check_eq((int)(p.flags & 0xFu), 6, "DG16 setzt die Modusgruppe auf 6");

    /* DG10/DG11 waehlen die Sturzmotion nach der laufenden Motion
       (en01.c:9483 / :9603); nur DG10 kennt mtn_no 37. */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 37u;
    recvx_en01_dg10(&w);
    check_eq((int)w.mtn_no, 25, "DG10 aus MTN 37: MTN 25");
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 37u;
    recvx_en01_dg11(&w);
    check_eq((int)w.mtn_no, 27, "DG11 kennt MTN 37 nicht: MTN 27");

    /* DG10 mit hp < 0 springt sofort auf mode3 2 (en01.c:9504), laeuft aber
       trotzdem durch den textuell folgenden case 1 (:9510). */
    reset_logs();
    work_init(&w, &p);
    w.hp = -1;
    recvx_en01_dg10(&w);
    check_eq((int)w.mode3, 2, "DG10 mit hp < 0 geht nach mode3 2");

    /* Der Verteiler kennt jetzt jeden Platz ausser den beiden echten
       NULL-Stellen 12 und 13 (en01.c:2340-2341). */
    reset_logs();
    work_init(&w, &p);
    w.type = 0; w.mode2 = 12;
    recvx_en01_dg_type00(&w);
    check(gap_seen("DamageMode2W[12] == NULL"), "DamageMode2W[12] meldet NULL");
    reset_logs();
    work_init(&w, &p);
    w.type = 0; w.mode2 = 13;
    recvx_en01_dg_type00(&w);
    check(gap_seen("DamageMode2W[13] == NULL"), "DamageMode2W[13] meldet NULL");

    /* DGType02 verschiebt den Index um 10 (en01.c:8139): mode2 2 trifft
       damit DG12, also den NULL-Platz. */
    reset_logs();
    work_init(&w, &p);
    w.type = 0; w.mode2 = 2;
    recvx_en01_dg_type02(&w);
    check(gap_seen("DamageMode2W[12] == NULL"),
          "DGType02 mode2 2 trifft Platz 12");
}

/* ===========================================================================
   10 — B-Familie: der verknuepfte Oberkoerper (en01b.c)
   =========================================================================== */
static void test_b_family(void)
{
    RECVX_ENEMY_WORK w, main_w;
    RECVX_EN01_EXP0  p, mp;

    bind_services(0u, 1);
    g_player_bound = 1;
    memset(&g_player, 0, sizeof(g_player));

    /* bhEne01_MV13B (en01b.c:211): MTN 42, 0x1000000 weg, 0x2000000 hin,
       way 256. */
    reset_logs();
    work_init(&w, &p);
    p.flags = 0x1000000u;
    recvx_en01_mv13_b(&w);
    check_eq((int)w.mtn_no, 42, "MV13B: MTN 42");
    check_eq((int)(p.flags & 0x1000000u), 0, "MV13B loescht 0x1000000");
    check((p.flags & 0x2000000u) != 0, "MV13B setzt 0x2000000");
    check_eq((int)w.way, 256, "MV13B setzt way 256");
    check_eq((int)w.mode3, 1, "MV13B geht nach mode3 1");

    /* bhEne01_DG12B (en01b.c:656): hp >= 0 -> mode3 1 und Treffer-SE,
       hp < 0 -> mode3 2 und Todes-SE. Die SE-Nummern sind 16786196/97
       (en01b.c:672/679) und laufen ueber den HAUPTKOERPER epp = lkwkp. */
    reset_logs();
    work_init(&w, &p);
    work_init(&main_w, &mp);
    w.lkwkp = &main_w;
    recvx_en01_dg12_b(&w);
    check_eq((int)w.mtn_no, 26, "DG12B: MTN 26");
    check_eq((int)w.mode3, 1, "DG12B mit hp >= 0: mode3 1");
    check_eq((int)(p.flags & 0x3000000u), 0, "DG12B loescht exp0 0x3000000");
    /* Der SE laeuft ueber bhEne01_SePlay auf dem HAUPTKOERPER. Seine
       Nummern 16786196/97 haben das Low-Byte 20 bzw. 21 und fallen damit
       in die Zufallssperre der persoenlichen Stimme (en01.c:11387); ob ein
       Ton herauskommt, gehoert zu test_se_play, nicht hierher. Dass der
       Aufruf am Hauptkoerper und nicht am Oberkoerper landet, zeigt die
       fehlende Luecke: ein exp0-loser Hauptkoerper wuerde melden. */
    check_eq(g_gap_n, 0, "DG12B meldet mit gebundenem Hauptkoerper nichts");

    reset_logs();
    work_init(&w, &p);
    work_init(&main_w, &mp);
    w.lkwkp = &main_w;
    w.hp = -1;
    recvx_en01_dg12_b(&w);
    check_eq((int)w.mode3, 2, "DG12B mit hp < 0: mode3 2");

    /* Ohne lkwkp meldet DG12B die Luecke und ruehrt nichts an
       (das Original dereferenziert dort ungeschuetzt). */
    reset_logs();
    work_init(&w, &p);
    recvx_en01_dg12_b(&w);
    check(gap_seen("DG12B: lkwkp == NULL"), "DG12B ohne lkwkp meldet die Luecke");
    check_eq((int)w.mtn_no, 0, "DG12B ohne lkwkp wechselt keine Motion");

    /* bhEne01_DG13B (en01b.c:707) ist immer toedlich und immer SE 16786196. */
    reset_logs();
    work_init(&w, &p);
    work_init(&main_w, &mp);
    w.lkwkp = &main_w;
    w.hp = -1;
    recvx_en01_dg13_b(&w);
    check_eq((int)w.mtn_no, 26, "DG13B: MTN 26");
    check_eq((int)w.mode3, 1, "DG13B geht immer nach mode3 1");
    /* DG13B hat keinen hp-Zweig: auch bei hp < 0 bleibt es mode3 1
       (en01b.c:722), anders als DG12B. */
    check_eq((int)(p.flags & 0x3000000u), 0, "DG13B loescht exp0 0x3000000");

    /* bhEne01_DG05B (en01b.c:402) loest den Oberkoerper: flg 0x80 weg,
       stflg 0x8 weg, Lage vom Hauptkoerper, exp0+0x7C = -1. */
    reset_logs();
    work_init(&w, &p);
    work_init(&main_w, &mp);
    w.lkwkp = &main_w;
    w.flg = 0x80u; w.stflg = 0x8u;
    main_w.px = 3.0f; main_w.py = 4.0f; main_w.pz = 5.0f;
    main_w.ay = 1234; main_w.flr_no = 2;
    recvx_en01_dg05_b(&w);
    check_eq((int)w.mtn_no, 224, "DG05B: MTN 224");
    check_eq((int)(w.flg & 0x80u), 0, "DG05B loest flg 0x80");
    check_eq((int)(w.stflg & 0x8u), 0, "DG05B loescht stflg 0x8");
    check((w.flg & 0x20u) != 0, "DG05B setzt flg 0x20");
    check((w.flg & 0x8u) != 0, "DG05B setzt flg 0x8");
    check(w.px == 3.0f && w.py == 4.0f && w.pz == 5.0f,
          "DG05B uebernimmt die Lage des Hauptkoerpers");
    check_eq((int)w.ay, 1234, "DG05B uebernimmt den Winkel");
    check_eq((int)w.flr_no, 2, "DG05B uebernimmt die Etage");
    check_eq((int)p.word7C, -1, "DG05B setzt exp0+0x7C auf -1");
    check_eq((int)(p.flags & 0xFu), 4, "DG05B setzt die Modusgruppe auf 4");

    /* Die B-Verteiler fuehren fuer jeden uebrigen Platz bhEne01_Dummy
       (en01b.c:144) aus und melden NICHTS. */
    reset_logs();
    work_init(&w, &p);
    p.flags = 0x80000000u;
    w.mode2 = 0;
    recvx_en01_mv_type00_b(&w);
    check_eq(g_gap_n, 0, "MoveMode2B[0] ist Dummy und meldet nichts");
}

/* ===========================================================================
   11 — Griff: Spielerzustand, Kopfabtrennung, Vertragsluecken
   =========================================================================== */
static void test_grab_states(void)
{
    RECVX_ENEMY_WORK w, cepw;
    RECVX_EN01_EXP0  p, cp;

    bind_services(0u, 1);

    /* bhEne01_NG02 (en01.c:7325) setzt den Spieler in den Griffzustand und
       haengt den Gegner auf den festen Abstand hinter ihn. */
    reset_logs();
    g_player_bound = 1;
    memset(&g_player, 0, sizeof(g_player));
    work_init(&w, &p);
    work_init(&cepw, &cp);
    w.exp1 = &cepw;
    w.mtn_no = 15u;
    recvx_en01_ng02(&w);
    check_eq((int)w.mtn_no, 9, "NG02: MTN 9");
    check_eq((int)cepw.mtn_no, 209, "NG02: Oberkoerper MTN 209");
    check_eq((int)w.frm_no, 0, "NG02 aus MTN 15 startet bei Frame 0");
    check_eq((int)g_player.mode0, 4, "NG02 setzt plp->mode0 = 4");
    check_eq((int)g_player.mode2, 1, "NG02 setzt plp->mode2 = 1");
    check((p.flags & 0x20000u) != 0, "NG02 setzt exp0 0x20000");
    check((w.flg2 & 1u) != 0, "NG02 setzt flg2 Bit 0");
    check_eq((int)(w.flg & 0x40u), 0, "NG02 loescht flg 0x40");
    check_eq((int)w.mode3, 1, "NG02 geht nach mode3 1");

    /* Aus einer anderen Motion startet der Biss bei Frame 10 (en01.c:7345). */
    reset_logs();
    work_init(&w, &p);
    w.mtn_no = 3u;
    recvx_en01_ng02(&w);
    check_eq((int)w.frm_no, 655360, "NG02 aus anderer Motion startet bei Frame 10");

    /* mode3 3, MTN 36, Frame 25: Kopf ab, hp -1, flg 0x20 weg
       (en01.c:7452-7458). */
    reset_logs();
    work_init(&w, &p);
    w.mode3 = 3; w.mtn_no = 36u; w.frm_no = 25 << 16;
    w.flg = 0x20u;
    recvx_en01_ng02(&w);
    check_eq((int)w.hp, -1, "NG02 Frame 25: hp = -1");
    check_eq((int)(w.flg & 0x20u), 0, "NG02 Frame 25: flg 0x20 weg");
    check_eq((int)p.hit_voice, -1, "CutHead setzt exp0+0x78 auf -1");
    check_eq(g_se[0], 4877, "NG02 MTN 36 nimmt SE 4877");

    /* MTN 35 Frame 34 ist die Kopfexplosion mit SE 4869 (en01.c:7472). */
    reset_logs();
    work_init(&w, &p);
    w.mode3 = 3; w.mtn_no = 35u; w.frm_no = 34 << 16;
    recvx_en01_ng02(&w);
    check_eq(g_se[0], 4869, "NG02 MTN 35 nimmt SE 4869");
    check_eq((int)w.hp, -1, "NG02 MTN 35 Frame 34: hp = -1");

    /* plp->ct1 fehlt in RECVX_ENEMY_PLAYER; der Zustand meldet das statt es
       zu ersetzen (en01.c:7432). ct0 == 0 beim Eintritt erfuellt die
       Abbruchbedingung :7404 sofort. */
    reset_logs();
    work_init(&w, &p);
    w.mode3 = 2; w.ct0 = 0;
    recvx_en01_ng02(&w);
    check(gap_seen("plp->ct1"), "NG02 meldet das fehlende plp->ct1");

    /* NageMode2W[3] ist im Original NULL (en01.c:2303). */
    reset_logs();
    work_init(&w, &p);
    w.type = 0; w.mode2 = 3;
    recvx_en01_ng_type00(&w);
    check(gap_seen("NageMode2W[3] == NULL"), "NageMode2W[3] meldet NULL");

    /* bhEne01_PlyDG00 (en01.c:7509): mode3 0 dreht den Spieler in den Griff.
       plp->ayp fehlt im Vertrag und wird gemeldet; waxp am GEGNER ist
       dagegen vorhanden und wird gesetzt (en01.c:7546). */
    reset_logs();
    work_init(&w, &p);
    memset(&g_player, 0, sizeof(g_player));
    p.flags = 0x4000u;
    w.ay = 0;
    recvx_en01_ply_dg00(&w, &g_player);
    check(gap_seen("plp->ayp"), "PlyDG00 meldet das fehlende plp->ayp");
    check_eq((int)g_player.mode3, 1, "PlyDG00 geht nach mode3 1");
    check_eq((int)(g_player.flg & 0x40000u), 0, "PlyDG00 loescht plp->flg 0x40000");
    check((g_player.exp1_flags & 0x1E0u) != 0, "PlyDG00 setzt exp1 0x1E0");
    check_eq((int)(g_player.exp1_flags & 4u), 0, "PlyDG00 loescht exp1 Bit 2");

    /* mode3 2 haengt den Spieler auf die Motion des Gegners um und nimmt
       die Griffbindung zurueck (en01.c:7613-7627). */
    reset_logs();
    work_init(&w, &p);
    memset(&g_player, 0, sizeof(g_player));
    g_player.mode3 = 2;
    p.flags = 0x80000u;
    recvx_en01_ply_dg00(&w, &g_player);
    check((g_player.exp1_flags & 4u) != 0, "PlyDG00 mode3 2 setzt exp1 Bit 2");
    check_eq((int)g_player.hokan_rate, 32768, "PlyDG00 mode3 2: hokan_rate 32768");
    check_eq((int)(p.flags & 0x80000u), 0, "PlyDG00 mode3 2 loescht exp0 0x80000");
    check_eq((int)g_player.mode3, 3, "PlyDG00 mode3 2 geht nach mode3 3");

    /* Ein ply_id ausserhalb der vier Zeilen von En01_PlyMtn_OffsetTbl wird
       gemeldet und nicht indiziert (en01.c:7593). */
    reset_logs();
    work_init(&w, &p);
    memset(&g_player, 0, sizeof(g_player));
    g_player.ply_id = 9u;
    recvx_en01_ply_dg00(&w, &g_player);
    check(gap_seen("En01_PlyMtn_OffsetTbl[ply_id] ausserhalb"),
          "PlyDG00 meldet ply_id ausserhalb der Tabelle");
}

int main(void)
{
    /* njSin/njCos liefern null, solange die Tabelle fehlt (ninja_math.c:7). */
    _Make_SinTable();
    recvx_core_random_seed(1u);

    test_personal_type();
    test_init_hp();
    test_init_types();
    test_se_play();
    test_check_mtn_tbl();
    test_player_link_table();
    test_table_shape();
    test_move_dispatch_hole();
    test_damage_states();
    test_b_family();
    test_grab_states();

    printf("en01_core_test: %d Pruefungen, %d Fehler\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
