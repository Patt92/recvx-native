#include "video_settings.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace recvx {
namespace {
constexpr const char* names[]={"original","low","high","ultra"};
constexpr const char* labels[]={"Original (sharp, shimmers)","Low (filter)","High (filter + 2x SSAA)","Ultra (filter + 3x SSAA)"};
constexpr const char* frame_rate_names[]={"30","60","120","unlimited"};
constexpr const char* frame_rate_labels[]={"30 (game tick)","60","120","Unlimited (no vsync)"};
constexpr const char* upscaler_names[]={"off","metalfx-spatial","metalfx-temporal"};
constexpr const char* upscaler_labels[]={"Off (nearest)","MetalFX spatial","MetalFX temporal"};
constexpr const char* frame_generation_names[]={"off","metalfx"};
constexpr const char* frame_generation_labels[]={"Off","MetalFX frame interpolation"};
constexpr const char* post_process_names[]={"off","sharpen","crt"};
constexpr const char* post_process_labels[]={"Off","Sharpen","CRT scanlines + sharpen"};
template<class E,std::size_t N> bool parse(const char* const (&table)[N],const std::string& value,E& out){
    for(std::size_t i=0;i<N;++i)if(value==table[i]){out=E(i);return true;}
    return false;
}
}
std::string VideoSettings::describe()const{return labels[int(quality)];}
std::string VideoSettings::describe_frame_rate()const{return frame_rate_labels[int(frame_rate)];}
std::string VideoSettings::describe_upscaler()const{return upscaler_labels[int(upscaler)];}
std::string VideoSettings::describe_frame_generation()const{return frame_generation_labels[int(frame_generation)];}
std::string VideoSettings::describe_post_process()const{return post_process_labels[int(post_process)];}
VideoSettings VideoSettings::read(const std::filesystem::path& path){
    VideoSettings out;std::ifstream file(path);if(!file)return out;std::string line;
    while(std::getline(file,line)){
        if(const auto hash=line.find_first_of("#;");hash!=std::string::npos)line.resize(hash);
        const auto eq=line.find('=');if(eq==std::string::npos)continue;
        auto trim=[](std::string s){const auto a=s.find_first_not_of(" \t\r"),b=s.find_last_not_of(" \t\r");return a==std::string::npos?std::string():s.substr(a,b-a+1);};
        const auto key=trim(line.substr(0,eq)),value=trim(line.substr(eq+1));
        if(key=="renderer"){
            if(value=="gpu")out.renderer=RenderBackend::Gpu;else if(value=="software")out.renderer=RenderBackend::Software;else if(value=="opengl")out.renderer=RenderBackend::OpenGL;
            else if(value=="vulkan")out.renderer=RenderBackend::Vulkan;
            else throw std::runtime_error("invalid video renderer: "+value);continue;}
        if(key=="frame_rate"){if(!parse(frame_rate_names,value,out.frame_rate))throw std::runtime_error("invalid video frame_rate: "+value);continue;}
        if(key=="upscaler"){if(!parse(upscaler_names,value,out.upscaler))throw std::runtime_error("invalid video upscaler: "+value);continue;}
        if(key=="frame_generation"){if(!parse(frame_generation_names,value,out.frame_generation))throw std::runtime_error("invalid video frame_generation: "+value);continue;}
        if(key=="post_process"){if(!parse(post_process_names,value,out.post_process))throw std::runtime_error("invalid video post_process: "+value);continue;}
        if(key!="quality")throw std::runtime_error("unknown video setting: "+key);
        bool found=false;for(int i=0;i<4;++i)if(value==names[i]){out.quality=RenderQuality(i);found=true;}
        if(!found)throw std::runtime_error("invalid video quality: "+value);
    }
    return out;
}
void VideoSettings::save(const std::filesystem::path& path)const{
    std::ostringstream text;text<<"# RECVX host render quality: original, low, high, ultra\nquality = "<<names[int(quality)]<<'\n'
        <<"# Rasterizer: gpu (platform default), opengl, vulkan or software\nrenderer = "
        <<(renderer==RenderBackend::Software?"software":renderer==RenderBackend::OpenGL?"opengl":renderer==RenderBackend::Vulkan?"vulkan":"gpu")<<'\n'
        <<"# Presentation frame rate (game tick stays 30 Hz): 30, 60, 120 or unlimited\nframe_rate = "<<frame_rate_names[int(frame_rate)]<<'\n'
        <<"# Presentation upscaler (macOS, MetalFX): off, metalfx-spatial or metalfx-temporal\nupscaler = "<<upscaler_names[int(upscaler)]<<'\n'
        <<"# Frame generation (macOS 26 or newer, MetalFX frame interpolation): off or metalfx\nframe_generation = "<<frame_generation_names[int(frame_generation)]<<'\n'
        <<"# Post-processing of the presented image: off, sharpen or crt\npost_process = "<<post_process_names[int(post_process)]<<'\n';
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    auto temporary=path;temporary+=".tmp";
    {std::ofstream file(temporary);if(!file)throw std::runtime_error("cannot save video settings");file<<text.str();file.close();if(!file)throw std::runtime_error("video settings save failed");}
    std::filesystem::rename(temporary,path);
}
VideoConfig::VideoConfig(std::filesystem::path path):path_(std::move(path)){
    try{settings_=VideoSettings::read(path_);}catch(const std::exception& e){status_=std::string("video.ini ignoriert: ")+e.what();}
}
void VideoConfig::persist(bool persist){
    status_.clear();
    if(persist&&!path_.empty())try{settings_.save(path_);status_="Saved";}catch(const std::exception& e){status_=e.what();}
}
void VideoConfig::set(RenderQuality quality,bool persist){settings_.quality=quality;this->persist(persist);}
void VideoConfig::set_renderer(RenderBackend renderer,bool persist){settings_.renderer=renderer;this->persist(persist);}
void VideoConfig::set_frame_rate(FrameRate value,bool persist){settings_.frame_rate=value;this->persist(persist);}
void VideoConfig::set_upscaler(Upscaler value,bool persist){settings_.upscaler=value;this->persist(persist);}
void VideoConfig::set_frame_generation(FrameGeneration value,bool persist){settings_.frame_generation=value;this->persist(persist);}
void VideoConfig::set_post_process(PostProcess value,bool persist){settings_.post_process=value;this->persist(persist);}
void VideoConfig::cycle(int direction,bool persist){set(RenderQuality((int(settings_.quality)+4+(direction<0?-1:1))%4),persist);}
void VideoConfig::cycle_frame_rate(int direction,bool persist){set_frame_rate(FrameRate((int(settings_.frame_rate)+4+(direction<0?-1:1))%4),persist);}
void VideoConfig::cycle_upscaler(int direction,bool persist){set_upscaler(Upscaler((int(settings_.upscaler)+3+(direction<0?-1:1))%3),persist);}
void VideoConfig::cycle_frame_generation(int direction,bool persist){set_frame_generation(FrameGeneration((int(settings_.frame_generation)+2+(direction<0?-1:1))%2),persist);}
void VideoConfig::cycle_post_process(int direction,bool persist){set_post_process(PostProcess((int(settings_.post_process)+3+(direction<0?-1:1))%3),persist);}
std::string VideoConfig::describe_upscaler()const{return settings_.describe_upscaler()+(settings_.upscaler!=Upscaler::Off&&!upscaler_available_?" (not available)":"");}
std::string VideoConfig::describe_frame_generation()const{return settings_.describe_frame_generation()+(settings_.frame_generation!=FrameGeneration::Off&&!frame_generation_available_?" (not available)":"");}
std::string VideoConfig::describe_post_process()const{return settings_.describe_post_process()+(settings_.post_process!=PostProcess::Off&&!presentation_available_?" (not available)":"");}
}
