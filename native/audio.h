#pragma once
#include <array>
#include "timing.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <vector>
namespace recvx {
inline constexpr unsigned audio_voice_count=24;
struct PcmClip {unsigned rate=0,channels=0;std::size_t loop_start=0,loop_end=0;std::vector<std::int16_t> samples;std::size_t frames()const{return channels?samples.size()/channels:0;}};
PcmClip decode_ps_adpcm(const std::vector<std::uint8_t>& bytes,unsigned rate);
PcmClip decode_system_sound(const std::vector<std::uint8_t>& bank,unsigned program);
PcmClip decode_adx(const std::vector<std::uint8_t>& bytes);
class AudioMixer {
public:
    explicit AudioMixer(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    void play(unsigned slot,const std::string& archive,unsigned id,float volume=1,bool loop=false,unsigned fade_ticks=0);
    void play_pcm(unsigned slot,std::shared_ptr<PcmClip> clip,float volume=1,bool loop=false);
    void play_system(unsigned program);
    bool play_weapon_sound(unsigned weapon_bank,unsigned program);
    void load_room_sounds(unsigned stage,unsigned room,unsigned variant=0);
    bool play_room_sound(unsigned bank,unsigned program,unsigned slot=4);
    bool enemy_sound(unsigned slot,unsigned sound);
    void stop_enemy_sound(unsigned slot);
    bool footstep(unsigned floor,unsigned type,unsigned actor,bool enabled=true);
    void load_door_sounds(unsigned bank);
    bool play_door_sound(unsigned program);
    void stop(unsigned slot,unsigned fade_ticks=0);
    bool active(unsigned slot)const;
    std::vector<std::int16_t> mix(unsigned frames=audio_frames_per_tick);
    // Original BGM control (sdfunc.c PlayBgmEx/PlayBgm2/StopBgm/StopBgm2,
    // Com_ExecRoomFadeOut/In; adxwrap.c volume and fade units) on ADX slot 0.
    // The request state persists across rooms like the original globals.
    void play_bgm_ex(int bgm,int fade_in_rate,int volume);
    void request_bgm2(int bgm,int volume);
    void stop_bgm(int fade_out_rate);
    void stop_bgm2();
    void room_fade_out(); // sound command 2 at door start
    void room_fade_in();  // sound command 1 once the new room is running
    int current_bgm()const{return current_bgm_;}
    int current_bgm_volume()const{return current_bgm_volume_;}
    int bgm_request()const{return req_fade_bgm_;}
    static float adx_gain(float volume);          // AdxVolTbl, 0.1 dB steps
    static unsigned adx_fade_ticks(int timer);    // RequestAdxFadeFunction2 count
    std::vector<std::string> played,unavailable;
private:
    struct Channel {std::shared_ptr<PcmClip> clip;double position=0;float volume=1,gain=1,target=1;unsigned fade=0;bool loop=false;};
    // Native shot/event voices are separate from CRI movie/ADX channels.
    std::array<Channel,audio_voice_count> slots_{};
    std::map<unsigned,std::shared_ptr<PcmClip>> system_sounds_;
    unsigned effect_slot_=2;
    int current_bgm_=-1,current_bgm_volume_=-127,req_fade_bgm_=0;
    void fade_gain(unsigned slot,float gain,unsigned ticks);
    std::map<unsigned,std::vector<std::uint8_t>> room_banks_;
    std::map<std::pair<unsigned,unsigned>,std::shared_ptr<PcmClip>> room_sounds_;
    unsigned room_pack_=0;
    std::array<bool,3> foot_switch_{};
    unsigned door_bank_=0,door_slot_=8;
    std::vector<std::uint8_t> door_source_;
    std::map<unsigned,std::shared_ptr<PcmClip>> door_sounds_;
    std::map<unsigned,std::vector<std::uint8_t>> weapon_banks_;
    std::map<std::pair<unsigned,unsigned>,std::shared_ptr<PcmClip>> weapon_sounds_;
    std::filesystem::path data_,mods_;
};
}
