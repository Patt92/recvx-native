#ifndef RECVX_NATIVE_COMBAT_GEOMETRY_H
#define RECVX_NATIVE_COMBAT_GEOMETRY_H
/* ===========================================================================
   A01-A04 Kampfgeometrie — eigenstaendige native Bestimmung der Ninja-/njplus-
   Kollisionsprimitive, die der Spielerkampf (weapon.c, pwksub.c, playpch.c)
   aufruft. Semantikreferenz, kein Originalkoerper:
     ps2_NaColi.c:77   njDistanceP2L      ps2_NaColi.c:153  njDistanceL2L
     ps2_NaColi.c:322  njCollisionCheckSS
     njplus.c:33       npCollisionCheckCC njplus.c:234      npCollisionCheckCCEx
     njplus.c:498      npCollisionCheckSC njplus.c:565      npDistanceP2C
   Typen sind layoutgleich zu NJS_POINT3/NJS_LINE/NJS_SPHERE/NJS_CAPSULE.
   =========================================================================== */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float x, y, z; } RECVX_CB_VEC;                 /* NJS_POINT3  */
typedef struct { float px, py, pz, vx, vy, vz; } RECVX_CB_LINE; /* NJS_LINE    */
typedef struct { RECVX_CB_VEC c; float r; } RECVX_CB_SPHERE;    /* NJS_SPHERE  */
typedef struct { RECVX_CB_VEC c1, c2; float r; } RECVX_CB_CAPSULE; /* NJS_CAPSULE */

/* njDistanceP2L: Abstand Punkt-Gerade, `cp` (darf NULL sein) Fusspunkt. */
float recvx_cb_distance_p2l(const RECVX_CB_VEC* p, const RECVX_CB_LINE* l, RECVX_CB_VEC* cp);
/* njDistanceL2L: Abstand zweier Geraden mit Fusspunkten (beide optional). */
float recvx_cb_distance_l2l(const RECVX_CB_LINE* l1, const RECVX_CB_LINE* l2,
                            RECVX_CB_VEC* cp1, RECVX_CB_VEC* cp2);
int   recvx_cb_check_ss(const RECVX_CB_SPHERE* a, const RECVX_CB_SPHERE* b);
int   recvx_cb_check_cc(const RECVX_CB_CAPSULE* a, const RECVX_CB_CAPSULE* b);
/* npCollisionCheckCCEx: wie CC, schreibt bei Treffer den Kontaktpunkt. */
int   recvx_cb_check_ccex(const RECVX_CB_CAPSULE* a, const RECVX_CB_CAPSULE* b, RECVX_CB_VEC* pos);
int   recvx_cb_check_sc(const RECVX_CB_SPHERE* s, const RECVX_CB_CAPSULE* c);
void  recvx_cb_distance_p2c(const RECVX_CB_VEC* pos, const RECVX_CB_CAPSULE* cap, RECVX_CB_VEC* htp);

#ifdef __cplusplus
}
#endif
#endif
