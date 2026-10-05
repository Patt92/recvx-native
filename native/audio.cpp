#include "audio.h"
#include "asset_io.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace recvx {
PcmClip decode_adx(const std::vector<std::uint8_t>& b){
    auto need=[&](std::size_t p,std::size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("truncated ADX audio");};
    auto be16=[&](std::size_t p){need(p,2);return unsigned(b[p])*256+b[p+1];};
    auto be32=[&](std::size_t p){return std::uint32_t(be16(p))<<16|be16(p+2);};
    need(0,24);const auto start=be16(2)+4;
    if(be16(0)!=0x8000||b[4]!=3||b[5]!=18||b[6]!=4||b[19]!=0||b[18]!=3)throw std::runtime_error("unsupported ADX encoding/version/encryption");
    need(start,0);if(start<24||std::memcmp(b.data()+start-6,"(c)CRI",6))throw std::runtime_error("invalid ADX header");
    PcmClip out;out.channels=b[7];out.rate=be32(8);const auto frames=be32(12),cutoff=be16(16);
    if(!out.channels||out.channels>2||out.rate<8000||out.rate>96000||frames>out.rate*600u||cutoff>=out.rate/2)throw std::runtime_error("invalid ADX audio dimensions");
    const auto blocks=(std::size_t(frames)+31)/32;need(start,blocks*out.channels*18);
    // Independent implementation of the ADX second-order high-pass predictor.
    // Format reference: FFmpeg libavcodec/adx.c and adxdec.c (see docs/AUDIO.md).
    const double root=std::sqrt(2.0),angle=6.283185307179586*cutoff/out.rate;
    const double x=root-std::cos(angle),y=root-1,filter=(x-std::sqrt((x+y)*(x-y)))/y;
    const auto first=std::int64_t(std::lrintf(float(8192*filter))),second=std::int64_t(std::lrintf(float(-4096*filter*filter)));
    std::array<int,2> previous{},older{};out.samples.resize(std::size_t(frames)*out.channels);
    for(std::size_t block=0;block<blocks;++block)for(unsigned ch=0;ch<out.channels;++ch){const auto p=start+(block*out.channels+ch)*18;const auto scale=be16(p);if(scale&0x8000)throw std::runtime_error("premature ADX end marker");
        for(unsigned k=0;k<32&&block*32+k<frames;++k){int residual=(b[p+2+k/2]>>((k&1)?0:4))&15;if(residual>=8)residual-=16;
            const auto prediction=first*previous[ch]+second*older[ch];const auto rounded=prediction>=0?prediction/4096:-((-prediction+4095)/4096);
            const int sample=int(std::clamp<std::int64_t>(std::int64_t(residual)*scale+rounded,-32768,32767));older[ch]=previous[ch];previous[ch]=sample;out.samples[(block*32+k)*out.channels+ch]=std::int16_t(sample);}
    }return out;
}
void AudioMixer::play_pcm(unsigned slot,std::shared_ptr<PcmClip> clip,float volume,bool loop){if(slot>=slots_.size()||!clip||!clip->channels||clip->channels>2||clip->rate<8000||clip->rate>96000||clip->samples.size()%clip->channels||!clip->frames()||!std::isfinite(volume)||volume<0||volume>8)throw std::runtime_error("invalid audio channel/clip");slots_[slot]={std::move(clip),0,volume,1,1,0,loop};}
void AudioMixer::play(unsigned slot,const std::string& archive,unsigned id,float volume,bool loop,unsigned fade_ticks){
    if(fade_ticks>600*60)throw std::runtime_error("audio fade too long");
    const auto relative=std::filesystem::path("audio")/archive/(asset_number(id)+".adx");auto file=mods_/relative;
    if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"archives"/archive/(asset_number(id)+".bin");
    auto clip=std::make_shared<PcmClip>(decode_adx(read_asset_file(file,32*1024*1024)));play_pcm(slot,clip,volume,loop);
    if(fade_ticks){slots_[slot].gain=0;slots_[slot].fade=fade_ticks*800;}played.push_back(archive+"/"+asset_number(id));
}
void AudioMixer::stop(unsigned slot,unsigned fade_ticks){if(slot>=slots_.size()||fade_ticks>600*60)throw std::runtime_error("invalid audio slot");auto& s=slots_[slot];if(fade_ticks){s.target=0;s.fade=fade_ticks*800;}else s.clip.reset();}
bool AudioMixer::active(unsigned slot)const{return slot<slots_.size()&&bool(slots_[slot].clip);}
std::vector<std::int16_t> AudioMixer::mix(unsigned frames){if(frames>48000*10)throw std::runtime_error("audio mix too large");std::vector<std::int16_t> out(std::size_t(frames)*2);
    for(unsigned f=0;f<frames;++f){float left=0,right=0;for(auto& channel:slots_){if(!channel.clip)continue;auto& clip=*channel.clip;
        if(channel.position>=clip.frames()){if(channel.loop)channel.position=std::fmod(channel.position,double(clip.frames()));else{channel.clip.reset();continue;}}
        if(channel.fade){channel.gain+=(channel.target-channel.gain)/channel.fade;if(!--channel.fade&&channel.target==0){channel.clip.reset();continue;}}
        const auto i=std::size_t(channel.position),next=std::min(i+1,clip.frames()-1);const float fraction=float(channel.position-i),gain=channel.volume*channel.gain;
        auto sample=[&](unsigned ch){const float a=clip.samples[i*clip.channels+ch],b=clip.samples[next*clip.channels+ch];return (a+(b-a)*fraction)*gain;};left+=sample(0);right+=sample(clip.channels==1?0:1);channel.position+=double(clip.rate)/48000;
    }out[f*2]=std::int16_t(std::clamp(left,-32768.f,32767.f));out[f*2+1]=std::int16_t(std::clamp(right,-32768.f,32767.f));}return out;
}
}
