#include "back_color.h"
#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "ninja_math.h"
#include "doors.h"
#include "gameover.h"
#include "map_screen.h"
#include "movie.h"
#include "typewriter.h"
#include "computer.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <cstring>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void equal(std::uint32_t got,std::uint32_t expected,const char* why){if(got!=expected){std::cerr<<why<<": "<<std::hex<<got<<" != "<<expected<<"\n";throw std::runtime_error(why);}}
struct Room {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
 Inventory inventory;MessageDialog dialog;AudioMixer audio;RoomWorld world;EventVm events;
 Room(const std::filesystem::path& data,const char* name):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns[0]),audio(data),world(bytes,scene,actor,motion,frame,audio,data,{},name),events(bytes,inventory,dialog){
  const auto id=RoomIdentity::parse(name);events.values[0]=id.stage;events.values[1]=id.room;events.values[21]=id.variant;
  events.flags[12][0]=0x3de00;world.bind_player_collision(events);world.initialize_camera_clip(0);
 }
 void clear_delay(unsigned n=1){world.restore_background(n);for(unsigned i=0;i<=n;++i)world.control_background();}
 void cut(unsigned n){events.flags[5][0]&=~1u;world.return_to_cut();world.tick_camera(scene,scene.cameras.at(n),n,false);}
 std::uint32_t rendered_clear(){Scene out=scene;world.append(out,{},world.camera(scene,scene.cameras[0]));return out.background;}
};
int main(int argc,char**argv){try{
 check(argc==2,"requires original data directory");_Make_SinTable();const std::filesystem::path data=argv[1];
 equal(installed_back_color(),0,"main.c:176 njSetBackColor(0,0,0) is the clear before any task");
 // Independent literal expectations: ps2_event.c room rows and exact bit words (not host flag-index conventions).
 const auto get=[](unsigned s,unsigned r,unsigned rc,unsigned c,bool ev,unsigned e9=0,unsigned e12=0){return camera_back_color(s,r,rc,c,ev,0x00412365,0x00123456,e9,e12);};
 equal(get(0,12,0,1,false),0x1e1a10,"stage0 row");equal(get(1,7,0,1,false),0x202018,"stage1 row");
 equal(get(2,4,0,0,false),0x201921,"stage2 row");equal(get(3,6,0,4,false),0x505030,"stage3 row");
 equal(get(4,0,0,2,false),0x101e20,"stage4 row");equal(get(5,68,0,0,false),0xb0c0d0,"stage5 row");
 equal(get(6,5,1,1,false),0x12181f,"room rcase1 row");equal(get(6,5,1,1,false,0x400000),0x10363b,"ev_flag1 true");
 equal(get(6,5,0,3,false,0x400000),0x12120f,"ev_flag0 unaffected");
 equal(get(7,7,1,3,false),0x103c50,"stage7 rcase1 row");equal(get(8,0,0,1,false),0x101e20,"stage8 row");
 equal(get(9,12,0,2,false),0x60,"ev_flag2 false");equal(get(9,12,0,2,false,0,0x20),0xe1f2fe,"ev_flag2 true");
 equal(get(6,6,0,2,false),0x304d50,"ev_flag3 false");equal(get(6,6,0,2,false,0x400000),0x5a8781,"ev_flag3 following col");
 equal(get(6,6,0,2,false,0x8400000),0x889850,"ev_flag3 following ev_col");
 equal(get(6,6,0,2,false,0x18400000),0x5a8781,"ev_flag3 inhibitor");equal(get(6,6,0,2,false,0x8000000),0x304d50,"ev_flag3 activation required");
 equal(get(0,12,1,1,false),0x412365,"rcase mismatch must not use row");equal(get(0,12,0,0,false),0x412365,"camera miss uses mutable rom bak_col");
 equal(get(3,23,0,6,true),0x386880,"event stage3 row");equal(get(6,3,0,0,true),0x10272c,"event stage6 row");
 equal(get(7,7,0,24,true),0x274359,"event stage7 row");equal(get(9,35,0,11,true),0x5b6c8a,"event stage9 row");
 equal(get(7,7,1,24,true),0x412365,"event rcase mismatch");equal(get(7,7,0,0,false),0x205070,"room not event table");equal(get(7,7,0,0,true),0x506070,"event not room table");
 // flag branch exists in both routines but no shipped table row has it set. Explicit selector fixture, not fabricated room data.
 const BackColorRow fog{0,0,0,1,3,0x111111,0x222222};equal(back_color_row(fog,nullptr,0x123456,0,0,false),0x123456,"flag takes cam fog before ev_flag");
 equal(back_color_row(fog,nullptr,0x234567,0,0,true),0x234567,"event flag takes cam fog");
 equal(back_color_pixel(0x00123456),0x00123456,"njSetBackColor RGB to ARGB and alpha0");
 equal(back_color_pixel(0x40123456),0x80123456,"original clear alpha64 host normalized");equal(back_color_pixel(0x80123456),0xff123456,"original clear alpha128 host normalized");
 // The same SoftwareRenderer clear used in production must preserve pixel channels and A=0 at ss=1.
 SoftwareRenderer image(8,8);image.set_supersampling(1);Scene empty;empty.background=back_color_pixel(0x00123456);
 SceneCamera view;view.perspective=10000;image.draw(empty,view);for(const auto pixel:image.pixels())equal(pixel,0x00123456,"renderer clear ARGB/alpha");
 bool threw=false;try{get(10,0,0,0,false);}catch(const std::runtime_error&){threw=true;}check(threw,"invalid stage cannot be disguised as default");
 {
  Room r(data,"RM_0120");check(!r.world.background_enabled(),"room-read must suspend camera writes");
  const auto initial=r.world.background_pixel();r.cut(1);equal(r.world.background_pixel(),initial,"cut while BackColorFlag0");
  r.world.restore_background(1);r.world.control_background();check(!r.world.background_enabled()&&r.world.background_delay()==0,"bcl_ct1 decrements without installing");
  r.world.control_background();equal(r.rendered_clear(),0x001e1a10,"room lookup reaches append clear");check(!(r.events.flags[11][0]&0x8000),"EOR consumes arm bit");
  r.world.suspend_background();r.cut(0);equal(r.world.background_pixel(),0x001e1a10,"suspended cut retains installed clear");
  check(r.world.bind({0xab,0,0,0,{0xab,0x40,0x12,0x34,0x56,2}}),"BGColorSet binding");
  r.cut(0);equal(r.world.background_pixel(),0x001e1a10,"BGColorSet does not immediately install");
  // Every original blocking task bit: timer must hold, independently of world tick and camera calls.
  for(unsigned bit:{0x200u,0x400u,0x800u,0x1000u,0x4000u,0x8000u,0x10000u,0x20000u}){
   r.events.flags[12][0]=0x3de00u&~bit;r.world.control_background();check(r.world.background_delay()==2,"EOR missing task gate");}
  r.events.flags[12][0]=0x3de80;r.world.control_background();check(r.world.background_delay()==2,"EOR ts_flg80 gate");r.events.flags[12][0]=0x3de00;
  r.world.tick_camera(r.scene,r.scene.cameras[0],0,false);r.world.tick_camera(r.scene,r.scene.cameras[0],0,false);check(r.world.background_delay()==2,"multiple camera checks advanced EOR timer");
  r.world.control_background();r.world.control_background();equal(r.world.background_pixel(),0x001e1a10,"nonzero timer does not install on decrement-to-zero");
  r.world.control_background();equal(r.rendered_clear(),0x80123456,"expiry uses updated rom bak_col on miss");
  r.cut(1);equal(r.rendered_clear(),0x001e1a10,"cut row overrides prior BGColorSet");
  // rom->bak_col is already changed before the countdown: a camera miss observes it immediately.
  r.world.bind({0xab,0,0,0,{0xab,0,0x13,0x57,0x9b,3}});r.cut(0);
  equal(r.rendered_clear(),0x0013579b,"camera miss must read rom bak_col before timer expiry");
  r.cut(1);equal(r.rendered_clear(),0x001e1a10,"matching row still wins while timer pending");
  equal(r.world.room_background(),0x0013579b,"shared ROM colour getter returns raw opcode value");
  {const auto saved_room=r.world.room_background();r.world.set_room_background(0);equal(r.world.room_background(),0,"effect138 shared ROM write");
   check(r.world.background_delay()==3,"raw ROM write must not implicitly arm/reset EOR");r.world.set_room_background(saved_room);}
  // A new opcode replaces the old counter/room colour without writing the installed clear.
  r.world.bind({0xab,0,0,0,{0xab,0,0x23,0x45,0x67,0}});check(r.world.background_delay()==0,"repeated opcode replaces timer");
  r.world.control_background();equal(r.rendered_clear(),0x001e1a10,"zero delay still selects camera row");
  // sp_flg=0 is Pause/frozen passes, NOT an EOR gate; the table must still win.
  r.events.flags[6][0]=0;r.world.restore_background(0);r.world.control_background();equal(r.world.background_pixel(),0x001e1a10,"pause must not disable lookup");
  r.events.values[21]=1;r.cut(0);r.world.bind({0xab,0,0,0,{0xab,0,0,0,0,0}});r.world.control_background();equal(r.rendered_clear(),0,"black is a color, not absent state");
 }
 {
  Room r(data,"RM_7070");r.cut(0);r.clear_delay();equal(r.rendered_clear(),0x00205070,"RM7070 room clear");
  r.world.set_fixed_cut(2);check(r.events.flags[5][0]&1u,"fixed room cut must retain original st_flg1");equal(r.rendered_clear(),0x00205070,"fixed room cut uses room table");r.world.restore_background(0);r.world.control_background();equal(r.rendered_clear(),0x00506070,"fixed room cut EOR uses st_flg1 and last evc_no, not cam type");r.events.flags[5][0]&=~1u;
  r.world.set_event_camera(0,0,false);equal(r.rendered_clear(),0x00506070,"event fixed cut selects event table");
  r.world.set_event_camera(2,0,true);equal(r.rendered_clear(),0x00304060,"event moving camera selects track not key");
  r.world.return_to_cut();r.cut(0);equal(r.rendered_clear(),0x00205070,"return restores room table");
  r.world.suspend_background();r.world.set_event_camera(0,0,true);equal(r.rendered_clear(),0x00205070,"subtask suppresses event camera writes");
  r.world.restore_background(0);r.world.control_background();equal(r.rendered_clear(),0x00506070,"EOR return dispatches on st_flg1");
 }
 {
  Room r(data,"RM_6060");r.cut(2);r.events.flags[1][9]=0x8400000;r.clear_delay(0);equal(r.rendered_clear(),0x00889850,"live event word and following row reach actual world");
  r.events.flags[1][9]|=0x10000000;r.cut(1);r.cut(2);equal(r.rendered_clear(),0x005a8781,"world live inhibitor branch");
 }
 // One Db.clear (ps2_NaSystem.c:146) across room reads and tasks. Asymmetric colour 0x123456 so channel
 // swaps, the room's own bak_col or a host black cannot pass for it.
 {
  Room a(data,"RM_0120");a.cut(0);a.world.bind({0xab,0,0,0,{0xab,0,0x12,0x34,0x56,0}});a.world.control_background();
  equal(a.rendered_clear(),0x00123456,"source room installs BGColorSet via camera miss");
  Room b(data,"RM_7070");check(b.world.room_background()!=0x00123456u,"destination bak_col must differ");b.cut(0);
  equal(b.rendered_clear(),0x00123456,"room read (system.c:1601-1895) keeps the installed clear before EOR");
  b.world.restore_background(1);b.world.control_background();equal(b.rendered_clear(),0x00123456,"bcl_ct 1: first EOR pass only counts down");
  b.world.control_background();equal(b.rendered_clear(),0x00205070,"EOR expiry installs the destination row");
  equal(a.rendered_clear(),0x00205070,"a second RoomWorld reads the same global clear, not a private copy");
  // door.c:243 bhSetDoor / system.c:982: a door demo installs 0, the clear of every door frame.
  b.world.bind({0xab,0,0,0,{0xab,0,0x12,0x34,0x56,0}});b.cut(1);b.world.control_background();equal(b.rendered_clear(),0x00123456,"camera miss reinstalls asymmetric bak_col");
  {DoorSequence direct;direct.begin(data,{},DoorRequest{0x80000000u,0,1,0,7,0});check(direct.direct(),"attr 0x80000000 is the mode 10 path");
   SoftwareRenderer image(16,12);image.set_supersampling(1);direct.draw(image);
   for(const auto p:image.pixels())equal(p,0x00123456,"direct transition frame clears with Db.clear, not host black");
   equal(installed_back_color(),0x00123456,"DoorSequence leaves the mode 10 argb install to bhSysCallDoordemo (runtime)");}
  {DoorSequence demo;demo.begin(data,{},DoorRequest{0,0,1,0,7,0});check(!demo.direct(),"door type 7 runs a demo");
   equal(installed_back_color(),0,"bhSetDoor njSetBackColor(0,0,0)");
   SoftwareRenderer image(16,12);image.set_supersampling(1);demo.draw(image);for(const auto p:image.pixels())equal(p,0,"door frame clear is installed 0 incl. alpha 0");
   for(unsigned t=0;t<400&&!(demo.state().status&0x200);++t)demo.tick();check((demo.state().status&0x200)&&!(demo.state().status&0x40),"door model on screen");
   SoftwareRenderer one(64,48),two(64,48);one.set_supersampling(1);two.set_supersampling(1);
   set_back_color(0x00123456);demo.draw(one);set_back_color(0x00563412);demo.draw(two);
   check(one.pixels()!=two.pixels(),"door model frame clears with Db.clear");set_back_color(0x00123456);}
  // gameover.c:266/334: both exits of md1 0 install 0; md1 0 itself (70 passes) leaves the room clear.
  for(const bool death:{false,true}){
   set_back_color(0x00123456);std::vector<std::uint8_t> raw(1500);const float version=1.73f;std::memcpy(raw.data(),&version,4);
   for(const auto [at,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{16,64},{28,1100},{102,1400},{1100,16},{1104,18},{1108,80},{1112,110}})std::memcpy(raw.data()+at,&v,4);
   Inventory inv;MessageDialog dialog;EventVm ev(raw,inv,dialog);AudioMixer audio(data);GameOver over(data);
   over.begin(ev,0,0,0,0,nullptr);over.tick(0,ev,audio);if(death)ev.flags[11][0]|=0x400u;
   unsigned passes=0;while(over.phase()==0&&passes<200){equal(installed_back_color(),0x00123456,"md1 0 keeps Db.clear");over.tick(0,ev,audio);++passes;}
   check(over.phase()==(death?10u:1u),"game over left md1 0");equal(installed_back_color(),0,death?"gameover.c:266":"gameover.c:334");
   GameUi ui(data);SoftwareRenderer first(32,24),second(32,24);first.set_supersampling(1);second.set_supersampling(1);
   bool differs=false;for(unsigned t=0;t<400&&!differs&&over.active();++t){set_back_color(0x00123456);over.draw(first,ui);set_back_color(0x00563412);over.draw(second,ui);
    differs=first.pixels()!=second.pixels();over.tick(0,ev,audio);}
   check(differs,"game over frame must clear with Db.clear, not a fixed host black");}
  // map.c:71 bhInitMap and :359 MP_MOD_IDX_READ.
  {std::uint32_t slots[16]={};MapServices services;services.flag_check=[](unsigned,unsigned){return false;};services.flag_set=[](unsigned,unsigned,unsigned){};
   services.inventory_slots=[&slots]{return static_cast<const std::uint32_t*>(slots);};
   services.read_inside_file=[&data](unsigned,unsigned id){char name[16];std::snprintf(name,sizeof name,"%04u.bin",id);return read_asset_file(data/"archives/MRY"/name,8*1024*1024);};
   set_back_color(0x00123456);MapScreen map;MapHost host;map.init(RECVX_MP_SET_SUB,host,services);equal(installed_back_color(),0,"map.c:71 bhInitMap");
   set_back_color(0x00123456);for(unsigned i=0;i<4&&installed_back_color();++i)map.control(host);equal(installed_back_color(),0,"map.c:359 MP_MOD_IDX_READ");}
  // bup_00.c:299-304 TypeWriterTextureInit case 6.
  {set_back_color(0x00123456);Typewriter tw;tw.open(Typewriter::Mode::Save,std::filesystem::temp_directory_path()/"recvx-back-color-no-saves");equal(installed_back_color(),0,"bup_00.c:303");}
  // system.c:1141/1171/1184 bhSysCallMovie once a movie plays. Minimal RVV1: 1x1, one JPEG SOI/EOI packet, no audio.
  {set_back_color(0x00123456);AudioMixer audio(data);MoviePlayer movie(data,{},audio);std::vector<std::uint8_t> rvv(36);std::memcpy(rvv.data(),"RVV1",4);
   for(const auto [at,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{4,1},{8,1},{12,30},{16,1},{20,1},{24,8000},{28,2},{32,0}})std::memcpy(rvv.data()+at,&v,4);
   const unsigned n=4;rvv.resize(44);std::memcpy(rvv.data()+36,&n,4);rvv[40]=0xff;rvv[41]=0xd8;rvv[42]=0xff;rvv[43]=0xd9;
   movie.load(rvv);check(movie.active(),"movie started");equal(installed_back_color(),0,"system.c:1141 bhSysCallMovie");}
  // system.c:2123: the terminal subtask (cb_flg 0x400000 -> mn_mode 7) installs 0 when it takes the screen.
  {set_back_color(0x00123456);std::vector<std::uint8_t> raw(1500);const float version=1.73f;std::memcpy(raw.data(),&version,4);
   for(const auto [at,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{16,64},{28,1100},{102,1400},{1100,16},{1104,18},{1108,80},{1112,110}})std::memcpy(raw.data()+at,&v,4);
   Inventory inv;MessageDialog dialog;EventVm ev(raw,inv,dialog);ComputerTerminal terminal(data);check(!terminal.begin(ev),"no request, no terminal");equal(installed_back_color(),0x00123456,"terminal without request keeps Db.clear");
   ev.flags[10][0]|=0x400000u;ev.com_number=0;check(terminal.begin(ev),"bhEvtComSet request starts the terminal");equal(installed_back_color(),0,"system.c:2123 terminal");}
  // Clear alpha above 128 is kept raw (GS RGBAQ byte) and reported by the room while it draws over it.
  set_back_color(0xff123456);equal(a.rendered_clear(),0xff123456,"A>128 keeps RGB, host alpha saturates");
  check(a.world.unbound_enemy_routines().count("njSetBackColor: clear alpha >128 exceeds native destination-alpha domain"),"A>128 destination-alpha gap reported");
  set_back_color(0);
 }
 std::cout<<"Back colour table, native binding, EOR timer, camera lifecycle and global Db.clear owner PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
