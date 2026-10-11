#include "effect_collision.h"
#include "geometry_line.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool near(float a,float b,float tolerance=.002f){return std::fabs(a-b)<tolerance;}
int main(){try{
    _Make_SinTable();
    const CollisionWall box{1,0,0,0,0,{0,0,0},{10,10,10}};
    // All six box faces, and a zero-height box inherits room height.
    const NJS_POINT3 origins[6]={{5,15,5},{5,5,-5},{-5,5,5},{5,5,15},{15,5,5},{5,-5,5}};
    const NJS_POINT3 ends[6]={{5,5,5},{5,5,5},{5,5,5},{5,5,5},{5,5,5},{5,5,5}};
    const NJS_POINT3 cuts[6]={{5,10,5},{5,5,0},{0,5,5},{5,5,10},{10,5,5},{5,0,5}};
    const NJS_POINT3 normals[6]={{0,1,0},{0,0,-1},{-1,0,0},{0,0,1},{1,0,0},{0,-1,0}};
    for(unsigned type:{0u,1u})for(unsigned i=0;i<6;++i){
        auto a=box;a.type=type;auto end=ends[i];NJS_POINT3 n{99,99,99};
        check(effect_line({a},100,origins[i],end,n).has_value(),"solid line face missing");
        check(near(end.x,cuts[i].x)&&near(end.y,cuts[i].y)&&near(end.z,cuts[i].z),"solid line clipped at wrong face");
        check(n.x==normals[i].x&&n.y==normals[i].y&&n.z==normals[i].z,"axis normal is not the original unit normal");
    }
    {auto a=box;a.extent.y=0;NJS_POINT3 end{5,5,5},n{};
        check(effect_line({a},10,{5,15,5},end,n)&&end.y==10,"zero-height line box ignores room height");}
    // A segment ending ON the face does not cross it; rectangle edges are
    // inclusive, unlike point-contact footprints and polygon segment ends.
    {NJS_POINT3 end{0,5,5},n{7,8,9};check(!effect_line({box},100,{-5,5,5},end,n)&&
        end.x==0&&n.x==7&&n.y==8&&n.z==9,"line endpoint or miss output contract incorrect");
        end={5,10,10};check(effect_line({box},100,{-5,10,10},end,n)&&end.x==0,"box rectangle upper edge incorrectly exclusive");
        end={20,5,20};check(!effect_line({box},100,{15,5,15},end,n),"box broad phase accepted an outside segment");
        end={5,5,5};check(!effect_line({box},100,end,end,n),"zero-length segment invents a hit");}
    // Original table traversal clips the same endpoint repeatedly: nearest
    // accepted cut wins, equal cuts keep the first entry, not the last table.
    {auto far=box,close=box;far.position.x=10;far.id=1;close.position.x=0;close.id=2;
        NJS_POINT3 end{30,5,5},n{};auto hit=effect_line({far,close},100,{-5,5,5},end,n);
        check(hit&&hit->id==2&&end.x==0,"later nearer line cut lost");
        end={30,5,5};hit=effect_line({close,far},100,{-5,5,5},end,n);
        check(hit&&hit->id==2&&end.x==0,"farther line entry overrides the nearer cut");
        close.id=3;close.position=far.position;end={30,5,5};hit=effect_line({far,close},100,{-5,5,5},end,n);
        check(hit&&hit->id==1,"equal segment cuts do not preserve the first entry");}
    // Explicitly separate unfiltered Line1 from Line2/3's gates.
    {auto a=box;a.type=1;a.attributes=3;a.floor=2;NJS_POINT3 n{};
        auto query=[&](unsigned flags,int floor,bool attr4=false){NJS_POINT3 end{5,5,5};return bool(effect_line({a},100,{-5,5,5},end,n,flags,floor,attr4));};
        NJS_POINT3 end{5,5,5};check(effect_line({a},100,{-5,5,5},end,n).has_value(),"Line1 applies invented filters");
        check(!query(0x400,2)&&!query(0,1)&&!query(0x4000,2)&&query(0,2),"Line2 flags/floor gates wrong");
        a.attributes=4;check(query(0,0)&&!query(0,0,true),"Line3 attribute-4 omission wrong");
        a.attributes=1;a.floor=-56;check(query(0,200),"line floor operand is not the original signed byte");
        a.flags=0;check(!query(0,-56),"disabled line geometry accepted");}
    // Slab type 7 uses actual thickness and bypasses Line2 solid/floor gates.
    {CollisionWall floor{1,7,0,3,3,{0,0,0},{10,-5,10}};NJS_POINT3 n{},end{5,-10,5};
        check(effect_line({floor},100,{5,10,5},end,n,0x4400,0)&&end.y==0&&n.y==1,"floor down crossing or filter bypass wrong");
        end={5,5,5};check(effect_line({floor},100,{5,-10,5},end,n)&&end.y==-5&&n.y==-1,"floor up crossing wrong");
        end={5,-2,5};check(effect_line({floor},100,{-5,-2,5},end,n)&&end.x==0,"floor left side missing");
        end={5,-2,5};check(!effect_line({floor},100,{15,-2,5},end,n),"original contradictory right-side gate was silently fixed");
        floor.extent.y=0;end={5,-2,5};check(!effect_line({floor},100,{-5,-2,5},end,n),"zero-thickness floor invents a side wall");
        floor.attributes=4;end={5,-10,5};check(!effect_line({floor},100,{5,10,5},end,n,0,0,true),"Line3 attr4 gate must also apply to type7");}
    // Triangle normal winding and radius-independent line faces, all signs.
    for(unsigned id=0;id<4;++id){const float sx=id<2?1.f:-1.f,sz=(id&1)?-1.f:1.f;
        CollisionWall a{1,4,id,0,0,{0,0,0},{10*sx,10,10*sz}};NJS_POINT3 n{},end{2*sx,5,2*sz};
        check(effect_line({a},100,{2*sx,15,2*sz},end,n)&&near(end.y,10)&&n.y==100,
              "triangle cap or unnormalized plane normal wrong");
        end={2*sx,5,2*sz};check(effect_line({a},100,{8*sx,5,8*sz},end,n)&&
            near(sx*end.x+sz*end.z,10)&&n.x*sx>0&&n.z*sz>0,"signed triangle diagonal line missing");
        end={8*sx,5,8*sz};check(!effect_line({a},100,{8*sx,15,8*sz},end,n),"triangle empty cap half blocks line");
        end={2*sx,5,2*sz};check(effect_line({a},100,{-5*sx,5,2*sz},end,n)&&near(end.x,0),"triangle perpendicular x face missing");
        end={2*sx,5,2*sz};check(effect_line({a},100,{2*sx,5,-5*sz},end,n)&&near(end.z,0),"triangle perpendicular z face missing");
        end={2*sx,5,2*sz};check(!effect_line({a},100,{2*sx,-5,2*sz},end,n),"triangle invents a bottom face");
    }
    for(unsigned id=0;id<4;++id){CollisionWall a{1,6,id,0,0,{0,0,0},{20,10,40}};NJS_POINT3 end{5,-5,10},n{};
        const float height=(id==0||id==2)?2.5f:7.5f;
        check(effect_line({a},100,{5,15,10},end,n)&&near(end.y,height)&&n.y==800,"ramp line shape or raw normal wrong");
        end={5,height,10};check(!effect_line({a},100,{5,15,10},end,n),"polygon endpoint equality incorrectly hits");
        end={5,15,10};check(!effect_line({a},100,{5,-5,10},end,n),"one-sided ramp is double-sided");}
    // Cylinders have eight flat sides, not analytic circle intersections.
    for(unsigned type:{2u,3u}){CollisionWall a{1,type,0,0,0,{0,0,0},{10,10,0}};NJS_POINT3 n{},end{0,5,5};
        check(effect_line({a},100,{20,5,5},end,n)&&near(end.x,7.9289f,.02f),"cylinder side was replaced by an analytic circle");
        end={0,5,0};check(!effect_line({a},100,{0,-5,0},end,n),"cylinder invents a bottom cap");
        // Downward cap: only the complete octagon of the last loop pass decides
        // (stale points of passes 0..6 never enclose; user decision 2026-10-11).
        end={0,-5,0};n={};
        check(effect_line({a},100,{0,15,0},end,n)&&end.y==10&&near(end.x,0)&&near(end.z,0)&&n.y>0&&n.x==0&&n.z==0,"downward cylinder cap misses its centre");
        const float c=0.9238795f,s=0.3826834f; // 22.5 degrees: octagon apothem 9.239
        end={9*c,-5,-9*s};check(effect_line({a},100,{9*c,15,-9*s},end,n)&&end.y==10,"cap misses inside the octagon");
        end={9.6f*c,5,-9.6f*s};check(!effect_line({a},100,{9.6f*c,15,-9.6f*s},end,n),"cap is a circle instead of the original octagon");
        end={9.5f,5,9.5f};check(!effect_line({a},100,{9.5f,15,9.5f},end,n),"stale cap points produce a phantom hit outside the octagon");}
    // Reflection: box side/top ties, cylinder sides/top, signed triangles,
    // both orthogonal edges, all ramps, and floors with negative thickness.
    for(unsigned type:{0u,1u}){auto a=box;a.type=type;NJS_POINT3 p{.01f,2,5};int angle=8192;
        reflect_effect_wall(a,100,p,.1f,.1f,angle);check(near(p.x,-.1f)&&angle==-8192,"box x reflection wrong");
        p={5,2,.01f};angle=8192;reflect_effect_wall(a,100,p,.1f,.1f,angle);check(near(p.z,-.1f)&&angle==24576,"box z reflection wrong");
        p={5,9.99f,5};angle=8192;reflect_effect_wall(a,100,p,.1f,.1f,angle);check(p.y==10&&angle==8192,"box top must not reflect angle");
        p={5,4.9f,5};angle=8192;reflect_effect_wall(a,100,p,.1f,.1f,angle);check(near(p.x,10.1f)&&angle==-8192,"box equal side/top distances must resolve x");}
    for(unsigned type:{2u,3u}){CollisionWall a{1,type,0,0,0,{0,0,0},{10,10,0}};NJS_POINT3 p{10,1,0};int angle=8192;
        reflect_effect_wall(a,100,p,1,1,angle);check(near(p.x,11)&&near(p.z,0)&&angle==-8192,"cylinder radial response wrong");
        p={9.5f,9.99f,0};angle=8192;reflect_effect_wall(a,100,p,1,1,angle);check(p.y==10&&angle==8192,"cylinder top response wrong");}
    for(unsigned id=0;id<4;++id){const float sx=id<2?1.f:-1.f,sz=(id&1)?-1.f:1.f;
        CollisionWall a{1,4,id,0,0,{0,0,0},{10*sx,10,10*sz}};
        NJS_POINT3 p{3*sx,1,3*sz};int angle=8192;
        reflect_effect_wall(a,100,p,0,0,angle);check(near(sx*p.x+sz*p.z,10),"triangle reflection does not project to infinite diagonal");
        p={3*sx,9.99f,3*sz};angle=8192;reflect_effect_wall(a,100,p,0,0,angle);check(p.y==10&&angle==8192,"triangle top response wrong");
        p={-.05f*sx,1,2*sz};angle=8192;reflect_effect_wall(a,100,p,.1f,1,angle);
        check(near(p.x,-.1f*sx)&&angle==-8192,"triangle perpendicular x edge response wrong");
        p={2*sx,1,-.05f*sz};angle=8192;reflect_effect_wall(a,100,p,.1f,1,angle);
        check(near(p.z,-.1f*sz)&&angle==-8192,"triangle perpendicular z edge must also negate original angle");}
    for(unsigned id=0;id<4;++id){CollisionWall a{1,6,id,0,0,{0,0,0},{20,10,40}};
        const float h=(id==0||id==2)?2.5f:7.5f;NJS_POINT3 p{5,h-.01f,10};int angle=8192;
        reflect_effect_wall(a,100,p,.1f,1,angle);check(p.y==h&&angle==8192,"ramp surface response wrong");
        p={5,h+1,10};reflect_effect_wall(a,100,p,.1f,1,angle);check(p.y==h+1,"ramp above-surface probe modified");}
    {CollisionWall a{1,7,0,0,0,{0,0,0},{10,-5,10}};NJS_POINT3 p{5,-.01f,5};int angle=8192;
        reflect_effect_wall(a,100,p,.1f,1,angle);check(p.y==0&&angle==8192,"floor reflection uses py+h instead of py");}
    // Plane/line contracts: infinite projection and parallel fallback.
    {NJS_POINT3 closest;check(near(line_geometry::point_line({3,4,0},{0,0,0},{2,0,0},closest),4)&&closest.x==3,
          "point-line operation clamps to segment or uses nonunit projection");
        check(line_geometry::line_plane({0,2,0},{1,0,0},{0,0,0},{0,2,0},closest)==2&&closest.y==0,
              "parallel line-plane fallback wrong");
        check(line_geometry::line_plane({0,2,0},{1,-1,0},{0,0,0},{0,2,0},closest)==0&&near(closest.x,2)&&near(closest.y,0),
              "line-plane intersection wrong");}
    std::cout<<"Native ATR line/reflection variants passed; downward cylinder caps use the complete octagon.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
