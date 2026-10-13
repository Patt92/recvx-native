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
    // BEWUSSTE ABWEICHUNG VOM ORIGINAL - NICHT ERNEUT WEGPORTIEREN.
    // Im Original gibt es keinen Tuer-Skip: bhSysCallDoordemo (system.c:939)
    // liest kein Pad und kein Flag, bhStartDoorDemo (room.c:978) loescht
    // pad_ps/pad_on, und bhControlDoor (door.c:386-528) endet erst, wenn
    // FadeProc1 das Statusbit 0x40 gesetzt hat (door.c:638-646).
    // skip() ist eine vom Nutzer ausdruecklich gewuenschte Host-Komfort-
    // funktion (Entscheidung 2026-10-13); die noch klingenden Tuerstimmen
    // werden dabei gestoppt. Siehe docs/port_ledger.tsv und CLAUDE.md.
    void skip();
    bool active()const{return active_;}
    bool finished()const{return finished_;}
    bool skipped()const{return skipped_;} // ended by the host skip, not by FadeProc1
    bool direct()const{return direct_;} // no door demo (attr 0x80000000 / type 255)
    unsigned sound_bank()const{return sound_bank_;}
    std::vector<unsigned> take_sounds(){auto out=std::move(sounds_);sounds_.clear();return out;}
    std::vector<unsigned> take_sound_commands(){auto out=std::move(sound_commands_);sound_commands_.clear();return out;}
    // PuruProc1: CallSystemSe numbers, and StartVibrationEx requests as
    // {attribute, VibP pattern}. The pattern envelope of pdVibMxStart is a
    // platform binding this port does not have; the request is not invented.
    std::vector<int> take_shake_sounds(){auto out=std::move(shake_sounds_);shake_sounds_.clear();return out;}
    std::vector<std::pair<int,int>> take_shake_patterns(){auto out=std::move(shake_patterns_);shake_patterns_.clear();return out;}
    unsigned ticks()const{return ticks_;}
    const DoorRequest& destination()const{return request_;}
    const RECVX_DOOR_STATE& state()const{return core_;}
    // njClipZ of bhSetDoor; clip_case is true when dmo_clip had an entry.
    float clip_near()const{return clip_near_;}
    float clip_far()const{return clip_far_;}
    bool clip_case()const{return clip_case_;}
    void draw(SoftwareRenderer& renderer);
    void light_scene(Scene& scene)const;
private:
    DoorRequest request_{};
    RECVX_DOOR_STATE core_{};
    std::vector<std::uint8_t> model_,parameters_;
    std::vector<RECVX_DOOR_NODE> nodes_;
    std::vector<SceneTexture> textures_;
    std::size_t model_offset_=0,texture_offset_=0,objects_=0,count_=0;
    bool active_=false,finished_=false,skipped_=false,direct_=false,clip_case_=false;
    float clip_near_=2.0f,clip_far_=20000.0f;
    unsigned ticks_=0,sound_bank_=0;
    std::vector<unsigned> sounds_,sound_commands_;
    std::vector<int> shake_sounds_;
    std::vector<std::pair<int,int>> shake_patterns_;
    std::array<int,11> sound_timing_{};
};
}
