#include "effect_collision.h"
#include <cmath>
namespace recvx {namespace {
struct ATR_WORK {unsigned char flg,type,id;char flr_no;unsigned attr;float px,py,pz,w,h,d;unsigned char prm0,prm1,prm2,prm3;};
struct NJS_POINT2 {float x,y;};
struct NJS_LINE {float px,py,pz,vx,vy,vz;};
struct {ATR_WORK* walp=nullptr;int wal_n=0;float h=0;} room;auto* rom=&room;
struct {ATR_WORK* mwalp=nullptr;int mwal_n=0;} system;auto* sys=&system;
constexpr float PI_2=6.2831853071795864769f;
float njDistanceP2L(NJS_POINT3* p,NJS_LINE* line,NJS_POINT3* out){const float length=line->vx*line->vx+line->vy*line->vy+line->vz*line->vz;const float t=length?((p->x-line->px)*line->vx+(p->y-line->py)*line->vy+(p->z-line->pz)*line->vz)/length:0;*out={line->px+t*line->vx,line->py+t*line->vy,line->pz+t*line->vz};return std::sqrt((p->x-out->x)*(p->x-out->x)+(p->y-out->y)*(p->y-out->y)+(p->z-out->z)*(p->z-out->z));}
using NJS_PLANE=NJS_LINE;
NJS_PLANE PLANE;
float njDistanceP2P(NJS_POINT3* a,NJS_POINT3* b){NJS_VECTOR d{b->x-a->x,b->y-a->y,b->z-a->z};return njScalor(&d);}
void njGetPlaneNormal(NJS_POINT3* p,NJS_VECTOR* out){NJS_VECTOR a{p[1].x-p[0].x,p[1].y-p[0].y,p[1].z-p[0].z},b{p[2].x-p[0].x,p[2].y-p[0].y,p[2].z-p[0].z};njOuterProduct(&a,&b,out);njUnitVector(out);}
int njDistanceL2PL(NJS_LINE* l,NJS_LINE* p,NJS_POINT3* out){const float den=l->vx*p->vx+l->vy*p->vy+l->vz*p->vz;if(std::abs(den)<1e-8f)return 1;const float t=((p->px-l->px)*p->vx+(p->py-l->py)*p->vy+(p->pz-l->pz)*p->vz)/den;*out={l->px+t*l->vx,l->py+t*l->vy,l->pz+t*l->vz};return 0;}
#include "effect_collision.inc"
ATR_WORK work(const CollisionWall& w){return {static_cast<unsigned char>(w.flags),static_cast<unsigned char>(w.type),static_cast<unsigned char>(w.id),static_cast<char>(w.floor),w.attributes,w.position.x,w.position.y,w.position.z,w.extent.x,w.extent.y,w.extent.z,static_cast<unsigned char>(w.parameters[0]),static_cast<unsigned char>(w.parameters[1]),static_cast<unsigned char>(w.parameters[2]),static_cast<unsigned char>(w.parameters[3])};}
}
std::optional<CollisionWall> effect_wall(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 position,unsigned flags,float radius,float actor_height){std::vector<ATR_WORK> records;records.reserve(walls.size());for(const auto& w:walls)records.push_back(work(w));room={records.data(),int(records.size()),height};const auto* hit=bhCheckWallType(&position,flags,radius,actor_height);if(!hit)return {};return walls[std::size_t(hit-records.data())];}
void reflect_effect_wall(const CollisionWall& wall,float room_height,NJS_POINT3& position,float radius,float height,int& angle){auto record=work(wall);room.h=room_height;bhSetWallRefAngle(&record,&position,radius,height,&angle);}
std::optional<CollisionWall> effect_line(const std::vector<CollisionWall>& walls,float height,NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor){std::vector<ATR_WORK> records;for(const auto& w:walls)records.push_back(work(w));room={records.data(),int(records.size()),height};const auto* hit=flags?bhCollisionCheckLine2(&start,&end,flags,char(floor)):bhCollisionCheckLine(&start,&end);if(!hit)return {};bhGetHitCollisionNormal(&normal);return walls[std::size_t(hit-records.data())];}

}
