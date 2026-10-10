#pragma once
#include "collision.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace recvx::atr_geometry {
// Native predicates derived from hitchk.c, applied to existing decoded ATR
// geometry. No original function body or separate game work is used.
struct Point2 {float x,z;};
inline float height(const CollisionWall& a,float room_height){return a.extent.y ? a.extent.y : room_height;}
inline bool rectangle(const CollisionWall& a,NJS_POINT3 p,float radius=0){
    const float x=p.x-(a.position.x-radius),z=p.z-(a.position.z-radius);
    return x>=0 && x<a.extent.x+2.f*radius && z>=0 && z<a.extent.z+2.f*radius;
}
inline float edge_x(Point2 a,Point2 b,float z){
    const float dz=b.z-a.z;
    const float slope=dz ? (b.x-a.x)/dz : 0.f;
    return a.x+slope*(z-a.z);
}
// bhCheckInnerP4 is a z-ordered scanline strip, not a winding test.
// Lower bounds are inclusive, upper bounds exclusive.
inline bool strip(Point2 p,const Point2 (&v)[4]){
    if(p.z<v[0].z || p.z>=v[3].z)return false;
    float lo,hi;
    const bool right=v[1].x-v[0].x>0;
    if(p.z<v[1].z){
        lo=edge_x(v[0],v[right?2:1],p.z);
        hi=edge_x(v[0],v[right?1:2],p.z);
    }else if(p.z<v[2].z){
        lo=right?edge_x(v[0],v[2],p.z):edge_x(v[1],v[3],p.z);
        hi=right?edge_x(v[1],v[3],p.z):edge_x(v[0],v[2],p.z);
    }else{
        lo=right?edge_x(v[2],v[3],p.z):edge_x(v[1],v[3],p.z);
        hi=right?edge_x(v[1],v[3],p.z):edge_x(v[2],v[3],p.z);
    }
    if(lo>hi)std::swap(lo,hi);
    return !(lo>p.x || hi<=p.x);
}
inline bool triangle(const CollisionWall& a,float room_height,NJS_POINT3 p,float radius=0,float actor_height=0){
    const auto base=a.position,span=a.extent;
    if(base.y>p.y+actor_height || base.y+height(a,room_height)<p.y)return false;
    const float lx=span.x>0?base.x:(base.x+span.x)-radius;
    const float hx=span.x>0?radius+(base.x+span.x):base.x;
    const float lz=span.z>0?base.z:(base.z+span.z)-radius;
    const float hz=span.z>0?radius+(base.z+span.z):base.z;
    if(lx>p.x || hx<=p.x || lz>p.z || hz<=p.z)return false;
    Point2 origin{base.x,base.z};
    if(radius && a.id<4){
        const float sx=a.id<2?1.f:-1.f,sz=(a.id&1)?-1.f:1.f;
        Point2 band[4];
        if(a.id&1){
            band[0]={base.x+sx*radius,(base.z+span.z)-radius};
            band[2]={sx*radius+(base.x+span.x),base.z-radius};
            band[1]={band[0].x-sx*radius,band[0].z+radius};
            band[3]={band[2].x-sx*radius,band[2].z+radius};
        }else{
            band[0]={base.x+span.x,base.z};
            band[2]={base.x,base.z+span.z};
            band[1]={band[0].x+sx*radius,band[0].z+radius};
            band[3]={band[2].x+sx*radius,band[2].z+radius};
        }
        if(strip({p.x,p.z},band))return true;
        origin={base.x+sx*radius,base.z+sz*radius};
        if((sx>0?origin.x>p.x:origin.x<=p.x) ||
           (sz>0?origin.z>p.z:origin.z<=p.z))return false;
    }
    // Preserve division order and the excluded diagonal, including float
    // boundary behaviour: a cross-product substitute is not equivalent.
    const float diagonal=std::fabs(span.z/(span.x/(p.x-origin.x)));
    return !(std::fabs((origin.z+span.z)-p.z)<=diagonal);
}
inline float ramp(const CollisionWall& a,NJS_POINT3 p){
    const auto o=a.position,s=a.extent;
    // Actual ramp height, including zero; never the room-height fallback.
    switch(a.id){
    case 0:return o.y+(s.y/s.x)*(p.x-o.x);
    case 1:return s.y+(o.y+(s.y/s.z)*(o.z-p.z));
    case 2:return o.y+(s.y/s.z)*(p.z-o.z);
    case 3:return s.y+(o.y+(s.y/s.x)*(o.x-p.x));
    default:throw std::runtime_error("ATR ramp direction outside 0..3");
    }
}
}
