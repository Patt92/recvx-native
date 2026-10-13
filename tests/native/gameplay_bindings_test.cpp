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
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
// Weich gemeldete Inventarwirkungen (D07/D06), gesammelt bis zum Ende.
static std::vector<std::string> soft_failures;
static EventCommand cmd(std::initializer_list<std::uint8_t> b){EventCommand c;c.bytes=b;c.opcode=c.bytes[0];return c;}
int main(int argc,char** argv){try{
 if(argc<2)return 0;_Make_SinTable();std::filesystem::path data=argv[1];AudioMixer audio(data);
 {auto bytes=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;RoomWorld world(bytes,scene,player,motion,frame,audio,data);auto asset=PlayerAsset::load(data);world.set_player_asset(&asset);Inventory inv;inv.acquire(84,1);MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.binding=[&](const EventCommand& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.initialize();vm.tick();
 // D07 ItemUse am originalen Bodentrigger 27 (Schlüssel 84): Use_05 schließt,
 // das Raumskript fragt, Ja wirft den Schlüssel weg (Skriptbefehl) und gibt die
 // Steuerung frei. Die Verbrauchsprüfung ist weich, damit die Hunde-/Datei-/
 // Zombieabschnitte dahinter geprüft bleiben.
 vm.flags[10][0]|=0x200;vm.values[16]=27;check(vm.use(84),"Padlock ItemUse refused original floor");bool question=false;
 for(unsigned tick=0;tick<400;++tick){vm.tick();world.tick();if(dialog.active()){if(dialog.choices()){question=true;dialog.selection=0;}if(tick%5==0)vm.confirm_dialog();}}
 check(question,"Padlock discard question missing");
 if(inv.contains(84)){std::string o;for(const auto& c:vm.pending_bindings){char b[8];std::snprintf(b,sizeof b,"%02x ",c.opcode);o+=b;}soft_failures.push_back("D07/D06 Padlock Yes did not discard the key (pending opcodes: "+o+")");}
 check(!dialog.active()&&!(vm.flags[5][0]&4)&&!(vm.flags[10][0]&4),"Padlock Yes retained cinematic/dialog lock");check(vm.errors.empty(),"Padlock script blocked");std::cout<<"Padlock use / original discard question / Yes / control release checked.\n";
 // PORTIERUNGSLÜCKE (Policy 2026-10-09): Hier wurde der Pistolenschuss auf
 // jeden RM_0050-Hund geprüft: der originale Zielmodus (bhCPM2_act_rdy mode 67
 // / phase 1), Schuss und Treffer, die en04-Schadensverteilung, das Blut und
 // die Todesmotion. Spielerkampf und Gegnerkern kamen aus player_combat.inc
 // bzw. en01_damage.inc/en04_damage.inc, die Partikel aus
 // effect_particles.inc; alle Generatoren sind Quarantäne, siehe
 // native/player_combat_core.c, native/en01_damage.c und
 // native/effect_particles.cpp.
 //
 // Geprüft bleibt der weiterhin eigenständige Host-Anteil: die Waffenauswahl,
 // die Trefferstrecke und die Wandabfrage gegen echte Raumgeometrie und die
 // Eventbindung 0x24. Dazu kommt, dass keine der drei Lücken etwas erfindet.
 for(unsigned i=0;i<16;++i)if(const auto* e=world.state(1,i);e&&e->identity==4){
   world.bind(cmd({0x24,1,std::uint8_t(i),1}));const auto at=e->transform.position;
   Inventory armed;armed.acquire(5,12);unsigned slot=1;while(slot<12&&Inventory::id(armed.slots()[slot])!=5)++slot;check(armed.equip(slot),"dog pistol fixture cannot equip");
   bool shooting_position=false;const auto& weapon=WpnTab[armed.weapon()];
   for(unsigned yaw=0;yaw<65536&&!shooting_position;yaw+=4096)for(float distance=9;distance<25&&!shooting_position;distance+=1){const float r=yaw*6.28318530718f/65536;const NJS_POINT3 forward{-std::sin(r),0,-std::cos(r)};SceneSpawn candidate{{at.x-forward.x*distance,at.y,at.z-forward.z*distance},int(yaw)};const NJS_POINT3 chest{candidate.position.x,at.y+11,candidate.position.z},origin{chest.x+forward.x*(3+weapon.r),chest.y,chest.z+forward.z*(3+weapon.r)},direction{forward.x*.70710678f,-.70710678f,forward.z*.70710678f};const float hit=CombatController::target_distance(origin,direction,weapon.l,at,3+weapon.r,4);if(world.combat_wall_distance(chest,forward,5)>=5&&hit<world.combat_wall_distance(origin,direction,weapon.l)){player=candidate;shooting_position=true;}}
   check(shooting_position,"no unobstructed dog shot fixture");CombatController gun(data);
   // Spielerkampf ist portiert (native/player_combat_core.c, A01-A04): mit
   // gehaltener Zieltaste, gehaltenem Abzug und pad 0x40 laeuft bhCPM0_action
   // -> bhCPM2_act_suw -> bhCPM2_act_atk und endet in bhCPM2_act_wpn mit
   // mode2 67 (Tiefzielen). Der Abzug ist halbautomatisch: at_flg 0x40 bleibt
   // verriegelt, solange pad 0x100 gehalten wird (player.c:4940), also genau
   // ein Schuss und genau eine Patrone. Treffer bleiben 0, solange world die
   // Gegnertabelle (RECVX_ENEMY_TABLE) nicht exponiert.
   for(unsigned t=0;t<240;++t)gun.tick({true,true,0,-1},false,armed,asset,player,motion,frame,&world,audio);
   check(gun.original_mode()==67&&gun.original_phase()==1,"combat did not reach the original aim-down hold");
   check(gun.shots==1&&gun.attacks==1,"the semi-automatic trigger did not fire exactly once while held");
   check(Inventory::quantity(armed.slots()[slot])==11,"bhCountBullet did not consume one round");
   check(!gun.hits,"no hit may be booked before world exposes the enemy table");
   // Lücke Gegnerkern/Partikel: der Hund ist nicht verwundbar, behält seine
   // Original-HP, blutet nicht und stirbt nicht.
   std::cout<<"Dog i="<<i<<" hp_initial="<<world.state(1,i)->hp<<std::endl;
   world.tick();
   const auto hp=world.state(1,i)->hp;
   std::cout<<"Dog i="<<i<<" hp_before="<<hp<<std::endl;
   check(!world.damage(i,3,500,0x10,{at.x,at.y+2,at.z},{0,0,-1}),"enemy gap accepted damage");
   world.tick();
   std::cout<<"Dog i="<<i<<" hp_after="<<world.state(1,i)->hp<<std::endl;
   check(world.state(1,i)->hp==hp,"enemy gap changed the original HP");
   check(!world.blood_particles(),"particle gap produced blood");
   for(unsigned t=0;t<200;++t)world.tick();check(!world.enemy_dead(i),"enemy gap ran a death motion");
 }
 const unsigned render_room_index=0;
 EventCommand render_off=cmd({0xa5,std::uint8_t(render_room_index),0,0});
 EventCommand render_on=cmd({0xa5,std::uint8_t(render_room_index),1,0});
 check(world.bind(render_off)&&world.state(1,render_room_index)->hidden,"bhEneRenderSet hide missed native draw gate");
 check(world.bind(render_on)&&!world.state(1,render_room_index)->hidden,"bhEneRenderSet show missed native draw gate");
 std::cout<<"Original bhEneRenderSet EventVm binding toggles the shared native draw gate.\n";
 std::cout<<"RM_0050 dogs: weapon selection, hit distance and wall query against real room data passed; aiming, firing, damage, blood and death are documented gaps.\n";
 }
 {auto bytes=read_asset_file(data/"rooms/RM_6040.RDT",64*1024*1024);Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.values[0]=6;vm.values[1]=4;vm.item_message=[](unsigned,unsigned){return Message{0xffff};};const auto a=vm.interactions.at(13);NJS_POINT3 pos{a.position.x+a.extent.x*.5f,a.position.y,a.position.z+a.extent.z*.5f+2};check(vm.interact(pos,0,a.floor),"original Memo file interaction failed");
// D03: FileNumberSwitch ordnet Raum 604 mit Item 132 die Datei 17 zu.
check(!inv.contains(132)&&vm.acquired_file()==17&&(inv.items[383]&(1u<<17)),"Memo entered inventory instead of original file 17");
check(vm.flags[10][0]&0x800,"file pickup did not acknowledge room event");
#ifdef RECVX_TEST_INPUT
StatusMenu menu;GameplayInput controls(inv,menu,dialog,&vm);controls.synchronize();menu.tick(inv);menu.skip_slide();menu.input_unslid(MenuInput::Cancel,inv);check(!menu.file_acquisition,"acquired file cannot close");for(unsigned tick=0;tick<120;++tick){menu.tick(inv);controls.synchronize();}check(!menu.active,"file acquisition reopened after close");
#else
vm.finish_file();
#endif
check(!(vm.flags[10][0]&0x20000),"file request retained after close");std::cout<<"Original RM_6040 Memo: file 17, no inventory slot, room acknowledgement and close passed.\n";}
#ifdef RECVX_TEST_INPUT
 // RM_0070 scd1 requests both cb_flg 0x10 and 0x20000 for Director's Memo.
 // Closing GetFile must consume both, as sub1.c ItemTaskCheck does; otherwise
 // the remaining 0x10 starts GetItem and inserts the same memo into pip.
 {auto bytes=read_asset_file(data/"rooms/RM_0070.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld world(bytes,scene,player,m,f,audio,data);auto body=PlayerAsset::load(data);world.set_player_asset(&body);Inventory inv;const auto slots_before=inv.items;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.values[0]=0;vm.values[1]=7;vm.binding=[&](const auto& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.item_message=[](unsigned,unsigned){return Message{0xffff};};StatusMenu menu;GameplayInput controls(inv,menu,dialog,&vm);vm.initialize();
 check(vm.interact({25,0,41},49152,0),"Director Memo original scripted interaction failed");
 for(unsigned t=0;t<80&&!menu.file_acquisition;++t){vm.tick();world.tick();controls.synchronize();menu.tick(inv);}
 // D03: Raum 7 mit Item 48 ist Datei 8 (FileNumberSwitch).
 check(menu.file_acquisition&&menu.file_number==8&&(inv.items[383]&(1u<<8)),"Director Memo did not open original file 8");
 check((vm.flags[10][0]&0x20010)==0x20010,"fixture did not exercise simultaneous file/item request");
 menu.skip_slide();menu.input_unslid(MenuInput::Cancel,inv);
 for(unsigned t=0;t<160;++t){menu.tick(inv);controls.synchronize();if(vm.acquired_file()==~0u)check(!(vm.flags[10][0]&0x20010),"file close retained an acquisition request");check(!vm.pickup_display(),"file close started a second GetItem acquisition");vm.tick();world.tick();}
 check(!menu.active&&!dialog.active()&&!inv.contains(48),"closing Director Memo inserted an inventory item or reopened a modal");check(!(vm.flags[10][0]&0x20010),"file close retained original acquisition request flags");check(vm.flag(7,97),"Director Memo original room pickup acknowledgement missing");for(unsigned i=0;i<383;++i)check(inv.items[i]==slots_before[i],"Director Memo changed an inventory slot");check(vm.errors.empty(),"Director Memo original room script blocked");std::cout<<"Original RM_0070 scripted Director Memo: file 8, close consumes both requests, no inventory slot.\n";}
#endif
 {auto bytes=read_asset_file(data/"rooms/RM_0090.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld world(bytes,scene,player,m,f,audio,data);auto body=PlayerAsset::load(data);world.set_player_asset(&body);Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.binding=[&](const auto& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.start(0,22);for(unsigned t=0;t<35;++t){vm.tick();world.tick();}check(world.state(2,8)->has_motion&&world.state(2,8)->motion==6,"original milling event did not bind room MTN 6");BinaryView v(bytes);const auto h=inspect_room(bytes).room_offset;for(unsigned i=4;i<v.u32(h+0x8c);++i)if(i!=8)world.bind(cmd({0x24,2,std::uint8_t(i),0}));
 Scene first=scene;world.append(first,{},scene.cameras[0]);for(unsigned t=0;t<40;++t){vm.tick();world.tick();}check(vm.errors.empty(),"original milling event blocked");Scene second=scene;world.append(second,{},scene.cameras[0]);for(auto* sample:{&first,&second})sample->triangles.erase(std::remove_if(sample->triangles.begin(),sample->triangles.end(),[](const auto& t){return t.light_category!=2;}),sample->triangles.end());check(!first.triangles.empty(),"milling machine not rendered");check(first.triangles.size()==second.triangles.size(),"milling geometry changed topology");bool moved=false;for(unsigned i=0;i<first.triangles.size();++i)for(unsigned j=0;j<3;++j){const auto a=first.triangles[i].vertices[j].position,b=second.triangles[i].vertices[j].position;if(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.01f)moved=true;}check(moved,"original milling motion does not change rendered vertices");std::cout<<"RM_0090 object 8 / original room MTN 6 changes rendered machine geometry.\n";}
 {auto bytes=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld world(bytes,scene,player,m,f,audio,data);auto body=PlayerAsset::load(data);world.set_player_asset(&body);world.set_enemy_ai(true);const unsigned index=3;Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.binding=[&](const auto& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.start(0,40);for(unsigned t=0;t<300;++t){vm.tick();world.tick();}check(vm.errors.empty(),"original RM_0080 event 40 blocked");check(!world.state(1,index)->scripted&&!world.state(1,index)->frozen,"Sub_controll did not release zombie");const auto at=world.state(1,index)->transform.position;player.position={at.x+40,at.y,at.z};for(unsigned t=0;t<120;++t)world.tick();const auto next=world.state(1,index)->transform.position;
 // PORTIERUNGSLÜCKE (Policy 2026-10-09): Verfolgungs-KI, Trefferannahme und
 // Blut kamen aus en01_damage.inc bzw. effect_particles.inc; die Generatoren
 // sind Quarantäne, siehe native/en01_damage.c und
 // native/effect_particles.cpp. Geprüft bleibt die Skriptfreigabe über
 // Sub_controll (Zusicherung oben) und dass der freigegebene Gegner ohne die
 // Originalroutinen inert bleibt.
 // 2026-10-13: Sub_controll 0x8f (event.c:9804-9844) ist jetzt an den nativen
 // Work gebunden: mode0 1 / mode1..3 0, mnwP = emtp[id], ct2-Byte 0; davor
 // Common 23 (1) und 46 (1) (event.c:11971/10358) setzen flg 0x118 und 0x20.
 // Der freigegebene Work laeuft damit in bhEne01_Move (en01.c:2207) und
 // naehert sich dem Spieler (bhEne01_Brain); die alte "inert"-Zusicherung
 // beschrieb die nicht gebundene Freigabe, nicht das Original.
 {const auto& w=world.state(1,index)->enemy;
  check(w.mode0==1&&w.mode1==0&&w.mode2==0&&w.mode3==0,"Sub_controll 0x8f did not set the original modes on the native work");
  check(w.mtn_room==0,"Sub_controll 0x8f did not select mnwP = emtp[id]");
  check((w.flg&0x118u)==0x118u&&(w.flg&0x20u),"Common 23/46 did not restore flg 0x118/0x20 before the release");}
 check(std::hypot(next.x-at.x,next.z-at.z)>=.01f,"released zombie stayed inert under the ported bhEne01_Move");
 std::cout<<"Original zombie script release passed: Sub_controll 0x8f modes, emtp library and flg 0x118/0x20 on the native work; the released zombie moves under bhEne01_Move.\n";}
 // event.c:4384/4446 bhEffBloodPoolSet/bhEffBloodPoolSet2: die beiden
 // Skriptbefehle der Blutlache. v1 ist djnt_no und Tabellenauswahl zugleich
 // (0/1 evnt_BldTbl2, 2 evnt_BldTbl), die Richtung ay + objP[1].ang[1] bzw.
 // der eigene Winkeloperand, der Ursprung owP[0] bzw. die eigene Lage.
 {auto bytes=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld world(bytes,scene,player,m,f,audio,data);auto body=PlayerAsset::load(data);world.set_player_asset(&body);world.set_enemy_ai(true);const unsigned index=3;Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.binding=[&](const auto& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};vm.start(0,40);for(unsigned t=0;t<300;++t){vm.tick();world.tick();}
  check(world.state(1,index)&&world.state(1,index)->enemy_bound&&world.state(1,index)->enemy_calculated,"blood pool premise: RM_0080 zombie work not bound/calculated");
  const auto before=world.blood_particles();
  check(world.bind(cmd({0x56,std::uint8_t(index),1,0,0x00,0x00})),"bhEffBloodPoolSet 0x56 unbound");
  check(world.blood_particles()==before+1,"bhEffBloodPoolSet spawned no original pool");
  check(world.state(1,index)->enemy.djnt_no==1,"bhEffBloodPoolSet did not write epw->djnt_no");
  // Der zweite Befehl traegt die Lage selbst (Vorzeichen in v0, Zentimeter) und
  // waehlt bei v1 == 2 die andere Tabelle; Winkel in Grad, wie im Original.
  check(world.bind(cmd({0x58,0x05,0x64,0x00,0x00,0x00,0x32,0x00,0,2,90,0,0x00,0x00})),"bhEffBloodPoolSet2 0x58 unbound");
  check(world.blood_particles()==before+2,"bhEffBloodPoolSet2 spawned no original pool");
  // event.c:4302 bhEffBloodSet: Typ 0..2 startet ueber bhEne_SetBlood2 den
  // Originaleffekt 5 (bhEff005) am Work; v1 ist djnt_no und Linkgelenk.
  const auto flames=[&]{unsigned n=0;for(unsigned i=0;world.effect_state(i);++i)if(world.effect_state(i)->id==5)++n;return n;};
  const auto flames_before=flames();
  check(world.bind(cmd({0x4e,std::uint8_t(index),3,0x00,0x64,0x00,0x00,0x00,0x00,0x00,0,0,90,0})),"bhEffBloodSet 0x4e unbound");
  if(flames()<flames_before+1){std::string gaps;for(const auto& g:world.unbound_enemy_routines())gaps+=" ["+g+"]";throw std::runtime_error("bhEffBloodSet spawned no original effect 5;"+gaps);}
  check(world.state(1,index)->enemy.djnt_no==3,"bhEffBloodSet did not write epw->djnt_no");
  check(!world.bind(cmd({0x4e,200,0,0,0,0,0,0,0,0,0,0,0,0})),"bhEffBloodSet accepted an absent enemy work");
  // event.c:1919 bhEffectSet (bhSetEffect in den geteilten eff[512]) und
  // :5336/5346 bhEffPush/bhEffPop (bhPushEffectWork/bhPopEffectWork).
  const auto slots=[&]{unsigned n=0;while(world.effect_state(n))++n;return n;};
  const auto eff_before=slots();
  check(world.bind(cmd({0x2c,0,0x07,0x01})),"bhEffectSet 0x2c unbound"); // id 263
  check(slots()==eff_before+1,"bhEffectSet allocated no eff[512]-Platz");
  check(world.effect_state(eff_before)->id==263&&(world.effect_state(eff_before)->flg&1u)&&world.effect_state(eff_before)->type==0,"bhEffectSet wrote other work fields than bhSetEffect");
  check(world.unbound_enemy_routines().count("bhEff263")==1,"unported effect procedure is not reported by name");
  const auto pushed=slots();
  check(world.bind(cmd({0x70,0})),"bhEffPush 0x70 unbound");
  check(world.bind(cmd({0x2c,0,0x05,0x00})),"bhEffectSet 0x2c unbound"); // id 5, portiert
  check(world.unbound_enemy_routines().count("bhEff5")==0,"a ported procedure was reported as a gap");
  check(slots()==pushed+1,"bhEffPush stopped the running pass");
  check(world.bind(cmd({0x71,0})),"bhEffPop 0x71 unbound");
  check(slots()==pushed,"bhEffPop did not restore the whole original eff[512] set");
  std::cout<<"Original effect-script surface passed: 0x2c allocates the shared eff[512] slot and names the missing procedure, 0x70/0x71 restore the set.\n";
  std::cout<<"Original blood script commands passed: 0x56/0x58 write the pool, 0x4e starts original effect 5 at the work joint and rejects an absent work.\n";}
 if(!soft_failures.empty()){for(const auto& f:soft_failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<soft_failures.size()<<" inventory expectation(s) failed; the remaining bindings passed.\n";return 1;}
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
