#include "geometry_motion.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static bool near(float a,float b){return std::fabs(a-b)<.003f;}
struct Fixture {
    unsigned flags=0x119,status=0,flags2=0,control=0,system=0,mode=1,sound=0,foot=0;
    int floor=0,count=0,index=-1,floor_index=-1,yaw=0;
    std::optional<std::size_t> step;
    std::array<float,32> heights{};
    std::vector<CollisionWall> walls,steps,floors;
    Fixture(){heights[0]=-10000;heights[1]=-1000;heights[3]=100;}
    MotionActor actor(){return {flags,status,flags2,control,system,mode,floor,count,index,sound,foot,step,yaw,true,{}};}
    MotionTerrain terrain(){return {walls,steps,floors,heights,100};}
    bool resolve(NJS_POINT3& p,NJS_POINT3 old,float radius=1,float height=16.5f){return resolve_motion_wall(terrain(),actor(),p,old,radius,height);}
};
int main(){try{
    _Make_SinTable();const CollisionWall wall{1,0,0,0,0,{0,0,0},{10,0,10}};
    // Old-position quadrants and old-position distances, not closest new
    // side, fixed skin margins, iterative passes or arbitrary substeps.
    for(unsigned type:{0u,1u}){Fixture f;auto a=wall;a.type=type;f.walls={a};
        NJS_POINT3 p{6,0,5};check(f.resolve(p,{-2,0,5})&&p.x==-1&&p.z==5&&(f.status&1),"old side/distance box response missing");
        p={4,0,5};check(f.resolve(p,{12,0,5})&&p.x==11,"reverse old box side lost");
        p={5,0,6};check(f.resolve(p,{5,0,-2})&&p.z==-1,"box z response missing");
        p={5,0,4};check(f.resolve(p,{5,0,12})&&p.z==11,"reverse box z response missing");
        p={-.8f,0,-.8f};f.count=4;check(f.resolve(p,{-2,0,-2})&&p.x==-.8f&&p.z==-.8f&&f.count==0,"outside rounded circle must still count box overlap without inventing a push");
        p={-.4f,0,-.4f};check(f.resolve(p,{-2,0,-2})&&near(p.x,-.7071f)&&near(p.z,-.7071f),"original rounded corner projection missing");
        p={11,0,5};check(!f.resolve(p,{12,0,5}),"upper expanded box bound must be exclusive");
        f.flags=0;p={5,7.1234f,5};f.walls.clear();check(!f.resolve(p,p)&&p.y==7.1234f,"WallEx invents vertical rounding or grand snap without flag100");
        f.flags=0x100;check(!f.resolve(p,p)&&p.y==0,"original grand snap on no hit missing");
    }
    {Fixture f;f.walls={wall};auto a=wall;a.attributes=1;a.floor=1;f.walls={a};
        NJS_POINT3 p{-0.1f,0,5};check(!f.resolve(p,{-2,0,5}),"floor filter missing");
        f.floor=1;check(f.resolve(p,{-2,0,5}),"matching floor filtered out");
        a.attributes=2;f.walls={a};f.flags=0x4000;p={-.1f,0,5};check(!f.resolve(p,{-2,0,5}),"flag4000 filter missing");
        a.attributes=4;f.walls={a};f.flags=0;f.status=0x40000000;p={-.1f,0,5};check(!f.resolve(p,{-2,0,5}),"player status attr4 filter missing");
        a.attributes=0;a.type=1;f.walls={a};f.flags=0x400;p={-.1f,0,5};check(!f.resolve(p,{-2,0,5}),"odd type filter missing");
        a.flags=0;f.walls={a};f.flags=0;p={-.1f,0,5};check(!f.resolve(p,{-2,0,5}),"disabled wall accepted");
    }
    for(unsigned type:{2u,3u}){Fixture f;f.flags=0;f.walls={{1,type,0,0,0,{0,0,0},{10,10,0}}};
        NJS_POINT3 p{10,0,0};check(f.resolve(p,{12,0,0})&&near(p.x,11)&&near(p.z,0),"cylinder radius response missing");
        p={11,0,0};check(!f.resolve(p,p),"cylinder radius bound must be exclusive");
        p={10,-17,0};check(!f.resolve(p,p),"vertical body overlap gate missing");
        f.walls[0].attributes=0x40;f.status=0x4000;p={10,0,0};check(!f.resolve(p,p),"attr40/status4000 cylinder exclusion missing");
        f.status=0x40000000;f.yaw=16384;unsigned calls=0;auto s=f.actor();s.effect29=[&](CollisionWall& a,std::size_t){check(a.attributes&0x40,"effect receives wrong ATR");++calls;return true;};
        p={10,0,0};check(resolve_motion_wall(f.terrain(),s,p,p,1,16.5f)&&calls==1&&!(f.walls[0].attributes&0x40)&&f.mode==2&&(f.flags&0x10004)==0x10004&&(f.system&4),"wall attr40 native side effects missing");
    }
    for(unsigned type:{4u,5u})for(unsigned id=0;id<4;++id){Fixture f;f.flags=0;const float sx=id<2?1.f:-1.f,sz=(id&1)?-1.f:1.f;
        f.walls={{1,type,id,0,0,{0,0,0},{10*sx,10,10*sz}}};
        NJS_POINT3 p{2*sx,1,2*sz};check(f.resolve(p,{9*sx,1,9*sz},0)&&near(sx*p.x+sz*p.z,10),"signed triangle infinite diagonal projection missing");
        p={-.05f*sx,1,2*sz};check(f.resolve(p,p,.1f)&&near(p.x,-.1f*sx),"triangle axis edge response missing");
        p={2*sx,1,-.05f*sz};check(f.resolve(p,p,.1f)&&near(p.z,-.1f*sz),"second triangle axis response missing");
        p={8*sx,1,8*sz};check(!f.resolve(p,p,0),"triangle empty half filled");
    }
    {Fixture f;f.flags=0;f.walls={{1,7,0,0,0,{0,0,0},{10,-5,10}}};
        NJS_POINT3 p{6,-2,5};check(f.resolve(p,{-2,-2,5})&&p.x==6&&p.z==11,"type7 previous side and candidate distance response wrong");
        p={5,0,5};check(!f.resolve(p,p),"standing ON a floor must not hit its underside");
        f.flags=0x100;p={5,-6,5};check(f.resolve(p,p,1,2),"type7 strict under-slab crossing missing");
        p={5,-5,5};check(!f.resolve(p,p,1,2),"type7 strict lower-face equality must not hit");
        f.flags=0x100;f.walls[0].extent.y=0;p={-.1f,99,5};check(f.resolve(p,{-2,99,5},1,2),"wall resolution's zero-height slab must inherit room height");
    }
    // Main/direct and Ex differ: Ex deliberately has no ramp branch.
    for(unsigned id=0;id<4;++id){Fixture f;f.flags=0x100;f.walls={{1,6,id,0,0,{0,0,0},{20,10,40}}};
        NJS_POINT3 p{5,1.1234f,10};check(!f.resolve(p,p)&&p.y==0,"WallEx must not invent a ramp branch");
        p={5,1,10};check(resolve_motion_wall_direct(f.terrain(),f.actor(),p,0,0)&&(f.status&1),"direct wall ramp projection missing");
    }
    {Fixture f;f.flags=0;NJS_POINT3 p{0,1.2345f,0};resolve_motion_wall_direct(f.terrain(),f.actor(),p,1,1);
        check(p.y==1.23f,"main/direct Y quantization missing");}
    {Fixture f;f.flags=0;f.status=0x80;f.yaw=16384;auto a=wall;a.attributes=0x10000;f.walls={a};
        NJS_POINT3 p{10.5f,0,5};check(f.resolve(p,{12,0,5})&&f.count==1&&f.index==0&&(f.walls[0].attributes&0x20000),"push contact count/index/ATR mutation missing");
        f.walls[0].attributes|=0x200000;p={10.5f,0,5};check(f.resolve(p,{12,0,5})&&f.count==0,"push directional exclusion missing");
    }
    // Step lookup is inclusive, takes first enabled matching type2/prm0;
    // limits are appended after real walls and do not escape the query.
    {Fixture f;f.flags=0;f.steps={{1,2,0,0,0,{0,0,0},{10,0,10},{1,0,0,0}}};
        check(motion_step_at(f.steps,0,10,10)==0&&!motion_step_at(f.steps,1,5,5),"step floor/edge lookup wrong");
        f.status=0x20000;f.step=0;NJS_POINT3 p{5,0,-.1f};check(f.resolve(p,{5,0,2},1)&&p.z==.5f&&f.step==0&&f.walls.empty(),"temporary front edge limit missing or leaked into the room table");
        f.status=0x20000;p={5,0,10.1f};check(f.resolve(p,{5,0,8},1)&&p.z==9.5f,"cached step / back edge limit missing");
        f.steps.clear();f.status=0x20000;p={5,0,5};check(!f.resolve(p,p)&&!(f.status&0x20000)&&!f.step,"removed step identity leaves a stale limit");
    }
    {Fixture f;f.flags=0;f.steps={{1,2,0,0,0,{0,0,0},{10,0,10},{1,0,0,0}}};
        check_motion_step(f.terrain(),f.actor(),{5,0,5});check(f.status&0x20000,"step status missing");
        f.status=0x30;check_motion_step(f.terrain(),f.actor(),{5,0,5});check(!(f.status&0x20000),"climbing modes must suppress CheckDansa status");
        f.floors={{1,0,0,0,1,{0,0,0},{10,0,10},{}}};
        check_motion_floor(f.terrain(),f.actor(),{5,0,8},{5,0,8},2,1,f.floor_index);check((f.control&0x200)&&f.floor_index==0,"original safe-point/arn floor trigger missing");
        f.floors[0].attributes|=0x1000;check_motion_floor(f.terrain(),f.actor(),{5,0,8},{5,0,8},2,1,f.floor_index);check(!(f.control&0x200),"floor direction exclusion missing");
        f.floors={{1,3,0,0,0,{0,0,0},{10,5,10},{1,0,0,0}},{1,3,0,0,0,{0,0,0},{10,5,10},{3,0,0,0}},{1,1,0,0,0,{0,0,0},{10,5,10},{7,0,0,0}}};
        check_motion_floor(f.terrain(),f.actor(),{5,1,5},{5,1,5},0,1,f.floor_index);
        check((f.status&0x100000)&&(f.flags2&8)&&f.foot==2&&f.sound==7,"floor water/foot effect/sound work missing");
        f.floors.clear();check_motion_floor(f.terrain(),f.actor(),{20,1,20},{20,1,20},0,1,f.floor_index);check(!(f.status&0x100000)&&!(f.flags2&8)&&!(f.status&0x20000),"floor/step work not cleared on departure");
    }
    std::cout<<"Native motion wall/direct/floor/step contracts passed; no original module or host substep solver.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
