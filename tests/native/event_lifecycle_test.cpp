#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
// Weiche Prüfung für die Inventarwirkung (D07/D06): meldet und läuft weiter,
// damit Türen und frühe Eingaben dahinter geprüft bleiben.
static std::vector<std::string> failures;
static void expect(bool ok,const std::string& why){if(!ok)failures.push_back(why);}
struct Room {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
 Inventory inventory;StatusMenu menu;MessageDialog dialog;AudioMixer audio;MoviePlayer movie;PlayerAsset body;RoomWorld world;EventVm vm;GameplayInput input;
 Room(const std::filesystem::path& data,const char* name):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm){
  world.set_player_asset(&body);vm.values[1]=std::string(name)=="RM_0050"?5:0;
  vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
  vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};
  vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
  input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
  if(std::string(name)=="RM_0050")inventory.acquire(84,1); // Diagnostic precondition: the player brings the padlock key into the room.
  audio.begin_room_init();vm.initialize();audio.load_room_sounds(0,vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
 }
 void tick(){input.apply({});vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.tick();input.synchronize();world.set_simulation_flags(vm.flags[6][0]);world.tick();vm.flags[4][0]|=world.take_enemy_room_flags();movie.tick();audio.mix();check(vm.errors.empty(),"original event stream blocked");}
 bool task(unsigned event){for(const auto& t:vm.task_states())if(t.event==event)return true;return false;}
};
int main(int argc,char** argv){try{
 if(argc<2)return 0;_Make_SinTable();const std::filesystem::path data=argv[1];
 // Explicit event starts isolate both original trap branches; no script bytes are changed.
 for(unsigned event:{15u,16u})for(unsigned exit:{0u,1u,2u}){
  Room r(data,"RM_0050");r.vm.start(0,event);r.tick();check(r.task(event),"dog event did not start");
  unsigned ticks=0;while(r.task(event)&&ticks++<1000)r.tick();check(!r.task(event)&&!r.vm.flag(4,4),"dog event fails to finish and release room flag");
  check(!(r.vm.flags[5][0]&4)&&!r.world.player_scripted,"dog event retains player control");
  // Same VM/world after the dog event, unlike the old BGM test's fresh room.
  r.actor.position={-50,0,63};r.actor.yaw=0;r.vm.check_floor(r.actor.position,r.actor.yaw,0);check(r.vm.values[16]==27&&(r.vm.flags[10][0]&0x200),"padlock fixture misses original floor");
  // D07 ItemUse am originalen Bodentrigger 27: Use_05 schließt den
  // Statusbildschirm, das Raumskript fragt nach dem Wegwerfen, Ja verbraucht den
  // Schlüssel (Skriptbefehl) und gibt die Steuerung frei.
  check(r.vm.use(84),"padlock refused after dog event");bool question=false;
  for(unsigned t=0;t<400;++t){r.tick();if(r.dialog.active()){if(r.dialog.choices()){question=true;r.dialog.selection=0;}r.vm.confirm_dialog();}}
  expect(question,"D07 padlock discard question missing after dog event, exit "+std::to_string(exit));
  expect(!r.inventory.contains(84),std::string("D07/D06 padlock Yes did not discard the key (pending opcodes: ")+[&]{std::string o;for(const auto& c:r.vm.pending_bindings){char b[8];std::snprintf(b,sizeof b,"%02x ",c.opcode);o+=b;}return o;}()+")");
  expect(!r.dialog.active()&&!(r.vm.flags[5][0]&4),"D07 padlock use retains the dialog or control lock after dog event, exit "+std::to_string(exit));
  const auto& a=r.vm.interactions.at(exit);bool requested=false;
  for(unsigned yaw=0;yaw<65536&&!requested;yaw+=16384){float rad=yaw*6.28318530718f/65536;float probe=a.type==0?4.5f:6.f;r.actor.position={a.position.x+a.extent.x*.5f+probe*std::sin(rad),a.position.y,a.position.z+a.extent.z*.5f+probe*std::cos(rad)};requested=r.vm.interact(r.actor.position,yaw,a.floor);}
  check(requested,"door refused after dog event and padlock");if(exit==0)r.vm.tick();r.vm.finish_room_exit();check(r.vm.flags[10][0]&1,"door scenario fails after dog event and padlock");check(r.vm.errors.empty(),"door exit blocked");
 }
 // Fresh Enter edges before the FMV: sweep the initial game frames, including
 // the cinematic start and subsequent animation waits. Finish the same flow.
 for(unsigned press:{0u,1u,2u,3u,10u,30u,60u,120u,300u,600u,900u,1100u,1200u,1300u}){
  Room r(data,"RM_0000");unsigned ticks=0;
  while(!(r.dialog.active()&&r.dialog.index()==4)&&ticks<5000){if(ticks==press)r.input.menu_input(MenuInput::Confirm);if(ticks>press&&r.movie.active())r.input.menu_input(MenuInput::Confirm);r.tick();++ticks;}
  if(!r.dialog.active()||r.dialog.index()!=4){std::cerr<<"early press="<<press<<" dialog="<<r.dialog.index()<<" movie="<<r.movie.active()<<" st="<<r.vm.flags[5][0]<<" cb="<<r.vm.flags[10][0]<<" tasks=";for(auto t:r.vm.task_states())std::cerr<<t.event<<":"<<t.offset<<",";std::cerr<<"\n";}
  check(r.dialog.active()&&r.dialog.index()==4,"early Enter fails to reach original hint");r.input.menu_input(MenuInput::Confirm);
  for(unsigned t=0;t<100&&!r.menu.active;++t)r.tick();check(r.menu.active,"early Enter loses automatic inventory request");for(unsigned t=0;t<40;++t)r.tick();
  r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();
  r.input.menu_input(MenuInput::Cancel);r.input.menu_input(MenuInput::Cancel);for(unsigned t=0;t<4000;++t)r.tick();
  if(r.inventory.weapon()!=1||r.menu.active||r.dialog.active()||r.world.player_scripted||r.world.player_hidden||(r.vm.flags[5][0]&4)){std::cerr<<"end press="<<press<<" weapon="<<r.inventory.weapon()<<" menu="<<r.menu.active<<" dialog="<<r.dialog.index()<<" st="<<r.vm.flags[5][0]<<" cb="<<r.vm.flags[10][0]<<" tasks=";for(auto t:r.vm.task_states())std::cerr<<t.event<<":"<<t.offset<<",";std::cerr<<"\n";}
  check(r.inventory.weapon()==1&&!r.menu.active&&!r.dialog.active()&&!r.world.player_scripted&&!r.world.player_hidden&&!(r.vm.flags[5][0]&4),"early Enter / lighter / Rodrigo retains control lock");
 }
 if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory expectation(s) failed; doors and early-Enter timings passed.\n";return 1;}
 std::cout<<"Both original dog events -> padlock -> all three exits, and 14 early Enter timings -> hint/lighter/Rodrigo passed. Diagnostic event starts/floor placement; no campaign proof.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
