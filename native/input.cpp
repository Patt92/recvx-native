#include "input.h"
#include <algorithm>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace recvx {
namespace {
constexpr const char* names[]={"forward","back","turn_left","turn_right","run","inventory","confirm","cancel","up","down","left","right","skip","pause","quit","wireframe","aim","attack","next_target"};
const char* defaults_text=R"INI(# RECVX controls: edit and restart. SDL scancode names use physical key positions.
# Multiple alternatives are separated by commas. 'none' disables this device's binding.
[keyboard]
forward = I, W, Up
back = K, S, Down
turn_left = J, A, Left
turn_right = L, D, Right
run = Left Shift, Right Shift
inventory = Tab
confirm = Return
cancel = Escape
up = Up, W
down = Down, S
left = Left, A
right = Right, D
skip = Space
pause = P
quit = Escape, F10
wireframe = F3
aim = R
attack = Left Ctrl, Return
next_target = Q

[gamepad]
forward = axis:lefty-, button:dpup
back = axis:lefty+, button:dpdown
turn_left = axis:leftx-, button:dpleft
turn_right = axis:leftx+, button:dpright
run = button:b, axis:righttrigger+
inventory = button:y
confirm = button:a
cancel = button:b
up = axis:lefty-, button:dpup
down = axis:lefty+, button:dpdown
left = axis:leftx-, button:dpleft
right = axis:leftx+, button:dpright
skip = button:start
pause = button:back
quit = none
wireframe = none
aim = button:rightshoulder
attack = button:a
next_target = button:leftshoulder

[settings]
deadzone = 0.25
menu_threshold = 0.55
repeat_delay_ticks = 18
repeat_interval_ticks = 5
)INI";
std::string trim(std::string s){const auto first=s.find_first_not_of(" \t\r\n");if(first==s.npos)return {};return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);}
unsigned action(const std::string& name){for(unsigned i=0;i<action_count;++i)if(name==names[i])return i;throw std::runtime_error("unknown input action: "+name);}
bool navigation(unsigned i){return i>=unsigned(Action::Up)&&i<=unsigned(Action::Right);}
InputConfig parse(std::istream& stream){InputConfig out;std::string section,line;std::set<std::string> seen;unsigned lineno=0;
    while(std::getline(stream,line)){++lineno;if(line.size()>4096)throw std::runtime_error("controls line too long");line=trim(line.substr(0,line.find_first_of("#;")));if(line.empty())continue;
        try{if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);if(section!="keyboard"&&section!="gamepad"&&section!="settings")throw std::runtime_error("unknown section");continue;}
            const auto equals=line.find('=');if(equals==line.npos||section.empty())throw std::runtime_error("expected section and key = value");const auto key=trim(line.substr(0,equals)),value=trim(line.substr(equals+1));
            if(!seen.insert(section+"/"+key).second)throw std::runtime_error("duplicate binding/setting");
            if(section=="settings"){std::size_t used=0;if(key=="deadzone"||key=="menu_threshold"){const auto n=std::stof(value,&used);if(used!=value.size()||!std::isfinite(n)||n<.05f||n>.95f)throw std::runtime_error("axis threshold must be 0.05..0.95");(key=="deadzone"?out.deadzone:out.menu_threshold)=n;}
                else if(key=="repeat_delay_ticks"||key=="repeat_interval_ticks"){if(value.empty()||value.find_first_not_of("0123456789")!=value.npos)throw std::runtime_error("repeat ticks must be integers");const auto n=std::stoul(value);if(!n||n>600)throw std::runtime_error("repeat ticks must be 1..600");(key=="repeat_delay_ticks"?out.repeat_delay:out.repeat_interval)=unsigned(n);}else throw std::runtime_error("unknown setting");continue;}
            const auto index=action(key);if(value=="none")continue;if(value.empty())throw std::runtime_error("empty binding; use none");std::istringstream values(value);std::string item;unsigned count=0;
            while(std::getline(values,item,',')){if(++count>16)throw std::runtime_error("too many alternatives");item=trim(item);InputBinding binding{InputBinding::Kind::Key,0,1};
                if(section=="keyboard"){binding.code=SDL_GetScancodeFromName(item.c_str());if(binding.code==SDL_SCANCODE_UNKNOWN)throw std::runtime_error("unknown keyboard scancode: "+item);}
                else if(item.rfind("button:",0)==0){binding.kind=InputBinding::Kind::Button;binding.code=SDL_GameControllerGetButtonFromString(item.substr(7).c_str());if(binding.code==SDL_CONTROLLER_BUTTON_INVALID)throw std::runtime_error("unknown gamepad button: "+item);}
                else if(item.rfind("axis:",0)==0&&item.size()>6&&(item.back()=='+'||item.back()=='-')){binding.kind=InputBinding::Kind::Axis;binding.direction=item.back()=='+'?1:-1;binding.code=SDL_GameControllerGetAxisFromString(item.substr(5,item.size()-6).c_str());if(binding.code==SDL_CONTROLLER_AXIS_INVALID)throw std::runtime_error("unknown gamepad axis: "+item);}
                else throw std::runtime_error("expected button:name or axis:name+/-");out.bindings[index].push_back(binding);}
            if(value.back()==',')throw std::runtime_error("empty trailing alternative");
        }catch(const std::exception& e){throw std::runtime_error("controls line "+std::to_string(lineno)+": "+e.what());}}
    for(const auto* device:{"keyboard","gamepad"})for(auto name:names)if(!seen.count(std::string(device)+"/"+name)){
        if(std::string(name)=="aim"||std::string(name)=="attack"||std::string(name)=="next_target"||std::string(name)=="pause"){
            const bool pad=std::string(device)=="gamepad";
            if(std::string(name)=="pause"){out.bindings[unsigned(Action::Pause)].push_back({pad?InputBinding::Kind::Button:InputBinding::Kind::Key,pad?int(SDL_CONTROLLER_BUTTON_BACK):int(SDL_SCANCODE_P),1});continue;}
            if(std::string(name)=="aim")out.bindings[unsigned(Action::Aim)].push_back({pad?InputBinding::Kind::Button:InputBinding::Kind::Key,pad?int(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER):int(SDL_SCANCODE_R),1});
            else if(std::string(name)=="next_target")out.bindings[unsigned(Action::NextTarget)].push_back({pad?InputBinding::Kind::Button:InputBinding::Kind::Key,pad?int(SDL_CONTROLLER_BUTTON_LEFTSHOULDER):int(SDL_SCANCODE_Q),1});
            else {out.bindings[unsigned(Action::Attack)].push_back({pad?InputBinding::Kind::Button:InputBinding::Kind::Key,pad?int(SDL_CONTROLLER_BUTTON_A):int(SDL_SCANCODE_LCTRL),1});if(!pad)out.bindings[unsigned(Action::Attack)].push_back({InputBinding::Kind::Key,SDL_SCANCODE_RETURN,1});}continue;
        }throw std::runtime_error(std::string("missing controls binding: ")+device+"/"+name);}
    if(out.menu_threshold<out.deadzone)throw std::runtime_error("menu_threshold must be at least deadzone");return out;
}
}
InputConfig InputConfig::defaults(){std::istringstream stream(defaults_text);return parse(stream);}
InputConfig InputConfig::read(const std::filesystem::path& path){std::ifstream stream(path);if(!stream)throw std::runtime_error("cannot read controls: "+path.string());if(std::filesystem::file_size(path)>65536)throw std::runtime_error("controls file too large");return parse(stream);}
void InputConfig::write_defaults(const std::filesystem::path& path){if(std::filesystem::exists(path))throw std::runtime_error("controls output already exists");std::ofstream file(path);if(!file)throw std::runtime_error("cannot write controls: "+path.string());file<<defaults_text;file.close();if(!file)throw std::runtime_error("controls write failed");}

std::string InputConfig::describe(Action action,bool gamepad)const{
    std::string result;
    for(const auto& binding:bindings[unsigned(action)]){
        if((binding.kind!=InputBinding::Kind::Key)!=gamepad)continue;
        std::string label;
        if(binding.kind==InputBinding::Kind::Key){label=SDL_GetScancodeName(SDL_Scancode(binding.code));if(label=="Return")label="Enter";if(label=="Space")label="Leertaste";}
        else if(binding.kind==InputBinding::Kind::Button){
            static const char* buttons[]={"A","B","X","Y","Back","Guide","Start/Options","Stick L","Stick R","LB","RB","D-pad hoch","D-pad runter","D-pad links","D-pad rechts"};
            label=binding.code<int(sizeof(buttons)/sizeof(*buttons))?buttons[binding.code]:SDL_GameControllerGetStringForButton(SDL_GameControllerButton(binding.code));
        }else{static const char* axes[]={"Stick X","Stick Y","Stick R X","Stick R Y","LT","RT"};label=std::string(axes[binding.code])+(binding.direction>0?" +":" -");}
        if(!result.empty())result+=" / ";result+=label;
    }return result.empty()?"Nicht belegt":result;
}
void InputConfig::save(const std::filesystem::path& path)const{
    std::ostringstream text;text<<"# RECVX keyboard and controller settings. Editable in the main menu.\n";
    for(const bool gamepad:{false,true}){text<<(gamepad?"\n[gamepad]\n":"\n[keyboard]\n");
        for(unsigned a=0;a<action_count;++a){text<<names[a]<<" = ";bool any=false;for(const auto& binding:bindings[a]){
            if((binding.kind!=InputBinding::Kind::Key)!=gamepad)continue;
            if(any)text<<", ";any=true;
            if(binding.kind==InputBinding::Kind::Key)text<<SDL_GetScancodeName(SDL_Scancode(binding.code));
            else if(binding.kind==InputBinding::Kind::Button)text<<"button:"<<SDL_GameControllerGetStringForButton(SDL_GameControllerButton(binding.code));
            else text<<"axis:"<<SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis(binding.code))<<(binding.direction>0?"+":"-");
        }if(!any)text<<"none";text<<'\n';}
    }text<<"\n[settings]\ndeadzone = "<<deadzone<<"\nmenu_threshold = "<<menu_threshold<<"\nrepeat_delay_ticks = "<<repeat_delay<<"\nrepeat_interval_ticks = "<<repeat_interval<<'\n';
    // Validate before opening an existing user configuration for writing.
    std::istringstream validation(text.str());parse(validation);
    auto temporary=path;temporary+=".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try{std::ofstream output(temporary);if(!output)throw std::runtime_error("cannot save controls");output<<text.str();output.close();if(!output)throw std::runtime_error("controls save failed");
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("controls replace failed");
#else
        std::filesystem::rename(temporary,path);
#endif
    }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}

}
void InputManager::configure(InputConfig config){config_=std::move(config);pending_.fill(false);age_.fill(0);evaluate(false);}
void InputManager::suppress_held(){evaluate(false);suppressed_=held_;pending_.fill(false);age_.fill(0);}
InputManager::InputManager(InputConfig config):config_(std::move(config)){for(int i=0;i<SDL_NumJoysticks();++i)open(i);evaluate(false);}
InputManager::~InputManager(){for(auto& item:pads_)SDL_GameControllerClose(item.second.handle);}
void InputManager::refresh(Pad& p){for(unsigned a=0;a<p.axes.size();++a)p.axes[a]=SDL_GameControllerGetAxis(p.handle,SDL_GameControllerAxis(a));for(unsigned b=0;b<p.buttons.size();++b)p.buttons[b]=SDL_GameControllerGetButton(p.handle,SDL_GameControllerButton(b));}
void InputManager::open(int index){if(!SDL_IsGameController(index))return;const auto id=SDL_JoystickGetDeviceInstanceID(index);if(pads_.count(id))return;
    auto* controller=SDL_GameControllerOpen(index);if(!controller){std::cerr<<"Controller unavailable: "<<SDL_GetError()<<'\n';return;}Pad pad;pad.handle=controller;refresh(pad);pads_.emplace(id,pad);std::cout<<"Controller connected: "<<SDL_GameControllerName(controller)<<'\n';evaluate(false);}
void InputManager::evaluate(bool edges){for(unsigned a=0;a<action_count;++a){bool down=false;if(focused_)for(const auto& b:config_.bindings[a]){
        if(b.kind==InputBinding::Kind::Key)down=down||keys_.at(b.code);else for(const auto& item:pads_){const auto& p=item.second;if(b.kind==InputBinding::Kind::Button)down=down||p.buttons.at(b.code);else{const auto raw=p.axes.at(b.code);const float axis=raw<0?raw/32768.f:raw/32767.f;down=down||axis*b.direction>=(navigation(a)?config_.menu_threshold:config_.deadzone);}}}
        if(edges&&down&&!held_[a])pending_[a]=true;held_[a]=down;}}
void InputManager::event(const SDL_Event& e){if(e.type==SDL_CONTROLLERDEVICEADDED){open(e.cdevice.which);return;}
    if(e.type==SDL_CONTROLLERDEVICEREMOVED){auto p=pads_.find(e.cdevice.which);if(p!=pads_.end()){SDL_GameControllerClose(p->second.handle);pads_.erase(p);std::cout<<"Controller disconnected\n";}evaluate();return;}
    if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){focused_=false;keys_.fill(false);pending_.fill(false);held_.fill(false);age_.fill(0);return;}
    if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED){focused_=true;for(auto& p:pads_)refresh(p.second);evaluate(false);return;}
    if(!focused_)return;
    if((e.type==SDL_KEYDOWN||e.type==SDL_KEYUP)&&!(e.type==SDL_KEYDOWN&&e.key.repeat)&&e.key.keysym.scancode>0&&e.key.keysym.scancode<SDL_NUM_SCANCODES){last_gamepad_=false;keys_[e.key.keysym.scancode]=e.type==SDL_KEYDOWN;evaluate();}
    if(e.type==SDL_CONTROLLERBUTTONDOWN||e.type==SDL_CONTROLLERBUTTONUP){const auto p=pads_.find(e.cbutton.which);if(p!=pads_.end()&&e.cbutton.button<SDL_CONTROLLER_BUTTON_MAX){last_gamepad_=true;p->second.buttons[e.cbutton.button]=e.type==SDL_CONTROLLERBUTTONDOWN;evaluate();}}
    if(e.type==SDL_CONTROLLERAXISMOTION){const auto p=pads_.find(e.caxis.which);if(p!=pads_.end()&&e.caxis.axis<SDL_CONTROLLER_AXIS_MAX){if((e.caxis.axis<SDL_CONTROLLER_AXIS_TRIGGERLEFT?std::abs(int(e.caxis.value)):int(e.caxis.value))>config_.deadzone*32767)last_gamepad_=true;p->second.axes[e.caxis.axis]=e.caxis.value;evaluate();}}
    if(e.type==SDL_CONTROLLERDEVICEREMAPPED){const auto p=pads_.find(e.cdevice.which);if(p!=pads_.end()){refresh(p->second);evaluate(false);}}
}
void InputManager::rumble(unsigned low,unsigned high,unsigned milliseconds){for(auto& item:pads_)SDL_GameControllerRumble(item.second.handle,Uint16(low>65535u?65535u:low),Uint16(high>65535u?65535u:high),Uint32(milliseconds));}
InputFrame InputManager::tick(){for(auto& item:pads_)refresh(item.second);evaluate();InputFrame frame;frame.gamepad_input=last_gamepad_;frame.held=held_;frame.pressed=pending_;pending_.fill(false);
    for(unsigned a=0;a<action_count;++a){if(!held_[a])suppressed_[a]=false;if(suppressed_[a])frame.held[a]=frame.pressed[a]=false;}
    for(unsigned a=0;a<action_count;++a){if(!held_[a])age_[a]=0;else{if(frame.pressed[a])age_[a]=0;if(age_[a]>=config_.repeat_delay+config_.repeat_interval+1)age_[a]=config_.repeat_delay+1;++age_[a];if(!suppressed_[a]&&navigation(a)&&age_[a]>config_.repeat_delay&&(age_[a]-config_.repeat_delay-1)%config_.repeat_interval==0)frame.repeat[a]=true;}}return frame;}
void GameplayInput::menu_input(MenuInput key){if(modal_input&&modal_input(key))return;if(transition_active&&transition_active()){if(key==MenuInput::Confirm&&confirm_transition)confirm_transition();return;}if(video_active&&video_active()){if((key==MenuInput::Confirm||key==MenuInput::Cancel)&&skip_video)skip_video();return;}if(!events_)return;if(dialog_.active()&&(!menu_.active||menu_.pickup)){if(menu_.active&&menu_.pickup&&menu_.slide.pad_locked())return; /* sub1.c GetItem: every testmode waits for !(statusflg & 0x40) */if(key==MenuInput::Confirm&&!menu_.active&&dialog_.timed()&&!dialog_.choices()&&(events_->flags[10][0]&4)&&events_->skip_cinematic()){if(sound)sound(3);return;} // host: OK skips the running cinematic like Select
        if(key==MenuInput::Confirm||key==MenuInput::Cancel){if(sound)sound(key==MenuInput::Confirm?3:0);if(key==MenuInput::Cancel)events_->cancel_dialog();else events_->confirm_dialog();if(!dialog_.active())inventory_grace_=60;synchronize();}else if(key==MenuInput::Left||key==MenuInput::Right||key==MenuInput::Up||key==MenuInput::Down){if(dialog_.choices()){if(sound)sound(2);events_->choose_dialog(key==MenuInput::Left||key==MenuInput::Up?-1:1);}}return;}
    // The original hint ends before its task releases cinematic control. Keep
    // one inventory request across that short handover, never across a video.
    if(key==MenuInput::Open&&!menu_.active&&(events_->flags[5][0]&4)){if(inventory_grace_&&!dialog_.active())inventory_pending_=true;return;}
    if(menu_.active||key==MenuInput::Open){const bool was_active=menu_.active;menu_.input(key,inventory_);if(!was_active&&menu_.active)events_->begin_status();if(menu_.use_item){unsigned index=0;if(!events_->use(menu_.use_item,&index,menu_.selected)){menu_.active=true;menu_.abort_close();if(index)menu_.show_message(index);menu_.feedback_sound=1;}menu_.use_item=0;}events_->menu_open=menu_.active;if(sound&&menu_.feedback_sound!=0xffffffff)sound(menu_.feedback_sound);return;}
    // Host: OK also requests the original cinematic skip (bhSysCallGame pad bit 0x10000, Select).
    if(key==MenuInput::Confirm&&(events_->flags[10][0]&4)){events_->skip_cinematic();return;}
    if(key==MenuInput::Confirm&&interact){interact();synchronize();}
}
void GameplayInput::synchronize(){
    if(events_)events_->menu_open=menu_.active;
    // swork.statusflg 0x40 für GetItem: die Einfahrt des Aufnahmebildschirms.
    if(events_&&!events_->status_busy)events_->status_busy=[this]{return menu_.active&&menu_.slide.pad_locked();};
    if(events_&&!(transition_active&&transition_active())&&!(video_active&&video_active()))events_->service_item_request();
    if(events_&&events_->acquired_file()!=~0u){
        if(!file_open_){if(sound)sound(3);menu_.acquire_file(inventory_,events_->acquired_file());events_->menu_open=true;file_open_=true;}
        else if(!menu_.active){events_->finish_file();events_->menu_open=false;file_open_=false;}
    }else file_open_=false;
    // bhSysCall: cb_flg 0x40000 requests ItemBox via the same status task as
    // keyboard/Pad inventory. Room scripts need no separate SubStatus opcode.
    if(events_&&!menu_.active&&(events_->flags[10][0]&0x40000))events_->status_requested=true;
    if(events_){const auto found=events_->pickup_display();if(found){if(!menu_.pickup){if(sound)sound(3); /* ItemTaskCheck: CallSystemSe(0,3) when the status task starts */menu_.commands=menu_.examining=menu_.combining=menu_.files=false;menu_.message=MessageDialog{};menu_.inspection=ItemInspection{};const auto angle=inspection_default_angles(found);menu_.examine_pitch=angle[0];menu_.examine_yaw=angle[1];menu_.examine_roll=angle[2];}menu_.pickup=found;menu_.active=true;events_->menu_open=true;}
        else if(menu_.pickup){menu_.pickup=0;menu_.begin_close();events_->menu_open=menu_.active;}}
    if(events_&&events_->status_requested&&!dialog_.active()&&!(transition_active&&transition_active())&&(!(events_->flags[5][0]&4)||(events_->flags[10][0]&0x64010))&& /* bhCheckSubTask: cb_flg 0x64010 requests ignore st_flg 0x4 */!(video_active&&video_active())){events_->status_requested=false;inventory_pending_=false;inventory_grace_=0;if(!menu_.active){if(sound)sound(3);menu_.input(MenuInput::Open,inventory_);events_->begin_status();}if(events_->flags[10][0]&0x40000)events_->flags[12][0]&=~0x200u;events_->menu_open=menu_.active;}}
bool GameplayInput::apply(const InputFrame& f){if(frame_input&&frame_input(f))return f.hit(Action::Quit);menu_.tick(inventory_);if(menu_.slide_sound!=0xffffffff){if(sound)sound(menu_.slide_sound);menu_.slide_sound=0xffffffff;}synchronize();if(inventory_grace_){--inventory_grace_;if(!inventory_grace_)inventory_pending_=false;}
    if(inventory_pending_&&events_&&!(events_->flags[5][0]&4)&&!dialog_.active()&&!(transition_active&&transition_active())&&!(video_active&&video_active())){inventory_pending_=false;menu_input(MenuInput::Open);}
    if(transition_active&&transition_active()){inventory_pending_=false;inventory_grace_=0;if(f.hit(Action::Quit))return true;if(f.hit(Action::Confirm)&&confirm_transition)confirm_transition();return false;}if(video_active&&video_active()){
        inventory_pending_=false;inventory_grace_=0;
        if(f.hit(Action::Confirm)||f.hit(Action::Skip)||f.hit(Action::Cancel)){if(skip_video)skip_video();return false;}
        return f.hit(Action::Quit);
    }const bool cancel_context=menu_.active||dialog_.active();if(f.hit(Action::Cancel)&&cancel_context)menu_input(MenuInput::Cancel);else if(f.hit(Action::Quit))return true;
    if(f.hit(Action::Inventory))menu_input(MenuInput::Open);
    if(f.hit(Action::Confirm)&&(menu_.active||dialog_.active()||!f.down(Action::Aim)||(events_&&(events_->flags[5][0]&4))))menu_input(MenuInput::Confirm);
    if(transition_active&&transition_active())return false;
    if(menu_.active&&menu_.examining&&!menu_.busy()){const int direction=int(f.down(Action::Right))-int(f.down(Action::Left));menu_.examine_yaw+=direction*512;menu_.examine_pitch+=(int(f.down(Action::Up))-int(f.down(Action::Down)))*512;return false;}
    for(auto pair:{std::pair<Action,MenuInput>{Action::Up,MenuInput::Up},{Action::Down,MenuInput::Down},{Action::Left,MenuInput::Left},{Action::Right,MenuInput::Right}})if(f.navigation(pair.first))menu_input(pair.second);
    if(f.hit(Action::Skip)&&events_)events_->skip_cinematic();return false;
}
}
