#include "debug_menu.h"
#include "user_paths.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
namespace recvx {
namespace {
std::string trim(std::string s){const auto first=s.find_first_not_of(" \t\r\n");return first==s.npos?std::string{}:s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);}
void panel(SoftwareRenderer& image,int x,int y,int w,int h,unsigned color){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)image.canvas().at(j*image.width()+i)=color;}
}
DebugConfig DebugConfig::read(const std::filesystem::path& path){
    if(!std::filesystem::exists(path)){write_defaults(path);return {};}
    if(std::filesystem::file_size(path)>4096)throw std::runtime_error("debug.ini exceeds 4096 bytes");
    std::ifstream input(path);if(!input)throw std::runtime_error("cannot read debug.ini: "+path.string());DebugConfig result;std::set<std::string> seen;std::string line,section;
    while(std::getline(input,line)){line=trim(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);if(section!="debug")throw std::runtime_error("unknown debug.ini section: "+section);continue;}
        const auto equals=line.find('=');if(section!="debug"||equals==line.npos)throw std::runtime_error("invalid debug.ini setting: "+line);
        const auto key=trim(line.substr(0,equals)),name=trim(line.substr(equals+1));
        if(!seen.insert(key).second)throw std::runtime_error("duplicate debug.ini setting: "+key);
        if(key=="hotkey"){result.hotkey=SDL_GetScancodeFromName(name.c_str());if(result.hotkey==SDL_SCANCODE_UNKNOWN)throw std::runtime_error("unknown debug hotkey: "+name);}
        else if(key=="enemy_grab"){if(name!="on"&&name!="off")throw std::runtime_error("debug enemy_grab expects on or off");result.enemy_grab=name=="on";result.enemy_grab_configured=true;}
        else if(key=="enemy_attacks"){if(name!="on"&&name!="off")throw std::runtime_error("debug enemy_attacks expects on or off");result.enemy_attacks=name=="on";result.enemy_attacks_configured=true;}
        else throw std::runtime_error("unknown debug.ini setting: "+key);
    }return result;
}
void DebugConfig::write_defaults(const std::filesystem::path& path){
    if(std::filesystem::exists(path))throw std::runtime_error("debug.ini already exists");if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());std::ofstream output(path);if(!output)throw std::runtime_error("cannot create debug.ini: "+path.string());
    // enemy_grab is left unset: the runtime then uses the original attacks in
    // interactive play (off only for --headless/--replay); "enemy_grab = off"
    // here is the user's explicit override.
    output<<"# RECVX debug menu. SDL key names; change hotkey and restart.\n[debug]\nhotkey = F2\n";output.close();if(!output)throw std::runtime_error("cannot write debug.ini");
}
std::filesystem::path DebugConfig::software_directory(const std::filesystem::path& executable){
    const auto directory=executable.parent_path();if(directory.filename()=="MacOS"&&directory.parent_path().filename()=="Contents"&&directory.parent_path().parent_path().extension()==".app")return directory.parent_path().parent_path().parent_path();return directory;
}
std::filesystem::path DebugConfig::default_path(){
    return platform_user_data_dir()/"debug.ini";
}
void DebugMenu::add(DebugAction action){
    if(action.id.empty()||!action.choices||!action.current||!action.execute)throw std::runtime_error("incomplete debug action");
    for(const auto& existing:actions_)if(existing.id==action.id)throw std::runtime_error("duplicate debug action: "+action.id);
    values_.push_back(action.current());actions_.push_back(std::move(action));
}
std::vector<DebugChoice> DebugMenu::choices(unsigned row)const{
    auto result=actions_.at(row).choices();std::set<unsigned> seen;for(const auto& choice:result)if(!seen.insert(choice.value).second)throw std::runtime_error("duplicate debug choice");return result;
}
void DebugMenu::toggle(){active_=!active_;if(!active_)return;selected_=std::min(selected_,unsigned(actions_.empty()?0:actions_.size()-1));status_.clear();for(unsigned i=0;i<actions_.size();++i)values_[i]=actions_[i].current();}
bool DebugMenu::event(const SDL_Event& event){
    if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&event.key.keysym.scancode==config_.hotkey){if(event.type==SDL_KEYDOWN&&!event.key.repeat)toggle();return true;}
    if(active_&&event.type==SDL_KEYDOWN&&!event.key.repeat&&(event.key.keysym.scancode==SDL_SCANCODE_PAGEUP||event.key.keysym.scancode==SDL_SCANCODE_PAGEDOWN)){step(event.key.keysym.scancode==SDL_SCANCODE_PAGEUP?-20:20);return true;}
    if(active_&&event.type==SDL_CONTROLLERBUTTONDOWN&&(event.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER||event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)){step(event.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER?-20:20);return true;}return false;
}
void DebugMenu::step(int amount){if(actions_.empty())return;const auto options=choices(selected_);if(options.empty())return;const auto found=std::find_if(options.begin(),options.end(),[&](const auto& c){return c.value==values_[selected_];});const auto index=found==options.end()?0:int(found-options.begin());const int count=int(options.size());values_[selected_]=options[(index+amount%count+count)%count].value;status_.clear();}
void DebugMenu::execute(){if(actions_.empty())return;try{const auto options=choices(selected_);if(std::none_of(options.begin(),options.end(),[&](const auto& c){return c.value==values_[selected_];}))throw std::runtime_error("selection is unavailable");status_=actions_[selected_].execute(values_[selected_]);}catch(const std::exception& e){status_=std::string("Fehler: ")+e.what();}}
void DebugMenu::select(const std::string& id,unsigned value){
    for(unsigned i=0;i<actions_.size();++i)if(actions_[i].id==id){const auto options=choices(i);if(std::none_of(options.begin(),options.end(),[&](const auto& c){return c.value==value;}))throw std::runtime_error("unavailable debug value for "+id);selected_=i;values_[i]=value;return;}throw std::runtime_error("unknown debug action: "+id);
}
void DebugMenu::input(MenuInput key){if(!active_)return;if(key==MenuInput::Cancel||key==MenuInput::Open){close();return;}if(actions_.empty())return;
    if(key==MenuInput::Up)selected_=(selected_+actions_.size()-1)%actions_.size();if(key==MenuInput::Down)selected_=(selected_+1)%actions_.size();
    if(key==MenuInput::Left)step(-1);if(key==MenuInput::Right)step(1);if(key==MenuInput::Confirm)execute();
}
void DebugMenu::apply(const InputFrame& frame){if(!active_)return;if(frame.hit(Action::Cancel)){close();return;}if(frame.hit(Action::Confirm)){input(MenuInput::Confirm);return;}
    for(const auto pair:{std::pair<Action,MenuInput>{Action::Up,MenuInput::Up},{Action::Down,MenuInput::Down},{Action::Left,MenuInput::Left},{Action::Right,MenuInput::Right}})if(frame.navigation(pair.first))input(pair.second);
}
void DebugMenu::draw(SoftwareRenderer& image,const GameUi& ui)const{
    if(!active_)return;for(auto& pixel:image.canvas())pixel=0xff000000|((pixel&0xfefefe)>>1);
    panel(image,24,24,592,432,0xff121c24);panel(image,24,24,592,3,0xff52c9dd);ui.label(image,"RECVX DEBUG",44,42,1);ui.label(image,std::string(SDL_GetScancodeName(config_.hotkey))+" / Zurueck: schliessen / Spiel pausiert",44,78,.65f);
    const unsigned first=selected_>=8?selected_-7:0;for(unsigned i=first;i<std::min(unsigned(actions_.size()),first+8);++i){const int y=114+int(i-first)*30;if(i==selected_)panel(image,36,y-2,568,29,0xff244657);{std::string label=actions_[i].label;while(!label.empty()&&ui.label_width(label,.68f)>192)label.pop_back();ui.label(image,label,44,y,.68f);} /* Wertspalte ab x 244: lange Namen abschneiden statt ueberlagern */
        const auto options=choices(i);const auto choice=std::find_if(options.begin(),options.end(),[&](const auto& c){return c.value==values_[i];});const auto value=choice==options.end()?"-":choice->label;ui.label(image,value.substr(0,38),244,y,.62f,i==selected_?0xff9ae5f0:0xffffffff);}
    ui.label(image,"Hoch/Runter: Auswahl / Links/Rechts: Wert / OK: anwenden",44,364,.58f);ui.label(image,"PageUp/PageDown oder LB/RB: 20 Werte weiter",44,386,.58f);
    if(!actions_.empty())ui.label(image,actions_[selected_].help.substr(0,84),44,410,.5f,0xffb7d1db);ui.label(image,status_.substr(0,90),44,432,.5f,0xfff1cc73);
}
}
