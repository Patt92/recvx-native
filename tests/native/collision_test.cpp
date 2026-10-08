#include "collision.h"
#include "effect_collision.h"
#include "effect_sprites.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    _Make_SinTable();
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
    std::size_t ground_hit=std::size_t(-1);
    check(original_ground_position({ground},100,{10,1,10},&ground_hit)==0&&ground_hit==0,"negative floor thickness or original htp side effect differs");
    for(const auto extent:std::array<NJS_POINT3,4>{{{10,0,10},{-10,0,10},{10,0,-10},{-10,0,-10}}}){
        auto triangle_extent=extent;triangle_extent.y=5;
        CollisionWall triangle{1,4,0,0,0,{0,0,0},triangle_extent};
        const float inside_x=extent.x>0?2.f:-2.f,inside_z=extent.z>0?2.f:-2.f;
        const float outside_z=extent.z>0?9.f:-9.f;
        check(ground_position({triangle},100,{inside_x,5,inside_z})==5,"original type-4 triangle rejected an interior point");
        check(ground_position({triangle},100,{inside_x,5,outside_z})==-1000,"original type-4 triangle accepted a point outside its sloped edge");
    }
    check(ground_position({ground},100,{10,-.00005f,10})==0,"original ground tolerance missing");
    check(ground_position({ground},100,{20,1,10})==-1000,"ground upper edge not exclusive");
    for(unsigned id=0;id<4;++id){CollisionWall ramp{1,6,id,0,0,{0,0,0},{20,10,20}};
        const float expected=id==0||id==2?2.5f:7.5f;check(std::abs(ground_position({ground,ramp},100,{5,1,5})-expected)<.0001f,"original ramp orientation/ascending surface missing");}
    auto upper=ground;upper.position.y=8;ground_hit=std::size_t(-1);check(original_ground_position({ground,upper},100,{5,9,5},&ground_hit)==8&&ground_hit==1,"nearest stacked floor or original htp index differs");
    ground_hit=0;check(original_ground_position({},100,{5,9,5},&ground_hit)==-1000&&ground_hit==std::size_t(-1),"original no-ground result did not clear htp");
    // htp persists in the room system, with separate original table indices.
    // Equal heights must retain the earlier room entry (strict yy < nr).
    GroundCollision shared,other_system;
    shared.update({ground,upper},100,{upper});
    check(shared.query({5,9,5})==8,"persistent original ground height differs");
    auto retained=shared.hit();
    check(retained&&retained->table==GroundHit::Table::Room&&retained->index==1&&retained->record->py==8,"room/mobile tie lost original room htp");
    (void)effect_wall({ground},100,{5,0,5},0x400,1,1);
    other_system.update({ground},100);other_system.query({5,1,5});
    check(shared.hit()->record==retained->record&&shared.hit()->index==1,"another world or wall query overwrote htp");
    auto moved=upper;moved.position.y=12;moved.id=3;
    // A host reallocation must preserve the table index and rebind the pointer.
    std::vector<CollisionWall> grown(256,ground);grown[1]=moved;
    shared.update(grown,100,{upper});
    check(shared.hit()->index==1&&shared.hit()->record->py==12&&shared.hit()->record->id==3,"live room htp lost on host table relocation");
    shared.update({ground},100,{moved,upper});
    check(shared.hit()->index==1&&shared.hit()->record->py==12,"wal_n shrink cleared htp before another original query");
    check(shared.query({5,13,5})==12&&shared.hit()->table==GroundHit::Table::Mobile&&shared.hit()->index==0,"mobile htp was flattened into a room index");
    retained=shared.hit();moved.position.x=2;moved.id=1;
    shared.update({ground},100,{moved});
    check(shared.hit()->record==retained->record&&retained->record->px==2&&retained->record->id==1,"mobile htp is a stale copy instead of live ATR work");
    shared.update({ground},100,{});
    check(shared.hit()->record==retained->record,"mwal_n shrink discarded allocated original htp");
    check(shared.query({50,13,50})==-1000&&!shared.hit(),"next original query did not clear a retained htp");
    check(other_system.hit()&&other_system.hit()->record->py==0,"no-hit query contaminated another system");
    shared.update({ground},100,{upper});shared.query({5,9,5});
    check(shared.hit()->table==GroundHit::Table::Mobile&&shared.hit()->index==0,"requery did not replace hit identity");
    bool capacity_rejected=false;
    try{shared.update({ground},100,std::vector<CollisionWall>(65,upper));}catch(const std::runtime_error&){capacity_rejected=true;}
    check(capacity_rejected&&shared.hit()->index==0,"original 64-entry mobile capacity not enforced atomically");
    {GroundCollision next_room;next_room.update({ground},100);check(!next_room.hit(),"room initialization inherited old htp");}
    // Original bhEff000 mode 3 consumes the exact htp selected by the shared
    // bhGetGroundPosition work. Type-6 id 2 turns the shell along that ramp.
    {GroundCollision effect_ground;CollisionWall ramp{1,6,2,0,0,{0,0,0},{20,10,20}};effect_ground.update({ramp},100);std::vector<CollisionWall> no_walls;
        set_effect_walls(&no_walls,100);set_effect_ground(&effect_ground,nullptr,nullptr);set_effect_context({});
        EffectSprite shell;shell.id=0;shell.flg=1;shell.type=0;shell.flr_no=1;shell.mode0=3;shell.px=5;shell.py=2;shell.pz=5;shell.zn=.4f;shell.yn=-.1f;
        bool queued=false;check(step_effect_sprite(shell,&queued)&&queued&&shell.polygon_queued,"bhEff000 did not enter the original ef_pol list");
        check(effect_ground.hit()&&effect_ground.hit()->table==GroundHit::Table::Room&&effect_ground.hit()->index==0&&effect_ground.hit()->record->id==2,"bhEff000 mode 3 lost exact room htp/index/id");
        check(shell.mode0==3&&shell.ay==24576&&std::abs(shell.py-2.6f)<.0001f&&std::abs(shell.zn-.41f)<.0001f,"bhEff000 type-6 ramp state transition differs from effsub1.c");
        set_effect_ground(nullptr,nullptr,nullptr);set_effect_walls(nullptr,0);
    }
    std::cout<<"Original box/cylinder/diagonal walls, height/floor flags, sliding, corners and swept substeps passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
