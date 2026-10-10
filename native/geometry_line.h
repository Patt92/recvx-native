#pragma once
#include "collision.h"
#include "ninja_math.h"
#include <cmath>
namespace recvx::line_geometry {
// Independent native geometry primitives. Reference: ps2_NaColi.c's
// mathematical contracts only; no original module or hardware state.
inline NJS_POINT3 subtract(NJS_POINT3 a,NJS_POINT3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline NJS_POINT3 advance(NJS_POINT3 p,NJS_POINT3 v,float t){return {p.x+v.x*t,p.y+v.y*t,p.z+v.z*t};}
inline float dot(NJS_POINT3 a,NJS_POINT3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline NJS_POINT3 cross(NJS_POINT3 a,NJS_POINT3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline NJS_POINT3 normalized(NJS_POINT3 a){
    const float inv=njInvertSqrt(dot(a,a));return {a.x*inv,a.y*inv,a.z*inv};
}
inline float length(NJS_POINT3 a){return njSqrt(dot(a,a));}
inline NJS_POINT3 normal(NJS_POINT3 a,NJS_POINT3 b,NJS_POINT3 c){return cross(subtract(b,a),subtract(c,b));}
inline float point_line(NJS_POINT3 p,NJS_POINT3 origin,NJS_POINT3 direction,NJS_POINT3& closest){
    const auto unit=normalized(direction),offset=subtract(p,origin);
    const float squared=dot(offset,offset);
    if(squared==0){closest=origin;return 0;}
    const float along=dot(unit,offset);closest=advance(origin,unit,along);
    return njSqrt(squared-along*along); // Infinite line, not a segment clamp.
}
inline float point_plane(NJS_POINT3 p,NJS_POINT3 origin,NJS_POINT3 normal,NJS_POINT3& closest){
    const auto unit=normalized(normal);const float distance=dot(unit,subtract(p,origin));
    closest=advance(p,unit,-distance);return std::fabs(distance);
}
// njDistanceL2PL's 0.025 parallel threshold and point-to-plane fallback are
// part of the contract. Do not substitute an epsilon/segment ray routine.
inline float line_plane(NJS_POINT3 origin,NJS_POINT3 direction,NJS_POINT3 plane,NJS_POINT3 normal,NJS_POINT3& closest){
    const auto n=normalized(normal),v=normalized(direction);
    const float cosine=dot(n,v);
    if(std::fabs(cosine)>.025f){closest=advance(origin,v,-dot(n,subtract(origin,plane))/cosine);return 0;}
    return point_plane(origin,plane,normal,closest);
}
}
