#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include <fstream>
#include <cstring>
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
        for(const bool controller:{false,true}){
            Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);StatusMenu menu;menu.active=true;menu.selected=2;MessageDialog room;
            std::unique_ptr<GameUi> ui;if(argc>1)ui=std::make_unique<GameUi>(std::filesystem::u8path(argv[1]));menu.message_source=[&](unsigned index){return ui?ui->system_message(index):Message{'L'-32,0xff02,0,'?'-32,0xff04,'Y'-32,0xff04,'N'-32,0xffff};};
            std::vector<std::uint8_t> bytes(1500);float version=1.73f;std::memcpy(bytes.data(),&version,4);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)bytes.at(p+i)=std::uint8_t(n>>(8*i));};put(16,64);put(28,1100);put(64+0x38,1400);put(1100,12);put(1104,14);put(1108,30);EventVm events(bytes,inv,room);
            GameplayInput gameplay(inv,menu,room,&events);std::vector<unsigned> sounds;gameplay.sound=[&](unsigned sound){sounds.push_back(sound);};auto tick=[&]{check(!gameplay.apply(input.tick()),"inventory control quits game");};
            auto held=[&](SDL_Scancode code,SDL_GameControllerButton b,bool down){if(controller)button(input,joystick,b,down);else key(input,code,down);};
            auto press=[&](SDL_Scancode code,SDL_GameControllerButton b){held(code,b,true);tick();held(code,b,false);tick();};
            auto ok=[&]{press(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A);};auto down=[&]{press(SDL_SCANCODE_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN);};
            ok();down();down();ok();press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);ok();check(inv.contains(25)&&!menu.combining&&menu.active,"keyboard/gamepad herb Combine failed");
            inv.slots()[2]=Inventory::pack(128,1);inv.slots()[3]=Inventory::pack(50,1);menu.selected=2;ok();down();ok();check(menu.examining,"keyboard/gamepad cannot examine case");ok();check(menu.message.index()==181,"keyboard/gamepad lockpick question missing");
            held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,true);tick();for(unsigned i=0;i<30;++i)tick();check(menu.message.active()&&!menu.opening_item&&inv.contains(128),"held OK answers its own new question");held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,false);tick();
            while(menu.message.active()&&!menu.message.choices())ok();press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);ok();check(inv.contains(128)&&!menu.opening_item,"keyboard/gamepad No opens case");
            ok();press(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B);check(!menu.message.active()&&menu.examining&&inv.contains(128),"keyboard/gamepad cancel closes inventory or consumes case");
            ok();while(menu.message.active()&&!menu.message.choices())ok();ok();for(unsigned i=0;i<23;++i)tick();check(inv.contains(35)&&inv.contains(50)&&menu.active&&menu.message.index()==164,"keyboard/gamepad case Yes does not retain lockpick or show contents");
            while(menu.message.active())ok();press(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B);menu.selected=3;ok();ok();check(menu.active&&events.menu_open&&inv.contains(50)&&menu.message.index()==161&&!(events.flags[10][0]&0x400),"invalid keyboard/gamepad key Use leaves inventory or dispatches event");check(!sounds.empty()&&sounds.back()==1&&std::find(sounds.begin(),sounds.end(),2)!=sounds.end(),"inventory failure/navigation misses common sound dispatch");
        }
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
