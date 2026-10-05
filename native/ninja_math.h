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
void njLinear(Float* input, Float* output, NJS_SPLINE* attr, Float frame);
void njOverhauserSpline(Float* input, Float* output, NJS_SPLINE* attr, Float frame);
void njBezierSpline(Float* input, Float* output, NJS_SPLINE* attr, Float frame);
unsigned int njFactorial(unsigned int n);
#ifdef __cplusplus
}
#endif
#endif
