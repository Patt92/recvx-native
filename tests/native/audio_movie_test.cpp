#include "audio.h"
#include "movie.h"
#include "asset_io.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include "movie_fixture.inc"
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> static void reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"malformed media accepted");}
static void le(std::vector<std::uint8_t>& b,unsigned n){for(unsigned i=0;i<4;++i)b.push_back(std::uint8_t(n>>(i*8)));}
int main(int argc,char** argv){try{
    static_assert(game_tick_hz==30&&audio_frames_per_tick==1600);
    {AudioMixer clock;auto second=std::make_shared<PcmClip>();second->rate=48000;second->channels=1;second->samples.assign(48000,100);clock.play_pcm(0,second);unsigned produced=0;for(unsigned tick=0;tick<30;++tick){auto samples=clock.mix();produced+=samples.size()/2;check(samples.front()==100&&samples.back()==100,"audio clock loses or advances samples");}check(produced==48000,"30 engine ticks do not represent one audio second");clock.mix(1);check(!clock.active(0),"one-second voice survives 30 engine ticks");}
    std::vector<std::uint8_t> spu(16);spu[0]=12;spu[1]=1;spu[2]=0xf1;auto ps=decode_ps_adpcm(spu,32000);check(ps.channels==1&&ps.frames()==28&&ps.samples[0]==1&&ps.samples[1]==-1,"SPU nibble order/sign/shift");
    auto malformed=spu;malformed[0]=0x50;reject([&]{decode_ps_adpcm(malformed,32000);});malformed=spu;malformed.pop_back();reject([&]{decode_ps_adpcm(malformed,32000);});
    // adxwrap.c units: AdxVolTbl is 0.1 dB; RequestAdxFadeFunction2 counts.
    check(std::abs(AudioMixer::adx_gain(0)-1)<1e-6f&&std::abs(AudioMixer::adx_gain(-25)-std::pow(10.f,-50/200.f))<1e-5f&&AudioMixer::adx_gain(-200)<AudioMixer::adx_gain(-126),"AdxVolTbl gain");
    check(AudioMixer::adx_fade_ticks(121)==21&&AudioMixer::adx_fade_ticks(400)==60&&AudioMixer::adx_fade_ticks(1)==1,"ADX fade counts");
    {AudioMixer bgm;bgm.request_bgm2(3,-45);check(bgm.current_bgm()==3&&bgm.bgm_request()==8,"PlayBgm2 new track");bgm.request_bgm2(3,-45);check(bgm.bgm_request()==8,"PlayBgm2 repeat keeps request");
        bgm.room_fade_out();check(bgm.bgm_request()==0&&bgm.current_bgm()==3,"room fade-out consumes request");bgm.request_bgm2(3,-30);check(bgm.bgm_request()==2,"PlayBgm2 volume change");
        bgm.room_fade_in();check(bgm.bgm_request()==0&&bgm.current_bgm_volume()==-30,"room fade-in volume request");bgm.stop_bgm2();check(bgm.bgm_request()==1&&bgm.current_bgm()==-1,"StopBgm2");}
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto bank=read_asset_file(root/"files/COMMON.MLT",8*1024*1024);for(unsigned id=0;id<11;++id){const auto sound=decode_system_sound(bank,id);check(sound.rate==32000&&sound.frames()>0&&std::any_of(sound.samples.begin(),sound.samples.end(),[](auto value){return value!=0;}),"original system sound silent/invalid");}reject([&]{decode_system_sound(bank,11);});auto truncated=bank;truncated.resize(256);reject([&]{decode_system_sound(truncated,0);});AudioMixer effects(root);effects.play_system(2);check(effects.active(2)&&effects.played.back()=="COMMON/0002","original cursor sound not routed");effects.load_room_sounds(0,0);for(unsigned id=0;id<5;++id){check(effects.play_room_sound(2,id,4),"original floor bank missing");const auto output=effects.mix(800);check(std::any_of(output.begin(),output.end(),[](auto value){return value!=0;}),"original floor sound silent");}check(effects.played.back()=="MULTSPQ1/0000/2/0004","floor sound route mismatch");effects.load_room_sounds(0,1);check(!effects.active(4)&&effects.play_room_sound(2,2,5)&&effects.played.back()=="MULTSPQ1/0001/2/0002","room sound reload incorrect");check(!effects.play_room_sound(15,0),"missing bank fabricated");reject([&]{effects.play_room_sound(2,100);});}

    std::vector<std::uint8_t> adx(32+36);adx[0]=128;adx[3]=28;adx[4]=3;adx[5]=18;adx[6]=4;adx[7]=2;adx[10]=187;adx[11]=128;adx[15]=32;adx[18]=3;
    std::copy_n("(c)CRI",6,adx.begin()+26);adx[33]=1;adx[34]=0x10;adx[51]=1;adx[52]=0xf0;
    const auto pcm=decode_adx(adx);check(pcm.rate==48000&&pcm.channels==2&&pcm.frames()==32,"ADX header/sample count");
    for(int i=0;i<32;++i)check(pcm.samples[i*2]==i+1&&pcm.samples[i*2+1]==-i-1,"ADX predictor/channel order");
    auto bad=adx;bad.pop_back();reject([&]{decode_adx(bad);});bad=adx;bad[19]=8;reject([&]{decode_adx(bad);});bad=adx;bad[7]=0;reject([&]{decode_adx(bad);});
    AudioMixer mixer;auto clip=std::make_shared<PcmClip>();clip->rate=24000;clip->channels=1;clip->samples={1000,3000};mixer.play_pcm(0,clip);
    check(mixer.mix(4)==std::vector<std::int16_t>({1000,1000,2000,2000,3000,3000,3000,3000}),"stereo resampling");mixer.mix(1);check(!mixer.active(0),"completed audio retained");
    clip->rate=48000;clip->samples={30000};mixer.play_pcm(0,clip,1,true);mixer.play_pcm(1,clip,1,true);check(mixer.mix(1)[0]==32767,"mixer clipping");mixer.stop(1);mixer.stop(0,1);
    const auto fade=mixer.mix(audio_frames_per_tick);check(fade.front()>29000&&fade.back()==0&&!mixer.active(0),"sample-accurate audio fade");
    reject([&]{mixer.play_pcm(0,clip,std::nanf(""));});clip->channels=3;reject([&]{mixer.play_pcm(0,clip);});
    std::vector<std::uint8_t> movie={'R','V','V','1'};for(unsigned n:{2,2,30,1,2,48000,2,3200})le(movie,n);movie.resize(36+12800);for(unsigned i=0;i<2;++i){le(movie,sizeof(jpeg_fixture));movie.insert(movie.end(),std::begin(jpeg_fixture),std::end(jpeg_fixture));}
    MoviePlayer player({}, {}, mixer);player.load(movie);check(player.active()&&player.frames()==2&&mixer.active(7),"movie/audio start");SoftwareRenderer image(8,8);player.draw(image);
    for(auto pixel:image.pixels())check(((pixel>>16)&255)>=250&&((pixel>>8)&255)<=3&&(pixel&255)<=3,"native JPEG frame color");
    player.tick();check(player.active(),"movie ended before frame duration");player.tick();check(!player.active()&&!mixer.active(7),"movie/audio termination");
    movie.pop_back();reject([&]{player.load(movie);});check(!player.active(),"failed movie start changed state");
    std::cout<<"ADX predictor, resampling, mixing, fades, movie frame decoding and timing passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
