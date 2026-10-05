#include "input.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace recvx {
namespace {
constexpr const char* names[]={"forward","back","turn_left","turn_right","run","inventory","confirm","cancel","up","down","left","right","skip","quit","wireframe"};
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
quit = Escape, F10
wireframe = F2

[gamepad]
forward = axis:lefty-, button:dpup
back = axis:lefty+, button:dpdown
turn_left = axis:leftx-, button:dpleft
turn_right = axis:leftx+, button:dpright
run = button:b, button:rightshoulder, axis:righttrigger+
inventory = button:y
confirm = button:a
cancel = button:b
up = axis:lefty-, button:dpup
down = axis:lefty+, button:dpdown
left = axis:leftx-, button:dpleft
right = axis:leftx+, button:dpright
skip = button:start
quit = none
wireframe = none

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
    for(const auto* device:{"keyboard","gamepad"})for(auto name:names)if(!seen.count(std::string(device)+"/"+name))throw std::runtime_error(std::string("missing controls binding: ")+device+"/"+name);
    if(out.menu_threshold<out.deadzone)throw std::runtime_error("menu_threshold must be at least deadzone");return out;
}
}
InputConfig InputConfig::defaults(){std::istringstream stream(defaults_text);return parse(stream);}
InputConfig InputConfig::read(const std::filesystem::path& path){std::ifstream stream(path);if(!stream)throw std::runtime_error("cannot read controls: "+path.string());if(std::filesystem::file_size(path)>65536)throw std::runtime_error("controls file too large");return parse(stream);}
void InputConfig::write_defaults(const std::filesystem::path& path){if(std::filesystem::exists(path))throw std::runtime_error("controls output already exists");std::ofstream file(path);if(!file)throw std::runtime_error("cannot write controls: "+path.string());file<<defaults_text;file.close();if(!file)throw std::runtime_error("controls write failed");}
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
    if((e.type==SDL_KEYDOWN||e.type==SDL_KEYUP)&&!(e.type==SDL_KEYDOWN&&e.key.repeat)&&e.key.keysym.scancode>0&&e.key.keysym.scancode<SDL_NUM_SCANCODES){keys_[e.key.keysym.scancode]=e.type==SDL_KEYDOWN;evaluate();}
    if(e.type==SDL_CONTROLLERBUTTONDOWN||e.type==SDL_CONTROLLERBUTTONUP){const auto p=pads_.find(e.cbutton.which);if(p!=pads_.end()&&e.cbutton.button<SDL_CONTROLLER_BUTTON_MAX){p->second.buttons[e.cbutton.button]=e.type==SDL_CONTROLLERBUTTONDOWN;evaluate();}}
    if(e.type==SDL_CONTROLLERAXISMOTION){const auto p=pads_.find(e.caxis.which);if(p!=pads_.end()&&e.caxis.axis<SDL_CONTROLLER_AXIS_MAX){p->second.axes[e.caxis.axis]=e.caxis.value;evaluate();}}
    if(e.type==SDL_CONTROLLERDEVICEREMAPPED){const auto p=pads_.find(e.cdevice.which);if(p!=pads_.end()){refresh(p->second);evaluate(false);}}
}
InputFrame InputManager::tick(){for(auto& item:pads_)refresh(item.second);evaluate();InputFrame frame;frame.held=held_;frame.pressed=pending_;pending_.fill(false);
    for(unsigned a=0;a<action_count;++a){if(!held_[a])age_[a]=0;else{if(frame.pressed[a])age_[a]=0;if(age_[a]>=config_.repeat_delay+config_.repeat_interval+1)age_[a]=config_.repeat_delay+1;++age_[a];if(navigation(a)&&age_[a]>config_.repeat_delay&&(age_[a]-config_.repeat_delay-1)%config_.repeat_interval==0)frame.repeat[a]=true;}}return frame;}
void GameplayInput::menu_input(MenuInput key){if(transition_active&&transition_active()){if(key==MenuInput::Confirm&&confirm_transition)confirm_transition();return;}if(!events_)return;if(dialog_.active()&&!menu_.active&&(key==MenuInput::Confirm||key==MenuInput::Cancel)){events_->confirm_dialog();return;}
    if(key==MenuInput::Open&&!menu_.active&&(events_->flags[5][0]&4))return;
    if(menu_.active||key==MenuInput::Open){menu_.input(key,inventory_);events_->menu_open=menu_.active;if(menu_.use_item){events_->use(menu_.use_item);menu_.use_item=0;}return;}
    if(key==MenuInput::Confirm&&interact)interact();
}
bool GameplayInput::apply(const InputFrame& f){if(transition_active&&transition_active()){if(f.hit(Action::Quit))return true;if(f.hit(Action::Confirm)&&confirm_transition)confirm_transition();return false;}const bool cancel_context=menu_.active||dialog_.active();if(f.hit(Action::Cancel)&&cancel_context)menu_input(MenuInput::Cancel);else if(f.hit(Action::Quit))return true;
    if(f.hit(Action::Inventory))menu_input(MenuInput::Open);
    if(f.hit(Action::Confirm))menu_input(MenuInput::Confirm);
    if(transition_active&&transition_active())return false;
    for(auto pair:{std::pair<Action,MenuInput>{Action::Up,MenuInput::Up},{Action::Down,MenuInput::Down},{Action::Left,MenuInput::Left},{Action::Right,MenuInput::Right}})if(f.navigation(pair.first))menu_input(pair.second);
    if(f.hit(Action::Skip)&&events_)events_->skip_cinematic();return false;
}
}
