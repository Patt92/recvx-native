// effsub1.c:6660 bhEff110 / effsub1b.c:17 bhEff133, mit echten unveraenderten Raumdaten.
// Explizite Eventstarts und isolierte sp_flg sind Diagnosevorbedingungen, kein Durchspielnachweis.
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "core_random.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct RoomScreen {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
 AudioMixer audio;Inventory inv;MessageDialog dialog;RoomWorld world;EventVm vm;std::vector<EventCommand> commands;
 RoomScreen(const std::filesystem::path& data,const std::string& name):bytes(RoomArchive(data).load(name+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns[0]),audio(data),world(bytes,scene,actor,motion,frame,audio,data,{},name),vm(bytes,inv,dialog){
  const auto id=RoomIdentity::parse(name);vm.values[0]=id.stage;vm.values[1]=id.room;world.bind_player_collision(vm);
  vm.binding=[&](const EventCommand& c){commands.push_back(c);return world.bind(c);};vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};
 }
 void pass(unsigned ps=0){world.set_pad(ps,ps);world.set_simulation_flags(vm.flags[6][0]);world.tick();}
 void mode(unsigned slot,unsigned mode){EventCommand c;c.opcode=0x91;c.bytes={0x91,std::uint8_t(slot),std::uint8_t(mode),0};check(world.bind(c),"0x91: effect binding missing");}
};
static Scene test_scene(){Scene out;out.background=0xff404040u;SceneTriangle t;t.world_lit=true;t.camera_mask=false;t.color=0xff4080c0u;
 t.vertices={SceneVertex{{-4,-2,10},0,0,{}},SceneVertex{{4,-2,10},0,0,{}},SceneVertex{{0,3,10},0,0,{}}};out.triangles.push_back(t);return out;}
static std::size_t differences(const SoftwareRenderer& a,const SoftwareRenderer& b){std::size_t n=0;for(std::size_t i=0;i<a.pixels().size();++i)n+=a.pixels()[i]!=b.pixels()[i];return n;}
int main(int argc,char** argv){try{_Make_SinTable();if(argc<2)return 0;const std::filesystem::path data=argv[1];const std::filesystem::path output=argc>2?argv[2]:std::filesystem::path{};
 check(supported_effect_sprite(110)&&supported_effect_sprite(133)&&ported_effect_atlas(42),"110/133/atlas42 registration missing");
 // RM_1050 slot0: EF_WORK type0, mdlver0, scale4. Keine Zufallsaufrufe in beiden Prozeduren.
 {RoomScreen r(data,"RM_1050");r.vm.flags[6][0]=8;const auto* s=r.world.effect_state(0);check(s&&s->id==110&&s->type==0&&s->mdlver==0&&s->sx==4,"RM_1050 original effect0 operands");
  recvx_core_random_seed(0x1234);const int first=recvx_core_random_next();recvx_core_random_seed(0x1234);
  r.vm.flags[11][0]=1;r.pass();check(s->mode0==0&&(s->flg&0x1000000u),"110 mode0 held by gm_flg1");
  r.vm.flags[11][0]=0;r.pass();check(s->mode0==1&&s->aspd==0,"110 grid initialization");
  r.mode(0,1);r.world.set_effect_flags(1);r.pass();check(s->mode1==1&&s->mode0==1,"110 ef_flg1 must retain mode request");r.world.set_effect_flags(0);
  r.pass();check(s->mode1==0&&s->mode0==11&&s->spd==0&&(r.vm.flags[11][0]&0x100u),"110 mode10 arms render and spd0");
  const Scene sc=test_scene();SceneCamera camera;camera.perspective=8192;
  SoftwareRenderer start(640,480),ramp(640,480);start.set_supersampling(1);ramp.set_supersampling(1);
  check(r.world.draw_full_screen(start,sc,camera),"110 fullscreen draw not connected");
  // bcol -1 is NOT halved in njDrawTextureH, MODULATE /128 then fixed F128: grey64 ->63.
  check(start.pixels()[20*640+20]==0xff3f3f3fu,"110 njDrawTextureH bcol/F128 differs from original");
  for(int i=0;i<80;++i)r.pass();check(s->mode0==11&&s->spd==20&&s->ct0==20480,"110 80 ramps of 0.25, not 79 or 81");
  r.pass();check(s->mode0==13&&s->spd==20&&s->ct0==20736,"110 stable mode13 after ramp boundary");
  check(r.world.draw_full_screen(ramp,sc,camera)&&differences(start,ramp)>1000,"110 grid deformation/color no visible effect");
  // ocol = ushort(20)<<17 = 0x280000: roter (nicht blauer) Nebelanteil 40*127>>8=19.
  check(ramp.pixels()[20*640+20]==0xff533f3fu,"110 ocol 0x280000 must tint ARGB red, never swap R/B");
  if(!output.empty()){start.save_bmp(output/"110-start.bmp");ramp.save_bmp(output/"110-ramp.bmp");}
  r.mode(0,2);r.pass();check(s->mode0==12&&std::fabs(s->spd-19.8f)<0.00001f,"110 mode12 decrement0.2");
  for(int i=0;i<110&&s->mode0==12;++i)r.pass();check(s->mode0==13&&s->spd==0&&s->ct3==4,"110 fadeout overshoot clamps0 and ct3=4, remains rendering");
  r.mode(0,3);r.pass();check(s->mode0==1&&!(r.vm.flags[11][0]&0x100u),"110 mode1=3 clears render flag");check(!r.world.draw_full_screen(ramp,sc,camera),"110 render after release");
  r.mode(0,4);r.pass();check(s->flg==0,"110 mode1=4 releases effect slot");
  check(recvx_core_random_next()==first,"110 must not advance random stream");
  std::cout<<"110 type0: init gate, ef_flg, 80 ramps, modes11/12/13, release, fixed fog RGB63, grid image PASS\n";
 }
 // Echter RM_1050 Erzeuger: Ereignis3 @0xe42 setzt mode1=1 nach seiner Skriptwartezeit.
 {RoomScreen r(data,"RM_1050");r.vm.initialize();r.vm.start(0,3);bool reached=false;
  for(int i=0;i<500&&!reached;++i){if(r.dialog.active()&&(r.dialog.turnable()||r.dialog.closable()))r.vm.confirm_dialog();r.vm.flags[6][0]|=0x10u;r.vm.tick();r.pass();
   reached=std::any_of(r.commands.begin(),r.commands.end(),[](const EventCommand& c){return c.opcode==0x91&&c.event==3&&c.offset==0xe42&&c.bytes[1]==0&&c.bytes[2]==1;});}
  check(reached&&r.vm.errors.empty()&&(r.vm.flags[11][0]&0x100u),"RM_1050 event3 @e42 did not activate bhEff110");
  std::cout<<"RM_1050 110: original event3 @e42 reaches rendering after its original wait PASS\n";
 }
 // RM_4030 / RM_8050: type1 sx0/sy19; cam.flg20 comes from CAM_WRK flag400, not a room-id exception.
 for(const char* name:{"RM_4030","RM_8050"}){RoomScreen r(data,name);r.vm.flags[6][0]=8;const auto* s=r.world.effect_state(0);check(s&&s->id==110&&s->type==1&&s->sx==0&&s->sy==19,"type1 original room operands");
  unsigned enabled=~0u,disabled=~0u;for(unsigned i=0;i<r.scene.cameras.size();++i){if(r.scene.cameras[i].flags&0x400u)enabled=i;else disabled=i;}check(enabled!=~0u&&disabled!=~0u,"type1 camera flags fixture");
  r.world.set_fixed_cut(enabled);r.pass();r.pass();check(s->mode0==14&&(r.vm.flags[11][0]&0x100u),"110 type1 cut flag400 fails mode14");r.pass();check((r.vm.flags[11][0]&0x100100u)==0x100100u&&s->ct0==256,"110 type1 first deformation tick");
  r.world.set_fixed_cut(disabled);r.pass();check(s->mode0==1&&!(r.vm.flags[11][0]&0x100100u),"110 leaving cut must clear both 0x100100 bits");
  r.world.set_fixed_cut(enabled);r.pass();check(s->mode0==14,"110 type1 re-entry");
  Scene image_scene=r.scene;r.world.append(image_scene,{},r.world.camera(r.scene,r.scene.cameras[enabled]));SoftwareRenderer image(640,480);image.set_supersampling(1);check(r.world.draw_full_screen(image,image_scene,r.world.camera(r.scene,r.scene.cameras[enabled])),"type1 room render not connected");if(!output.empty())image.save_bmp(output/(std::string(name)+"-110.bmp"));
  std::cout<<name<<" 110 type1: cut "<<enabled<<" enables, cut "<<disabled<<" releases, re-entry and original room image PASS\n";
 }
 // RM_7050 event17 @0xac0 is the original 0x91 02 01 00; drive that command through EventVm.
 {RoomScreen r(data,"RM_7050");r.vm.initialize();r.vm.start(0,17);r.vm.flags[6][0]|=0x10u; // diagnostic isolated scheduler, as bhMainSequence's caller
  r.vm.tick();check(r.vm.errors.empty(),"RM_7050 event17 blocked");
  check(std::any_of(r.commands.begin(),r.commands.end(),[](const EventCommand& c){return c.opcode==0x91&&c.event==17&&c.offset==0xac0&&c.bytes[1]==2&&c.bytes[2]==1;}),"133 original script activation absent");
  r.vm.flags[6][0]=0x3cu; // sp_flg 0x20 Eingabe, 0x10 Scheduler, 0x8 Effekte, 0x4 Objekte; Spieler/Gegner isoliert
  const unsigned previous_sp=r.vm.flags[6][0];r.pass();const auto* s=r.world.effect_state(2);check(s&&s->id==133&&s->mode0==1&&(r.vm.flags[5][0]&0x40000000u)&&r.vm.flags[6][0]==(previous_sp&~3u),"133 enters, saves sp and clears bits0/1");
  r.mode(2,0);r.pass();check(s->mode0==1&&(r.vm.flags[5][0]&0x40000000u),"133 active st_flg must retain operation with mode1=0");
  SoftwareRenderer bg(640,480),panel(640,480);std::fill(bg.canvas().begin(),bg.canvas().end(),0xff202020u);panel.canvas()=bg.pixels();r.world.draw_thermometer(panel);check(differences(bg,panel)>15000,"133 no real atlas42 image");if(!output.empty())panel.save_bmp(output/"RM_7050-133-start.bmp");
  const auto press=[&](unsigned bits){r.pass(bits);};
  press(2); // lower limit digit0=1, no movement
  for(int i=0;i<6;++i)press(1);for(int i=0;i<4;++i)press(2); // upper limit5, then back1
  press(8);for(int i=0;i<11;++i)press(1);for(int i=0;i<7;++i)press(2); // upper limit9 ->2
  press(8);for(int i=0;i<10;++i)press(1);press(2); // upper limit9 ->8, password128
  panel.canvas()=bg.pixels();r.world.draw_thermometer(panel);if(!output.empty())panel.save_bmp(output/"RM_7050-133-128.bmp");
  // Cancel is not a handler in the original; ONLY OK at cursor3 confirms.
  press(0x1000);check(s->mode0==1,"133 invented Cancel exit");press(0x800);check(s->mode0==1,"133 OK on digit must not confirm");
  press(8);press(0x800);check(s->mode0==2&&!r.vm.flag(1,58),"133 confirmation must defer flag until following pass");
  press(0);check(r.vm.flag(1,58)&&s->mode0==0&&s->mode1==0&&s->mode2==0&&s->mode3==0&&!(r.vm.flags[5][0]&0x40000000u)&&r.vm.flags[6][0]==previous_sp,"133 128 completion flag58/sp restore/mode reset");
  r.mode(2,1);r.pass();press(4);press(2);press(8);press(0x800);press(0);check(r.vm.flag(1,58),"133 wrong retry must NOT clear an existing solved flag");
  r.vm.set_flag(1,58,1);r.mode(2,1);r.pass();press(0x800);press(0);check(!r.vm.flag(1,58),"133 wrong retry must not set flag58");
  for(const auto& gap:r.world.unbound_enemy_routines())check(gap.find("Thermo")==std::string::npos&&gap!="bhEff133"&&gap!="bhEff133: sys->st_flg/sp_flg/ev_flg ohne EventVm","133 unresolved production service");
  std::cout<<"RM_7050 133: original event17 @ac0, atlas42 image, digits128, deferred flag58, cancel ignored, repeat and saved sp PASS\n";
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
