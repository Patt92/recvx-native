#ifndef RECVX_NATIVE_EN03_COLLISION_H
#define RECVX_NATIVE_EN03_COLLISION_H
#include "en03_lib.h"

/* ===========================================================================
   EN03-Kollisionsfamilie — der GEMEINSAME Gegner-gegen-Raumwand-Durchlauf
   ---------------------------------------------------------------------------
   Semantikreferenz: en03.c:5064 bhEne03_CollisionWalls, :5203 GetWall,
   :5220 Collision, :5310 Collision2, :5399 CollisionWallBox,
   :5528 CollisionWallGround, :5549 CollisionWallCylinder,
   :5594 CollisionWallSlope, :5770 CollisionWallTriangle,
   :5938 CollisionBoxEdge. Hilfsgeometrie ps2_NaColi.c:65 njDistanceP2P,
   :77 njDistanceP2L, :125 njDistanceP2PL, :301 njGetPlaneNormal und
   hitchkl.c:682 bhInOutCheck.

   Dieser Satz ist nicht EN03-eigen: er ist der Dienst `collision_works`
   (native/enemy_work.h:365), den en03sub.c:286/389, en05sub.c, en06.c:1840,
   en07.c:1863, en08.c, en11.c:852, en23.c:3066, en24.c:512 und en30.c:84
   benutzen. Darum liegt er in einer eigenen Datei und wird vom Integrator
   EINMAL als dieser Dienst gebunden.

   Eigenstaendige native Bestimmung; kein Originalkoerper, kein Generator,
   keine zweite Workwelt. Die Flaechen kommen ueber RECVX_ENEMY_EXT.wal_at
   aus dem vorhandenen Besitzer; dieser Satz haelt keine eigene Flaechenliste.
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* en03.c:5220 bhEne03_Collision(epw): laeuft rom->walp + sys->mwalp in
   Originalreihenfolge und STAUCHT die Lage des Works direkt. Rueckgabe wie
   der Vertrag es verlangt: 1, wenn wenigstens eine Flaeche getroffen hat
   (das Original gibt die LETZTE Trefferflaeche zurueck).
   `hit_index` erhaelt, wenn != NULL, den Index dieser letzten Trefferflaeche
   im zusammengesetzten Lauf, sonst -1. Das ist die native Entsprechung des
   Original-Zeigervergleichs, den bhEne03_Collision2 braucht. */
int recvx_en03_collision(RECVX_ENEMY_WORK* w, int* hit_index);

/* en03.c:5310 bhEne03_Collision2(epw, gnd): derselbe Lauf, aber die Flaeche
   mit dem Index `exclude_index` wird uebersprungen (Original: hp != gnd).
   `exclude_index` < 0 schliesst keine aus. Zu beachten: Collision2 prueft
   flg & 1 UND den Ausschluss in derselben Bedingung; eine ausgeschlossene
   Flaeche durchlaeuft deshalb auch den Typwechsel nicht. */
int recvx_en03_collision2(RECVX_ENEMY_WORK* w, int exclude_index, int* hit_index);

/* Die fuenf Wandformen. `pos` wird bei Treffer direkt verschoben; `ar` ist
   der Koerperradius. Sie sind oeffentlich, weil die Gegneridentitaeten sie
   einzeln rufen (en03.c:6074 BoxEdge2, en09.c, en26.c). */
int recvx_en03_wall_box(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar);
int recvx_en03_wall_ground(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar);
int recvx_en03_wall_cylinder(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar);
int recvx_en03_wall_slope(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar);
int recvx_en03_wall_triangle(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar);

/* en03.c:5938 bhEne03_CollisionBoxEdge: zwoelf Kantenkaesten um eine Flaeche.
   Rueckgabe ist die ODER-Summe aller zwoelf Einzelproben. */
int recvx_en03_collision_box_edge(const RECVX_T3_ATR* hp, RECVX_ENEMY_VEC* pos, float ar);

/* Hilfsgeometrie der Originalbibliothek, hier gebraucht und darum
   mitveroeffentlicht (Fundstellen oben). */
float recvx_en03_distance_p2p(const RECVX_ENEMY_VEC* a, const RECVX_ENEMY_VEC* b);
/* NJS_LINE: Punkt (px,py,pz) plus Richtung (vx,vy,vz). */
typedef struct { RECVX_ENEMY_VEC p, v; } RECVX_EN03_LINE;
float recvx_en03_distance_p2l(const RECVX_ENEMY_VEC* p, const RECVX_EN03_LINE* l,
                              RECVX_ENEMY_VEC* cp);
/* NJS_PLANE: Punkt plus Normale (nicht notwendig Einheitslaenge). */
typedef struct { RECVX_ENEMY_VEC p, v; } RECVX_EN03_PLANE;
float recvx_en03_distance_p2pl(const RECVX_ENEMY_VEC* p, const RECVX_EN03_PLANE* pl,
                               RECVX_ENEMY_VEC* cp);
void  recvx_en03_plane_normal(const RECVX_ENEMY_VEC* area, RECVX_ENEMY_VEC* out);
int   recvx_en03_in_out_check(const RECVX_ENEMY_VEC* p, const RECVX_ENEMY_VEC* area,
                              const RECVX_ENEMY_VEC* normal, int num);

#ifdef __cplusplus
}
#endif
#endif
