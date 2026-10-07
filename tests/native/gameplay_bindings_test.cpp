#define SDL_MAIN_HANDLED
#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "ui.h"
#include "inspection.h"
#include "combat.h"
#include "weapon_tables.h"
#ifdef RECVX_TEST_INPUT
#include "input.h"
#endif
#include "binary_view.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static EventCommand cmd(std::initializer_list<std::uint8_t> b){EventCommand c;c.bytes=b;c.opcode=c.bytes[0];return c;}
int main(int argc,char** argv){try{
 if(argc<2)return 0;_Make_SinTable();std::filesystem::path data=argv[1];AudioMixer audio(data);
 {auto bytes=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;RoomWorld world(bytes,scene,player,motion,frame,audio,data);auto asset=PlayerAsset::load(data);world.set_player_asset(&asset);Inventory inv;inv.acquire(84,1);MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.binding=[&](const EventCommand& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.initialize();vm.tick();
 // Isolate ItemUse from movement: the original floor 27 requires key 84.
 vm.flags[10][0]|=0x200;vm.values[16]=27;check(vm.use(84),"Padlock ItemUse refused original floor");bool question=false;
 for(unsigned tick=0;tick<400;++tick){vm.tick();world.tick();if(dialog.active()){if(dialog.choices()){question=true;dialog.selection=0;}if(tick%5==0)vm.confirm_dialog();}}
 check(question&&!inv.contains(84),"Padlock Yes did not discard the key");check(!dialog.active()&&!(vm.flags[5][0]&4)&&!(vm.flags[10][0]&4),"Padlock Yes retained cinematic/dialog lock");check(vm.errors.empty(),"Padlock script blocked");std::cout<<"Padlock use / original discard question / Yes / control release passed.\n";
 // Every original dog is hittable; damage dispatch must use en04, not en01.
 for(unsigned i=0;i<16;++i)if(const auto* e=world.state(1,i);e&&e->identity==4){
   world.bind(cmd({0x24,1,std::uint8_t(i),1}));const auto at=e->transform.position;
   Inventory armed;armed.acquire(5,12);unsigned slot=1;while(slot<12&&Inventory::id(armed.slots()[slot])!=5)++slot;check(armed.equip(slot),"dog pistol fixture cannot equip");
   bool shooting_position=false;const auto& weapon=WpnTab[armed.weapon()];
   for(unsigned yaw=0;yaw<65536&&!shooting_position;yaw+=4096)for(float distance=9;distance<25&&!shooting_position;distance+=1){const float r=yaw*6.28318530718f/65536;const NJS_POINT3 forward{-std::sin(r),0,-std::cos(r)};SceneSpawn candidate{{at.x-forward.x*distance,at.y,at.z-forward.z*distance},int(yaw)};const NJS_POINT3 chest{candidate.position.x,at.y+11,candidate.position.z},origin{chest.x+forward.x*(3+weapon.r),chest.y,chest.z+forward.z*(3+weapon.r)},direction{forward.x*.70710678f,-.70710678f,forward.z*.70710678f};const float hit=CombatController::target_distance(origin,direction,weapon.l,at,3+weapon.r,4);if(world.combat_wall_distance(chest,forward,5)>=5&&hit<world.combat_wall_distance(origin,direction,weapon.l)){player=candidate;shooting_position=true;}}
   check(shooting_position,"no unobstructed dog shot fixture");CombatController gun(data);
   // bhCPM2_act_rdy -> original downward aim (mode67/phase1); the old
   // ten-host-tick fixture fired before the original ready transition.
   gun.tick({true,false,0,-1},false,armed,asset,player,motion,frame,&world,audio);
   const unsigned ready_limit=asset.motion_frames(100)+asset.motion_frames(114)+16;
   for(unsigned t=0;t<ready_limit&&(gun.original_mode()!=67||gun.original_phase()!=1);++t)gun.tick({true,false,0,-1},false,armed,asset,player,motion,frame,&world,audio);
   check(gun.original_mode()==67&&gun.original_phase()==1,"dog pistol does not reach original down aim");
   for(unsigned t=0;t<asset.motion_frames(114);++t)gun.tick({true,false,0,-1},false,armed,asset,player,motion,frame,&world,audio);
   for(unsigned t=0;t<asset.motion_frames(111)+4&&gun.shots==0;++t)gun.tick({true,true,0,-1},false,armed,asset,player,motion,frame,&world,audio);
   world.tick();std::cout<<"dog "<<i<<" shot="<<gun.shots<<" hit="<<gun.hits<<" pos="<<at.x<<","<<at.z<<" yaw="<<player.yaw<<" target="<<gun.targets().right<<" hp="<<world.state(1,i)->hp<<" motion="<<motion<<"\n";check(gun.shots==1&&gun.hits==1,"pistol shot cannot hit original dog");for(unsigned t=0;t<160;++t)world.tick();
   check(world.damage(i,3,500,0x10,{at.x,at.y+2,at.z},{0,0,-1}),"original dog excluded from damage");world.tick();check(world.state(1,i)->hp<0,"dog original HP not reduced");check(world.blood_particles()>0,"dog damage has no blood");
   for(unsigned t=0;t<200;++t)world.tick();check(world.enemy_dead(i),"dog damage/death motion does not finish");
 }
 std::cout<<"RM_0050 dogs: hit, blood, damage/death motion passed.\n";
 }
 {auto bytes=read_asset_file(data/"rooms/RM_6040.RDT",64*1024*1024);Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.values[0]=6;vm.values[1]=4;vm.item_message=[](unsigned,unsigned){return Message{0xffff};};const auto a=vm.interactions.at(13);NJS_POINT3 pos{a.position.x+a.extent.x*.5f,a.position.y,a.position.z+a.extent.z*.5f+2};check(vm.interact(pos,0,a.floor),"original Memo file interaction failed");check(!inv.contains(132)&&vm.acquired_file()==17&&(inv.items[383]&(1u<<17)),"Memo entered inventory instead of file list");check(vm.flags[10][0]&0x800,"file pickup did not acknowledge room event");
#ifdef RECVX_TEST_INPUT
StatusMenu menu;GameplayInput controls(inv,menu,dialog,&vm);controls.synchronize();menu.tick(inv);menu.skip_slide();menu.input_unslid(MenuInput::Cancel,inv);check(!menu.file_acquisition,"acquired file cannot close");for(unsigned tick=0;tick<120;++tick){menu.tick(inv);controls.synchronize();}check(!menu.active,"file acquisition reopened after close");
#else
vm.finish_file();
#endif
check(!(vm.flags[10][0]&0x20000),"file request retained after close");std::cout<<"Original RM_6040 Memo: file 17, no inventory slot, acknowledgement and close passed.\n";}
 {auto bytes=read_asset_file(data/"rooms/RM_0090.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld world(bytes,scene,player,m,f,audio,data);Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.binding=[&](const auto& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.start(0,22);for(unsigned t=0;t<35;++t){vm.tick();world.tick();}check(world.state(2,8)->has_motion&&world.state(2,8)->motion==6,"original milling event did not bind room MTN 6");BinaryView v(bytes);const auto h=inspect_room(bytes).room_offset;for(unsigned i=4;i<v.u32(h+0x8c);++i)if(i!=8)world.bind(cmd({0x24,2,std::uint8_t(i),0}));
 Scene first=scene;world.append(first,{},scene.cameras[0]);for(unsigned t=0;t<40;++t){vm.tick();world.tick();}check(vm.errors.empty(),"original milling event blocked");Scene second=scene;world.append(second,{},scene.cameras[0]);for(auto* sample:{&first,&second})sample->triangles.erase(std::remove_if(sample->triangles.begin(),sample->triangles.end(),[](const auto& t){return t.light_category!=2;}),sample->triangles.end());check(!first.triangles.empty(),"milling machine not rendered");check(first.triangles.size()==second.triangles.size(),"milling geometry changed topology");bool moved=false;for(unsigned i=0;i<first.triangles.size();++i)for(unsigned j=0;j<3;++j){const auto a=first.triangles[i].vertices[j].position,b=second.triangles[i].vertices[j].position;if(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.01f)moved=true;}check(moved,"original milling motion does not change rendered vertices");std::cout<<"RM_0090 object 8 / original room MTN 6 changes rendered machine geometry.\n";}
 {auto bytes=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld world(bytes,scene,player,m,f,audio,data);world.set_enemy_ai(true);const unsigned index=3;Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.binding=[&](const auto& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.start(0,40);for(unsigned t=0;t<300;++t){vm.tick();world.tick();}check(vm.errors.empty(),"original RM_0080 event 40 blocked");check(!world.state(1,index)->scripted&&!world.state(1,index)->frozen,"Sub_controll did not release zombie");const auto at=world.state(1,index)->transform.position;player.position={at.x+40,at.y,at.z};for(unsigned t=0;t<120;++t)world.tick();const auto next=world.state(1,index)->transform.position;check(std::hypot(next.x-at.x,next.z-at.z)>1,"released original zombie cannot move");check(world.damage(index,2,20,0x10,{next.x,next.y+10,next.z},{0,0,-1}),"released original zombie cannot be hit");world.tick();check(world.blood_particles()>0,"released original zombie has no blood");std::cout<<"Original zombie script release / AI movement / hit and blood passed.\n";}
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
