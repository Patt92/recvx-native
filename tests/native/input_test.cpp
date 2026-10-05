#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void pump(InputManager& input){SDL_GameControllerUpdate();SDL_Event event;while(SDL_PollEvent(&event))input.event(event);}
static void key(InputManager& input,SDL_Scancode code,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=code;input.event(e);}
static void focus(InputManager& input,bool on){SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=on?SDL_WINDOWEVENT_FOCUS_GAINED:SDL_WINDOWEVENT_FOCUS_LOST;input.event(e);}
static void button(InputManager& input,SDL_Joystick* joystick,SDL_GameControllerButton code,bool down){check(SDL_JoystickSetVirtualButton(joystick,code,down)==0,"virtual button rejected");pump(input);}
static void axis(InputManager& input,SDL_Joystick* joystick,SDL_GameControllerAxis code,Sint16 value){check(SDL_JoystickSetVirtualAxis(joystick,code,value)==0,"virtual axis rejected");pump(input);}
static int attach(){const auto index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);check(index>=0,"virtual controller attach failed");check(SDL_IsGameController(index),"SDL does not recognize virtual gamepad");return index;}
static void detach(SDL_JoystickID id){for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_JoystickGetDeviceInstanceID(i)==id){check(SDL_JoystickDetachVirtual(i)==0,"virtual detach failed");return;}throw std::runtime_error("virtual device not found");}
struct Quit{~Quit(){SDL_Quit();}};
int main(int argc,char** argv){try{
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL controller init failed");Quit quit;_Make_SinTable();
    const auto config=InputConfig::defaults();const int index=attach();SDL_Joystick* joystick=SDL_JoystickOpen(index);check(joystick,"virtual joystick open failed");const auto id=SDL_JoystickInstanceID(joystick);
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_GameControllerUpdate();
    {InputManager input(config);pump(input);check(input.controllers()>=1,"controller present at startup not opened");input.tick();
        key(input,SDL_SCANCODE_W,true);auto frame=input.tick();check(frame.move()==1&&frame.hit(Action::Forward),"default keyboard movement missing");check(!input.tick().hit(Action::Forward),"held key repeats an edge");
        key(input,SDL_SCANCODE_S,true);check(input.tick().move()==0,"opposite movement does not cancel");key(input,SDL_SCANCODE_W,false);key(input,SDL_SCANCODE_S,false);input.tick();
        key(input,SDL_SCANCODE_RETURN,true);key(input,SDL_SCANCODE_RETURN,false);frame=input.tick();check(frame.hit(Action::Confirm)&&!frame.down(Action::Confirm),"short keyboard press lost between ticks");
        axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,-4000);check(input.tick().move()==0,"stick drift not filtered");axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,-12000);frame=input.tick();check(frame.move()==1&&!frame.down(Action::Up),"movement/menu axis thresholds conflated");
        axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,-32768);frame=input.tick();check(frame.move()==1&&frame.hit(Action::Up),"negative stick axis not mapped");
        for(unsigned i=2;i<=80;++i){frame=input.tick();const bool expected=i>config.repeat_delay&&(i-config.repeat_delay-1)%config.repeat_interval==0;check(frame.repeat[unsigned(Action::Up)]==expected,"menu repeat timing differs from config");}axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,0);input.tick();
        axis(input,joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);check(input.tick().down(Action::Run),"trigger binding missing");axis(input,joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);input.tick();
        button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);check(input.tick().hit(Action::Confirm),"short SDL gamepad press lost");
        key(input,SDL_SCANCODE_TAB,true);focus(input,false);frame=input.tick();check(!frame.hit(Action::Inventory)&&!frame.down(Action::Inventory)&&frame.move()==0,"focus loss leaves actions latched");focus(input,true);input.tick();
        SDL_Event repeated_key{};repeated_key.type=SDL_KEYDOWN;repeated_key.key.keysym.scancode=SDL_SCANCODE_TAB;repeated_key.key.repeat=1;input.event(repeated_key);frame=input.tick();check(!frame.hit(Action::Inventory)&&!frame.down(Action::Inventory),"OS key repeat synthesizes an action after focus regain");
        key(input,SDL_SCANCODE_W,true);axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTX,32767);const auto extra_index=attach();SDL_Joystick* extra=SDL_JoystickOpen(extra_index);const auto extra_id=SDL_JoystickInstanceID(extra);SDL_JoystickClose(extra);pump(input);
        const auto count=input.controllers();check(count>=2,"hot-plug controller not opened");detach(extra_id);pump(input);check(input.controllers()==count-1,"removed device index confused with instance ID");
        axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTX,0);key(input,SDL_SCANCODE_W,false);input.tick();
        {Inventory inventory;StatusMenu menu;MessageDialog dialog;GameplayInput gameplay(inventory,menu,dialog,nullptr);bool active=true;unsigned skips=0;
            gameplay.transition_active=[&]{return active;};gameplay.confirm_transition=[&]{++skips;};
            key(input,SDL_SCANCODE_RETURN,true);gameplay.apply(input.tick());check(skips==1,"keyboard OK does not skip door");for(unsigned i=0;i<30;++i)gameplay.apply(input.tick());check(skips==1,"held keyboard OK skips repeatedly");key(input,SDL_SCANCODE_RETURN,false);input.tick();
            button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);gameplay.apply(input.tick());check(skips==2,"controller OK does not skip door");for(unsigned i=0;i<30;++i)gameplay.apply(input.tick());check(skips==2,"held controller OK skips repeatedly");button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);input.tick();
            key(input,SDL_SCANCODE_TAB,true);gameplay.apply(input.tick());check(!menu.active,"inventory opens during door transition");key(input,SDL_SCANCODE_TAB,false);input.tick();}
        if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto room=read_asset_file(root/"rooms/RM_0000.RDT",64*1024*1024);const auto stage=decode_scene(room);Inventory inventory;StatusMenu menu;MessageDialog dialog;auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t clock=0;
            AudioMixer audio(root);RoomWorld world(room,stage,actor,motion,clock,audio,root);MoviePlayer movie(root,{},audio);EventVm events(room,inventory,dialog);GameplayInput gameplay(inventory,menu,dialog,&events);
            events.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};events.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};events.audio_active=[&](unsigned slot){return audio.active(slot);};events.audio_stop=[&](unsigned slot){audio.stop(slot);};events.movie_active=[&]{return movie.active();};
            auto tick=[&]{check(!gameplay.apply(input.tick()),"gamepad menu action quits game");events.check_floor(actor.position,actor.yaw);events.tick();gameplay.synchronize();world.tick();movie.tick();audio.mix();};
            auto press=[&](SDL_GameControllerButton b){button(input,joystick,b,true);tick();button(input,joystick,b,false);tick();};
            events.initialize();tick();press(SDL_CONTROLLER_BUTTON_Y);check(!menu.active,"controller opens inventory during cinematic");press(SDL_CONTROLLER_BUTTON_START);for(unsigned i=0;i<60;++i)tick();
            check(dialog.active()&&dialog.index()==4&&world.event_camera_index==37,"controller skip does not reach original cell hint");press(SDL_CONTROLLER_BUTTON_A);for(unsigned i=0;i<40;++i)tick();check(menu.active,"original hint does not automatically open inventory");
            button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);tick();for(unsigned i=0;i<30;++i)tick();check(menu.commands&&inventory.weapon()==0,"held A auto-equips or advances multiple actions");button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);tick();press(SDL_CONTROLLER_BUTTON_A);
            check(inventory.weapon()==1&&!menu.active,"A does not equip lighter");bool voice_advanced=false;for(unsigned i=0;i<3000;++i){tick();if(!voice_advanced&&dialog.active()&&audio.active(1)){for(unsigned page=0;page<8&&dialog.active();++page)gameplay.menu_input(MenuInput::Confirm);check(!dialog.active()&&!audio.active(1),"Rodrigo voice keeps playing after confirming its dialogue");voice_advanced=true;}}check(voice_advanced,"real Rodrigo voice/dialogue advance was not exercised");check(events.errors.empty()&&!world.player_hidden&&!world.player_scripted&&!(events.flags[5][0]&4),"controller equipment path fails to restore game control");
            press(SDL_CONTROLLER_BUTTON_Y);press(SDL_CONTROLLER_BUTTON_B);check(!menu.active,"B does not cancel inventory");
            const auto body=PlayerAsset::load(root,10);SceneSpawn walker{{100,0,100},0};std::size_t walker_motion=42;std::int64_t walker_frame=0;
            axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,32767);const auto backward=input.tick();body.locomotion(walker,walker_motion,walker_frame,backward.move(),backward.down(Action::Run),backward.turn());
            check(walker_motion==16&&walker.position.z>100&&walker.yaw==0,"SDL gamepad backward does not animate original backstep");axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,0);input.tick();
            const auto direction_yaw=walker.yaw;key(input,SDL_SCANCODE_A,true);auto left=input.tick();body.locomotion(walker,walker_motion,walker_frame,0,false,left.turn());check(walker.yaw==((direction_yaw+int(182.04445f*4.5f))&65535),"A rotates right");key(input,SDL_SCANCODE_A,false);input.tick();key(input,SDL_SCANCODE_D,true);auto right=input.tick();body.locomotion(walker,walker_motion,walker_frame,0,false,right.turn());check(walker.yaw==direction_yaw,"D rotates left");key(input,SDL_SCANCODE_D,false);input.tick();
            menu.active=menu.examining=true;menu.examine_yaw=0;button(input,joystick,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,true);for(unsigned i=0;i<30;++i)tick();check(menu.examine_yaw==30*512,"3D inspection is limited by menu repeat delay");button(input,joystick,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,false);tick();menu.active=menu.examining=false;
            std::cout<<"Original cell skip/dialog/inventory/lighter/control flow verified with SDL virtual gamepad.\n";
        }
        // Disconnect while moving; keyboard input must remain usable.
        axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,-32768);key(input,SDL_SCANCODE_J,true);input.tick();SDL_JoystickClose(joystick);joystick=nullptr;detach(id);pump(input);frame=input.tick();check(frame.move()==0&&frame.turn()==-1,"disconnect leaves movement stuck or clears keyboard");key(input,SDL_SCANCODE_J,false);
    }
    // Actual INI parsing: custom keyboard/gamepad buttons and axis inversion.
    const auto folder=std::filesystem::temp_directory_path()/("recvx-controls-"+std::to_string(SDL_GetTicks64()));std::filesystem::create_directory(folder);const auto file=folder/"controls.ini";InputConfig::write_defaults(file);
    std::ifstream source(file);std::string text((std::istreambuf_iterator<char>(source)),{});source.close();const auto replace=[&](const std::string& from,const std::string& to){auto at=text.find(from);check(at!=text.npos,"fixture binding missing");text.replace(at,from.size(),to);};
    replace("confirm = Return","confirm = E");replace("confirm = button:a","confirm = button:x");replace("axis:lefty-","axis:lefty+");std::ofstream(file)<<text;const auto custom=InputConfig::read(file);
    const int new_index=attach();joystick=SDL_JoystickOpen(new_index);const auto new_id=SDL_JoystickInstanceID(joystick);SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_GameControllerUpdate();
    {InputManager input(custom);pump(input);key(input,SDL_SCANCODE_RETURN,true);check(!input.tick().hit(Action::Confirm),"old keyboard binding remains active");key(input,SDL_SCANCODE_E,true);check(input.tick().hit(Action::Confirm),"keyboard remap not applied");key(input,SDL_SCANCODE_E,false);input.tick();button(input,joystick,SDL_CONTROLLER_BUTTON_X,true);check(input.tick().hit(Action::Confirm),"gamepad remap not applied");button(input,joystick,SDL_CONTROLLER_BUTTON_X,false);input.tick();axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,32767);check(input.tick().down(Action::Forward),"axis inversion not applied");}
    SDL_JoystickClose(joystick);detach(new_id);
    for(const auto bad:{"[settings]\ndeadzone = nan\n","[settings]\nrepeat_interval_ticks = 0\n","[keyboard]\nconfirm = NotARealKey\n","[gamepad]\nconfirm = button:fake\n","[settings]\nunknown = 1\n","[keyboard]\nconfirm = E\nconfirm = Q\n"}){std::ofstream(file)<<bad;bool rejected=false;try{InputConfig::read(file);}catch(const std::runtime_error&){rejected=true;}check(rejected,"malformed controls accepted");}
    std::filesystem::remove_all(folder);std::cout<<"Keyboard/config/controller axes, buttons, repeat, focus, hot-plug and disconnect tests passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<": "<<SDL_GetError()<<'\n';return 1;}}
