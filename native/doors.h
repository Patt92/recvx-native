#pragma once
#include "door_core.h"
#include "renderer.h"
#include <filesystem>
namespace recvx {
struct DoorRequest {
    unsigned attribute=0,stage=0,room=0,spawn=0,type=0,room_case=0;
    std::string room_name() const;
};
class DoorSequence {
public:
    void begin(const std::filesystem::path& data,const std::filesystem::path& mods,const DoorRequest& request);
    void tick();
    void skip();
    bool active()const{return active_;}
    bool finished()const{return finished_;}
    bool skipped()const{return skipped_;}
    unsigned sound_bank()const{return sound_bank_;}
    std::vector<unsigned> take_sounds(){auto out=std::move(sounds_);sounds_.clear();return out;}
    unsigned ticks()const{return ticks_;}
    const DoorRequest& destination()const{return request_;}
    const RECVX_DOOR_STATE& state()const{return core_;}
    void draw(SoftwareRenderer& renderer);
private:
    DoorRequest request_{};
    RECVX_DOOR_STATE core_{};
    std::vector<std::uint8_t> model_,parameters_;
    std::vector<RECVX_DOOR_NODE> nodes_;
    std::vector<SceneTexture> textures_;
    std::size_t model_offset_=0,texture_offset_=0,objects_=0,count_=0;
    bool active_=false,finished_=false,skipped_=false;
    unsigned ticks_=0,sound_bank_=0;
    std::vector<unsigned> sounds_;
};
}
