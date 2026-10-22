#include "audio_settings.h"
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace recvx {
AudioConfig::AudioConfig(std::filesystem::path path):path_(std::move(path)){
    std::ifstream file(path_);if(!file)return;
    std::string line;
    try{while(std::getline(file,line)){
        if(const auto comment=line.find_first_of("#;");comment!=std::string::npos)line.resize(comment);
        const auto first=line.find_first_not_of(" \t\r");if(first==std::string::npos)continue;
        const auto eq=line.find('=');if(eq==std::string::npos)throw std::runtime_error("invalid audio setting");
        auto trim=[](std::string s){const auto a=s.find_first_not_of(" \t\r"),b=s.find_last_not_of(" \t\r");return a==std::string::npos?std::string():s.substr(a,b-a+1);};
        const auto key=trim(line.substr(0,eq)),value=trim(line.substr(eq+1));
        if(key!="sound_mode"||(value!="stereo"&&value!="mono"))throw std::runtime_error("invalid sound_mode (expected stereo or mono)");
        mode_=value=="mono"?1u:0u;
    }}catch(const std::exception& e){mode_=0;status_=e.what();}
}
void AudioConfig::set(unsigned mode,bool persist){
    if(mode>1)throw std::runtime_error("invalid sound mode");
    mode_=mode;status_.clear();
    if(persist&&!path_.empty())try{save();status_="Saved";}catch(const std::exception& e){status_=e.what();}
}
void AudioConfig::save()const{
    if(path_.has_parent_path())std::filesystem::create_directories(path_.parent_path());
    auto temporary=path_;temporary+=".tmp";
    {std::ofstream file(temporary);if(!file)throw std::runtime_error("cannot save audio settings");
        file<<"# Original sound output mode: stereo or mono\nsound_mode = "<<(mode_?"mono":"stereo")<<'\n';
        file.close();if(!file)throw std::runtime_error("audio settings save failed");}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("audio settings replace failed");
#else
    std::filesystem::rename(temporary,path_);
#endif
}
}
