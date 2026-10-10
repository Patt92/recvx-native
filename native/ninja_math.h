#ifndef RECVX_NATIVE_NINJA_MATH_H
#define RECVX_NATIVE_NINJA_MATH_H
#include <stdint.h>
typedef float Float;
typedef int32_t Angle;
typedef struct {
    int keys;
    int* iparam;
    float* fparam;
} NJS_SPLINE;
#ifdef __cplusplus
extern "C" {
#endif
void _Make_SinTable(void);
Float njSin(Angle n);
Float njCos(Angle n);
void njSinCos(int angle, float* sine, float* cosine);
Float njFraction(Float n);
Float njSqrt(Float n);
Float njInvertSqrt(Float n);
/* macros.h:102/108 — im Original ein Makro:
   njArcSin(n) = (Angle)(asinf(n) * (65536.0f / PI_2)), PI_2 = 3.141592f * 2.0f.
   Nativ als Funktion, weil njSin/njCos hier ebenfalls Funktionen sind. Der
   Umrechnungsfaktor benutzt die Originalkonstante 3.141592f, nicht M_PI; mit
   dem genaueren Wert weichen die Winkel ab. Die Wandlung nach Angle schneidet
   wie im Original gegen Null ab. Werte ausserhalb [-1,1] ergeben wie im
   Original NaN; es wird nicht geklemmt. */
Angle njArcSin(Float n);
void njLinear(Float* input, Float* output, NJS_SPLINE* attr, Float frame);
void njOverhauserSpline(Float* input, Float* output, NJS_SPLINE* attr, Float frame);
void njBezierSpline(Float* input, Float* output, NJS_SPLINE* attr, Float frame);
unsigned int njFactorial(unsigned int n);
#ifdef __cplusplus
}
#endif
#endif
