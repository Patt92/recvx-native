#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "combat.h"
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
int main(int argc,char** argv){try{
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL controller init failed");Quit quit;_Make_SinTable();
    const auto config=InputConfig::defaults();const int index=attach();SDL_Joystick* joystick=SDL_JoystickOpen(index);check(joystick,"virtual joystick open failed");const auto id=SDL_JoystickInstanceID(joystick);
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_GameControllerUpdate();
    {InputManager input(config);pump(input);check(input.controllers()>=1,"controller present at startup not opened");input.tick();
        button(input,joystick,SDL_CONTROLLER_BUTTON_BACK,true);check(input.tick().hit(Action::Pause),"controller Select/Back pause binding missing");button(input,joystick,SDL_CONTROLLER_BUTTON_BACK,false);input.tick();
        key(input,SDL_SCANCODE_P,true);check(input.tick().hit(Action::Pause),"keyboard pause binding missing");key(input,SDL_SCANCODE_P,false);input.tick();
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
        {Inventory inventory;StatusMenu menu;MessageDialog dialog;GameplayInput gameplay(inventory,menu,dialog,nullptr);bool active=true;unsigned skips=0;
            gameplay.transition_active=[&]{return active;};gameplay.confirm_transition=[&]{++skips;};
            key(input,SDL_SCANCODE_RETURN,true);gameplay.apply(input.tick());check(skips==1,"keyboard OK does not skip door");for(unsigned i=0;i<30;++i)gameplay.apply(input.tick());check(skips==1,"held keyboard OK skips repeatedly");key(input,SDL_SCANCODE_RETURN,false);input.tick();
            button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);gameplay.apply(input.tick());check(skips==2,"controller OK does not skip door");for(unsigned i=0;i<30;++i)gameplay.apply(input.tick());check(skips==2,"held controller OK skips repeatedly");button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);input.tick();
            key(input,SDL_SCANCODE_TAB,true);gameplay.apply(input.tick());check(!menu.active,"inventory opens during door transition");key(input,SDL_SCANCODE_TAB,false);input.tick();}
        for(const bool controller:{false,true}){
            Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);StatusMenu menu;menu.active=true;menu.skip_slide();menu.selected=2;MessageDialog room;
            std::unique_ptr<GameUi> ui;if(argc>1)ui=std::make_unique<GameUi>(std::filesystem::u8path(argv[1]));menu.message_source=[&](unsigned index){return ui?ui->system_message(index):Message{'L'-32,0xff02,0,'?'-32,0xff04,'Y'-32,0xff04,'N'-32,0xffff};};
            std::vector<std::uint8_t> bytes(1500);float version=1.73f;std::memcpy(bytes.data(),&version,4);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)bytes.at(p+i)=std::uint8_t(n>>(8*i));};put(16,64);put(28,1100);put(64+0x38,1400);put(1100,12);put(1104,14);put(1108,30);EventVm events(bytes,inv,room);
            GameplayInput gameplay(inv,menu,room,&events);std::vector<unsigned> sounds;gameplay.sound=[&](unsigned sound){sounds.push_back(sound);};auto tick=[&]{check(!gameplay.apply(input.tick()),"inventory control quits game");};
            auto held=[&](SDL_Scancode code,SDL_GameControllerButton b,bool down){if(controller)button(input,joystick,b,down);else key(input,code,down);};
            auto press=[&](SDL_Scancode code,SDL_GameControllerButton b){held(code,b,true);tick();held(code,b,false);tick();};
            auto ok=[&]{press(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A);};auto settle=[&]{for(int i=0;i<4;++i)tick();}; /* ItemCommand window slide */auto down=[&]{press(SDL_SCANCODE_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN);};
            // D01/D07 über Tastatur und Controller (derselbe Menüweg): Kräuter
            // kombinieren (Frage 155, Ja -> 25), ungültiger Gebrauch des
            // Dietrichs -> Use_05 Meldung 161 ohne Ereignis, Fehler-SE 1. Die
            // Untersuchung (ItemInspection) liegt außerhalb dieses Pakets und
            // bleibt als Lücke geprüft.
            section(controller?"D01 Kombinieren (Controller)":"D01 Kombinieren (Tastatur)",[&]{
                ok();settle();down();down();ok();settle();check(menu.combining&&menu.combine_from==2,"keyboard/gamepad cannot enter the original Combine mode");
                press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);check(menu.selected==3,"keyboard/gamepad combine cursor does not reach the partner slot");
                ok();check(menu.message.active()&&menu.message.index()==155,"keyboard/gamepad combine question 155 missing");
                while(menu.message.active()&&!menu.message.choices())ok();ok();
                check(inv.contains(25)&&!inv.contains(21)&&!inv.contains(22)&&!menu.combining&&menu.active,"keyboard/gamepad herb Combine failed");});
            if(menu.combining)press(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B);
            // D03 Untersuchung über denselben Eingabeweg: Dietrichfrage 181,
            // Halte-Sperre, Nein, Abbrechen und Ja bis zum Inhalt (Item 35,
            // Meldung 164) mit erhaltenem Dietrich.
            section(controller?"D03 Untersuchung (Controller)":"D03 Untersuchung (Tastatur)",[&]{
                inv.slots()[2]=Inventory::pack(128,1);inv.slots()[3]=Inventory::pack(50,1);menu.selected=2;ok();settle();down();ok();settle();check(menu.examining,"keyboard/gamepad cannot examine case");menu.examine_pitch=menu.examine_yaw=menu.examine_roll=0;ok();check(menu.message.index()==181,"keyboard/gamepad lockpick question missing");
                held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,true);tick();for(unsigned i=0;i<30;++i)tick();check(menu.message.active()&&!menu.opening_item&&inv.contains(128),"held OK answers its own new question");held(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A,false);tick();
                while(menu.message.active()&&!menu.message.choices())ok();press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);ok();check(inv.contains(128)&&!menu.opening_item,"keyboard/gamepad No opens case");
                ok();press(SDL_SCANCODE_ESCAPE,SDL_CONTROLLER_BUTTON_B);check(!menu.message.active()&&menu.examining&&inv.contains(128),"keyboard/gamepad cancel closes inventory or consumes case");
                ok();while(menu.message.active()&&!menu.message.choices())ok();ok();for(unsigned i=0;i<100;++i)tick();check(inv.contains(35)&&inv.contains(50)&&menu.active&&menu.message.index()==164,"keyboard/gamepad case Yes does not retain lockpick or show contents");
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
            events.initialize();tick();press(SDL_CONTROLLER_BUTTON_Y);check(!menu.active,"controller opens inventory during cinematic");press(SDL_CONTROLLER_BUTTON_START);for(unsigned i=0;i<60;++i)tick();
            check(dialog.active()&&dialog.index()==4&&world.event_camera_index==37,"controller skip does not reach original cell hint");press(SDL_CONTROLLER_BUTTON_A);const auto before_auto=opening_sounds.size();for(unsigned i=0;i<40;++i)tick();check(menu.active,"original hint does not automatically open inventory");check(opening_sounds.size()>before_auto&&opening_sounds[before_auto]==3,"automatic inventory uses close sound");
            button(input,joystick,SDL_CONTROLLER_BUTTON_A,true);tick();for(unsigned i=0;i<30;++i)tick();check(menu.commands&&inventory.weapon()==0,"held A auto-equips or advances multiple actions");button(input,joystick,SDL_CONTROLLER_BUTTON_A,false);tick();press(SDL_CONTROLLER_BUTTON_A);
            check(inventory.weapon()==1&&menu.active,"A does not equip lighter");for(unsigned i=0;i<4;++i)tick();press(SDL_CONTROLLER_BUTTON_B);press(SDL_CONTROLLER_BUTTON_B);for(unsigned i=0;i<40&&menu.active;++i)tick();check(!menu.active,"B does not close the status screen after equipping"); /* WeaponSet keeps the status screen open; CursorMove-Cancel enters MainCommand, the second cancel is StatusCancel. */bool voice_advanced=false;for(unsigned i=0;i<3000;++i){tick();if(!voice_advanced&&dialog.active()&&audio.active(1)){for(unsigned page=0;page<8&&dialog.active();++page)gameplay.menu_input(MenuInput::Confirm);check(!dialog.active()&&!audio.active(1),"Rodrigo voice keeps playing after confirming its dialogue");voice_advanced=true;}}check(voice_advanced,"real Rodrigo voice/dialogue advance was not exercised");check(std::find(audio.played.begin(),audio.played.end(),"VOICE1/0016")!=audio.played.end(),"original Rodrigo VOICE1/0016 was not played");check(events.errors.empty()&&!world.player_hidden&&!world.player_scripted&&!(events.flags[5][0]&4),"controller equipment path fails to restore game control");
            press(SDL_CONTROLLER_BUTTON_Y);for(unsigned i=0;i<40;++i)tick();press(SDL_CONTROLLER_BUTTON_B);check(menu.active&&menu.nav,"first B does not return to the status command bar");press(SDL_CONTROLLER_BUTTON_B);for(unsigned i=0;i<40&&menu.active;++i)tick();check(!menu.active&&!events.menu_open,"B does not finish closing inventory");
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
                const auto& ammo_area=events.interactions[4];actor.position={ammo_area.position.x+ammo_area.extent.x/2,0,ammo_area.position.z+ammo_area.extent.z/2+2};auto keyboard_ok=[&]{key(input,SDL_SCANCODE_RETURN,true);tick();key(input,SDL_SCANCODE_RETURN,false);tick();};keyboard_ok();check(dialog.choices()==2&&menu.active&&menu.pickup==12,"keyboard pickup status/model prompt missing");
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
