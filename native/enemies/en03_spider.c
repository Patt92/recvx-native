/* Gemeinsame Spinnenbausteine EN03/EN23, siehe en03_spider.h. Eigenstaendige
   native Formulierung; Ninja-Matrixfolgen auf expliziten lokalen Matrizen
   statt des globalen Stapels (gleiche Reihenfolge der Operationen). */
#include "en03_spider.h"
#include "en03_lib.h"
#include "en03_collision.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const RECVX_EN03_SERVICES* en03_sv;
void recvx_en03_bind(const RECVX_EN03_SERVICES* services) { en03_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
static NJS_VECTOR nv(RECVX_ENEMY_VEC v) { NJS_VECTOR o = { v.x, v.y, v.z }; return o; }
static RECVX_ENEMY_VEC rv(NJS_VECTOR v) { RECVX_ENEMY_VEC o = { v.x, v.y, v.z }; return o; }
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "njRandom (en03)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}

/* --- en03.c Bausteine ------------------------------------------------------ */
int recvx_en03_dg_dir_check(const RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b)
{
    NJS_VECTOR v = { -b->m[8], -b->m[9], -b->m[10] };
    NJS_VECTOR d = { w->dvx, w->dvy, w->dvz };
    return njInnerProduct(&v, &d) < 0 ? 0 : 1;
}
int recvx_en03_dir_target_body(const RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b,
                               const RECVX_ENEMY_VEC* pos, int lim)
{
    NJS_MATRIX m;
    NJS_POINT3 vec = { w->px - pos->x, w->py - pos->y, w->pz - pos->z };
    int ang;
    memcpy(m, b->m, sizeof m);
    njInvertMatrix(&m);
    njCalcPoint(&m, &vec, &vec);
    ang = recvx_t3_arc_tan2(vec.x, vec.z);
    if (lim < abs(ang)) ang = (ang > 0) ? lim : -lim;
    return ang;
}
void recvx_en03_go_ahead(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b)
{
    w->px -= b->m[8] * w->spd;
    w->py -= b->m[9] * w->spd;
    w->pz -= b->m[10] * w->spd;
}
int recvx_en03_parts_pos(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b,
                         const int8_t* parts, RECVX_ENEMY_VEC* p)
{
    const RECVX_ENEMY_EXT* x = recvx_enemy_ext();
    NJS_MATRIX m, o;
    NJS_POINT3 t;
    njUnitMatrix(&m);
    njTranslate(&m, w->px, w->py, w->pz);
    memcpy(o, b->m, sizeof o);
    njMultiMatrix(&m, &o);
    for (; *parts != -1; ++parts) {
        RECVX_ENEMY_VEC lp;
        int32_t ang[3];
        if (!(x && x->node_transform && x->node_transform(w, (uint32_t)(uint8_t)*parts, &lp, ang))) {
            gap(w, "bhEne03_GetPartsPos: mlwP->objP[n].pos/.ang"); return 0;
        }
        njTranslate(&m, lp.x, lp.y, lp.z);
        njRotateXYZ(&m, (Angle)ang[0], (Angle)ang[1], (Angle)ang[2]);
    }
    njGetTranslation(&m, &t);
    p->x = t.x; p->y = t.y; p->z = t.z;
    return 1;
}
void recvx_en03_make_matrix(RECVX_EN03_BODY* b)
{
    const float px = -b->m[8], py = -b->m[9], pz = -b->m[10];
    NJS_MATRIX* m = (NJS_MATRIX*)b->m;
    int ang;
    njUnitMatrix(m);
    switch (b->surface) {
    case 0:
        ang = recvx_t3_arc_tan2(-px, -pz);
        njRotateY(m, (Angle)ang);
        break;
    case 1:
        ang = recvx_t3_arc_tan2(-px, -pz);
        njRotateY(m, (Angle)ang);
        njRotateZ(m, 32768);
        break;
    case 3:
        ang = recvx_t3_arc_tan2(-px, py);
        njRotateX(m, 16384); njRotateY(m, (Angle)ang); njRotateZ(m, 32768);
        break;
    case 5:
        ang = recvx_t3_arc_tan2(-px, py);
        njRotateX(m, 16384); njRotateY(m, (Angle)ang);
        break;
    case 2:
        ang = recvx_t3_arc_tan2(py, -pz);
        njRotateZ(m, -16384); njRotateY(m, (Angle)ang);
        break;
    case 4:
        ang = recvx_t3_arc_tan2(py, -pz);
        njRotateZ(m, -16384); njRotateY(m, (Angle)ang); njRotateZ(m, 32768);
        break;
    }
}
int recvx_en03_get_wall(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b)
{
    RECVX_ENEMY_VEC p1, p2, n;
    uint32_t id = 0;
    p1.x = w->px + b->m[4]; p1.y = w->py + b->m[5]; p1.z = w->pz + b->m[6];
    p2.x = p1.x - (999.0f * b->m[4]); p2.y = p1.y - (999.0f * b->m[5]); p2.z = p1.z - (999.0f * b->m[6]);
    if (!w->sv || !w->sv->line) { gap(w, "bhEne03_GetWall: bhCollisionCheckLine"); return -1; }
    if (!w->sv->line(1, &p1, &p2, &n, 0, 0, NULL, &id)) return -1;
    return id ? (int)id - 1 : -1;
}
int recvx_en03_hide_parts(RECVX_ENEMY_WORK* w, uint32_t node, int hide)
{
    if (!en03_sv || !en03_sv->hide_parts || !en03_sv->hide_parts(w, node, hide)) {
        gap(w, "bhEne03_HideParts: objP[n].evalflags"); return 0;
    }
    return 1;
}
int recvx_en03_set_model_flg(RECVX_ENEMY_WORK* w, uint32_t and_flg, uint32_t or_flg)
{
    if (!en03_sv || !en03_sv->model_flags || !en03_sv->model_flags(w, and_flg, or_flg)) {
        gap(w, "bhEne03_SetModelFlg: owP[i].flg"); return 0;
    }
    return 1;
}
int recvx_en03_owp_flag_or(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t bits)
{
    if (!en03_sv || !en03_sv->owp_flag_or || !en03_sv->owp_flag_or(w, node, bits)) {
        gap(w, "owP[n].flg |= (en03)"); return 0;
    }
    return 1;
}
int recvx_en03_bind_root_angles(RECVX_ENEMY_WORK* w, const int32_t* ang3)
{
    if (!en03_sv || !en03_sv->root_angles_set || !en03_sv->root_angles_set(w, ang3)) {
        gap(w, "mlwP->objP->ang (en03)"); return 0;
    }
    return 1;
}
void recvx_en03_add_null_trans(RECVX_ENEMY_WORK* w, const RECVX_EN03_BODY* b,
                               const RECVX_ENEMY_VEC* mtn, uint32_t n)
{
    NJS_MATRIX m;
    NJS_VECTOR v;
    const uint32_t k = (uint32_t)(w->frm_no / 65536);
    if (w->frm_no < 0 || k >= n) { gap(w, "bhEne03_AddNullTrans: mtn[frm_no/65536] hinter der Tabelle"); return; }
    memcpy(m, b->m, sizeof m);
    v = nv(mtn[k]);
    njCalcVector(&m, &v, &v);
    w->px += v.x;
    w->pz += v.z;
}

/* --- en03sub.c bhEne03s ---------------------------------------------------- */
typedef struct RECVX_EN03S_STATE {
    uint16_t flags;                 /* EXP0_US(0)                            */
    RECVX_ENEMY_VEC vel;            /* EXP0_F(0x04..0x0C)                    */
    RECVX_ENEMY_VEC normal;         /* EXP0_F(0x10..0x18)                    */
    RECVX_ENEMY_VEC local[3];       /* exp0 + 0x1C + 12i                     */
    RECVX_ENEMY_VEC cur[3];         /* exp0 + 0x40 + 12i                     */
    RECVX_ENEMY_VEC prev[3];        /* exp0 + 0x64 + 12i                     */
    RECVX_ENEMY_VEC center;         /* EXP0_F(0x88..0x90)                    */
    RECVX_ENEMY_VEC unused[3];      /* exp0 + 0x94 + 12i, nur genullt        */
    RECVX_ENEMY_VEC axis;           /* EXP0_F(0xB8..0xC0)                    */
    int32_t last;                   /* EXP0_I(0xC4)                          */
    RECVX_ENEMY_VEC center_world;   /* EXP0_F(0xC8..0xD0)                    */
} RECVX_EN03S_STATE;

static RECVX_EN03S_STATE* sst(RECVX_ENEMY_WORK* w) { return (RECVX_EN03S_STATE*)w->exp0; }
static const RECVX_EN03S_LINK* slink(RECVX_ENEMY_WORK* w) { return (const RECVX_EN03S_LINK*)w->exp1; }
/* *epw->mtx: das owP des Elternworks (en23.c:509). */
static int mtx_get(RECVX_ENEMY_WORK* w, NJS_MATRIX* m)
{
    const RECVX_EN03S_LINK* l = slink(w);
    if (!l || !en03_sv || !en03_sv->owp_matrix || !en03_sv->owp_matrix(l->owner, l->node, (float*)m)) {
        gap(w, "bhEne03s: *epw->mtx (owP des Elternworks)"); return 0;
    }
    return 1;
}
static void mtx_put(RECVX_ENEMY_WORK* w, NJS_MATRIX* m)
{
    const RECVX_EN03S_LINK* l = slink(w);
    if (!l || !en03_sv || !en03_sv->owp_matrix_set || !en03_sv->owp_matrix_set(l->owner, l->node, (const float*)m))
        gap(w, "bhEne03s: *epw->mtx schreiben");
}
static int owk_get(RECVX_ENEMY_WORK* w, uint32_t k, NJS_MATRIX* m)
{
    const RECVX_EN03S_LINK* l = slink(w);
    if (!l || !en03_sv || !en03_sv->owp_matrix || !en03_sv->owp_matrix(l->owner, l->node + k, (float*)m)) {
        gap(w, "bhEne03s_InitCollision: owk[k].mtx"); return 0;
    }
    return 1;
}
/* m = R(axis, ang) * mtx um den Punkt `pivot` (njSubVector/njAddVector der Translation). */
static void rotate_about(NJS_MATRIX* mtx, RECVX_ENEMY_VEC axis, int ang, RECVX_ENEMY_VEC pivot)
{
    NJS_MATRIX r;
    NJS_VECTOR a = nv(axis);
    (*mtx)[12] -= pivot.x; (*mtx)[13] -= pivot.y; (*mtx)[14] -= pivot.z;
    njUnitMatrix(&r);
    njRotate(&r, &a, (Angle)ang);
    njMultiMatrix(&r, mtx);
    memcpy(*mtx, r, sizeof r);
    (*mtx)[12] += pivot.x; (*mtx)[13] += pivot.y; (*mtx)[14] += pivot.z;
}
static int line(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* from, RECVX_ENEMY_VEC* to, RECVX_ENEMY_VEC* n)
{
    if (!w->sv || !w->sv->line) { gap(w, "bhEne03s: bhCollisionCheckLine"); return 0; }
    return w->sv->line(1, from, to, n, 0, 0, NULL, NULL);
}

static void s_init_collision(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03S_STATE* s = sst(w);
    NJS_MATRIX o0, o1, o2, inv, r;
    NJS_POINT3 pos, q;
    NJS_VECTOR up = { 0.0f, 1.0f, 0.0f };
    float a, b, c;
    int i;
    if (!owk_get(w, 0, &o0) || !owk_get(w, 1, &o1) || !owk_get(w, 2, &o2)) return;
    s->local[0].x = s->local[0].y = s->local[0].z = 0.0f;
    s->cur[0].x = o0[12]; s->cur[0].y = o0[13]; s->cur[0].z = o0[14];
    pos.x = o1[12] + o2[12]; pos.y = o1[13] + o2[13]; pos.z = o1[14] + o2[14];
    s->cur[1].x = 0.5f * pos.x; s->cur[1].y = 0.5f * pos.y; s->cur[1].z = 0.5f * pos.z;
    if (w->type == 0) { pos.x = w->lkono ? 2.5f : -2.5f; pos.y = -1.0f; pos.z = 0.0f; }
    else { pos.x = w->lkono ? -5.0f : 5.0f; pos.y = -1.0f; pos.z = 0.0f; }
    njCalcPoint(&o2, &pos, &pos);
    s->cur[2].x = pos.x; s->cur[2].y = pos.y; s->cur[2].z = pos.z;
    if (!mtx_get(w, &inv)) return;
    njInvertMatrix(&inv);
    q.x = s->cur[1].x; q.y = s->cur[1].y; q.z = s->cur[1].z; njCalcPoint(&inv, &q, &q); s->local[1] = rv(q);
    q.x = s->cur[2].x; q.y = s->cur[2].y; q.z = s->cur[2].z; njCalcPoint(&inv, &q, &q); s->local[2] = rv(q);
    s->center.x = s->center.y = s->center.z = 0.0f;
    s->center.x += s->local[1].x; s->center.y += s->local[1].y; s->center.z += s->local[1].z;
    s->center.x += s->local[2].x; s->center.y += s->local[2].y; s->center.z += s->local[2].z;
    s->center.x /= 3.0f; s->center.y /= 3.0f; s->center.z /= 3.0f;
    for (i = 0; i < 3; i++) {
        s->prev[i] = s->cur[i];
        s->unused[i].x = s->unused[i].y = s->unused[i].z = 0.0f;
    }
    if (!rnd(w, &a) || !rnd(w, &b)) return;
    njUnitMatrix(&r);
    njRotateX(&r, (Angle)(65536 * a));
    njRotateZ(&r, (Angle)(65536 * b));
    njCalcVector(&r, &up, &up);
    s->axis = rv(up);
    if (!rnd(w, &c)) return;
    w->azp = (int32_t)((65536.0f / 360.0f) * (5.0f + (8.0f * c)));
}
static void s_mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03S_STATE* s = sst(w);
    NJS_MATRIX m;
    float spd, r;
    switch (w->mode3) {
    case 0: {
        RECVX_ENEMY_WORK* ep = w->lkwkp;
        NJS_VECTOR v;
        if (!ep) { gap(w, "bhEne03s: lkwkp"); return; }
        if (!mtx_get(w, &m)) return;
        v.x = m[12] - ep->px; v.y = m[13] - ep->py; v.z = m[14] - ep->pz;
        njUnitVector(&v);
        s->vel = rv(v);
        if (w->type == 0) {
            switch (w->ct0) {
            case 0:
                if (!rnd(w, &r)) return; spd = 0.5f + r;
                s->vel.x *= spd; s->vel.z *= spd;
                if (!rnd(w, &r)) return; s->vel.y = 0.5f + (2.0f * r);
                break;
            case 1:
                if (!rnd(w, &r)) return; spd = 0.5f + r;
                s->vel.x *= spd; s->vel.z *= spd; s->vel.y = 0.0f;
                break;
            case 2: case 3: case 4: case 5:
                if (!rnd(w, &r)) return; spd = 0.5f + r;
                s->vel.x *= spd; s->vel.z *= spd; s->vel.y *= spd;
                break;
            }
        } else {
            if (!rnd(w, &r)) return; spd = 0.5f + (0.5f * r);
            s->vel.x *= spd; s->vel.z *= spd; s->vel.y *= spd;
        }
        s->flags |= 0x1u;
        s_init_collision(w);
        if (!rnd(w, &r)) return;
        w->ct0 = 5 + (int)(6.0f * r);
        w->mode3++;
        break;
    }
    case 1:
        if (s->flags & 1u) {
            NJS_POINT3 c, cl;
            s->vel.y -= 0.2f;
            if (!mtx_get(w, &m)) return;
            m[12] += s->vel.x; m[13] += s->vel.y; m[14] += s->vel.z;
            cl.x = s->center.x; cl.y = s->center.y; cl.z = s->center.z;
            njCalcPoint(&m, &cl, &c);
            rotate_about(&m, s->axis, w->azp, rv(c));
            mtx_put(w, &m);
        } else {
            w->ct3 = 8;
            w->mode3++;
        }
        if (w->ct0-- == 0) {
            const RECVX_ENEMY_VEC at = { w->px, w->py, w->pz };
            if (!rnd(w, &r)) return;
            w->ct0 = 15 + (int)(10.0f * r);
            recvx_t3_set_blood_effect4(&at, &at, 1, 9, 0);
        }
        break;
    case 2:
        if (!mtx_get(w, &m)) return;
        if (w->ct3) {
            NJS_VECTOR v = { s->normal.x, s->normal.z, s->normal.y }, vd, ov, n = nv(s->normal);
            float out;
            int ang;
            if (s->last < 0 || s->last > 2) { gap(w, "bhEne03s_MV00: EXP0_I(0xC4) ausserhalb"); return; }
            njCalcVector(&m, &v, &vd);
            if (vd.y < 0.0f) { vd.x *= -1.0f; vd.y *= -1.0f; vd.z *= -1.0f; }
            out = njOuterProduct(&vd, &n, &ov);
            njUnitVector(&ov);
            ang = njArcSin(out);
            rotate_about(&m, rv(ov), ang / w->ct3, s->cur[s->last]);
            w->ct3--;
        } else {
            NJS_POINT3 cl = { s->center.x, s->center.y, s->center.z }, cw;
            s->vel.x = s->vel.y = s->vel.z = 0.0f;
            s->flags |= 0x2u;
            njCalcPoint(&m, &cl, &cw);
            s->center_world = rv(cw);
            w->mode3++;
        }
        m[13] -= 0.2f;
        mtx_put(w, &m);
        break;
    case 3:
        if (!(s->flags & 2u)) w->mode0 = 2;
        else {
            if (!mtx_get(w, &m)) return;
            m[12] += s->vel.x; m[13] += s->vel.y; m[14] += s->vel.z;
            mtx_put(w, &m);
            s->vel.y -= 0.2f;
        }
        break;
    }
}
static void s_wall_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03S_STATE* s = sst(w);
    NJS_MATRIX m;
    RECVX_ENEMY_VEC n[3];
    int i, flg[3], flgx = 0, flgy = 0, flgz = 0, last = -1;
    if (!mtx_get(w, &m)) return;
    w->px = m[12]; w->py = m[13]; w->pz = m[14];
    for (i = 2; i >= 0; i--) {
        NJS_VECTOR v = nv(s->local[i]);
        flg[i] = 0;
        njCalcVector(&m, &v, &v);
        w->px += v.x; w->py += v.y; w->pz += v.z;
        if (s->flags & 1u) {
            RECVX_ENEMY_VEC to = { w->px, w->py, w->pz };
            if (line(w, &s->prev[i], &to, &n[i])) {
                NJS_VECTOR u = nv(n[i]);
                w->px = to.x; w->py = to.y; w->pz = to.z;
                flg[i] = 1;
                njUnitVector(&u); n[i] = rv(u);
                if (n[i].y < 0.0f) s->vel.y = 0.0f;
                if (n[i].y == 0.0f) {
                    if (fabsf(n[i].x) > fabsf(n[i].z)) flgx = 1;
                    else flgz = 1;
                }
                if (n[i].y > 0.99f) { flgy = 1; last = i; }
            }
        }
        recvx_en03_collision(w, NULL);
        w->px -= v.x; w->py -= v.y; w->pz -= v.z;
    }
    m[12] = w->px; m[13] = w->py; m[14] = w->pz;
    mtx_put(w, &m);
    if (flgx) s->vel.x *= -0.3f;
    if (flgy) s->vel.y *= -0.3f;
    if (flgz) s->vel.z *= -0.3f;
    for (i = 0; i < 3; i++) {
        NJS_VECTOR v = nv(s->local[i]);
        s->prev[i] = s->cur[i];
        njCalcVector(&m, &v, &v);
        s->cur[i].x = w->px + v.x; s->cur[i].y = w->py + v.y; s->cur[i].z = w->pz + v.z;
    }
    for (i = 0; i < 3; i++) {
        if (flg[i]) {
            NJS_POINT3 p[3];
            NJS_VECTOR a;
            p[0].x = (s->cur[0].x + s->cur[1].x + s->cur[2].x) / 3.0f;
            p[0].y = (s->cur[0].y + s->cur[1].y + s->cur[2].y) / 3.0f;
            p[0].z = (s->cur[0].z + s->cur[1].z + s->cur[2].z) / 3.0f;
            p[1].x = s->cur[i].x; p[1].y = s->cur[i].y; p[1].z = s->cur[i].z;
            p[2].x = p[1].x + n[i].x; p[2].y = p[1].y + n[i].y; p[2].z = p[1].z + n[i].z;
            njGetPlaneNormal(p, &a);
            njUnitVector(&a);
            s->axis = rv(a);
            w->azp = (int32_t)(w->azp * 0.7);
        }
    }
    if (last >= 0 && w->azp < 546) {                  /* NJM_DEG_ANG(3) */
        s->flags &= (uint16_t)~0x1u;
        s->last = last;
        s->normal = n[last];
        s->vel.x = 0.0f; s->vel.z = 0.0f;
    }
}
static void s_wall_check2(RECVX_ENEMY_WORK* w)
{
    RECVX_EN03S_STATE* s = sst(w);
    NJS_MATRIX m;
    NJS_VECTOR v = nv(s->center);
    NJS_POINT3 cl, cw;
    if (!mtx_get(w, &m)) return;
    w->px = m[12]; w->py = m[13]; w->pz = m[14];
    njCalcVector(&m, &v, &v);
    w->px += v.x; w->py += v.y; w->pz += v.z;
    if (s->flags & 2u) {
        RECVX_ENEMY_VEC to = { w->px, w->py, w->pz }, n;
        if (line(w, &s->center_world, &to, &n)) {
            NJS_VECTOR u = nv(n);
            w->px = to.x; w->py = to.y; w->pz = to.z;
            njUnitVector(&u);
            if (u.y > 0.99f) s->flags &= (uint16_t)~0x2u;
        }
    }
    recvx_en03_collision(w, NULL);
    w->px -= v.x; w->py -= v.y; w->pz -= v.z;
    m[12] = w->px; m[13] = w->py; m[14] = w->pz;
    mtx_put(w, &m);
    cl.x = s->center.x; cl.y = s->center.y; cl.z = s->center.z;
    njCalcPoint(&m, &cl, &cw);
    s->center_world = rv(cw);
}
void recvx_en03s_leg_mark(RECVX_ENEMY_WORK* child)
{
    unsigned char* b;
    if (!child || !child->exp0) { gap(child, "bhEne03: EXP0_BYTE(pno,12) ohne Kindexp0"); return; }
    /* Byte 12 ist die niedrigste Stelle von EXP0_F(0xC); 13 bleibt stehen. */
    b = (unsigned char*)child->exp0;
    b[12] = 1;
    b[14] = 0;
    b[15] = 0;
}

void recvx_en03s(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    switch (w->mode0) {
    case 0:
        w->ar = 0.3f; w->ah = 0.3f; w->car = 0.3f; w->cah = 0.3f;
        w->mode0 = 2; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
        if (!w->exp0) {
            if (!w->sv || !w->sv->calloc_work) { gap(w, "bhEne_CallocWork (en03s)"); return; }
            w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN03S_STATE));
            if (!w->exp0) { gap(w, "bhEne03s_Init: EXP-Budget erschoepft"); return; }
        }
        sst(w)->flags = 0;
        break;
    case 1:
        if (!w->exp0) { gap(w, "bhEne03s: exp0 fehlt"); return; }
        if (w->mode2 != 0) { gap(w, "bhEne03s_MoveMode2[mode2]"); return; }
        s_mv00(w);
        if (w->mode0 == 1) {
            if (!(sst(w)->flags & 0x2u)) s_wall_check(w);
            else s_wall_check2(w);
        }
        break;
    case 2: break;                           /* bhEne03s_Dummy */
    default: gap(w, "bhEne03s_Mode0[mode0]"); break;
    }
}
