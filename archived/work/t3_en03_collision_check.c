/* Komponentenpruefung der EN03-Kollisionsfamilie native/en03_collision.c.
   Eigenstaendiges Pruefprogramm, kein Teil des Spiels. Die Erwartungen sind
   aus der Geometrie der Originalroutinen hergeleitet (en03.c:5399-6072) und
   so gewaehlt, dass sie Vorzeichen-, Seiten- und Reihenfolgefehler aufdecken.
   Mehrere Faelle pruefen ausdruecklich Originalquirks. */

#include "en03_collision.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures, checks;

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
    if (got != want) { failures++; printf("FAIL: %s (got %ld want %ld)\n", what, got, want); }
}

/* -------------------------------------------------------------------------- */

static char gap_log[32][96];
static int gap_count;
static void stub_gap(const char* name)
{
    if (gap_count < 32) snprintf(gap_log[gap_count], sizeof gap_log[0], "%s", name);
    gap_count++;
}
static int gap_seen(const char* needle)
{
    int i;
    for (i = 0; i < gap_count && i < 32; i++) if (strstr(gap_log[i], needle)) return 1;
    return 0;
}

static float room_h = 20.0f;
static float stub_room_height(void) { return room_h; }

static RECVX_T3_ATR walls[8];
static int wall_n;
static int stub_wal_count(void) { return wall_n; }
static int stub_wal_at(uint32_t i, RECVX_T3_ATR* out)
{
    if ((int)i >= wall_n) return 0;
    *out = walls[i];
    return 1;
}

static RECVX_ENEMY_SERVICES services;
static RECVX_ENEMY_EXT ext;

static void setup(RECVX_ENEMY_WORK* w)
{
    memset(&services, 0, sizeof services);
    memset(&ext, 0, sizeof ext);
    if (w) memset(w, 0, sizeof *w);
    gap_count = 0;
    room_h = 20.0f;
    services.gap = stub_gap;
    ext.gap = stub_gap;
    ext.room_height = stub_room_height;
    ext.wal_count = stub_wal_count;
    ext.wal_at = stub_wal_at;
    recvx_enemy_ext_bind(&ext);
    if (w) { w->sv = &services; w->flg = 1u; w->ar = 1.0f; }
}

static RECVX_T3_ATR box_atr(float px, float py, float pz, float w, float h, float d)
{
    RECVX_T3_ATR a;
    memset(&a, 0, sizeof a);
    a.flg = 1u;
    a.type = 0u;
    a.px = px; a.py = py; a.pz = pz;
    a.w = w; a.h = h; a.d = d;
    return a;
}

/* ========================================================================== */

static void test_geometry_primitives(void)
{
    RECVX_ENEMY_VEC a = { 0.0f, 0.0f, 0.0f }, b = { 3.0f, 4.0f, 0.0f };
    RECVX_EN03_LINE l;
    RECVX_EN03_PLANE pl;
    RECVX_ENEMY_VEC p, cp, area[4], n;

    okf(recvx_en03_distance_p2p(&a, &b), 5.0f, 1e-5f, "distance_p2p");

    /* ps2_NaColi.c:77 — Abstand zur GERADEN, Fusspunkt auf der Geraden. */
    l.p = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };
    l.v = (RECVX_ENEMY_VEC){ 1.0f, 0.0f, 0.0f };
    p = (RECVX_ENEMY_VEC){ 5.0f, 2.0f, 0.0f };
    okf(recvx_en03_distance_p2l(&p, &l, &cp), 2.0f, 1e-4f, "distance_p2l: Abstand");
    okf(cp.x, 5.0f, 1e-4f, "distance_p2l: Fusspunkt x");
    okf(cp.y, 0.0f, 1e-4f, "distance_p2l: Fusspunkt y");

    /* Der Fusspunkt liegt auch HINTER dem Linienursprung (unbegrenzte Gerade). */
    p = (RECVX_ENEMY_VEC){ -5.0f, 2.0f, 0.0f };
    (void)recvx_en03_distance_p2l(&p, &l, &cp);
    okf(cp.x, -5.0f, 1e-4f, "distance_p2l: Gerade ist unbegrenzt");

    /* Punkt genau auf dem Linienursprung: Abstand 0 und cp == Ursprung. */
    p = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };
    okf(recvx_en03_distance_p2l(&p, &l, &cp), 0.0f, 1e-6f, "distance_p2l: Sonderfall 0");

    /* ps2_NaColi.c:125 — vorzeichenloser Abstand; die Normale muss nicht
       Einheitslaenge haben. */
    pl.p = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };
    pl.v = (RECVX_ENEMY_VEC){ 0.0f, 7.0f, 0.0f };
    p = (RECVX_ENEMY_VEC){ 1.0f, -3.0f, 2.0f };
    okf(recvx_en03_distance_p2pl(&p, &pl, &cp), 3.0f, 1e-4f,
        "distance_p2pl: Betrag auch unterhalb der Ebene");
    okf(cp.y, 0.0f, 1e-4f, "distance_p2pl: Lotfusspunkt auf der Ebene");
    okf(cp.x, 1.0f, 1e-4f, "distance_p2pl: Lotfusspunkt behaelt x");

    /* ps2_NaColi.c:301 — Kreuzprodukt der ersten drei Punkte, ohne Normierung.
       Ein Viereck in der XZ-Ebene gegen den Uhrzeigersinn ergibt +Y. */
    area[0] = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };
    area[1] = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 1.0f };
    area[2] = (RECVX_ENEMY_VEC){ 1.0f, 0.0f, 1.0f };
    area[3] = (RECVX_ENEMY_VEC){ 1.0f, 0.0f, 0.0f };
    recvx_en03_plane_normal(area, &n);
    okf(n.x, 0.0f, 1e-6f, "plane_normal: x");
    okf(n.z, 0.0f, 1e-6f, "plane_normal: z");
    ok(fabsf(n.y) > 0.0f, "plane_normal: y ungleich null");

    /* hitchkl.c:682 — Punkt innen/aussen, Kanten inklusive. */
    p = (RECVX_ENEMY_VEC){ 0.5f, 0.0f, 0.5f };
    oki(recvx_en03_in_out_check(&p, area, &n, 4), 1, "in_out_check: innen");
    p = (RECVX_ENEMY_VEC){ 1.5f, 0.0f, 0.5f };
    oki(recvx_en03_in_out_check(&p, area, &n, 4), 0, "in_out_check: aussen");
    p = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.5f };
    oki(recvx_en03_in_out_check(&p, area, &n, 4), 1, "in_out_check: Kante gilt als innen");
    oki(recvx_en03_in_out_check(&p, area, &n, 2), 0, "in_out_check: unter drei Punkten -> 0");
    /* Ohne uebergebene Normale wird sie aus area[0..2] berechnet. */
    p = (RECVX_ENEMY_VEC){ 0.5f, 0.0f, 0.5f };
    oki(recvx_en03_in_out_check(&p, area, NULL, 4), 1,
        "in_out_check: Normale wird bei NULL selbst bestimmt");
}

static void test_wall_box(void)
{
    RECVX_T3_ATR hp = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    RECVX_ENEMY_VEC pos;

    setup(NULL);

    /* QUIRK: tief im Kasten liegt der Punkt von allen aufgeweiteten Ebenen
       weiter als ar entfernt; kein Zweig verschiebt, die Rueckgabe ist
       trotzdem 1. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 5.0f, 5.0f };
    oki(recvx_en03_wall_box(&hp, &pos, 1.0f), 1, "wall_box: Mitte meldet Treffer");
    okf(pos.x, 5.0f, 1e-6f, "wall_box: Mitte wird NICHT verschoben (Quirk)");
    okf(pos.y, 5.0f, 1e-6f, "wall_box: Mitte y unveraendert");
    okf(pos.z, 5.0f, 1e-6f, "wall_box: Mitte z unveraendert");

    /* Nahe der x-Grundflaeche: Flaechenzweig schiebt auf px - ar. */
    pos = (RECVX_ENEMY_VEC){ 0.5f, 5.0f, 5.0f };
    oki(recvx_en03_wall_box(&hp, &pos, 1.0f), 1, "wall_box: nahe x-Flaeche trifft");
    okf(pos.x, -1.0f, 1e-5f, "wall_box: auf px - ar hinausgeschoben");
    okf(pos.y, 5.0f, 1e-6f, "wall_box: y bleibt");

    /* Jenseits der Mitte wird die FERNE Seite gewaehlt. */
    pos = (RECVX_ENEMY_VEC){ 9.5f, 5.0f, 5.0f };
    oki(recvx_en03_wall_box(&hp, &pos, 1.0f), 1, "wall_box: nahe ferner x-Flaeche trifft");
    okf(pos.x, 11.0f, 1e-5f, "wall_box: auf px + w + ar hinausgeschoben");

    /* Ecke: radiale Verschiebung auf den Abstand ar von der Ecke. */
    pos = (RECVX_ENEMY_VEC){ -0.5f, -0.5f, -0.5f };
    oki(recvx_en03_wall_box(&hp, &pos, 1.0f), 1, "wall_box: Eckzweig trifft");
    okf(njSqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z), 1.0f, 1e-4f,
        "wall_box: Eckzweig legt den Punkt auf Abstand ar zur Ecke");
    ok(pos.x < 0.0f && pos.y < 0.0f && pos.z < 0.0f,
       "wall_box: Eckzweig schiebt nach aussen, nicht hinein");

    /* Ausserhalb des aufgeweiteten Kastens: 0 und keine Bewegung. */
    pos = (RECVX_ENEMY_VEC){ -2.0f, 5.0f, 5.0f };
    oki(recvx_en03_wall_box(&hp, &pos, 1.0f), 0, "wall_box: aussen -> 0");
    okf(pos.x, -2.0f, 1e-6f, "wall_box: aussen unveraendert");

    /* h == 0 setzt die Raumhoehe ein (en03.c:5418). */
    hp.h = 0.0f;
    room_h = 20.0f;
    pos = (RECVX_ENEMY_VEC){ 5.0f, 15.0f, 5.0f };
    oki(recvx_en03_wall_box(&hp, &pos, 1.0f), 1, "wall_box: h 0 nutzt die Raumhoehe");
    room_h = 2.0f;
    pos = (RECVX_ENEMY_VEC){ 5.0f, 15.0f, 5.0f };
    oki(recvx_en03_wall_box(&hp, &pos, 1.0f), 0, "wall_box: kleinere Raumhoehe trifft nicht");

    /* Ohne gebundene Raumhoehe wird sie gemeldet, nicht geraten. */
    ext.room_height = NULL;
    gap_count = 0;
    pos = (RECVX_ENEMY_VEC){ 5.0f, 15.0f, 5.0f };
    (void)recvx_en03_wall_box(&hp, &pos, 1.0f);
    ok(gap_seen("rom->h"), "wall_box meldet die fehlende Raumhoehe");
}

static void test_wall_ground(void)
{
    RECVX_T3_ATR hp;
    RECVX_ENEMY_VEC pos;

    setup(NULL);

    /* en03.c:5528: die Bodenplatte wird zu einem Kasten mit NEGATIVER Hoehe
       ab der Oberkante. Daraus folgt das y-Band (py+h-ar, py+ar) — es ist nur
       bei duennen Platten (h < 2*ar) nicht leer. Das ist Originalgeometrie
       und keine Annaeherung. */
    hp = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 0.5f, 10.0f);
    hp.type = 7u;

    pos = (RECVX_ENEMY_VEC){ 5.0f, 0.2f, 5.0f };
    oki(recvx_en03_wall_ground(&hp, &pos, 1.0f), 1, "wall_ground: duenne Platte trifft");

    pos = (RECVX_ENEMY_VEC){ 5.0f, 2.0f, 5.0f };
    oki(recvx_en03_wall_ground(&hp, &pos, 1.0f), 0, "wall_ground: oberhalb des Bandes -> 0");

    /* Eine dicke Platte hat ein LEERES Band; genau das sagt die Rechnung des
       Originals. */
    hp.h = 5.0f;
    pos = (RECVX_ENEMY_VEC){ 5.0f, 2.0f, 5.0f };
    oki(recvx_en03_wall_ground(&hp, &pos, 1.0f), 0,
        "wall_ground: dicke Platte ergibt ein leeres Band (Originalrechnung)");

    /* h == 0 bekommt 0.01 Dicke und wird um ebenso viel abgesenkt; die
       Raumhoehe darf dann NICHT einspringen. */
    hp.h = 0.0f;
    room_h = 500.0f;
    pos = (RECVX_ENEMY_VEC){ 5.0f, 0.0f, 5.0f };
    oki(recvx_en03_wall_ground(&hp, &pos, 1.0f), 1, "wall_ground: h 0 trifft am Nullniveau");
    pos = (RECVX_ENEMY_VEC){ 5.0f, 50.0f, 5.0f };
    oki(recvx_en03_wall_ground(&hp, &pos, 1.0f), 0,
        "wall_ground: h 0 nutzt NICHT die Raumhoehe");
}

static void test_wall_cylinder(void)
{
    RECVX_T3_ATR hp;
    RECVX_ENEMY_VEC pos;

    setup(NULL);

    /* en03.c:5549: Mittelachse (px,pz), Radius w, Hoehe py..py+h. */
    hp = box_atr(0.0f, 0.0f, 0.0f, 3.0f, 10.0f, 0.0f);
    hp.type = 2u;

    /* Innerhalb der Hoehe: radiale Verschiebung auf w + ar. */
    pos = (RECVX_ENEMY_VEC){ 1.0f, 5.0f, 0.0f };
    oki(recvx_en03_wall_cylinder(&hp, &pos, 1.0f), 1, "wall_cylinder: innen trifft");
    okf(njSqrt(pos.x * pos.x + pos.z * pos.z), 4.0f, 1e-4f,
        "wall_cylinder: auf Radius w + ar geschoben");
    okf(pos.x, 4.0f, 1e-4f, "wall_cylinder: Richtung bleibt erhalten");
    okf(pos.y, 5.0f, 1e-6f, "wall_cylinder: Hoehe bleibt unberuehrt");

    /* Ausserhalb des Radius: kein Treffer. */
    pos = (RECVX_ENEMY_VEC){ 10.0f, 5.0f, 0.0f };
    oki(recvx_en03_wall_cylinder(&hp, &pos, 1.0f), 0, "wall_cylinder: aussen -> 0");
    okf(pos.x, 10.0f, 1e-6f, "wall_cylinder: aussen unveraendert");

    /* Unterhalb der Grundflaeche greift der erste Zweig nicht (py < pos->y
       ist falsch); nahe der Deckelhoehe greift der Sehnenzweig. */
    pos = (RECVX_ENEMY_VEC){ 1.0f, -5.0f, 0.0f };
    oki(recvx_en03_wall_cylinder(&hp, &pos, 1.0f), 0,
        "wall_cylinder: weit unter der Grundflaeche -> 0");

    pos = (RECVX_ENEMY_VEC){ 3.5f, 10.5f, 0.0f };
    oki(recvx_en03_wall_cylinder(&hp, &pos, 1.0f), 1,
        "wall_cylinder: Sehnenzweig an der Deckelkante trifft");
    ok(pos.x > 3.5f, "wall_cylinder: Sehnenzweig schiebt nach aussen");
    ok(pos.x < 4.0f, "wall_cylinder: Sehnenradius ist kleiner als w + ar");

    /* h == 0 nutzt die Raumhoehe. */
    hp.h = 0.0f;
    room_h = 2.0f;
    pos = (RECVX_ENEMY_VEC){ 1.0f, 5.0f, 0.0f };
    oki(recvx_en03_wall_cylinder(&hp, &pos, 1.0f), 0,
        "wall_cylinder: ueber der Raumhoehe -> 0");
    room_h = 20.0f;
    pos = (RECVX_ENEMY_VEC){ 1.0f, 5.0f, 0.0f };
    oki(recvx_en03_wall_cylinder(&hp, &pos, 1.0f), 1,
        "wall_cylinder: h 0 nutzt die Raumhoehe");
}

static void test_wall_slope(void)
{
    RECVX_T3_ATR hp;
    RECVX_ENEMY_VEC pos;

    setup(NULL);

    /* en03.c:5594 mit id 0: area[2] und area[3] (die Seite x = px+w) liegen
       auf py+h, die Seite x = px auf py. Die Flaeche steigt also in +x. */
    hp = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    hp.type = 6u;
    hp.id = 0u;

    /* Ein Punkt mitten UEBER der Schraege, naeher als ar: der Flaechenzweig
       legt ihn auf Abstand ar entlang der Flaechennormalen. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 5.2f, 5.0f };
    oki(recvx_en03_wall_slope(&hp, &pos, 1.0f), 1, "wall_slope: Flaechenzweig trifft");
    {
        /* Nach dem Schub muss der Abstand zur Ebene genau ar sein. */
        RECVX_EN03_PLANE pl;
        RECVX_ENEMY_VEC area[4], cp;
        area[0] = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };
        area[1] = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 10.0f };
        area[2] = (RECVX_ENEMY_VEC){ 10.0f, 10.0f, 10.0f };
        area[3] = (RECVX_ENEMY_VEC){ 10.0f, 10.0f, 0.0f };
        recvx_en03_plane_normal(area, &pl.v);
        pl.p = area[0];
        okf(recvx_en03_distance_p2pl(&pos, &pl, &cp), 1.0f, 1e-3f,
            "wall_slope: Flaechenzweig setzt den Abstand auf ar");
    }

    /* Weit weg: kein Treffer. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 50.0f, 5.0f };
    oki(recvx_en03_wall_slope(&hp, &pos, 1.0f), 0, "wall_slope: weit darueber -> 0");

    /* Vor der Flaeche in x: der erste Torwaechter greift. */
    pos = (RECVX_ENEMY_VEC){ -5.0f, 5.0f, 5.0f };
    oki(recvx_en03_wall_slope(&hp, &pos, 1.0f), 0, "wall_slope: vor der Flaeche -> 0");

    /* id 2 kippt die hohe Seite auf z = pz+d; derselbe Punkt liegt dann
       anders zur Ebene. Damit ist belegt, dass `id` ausgewertet wird. */
    hp.id = 2u;
    pos = (RECVX_ENEMY_VEC){ 5.0f, 5.2f, 5.0f };
    {
        const int hit2 = recvx_en03_wall_slope(&hp, &pos, 1.0f);
        RECVX_EN03_PLANE pl;
        RECVX_ENEMY_VEC area[4], cp;
        area[0] = (RECVX_ENEMY_VEC){ 0.0f, 0.0f, 0.0f };
        area[1] = (RECVX_ENEMY_VEC){ 0.0f, 10.0f, 10.0f };
        area[2] = (RECVX_ENEMY_VEC){ 10.0f, 10.0f, 10.0f };
        area[3] = (RECVX_ENEMY_VEC){ 10.0f, 0.0f, 0.0f };
        oki(hit2, 1, "wall_slope: id 2 trifft ebenfalls");
        recvx_en03_plane_normal(area, &pl.v);
        pl.p = area[0];
        okf(recvx_en03_distance_p2pl(&pos, &pl, &cp), 1.0f, 1e-3f,
            "wall_slope: id 2 benutzt die um z gekippte Ebene");
    }
}

static void test_wall_triangle(void)
{
    RECVX_T3_ATR hp;
    RECVX_ENEMY_VEC pos;

    setup(NULL);

    /* en03.c:5770 mit id 0: rechter Winkel bei (px, pz), Hypotenuse von
       (px+w, pz) nach (px, pz+d). */
    hp = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    hp.type = 4u;
    hp.id = 0u;

    /* Unterhalb der Grundflaeche: kein Treffer. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, -5.0f, 5.0f };
    oki(recvx_en03_wall_triangle(&hp, &pos, 1.0f), 0, "wall_triangle: unter py -> 0");

    /* Oberhalb der Deckelhoehe: kein Treffer. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 50.0f, 5.0f };
    oki(recvx_en03_wall_triangle(&hp, &pos, 1.0f), 0, "wall_triangle: ueber py + h -> 0");

    /* QUIRK: im weiten y-Band (py <= y-ar und py+h >= y-ar), aber NICHT im
       engeren Band (py <= y und py+h >= y), meldet die Routine 1 ohne
       irgendetwas zu verschieben. Mit py 0, h 10 und ar 1 trifft das auf
       y = 10.5 zu: y-ar = 9.5 liegt im Band, y = 10.5 nicht. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 10.5f, 5.0f };
    oki(recvx_en03_wall_triangle(&hp, &pos, 1.0f), 1,
        "wall_triangle: Treffer ohne Verschiebung im aeusseren y-Band (Quirk)");
    okf(pos.x, 5.0f, 1e-6f, "wall_triangle: Quirk verschiebt x nicht");
    okf(pos.z, 5.0f, 1e-6f, "wall_triangle: Quirk verschiebt z nicht");

    /* Weit ausserhalb des Grundrisses in x: 0. */
    pos = (RECVX_ENEMY_VEC){ 50.0f, 5.0f, 5.0f };
    oki(recvx_en03_wall_triangle(&hp, &pos, 1.0f), 0,
        "wall_triangle: ausserhalb des Grundrisses -> 0");

    /* Direkt auf der Hypotenuse (x+z == 10): das Hypotenusenband greift und
       zieht auf die Hypotenusengerade. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 5.0f, 5.0f };
    oki(recvx_en03_wall_triangle(&hp, &pos, 1.0f), 1,
        "wall_triangle: Punkt auf der Hypotenuse trifft");
    /* Die Hypotenuse laeuft bei x + z == 10. Das Band ist um `ar` nach
       beiden Seiten verbreitert (vd ist die auf ar skalierte Normale), und
       gezogen wird auf die Gerade durch area[2]/area[3], also die AEUSSERE
       Bandkante: x + z == 10 + ar*sqrt(2). Genau das haelt den Koerper um
       seinen Radius von der Wand ab. */
    okf(pos.x + pos.z, 10.0f + njSqrt(2.0f), 1e-3f,
        "wall_triangle: Hypotenusenzweig zieht auf die aeussere Bandkante");
    okf(pos.y, 5.0f, 1e-6f, "wall_triangle: Hoehe bleibt unberuehrt");

    /* Jenseits der Hypotenuse (ausserhalb des Dreiecks) steigt die Routine
       bei id 0 aus, sobald t < z - pz ist. */
    pos = (RECVX_ENEMY_VEC){ 9.0f, 5.0f, 9.0f };
    oki(recvx_en03_wall_triangle(&hp, &pos, 1.0f), 0,
        "wall_triangle: jenseits der Hypotenuse -> 0 (id 0)");

    /* id 1 kehrt diese Entscheidung um — Beleg, dass `id` ausgewertet wird. */
    hp.id = 1u;
    pos = (RECVX_ENEMY_VEC){ 1.0f, 5.0f, 1.0f };
    oki(recvx_en03_wall_triangle(&hp, &pos, 1.0f), 0,
        "wall_triangle: id 1 kehrt die Dreiecksseite um");
}

static void test_box_edge(void)
{
    RECVX_T3_ATR hp = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    RECVX_ENEMY_VEC pos;

    setup(NULL);

    /* en03.c:5938: die obere Umrandung liegt auf h1 = py + h = 10 und ist
       2*ar dick. Ein Punkt direkt ueber der Oberkante trifft sie. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 10.5f, -1.0f };
    oki(recvx_en03_collision_box_edge(&hp, &pos, 1.0f), 1,
        "collision_box_edge: obere Umrandung trifft");

    /* Mitten im Kasten liegt KEINE Kante: die Umrandungen sitzen um den
       Grundriss herum, nicht darin. */
    pos = (RECVX_ENEMY_VEC){ 5.0f, 5.0f, 5.0f };
    oki(recvx_en03_collision_box_edge(&hp, &pos, 1.0f), 0,
        "collision_box_edge: Kasteninneres trifft keine Kante");

    /* Weit weg: 0. */
    pos = (RECVX_ENEMY_VEC){ 100.0f, 100.0f, 100.0f };
    oki(recvx_en03_collision_box_edge(&hp, &pos, 1.0f), 0,
        "collision_box_edge: weit weg -> 0");

    /* Senkrechter Eckkasten an (px-r, pz-r) ueber die ganze Hoehe. */
    pos = (RECVX_ENEMY_VEC){ -1.0f, 5.0f, -1.0f };
    oki(recvx_en03_collision_box_edge(&hp, &pos, 1.0f), 1,
        "collision_box_edge: senkrechte Ecke trifft");

    /* type 7 vertauscht h1 und h2 (Quirk); die Routine bleibt lauffaehig und
       trifft an der UNTEREN Kante py. */
    hp.type = 7u;
    pos = (RECVX_ENEMY_VEC){ 5.0f, 0.5f, -1.0f };
    oki(recvx_en03_collision_box_edge(&hp, &pos, 1.0f), 1,
        "collision_box_edge: type 7 trifft an der unteren Kante");
}

static void test_collision_run(void)
{
    RECVX_ENEMY_WORK w;
    int idx = -99;

    /* en03.c:5220: Reihenfolge rom->walp dann sys->mwalp; die LETZTE
       Trefferflaeche gewinnt. */
    setup(&w);
    walls[0] = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    walls[1] = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    wall_n = 2;
    w.px = 5.0f; w.py = 5.0f; w.pz = 5.0f;
    w.ar = 1.0f;

    oki(recvx_en03_collision(&w, &idx), 1, "collision: Treffer gemeldet");
    oki(idx, 1, "collision: die LETZTE Trefferflaeche gewinnt");

    /* Collision2 laesst genau eine Flaeche aus. */
    idx = -99;
    oki(recvx_en03_collision2(&w, 1, &idx), 1, "collision2: Treffer gemeldet");
    oki(idx, 0, "collision2: die ausgeschlossene Flaeche zaehlt nicht");

    idx = -99;
    oki(recvx_en03_collision2(&w, 0, &idx), 1, "collision2: andere Flaeche ausgeschlossen");
    oki(idx, 1, "collision2: Index der verbleibenden Flaeche");

    /* flg & 1 der Flaeche ist das Gate. */
    setup(&w);
    walls[0] = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    walls[0].flg = 0u;
    wall_n = 1;
    w.px = 5.0f; w.py = 5.0f; w.pz = 5.0f; w.ar = 1.0f;
    oki(recvx_en03_collision(&w, &idx), 0, "collision: Flaeche ohne flg 1 wird ausgelassen");

    /* Ungerader Typ plus Workflag 0x400 laesst die Flaeche aus; Typ 6 und 7
       kennen dieses Gate nicht. */
    setup(&w);
    walls[0] = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    walls[0].type = 1u;
    wall_n = 1;
    w.px = 5.0f; w.py = 5.0f; w.pz = 5.0f; w.ar = 1.0f;
    oki(recvx_en03_collision(&w, &idx), 1, "collision: Typ 1 ohne flg 0x400 trifft");

    w.flg |= 0x400u;
    w.px = 5.0f; w.py = 5.0f; w.pz = 5.0f;
    oki(recvx_en03_collision(&w, &idx), 0, "collision: Typ 1 mit flg 0x400 ausgelassen");

    walls[0].type = 0u;
    w.px = 5.0f; w.py = 5.0f; w.pz = 5.0f;
    oki(recvx_en03_collision(&w, &idx), 1, "collision: Typ 0 trifft auch mit flg 0x400");

    /* Die Lage des Works wird wirklich verschoben. */
    setup(&w);
    walls[0] = box_atr(0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f);
    wall_n = 1;
    w.px = 0.5f; w.py = 5.0f; w.pz = 5.0f; w.ar = 1.0f;
    oki(recvx_en03_collision(&w, &idx), 1, "collision: Randlage trifft");
    okf(w.px, -1.0f, 1e-5f, "collision schiebt die Lage des Works hinaus");

    /* Ohne gebundene Flaechenliste: Luecke und 0, keine Ersatzflaeche. */
    setup(&w);
    ext.wal_count = NULL;
    ext.wal_at = NULL;
    gap_count = 0;
    w.px = 1.0f;
    oki(recvx_en03_collision(&w, &idx), 0, "collision ohne Flaechenliste -> 0");
    oki(idx, -1, "collision ohne Flaechenliste: kein Index");
    ok(gap_seen("en03.c:5222"), "collision meldet die fehlende Flaechenliste");
    okf(w.px, 1.0f, 1e-6f, "collision ohne Flaechenliste laesst die Lage stehen");
}

int main(void)
{
    _Make_SinTable();

    test_geometry_primitives();
    test_wall_box();
    test_wall_ground();
    test_wall_cylinder();
    test_wall_slope();
    test_wall_triangle();
    test_box_edge();
    test_collision_run();

    printf("%d Pruefungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
