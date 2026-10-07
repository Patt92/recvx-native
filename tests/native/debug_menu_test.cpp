#define SDL_MAIN_HANDLED
#include "debug_menu.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static SDL_Event key(SDL_Scancode code,bool down,bool repeat=false){SDL_Event event{};event.type=down?SDL_KEYDOWN:SDL_KEYUP;event.key.keysym.scancode=code;event.key.repeat=repeat;return event;}
int main(int argc,char** argv){try{
    SDL_SetMainReady();check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL initialization failed");
    const auto dir=std::filesystem::temp_directory_path()/("recvx-debug-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(dir);const auto file=dir/"debug.ini";
    auto config=DebugConfig::read(file);check(!config.enemy_grab&&config.hotkey==SDL_SCANCODE_F2&&std::filesystem::is_regular_file(file),"default file/hotkey missing");
    std::ofstream(file)<<"[debug]\nhotkey = F6\n";config=DebugConfig::read(file);check(config.hotkey==SDL_SCANCODE_F6&&!config.enemy_grab,"legacy config loses debug default");std::ofstream(file)<<"[debug]\nhotkey = F6\nenemy_grab = on\n";config=DebugConfig::read(file);check(config.enemy_grab,"explicit grab debug override missing");std::ofstream(file)<<"[debug]\nhotkey = MissingKey\n";bool rejected=false;try{DebugConfig::read(file);}catch(...){rejected=true;}check(rejected,"invalid hotkey silently accepted");
    check(DebugConfig::software_directory(std::filesystem::u8path("/games/RECVX Native.app/Contents/MacOS/recvx-runtime"))==std::filesystem::u8path("/games"),"Mac app config path wrong");check(DebugConfig::software_directory(dir/"recvx-runtime.exe")==dir,"Windows/direct executable config path wrong");
    unsigned current=10,calls=0;DebugMenu menu(config);menu.add({"character","Character","Apply with OK",[]{return std::vector<DebugChoice>{{10,"Claire"},{11,"Chris"}};},[&]{return current;},[&](unsigned value){current=value;++calls;return "changed";}});
    check(!menu.event(key(SDL_SCANCODE_F2,true))&&!menu.active(),"old hotkey still opens");check(menu.event(key(SDL_SCANCODE_F6,true))&&menu.active(),"remapped hotkey fails");check(menu.event(key(SDL_SCANCODE_F6,true,true))&&menu.active(),"repeat toggles menu");check(menu.event(key(SDL_SCANCODE_F6,false))&&menu.active(),"keyup toggles menu");menu.input(MenuInput::Right);check(current==10&&!calls,"browsing applies mutation");menu.input(MenuInput::Confirm);check(current==11&&calls==1,"apply callback missing");
    InputFrame frame;frame.held[unsigned(Action::Confirm)]=true;for(unsigned i=0;i<30;++i)menu.apply(frame);check(calls==1,"held OK repeats mutation");frame={};frame.pressed[unsigned(Action::Cancel)]=true;menu.apply(frame);check(!menu.active(),"cancel does not close");
    menu.add({"test","Extension","Registry test",[]{return std::vector<DebugChoice>{{0,"Run"}};},[]{return 0;},[](unsigned)->std::string{throw std::runtime_error("resource unavailable");}});menu.toggle();menu.select("test",0);menu.input(MenuInput::Confirm);check(menu.active()&&menu.status().find("resource unavailable")!=std::string::npos,"operation failure crashes/closes debug menu");
    rejected=false;try{menu.select("character",99);}catch(...){rejected=true;}check(rejected,"invalid target accepted");
    // Suppress gameplay edges/held movement on modal handover; release re-arms it.
    InputManager input(InputConfig::defaults());input.event(key(SDL_SCANCODE_RETURN,true));input.event(key(SDL_SCANCODE_W,true));input.suppress_held();auto physical=input.tick();check(!physical.hit(Action::Confirm)&&!physical.down(Action::Forward),"modal handover leaks input");input.event(key(SDL_SCANCODE_RETURN,false));input.event(key(SDL_SCANCODE_W,false));input.tick();input.event(key(SDL_SCANCODE_RETURN,true));check(input.tick().hit(Action::Confirm),"release does not re-arm input");
    {Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(30,6);StatusMenu status;status.active=status.examining=true;status.selected=2;status.message_source=[](unsigned){return Message{0xffff};};status.reset(inv);check(!status.active&&!status.examining&&inv.slots()[2]==Inventory::pack(13,6)&&bool(status.message_source),"debug status reset loses source or case exit rule");}
    if(argc>1){GameUi ui(std::filesystem::u8path(argv[1]));SoftwareRenderer image(640,480);ui.draw_title(image);menu.draw(image,ui);if(argc>2)image.save_bmp(std::filesystem::u8path(argv[2]));check(image.pixels()[24*640+24]==0xff52c9dd,"debug overlay absent");}
    std::filesystem::remove_all(dir);SDL_Quit();std::cout<<"Default/remapped hotkey, bundle paths, repeat guard, registry extension, failures and modal input suppression passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';SDL_Quit();return 1;}}
