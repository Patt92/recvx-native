#include "frontend.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace recvx {
namespace {
const char* const labels[]={"Vorwaerts","Rueckwaerts","Links drehen","Rechts drehen","Laufen","Inventar","OK / Aktion","Zurueck","Menue hoch","Menue runter","Menue links","Menue rechts","Video / Szene skip","Spiel beenden","Drahtgitter","Zielen","Angreifen / Schiessen"};
void panel(SoftwareRenderer& image,int x,int y,int w,int h){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i){auto& pixel=image.canvas()[j*image.width()+i];pixel=0xff000000|((pixel&0xfcfcfc)>>2);}}
std::string percent(float value){return std::to_string(int(std::round(value*100)))+" %";}
}
void Frontend::commit(const InputConfig& candidate){try{candidate.save(path_);config_=candidate;if(changed)changed(config_);status_="Gespeichert";}catch(const std::exception&){status_="Speichern fehlgeschlagen - Belegung unveraendert";}}
void Frontend::arm(){capture_=true;status_.clear();neutral_.fill(true);
    for(int i=0;i<SDL_NumJoysticks();++i)if(auto* pad=SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i)))
        for(unsigned axis=0;axis<neutral_.size();++axis){const auto value=SDL_GameControllerGetAxis(pad,SDL_GameControllerAxis(axis));if(axis<SDL_CONTROLLER_AXIS_TRIGGERLEFT&&std::abs(int(value))>16000)neutral_[axis]=false;}
}
FrontendCommand Frontend::input(MenuInput key,bool pad){
    if(!active()||capture_)return FrontendCommand::None;
    if(sound)sound(key==MenuInput::Cancel?3:key==MenuInput::Confirm?0:2);
    if(screen_==Screen::Main){
        if(key==MenuInput::Up)selected_=(selected_+2)%3;
        if(key==MenuInput::Down)selected_=(selected_+1)%3;
        if(key==MenuInput::Confirm){if(selected_==0){hide();return FrontendCommand::NewGame;}if(selected_==2)return FrontendCommand::Quit;screen_=Screen::Controls;selected_=0;gamepad_=pad;status_.clear();}
        if(key==MenuInput::Cancel)return FrontendCommand::Quit;
    }else{
        if(key==MenuInput::Cancel){screen_=Screen::Main;selected_=1;status_.clear();return FrontendCommand::None;}
        if(key==MenuInput::Up)selected_=(selected_+action_count+3)%(action_count+4);
        if(key==MenuInput::Down)selected_=(selected_+1)%(action_count+4);
        if(key==MenuInput::Left||key==MenuInput::Right){
            if(selected_==action_count||selected_==action_count+1){auto candidate=config_;const float increment=key==MenuInput::Left?-.05f:.05f;
                if(selected_==action_count){candidate.deadzone=std::clamp(candidate.deadzone+increment,.05f,.95f);candidate.menu_threshold=std::max(candidate.menu_threshold,candidate.deadzone);}
                else candidate.menu_threshold=std::clamp(candidate.menu_threshold+increment,candidate.deadzone,.95f);commit(candidate);
            }else gamepad_=!gamepad_;
        }
        if(key==MenuInput::Confirm){if(selected_<action_count)arm();else if(selected_==action_count+2)commit(InputConfig::defaults());else if(selected_==action_count+3){screen_=Screen::Main;selected_=1;}}
    }return FrontendCommand::None;
}
FrontendCommand Frontend::apply(const InputFrame& frame){
    if(suppress_){suppress_=false;return FrontendCommand::None;}if(capture_)return FrontendCommand::None;
    if(frame.hit(Action::Cancel))return input(MenuInput::Cancel,frame.gamepad_input);
    if(frame.hit(Action::Quit))return FrontendCommand::Quit;
    for(auto pair:{std::pair<Action,MenuInput>{Action::Up,MenuInput::Up},{Action::Down,MenuInput::Down},{Action::Left,MenuInput::Left},{Action::Right,MenuInput::Right}})if(frame.navigation(pair.first))input(pair.second,frame.gamepad_input);
    if(frame.hit(Action::Confirm))return input(MenuInput::Confirm,frame.gamepad_input);
    return FrontendCommand::None;
}
void Frontend::event(const SDL_Event& event){if(!capture_)return;
    if(event.type==SDL_KEYDOWN&&!event.key.repeat&&event.key.keysym.scancode==SDL_SCANCODE_ESCAPE){capture_=false;suppress_=true;status_="Abgebrochen";return;}
    InputBinding binding{InputBinding::Kind::Key,0,1};bool accepted=false;
    if(!gamepad_&&event.type==SDL_KEYDOWN&&!event.key.repeat&&event.key.keysym.scancode>0){binding.code=event.key.keysym.scancode;accepted=true;}
    if(gamepad_&&event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.button<SDL_CONTROLLER_BUTTON_MAX){binding={InputBinding::Kind::Button,event.cbutton.button,1};accepted=true;}
    if(gamepad_&&event.type==SDL_CONTROLLERAXISMOTION&&event.caxis.axis<SDL_CONTROLLER_AXIS_MAX){const unsigned axis=event.caxis.axis;const int value=event.caxis.value;
        if(std::abs(value)<10000||axis>=SDL_CONTROLLER_AXIS_TRIGGERLEFT&&value<10000)neutral_[axis]=true;
        if(neutral_[axis]&&(axis<SDL_CONTROLLER_AXIS_TRIGGERLEFT?std::abs(value)>24000:value>24000)){binding={InputBinding::Kind::Axis,int(axis),value>0?1:-1};accepted=true;}
    }
    if(!accepted)return;auto candidate=config_;auto& bindings=candidate.bindings[selected_];
    if(selected_==unsigned(Action::Confirm)||selected_==unsigned(Action::Cancel)){
        auto& opposite=candidate.bindings[unsigned(selected_==unsigned(Action::Confirm)?Action::Cancel:Action::Confirm)];
        const auto before=opposite.size();opposite.erase(std::remove_if(opposite.begin(),opposite.end(),[&](const InputBinding& b){return b.kind==binding.kind&&b.code==binding.code&&b.direction==binding.direction;}),opposite.end());
        if(opposite.size()!=before&&!std::any_of(opposite.begin(),opposite.end(),[&](const InputBinding& b){return (b.kind!=InputBinding::Kind::Key)==gamepad_;}))
            for(const auto& b:bindings)if((b.kind!=InputBinding::Kind::Key)==gamepad_)opposite.push_back(b);
    }
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[&](const InputBinding& value){return (value.kind!=InputBinding::Kind::Key)==gamepad_;}),bindings.end());bindings.push_back(binding);
    capture_=false;suppress_=true;commit(candidate);
}
void Frontend::draw(SoftwareRenderer& image,GameUi& ui,std::size_t controllers){ui.draw_title(image);
    if(screen_==Screen::Main){ui.draw_title_options(image,selected_);
        panel(image,0,438,640,42);ui.label(image,"OK: "+config_.describe(Action::Confirm,false)+"   Controller: "+config_.describe(Action::Confirm,true),24,443,.5f);
        ui.label(image,"Auswahl: Pfeiltasten / Stick / Steuerkreuz",24,460,.45f);
    }else{
        panel(image,10,10,620,462);ui.label(image,"Einstellungen",26,18,.9f);ui.label(image,gamepad_?"Controller":"Tastatur",26,52,.65f);
        ui.label(image,std::to_string(controllers)+" Controller verbunden",342,57,.48f);
        for(unsigned i=0;i<action_count+4;++i){const int y=87+int(i)*16;const auto color=i==selected_?0xffffff70:0xffffffff;
            std::string label,value;if(i<action_count){label=labels[i];value=config_.describe(Action(i),gamepad_);}
            else if(i==action_count){label="Stick-Totzone";value=percent(config_.deadzone)+"  links/rechts";}
            else if(i==action_count+1){label="Menue-Stickschwelle";value=percent(config_.menu_threshold)+"  links/rechts";}
            else if(i==action_count+2)label="Standardbelegung wiederherstellen";else label="Zurueck zum Hauptmenue";
            if(i==selected_)for(int yy=y+5;yy<y+12;++yy)for(int x=20;x<24;++x)image.canvas()[yy*640+x]=0xffffff70;ui.label(image,label,32,y,.48f,color);ui.label(image,value,245,y,.48f,color);
        }
        if(capture_){panel(image,28,188,584,96);ui.label(image,gamepad_?"Controller-Taste druecken oder Stick bewegen":"Neue Tastaturtaste druecken",48,209,.68f);ui.label(image,"Escape: Abbrechen",48,243,.58f);}
        ui.label(image,status_.empty()?(selected_==action_count||selected_==action_count+1?"Links/Rechts: Wert aendern":"OK: Belegung aendern; Links/Rechts: Tastatur/Controller"):status_,22,436,.45f);
        ui.label(image,"Zurueck: "+config_.describe(Action::Cancel,false)+" / "+config_.describe(Action::Cancel,true),22,457,.45f);
    }
}
void draw_skip_hint(SoftwareRenderer& image,const GameUi& ui,const InputConfig& config){
    ui.label(image,"Ueberspringen: "+config.describe(Action::Confirm,false)+" / "+config.describe(Action::Skip,false),20,432,.65f);
    ui.label(image,"Controller: "+config.describe(Action::Confirm,true)+" / "+config.describe(Action::Skip,true),20,456,.55f);
}
}
