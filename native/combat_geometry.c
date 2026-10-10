#include "combat_geometry.h"
#include "ninja_matrix.h"
#include <math.h>
/* Eigenstaendige Bestimmung, Fundstellen in combat_geometry.h. Vektorgroesse,
   Normierung und Skalarprodukt laufen ueber den vorhandenen nativen
   Ninja-Port (njScalor/njUnitVector/njInnerProduct), damit Rundung und
   Nullvektorverhalten dieselben sind wie in den uebrigen Ports. */

static float scalor(RECVX_CB_VEC v) { return njScalor((NJS_VECTOR*)&v); }
static void unit(RECVX_CB_VEC* v) { njUnitVector((NJS_VECTOR*)v); }
static float inner(RECVX_CB_VEC a, RECVX_CB_VEC b) { return njInnerProduct((NJS_VECTOR*)&a, (NJS_VECTOR*)&b); }
static RECVX_CB_VEC line_dir(const RECVX_CB_LINE* l) { RECVX_CB_VEC v = { l->vx, l->vy, l->vz }; return v; }
static RECVX_CB_VEC line_org(const RECVX_CB_LINE* l) { RECVX_CB_VEC v = { l->px, l->py, l->pz }; return v; }
static RECVX_CB_VEC sub(RECVX_CB_VEC a, RECVX_CB_VEC b) { RECVX_CB_VEC v = { a.x - b.x, a.y - b.y, a.z - b.z }; return v; }

/* ps2_NaColi.c:77 */
float recvx_cb_distance_p2l(const RECVX_CB_VEC* p, const RECVX_CB_LINE* l, RECVX_CB_VEC* cp)
{
    float dx = l->vx, dy = l->vy, dz = l->vz;
    float dl = njInvertSqrt(dx * dx + dy * dy + dz * dz);
    float lx, ly, lz, ll;
    dx *= dl; dy *= dl; dz *= dl;
    lx = p->x - l->px; ly = p->y - l->py; lz = p->z - l->pz;
    ll = lx * lx + ly * ly + lz * lz;
    if (ll == 0) {
        if (cp) { cp->x = l->px; cp->y = l->py; cp->z = l->pz; }
        return 0;
    }
    dl = dx * lx + dy * ly + dz * lz;
    if (cp) { cp->x = l->px + dx * dl; cp->y = l->py + dy * dl; cp->z = l->pz + dz * dl; }
    return njSqrt(ll - dl * dl);
}

/* ps2_NaColi.c:153 */
float recvx_cb_distance_l2l(const RECVX_CB_LINE* l1, const RECVX_CB_LINE* l2, RECVX_CB_VEC* cp1, RECVX_CB_VEC* cp2)
{
    float v1x = l1->vx, v1y = l1->vy, v1z = l1->vz;
    float t = njInvertSqrt(v1x * v1x + v1y * v1y + v1z * v1z);
    float v2x = l2->vx, v2y = l2->vy, v2z = l2->vz;
    float iv, nx, ny, nz, dx, dy, dz;
    v1x *= t; v1y *= t; v1z *= t;
    t = njInvertSqrt(v2x * v2x + v2y * v2y + v2z * v2z);
    v2x *= t; v2y *= t; v2z *= t;
    iv = v1x * v2x + v1y * v2y + v1z * v2z;
    if (0.975f < fabsf(iv)) {
        RECVX_CB_VEC o = line_org(l1);
        if (cp1) *cp1 = o;
        return recvx_cb_distance_p2l(&o, l2, cp2);
    }
    dx = l2->px - l1->px; dy = l2->py - l1->py; dz = l2->pz - l1->pz;
    if (cp1 || cp2) {
        nx = dx * v1x + dy * v1y + dz * v1z;
        ny = dx * v2x + dy * v2y + dz * v2z;
        nz = 1.0f - iv * iv;
        if (cp1) {
            t = (nx - iv * ny) / nz;
            cp1->x = l1->px + v1x * t; cp1->y = l1->py + v1y * t; cp1->z = l1->pz + v1z * t;
        }
        if (cp2) {
            t = (nx * iv - ny) / nz;
            cp2->x = l2->px + v2x * t; cp2->y = l2->py + v2y * t; cp2->z = l2->pz + v2z * t;
        }
    }
    nx = v1y * v2z - v1z * v2y;
    ny = v1z * v2x - v1x * v2z;
    nz = v1x * v2y - v1y * v2x;
    t = fabsf(nx * dx + ny * dy + nz * dz);
    v1x = njInvertSqrt(nx * nx + ny * ny + nz * nz);
    iv = t * v1x;
    if (iv < 0.158f) iv = 0;
    return iv;
}

/* ps2_NaColi.c:322 */
int recvx_cb_check_ss(const RECVX_CB_SPHERE* a, const RECVX_CB_SPHERE* b)
{
    float dx = b->c.x - a->c.x, dy = b->c.y - a->c.y, dz = b->c.z - a->c.z, r = a->r + b->r;
    return (dx * dx + dy * dy + dz * dz) <= (r * r) ? 1 : 0;
}

/* Endpunktklemme, die npCollisionCheckCC/CCEx dreimal benutzt: liegt der
   Fusspunkt hinter dem Segmentanfang, gilt c1; liegt er jenseits der Laenge,
   gilt c2. `normalize_dir` bildet die Stellen nach, an denen das Original
   die Richtung im selben Schritt normiert. */
static void clamp_segment(RECVX_CB_VEC* point, const RECVX_CB_LINE* ln, RECVX_CB_VEC* dir, float len,
                          const RECVX_CB_CAPSULE* cap, int normalize_dir)
{
    RECVX_CB_VEC v = sub(*point, line_org(ln));
    float sca = scalor(v), inn;
    unit(&v);
    if (normalize_dir) unit(dir);
    inn = inner(v, *dir);
    if (inn > 0) {
        if (sca > len) *point = cap->c2;
    } else {
        *point = cap->c1;
    }
}

/* Gemeinsamer Ablauf von npCollisionCheckCC (njplus.c:33) und CCEx (:234).
   `pos` NULL waehlt CC. */
static void contact_point(const RECVX_CB_SPHERE* sa, const RECVX_CB_SPHERE* sb, RECVX_CB_VEC* pos)
{
    RECVX_CB_VEC scl = sub(sb->c, sa->c);
    if (scalor(scl) > sa->r) {
        unit(&scl);
        pos->x = sa->c.x + scl.x * sa->r;
        pos->y = sa->c.y + scl.y * sa->r;
        pos->z = sa->c.z + scl.z * sa->r;
    } else {
        *pos = sb->c;
    }
}
static int capsule_capsule(const RECVX_CB_CAPSULE* cpa, const RECVX_CB_CAPSULE* cpb, RECVX_CB_VEC* pos)
{
    RECVX_CB_LINE lna, lnb, lnc;
    const RECVX_CB_CAPSULE* capa;
    const RECVX_CB_CAPSULE* capb;
    RECVX_CB_SPHERE sa, sb;
    RECVX_CB_VEC da, db;
    float lena, lenb, tmp;
    lna.px = cpa->c1.x; lna.py = cpa->c1.y; lna.pz = cpa->c1.z;
    lna.vx = cpa->c2.x - lna.px; lna.vy = cpa->c2.y - lna.py; lna.vz = cpa->c2.z - lna.pz;
    lnb.px = cpb->c1.x; lnb.py = cpb->c1.y; lnb.pz = cpb->c1.z;
    lnb.vx = cpb->c2.x - lnb.px; lnb.vy = cpb->c2.y - lnb.py; lnb.vz = cpb->c2.z - lnb.pz;
    lena = scalor(line_dir(&lna));
    lenb = scalor(line_dir(&lnb));
    if (lena > lenb) {
        capa = cpb; capb = cpa;
        lnc = lna; lna = lnb; lnb = lnc;
        tmp = lena; lena = lenb; lenb = tmp;
    } else {
        capa = cpa; capb = cpb;
    }
    recvx_cb_distance_l2l(&lna, &lnb, &sa.c, &sb.c);
    sa.r = capa->r;
    sb.r = capb->r;
    /* Die Originalroutine normiert lna.v/lnb.v beim ersten Klemmen dauerhaft;
       die spaeteren Klemmen lesen die bereits normierte Richtung. */
    da = line_dir(&lna);
    db = line_dir(&lnb);
    clamp_segment(&sa.c, &lna, &da, lena, capa, 1);
    clamp_segment(&sb.c, &lnb, &db, lenb, capb, 1);
    lna.vx = da.x; lna.vy = da.y; lna.vz = da.z;
    lnb.vx = db.x; lnb.vy = db.y; lnb.vz = db.z;
    if (recvx_cb_check_ss(&sa, &sb)) { if (pos) contact_point(&sa, &sb, pos); return 1; }
    sa.c = capa->c1;
    recvx_cb_distance_p2l(&sa.c, &lnb, &sb.c);
    clamp_segment(&sb.c, &lnb, &db, lenb, capb, 0);
    if (recvx_cb_check_ss(&sa, &sb)) { if (pos) contact_point(&sa, &sb, pos); return 1; }
    sa.c = capa->c2;
    recvx_cb_distance_p2l(&sa.c, &lnb, &sb.c);
    clamp_segment(&sb.c, &lnb, &db, lenb, capb, 0);
    if (recvx_cb_check_ss(&sa, &sb)) { if (pos) contact_point(&sa, &sb, pos); return 1; }
    return 0;
}
int recvx_cb_check_cc(const RECVX_CB_CAPSULE* a, const RECVX_CB_CAPSULE* b) { return capsule_capsule(a, b, 0); }
int recvx_cb_check_ccex(const RECVX_CB_CAPSULE* a, const RECVX_CB_CAPSULE* b, RECVX_CB_VEC* pos) { return capsule_capsule(a, b, pos); }

/* njplus.c:498 */
int recvx_cb_check_sc(const RECVX_CB_SPHERE* sa, const RECVX_CB_CAPSULE* cpb)
{
    RECVX_CB_LINE lnb;
    RECVX_CB_SPHERE sb;
    RECVX_CB_VEC vec, dir;
    float lr = sa->r + cpb->r, lenb, sca, inn;
    lnb.px = cpb->c1.x; lnb.py = cpb->c1.y; lnb.pz = cpb->c1.z;
    lnb.vx = cpb->c2.x - lnb.px; lnb.vy = cpb->c2.y - lnb.py; lnb.vz = cpb->c2.z - lnb.pz;
    recvx_cb_distance_p2l(&sa->c, &lnb, &sb.c);
    sb.r = cpb->r;
    dir = line_dir(&lnb);
    lenb = scalor(dir);
    vec = sub(sb.c, line_org(&lnb));
    sca = scalor(vec);
    unit(&vec);
    unit(&dir);
    inn = inner(vec, dir);
    if (inn > 0) {
        if (sca > lenb + lr) return 0;
        if (sca > lenb) sb.c = cpb->c2;
    } else {
        if (sca > lr) return 0;
        sb.c = cpb->c1;
    }
    return recvx_cb_check_ss(sa, &sb) ? 1 : 0;
}

/* njplus.c:565 */
void recvx_cb_distance_p2c(const RECVX_CB_VEC* pos, const RECVX_CB_CAPSULE* cap, RECVX_CB_VEC* htp)
{
    RECVX_CB_LINE ln;
    RECVX_CB_VEC vec, dir;
    float len, sca, inn;
    ln.px = cap->c1.x; ln.py = cap->c1.y; ln.pz = cap->c1.z;
    ln.vx = cap->c2.x - ln.px; ln.vy = cap->c2.y - ln.py; ln.vz = cap->c2.z - ln.pz;
    recvx_cb_distance_p2l(pos, &ln, htp);
    dir = line_dir(&ln);
    len = scalor(dir);
    vec = sub(*htp, line_org(&ln));
    sca = scalor(vec);
    unit(&vec);
    unit(&dir);
    inn = inner(vec, dir);
    if (inn > 0) {
        if (sca > len) *htp = cap->c2;
    } else {
        *htp = cap->c1;
    }
    vec = sub(*pos, *htp);
    if (cap->r < scalor(vec)) {
        unit(&vec);
        htp->x += vec.x * cap->r; htp->y += vec.y * cap->r; htp->z += vec.z * cap->r;
    } else {
        *htp = *pos;
    }
}
