#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace recvx {
struct PcmClip {unsigned rate=0,channels=0;std::vector<std::int16_t> samples;std::size_t frames()const{return channels?samples.size()/channels:0;}};
PcmClip decode_adx(const std::vector<std::uint8_t>& bytes);
class AudioMixer {
public:
    explicit AudioMixer(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    void play(unsigned slot,const std::string& archive,unsigned id,float volume=1,bool loop=false,unsigned fade_ticks=0);
    void play_pcm(unsigned slot,std::shared_ptr<PcmClip> clip,float volume=1,bool loop=false);
    void stop(unsigned slot,unsigned fade_ticks=0);
    bool active(unsigned slot)const;
    std::vector<std::int16_t> mix(unsigned frames=800);
    std::vector<std::string> played;
private:
    struct Channel {std::shared_ptr<PcmClip> clip;double position=0;float volume=1,gain=1,target=1;unsigned fade=0;bool loop=false;};
    std::array<Channel,8> slots_{};
    std::filesystem::path data_,mods_;
};
}
