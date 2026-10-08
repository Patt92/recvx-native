#define SDL_MAIN_HANDLED
#include "frontend.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static SDL_Event key(SDL_Scancode code,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=code;return e;}
struct Quit{~Quit(){SDL_Quit();}};
int main(int argc,char** argv){try{
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL init");Quit quit;_Make_SinTable();
    const auto path=std::filesystem::temp_directory_path()/("recvx-frontend-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");auto config=InputConfig::defaults();config.save(path);InputManager devices(config);Frontend front(config,path);front.changed=[&](const InputConfig& value){devices.configure(value);};
    auto event=[&](const SDL_Event& e){devices.event(e);front.event(e);};auto tick=[&]{return front.apply(devices.tick());};
    auto press=[&](SDL_Scancode code){event(key(code,true));const auto result=tick();event(key(code,false));tick();return result;};
    check(front.active()&&!front.settings(),"default main menu missing");press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_RETURN);check(front.settings()&&!front.gamepad_page(),"keyboard settings inaccessible");
    for(unsigned i=0;i<12;++i)front.input(MenuInput::Down);front.input(MenuInput::Confirm);check(front.capturing(),"keyboard rebinding not armed");press(SDL_SCANCODE_F9);
    check(!front.capturing()&&config.describe(Action::Skip,false)=="F9"&&config.describe(Action::Skip,true)=="Start/Options","keyboard rebind changed wrong device");
    check(InputConfig::read(path).describe(Action::Skip,false)=="F9","keyboard rebind not persistent");
    for(unsigned i=0;i<6;++i)front.input(MenuInput::Up);front.input(MenuInput::Confirm);press(SDL_SCANCODE_E);check(config.describe(Action::Confirm,false)=="E","confirm remap failed");
    front.input(MenuInput::Right);check(front.gamepad_page(),"controller settings page inaccessible");
    const int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);check(index>=0,"virtual controller attach");auto* joystick=SDL_JoystickOpen(index);check(joystick,"virtual controller open");
    auto pump=[&]{SDL_GameControllerUpdate();SDL_Event e;while(SDL_PollEvent(&e))event(e);};pump();tick();
    front.input(MenuInput::Confirm);SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_X,1);pump();tick();
    check(!front.capturing()&&config.describe(Action::Confirm,true)=="X"&&config.describe(Action::Confirm,false)=="E","controller rebind failed");SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_X,0);pump();tick();
    // The live InputManager must use the edited controller binding immediately.
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,1);pump();check(!devices.tick().hit(Action::Confirm),"old A confirm remains bound");SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,0);pump();devices.tick();
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_X,1);pump();check(devices.tick().hit(Action::Confirm),"new X confirm not live");SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_X,0);pump();devices.tick();
    front.input(MenuInput::Confirm);SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_B,1);pump();tick();
    check(config.describe(Action::Confirm,true)=="B"&&config.describe(Action::Cancel,true)=="X","confirm/cancel conflict does not swap old binding");SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_B,0);pump();tick();
    for(unsigned i=6;i<action_count;++i)front.input(MenuInput::Down);front.input(MenuInput::Right);check(config.deadzone>.29f&&InputConfig::read(path).deadzone>.29f,"deadzone adjustment not saved");
    front.input(MenuInput::Down);front.input(MenuInput::Left);check(config.menu_threshold>=config.deadzone,"threshold below deadzone");
    // Cancel capture must not also exit the settings page.
    for(unsigned i=0;i<4;++i)front.input(MenuInput::Up);front.input(MenuInput::Confirm);press(SDL_SCANCODE_ESCAPE);check(front.settings()&&!front.capturing(),"capture cancel exits settings");
    for(unsigned i=action_count-3;i<action_count+2;++i)front.input(MenuInput::Down);front.input(MenuInput::Confirm);check(config.describe(Action::Confirm,false)=="Enter"&&config.describe(Action::Confirm,true)=="A","default restoration failed");
    press(SDL_SCANCODE_ESCAPE);check(front.active()&&!front.settings(),"Escape quits instead of returning to main menu");press(SDL_SCANCODE_UP);check(press(SDL_SCANCODE_RETURN)==FrontendCommand::TitleCall&&front.starting(),"new game must start the title call");{FrontendCommand r=FrontendCommand::None;for(int i=0;i<200&&r!=FrontendCommand::NewGame;++i)r=tick();check(r==FrontendCommand::NewGame&&!front.active(),"new game command missing after title fade");}
    SDL_JoystickClose(joystick);SDL_JoystickDetachVirtual(index);std::filesystem::remove(path);
    // Video routing must work even without the event cinematic-skip flag.
    Inventory inventory;StatusMenu menu;MessageDialog dialog;GameplayInput gameplay(inventory,menu,dialog,nullptr);bool video=true;unsigned skips=0;gameplay.video_active=[&]{return video;};gameplay.skip_video=[&]{video=false;++skips;};InputFrame frame;frame.pressed[unsigned(Action::Confirm)]=true;
    gameplay.apply(frame);check(!video&&skips==1,"OK cannot skip active video without cinematic flag");video=true;frame={};frame.pressed[unsigned(Action::Skip)]=true;gameplay.apply(frame);check(!video&&skips==2,"skip action cannot skip video");video=true;gameplay.menu_input(MenuInput::Confirm);check(!video&&skips==3,"replay/menu confirm cannot skip video");
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);
        for(unsigned mode=0;mode<2;++mode){const auto bytes=read_asset_file(root/"rooms/RM_0000.RDT",64*1024*1024);const auto scene=decode_scene(bytes);Inventory inventory;StatusMenu menu;MessageDialog dialog;auto actor=scene.spawns[0];std::size_t motion=42;std::int64_t animation=0;AudioMixer audio(root);MoviePlayer movie(root,{},audio);RoomWorld world(bytes,scene,actor,motion,animation,audio,root);EventVm events(bytes,inventory,dialog);GameplayInput gameplay(inventory,menu,dialog,&events);
            events.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};events.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};events.audio_active=[&](unsigned slot){return audio.active(slot);};events.movie_active=[&]{return movie.active();};
            gameplay.video_active=[&]{return movie.active();};gameplay.skip_video=[&]{movie.stop();};
            auto tick=[&]{gameplay.apply(devices.tick());events.check_floor(actor.position,actor.yaw);events.tick();gameplay.synchronize();world.tick();movie.tick();audio.mix();};events.initialize();unsigned ticks=0;while(!movie.active()&&ticks++<5000)tick();check(movie.active()&&audio.active(7),"original film did not start");for(unsigned i=0;i<90;++i)tick();check(movie.active(),"film not playing at skip point");
            if(mode==0){devices.event(key(SDL_SCANCODE_SPACE,true));gameplay.apply(devices.tick());devices.event(key(SDL_SCANCODE_SPACE,false));devices.tick();}
            else{const int pad_index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);check(pad_index>=0,"late-skip controller attach");auto* pad=SDL_JoystickOpen(pad_index);check(pad,"late-skip controller open");pump();devices.tick();SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,1);pump();gameplay.apply(devices.tick());SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,0);pump();devices.tick();SDL_JoystickClose(pad);SDL_JoystickDetachVirtual(pad_index);pump();devices.tick();}
            check(!movie.active()&&!audio.active(7),"late skip retains movie/audio");
            for(unsigned i=0;i<1000&&!dialog.active();++i)tick();check(events.errors.empty()&&dialog.active()&&dialog.index()==4&&world.event_camera_index==37,"late film skip desynchronizes original cell hint");
            auto press_game=[&](SDL_Scancode code){devices.event(key(code,true));tick();devices.event(key(code,false));tick();};
            press_game(SDL_SCANCODE_RETURN);check(!dialog.active(),"hint confirmation failed");
            check(!menu.active,"inventory bypasses original cinematic handover");
            for(unsigned i=0;i<40;++i){tick();}check(menu.active,"inventory does not open automatically after cell hint");
            // SpriteH locks input during the opening; ItemCommand and its erase
            // animation also ignore input. Advance each frame through GameplayInput.
            for(unsigned i=0;i<120&&menu.slide.moving();++i)tick();check(!menu.slide.moving(),"status opening does not finish");
            press_game(SDL_SCANCODE_RETURN);check(menu.commands,"Enter does not open the lighter commands");
            for(unsigned i=0;i<8&&menu.command_window_moving();++i)tick();
            press_game(SDL_SCANCODE_RETURN);check(inventory.weapon()==1,"Enter does not equip the lighter");
            for(unsigned i=0;i<8&&menu.command_window_moving();++i)tick();
            press_game(SDL_SCANCODE_ESCAPE);press_game(SDL_SCANCODE_ESCAPE);
            for(unsigned i=0;i<40&&menu.active;++i)tick();check(!menu.active&&!events.menu_open,"status close does not finish after equipping");
            check(events.flag(1,99),"lighter equipment did not start Rodrigo event");
            for(unsigned i=0;i<3000;++i){tick();}
            if(!events.errors.empty()||world.player_hidden||world.player_scripted||(events.flags[5][0]&4)){std::cerr<<"mode="<<mode<<" hidden="<<world.player_hidden<<" scripted="<<world.player_scripted<<" cinematic="<<events.flags[5][0]<<" dialog="<<dialog.active()<<" index="<<dialog.index()<<" movie="<<movie.active()<<"\n";for(const auto& e:events.errors)std::cerr<<e<<"\n";}check(events.errors.empty()&&!world.player_hidden&&!world.player_scripted&&!(events.flags[5][0]&4),"late-skip/light equipment flow did not restore control");
            check(std::find(audio.played.begin(),audio.played.end(),"VOICE1/0016")!=audio.played.end(),"Rodrigo speech absent after fast inventory input");
        }
        GameUi ui(root);SoftwareRenderer image(640,480);ui.draw_title(image);check(std::any_of(image.pixels().begin(),image.pixels().end(),[](auto c){return (c&0xffffff)!=0;}),"original title art missing");
        std::cout<<"Original running film skipped by OK/Skip; audio stopped and original cell hint restored.\n";
    }
    {// adv.c TitleCall/Mode 17: New Game starts the voice and a 138-frame fade; buttons are ignored until black.
        auto title_config=InputConfig::defaults();Frontend title(title_config,path);
        check(title.input(MenuInput::Confirm)==FrontendCommand::TitleCall&&title.starting()&&title.active(),"New Game must start TitleCall");
        check(title.input(MenuInput::Cancel)==FrontendCommand::None&&title.starting(),"title fade accepted input");
        unsigned ticks=0;FrontendCommand result=FrontendCommand::None;while(result==FrontendCommand::None&&ticks<400){result=title.apply({});++ticks;}
        check(result==FrontendCommand::NewGame&&ticks==139&&!title.active(),"title fade length differs from GetSamurai(430)"); // float32 sum of 1/138 reaches 1 after 139 frames
        Frontend flash(title_config,path);flash.input(MenuInput::Confirm);for(int i=0;i<60;++i)flash.apply({});check(flash.flush()==0&&flash.fade()>.4f&&flash.fade()<.45f,"FlushCount/fade rate per frame");}
    std::cout<<"Main menu, keyboard/controller settings, live rebinding, persistence and video routing passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<" SDL: "<<SDL_GetError()<<'\n';return 1;}}
