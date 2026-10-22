#pragma once
#include "audio.h"
#include "renderer.h"
namespace recvx {
// Cooked JPEG frames and original PCM, indexed independently of display rate.
class MoviePlayer {
public:
    MoviePlayer(std::filesystem::path data, std::filesystem::path mods, AudioMixer& audio)
        : data_(std::move(data)), mods_(std::move(mods)), audio_(audio) {}
    void start(unsigned id,unsigned type=0);
    void load(std::vector<std::uint8_t> bytes,unsigned id=0,unsigned type=0);
    void stop();
    bool request_cancel(); // PlayMovieMain: MovieCancelFlag and optional MovieFadeFlag
    bool cancelling()const{return cancelling_;}
    bool cancel_allowed()const{return cancel_allowed_;}
    float volume()const{return volume_;}
    // The running world's sys->cb_flg. Movie fade uses the existing process-wide ScreenFade.
    std::function<std::uint32_t&()> fade_flags;
    void tick();
    void draw(SoftwareRenderer& image);
    bool active() const { return active_; }
    unsigned time_centiseconds() const { return active_?unsigned(ticks_*100/30):0; }
    unsigned frame() const { return frame_; }
    unsigned frames() const { return frames_.size(); }
private:
    struct Packet {std::size_t offset, bytes;};
    std::filesystem::path data_, mods_;
    AudioMixer& audio_;
    std::vector<std::uint8_t> bytes_;
    std::vector<Packet> frames_;
    std::vector<std::uint32_t> pixels_;
    unsigned width_=0,height_=0,numerator_=0,denominator_=0,frame_=0,decoded_=~0u;
    std::uint64_t ticks_=0,end_ticks_=0;
    bool active_=false;
    bool cancel_allowed_=true,fade_on_cancel_=false,cancelling_=false;
    float volume_=0,volume_speed_=0;
};
}
