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
extern NJS_MATRIX* pNaMatMatrixStuckPtr;
extern NJS_MATRIX NaViwViewMatrix;
#ifdef __cplusplus
}
#endif
