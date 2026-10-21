/* Komponentenpruefung der T3-Gemeinschaftsbibliothek native/en03_lib.c.
   Kein Teil des Spiels: ein eigenstaendiges Pruefprogramm, das gegen die
   originalen Zustandsuebergaenge und Tabellenwerte aus zonzon.c/zonzon1.c
   prueft. Die Erwartungen sind so gewaehlt, dass sie eine falsche Bindung
   aufdecken (Vorzeichen, Reihenfolge, Flagmasken, Quirks) und nicht nur den
   neuen Code nacherzaehlen.

   Bau/Lauf siehe archived/work/native_port_T3_large.result.md. */

#include "en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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
    if (!(fabsf(got - want) <= eps)) {
        failures++;
        printf("FAIL: %s (got %.6f want %.6f)\n", what, (double)got, (double)want);
    }
}

static void oki(long got, long want, const char* what)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got %ld want %ld)\n", what, got, want);
    }
}

/* --------------------------------------------------------------------------
   Pruefstand: ein Gegnerwork mit genau den Diensten, die die geprueften
   Mechanismen anfassen. Alles andere bleibt NULL, damit eine ungewollte
   Abhaengigkeit als Lueckenmeldung auffaellt.
   -------------------------------------------------------------------------- */

static char gap_log[64][96];
static int  gap_count;

static void stub_gap(const char* name)
{
    if (gap_count < 64) snprintf(gap_log[gap_count], sizeof gap_log[0], "%s", name);
    gap_count++;
}

static int gap_seen(const char* needle)
{
    int i;
    for (i = 0; i < gap_count && i < 64; i++)
        if (strstr(gap_log[i], needle)) return 1;
    return 0;
}

/* Fester Zufallsstrom: liefert der Reihe nach vorgegebene Werte, damit die
   REIHENFOLGE der Ziehungen pruefbar ist. */
static int32_t rnd_seq[32];
static int rnd_n, rnd_i;

static int32_t stub_random(void)
{
    if (rnd_i < rnd_n) return rnd_seq[rnd_i++];
    rnd_i++;
    return 0;
}

static uint32_t stub_obj_num_value = 8u;
static uint32_t stub_obj_num(const RECVX_ENEMY_WORK* w) { (void)w; return stub_obj_num_value; }

/* Gelenkmatrizen: Einheitsmatrix mit vorgegebener Translation. */
static RECVX_ENEMY_VEC stub_joint[16];
static int stub_joint_matrix(const RECVX_ENEMY_WORK* w, uint32_t j, float* out16)
{
    (void)w;
    if (j >= 16u) return 0;
    njUnitMatrix((NJS_MATRIX*)(void*)out16);
    out16[12] = stub_joint[j].x;
    out16[13] = stub_joint[j].y;
    out16[14] = stub_joint[j].z;
    return 1;
}

static RECVX_ENEMY_PLAYER stub_player;
static int stub_player_get(RECVX_ENEMY_PLAYER* out) { *out = stub_player; return 1; }
static void stub_player_set(const RECVX_ENEMY_PLAYER* in) { stub_player = *in; }

/* Wandprobe: ein Rechteck in x, damit die Seiten-/Richtungsproben echte
   Treffer und Fehltreffer liefern. */
static float wall_min_x = 1.0f, wall_max_x = 2.0f;
static int stub_wall_type(const RECVX_ENEMY_VEC* p, uint32_t flg, float r, float h)
{
    (void)flg; (void)r; (void)h;
    return (p->x >= wall_min_x && p->x <= wall_max_x) ? 1 : 0;
}

static int stub_check_wall_calls;
static float stub_check_wall_push;
static int stub_check_wall(RECVX_ENEMY_WORK* w)
{
    stub_check_wall_calls++;
    w->px += stub_check_wall_push;
    return 1;
}

static int stub_dansa_calls;
static void stub_dansa(RECVX_ENEMY_WORK* w) { (void)w; stub_dansa_calls++; }

static RECVX_ENEMY_SERVICES services;
static RECVX_ENEMY_EXT ext;

static void setup(RECVX_ENEMY_WORK* w)
{
    memset(&services, 0, sizeof services);
    memset(&ext, 0, sizeof ext);
    memset(w, 0, sizeof *w);
    gap_count = 0;
    rnd_n = rnd_i = 0;
    stub_check_wall_calls = 0;
    stub_dansa_calls = 0;
    stub_check_wall_push = 0.0f;

    services.gap = stub_gap;
    services.random_next = stub_random;
    services.obj_num = stub_obj_num;
    services.joint_matrix = stub_joint_matrix;
    services.player_get = stub_player_get;
    services.player_set = stub_player_set;
    services.wall_type = stub_wall_type;
    services.check_wall = stub_check_wall;

    ext.gap = stub_gap;
    ext.dansa = stub_dansa;
    ext.player_get = stub_player_get;
    ext.player_set = stub_player_set;
    recvx_enemy_ext_bind(&ext);

    w->sv = &services;
    w->flg = 1u;
    w->ar = 1.0f;
    w->ah = 2.0f;
}

/* ========================================================================== */

static void test_sin_table(void)
{
    /* Die Falle aus dem Auftrag: ohne _Make_SinTable() sind alle Drehungen
       null. Hier wird belegt, dass die Tabelle wirkt. */
    okf(njSin(0x4000), 1.0f, 1e-3f, "njSin(0x4000) == 1 (Sinustabelle aufgebaut)");
    okf(njCos(0), 1.0f, 1e-3f, "njCos(0) == 1");
    okf(njSin(0xC000), -1.0f, 1e-3f, "njSin(0xC000) == -1");
}

static void test_niten_and_arctan(void)
{
    /* zonzon.c:70: atan2f(dx, dz) * 10430.381. Ziel genau in -Z-Richtung vom
       Gegner aus heisst dz = +len, dx = 0 -> Winkel 0. */
    oki(recvx_t3_niten_dir(0.0f, 0.0f, 0.0f, -10.0f), 0, "niten_dir: Ziel auf -Z -> 0");
    /* Ziel auf +Z: atan2(0, -len) = PI, und PI * 10430.381 liegt knapp ueber
       32768, der Schnitt zur Null hin ergibt genau 32768. */
    oki(recvx_t3_niten_dir(0.0f, 0.0f, 0.0f, 10.0f), 32768,
        "niten_dir: Ziel auf +Z -> 32768");
    /* Ziel auf -X: atan2(+len, 0) = PI/2 -> 0x4000. */
    oki(recvx_t3_niten_dir(0.0f, 0.0f, -10.0f, 0.0f), 16384, "niten_dir: Ziel auf -X -> 0x4000");

    /* zonzon1.c:864 bhArcTan2 nutzt asinf, nicht atan2: fuer x < 0 wird der
       Winkel an 0x8000 gespiegelt und (short)-geschnitten. */
    oki(recvx_t3_arc_tan2(0.0f, 0.0f), 0, "arc_tan2(0,0) == 0 (Laenge 0)");
    oki(recvx_t3_arc_tan2(0.0f, 1.0f), 0, "arc_tan2(0,+x) == 0");
    oki(recvx_t3_arc_tan2(1.0f, 0.0f), 16384, "arc_tan2(+y,0) == 0x4000");
    /* Der Spiegelzweig rechnet 32768 - ang und schneidet auf short: fuer
       ang 0 ergibt das -32768, nicht +32768. Genau dieser Umlauf gehoert zur
       Rechnung (zonzon1.c:878). */
    oki(recvx_t3_arc_tan2(0.0f, -1.0f), -32768,
        "arc_tan2(0,-x) == -32768 (short-Umlauf des Spiegelzweigs)");
    /* Im Spiegelzweig wird der Winkel von 32768 ABGEZOGEN; fuer y>0 und x<0
       liegt das Ergebnis damit knapp unter 0x6000 (hier 0x8000 - 0x2000). */
    ok(recvx_t3_arc_tan2(1.0f, -1.0f) >= 24575 && recvx_t3_arc_tan2(1.0f, -1.0f) <= 24577,
       "arc_tan2: Spiegelzweig zieht den Winkel von 32768 ab");
}

static void test_cdir_check(void)
{
    /* zonzon.c:76: 1, solange die Differenz im Fenster [-0x4000, 0x3fff) liegt. */
    oki(recvx_t3_cdir_check(0, 0), 1, "cdir_check: gleiche Richtung -> 1");
    /* Das Fenster ist (my - trg) in [-0x4000, 0x3FFF]: die untere Kante
       -0x4000 liegt noch innen, die obere +0x4000 nicht mehr. */
    oki(recvx_t3_cdir_check(0, 0x3fff), 1, "cdir_check: Differenz -0x3fff innen");
    oki(recvx_t3_cdir_check(0, 0x4000), 1, "cdir_check: Differenz -0x4000 noch innen");
    oki(recvx_t3_cdir_check(0, -0x3fff), 1, "cdir_check: Differenz +0x3fff innen");
    oki(recvx_t3_cdir_check(0, -0x4000), 0, "cdir_check: Differenz +0x4000 schon aussen");
    oki(recvx_t3_cdir_check(0, 0x8000), 0, "cdir_check: Gegenrichtung -> 0");
    oki(recvx_t3_cdir_check(0, 0x4001), 0, "cdir_check: Differenz -0x4001 aussen");
}

static void test_ikou(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_VEC target;

    /* Ziel auf -Z, also Sollwinkel 0. */
    target = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, -10.0f };

    /* Grosser Restwinkel: es wird genau add_dir gedreht, nicht gesprungen. */
    setup(&w);
    w.ay = 0x4000;
    recvx_t3_ikou(&w, &target, 0x100);
    oki(w.ay, 0x4000 - 0x100, "ikou: dreht um add_dir in Richtung Ziel");

    /* Restwinkel kleiner als 2*add_dir: der Zielwinkel wird uebernommen. */
    setup(&w);
    w.ay = 0x100;
    recvx_t3_ikou(&w, &target, 0x100);
    oki(w.ay, 0, "ikou: Restwinkel < 2*add_dir -> Zielwinkel uebernommen");

    /* Andere Seite: Restwinkel 0x10000-0x100 dreht in die Gegenrichtung. */
    setup(&w);
    w.ay = -0x400;
    recvx_t3_ikou(&w, &target, 0x100);
    oki(w.ay, -0x400 + 0x100, "ikou: dreht auf der kurzen Seite zurueck");

    /* flg 0x80 (an einen anderen Work gehaengt) dreht nicht. */
    setup(&w);
    w.flg |= 0x80u;
    w.ay = 0x4000;
    recvx_t3_ikou(&w, &target, 0x100);
    oki(w.ay, 0x4000, "ikou: flg 0x80 dreht nicht");

    /* Negatives add_dir dreht VOM Ziel weg: Zielwinkel 0 wird zu 0x8000.
       Aus ay == 0x8000 heisst das: schon in Gegenrichtung -> uebernommen. */
    setup(&w);
    w.ay = 0x8000 - 0x80;
    recvx_t3_ikou(&w, &target, -0x100);
    oki(w.ay, 0x8000, "ikou: negatives add_dir zielt auf die Gegenrichtung");

    /* ikou3 entscheidet nur und hat KEINE Vorzeichenbehandlung. */
    setup(&w);
    w.ay = 0x4000;
    oki(recvx_t3_ikou3(&w, &target, 0x100), -0x100,
        "ikou3: Ziel rechts -> -add_dir");
    oki(w.ay, 0x4000, "ikou3: dreht nicht selbst");
    setup(&w);
    w.ay = -0x4000;
    oki(recvx_t3_ikou3(&w, &target, 0x100), 0x100, "ikou3: Ziel links -> +add_dir");
    setup(&w);
    w.ay = 0x100;
    oki(recvx_t3_ikou3(&w, &target, 0x100), 0, "ikou3: nahe genug -> 0");
    setup(&w);
    w.flg |= 0x80u;
    w.ay = 0x4000;
    oki(recvx_t3_ikou3(&w, &target, 0x100), 0, "ikou3: flg 0x80 -> 0");
}

static void test_dgc_dir_checks(void)
{
    RECVX_ENEMY_VEC dv;
    float m[16];

    /* zonzon.c:101: Achse ist (-sin(rot), 0, -cos(rot)); bei rot 0 also -Z.
       Ein Vektor nach -Z liegt davor -> 1. */
    dv = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, -1.0f };
    oki(recvx_t3_dgc_dir_check(&dv, 0), 1, "dgc_dir_check: gleiche Richtung -> 1");
    dv = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 1.0f };
    oki(recvx_t3_dgc_dir_check(&dv, 0), 0, "dgc_dir_check: Gegenrichtung -> 0");

    /* Der Eingabevektor darf NICHT veraendert werden (lokale Kopie im Original). */
    dv = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, -4.0f };
    (void)recvx_t3_dgc_dir_check(&dv, 0);
    okf(dv.z, -4.0f, 1e-6f, "dgc_dir_check laesst den Eingabevektor unberuehrt");

    /* zonzon.c:125: dieselbe Probe gegen die -Z-Achse einer Matrix. Eine um
       0x8000 gedrehte Matrix kehrt die Achse um. */
    njUnitMatrix((NJS_MATRIX*)(void*)m);
    njRotateY((NJS_MATRIX*)(void*)m, 0x8000);
    njTranslate((NJS_MATRIX*)(void*)m, 50.0f, 50.0f, 50.0f);
    dv = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, -1.0f };
    oki(recvx_t3_dgc_dir_check2(&dv, m), 0,
        "dgc_dir_check2: um 0x8000 gedrehte Matrix kehrt die Achse um");
    dv = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 1.0f };
    oki(recvx_t3_dgc_dir_check2(&dv, m), 1,
        "dgc_dir_check2: Translation der Matrix wirkt nicht (njCalcVector)");

    /* zonzon.c:150: Quadranten. atan2(dv.x, dv.z) ist 0 fuer +Z. */
    dv = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 1.0f };
    oki(recvx_t3_dgc_dir_check3(&dv, 0), 0, "dgc_dir_check3: vorderer Sektor -> 0");
    dv = (RECVX_ENEMY_VEC){ 1.0f, 0.0f, 0.0f };
    oki(recvx_t3_dgc_dir_check3(&dv, 0), 1, "dgc_dir_check3: +X -> 1");
    dv = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, -1.0f };
    oki(recvx_t3_dgc_dir_check3(&dv, 0), 2, "dgc_dir_check3: -Z -> 2");
    dv = (RECVX_ENEMY_VEC){ -1.0f, 0.0f, 0.0f };
    oki(recvx_t3_dgc_dir_check3(&dv, 0), 3, "dgc_dir_check3: -X -> 3");
}

static void test_angle_check(void)
{
    RECVX_ENEMY_VEC v;

    /* zonzon.c:180: die Achse ist um rot+0x8000 gedreht, die Schranke um
       chk_ang. Mit rot 0 zeigt die Achse nach +Z. Ein Vektor nach +Z hat
       Skalarprodukt 1 und liegt damit ueber jeder Schranke < 0 Grad. */
    v = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 2.0f };
    oki(recvx_t3_angle_check(&v, 0, 0x2000), 1,
        "angle_check: Vektor in der Achse liegt im Fenster");
    /* Der Vektor wird in-place normalisiert (Originalseiteneffekt). */
    okf(v.z, 1.0f, 1e-4f, "angle_check normalisiert den Eingabevektor in-place");

    v = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, -1.0f };
    oki(recvx_t3_angle_check(&v, 0, 0x2000), 0,
        "angle_check: Gegenrichtung liegt nicht im Fenster");
}

static void test_pos_check(void)
{
    /* zonzon.c:402: beide Kanten sind inklusive. */
    oki(recvx_t3_pos_check(1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f), 1, "pos_check: innen");
    oki(recvx_t3_pos_check(0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 2.0f), 1, "pos_check: untere Kante inklusive");
    oki(recvx_t3_pos_check(2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f), 1, "pos_check: obere Kante inklusive");
    oki(recvx_t3_pos_check(2.1f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f), 0, "pos_check: ausserhalb x");
    oki(recvx_t3_pos_check(-0.1f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f), 0, "pos_check: vor der Flaeche");
}

static void test_chg_mtn(void)
{
    RECVX_ENEMY_WORK w;

    setup(&w);
    w.mtn_no = 3u;
    w.flg |= 0x40000u | 0x2000000u;
    w.mtn_add = 0;

    oki(recvx_t3_chg_mtn(&w, 7u, 0x20000, 4), 0, "chg_mtn: neue Motion -> 0");
    oki((long)w.mtn_no, 7, "chg_mtn setzt mtn_no");
    oki(w.frm_no, 0x20000, "chg_mtn setzt frm_no");
    oki((long)w.hokan_count, 4, "chg_mtn setzt hokan_count aus rate");
    oki(w.hokan_rate, 0, "chg_mtn nullt hokan_rate");
    oki((long)w.mtn_md, 0x20, "chg_mtn setzt mtn_md 0x20");
    oki(w.mtn_add, 0x10000, "chg_mtn setzt mtn_add 0x10000");
    ok(!(w.flg & 0x40000u), "chg_mtn loescht flg 0x40000");
    ok(!(w.flg & 0x2000000u), "chg_mtn loescht flg 0x2000000");

    /* Gleiche Motion: nur mtn_add, sonst nichts, und Rueckgabe -1. */
    w.frm_no = 0x50000;
    w.mtn_add = 0;
    w.flg |= 0x40000u;
    oki(recvx_t3_chg_mtn(&w, 7u, 0, 9), -1, "chg_mtn: gleiche Motion -> -1");
    oki(w.frm_no, 0x50000, "chg_mtn: gleiche Motion laesst frm_no stehen");
    oki(w.mtn_add, 0x10000, "chg_mtn: mtn_add wird auch dann gesetzt");
    ok((w.flg & 0x40000u) != 0u, "chg_mtn: gleiche Motion laesst flg 0x40000 stehen");
}

static void test_lever_check(void)
{
    RECVX_ENEMY_WORK w;

    /* Ohne gebundene Padquelle gibt es keinen erfundenen Wert, sondern eine
       Lueckenmeldung. */
    setup(&w);
    ext.pad_ps = NULL;
    oki(recvx_t3_lever_check(&w), 0, "lever_check ohne Dienst -> 0");
    ok(gap_seen("zonzon.c:82"), "lever_check meldet die fehlende Padquelle");
}

static uint32_t test_pad;
static uint32_t stub_pad_ps(void) { return test_pad; }

static void test_lever_check2(void)
{
    RECVX_ENEMY_WORK w;

    setup(&w);
    ext.pad_ps = stub_pad_ps;

    test_pad = 0x0u;
    oki(recvx_t3_lever_check(&w), 0, "lever_check: nichts gedrueckt -> 0");
    test_pad = 0x1u;
    oki(recvx_t3_lever_check(&w), 4, "lever_check: Richtung -> 4");
    test_pad = 0xFu;
    oki(recvx_t3_lever_check(&w), 4, "lever_check: mehrere Richtungen bleiben 4 (Zuweisung)");
    test_pad = 0x0100u;
    oki(recvx_t3_lever_check(&w), 3, "lever_check: Knopf 0x0100 -> 3");
    test_pad = 0x2d00u;
    oki(recvx_t3_lever_check(&w), 3, "lever_check: ganze Knopfgruppe bleibt 3");
    test_pad = 0x2d01u;
    oki(recvx_t3_lever_check(&w), 7, "lever_check: Richtung und Knopf -> 7");
    test_pad = 0x0200u;
    oki(recvx_t3_lever_check(&w), 0, "lever_check: Bit ausserhalb 0x2d00 -> 0");
}

static void test_dir_wall(void)
{
    RECVX_ENEMY_WORK w;

    /* zonzon.c:1279: Probe bei pos - step*(sin,cos) des Winkels ay+ang.
       Wand liegt bei x in [1,2]. */
    setup(&w);
    w.px = 0.0f; w.py = 0.0f; w.pz = 0.0f;
    w.ay = 0;

    /* ang -0x4000: sin = -1, also x = 0 - step*(-1) = +step. step 1.5 trifft. */
    oki(recvx_t3_check_dir_wall(&w, -0x4000, 1.5f), 1,
        "check_dir_wall: linke Probe trifft die Wand bei x 1.5");
    oki(recvx_t3_check_dir_wall(&w, -0x4000, 0.5f), 0,
        "check_dir_wall: zu kurzer Schritt trifft nicht");
    oki(recvx_t3_check_dir_wall(&w, 0x4000, 1.5f), 0,
        "check_dir_wall: rechte Probe geht nach -x und trifft nicht");

    /* zonzon.c:1265: Variante 2 addiert aox/aoz, aber NICHT aoy. */
    w.aox = -1.0f;
    oki(recvx_t3_check_dir_wall2(&w, -0x4000, 2.5f), 1,
        "check_dir_wall2: Koerperversatz aox wirkt");

    /* zonzon.c:1321: beidseitiger Treffer liefert `both`. */
    setup(&w);
    w.px = 0.0f; w.ay = 0;
    wall_min_x = -2.0f; wall_max_x = 2.0f;
    oki(recvx_t3_check_side_wall(&w, 1.5f, 77), 77,
        "check_side_wall: beidseitiger Treffer liefert den both-Wert");
    wall_min_x = 1.0f; wall_max_x = 2.0f;
    oki(recvx_t3_check_side_wall(&w, 1.5f, 77), 1,
        "check_side_wall: nur links -> +1");
    wall_min_x = -2.0f; wall_max_x = -1.0f;
    oki(recvx_t3_check_side_wall(&w, 1.5f, 77), -1,
        "check_side_wall: nur rechts -> -1");
    wall_min_x = 10.0f; wall_max_x = 11.0f;
    oki(recvx_t3_check_side_wall(&w, 1.5f, 77), 0,
        "check_side_wall: kein Treffer -> 0");
    wall_min_x = 1.0f; wall_max_x = 2.0f;
}

static void test_collision_check_wall(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_VEC ps, pd;

    /* zonzon.c:1173: Lage/Radius/Hoehe werden fuer den Lauf getauscht, flg
       0x100 geloescht, danach ALLES zurueckgesetzt. Die Stufenanhebung laeuft
       ausdruecklich vor der Wandprobe. */
    setup(&w);
    w.px = 10.0f; w.py = 11.0f; w.pz = 12.0f;
    w.ar = 1.0f; w.ah = 2.0f;
    w.flg |= 0x100u;
    stub_check_wall_push = 0.25f;

    ps = (RECVX_ENEMY_VEC){ 1.0f, 2.0f, 3.0f };
    oki(recvx_t3_collision_check_wall(&w, &ps, &pd, 7.0f, 8.0f), 1,
        "collision_check_wall: Verschiebung in x meldet Kontakt");
    okf(pd.x, 0.25f, 1e-6f, "collision_check_wall liefert die Verschiebung");
    okf(w.px, 10.0f, 1e-6f, "collision_check_wall stellt px wieder her");
    okf(w.py, 11.0f, 1e-6f, "collision_check_wall stellt py wieder her");
    okf(w.pz, 12.0f, 1e-6f, "collision_check_wall stellt pz wieder her");
    okf(w.ar, 1.0f, 1e-6f, "collision_check_wall stellt ar wieder her");
    okf(w.ah, 2.0f, 1e-6f, "collision_check_wall stellt ah wieder her");
    ok((w.flg & 0x100u) != 0u, "collision_check_wall stellt flg 0x100 wieder her");
    oki(stub_dansa_calls, 1, "collision_check_wall ruft die Stufenanhebung genau einmal");
    oki(stub_check_wall_calls, 1, "collision_check_wall ruft die Wandprobe genau einmal");

    /* Keine Verschiebung -> 0. */
    stub_check_wall_push = 0.0f;
    oki(recvx_t3_collision_check_wall(&w, &ps, &pd, 7.0f, 8.0f), 0,
        "collision_check_wall: ohne Verschiebung -> 0");
}

static void test_quick_sort(void)
{
    RECVX_T3_SORT a[6];
    int i;

    /* zonzon.c:1100: aufsteigend nach wk. */
    const float in[6] = { 5.0f, 1.0f, 4.0f, 1.0f, 3.0f, 2.0f };
    for (i = 0; i < 6; i++) { a[i].wk = in[i]; a[i].indx = i; }

    recvx_t3_quick_sort(a, 0, 5);

    for (i = 1; i < 6; i++)
        ok(a[i - 1].wk <= a[i].wk, "quick_sort sortiert aufsteigend");

    /* Der Index wandert mit dem Schluessel. */
    ok(a[0].wk == 1.0f && (a[0].indx == 1 || a[0].indx == 3),
       "quick_sort fuehrt den Index mit");
}

static void test_get_shape_cnt(void)
{
    static const RECVX_T3_PKEY p[3] = { { 0, 0.0f }, { 10, 1.0f }, { 20, 5.0f } };

    /* zonzon1.c:701: Treffer auf einen Schluessel liefert dessen Wert,
       sonst lineare Interpolation zwischen den Nachbarn. */
    okf(recvx_t3_get_shape_cnt(p, 3, 0), 0.0f, 1e-6f, "get_shape_cnt: erster Schluessel");
    okf(recvx_t3_get_shape_cnt(p, 3, 10), 1.0f, 1e-6f, "get_shape_cnt: Schluesseltreffer");
    okf(recvx_t3_get_shape_cnt(p, 3, 5), 0.5f, 1e-6f, "get_shape_cnt: Mitte des ersten Abschnitts");
    okf(recvx_t3_get_shape_cnt(p, 3, 15), 3.0f, 1e-6f, "get_shape_cnt: Mitte des zweiten Abschnitts");

    gap_count = 0;
    recvx_enemy_ext_bind(&ext);
    (void)recvx_t3_get_shape_cnt(p, 3, 999);
    ok(gap_seen("zonzon1.c:701"), "get_shape_cnt meldet das Tabellenende statt zu raten");
}

static void test_calc_damage(void)
{
    RECVX_ENEMY_WORK w;
    /* zonzon1.h:28 COMBWEP_WORK: crit, pt[3], timeout, bonus. */
    static const RECVX_T3_COMBWEP wep[2] = {
        { 0, { 0, 0, 0 }, 0, 0 },
        { 10, { 4, 0, 0 }, 30, 50 }
    };
    static const RECVX_T3_COMBJOINT joint[8] = {
        { 0u, 0 }, { 0u, 0 }, { 0u, 100 }, { 1u, 0 },
        { 0u, 0 }, { 0u, 0 }, { 0u, 0 }, { 0u, 0 }
    };

    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 1;
    w.dvx = 0.0f; w.dvy = 0.0f; w.dvz = -1.0f;   /* Treffer von vorn */
    w.ay = 0;

    /* Gelenk 1 traegt 10 Schaden, correct 0 -> genau 10. Gelenk 0 wird
       ausgelassen, auch wenn dort Schaden steht. */
    w.dam[0] = 999;
    w.dam[1] = 10;
    recvx_t3_calc_damage(&w, wep, joint);
    oki(w.total_dam, 10, "calc_damage: Gelenk 0 wird ausgelassen");
    oki((long)w.comb_wep, 1, "calc_damage setzt comb_wep auf die Waffe");
    oki(w.comb_timeout, 30, "calc_damage setzt comb_timeout aus der Waffentabelle");

    /* correct 100 verdoppelt den Schaden des Gelenks (dam + dam*100/100). */
    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 1;
    w.dvz = -1.0f;
    w.dam[2] = 10;
    recvx_t3_calc_damage(&w, wep, joint);
    oki(w.total_dam, 20, "calc_damage: correct 100 verdoppelt");

    /* cjp->flg Bit 0 ueberspringt NUR die Kombozaehlung, nicht den Schaden. */
    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 1;
    w.dvz = -1.0f;
    w.dam[3] = 10;
    recvx_t3_calc_damage(&w, wep, joint);
    oki(w.total_dam, 10, "calc_damage: Gelenk mit flg 1 zaehlt weiter Schaden");
    oki(w.comb_pnt, 0, "calc_damage: Gelenk mit flg 1 zaehlt keine Kombopunkte");
    oki(w.comb_timeout, 0, "calc_damage: Gelenk mit flg 1 setzt keinen Timeout");

    /* Ein WAFFENWECHSEL nullt die Kombopunkte: comb_wep != wpnr_no nimmt den
       else-Zweig, auch wenn schon Punkte stehen. */
    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 1;
    w.dvz = 1.0f;
    w.comb_flg = 0x10u;
    w.comb_wep = 0u;        /* andere Waffe als wpnr_no */
    w.comb_timeout = 5;
    w.comb_pnt = 8;
    w.dam[1] = 10;
    recvx_t3_calc_damage(&w, wep, joint);
    oki(w.comb_pnt, 4, "calc_damage: Waffenwechsel nullt comb_pnt und zaehlt pt[0] neu");
    oki((long)(w.comb_flg & 0x1u), 0, "calc_damage: unter crit kein comb_flg 0x1");
    oki(w.total_dam, 10, "calc_damage: unter crit kein Bonus");

    /* Dieselbe Waffe mit laufendem Timeout BEHAELT die Punkte; damit wird
       crit erreicht und der Bonus 50 wirkt. */
    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 1;
    w.dvz = 1.0f;
    w.comb_flg = 0x10u;
    w.comb_wep = 1u;
    w.comb_timeout = 5;
    w.comb_pnt = 8;
    w.dam[1] = 10;
    recvx_t3_calc_damage(&w, wep, joint);
    oki((long)(w.comb_flg & 0x1u), 1, "calc_damage setzt comb_flg 0x1 ab crit");
    oki(w.total_dam, 15, "calc_damage: Bonus 50 ab crit -> 15");
    oki(w.comb_pnt, 0, "calc_damage nullt comb_pnt ohne comb_flg 0x2");
    oki(w.comb_timeout, 30, "calc_damage erneuert comb_timeout aus der Waffentabelle");

    /* comb_flg 0x2 haelt die Punkte ueber den crit hinaus. */
    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 1;
    w.dvz = 1.0f;
    w.comb_flg = 0x10u | 0x2u;
    w.comb_wep = 1u;
    w.comb_timeout = 5;
    w.comb_pnt = 8;
    w.dam[1] = 10;
    recvx_t3_calc_damage(&w, wep, joint);
    oki(w.comb_pnt, 12, "calc_damage: comb_flg 0x2 haelt comb_pnt");

    /* Trefferseite (zonzon1.c:493/515): dv zeigt vom Trefferpunkt zum
       Koerperzentrum. Bei ay 0 blickt der Gegner nach -Z; ein Treffer von
       VORN liegt also bei -Z und dv zeigt nach +Z. dir_check liefert dann 0
       und calc_damage setzt comb_flg 0x4. Ein Treffer von HINTEN ergibt
       dv nach -Z, dir_check 1 und comb_flg 0x8. */
    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 0;
    w.ay = 0;
    w.dvz = 1.0f;
    recvx_t3_calc_damage(&w, wep, joint);
    oki((long)(w.comb_flg & 0xCu), 0x4L, "calc_damage: Treffer von vorn -> comb_flg 0x4");

    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 0;
    w.ay = 0;
    w.dvz = -1.0f;
    recvx_t3_calc_damage(&w, wep, joint);
    oki((long)(w.comb_flg & 0xCu), 0x8L, "calc_damage: Treffer von hinten -> comb_flg 0x8");

    /* comb_flg verliert zu Beginn genau 0x1|0x4|0x8, behaelt aber 0x10. */
    setup(&w);
    stub_obj_num_value = 8u;
    w.wpnr_no = 0;
    w.dvz = 1.0f;
    w.comb_flg = 0x1u | 0x4u | 0x8u | 0x10u | 0x20u | 0x40u;
    recvx_t3_calc_damage(&w, wep, joint);
    oki((long)(w.comb_flg & 0x70u), 0x70L, "calc_damage laesst 0x10/0x20/0x40 stehen");
    oki((long)(w.comb_flg & 0x1u), 0L, "calc_damage loescht comb_flg 0x1 zu Beginn");
}

static void test_hit_check_parts(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_VEC pos;
    RECVX_T3_LINK lk;

    /* zonzon.c:1376 QUIRK: die Schleife laeuft von Gelenk 1 bis obj_num-2,
       laesst also Wurzel UND letztes Gelenk aus. */
    setup(&w);
    stub_obj_num_value = 4u;
    memset(stub_joint, 0, sizeof stub_joint);
    stub_joint[0] = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };   /* Wurzel */
    stub_joint[1] = (RECVX_ENEMY_VEC){ 10.0f, 0.0f, 0.0f };
    stub_joint[2] = (RECVX_ENEMY_VEC){ 20.0f, 0.0f, 0.0f };
    stub_joint[3] = (RECVX_ENEMY_VEC){ 30.0f, 0.0f, 0.0f };  /* letztes */

    lk.w = &w; lk.player = 0;

    /* Punkt genau an der Wurzel: Gelenk 0 ist ausgeschlossen, also gewinnt 1. */
    pos = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };
    recvx_t3_hit_check_parts(lk, &pos);
    oki(w.djnt_no, 1, "hit_check_parts: Wurzel ist ausgeschlossen");

    /* Punkt am letzten Gelenk: auch das ist ausgeschlossen, 2 gewinnt. */
    pos = (RECVX_ENEMY_VEC){ 30.0f, 0.0f, 0.0f };
    recvx_t3_hit_check_parts(lk, &pos);
    oki(w.djnt_no, 2, "hit_check_parts: letztes Gelenk ist ausgeschlossen");
    okf(w.dpx, 30.0f, 1e-6f, "hit_check_parts setzt dpx auf den Trefferpunkt");
    okf(w.dvx, -10.0f, 1e-6f, "hit_check_parts setzt dv auf die umgekehrte Differenz");

    /* Abstand 0 am Gelenk 1 muss greifen (Startwert -1 wird mitgeprueft). */
    pos = (RECVX_ENEMY_VEC){ 10.0f, 0.0f, 0.0f };
    recvx_t3_hit_check_parts(lk, &pos);
    oki(w.djnt_no, 1, "hit_check_parts: Abstand 0 greift trotz Startwert -1");
    okf(w.dvx, 0.0f, 1e-6f, "hit_check_parts: dv ist am Gelenk null");
}

static uint32_t ball_count_value;
static RECVX_ENEMY_WORK* ball_list[4];
static int ball_player_index = -1;

static uint32_t stub_ene_count(void) { return ball_count_value; }
static RECVX_ENEMY_WORK* stub_ene_at(uint32_t i)
{
    if (i >= ball_count_value) return NULL;
    return ball_list[i];
}
static int stub_ene_is_player(uint32_t i) { return ((int)i == ball_player_index) ? 1 : 0; }

static RECVX_T3_PLAYER_BODY stub_body;
static int stub_player_body(struct RECVX_T3_PLAYER_BODY* out) { *out = stub_body; return 1; }
static uint32_t stub_player_stflg;
static void stub_player_stflg_or(uint32_t bits) { stub_player_stflg |= bits; }

static void test_check_enemies_ball(void)
{
    RECVX_ENEMY_WORK a, b;

    /* zonzon1.c:529: Abstossen auf den Randabstand car+car, danach stflg 0x2
       am ANDEREN Work. flg2 Bit 0 schaltet das ganze Abstossen ab. */
    setup(&a);
    memset(&b, 0, sizeof b);
    b.sv = &services;
    b.flg = 1u | 0x8u;
    b.px = 1.0f; b.car = 1.0f;

    a.car = 1.0f;
    a.px = 0.0f;

    ball_list[0] = &a;
    ball_list[1] = &b;
    ball_count_value = 2u;
    ball_player_index = -1;

    ext.ene_count = stub_ene_count;
    ext.ene_at = stub_ene_at;
    ext.ene_is_player = stub_ene_is_player;

    recvx_t3_check_enemies_ball(&a);
    okf(a.px, -1.0f, 1e-5f, "check_enemies_ball: auf den Randabstand car+car geschoben");
    oki((long)(b.stflg & 0x2u), 2, "check_enemies_ball setzt stflg 0x2 am anderen Work");
    oki((long)(a.stflg & 0x2u), 0, "check_enemies_ball setzt am eigenen Work nichts");

    /* Ohne flg 0x8 des anderen Works wird nicht geschoben. */
    setup(&a);
    ext.ene_count = stub_ene_count;
    ext.ene_at = stub_ene_at;
    ext.ene_is_player = stub_ene_is_player;
    b.flg = 1u;
    b.stflg = 0u;
    a.car = 1.0f; a.px = 0.0f;
    recvx_t3_check_enemies_ball(&a);
    okf(a.px, 0.0f, 1e-6f, "check_enemies_ball: ohne flg 0x8 kein Abstossen");

    /* flg2 Bit 0 schaltet ab. */
    setup(&a);
    ext.ene_count = stub_ene_count;
    ext.ene_at = stub_ene_at;
    ext.ene_is_player = stub_ene_is_player;
    b.flg = 1u | 0x8u;
    b.stflg = 0u;
    a.flg2 = 1u;
    a.car = 1.0f; a.px = 0.0f;
    recvx_t3_check_enemies_ball(&a);
    okf(a.px, 0.0f, 1e-6f, "check_enemies_ball: flg2 Bit 0 schaltet ab");

    /* Der Spielerplatz der Liste: der Spieler erhaelt stflg 0x2, nicht 0x4
       (0x4 gehoert dem Zweig epw == plp, der von hier unerreichbar ist). */
    setup(&a);
    ext.ene_count = stub_ene_count;
    ext.ene_at = stub_ene_at;
    ext.ene_is_player = stub_ene_is_player;
    ext.player_body = stub_player_body;
    ext.player_stflg_or = stub_player_stflg_or;
    stub_player_stflg = 0u;
    memset(&stub_body, 0, sizeof stub_body);
    stub_body.pos = (RECVX_ENEMY_VEC){ 1.0f, 0.0f, 0.0f };
    stub_body.car = 1.0f;
    stub_body.flg = 1u | 0x8u;
    ball_player_index = 1;
    a.car = 1.0f; a.px = 0.0f;
    recvx_t3_check_enemies_ball(&a);
    okf(a.px, -1.0f, 1e-5f, "check_enemies_ball: Abstossen auch gegen den Spielerplatz");
    oki((long)(stub_player_stflg & 0x6u), 2,
        "check_enemies_ball: Spieler erhaelt stflg 0x2, nicht 0x4");
    ball_player_index = -1;
}

static void test_blood_tables(void)
{
    /* zonzon1.c:9/43 — Stichproben der Originaltabellen. */
    oki(recvx_t3_blood_type[2].color, 1, "BloodType[2].color");
    oki(recvx_t3_blood_type[2].tex_id, 5, "BloodType[2].tex_id");
    oki(recvx_t3_blood_type[12].color, 3, "BloodType[12].color");
    oki(recvx_t3_blood_type[12].mdlno, 2, "BloodType[12].mdlno");
    oki(recvx_t3_blood_type[13].color, 2, "BloodType[13].color");
    oki(recvx_t3_blood_type[13].mdlno, 3, "BloodType[13].mdlno");
    oki(recvx_t3_blood_type[15].tex_id, 8, "BloodType[15].tex_id");
    oki(recvx_t3_blood_type[23].mdlno, 6, "BloodType[23].mdlno");
    oki(recvx_t3_blood_type[30].color, 2, "BloodType[30].color");
    oki(recvx_t3_blood_type[30].tex_id, 2, "BloodType[30].tex_id");

    oki((long)recvx_t3_blood_col[0][0], 0xFF5F0000L, "BloodCol[0][0]");
    oki((long)recvx_t3_blood_col[1][1], 0xFF20102FL, "BloodCol[1][1]");
    oki((long)recvx_t3_blood_col[2][0], 0xFF3F3F16L, "BloodCol[2][0]");
    oki((long)recvx_t3_blood_col[3][1], 0xFF2F0000L, "BloodCol[3][1]");
}

/* -------------------------------------------------------------------------- */

static RECVX_T3_EFF eff_log[16];
static int eff_joint_log[16];
static int eff_has_ofs[16];
static RECVX_ENEMY_VEC eff_ofs_log[16];
static int eff_count;

static int stub_effect_tb(const RECVX_T3_EFF* ef, const RECVX_ENEMY_VEC* ofs,
                          RECVX_T3_LINK link, int joint)
{
    (void)link;
    if (eff_count >= 16) return -1;
    eff_log[eff_count] = *ef;
    eff_joint_log[eff_count] = joint;
    eff_has_ofs[eff_count] = (ofs != NULL);
    if (ofs) eff_ofs_log[eff_count] = *ofs;
    return eff_count++;
}

static RECVX_T3_EFFPATCH patch_log[16];
static int patch_slot_log[16];
static int patch_count;
static void stub_effect_patch(int slot, const RECVX_T3_EFFPATCH* p)
{
    if (patch_count >= 16) return;
    patch_slot_log[patch_count] = slot;
    patch_log[patch_count++] = *p;
}

static int part_kind_log[16];
static uint32_t part_col_log[16];
static int part_count;
static int stub_particle(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos,
                         const RECVX_ENEMY_VEC* vec, uint32_t color, int kind)
{
    (void)lk; (void)joint; (void)pos; (void)vec;
    if (part_count < 16) { part_kind_log[part_count] = kind; part_col_log[part_count] = color; }
    return part_count++;
}

static int stub_player_way_value = 0x1234;
static int stub_player_way(void) { return stub_player_way_value; }

static void effects_reset(void)
{
    eff_count = 0;
    patch_count = 0;
    part_count = 0;
    ext.effect_tb = stub_effect_tb;
    ext.effect_patch = stub_effect_patch;
    ext.effect_particle = stub_particle;
    ext.player_way = stub_player_way;
}

static void test_set_blood2(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_VEC ofs;

    /* zonzon.c:466 Typ 2: Blutspritzer id 5, Groesse 3.0, Gelenk djnt_no. */
    setup(&w);
    effects_reset();
    w.id = 3u;
    w.djnt_no = 6;
    ofs = (RECVX_ENEMY_VEC){ 0.1f, 0.2f, 0.3f };
    recvx_t3_set_blood2(&w, 2, &ofs, 0);
    oki(eff_count, 1, "set_blood2 Typ 2 legt genau einen Effekt an");
    oki(eff_log[0].id, 5, "set_blood2 Typ 2: Effekt-id 5");
    oki(eff_log[0].type, 2, "set_blood2 Typ 2: Effekttyp 2");
    okf(eff_log[0].sx, 3.0f, 1e-6f, "set_blood2 Typ 2: Groesse 3.0");
    oki(eff_log[0].mdlver, 0, "set_blood2: id != 17 -> mdlver 0");
    oki(eff_joint_log[0], 6, "set_blood2 haengt am Trefferglied");
    okf(ofs.z, 0.3f, 1e-6f, "set_blood2 Typ 2 laesst ofs->z stehen");

    /* id 17 waehlt mdlver 3 und die andere Blutfarbe. */
    setup(&w);
    effects_reset();
    w.id = 17u;
    recvx_t3_set_blood2(&w, 0, &ofs, 0);
    oki(eff_log[0].mdlver, 3, "set_blood2: id 17 -> mdlver 3");

    /* Typ 3 legt Effekt UND Partikel an und nullt ofs->z beim Aufrufer. */
    setup(&w);
    effects_reset();
    w.id = 3u;
    w.dvx = 1.0f; w.dvy = 2.0f; w.dvz = 3.0f;
    ofs = (RECVX_ENEMY_VEC){ 0.1f, 0.2f, 0.3f };
    recvx_t3_set_blood2(&w, 3, &ofs, 0);
    oki(eff_count, 1, "set_blood2 Typ 3: ein Effekt");
    oki(part_count, 1, "set_blood2 Typ 3: ein Partikel");
    oki(part_kind_log[0], 0, "set_blood2 Typ 3: Partikelart 0");
    oki((long)part_col_log[0], 0xFF5F0000L, "set_blood2 Typ 3: Blutfarbe");
    okf(ofs.z, 0.0f, 1e-6f, "set_blood2 Typ 3 nullt ofs->z beim Aufrufer (Quirk)");

    /* Typ 4 SCHREIBT dvx/dvy/dvz aus dem Winkel ry. */
    setup(&w);
    effects_reset();
    w.id = 3u;
    ofs = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.5f };
    recvx_t3_set_blood2(&w, 4, &ofs, 0);
    oki(eff_count, 0, "set_blood2 Typ 4: kein Spritzer, nur Partikel");
    oki(part_count, 1, "set_blood2 Typ 4: ein Partikel");
    oki(part_kind_log[0], 1, "set_blood2 Typ 4: Partikelart 1");
    okf(w.dvz, -1.0f, 1e-4f, "set_blood2 Typ 4 schreibt dvz aus ry");
    okf(w.dvy, njSin(10922), 1e-6f, "set_blood2 Typ 4 schreibt dvy aus dem festen Winkel");
    okf(ofs.z, 0.5f, 1e-6f, "set_blood2 Typ 4 laesst ofs->z stehen");

    /* Typ 11 nutzt eine eigene Farbe und setzt mode1 am Partikel. */
    setup(&w);
    effects_reset();
    w.id = 3u;
    ofs = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.5f };
    recvx_t3_set_blood2(&w, 11, &ofs, 0);
    oki(part_count, 1, "set_blood2 Typ 11: ein Partikel");
    oki(part_kind_log[0], 18, "set_blood2 Typ 11: Partikelart 18");
    oki((long)part_col_log[0], 0xFF9F0000L, "set_blood2 Typ 11: eigene Farbe");
    oki(patch_count, 1, "set_blood2 Typ 11 zieht ein Nachfeld nach");
    oki((long)(patch_log[0].fields & RECVX_T3_EFF_MODE1), RECVX_T3_EFF_MODE1,
        "set_blood2 Typ 11 setzt mode1");
    oki((long)patch_log[0].mode1, 1, "set_blood2 Typ 11: mode1 == 1");

    /* Typ 9 und Typ 12 tun nichts (kein Zweig im Original). */
    setup(&w);
    effects_reset();
    recvx_t3_set_blood2(&w, 9, &ofs, 0);
    recvx_t3_set_blood2(&w, 12, &ofs, 0);
    oki(eff_count + part_count, 0, "set_blood2: Typ 9 und 12 tun nichts");
}

static void test_set_blood_table_branch(void)
{
    RECVX_ENEMY_WORK w;
    /* BT_WORK (zonzon.h:13): lnk_obj, x, y, z, xlen, ylen, size, len. */
    static const RECVX_T3_BT bt[4] = {
        { 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { 9, 1.0f, 2.0f, 3.0f, 0.5f, 0.25f, 5.0f, 1.0f },
        { 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }
    };

    /* zonzon.c:415 mit Tabelle: der Versatz kommt aus dem Eintrag des
       Trefferglieds, mit zwei Zufallsziehungen (x dann y), und djnt_no
       wechselt auf lnk_obj. Blutrichtung ist plp->way. */
    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    w.id = 3u;
    w.djnt_no = 2;
    w.wpnr_no = 1;              /* nicht 10/13 -> Tabellenzweig */
    w.dvz = -1.0f;              /* Treffer von vorn */
    rnd_seq[0] = 0;             /* rand01 == 0 -> x = x + xlen */
    rnd_seq[1] = 0x40000000;    /* rand01 == 0.5 -> y = y + 0 */
    rnd_n = 2;

    recvx_t3_set_blood(&w, 2, bt);

    oki(w.djnt_no, 9, "set_blood: djnt_no wechselt auf lnk_obj der Tabelle");
    oki(eff_count, 1, "set_blood: ein Spritzer");
    oki(eff_joint_log[0], 9, "set_blood: der Spritzer haengt am Folgeglied");
    ok(eff_has_ofs[0], "set_blood uebergibt einen Versatz");
    okf(eff_ofs_log[0].x, 1.5f, 1e-5f, "set_blood: x = x + xlen bei rand 0");
    okf(eff_ofs_log[0].y, 2.0f, 1e-5f, "set_blood: y = y bei rand 0.5");

    /* wpnr_no 10 erzwingt den Zweig OHNE Tabelle: djnt_no bleibt stehen. */
    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    w.id = 3u;
    w.djnt_no = 2;
    w.wpnr_no = 10;
    w.dpx = 4.0f; w.dpy = 5.0f; w.dpz = 6.0f;
    stub_joint[2] = (RECVX_ENEMY_VEC){ 1.0f, 1.0f, 1.0f };
    recvx_t3_set_blood(&w, 2, bt);
    oki(w.djnt_no, 2, "set_blood: Waffe 10 nimmt den Zweig ohne Tabelle");
    okf(eff_ofs_log[0].x, 3.0f, 1e-5f, "set_blood ohne Tabelle: Weltversatz zurueckgerechnet");

    /* Ohne Tabelle (bt == NULL) derselbe Zweig. */
    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    w.id = 3u;
    w.djnt_no = 2;
    w.wpnr_no = 1;
    w.dpx = 4.0f;
    recvx_t3_set_blood_nbt(&w, 2);
    oki(w.djnt_no, 2, "set_blood_nbt: Zweig ohne Tabelle");
}

static void test_blood4_blocks(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_VEC ofs = { 0.0f, 0.0f, 0.0f };

    /* zonzon.c:683/708: die drei Bloecke von bld_tbl enden an type == -1.
       Block 0 (type 0 und 1) hat neun Eintraege, Block 14.. hat fuenf,
       Block 10.. hat drei plus den Wasserstrahl. */
    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    w.id = 3u;
    recvx_t3_set_blood4(&w, 1, &ofs, 0, 0);
    oki(eff_count, 9, "set_blood4 Typ 0: neun Strahlen (Block endet bei Index 9)");
    oki(eff_log[0].id, 370, "set_blood4: Effekt-id 370");
    okf(eff_log[0].sx, 3.5f, 1e-6f, "set_blood4: Groesse 3.5");

    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    w.id = 3u;
    recvx_t3_set_blood4(&w, 1, &ofs, 0, 3);
    oki(eff_count, 5, "set_blood4 Typ 3: fuenf Strahlen aus dem Block ab 14");

    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    w.id = 3u;
    w.dvz = -1.0f;
    recvx_t3_set_blood4(&w, 1, &ofs, 0, 2);
    oki(eff_count, 4, "set_blood4 Typ 2: Wasserstrahl plus drei aus dem Block ab 10");
    oki(eff_log[0].type, 9, "set_blood4 Typ 2: der erste Strahl ist Typ 9");

    /* Die Nachfelder tragen ct0 = wcnt und ct1 = blood_n[type]. */
    ok(patch_count >= 1, "set_blood4 zieht Nachfelder nach");
    oki(patch_log[0].ct0, 3, "set_blood4 Typ 2: ct0 des Wasserstrahls == 3");
    oki(patch_log[0].ct1, 1, "set_blood4 Typ 2: ct1 des Wasserstrahls == 1");

    /* Ein Typ ausserhalb 0..3 wird gemeldet, nicht geraten. */
    setup(&w);
    effects_reset();
    gap_count = 0;
    recvx_t3_set_blood4(&w, 1, &ofs, 0, 7);
    oki(eff_count, 0, "set_blood4: unbekannter Typ legt nichts an");
    ok(gap_seen("zonzon.c:683"), "set_blood4 meldet den unbekannten Typ");
}

static void test_fire_and_acid(void)
{
    RECVX_ENEMY_WORK w;

    /* zonzon1.c:354: Flamme 252, sz bleibt 1.0 unabhaengig von size, len in ct0. */
    setup(&w);
    effects_reset();
    recvx_t3_set_fire_effect(&w, 5, NULL, 4.0f, 90);
    oki(eff_count, 1, "set_fire_effect legt einen Effekt an");
    oki(eff_log[0].id, 252, "set_fire_effect: Effekt-id 252");
    okf(eff_log[0].sx, 4.0f, 1e-6f, "set_fire_effect: sx == size");
    okf(eff_log[0].sz, 1.0f, 1e-6f, "set_fire_effect: sz bleibt 1.0");
    oki(eff_joint_log[0], 5, "set_fire_effect haengt am Gelenk");
    oki(patch_log[0].ct0, 90, "set_fire_effect: len landet in ct0");

    /* zonzon1.c:397: Saeure 298 Typ 20; mit Gelenk flg 0x81, ohne nur 0x1. */
    setup(&w);
    effects_reset();
    recvx_t3_set_acid_effect(&w, 3, NULL, 2.0f);
    oki(eff_log[0].id, 298, "set_acid_effect: Effekt-id 298");
    oki(eff_log[0].type, 20, "set_acid_effect: Typ 20");
    oki((long)eff_log[0].flg, 0x81L, "set_acid_effect: mit Gelenk flg 0x81");

    setup(&w);
    effects_reset();
    recvx_t3_set_acid_effect(&w, -1, NULL, 2.0f);
    oki((long)eff_log[0].flg, 0x1L, "set_acid_effect: ohne Gelenk flg 0x1");
}

static void test_bloodstain(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_VEC ofp = { 1.0f, 0.0f, 0.0f };

    /* zonzon1.c:301: der Fleck wird VIERMAL angelegt. */
    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    stub_joint[2] = (RECVX_ENEMY_VEC){ 10.0f, 0.0f, 0.0f };
    w.id = 3u;
    w.dvx = 0.0f; w.dvy = 0.0f; w.dvz = -1.0f;
    recvx_t3_set_bloodstain(&w, 1, 2, &ofp);
    oki(eff_count, 4, "set_bloodstain legt vier Effekte an");
    oki(eff_log[0].id, 301, "set_bloodstain: Effekt-id 301");
    okf(eff_log[0].px, 11.0f, 1e-5f, "set_bloodstain legt ofp durch die Gelenkmatrix");

    /* Ohne Versatz ist es die Gelenklage. */
    setup(&w);
    effects_reset();
    memset(stub_joint, 0, sizeof stub_joint);
    stub_joint[2] = (RECVX_ENEMY_VEC){ 10.0f, 0.0f, 0.0f };
    w.id = 3u;
    recvx_t3_set_bloodstain(&w, 1, 2, NULL);
    okf(eff_log[0].px, 10.0f, 1e-5f, "set_bloodstain ohne Versatz: Gelenklage");

    /* jno < 0 ohne Versatz ist die eigene Lage. */
    setup(&w);
    effects_reset();
    w.id = 3u;
    w.px = 7.0f; w.py = 8.0f; w.pz = 9.0f;
    recvx_t3_set_bloodstain(&w, 1, -1, NULL);
    okf(eff_log[0].px, 7.0f, 1e-5f, "set_bloodstain jno < 0 ohne Versatz: eigene Lage");
}

static void test_translate_mtn_gate(void)
{
    RECVX_ENEMY_WORK w;

    /* zonzon.c:209: das Gate laeuft nur, wenn die Motion vorrueckt ODER der
       verlangte Frame nicht der laufende ist. */
    setup(&w);
    w.px = 1.0f; w.py = 2.0f; w.pz = 3.0f;
    w.mtn_add = 0;
    w.frm_no = 5 * 65536;
    gap_count = 0;
    recvx_t3_get_translate_mtn(&w, 5, 0);
    okf(w.px, 1.0f, 1e-6f, "get_translate_mtn: stehender Clip auf dem Frame tut nichts");
    oki(gap_count, 0, "get_translate_mtn: kein Dienstzugriff im gesperrten Fall");

    /* Vorrueckender Clip ohne gebundenen Schluesseldienst -> Luecke. */
    setup(&w);
    w.mtn_add = 0x10000;
    gap_count = 0;
    recvx_t3_get_translate_mtn(&w, 5, 0);
    ok(gap_seen("zonzon.c:209"), "get_translate_mtn meldet den fehlenden Schluesseldienst");
}

static RECVX_ENEMY_VEC key_at[8];
static int stub_motion_root(const RECVX_ENEMY_WORK* w, uint32_t mtn, uint32_t frame,
                            RECVX_ENEMY_VEC* out)
{
    (void)w; (void)mtn;
    if (frame >= 8u) return 0;
    *out = key_at[frame];
    return 1;
}

static void test_translate_mtn_values(void)
{
    RECVX_ENEMY_WORK w;

    memset(key_at, 0, sizeof key_at);
    key_at[0] = (RECVX_ENEMY_VEC){ 1.0f, 1.0f, 1.0f };
    key_at[1] = (RECVX_ENEMY_VEC){ 2.0f, 3.0f, 4.0f };
    key_at[2] = (RECVX_ENEMY_VEC){ 5.0f, 7.0f, 9.0f };

    /* Frame 0 nimmt den Schluessel selbst; mode 0 verwirft die Hoehe. */
    setup(&w);
    services.motion_root_position = stub_motion_root;
    w.mtn_add = 0x10000;
    w.px = 0.0f; w.py = 0.0f; w.pz = 0.0f;
    w.ay = 0;
    recvx_t3_get_translate_mtn(&w, 0, 0);
    okf(w.px, 1.0f, 1e-5f, "get_translate_mtn Frame 0: Schluessel selbst in x");
    okf(w.pz, 1.0f, 1e-5f, "get_translate_mtn Frame 0: Schluessel selbst in z");
    okf(w.py, 0.0f, 1e-5f, "get_translate_mtn mode 0 verwirft die Hoehe");

    /* Frame > 0 nimmt die Differenz zum Vorgaenger. */
    setup(&w);
    services.motion_root_position = stub_motion_root;
    w.mtn_add = 0x10000;
    recvx_t3_get_translate_mtn(&w, 2, 0);
    okf(w.px, 3.0f, 1e-5f, "get_translate_mtn Frame 2: Differenz in x");
    okf(w.pz, 5.0f, 1e-5f, "get_translate_mtn Frame 2: Differenz in z");

    /* mode != 0 behaelt die Hoehe und rastert sie auf 1/100. */
    setup(&w);
    services.motion_root_position = stub_motion_root;
    w.mtn_add = 0x10000;
    recvx_t3_get_translate_mtn(&w, 2, 1);
    okf(w.py, 4.0f, 1e-4f, "get_translate_mtn mode 1 behaelt die Hoehe");

    /* Variante 2 schreibt nur spd (Betrag des Versatzes). */
    setup(&w);
    services.motion_root_position = stub_motion_root;
    w.mtn_add = 0x10000;
    w.px = 100.0f;
    recvx_t3_get_translate_mtn2(&w, 0, 0);
    okf(w.px, 100.0f, 1e-6f, "get_translate_mtn2 laesst die Lage unberuehrt");
    okf(w.spd, njSqrt(2.0f), 1e-4f, "get_translate_mtn2 setzt spd auf den Betrag");
}

static void test_add_null_trans(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_T3_LINK lk;
    static RECVX_ENEMY_VEC mtn[4];

    memset(mtn, 0, sizeof mtn);
    mtn[2] = (RECVX_ENEMY_VEC){ 1.0f, 9.0f, 2.0f };

    /* zonzon1.c:668: Versatz des GANZZAHLIGEN Frames, um ay gedreht, nur
       auf x und z. Die Hoehe des Schluessels wirkt nicht. */
    setup(&w);
    lk.w = &w; lk.player = 0;
    w.frm_no = 2 * 65536 + 30000;
    w.ay = 0;
    w.px = 10.0f; w.py = 10.0f; w.pz = 10.0f;
    recvx_t3_add_null_trans(lk, mtn);
    okf(w.px, 11.0f, 1e-4f, "add_null_trans addiert x des Frameschluessels");
    okf(w.pz, 12.0f, 1e-4f, "add_null_trans addiert z des Frameschluessels");
    okf(w.py, 10.0f, 1e-6f, "add_null_trans laesst die Hoehe unberuehrt");

    /* Mit ay 0x8000 kehren sich x und z um. */
    setup(&w);
    lk.w = &w; lk.player = 0;
    w.frm_no = 2 * 65536;
    w.ay = 0x8000;
    recvx_t3_add_null_trans(lk, mtn);
    okf(w.px, -1.0f, 1e-3f, "add_null_trans dreht den Versatz um ay");
    okf(w.pz, -2.0f, 1e-3f, "add_null_trans dreht z um ay");

    /* Die Spielervariante geht an den Spielerbesitzer, nicht an einen
       eigenen Spielerwork (en23.c:3572). */
    setup(&w);
    memset(&stub_player, 0, sizeof stub_player);
    stub_player.frm_no = 2 * 65536;
    stub_player.ay = 0;
    stub_player.pos = (RECVX_ENEMY_VEC){ 100.0f, 0.0f, 100.0f };
    lk.w = NULL; lk.player = 1;
    recvx_t3_add_null_trans(lk, mtn);
    okf(stub_player.pos.x, 101.0f, 1e-4f, "add_null_trans schreibt den Spielerbesitzer");
    okf(stub_player.pos.z, 102.0f, 1e-4f, "add_null_trans schreibt plp->pz");

    /* Ohne Spielerbesitzer: Luecke, kein erfundener Wert. */
    setup(&w);
    ext.player_get = NULL;
    ext.player_set = NULL;
    gap_count = 0;
    lk.w = NULL; lk.player = 1;
    recvx_t3_add_null_trans(lk, mtn);
    ok(gap_seen("zonzon1.c:668"), "add_null_trans meldet den fehlenden Spielerbesitzer");
}

static void test_kaidan_quirk(void)
{
    RECVX_ENEMY_WORK w;
    int idx = -1;
    RECVX_ENEMY_ATR out;

    /* zonzon.c:310 ohne gebundene ETC-Liste: Luecke, kein Treffer. */
    setup(&w);
    gap_count = 0;
    oki(recvx_t3_kaidan_atr_check(&w, 1.0f, &idx, &out), 0,
        "kaidan_atr_check ohne ETC-Liste -> 0");
    ok(gap_seen("zonzon.c:310"), "kaidan_atr_check meldet die fehlende ETC-Liste");
}

static void test_vibration_and_voice(void)
{
    /* zonzon.c:1350: nur 0/1/2 wirken, immer Motor 1. */
    static int motor_log[4], power_log[4], vib_count;
    vib_count = 0;
    (void)motor_log; (void)power_log;

    memset(&ext, 0, sizeof ext);
    ext.gap = stub_gap;
    recvx_enemy_ext_bind(&ext);
    gap_count = 0;
    recvx_t3_set_vibration(3);
    oki(gap_count, 0, "set_vibration: unbekannte Nummer tut nichts und meldet nichts");
    recvx_t3_set_vibration(0);
    ok(gap_seen("zonzon.c:1350"), "set_vibration meldet den fehlenden Dienst");
    (void)vib_count;
}

static int voice_log[4], voice_count;
static void stub_player_voice_free(int se)
{
    if (voice_count < 4) voice_log[voice_count] = se;
    voice_count++;
}
static int gate_voice_log[4], gate_voice_count;
static void stub_player_voice_gated(int se)
{
    if (gate_voice_count < 4) gate_voice_log[gate_voice_count] = se;
    gate_voice_count++;
}

static void test_voice_paths(void)
{
    RECVX_ENEMY_WORK w;

    /* zonzon1.c:521 legt 0x400 dazu, zonzon.c:1367 NICHT, und letzteres hat
       das Gate flg 0x10000. */
    setup(&w);
    voice_count = 0;
    ext.player_voice_free = stub_player_voice_free;
    recvx_t3_call_player_voice(5);
    oki(voice_count, 1, "call_player_voice ruft die Stimme");
    oki(voice_log[0], 5 | 0x400, "call_player_voice setzt Bit 0x400");

    setup(&w);
    gate_voice_count = 0;
    services.player_voice = stub_player_voice_gated;
    recvx_t3_player_se_play(&w, 5);
    oki(gate_voice_count, 1, "player_se_play ruft ohne Gate");
    oki(gate_voice_log[0], 5, "player_se_play setzt KEIN Bit 0x400");

    w.flg |= 0x10000u;
    recvx_t3_player_se_play(&w, 6);
    oki(gate_voice_count, 1, "player_se_play: flg 0x10000 unterdrueckt die Stimme");
}

static int se_log[4], se_count;
static uint32_t se_enemy_log[4];
static void stub_enemy_se(uint32_t e, const RECVX_ENEMY_VEC* p, int se)
{
    (void)p;
    if (se_count < 4) { se_log[se_count] = se; se_enemy_log[se_count] = e; }
    se_count++;
}
static uint32_t stub_enemy_now_value = 3u;
static uint32_t stub_enemy_now(void) { return stub_enemy_now_value; }

static void test_call_se(void)
{
    RECVX_ENEMY_WORK w;
    RECVX_ENEMY_VEC pos = { 0.0f, 0.0f, 0.0f };

    /* zonzon1.c:828: Gate flg 0x10000, Gegnernummer aus sys->enow. */
    setup(&w);
    se_count = 0;
    services.enemy_se = stub_enemy_se;
    services.enemy_now = stub_enemy_now;
    recvx_t3_call_se(&w, &pos, 12);
    oki(se_count, 1, "call_se ruft den Gegner-SE");
    oki(se_log[0], 12, "call_se uebergibt die SE-Nummer");
    oki((long)se_enemy_log[0], 3, "call_se nutzt die laufende Gegnernummer");

    w.flg |= 0x10000u;
    recvx_t3_call_se(&w, &pos, 13);
    oki(se_count, 1, "call_se: flg 0x10000 unterdrueckt den Ruf");
}

int main(void)
{
    /* Ohne diesen Aufbau liefern njSin/njCos null und jede Drehung waere 0
       (native/ninja_math.c:7; Runtime ruft es in native/runtime.cpp:159). */
    _Make_SinTable();

    test_sin_table();
    test_niten_and_arctan();
    test_cdir_check();
    test_ikou();
    test_dgc_dir_checks();
    test_angle_check();
    test_pos_check();
    test_chg_mtn();
    test_lever_check();
    test_lever_check2();
    test_dir_wall();
    test_collision_check_wall();
    test_quick_sort();
    test_get_shape_cnt();
    test_calc_damage();
    test_hit_check_parts();
    test_check_enemies_ball();
    test_blood_tables();
    test_set_blood2();
    test_set_blood_table_branch();
    test_blood4_blocks();
    test_fire_and_acid();
    test_bloodstain();
    test_translate_mtn_gate();
    test_translate_mtn_values();
    test_add_null_trans();
    test_kaidan_quirk();
    test_vibration_and_voice();
    test_voice_paths();
    test_call_se();

    printf("%d Pruefungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
