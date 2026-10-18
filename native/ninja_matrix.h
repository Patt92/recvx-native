#pragma once
#include "ninja_math.h"
typedef int32_t Sint32;
typedef uint32_t Uint32;
typedef int Int;
typedef int Bool;
typedef Float NJS_MATRIX[16];
typedef struct { Float x,y,z; } NJS_VECTOR;
typedef NJS_VECTOR NJS_POINT3;
typedef struct { Float x,y,z,w; } NJS_POINT4;
typedef struct { Float x,y; } NJS_POINT2;
/* ninja.h NJS_SCREEN (types.h:2112, 0x14 Bytes): dist, w, h, cx, cy. */
typedef struct { Float dist,w,h,cx,cy; } NJS_SCREEN;
/* ninja.h NJS_PLANE (types.h:791 sys->mr_pl, 0x18 Bytes): Punkt px/py/pz, Normale vx/vy/vz. */
typedef struct { Float px,py,pz,vx,vy,vz; } NJS_PLANE;
#ifdef __cplusplus
extern "C" {
#endif
// n is the number of available storage slots, including the current matrix.
void njInitMatrix(NJS_MATRIX* storage, Sint32 n, Int flag);
void njUnitMatrix(NJS_MATRIX* m);
void njSetMatrix(NJS_MATRIX* dest, NJS_MATRIX* source);
void njGetMatrix(NJS_MATRIX* dest);
void njCopyMatrix(NJS_MATRIX* dest, NJS_MATRIX* source);
void njClearMatrix(void);
Bool njPushMatrix(NJS_MATRIX* source);
Bool njPopMatrix(Uint32 count);
Bool njPushMatrixEx(void);
Bool njPopMatrixEx(void);
void njMultiMatrix(NJS_MATRIX* dest, NJS_MATRIX* source);
void njTranslate(NJS_MATRIX* m, Float x, Float y, Float z);
void njTranslateV(NJS_MATRIX* m, NJS_VECTOR* v);
void njScale(NJS_MATRIX* m, Float x, Float y, Float z);
void njScaleV(NJS_MATRIX* m, NJS_VECTOR* v);
void njRotateX(NJS_MATRIX* m, Angle angle);
void njRotateY(NJS_MATRIX* m, Angle angle);
void njRotateZ(NJS_MATRIX* m, Angle angle);
void njRotateXYZ(NJS_MATRIX* m, Angle x, Angle y, Angle z);
/* ps2_NaMatrix.c:737 — Drehung um die freie Achse v. Die Achse wird in der
   Routine normiert; v darf jede Laenge haben. Siehe Beleg im Koerper. */
void njRotate(NJS_MATRIX* m, NJS_VECTOR* v, Angle ang);
void njRotXYZ(NJS_MATRIX* m, int x, int y, int z);
void njCalcPoint(NJS_MATRIX* m, NJS_POINT3* source, NJS_POINT3* dest);
void njCalcPoint4(NJS_MATRIX* m, NJS_POINT4* source, NJS_POINT4* dest);
void njCalcPoints(NJS_MATRIX* m, NJS_POINT3* source, NJS_POINT3* dest, Int count);
void njCalcVector(NJS_MATRIX* m, NJS_VECTOR* source, NJS_VECTOR* dest);
void njGetTranslation(NJS_MATRIX* m, NJS_POINT3* dest);
void njUnitTransPortion(NJS_MATRIX* m);
void njUnitRotPortion(NJS_MATRIX* m);
Bool njInvertMatrix(NJS_MATRIX* m);
void njTransposeMatrix(NJS_MATRIX* m);
void njAddVector(NJS_VECTOR* dest, NJS_VECTOR* source);
void njSubVector(NJS_VECTOR* dest, NJS_VECTOR* source);
Float njScalor2(NJS_VECTOR* v);
Float njScalor(NJS_VECTOR* v);
Float njUnitVector(NJS_VECTOR* v);
Float njInnerProduct(NJS_VECTOR* a, NJS_VECTOR* b);
Float njOuterProduct(NJS_VECTOR* a, NJS_VECTOR* b, NJS_VECTOR* dest);
/* ps2_NaColi.c:301 — Flaechennormale aus den ERSTEN DREI Punkten von p:
   (p1-p0) x (p2-p1), unnormiert. */
void njGetPlaneNormal(NJS_POINT3* p, NJS_VECTOR* v);
/* ps2_NaColi.c:314 — dieselbe Normale aus drei einzelnen Punkten. Die
   Kantenreihenfolge ist (p1-p0) x (p2-p1), nicht (p1-p0) x (p2-p0). */
void njGetPlaneNormal2(NJS_POINT3* p0, NJS_POINT3* p1, NJS_POINT3* p2, NJS_VECTOR* v);
/* ps2_NaView.c:217 — NaViewScreenMatrix = diag(AspectW, AspectH, 1) mit [15] = dist. */
void njViewScreenMatrix(NJS_MATRIX* vs);
/* ps2_NaView.c:27 / :81 / :55 — Bildschirm-, Seitenverhaeltnis- und
   Perspektivzustand. Die Hardwareteile des Originals (Ps2CalcScreenCone,
   vu1SetScreenProjection/Offset, njClearMatrix) haben hier keinen Abnehmer. */
void njSetScreen(NJS_SCREEN* s);
void njSetAspect(Float ax, Float ay);
void njSetPerspective(Angle ang);
/* ps2_NaMatrix.c:1572 — Bildschirmpunkt von p3 unter m (NULL = aktuelle
   Matrix): NaViewScreenMatrix * m, dann x/y durch z geteilt, mit dist
   skaliert und um fNaViwOffsetX/Y verschoben. p2->z des Originals landet in
   einer lokalen Variablen und ist damit kein Ergebnis. */
void njProjectScreen(NJS_MATRIX* m, NJS_POINT3* p3, NJS_POINT2* p2);
/* ps2_NaMatrix.c:1016 njMirror (ELF 0x002D7260..0x002D74D0): m = m * mat, mat die
   Spiegelung an pl. Nativ eigenstaendig, ohne VU-Nachbildung, nach der Disassembly:
     n = v / sqrt(|v|^2) (vrsqrt), d = ((0 - n.x*px) - n.y*py) - n.z*pz,
     Verschiebung T = -(v * 2d) mit dem UNNORMIERTEN v,
     lAngleX = (int)((atan2f(vz, vy) * 32768) / 3.141592f),
     lAngleZ = -(int)((atan2f(vx, sqrt(|vy + vz|)) * 32768) / 3.141592f).
   Der zweite Winkel nimmt im Original vf5 (v) statt der Quadrate vf6: vaddy.x vf7, vf0, vf5y /
   vaddz.x vf7, vf7, vf5z (0x002D73C8/0x002D73CC), vsqrt nimmt den Betrag (0x002D73E4). Der
   Faktor 32768 ist vf3.z * vf2.w = 65536 * 0.5, der Teiler vf3.w = 3.141592f (njInitMatrix
   0x002D66B0). atan2 rechnet njAtan2b in double (fptodp/atan2/dptofp, 0x002D7210).
   mat = Einheitsdrehung mit T, dann njRotateX(ax), njRotateZ(az), njScale(1,-1,1),
   njRotateZ(-az), njRotateX(-ax); zuletzt njMultiMatrix(m, &mat).
   Zwei Originalzweige sind undefiniert und werden NICHT ausgefuehrt (Rueckgabe 0, m bleibt):
   d == +0 (Bitmuster) springt ohne Stapelabbau mit jr ra zurueck (0x002D7324); v.y + v.z == +0
   und 0 - n.y*d == +0 (Bitmuster) laesst lAngleX/lAngleZ ungeschrieben (0x002D7348..7370).
   Rueckgabe 1: der belegte Zweig lief. */
int njMirror(NJS_MATRIX* m, NJS_PLANE* pl);
extern NJS_MATRIX* pNaMatMatrixStuckPtr;
extern NJS_MATRIX NaViwViewMatrix;
extern NJS_MATRIX NaViewScreenMatrix;
extern NJS_SCREEN _nj_screen_;
extern Float fNaViwOffsetX, fNaViwOffsetY, fNaViwAspectW, fNaViwAspectH;
#ifdef __cplusplus
}
#endif
