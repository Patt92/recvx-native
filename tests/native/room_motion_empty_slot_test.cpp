#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "renderer.h"
#include "asset_io.h"
#include "room.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
// Regression RM_3130 ("Native runtime error: vector"): Ereignis 2 (Task 2, Skript @0d5a)
// setzt per Common_controll 24, Selektor 1, mtn_no 5 auf dem Subplayerwerk (X04, id 99).
// Die Raumbibliothek hat nur 3 Motionen. Original: sys->rmthp sind 512 genullte MN_WORK
// (room.c:700-706), mnwP[5].datP ist NULL und bhSetMotion (Motion.c:29) tut nichts; das
// Werk wird trotzdem beschrieben (event.c:11501-11570). Vorher griff der Host-Spiegel
// e.motion per room_motions_.at(5) hinter die Bibliothek (world.cpp entity_motion_data),
// zuerst in RoomWorld::bind (Task blockiert), dann ungefangen in RoomWorld::tick.
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
struct Room {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
 std::uint32_t head_flags=0;Inventory inventory;StatusMenu menu;MessageDialog dialog;AudioMixer audio;MoviePlayer movie;PlayerAsset body;RoomWorld world;EventVm vm;GameplayInput input;
 Room(const std::filesystem::path& data,const char* name,const std::function<void(EventVm&,RoomWorld&)>& pre={}):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns.at(0)),audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data,{},name),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm){
  bind_item_names(read_asset_file(data/"files/SYSMES.ALD",4*1024*1024));
  world.set_player_asset(&body);world.bind_player_collision(vm);world.bind_enemy_player(inventory.hp,head_flags);world.set_enemy_ai(true);world.set_enemy_grab(false); /* wie barrel_blast_test.cpp:54 bzw. Runtime --enemy-ai on */{const auto id=RoomIdentity::parse(std::string(name)+".RDX");vm.values[0]=id.stage;vm.values[1]=id.room;}
  vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
  vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};vm.enemy_joint_position=[&](unsigned n,unsigned node){return world.enemy_joint_position(n,node);};vm.set_item_position=[&](unsigned n,NJS_POINT3 p){world.set_item_position(n,p);};vm.enemy_disable=[&](unsigned n){world.deactivate_enemy(n);};
  vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
  input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
  audio.mount_sound_afs(vm.values[0]);if(pre)pre(vm,world);audio.begin_room_init();vm.initialize();audio.load_room_sounds(vm.values[0],vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
 }
 void tick(){input.apply({});vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.tick();input.synchronize();world.set_simulation_flags(vm.flags[6][0]);world.tick();vm.flags[4][0]|=world.take_enemy_room_flags();movie.tick();audio.mix();}
 bool task(unsigned event){for(const auto& t:vm.task_states())if(t.event==event)return true;return false;}
};
int main(int argc,char** argv){try{
 if(argc<2)return 0;_Make_SinTable();const std::filesystem::path data=argv[1];
 // Testaufstellung (kein natuerliches Durchspielen): it_flg 232 wie mitgebracht vor Event_init
 // (scd1 @0840 bhCk(7,232) "ist 1"), Spieler auf Bodentrigger 0 (scd1 @0848 bhCk(10,22,0)).
 // scd1 startet dann Ereignis 0..6 (@084e-0866); Ereignis 2 traegt Common 24/Sel. 1/mtn 5.
 Room r(data,"RM_3130",[](EventVm& vm,RoomWorld&){vm.set_flag(7,232,0);});
 r.actor.position={90.f,0.f,30.f};r.actor.yaw=0;
 bool started=false;unsigned t=0;
 for(;t<6000;++t){r.tick();if(r.task(2))started=true;if(r.dialog.active()&&t%8==0){r.dialog.selection=0;r.vm.confirm_dialog();}if(started&&r.vm.task_states().empty())break;}
 check(started,"RM_3130 event 2 (Common_controll 24 mtn 5) never started");
 for(const auto& e:r.vm.errors)std::cerr<<e<<"\n";
 check(r.vm.errors.empty(),"RM_3130 event stream blocked (bhSetMotion on empty rmthp slot)");
 check(r.vm.task_states().empty(),"RM_3130 events 0..8 did not finish");
 std::cout<<"RM_3130 empty rmthp slot: events finished after "<<t<<" ticks, no blocked task.\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
