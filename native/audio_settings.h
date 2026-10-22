#pragma once
#include <filesystem>
#include <string>
namespace recvx {
// Host storage of the original system configuration's ssd_reserve / syCfg sound mode.
// This is global configuration, not part of SAVEFILE or a room snapshot.
class AudioConfig {
public:
    explicit AudioConfig(std::filesystem::path path={});
    unsigned mode()const{return mode_;} // SetSoundModeEx: 0 stereo, 1 mono
    void set(unsigned mode,bool persist=true);
    void toggle(){set(mode_^1u);}
    std::string describe()const{return mode_?"Mono":"Stereo";}
    const std::string& status()const{return status_;}
private:
    std::filesystem::path path_;
    unsigned mode_=0;
    std::string status_;
    void save()const;
};
}
