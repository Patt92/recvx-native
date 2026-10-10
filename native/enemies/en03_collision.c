/* EN03-Kollisionsfamilie — Koerper zu native/en03_collision.h.

   Eigenstaendige native Bestimmung aus der Semantik von en03.c:5203-6072
   (GetWall, Collision, Collision2, CollisionWallBox/Ground/Cylinder/Slope/
   Triangle, CollisionBoxEdge) und der Hilfsgeometrie ps2_NaColi.c:65/77/125/
   301 sowie hitchkl.c:682. Kein Originalkoerper, kein Generator, keine
   zweite Workwelt; die Flaechen kommen ueber RECVX_ENEMY_EXT.wal_at aus dem
   vorhandenen Besitzer.

   Alle Formen verschieben die uebergebene Lage DIREKT (das Original uebergibt
   dafuer (NJS_POINT3*)&epw->px) und melden nur, ob die Flaeche gegriffen hat.
   Mehrere Originalquirks bleiben erhalten und sind einzeln kommentiert. */

#include "en03_collision.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <math.h>
#include <string.h>

/* ---------------------------------------------------------------------------
   Raumhoehe: alle Wandformen setzen bei h == 0 die Raumhoehe rom->h ein
   (en03.c:5418 und die uebrigen Formen). Ohne gebundene Quelle wird die
   Luecke gemeldet und 0 weiterverwendet — das ist dann genau der Zustand,
   in dem die Flaeche keine Hoehe hat, und kein erfundener Ersatzwert.
   --------------------------------------------------------------------------- */
static float en03_room_height(void)
{
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    if (ext && ext->room_height) return ext->room_height();
    recvx_t3_gap("en03.c:5418 rom->h (Raumhoehe)");
    return 0.0f;
}

static float en03_height_of(const RECVX_T3_ATR* hp)
{
    return (hp->h == 0.0f) ? en03_room_height() : hp->h;
}

/* ---------------------------------------------------------------------------
   Hilfsgeometrie
   --------------------------------------------------------------------------- */

/* ps2_NaColi.c:65 njDistanceP2P. */
float recvx_en03_distance_p2p(const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b)
{
    const float dx = b->x - a->x;
    const float dy = b->y - a->y;
    const float dz = b->z - a->z;
    return njSqrt((dx * dx) + (dy * dy) + (dz * dz));
}

/* ps2_NaColi.c:77 njDistanceP2L. Die Richtung wird ueber den Kehrwert der
   Wurzel normalisiert; `cp` ist der Fusspunkt auf der GERADEN (nicht auf dem
   Abschnitt). Liegt der Punkt genau auf dem Linienursprung, ist das Ergebnis
   0 und `cp` der Ursprung. */
float recvx_en03_distance_p2l(const RECVX_ENEMY_VEC* p, const RECVX_EN03_LINE* l,
                              RECVX_ENEMY_VEC* cp)
{
    float dx = l->v.x, dy = l->v.y, dz = l->v.z;
    float dl = njInvertSqrt((dx * dx) + (dy * dy) + (dz * dz));
    float lx, ly, lz, ll;

    dx *= dl; dy *= dl; dz *= dl;

    lx = p->x - l->p.x;
    ly = p->y - l->p.y;
    lz = p->z - l->p.z;

    ll = (lx * lx) + (ly * ly) + (lz * lz);

    if (ll == 0.0f) {
        if (cp) *cp = l->p;
        return 0.0f;
    }

    dl = (dx * lx) + (dy * ly) + (dz * lz);

    if (cp) {
        cp->x = l->p.x + (dx * dl);
        cp->y = l->p.y + (dy * dl);
        cp->z = l->p.z + (dz * dl);
    }

    return njSqrt(ll - (dl * dl));
}

/* ps2_NaColi.c:125 njDistanceP2PL — vorzeichenloser Abstand zur Ebene,
   `cp` ist der Lotfusspunkt. */
float recvx_en03_distance_p2pl(const RECVX_ENEMY_VEC* p, const RECVX_EN03_PLANE* pl,
                               RECVX_ENEMY_VEC* cp)
{
    float vx = pl->v.x, vy = pl->v.y, vz = pl->v.z;
    float len = njInvertSqrt((vx * vx) + (vy * vy) + (vz * vz));

    vx *= len; vy *= len; vz *= len;

    len = (vx * (p->x - pl->p.x)) + (vy * (p->y - pl->p.y)) + (vz * (p->z - pl->p.z));

    if (cp) {
        cp->x = p->x - (vx * len);
        cp->y = p->y - (vy * len);
        cp->z = p->z - (vz * len);
    }

    return fabsf(len);
}

/* ps2_NaColi.c:301 njGetPlaneNormal — Kreuzprodukt ueber die ersten DREI
   Punkte, ohne Normierung. */
void recvx_en03_plane_normal(const RECVX_ENEMY_VEC* area, RECVX_ENEMY_VEC* out)
{
    const RECVX_ENEMY_VEC* p1 = &area[0];
    const RECVX_ENEMY_VEC* p2 = &area[1];
    const RECVX_ENEMY_VEC* p3 = &area[2];

    out->x = ((p2->y - p1->y) * (p3->z - p2->z)) - ((p2->z - p1->z) * (p3->y - p2->y));
    out->y = ((p2->z - p1->z) * (p3->x - p2->x)) - ((p2->x - p1->x) * (p3->z - p2->z));
    out->z = ((p2->x - p1->x) * (p3->y - p2->y)) - ((p2->y - p1->y) * (p3->x - p2->x));
}

/* hitchkl.c:682 bhInOutCheck — der Punkt liegt innen, wenn er auf keiner
   Kante die falsche Seite hat. Die Summationsreihenfolge des Originals ist
   z + (x + y); sie bleibt erhalten, weil sie das Rundungsergebnis an der
   Kante bestimmt. Weniger als drei Punkte sind immer aussen. */
int recvx_en03_in_out_check(const RECVX_ENEMY_VEC* p, const RECVX_ENEMY_VEC* area,
                            const RECVX_ENEMY_VEC* normal, int num)
{
    RECVX_ENEMY_VEC n;
    int i;

    if (num < 3) return 0;

    if (normal) n = *normal;
    else        recvx_en03_plane_normal(area, &n);

    for (i = 0; i < num; i++) {
        const RECVX_ENEMY_VEC* p2 = &area[i];
        const RECVX_ENEMY_VEC* p1 = &area[(i == num - 1) ? 0 : (i + 1)];

        const float v1x = p1->x - p2->x;
        const float v1y = p1->y - p2->y;
        const float v1z = p1->z - p2->z;

        const float v2x = p->x - p2->x;
        const float v2y = p->y - p2->y;
        const float v2z = p->z - p2->z;

        const float side = (n.z * ((v1x * v2y) - (v1y * v2x))) +
                           ((n.x * ((v1y * v2z) - (v1z * v2y))) +
                            (n.y * ((v1z * v2x) - (v1x * v2z))));

        if (side < 0.0f) return 0;
    }

    return 1;
}

/* ---------------------------------------------------------------------------
   en03.c:5399 bhEne03_CollisionWallBox
   ---------------------------------------------------------------------------
   Erst wird geprueft, ob die Lage im um `ar` aufgeweiteten Kasten liegt.
   Danach waehlen die drei Mittenvergleiche je Achse die NAEHERE Seitenflaeche:
   `d*` wird auf deren aufgeweitete Ebene und `p*` auf die Kante selbst gezogen.
   Die Abstaende ax/ay/az zu diesen Ebenen entscheiden, ob der Punkt an einer
   Ecke (drei Achsen), einer Kante (zwei Achsen) oder einer Flaeche haengt.

   QUIRK 1: die Routine gibt 1 zurueck, SOBALD der Punkt im aufgeweiteten
   Kasten liegt — auch wenn keiner der Zweige ihn verschiebt (Kanten-/
   Eckzweige verschieben nur bei len < ar, der Flaechenzweig nur bei echter
   Ungleichheit der Abstaende).
   QUIRK 2: der Flaechenzweig besteht aus DREI unabhaengigen Abfragen, nicht
   aus einer Auswahl; bei genau gleichen Abstaenden greift keine davon.
   QUIRK 3: Division durch len ohne Schutz. Bei len == 0 (Punkt genau auf der
   Kante) entsteht wie im Original Unendlich beziehungsweise NaN.
   --------------------------------------------------------------------------- */
int recvx_en03_wall_box(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar)
{
    const float h = en03_height_of(hp);
    float px = hp->px, py = hp->py, pz = hp->pz;
    float dx = px - ar, dy = py - ar, dz = pz - ar;
    const float cx = px + (hp->w / 2.0f);
    const float cy = py + (h / 2.0f);
    const float cz = pz + (hp->d / 2.0f);
    float ax, ay, az, len;

    if (!((pos->x > dx) && (pos->x < (ar + (px + hp->w))) &&
          (pos->y > dy) && (pos->y < (ar + (py + h))) &&
          (pos->z > dz) && (pos->z < (ar + (pz + hp->d)))))
        return 0;

    if (pos->x > cx) { dx = ar + (px + hp->w); px += hp->w; }
    if (pos->z > cz) { dz = ar + (pz + hp->d); pz += hp->d; }
    if (pos->y > cy) { dy = ar + (py + h);     py += h;     }

    ax = fabsf(pos->x - dx);
    ay = fabsf(pos->y - dy);
    az = fabsf(pos->z - dz);

    if ((ax < ar) && (ay < ar) && (az < ar)) {
        dx = pos->x - px;
        dy = pos->y - py;
        dz = pos->z - pz;
        len = njSqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (len < ar) {
            pos->x = px + ((dx * ar) / len);
            pos->y = py + ((dy * ar) / len);
            pos->z = pz + ((dz * ar) / len);
        }
    } else if ((ay < ar) && (az < ar)) {
        dy = pos->y - py;
        dz = pos->z - pz;
        len = njSqrt((dy * dy) + (dz * dz));
        if (len < ar) {
            pos->y = py + ((dy * ar) / len);
            pos->z = pz + ((dz * ar) / len);
        }
    } else if ((ax < ar) && (az < ar)) {
        dx = pos->x - px;
        dz = pos->z - pz;
        len = njSqrt((dx * dx) + (dz * dz));
        if (len < ar) {
            pos->x = px + ((dx * ar) / len);
            pos->z = pz + ((dz * ar) / len);
        }
    } else if ((ax < ar) && (ay < ar)) {
        dx = pos->x - px;
        dy = pos->y - py;
        len = njSqrt((dx * dx) + (dy * dy));
        if (len < ar) {
            pos->x = px + ((dx * ar) / len);
            pos->y = py + ((dy * ar) / len);
        }
    } else {
        if ((ay > ax) && (az > ax)) pos->x = dx;
        if ((ax > ay) && (az > ay)) pos->y = dy;
        if ((ax > az) && (ay > az)) pos->z = dz;
    }

    return 1;
}

/* en03.c:5528 bhEne03_CollisionWallGround — eine Bodenflaeche wird als Kasten
   mit UMGEKEHRTER Hoehe von der Oberkante aus geprueft. Ohne eigene Hoehe
   bekommt sie 0.01 und wird um ebenso viel abgesenkt; damit greift im Kasten
   die h-==-0-Ersetzung durch die Raumhoehe gerade NICHT. */
int recvx_en03_wall_ground(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar)
{
    RECVX_T3_ATR hp2 = *hp;

    hp2.type = 0;
    hp2.py = hp->py + hp->h;

    if (hp->h == 0.0f) {
        hp2.h = 0.01f;
        hp2.py -= 0.01f;
    } else {
        hp2.h = -hp->h;
    }

    return recvx_en03_wall_box(&hp2, pos, ar);
}

/* en03.c:5549 bhEne03_CollisionWallCylinder — zwei Faelle: oberhalb der
   Grundflaeche und unterhalb der Deckelhoehe wird radial auf w+ar
   hinausgeschoben; nahe der Deckelhoehe schrumpft der wirksame Radius auf
   die Kugelsehne sqrt(ar^2 - dy^2).
   QUIRK: `ln` wird ohne Schutz als Divisor benutzt (Mittelachse). */
int recvx_en03_wall_cylinder(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar)
{
    const float h = en03_height_of(hp);
    const float dx = hp->px - pos->x;
    const float dy = hp->py + h - pos->y;
    const float dz = hp->pz - pos->z;
    const float ln = njSqrt((dx * dx) + (dz * dz));
    float radius = hp->w + ar;

    if ((dy >= 0.0f) && (hp->py < pos->y) && (ln < radius)) {
        pos->x = hp->px - ((dx * radius) / ln);
        pos->z = hp->pz - ((dz * radius) / ln);
        return 1;
    }

    if (fabsf(dy) < ar) {
        radius = njSqrt((ar * ar) - (dy * dy));
        if (ln < (hp->w + radius)) {
            radius += hp->w;
            pos->x = hp->px - ((dx * radius) / ln);
            pos->z = hp->pz - ((dz * radius) / ln);
            return 1;
        }
    }

    return 0;
}

/* en03.c:5594 bhEne03_CollisionWallSlope — eine Schraege ueber dem Grundriss;
   `id` waehlt, welche zwei Ecken auf Hoehe py+h liegen. Der Lotfusspunkt
   entscheidet, ob auf die FLAECHE, auf eine RANDKANTE oder auf eine ECKE
   hinausgeschoben wird. */
int recvx_en03_wall_slope(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar)
{
    const float h = en03_height_of(hp);
    RECVX_ENEMY_VEC area[4], cp;
    RECVX_EN03_PLANE pl;
    float len;
    int xf0, xf1, zf0, zf1;

    if (!(pos->x > hp->px - ar)) return 0;
    if (!(pos->x < hp->px + hp->w + ar &&
          pos->z > hp->pz - ar &&
          pos->z < hp->pz + hp->d + ar &&
          pos->y > hp->py - ar &&
          pos->y < hp->py + h + ar))
        return 0;

    area[0].x = hp->px;           area[0].y = hp->py; area[0].z = hp->pz;
    area[1].x = hp->px;           area[1].y = hp->py; area[1].z = hp->pz + hp->d;
    area[2].x = hp->px + hp->w;   area[2].y = hp->py; area[2].z = hp->pz + hp->d;
    area[3].x = hp->px + hp->w;   area[3].y = hp->py; area[3].z = hp->pz;

    switch (hp->id) {
    case 0: area[2].y = area[3].y = hp->py + h; break;
    case 1: area[0].y = area[3].y = hp->py + h; break;
    case 2: area[1].y = area[2].y = hp->py + h; break;
    case 3: area[0].y = area[1].y = hp->py + h; break;
    default:
        /* Das Original hat keinen weiteren Zweig: die Flaeche bleibt flach
           auf py. Das bleibt so und wird nicht geraten. */
        break;
    }

    recvx_en03_plane_normal(area, &pl.v);
    pl.p = area[0];

    len = recvx_en03_distance_p2pl(pos, &pl, &cp);
    if (!(len < ar)) return 0;

    xf0 = xf1 = zf0 = zf1 = 0;
    if (cp.x < hp->px)             xf0 = 1;
    if (cp.x > (hp->px + hp->w))   xf1 = 1;
    if (cp.z < hp->pz)             zf0 = 1;
    if (cp.z > (hp->pz + hp->d))   zf1 = 1;

    if (!xf0 && !zf0 && !xf1 && !zf1) {
        NJS_VECTOR n;
        n.x = pl.v.x; n.y = pl.v.y; n.z = pl.v.z;
        njUnitVector(&n);
        pos->x = cp.x + n.x * ar;
        pos->y = cp.y + n.y * ar;
        pos->z = cp.z + n.z * ar;
        return 1;
    }

    if (xf0 + xf1 + zf0 + zf1 == 1) {
        RECVX_EN03_LINE ln;
        int idx, idx2;

        if (xf0)      { idx = 0; idx2 = 1; }
        else if (xf1) { idx = 2; idx2 = 3; }
        else if (zf0) { idx = 0; idx2 = 3; }
        else          { idx = 1; idx2 = 2; }

        ln.p = area[idx];
        ln.v.x = area[idx2].x - ln.p.x;
        ln.v.y = area[idx2].y - ln.p.y;
        ln.v.z = area[idx2].z - ln.p.z;

        len = recvx_en03_distance_p2l(pos, &ln, &cp);
        if (len < ar) {
            pos->x = cp.x - ar * (cp.x - pos->x) / len;
            pos->y = cp.y - ar * (cp.y - pos->y) / len;
            pos->z = cp.z - ar * (cp.z - pos->z) / len;
            return 1;
        }
    }

    if (xf0 + xf1 + zf0 + zf1 == 2) {
        int idx = -1;

        if (xf0 && zf0) idx = 0;
        if (xf0 && zf1) idx = 1;
        if (xf1 && zf1) idx = 2;
        if (xf1 && zf0) idx = 3;

        if (idx < 0) {
            /* Zwei gesetzte Marken, die KEIN x/z-Paar bilden (also xf0 und
               xf1 oder zf0 und zf1 gleichzeitig; moeglich bei negativer
               Breite/Tiefe). Das Original liest dort einen ungesetzten
               Index und greift neben die Eckliste. Es wird deshalb kein
               Index geraten, sondern die Luecke gemeldet. */
            recvx_t3_gap("en03.c:5739 Eckindex der Schraege ungesetzt");
            return 0;
        }

        len = recvx_en03_distance_p2p(&area[idx], pos);
        if (len < ar) {
            pos->x = area[idx].x - ar * (area[idx].x - pos->x) / len;
            pos->y = area[idx].y - ar * (area[idx].y - pos->y) / len;
            pos->z = area[idx].z - ar * (area[idx].z - pos->z) / len;
            return 1;
        }
    }

    return 0;
}

/* en03.c:5770 bhEne03_CollisionWallTriangle — ein rechtwinkliges Dreieck im
   Grundriss, `id` legt die Lage des rechten Winkels fest. Innerhalb des um
   `ar` verbreiterten Hypotenusenbandes wird auf die Hypotenuse gezogen,
   ausserhalb uebernimmt der Kasten an der passenden Ecke.

   QUIRK: das `return 1` steht NACH dem innersten Block. Liegt die Lage im
   y-Band von py-ar..py+h, aber nicht im engeren Band py..py+h, meldet die
   Routine trotzdem einen Treffer OHNE zu verschieben. Auch ein erfolgloser
   Kastenzweig endet in diesem `return 1`. */
int recvx_en03_wall_triangle(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar)
{
    const float h = en03_height_of(hp);
    RECVX_ENEMY_VEC area[4], pd, vd;
    NJS_VECTOR nvd;
    float x1, x2, z1, z2, t, px;

    if (!(hp->py <= pos->y - ar)) return 0;
    if (!(hp->py + h >= pos->y - ar)) return 0;

    if ((hp->py <= pos->y) && (hp->py + h >= pos->y)) {
        if (hp->w > 0.0f) { x1 = hp->px - ar;           x2 = hp->px + hp->w + ar; }
        else              { x1 = hp->px + hp->w - ar;   x2 = hp->px + ar;         }

        if (hp->d > 0.0f) { z1 = hp->pz - ar;           z2 = hp->pz + hp->d + ar; }
        else              { z1 = hp->pz + hp->d - ar;   z2 = hp->pz + ar;         }

        if (x1 > pos->x || x2 <= pos->x || z1 > pos->z || z2 <= pos->z) return 0;

        if (hp->id == 0u || hp->id == 3u) { vd.x =  hp->d; vd.z =  hp->w; }
        else                              { vd.x = -hp->d; vd.z = -hp->w; }
        vd.y = 0.0f;

        nvd.x = vd.x; nvd.y = vd.y; nvd.z = vd.z;
        njUnitVector(&nvd);
        vd.x = nvd.x * ar;
        vd.y = nvd.y;
        vd.z = nvd.z * ar;

        area[0].x = hp->px + hp->w - vd.x;
        area[0].y = 0.0f;
        area[0].z = hp->pz - vd.z;

        area[1].x = hp->px - vd.x;
        area[1].y = 0.0f;
        area[1].z = hp->pz + hp->d - vd.z;

        area[2].x = area[1].x + 2.0f * vd.x;
        area[2].y = 0.0f;
        area[2].z = area[1].z + 2.0f * vd.z;

        area[3].x = area[0].x + 2.0f * vd.x;
        area[3].y = 0.0f;
        area[3].z = area[0].z + 2.0f * vd.z;

        if (recvx_en03_in_out_check(pos, area, NULL, 4)) {
            RECVX_EN03_LINE ln;
            ln.p.x = area[2].x; ln.p.y = 0.0f; ln.p.z = area[2].z;
            ln.v.x = hp->w;     ln.v.y = 0.0f; ln.v.z = -hp->d;

            (void)recvx_en03_distance_p2l(pos, &ln, &pd);

            pos->x = pd.x;
            pos->z = pd.z;
        } else {
            RECVX_T3_ATR bhp;

            px = pos->x - hp->px;
            t = hp->d + ((px * -hp->d) / hp->w);

            switch (hp->id) {
            case 0u:
            case 2u:
                if (t < pos->z - hp->pz) return 0;
                break;
            case 1u:
            case 3u:
                if (t > pos->z - hp->pz) return 0;
                break;
            default:
                break;
            }

            /* Das Original fuellt nur type/w/d/h/px/py/pz; flg und id bleiben
               ungesetzt und werden vom Kastenzweig auch nicht gelesen. Hier
               wird der ganze Satz genullt, damit kein ungesetzter Speicher
               gelesen wird — am Verhalten aendert das nichts. */
            memset(&bhp, 0, sizeof bhp);
            bhp.type = 0u;
            bhp.w = fabsf(hp->w);
            bhp.d = fabsf(hp->d);
            bhp.h = hp->h;

            switch (hp->id) {
            case 0u: bhp.px = hp->px;           bhp.py = hp->py; bhp.pz = hp->pz;           break;
            case 1u: bhp.px = hp->px;           bhp.py = hp->py; bhp.pz = hp->pz + hp->d;   break;
            case 2u: bhp.px = hp->px + hp->w;   bhp.py = hp->py; bhp.pz = hp->pz;           break;
            case 3u: bhp.px = hp->px + hp->w;   bhp.py = hp->py; bhp.pz = hp->pz + hp->d;   break;
            default: break;
            }

            if (recvx_en03_wall_box(&bhp, pos, ar)) return 1;
        }
    }

    return 1;
}

/* en03.c:5938 bhEne03_CollisionBoxEdge — zwoelf Kantenkaesten der Dicke 2*ar
   um eine Flaeche: vier an der oberen Umrandung, vier senkrecht in den Ecken
   und vier an der unteren Umrandung. Rueckgabe ist die ODER-Summe.

   QUIRK: fuer type 7 (Boden) sind h1 und h2 vertauscht (h1 ist die UNTERE
   Kante), wodurch die senkrechte Hoehe h1-h2 negativ wird. Das bleibt so. */
int recvx_en03_collision_box_edge(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar)
{
    const float r = ar * 2.0f;
    RECVX_T3_ATR ht;
    float h1, h2, hv;
    int ret = 0;

    if (hp->type == 7u) {
        h1 = hp->py;
        h2 = h1 + hp->h;
    } else {
        h1 = (hp->h != 0.0f) ? (hp->py + hp->h) : (hp->py + en03_room_height());
        h2 = hp->py;
    }

    memset(&ht, 0, sizeof ht);
    ht.type = 0u;

    /* Obere Umrandung: vorn, hinten, links, rechts. */
    ht.px = hp->px;      ht.py = h1; ht.pz = hp->pz - r;
    ht.w = hp->w;        ht.h = r;   ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px;      ht.py = h1; ht.pz = hp->pz + hp->d;
    ht.w = hp->w;        ht.h = r;   ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px - r;  ht.py = h1; ht.pz = hp->pz;
    ht.w = r;            ht.h = r;   ht.d = hp->d;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px + hp->w; ht.py = h1; ht.pz = hp->pz;
    ht.w = r;               ht.h = r;   ht.d = hp->d;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    /* Senkrechte Eckkaesten ueber die ganze Hoehe. */
    hv = h1 - h2;

    ht.px = hp->px - r;     ht.py = h2; ht.pz = hp->pz - r;
    ht.w = r;               ht.h = hv;  ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px + hp->w; ht.py = h2; ht.pz = hp->pz - r;
    ht.w = r;               ht.h = hv;  ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px - r;     ht.py = h2; ht.pz = hp->pz + hp->d;
    ht.w = r;               ht.h = hv;  ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px + hp->w; ht.py = h2; ht.pz = hp->pz + hp->d;
    ht.w = r;               ht.h = hv;  ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    /* Untere Umrandung, um r unter h2. */
    ht.type = 0u;
    h2 -= r;

    ht.px = hp->px;      ht.py = h2; ht.pz = hp->pz - r;
    ht.w = hp->w;        ht.h = r;   ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px;      ht.py = h2; ht.pz = hp->pz + hp->d;
    ht.w = hp->w;        ht.h = r;   ht.d = r;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px - r;  ht.py = h2; ht.pz = hp->pz;
    ht.w = r;            ht.h = r;   ht.d = hp->d;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    ht.px = hp->px + hp->w; ht.py = h2; ht.pz = hp->pz;
    ht.w = r;               ht.h = r;   ht.d = hp->d;
    ret |= recvx_en03_wall_box(&ht, pos, ar);

    return ret;
}

/* ---------------------------------------------------------------------------
   en03.c:5220/5310 — der gemeinsame Durchlauf
   ---------------------------------------------------------------------------
   Die Reihenfolge ist rom->walp[0..] gefolgt von sys->mwalp[0..]. Die
   LETZTE Trefferflaeche gewinnt (das Original ueberschreibt `ret` bei jedem
   Treffer). Flaechen mit ungeradem Typ werden fuer Works mit flg 0x400
   ausgelassen — das sind die Doppelseiten-Varianten. Typ 6 und 7 kennen
   dieses Gate nicht.
   --------------------------------------------------------------------------- */
static int en03_collision_run(RECVX_ENEMY_WORK* w, int exclude_index, int* hit_index)
{
    const RECVX_ENEMY_EXT* ext = recvx_enemy_ext();
    RECVX_ENEMY_VEC pos;
    int n, i, last = -1;

    if (hit_index) *hit_index = -1;

    if (!(ext && ext->wal_count && ext->wal_at)) {
        if (w && w->sv && w->sv->gap) w->sv->gap("en03.c:5222 rom->walp/sys->mwalp");
        else recvx_t3_gap("en03.c:5222 rom->walp/sys->mwalp");
        return 0;
    }

    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    n = ext->wal_count();

    for (i = 0; i < n; i++) {
        RECVX_T3_ATR hp;
        int hit = 0;

        if (!ext->wal_at((uint32_t)i, &hp)) break;
        if (!(hp.flg & 0x1u)) continue;
        if (i == exclude_index) continue;

        switch (hp.type) {
        case 0u:
        case 1u:
            if ((hp.type & 1u) && (w->flg & 0x400u)) continue;
            hit = recvx_en03_wall_box(&hp, &pos, w->ar);
            break;

        case 2u:
        case 3u:
            if ((hp.type & 1u) && (w->flg & 0x400u)) continue;
            hit = recvx_en03_wall_cylinder(&hp, &pos, w->ar);
            break;

        case 4u:
        case 5u:
            if ((hp.type & 1u) && (w->flg & 0x400u)) continue;
            hit = recvx_en03_wall_triangle(&hp, &pos, w->ar);
            break;

        case 6u:
            hit = recvx_en03_wall_slope(&hp, &pos, w->ar);
            break;

        case 7u:
            hit = recvx_en03_wall_ground(&hp, &pos, w->ar);
            break;

        default:
            /* Das Original hat keinen weiteren Zweig: hoehere Typen werden
               ausgelassen, ohne gemeldet zu werden. */
            break;
        }

        if (hit) last = i;
    }

    w->px = pos.x; w->py = pos.y; w->pz = pos.z;

    if (hit_index) *hit_index = last;
    return (last >= 0) ? 1 : 0;
}

int recvx_en03_collision(RECVX_ENEMY_WORK* w, int* hit_index)
{
    return en03_collision_run(w, -1, hit_index);
}

int recvx_en03_collision2(RECVX_ENEMY_WORK* w, int exclude_index, int* hit_index)
{
    return en03_collision_run(w, exclude_index, hit_index);
}
