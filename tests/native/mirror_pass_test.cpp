// Spiegelpass: effsub1.c:7175 bhEff112, ps2_NaMatrix.c:1016 njMirror (ELF 0x002D7260),
// game.c:77-91 bhEtcMirrorDrawModel, MdlPut.c:57/89 (zweiter bhPutModel-Aufruf), player.c:1666.
// Raumdaten unveraendert (RM_2040, RM_9230, RM_1090); feste Kameraschnitte und gesetzte
// Spielerlagen sind Diagnoseaufstellungen, kein Durchspielnachweis.
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "renderer.h"
#include "binary_view.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <tuple>
#include "ninja_math.h"
#include "effect_sprites.h"
#include "actor.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool v,const std::string& why){if(!v)throw std::runtime_error(why);}
// 1e-3: Sicht (camera_view_matrix) und Billboardinverse rechnen ueber die njSin-Tabelle (Restfehler ~2e-4
// an Einheitsversaetzen); die geprueften Fehlbilder (fehlender Spiegelpraefix/mr_pl-Drehung) weichen um ~1 ab.
static bool near(float a,float b,float e=1e-3f){return std::fabs(a-b)<=e;}
struct Room {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
 AudioMixer audio;Inventory inv;MessageDialog dialog;RoomWorld world;EventVm vm;
 Room(const std::filesystem::path& data,const std::string& name):bytes(RoomArchive(data).load(name+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns[0]),audio(data),world(bytes,scene,actor,motion,frame,audio,data,{},name),vm(bytes,inv,dialog){
  const auto id=RoomIdentity::parse(name);vm.values[0]=id.stage;vm.values[1]=id.room;world.bind_player_collision(vm);
  vm.binding=[&](const EventCommand& c){return world.bind(c);};vm.task_started=[&](unsigned n){world.start_task(n);};
 }
 // Diagnose: ein isolierter Steuerpass mit sp_flg 8 (bhControlEffect, game.c:50).
 void pass(){world.set_pad(0,0);vm.flags[6][0]|=8u;world.set_simulation_flags(vm.flags[6][0]);world.tick();if(!scene.cameras.empty())world.control_camera(scene,scene.cameras.at(0));} // game.c:48-59
 Scene compose(std::size_t cut){Scene out;out.triangles=scene.triangles;out.ambient_writes=scene.ambient_writes;out.textures=scene.textures;out.background=scene.background;
  auto camera=scene.cameras.at(cut);world.append(out,{},camera);return out;}
};
static std::size_t count_mirror(const Scene& s,unsigned pass){std::size_t n=0;for(const auto& t:s.triangles)n+=t.mirror==pass;return n;}
static std::size_t differences(const SoftwareRenderer& a,const SoftwareRenderer& b){std::size_t n=0;for(std::size_t i=0;i<a.pixels().size();++i)n+=a.pixels()[i]!=b.pixels()[i];return n;}
// --- game.c:87 -> :427 bhDrawEffect im Spiegelpass (gm_flg 0x4000) ---
// Unit: effect.c:1219-1239 Billboardbasis unter cam.mtx * njMirror, mr_pl-Zweig (effect.c:1225,
// effsub2.c:4644), mdflg-0x40-Tor (effect.c:1301); effsub1.c:5474 bhDraw025 / :6299 bhDraw107 /
// :6229 bhEff106 schreiben im Zeichenteil.
static void mirror_effect_offsets(const Scene& sc,const SceneCamera& cam,std::size_t first,NJS_POINT3 center,std::array<NJS_POINT3,3>& out){
 NJS_MATRIX view;camera_view_matrix(cam,view);NJS_MATRIX m;std::copy_n(sc.mirror_matrix.data(),16,m);njMultiMatrix(&view,&m);
 NJS_POINT3 c;njCalcPoint(&view,&center,&c);
 for(unsigned k=0;k<3;++k){NJS_POINT3 p=sc.triangles.at(first).vertices[k].position,q;njCalcPoint(&view,&p,&q);out[k]={q.x-c.x,q.y-c.y,q.z-c.z};}}
static Scene mirror_scene(NJS_PLANE pl){Scene sc;NJS_MATRIX m;njUnitMatrix(&m);check(njMirror(&m,&pl)==1,"test plane must take the defined njMirror branch");
 sc.mirror_enabled=true;std::copy_n(m,16,sc.mirror_matrix.begin());effect_mirror_plane()=pl;return sc;}
static void mirror_effect_unit_tests(){
 SceneCamera cam;cam.position={1,2,3};cam.angles={1000,20000,300};cam.distance=12;
 EffectSprite s;s.id=302;s.flg=0x100000u; // Billboard, Eckpunkte tv (-1,-1)(1,-1)(-1,1)(1,1)
 const NJS_POINT3 center{4,1,-2};
 // Ebene ohne vx/vy: keine mr_pl-Drehung. Unter cam.mtx*njMirror liegt das Viereck kameraparallel.
 {Scene sc=mirror_scene({0,0,5,0,0,1});append_effect_sprite(sc,s,center,{1,1,1},cam,0,nullptr,nullptr,true,true);
  check(sc.triangles.size()==2&&sc.triangles[0].mirror==3&&sc.triangles[1].mirror==3,"mirror effect triangles must carry SceneTriangle::mirror 3");
  std::array<NJS_POINT3,3> o;mirror_effect_offsets(sc,cam,0,center,o);
  check(near(o[0].x,-1)&&near(o[0].y,-1)&&near(o[0].z,0)&&near(o[1].x,1)&&near(o[1].y,-1)&&near(o[2].x,-1)&&near(o[2].y,1)&&near(o[2].z,0),"njUnitRotPortion under cam.mtx*njMirror: billboard not screen-parallel in the mirror view");
  // Gegenprobe: die ungespiegelte Basis liegt in der Spiegelsicht schraeg (der Test sieht den Fehler).
  Scene wrong=sc;wrong.triangles.clear();append_effect_sprite(wrong,s,center,{1,1,1},cam,0,nullptr,nullptr,false,true);
  std::array<NJS_POINT3,3> w;mirror_effect_offsets(wrong,cam,0,center,w);check(!(near(w[0].x,-1)&&near(w[0].y,-1)&&near(w[0].z,0)),"unmirrored basis must differ in the mirror view");}
 // mr_pl.vx != 0: njRotateY(32768) nach njRotateZ(az) -> x und z kehren um (effect.c:1227-1230).
 {Scene sc=mirror_scene({0,0,5,0.6f,0,0.8f});append_effect_sprite(sc,s,center,{1,1,1},cam,0,nullptr,nullptr,true,true);
  std::array<NJS_POINT3,3> o;mirror_effect_offsets(sc,cam,0,center,o);check(near(o[0].x,1)&&near(o[0].y,-1)&&near(o[0].z,0),"mr_pl.vx: njRotateY(32768) missing");
  Scene plain=sc;plain.triangles.clear();append_effect_sprite(plain,s,center,{1,1,1},cam,0,nullptr,nullptr,true,false);
  mirror_effect_offsets(plain,cam,0,center,o);check(near(o[0].x,-1)&&near(o[0].y,-1),"njPtclDrawSprite/ef_fnc billboards must not take the mr_pl branch");}
 // mr_pl.vy != 0 (vx 0): njRotateX(32768) -> y und z kehren um (effect.c:1232-1235).
 {Scene sc=mirror_scene({0,5,0,0,0.6f,0.8f});append_effect_sprite(sc,s,center,{1,1,1},cam,0,nullptr,nullptr,true,true);
  std::array<NJS_POINT3,3> o;mirror_effect_offsets(sc,cam,0,center,o);check(near(o[0].x,-1)&&near(o[0].y,1)&&near(o[0].z,0),"mr_pl.vy: njRotateX(32768) missing");}
 // effect.c:1301 (|| gm_flg 0x4000 && mdflg 0x40): nur der Spiegelaufruf laesst den Satz aus.
 {Scene sc=mirror_scene({0,0,5,0,0,1});EffectSprite m=s;m.mdflg=0x40u;
  append_effect_sprite(sc,m,center,{1,1,1},cam,0,nullptr,nullptr,true,true);check(sc.triangles.empty(),"mdflg 0x40 must skip the mirror call");
  append_effect_sprite(sc,m,center,{1,1,1},cam,0);check(sc.triangles.size()==2,"mdflg 0x40 must not skip the normal call");}
 // Zeichenteile: Steuerung mit defer_custom_draw reiht nur ein; jeder Zeichenaufruf schreibt.
 int slot=0;set_effect_extra_allocator([&slot]{return slot++;},[](int){});
 EffectContext c;c.pt_flg=0x10;c.sp_flg=8;c.defer_custom_draw=true;c.camera_ready=true;c.camera_position={0,0,0};c.camera_view={0,0,1};
 {EffectSpawn sp;sp.id=25;sp.flg=1;EffectSprite e;initialize_effect_sprite(e,sp);bool queued=false;
  step_effect_sprite(e,c,&queued);for(int i=0;i<6;++i)step_effect_sprite(e,c,&queued); // ct0 6: ab hier eingereiht (effsub1.c:5600)
  check(queued&&e.custom_queued&&e.quads.empty(),"bhEff025: deferred control must enqueue without drawing");
  check(draw_effect_sprite_work(e,c)&&e.quads.size()==8&&near(e.quads[0].vertices[1].x,1.0f),"bhDraw025 first draw: scale 1");
  check(draw_effect_sprite_work(e,c)&&e.quads.size()==8&&near(e.quads[0].vertices[1].x,0.8f),"bhDraw025 second draw (mirror) must see scl *= 0.8 of the first (effsub1.c:5546)");
  EffectContext off=c;off.pt_flg=1;check(!draw_effect_sprite_work(e,off),"no draw without pt_flg 0x10 (game.c:339/347/426)");}
 // effsub1.c:6351-6356: ay++ vor dem Test ay < 5.
 {EffectSprite e;e.id=107;e.flg=1;e.custom_queued=true;e.extra=std::make_shared<std::array<unsigned,312>>();
  struct Row{float px,py,pz;short ax,ay;};static_assert(sizeof(Row)==16,"ER_WORK");
  Row rows[8]{};rows[0]={0,0,0,1,4};rows[1]={0,0,0,1,3};std::memcpy(reinterpret_cast<unsigned char*>(e.extra->data())+4,rows,sizeof rows);
  check(draw_effect_sprite_work(e,c)&&e.quads.size()==1&&near(e.quads[0].vertices[0].u,0.328125f),"bhDraw107: ay 4 -> 5 must not draw, ay 3 -> 4 draws uvinfo[3]");
  EffectContext still=c;still.sp_flg=0;draw_effect_sprite_work(e,still);check(e.quads.size()==1&&near(e.quads[0].vertices[0].u,0.4375f),"bhDraw107 without sp_flg 8 draws uvinfo[ay] unchanged");}
 // effsub1.c:6248/6249: bhEff106 laesst 8/10 stehen.
 {EffectSpawn sp;sp.id=100;sp.flg=1;EffectSprite e;initialize_effect_sprite(e,sp);bool queued=false;
  step_effect_sprite(e,c,&queued);step_effect_sprite(e,c,&queued);
  check(e.custom_queued&&e.lines.empty(),"bhEff100 deferred: control must not draw");
  check(draw_effect_sprite_work(e,c)&&e.lines.size()==77,"bhEff106 draws 77 lines");}
 set_effect_extra_allocator({},{});
 std::cout<<"Spiegel-bhDrawEffect: Billboardbasis, mr_pl-Zweig, mdflg 0x40, Zeichenteile 025/107/100 PASS\n";}
static unsigned trail_ct(const EffectSprite& s,unsigned row){std::int16_t ct;std::memcpy(&ct,reinterpret_cast<const unsigned char*>(s.extra->data())+4+16*row+2,2);return unsigned(ct);}
static const EffectSprite* first_trail(RoomWorld& w){for(unsigned i=0;i<512;++i){const auto* s=w.effect_state(i);if(s&&s->id==24&&(s->flg&1u)&&s->extra)return s;}return nullptr;}
// Daten: bhSetExplosionEffect (weapon.c:1812) erzeugt zwei bhEff024; bhDraw024 (effsub1.c:5261-5360)
// schreibt die Spurzeile jedes Zeichenaufrufs neu und zaehlt alte Zeilen je Aufruf weiter.
static void mirror_effect_tests(const std::filesystem::path& data){
 // RM_1090 ohne Spiegel: ein bhDrawEffect je Bild.
 {Room r(data,"RM_1090");r.world.tick_camera(r.scene,r.scene.cameras.at(0),0,false);r.pass();
  check(r.world.explode_effect(r.actor.position),"RM_1090 explosion");r.pass();r.pass();
  const auto* t=first_trail(r.world);check(t&&t->ct0==1,"RM_1090 bhEff024 after init");
  auto x=r.compose(0);check(count_mirror(x,3)==0,"RM_1090 no mirror effect output");
  r.pass();x=r.compose(0);check(trail_ct(*t,0)==2,"RM_1090 one draw per frame: row 0 ct 2, got "+std::to_string(trail_ct(*t,0)));
  x=r.compose(0);check(trail_ct(*t,0)==2,"recomposing the same frame must not advance bhDraw024");
  r.pass();r.pass();check(trail_ct(*t,0)==3,"an uncomposed frame still draws once (game.c:61-92), got "+std::to_string(trail_ct(*t,0)));
  std::cout<<"RM_1090: bhDraw024 ein Zeichenlauf je Bild, kein Fortschritt bei Wiederzusammensetzung PASS\n";}
 // RM_2040 mit Spiegel (Ereignis 0 +0xc4/+0xc8 wie oben): zwei bhDrawEffect je Bild.
 {Room r(data,"RM_2040");r.vm.initialize();r.pass();
  const auto info=inspect_room(r.bytes);const BinaryView v(r.bytes);const auto ev0=info.event_offset+v.u32(info.event_offset+8);
  EventCommand on;on.opcode=0x43;on.bytes.assign(r.bytes.begin()+std::ptrdiff_t(ev0+0xc4),r.bytes.begin()+std::ptrdiff_t(ev0+0xc8));
  check(on.bytes==std::vector<std::uint8_t>{0x43,0,1,0},"RM_2040 event 0 +0xc4 moved");
  r.vm.set_flag(5,0x17,0);check(r.world.bind(on),"bhEffDispSet on");r.world.tick_camera(r.scene,r.scene.cameras.at(0),0,false);r.pass();
  check((r.vm.flags[5][0]&0x100u)!=0,"RM_2040 mirror on");
  check(r.world.explode_effect(r.actor.position),"RM_2040 explosion");r.pass();r.pass();
  const auto* t=first_trail(r.world);check(t&&t->ct0==1,"RM_2040 bhEff024 after init");
  auto x=r.compose(0);check(x.mirror_enabled&&count_mirror(x,3)>0,"RM_2040 second bhDrawEffect produced no mirrored output");
  for(const auto& tr:x.triangles)if(tr.mirror==3)check(tr.after_modifiers&&tr.material_phase==10&&!tr.camera_mask,"mirror effect triangle state");
  const auto& gaps=r.world.unbound_enemy_routines();
  check(!gaps.count("game.c:91 bhEtcMirrorDrawModel: bhDrawEffect unter gm_flg 0x4000 (gespiegelte Effektpaesse) nicht portiert"),"old mirror-effect gap still reported");
  r.pass();x=r.compose(0);check(trail_ct(*t,0)==3,"RM_2040 two draws per frame: row 0 ct 3, got "+std::to_string(trail_ct(*t,0)));
  x=r.compose(0);check(trail_ct(*t,0)==3,"recomposing the mirror frame must not advance bhDraw024");
  std::cout<<"RM_2040: zweiter bhDrawEffect unter gm_flg 0x4000, bhDraw024 zwei Zeichenlaeufe je Bild PASS\n";}
}
int main(int argc,char** argv){try{_Make_SinTable();
 mirror_effect_unit_tests();
 // --- njMirror, Formel der Disassembly ---
 {NJS_MATRIX m;njUnitMatrix(&m);NJS_PLANE pl{0,0,5,0,0,1};check(njMirror(&m,&pl)==1,"njMirror z plane path");
  NJS_POINT3 p{1,2,3},q;njCalcPoint(&m,&p,&q);check(near(q.x,1)&&near(q.y,2)&&near(q.z,7),"njMirror z=5 reflects 3 -> 7");
  NJS_PLANE origin{0,0,0,0,1,0};njUnitMatrix(&m);check(njMirror(&m,&origin)==0,"njMirror d == +0 must stay the undefined original branch");
  NJS_PLANE axis{3,0,0,1,0,0};njUnitMatrix(&m);check(njMirror(&m,&axis)==0,"njMirror vy+vz == +0 must stay the undefined original branch");
  std::cout<<"njMirror: Reflexion z=5, undefinierte Originalzweige d=+0 / vy+vz=+0 PASS\n";}
 // --- bhEff112 ---
 {EffectSprite s;s.id=112;s.flg=1;s.px=-1.7511461973190308f;s.py=0;s.pz=29.02325439453125f;s.ax=0;s.ay=-16380;s.az=0;
  EffectContext c;c.stage_no=2;take_effect_output();step_effect_sprite(s,c);auto out=take_effect_output();
  check((s.flg&0x1000000u)&&out.st_flg_set==0x100u&&!out.st_flg_clear,"bhEff112 stage 2: st_flg 0x100 and hidden work");
  const auto& pl=effect_mirror_plane();
  // cmat diag(1,-1,-1) * RotY(-16380) * (0,0,-1): vx = sin(-16380), vz = cos(-16380) mit Vorzeichen aus cmat.
  check(near(pl.px,s.px)&&near(pl.pz,s.pz)&&near(pl.vx,-njSin(-16380),1e-6f)&&near(pl.vy,0,1e-6f)&&near(pl.vz,njCos(-16380),1e-6f),"bhEff112 mr_pl from cmat*RotXYZ*(0,0,-1)");
  // njMirror auf diese Ebene: lAngleZ nimmt sqrt(|vy+vz|) (0x002D73C8/CC), nicht sqrt(vy^2+vz^2).
  const float buggy=std::sqrt(std::fabs(0.0f+pl.vy+pl.vz)),squares=std::sqrt(pl.vy*pl.vy+pl.vz*pl.vz);
  const int az_orig=-int((float(std::atan2(double(pl.vx),double(buggy)))*32768.0f)/3.141592f);
  const int az_squares=-int((float(std::atan2(double(pl.vx),double(squares)))*32768.0f)/3.141592f);
  check(az_orig!=az_squares,"RM_2040 plane separates the original vf5 operand from the squared one");
  NJS_MATRIX m;njUnitMatrix(&m);NJS_PLANE copy=pl;check(njMirror(&m,&copy)==1,"RM_2040 plane defined path");
  NJS_MATRIX e;njUnitMatrix(&e);const int ax=int((float(std::atan2(double(pl.vz),double(pl.vy)))*32768.0f)/3.141592f);
  const float q=njInvertSqrt((pl.vx*pl.vx+pl.vy*pl.vy)+pl.vz*pl.vz),d=((0.0f-pl.vx*q*pl.px)-pl.vy*q*pl.py)-pl.vz*q*pl.pz;
  e[12]=0.0f-pl.vx*(d+d);e[13]=0.0f-pl.vy*(d+d);e[14]=0.0f-pl.vz*(d+d);njRotateX(&e,ax);njRotateZ(&e,az_orig);njScale(&e,1,-1,1);njRotateZ(&e,-az_orig);njRotateX(&e,-ax);
  for(int i=0;i<16;++i)check(m[i]==e[i],"njMirror matrix element "+std::to_string(i));
  s.flg=1;c.stage_no=9;step_effect_sprite(s,c);out=take_effect_output();check(out.st_flg_clear==0x100u&&!out.st_flg_set,"bhEff112 stage 9 clears st_flg 0x100");
  s.flg=1;s.mdflg=1;c.stage_no=2;step_effect_sprite(s,c);out=take_effect_output();check(out.st_flg_clear==0x100u&&!out.st_flg_set,"bhEff112 mdflg 1 clears st_flg 0x100");
  std::cout<<"bhEff112: Stufe 2 setzt st_flg 0x100/mr_pl, Stufe 9 und mdflg 1 loeschen; njMirror lAngleZ="<<az_orig<<" (Quadrate waeren "<<az_squares<<") PASS\n";}
 if(argc<2)return 0;const std::filesystem::path data=argv[1];const std::filesystem::path output=argc>2?argv[2]:std::filesystem::path{};
 // --- RM_2040: Raumeffekt 0 = 112 (room.c bhSetEffectTable); das Raumskript schaltet ihn ---
 {Room r(data,"RM_2040");const auto* s=r.world.effect_state(0);check(s&&s->id==112,"RM_2040 room effect 0 is 112");
  // Event_init: 43 00 00 00 = bhEffDispSet(0, 0) (event.c:3653) versteckt den Satz, bhControlEffect
  // (effect.c:776) ruft bhEff112 dann nicht: kein Spiegel beim Betreten.
  r.vm.initialize();r.pass();check(!(r.vm.flags[5][0]&0x100u)&&(r.world.effect_state(0)->stflg&0x1000000u),"RM_2040 Event_init must keep the mirror effect off");
  // Ereignis 0 (RDT 0x165706) schaltet den Spiegel bei +0xc4 ein und bei +0xe2 wieder aus, dazwischen
  // bhCamSet auf Ereigniskamera 7. Diagnose: genau diese Skriptbefehle einzeln an den Raumbesitzer.
  const auto info=inspect_room(r.bytes);const BinaryView v(r.bytes);const auto ev0=info.event_offset+v.u32(info.event_offset+8);
  auto command=[&](std::size_t at,std::size_t n){EventCommand c;c.opcode=r.bytes.at(at);c.bytes.assign(r.bytes.begin()+std::ptrdiff_t(at),r.bytes.begin()+std::ptrdiff_t(at+n));return c;};
  // +0xc4 43 00 01 00 bhEffDispSet(0, 1); +0xc8 05 05 17 00 00 00 bhSet (event.c:554: Bank 5 = sys->st_flg,
  // Bit 0x17 = 0x80000000 >> 23 = 0x100, Art 0 setzen). +0xe2/+0xe6 dasselbe mit 0 bzw. Art 1 (loeschen).
  const auto on=command(ev0+0xc4,4),set_on=command(ev0+0xc8,6),off=command(ev0+0xe2,4),set_off=command(ev0+0xe6,6);
  check(on.bytes==std::vector<std::uint8_t>{0x43,0,1,0}&&off.bytes==std::vector<std::uint8_t>{0x43,0,0,0},"RM_2040 event 0 mirror commands moved");
  check(set_on.bytes==std::vector<std::uint8_t>{5,5,0x17,0,0,0}&&set_off.bytes==std::vector<std::uint8_t>{5,5,0x17,0,0,1},"RM_2040 event 0 st_flg commands moved");
  auto bh_set=[&](const EventCommand& c){r.vm.set_flag(c.bytes[1],unsigned(c.bytes[2]|c.bytes[3]<<8),c.bytes[5]);}; // EventVm-Primitive von bhSet
  // Gegenprobe: bhSet allein (ohne bhEff112) schaltet den Spiegel nicht auf eine Ebene, die niemand schrieb.
  effect_mirror_plane()=NJS_PLANE{};bh_set(set_on);{auto x=r.compose(0);check(!x.mirror_enabled,"njMirror on an unwritten mr_pl (d == +0) must stay off");}
  check(r.world.bind(on),"bhEffDispSet on");r.pass();
  check((r.vm.flags[5][0]&0x100u)!=0,"RM_2040 bhEff112 did not set st_flg 0x100");
  check((r.world.effect_state(0)->flg&0x1000000u)!=0,"RM_2040 bhEff112 work not hidden");
  const std::size_t room=std::count_if(r.scene.triangles.begin(),r.scene.triangles.end(),[](const SceneTriangle& t){return t.camera_mask;});
  // Ereigniskamera 7 Schluessel 0 (Ereignis 0 bei +0xd0: 13 00 07 00): hidobj[4..5] = bbfff7ff ffe7fc20.
  r.world.set_event_camera(7,0,false);const auto camera=r.world.camera(r.scene,r.scene.cameras.at(0));
  check(camera.hidden_objects[4]==0xbbfff7ffu&&camera.hidden_objects[5]==0xffe7fc20u,"RM_2040 event camera 7/0 mirror mask");
  Scene full;full.triangles=r.scene.triangles;full.ambient_writes=r.scene.ambient_writes;full.textures=r.scene.textures;r.world.append(full,{},camera);
  check(full.mirror_enabled&&count_mirror(full,2)==room,"RM_2040 bhEtcMirrorDrawModel copies every room triangle");
  for(const auto& t:full.triangles)if(t.mirror==2)check(t.hidden_offset==4&&t.material_phase==9&&t.camera_mask&&t.after_modifiers,"mirror room triangle state");
  check(count_mirror(full,1)>0,"RM_2040 no bhPutModel mirror call for the room objects");
  SoftwareRenderer a(640,480),b(640,480);Scene plain=full;plain.erase_triangles([](const SceneTriangle& t){return t.mirror!=0;});
  a.draw(plain,camera);b.draw(full,camera);const auto changed=differences(a,b);
  check(changed>500,"RM_2040 event camera 7: mirror pass draws no pixels ("+std::to_string(changed)+")");
  // Gegenprobe Maske: dieselbe Spiegelung mit hidobj ab Wort 0 statt 4 ergibt ein anderes Bild.
  Scene wrong=full;for(auto& t:wrong.triangles)if(t.mirror==2)t.hidden_offset=0;for(auto& w:wrong.ambient_writes)if(w.hidden_offset)w.hidden_offset=0;
  SoftwareRenderer w(640,480);w.draw(wrong,camera);check(differences(w,b)>0,"RM_2040 mirror mask offset 4 not observable");
  if(!output.empty()){a.save_bmp(output/"RM_2040-evcam7-ohne-spiegel.bmp");b.save_bmp(output/"RM_2040-evcam7-spiegel.bmp");}
  // Schnitt 0: hidobj[4..5] = ffffffff ffffffe0 versteckt alle 59 Objekte der Raumspiegelung.
  {auto s0=r.compose(0);s0.erase_triangles([](const SceneTriangle& t){return t.mirror==1;});Scene p0=s0;p0.erase_triangles([](const SceneTriangle& t){return t.mirror!=0;});
   SoftwareRenderer c(640,480),d(640,480);c.draw(p0,r.scene.cameras.at(0));d.draw(s0,r.scene.cameras.at(0));check(differences(c,d)==0,"RM_2040 cut 0: room mirror must be fully hidden");}
  // Ereignis 0 +0xe2/+0xe6: bhEffDispSet(0, 0) haelt bhEff112 an, das bhSet loescht st_flg 0x100.
  check(r.world.bind(off),"bhEffDispSet off");r.pass();check((r.vm.flags[5][0]&0x100u)!=0,"RM_2040 hidden bhEff112 must not clear st_flg 0x100 itself");
  bh_set(set_off);r.pass();check(!(r.vm.flags[5][0]&0x100u),"RM_2040 bhSet clear");{auto x=r.compose(0);check(!x.mirror_enabled&&count_mirror(x,2)==0,"RM_2040 mirror after event 0 end");}
  std::cout<<"RM_2040: Event_init aus, Ereignis 0 ein (bhEffDispSet + bhSet st_flg 0x100), Ereigniskamera 7 Spiegelbild "<<changed<<" Pixel, Schnitt 0 leer, Ereignisende aus PASS\n";
  // Spiegel fuer die Spielerpruefung wieder wie bei +0xc4/+0xc8 einschalten (Diagnose: zweiter Durchlauf).
  bh_set(set_on);check(r.world.bind(on),"bhEffDispSet on again");r.pass();
  // player.c:1666 ohne st_flg 0x100 laesst plp->mdflg stehen: erst mit gesetztem Bit wird es neu bestimmt.
  // player.c:1666: prm0-2-Flaeche (Bodentabelle Satz 4, x -47..63, z -54..17) schliesst den Spieler aus.
  r.actor.position={0,0,0};r.world.check_player_mirror_floor(0);check(r.world.player_mirror_off(),"RM_2040 player inside prm0 2 area keeps mdflg 0x40");
  r.actor.position={0,0,25};r.world.check_player_mirror_floor(0);check(!r.world.player_mirror_off(),"RM_2040 player outside the area clears mdflg 0x40");
  r.actor.position={0,0,0};r.world.check_player_mirror_floor(1);check(!r.world.player_mirror_off(),"RM_2040 other floor number must not match");
  std::cout<<"RM_2040: plp->mdflg 0x40 aus bhCheckFloorEffect prm0 2 (Diagnoselagen) PASS\n";
  // bhPutModel(plp) unter Ereigniskamera 7 wie die Laufzeit (sample_player, append_actor vor RoomWorld::append):
  // Diagnoselagen ausserhalb bzw. innerhalb der prm0-2-Flaeche.
  const auto player=PlayerAsset::load(data,10);r.world.set_player_asset(&player);
  check(player.encoded_model().size()>3&&(player.encoded_model()[3]&0x80u),"player MDL status 0x80 (mlwP->flg)");
  auto with_player=[&](NJS_POINT3 at){r.actor.position=at;r.world.check_player_mirror_floor(0);Scene out;out.triangles=r.scene.triangles;out.ambient_writes=r.scene.ambient_writes;out.textures=r.scene.textures;
   std::vector<BoneMatrix> nodes;const auto body=r.world.sample_player(player,&nodes);const auto base=out.textures.size();out.textures.insert(out.textures.end(),player.textures().begin(),player.textures().end());append_actor(out,body,base);
   r.world.append(out,nodes,camera);
   if(!output.empty()){Scene plain=out;plain.erase_triangles([](const SceneTriangle& t){return t.mirror!=0;});SoftwareRenderer x(640,480),y(640,480);x.draw(plain,camera);y.draw(out,camera);
    const std::string tag=std::to_string(int(at.x))+"_"+std::to_string(int(at.z));
    x.save_bmp(output/("RM_2040-evcam7-spieler-"+tag+"-ohne-spiegel.bmp"));y.save_bmp(output/("RM_2040-evcam7-spieler-"+tag+"-spiegel.bmp"));}
   std::size_t bodies=0,copies=0;for(const auto& t:out.triangles){if(t.player_body&&!t.mirror)++bodies;if(t.player_body&&t.mirror==1)++copies;}return std::pair<std::size_t,std::size_t>{bodies,copies};};
  const auto outside=with_player({2,0,35}),inside=with_player({2,0,10});
  check(outside.first>0&&outside.second==outside.first,"RM_2040 player outside prm0 2: second bhPutModel draw missing");
  check(inside.first>0&&inside.second==0,"RM_2040 player inside prm0 2: plp->mdflg 0x40 must suppress the mirror draw");
  std::cout<<"RM_2040: bhPutModel(plp) "<<outside.first<<" Dreiecke gespiegelt ausserhalb, keine innerhalb der prm0-2-Flaeche PASS\n";}
 // --- RM_9230: derselbe Effekt 112, Stufe 9: kein Spiegel ---
 {Room r(data,"RM_9230");const auto* s=r.world.effect_state(0);check(s&&s->id==112,"RM_9230 room effect 0 is 112");
  r.vm.initialize();EventCommand on;on.opcode=0x43;on.bytes={0x43,0,1,0};check(r.world.bind(on),"RM_9230 bhEffDispSet"); // Diagnose: Satz sichtbar
  r.pass();check((r.world.effect_state(0)->flg&0x1000000u)!=0,"RM_9230 bhEff112 ran");check(!(r.vm.flags[5][0]&0x100u),"RM_9230 stage 9 must not mirror");
  for(std::size_t cut=0;cut<r.scene.cameras.size();++cut){auto x=r.compose(cut);check(!x.mirror_enabled&&count_mirror(x,1)==0&&count_mirror(x,2)==0,"RM_9230 no mirror triangles");}
  std::cout<<"RM_9230: bhEff112 in Stufe 9 laesst st_flg 0x100 aus PASS\n";}
 // --- Gegenprobe: ein Raum ohne 112 bleibt ohne Spiegel ---
 {Room r(data,"RM_1090");r.pass();check(!(r.vm.flags[5][0]&0x100u),"RM_1090 st_flg 0x100");auto x=r.compose(0);check(!x.mirror_enabled&&count_mirror(x,2)==0,"RM_1090 no mirror");
  std::cout<<"RM_1090: ohne bhEff112 kein Spiegelpass PASS\n";}
 mirror_effect_tests(data);
 return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
