#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "binary_view.h"
#include "room.h"
#include "events.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    check(argc==2,"original data directory required; this test must not silently skip its resource checks");
    _Make_SinTable();const std::filesystem::path data=argv[1];
    const auto room=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);
    const auto scene=decode_scene(room);auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,scene,actor,motion,frame,audio,data,{},"RM_0050");
    const auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
    const auto probe=NJS_POINT3{actor.position.x,actor.position.y+1,actor.position.z};
    const float initial=world.combat_ground(probe);
    check(initial!=-1000&&world.ground_hit(),"real room spawn has no native ground hit");
    const auto hit=*world.ground_hit();
    check(hit.table==GroundHit::Table::Room&&hit.record->type==7&&initial==hit.record->py,
          "real spawn floor is not the original room ATR floor surface");
    const BinaryView bytes(room);const auto header=inspect_room(room).room_offset;
    const auto wall_count=bytes.u32(header+0x98),wall_base=bytes.u32(header+0x18);
    check(hit.index<wall_count&&bytes.f32(wall_base+36*hit.index+12)==initial,
          "room-owned ground identity does not identify its decoded source");
    // Exercise production event mutations on the selected real ATR entry.
    // Turn all entries off through the native event binding, not by replacing
    // the World table with a fake geometry owner.
    check(wall_count<=256,"test room wall indexes exceed byte event operands");
    for(unsigned i=0;i<wall_count;++i)
        check(world.bind({0x0a,0,0,0,{0x0a,std::uint8_t(i),1,0}}),"real wall disable event unbound");
    check(world.combat_ground(probe)==-1000&&!world.ground_hit(),"disabled room walls leave a stale ground hit");
    check(world.bind({0x0a,0,0,0,{0x0a,std::uint8_t(hit.index),0,0}}),"real floor enable event unbound");
    check(world.combat_ground(probe)==initial&&world.ground_hit()->index==hit.index,
          "native event floor update does not reach the same ground owner");
    NJS_POINT3 line_end{probe.x,initial-10,probe.z},line_normal{};
    const auto line_hit=world.collision_line({probe.x,initial+10,probe.z},line_end,line_normal);
    check(line_hit&&line_hit->type==7&&line_end.y==initial&&line_normal.y==1,
          "real room World line service does not clip at the selected original floor");
    const auto atr=collision_record(*line_hit);
    check(atr.py==bytes.f32(wall_base+36*hit.index+12)&&atr.w==bytes.f32(wall_base+36*hit.index+20)&&
          atr.prm3==bytes.u8(wall_base+36*hit.index+35),"native line hit supplies only an ATR header, not the actual full record");
    check(world.bind({0x0a,0,0,0,{0x0a,std::uint8_t(hit.index),1,0}}),"line floor disable event unbound");
    line_end={probe.x,initial-10,probe.z};
    check(!world.collision_line({probe.x,initial+10,probe.z},line_end,line_normal),
          "real event mutation does not reach native line geometry");
    check(world.bind({0x0a,0,0,0,{0x0a,std::uint8_t(hit.index),0,0}}),"line floor restore event unbound");
    // No enemy/player state owner is bound yet. Its absence is not damage or
    // an authoritative released-grab/dead pose, regardless of actual HP.
    for(int hp:{160,93,1,0}){world.set_player_hp(hp);
        for(unsigned i=0;i<3;++i){world.tick();check(world.take_blast_damage()==0&&
            !world.player_killed()&&!world.player_dead(),"absent enemy player state consumes HP as damage");}}
    // Native body movement consumes the same mutable real ATR owner. A
    // diagnostic candidate crosses wall11; the old neutral push service
    // returned it unchanged. There is no fixture geometry or room special fix.
    const unsigned barrier=11;const auto b=wall_base+36*barrier;
    check(bytes.u8(b+1)==0&&bytes.u32(b+4)==0,"real movement barrier contract changed");
    const float edge=bytes.f32(b+8)+bytes.f32(b+20),z=bytes.f32(b+16)+.25f*bytes.f32(b+28);
    NJS_POINT3 from{edge+9,0,z},candidate{edge-1,0,z};
    check(world.bind({0x0a,0,0,0,{0x0a,std::uint8_t(barrier),0,0}}),"movement barrier enable unbound");
    check(std::fabs(world.move(from,candidate).x-(edge+3.5f))<.003f,"real World body movement still crosses enabled wall");
    check(world.bind({0x0a,0,0,0,{0x0a,std::uint8_t(barrier),1,0}}),"movement barrier disable unbound");
    check(world.move(from,candidate).x==candidate.x,"disabled wall still blocks World movement");
    check(world.bind({0x0a,0,0,0,{0x0a,std::uint8_t(barrier),0,0}}),"movement barrier restore unbound");
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);
    actor.position=from;actor.yaw=16384;world.bind_player_collision(events);
    const auto after=world.move_player(from,candidate,1,false);
    check(std::fabs(after.x-(edge+4.5f))<.003f,"leading-point/body pass not bound to real player flags");
    check(events.player_flags==events.flags[13][0],"native player flag mirror diverges");
    events.flags[13][0]&=~0x10u;
    check(world.move_player(from,candidate,1,false).x==candidate.x,"original player wall enable gate ignored");
    events.flags[13][0]|=0x10u;events.flags[6][0]&=~1u;
    check(world.move_player(from,candidate,1,false).x==candidate.x,"original simulation pause gate ignored");
    events.flags[6][0]|=1u;events.flags[14][0]|=0x1000000;
    check(world.move_player(from,candidate,1,false).x==candidate.x,"original hidden work gate ignored");
    // Camera clip installation is a camera invocation, not a render-time
    // function of mutable display flags or the fixed camera's cno argument.
    {const auto camera_room=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);
    const auto camera_scene=decode_scene(camera_room);auto camera_actor=camera_scene.spawns.at(0);
    RoomWorld camera_world(camera_room,camera_scene,camera_actor,motion,frame,audio,data,{},"RM_0020");
    Inventory camera_inventory;MessageDialog camera_dialog;EventVm camera_events(camera_room,camera_inventory,camera_dialog);
    camera_world.bind_player_collision(camera_events);
    camera_events.values[0]=0;camera_events.values[1]=2;camera_events.flags[12][0]|=0x200u;
    camera_world.initialize_camera_clip(4);
    check(camera_world.camera_clip().near==4&&camera_world.camera_clip().far==20000,"room cut did not install its original clip range");
    check(camera_world.bind({0x2a,0,0,0,{0x2a,0,0,0}}),"fixed camera binding missing");
    check(camera_world.camera_clip().near==4&&camera_world.camera_clip().far==20000,"fixed camera used cno instead of cam.ncut for clip lookup");
    camera_events.flags[12][0]&=~0x200u;
    camera_world.tick_camera(camera_scene,camera_scene.cameras[0],0,false);
    check(camera_world.camera_clip().near==4&&camera_world.camera_clip().far==20000,"display flag change alone rewrote installed clip range");
    check(camera_world.bind({0x13,0,0,0,{0x13,1,0,0}}),"room-camera return missing");
    camera_world.tick_camera(camera_scene,camera_scene.cameras[0],0,false);
    check(camera_world.camera_clip().near==1&&camera_world.camera_clip().far==99,"non-game camera return did not install 1/99");
    camera_events.flags[12][0]|=0x200u;camera_events.flags[11][0]|=0x40u;
    check(camera_world.bind({0x13,0,0,0,{0x13,1,0,0}}),"eye-view return missing");
    camera_world.tick_camera(camera_scene,camera_scene.cameras[0],0,false);
    check(camera_world.camera_clip().near==1&&camera_world.camera_clip().far==20000,"eye-view return did not install 1/20000");}
    std::cout<<"RM_0050 native World ground/event updates and absent-player-state HP regression passed (ground "
             <<initial<<", room entry "<<hit.index<<").\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
