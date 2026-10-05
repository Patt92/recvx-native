#include "collision.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    std::array<float,32> floors{};floors[3]=20;
    CollisionWall wall{1,0,0,0,0,{0,0,-20},{.01f,0,40}};
    auto move=[&](NJS_POINT3 a,NJS_POINT3 b,float radius=1){return move_walls({wall},100,floors,a,b,radius);};
    // Thin, zero-height original walls must stop both directions, including
    // movement longer than an animation step. Tangential movement can slide.
    check(move({-10,0,0},{10,0,0}).x<=-1,"zero-height wall crossed forward");
    check(move({-10,5,0},{10,5,0}).x<=-1,"zero-height wall fails to inherit room height");
    check(move({10,0,0},{-10,0,0}).x>=1.01f,"wall crossed backwards");
    const auto slide=move({-2,0,-5},{2,0,5});check(slide.x<=-1&&std::abs(slide.z-5)<.01f,"wall blocks sliding or permits diagonal tunneling");
    wall.type=1;check(move({-10,0,0},{10,0,0}).x<=-1,"odd solid box ignored");
    wall.flags=0;check(std::abs(move({-10,0,0},{10,0,0}).x-10)<.01f,"disabled script wall remains solid");wall.flags=1;
    wall.extent.y=5;check(std::abs(move({-10,10,0},{10,10,0}).x-10)<.01f,"wall blocks above its vertical extent");
    wall.position.y=15;check(move({-10,0,0},{10,0,0}).x<=-1,"original player height ignored");
    wall.position.y=0;wall.extent.y=0;wall.attributes=1;wall.floor=1;check(std::abs(move({-10,0,0},{10,0,0}).x-10)<.01f,"floor-specific wall blocks another floor");check(move({-10,20,0},{10,20,0}).x<=-1,"original grand floor index ignored");
    wall.attributes=0;wall.position={0,0,0};wall.extent={2,0,0};
    for(unsigned type:{2u,3u}){wall.type=type;const auto circle=move({-10,0,0},{10,0,0});check(circle.x<=-3,"cylinder crossed");const auto tangent=move({-5,0,3.1f},{5,0,3.1f});check(std::abs(tangent.x-5)<.01f,"cylinder treated as its bounding box");}
    // Original triangle widths/depths are signed; its empty half remains open.
    for(unsigned id=0;id<4;++id){const float sx=id<2?1.f:-1.f,sz=(id&1)?-1.f:1.f;wall.type=4+(id&1);wall.id=id;wall.extent={10*sx,0,10*sz};
        const auto empty=move({8*sx,0,8*sz},{9*sx,0,9*sz},.5f);check(std::abs(empty.x-9*sx)<.01f&&std::abs(empty.z-9*sz)<.01f,"diagonal triangle fills empty half");
        const auto hit=move({8*sx,0,8*sz},{2*sx,0,2*sz},.5f);check(sx*hit.x+sz*hit.z>=10.7f,"signed diagonal triangle crossed");}
    wall.type=0;wall.position={0,0,0};wall.extent={10,0,10};
    const auto corner=move({-2,0,-2},{-.8f,0,-.8f});check(std::abs(corner.x+.8f)<.01f,"box corner uses square rather than player circle");
    const auto push=move({-2,0,-2},{-.4f,0,-.4f});check(std::hypot(push.x,push.z)>=1,"rounded box corner penetrated");
    wall.position={0,0,-20};wall.extent={.01f,0,40};
    const CollisionWall other{1,1,0,0,0,{-20,0,0},{40,0,.01f}};
    const auto junction=move_walls({wall,other},100,floors,{-5,0,-5},{5,0,5},1);check(junction.x<=-1&&junction.z<=-1,"adjoining corner contacts crossed");
    bool rejected=false;try{move({0,0,0},{std::numeric_limits<float>::quiet_NaN(),0,0});}catch(const std::runtime_error&){rejected=true;}check(rejected,"nonfinite movement accepted");
    const CollisionWall ground{1,7,0,0,0,{0,0,0},{20,-5,20}};
    check(ground_position({ground},100,{10,1,10})==0,"negative floor thickness used as surface height");
    check(ground_position({ground},100,{10,-.00005f,10})==0,"original ground tolerance missing");
    check(ground_position({ground},100,{20,1,10})==-1000,"ground upper edge not exclusive");
    for(unsigned id=0;id<4;++id){CollisionWall ramp{1,6,id,0,0,{0,0,0},{20,10,20}};
        const float expected=id==0||id==2?2.5f:7.5f;check(std::abs(ground_position({ground,ramp},100,{5,1,5})-expected)<.0001f,"original ramp orientation/ascending surface missing");}
    auto upper=ground;upper.position.y=8;check(ground_position({ground,upper},100,{5,9,5})==8,"nearest stacked floor not selected");
    std::cout<<"Original box/cylinder/diagonal walls, height/floor flags, sliding, corners and swept substeps passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
