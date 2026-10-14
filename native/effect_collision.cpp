#include "effect_collision.h"
#include "geometry_atr.h"
#include "geometry_line.h"
#include <cmath>
#include <type_traits>
namespace recvx {
namespace {
const CollisionWall& wall_view(const CollisionWall& a){return a;}
CollisionWall wall_view(const RECVX_STAIR_AREA& a){
    return {a.flg,a.type,a.id,a.flr_no,a.attr,{a.px,a.py,a.pz},{a.w,a.h,a.d},
        {a.prm0,a.prm1,a.prm2,a.prm3}};
}
// hitchk.c:3557: stable room-then-mobile traversal and strict closest-gap
// replacement. Odd solid types deliberately do not contribute ground.
struct GroundQuery {
    NJS_POINT3 probe;
    float room_height;
    float closest=10000.f,result=-1000.f;
    std::optional<std::pair<GroundHit::Table,std::size_t>> entry;
    template<typename T>void visit(const std::vector<T>& table,GroundHit::Table owner){
        for(std::size_t i=0;i<table.size();++i){
            const auto& a=wall_view(table[i]);
            if(!(a.flags&1))continue;
            float surface=0,gap=0;bool inside=false;
            switch(a.type){
            case 0:
                inside=atr_geometry::rectangle(a,probe);
                surface=a.position.y+atr_geometry::height(a,room_height);break;
            case 2:{
                const float x=a.position.x-probe.x,z=a.position.z-probe.z;
                inside=std::sqrt(x*x+z*z)<a.extent.x;
                surface=a.position.y+atr_geometry::height(a,room_height);break;
            }
            case 4:
                inside=atr_geometry::triangle(a,room_height,{probe.x,a.position.y,probe.z});
                surface=a.position.y+atr_geometry::height(a,room_height);break;
            case 6:
                if(!atr_geometry::rectangle(a,probe) || !(probe.y-a.position.y>=0))continue;
                surface=atr_geometry::ramp(a,probe);gap=probe.y-surface;
                if(gap<closest){closest=gap;result=surface;entry={{owner,i}};}
                continue; // Ramps may be above the probe; no gap clamp.
            case 7:
                inside=atr_geometry::rectangle(a,probe);surface=a.position.y;break;
            default:continue;
            }
            gap=probe.y-surface;
            if(a.type==7 && gap<=0 && gap>=-0.0001f)gap=0;
            if(inside && gap>=0 && gap<closest){closest=gap;result=surface;entry={{owner,i}};}
        }
    }
};
RECVX_STAIR_AREA record(const CollisionWall& a){
    return {static_cast<unsigned char>(a.flags),static_cast<unsigned char>(a.type),
        static_cast<unsigned char>(a.id),static_cast<signed char>(a.floor),a.attributes,
        a.position.x,a.position.y,a.position.z,a.extent.x,a.extent.y,a.extent.z,
        static_cast<unsigned char>(a.parameters[0]),static_cast<unsigned char>(a.parameters[1]),
        static_cast<unsigned char>(a.parameters[2]),static_cast<unsigned char>(a.parameters[3])};
}
bool wall_contact(const CollisionWall& a,float room_height,NJS_POINT3 p,unsigned flags,float radius,float actor_height){
    if(!(a.flags&1))return false;
    if(a.type<6 && (a.type&1) && (flags&0x400))return false;
    const bool vertical=p.y+actor_height>=a.position.y && p.y<=a.position.y+atr_geometry::height(a,room_height);
    switch(a.type){
    case 0:case 1:return vertical && atr_geometry::rectangle(a,p,radius);
    case 2:case 3:{
        const float x=p.x-a.position.x,z=p.z-a.position.z;
        return std::sqrt(x*x+z*z)<radius+a.extent.x && vertical;
    }
    case 4:case 5:return atr_geometry::triangle(a,room_height,p,radius,actor_height);
    case 6:return vertical && atr_geometry::rectangle(a,p);
    case 7:{
        const float lower=a.position.y+a.extent.y;
        return atr_geometry::rectangle(a,p,radius) && ((flags&0x100)?
            (p.y<lower && p.y+actor_height>lower):
            (p.y+actor_height>=lower && p.y<a.position.y));
    }
    default:return false;
    }
}
int reflected_angle(int axis_angle,int angle){return static_cast<std::int32_t>(std::uint32_t(axis_angle)-std::uint32_t(angle));}
// Shared box/ramp/floor response, factored from the reference's quadrants.
// Ties resolve on x; a strictly closer top changes y but not the angle.
void box_response(const CollisionWall& a,NJS_POINT3& p,float radius,float top,int& angle){
    const bool left=p.x<a.position.x+.5f*a.extent.x,front=p.z<a.position.z+.5f*a.extent.z;
    const float x0=a.position.x-radius,z0=a.position.z-radius;
    const float x=left?x0:x0+(a.extent.x+2.f*radius);
    const float z=front?z0:z0+(a.extent.z+2.f*radius);
    const float dx=std::fabs(p.x-x),dz=std::fabs(p.z-z),dy=std::fabs(p.y-top);
    if(dx>dz){if(dy<dz)p.y=top;else{p.z=z;angle=reflected_angle(32768,angle);}}
    else if(dy<dx)p.y=top;
    else{p.x=x;angle=reflected_angle(0,angle);}
}
// The two perpendicular triangle edges use bhCheckBox's original rounded
// corner mask; they do not use the general box reflection above.
bool edge_response(const CollisionWall& a,float room_height,NJS_POINT3& p,float radius,float actor_height,unsigned mask){
    if(!(atr_geometry::rectangle(a,p,radius) && p.y+actor_height>=a.position.y &&
       p.y<=a.position.y+atr_geometry::height(a,room_height)))return false;
    const bool left=p.x<a.position.x+.5f*a.extent.x,front=p.z<a.position.z+.5f*a.extent.z;
    const float x0=a.position.x-radius,z0=a.position.z-radius;
    const float side_x=left?x0:x0+(a.extent.x+2.f*radius);
    const float side_z=front?z0:z0+(a.extent.z+2.f*radius);
    const float dx=std::fabs(p.x-side_x),dz=std::fabs(p.z-side_z);
    const unsigned corner=(left?1u:4u)<<(front?0:1);
    if(dx<radius && dz<radius && !(mask&corner)){
        const float x=a.position.x+(left?0:a.extent.x),z=a.position.z+(front?0:a.extent.z);
        const auto delta=NJS_POINT3{x-p.x,0,z-p.z};
        if(!(line_geometry::length(delta)<=radius))return false;
        const int direction=int(10430.381f*std::atan2(delta.x,delta.z));
        float sine,cosine;njSinCos(direction,&sine,&cosine);
        p.x=x-sine*radius;p.z=z-cosine*radius;return true;
    }
    if(dx>dz)p.z=side_z;else p.x=side_x;
    return true;
}
float coordinate(NJS_POINT3 p,unsigned axis){return axis==0?p.x:axis==1?p.y:p.z;}
void coordinate(NJS_POINT3& p,unsigned axis,float value){if(axis==0)p.x=value;else if(axis==1)p.y=value;else p.z=value;}
// Local query state, not a static PLANE or game work. Direction used by ATR
// face dispatch remains the ORIGINAL segment direction as end is shortened.
struct LineClip {
    NJS_POINT3 start,end,direction,normal{};
    bool touched=false;
    LineClip(NJS_POINT3 a,NJS_POINT3 b):start(a),end(b),direction(line_geometry::subtract(b,a)){}
    bool bounds(float x0,float x1,float z0,float z1)const{
        return !((x0>start.x&&x0>end.x)||(x1<start.x&&x1<end.x)||
                 (z0>start.z&&z0>end.z)||(z1<start.z&&z1<end.z));
    }
    bool crosses(unsigned axis,float at,int sign)const{
        const float v=coordinate(direction,axis),a=coordinate(start,axis),b=coordinate(end,axis);
        return sign>0?(v>0&&a<=at&&b>at):(v<0&&a>=at&&b<at);
    }
    bool rectangle(unsigned axis,NJS_POINT3 anchor,NJS_POINT3 span,int outward){
        NJS_POINT3 n{};coordinate(n,axis,float(outward));NJS_POINT3 closest;
        if(line_geometry::line_plane(start,line_geometry::subtract(end,start),anchor,n,closest)!=0)return false;
        for(unsigned i=0;i<3;++i)if(i!=axis){
            const float p=coordinate(closest,i),lo=coordinate(anchor,i);
            if(!(p>=lo&&p<=lo+coordinate(span,i)))return false;
        }
        end=closest;coordinate(end,axis,coordinate(anchor,axis));normal=n;touched=true;return true;
    }
    bool polygon(const NJS_POINT3* vertices,unsigned count){
        const auto v=line_geometry::subtract(end,start);
        const auto n=line_geometry::normal(vertices[0],vertices[1],vertices[2]);
        if(!(line_geometry::dot(v,n)<0))return false;
        NJS_POINT3 closest;
        if(line_geometry::line_plane(start,v,vertices[0],n,closest)!=0)return false;
        if(!(line_geometry::dot(v,line_geometry::subtract(closest,start))>=0 &&
             line_geometry::length(v)>line_geometry::length(line_geometry::subtract(closest,start))))return false;
        for(unsigned i=0;i<count;++i){
            const auto a=line_geometry::subtract(vertices[(i+1)%count],vertices[i]);
            const auto b=line_geometry::subtract(closest,vertices[i]);
            // bhInOutCheck's order is z + (x + y), with inclusive edges.
            const auto edge=line_geometry::cross(a,b);
            if(n.z*edge.z+(n.x*edge.x+n.y*edge.y)<0)return false;
        }
        end=closest;normal=n;touched=true;return true;
    }
    void solid(const CollisionWall& a,float h){
        const auto o=a.position,s=a.extent;
        if(!bounds(o.x,o.x+s.x,o.z,o.z+s.z))return;
        auto anchor=o;
        // Ordered top/front/left/back/right/bottom, including endpoint gates.
        if(crosses(1,o.y+h,-1)){anchor.y=o.y+h;rectangle(1,anchor,s,1);}
        anchor=o;if(crosses(2,o.z,1))rectangle(2,anchor,{s.x,h,s.z},-1);
        if(crosses(0,o.x,1))rectangle(0,anchor,{s.x,h,s.z},-1);
        anchor.z=o.z+s.z;if(crosses(2,anchor.z,-1))rectangle(2,anchor,{s.x,h,s.z},1);
        anchor=o;anchor.x=o.x+s.x;if(crosses(0,anchor.x,-1))rectangle(0,anchor,{s.x,h,s.z},1);
        if(crosses(1,o.y,1))rectangle(1,o,s,-1);
    }
    void floor(const CollisionWall& a){
        const auto o=a.position,s=a.extent;
        if(!bounds(o.x,o.x+s.x,o.z,o.z+s.z))return;
        auto anchor=o;anchor.y=o.y+s.y;
        if(crosses(1,anchor.y,1))rectangle(1,anchor,s,-1);
        if(crosses(1,o.y,-1))rectangle(1,o,s,1);
        if(!s.y)return;
        const NJS_POINT3 side{s.x,-s.y,s.z};
        if(crosses(2,o.z,1))rectangle(2,anchor,side,-1);
        if(crosses(0,o.x,1))rectangle(0,anchor,side,-1);
        anchor.z=o.z+s.z;if(crosses(2,anchor.z,-1))rectangle(2,anchor,side,1);
        anchor=o;anchor.y=o.y+s.y;anchor.x=o.x+s.x;
        // Preserve the original's contradictory right-side gate, independently
        // verified at 0x26A150..0x26A19C. No invented correction for this face.
        if(direction.x<0&&start.x<=anchor.x&&end.x>anchor.x)rectangle(0,anchor,side,1);
    }
    void triangle(const CollisionWall& a,float h){
        const auto o=a.position,s=a.extent;
        if(!bounds(a.id<2?o.x:o.x+s.x,a.id<2?o.x+s.x:o.x,
                   (a.id==0||a.id==2)?o.z:o.z+s.z,(a.id==0||a.id==2)?o.z+s.z:o.z))return;
        const NJS_POINT3 bottom[3]={o,{o.x,o.y,o.z+s.z},{o.x+s.x,o.y,o.z}};
        const NJS_POINT3 top[3]={{o.x,o.y+h,o.z},{o.x,o.y+h,o.z+s.z},{o.x+s.x,o.y+h,o.z}};
        const bool forward=a.id==0||a.id==3;
        if(direction.y<0){
            const NJS_POINT3 cap[3]={top[0],top[forward?1:2],top[forward?2:1]};
            if(polygon(cap,3))end.y=top[0].y;
        }
        NJS_POINT3 side[4]={bottom[0],bottom[1],top[1],top[0]};
        if(!forward)std::swap(side[1],side[3]);
        if(polygon(side,4))end.x=o.x;
        side[0]=bottom[0];side[1]=top[0];side[2]=top[2];side[3]=bottom[2];
        if(!forward)std::swap(side[1],side[3]);
        if(polygon(side,4))end.z=o.z;
        side[0]=bottom[2];side[1]=top[2];side[2]=top[1];side[3]=bottom[1];
        if(!forward)std::swap(side[1],side[3]);
        polygon(side,4);
    }
    void ramp(const CollisionWall& a,float h){
        const auto o=a.position,s=a.extent;
        if(!bounds(o.x,o.x+s.x,o.z,o.z+s.z))return;
        NJS_POINT3 points[4]={o,{o.x,o.y,o.z+s.z},{o.x+s.x,o.y,o.z+s.z},{o.x+s.x,o.y,o.z}};
        for(unsigned i=0;i<4;++i){
            bool raised=false;
            switch(a.id){case 0:raised=i>=2;break;case 1:raised=i==0||i==3;break;
                         case 2:raised=i==1||i==2;break;case 3:raised=i<2;break;default:break;}
            if(raised)points[i].y=o.y+h;
        }
        polygon(points,4);
    }
    void cylinder(const CollisionWall& a,float h){
        const auto o=a.position;
        if(!bounds(o.x-a.extent.x,o.x+a.extent.x,o.z-a.extent.x,o.z+a.extent.x))return;
        // Side geometry is the original octagon, NOT an analytic circle ray.
        NJS_POINT3 old_bottom{o.x+a.extent.x,o.y,o.z},old_top{old_bottom.x,o.y+h,o.z};
        for(unsigned j=1;j<=8;++j){
            const int angle=int(182.04445f*float((360*j)/8));
            const NJS_POINT3 next_bottom{o.x+a.extent.x*njCos(angle),o.y,o.z-a.extent.x*njSin(angle)};
            const NJS_POINT3 next_top{next_bottom.x,o.y+h,next_bottom.z};
            const NJS_POINT3 side[4]={next_bottom,next_top,old_top,old_bottom};polygon(side,4);
            old_bottom=next_bottom;old_top=next_top;
        }
        if(!(direction.y<0))return;
        // Downward cap, hitchkl.c:350-365 (0x26A418..0x26A4CC): iteration j
        // writes area[j] and immediately tests all eight points, so passes
        // 0..6 still read the last side quad (area[1..3]) and stack never
        // written by this function (area[4..7]). Pass 0 has area[0]==area[1],
        // a zero normal, and never hits. Decided with the user (2026-10-11):
        // unwritten points never enclose the crossing point, so passes 1..6
        // miss (a miss writes nothing) and only the complete octagon of pass
        // 7 decides. Not derivable from the original; see docs/VALIDATION.md.
        NJS_POINT3 cap[8];
        for(unsigned j=0;j<8;++j){
            const int angle=int(182.04445f*float((360*j)/8));
            cap[j]={o.x+a.extent.x*njCos(angle),o.y+h,o.z-a.extent.x*njSin(angle)};
        }
        if(polygon(cap,8))end.y=cap[0].y; // p2->y = area[0].y
    }
    bool visit(const CollisionWall& a,float room_height){
        // Per-record return is distinct from the accumulated query hit.
        touched=false;
        const float h=atr_geometry::height(a,room_height);
        switch(a.type){
        case 0:case 1:solid(a,h);break;case 2:case 3:cylinder(a,h);break;
        case 4:case 5:triangle(a,h);break;case 6:ramp(a,h);break;case 7:floor(a);break;
        default:break;
        }
        return touched;
    }
};
std::optional<CollisionWall> line_query(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,bool filtered,unsigned flags,int floor,bool omit_attribute4){
    LineClip clip(start,end);std::optional<CollisionWall> hit;
    for(const auto& a:walls){
        if(!(a.flags&1))continue;
        if(filtered){
            if(omit_attribute4&&(a.attributes&4))continue;
            if(a.type!=7&&(((a.type&1)&&(flags&0x400)) ||
                ((a.attributes&1)&&a.floor!=static_cast<std::int8_t>(floor)) ||
                ((a.attributes&2)&&(flags&0x4000))))continue;
        }
        if(clip.visit(a,height))hit=a;
    }
    if(hit){end=clip.end;normal=clip.normal;}
    return hit;
}
}
static_assert(sizeof(RECVX_STAIR_AREA)==36 && std::is_standard_layout_v<RECVX_STAIR_AREA>);
// ATR geometry snapshots (mobile input can be temporary) and a selected
// table/index only. No SYS/ROM/Player/Enemy/Effect work is constructed here.
// Each returned pointer identifies its own record, not a shared scratch slot
// overwritten by a later query from a different ground consumer.
struct GroundCollision::Work {
    std::vector<RECVX_STAIR_AREA> room,mobile;
    float height=0;
    std::optional<std::pair<GroundHit::Table,std::size_t>> selected;
    void refresh(){
        if(!selected)return;
        const auto& table=selected->first==GroundHit::Table::Room?room:mobile;
        if(selected->second>=table.size()){selected.reset();return;}
    }
};
GroundCollision::GroundCollision():work_(std::make_unique<Work>()){}
GroundCollision::~GroundCollision()=default;
void GroundCollision::update(const std::vector<CollisionWall>& walls,float height,const std::vector<CollisionWall>& mobile){
    const auto copy=[](const auto& source,auto& destination){
        destination.resize(source.size());
        for(std::size_t i=0;i<source.size();++i)destination[i]=record(source[i]);
    };
    copy(walls,work_->room);copy(mobile,work_->mobile);work_->height=height;work_->refresh();
}
float GroundCollision::query(NJS_POINT3 position){
    GroundQuery query{position,work_->height};
    query.visit(work_->room,GroundHit::Table::Room);query.visit(work_->mobile,GroundHit::Table::Mobile);
    work_->selected=query.entry;work_->refresh();return query.result;
}
std::optional<GroundHit> GroundCollision::hit()const{
    if(!work_->selected)return {};
    const auto& table=work_->selected->first==GroundHit::Table::Room?work_->room:work_->mobile;
    return GroundHit{work_->selected->first,work_->selected->second,&table[work_->selected->second]};
}
std::optional<CollisionWall> effect_wall(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 position,unsigned flags,float radius,float actor_height){
    for(const auto& wall:walls)if(wall_contact(wall,height,position,flags,radius,actor_height))return wall;
    return {};
}
RECVX_STAIR_AREA collision_record(const CollisionWall& wall){return record(wall);}
void reflect_effect_wall(const CollisionWall& wall,float room_height,NJS_POINT3& position,float radius,float height,int& angle){
    constexpr float angle_scale=65536.f/(3.141592f*2.f);
    const float top=wall.position.y+atr_geometry::height(wall,room_height);
    switch(wall.type){
    case 0:case 1:box_response(wall,position,radius,top,angle);break;
    case 2:case 3:{
        const auto delta=NJS_POINT3{wall.position.x-position.x,0,wall.position.z-position.z};
        const float reach=radius+wall.extent.x;
        if(std::fabs(reach-line_geometry::length(delta))<std::fabs(position.y-top)){
            const int direction=int(angle_scale*std::atan2(delta.x,delta.z));
            float sine,cosine;njSinCos(direction,&sine,&cosine);
            position.x=wall.position.x-sine*reach;position.z=wall.position.z-cosine*reach;
            angle=reflected_angle(2*direction+32768,angle);
        }else position.y=top;
        break;
    }
    case 4:case 5:{
        if(wall.id>3)throw std::runtime_error("ATR triangle direction outside 0..3");
        if(atr_geometry::triangle(wall,room_height,position,radius,height)){
            const float sx=wall.id<2?1.f:-1.f,sz=(wall.id&1)?-1.f:1.f;
            NJS_POINT3 closest;
            line_geometry::point_line(position,{wall.position.x+sx*radius,0,wall.position.z+sz*radius+wall.extent.z},
                                      {wall.extent.x,0,-wall.extent.z},closest);
            const float horizontal=line_geometry::length({closest.x-position.x,0,closest.z-position.z});
            if(std::fabs(position.y-top)>=horizontal){
                position.x=closest.x;position.z=closest.z;
                angle=reflected_angle(2*int(angle_scale*std::atan2(wall.extent.x,wall.extent.z))+32768,angle);
            }else position.y=top;
        }else{
            auto vertical=wall,horizontal=wall;
            vertical.extent.x=0;horizontal.extent.z=0;
            if(wall.id&1){vertical.position.z+=wall.extent.z;vertical.extent.z=-wall.extent.z;}
            if(wall.id>=2){horizontal.position.x+=wall.extent.x;horizontal.extent.x=-wall.extent.x;}
            if(edge_response(vertical,room_height,position,radius,height,wall.id<2?12:3) ||
               edge_response(horizontal,room_height,position,radius,height,(wall.id&1)?5:10))
                angle=reflected_angle(0,angle); // Original negates for either edge.
        }
        break;
    }
    case 6:{const float surface=atr_geometry::ramp(wall,position);
        if(position.y<surface)box_response(wall,position,radius,surface,angle);break;}
    case 7:box_response(wall,position,radius,wall.position.y,angle);break;
    default:break;
    }
}
float original_ground_position(const std::vector<CollisionWall>& walls,float room_height,NJS_POINT3 position,std::size_t* hit_index){
    GroundQuery query{position,room_height};query.visit(walls,GroundHit::Table::Room);
    if(hit_index)*hit_index=query.entry?query.entry->second:std::size_t(-1);
    return query.result;
}
bool effect_line_polygon(NJS_POINT3 start,NJS_POINT3& end,const NJS_POINT3* vertices,unsigned count){LineClip clip(start,end);if(!clip.polygon(vertices,count))return false;end=clip.end;return true;}
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal){return line_query(walls,height,start,end,normal,false,0,0,false);}
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,bool omit_attribute4){return line_query(walls,height,start,end,normal,true,flags,floor,omit_attribute4);}
}
