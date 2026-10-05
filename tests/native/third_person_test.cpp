#define SDL_MAIN_HANDLED
#include "frontend.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static float horizontal(NJS_POINT3 a,NJS_POINT3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z));}
int main(){try{
    _Make_SinTable();
    const auto stamp=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path=std::filesystem::temp_directory_path()/("recvx-third-person-"+stamp+".ini");
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}}cleanup{path};
    ThirdPersonCamera camera(path);check(!camera.enabled(),"camera mod must default to off");
    SceneCamera original;original.position={1,2,3};original.angles={10,20,30};original.perspective=9000;original.hidden_objects[0]=0x80000000u;
    SceneSpawn actor;actor.position={100,0,-50};actor.yaw=0;
    camera.tick(actor,{true,false},nullptr);check(!camera.active()&&camera.view(original).position.x==1,"disabled mod changed the original camera");
    unsigned notified=0;camera.changed=[&](const ThirdPersonSettings&){++notified;};
    camera.set_enabled(true);check(camera.enabled()&&notified==1,"enable failed");check(ThirdPersonCamera::read(path).enabled,"enable not persisted");
    // Free control: eye trails behind the player's forward axis (-sin,-cos) and looks along it.
    camera.tick(actor,{true,false},nullptr);check(camera.active(),"free control did not activate the chase camera");
    auto view=camera.view(original);const auto& s=camera.settings();
    check(view.position.z>actor.position.z+s.distance*.9f,"eye is not behind the player");
    check(std::abs(horizontal(view.position,actor.position)-std::sqrt(s.distance*s.distance+s.shoulder*s.shoulder))<1,"explore distance wrong");
    check(view.hidden_objects[0]==0&&view.distance==0,"fixed-camera object masks leaked into the chase view");
    check(std::min(view.angles[1],65536-view.angles[1])<200,"chase camera heading does not match player yaw");
    check(std::abs(view.perspective-int(s.fov*182.04445f))<2,"explore field of view wrong");
    // Aiming pulls the boom in over 8 ticks.
    for(int i=0;i<8;++i)camera.tick(actor,{true,true},nullptr);view=camera.view(original);
    check(horizontal(view.position,actor.position)<s.aim_distance+s.aim_shoulder+1,"aim pose not reached");
    check(std::abs(view.perspective-int(s.aim_fov*182.04445f))<2,"aim field of view wrong");
    // Turning lags (smoothing) and converges on the new heading.
    actor.yaw=16384;camera.tick(actor,{true,false},nullptr);const auto lag=camera.view(original).angles[1];
    for(int i=0;i<240;++i)camera.tick(actor,{true,false},nullptr);const auto settled=camera.view(original).angles[1];
    check(lag!=settled,"heading change was not smoothed");check(std::abs(int(settled)-(65536-16384))<400,"heading did not converge"); // Ninja view yaw is the negated actor yaw.
    // Scripted states (event camera, cinematic, door, FMV) hand the view back immediately.
    camera.tick(actor,{false,false},nullptr);view=camera.view(original);check(!camera.active()&&view.position.x==1&&view.angles[0]==10&&view.hidden_objects[0],"scripted state kept the chase camera");
    // Settings file validation and left shoulder.
    auto settings=camera.settings();settings.left_shoulder=true;settings.distance=30;camera.configure(settings);
    const auto loaded=ThirdPersonCamera::read(path);check(loaded.left_shoulder&&loaded.distance==30&&loaded.enabled,"settings roundtrip failed");
    settings.fov=500;bool rejected=false;try{camera.configure(settings);}catch(const std::exception&){rejected=true;}check(rejected&&camera.settings().fov!=500,"invalid fov accepted");
    camera.toggle(false);check(!camera.enabled()&&ThirdPersonCamera::read(path).enabled,"non-persistent toggle wrote the file");
    // Main menu settings row toggles and persists the mod.
    camera.set_enabled(false);
    const auto controls=std::filesystem::temp_directory_path()/("recvx-third-person-controls-"+stamp+".ini");Cleanup controls_cleanup{controls};
    auto config=InputConfig::defaults();config.save(controls);Frontend front(config,controls);front.third_person=&camera;
    front.input(MenuInput::Down);front.input(MenuInput::Confirm);check(front.settings(),"settings screen not reached");
    for(unsigned i=0;i<Frontend::third_person_row;++i)front.input(MenuInput::Down);check(front.selected()==Frontend::third_person_row,"camera row not selectable");
    front.input(MenuInput::Right);check(camera.enabled()&&ThirdPersonCamera::read(path).enabled,"menu toggle on failed");
    front.input(MenuInput::Confirm);check(!camera.enabled()&&!ThirdPersonCamera::read(path).enabled,"menu toggle off failed");
    front.input(MenuInput::Down);front.input(MenuInput::Confirm);check(front.active()&&!front.settings(),"back row moved");
    std::cout<<"Third-person camera mod: defaults, persistence, chase/aim pose, smoothing, scripted handover and menu row verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Third-person test failed: "<<e.what()<<'\n';return 1;}}
