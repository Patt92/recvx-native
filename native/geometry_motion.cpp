#include "geometry_motion.h"
#include "geometry_atr.h"
#include "geometry_line.h"
#include "ninja_math.h"
#include <cmath>
#include <stdexcept>
namespace recvx {
namespace {
bool inclusive(const CollisionWall& a,float x,float z){return a.position.x<=x&&x<=a.position.x+a.extent.x&&a.position.z<=z&&z<=a.position.z+a.extent.z;}
float floor_height(const MotionTerrain& t,int floor){if(floor< -2||floor>29)throw std::runtime_error("motion floor outside original grand table");return t.heights[std::size_t(floor+2)];}
struct BoxHit {bool overlap=false,hit=false,rounded=false;};
// Factor the original quadrants, not a generic polygon separation heuristic.
BoxHit box(const CollisionWall& a,float room_height,NJS_POINT3& p,NJS_POINT3 previous,float r,float h,unsigned mask,bool previous_side,bool previous_distance,bool rounded=true){
    if(!(atr_geometry::rectangle(a,p,r)&&p.y+h>=a.position.y&&p.y<=a.position.y+atr_geometry::height(a,room_height)))return {};
    const auto side=previous_side?previous:p;
    const bool left=side.x<a.position.x+.5f*a.extent.x,front=side.z<a.position.z+.5f*a.extent.z;
    const float x0=a.position.x-r,z0=a.position.z-r;
    const float x=left?x0:x0+(a.extent.x+2.f*r),z=front?z0:z0+(a.extent.z+2.f*r);
    const unsigned corner=(left?1u:4u)<<(front?0:1);
    if(rounded&&std::fabs(p.x-x)<r&&std::fabs(p.z-z)<r&&!(mask&corner)){
        const float cx=a.position.x+(left?0:a.extent.x),cz=a.position.z+(front?0:a.extent.z);
        const float dx=cx-p.x,dz=cz-p.z;
        if(!(njSqrt(dx*dx+dz*dz)<=r))return {true,false,true};
        float sine,cosine;njSinCos(int(10430.381f*std::atan2(dx,dz)),&sine,&cosine);
        p.x=cx-sine*r;p.z=cz-cosine*r;return {true,true,true};
    }
    const auto distance=previous_distance?previous:p;
    if(std::fabs(distance.x-x)>std::fabs(distance.z-z))p.z=z;else p.x=x;
    return {true,true,false};
}
bool solid_enabled(const CollisionWall& a,const MotionActor& s){return !((a.type&1)&&(s.flags&0x400))&&
    (!(a.attributes&1)||a.floor==s.floor)&&!(a.attributes&2&&s.flags&0x4000)&&!(a.attributes&4&&s.status&0x40000000u);}
std::vector<CollisionWall> limits(const MotionTerrain& t,MotionActor s,NJS_POINT3 p,float r){
    std::vector<CollisionWall> result;
    if(!(s.status&0x20000))return result;
    if(const auto found=motion_step_at(t.steps,s.floor,p.x,p.z))s.step=found;
    if(!s.step||*s.step>=t.steps.size()){s.step.reset();s.status&=~0x20000u;return result;}
    const auto& a=t.steps[*s.step];const float twice=2.f*r,half=.5f*r;
    // Front/right/back/left probes and temporary ATR walls, discarded after
    // this query. The remembered ETC identity remains with its native owner.
    for(unsigned side=0;side<4;++side){
        const float x=p.x+(side==1?r:side==3?-r:0),z=p.z+(side==2?r:side==0?-r:0);
        if(motion_step_at(t.steps,s.floor,x,z))continue;
        CollisionWall w{1,1,0,s.floor,0x800000,a.position,{}, {}};
        if(side==0||side==2){w.position.x=a.position.x-twice;
            w.position.z=side==0?a.position.z-(twice+half):half+(a.position.z+a.extent.z);
            w.extent={a.extent.x+4.f*r,1,twice};}
        else{w.position.x=side==1?half+(a.position.x+a.extent.x):a.position.x-(twice+half);
            w.position.z=a.position.z-twice;w.extent={twice,1,a.extent.z+4.f*r};}
        result.push_back(w);
    }
    return result;
}
bool query(MotionTerrain t,MotionActor s,NJS_POINT3& p,NJS_POINT3 previous,float radius,float height,bool direct){
    if(direct)p.y=float(int(100.f*(.001f+p.y)))/100.f;
    auto extra=limits(t,s,p,radius);unsigned hits=0;
    auto visit=[&](CollisionWall& source,std::size_t index){
        if(!(source.flags&1))return;
        auto a=source;if(direct)a.position.y=float(int(100.f*(.001f+a.position.y)))/100.f;
        const float h=atr_geometry::height(a,t.room_height);
        if(a.type<=5&&!solid_enabled(a,s))return;
        switch(a.type){
        case 0:case 1:{
            // Side and rounded-corner classification use the untouched query
            // position; contact metadata is evaluated before resolution.
            if(!(atr_geometry::rectangle(a,p,radius)&&p.y+height>=a.position.y&&p.y<=a.position.y+h))break;
            if(!direct){
                if((a.attributes&0x10000)&&s.player){
                    const unsigned angle=(unsigned(s.yaw)+8192u)&~0x3fffu;
                    const unsigned masks[4]={0x40000,0x200000,0x100000,0x80000};
                    float sine,cosine;njSinCos(int(angle),&sine,&cosine);
                    if((a.attributes&masks[(angle&0xc000)>>14])||!inclusive(a,p.x-5.f*sine,p.z-5.f*cosine))s.push_count=0;
                    else ++s.push_count;
                }
                if(!(a.attributes&0x20000)&&(s.status&0x80)){source.attributes|=0x20000;s.push_index=int(index);}
            }
            const auto result=box(a,t.room_height,p,previous,radius,height,0,!direct,!direct);
            ++hits; // Even a square-corner overlap outside the rounded circle.
            if(result.rounded)s.push_count=0;
            break;
        }
        case 2:case 3:{
            if((a.attributes&0x40)&&(s.status&0x4000))break;
            const float dx=a.position.x-p.x,dz=a.position.z-p.z,reach=radius+a.extent.x;
            if(!(njSqrt(dx*dx+dz*dz)<reach&&p.y+height>=a.position.y&&p.y<=a.position.y+h))break;
            ++hits;
            if((a.attributes&0x40)&&(s.status&0x40000000u)&&!(s.flags&6)){
                float sine,cosine;njSinCos(s.yaw,&sine,&cosine);NJS_VECTOR direction{dx,0,dz};njUnitVector(&direction);
                s.mode=(-sine*direction.x-cosine*direction.z)>0?2:0x102;
                s.flags|=0x10004;s.system|=4;
                if(!s.effect29)throw std::runtime_error("motion wall attribute 0x40 requires native effect29 owner");
                if(s.effect29(source,index))source.attributes&=~0x40u;
            }
            float sine,cosine;njSinCos(int(10430.381f*std::atan2(dx,dz)),&sine,&cosine);
            p.x=a.position.x-sine*reach;p.z=a.position.z-cosine*reach;break;
        }
        case 4:case 5:{
            if(a.id>3)throw std::runtime_error("motion triangle direction outside 0..3");
            // Inner triangle uses the original unquantized ATR record.
            if(atr_geometry::triangle(source,t.room_height,p,radius,height)){
                const float sx=a.id<2?1.f:-1.f,sz=(a.id&1)?-1.f:1.f;NJS_POINT3 closest;
                line_geometry::point_line(p,{a.position.x+sx*radius,0,a.position.z+sz*radius+a.extent.z},{a.extent.x,0,-a.extent.z},closest);
                p.x=closest.x;p.z=closest.z;++hits;
            }else{
                auto vertical=a;vertical.extent.x=0;vertical.extent.y=h;
                if(a.id&1){vertical.position.z+=a.extent.z;vertical.extent.z=-a.extent.z;}
                auto horizontal=a;horizontal.extent.z=0;horizontal.extent.y=h;
                if(a.id>=2){horizontal.position.x+=a.extent.x;horizontal.extent.x=-a.extent.x;}
                if(box(vertical,t.room_height,p,p,radius,height,a.id<2?12:3,false,false).hit)++hits;
                if(box(horizontal,t.room_height,p,p,radius,height,(a.id&1)?5:10,false,false).hit)++hits;
            }break;
        }
        case 6:{
            if(!direct||!(s.flags&0x100)||!atr_geometry::rectangle(a,p)||!(p.y+height>=a.position.y&&p.y<=a.position.y+h))break;
            NJS_POINT3 origin{},direction{},closest;
            switch(a.id){
            case 0:origin={a.position.x,a.position.y,p.z};direction={a.extent.x,h,0};break;
            case 1:origin={p.x,a.position.y,a.position.z+a.extent.z};direction={0,h,-a.extent.z};break;
            case 2:origin={p.x,a.position.y,a.position.z};direction={0,h,a.extent.z};break;
            case 3:origin={a.position.x+a.extent.x,a.position.y,p.z};direction={-a.extent.x,h,0};break;
            default:throw std::runtime_error("motion ramp direction outside 0..3");}
            line_geometry::point_line(p,origin,direction,closest);p.y=closest.y;++hits;
            for(unsigned j=0;j<31;++j)if((t.heights[j]||j==2)&&p.y>=t.heights[j])s.floor=int(j)-2;
            const float ground=floor_height(t,s.floor);
            if(s.floor!=a.floor){if(p.y<ground){p.x=closest.x;p.z=closest.z;}else p.y=ground;}
            else{p.x=closest.x;p.z=closest.z;if(p.y<ground)p.y=ground;}break;
        }
        case 7:{
            if((a.attributes&1)&&a.floor!=s.floor)break;
            const float lower=a.position.y+h;
            if(!atr_geometry::rectangle(a,p,radius)||!((s.flags&0x100)?(p.y<lower&&p.y+height>lower):(p.y+height>=lower&&p.y<a.position.y)))break;
            // Floor response has no rounded corners and chooses its side
            // from the old position, but distances from the new position.
            auto footprint=a;footprint.position.y=p.y;footprint.extent.y=height;
            box(footprint,t.room_height,p,previous,radius,height,15,!direct,false,false);++hits;break;
        }
        default:break;
        }
    };
    for(std::size_t i=0;i<t.walls.size();++i)visit(t.walls[i],i);
    for(std::size_t i=0;i<extra.size();++i)visit(extra[i],t.walls.size()+i);
    if(hits)s.status|=1;
    else if(s.flags&0x100)p.y=floor_height(t,s.floor);
    return hits!=0;
}
}
std::optional<std::size_t> motion_step_at(const std::vector<CollisionWall>& steps,int floor,float x,float z){
    for(std::size_t i=0;i<steps.size();++i){const auto& a=steps[i];
        if((a.flags&1)&&a.type==2&&a.parameters[0]&&a.floor==floor&&inclusive(a,x,z))return i;}
    return {};
}
bool resolve_motion_wall(MotionTerrain t,MotionActor s,NJS_POINT3& p,NJS_POINT3 previous,float radius,float height){return query(t,s,p,previous,radius,height,false);}
bool resolve_motion_wall_direct(MotionTerrain t,MotionActor s,NJS_POINT3& p,float radius,float height){return query(t,s,p,p,radius,height,true);}
void check_motion_step(MotionTerrain t,MotionActor s,NJS_POINT3 p){
    s.status&=~0x20000u;
    if(!(s.status&0x30)&&motion_step_at(t.steps,s.floor,p.x,p.z))s.status|=0x20000;
}
void check_motion_floor(MotionTerrain t,MotionActor s,NJS_POINT3 p,NJS_POINT3 safe,float offset,float radius,int& floor_index){
    float sine,cosine;njSinCos(s.yaw,&sine,&cosine);
    const float x=safe.x-sine*(3.f+(3.f-offset)),z=safe.z-cosine*(3.f+(3.f-offset));
    s.status&=~0x100000u;s.flags2&=~8u;s.control&=~0x8000200u;
    const unsigned angle=(unsigned(s.yaw)+8192u)&0xc000;
    const unsigned masks[4]={0x1000,0x800,0x400,0x2000};
    for(std::size_t i=0;i<t.floors.size();++i){const auto& a=t.floors[i];if(!(a.flags&1))continue;
        const bool forward=a.type==0&&(a.attributes&1);
        const bool inside=forward?(inclusive(a,x,z)&&a.floor==s.floor):
            (inclusive(a,p.x,p.z)&&a.position.y<=p.y&&p.y<=a.position.y+a.extent.y);
        if(!inside)continue;
        switch(a.type){
        case 0:if(!(s.flags&6)&&!(a.attributes&masks[angle>>14])){s.control|=0x200;floor_index=int(i);}break;
        case 1:if(!(a.attributes&1)||(s.status&0x10))s.floor_sound=a.parameters[0];break;
        case 3:switch(a.parameters[0]){
            case 1:s.status|=0x100000;break;
            case 3:s.flags2|=8;s.foot_effect=2;break;
            case 4:floor_index=int(i);if(s.flags&0x10000000)s.control|=0x8000000;break;
            default:break;}break;
        default:break;
        }
    }
    s.status&=~0x20000u;
    if(motion_step_at(t.steps,s.floor,p.x,p.z)||motion_step_at(t.steps,s.floor,p.x,p.z-radius)||
       motion_step_at(t.steps,s.floor,p.x+radius,p.z)||motion_step_at(t.steps,s.floor,p.x,p.z+radius)||
       motion_step_at(t.steps,s.floor,p.x-radius,p.z))s.status|=0x20000;
}
}
