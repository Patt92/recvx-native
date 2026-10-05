#include "collision.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace recvx {
namespace {
struct Point {float x,z;};
constexpr float skin=.001f;
bool push_polygon(NJS_POINT3& pos,const Point* vertices,unsigned count,float radius){
    float area=0,best=std::numeric_limits<float>::max();Point closest{},normal{};bool inside=true;
    for(unsigned i=0;i<count;++i){const auto a=vertices[i],b=vertices[(i+1)%count];area+=a.x*b.z-b.x*a.z;}
    const float winding=area>=0?1.f:-1.f;
    for(unsigned i=0;i<count;++i){const auto a=vertices[i],b=vertices[(i+1)%count];const float dx=b.x-a.x,dz=b.z-a.z,length2=dx*dx+dz*dz;
        if(length2<1e-12f)continue;
        if(winding*(dx*(pos.z-a.z)-dz*(pos.x-a.x))<0)inside=false;
        const float t=std::clamp(((pos.x-a.x)*dx+(pos.z-a.z)*dz)/length2,0.f,1.f);const Point q{a.x+t*dx,a.z+t*dz};const float distance2=(pos.x-q.x)*(pos.x-q.x)+(pos.z-q.z)*(pos.z-q.z);
        if(distance2<best){best=distance2;closest=q;const float length=std::sqrt(length2);normal={winding*dz/length,-winding*dx/length};}
    }
    if(std::abs(area)<1e-8f)inside=false;
    if(best==std::numeric_limits<float>::max()||(!inside&&best>=radius*radius))return false;
    const float distance=std::sqrt(best);
    if(distance>1e-6f){const float sign=inside?-1.f:1.f;normal={sign*(pos.x-closest.x)/distance,sign*(pos.z-closest.z)/distance};}
    pos.x=closest.x+normal.x*(radius+skin);pos.z=closest.z+normal.z*(radius+skin);return true;
}
bool push_wall(const CollisionWall& wall,NJS_POINT3& pos,NJS_POINT3 previous,float radius){
    const auto p=wall.position,e=wall.extent;
    if(wall.type<=1){const float x0=std::min(p.x,p.x+e.x),x1=std::max(p.x,p.x+e.x),z0=std::min(p.z,p.z+e.z),z1=std::max(p.z,p.z+e.z);
        const Point vertices[]={{x0,z0},{x1,z0},{x1,z1},{x0,z1}};return push_polygon(pos,vertices,4,radius);}
    if(wall.type<=3){const float limit=std::abs(e.x)+radius;float dx=pos.x-p.x,dz=pos.z-p.z,distance=std::hypot(dx,dz);if(distance>=limit)return false;
        if(distance<1e-6f){dx=previous.x-p.x;dz=previous.z-p.z;distance=std::hypot(dx,dz);if(distance<1e-6f){dx=1;dz=0;distance=1;}}
        pos.x=p.x+dx/distance*(limit+skin);pos.z=p.z+dz/distance*(limit+skin);return true;}
    // Signed width/depth encode the original triangle orientation (id 0..3).
    const Point vertices[]={{p.x,p.z},{p.x+e.x,p.z},{p.x,p.z+e.z}};return push_polygon(pos,vertices,3,radius);
}
}
NJS_POINT3 move_walls(const std::vector<CollisionWall>& walls,float room_height,const std::array<float,32>& floors,NJS_POINT3 from,NJS_POINT3 to,float radius,float height){
    for(float n:{from.x,from.y,from.z,to.x,to.y,to.z,radius,height,room_height})if(!std::isfinite(n))throw std::runtime_error("nonfinite collision input");
    if(radius<=0||height<=0||room_height<0)throw std::runtime_error("invalid collision dimensions");
    const float distance=std::hypot(to.x-from.x,to.z-from.z),step_count=std::ceil(distance/(radius*.25f));if(step_count>4096)throw std::runtime_error("movement too large");
    const unsigned steps=std::max(1u,unsigned(step_count));const float dx=(to.x-from.x)/steps,dz=(to.z-from.z)/steps;auto pos=from;
    for(unsigned step=0;step<steps;++step){const auto previous=pos;pos.x+=dx;pos.z+=dz;pos.y=from.y+(to.y-from.y)*(step+1)/steps;
        int floor=0;for(unsigned i=0;i<31;++i)if((floors[i]!=0||i==2)&&pos.y>=floors[i])floor=int(i)-2;
        // Several adjoining volumes may push into each other. Revisit contacts
        // within each bounded substep so diagonal running cannot tunnel through.
        for(unsigned pass=0;pass<8;++pass){bool changed=false;for(const auto& wall:walls){
            if(!(wall.flags&1)||wall.type>5||((wall.attributes&1)&&wall.floor!=floor))continue;
            const float wall_height=wall.extent.y==0?room_height:wall.extent.y;
            if(pos.y+height<wall.position.y||pos.y>wall.position.y+wall_height)continue;
            changed=push_wall(wall,pos,previous,radius)||changed;
        }if(!changed)break;}
    }return pos;
}
}
