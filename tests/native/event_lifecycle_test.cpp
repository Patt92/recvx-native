#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "renderer.h"
#include "asset_io.h"
#include "room.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <algorithm>
#include <functional>
#include <vector>
#include <map>
#include <array>
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
 // `pre` runs before bhInitEvent (scd0), like flags carried into the room from elsewhere.
 Room(const std::filesystem::path& data,const char* name,const std::function<void(EventVm&,RoomWorld&)>& pre={}):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns.at(0)) /* wie runtime.cpp:420 (RM_9360 hat keine posp) */,audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data,{},name),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm){
  bind_item_names(read_asset_file(data/"files/SYSMES.ALD",4*1024*1024)); // sys->mes_ip wie GameUi in der Runtime
  world.set_player_asset(&body);{const auto id=RoomIdentity::parse(std::string(name)+".RDX");vm.values[0]=id.stage;vm.values[1]=id.room;} // runtime.cpp: sys->stage_no/room_no aus dem Raumnamen
  vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
  vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};vm.enemy_joint_position=[&](unsigned n,unsigned node){return world.enemy_joint_position(n,node);};vm.set_item_position=[&](unsigned n,NJS_POINT3 p){world.set_item_position(n,p);};vm.enemy_disable=[&](unsigned n){world.deactivate_enemy(n);};
  vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
  input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
  if(std::string(name)=="RM_0050")inventory.acquire(84,1); // Diagnostic precondition: the player brings the padlock key into the room.
  audio.mount_sound_afs(vm.values[0]); // system.c:1602 Diskwechsel vor dem Raumladen wie runtime.cpp (Stufe >= 6: Satz 2)
  if(pre)pre(vm,world);audio.begin_room_init();vm.initialize();audio.load_room_sounds(vm.values[0],vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
 }
 void tick(){input.apply({});vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.tick();input.synchronize();world.set_simulation_flags(vm.flags[6][0]);world.tick();vm.flags[4][0]|=world.take_enemy_room_flags();movie.tick();audio.mix();if(!vm.errors.empty()){for(const auto& e:vm.errors)std::cerr<<e<<"\n";}check(vm.errors.empty(),"original event stream blocked");}
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
 // RM_1050 Ereignis 34 (Steve-Schalter, Skript @2ba2): bhCamSet 1 (Monitor), Meldung 24,
 // WHILE !bhFlagCk(5,19) [st_flg 0x1000 Seite fertig], WHILE bhFlagCk(5,19) [bis OK
 // umblaettert], erst dann bhCamSet 4 auf die Schaltertafel. Die Meldungsbits sind
 // sys->st_flg selbst (message.c), kein eigener Dialogzustand des Hosts.
 {Room r(data,"RM_1050");r.vm.set_flag(1,0x92,1);r.vm.start(0,34);r.tick();check(r.task(34),"RM_1050 switch event did not start");
  bool page=false,cam4=false;
  for(unsigned t=0;t<600&&r.task(34)&&!cam4;++t){r.tick();
   check((r.vm.flags[5][0]&0x8dc00u)==(r.dialog.status()&0x8dc00u),"st_flg message bits differ from the bhControlMessage state");
   if(r.dialog.active()&&(r.vm.flags[5][0]&0x1000u))page=true;
   if(r.dialog.active()&&(r.dialog.turnable()||r.dialog.choosing())&&t%4==0){r.dialog.selection=0;r.vm.confirm_dialog();}
   if(r.world.event_camera&&r.world.event_camera_index==4)cam4=true;}
  check(page,"RM_1050 message 24 never raised st_flg 0x1000 for the script");
  check(cam4,"RM_1050 switch event did not reach bhCamSet 4 after the page turn");
  std::cout<<"RM_1050 switch event: st_flg message bits mirrored, bhCamSet 4 reached after the page turn.\n";}
 // Common_controll (event.c:9963-12710) advances the script pointer per command; the
 // whole table, including the selector-dependent 24/25/34/35 and axis-dependent 10/22
 // lengths, must match the original or the task desynchronizes (RM_0080 below).
 {struct Row{unsigned cmd,b2,b3;std::size_t len;};const Row rows[]={
   {0,0,0,2},{36,0,0,2},{47,0,0,2},{2,0,0,3},{17,0,0,3},{33,0,0,3},{14,0,0,4},{23,1,0,4},{40,0,0,4},{41,0,0,4},{42,0,0,4},{46,1,0,4},{48,0,0,4},{49,0,0,4},{50,1,0,4},{52,0,0,4},{53,0,0,4},
   {43,0,7,5},{44,0,7,5},{45,0,7,5},{5,0,0,6},{6,0,0,6},{9,0,0,6},{11,0,0,6},{12,0,0,6},{13,0,0,6},{15,0,0,6},{16,0,0,6},{51,0,0,6},{132,0,0,6},{141,0,0,6},
   {10,0,0,6},{10,2,0,6},{10,3,0,2},{22,1,0,6},{22,9,0,2},{7,0,0,8},{8,0,0,8},{27,0,0,8},{140,0,0,8},{26,0,0,10},{38,0,0,10},
   {24,1,0,8},{24,1,3,8},{24,1,4,3},{25,1,0,8},{25,1,1,8},{25,1,2,3},{34,1,0,10},{34,1,3,10},{34,1,4,3},{35,1,0,8},{35,1,1,9},{35,1,2,3},{1,0,0,0},{37,0,0,0},{54,0,0,0}};
  for(const auto& r:rows){const auto got=EventVm::common_controll_length(r.cmd,r.b2,r.b3);if(got!=r.len){std::cerr<<"Common_controll "<<r.cmd<<" bytes "<<r.b2<<"/"<<r.b3<<": length "<<got<<" expected "<<r.len<<"\n";check(false,"Common_controll length differs from the original pointer advance");}}}
 // Obj_controll (event.c:9719-9744) hinter bhObjCtr (:9709, verbraucht 0x66, gibt immer 1 zurueck):
 // 20 (:9727) ++ +=2 -> 4 Bytes, 1 (:9737) ++ -> 2 Bytes; jeder andere Befehl faellt in den
 // Default-Zweig (:9742) ohne Vorschub: das Befehlsbyte laeuft als naechster Opcode (event.c:13047).
 {struct Row{unsigned cmd;std::size_t len;};const Row rows[]={{1,2},{20,4},{0,1},{2,1},{12,1},{19,1},{21,1},{128,1},{255,1}};
  for(const auto& r:rows){const auto got=EventVm::obj_controll_length(r.cmd);if(got!=r.len){std::cerr<<"Obj_controll "<<r.cmd<<": length "<<got<<" expected "<<r.len<<"\n";check(false,"Obj_controll length differs from the original pointer advance");}}}
 // RM_3020 Ereignis 6 (@040a): 0x65 bhLoadWork (Objekt 4), 0x66 Obj_controll 12 = Default-Zweig, das
 // Befehlsbyte 0x0c laeuft als bhFlrAtariSet(flrp[0], v1 0 -> flg |= 1) @040f, dann 0x00 bhEnd @0413
 // (event.c:387, gibt 0 zurueck ohne Vorschub: die Task bleibt dort wie im Original stehen).
 // Kein Raumskript startet Ereignis 6 (kein 0x14 mit evt 6 in RM_3020); expliziter Start.
 {Room r(data,"RM_3020");check(!r.vm.floor_areas().empty(),"RM_3020 has no floor ATR 0");
  const bool before=r.vm.floor_areas()[0].flags&1u;r.vm.start(0,6);r.vm.set_flag(6,27,0); // sp_flg 0x10 (bhEventScheduler2, event.c:13024), sonst von scd1 @014a/@0172 gesetzt
  unsigned offset=0;for(unsigned t=0;t<4;++t){r.tick();for(const auto& s:r.vm.task_states())if(s.event==6)offset=s.offset;}
  for(const auto& err:r.vm.errors)std::cerr<<"RM_3020: "<<err<<'\n';
  check(r.vm.errors.empty(),"RM_3020 event 6 blocked on Obj_controll 12");
  check(offset==0x413,"RM_3020 event 6 did not reach bhEnd @0413 through the default branch and 0x0c");
  check(r.vm.floor_areas()[0].flags&1u,"RM_3020 event 6: 0x0c bhFlrAtariSet did not set flrp[0].flg bit 0");
  check(std::none_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.opcode==0x66||c.opcode==0x0c;}),"RM_3020 event 6 left 0x66/0x0c pending");
  std::cout<<"RM_3020 event 6: Obj_controll 12 default branch, 0x0c bhFlrAtariSet (flrp[0] bit 0 "<<before<<" -> 1), task parked at bhEnd @0413.\n";}
 // RM_0080 shutter switch (interaction 8, scd1 @0c46): with ev_flg 110 set (lock released)
 // and ev_flg 10 clear, "Push the switch?" Yes starts events 5-11/40/41. Events 7 and 8 are the
 // two zombies behind the shutter: Common 25 walk, Common 53 (@132a/@1360), then
 // Sub_controll 0x8f mode0 1 hands them to their own Brain. A task that stops at an
 // unparsed command leaves them in mode0 5 (script walk) for good.
 // ev_flg 110 is carried into the room (scd0 @022e places the switch work by it), so it is set before bhInitEvent.
 {Room r(data,"RM_0080",[](EventVm& vm,RoomWorld&){vm.set_flag(1,110,0);vm.set_flag(1,10,1);});for(unsigned t=0;t<5;++t)r.tick();
  const auto& a=r.vm.interactions.at(8);r.actor.position={a.position.x+a.extent.x*.5f,0,a.position.z+a.extent.z+3.5f};r.actor.yaw=0;r.world.place_player(r.actor.position);
  check(r.vm.interact(r.actor.position,r.actor.yaw,r.world.floor_at(r.actor.position))&&r.vm.values[20]==8&&r.dialog.index()==2,"RM_0080 shutter switch question missing");
  bool started=false,both_done=false;const auto* z0=r.world.state(1,0);const auto* z1=r.world.state(1,1);check(z0&&z1,"RM_0080 zombies missing");const auto p0=z0->transform.position,p1=z1->transform.position;
  for(unsigned t=0;t<1200&&!both_done;++t){r.tick();
   // Meldung 2 hat zwei Seiten: OK blaettert die erste um (message.c:325), die zweite traegt Ja/Nein.
   if(r.dialog.active()&&(r.dialog.turnable()||r.dialog.choosing())&&t%3==2){r.dialog.selection=0;r.vm.confirm_dialog();}
   if(r.task(7)&&r.task(8))started=true;
   if(started&&!r.task(7)&&!r.task(8))both_done=true;}
  check(started,"RM_0080 zombie events 7/8 never started");
  check(both_done&&r.vm.errors.empty(),"RM_0080 zombie events 7/8 did not finish (Common_controll 53 parse)");
  check(z0->enemy.mode0==1&&z1->enemy.mode0==1&&!z0->scripted&&!z1->scripted,"RM_0080 zombies stayed scripted dummies after the shutter (Sub_controll 0x8f not reached)");
  // Released to bhEne01 Brain: the next passes move them on their own, not a script walk.
  const auto q0=z0->transform.position,q1=z1->transform.position;for(unsigned t=0;t<240;++t)r.tick();
  check(z0->enemy.mode0==1&&z1->enemy.mode0==1&&(z0->transform.position.x!=q0.x||z0->transform.position.z!=q0.z)&&(z1->transform.position.x!=q1.x||z1->transform.position.z!=q1.z),"RM_0080 zombies did not move under their own Brain after release");(void)p0;(void)p1;
  std::cout<<"RM_0080 shutter: Common_controll 53 parsed, both zombies released to their Brain.\n";}
 // RM_3140 Ueberwachungskamera: scd1 (@04be) startet mit ev_flg 218 ueber 0x91 bhEffModeSet den
 // Raumeffekt 9 = bhEff137 (effsub1b.c:1015). Er besitzt die Kamera unter cam.flg 0x40, schwenkt mit
 // pad_on, zoomt mit OK und setzt ev_flg 65/66/72, die das Skript in Events umsetzt.
 {Room r(data,"RM_3140",[](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);vm.set_flag(1,218,0);}); // runtime.cpp: bind_player_collision vor Event_init
  for(unsigned t=0;t<2;++t)r.tick();
  check(std::none_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.opcode==0x91;}),"RM_3140 bhEffModeSet on bhEff137 unbound");
  auto cam=r.world.camera(r.scene,r.scene.cameras.at(0));
  if(!(cam.position.x==67.5f&&cam.perspective==14563))std::cerr<<"cam "<<cam.position.x<<","<<cam.position.y<<","<<cam.position.z<<" ang "<<cam.angles[0]<<","<<cam.angles[1]<<" pers "<<cam.perspective<<" ln "<<cam.distance<<" eff9 mode0 "<<(r.world.effect_state(9)?r.world.effect_state(9)->mode0:99)<<" mode1 "<<(r.world.effect_state(9)?r.world.effect_state(9)->mode1:99)<<" id "<<(r.world.effect_state(9)?r.world.effect_state(9)->id:0)<<" rm "<<std::hex<<r.vm.flags[4][0]<<" ev218 "<<r.vm.flag(1,218)<<" st "<<r.vm.flags[5][0]<<" pend "<<r.vm.pending_bindings.size()<<std::dec<<"\n";for(auto& e:r.vm.errors)std::cerr<<e<<"\n";
  check(cam.position.x==67.5f&&cam.position.y==29.3f&&cam.position.z==6.5f&&cam.perspective==14563&&cam.distance==-2.0f&&cam.angles[1]==(int(182.04445f*226.0f)&0xffff),"bhEff137 monitor camera pose differs");
  check((r.vm.flags[5][0]&1u)&&(r.vm.flags[10][0]&0x44u)==0x44u,"bhEff137 st_flg 1 / cb_flg 0x44 missing");
  for(unsigned t=0;t<70;++t)r.tick();
  r.world.set_pad(0x8,0x8);r.tick();r.world.set_pad(0,0);for(unsigned t=0;t<9;++t)r.tick();
  cam=r.world.camera(r.scene,r.scene.cameras.at(0));check(cam.angles[1]==(int(182.04445f*230.0f)&0xffff),"bhEff137 pad pan (8 steps of 0.5 deg) differs");
  r.world.set_pad(0x800,0x800);r.tick();r.world.set_pad(0,0);
  bool flag66=false;for(unsigned t=0;t<120&&!(r.dialog.active()&&r.dialog.index()==23);++t){r.tick();flag66=flag66||r.vm.flag(1,66);}
  check(flag66&&r.dialog.active()&&r.dialog.index()==23,"bhEff137 zoom did not set ev_flg 66 / room event 1 question");
  { // effsub1b.c:828 bhDraw137 ueber ef_fnc: Streifen, Fadenkreuz, Ecken, Pfeile, ZOOM/X/Y (Gruppe 440).
    SoftwareRenderer hud(640,480);std::fill(hud.canvas().begin(),hud.canvas().end(),0xff000000u);r.world.draw_fnc_huds(hud);
    const auto lit=[&](int x0,int y0,int x1,int y1){unsigned n=0;for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)if(hud.canvas()[std::size_t(y)*640+x]!=0xff000000u)++n;return n;};
    check(!r.world.unbound_enemy_routines().count("bhDraw137: Texturgruppe 440 (sys->ef_tn[440]) nicht geladen"),"bhDraw137 texture group 440 not loaded");
    check(lit(292,216,348,264)>200&&lit(144,352,240,368)>100&&lit(384,384,496,400)>100,"bhDraw137 monitor display (crosshair / ZOOM / Y digits) not drawn");
    // Texel = (u+0.5)/256 * Breite (vitof12 + GS-ST, Ps2AddPrim): die Klammern des Fadenkreuzes
    // (Gruppe 440 ist 128x128) sind hell, die roten Streifen nicht.
    unsigned white=0;for(int y=216;y<264;++y)for(int x=292;x<348;++x){const auto c=hud.canvas()[std::size_t(y)*640+x];if(((c>>8)&255u)>100&&(c&255u)>100)++white;}
    check(white>20,"bhDraw137 crosshair not sampled with normalized ST coordinates");}
  std::cout<<"RM_3140 surveillance camera: bhEff137 owns the camera, pans, zooms and hands ev_flg 66 to the room script.\n";}
 // Raumwechsel maskiert gm_flg (bhInitRoomChangeSystem, system.c:185: &= 0x9B8C00CB). RM_1130 verlaesst
 // den Raum nach "Climb down the ladder?" mit 57 00 (gm_flg 0x20000) ohne 57 01; dieses Bit darf in
 // RM_1080 die Rueckkehr der Untersuchungskamera der Luger-Tuer (Interaktion 1, attr 0x800e) nicht sperren.
 {Room before(data,"RM_1130");before.vm.flags[11][0]|=0x30800u;before.vm.set_flag(1,6,0); // Stand nach der Leiterfrage, Tasche aufgenommen (Diagnosevorbedingung)
  unsigned returned=0;
  Room r(data,"RM_1080",[&](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);vm.inherit_progress(before.vm);vm.return_cut=[&]{++returned;};});
  check(!(r.vm.flags[11][0]&0x30800u)&&(r.vm.flags[11][0]&0x8000000u),"room change keeps room-bound gm_flg camera bits or drops the side pack");
  r.vm.event_fixed_cut=[&](unsigned c,unsigned k){r.world.set_event_camera(c,k,false);};
  const auto step=[&]{r.vm.control_exm();r.tick();}; // runtime: bhMainSequence (control_exm) vor bhControlEvent
  for(unsigned t=0;t<4;++t)step();const unsigned before_door=returned;
  r.actor.position={20,0,-34};r.actor.yaw=0;r.world.place_player(r.actor.position);
  check(r.vm.interact(r.actor.position,r.actor.yaw,r.world.floor_at(r.actor.position))&&r.dialog.index()==1,"RM_1080 Luger door message missing");
  for(unsigned t=0;t<400&&r.dialog.active();++t){step();if(r.dialog.turnable()||r.dialog.closable())r.vm.confirm_dialog();}
  for(unsigned t=0;t<4;++t)step();
  if(!(!r.dialog.active()&&returned>before_door&&!(r.vm.flags[5][0]&1u)))std::cerr<<std::hex<<"dlg "<<r.dialog.active()<<" ret "<<returned<<"/"<<before_door<<" st "<<r.vm.flags[5][0]<<" gm "<<r.vm.flags[11][0]<<" sp "<<r.vm.flags[6][0]<<" cb "<<r.vm.flags[10][0]<<std::dec<<"\n";
  check(!r.dialog.active()&&returned>before_door&&!(r.vm.flags[5][0]&1u),"RM_1080 Luger door close-up camera never returns (bhCheckCut(1))");
  std::cout<<"Room change masks gm_flg (system.c:185); RM_1080 Luger door camera returns after the message.\n";}
 // RM_1050 Monitor (Raumeffekt 1, bhEff111 effsub1.c:6927): scd0 schaltet ihn mit ev_flg 0x92 per
 // 0x91 01 00 live; der Effekt fordert ueber gm_flg 0x200 bhDrawSmallScreenRenderTexture (screen.c:920)
 // an, das Schnitt 6 (Steve) ohne Spieler in eine 512x512-Textur zeichnet; bhDrawThlEffect3D zeigt sie.
 {Room r(data,"RM_1050",[](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);vm.set_flag(1,0x92,0);});
  for(unsigned t=0;t<3;++t)r.tick();
  check(std::none_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.opcode==0x91;}),"RM_1050 bhEffModeSet on bhEff111 unbound");
  const auto* m=r.world.effect_state(1);check(m&&m->id==111&&m->mode0==2&&m->tex_id==111&&m->lkono==1&&(m->flg&0x4020000u)==0x4020000u,"bhEff111 state differs (room.c:874-878 bhSetEffectLink lkflg 0: op->lkono = efp->lkono = 1 for RM_1050 entry 1, so effsub1.c:7036 takes the ocol 0x400000/ct0 64 branch)");
  check(r.vm.flags[11][0]&0x200u,"bhEff111 does not request the small-screen render (gm_flg 0x200)");
  SceneCamera view;view.position={24.7329f,14.44f,32.7362f};view.angles={0,0,0};view.perspective=10012;view.clip_near=1;view.clip_far=20000;
  const auto rt=r.world.small_screen_pass(view);check(rt&&rt->slot==1&&rt->cut==6,"monitor render pass missing for a camera that sees the monitor");
  Scene screen=r.scene;r.world.append(screen,{},rt->camera,{true,rt->cut});
  SoftwareRenderer target(512,512);target.set_supersampling(1);target.set_render_threads(1);target.draw_screen(screen,rt->camera,rt->dist);
  check(target.covered_pixels()>1000,"cut 6 rendered nothing into the monitor texture");
  r.world.commit_small_screen(target.pixels(),512,512);
  Scene shown=r.scene;const auto base=shown.triangles.size();r.world.append(shown,{},view);
  unsigned monitor=0;for(std::size_t i=base;i<shown.triangles.size();++i){const auto& t=shown.triangles[i];if(t.textured&&t.texture<shown.textures.size()&&shown.textures[t.texture].width==512&&t.blend==SceneBlend::Opaque)++monitor;}
  check(monitor==2,"bhDrawThlEffect3D monitor quad with the render texture missing");
  // Fernsicht aus Schnitt 1 (Raumnebel an: CAM_WRK Nebel 0x28281C, near 0, far -105; Monitor ~85 Einheiten vor
  // der Kamera). njDrawTextureSub3DH (ps2_NaDraw.c:443) schreibt XYZF2.F = 128 fest in jeden Eckpunkt, FOGCOL =
  // ocol (:469; 0x400000 bei lkono 1): Cv = (Cs*128 + ocol*127) >> 8 unabhaengig von Entfernung und Raumnebel.
  // Mit grauem Testbild (0x80 je Kanal, Grundfarbe 0xFF/0xC0 im Wechsel) liegen G/B des Monitors bei 64 bzw. 48
  // und R um 0x1F darueber; Distanznebel (F << 128) liesse von dem Bild nur die Nebelfarbe uebrig.
  // Kamerapose = Laufzeitpose von Schnitt 1 am Startplatz (headless --state camera_pose), damit das Rechteck mit
  // dem Runtime-Endbild uebereinstimmt; die Erwartung haengt nicht vom Raumnebel ab (weder ungenebelt 128/96 noch
  // Distanznebel erfuellen G/B 48..64 mit R um 0x1F darueber).
  {SceneCamera far;far.position={22.1396f,20.2529f,110.037f};far.angles={1786,-2288,0};far.distance=0;far.perspective=10012;far.clip_near=20;far.clip_far=20000;
   const auto draw=[&](std::uint32_t fill){std::vector<std::uint32_t> flat(512u*512u,fill);r.world.commit_small_screen(flat,512,512);Scene s=r.scene;r.world.append(s,{},far);SoftwareRenderer out(640,480);out.set_supersampling(1);out.set_render_threads(1);out.draw(s,far);return out.pixels();};
   const auto ka=draw(0xff00ff00u),kb=draw(0xff0000ffu),grey=draw(0xff808080u);
   unsigned n=0;double rs=0,gs=0,bs=0;for(std::size_t i=0;i<ka.size();++i)if(ka[i]!=kb[i]){++n;rs+=(grey[i]>>16)&255u;gs+=(grey[i]>>8)&255u;bs+=grey[i]&255u;}
   unsigned x0=640,x1=0,y0=480,y1=0;for(std::size_t i=0;i<ka.size();++i)if(ka[i]!=kb[i]){x0=std::min<unsigned>(x0,unsigned(i%640));x1=std::max<unsigned>(x1,unsigned(i%640));y0=std::min<unsigned>(y0,unsigned(i/640));y1=std::max<unsigned>(y1,unsigned(i/640));}
   check(n>200,"monitor quad not visible from cut 1");
   const double rm=rs/n,gm=gs/n,bm=bs/n;const bool half=gm>40&&gm<72&&bm>40&&bm<72&&rm>gm+20&&rm<gm+44;
   std::cout<<"RM_1050 cut 1 monitor quad: "<<n<<" px in x "<<x0<<".."<<x1<<" y "<<y0<<".."<<y1<<", mean RGB "<<rm<<"/"<<gm<<"/"<<bm<<" (grey 0x80 test image, F=128 blend with ocol 0x400000)\n";
   check(half,"bhDrawThlEffect3D far view is not the fixed F=128 blend with ocol (ps2_NaDraw.c:443): distance fog or wrong fog colour");}
  // screen.c:928-939: ohne initflag rendert der Monitor nur unter st_flg 0x1 oder gm_flg 0x20 (Schnittwechsel,
  // cut.c:298/455/1651/1731; bhCheckCut cut.c:22 loescht es je Bild). Diagnosevorbedingung: st_flg 0x1 aus.
  r.world.set_fixed_cut(5);check(r.vm.flags[11][0]&0x20u,"bhSetFixedCut did not raise gm_flg 0x20");r.vm.flags[5][0]&=~1u;
  check(bool(r.world.small_screen_pass(view)),"a cut change (gm_flg 0x20) does not refresh the monitor");
  r.world.commit_small_screen(target.pixels(),512,512);r.world.begin_cut_check();
  check(!(r.vm.flags[11][0]&0x20u)&&!r.world.small_screen_pass(view),"bhCheckCut does not clear gm_flg 0x20; the monitor re-renders without initflag, st_flg 1 or a cut change");
  std::cout<<"RM_1050 monitor: bhEff111 live, cut 6 rendered into the 512x512 texture and shown on the quad.\n";}
 // RM_1050 Steve-Ereignis: Ereignis 31 (@2a74) startet 12..17 (Diagnosevorbedingung: direkter Start, ev_flg 146 gesetzt).
 // Ereignis 12 ruft @144a 0xb0 bhHEffectSet2 (event.c:7472): sys->ef = {type 1, Lage 30.20/12.60/22.00, ax 5 Grad,
 // ay 180 Grad, Skalierung 1.70/1.10/4.00, id 211} -> bhSetEffectTb(&sys->ef, NULL, NULL, 0): bhEff211 (effsub5.c:1039,
 // Monitorrauschen) am Monitor. Danach wartet 12 per bhFrameCheck auf Steves frm_no 642 und endet mit CineSet 1 (@14b0);
 // ohne 0xb0 blieb Task 12 davor stehen und das Cinematic lief bis zum SELECT-Skip weiter.
 {Room r(data,"RM_1050",[](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);vm.set_flag(1,0x92,0);});
  for(unsigned t=0;t<3;++t)r.tick();
  for(unsigned e=12;e<=17;++e)r.vm.start(e-11,e);
  for(unsigned t=0;t<3;++t)r.tick();
  check(r.vm.flags[10][0]&4u,"RM_1050 Steve cinematic did not start (cb_flg 4)");
  const EffectSprite* noise=nullptr;for(unsigned i=0;const auto* e=r.world.effect_state(i);++i)if(e->id==211&&(e->flg&1u)){noise=e;break;}
  check(noise!=nullptr,"bhHEffectSet2 (0xb0) did not allocate bhEff211 in eff[512]");
  check(noise->type==1&&noise->lkono==0&&std::fabs(noise->px-30.2f)<1e-3f&&std::fabs(noise->py-12.6f)<1e-3f&&std::fabs(noise->pz-22.0f)<1e-3f&&std::fabs(noise->sx-1.7f)<1e-4f&&std::fabs(noise->sy-1.1f)<1e-4f&&std::fabs(noise->sz-4.0f)<1e-4f&&(noise->ax&0xffff)==910&&(noise->ay&0xffff)==32768,"bhHEffectSet2 fields differ from event.c:7472 (type, lkono v4, px/py/pz, sx/sy/sz, ax/ay as short degrees)");
  unsigned ticks=0;while(r.task(12)&&ticks++<3000)r.tick();
  check(!r.task(12)&&!r.task(15),"RM_1050 event 12/15 never finished");
  check(!(r.vm.flags[10][0]&4u)&&!r.world.player_scripted,"RM_1050 Steve cinematic keeps cb_flg 4 / player control after event 12 (CineSet 1 @14b0 not reached)");
  std::cout<<"RM_1050 Steve: bhHEffectSet2 places bhEff211 on the monitor; event 12 reaches CineSet 1 without SELECT.\n";}
 // bhSetDispObj (0x20, event.c:2283) schreibt fuer Gegnerworks mdflg 0x1 (Zeichentor eneset.c:814),
 // bhEneRenderSet (0xa5, event.c:6985) mdflg 0x200; beide bleiben am Work ueber den naechsten Pass.
 {Room r(data,"RM_0080");for(unsigned t=0;t<3;++t)r.tick();
  const auto* z=r.world.state(1,0);check(z&&z->enemy_bound&&z->enemy_native,"RM_0080 zombie 0 not a native work");
  check(r.world.bind({0x20,0,0,0,{0x20,0,1,0}})&&(z->enemy.mdflg&0x1u)&&!(z->enemy.mdflg&0x200u),"bhSetDispObj must set mdflg 0x1, not 0x200");
  r.tick();check(z->enemy.mdflg&0x1u,"bhSetDispObj mdflg 0x1 lost after a pass");
  check(r.world.bind({0x20,0,0,0,{0x20,0,1,1}})&&!(z->enemy.mdflg&0x1u),"bhSetDispObj show does not clear mdflg 0x1");
  check(r.world.bind({0xa5,0,0,0,{0xa5,0,0,0}})&&(z->enemy.mdflg&0x200u),"bhEneRenderSet must set mdflg 0x200");
  r.tick();check(z->enemy.mdflg&0x200u,"bhEneRenderSet mdflg 0x200 lost after a pass");
  check(r.world.bind({0xa5,0,0,0,{0xa5,0,1,0}})&&!(z->enemy.mdflg&0x200u),"bhEneRenderSet show does not clear mdflg 0x200");
  std::cout<<"bhSetDispObj/bhEneRenderSet write mdflg 0x1/0x200 on the enemy work and keep them.\n";}
 // RM_0110 (zombie 4 haelt ein Item): scd1 @06ec bhEtcAtariEnePosSet (0xb3) waehlt aus rom->posp[3..59) den
 // Platz naechst owP[9] des Gegners, zentriert etcp 12 darum und merkt ihn in sys->evt_posno[0]; scd0 @0184
 // bhEtcAtariEvtPosSet (0xb4) stellt Feld 12 und itwp[4] beim Wiederbetreten dort hin. evt_posno ist global
 // (SAVEFILE 0x820). Diagnosevorbedingungen: ed_flg 35 und rm_flg 7 wie nach dem Tod des Zombies.
 {Room r(data,"RM_0110",[](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);});
  for(unsigned t=0;t<3;++t)r.tick();
  r.vm.set_flag(3,35,0); // der Zombie stirbt waehrend dieses Besuchs
  const auto joint=r.world.enemy_joint_position(4,9);check(bool(joint),"RM_0110 zombie 4 joint 9 unavailable");
  const auto& posp=r.vm.room_positions();float best=1e30f;unsigned want=3;
  const auto dist=[&](const NJS_POINT3& a){const float dx=a.x-joint->x,dy=a.y-joint->y,dz=a.z-joint->z;return std::sqrt(dx*dx+dy*dy+dz*dz);};
  best=dist(posp.at(3));for(unsigned c=3;c<59;++c)if(dist(posp[c])<best){best=dist(posp[c]);want=c;}
  r.vm.set_flag(4,7,0);r.tick();
  check(r.vm.errors.empty(),"RM_0110 bhEtcAtariEnePosSet blocked the scenario");
  check(r.vm.evt_posno[0]==want,"bhEtcAtariEnePosSet chose another rom->posp entry than the nearest to owP[9]");
  const auto& a=r.vm.interactions.at(12);check(std::fabs(a.position.x-(posp[want].x-a.extent.x/2))<1e-4f&&std::fabs(a.position.z-(posp[want].z-a.extent.z/2))<1e-4f,"etcp 12 not centred on the chosen position");
  Room again(data,"RM_0110",[&](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);vm.inherit_progress(r.vm);});
  for(unsigned t=0;t<2;++t)again.tick();
  check(again.vm.errors.empty()&&again.vm.evt_posno[0]==want,"sys->evt_posno lost on re-entry or 0xb4 blocked scd0");
  const auto item=again.world.position(3,4);check(item&&item->x==posp[want].x&&item->z==posp[want].z,"bhEtcAtariEvtPosSet did not move itwp[4]");
  std::cout<<"RM_0110 bhEtcAtariEnePosSet/EvtPosSet: nearest rom->posp "<<want<<", etcp 12 and itwp[4] follow it on re-entry.\n";}
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
 // bhLoadWork (0x65) / bhLoadWorkEx (0xB5) Fall 0 und bhLoadWork2 (0x68) Fall 0 schreiben
 // den Spielerwork direkt (event.c:12722-12737, :12822-12837, :12909-12919).
 {
  Room r(data,"RM_0000");r.world.bind_player_collision(r.vm);
  const auto bind=[&](unsigned op){std::vector<std::uint8_t> b{std::uint8_t(op),0,0,0};if(op==0x68)b.push_back(0);EventCommand c{op,0,0,0,b};return r.world.bind(c);};
  r.world.set_player_mode(0x03020100u);r.vm.flags[13][0]=0;r.vm.flags[14][0]=0x80;r.motion=0x1234;
  check(bind(0x65),"bhLoadWork case 0 not bound");
  check(r.world.player_mode()==0x03000107u,"bhLoadWork case 0 does not write mode0 7 / mode2 0");
  check((r.motion&0xffu)==42u&&(r.motion>>8)==0x12u,"bhLoadWork case 0 does not write only the mtn_no low byte (event.c:12728)");
  check((r.vm.flags[13][0]&0x10000u)&&(r.vm.flags[14][0]&0x10000u)&&!(r.vm.flags[14][0]&0x80u),"bhLoadWork case 0 misses flg/stflg 0x10000 or keeps stflg 0x80");
  r.world.set_player_mode(0x03020100u);r.vm.flags[13][0]=0;r.vm.flags[14][0]=0x80;r.motion=0x1234;
  check(bind(0xb5),"bhLoadWorkEx case 0 not bound");
  check(r.world.player_mode()==0x03000107u&&(r.motion&0xffu)==42u&&(r.vm.flags[13][0]&0x10000u)&&(r.vm.flags[14][0]&0x10000u)&&!(r.vm.flags[14][0]&0x80u),"bhLoadWorkEx case 0 differs from event.c:12822-12837");
  r.world.set_player_mode(0x03020100u);r.vm.flags[13][0]=0;r.vm.flags[14][0]=0x80;r.motion=0x1234;
  check(bind(0x68),"bhLoadWork2 case 0 not bound");
  check(r.world.player_mode()==0x03020107u,"bhLoadWork2 case 0 does not write mode0 7 (event.c:12913)");
  check(r.motion==0x1234&&(r.vm.flags[13][0]&0x10000u)&&(r.vm.flags[14][0]&0x10000u)&&(r.vm.flags[14][0]&0x80u),"bhLoadWork2 case 0 writes beyond mode0 / stflg 0x10000 / flg 0x10000");
 }
 auto drawn_enemy_triangles=[](const Room& r){Scene s;r.world.append(s,{},r.scene.cameras.at(0));std::size_t n=0;for(const auto& t:s.triangles)if(t.light_category==1&&t.after_modifiers)++n;return n;};
 std::size_t natural_enemy_triangles=0;
 {
  Room r(data,"RM_0000");r.world.bind_player_collision(r.vm);unsigned ticks=0;
  while(!(r.dialog.active()&&r.dialog.index()==4)&&ticks<5000){if(ticks==1)r.input.menu_input(MenuInput::Confirm);if(ticks>1&&r.movie.active())r.input.menu_input(MenuInput::Confirm);r.tick();++ticks;}
  check(r.dialog.active()&&r.dialog.index()==4,"natural fixture misses the original hint");
  for(unsigned t=0;t<200&&!r.dialog.closable();++t)r.tick(); // erst die aufgebaute Seite nimmt OK an (game.c:264)
  r.input.menu_input(MenuInput::Confirm);
  for(unsigned t=0;t<100&&!r.menu.active;++t)r.tick();check(r.menu.active,"natural fixture loses the automatic inventory request");for(unsigned t=0;t<40;++t)r.tick();
  r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();r.input.menu_input(MenuInput::Confirm);for(unsigned t=0;t<8;++t)r.tick();
  r.input.menu_input(MenuInput::Cancel);r.input.menu_input(MenuInput::Cancel);
  unsigned wait=0;while(!(r.vm.flags[4][0]&0x00400000u)&&wait++<4000)r.tick();check(r.vm.flags[4][0]&0x00400000u,"natural cell cinematic never started");
  // Der Zellenschnitt fuehrt bhLoadWork/bhLoadWorkEx Fall 0 aus (RECVX_EVENT_TRACE,
  // scd1 op=65 Byte 2 = 0). Er setzt flg/stflg 0x10000 an derselben Stelle, an der
  // scd1 die Cinematic beendet; bhCheckSubTask loescht sie im selben Durchlauf
  // (system.c:736-744). Der Zwischenstand ist deshalb nur am Befehl selbst
  // beobachtbar (Testblock oben); hier zaehlt, dass die Freigabe echte Schreiber trifft.
  unsigned release=0;while((r.vm.flags[10][0]&4)&&release++<6000)r.tick();check(!(r.vm.flags[10][0]&4),"natural cell cinematic never ends");
  for(unsigned t=0;t<400;++t)r.tick();
  const auto* stand_in=r.world.state(1,0);check(stand_in&&stand_in->inactive,"natural cell cinematic leaves the stand-in active");
  natural_enemy_triangles=drawn_enemy_triangles(r);
  check(natural_enemy_triangles>0,"natural cell cinematic end draws no enemy work (Rodrigo expected)");
 }
 for(unsigned delay:{0u,60u,200u,400u,700u}){
  Room r(data,"RM_0000");r.world.bind_player_collision(r.vm);unsigned ticks=0;
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
  // bhLoadWork/bhLoadWorkEx Fall 0 (event.c:12722-12737) setzen diese Bits im
  // Zellenschnitt; die Freigabe muss sie hier wieder loeschen.
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
 // RM_5010 Ereignis 8 (scd1 @026c: ELSE-Zweig von IF rm_flg 0x4, den en19.c:1068 bei TY_BR0_PINCH oder
 // hp < 0 setzt; Katapulthebel mit liegendem Tyrant): Befehl 0xc8 @09ea = event.c:8722 bhEffBakuDrmSet,
 // Vorzeichenbyte 2 und drei u16/100 -> (200, -18, 22), dann weapon.c:1812 bhSetExplosionEffect am
 // Weltbesitzer (RoomWorld::explode_effect): Feuerball 23 und zwei Truemmer 24 im eff[512]-Pool.
 // Expliziter Ereignisstart; die bhFrameCheck-Warten davor laufen auf den EN19-Motions (Common_controll 24).
 // Gegenprobe im selben Lauf: ohne Bindung von 0xc8 bleibt der Befehl pending und der Pool ohne 23/24.
 for(const bool bind_c8:{true,false}){
  int hp=160;std::uint32_t head=0; // plp fuer bhEne19 (native/enemies/en19.c:62 player_get), wie runtime.cpp bind_enemy_player
  Room r(data,"RM_5010",[&](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);w.bind_enemy_player(hp,head);vm.set_flag(1,269,0);}); // 1:269 gesetzt: kein Erstbesuchs-Ereignis 0-3
  r.world.set_enemy_ai(true); // Gegnerpass (eneset.c:352) treibt die EN19-Motion der Warten
  const auto inner=r.vm.binding;r.vm.binding=[&,inner](const EventCommand& c){if(c.opcode==0xc8&&!bind_c8)return false;return inner(c);};
  const auto played_before=r.audio.played.size();
  r.vm.start(0,8);r.vm.set_flag(6,27,0); // scd1 @0204 setzt im Hebelzweig sp_flg 0x10 (bhEventScheduler2, event.c:13024) neben den EvtStarts
  bool reached=false,fire=false,debris=false;unsigned ticks=0;
  for(;ticks<1500&&r.task(8);++ticks){r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();
   for(const auto& t:r.vm.task_states())if(t.event==8&&t.offset>=0x9f2)reached=true;
   for(unsigned i=0;const auto* e=r.world.effect_state(i);++i){if(!e->flg)continue;if(e->id==23)fire=true;if(e->id==24)debris=true;}}
  for(const auto& err:r.vm.errors)std::cerr<<"RM_5010: "<<err<<'\n';
  check(r.vm.errors.empty(),"RM_5010 event 8 blocked on an unimplemented opcode (0xc8 bhEffBakuDrmSet)");
  check(reached,"RM_5010 event 8 never passed 0x9ea (bhFrameCheck waits on the EN19 motions stalled)");
  const bool pending=std::any_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.opcode==0xc8;});
  if(bind_c8){check(!pending,"0xc8 bhEffBakuDrmSet left unbound");
   for(const auto& gap:r.world.unbound_enemy_routines())if(gap.find("bhSetExplosionEffect")!=std::string::npos)throw std::runtime_error("RM_5010 event 8: "+gap);
   check(fire&&debris,"bhSetExplosionEffect from the event owner spawned no fireball 23 / debris 24");
   // 0x86 @0908 bhEasySESet Typ 7, Art 2 Index 0 (sys->obwp[0], die Waffe), Knoten 0, SeNo 0x206:
   // CallNativeEventSe Mode 1 (sdfunc.c:1861) liest pPos nicht, der Ruf laeuft ohne owP-Aufloesung.
   // In Ereignis 8 nennt nur dieser Befehl Sequenz 6 der Bank 2.
   for(const auto& gap:r.world.unbound_enemy_routines())if(gap.find("bhEasySESet")!=std::string::npos)throw std::runtime_error("RM_5010 event 8: "+gap);
   const auto seq6=std::count_if(r.audio.played.begin()+std::ptrdiff_t(played_before),r.audio.played.end(),[](const std::string& s){return s.find("/2/seq6/")!=std::string::npos;});
   if(seq6!=1)std::cerr<<"RM_5010 event 8: bank 2 seq 6 played "<<seq6<<" times\n";
   check(seq6==1,"RM_5010 event 8: bhEasySESet @0908 (obwp[0], CallNativeEventSe mode 1) did not play bank 2 sequence 6");}
  else check(pending&&!fire&&!debris,"counter-check: without the 0xc8 binding the pool must stay without 23/24");
 }
 std::cout<<"RM_5010 event 8: 0xc8 bhEffBakuDrmSet -> bhSetExplosionEffect spawns 23/24; unbound counter-check leaves the pool empty; 0x86 @0908 plays bank 2 seq 6 without owP.\n";
 // RM_3230 Ereignis 3 (@0af4): WHILE bhFrameCheck(Art 1 Index 2, frm_no < 124<<16) WAIT. rom->enep[2] ist
 // bhSubpl id 96 (subpl.c:357 mv00_subpl0_1: nur mode3 0/1 schieben). Ereignis 1 (@09e8 Common_controll 24,
 // mtn_add 0x8000; @0a0c bhMotionPauseSet 0 -> mode3 4) wartet bis frm_no >= 123<<16; bhEwhile (event.c:9147)
 // gibt beim Schleifenende 0 zurueck, der Gegnerpass des naechsten Bildes schiebt noch einmal um 0x8000, dann
 // steht der Work bei 123.5. 124<<16 wird nie erreicht: Originalverhalten, der Raumwechsel beendet die Task.
 {int hp=160;std::uint32_t head=0;Room r(data,"RM_3230",[&](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);w.bind_enemy_player(hp,head);});r.world.set_enemy_ai(true);
  const auto* e=r.world.state(1,2);check(e&&e->identity==96,"RM_3230 rom->enep[2] is not bhSubpl id 96");
  // scd1 @0770 startet die Szene nur mit ev_flg 260 frei und cb_flg Bit 22 (Bodenfeld 0 des Spielers); hier explizit
  // Ereignis 1 (Ebene 1, Motion/Pause) und 3 (Ebene 3, Warte 124) mit sp_flg 0x10 wie @078c, sp_flg 0x1 frei wie @0786.
  r.vm.start(1,1);r.vm.start(3,3);r.vm.set_flag(6,27,0);r.vm.set_flag(6,31,1);r.tick();check(r.task(3)&&r.task(1),"RM_3230 events 1 and 3 did not start");
  std::int64_t peak=0;bool paused=false;unsigned ticks=0;
  for(;ticks<800&&r.task(3);++ticks){r.tick();if(e->enemy_bound){peak=std::max<std::int64_t>(peak,e->enemy.frm_no);if(e->enemy.mode3==4)paused=true;}}
  if(!(paused&&e->enemy.mode3==4&&e->enemy.frm_no==123*65536+32768&&peak==123*65536+32768))std::cerr<<"RM_3230 enep[2] mode3 "<<unsigned(e->enemy.mode3)<<" frm_no "<<e->enemy.frm_no/65536.0<<" peak "<<peak/65536.0<<" ticks "<<ticks<<'\n';
  check(paused&&e->enemy.mode3==4&&e->enemy.frm_no==123*65536+32768&&peak==123*65536+32768,"RM_3230 enep[2] does not hold at 123.5 under bhMotionPauseSet (subpl.c:357, Motion.c:21)");
  bool waiting=false;for(const auto& t:r.vm.task_states())if(t.event==3&&t.offset>=0xaf4&&t.offset<=0xafd)waiting=true;
  check(waiting&&r.vm.errors.empty(),"RM_3230 event 3 left its 124 wait although frm_no never reaches 124<<16 (original behaviour)");
  std::cout<<"RM_3230 event 3: enep[2] (bhSubpl 96) holds at 123.5 after bhMotionPauseSet, the 124 wait stays open as in the original.\n";}
 // RM_9370 Ereignis 5 (@133e): zehn 0xc8 = event.c:8722 bhEffBakuDrmSet (@1ad4/1adc/1b00/1b1a/1c00/2396/23aa/
 // 23c2/23ce/23da), dazwischen bhCamCheck (0x62, event.c:4881: sp_flg 0x40, st_flg 0x1, cam.flg 0x2, evc_no)
 // auf die Kameras 6/29/31 von Ereignis 2 und bhFrameCheck (0x38) auf rom->enep[0/1] (bhSubpl 92/97), Ende bhEvtEnd
 // @23f2. Kein Ausloeser ausser dem Raumeintritt: scd0 @0054 loescht ev_flg 458, scd1 @0430 prueft 458 frei und
 // startet @044c Ebene 5 Ereignis 5 (Eintritt aus RM_9091 Ereignis 2 @036e, Tuer 09/25). Hier also kein expliziter
 // Start, sondern derselbe scd1-Pfad. Jede 0xc8 fuehrt weapon.c:1812 bhSetExplosionEffect aus: genau ein Feuerball 23
 // Art 1 (seine Nachfolgeballe aus bhEff023 sind Art 0), zwei Truemmer 24, aus denen 25 entstehen.
 // Gegenprobe im selben Lauf: ohne Bindung von 0xc8 bleibt der Befehl pending und kein 23 Art 1 entsteht.
 for(const bool bind_c8:{true,false}){
  int hp=160;std::uint32_t head=0;
  // player.c:840 bhSetPlayer: bhCalcModel(plp) vor Event_init und danach je Bild (bhControlPlayer), wie runtime.cpp
  // commit_player_joints; bhEff001 der Raumskripte liest plp->mlwP->owP.
  const PlayerAsset pose_body=PlayerAsset::load(data);const auto spawn=decode_scene(read_asset_file(data/"rooms/RM_9370.RDT",64*1024*1024)).spawns.at(0);
  const auto commit=[&](RoomWorld& w,const SceneSpawn& a,std::size_t m,std::int64_t f){std::vector<BoneMatrix> nodes;if(w.player_scripted)(void)w.sample_player(pose_body,&nodes);else nodes=pose_body.pose(m,f,a.position,a.yaw);w.commit_player_joints(pose_body.script_nodes(nodes));};
  Room r(data,"RM_9370",[&](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);w.bind_enemy_player(hp,head);commit(w,spawn,42,0);});
  r.world.set_enemy_ai(true); // Gegnerpass (eneset.c:352) treibt die bhSubpl-Motions der bhFrameCheck-Warten
  r.vm.event_camera=[&](unsigned& index,unsigned& key){index=r.world.event_camera_index;key=r.world.event_camera_key();return r.world.event_camera;}; // wie runtime.cpp
  // event.c:6519 bhEffModeSet (0x91, 4 Bytes) auf Raumeffekt 57 = bhEff085 (effsub6.c:18, rom->effp[57] lkflg 2
  // lkno 1): Ereignis 2 @0fb0 mode1 1, Ereignis 0 @07ee mode1 2 und @07f2 mode1 0. Der Tausch zeigt sich als
  // derselbe texaddr (geteilte Pixel) wie das erste Bild der Effektgruppe 85. @07ee und @07f2 geben beide 1
  // zurueck und laufen im selben Schedulerpass (event.c:13047), nach bhControlEffect (main.c:195): bhEff085
  // sieht mode1 2 nie, ene[1] behaelt den getauschten Eintrag (Originalverhalten, kein Zuruecksetzen).
  struct ModeSet{unsigned mode;bool bound;unsigned pass;};std::vector<ModeSet> mode_sets;unsigned passes=0,swapped_ticks=0,restored_after_swap=0;
  const auto* tex1=r.world.model_textures(1,1);const auto* group85=r.world.effect_group_textures(85);
  check(tex1&&tex1->size()>3&&group85&&!group85->empty(),"RM_9370 ene[1] texP[3] or effect group 85 missing");
  const auto* original3=(*tex1)[3].pixels.data();const auto* effect85=group85->front().pixels.data();
  unsigned requests=0,hits=0,hits_bound=0;const auto inner=r.vm.binding;r.vm.binding=[&,inner](const EventCommand& c){if(c.opcode==0xc8&&!bind_c8)return false;const bool bound=inner(c);if(c.opcode==0xc8&&bound)++requests;if(c.opcode==0x6d){++hits;if(bound)++hits_bound;}if(c.opcode==0x91&&c.bytes.size()>2&&c.bytes[1]==0x39)mode_sets.push_back({c.bytes[2],bound,passes});return bound;};
  r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();commit(r.world,r.actor,r.motion,r.frame);
  check(r.task(5),"RM_9370 scd1 @044c did not start event 5 on room entry (ev_flg 458 cleared by scd0 @0054)");
  std::vector<int> fireball_ct(1024,-1);unsigned fireballs=0,ticks=0,peak23=0,peak24=0,peak25=0;std::size_t peak_particles=0;
  for(;ticks<8000&&r.task(5);++ticks){r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();commit(r.world,r.actor,r.motion,r.frame);
   unsigned n23=0,n24=0,n25=0;
   for(unsigned i=0;const auto* e=r.world.effect_state(i);++i){const bool live=(e->flg&1u)!=0;
    if(live&&e->id==23)++n23;if(live&&e->id==24)++n24;if(live&&e->id==25)++n25;
    const bool primary=live&&e->id==23&&e->type==1; // nur bhSetExplosionEffect setzt 23 Art 1
    if(i<fireball_ct.size()){if(primary&&(fireball_ct[i]<0||e->ct0<fireball_ct[i]))++fireballs;fireball_ct[i]=primary?e->ct0:-1;}}
   peak23=std::max(peak23,n23);peak24=std::max(peak24,n24);peak25=std::max(peak25,n25);peak_particles=std::max(peak_particles,r.world.blood_particles());
   ++passes;const auto* now3=(*r.world.model_textures(1,1))[3].pixels.data();if(now3==effect85)++swapped_ticks;else if(swapped_ticks&&now3==original3)++restored_after_swap;}
  // Ereignis 0 kann nach Ereignis 5 enden: bis zum Befehl @07f2 weiterlaufen lassen (gleicher Raum).
  for(unsigned extra=0;extra<4000&&mode_sets.size()<3;++extra){r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();commit(r.world,r.actor,r.motion,r.frame);
   ++passes;const auto* now3=(*r.world.model_textures(1,1))[3].pixels.data();if(now3==effect85)++swapped_ticks;else if(swapped_ticks&&now3==original3)++restored_after_swap;}
  for(unsigned extra=0;extra<8;++extra){r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();++passes;const auto* now3=(*r.world.model_textures(1,1))[3].pixels.data();if(now3==effect85)++swapped_ticks;else if(swapped_ticks&&now3==original3)++restored_after_swap;}
  for(const auto& err:r.vm.errors)std::cerr<<"RM_9370: "<<err<<'\n';
  check(r.vm.errors.empty(),"RM_9370 event 5 blocked");
  check(!r.task(5),"RM_9370 event 5 did not reach bhEvtEnd @23f2");
  const bool pending=std::any_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.opcode==0xc8;});
  if(bind_c8){
   for(const auto& gap:r.world.unbound_enemy_routines())std::cerr<<"RM_9370 event 5 gap: "<<gap<<'\n';
   check(r.world.unbound_enemy_routines().empty(),"RM_9370 event 5 reported gaps");
   if(requests!=10||fireballs!=10)std::cerr<<"RM_9370 event 5: "<<requests<<" bhEffBakuDrmSet, "<<fireballs<<" fireballs 23 type 1\n";
   check(!pending&&requests==10&&fireballs==10,"RM_9370 event 5: expected ten bhEffBakuDrmSet -> bhSetExplosionEffect (ten fireballs 23 type 1)");
   check(peak24>=2&&peak25>0,"RM_9370 event 5: explosions spawned no debris 24 / 25");
   // event.c:5028 bhCyodanSet (0x6d, 22 Byte) -> effsub3.c:911 bhSetEffParticle: Treffer der Gefechtsszene.
   for(const auto& c:r.vm.pending_bindings)if(c.event==5)std::cerr<<"RM_9370 event 5 pending opcode 0x"<<std::hex<<unsigned(c.opcode)<<" @"<<c.offset<<std::dec<<'\n';
   check(std::none_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.event==5;}),"RM_9370 event 5 left commands pending");
   check(hits>0&&hits_bound==hits&&peak_particles>0,"RM_9370 event 5: bhCyodanSet (0x6d) did not spawn bhSetEffParticle particles");
   std::string modes;for(const auto& m:mode_sets)modes+=std::to_string(m.mode)+(m.bound?"b@":"p@")+std::to_string(m.pass)+" ";
   std::cout<<"RM_9370 effect 57 (bhEff085): 0x91 mode1 sequence "<<modes<<"; ene[1] texP[3] = ef_tn[85] for "<<swapped_ticks<<" passes, restored "<<restored_after_swap<<".\n";
   check(mode_sets.size()==3&&mode_sets[0].mode==1&&mode_sets[1].mode==2&&mode_sets[2].mode==0&&mode_sets[0].bound&&mode_sets[1].bound&&mode_sets[2].bound,"RM_9370: 0x91 bhEffModeSet on effect 57 not bound in the order 1 (event 2), 2, 0 (event 0)");
   check(mode_sets[1].pass==mode_sets[2].pass&&mode_sets[0].pass<mode_sets[1].pass,"RM_9370: 0x91 @07ee/@07f2 not in the same scheduler pass");
   check(swapped_ticks>0&&restored_after_swap==0&&(*r.world.model_textures(1,1))[3].pixels.data()==effect85,"RM_9370 bhEff085: ene[1] texP[3] must hold ef_tn[85] from event 2 on (mode1 2 is overwritten by 0 in the same pass)");
   for(const auto& gap:r.world.unbound_enemy_routines())if(gap.find("bhEff085")!=std::string::npos||gap=="bhEff85")throw std::runtime_error("RM_9370: "+gap);
   std::cout<<"RM_9370 event 5: 10x 0xc8 -> 10 fireballs 23 type 1 in "<<ticks<<" ticks, peak live 23/24/25 = "<<peak23<<"/"<<peak24<<"/"<<peak25<<"; "<<hits<<"x 0x6d bhCyodanSet, peak particles "<<peak_particles<<".\n";}
  else check(pending&&requests==0&&fireballs==0,"counter-check: without the 0xc8 binding RM_9370 event 5 must spawn no fireball 23 type 1");
 }
 // RM_9370 Ereignis 11 (@330c, laeuft mit dem Raumeintritt): bhEasySESet (0x86, event.c:6117) Typ 7 -> CallNativeEventSe
 // (sdfunc.c:1830) auf Bank 2, u. a. @35fe Sequenz 11 (Programm 11, Tasten 60/61), @37e4/@3898/@38a8 Sequenzen 23/29/28
 // (leere Programmeintraege). event.c:6205 gibt nach PlayGameSe4Event immer 1 zurueck; MODHSYN (.text 0x75c/0x5f90)
 // startet fuer leere Programme und ungedeckte Tasten keine Stimme. Erwartung: alle vier Befehle gebunden, keiner
 // pending, die Sequenzen laufen (seqNN/progNN), keine Lueckenzeile. Zweiter Teil: ein nicht spielbarer Ruf
 // (Testaufstellung: Sequenz 255 liegt hinter der Tabelle) gilt trotzdem als ausgefuehrt und bleibt benannte Audioluecke.
 {
  int hp=160;std::uint32_t head=0;
  const PlayerAsset pose_body=PlayerAsset::load(data);const auto spawn=decode_scene(read_asset_file(data/"rooms/RM_9370.RDT",64*1024*1024)).spawns.at(0);
  const auto commit=[&](RoomWorld& w,const SceneSpawn& a,std::size_t m,std::int64_t f){std::vector<BoneMatrix> nodes;if(w.player_scripted)(void)w.sample_player(pose_body,&nodes);else nodes=pose_body.pose(m,f,a.position,a.yaw);w.commit_player_joints(pose_body.script_nodes(nodes));};
  Room r(data,"RM_9370",[&](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);w.bind_enemy_player(hp,head);commit(w,spawn,42,0);});
  r.world.set_enemy_ai(true);
  r.vm.event_camera=[&](unsigned& index,unsigned& key){index=r.world.event_camera_index;key=r.world.event_camera_key();return r.world.event_camera;};
  struct Call{std::size_t offset;unsigned seq;bool seen=false,bound=false;};std::array<Call,4> calls{{{0x35fe,11},{0x37e4,23},{0x3898,29},{0x38a8,28}}};
  const auto inner=r.vm.binding;r.vm.binding=[&,inner](const EventCommand& c){const bool bound=inner(c);if(c.opcode==0x86&&c.event==11)for(auto& k:calls)if(k.offset==c.offset){k.seen=true;k.bound=bound;}return bound;};
  unsigned ticks=0;for(;ticks<6000&&!std::all_of(calls.begin(),calls.end(),[](const Call& k){return k.seen;});++ticks){r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();commit(r.world,r.actor,r.motion,r.frame);}
  const auto need=[](bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);};
  for(const auto& k:calls){need(k.seen,"RM_9370 event 11: bhEasySESet @"+std::to_string(k.offset)+" not reached");need(k.bound,"RM_9370 event 11: bhEasySESet @"+std::to_string(k.offset)+" left unbound");
   need(std::any_of(r.audio.played.begin(),r.audio.played.end(),[&](const std::string& s){return s.find("/2/seq"+std::to_string(k.seq)+"/prog"+std::to_string(k.seq))!=std::string::npos;}),"RM_9370 event 11: bank 2 sequence "+std::to_string(k.seq)+" not run");}
  check(std::none_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.opcode==0x86;}),"RM_9370: bhEasySESet left pending");
  for(const auto& gap:r.audio.sequence_unavailable)std::cerr<<"RM_9370 sequence gap: "<<gap<<'\n';
  check(r.audio.sequence_unavailable.empty(),"RM_9370 event 11: bank 2 sequences reported unavailable");
  const std::vector<std::uint8_t> unplayable{0x86,0x07,0x00,0x0f,0x0f,0x80,0x80,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0xff,0x02};
  check(r.world.bind(EventCommand{0x86,11,11,0,unplayable}),"bhEasySESet with an unplayable sequence must still count as executed (event.c:6205)");
  check(std::find(r.audio.unavailable.begin(),r.audio.unavailable.end(),"bhEasySESet type 7 slot 0 SeNo 767")!=r.audio.unavailable.end()&&!r.audio.sequence_unavailable.empty(),"unplayable bhEasySESet not reported as an audio gap");
  std::cout<<"RM_9370 event 11: 4x bhEasySESet type 7 (seq 11/23/29/28) bound and run after "<<ticks<<" ticks; unplayable request reported as audio gap, not pending.\n";
 }
 // RM_9360 Ereignis 14 (@0ad6): 0xca @0b9a = event.c:8801 bhEffClearEvt (2 Bytes) -> effect.c:159 bhClearEventEffect,
 // die selektive Freigabe (bhClrEff_YT effect.c:166, bhClrEff_RY effsub3.c:117, bhClrEff_YG effsub2.c:4793), nicht
 // bhClearEffect (effect.c:153, ganzes eff[]). Start ueber den Raumeintritt: ev_flg 458 mitgebracht -> scd0 @0076 setzt
 // Bank 4 Bit 0, scd1 @01f2 startet Ereignisse 10-17 (Ebene 4 Ereignis 14 @020a). Testaufstellung: rund zehn Passes vor
 // 0xca legt der Test am Weltbesitzer eine Explosion (weapon.c:1812: 23 Art 1, zwei 24 mit Hilfssatz aus
 // bhSetExtraEffectWork, daraus 25) und einen bhCyodanSet-Treffer (0x6d-Bytes aus RM_9370 @1634: Partikelwurzeln 302
 // mit exp0/exp1) an. Erwartung direkt um den Befehl: 24/25/302 und ihre Hilfsplaetze frei, 23 und alle Raumeffekte
 // unveraendert. Gegenprobe im selben Lauf: ohne Bindung bleibt 0xca pending und der Pool unveraendert.
 for(const bool bind_ca:{true,false}){
  int hp=160;std::uint32_t head=0;
  Room r(data,"RM_9360",[&](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);w.bind_enemy_player(hp,head);vm.set_flag(1,458,0);});
  r.world.set_enemy_ai(true); // bhFrameCheck @0aea wartet auf rom->enep[0]
  r.vm.event_camera=[&](unsigned& index,unsigned& key){index=r.world.event_camera_index;key=r.world.event_camera_key();return r.world.event_camera;};
  struct Slot{unsigned id,flg;int extra;};
  const auto snapshot=[&]{std::vector<Slot> out;for(unsigned i=0;const auto* e=r.world.effect_state(i);++i)out.push_back({e->id,e->flg,e->extra_slot});return out;};
  std::vector<Slot> before,after;std::size_t particles_before=0,particles_after=0;unsigned calls=0;
  const auto inner=r.vm.binding;r.vm.binding=[&,inner](const EventCommand& c){
   if(c.opcode!=0xca)return inner(c);
   ++calls;before=snapshot();particles_before=r.world.blood_particles();
   const bool bound=bind_ca&&inner(c);after=snapshot();particles_after=r.world.blood_particles();return bound;};
  r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();
  check(r.task(14),"RM_9360 scd1 @020a did not start event 14 with ev_flg 458 carried into the room");
  bool spawned=false,hit_set=false;unsigned ticks=0;
  for(;ticks<3000&&r.task(14)&&!calls;++ticks){
   for(const auto& t:r.vm.task_states()){if(t.event!=14)continue;
    if(t.offset>=0xb4a&&!spawned){spawned=true; // 24/25 brauchen einige Passes fuer ihren Hilfssatz (mode0 0)
     check(r.world.explode_effect({r.actor.position.x,r.actor.position.y+10.0f,r.actor.position.z}),"RM_9360: bhSetExplosionEffect fixture failed");}
    if(t.offset>=0xb92&&!hit_set){hit_set=true; // die Partikel dieses Treffers leben nur wenige Passes
     const std::vector<std::uint8_t> hit{0x6d,0x64,0xb4,0xb4,0xf0,0x04,0x22,0x1a,0x2c,0x01,0x86,0x06,0x06,0x00,0x00,0x00,0xf0,0x00,0x78,0x00,0x00,0x00};
     check(r.world.bind(EventCommand{0x6d,4,14,0,hit}),"RM_9360: bhCyodanSet fixture failed");}}
   r.world.tick_camera(r.scene,r.scene.cameras.at(0),0);r.tick();}
  for(const auto& err:r.vm.errors)std::cerr<<"RM_9360: "<<err<<'\n';
  check(r.vm.errors.empty(),"RM_9360 event 14 blocked");
  check(spawned&&hit_set&&calls==1,"RM_9360 event 14 did not reach 0xca @0b9a after the fixture");
  const bool pending=std::any_of(r.vm.pending_bindings.begin(),r.vm.pending_bindings.end(),[](const EventCommand& c){return c.opcode==0xca;});
  unsigned live23=0,live24=0,live25=0,live302=0,live_other_listed=0,extras=0,kept23=0,cleared=0,extras_freed=0,room_changed=0;std::map<unsigned,unsigned> other_ids;
  for(std::size_t i=0;i<before.size();++i){const auto& b=before[i];const bool was=(b.flg&1u)!=0;const bool now=i<after.size()&&(after[i].flg&1u)!=0;
   if(!was)continue;
   if(b.id==23){++live23;if(now&&after[i].flg==b.flg)++kept23;}
   // effect.c:166-203 / effsub3.c:117-142 / effsub2.c:4793-4823: die vollstaendigen id-Listen der drei Durchlaeufe
   // (RM_9360 hat selbst Raumeffekte 159 in der Liste von bhClrEff_YT).
   static const unsigned clear_ids[]={5,11,113,114,136,159,225,398,22,24,25,27,301,302,250,253,256,257,272,296,297,298,254,260};
   const bool listed=std::find(std::begin(clear_ids),std::end(clear_ids),b.id)!=std::end(clear_ids);
   if(listed&&b.id!=24&&b.id!=25&&b.id!=302){++live_other_listed;++other_ids[b.id];}
   if(b.id==24)++live24;if(b.id==25)++live25;if(b.id==302)++live302;
   if(listed){if(!now&&after[i].flg==0)++cleared;
    if((b.id==24||b.id==25)&&b.extra>=0){++extras;if(std::size_t(b.extra)<after.size()&&after[std::size_t(b.extra)].flg==0)++extras_freed;}}
   else if(b.id!=23&&(i>=after.size()||after[i].flg!=b.flg))++room_changed;}
  std::cout<<"RM_9360 event 14 ("<<(bind_ca?"bound":"counter-check")<<") 0xca after "<<ticks<<" ticks: before live 23/24/25/302 = "<<live23<<"/"<<live24<<"/"<<live25<<"/"<<live302
   <<", other listed "<<live_other_listed<<[&]{std::string o=" (";for(const auto& [id,n]:other_ids)o+=std::to_string(n)+"x"+std::to_string(id)+" ";o.back()=')';return other_ids.empty()?std::string():o;}()<<", extra works "<<extras<<", particles "<<particles_before<<" -> "<<particles_after<<"; cleared "<<cleared<<", extras freed "<<extras_freed<<", 23 kept "<<kept23<<", other changed "<<room_changed<<".\n";
  check(live23>0&&live24>0&&live25>0&&live302>0&&extras>0&&particles_before>0,"RM_9360: fixture did not fill the pool with 23/24/25/302 before 0xca");
  check(room_changed==0,"RM_9360 0xca changed effects outside bhClearEventEffect's id lists");
  if(bind_ca){check(!pending,"0xca bhEffClearEvt left pending");
   check(cleared==live24+live25+live302+live_other_listed&&extras_freed==extras&&particles_after==0,"bhClearEventEffect did not free every listed id (24/25/302, room 159) and their extra works");
   check(kept23==live23,"bhClearEventEffect freed the explosion fireball 23 (not in bhClrEff_YT)");}
  else check(pending&&cleared==0&&extras_freed==0&&particles_after==particles_before,"counter-check: without the 0xca binding the pool must stay unchanged");
 }
 if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory expectation(s) failed; doors and early-Enter timings passed.\n";return 1;}
 std::cout<<"Both original dog events -> padlock -> all three exits, 14 early Enter timings -> hint/lighter/Rodrigo, five mid-scene cinematic skips and the RM_0090 laser event past its original 0xc0 opcode passed. Diagnostic event starts/floor placement; no campaign proof.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
