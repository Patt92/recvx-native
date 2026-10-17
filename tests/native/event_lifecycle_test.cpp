#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "renderer.h"
#include "asset_io.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <algorithm>
#include <functional>
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
 // `pre` runs before bhInitEvent (scd0), like flags carried into the room from elsewhere.
 Room(const std::filesystem::path& data,const char* name,const std::function<void(EventVm&,RoomWorld&)>& pre={}):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data,{},name),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm){
  bind_item_names(read_asset_file(data/"files/SYSMES.ALD",4*1024*1024)); // sys->mes_ip wie GameUi in der Runtime
  world.set_player_asset(&body);vm.values[1]=std::string(name)=="RM_0050"?5:0;
  vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
  vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};vm.enemy_joint_position=[&](unsigned n,unsigned node){return world.enemy_joint_position(n,node);};vm.set_item_position=[&](unsigned n,NJS_POINT3 p){world.set_item_position(n,p);};vm.enemy_disable=[&](unsigned n){world.deactivate_enemy(n);};
  vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
  input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
  if(std::string(name)=="RM_0050")inventory.acquire(84,1); // Diagnostic precondition: the player brings the padlock key into the room.
  if(pre)pre(vm,world);audio.begin_room_init();vm.initialize();audio.load_room_sounds(0,vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
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
  const auto* m=r.world.effect_state(1);check(m&&m->id==111&&m->mode0==2&&m->tex_id==111&&m->lkono==0&&(m->flg&0x4020000u)==0x4020000u,"bhEff111 state differs (lkono must be 0 without lkflg, room.c:781)");
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
  // screen.c:928-939: ohne initflag rendert der Monitor nur unter st_flg 0x1 oder gm_flg 0x20 (Schnittwechsel,
  // cut.c:298/455/1651/1731; bhCheckCut cut.c:22 loescht es je Bild). Diagnosevorbedingung: st_flg 0x1 aus.
  r.world.set_fixed_cut(5);check(r.vm.flags[11][0]&0x20u,"bhSetFixedCut did not raise gm_flg 0x20");r.vm.flags[5][0]&=~1u;
  check(bool(r.world.small_screen_pass(view)),"a cut change (gm_flg 0x20) does not refresh the monitor");
  r.world.commit_small_screen(target.pixels(),512,512);r.world.begin_cut_check();
  check(!(r.vm.flags[11][0]&0x20u)&&!r.world.small_screen_pass(view),"bhCheckCut does not clear gm_flg 0x20; the monitor re-renders without initflag, st_flg 1 or a cut change");
  std::cout<<"RM_1050 monitor: bhEff111 live, cut 6 rendered into the 512x512 texture and shown on the quad.\n";}
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
