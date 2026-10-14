#include "third_person.h"
#include "world.h"
#include "timing.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace recvx {
namespace {
constexpr float pi=3.14159265358979f;
constexpr float angle_to_radians=2*pi/65536,radians_to_angle=65536/(2*pi);
// Rates are tuned per 60-Hz step (third_person.ini keeps that unit); tick() runs
// once per original 30-Hz game tick (timing.h), so convert the per-step factor.
constexpr float steps_per_tick=60.f/float(game_tick_hz);
float per_tick(float rate){return 1-std::pow(1-std::min(1.f,rate),steps_per_tick);}
std::string trim(std::string s){const auto first=s.find_first_not_of(" \t\r\n");if(first==s.npos)return {};return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);}
float mix(float a,float b,float t){return a+(b-a)*t;}
NJS_POINT3 mix(NJS_POINT3 a,NJS_POINT3 b,float t){return {mix(a.x,b.x,t),mix(a.y,b.y,t),mix(a.z,b.z,t)};}
float wrap(float a){while(a>pi)a-=2*pi;while(a<-pi)a+=2*pi;return a;}
void validate(const ThirdPersonSettings& s){
    auto range=[](float v,float lo,float hi,const char* name){if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error(std::string("third_person ")+name+" out of range");};
    range(s.distance,4,80,"distance");range(s.height,0,40,"height");range(s.shoulder,-20,20,"shoulder");
    range(s.aim_distance,2,80,"aim_distance");range(s.aim_height,0,40,"aim_height");range(s.aim_shoulder,-20,20,"aim_shoulder");
    range(s.fov,20,110,"fov");range(s.aim_fov,20,110,"aim_fov");range(s.smoothing,.01f,1,"smoothing");
}
}
ThirdPersonCamera::ThirdPersonCamera(std::filesystem::path path):path_(std::move(path)){
    std::error_code ignored;if(path_.empty()||!std::filesystem::exists(path_,ignored))return;
    try{settings_=read(path_);}catch(const std::exception& e){settings_={};status_=e.what();}
}
ThirdPersonSettings ThirdPersonCamera::read(const std::filesystem::path& path){
    std::ifstream stream(path);if(!stream)throw std::runtime_error("cannot read third_person settings: "+path.string());
    ThirdPersonSettings out;std::string line,section;unsigned lineno=0;
    while(std::getline(stream,line)){++lineno;if(lineno>256)throw std::runtime_error("third_person settings too long");line=trim(line.substr(0,line.find_first_of("#;")));if(line.empty())continue;
        if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);continue;}
        if(section!="third_person")continue;
        const auto equals=line.find('=');if(equals==line.npos)throw std::runtime_error("third_person line "+std::to_string(lineno)+": expected key = value");
        const auto key=trim(line.substr(0,equals)),value=trim(line.substr(equals+1));
        if(key=="enabled"){if(value!="0"&&value!="1")throw std::runtime_error("third_person enabled must be 0 or 1");out.enabled=value=="1";continue;}
        if(key=="shoulder_side"){if(value!="left"&&value!="right")throw std::runtime_error("third_person shoulder_side must be left or right");out.left_shoulder=value=="left";continue;}
        float* target=key=="distance"?&out.distance:key=="height"?&out.height:key=="shoulder"?&out.shoulder:key=="aim_distance"?&out.aim_distance:key=="aim_height"?&out.aim_height:key=="aim_shoulder"?&out.aim_shoulder:key=="fov"?&out.fov:key=="aim_fov"?&out.aim_fov:key=="smoothing"?&out.smoothing:nullptr;
        if(!target)throw std::runtime_error("unknown third_person setting: "+key);
        std::size_t used=0;*target=std::stof(value,&used);if(used!=value.size())throw std::runtime_error("third_person "+key+" must be a number");
    }
    validate(out);return out;
}
void ThirdPersonCamera::save(const std::filesystem::path& path,const ThirdPersonSettings& s){
    validate(s);std::ostringstream text;
    text<<"# RECVX host camera mod (not original game behaviour). Editable in the main menu.\n[third_person]\nenabled = "<<(s.enabled?1:0)
        <<"\nshoulder_side = "<<(s.left_shoulder?"left":"right")<<"\ndistance = "<<s.distance<<"\nheight = "<<s.height<<"\nshoulder = "<<s.shoulder
        <<"\naim_distance = "<<s.aim_distance<<"\naim_height = "<<s.aim_height<<"\naim_shoulder = "<<s.aim_shoulder
        <<"\nfov = "<<s.fov<<"\naim_fov = "<<s.aim_fov<<"\nsmoothing = "<<s.smoothing<<'\n';
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    auto temporary=path;temporary+=".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try{std::ofstream output(temporary);if(!output)throw std::runtime_error("cannot save third_person settings");output<<text.str();output.close();if(!output)throw std::runtime_error("third_person save failed");
        std::error_code error;std::filesystem::rename(temporary,path,error);
        if(error){std::filesystem::remove(path,error);std::filesystem::rename(temporary,path);}
    }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
void ThirdPersonCamera::configure(const ThirdPersonSettings& settings,bool persist){
    validate(settings);
    if(persist&&!path_.empty()){try{save(path_,settings);status_="Saved";}catch(const std::exception&){status_="Save failed";}}
    settings_=settings;valid_=false;if(changed)changed(settings_);
}
void ThirdPersonCamera::set_enabled(bool enabled,bool persist){auto candidate=settings_;candidate.enabled=enabled;configure(candidate,persist);}
void ThirdPersonCamera::tick(const SceneSpawn& actor,ThirdPersonState state,const RoomWorld* world){
    state_=state;
    if(!enabled()||!state.free||!std::isfinite(actor.position.x)||!std::isfinite(actor.position.y)||!std::isfinite(actor.position.z)){valid_=false;blend_=state.aiming?1.f:0.f;return;}
    const float heading=float(actor.yaw&65535)*angle_to_radians;
    if(!valid_){yaw_=heading;anchor_=actor.position;}
    else{
        // Lagging heading and anchor give the RE4/RE2-remake style trailing shoulder view.
        const float k=per_tick(settings_.smoothing),aim_k=per_tick(std::min(1.f,settings_.smoothing*2.5f));
        yaw_=wrap(yaw_+wrap(heading-yaw_)*(state.aiming?aim_k:k));
        anchor_=mix(anchor_,actor.position,per_tick(std::min(1.f,settings_.smoothing*2.f)));
    }
    blend_=std::clamp(blend_+(state.aiming?1.f:-1.f)*steps_per_tick/8,0.f,1.f);
    const auto& s=settings_;const float side=s.left_shoulder?-1.f:1.f;
    const float distance=mix(s.distance,s.aim_distance,blend_),height=mix(s.height,s.aim_height,blend_),shoulder=mix(s.shoulder,s.aim_shoulder,blend_)*side;
    const NJS_POINT3 head{anchor_.x,anchor_.y+height,anchor_.z};
    // Wall records from the original ATR_WORK table keep the camera inside the room.
    auto clip=[&](NJS_POINT3 from,NJS_POINT3 to,float margin){
        if(!world)return to;const NJS_POINT3 d{to.x-from.x,to.y-from.y,to.z-from.z};const float length=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(length<.001f)return to;
        const NJS_POINT3 direction{d.x/length,d.y/length,d.z/length};const float hit=world->shot_distance(from,direction,length+margin);
        if(hit>=length+margin)return to;const float t=std::max(0.f,hit-margin)/length;return NJS_POINT3{from.x+d.x*t,from.y+d.y*t,from.z+d.z*t};
    };
    struct Rig {NJS_POINT3 pivot,eye,forward;float clearance;};
    // Player forward follows PlayerAsset::locomotion: x -= sin(yaw), z -= cos(yaw).
    auto rig=[&](float swing,float lift){const float a=yaw_+swing;const NJS_POINT3 forward{-std::sin(a),0,-std::cos(a)},right{std::cos(a),0,-std::sin(a)};
        const auto pivot=clip(head,{head.x+right.x*shoulder,head.y,head.z+right.z*shoulder},1.5f);
        const NJS_POINT3 desired{pivot.x-forward.x*distance,pivot.y+2.f*(1-blend_)+lift,pivot.z-forward.z*distance};const auto eye=clip(pivot,desired,2.f);
        const float dx=eye.x-pivot.x,dy=eye.y-pivot.y,dz=eye.z-pivot.z,full=std::sqrt(distance*distance+(desired.y-pivot.y)*(desired.y-pivot.y));
        return Rig{pivot,eye,forward,std::sqrt(dx*dx+dy*dy+dz*dz)/full};};
    // Like the RE remakes, swing around/raise the boom when a wall or staircase
    // sits directly behind the player instead of placing the eye inside geometry.
    constexpr float degree=pi/180,enough=.55f;
    float wanted_swing=0,wanted_lift=0,best=-1;
    auto consider=[&](float swing,float lift){const float c=rig(swing,lift).clearance;if(c>best+.05f){best=c;wanted_swing=swing;wanted_lift=lift;}return c>=enough;};
    bool found=consider(0,0)||consider(swing_target_,lift_target_);
    for(const float lift:{0.f,9.f})for(const float step:{25.f,50.f,75.f,100.f,130.f,160.f,180.f}){if(found)break;for(const float sign:{1.f,-1.f})if(!found&&consider(sign*step*degree,lift))found=true;}
    swing_target_=wanted_swing;lift_target_=wanted_lift;
    if(!valid_){swing_=wanted_swing;lift_=wanted_lift;}
    else{swing_=wrap(swing_+wrap(wanted_swing-swing_)*per_tick(.12f));lift_=mix(lift_,wanted_lift,per_tick(.12f));}
    const auto chosen=rig(swing_,lift_);
    eye_=chosen.eye;
    target_={chosen.pivot.x+chosen.forward.x*40,head.y-mix(4.5f,1.5f,blend_),chosen.pivot.z+chosen.forward.z*40};
    valid_=true;
}
SceneCamera ThirdPersonCamera::view(const SceneCamera& original)const{
    if(!active())return original;
    SceneCamera camera=original;camera.position=eye_;camera.distance=0;camera.hidden_objects.fill(0);
    const float dx=target_.x-eye_.x,dy=target_.y-eye_.y,dz=target_.z-eye_.z;
    camera.angles[0]=int(-std::atan2(dy,std::sqrt(dx*dx+dz*dz))*radians_to_angle)&65535;
    camera.angles[1]=(32768-int(std::atan2(dx,dz)*radians_to_angle))&65535;
    camera.angles[2]=0;
    camera.perspective=std::clamp(int(mix(settings_.fov,settings_.aim_fov,blend_)*182.04445f),1,32767);
    return camera;
}
}
