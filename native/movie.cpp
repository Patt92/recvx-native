#include "movie.h"
#include "back_color.h"
#include "timing.h"
#include "asset_io.h"
#include "binary_view.h"
#include "screen_fade.h"
#include <cstring>
#include <limits>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "third_party/stb/stb_image.h"
namespace recvx {
void MoviePlayer::start(unsigned id,unsigned type) {
    if(active_)return; // PlayStartMovieEx: ExecMovieSystemFlag -> error 2, existing movie remains
    auto file=mods_/"movies"/(asset_number(id)+".rvv");
    if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"movies"/(asset_number(id)+".rvv");
    load(read_asset_file(file,512*1024*1024),id,type);
}
void MoviePlayer::load(std::vector<std::uint8_t> bytes,unsigned id,unsigned type) {
    // sdfunc.c:113, verified at ELF 0x32dcf0. MovieVolDef[22] (0x32dd10) is all zero.
    static constexpr unsigned char types[22]={2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,0x80,0x42,0x82,0,0x80,0,0x82};
    if(id>=22)throw std::runtime_error("movie outside MovieTypeDef");
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
    stop();
    // PlayStartMovieEx (sdfunc.c:3170): order is StopBgm, StopVoice, SleepAdxStream.
    audio_.stop_bgm(0);audio_.stop_voice(0);audio_.sleep_adx();
    bytes_=std::move(bytes);frames_=std::move(packets);width_=width;height_=height;numerator_=num;denominator_=den;
    frame_=0;ticks_=0;end_ticks_=std::max((std::uint64_t(count)*den*game_tick_hz+num-1)/num,(std::uint64_t(samples)*game_tick_hz+rate-1)/rate);decoded_=~0u;active_=true;
    // bhSysCallMovie: njSetBackColor(0,0,0) once the movie is ready (system.c:1141) and in every playing pass (:1171/:1184).
    set_back_color(0);
    cancel_allowed_=!(types[id]&0x80)||(type&1);fade_on_cancel_=(types[id]&0x40)||(type&2);cancelling_=false;
    volume_=0;volume_speed_=(volume_- -999.f)/30.f;
    // audioDecResume sets both block volumes to 32767 (ps2_MovieFunc.c:911).
    if(clip->frames())audio_.play_pcm(7,std::move(clip),32767.f/32768.f);
}
void MoviePlayer::stop(){
    const bool playing=active_;
    active_=false;cancelling_=false;audio_.stop_movie_audio();bytes_.clear();frames_.clear();pixels_.clear();
    if(playing){audio_.wakeup_adx();if(fade_flags)fade_flags()&=~2u;}
}
bool MoviePlayer::request_cancel(){
    if(!active_||!cancel_allowed_)return false;
    if(!fade_on_cancel_){stop();return true;}
    if(!cancelling_){
        if(!fade_flags)throw std::runtime_error("movie cancel fade without sys->cb_flg owner");
        cancelling_=true;set_screen_fade(fade_flags(),0xff000000u,30.f);
    }
    return true;
}
void MoviePlayer::tick(){if(!active_)return;
    // PlayMovieMain (sdfunc.c:3345): test cb_flg BEFORE controlling fade; a completed fade stops next pass.
    if(cancelling_){
        auto& flags=fade_flags();if(!(flags&2u)){stop();return;}
        control_screen_fade(flags);volume_-=volume_speed_;audio_.set_movie_volume(int(volume_));
    }
    const auto index=(++ticks_*numerator_)/(std::uint64_t(game_tick_hz)*denominator_);
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
    if(cancelling_&&system_screen_fade().visible())system_screen_fade().draw(out,image.width(),image.height());
}
}
