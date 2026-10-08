/* Minimal libm surface for the ILP32 (wasm32) build of the original engine.
 * Declarations only: each function becomes a module import that the host binds
 * to its native libm (see native/core32_host.c). */
#ifndef RECVX_W32_MATH_H
#define RECVX_W32_MATH_H
#define M_PI 3.14159265358979323846
#define HUGE_VALF __builtin_huge_valf()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define signbit(x) __builtin_signbit(x)
#define isfinite(x) __builtin_isfinite(x)
#define fabsf(x) __builtin_fabsf(x)
#define fabs(x) __builtin_fabs(x)
#define sqrtf(x) __builtin_sqrtf(x)
#define sqrt(x) __builtin_sqrt(x)
#define floorf(x) __builtin_floorf(x)
#define ceilf(x) __builtin_ceilf(x)
#define truncf(x) __builtin_truncf(x)
float frexpf(float, int*); float ldexpf(float, int);
float sinf(float); float cosf(float); float tanf(float);
float asinf(float); float acosf(float); float atanf(float); float atan2f(float, float);
float powf(float, float); float expf(float); float logf(float); float fmodf(float, float);
double sin(double); double cos(double); double tan(double); double asin(double); double acos(double);
double atan(double); double atan2(double, double); double pow(double, double); double exp(double);
double log(double); double fmod(double, double); double floor(double); double ceil(double);
#endif
