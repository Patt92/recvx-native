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
    if(!world){tick(actor,state,RayQuery{});return;}
    tick(actor,state,[world](NJS_POINT3 origin,NJS_POINT3 direction,float range){return world->shot_distance(origin,direction,range);});
}
void ThirdPersonCamera::tick(const SceneSpawn& actor,ThirdPersonState state,const RayQuery& walls){
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
    // Free length of a segment before a wall record of the original ATR_WORK table, minus a margin.
    auto reach=[&](NJS_POINT3 from,NJS_POINT3 to,float margin){
        const NJS_POINT3 d{to.x-from.x,to.y-from.y,to.z-from.z};const float length=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
        if(!walls||length<.001f)return 1.f;
        const float hit=walls(from,{d.x/length,d.y/length,d.z/length},length+margin);
        return hit>=length+margin?1.f:std::max(0.f,hit-margin)/length;};
    const float a=yaw_;const NJS_POINT3 forward{-std::sin(a),0,-std::cos(a)},right{std::cos(a),0,-std::sin(a)};
    // Shoulder pivot stays out of side walls.
    const NJS_POINT3 side_point{head.x+right.x*shoulder,head.y,head.z+right.z*shoulder};
    const auto shoulder_pivot=mix(head,side_point,reach(head,side_point,1.5f));
    // The boom always stays straight behind the player (no swing to the front).
    // A wall behind shortens it and raises it so the view looks over the head.
    constexpr float max_lift=9;
    auto behind=[&](NJS_POINT3 from,float lift){return NJS_POINT3{from.x-forward.x*distance,from.y+2.f*(1-blend_)+lift,from.z-forward.z*distance};};
    const float flat=reach(shoulder_pivot,behind(shoulder_pivot,0),2.f);
    const float wanted_lift=max_lift*(1-flat);
    lift_=valid_?mix(lift_,wanted_lift,per_tick(.12f)):wanted_lift;
    const float room=reach(shoulder_pivot,behind(shoulder_pivot,lift_),2.f);
    // Snap in at once so the eye never enters a wall, ease back out afterwards.
    boom_=valid_?std::min(room,mix(boom_,room,per_tick(.12f))):room;
    // The shoulder offset shrinks with the boom, otherwise a short boom leaves the player outside the view.
    const auto pivot=mix(head,shoulder_pivot,boom_);
    const auto desired=behind(pivot,lift_);
    eye_=mix(pivot,desired,boom_*reach(pivot,mix(pivot,desired,boom_),2.f));
    target_={pivot.x+forward.x*40,head.y-mix(4.5f,1.5f,blend_)-lift_*.5f,pivot.z+forward.z*40};
    // A short boom puts the eye into the player model: fade it instead of turning the camera around.
    constexpr float fade_far=9,fade_near=4,min_alpha=.2f;
    const float dx=eye_.x-anchor_.x,dz=eye_.z-anchor_.z,close=std::sqrt(dx*dx+dz*dz);
    const float wanted_fade=std::clamp(min_alpha+(1-min_alpha)*(close-fade_near)/(fade_far-fade_near),min_alpha,1.f);
    fade_=valid_?mix(fade_,wanted_fade,per_tick(.25f)):wanted_fade;
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
