/* bhSetEasyDirLight, light.c:279-328, SLUS_201.84 0x2810e0..0x28126c.
   No original source is compiled or executed. Owner tests require the separate
   archived/work/easy_dir_light.integration.patch (control-call side effects ONLY).
   Draw sequence tests below are explicitly supplied calls, NOT a scheduler or
   a claim that RoomWorld::append already reproduces the original draw order.

   One-line counterchecks, one at a time, then restore:
   - easy_dir_light.h: intensity*3.0f -> intensity               => static_assert "100 first call x3"
   - easy_dir_light.h: mode==1u -> mode==2u                     => static_assert "100 second call clears only bit 2"
   - easy_dir_light.h: !(flags&2u) -> (flags&1u)==0              => static_assert "bit 1 is not an enable gate"
   - easy_dir_light.h: clock_bits>16383u -> clock_bits>16384u    => "101 exact threshold"
   - easy_dir_light.h: pulse=3.0f*njCos(angle) -> intensity*...   => "101 ignores it and samples old ct0"
   - integration: guard step_easy_dir_light with sp_flg 0x80    => "owner advances while simulation paused"
   - integration: remove step_easy_dir_light control call       => "owner advances while simulation paused"
*/
#include "easy_dir_light.h"
#include "world.h"
#include "asset_io.h"
#include "core_random.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;

// These assertions are executed by the C++17 constant evaluator even with
// -fsyntax-only; they do not require or execute the original program or a test binary.
static constexpr bool type100_first(){
    std::uint32_t flags=0xa2u,mode=0,clock=77;
    return step_easy_dir_light(.6f,100,8,flags,mode,clock)==.6f*3.f&&
        flags==0xa2u&&mode==1&&clock==77;
}
static constexpr bool type100_second(){
    std::uint32_t flags=0xa3u,mode=1,clock=77;
    return step_easy_dir_light(.6f,100,8,flags,mode,clock)==.6f&&
        flags==0xa1u&&mode==1&&clock==77;
}
static constexpr bool bit2_only(){
    std::uint32_t flags=2,mode=0,clock=77;
    return step_easy_dir_light(1.f,100,8,flags,mode,clock)==3.f&&mode==1&&flags==2;
}
static constexpr bool inactive(){
    std::uint32_t flags=0xa1u,mode=0,clock=77;
    return step_easy_dir_light(.6f,100,8,flags,mode,clock)==.6f&&flags==0xa1u&&mode==0&&clock==77;
}
static constexpr bool other_mode(){
    std::uint32_t flags=3,mode=2,clock=77;
    return step_easy_dir_light(.6f,100,8,flags,mode,clock)==.6f&&flags==3&&mode==2&&clock==77;
}
static constexpr bool other_type(){
    std::uint32_t flags=3,mode=0,clock=77;
    return step_easy_dir_light(.6f,13,8,flags,mode,clock)==.6f&&flags==3&&mode==0&&clock==77;
}
static_assert(type100_first(),"100 first call x3");
static_assert(type100_second(),"100 second call clears only bit 2");
static_assert(bit2_only(),"bit 1 is not an enable gate");
static_assert(inactive(),"inactive slot must not advance");
static_assert(other_mode(),"100 has no fallback for mode 2");
static_assert(other_type(),"other type is unchanged");

static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static bool close(float a,float b){return std::fabs(a-b)<2e-5f;}
static bool same(NJS_POINT3 a,NJS_POINT3 b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
// Independent reference to the Ninja table lookup, not a second production
// implementation: ps2_NaMath.c:21-24/96-166, native/ninja_math.c. The table is
// quarter-wave with endpoint 16383, not analytic cos(2*pi*n/65536).
static float cosine_ref(std::uint32_t angle){
    std::uint32_t i=angle&0x3fffu;
    if(!(angle&0x4000u))i=16383u-i;
    const float v=std::sin(0.0000958738f*float(i));
    return ((angle^(angle<<1))&0x8000u)?-v:v;
}
static void type101(){
    std::uint32_t flags=0xa2u,mode=73,clock=0;
    // aspd 8 -> 2048 per actual call, no input factor; sample precedes advance.
    for(unsigned call=0;call<8;++call){
        const auto old=clock;
        const float got=step_easy_dir_light(.17f,101,8,flags,mode,clock);
        check(close(got,3.f*cosine_ref(old)),"101 ignores it and samples old ct0");
        check(clock==(call+1)*2048&&mode==73,"101 advances ct0 only, leaves mode");
        check(flags==(call==7?0xa0u:0xa2u),"101 exact threshold");
    }
    check(step_easy_dir_light(.17f,101,8,flags,mode,clock)==.17f&&clock==16384,
          "101 call after clear returns it without a step");
    flags=3;clock=16383;
    check(close(step_easy_dir_light(0,101,0,flags,mode,clock),0)&&flags==3&&clock==16383,
          "101 equal 16383 remains enabled");
    step_easy_dir_light(0,101,1,flags,mode,clock);
    check(flags==1&&clock==16639,"101 boundary is tested after advance");
    flags=3;clock=0;
    for(int i=0;i<3;++i)check(close(step_easy_dir_light(.4f,101,0,flags,mode,clock),3.f*cosine_ref(0))&&clock==0&&flags==3,
                              "101 zero speed still ignores it");
    // aspd and ct0 are signed ints in LGT_WORK (types.h:385/413), carried
    // as unsigned bits by the existing owner. ADDU/SLL + signed SLTI in ELF.
    flags=3;clock=0;
    step_easy_dir_light(1,101,0xffffffffu,flags,mode,clock);
    check(clock==0xffffff00u&&flags==3,"101 negative speed signed comparison");
    check(close(step_easy_dir_light(.4f,101,1,flags,mode,clock),3.f*cosine_ref(0xffffff00u))&&clock==0&&flags==3,
          "101 negative old angle");
    clock=0x7fffff80u;
    step_easy_dir_light(1,101,1,flags,mode,clock);
    check(clock==0x80000080u&&flags==3,"101 ct0 wraps 32 bits then signed threshold");
    clock=0x4000;flags=3;
    check(close(step_easy_dir_light(.4f,101,1,flags,mode,clock),3.f*cosine_ref(0x4000))&&flags==1,
          "101 no pre-sample threshold shortcut");
}
static void explicit_calls(){
    // Hypothetical admitted calls supplied individually, not a draw count.
    // light.c:369 control, objitm.c:507 full, :520 half. Type 100's first
    // flash is consumed by control, so the following full call does NOT flash.
    std::uint32_t flags=3,mode=0,clock=0;
    check(step_easy_dir_light(1,100,0,flags,mode,clock)==3,"control consumes first flash");
    check(step_easy_dir_light(1,100,0,flags,mode,clock)==1&&flags==1,"next setLight clears flash");
    check(step_easy_dir_light(.6f,100,0,flags,mode,clock)==.6f,"later half is baseline");
    flags=3;mode=0;clock=0;
    // No monitor/reflection API is exercised here: proving the absence of a
    // step there needs an integrated draw-order test, not a local helper test.
    // Two individually admitted calls in the same host image must NOT be
    // collapsed to one tick; no renderer API or fixed frame number is involved.
    const float a=step_easy_dir_light(.6f,101,8,flags,mode,clock);
    const float b=step_easy_dir_light(.6f,101,8,flags,mode,clock);
    check(close(a,3.f*cosine_ref(0))&&close(b,3.f*cosine_ref(2048))&&clock==4096,
          "distinct invocations cannot share one sampled value");
    // A monitor exit's bhSetLight (screen.c:1042) and a mirror outer
    // bhSetLight (game.c:85) are real calls, unlike their inner Ninja draws.
    step_easy_dir_light(1,101,8,flags,mode,clock);
    step_easy_dir_light(1,101,8,flags,mode,clock);
    check(clock==8192,"explicit monitor restore and mirror outer calls advance independently");
    recvx_core_random_seed(123);const auto next=recvx_core_random_next();recvx_core_random_seed(123);
    step_easy_dir_light(.6f,101,8,flags,mode,clock);
    check(recvx_core_random_next()==next,"easy dir must not consume random values");
}
static void owner_control(const std::filesystem::path& data){
    // RM_1020 has an active type-9 room light in slot 13. No motion, AI,
    // effect, audio command or renderer is run; call control_lights directly.
    const auto room=read_asset_file(data/"rooms/RM_1020.RDT",64*1024*1024);
    const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_1020");
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);
    world.bind_player_collision(events);events.flags[11][0]|=0x80u;
    world.set_simulation_flags(0);
    const auto lamp=world.light_state(13);check(bool(lamp),"room type 9 sentinel exists");
    const auto event0=world.light_state(0,true);
    RECVX_CB_LIGHT gun{};gun.flg=3;gun.type=101;gun.aspd=64;gun.lsrc=4;gun.r=.23f;gun.g=.47f;gun.b=.71f;gun.nr=5;gun.fr=20;
    world.set_gun_light(gun);world.control_lights();
    check(world.light_state(0)->clock==16384&&world.light_state(0)->flags==1,
          "first owner 101 call clears bit 2");
    check(world.unbound_enemy_routines().count("bhSetEasyDirLight: Zeichenaufruffolge/Einzellichtzustand Typ 100/101 offen")!=0,
          "cleared bit must not hide the open draw binding");
    gun.flg=2;gun.type=100;gun.aspd=0;
    world.set_gun_light(gun);world.control_lights();
    auto p=world.light_state(0);
    check(p&&p->mode==1&&p->flags==2&&p->clock==0,"owner advances while simulation paused");
    check(same(p->color,{.23f,.47f,.71f}),"easy control must not write RGB or cached wRGB");
    const auto after=world.light_state(13);
    check(after->clock==lamp->clock&&after->working_angles==lamp->working_angles&&same(after->color,lamp->color),
          "gm_flg 0x80 must skip every other light");
    if(event0){const auto e=world.light_state(0,true);check(e&&e->flags==event0->flags&&e->clock==event0->clock&&e->mode==event0->mode,
                                                        "easy control selects rom lgtp, never evlp");}
    world.control_lights();p=world.light_state(0);
    check(p->flags==0&&p->mode==1,"second control call advances within same host frame");
    world.control_lights();check(world.light_state(0)->mode==1&&world.light_state(0)->flags==0,"cleared owner remains off");
    world.set_gun_light(gun);check(world.light_state(0)->mode==0,"new muzzle resets the existing owner mode");
    world.control_lights();check(world.light_state(0)->mode==1,"reentry consumes the new flash");
    gun.type=101;gun.aspd=64;gun.flg=3;world.set_gun_light(gun);world.control_lights();p=world.light_state(0);
    check(p->clock==16384&&p->flags==1&&same(p->color,{.23f,.47f,.71f}),"owner 101 threshold and no colour write");
    gun.type=100;gun.aspd=0;gun.flg=3;world.set_gun_light(gun);
    events.flags[11][0]&=~0x80u;
    world.control_lights();check(world.light_state(0)->mode==0&&world.light_state(0)->flags==3,
                                 "normal paused control must not use the easy step");
    world.set_simulation_flags(0x80u);world.control_lights();
    check(world.light_state(0)->mode==1&&world.light_state(0)->flags==3,"normal 100 control is preserved");
    world.control_lights();check(world.light_state(0)->flags==1,"normal second 100 control is preserved");
}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");_Make_SinTable();
    type101();explicit_calls();owner_control(argv[1]);
    std::cout<<"PASS: easy-dir single-call semantics and control-call side effects (draw scheduling still open)\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
