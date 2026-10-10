// Native math API. Reference: ps2_NaMath.c; no original module is compiled.
#include "ninja_math.h"
#include <math.h>
#include <float.h>

static float sine_table[16384];
void _Make_SinTable(void) {
    for (int i=0;i<16384;++i) sine_table[i]=sinf(0.0000958738f*i);
}
Float njSin(Angle n) {
    const uint32_t angle=(uint32_t)n;
    uint32_t index=angle&0x3fff;
    if (angle&0x4000) index=0x3fff-index;
    return (angle&0x8000)?-sine_table[index]:sine_table[index];
}
Float njCos(Angle n) {
    const uint32_t angle=(uint32_t)n;
    uint32_t index=angle&0x3fff;
    if (!(angle&0x4000)) index=0x3fff-index;
    return ((angle^(angle<<1))&0x8000)?-sine_table[index]:sine_table[index];
}
void njSinCos(int angle,float* sine,float* cosine) {
    *sine=njSin(angle);*cosine=njCos(angle);
}
/* macros.h:102/108 — PI_2 ist dort (3.141592f * 2.0f), nicht 2*M_PI. */
#define RECVX_NJ_RAD_ANG (65536.0f/(3.141592f*2.0f))
Angle njArcSin(Float n) {return (Angle)(asinf(n)*RECVX_NJ_RAD_ANG);}
Float njFraction(Float n) {return n-floorf(n);}
Float njSqrt(Float n) {return sqrtf(fabsf(n));}
Float njInvertSqrt(Float n) {
    if (n==0.0f) return signbit(n)?-FLT_MAX:FLT_MAX;
    return 1.0f/sqrtf(fabsf(n));
}
void njLinear(Float* input,Float* output,NJS_SPLINE* attr,Float frame) {
    (void)attr;float value[3];
    for (int i=0;i<3;++i) value[i]=input[i]+(input[i+3]-input[i])*frame;
    for (int i=0;i<3;++i) output[i]=value[i];
}
void njOverhauserSpline(Float* input,Float* output,NJS_SPLINE* attr,Float frame) {
    (void)attr;
    const float t2=frame*frame,t3=t2*frame;
    const float weights[4]={2.0f*t2-t3-frame,3.0f*t3-5.0f*t2+2.0f,
        -3.0f*t3+4.0f*t2+frame,t3-t2};
    float value[3];
    for (int i=0;i<3;++i) value[i]=0.5f*(((input[i]*weights[0]+
        input[i+3]*weights[1])+input[i+6]*weights[2])+input[i+9]*weights[3]);
    for (int i=0;i<3;++i) output[i]=value[i];
}
unsigned int njFactorial(unsigned int n) {
    unsigned int result=1;
    for (;n!=0;--n) result*=n;
    return result;
}
void njBezierSpline(Float* input,Float* output,NJS_SPLINE* attr,Float frame) {
    output[0]=output[1]=output[2]=0;
    const unsigned int maximum=*attr->iparam-1;
    const float factorial=njFactorial(maximum);
    for (unsigned int i=0;i<=maximum;++i) {
        const float weight=(powf(frame,i)*(factorial/(njFactorial(i)*
            njFactorial(maximum-i))))*powf(1.0f-frame,maximum-i);
        output[0]+=weight**input++;output[1]+=weight**input++;output[2]+=weight**input++;
    }
}
