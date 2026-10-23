// Originale Eventbefehle am produktiven World-/Spielerbesitzer, mit markierter
// Skript-/POS-Fixture. Kein natuerlicher Spielverlauf.
#include "events.h"
#include "world.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static void put(std::vector<std::uint8_t>& b,std::size_t p,unsigned value){for(unsigned i=0;i<4;++i)b.at(p+i)=std::uint8_t(value>>(i*8));}
static void floating(std::vector<std::uint8_t>& b,std::size_t p,float value){unsigned bits;std::memcpy(&bits,&value,4);put(b,p,bits);}
int main(int argc,char** argv){try{
    check(argc==2,"original data directory required");_Make_SinTable();const std::filesystem::path data=argv[1];
    auto room=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);
    const auto header=inspect_room(room).room_offset;const auto pos=BinaryView(room).u32(header+0x24);
    floating(room,pos,-12.5f);floating(room,pos+4,17.25f);floating(room,pos+8,24.f);put(room,pos+12,0x3456);
    for(unsigned i=0;i<32;++i)floating(room,header+0x120+i*4,0.f);
    floating(room,header+0x120+3*4,10.f);
    const auto base=room.size();room.resize(base+14);put(room,28,unsigned(base));
    put(room,base,12);put(room,base+4,13);put(room,base+8,14);
    const std::vector<std::uint8_t> script={0x65,0,0,0,
        0x64,12,1,0,1,0, 0x64,7,100,0,0,0,200,0,
        0x64,13,0,1,0,0, 0x69,11,0,20,0,0, 0xfe,
        0x64,137,0,0, 0xff,0};
    room.insert(room.end(),script.begin(),script.end());
    const auto scene=decode_scene(room);SceneSpawn actor{};std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,scene,actor,motion,frame,audio,data,{},"RM_0080");
    Inventory inv;MessageDialog dialog;EventVm vm(room,inv,dialog);world.bind_player_collision(vm);
    vm.binding=[&](const EventCommand& c){return world.bind(c);};vm.task_started=[&](unsigned level){world.start_task(level);};
    vm.start(0,0);vm.tick();
    check(vm.errors.empty()&&vm.pending_bindings.empty(),"Player_controll 12/13 missing from World");
    check(actor.position.x==-1.f&&actor.position.z==-2.f&&actor.yaw==-3640,"task signs did not reach position/angle consumers");
    const auto before=world.player_pass_origin();vm.tick();
    check(vm.errors.empty()&&vm.pending_bindings.empty(),"Player_controll 137 missing from World");
    check(actor.position.x==-12.5f&&actor.position.y==17.25f&&actor.position.z==24.f&&actor.yaw==0x3456,"POS selection did not set the player work");
    check(world.player_mode()==0x00010007u&&world.player_floor_number()==1,"137 did not install mode 7/0/1/0 and the POS floor");
    check(world.player_event().requested&&world.player_event().reset&&world.player_event().mode2==1,"137 not connected to native player event progression");
    const auto after=world.player_pass_origin();check(before.x==after.x&&before.y==after.y&&before.z==after.z,"137 changed pxb/pyb/pzb, which the original leaves untouched");
    frame=-65536;check(world.frame(0,0)==0xffff0000u,"frame binding clamped the signed original work value");
    std::cout<<"event_player_bindings: task signs, POS, floor, modes and event driver passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
