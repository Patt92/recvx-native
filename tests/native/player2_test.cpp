#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "room.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static bool same(NJS_POINT3 a,NJS_POINT3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static void routing(){
    check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL init failed");
    const auto attach=[](){int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);check(index>=0&&SDL_IsGameController(index),"virtual pad failed");return index;};
    const int first=attach(),second=attach();auto* pad1=SDL_JoystickOpen(first);auto* pad2=SDL_JoystickOpen(second);
    check(pad1&&pad2,"virtual pad open failed");
    {
        InputManager one(InputConfig::defaults(),{true,0}),two(InputConfig::defaults(),{false,1});
        auto pump=[&]{SDL_GameControllerUpdate();SDL_Event e;while(SDL_PollEvent(&e)){one.event(e);two.event(e);}};
        auto button=[&](SDL_Joystick* pad,SDL_GameControllerButton b,bool on){check(SDL_JoystickSetVirtualButton(pad,b,on)==0,"virtual button failed");pump();};
        pump();one.tick();two.tick();
        button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,true);
        check(!one.tick().down(Action::Forward)&&two.tick().hit(Action::Forward),"pad 2 leaks into Player 1 or loses edge");
        button(pad1,SDL_CONTROLLER_BUTTON_DPAD_LEFT,true);
        check(one.tick().turn()==-1&&two.tick().turn()==0,"pad 1 leaks into Player 2");
        SDL_Event key{};key.type=SDL_KEYDOWN;key.key.keysym.scancode=SDL_SCANCODE_S;one.event(key);two.event(key);
        check(one.tick().down(Action::Back)&&!two.tick().down(Action::Back),"keyboard routing failed");
        button(pad2,SDL_CONTROLLER_BUTTON_A,true);
        check(!one.tick().down(Action::Confirm)&&two.tick().hit(Action::Confirm),"Player 2 menu button leaks into Player 1");
        // A disconnected Player 1 never promotes the held pad 2 into slot 0.
        SDL_JoystickClose(pad1);pad1=nullptr;check(SDL_JoystickDetachVirtual(first)==0,"detach pad 1 failed");pump();
        check(!one.tick().down(Action::Forward)&&two.tick().down(Action::Forward),"pad 2 promoted after pad 1 removal");
        const int replacement=attach();auto* replug=SDL_JoystickOpen(replacement);pump();
        button(replug,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,true);
        check(one.tick().turn()==1&&two.tick().turn()==0,"replacement does not fill free slot 0");
        two.suppress_held();check(!two.tick().down(Action::Forward),"held suppression failed on Player 2");
        button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,false);two.tick();button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,true);
        check(two.tick().hit(Action::Forward),"Player 2 release/repress lost after suppression");
        SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;one.event(focus);two.event(focus);
        check(!two.tick().down(Action::Forward)&&!one.tick().down(Action::Back),"focus loss retains input");
        focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;one.event(focus);two.event(focus);two.tick();
        button(pad2,SDL_CONTROLLER_BUTTON_DPAD_UP,false);button(pad2,SDL_CONTROLLER_BUTTON_A,false);two.tick();
        SDL_JoystickClose(replug);check(SDL_JoystickDetachVirtual(replacement)==0,"replacement detach failed");pump();
        SDL_JoystickClose(pad2);pad2=nullptr;check(SDL_JoystickDetachVirtual(0)==0,"pad 2 detach failed");pump();
        check(!two.tick().down(Action::Forward)&&!two.tick().hit(Action::Confirm),"unplug retains Player 2 buttons");
    }
    SDL_Quit();std::cout<<"Player 2 independent keyboard/controller routing, hotplug, focus and suppression PASS\n";
}
static void world_actor(const std::filesystem::path& data){
    _Make_SinTable();const auto bytes=RoomArchive(data).load("RM_0080.RDX",64*1024*1024);const auto scene=decode_scene(bytes);
    auto primary=scene.spawns.at(0);const auto original=primary;std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    auto asset=std::make_shared<PlayerAsset>(PlayerAsset::load(data,11));
    RoomWorld world(bytes,scene,primary,motion,frame,audio,data,{},"RM_0080");
    check(!world.player2_state()&&world.player2_mode()==Player2Mode::Off,"Player 2 not off by default");
    world.enable_player2(Player2Mode::Local,asset,primary,1);
    check(world.player2_state()->own_models.at(0).calculated,"Player 2 has no initial committed tree");
    const auto start=world.player2_state()->transform.position;
    for(unsigned i=0;i<12;++i)world.tick_player2({1,0,false},true);
    const auto* p=world.player2_state();
    check(!same(start,p->transform.position),"Player 2 native walk did not advance");
    check(p->motion==0&&p->frame>0,"Player 2 did not enter the original healthy unarmed walk clip");
    check(same(primary.position,original.position)&&primary.yaw==original.yaw&&motion==42&&frame==0,"Player 2 changed Player 1");
    const auto position=p->transform.position;const auto clock=p->frame;const auto matrices=p->own_models.at(0).owp_mtx;
    for(unsigned i=0;i<5;++i)world.tick_player2({1,1,true},false);
    Scene draw1=scene;world.append(draw1);Scene draw2=scene;world.append(draw2);
    check(p->frame==clock&&same(p->transform.position,position),"paused Player 2 advanced");
    check(draw1.triangles.size()==draw2.triangles.size(),"repeat draw changes Player 2 geometry");
    for(unsigned n=0;n<matrices.size();++n)for(unsigned j=0;j<16;++j)check(p->own_models.at(0).owp_mtx[n].value[j]==matrices[n].value[j],"draw rebuilt the paused tree");
    world.set_player2_mode(Player2Mode::Off);Scene off=scene;world.append(off);
    check(off.triangles.size()<draw1.triangles.size(),"disabled Player 2 still draws");
    world.tick_player2({1,0,false},true);check(p->frame==clock,"disabled Player 2 still ticks");
    world.set_player2_mode(Player2Mode::Npc);unsigned driver_calls=0;
    world.set_player2_driver([&](const RoomWorld& owner){check(&owner==&world&&owner.player2_mode()==Player2Mode::Npc,"NPC driver sees wrong World");++driver_calls;return Player2Command{0,1,false};});
    const auto yaw=p->transform.yaw;for(unsigned i=0;i<12;++i)world.tick_player2({-1,-1,true},true);
    check(driver_calls==12&&p->transform.yaw!=yaw,"NPC source did not drive the shared action path");
    world.tick_player2({},false);check(driver_calls==12,"NPC driver called while paused");
    world.set_player2_mode(Player2Mode::Local);world.tick_player2({},true);check(driver_calls==12,"NPC driver called in local mode");
    world.set_player2_driver({});world.set_player2_mode(Player2Mode::Npc);world.tick_player2({},true);
    check(world.player2_gaps().count("NPC driver not bound"),"missing NPC driver hidden");
    // Re-entry allocates a new room-local actor and releases its previous tree.
    world.enable_player2(Player2Mode::Local,asset,primary,1);
    check(same(world.player2_state()->transform.position,start)&&world.player2_state()->frame==0,"Player 2 reinitialization retains prior room state");
    world.enable_player2(Player2Mode::Off,{},{});check(!world.player2_state(),"Player 2 release retains its work");
    std::cout<<"Player 2 native walk, independent state, committed pose, mode switch, NPC driver and re-entry PASS\n";
}
int main(int argc,char** argv){try{SDL_SetMainReady();routing();check(argc==2,"data path required");world_actor(argv[1]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
