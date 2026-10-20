// Common_controll 7 gegen 10 am echten Work (bhCommonCtr, event.c): 7 schreibt work->px/py/pz
// samt pxb/pyb/pzb (event.c:12038-12065), 10 nur die eine Achse von px/py/pz (event.c:12452-12490).
// Gegnerwork: RM_0080 ene[0] (nativer Zombiework), Taskkontext ueber 0x68 Fall 1 (bhLoadWork).
// Spielerwork: 0x68 Fall 0; plp->pxb ist RoomWorld::player_pass_origin().
#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "room.h"
#include "events.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    check(argc==2,"original data directory required; this test must not silently skip its resource checks");
    _Make_SinTable();const std::filesystem::path data=argv[1];
    const auto room=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);
    const auto scene=decode_scene(room);auto actor=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,scene,actor,motion,frame,audio,data,{},"RM_0080");
    const auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
    const auto* z=world.state(1,0);check(z&&z->enemy_bound&&z->enemy_native,"RM_0080 ene[0] is not a native enemy work");
    // u16 little endian aus Hundertsteln: 1234 -> 12.34
    auto u16=[](unsigned v){return std::vector<std::uint8_t>{std::uint8_t(v&255u),std::uint8_t(v>>8)};};
    auto cc7=[&](unsigned x,unsigned y,unsigned zz){std::vector<std::uint8_t> b{0x69,7};for(auto v:{x,y,zz}){auto p=u16(v);b.insert(b.end(),p.begin(),p.end());}return world.bind({0x69,0,0,0,b});};
    auto cc10=[&](unsigned axis,unsigned v){auto p=u16(v);return world.bind({0x69,0,0,0,{0x69,10,std::uint8_t(axis),0,p[0],p[1]}});};

    // Gegnerwork
    check(world.bind({0x68,0,0,0,{0x68,1,0}}),"bhLoadWork kind 1 ene[0] unbound");
    check(cc7(1234,0,5678),"Common_controll 7 unbound");
    check(z->enemy.px==12.34f&&z->enemy.pxb==12.34f&&z->enemy.pz==56.78f&&z->enemy.pzb==56.78f&&z->enemy.py==0.f&&z->enemy.pyb==0.f,
          "Common_controll 7 must write px/py/pz together with pxb/pyb/pzb (event.c:12044/12054/12065)");
    check(cc10(0,2000)&&cc10(2,3000)&&cc10(1,150),"Common_controll 10 unbound");
    check(z->enemy.px==20.f&&z->enemy.pz==30.f&&z->enemy.py==1.5f,"Common_controll 10 did not write work->px/py/pz");
    check(z->transform.position.x==20.f&&z->transform.position.z==30.f,"Common_controll 10 entity view diverges from the work");
    check(z->enemy.pxb==12.34f&&z->enemy.pyb==0.f&&z->enemy.pzb==56.78f,
          "Common_controll 10 must not write pxb/pyb/pzb (event.c:12452-12490 writes px/py/pz only)");

    // Spielerwork: 7 setzt plp->pxb, 10 nicht.
    check(world.bind({0x68,0,0,0,{0x68,0,0}}),"bhLoadWork kind 0 unbound");
    check(cc7(4000,0,5000),"Common_controll 7 (player) unbound");
    auto o=world.player_pass_origin();check(actor.position.x==40.f&&actor.position.z==50.f&&o.x==40.f&&o.z==50.f,"Common_controll 7 must set plp->pxb with px");
    check(cc10(0,4500),"Common_controll 10 (player) unbound");
    o=world.player_pass_origin();check(actor.position.x==45.f&&o.x==40.f&&o.z==50.f,"Common_controll 10 must not set plp->pxb");

    std::cout<<"common10_pxb: Common_controll 7 writes px+pxb, 10 only px (RM_0080 ene[0] and player) passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
