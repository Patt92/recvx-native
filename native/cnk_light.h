#pragma once
#include "ninja_matrix.h"
#include <cmath>

namespace recvx {
// njCnkCvVn (ps2_NinjaCnk.c:1226-1262), the per-vertex light term of the
// EasyMulti chunk path. The VU0 form (njCnkCvVnPs2, :1316-1324) feeds the same
// squared ranges and colours to VU0_CALCPOINT.
//
// Point light (ulMode 1): d2 = |C - V|^2 and only while fFarRR > d2 does the
// light count; the diffuse term is N.L with L unit; once fNearRR < d2 it falls
// off by fNearRR / d2. njCnkSetEasyMultiLightRange stores both ranges squared
// (:352-363), so a near range of 0 contributes nothing beyond d 0. d2 == 0 is
// njInvertSqrt(0) in the original: NaN, rejected by the f > 0 test.
inline float cnk_point_light_term(NJS_POINT3 light,float near_range,float far_range,NJS_POINT3 vertex,NJS_VECTOR normal){
    const float dx=light.x-vertex.x,dy=light.y-vertex.y,dz=light.z-vertex.z;
    const float d2=dx*dx+dy*dy+dz*dz;
    const float near2=near_range*near_range,far2=far_range*far_range;
    if(!(far2>d2)||!(d2>0.f))return 0.f;
    const float inv=1.f/std::sqrt(d2);
    float f=(dx*normal.x+dy*normal.y+dz*normal.z)*inv;
    if(!(f>0.f))return 0.f;
    if(near2<d2)f*=near2/d2;
    return f;
}
// Directional light (ulMode 0): njCnkSetEasyMultiLightVector keeps -v (:328-336),
// njCnkSetEasyMultiLightMatrices rotates it into camera space and the term is
// N.fC * fI (:1234-1239). fI is 1 on the multi set (only njCnkSetEasyLightIntensity
// writes the single light's fI, :260) and v is used unnormalised.
inline float cnk_directional_light_term(NJS_VECTOR v,NJS_VECTOR normal){
    const float f=-(v.x*normal.x+v.y*normal.y+v.z*normal.z);
    return f>0.f?f:0.f;
}
}
