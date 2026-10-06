#include "video_settings.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace recvx {
namespace {
constexpr const char* names[]={"original","low","high","ultra"};
constexpr const char* labels[]={"Original (scharf, flimmert)","Niedrig (Filter)","Hoch (Filter + 2x SSAA)","Ultra (Filter + 3x SSAA)"};
}
std::string VideoSettings::describe()const{return labels[int(quality)];}
VideoSettings VideoSettings::read(const std::filesystem::path& path){
    VideoSettings out;std::ifstream file(path);if(!file)return out;std::string line;
    while(std::getline(file,line)){
        if(const auto hash=line.find_first_of("#;");hash!=std::string::npos)line.resize(hash);
        const auto eq=line.find('=');if(eq==std::string::npos)continue;
        auto trim=[](std::string s){const auto a=s.find_first_not_of(" \t\r"),b=s.find_last_not_of(" \t\r");return a==std::string::npos?std::string():s.substr(a,b-a+1);};
        const auto key=trim(line.substr(0,eq)),value=trim(line.substr(eq+1));
        if(key!="quality")throw std::runtime_error("unknown video setting: "+key);
        bool found=false;for(int i=0;i<4;++i)if(value==names[i]){out.quality=RenderQuality(i);found=true;}
        if(!found)throw std::runtime_error("invalid video quality: "+value);
    }
    return out;
}
void VideoSettings::save(const std::filesystem::path& path)const{
    std::ostringstream text;text<<"# RECVX host render quality: original, low, high, ultra\nquality = "<<names[int(quality)]<<'\n';
    auto temporary=path;temporary+=".tmp";
    {std::ofstream file(temporary);if(!file)throw std::runtime_error("cannot save video settings");file<<text.str();file.close();if(!file)throw std::runtime_error("video settings save failed");}
    std::filesystem::rename(temporary,path);
}
VideoConfig::VideoConfig(std::filesystem::path path):path_(std::move(path)){
    try{settings_=VideoSettings::read(path_);}catch(const std::exception& e){status_=std::string("video.ini ignoriert: ")+e.what();}
}
void VideoConfig::set(RenderQuality quality,bool persist){
    settings_.quality=quality;status_.clear();
    if(persist&&!path_.empty())try{settings_.save(path_);status_="Gespeichert";}catch(const std::exception& e){status_=e.what();}
}
void VideoConfig::cycle(int direction,bool persist){set(RenderQuality((int(settings_.quality)+4+(direction<0?-1:1))%4),persist);}
}
