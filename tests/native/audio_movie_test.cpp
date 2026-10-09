#include "audio.h"
#include "movie.h"
#include "asset_io.h"
#include "binary_view.h"
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
    if(argc>1){ // CORE character banks: original CallPlayerVoice bank 4, slot 7; replacement and room retention.
        const auto root=std::filesystem::u8path(argv[1]);
        for(unsigned disc:{0u,1u}){AudioMixer voices(root);voices.set_disc(disc);voices.load_room_sounds(0,2);
            std::vector<std::int16_t> previous;
            for(unsigned player=0;player<4;++player){check(voices.load_player_sounds(player),"original CORE bank missing");check(!voices.active(7),"character change keeps old voice");
                voices.play_player_voice(1026);check(voices.active(7),"bite voice not routed to slot 7");
                const auto pcm=voices.mix(800);check(std::any_of(pcm.begin(),pcm.end(),[](auto n){return n!=0;}),"CORE bite voice silent");
                if(!previous.empty())check(pcm!=previous,"character change retains cached CORE sample");previous=pcm;
                const auto route=voices.played.back();voices.load_room_sounds(0,9);voices.play_player_voice(1025);
                check(voices.active(7)&&voices.played.back().substr(0,voices.played.back().rfind('/'))==route.substr(0,route.rfind('/')),"room change loses CORE bank/provenance");
                const auto death=voices.mix(800);check(std::any_of(death.begin(),death.end(),[](auto n){return n!=0;}),"CORE death voice silent");}
            check(voices.unavailable.empty(),"CORE voices reported unavailable");}
        // system.c:1602 hands over to the disc change task whenever the mounted
        // sound set does not match the stage, so stages 6..9 always run from the
        // disc-2 archives. BGM ids 121, 122 and 124 exist only there.
        {AudioMixer mount(root);
            for(unsigned stage=0;stage<10;++stage){mount.mount_sound_afs(stage);
                check(mount.disc()==(stage>=6?1u:0u),"original stage/disc rule lost");}
            mount.mount_sound_afs(9);check(mount.set_name("BGM")=="BGM2","stage 9 does not mount the disc-2 BGM set");
            for(const unsigned bgm:{121u,122u,124u}){
                mount.play(0,mount.set_name("BGM"),bgm,1,true,0);
                check(mount.active(0),"disc-2 BGM entry did not play");
                check(mount.played.back()=="BGM2/"+asset_number(bgm),"disc-2 BGM came from the wrong archive");
                mount.stop(0);}
            mount.mount_sound_afs(0);check(mount.set_name("BGM")=="BGM1","stage 0 does not mount the disc-1 BGM set");}
    }
    // adxwrap.c units: AdxVolTbl is 0.1 dB; RequestAdxFadeFunction2 counts.
    check(std::abs(AudioMixer::adx_gain(0)-1)<1e-6f&&std::abs(AudioMixer::adx_gain(-25)-std::pow(10.f,-50/200.f))<1e-5f&&AudioMixer::adx_gain(-200)<AudioMixer::adx_gain(-126),"AdxVolTbl gain");
    check(AudioMixer::adx_fade_ticks(121)==21&&AudioMixer::adx_fade_ticks(400)==60&&AudioMixer::adx_fade_ticks(1)==1,"ADX fade counts");
    {AudioMixer bgm;bgm.request_bgm2(3,-45);check(bgm.current_bgm()==3&&bgm.bgm_request()==8,"PlayBgm2 new track");bgm.request_bgm2(3,-45);check(bgm.bgm_request()==8,"PlayBgm2 repeat keeps request");
        bgm.room_fade_out();check(bgm.bgm_request()==0&&bgm.current_bgm()==3,"room fade-out consumes request");bgm.request_bgm2(3,-30);check(bgm.bgm_request()==2,"PlayBgm2 volume change");
        bgm.room_fade_in();check(bgm.bgm_request()==0&&bgm.current_bgm_volume()==-30,"room fade-in volume request");bgm.stop_bgm2();check(bgm.bgm_request()==1&&bgm.current_bgm()==-1,"StopBgm2");}
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto bank=read_asset_file(root/"files/COMMON.MLT",8*1024*1024);for(unsigned id=0;id<11;++id){const auto sound=decode_system_sound(bank,id);check(sound.rate==32000&&sound.frames()>0&&std::any_of(sound.samples.begin(),sound.samples.end(),[](auto value){return value!=0;}),"original system sound silent/invalid");}reject([&]{decode_system_sound(bank,11);});auto truncated=bank;truncated.resize(256);reject([&]{decode_system_sound(truncated,0);});AudioMixer effects(root);effects.play_system(2);check(effects.active(2)&&effects.played.back()=="COMMON/0002","original cursor sound not routed");effects.load_room_sounds(0,0);for(unsigned id=0;id<5;++id){check(effects.play_room_sound(2,id,4),"original floor bank missing");const auto output=effects.mix(800);check(std::any_of(output.begin(),output.end(),[](auto value){return value!=0;}),"original floor sound silent");}check(effects.played.back()=="MULTSPQ1/0000/2/0004","floor sound route mismatch");effects.load_room_sounds(0,1);check(!effects.active(4)&&effects.play_room_sound(2,2,5)&&effects.played.back()=="MULTSPQ1/0001/2/0002","room sound reload incorrect");check(!effects.play_room_sound(15,0),"missing bank fabricated");reject([&]{effects.play_room_sound(2,100);});}

    if(argc>1){AudioMixer enemies(std::filesystem::u8path(argv[1]));enemies.load_room_sounds(0,2);for(unsigned slot=0;slot<6;++slot)check(enemies.enemy_sound(slot,0)&&enemies.active(18+slot),"original enemy bank or independent slot missing");check(enemies.played.back()=="MULTSPQ1/0002/3/0000","enemy bank/list selection incorrect");enemies.stop_enemy_sound(2);for(unsigned slot=0;slot<6;++slot)check(enemies.active(18+slot)==(slot!=2),"enemy sound stop steals another voice");const auto samples=enemies.mix(100);check(std::any_of(samples.begin(),samples.end(),[](auto n){return n!=0;}),"enemy PCM silent");enemies.load_room_sounds(0,0);check(!enemies.active(18),"enemy voice survives room reload");reject([&]{enemies.enemy_sound(6,0);});}
    // RM_9340 bank 3 programs 60..67 carry no split. The mdlver-38 zombie (ene01_typ_tbl type 18,
    // add_seno 40) requests list 63 there: an original silent voice, not an unavailable sound.
    if(argc>1){AudioMixer silent(std::filesystem::u8path(argv[1]));silent.set_disc(1);silent.load_room_sounds(9,34);
        silent.request_enemy_se(3,0x0101131c);silent.exec_enemy_se();check(silent.active(18),"RM_9340 zombie voice 28 missing");
        silent.request_enemy_se(1,0x0101133f);silent.exec_enemy_se();check(silent.unavailable.empty(),"split-less enemy program reported unavailable");
        check(silent.played.back()=="MULTSPQ2/0083/3/0063"&&!silent.active(19),"split-less enemy program not served silently");}

    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto index=read_asset_file(root/"files/MULTSPQ1.IDX",4096);BinaryView ids(index);
        // COMMON SQ bank 1 uses key 60 and velocities 120/110/120/120.
        // Compare actual stereo PCM against the independently decoded HD split,
        // including a room load before the closing sample is first mixed.
        for(unsigned bank:{2u,7u}){unsigned pack=0;while(pack<index.size()/2&&ids.u16(pack*2)!=(0x8000|bank))++pack;check(pack<index.size()/2,"original door SPQ not found");const auto bytes=read_asset_file(root/"archives/MULTSPQ1"/(asset_number(pack)+".bin"),32*1024*1024);BinaryView spq(bytes);std::vector<std::uint8_t> programs;
            for(unsigned i=0;i<16&&spq.u32(i*16);++i)if(spq.u32(i*16+8)==1&&spq.u32(i*16+12)==1){const auto offset=spq.u32(i*16),size=spq.u32(i*16+4);spq.require(offset,size);programs={bytes.begin()+offset,bytes.begin()+offset+size};break;}
            for(unsigned list=0;list<(bank==2?2u:3u);++list){AudioMixer door(root),reference;door.load_door_sounds(bank);check(door.play_door_sound(list),"COMMON door sequence unresolved");auto clip=std::make_shared<PcmClip>(decode_system_sound(programs,list,60));reference.play_pcm(27,clip,float(list==1?110:120)/127.f);door.load_room_sounds(0,6);const auto actual=door.mix(1600),pcm=reference.mix(1300);std::vector<std::int16_t> expected(600);expected.insert(expected.end(),pcm.begin(),pcm.end()); // COMMON: 6 ticks, PPQN 480, tempo 500000 => 300 output samples
                check(std::any_of(actual.begin(),actual.end(),[](auto value){return value!=0;}),"door PCM silent after destination load");for(unsigned i=0;i<actual.size();++i)check(std::abs(int(actual[i])-int(expected[i]))<=1,"door key/velocity/program/pan/timing differs from COMMON SQ");}
        }
        AudioMixer tail(root);tail.load_door_sounds(7);check(tail.play_door_sound(2)&&tail.active(27),"closing door voice missing");tail.load_room_sounds(0,7);tail.finish_door_sound();check(tail.active(27),"room completion cuts closing door voice immediately");const auto closing=tail.mix(1600);check(std::any_of(closing.begin(),closing.end(),[](auto value){return value!=0;}),"closing door fade starts silent");tail.mix(48000*2);check(!tail.active(27),"completed closing voice not released");
    }
    if(argc>1){AudioMixer detector(std::filesystem::u8path(argv[1]));detector.load_room_sounds(0,9);detector.custom_midi_slots(1,4);
        check(detector.event_sound(4,0x202),"original detector SQ missing");
        for(unsigned cycle=0;cycle<5;++cycle){const auto pcm=detector.mix(60000);check(detector.active(8)&&std::any_of(pcm.begin(),pcm.end(),[](auto sample){return sample!=0;}),"Sony NRPN alarm loop stopped or remained silent");}
        check(detector.stop_event_sound(4)&&!detector.active(8),"script alarm stop ignored");const auto quiet=detector.mix(1600);check(std::all_of(quiet.begin(),quiet.end(),[](auto sample){return sample==0;}),"stopped alarm still sounds");
        detector.custom_midi_slots(3,2);const auto count=detector.played.size();check(detector.event_sound(4,0x202)&&detector.played.size()==count,"CustomMidiSlotDef event limit ignored");
        detector.custom_midi_slots(4,5);check(detector.event_sound(4,0x202)&&detector.active(8),"invalid custom slots did not restore original defaults");
        detector.begin_room_init();detector.custom_midi_slots(3,2);detector.stop_event_sound(2);check(detector.active(8),"stopping a different event slot stole alarm voice");
    }

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
    check(player.time_centiseconds()==0,"movie starts at nonzero script time");player.tick();check(player.active()&&player.time_centiseconds()==3,"movie time must truncate 30-Hz ticks to centiseconds");player.tick();check(!player.active()&&!mixer.active(7)&&player.time_centiseconds()==0,"movie/audio termination");
    movie.pop_back();reject([&]{player.load(movie);});check(!player.active(),"failed movie start changed state");
    std::cout<<"ADX predictor, resampling, mixing, fades, movie frame decoding and timing passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
