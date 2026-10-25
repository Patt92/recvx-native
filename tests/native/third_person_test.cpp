#define SDL_MAIN_HANDLED
#include "frontend.h"
#include <chrono>
#include <cmath>
#include <fstream>
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
    // Room entry with the door wall right behind the player: the boom shortens and rises
    // but never swings in front of her, and the player model fades instead.
    {ThirdPersonCamera tight;tight.configure(camera.settings(),false);tight.set_enabled(true,false);
        auto wall_z=[](float z){return ThirdPersonCamera::RayQuery([z](NJS_POINT3 o,NJS_POINT3 d,float range){if(d.z<=1e-6f||o.z>=z)return range;return std::min(range,(z-o.z)/d.z);});};
        SceneSpawn entry;entry.position={0,0,0};entry.yaw=0; // forward is -z, behind is +z
        for(const float gap:{2.f,6.f,12.f}){
            tight.reset();for(int i=0;i<60;++i)tight.tick(entry,{true,false},wall_z(gap));
            const auto v=tight.view(original);
            check(tight.active(),"tight entry dropped the chase camera");
            check(v.position.z>=entry.position.z-.01f,"camera swung in front of the player at a wall");
            check(v.position.z<=gap,"camera eye inside the wall behind the player");
            check(std::min(v.angles[1],65536-v.angles[1])<200,"tight entry view does not look along the player heading");
            if(gap<=6.f)check(tight.player_alpha()<1.f,"player not faded with the eye inside the model");}
        tight.reset();for(int i=0;i<60;++i)tight.tick(entry,{true,false},ThirdPersonCamera::RayQuery{});
        check(tight.player_alpha()==1.f,"player faded without a wall");
        // Leaving the wall eases the boom back out instead of snapping.
        for(int i=0;i<60;++i)tight.tick(entry,{true,false},wall_z(2));const float near_z=tight.view(original).position.z;
        tight.tick(entry,{true,false},ThirdPersonCamera::RayQuery{});const float eased=tight.view(original).position.z;
        for(int i=0;i<120;++i)tight.tick(entry,{true,false},ThirdPersonCamera::RayQuery{});const float settled_z=tight.view(original).position.z;
        check(eased>near_z&&eased<settled_z-1,"boom did not ease back out");}
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
    // adv.c title task: PRESS START (Mode 6), Start, NEW GAME -> OPTION (Cursor 2), OK, GetSamurai(50) fade, then the host settings screen.
    auto settle=[&](auto ready){for(int i=0;i<600&&!ready();++i)front.apply({});};
    settle([&]{return front.mode()==6;});front.input(MenuInput::Confirm);settle([&]{return front.menu_ready();});
    front.input(MenuInput::Down);front.apply({});front.input(MenuInput::Confirm);settle([&]{return front.settings();});check(front.settings(),"settings screen not reached");
    for(unsigned i=0;i<Frontend::third_person_row;++i)front.input(MenuInput::Down);check(front.selected()==Frontend::third_person_row,"camera row not selectable");
    front.input(MenuInput::Right);check(camera.enabled()&&ThirdPersonCamera::read(path).enabled,"menu toggle on failed");
    front.input(MenuInput::Confirm);check(!camera.enabled()&&!ThirdPersonCamera::read(path).enabled,"menu toggle off failed");
    // Host render quality row follows the camera row and persists video.ini.
    const auto video_path=path.parent_path()/"recvx-video-test.ini";std::filesystem::remove(video_path);VideoConfig video(video_path);front.video=&video;
    int applied=-1;front.video_changed=[&](const VideoSettings& s){applied=s.supersampling();};
    check(video.settings().quality==RenderQuality::High&&video.settings().supersampling()==2&&video.settings().filtering(),"video defaults");
    front.input(MenuInput::Down);check(front.selected()==Frontend::video_row,"video row not selectable");
    front.input(MenuInput::Right);check(video.settings().quality==RenderQuality::Ultra&&applied==3&&VideoSettings::read(video_path).quality==RenderQuality::Ultra,"quality up");
    front.input(MenuInput::Right);check(video.settings().quality==RenderQuality::Original&&!video.settings().filtering()&&applied==1,"quality wrap");
    front.input(MenuInput::Right);front.input(MenuInput::Left);front.input(MenuInput::Left);check(VideoSettings::read(video_path).quality==RenderQuality::Ultra&&applied==3,"quality down");
    // renderer bleibt beim Speichern der Qualitaet erhalten; Standard ist der GPU-Rasterer.
    check(VideoSettings::read(video_path).renderer==RenderBackend::Gpu,"renderer default lost on save");
    // Debugmenue/Debugaktion "Rasterer": gleiche Persistenz wie die Qualitaet, Qualitaet unberuehrt.
    const auto quality_before=video.settings().quality;
    video.set_renderer(RenderBackend::Software);check(VideoSettings::read(video_path).renderer==RenderBackend::Software&&video.settings().renderer==RenderBackend::Software,"renderer switch not persisted");
    video.set_renderer(RenderBackend::Gpu);check(VideoSettings::read(video_path).renderer==RenderBackend::Gpu&&video.settings().quality==quality_before,"renderer switch changed quality");
    video.set_renderer(RenderBackend::OpenGL);check(VideoSettings::read(video_path).renderer==RenderBackend::OpenGL&&video.settings().quality==quality_before,"OpenGL renderer roundtrip lost quality");
    video.set_renderer(RenderBackend::Vulkan);check(VideoSettings::read(video_path).renderer==RenderBackend::Vulkan&&video.settings().quality==quality_before,"Vulkan renderer roundtrip lost quality");
    // Host presentation rows (MetalFX upscaler, frame generation, post-processing) follow the quality row,
    // cycle with wrap-around, persist to video.ini and report availability set by the runtime.
    check(video.settings().upscaler==Upscaler::Off&&video.settings().frame_generation==FrameGeneration::Off&&video.settings().post_process==PostProcess::Off&&!video.settings().presentation(),"presentation defaults");
    {unsigned applied_presentation=0;front.video_changed=[&](const VideoSettings& s){applied=s.supersampling();applied_presentation+=s.presentation()?1u:0u;};
     front.input(MenuInput::Down);check(front.selected()==Frontend::frame_rate_row,"frame rate row not selectable");
     check(video.settings().frame_rate==FrameRate::Hz30&&video.settings().presents_per_tick()==1&&video.settings().vsync(),"frame rate default");
     front.input(MenuInput::Right);check(video.settings().frame_rate==FrameRate::Hz60&&video.settings().presents_per_tick()==2&&VideoSettings::read(video_path).frame_rate==FrameRate::Hz60,"frame rate 60");
     front.input(MenuInput::Right);check(video.settings().frame_rate==FrameRate::Hz120&&video.settings().presents_per_tick()==4,"frame rate 120");
     front.input(MenuInput::Right);check(video.settings().frame_rate==FrameRate::Unlimited&&video.settings().presents_per_tick()==0&&!video.settings().vsync()&&video.settings().describe_frame_rate()=="Unlimited (no vsync)","frame rate unlimited");
     front.input(MenuInput::Right);check(video.settings().frame_rate==FrameRate::Hz30,"frame rate wrap");
     front.input(MenuInput::Down);check(front.selected()==Frontend::upscaler_row,"upscaler row not selectable");
     front.input(MenuInput::Right);check(video.settings().upscaler==Upscaler::MetalFxSpatial&&VideoSettings::read(video_path).upscaler==Upscaler::MetalFxSpatial&&applied_presentation==1,"upscaler spatial");
     front.input(MenuInput::Right);check(video.settings().upscaler==Upscaler::MetalFxTemporal&&VideoSettings::read(video_path).upscaler==Upscaler::MetalFxTemporal,"upscaler temporal");
     front.input(MenuInput::Right);check(video.settings().upscaler==Upscaler::Fsr4&&VideoSettings::read(video_path).upscaler==Upscaler::Fsr4,"upscaler fsr4");
     front.input(MenuInput::Right);check(video.settings().upscaler==Upscaler::Fsr4Int8&&VideoSettings::read(video_path).upscaler==Upscaler::Fsr4Int8,"upscaler fsr4 int8");
     front.input(MenuInput::Right);check(video.settings().upscaler==Upscaler::Off,"upscaler wrap");
     front.input(MenuInput::Left);check(video.settings().upscaler==Upscaler::Fsr4Int8,"upscaler left wrap");
     front.input(MenuInput::Left);front.input(MenuInput::Left);check(video.settings().upscaler==Upscaler::MetalFxTemporal,"upscaler left twice");
     check(video.describe_upscaler()=="MetalFX temporal (not available)","upscaler availability note missing");
     video.set_availability(true,true,false);check(video.describe_upscaler()=="MetalFX temporal","upscaler availability note stuck");
     front.input(MenuInput::Down);check(front.selected()==Frontend::frame_generation_row,"frame generation row not selectable");
     front.input(MenuInput::Right);check(video.settings().frame_generation==FrameGeneration::MetalFx&&VideoSettings::read(video_path).frame_generation==FrameGeneration::MetalFx,"frame generation on");
     check(video.describe_frame_generation()=="MetalFX frame interpolation (not available)","frame generation availability note missing");
     video.set_frame_generation(FrameGeneration::MetalFx);check(video.settings().presents_per_tick()==2,"frame interpolation needs two presents per tick at 30");video.set_frame_rate(FrameRate::Hz120);check(video.settings().presents_per_tick()==4,"frame interpolation keeps 120");video.set_frame_rate(FrameRate::Hz30);
     front.input(MenuInput::Right);check(video.settings().frame_generation==FrameGeneration::Off,"frame generation wrap");
     front.input(MenuInput::Down);check(front.selected()==Frontend::sharpness_row,"sharpness row not selectable");
     check(video.settings().sharpness==Sharpness::Off&&video.settings().sharpness_amount()==0.0f,"sharpness default");
     front.input(MenuInput::Right);front.input(MenuInput::Right);check(video.settings().sharpness==Sharpness::Medium&&video.settings().sharpness_amount()==0.6f&&VideoSettings::read(video_path).sharpness==Sharpness::Medium&&video.settings().presentation(),"sharpness medium");
     front.input(MenuInput::Right);front.input(MenuInput::Right);check(video.settings().sharpness==Sharpness::Off,"sharpness wrap");
     front.input(MenuInput::Down);check(front.selected()==Frontend::post_process_row,"post-process row not selectable");
     front.input(MenuInput::Right);front.input(MenuInput::Right);check(video.settings().post_process==PostProcess::CrtFull&&VideoSettings::read(video_path).post_process==PostProcess::CrtFull,"post-process crt full");
     check(video.describe_post_process()=="CRT full (mask, bloom)","post-process label");
     front.input(MenuInput::Right);check(video.settings().post_process==PostProcess::Off,"post-process wrap");
     {std::ofstream old(video_path);old<<"quality = high\npost_process = sharpen\n";}check(VideoSettings::read(video_path).sharpness==Sharpness::Medium&&VideoSettings::read(video_path).post_process==PostProcess::Off,"old sharpen value not mapped");
     {std::ofstream old(video_path);old<<"quality = high\npost_process = crt\n";}check(VideoSettings::read(video_path).post_process==PostProcess::CrtLight,"old crt value not mapped");
     video.set_upscaler(Upscaler::MetalFxTemporal);video.set_renderer(RenderBackend::Vulkan);
     const auto reread=VideoSettings::read(video_path);check(reread.upscaler==Upscaler::MetalFxTemporal&&reread.frame_generation==FrameGeneration::Off&&reread.post_process==PostProcess::Off&&reread.quality==quality_before&&reread.renderer==RenderBackend::Vulkan,"presentation roundtrip");
     front.input(MenuInput::Down);check(front.selected()==Frontend::audio_row,"audio row must follow the presentation rows");
     for(int i=0;i<6;++i)front.input(MenuInput::Up);check(front.selected()==Frontend::video_row,"row order above presentation rows");
     {std::ofstream bad(video_path);bad<<"quality = high\nframe_rate = 45\n";}bool rejected_rate=false;try{VideoSettings::read(video_path);}catch(const std::exception&){rejected_rate=true;}check(rejected_rate,"invalid frame rate accepted");
     {std::ofstream bad(video_path);bad<<"quality = high\nupscaler = fsr3\n";}bool rejected=false;try{VideoSettings::read(video_path);}catch(const std::exception&){rejected=true;}check(rejected,"invalid upscaler accepted");
     video.set_upscaler(Upscaler::Off);}
    video.set_renderer(RenderBackend::OpenGL);
    video.set_renderer(RenderBackend::Software,false);check(VideoSettings::read(video_path).renderer==RenderBackend::OpenGL,"temporary renderer switch persisted");
    std::filesystem::remove(video_path);
    for(unsigned i=Frontend::video_row;i<Frontend::back_row;++i)front.input(MenuInput::Down);front.input(MenuInput::Confirm);check(front.active()&&!front.settings(),"back row moved");
    std::cout<<"Third-person camera mod: defaults, persistence, chase/aim pose, smoothing, scripted handover and menu row verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Third-person test failed: "<<e.what()<<'\n';return 1;}}
