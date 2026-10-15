#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "combat.h"
#include "box_menu.h"
#include "asset_io.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
// Inventarabschnitte (D01/D02/D07) laufen einzeln; Fehlschläge werden gesammelt.
static std::vector<std::string> failures;
static void section(const char* name,const std::function<void()>& body){try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}}
static void pump(InputManager& input){SDL_GameControllerUpdate();SDL_Event event;while(SDL_PollEvent(&event))input.event(event);}
static void key(InputManager& input,SDL_Scancode code,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=code;input.event(e);}
static void focus(InputManager& input,bool on){SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=on?SDL_WINDOWEVENT_FOCUS_GAINED:SDL_WINDOWEVENT_FOCUS_LOST;input.event(e);}
static void button(InputManager& input,SDL_Joystick* joystick,SDL_GameControllerButton code,bool down){check(SDL_JoystickSetVirtualButton(joystick,code,down)==0,"virtual button rejected");pump(input);}
static void axis(InputManager& input,SDL_Joystick* joystick,SDL_GameControllerAxis code,Sint16 value){check(SDL_JoystickSetVirtualAxis(joystick,code,value)==0,"virtual axis rejected");pump(input);}
static int attach(){const auto index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);check(index>=0,"virtual controller attach failed");check(SDL_IsGameController(index),"SDL does not recognize virtual gamepad");return index;}
static void detach(SDL_JoystickID id){for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_JoystickGetDeviceInstanceID(i)==id){check(SDL_JoystickDetachVirtual(i)==0,"virtual detach failed");return;}throw std::runtime_error("virtual device not found");}
struct Quit{~Quit(){SDL_Quit();}};
// Unabhaengiger Aufruf, damit die Dialogregression nicht hinter dem RM_0000-Select-
// Ablauf verschwindet. message.c:269-295 liest nur pad_ps 0x4/0x8, pad.c:275 bildet
// diese Flanken ohne Wiederholung. Der bestehende Cinematic-Test bleibt unveraendert.
static void dialog_edges(const std::filesystem::path& root){
    const auto bytes=read_asset_file(root/"rooms/RM_0060.RDT",64*1024*1024);
    const auto scene=decode_scene(bytes);
    for(const bool controller:{false,true}){
        const int index=attach();SDL_Joystick* joystick=SDL_JoystickOpen(index);check(joystick,"dialog virtual joystick open failed");
        const auto id=SDL_JoystickInstanceID(joystick);
        SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
        SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
        {
            InputManager input(InputConfig::defaults());pump(input);input.tick();
            Inventory inventory;StatusMenu menu;MessageDialog dialog;auto actor=scene.spawns.at(0);
            std::size_t motion=42;std::int64_t clock=0;AudioMixer audio(root);
            RoomWorld world(bytes,scene,actor,motion,clock,audio,root);
            EventVm events(bytes,inventory,dialog);GameplayInput gameplay(inventory,menu,dialog,&events);
            events.binding=[&](const EventCommand& c){return world.bind(c);};
            events.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};
            events.audio_active=[&](unsigned s){return audio.active(s);};events.audio_stop=[&](unsigned s){audio.stop(s);};
            std::vector<unsigned> sounds;gameplay.sound=[&](unsigned s){sounds.push_back(s);};
            gameplay.interact=[&]{events.interact(actor.position,actor.yaw,world.floor_at(actor.position));};
            auto tick=[&]{const auto f=input.tick();check(!gameplay.apply(f),"dialog input quits");events.check_floor(actor.position,actor.yaw);events.tick();gameplay.synchronize();world.tick();audio.mix();return f;};
            auto held=[&](SDL_Scancode k,SDL_GameControllerButton b,bool down){if(controller)button(input,joystick,b,down);else key(input,k,down);};
            events.initialize();for(unsigned i=0;i<60;++i)tick();
            actor.position={26,0,90};actor.yaw=16384; // echte Gefaengniskartenfrage, verify_pickups.py MAP
            held(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,true);for(unsigned i=0;i<40;++i)tick();
            held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,true);tick();held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,false);tick();
            check(dialog.active()&&dialog.index()==5&&dialog.choices()==2&&dialog.selection==0,"RM_0060 default question missing");
            const auto before=sounds.size();unsigned repeats=0;
            for(unsigned i=0;i<40;++i){const auto f=tick();repeats+=f.repeat[unsigned(Action::Right)];check(dialog.selection==0,"held direction changes Yes/No selection");}
            check(repeats>0&&std::count(sounds.begin()+before,sounds.end(),2u)==0,"dialog repeat test did not exercise/suppress real repeat");
            held(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,false);tick();
            held(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,true);tick();check(dialog.selection==1,"new right edge does not select No");
            held(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,false);tick();
            held(SDL_SCANCODE_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT,true);tick();check(dialog.selection==0,"new left edge does not select Yes");
            held(SDL_SCANCODE_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT,false);tick();
            held(SDL_SCANCODE_W,SDL_CONTROLLER_BUTTON_DPAD_UP,true);for(unsigned i=0;i<40;++i){tick();check(dialog.selection==0,"vertical direction changes Yes/No selection");}
            held(SDL_SCANCODE_W,SDL_CONTROLLER_BUTTON_DPAD_UP,false);tick();
            held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,true);tick();held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,false);tick();
            for(unsigned i=0;i<40;++i)tick();
            check(events.flag(7,133),"default Yes did not acquire prison map");
            // Statusmenue-Cursor behaelt seine Wiederholung (sub1.c CursorMove),
            // unabhaengig von der pad_ps-only-Auswahl in bhControlMessage.
            dialog=MessageDialog{};menu.active=true;menu.skip_slide();menu.nav=true;menu.maincsr=3;events.menu_open=true;
            held(SDL_SCANCODE_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT,true);gameplay.apply(input.tick());check(menu.maincsr==2,"status cursor initial edge missing");
            for(unsigned i=0;i<40;++i)gameplay.apply(input.tick());check(menu.maincsr==0,"status cursor held repeat stopped");
            held(SDL_SCANCODE_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT,false);input.tick();
            std::cout<<(controller?"controller":"keyboard")<<": RM_0060 held/no repeat, new edges, vertical pad, map Yes and status repeat PASS\n";
        }
        SDL_JoystickClose(joystick);detach(id);
    }
}
// Same raw-pad adapter and modal suppression as runtime.cpp, exercised with
// real InputManager keyboard events and an SDL virtual controller.
static void box_pad_frames(const std::filesystem::path& root){
    const auto bytes=read_asset_file(root/"rooms/RM_0090.RDT",64*1024*1024);
    for(const bool controller:{false,true}){
        const int index=attach();auto* joystick=SDL_JoystickOpen(index);check(joystick,"box virtual joystick open failed");
        const auto id=SDL_JoystickInstanceID(joystick);
        SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
        SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
        {
            InputManager input(InputConfig::defaults());pump(input);input.tick();
            Inventory inv;StatusMenu menu;MessageDialog dialog;EventVm vm(bytes,inv,dialog);AudioMixer audio;
            BoxMenu box;menu.active=true;vm.flags[10][0]|=0x40000;
            GameplayInput gameplay(inv,menu,dialog,&vm);
            gameplay.modal_input=[&](MenuInput){return box.active();};
            unsigned previous=0;
            auto tick=[&]{const auto f=input.tick();check(!gameplay.apply(f),"box input quit");const unsigned on=item_box_pad(f);box.tick(on,on&~previous,inv,menu,&vm,audio);previous=on;return f;};
            auto held=[&](SDL_Scancode k,SDL_GameControllerButton b,bool down){if(controller)button(input,joystick,b,down);else key(input,k,down);};
            tick();for(unsigned i=0;i<9;++i)tick();
            held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,true);tick();
            held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,false);tick();
            check(box.box_index()==0,"confirm must not also act as right shoulder");
            held(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,true);
            unsigned repeats=0;for(unsigned i=0;i<30;++i){const auto f=tick();repeats+=f.repeat[unsigned(Action::Right)];check(box.box_index()==0,"horizontal D-pad scrolls the box list");}
            check(repeats>0,"box did not exercise host repeat suppression");
            held(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,false);tick();
            held(SDL_SCANCODE_R,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,true);
            for(unsigned i=1;i<=16;++i){tick();const unsigned expected=i<12?5u:i<16?10u:15u;check(box.box_index()==expected,"right shoulder frame counters differ from ItemBox");}
            held(SDL_SCANCODE_R,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);tick();
            held(SDL_SCANCODE_Q,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,true);tick();check(box.box_index()==10,"left shoulder first frame missing");
            held(SDL_SCANCODE_Q,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,false);tick();
            held(SDL_SCANCODE_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN,true);tick();check(box.row_shift()==-19.f,"held Down first row frame missing");
            tick();check(box.row_shift()==-38.f,"held Down second row frame missing");
            tick();check(box.box_index()==11&&box.row_shift()==0.f,"held Down third frame did not advance index");
            tick();check(box.row_shift()==-19.f,"held Down must restart the row scroll without host repeat");
            held(SDL_SCANCODE_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN,false);tick();tick();
            held(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B,true);tick();held(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B,false);tick();
            check(box.active()&&!box.moving(),"first Cancel must return from the box list");
            held(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B,true);tick();held(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B,false);tick();
            for(unsigned i=0;i<9&&box.active();++i)tick();
            check(!box.active()&&!menu.active&&!(vm.flags[10][0]&0x1c0000u),"Cancel did not release box task");
            std::cout<<(controller?"virtual controller":"keyboard")<<": Box pad counters, row timing, repeat suppression and release PASS\n";
        }
        SDL_JoystickClose(joystick);detach(id);
    }
}
int main(int argc,char** argv){try{
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL controller init failed");Quit quit;_Make_SinTable();
    if(argc==3&&std::string(argv[1])=="--dialog-edges"){dialog_edges(std::filesystem::u8path(argv[2]));return 0;}
    if(argc==3&&std::string(argv[1])=="--box-pad"){box_pad_frames(std::filesystem::u8path(argv[2]));return 0;}
    const auto config=InputConfig::defaults();const int index=attach();SDL_Joystick* joystick=SDL_JoystickOpen(index);check(joystick,"virtual joystick open failed");const auto id=SDL_JoystickInstanceID(joystick);
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_GameControllerUpdate();
    {InputManager input(config);pump(input);check(input.controllers()>=1,"controller present at startup not opened");input.tick();
        button(input,joystick,SDL_CONTROLLER_BUTTON_BACK,true);check(input.tick().hit(Action::Pause),"controller Select/Back pause binding missing");button(input,joystick,SDL_CONTROLLER_BUTTON_BACK,false);input.tick();
        key(input,SDL_SCANCODE_P,true);check(input.tick().hit(Action::Pause),"keyboard pause binding missing");key(input,SDL_SCANCODE_P,false);input.tick();
        // PS2 SELECT (pad.c pad_tab_a[16] = 0x100 -> pad_ps 0x10000): Back/Select on the controller, P on the keyboard.
        button(input,joystick,SDL_CONTROLLER_BUTTON_BACK,true);check(input.tick().hit(Action::Select),"controller Select binding missing");button(input,joystick,SDL_CONTROLLER_BUTTON_BACK,false);input.tick();
        key(input,SDL_SCANCODE_P,true);check(input.tick().hit(Action::Select),"keyboard Select binding missing");key(input,SDL_SCANCODE_P,false);input.tick();
        button(input,joystick,SDL_CONTROLLER_BUTTON_START,true);check(!input.tick().hit(Action::Select),"Start counts as Select");button(input,joystick,SDL_CONTROLLER_BUTTON_START,false);input.tick();
        key(input,SDL_SCANCODE_SPACE,true);check(!input.tick().hit(Action::Select),"Space counts as Select");key(input,SDL_SCANCODE_SPACE,false);input.tick();
        // Escape ist Menue-Abbruch und Host-Pause, nie die Beendigungstaste. Der Key
        // darf unter quit nicht mehr auflaufen, sonst schliesst er ein laufendes Spiel.
        key(input,SDL_SCANCODE_ESCAPE,true);{const auto escape_frame=input.tick();check(escape_frame.hit(Action::Cancel)&&escape_frame.hit(Action::Pause),"Escape is not Cancel+Pause in the default bindings");check(!escape_frame.hit(Action::Quit),"Escape is still a quit key in the default bindings");}key(input,SDL_SCANCODE_ESCAPE,false);input.tick();
        key(input,SDL_SCANCODE_F10,true);check(input.tick().hit(Action::Quit),"keyboard quit hotkey missing");key(input,SDL_SCANCODE_F10,false);input.tick();
        {const std::string legacy("[keyboard]\nforward = I\nback = K\nturn_left = J\nturn_right = L\nrun = Left Shift\ninventory = Tab\nconfirm = Return\ncancel = Escape\nup = Up\ndown = Down\nleft = Left\nright = Right\nskip = Space\nquit = Escape, F10\nwireframe = F3\n[gamepad]\nforward = button:dpup\nback = button:dpdown\nturn_left = button:dpleft\nturn_right = button:dpright\nrun = button:b\ninventory = button:y\nconfirm = button:a\ncancel = button:b\nup = button:dpup\ndown = button:dpdown\nleft = button:dpleft\nright = button:dpright\nskip = button:start\nquit = none\nwireframe = none\n");
            const auto temporary=std::filesystem::temp_directory_path()/"recvx-input-test-legacy-controls.ini";{std::ofstream out(temporary);out<<legacy;}
            const auto old=InputConfig::read(temporary);std::filesystem::remove(temporary);
            check(old.describe(Action::Select,false)=="P"&&old.describe(Action::Select,true)=="Back","controls.ini without a select line loses the SELECT button");
            // Der ausgelieferte Altstand `quit = Escape, F10` behaelt F10 und verliert
            // Escape, und er bekommt die Host-Pause auf Escape dazu (Fallback-Zeile).
            check(old.describe(Action::Quit,false)=="F10",std::string("an old controls.ini keeps Escape as a quit key: ")+old.describe(Action::Quit,false));
            check(old.describe(Action::Pause,false)=="P / Escape","controls.ini without a pause line loses the Escape pause key");
            {InputManager legacy_input(old);pump(legacy_input);legacy_input.tick();
                key(legacy_input,SDL_SCANCODE_ESCAPE,true);{const auto frame=legacy_input.tick();check(frame.hit(Action::Cancel)&&frame.hit(Action::Pause)&&!frame.hit(Action::Quit),"an old controls.ini still terminates on Escape");}key(legacy_input,SDL_SCANCODE_ESCAPE,false);legacy_input.tick();
                key(legacy_input,SDL_SCANCODE_F10,true);check(legacy_input.tick().hit(Action::Quit),"an old controls.ini lost the F10 quit key");key(legacy_input,SDL_SCANCODE_F10,false);legacy_input.tick();}
            /* Der Einstellungsschirm kann Cancel und Quit auf denselben Key legen; die
               Flanke darf dann trotzdem kein Quit tragen (siehe InputManager::tick). */
            auto overlap=InputConfig::defaults();overlap.bindings[unsigned(Action::Quit)].push_back({InputBinding::Kind::Key,int(SDL_SCANCODE_ESCAPE),1});
            {InputManager shared(overlap);pump(shared);shared.tick();
                key(shared,SDL_SCANCODE_ESCAPE,true);{const auto frame=shared.tick();check(frame.hit(Action::Cancel)&&frame.hit(Action::Pause)&&!frame.hit(Action::Quit),"a key bound to cancel and quit produces a quit edge");}key(shared,SDL_SCANCODE_ESCAPE,false);shared.tick();}}
        key(input,SDL_SCANCODE_Q,true);check(input.tick().hit(Action::NextTarget),"keyboard target switch missing");check(!input.tick().hit(Action::NextTarget),"held target switch repeats");key(input,SDL_SCANCODE_Q,false);input.tick();
        button(input,joystick,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,true);check(input.tick().hit(Action::NextTarget),"L1/LB target switch missing");button(input,joystick,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,false);input.tick();
        key(input,SDL_SCANCODE_R,true);key(input,SDL_SCANCODE_LCTRL,true);auto weapon_input=input.tick();check(weapon_input.down(Action::Aim)&&weapon_input.hit(Action::Attack),"keyboard aim/attack missing");key(input,SDL_SCANCODE_R,false);key(input,SDL_SCANCODE_LCTRL,false);input.tick();
        button(input,joystick,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,true);button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);weapon_input=input.tick();check(weapon_input.down(Action::Aim)&&weapon_input.hit(Action::Attack)&&!weapon_input.down(Action::Run),"gamepad aim/attack conflict with run");button(input,joystick,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);input.tick();
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
        {Inventory inventory;StatusMenu menu;MessageDialog dialog;GameplayInput gameplay(inventory,menu,dialog,nullptr);bool active=true;unsigned interactions=0,skips=0;
            // BEWUSSTE ABWEICHUNG VOM ORIGINAL (doors.h, Nutzerentscheidung 2026-10-13):
            // das Original liest waehrend der Tuerdemo kein Pad (bhStartDoorDemo loescht
            // pad_ps/pad_on). Der Host-Skip ist gewuenscht, darf aber nur einmal je Druck
            // ausloesen, nie die Aktion erreichen und nie das Inventar oeffnen.
            gameplay.transition_active=[&]{return active;};gameplay.interact=[&]{++interactions;};gameplay.confirm_transition=[&]{++skips;};
            key(input,SDL_SCANCODE_RETURN,true);check(!gameplay.apply(input.tick()),"keyboard OK quits during door");check(skips==1,"keyboard OK does not skip door");for(unsigned i=0;i<30;++i)gameplay.apply(input.tick());check(skips==1,"held keyboard OK skips repeatedly");check(interactions==0,"keyboard OK acted during door transition");key(input,SDL_SCANCODE_RETURN,false);input.tick();
            button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);check(!gameplay.apply(input.tick()),"controller OK quits during door");check(skips==2,"controller OK does not skip door");for(unsigned i=0;i<30;++i)gameplay.apply(input.tick());check(skips==2,"held controller OK skips repeatedly");check(interactions==0,"controller OK acted during door transition");button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);input.tick();
            gameplay.menu_input(MenuInput::Confirm);check(skips==3&&interactions==0&&!menu.active,"menu confirm did not reach the door skip alone");
            gameplay.menu_input(MenuInput::Cancel);check(skips==3,"menu cancel skipped the door");
            // Escape ist Cancel, nicht Quit: es beendet weder den Tuerlauf noch das Programm.
            key(input,SDL_SCANCODE_ESCAPE,true);check(!gameplay.apply(input.tick()),"Escape quits the program during a door transition");check(skips==3&&interactions==0,"Escape skipped or interacted during a door transition");key(input,SDL_SCANCODE_ESCAPE,false);input.tick();
            key(input,SDL_SCANCODE_TAB,true);gameplay.apply(input.tick());check(!menu.active,"inventory opens during door transition");key(input,SDL_SCANCODE_TAB,false);input.tick();
            // Without a transition the same press is the ordinary action again.
            active=false;key(input,SDL_SCANCODE_RETURN,true);gameplay.apply(input.tick());check(skips==3,"the skip ran outside a door transition");key(input,SDL_SCANCODE_RETURN,false);input.tick();active=true;}
        {   // Freier Spielzustand: kein Menue, kein Dialog, kein Tuerlauf, kein Video.
            Inventory inventory;StatusMenu menu;MessageDialog dialog;GameplayInput gameplay(inventory,menu,dialog,nullptr);
            key(input,SDL_SCANCODE_ESCAPE,true);check(!gameplay.apply(input.tick()),"Escape quits the running game");for(unsigned i=0;i<30;++i)check(!gameplay.apply(input.tick()),"held Escape quits the running game");key(input,SDL_SCANCODE_ESCAPE,false);input.tick();
            key(input,SDL_SCANCODE_F10,true);check(gameplay.apply(input.tick()),"F10 no longer quits the game");key(input,SDL_SCANCODE_F10,false);input.tick();}
        for(const bool controller:{false,true}){
            Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);StatusMenu menu;menu.active=true;menu.skip_slide();menu.selected=2;MessageDialog room;
            std::unique_ptr<GameUi> ui;if(argc>1)ui=std::make_unique<GameUi>(std::filesystem::u8path(argv[1]));menu.message_source=[&](unsigned index){return ui?ui->system_message(index):Message{'L'-32,0xff02,0,'?'-32,0xff04,'Y'-32,0xff04,'N'-32,0xffff};};
            std::vector<std::uint8_t> bytes(1500);float version=1.73f;std::memcpy(bytes.data(),&version,4);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)bytes.at(p+i)=std::uint8_t(n>>(8*i));};put(16,64);put(28,1100);put(64+0x38,1400);put(1100,12);put(1104,14);put(1108,30);EventVm events(bytes,inv,room);
            GameplayInput gameplay(inv,menu,room,&events);std::vector<unsigned> sounds;gameplay.sound=[&](unsigned sound){sounds.push_back(sound);};auto tick=[&]{check(!gameplay.apply(input.tick()),"inventory control quits game");menu.tick(inv);}; /* StatusMain je Bild: bhControlMessage baut den Text auf (message.c:345) */
            auto held=[&](SDL_Scancode code,SDL_GameControllerButton b,bool down){if(controller)button(input,joystick,b,down);else key(input,code,down);};
            auto press=[&](SDL_Scancode code,SDL_GameControllerButton b){held(code,b,true);tick();held(code,b,false);tick();};
            auto ok=[&]{press(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A);};auto settle=[&]{for(int i=0;i<4;++i)tick();}; /* bhControlMessage: Zwischenseiten blaettert OK, eine Auswahl nimmt OK erst offen an (message.c:233/325) */auto ready=[&]{for(unsigned i=0;i<400&&menu.message.active()&&!menu.message.choosing();++i){if(menu.message.turnable())ok();else tick();}}; /* ItemCommand window slide */auto down=[&]{press(SDL_SCANCODE_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN);};
            // D01/D07 über Tastatur und Controller (derselbe Menüweg): Kräuter
            // kombinieren (Frage 155, Ja -> 25), ungültiger Gebrauch des
            // Dietrichs -> Use_05 Meldung 161 ohne Ereignis, Fehler-SE 1. Die
            // Untersuchung (ItemInspection) liegt außerhalb dieses Pakets und
            // bleibt als Lücke geprüft.
            section(controller?"D01 Kombinieren (Controller)":"D01 Kombinieren (Tastatur)",[&]{
                ok();settle();down();down();ok();settle();check(menu.combining&&menu.combine_from==2,"keyboard/gamepad cannot enter the original Combine mode");
                press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);check(menu.selected==3,"keyboard/gamepad combine cursor does not reach the partner slot");
                ok();check(menu.message.active()&&menu.message.index()==155,"keyboard/gamepad combine question 155 missing");
                ready();ok();
                for(int i=0;i<20&&(menu.combining||menu.erase_frames);++i)tick(); // ItemCombination subcsr 2/3 und ItemCommandErase laufen je Statusframe
                check(inv.contains(25)&&!inv.contains(21)&&!inv.contains(22)&&!menu.combining&&menu.active,"keyboard/gamepad herb Combine failed");});
            if(menu.combining)press(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B);
            // D03 Untersuchung über denselben Eingabeweg: Dietrichfrage 181,
            // Halte-Sperre, Nein, Abbrechen und Ja bis zum Inhalt (Item 35,
            // Meldung 164) mit erhaltenem Dietrich.
            section(controller?"D03 Untersuchung (Controller)":"D03 Untersuchung (Tastatur)",[&]{
                inv.slots()[2]=Inventory::pack(128,1);inv.slots()[3]=Inventory::pack(50,1);menu.selected=2;ok();settle();down();ok();settle();check(menu.examining,"keyboard/gamepad cannot examine case");menu.examine_pitch=menu.examine_yaw=menu.examine_roll=0;ok();check(menu.message.index()==181,"keyboard/gamepad lockpick question missing");
                held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,true);tick();for(unsigned i=0;i<30;++i)tick();check(menu.message.active()&&!menu.opening_item&&inv.contains(128),"held OK answers its own new question");held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,false);tick();
                ready();press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);ok();check(inv.contains(128)&&!menu.opening_item,"keyboard/gamepad No opens case");
                // itemview.c:568: Cancel wirkt nur ohne offenen Text (st_flg 0x200); die Frage 181 beantwortet allein OK (message.c:233).
                ok();ready();press(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B);check(menu.message.active()&&menu.message.index()==181&&menu.examining&&inv.contains(128),"keyboard/gamepad cancel answers the lockpick question");
                press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);ok();check(!menu.message.active()&&menu.examining&&inv.contains(128),"keyboard/gamepad No closes inventory or consumes case");
                ok();ready();ok();for(unsigned i=0;i<100;++i)tick();check(inv.contains(35)&&inv.contains(50)&&menu.active&&menu.message.index()==164,"keyboard/gamepad case Yes does not retain lockpick or show contents");
                while(menu.message.active())ok();press(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B);});
            section(controller?"D07 Gebrauch ohne Trigger (Controller)":"D07 Gebrauch ohne Trigger (Tastatur)",[&]{
                unsigned key_slot=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==50)key_slot=i;
                check(key_slot,"lockpick lost before the use check");menu.selected=key_slot;const auto before_use=sounds.size();ok();settle();ok();settle();
                check(menu.active&&events.menu_open&&inv.contains(50)&&menu.message.index()==161&&!(events.flags[10][0]&0x400),"invalid keyboard/gamepad key Use leaves inventory, lacks 161 or dispatches event");
                check(std::find(sounds.begin()+before_use,sounds.end(),1u)!=sounds.end()&&std::find(sounds.begin(),sounds.end(),2u)!=sounds.end()&&std::find(sounds.begin(),sounds.end(),3u)!=sounds.end(),"inventory failure/navigation misses common sound dispatch");});
        }
        if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto room=read_asset_file(root/"rooms/RM_0000.RDT",64*1024*1024);const auto stage=decode_scene(room);Inventory inventory;StatusMenu menu;MessageDialog dialog;auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t clock=0;
            AudioMixer audio(root);RoomWorld world(room,stage,actor,motion,clock,audio,root);MoviePlayer movie(root,{},audio);EventVm events(room,inventory,dialog);GameplayInput gameplay(inventory,menu,dialog,&events);std::vector<unsigned> opening_sounds;gameplay.sound=[&](unsigned id){opening_sounds.push_back(id);};
            events.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes.at(1));return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};events.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};events.audio_active=[&](unsigned slot){return audio.active(slot);};events.audio_stop=[&](unsigned slot){audio.stop(slot);};events.movie_active=[&]{return movie.active();};
            auto tick=[&]{check(!gameplay.apply(input.tick()),"gamepad menu action quits game");events.check_floor(actor.position,actor.yaw);events.tick();gameplay.synchronize();world.tick();movie.tick();audio.mix();};
            auto press=[&](SDL_GameControllerButton b){button(input,joystick,b,true);tick();button(input,joystick,b,false);tick();};
            gameplay.video_active=[&]{return movie.active();};gameplay.skip_video=[&]{movie.stop();};
            movie.start(0);press(SDL_CONTROLLER_BUTTON_START);check(!movie.active(),"SDL controller Start does not skip active original FMV");
            movie.start(0);key(input,SDL_SCANCODE_RETURN,true);tick();key(input,SDL_SCANCODE_RETURN,false);tick();check(!movie.active(),"keyboard OK does not skip active original FMV");
            events.initialize();tick();press(SDL_CONTROLLER_BUTTON_Y);check(!menu.active,"controller opens inventory during cinematic");
            // system.c:545 bhSysCallGame: only pad_ps 0x10000 (SELECT) sets cb_flg 0x10000000. OK (A/Return), Start and Space are not a skip.
            check((events.flags[10][0]&4)&&!(events.flags[10][0]&0x10000000),"cell cinematic not running before the skip checks");
            press(SDL_CONTROLLER_BUTTON_A);check((events.flags[10][0]&4)&&!(events.flags[10][0]&0x10000000),"controller OK requested the cinematic skip");
            key(input,SDL_SCANCODE_RETURN,true);tick();key(input,SDL_SCANCODE_RETURN,false);tick();check((events.flags[10][0]&4)&&!(events.flags[10][0]&0x10000000),"keyboard OK requested the cinematic skip");
            press(SDL_CONTROLLER_BUTTON_START);check((events.flags[10][0]&4)&&!(events.flags[10][0]&0x10000000),"controller Start requested the cinematic skip");
            key(input,SDL_SCANCODE_SPACE,true);tick();key(input,SDL_SCANCODE_SPACE,false);tick();check((events.flags[10][0]&4)&&!(events.flags[10][0]&0x10000000),"keyboard Space requested the cinematic skip");
            press(SDL_CONTROLLER_BUTTON_BACK);check((events.flags[10][0]&0x10000000)||!(events.flags[10][0]&4),"controller Select did not request the cinematic skip");for(unsigned i=0;i<60;++i)tick();
            check(dialog.active()&&dialog.index()==4&&world.event_camera_index==37,"controller Select skip does not reach original cell hint");press(SDL_CONTROLLER_BUTTON_A);const auto before_auto=opening_sounds.size();for(unsigned i=0;i<40;++i)tick();check(menu.active,"original hint does not automatically open inventory");check(opening_sounds.size()>before_auto&&opening_sounds[before_auto]==3,"automatic inventory uses close sound");
            button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);tick();for(unsigned i=0;i<30;++i)tick();check(menu.commands&&inventory.weapon()==0,"held A auto-equips or advances multiple actions");button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);tick();press(SDL_CONTROLLER_BUTTON_A);
            check(inventory.weapon()==1&&menu.active,"A does not equip lighter");for(unsigned i=0;i<4;++i)tick();press(SDL_CONTROLLER_BUTTON_B);press(SDL_CONTROLLER_BUTTON_B);for(unsigned i=0;i<40&&menu.active;++i)tick();check(!menu.active,"B does not close the status screen after equipping"); /* WeaponSet keeps the status screen open; CursorMove-Cancel enters MainCommand, the second cancel is StatusCancel. */bool voice_advanced=false;for(unsigned i=0;i<3000;++i){tick();if(!voice_advanced&&dialog.active()&&audio.active(1)){for(unsigned page=0;page<8&&dialog.active();++page)gameplay.menu_input(MenuInput::Confirm);check(!dialog.active()&&!audio.active(1),"Rodrigo voice keeps playing after confirming its dialogue");voice_advanced=true;}}check(voice_advanced,"real Rodrigo voice/dialogue advance was not exercised");check(std::find(audio.played.begin(),audio.played.end(),"VOICE1/0016")!=audio.played.end(),"original Rodrigo VOICE1/0016 was not played");check(events.errors.empty()&&!world.player_hidden&&!world.player_scripted&&!(events.flags[5][0]&4),"controller equipment path fails to restore game control");
            press(SDL_CONTROLLER_BUTTON_Y);for(unsigned i=0;i<40;++i)tick();press(SDL_CONTROLLER_BUTTON_B);check(menu.active&&menu.nav,"first B does not return to the status command bar");press(SDL_CONTROLLER_BUTTON_B);for(unsigned i=0;i<40&&menu.active;++i)tick();check(!menu.active&&!events.menu_open,"B does not finish closing inventory");
            /* Kartentask: sub1.c:3141 setzt st_flg 0x40000, bhCheckSubTask (system.c:561,
               Maske 0x1C040008) sperrt damit Statusbildschirm und Interaktion; das Pad liest
               allein MapPadMain (map.c:569). Inventar darf nicht ueber der Karte aufgehen,
               Escape (Cancel+Pause, nie Quit) darf das Programm nicht beenden. Positivkontrolle ohne Bit. */
            events.flags[5][0]|=0x40000u;
            press(SDL_CONTROLLER_BUTTON_Y);check(!menu.active&&!events.menu_open,"controller inventory opened over the map task");
            key(input,SDL_SCANCODE_TAB,true);tick();key(input,SDL_SCANCODE_TAB,false);tick();check(!menu.active,"keyboard inventory opened over the map task");
            key(input,SDL_SCANCODE_ESCAPE,true);{const auto f=input.tick();check(f.hit(Action::Pause)&&f.hit(Action::Cancel)&&!f.hit(Action::Quit),"Escape is not Cancel+Pause in the default bindings");check(!gameplay.apply(f),"Escape quits the program while the map task runs");}key(input,SDL_SCANCODE_ESCAPE,false);tick();
            gameplay.menu_input(MenuInput::Open);check(!menu.active,"replay Open bypassed the map lock");
            check((events.flags[5][0]&0x40000u)&&!dialog.active(),"the input lock changed st_flg 0x40000 or opened a dialogue");
            events.flags[5][0]&=~0x40000u;
            press(SDL_CONTROLLER_BUTTON_Y);for(unsigned i=0;i<40;++i)tick();check(menu.active,"inventory stays locked after the map task released st_flg 0x40000");
            press(SDL_CONTROLLER_BUTTON_B);press(SDL_CONTROLLER_BUTTON_B);for(unsigned i=0;i<40&&menu.active;++i)tick();check(!menu.active&&!events.menu_open,"B does not close the inventory after the map lock check");
            GameUi pickup_ui(root);events.item_message=[&](unsigned index,unsigned item){auto message=pickup_ui.system_message(index);for(unsigned i=0;i+1<message.size();++i)if(message[i]==0xff03&&message[i+1]==0xffff)message[++i]=std::uint16_t(item);return message;};
            // Real room pickup routed through the same keyboard/controller OK.
            // D02 GetItem über die echte Raumaufnahme (RM_0000): Controller-OK ->
            // Frage 157 mit Modell, Ja -> Messer im Inventar, 158, nach der
            // Quittung freigegebenes Raumitem und Steuerung; Tastatur dasselbe für
            // die Munition.
            section("D02 Raumaufnahme Controller/Tastatur",[&]{
                // GetItem testmode 1/3 warten auf !(statusflg & 0x40): erst nach
                // der Einfahrt (SpriteH) zählt die Antwort, erst nach der Quittung
                // fährt StatusCancel den Bildschirm aus.
                auto settle_slide=[&]{for(unsigned i=0;i<60&&menu.slide.pad_locked();++i)tick();check(!menu.slide.pad_locked(),"pickup screen never released the pad");};
                auto settle_close=[&]{for(unsigned i=0;i<60&&menu.active;++i)tick();};
                gameplay.interact=[&]{events.interact(actor.position,actor.yaw,world.floor_at(actor.position));};actor.position={19,0,55};actor.yaw=0;press(SDL_CONTROLLER_BUTTON_A);check(dialog.index()==157&&dialog.choices()==2&&menu.active&&menu.pickup==8&&events.menu_open,"controller pickup status/model prompt missing");
                press(SDL_CONTROLLER_BUTTON_A);check(!inventory.contains(8)&&dialog.index()==157,"pickup answer accepted while the status screen slides in (statusflg 0x40)");
                settle_slide();press(SDL_CONTROLLER_BUTTON_A);check(inventory.contains(8)&&dialog.index()==158&&menu.active&&menu.pickup==8,std::string("controller pickup result closes model too early: knife=")+std::to_string(inventory.contains(8))+" dialog="+std::to_string(dialog.index())+" active="+std::to_string(menu.active)+" pickup="+std::to_string(menu.pickup));
                while(dialog.active())press(SDL_CONTROLLER_BUTTON_A);settle_close();check(!(events.interactions[7].flags&1)&&!menu.active&&!menu.pickup,std::string("controller pickup completion does not restore room control: item_flag=")+std::to_string(events.interactions[7].flags&1)+" active="+std::to_string(menu.active)+" pickup="+std::to_string(menu.pickup));
                const auto& ammo_area=events.interactions[4];actor.position={ammo_area.position.x+ammo_area.extent.x/2,0,ammo_area.position.z+ammo_area.extent.z/2+2};auto keyboard_ok=[&]{key(input,SDL_SCANCODE_RETURN,true);tick();key(input,SDL_SCANCODE_RETURN,false);tick();};
                // Der Spieler laeuft mit gehaltener Richtung auf das Item zu und drueckt
                // OK, waehrend die Taste gehalten bleibt. bhControlMessage (message.c:
                // 269-285) bewegt mes_sel nur mit der Druckflanke sys->pad_ps 0x4/0x8
                // (pad.c:275 `pad & ~pad_oncpy`) und kennt keinen Wiederholzaehler, also
                // bleibt die Antwort auf dem Standard, bis eine neue Flanke kommt.
                key(input,SDL_SCANCODE_RIGHT,true);for(unsigned i=0;i<40;++i)tick(); /* laenger als repeat_delay: die Host-Wiederholung laeuft */
                keyboard_ok();check(dialog.choices()==2&&menu.active&&menu.pickup==12,"keyboard pickup status/model prompt missing");
                settle_slide();const auto before_hold=opening_sounds.size();for(unsigned i=0;i<40;++i)tick();
                check(dialog.selection==0&&std::find(opening_sounds.begin()+before_hold,opening_sounds.end(),2u)==opening_sounds.end(),std::string("held direction moves the pickup answer: selection=")+std::to_string(dialog.selection));
                key(input,SDL_SCANCODE_RIGHT,false);tick();const auto before_edge=opening_sounds.size();key(input,SDL_SCANCODE_RIGHT,true);tick();
                check(dialog.selection==1&&std::find(opening_sounds.begin()+before_edge,opening_sounds.end(),2u)!=opening_sounds.end(),"new direction edge does not move the pickup answer");
                key(input,SDL_SCANCODE_RIGHT,false);tick();key(input,SDL_SCANCODE_LEFT,true);tick();check(dialog.selection==0,"left edge does not return to the default answer");key(input,SDL_SCANCODE_LEFT,false);tick();
                // Vorwaerts (W/Pfeil hoch) belegt Forward und Up; message.c liest die
                // senkrechten Padbits 0x1/0x2 nicht, die Auswahlfelder stehen waagerecht.
                key(input,SDL_SCANCODE_W,true);for(unsigned i=0;i<40;++i)tick();check(dialog.selection==0,"forward key moves the pickup answer");key(input,SDL_SCANCODE_W,false);tick();
                settle_slide();keyboard_ok();check(inventory.contains(12),"keyboard pickup Yes failed");while(dialog.active())keyboard_ok();settle_close();check(!(events.interactions[4].flags&1)&&!menu.active,"keyboard picked ammo remains available or the screen stays open");});
            // Schließt die Aufnahme nicht, bleibt der Statusbildschirm offen und
            // blockiert die folgenden Eingabeprüfungen; dann wird er für die
            // Diagnose geschlossen (keine Aussage über die Aufnahme).
            if(menu.active){menu.reset(inventory);events.menu_open=false;dialog=MessageDialog{};}
            if(!inventory.contains(8))check(inventory.acquire(8,1),"diagnostic knife acquisition failed");
            auto weapon_body=PlayerAsset::load(root,10);CombatController combat(root);unsigned knife=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inventory.slots()[i])==8)knife=i;check(knife&&inventory.equip(knife),"acquired knife cannot equip");unsigned interactions=0;gameplay.interact=[&]{++interactions;};
            auto weapon_tick=[&]{const auto f=input.tick();gameplay.apply(f);combat.tick({f.down(Action::Aim),f.down(Action::Attack)||f.hit(Action::Attack),f.turn(),f.move(),f.hit(Action::NextTarget)},menu.active||dialog.active(),inventory,weapon_body,actor,motion,clock,&world,audio);};
            // PORTIERUNGSLÜCKE (Policy 2026-10-09): Der gezählte Messerhieb
            // (combat.attacks) kam aus dem Spielerkampf in player_combat.inc;
            // der Generator ist gesperrt, siehe native/player_combat_core.c.
            // Geprüft bleibt die Eingabeverteilung des Hosts: solange Zielen
            // gehalten wird, geht OK an die Waffe und löst keine
            // Raumaufnahme aus (input.cpp apply, Aim-Sperre). Dass dabei kein
            // Angriff entsteht, ist die dokumentierte Lücke.
            button(input,joystick,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,true);for(unsigned i=0;i<10;++i)weapon_tick();button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);weapon_tick();check(!interactions,"controller attack dispatches pickup instead of knife");check(!combat.attacks&&!combat.shots,"combat gap produced an attack");button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);button(input,joystick,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);for(unsigned i=0;i<40;++i)weapon_tick();
            key(input,SDL_SCANCODE_R,true);for(unsigned i=0;i<10;++i)weapon_tick();key(input,SDL_SCANCODE_RETURN,true);weapon_tick();check(!interactions,"keyboard attack dispatches pickup instead of knife");check(!combat.attacks&&!combat.shots,"combat gap produced an attack from the keyboard");key(input,SDL_SCANCODE_R,false);key(input,SDL_SCANCODE_RETURN,false);for(unsigned i=0;i<40;++i)weapon_tick();
            const auto body=PlayerAsset::load(root,10);SceneSpawn walker{{100,0,100},0};std::size_t walker_motion=42;std::int64_t walker_frame=0;
            axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,32767);const auto backward=input.tick();body.locomotion(walker,walker_motion,walker_frame,backward.move(),backward.down(Action::Run),backward.turn());
            check(walker_motion==16&&walker.position.z>100&&walker.yaw==0,"SDL gamepad backward does not animate original backstep");axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,0);input.tick();
            const auto direction_yaw=walker.yaw;key(input,SDL_SCANCODE_A,true);auto left=input.tick();body.locomotion(walker,walker_motion,walker_frame,0,false,left.turn());check(walker.yaw==((direction_yaw+int(182.04445f*4.5f))&65535),"A rotates right");key(input,SDL_SCANCODE_A,false);input.tick();key(input,SDL_SCANCODE_D,true);auto right=input.tick();body.locomotion(walker,walker_motion,walker_frame,0,false,right.turn());check(walker.yaw==direction_yaw,"D rotates left");key(input,SDL_SCANCODE_D,false);input.tick();
            menu.active=menu.examining=true;menu.examine_yaw=0;button(input,joystick,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,true);for(unsigned i=0;i<30;++i)tick();check(menu.examine_yaw==30*512,"3D inspection is limited by menu repeat delay");button(input,joystick,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,false);tick();menu.active=menu.examining=false;
            std::cout<<"Original cell skip/dialog/inventory/lighter/GetItem pickup/control flow verified with SDL virtual gamepad.\n";
        }
        // Disconnect while moving; keyboard input must remain usable.
        axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,-32768);key(input,SDL_SCANCODE_J,true);input.tick();SDL_JoystickClose(joystick);joystick=nullptr;detach(id);pump(input);frame=input.tick();check(frame.move()==0&&frame.turn()==-1,"disconnect leaves movement stuck or clears keyboard");key(input,SDL_SCANCODE_J,false);
    }
    // Actual INI parsing: custom keyboard/gamepad buttons and axis inversion.
    const auto folder=std::filesystem::temp_directory_path()/("recvx-controls-"+std::to_string(SDL_GetTicks64()));std::filesystem::create_directory(folder);const auto file=folder/"controls.ini";InputConfig::write_defaults(file);
    {const auto written=InputConfig::read(file);check(written.describe(Action::Pause,false)=="P / Escape"&&written.describe(Action::Quit,false)=="F10",std::string("the shipped control template loses the Escape pause key or keeps Escape as quit: ")+written.describe(Action::Pause,false)+" / "+written.describe(Action::Quit,false));}
    std::ifstream source(file);std::string text((std::istreambuf_iterator<char>(source)),{});source.close();const auto replace=[&](const std::string& from,const std::string& to){auto at=text.find(from);check(at!=text.npos,"fixture binding missing");text.replace(at,from.size(),to);};
    replace("confirm = Return","confirm = E");replace("confirm = button:a","confirm = button:x");replace("axis:lefty-","axis:lefty+");std::ofstream(file)<<text;const auto custom=InputConfig::read(file);
    const int new_index=attach();joystick=SDL_JoystickOpen(new_index);const auto new_id=SDL_JoystickInstanceID(joystick);SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_GameControllerUpdate();
    {InputManager input(custom);pump(input);key(input,SDL_SCANCODE_RETURN,true);check(!input.tick().hit(Action::Confirm),"old keyboard binding remains active");key(input,SDL_SCANCODE_E,true);check(input.tick().hit(Action::Confirm),"keyboard remap not applied");key(input,SDL_SCANCODE_E,false);input.tick();button(input,joystick,SDL_CONTROLLER_BUTTON_X,true);check(input.tick().hit(Action::Confirm),"gamepad remap not applied");button(input,joystick,SDL_CONTROLLER_BUTTON_X,false);input.tick();axis(input,joystick,SDL_CONTROLLER_AXIS_LEFTY,32767);check(input.tick().down(Action::Forward),"axis inversion not applied");}
    SDL_JoystickClose(joystick);detach(new_id);
    // Older customized INIs gain only new actions; existing bindings survive.
    auto legacy=text;for(const auto binding:{"aim = R\n","attack = Left Ctrl, Return\n","aim = button:rightshoulder\n","attack = button:a\n","next_target = Q\n","next_target = button:leftshoulder\n"}){auto at=legacy.find(binding);check(at!=legacy.npos,"legacy fixture action missing");legacy.erase(at,std::string(binding).size());}std::ofstream(file)<<legacy;const auto upgraded=InputConfig::read(file);check(upgraded.describe(Action::Confirm,false)=="E"&&upgraded.describe(Action::Aim,false)=="R"&&upgraded.describe(Action::Attack,true)=="A"&&upgraded.describe(Action::NextTarget,true)=="LB"&&upgraded.describe(Action::NextTarget,false)=="Q","legacy config migration overwrites bindings or lacks combat");
    for(const auto bad:{"[settings]\ndeadzone = nan\n","[settings]\nrepeat_interval_ticks = 0\n","[keyboard]\nconfirm = NotARealKey\n","[gamepad]\nconfirm = button:fake\n","[settings]\nunknown = 1\n","[keyboard]\nconfirm = E\nconfirm = Q\n"}){std::ofstream(file)<<bad;bool rejected=false;try{InputConfig::read(file);}catch(const std::runtime_error&){rejected=true;}check(rejected,"malformed controls accepted");}
    std::filesystem::remove_all(folder);
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory section(s) failed; the remaining input checks passed.\n";return 1;}
    std::cout<<"Keyboard/config/controller axes, buttons, repeat, focus, hot-plug and disconnect tests passed.\n";return 0;
}catch(const std::exception& e){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<e.what()<<": "<<SDL_GetError()<<'\n';return 1;}}
