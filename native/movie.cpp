#include "movie.h"
#include "timing.h"
#include "asset_io.h"
#include "binary_view.h"
#include <cstring>
#include <limits>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "third_party/stb/stb_image.h"
namespace recvx {
void MoviePlayer::start(unsigned id) {
    auto file=mods_/"movies"/(asset_number(id)+".rvv");
    if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"movies"/(asset_number(id)+".rvv");
    load(read_asset_file(file,512*1024*1024));
}
void MoviePlayer::load(std::vector<std::uint8_t> bytes) {
    BinaryView v(bytes);v.require(0,36);
    if(std::memcmp(bytes.data(),"RVV1",4))throw std::runtime_error("invalid cooked movie magic");
    const auto width=v.u32(4),height=v.u32(8),num=v.u32(12),den=v.u32(16),count=v.u32(20);
    const auto rate=v.u32(24),channels=v.u32(28),samples=v.u32(32);
    if(!width||!height||width>4096||height>4096||!num||num>120000||!den||den>10000||!count||count>100000||rate<8000||rate>96000||channels!=2||samples>rate*600u)
        throw std::runtime_error("invalid cooked movie dimensions/timebase/audio");
    auto clip=std::make_shared<PcmClip>();clip->rate=rate;clip->channels=channels;
    std::size_t p=36;v.require(p,std::size_t(samples)*channels*2);
    clip->samples.resize(std::size_t(samples)*channels);
    for(auto& sample:clip->samples){sample=std::int16_t(v.u16(p));p+=2;}
    std::vector<Packet> packets;packets.reserve(count);
    for(unsigned i=0;i<count;++i){const auto n=v.u32(p);p+=4;if(n<4||n>16*1024*1024)throw std::runtime_error("invalid cooked movie frame length");v.require(p,n);
        if(v.u8(p)!=255||v.u8(p+1)!=216||v.u8(p+n-2)!=255||v.u8(p+n-1)!=217)throw std::runtime_error("invalid movie JPEG packet");packets.push_back({p,n});p+=n;}
    if(p!=bytes.size())throw std::runtime_error("trailing cooked movie data");
    // Commit only after every packet has passed validation.
    stop();bytes_=std::move(bytes);frames_=std::move(packets);width_=width;height_=height;numerator_=num;denominator_=den;
    frame_=0;ticks_=0;end_ticks_=std::max((std::uint64_t(count)*den*game_tick_hz+num-1)/num,(std::uint64_t(samples)*game_tick_hz+rate-1)/rate);decoded_=~0u;active_=true;
    if(clip->frames())audio_.play_pcm(7,std::move(clip));
}
void MoviePlayer::stop(){active_=false;audio_.stop(7);bytes_.clear();frames_.clear();pixels_.clear();}
void MoviePlayer::tick(){if(!active_)return;const auto index=(++ticks_*numerator_)/(std::uint64_t(game_tick_hz)*denominator_);
    if(ticks_>=end_ticks_){stop();return;}frame_=unsigned(std::min<std::uint64_t>(index,frames_.size()-1));
}
void MoviePlayer::draw(SoftwareRenderer& image){if(!active_)return;
    if(decoded_!=frame_){const auto packet=frames_.at(frame_);int width=0,height=0,channels=0;
        auto* rgb=stbi_load_from_memory(bytes_.data()+packet.offset,int(packet.bytes),&width,&height,&channels,3);
        if(!rgb)throw std::runtime_error("movie JPEG decoding failed");
        if(unsigned(width)!=width_||unsigned(height)!=height_){stbi_image_free(rgb);throw std::runtime_error("movie JPEG dimensions disagree with header");}
        pixels_.resize(std::size_t(width)*height);
        for(std::size_t i=0;i<pixels_.size();++i)pixels_[i]=0xff000000|(unsigned(rgb[i*3])<<16)|(unsigned(rgb[i*3+1])<<8)|rgb[i*3+2];
        stbi_image_free(rgb);decoded_=frame_;
    }
    auto& out=image.canvas();for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
        out[std::size_t(y)*image.width()+x]=pixels_[(std::size_t(y)*height_/image.height())*width_+std::size_t(x)*width_/image.width()];
}
}
