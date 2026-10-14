#include "collision.h"
#include "effect_collision.h"
#include "effect_sprites.h"
#include "ninja_math.h"
#include "geometry_atr.h"
#include "wall_capsule.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
// pwksub.c:1891 bhCheckC2Wall / sdfunc.c:824 CheckCollision4Sound, expectations from the original formulas.
static void c2_wall(){
    auto cap=[](NJS_POINT3 a,NJS_POINT3 b){return RECVX_CB_CAPSULE{{a.x,a.y,a.z},{b.x,b.y,b.z},0.1f};};
    const CollisionWall box{1,0,0,0,0,{0,0,-20},{.01f,0,40}};
    check(check_c2_wall({box},100,cap({-10,5,0},{10,5,0}))==1,"type 0 box (h 0 -> rom->h) must block the sound capsule");
    check(check_c2_wall({box},100,cap({-10,5,30},{10,5,30}))==0,"capsule beside the type 0 box blocked");
    auto off=box;off.attributes=0x10;check(check_c2_wall({off},100,cap({-10,5,0},{10,5,0}))==0,"attr 0x10 walls are skipped");
    off=box;off.flags=0;check(check_c2_wall({off},100,cap({-10,5,0},{10,5,0}))==0,"disabled walls are skipped");
    const CollisionWall pillar{1,2,0,0,0,{0,0,0},{1,10,0}};
    check(check_c2_wall({pillar},100,cap({-10,5,0},{10,5,0}))==1&&check_c2_wall({pillar},100,cap({-10,5,5},{10,5,5}))==0,"type 2 cylinder radius w");
    // Type 4 prism over (0,0),(10,0),(0,10): bhCheckInnerTriangle(hp,pos,0,0) keeps |pz+d-z| > |d/(w/(x-px))|.
    const CollisionWall prism{1,4,0,0,0,{0,0,0},{10,10,10}};
    check(check_c2_wall({prism},100,cap({2,20,2},{2,-5,2}))==1,"type 4 top face inside the triangle");
    check(check_c2_wall({prism},100,cap({8,20,8},{8,-5,8}))==0,"type 4 outside the hypotenuse");
    // Type 7 boxes put v[0].y = py-0.1 under v[6].y = py+h; njCollisionCheckBC still hits through the faces.
    const CollisionWall slab{1,7,0,0,0,{0,0,0},{10,.05f,10}};
    check(check_c2_wall({slab},100,cap({2,50,2},{2,-50,2}))==1&&check_c2_wall({slab},100,cap({2,50,2},{2,10,2}))==0,"type 7 slab");
    check(check_collision_4_sound({box},100,0,{-10,5,0},{10,5,0})==0,"rom->flg without bit 2 never occludes");
    check(check_collision_4_sound({box},100,2,{-10,5,0},{10,5,0})==-0x1C,"CheckCollision4Sound returns -0x1C");
}
// pwksub.c:718 bhCheckL2Wall: *len = nr, sys->apos and sys->ahtp (the winning record),
// expectations computed by hand from the original face planes.
static void l2_wall(){
    auto near=[](float a,float b){return std::fabs(a-b)<1e-4f;};
    auto line=[](NJS_POINT3 p,NJS_POINT3 v){return RECVX_CB_LINE{p.x,p.y,p.z,v.x,v.y,v.z};};
    float len=0;RECVX_CB_VEC apos{};int ahtp=-2;
    // Two type 0 boxes, the farther one first in rom->walp: face px+0.1 (normal -x) of each is hit;
    // the nearer one wins with nr, and sys->ahtp names it together with its attr/prm.
    CollisionWall far{1,0,0,0,0x300,{5,0,-20},{2,0,40},{1,5,255,255}},near_box{1,0,0,0,0x100,{0,0,-20},{2,0,40},{0,0,0,0}};
    check(check_l2_wall({far,near_box},100,line({-10,5,0},{20,0,0}),0x400,&len,&apos,&ahtp)==1,"type 0 box not hit");
    check(ahtp==1&&near(len,10.1f)&&near(apos.x,0.1f)&&near(apos.y,5)&&near(apos.z,0),"bhCheckL2Wall does not keep the nearest face hit as sys->apos/sys->ahtp");
    check(check_l2_wall({far},100,line({-10,5,0},{20,0,0}),0x400,&len,&apos,&ahtp)==1&&ahtp==0&&near(apos.x,5.1f)&&near(len,15.1f),"far box face");
    // Type 1 is skipped with flg 0x400; without a hit apos = lp->p + lp->v, ahtp = NULL, *len = |lp->v|.
    auto see=near_box;see.type=1;
    check(check_l2_wall({see},100,line({-10,5,0},{20,0,0}),0x400,&len,&apos,&ahtp)==0&&ahtp==-1&&near(apos.x,10)&&near(len,20),"type 1 with flg 0x400 or the no-hit result");
    check(check_l2_wall({see},100,line({-10,5,0},{20,0,0}),0,&len,&apos,&ahtp)==1&&ahtp==0,"type 1 without flg 0x400");
    auto off=near_box;off.flags=0;check(check_l2_wall({off},100,line({-10,5,0},{20,0,0}),0x400,&len,&apos,&ahtp)==0,"disabled wall hit");
    // Type 7 slab (h < 0, prm0 1 = water): face py-0.1 with ps.y <= 0 && ps.y >= h.
    const CollisionWall water{1,7,0,0,0,{0,0,0},{10,-5,10},{1,0,0,0}};
    check(check_l2_wall({water},100,line({5,10,5},{0,-20,0}),0x400,&len,&apos,&ahtp)==1&&ahtp==0&&near(apos.y,-0.1f)&&near(len,10.1f),"type 7 slab not hit from above");
    check(check_l2_wall({water},100,line({-5,5,5},{20,0,0}),0x400,&len,&apos,&ahtp)==0,"type 7 slab hit by a line above it");
    // Type 2 cylinder (radius w): the mantle hit is pulled onto the radius facing lp->p.
    const CollisionWall pillar{1,2,0,0,0,{0,0,0},{1,10,0}};
    check(check_l2_wall({pillar},100,line({-10,5,0},{20,0,0}),0x400,&len,&apos,&ahtp)==1&&near(apos.x,-1)&&near(len,9),"type 2 mantle");
    // Type 4 prism id 0 over (0,0),(10,0),(0,10): top face py+h-0.1 inside the triangle.
    const CollisionWall prism{1,4,0,0,0,{0,0,0},{10,10,10}};
    check(check_l2_wall({prism},100,line({2,20,2},{0,-25,0}),0x400,&len,&apos,&ahtp)==1&&near(apos.y,9.9f)&&near(len,10.1f),"type 4 top face");
    check(check_l2_wall({prism},100,line({8,20,8},{0,-25,0}),0x400,&len,&apos,&ahtp)==0,"type 4 outside the hypotenuse");
}
int main(){try{
    _Make_SinTable();
    c2_wall();
    l2_wall();
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
    // Positive native ATR tests. The host movement tests above are legacy
    // host-code checks, NOT evidence for the missing bhCheckWall mechanism.
    const CollisionWall ground{1,7,0,0,0,{0,0,0},{20,-5,20}};
    std::size_t ground_hit=0;
    check(original_ground_position({ground},100,{10,1,10},&ground_hit)==0&&ground_hit==0,
          "floor surface must use py, not its negative thickness");
    check(ground_position({ground},100,{10,1,10})==0,"shared collision.cpp ground consumer not bound");
    check(ground_position({ground},100,{0,0,0})==0&&ground_position({ground},100,{20,0,10})==-1000,
          "rectangular ground edges are not lower-inclusive/upper-exclusive");
    check(ground_position({ground},100,{5,-.0001f,5})==0&&
          ground_position({ground},100,{5,-.000101f,5})==-1000,
          "floor tolerance differs from the original 0.0001");
    check(ground_position({ground},100,{5,10000,5})==-1000,
          "ground query accepted a gap at the original strict 10000 limit");
    auto zero_floor=ground;zero_floor.extent.y=0;
    check(!effect_wall({zero_floor},100,{5,1,5},0,0,1),"floor contact invents room height for zero thickness");
    CollisionWall box{1,0,11,3,0xf,{0,0,0},{20,5,20},{1,2,3,4}};
    check(ground_position({box},100,{5,5,5})==5&&ground_position({box},100,{5,4.99f,5})==-1000,
          "solid ground height or below-top gate incorrect");
    box.extent.y=0;check(ground_position({box},10,{5,10,5})==10,"box room-height fallback missing");
    box.extent.y=-5;check(ground_position({box},10,{5,-4,5})==-5,"signed solid height lost");
    box.extent.y=5;
    auto upper=box;upper.position.y=2;
    check(original_ground_position({box,upper},100,{5,9,5},&ground_hit)==7&&ground_hit==1,
          "stacked floors do not choose the nearest surface");
    for(unsigned type:{1u,3u,5u}){auto odd=box;odd.type=type;
        check(ground_position({odd},100,{5,20,5})==-1000,"odd solid type incorrectly contributes ground");}
    auto disabled=box;disabled.flags=0;
    check(ground_position({disabled},100,{5,20,5})==-1000,"disabled ATR contributes ground");
    CollisionWall cylinder{1,2,0,0,0,{0,0,0},{10,5,0}};
    check(ground_position({cylinder},100,{0,5,0})==5&&
          ground_position({cylinder},100,{10,5,0})==-1000&&
          ground_position({cylinder},100,{8,5,8})==-1000,"cylinder ground radius or strict edge incorrect");
    for(unsigned id=0;id<4;++id){
        const float sx=id<2?1.f:-1.f,sz=(id&1)?-1.f:1.f;
        CollisionWall tri{1,4,id,0,0,{0,0,0},{10*sx,5,10*sz}};
        check(ground_position({tri},100,{2*sx,6,2*sz})==5,"signed triangle interior ground missing");
        check(ground_position({tri},100,{8*sx,6,8*sz})==-1000&&
              ground_position({tri},100,{5*sx,6,5*sz})==-1000,"triangle empty half or diagonal accepted");
        check(effect_wall({tri},100,{2*sx,1,2*sz},0,0,0).has_value(),"triangle point contact missing");
        check(!effect_wall({tri},100,{5.25f*sx,1,5.25f*sz},0,0,0)&&
              effect_wall({tri},100,{5.25f*sx,1,5.25f*sz},0,1,0).has_value(),
              "expanded triangle diagonal strip missing");
        check(!effect_wall({tri},100,{9*sx,1,9*sz},0,1,0),"expanded triangle fills its empty half");
        tri.type=5;check(!effect_wall({tri},100,{2*sx,1,2*sz},0x400,1,0),"odd triangle filter ignored");
    }
    const float ramps[4]={2.5f,7.5f,2.5f,7.5f};
    for(unsigned id=0;id<4;++id){CollisionWall ramp{1,6,id,0,0,{0,0,0},{20,10,40}};
        check(ground_position({ground,ramp},100,{5,1,10})==ramps[id],"ramp direction or above-probe selection incorrect");
        check(ground_position({ramp},100,{5,-.001f,10})==-1000,"ramp accepted probe below its base");
        check(!effect_wall({ramp},100,{-.1f,1,10},0,1,1),"ramp contact incorrectly expands footprint");
        ramp.extent.y=0;check(ground_position({ramp},100,{5,1,10})==0,"ramp uses room height instead of its actual zero height");}
    for(unsigned type:{0u,1u}){auto solid=box;solid.type=type;
        check(effect_wall({solid},100,{-1,0,5},0,1,0).has_value()&&
              !effect_wall({solid},100,{21,0,5},0,1,0),"box contact expanded edge semantics wrong");
        check(effect_wall({solid},100,{5,-1,5},0,0,1).has_value()&&
              !effect_wall({solid},100,{5,-1.01f,5},0,0,1)&&
              !effect_wall({solid},100,{5,5.01f,5},0,0,1),"box contact vertical overlap wrong");
        check(bool(effect_wall({solid},100,{5,1,5},0x400,0,0))==!(type&1),"odd solid suppression wrong");}
    for(unsigned type:{2u,3u}){cylinder.type=type;
        check(effect_wall({cylinder},100,{10.5f,0,0},0,1,0).has_value()&&
              !effect_wall({cylinder},100,{11,0,0},0,1,0),"cylinder contact radius wrong");
        check(bool(effect_wall({cylinder},100,{0,0,0},0x400,1,0))==!(type&1),"odd cylinder filter wrong");}
    check(effect_wall({ground},100,{5,-5,5},0,0,0).has_value()&&
          !effect_wall({ground},100,{5,0,5},0,0,1),"floor underside normal contact wrong");
    check(effect_wall({ground},100,{5,-5,5},0x400,0,0).has_value(),"odd-solid suppression incorrectly skips floor type 7");
    check(effect_wall({ground},100,{5,-6,5},0x100,0,2).has_value()&&
          !effect_wall({ground},100,{5,-6,5},0x100,0,1)&&
          !effect_wall({ground},100,{5,-5,5},0x100,0,1),"floor special crossing must be strict at both ends");
    const auto first=effect_wall({box,upper},100,{5,3,5},0,0,0);
    check(first&&first->id==box.id&&first->position.y==0&&first->attributes==0xf,
          "contact does not preserve first table entry or invents floor/attribute filters");
    {GroundCollision shared;shared.update({box},100,{box});
        check(shared.query({5,9,5})==5&&shared.hit()&&shared.hit()->table==GroundHit::Table::Room&&
              shared.hit()->index==0,"equal room/mobile floor tie not stable");
        check(shared.hit()->record->flr_no==3&&shared.hit()->record->prm3==4,
              "selected ATR view dropped floor or parameters");
        shared.update({box},100,{upper});
        check(shared.query({5,9,5})==7&&shared.hit()->table==GroundHit::Table::Mobile,"nearest mobile ground not selected");
        const auto* view=shared.hit()->record;upper.position.y=3;upper.parameters[2]=7;
        shared.update({box},100,{upper});
        check(shared.hit()->index==0&&shared.hit()->record==view&&view->py==3&&view->prm2==7,
              "selected native ATR view lost identity during geometry update");
        upper.extent.x=upper.extent.z=10;shared.update({box},100,{upper});
        check(shared.query({15,9,15})==5&&shared.hit()->table==GroundHit::Table::Room&&view->py==3,
              "another ground query overwrites an earlier consumer's ATR pointer");
        check(shared.query({30,9,30})==-1000&&!shared.hit(),"miss does not clear previous hit identity");
        shared.update({box},100,{upper});shared.query({5,9,5});shared.update({box},100);
        check(!shared.hit(),"removed mobile table leaves a stale hit");}
    {const atr_geometry::Point2 strip[4]={{0,0},{2,2},{-2,4},{0,6}};
        check(atr_geometry::strip({0,1},strip)&&atr_geometry::strip({0,3},strip)&&
              atr_geometry::strip({0,5},strip)&&!atr_geometry::strip({0,6},strip),
              "three scanline-strip regions or exclusive upper end incorrect");}
    {const atr_geometry::Point2 strip[4]={{0,0},{-2,2},{2,4},{0,6}};
        check(atr_geometry::strip({0,1},strip)&&atr_geometry::strip({0,3},strip)&&
              atr_geometry::strip({0,5},strip)&&!atr_geometry::strip({0,-.01f},strip),
              "reversed strip orientation or lower end incorrect");}
    std::cout<<"Native ATR ground/contact predicates and selected-entry identity passed. Player wall resolution, lines, reflection and effect procedures remain separate gaps.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
