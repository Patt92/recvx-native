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
  bind_item_names(read_asset_file(data/"files/SYSMES.ALD",4*1024*1024)); // sys->mes_ip wie GameUi in der Runtime
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
  check(r.dialog.active()&&r.dialog.index()==4,"early Enter fails to reach original hint");
  for(unsigned t=0;t<200&&!r.dialog.closable();++t)r.tick(); // erst die aufgebaute Seite nimmt OK an (message.c:345, game.c:264)
  r.input.menu_input(MenuInput::Confirm);
  for(unsigned t=0;t<100&&!r.menu.active;++t)r.tick();check(r.menu.active,"early Enter loses automatic inventory request");for(unsigned t=0;t<40;++t)r.tick();
  r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();
  r.input.menu_input(MenuInput::Cancel);r.input.menu_input(MenuInput::Cancel);for(unsigned t=0;t<4000;++t)r.tick();
  if(r.inventory.weapon()!=1||r.menu.active||r.dialog.active()||r.world.player_scripted||r.world.player_hidden||(r.vm.flags[5][0]&4)){std::cerr<<"end press="<<press<<" weapon="<<r.inventory.weapon()<<" menu="<<r.menu.active<<" dialog="<<r.dialog.index()<<" st="<<r.vm.flags[5][0]<<" cb="<<r.vm.flags[10][0]<<" tasks=";for(auto t:r.vm.task_states())std::cerr<<t.event<<":"<<t.offset<<",";std::cerr<<"\n";}
  check(r.inventory.weapon()==1&&!r.menu.active&&!r.dialog.active()&&!r.world.player_scripted&&!r.world.player_hidden&&!(r.vm.flags[5][0]&4),"early Enter / lighter / Rodrigo retains control lock");
 }
 // Cinematic skip in the middle of the running scene. The original path is
 // bhSysCallGame (system.c:538-556, cb_flg 0x10000000) -> the scd1 skip block
 // of RM_0000 at scenario offset 1194 (bhCk cb_flg bit 3 with ev_flg 99 and
 // rm_flg bit 9), which clears the request, bhEvtKill levels 0..7 and starts
 // the cleanup event 34; event 34 hides the cinematic stand-in again with
 // bhInitModelSet 1/0/0 (scenario offset 7154) and shows the player.
 // bhCheckSubTask (system.c:736-744) then releases the player work.
 // Drawn enemy triangles (eneset.c:797 bhDrawEnemy gate). The natural end of
 // the scene leaves only Rodrigo visible; the stand-in (ene[0], id 91 bhSubpl)
 // is hidden by bhInitModelSet 1/0/0 through stflg 0x1000000 regardless of
 // whether Sub_controll 0x80 released it before (natural end) or the task was
 // killed first (skip). Measured before the fix: 2x the Rodrigo count after a skip.
  // Enemy works only (world.cpp: kind 1 models draw after the modifiers, game.c:318). Effect
 // sprites keep the default light category 1; their set follows bhRandamSet branches
 // (RM_0000 scd1 picks events 12..15), which differ between the natural and the skipped run.
 auto drawn_enemy_triangles=[](const Room& r){Scene s;r.world.append(s,{},r.scene.cameras.at(0));std::size_t n=0;for(const auto& t:s.triangles)if(t.light_category==1&&t.after_modifiers)++n;return n;};
 std::size_t natural_enemy_triangles=0;
 {
  Room r(data,"RM_0000");unsigned ticks=0;
  while(!(r.dialog.active()&&r.dialog.index()==4)&&ticks<5000){if(ticks==1)r.input.menu_input(MenuInput::Confirm);if(ticks>1&&r.movie.active())r.input.menu_input(MenuInput::Confirm);r.tick();++ticks;}
  check(r.dialog.active()&&r.dialog.index()==4,"natural fixture misses the original hint");
  for(unsigned t=0;t<200&&!r.dialog.closable();++t)r.tick(); // erst die aufgebaute Seite nimmt OK an (game.c:264)
  r.input.menu_input(MenuInput::Confirm);
  for(unsigned t=0;t<100&&!r.menu.active;++t)r.tick();check(r.menu.active,"natural fixture loses the automatic inventory request");for(unsigned t=0;t<40;++t)r.tick();
  r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();
  r.input.menu_input(MenuInput::Cancel);r.input.menu_input(MenuInput::Cancel);
  unsigned wait=0;while(!(r.vm.flags[4][0]&0x00400000u)&&wait++<4000)r.tick();check(r.vm.flags[4][0]&0x00400000u,"natural cell cinematic never started");
  unsigned release=0;while((r.vm.flags[10][0]&4)&&release++<6000)r.tick();check(!(r.vm.flags[10][0]&4),"natural cell cinematic never ends");
  for(unsigned t=0;t<400;++t)r.tick();
  const auto* stand_in=r.world.state(1,0);check(stand_in&&stand_in->inactive,"natural cell cinematic leaves the stand-in active");
  natural_enemy_triangles=drawn_enemy_triangles(r);
  check(natural_enemy_triangles>0,"natural cell cinematic end draws no enemy work (Rodrigo expected)");
 }
 for(unsigned delay:{0u,60u,200u,400u,700u}){
  Room r(data,"RM_0000");unsigned ticks=0;
  while(!(r.dialog.active()&&r.dialog.index()==4)&&ticks<5000){if(ticks==1)r.input.menu_input(MenuInput::Confirm);if(ticks>1&&r.movie.active())r.input.menu_input(MenuInput::Confirm);r.tick();++ticks;}
  check(r.dialog.active()&&r.dialog.index()==4,"skip fixture misses the original hint");
  for(unsigned t=0;t<200&&!r.dialog.closable();++t)r.tick(); // erst die aufgebaute Seite nimmt OK an (game.c:264)
  r.input.menu_input(MenuInput::Confirm);
  for(unsigned t=0;t<100&&!r.menu.active;++t)r.tick();check(r.menu.active,"skip fixture loses the automatic inventory request");for(unsigned t=0;t<40;++t)r.tick();
  r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();
  r.input.menu_input(MenuInput::Cancel);r.input.menu_input(MenuInput::Cancel);
  check(r.inventory.weapon()==1,"skip fixture did not equip the original lighter");
  // rm_flg bit 9 (0x00400000) marks the running cell cinematic; the scd1 block
  // at 1080 sets it, the skip block at 1194 and the regular end clear it.
  unsigned wait=0;while(!(r.vm.flags[4][0]&0x00400000u)&&wait++<4000)r.tick();
  check(r.vm.flags[4][0]&0x00400000u,"original cell cinematic never started");
  for(unsigned t=0;t<delay;++t)r.tick();
  if(!(r.vm.flags[10][0]&4)){std::cerr<<"skip delay="<<delay<<" cinematic already over\n";}
  check(r.vm.flags[10][0]&4,"cell cinematic ended before the skip request");
  check(r.vm.skip_cinematic(),"bhSysCallGame refused the original skip request");
  unsigned release=0;while((r.vm.flags[10][0]&4)&&release++<900)r.tick();
  if(r.vm.flags[10][0]&4){std::cerr<<"skip delay="<<delay<<" cb="<<r.vm.flags[10][0]<<" tasks=";for(auto t:r.vm.task_states())std::cerr<<t.event<<":"<<t.level<<":"<<t.offset<<",";std::cerr<<"\n";}
  check(!(r.vm.flags[10][0]&4),"skipped cell cinematic never reaches bhCineSet 1");
  // system.c:738-742 in the same bhCheckSubTask pass as the falling cb_flg 4.
  // Measured gap, not coverage: the host's bhLoadWork case 0 (world.cpp op 0x65)
  // does not yet write plp->flg 0x10000 / stflg 0x10000 / mode0 7 / mtn_no 42
  // (event.c:12721-12738), so nothing sets these bits during a cinematic today
  // and the three checks below only bite once that half is ported.
  check(!(r.vm.flags[13][0]&0x10000u),"cinematic end keeps plp->flg 0x10000");
  check(!(r.vm.flags[14][0]&0x18000u),"cinematic end keeps plp->stflg 0x18000");
  check(!(r.vm.flags[10][0]&0x10000000u),"cinematic end keeps the skip request");
  if(r.world.player_mode()>>24)std::cerr<<"skip delay="<<delay<<" plp->mode3="<<(r.world.player_mode()>>24)<<"\n";
  check(!(r.world.player_mode()>>24),"cinematic end keeps plp->mode3 (system.c:742)");
  for(unsigned t=0;t<400;++t)r.tick();
  const auto* stand_in=r.world.state(1,0);
  if(!stand_in||!stand_in->inactive)std::cerr<<"skip delay="<<delay<<" stand-in still active\n";
  check(stand_in&&stand_in->inactive,"skipped cell cinematic leaves the stand-in visible");
  bool scene_task=false;for(const auto& t:r.vm.task_states())if(t.level<8)scene_task=true;
  if(scene_task){std::cerr<<"skip delay="<<delay<<" surviving tasks=";for(auto t:r.vm.task_states())std::cerr<<t.event<<":"<<t.level<<":"<<t.offset<<",";std::cerr<<"\n";}
  check(!scene_task,"skipped cell cinematic keeps a scene task running");
  check(!r.world.player_hidden&&!r.world.player_scripted&&!(r.vm.flags[5][0]&4),"skipped cell cinematic does not return control");
  // The killed scene task never ran Sub_controll 0x80 for ene[0]; bhDrawEnemy
  // decides by the work flags alone, so the drawn set must equal the natural end.
  const auto skipped_enemy_triangles=drawn_enemy_triangles(r);
  if(skipped_enemy_triangles!=natural_enemy_triangles)std::cerr<<"skip delay="<<delay<<" drawn enemy triangles="<<skipped_enemy_triangles<<" natural="<<natural_enemy_triangles<<"\n";
  check(skipped_enemy_triangles==natural_enemy_triangles,"skipped cell cinematic still draws the hidden stand-in (eneset.c:797 gate bypassed)");
 }
 // RM_0090 event 23 is the 3D-laser sequence (bhEffTypeSet 0xad + 0xc0
 // bhCyodanSetEx records, event.c:8307). Measured before the first fix the task
 // died at offset 0x20fa with "unimplemented opcode 0xc0". The records link to
 // sys->obwp[8] node 2 (w4 2, w5 8, w6 2) and spawn bhSetEffParticleMk2 with
 // WpnTab[4].hiteff (event.c:8441-8461) through the native particle owner, so
 // no 0xc0 stays pending and particles must appear while the event runs.
 {
  Room r(data,"RM_0090");r.vm.start(0,23);
  bool active=true;unsigned ticks=0;std::size_t particles=0;
  for(;ticks<400&&active;++ticks){r.tick();if(r.world.blood_particles()>particles)particles=r.world.blood_particles();active=false;for(const auto& t:r.vm.task_states())if(t.event==23)active=true;}
  check(r.vm.errors.empty(),"RM_0090 laser event blocked on an unimplemented opcode");
  check(!active,"RM_0090 laser event did not run to its end");
  bool saw_c0=false;for(const auto& c:r.vm.pending_bindings)if(c.opcode==0xc0)saw_c0=true;
  check(!saw_c0,"RM_0090 laser event leaves bhCyodanSetEx (0xc0) unbound");
  for(const auto& gap:r.world.unbound_enemy_routines())if(gap.find("bhCyodanSetEx")!=std::string::npos||gap.find("PtclTbl")!=std::string::npos)throw std::runtime_error("RM_0090 laser event: "+gap);
  check(particles>0,"RM_0090 laser event spawned no bhSetEffParticleMk2 particles");
 }
 if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory expectation(s) failed; doors and early-Enter timings passed.\n";return 1;}
 std::cout<<"Both original dog events -> padlock -> all three exits, 14 early Enter timings -> hint/lighter/Rodrigo, five mid-scene cinematic skips and the RM_0090 laser event past its original 0xc0 opcode passed. Diagnostic event starts/floor placement; no campaign proof.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
