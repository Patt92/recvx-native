/* Lichttypen 7..12 aus bhControlLight (light.c:486-657) auf dem nativen Raumlichtbesitzer.
   Erwartungen kommen aus den Originalformeln, der Disassembly von SLUS_201.84 (bhControlLight
   0x00281270) und den w*-Feldern der Raumdateien, die das Original nach einem Pass
   gespeichert hat; nicht aus dem nativen Code.

   Gegenprobe (jeweils eine Aenderung, Test muss rot werden, danach zurueck und touch):
   - effect_sprites.cpp step_light_type Typ 8/9: way zusaetzlich & 0xFFFF       -> "way unmasked"
   - Typ 9 auf -step statt +step                                               -> "type 9 turns"
   - Typ 12: gain nach dem Umschalten statt davor                               -> "type 12 sample"
   - Typ 10: mode 2 -> 1 statt -> 3                                             -> "type 10 second dim"
   - world.cpp animate_light: sp_flg-0x80-Tor entfernen                         -> "paused light"
   - world.cpp: gm_flg-0x80-Ruecksprung entfernen                                -> "gm_flg 0x80"
   - world.cpp read_lights: w* nicht aus der Raumdatei                           -> "file w*"
   - world.cpp control_lights als leere Huelle (Lichtschritt nur in tick)          -> "bhInitCamSet: bhControlLight"
   - world.cpp link_work_active: Rueckgabe immer true                              -> "link death" */
#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "core_random.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<=eps;}
static bool same(NJS_POINT3 a,NJS_POINT3 b,float eps=1e-4f){return close(a.x,b.x,eps)&&close(a.y,b.y,eps)&&close(a.z,b.z,eps);}
// Unabhaengige Bezugsrechnung: Kreis = 65536, (0,0,-1) um Y gedreht.
static float ref_sin(long long a){return float(std::sin(double(a&0xffff)*6.283185307179586/65536.0));}
static NJS_POINT3 ref_yaw_vector(std::int32_t way){return {-ref_sin(way),0.f,-ref_sin(std::int64_t(way)+16384)};}
// recvx_core_random: dieselbe LCG wie der EE-rand() (vgl. lighter_test.cpp light_states).
struct Lcg{std::uint32_t s;int next(){s=s*0x41c64e6du+0x3039u;return int(s&0x7fffffff);}float unit(){return -next()/-2.1474836e9f;}};

static void type7(){
    // light.c:487 fabsf(njSin(sys->thunder << 6)); thunder ist unsigned (types.h:740, bhEff105 fptoui).
    for(unsigned thunder:{0u,64u,100u,255u}){LightTypeWork w;NJS_POINT3 c{9,9,9};NJS_VECTOR d{1,2,3};
        check(step_light_type(7,57,{4,.3f,0},thunder,w,c,d),"type 7 not handled");
        const float f=std::fabs(ref_sin(std::int64_t(thunder)<<6));
        check(same(c,{f,f,f},2e-3f)&&same(d,{1,2,3}),"type 7 is not |sin(thunder << 6)| on all channels");}
}
static void type8_9(){
    // RM_1020 Raumlicht 13 (Typ 9, aspd 30): das Original hat way = 944230564 und den daraus
    // berechneten Vektor (0.91775, 0, -0.39716) gespeichert; way ist also nicht 16-bit gekuerzt.
    LightTypeWork w;w.angles={0,944230564,0};NJS_POINT3 c{};NJS_VECTOR d{};
    check(step_light_type(9,0,{3,0,0},0,w,c,d)&&w.angles[1]==944230564,"aspd 0 must not turn");
    check(same(d,{0.9177485f,0.f,-0.3971620f},2e-3f),"bhGetLightVector differs from the stored RM_1020 wv");
    const int step=int(182.04445f*(0.5f*30.f))&0xffff;check(step==2730,"aspd 30 step");
    step_light_type(9,30,{3,0,0},0,w,c,d);
    check(w.angles[1]==944230564+2730,"way unmasked: type 9 turns by +step without & 0xFFFF on way");
    check(same(d,ref_yaw_vector(w.angles[1]),2e-3f)&&same(c,{3,0,0}),"type 9 direction/colour");
    LightTypeWork e;e.angles={0,0,0};step_light_type(8,30,{3,0,0},0,e,c,d);
    check(e.angles[1]==-2730,"type 8 turns by -step");check(same(d,ref_yaw_vector(-2730),2e-3f),"type 8 direction");
}
static void type10(){
    // RM_0020 Raumlicht 34 (Typ 10, rgb 1): gespeicherte spc/w* 0.6665 = ein Abklingschritt.
    const NJS_POINT3 color{5,.6f,0};LightTypeWork w;NJS_POINT3 c{};NJS_VECTOR d{};recvx_core_random_seed(3);Lcg r{3};
    step_light_type(10,100,color,0,w,c,d);check(w.mode==1&&same(c,color),"type 10 mode 0");
    step_light_type(10,100,color,0,w,c,d);r.next();const int k1=int(6.0f*r.unit())+3;
    check(w.mode==2&&int(w.clock)==k1&&same(c,color),"type 10 mode 1 with aspd 100 must start a dim run");
    NJS_POINT3 expect=color;
    for(int i=0;i<k1;++i){expect={expect.x+.5f*(.333f*color.x-expect.x),expect.y+.5f*(.333f*color.y-expect.y),expect.z+.5f*(.333f*color.z-expect.z)};
        step_light_type(10,100,color,0,w,c,d);if(i+1<k1)check(same(c,expect)&&w.mode==2,"type 10 dim step");}
    const int k2=int(6.0f*r.unit())+3;check(w.mode==3&&int(w.clock)==k2&&same(c,color),"type 10 second dim: mode 2 must reset and go to 3");
    for(int i=0;i<k2;++i)step_light_type(10,100,color,0,w,c,d);
    check(w.mode==1&&same(c,color),"type 10 mode 3 returns to 1");
    check(recvx_core_random_next()==r.next(),"type 10 consumes a different number of rand() calls");
    LightTypeWork one;one.mode=2;one.clock=5;one.flicker={1,1,1};step_light_type(10,0,{1,1,1},0,one,c,d);
    check(same(c,{.6665f,.6665f,.6665f}),"one dim step differs from the stored RM_0020 light 34 value");
    // aspd 0: (-rand()/-2^31) <= 0.0 nie, Typ 10 bleibt in mode 1 und verbraucht je Pass einen Wert.
    LightTypeWork idle;idle.mode=1;recvx_core_random_seed(9);Lcg q{9};
    for(int i=0;i<5;++i){step_light_type(10,0,color,0,idle,c,d);q.next();check(idle.mode==1&&same(c,color),"type 10 aspd 0 idle");}
    check(recvx_core_random_next()==q.next(),"type 10 idle rand count");
}
static void type11(){
    const NJS_POINT3 color{1,.1f,0};LightTypeWork w;NJS_POINT3 c{};NJS_VECTOR d{};recvx_core_random_seed(5);Lcg r{5};
    step_light_type(11,0,color,0,w,c,d);check(w.mode==1&&same(c,color),"type 11 mode 0");
    NJS_POINT3 expect=color;
    for(int i=0;i<4;++i){step_light_type(11,0,color,0,w,c,d);r.next();expect={expect.x+.5f*(.333f*color.x-expect.x),expect.y+.5f*(.333f*color.y-expect.y),expect.z};
        check(w.mode==1&&same(c,expect),"type 11 mode 1 dims every pass");}
    step_light_type(11,100,color,0,w,c,d);r.next();const int k=int(6.0f*r.unit())+3;
    check(w.mode==2&&int(w.clock)==k&&same(c,color),"type 11 flash resets to full colour");
    for(int i=0;i<k;++i)step_light_type(11,100,color,0,w,c,d);
    check(w.mode==1&&same(c,color),"type 11 mode 2 ends in mode 1 with full colour");
    check(recvx_core_random_next()==r.next(),"type 11 rand count");
}
static void type12(){
    // aspd 1: ct0 = (int)(1 * r) + 1 = 1, also Wechsel in jedem Pass, ein rand() je Pass.
    const NJS_POINT3 color{1,.5f,0};LightTypeWork w;NJS_POINT3 c{};NJS_VECTOR d{};recvx_core_random_seed(11);Lcg r{11};
    for(int i=0;i<6;++i){const bool half=w.mode==1;step_light_type(12,1,color,0,w,c,d);r.next();
        check(same(c,half?NJS_POINT3{.5f,.25f,0}:color),"type 12 sample: colour belongs to the mode before the switch");
        check(w.clock==1u&&w.mode==(half?0u:1u),"type 12 alternation");}
    check(recvx_core_random_next()==r.next(),"type 12 rand count");
    LightTypeWork v;v.mode=0;v.clock=3;recvx_core_random_seed(2);step_light_type(12,2,color,0,v,c,d);step_light_type(12,2,color,0,v,c,d);
    check(v.mode==0&&v.clock==1u,"type 12 counts ct0 down without rand");
    LightTypeWork other;other.mode=4;other.clock=7;step_light_type(12,2,color,0,other,c,d);check(other.mode==4&&other.clock==7u&&same(c,color),"type 12 other modes keep r/g/b");
    check(!step_light_type(13,2,color,0,other,c,d)&&!step_light_type(6,2,color,0,other,c,d),"only types 7..12 belong here");
}
struct Room{std::vector<std::uint8_t> bytes;Scene stage;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;};
static void rooms(const std::filesystem::path& data){
    auto player=PlayerAsset::load(data,10);
    const auto open=[&](const char* name,auto&& body){
        Room r;r.bytes=read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024);r.stage=decode_scene(r.bytes);r.actor=r.stage.spawns.at(0);
        AudioMixer audio(data);RoomWorld world(r.bytes,r.stage,r.actor,r.motion,r.frame,audio,data,{},name);world.set_player_asset(&player);
        Inventory inventory;MessageDialog dialog;EventVm events(r.bytes,inventory,dialog);world.bind_player_collision(events);
        std::vector<BoneMatrix> nodes;(void)world.sample_player(player,&nodes);world.commit_player_joints(player.script_nodes(nodes));
        body(world,events);};
    open("RM_0020",[&](RoomWorld& world,EventVm&){
        // bhInitLight (light.c:178-192) nullt mode/ct0, nicht w*. Unter sp_flg ohne 0x80 liest
        // bhControlLight wr/wg/wb (:703-717): die gespeicherten Werte der Raumdatei.
        world.set_simulation_flags(0);world.tick();auto l=world.light_state(11);
        check(l&&l->type==10&&l->mode==0&&l->clock==0,"room light 11 not reset by bhInitLight");
        check(same(l->color,{2.0818748f,0.2498249f,0.f})&&same(l->position,{90.37957f,.692993f,109.21885f}),"file w*: paused light must draw the stored wr/wg/wb and wpx");
        world.set_simulation_flags(~0u);effect_weather().thunder=100;world.tick();l=world.light_state(11);
        check(l->mode==1&&same(l->color,{5.f,.6f,0.f},1e-3f),"type 10 mode 0 on the room owner");
        const auto t=world.light_state(13);const unsigned thunder=effect_weather().thunder;
        check(t&&t->type==7&&thunder==100u,"RM_0020 light 13 type 7 / thunder rewritten in between");
        const float f=std::fabs(ref_sin(std::int64_t(thunder)<<6));check(same(t->color,{f,f,f},2e-3f),"type 7 room light does not read sys->thunder");
        effect_weather().thunder=0;});
    open("RM_1020",[&](RoomWorld& world,EventVm& events){
        auto l=world.light_state(13);check(l&&l->type==9&&l->speed==30&&l->working_angles[1]==944230564,"file wax/way/waz not loaded");
        world.tick();l=world.light_state(13);
        check(l->working_angles[1]==944230564+2730&&same(l->direction,ref_yaw_vector(944230564+2730),2e-3f),"type 9 turns the room light once per pass");
        world.set_simulation_flags(~0u&~0x80u);world.tick();check(world.light_state(13)->working_angles[1]==944230564+2730,"paused light: sp_flg 0x80 gate");
        world.set_simulation_flags(~0u);events.flags[11][0]|=0x80u;world.tick();
        check(world.light_state(13)->working_angles[1]==944230564+2730,"gm_flg 0x80: bhControlLight must return after bhSetEasyDirLight");
        events.flags[11][0]&=~0x80u;world.tick();check(world.light_state(13)->working_angles[1]==944230564+2*2730,"light does not resume after gm_flg 0x80");});
    open("RM_1020",[&](RoomWorld& world,EventVm&){
        // event.c:4745 bhInitCamSet (Opcode 0x5b) ruft bhControlLight nach bhControlCamera direkt:
        // das Licht rueckt in diesem Bild einen Takt zusaetzlich zum Pass game.c:59 weiter.
        world.tick();check(world.light_state(13)->working_angles[1]==944230564+2730,"one pass, one step");
        world.control_lights();check(world.light_state(13)->working_angles[1]==944230564+2*2730,"bhInitCamSet: bhControlLight ausserhalb des Passes rueckt das Licht nicht weiter");
        world.tick();check(world.light_state(13)->working_angles[1]==944230564+3*2730,"pass after the extra bhControlLight");});
    open("RM_3230",[&](RoomWorld& world,EventVm&){
        // light.c:719-748: Raumlicht 5 (flg 0x3, lkflg 3 = obwp[7], lkono 0) haengt am Objekt 7. Ist das
        // Zielwerk nicht aktiv (!(pwp->flg & 0x1)), loescht bhControlLight flg 0x3 dauerhaft. Das Werk-flg
        // hat im Original nach bhSetObject keinen Schreiber; die Naht set_work_flags stellt den Fall nach.
        auto l=world.light_state(5);check(l&&l->link==3&&l->index==7&&(l->flags&3u)==3u,"RM_3230 light 5 is the obwp[7]-linked light");
        const auto* o=world.state(2,7);check(o&&(o->object_flags&1u),"obwp[7] active");
        world.set_simulation_flags(~0u);world.tick();l=world.light_state(5);check((l->flags&3u)==3u,"active link keeps the light");
        world.set_work_flags(2,7,o->object_flags&~1u);world.tick();l=world.light_state(5);
        check((l->flags&3u)==0u,"link death: light linked to an inactive work must drop flg 0x3");
        world.set_work_flags(2,7,o->object_flags|1u);world.tick();check((world.light_state(5)->flags&3u)==0u,"link death is permanent (no relight)");});
    open("RM_6070",[&](RoomWorld& world,EventVm&){
        // evlp wird von bhInitLight nicht beruehrt: mode/ct0 der Datei bleiben stehen.
        const auto e=world.light_state(1,true);check(e&&e->type==12&&e->speed==2&&e->mode==0&&e->clock==2,"event light keeps its stored mode/ct0");});
}
int main(int argc,char** argv){try{check(argc==2,"original data directory required");_Make_SinTable();
    type7();type8_9();type10();type11();type12();rooms(argv[1]);std::cout<<"PASS: light.c types 7..12\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
