// obwp[2] (Zopf, lkmtab id 1211) folgt seinem lkwkp, nicht plp->stflg.
// Original: bhSetPlayer/bhResetPlayer binden obwp[2] an plp, lkono 5 (player.c:22-26, 759-783,
// 965-989). bhObjLinkSet (0x32, event.c:2177-2240) setzt lkwkp = ene[enep[v0].wrk_no], lkono und
// lox/loy/loz; bhObjLinkSetPly (0x34, event.c:2501-2560) setzt es zurueck auf plp. bhControlObjItm
// spiegelt stflg 0x1000000 vom lkwkp (objitm.c:310-319) und laeuft nur ohne es (:321); ob_hlg
// nimmt id >= 1210 nur mit pt_flg 0x1 und ohne mdflg 0x1 (:363-371); bhDrawObjItm/bhDrawObject
// zeichnen mit pt_flg 0x4 (:503) und erneut gespiegeltem stflg (:635-656). bhObjClpn liest pp =
// lkwkp: ax/ay/az, objP[0..5].ang, owP[5].mtx (player.c:7219-7241, objitm.c:2180-2342).
// RM_1050 Steve-Ereignis (Diagnosevorbedingung wie event_lifecycle_test: ev_flg 146 gesetzt,
// Ereignisse 12..17 direkt gestartet): 24 00 00 00 setzt plp->stflg 0x1000000 (Claire wird die
// Ereignisfigur enep[0], Identitaet 91), 32 00 02 05 .. 9e 00 4d 00 haengt obwp[2] an sie,
// am Ende 34 00 02 05 .. 9e 00 4d 00 zurueck an plp. Vorher fehlte der Zopf waehrend des Ereignisses.
#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include "room.h"
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
namespace {
struct Room {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
 Inventory inventory;StatusMenu menu;MessageDialog dialog;AudioMixer audio;MoviePlayer movie;PlayerAsset body;RoomWorld world;EventVm vm;GameplayInput input;
 Room(const std::filesystem::path& data,const char* name,const std::function<void(EventVm&,RoomWorld&)>& pre):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data,{},name),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm){
  bind_item_names(read_asset_file(data/"files/SYSMES.ALD",4*1024*1024));
  world.set_player_asset(&body);{const auto id=RoomIdentity::parse(std::string(name)+".RDX");vm.values[0]=id.stage;vm.values[1]=id.room;}
  vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
  vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};vm.enemy_joint_position=[&](unsigned n,unsigned node){return world.enemy_joint_position(n,node);};vm.set_item_position=[&](unsigned n,NJS_POINT3 p){world.set_item_position(n,p);};
  vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
  input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
  audio.mount_sound_afs(vm.values[0]);pre(vm,world);audio.begin_room_init();vm.initialize();audio.load_room_sounds(vm.values[0],vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
 }
 void tick(){input.apply({});vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.tick();input.synchronize();world.set_simulation_flags(vm.flags[6][0]);world.tick();vm.flags[4][0]|=world.take_enemy_room_flags();movie.tick();audio.mix();check(vm.errors.empty(),"original event stream blocked");}
 bool task(unsigned event){for(const auto& t:vm.task_states())if(t.event==event)return true;return false;}
};
bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
}
int main(int argc,char** argv){try{
 if(argc<2)return 0;_Make_SinTable();const std::filesystem::path data=argv[1];
 const NJS_POINT3 bound{0,1.5869f,.7747f}; // player.c:763-765 (ply_id 0)
 Room r(data,"RM_1050",[](EventVm& vm,RoomWorld& w){w.bind_player_collision(vm);vm.set_flag(1,0x92,0);});
 for(unsigned t=0;t<3;++t)r.tick();
 {const auto a=r.world.attached_object(2,bound);
  check(a.linked&&a.kind==0&&a.node==5&&near(a.offset.y,bound.y)&&near(a.offset.z,bound.z),"obwp[2] before the event is not the bhSetPlayer link (plp, lkono 5, lox/loy/loz of player.c:763-765)");
  check(!a.sleeping&&a.control&&a.listed&&a.draw,"obwp[2] on the controlled player is not run/listed/drawn");}
 for(unsigned e=12;e<=17;++e)r.vm.start(e-11,e);
 for(unsigned t=0;t<10;++t)r.tick();
 check((r.vm.flags[10][0]&4u)&&(r.vm.flags[14][0]&0x1000000u),"precondition: Steve cinematic with plp->stflg 0x1000000 (bhInitModelSet 24 00 00 00)");
 {const auto a=r.world.attached_object(2,bound);
  check(a.linked&&a.kind==1&&a.index==0&&a.node==5,"bhObjLinkSet (event.c:2193-2201) did not move obwp[2].lkwkp to ene[enep[0].wrk_no], lkono 5");
  check(near(a.offset.x,0)&&near(a.offset.y,1.58f)&&near(a.offset.z,.77f),"bhObjLinkSet lox/loy/loz differ from the script (u16/100, event.c:2218-2240)");
  check(!a.sleeping&&a.control&&a.listed&&a.draw,"obwp[2] on the event Claire is hidden by plp->stflg; objitm.c:310-319 mirrors stflg from lkwkp (the active enemy work)");
  const auto w=r.world.enemy_link_work(0);check(bool(w),"bhObjClpn pp = enep[0] work has no native object tree");
  const auto j=r.world.enemy_joint_position(0,5);check(j&&near(w->head.value[12],j->x)&&near(w->head.value[13],j->y)&&near(w->head.value[14],j->z),"bhObjClpn owP[5] of pp is not the enemy's committed owP[5]");}
 // bhInitModelSet am lkwkp (event.c:2802-2860, Fall 1: stflg 0x1000000) haelt Lauf und Zeichnung an (objitm.c:321/654).
 check(r.world.bind({0x24,0,0,0,{0x24,1,0,0}}),"bhInitModelSet enemy unbound");
 {const auto a=r.world.attached_object(2,bound);check(a.sleeping&&!a.control&&!a.draw,"obwp[2] ignores stflg 0x1000000 of its lkwkp");}
 check(r.world.bind({0x24,0,0,0,{0x24,1,0,1}}),"bhInitModelSet enemy unbound");
 // bhSetDispObj auf obwp[2] (event.c:2318-2325: mdflg 0x1) nimmt es aus ob_hlg (objitm.c:363).
 check(r.world.bind({0x20,0,0,0,{0x20,2,2,0}}),"bhSetDispObj object unbound");
 {const auto a=r.world.attached_object(2,bound);check(!a.listed&&a.control,"bhSetDispObj mdflg 0x1 does not remove obwp[2] from ob_hlg");}
 check(r.world.bind({0x20,0,0,0,{0x20,2,2,1}}),"bhSetDispObj object unbound");
 unsigned ticks=0;while(r.task(12)&&ticks++<3000)r.tick();
 check(!r.task(12)&&!(r.vm.flags[14][0]&0x1000000u),"RM_1050 event 12 did not end with the player shown again");
 {const auto a=r.world.attached_object(2,bound);
  check(a.linked&&a.kind==0&&a.node==5&&near(a.offset.y,1.58f)&&near(a.offset.z,.77f),"bhObjLinkSetPly (event.c:2501-2560) did not return obwp[2] to plp with the script offsets");
  check(!a.sleeping&&a.control&&a.listed&&a.draw,"obwp[2] back on the player is not run/listed/drawn");}
 std::cout<<"RM_1050 Steve: obwp[2] follows bhObjLinkSet to enep[0] and bhObjLinkSetPly back; stflg mirrors the lkwkp.\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}return 0;}
