#include "audio.h"
#include "asset_io.h"
#include "binary_view.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace recvx {
namespace {
#include "adx_tables.inc"
}
// Sony HD/BD metadata and SPU-ADPCM format references are listed in docs/AUDIO.md.
PcmClip decode_ps_adpcm(const std::vector<std::uint8_t>& bytes,unsigned rate){
    if(bytes.empty()||bytes.size()%16||bytes.size()>32*1024*1024||rate<8000||rate>96000)throw std::runtime_error("invalid SPU-ADPCM dimensions");
    static constexpr int coefficients[5][2]={{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
    PcmClip out;out.rate=rate;out.channels=1;int previous=0,older=0;
    for(std::size_t p=0;p<bytes.size();p+=16){const unsigned filter=bytes[p]>>4,shift=bytes[p]&15,flags=bytes[p+1];
        if(filter>4||shift>12||flags>7)throw std::runtime_error("invalid SPU-ADPCM frame");
        for(unsigned i=0;i<28;++i){int nibble=(bytes[p+2+i/2]>>((i&1)*4))&15;if(nibble>=8)nibble-=16;
            const int residual=nibble*(1<<(12-shift));const int predictor=coefficients[filter][0]*previous+coefficients[filter][1]*older;
            const int sample=flags==7?0:std::clamp(residual+(predictor>=0?predictor/64:-((-predictor+63)/64)),-32768,32767);
            out.samples.push_back(std::int16_t(sample));older=previous;previous=sample;
        }if(flags&1)break;
    }return out;
}
PcmClip decode_system_sound(const std::vector<std::uint8_t>& bytes,unsigned program){
    BinaryView v(bytes);v.require(0,24);const auto hd=v.u32(0),hd_size=v.u32(4),bd=v.u32(8),bd_size=v.u32(12);v.require(hd,hd_size);v.require(bd,bd_size);
    auto magic=[&](std::size_t p,const char* expected){v.require(p,8);if(std::memcmp(bytes.data()+p,expected,8))throw std::runtime_error("invalid Sony sound-bank section");};
    magic(hd,"IECSsreV");const auto head=hd+v.u32(hd+8);magic(head,"IECSdaeH");
    auto section=[&](unsigned field,const char* name){const auto p=hd+v.u32(head+field);magic(p,name);const auto length=v.u32(p+8),last=v.u32(p+12);if(last>4096)throw std::runtime_error("Sony sound-bank table too large");v.require(p,length);v.require(p+16,(last+1)*4);return p;};
    const auto prog=section(0x14,"IECSgorP"),sets=section(0x18,"IECStesS"),samples=section(0x1c,"IECSlpmS"),vags=section(0x20,"IECSigaV");
    auto entry=[&](std::size_t table,unsigned index,std::size_t size){if(index>v.u32(table+12))throw std::runtime_error("Sony sound-bank index outside table");const auto offset=v.u32(table+16+index*4),length=v.u32(table+8);if(offset>length||size>length-offset)throw std::runtime_error("Sony sound-bank entry outside section");return v.relative(table,offset,size);};
    const auto pg=entry(prog,program,0x28);const auto zone=v.relative(pg,v.u32(pg),2);const auto set=entry(sets,v.u16(zone),6);
    if(v.u8(set+3)!=1)throw std::runtime_error("layered system sound unsupported");const auto sample=entry(samples,v.u16(set+4),2);const auto vag=entry(vags,v.u16(sample),8);
    const auto start=v.u32(vag);std::size_t end=bd_size;for(unsigned i=0;i<=v.u32(vags+12);++i){const auto next=v.u32(entry(vags,i,8));if(next>start)end=std::min(end,std::size_t(next));}
    if(start>=end||end>bd_size)throw std::runtime_error("invalid Sony sound-bank waveform range");
    return decode_ps_adpcm(std::vector<std::uint8_t>(bytes.begin()+bd+start,bytes.begin()+bd+end),v.u16(vag+4));
}
void AudioMixer::play_system(unsigned program){
    auto found=system_sounds_.find(program);if(found==system_sounds_.end()){auto file=mods_/"audio/system/COMMON.MLT";if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"files/COMMON.MLT";
        found=system_sounds_.emplace(program,std::make_shared<PcmClip>(decode_system_sound(read_asset_file(file,8*1024*1024),program))).first;}
    play_pcm(effect_slot_,found->second);effect_slot_=effect_slot_==2?3:2;played.push_back("COMMON/"+asset_number(program));
}
bool AudioMixer::play_weapon_sound(unsigned bank,unsigned program){
    if(data_.empty())return false;
    auto source=weapon_banks_.find(bank);if(source==weapon_banks_.end()){
        const auto index=read_asset_file(data_/"files/MULTSPQ1.IDX",4096);BinaryView ids(index);unsigned pack=0;bool found=false;
        for(;pack<index.size()/2;++pack){const auto key=ids.u16(pack*2);if(key==0xffff)break;if(key==(0x4000|bank)){found=true;break;}}if(!found)return false;
        auto file=mods_/"audio/MULTSPQ1"/(asset_number(pack)+".bin");if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"archives/MULTSPQ1"/(asset_number(pack)+".bin");const auto bytes=read_asset_file(file,32*1024*1024);BinaryView v(bytes);
        for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset)break;const auto size=v.u32(p+4);v.require(offset,size);if(v.u32(p+8)==2&&v.u32(p+12)==1){source=weapon_banks_.emplace(bank,std::vector<std::uint8_t>(bytes.begin()+offset,bytes.begin()+offset+size)).first;break;}}
        if(source==weapon_banks_.end())return false;
    }
    auto found=weapon_sounds_.find({bank,program});if(found==weapon_sounds_.end())found=weapon_sounds_.emplace(std::make_pair(bank,program),std::make_shared<PcmClip>(decode_system_sound(source->second,program))).first;
    play_pcm(6,found->second);played.push_back("ARMS/"+asset_number(bank)+"/"+asset_number(program));return true;
}
void AudioMixer::load_room_sounds(unsigned stage,unsigned room,unsigned variant){
    if(stage>15||room>255||variant>15)throw std::runtime_error("invalid room sound key");
    stop(4);stop(5);for(unsigned slot=14;slot<audio_voice_count;++slot)stop(slot);for(unsigned slot=8;slot<13;++slot)stop(slot);room_banks_.clear();room_sounds_.clear();
    const auto index=read_asset_file(data_/"files/MULTSPQ1.IDX",4096);BinaryView ids(index);const auto key=stage*1000+room*10+variant;
    unsigned pack=0;bool found=false;for(;pack<index.size()/2;++pack){const auto id=ids.u16(pack*2);if(id==0xffff)break;if(id==key){found=true;break;}}
    if(!found)return;
    auto file=mods_/"audio/MULTSPQ1"/(asset_number(pack)+".bin");if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"archives/MULTSPQ1"/(asset_number(pack)+".bin");
    const auto bytes=read_asset_file(file,32*1024*1024);BinaryView v(bytes);bool terminated=false;
    for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset){terminated=true;break;}const auto size=v.u32(p+4),type=v.u32(p+8),bank=v.u32(p+12);v.require(offset,size);
        // Type 2 is the original SDE shot bank (HD/BD wrapper); type 0/1
        // MIDI sequence/program and type 5 environment records remain separate.
        if(type==2){if(bank>15)throw std::runtime_error("invalid room sound bank");room_banks_[bank]={bytes.begin()+offset,bytes.begin()+offset+size};}}
    if(!terminated)throw std::runtime_error("unterminated room sound pack");room_pack_=pack;
}
bool AudioMixer::play_room_sound(unsigned bank,unsigned program,unsigned slot){
    if(slot<3||slot>=slots_.size())throw std::runtime_error("invalid room effect channel");
    const auto source=room_banks_.find(bank);if(source==room_banks_.end())return false;
    auto found=room_sounds_.find({bank,program});if(found==room_sounds_.end())found=room_sounds_.emplace(std::make_pair(bank,program),std::make_shared<PcmClip>(decode_system_sound(source->second,program))).first;
    play_pcm(slot,found->second);played.push_back("MULTSPQ1/"+asset_number(room_pack_)+"/"+std::to_string(bank)+"/"+asset_number(program));return true;
}
bool AudioMixer::footstep(unsigned floor,unsigned type,unsigned actor,bool enabled){
    if(actor>=foot_switch_.size()||type>3||floor>4)return false;
    static constexpr unsigned pairs[3][2]={{4,5},{14,15},{16,17}};
    unsigned slot=6;if(type<2){foot_switch_[actor]=!foot_switch_[actor];slot=pairs[actor][foot_switch_[actor]?0:1];}
    if(!enabled){stop(slot);return true;}
    return play_room_sound(2,type==2?5:floor,slot);
}
void AudioMixer::load_door_sounds(unsigned bank){
    for(unsigned slot=8;slot<=10;++slot)stop(slot);door_source_.clear();door_sounds_.clear();door_bank_=bank;door_slot_=8;
    if(data_.empty())return;const auto bytes=read_asset_file(data_/"files/MULTSPQ1.IDX",4096);BinaryView ids(bytes);unsigned pack=0;
    for(;pack<bytes.size()/2;++pack){if(ids.u16(pack*2)==(0x8000|bank))break;if(ids.u16(pack*2)==0xffff)return;}if(pack==bytes.size()/2)return;
    auto file=mods_/"audio/MULTSPQ1"/(asset_number(pack)+".bin");if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"archives/MULTSPQ1"/(asset_number(pack)+".bin");const auto data=read_asset_file(file,32*1024*1024);BinaryView v(data);
    for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset)break;const auto size=v.u32(p+4);v.require(offset,size);if(v.u32(p+8)==1&&v.u32(p+12)==1){door_source_={data.begin()+offset,data.begin()+offset+size};break;}}
}
bool AudioMixer::play_door_sound(unsigned program){
    if(door_source_.empty())return false;
    // CallDoorSe -> sdMidiPlay has an 8-bit data_num. Door packs supply a
    // Sony program bank; use its original program/waveform at this trigger.
    program&=255;BinaryView v(door_source_);const auto hd=v.u32(0),head=hd+v.u32(hd+8),programs=hd+v.u32(head+0x14);
    if(program>v.u32(programs+12)){unavailable.push_back("DOOR/"+asset_number(door_bank_)+"/"+asset_number(program));return false;}
    auto found=door_sounds_.find(program);if(found==door_sounds_.end())found=door_sounds_.emplace(program,std::make_shared<PcmClip>(decode_system_sound(door_source_,program))).first;
    play_pcm(door_slot_,found->second);door_slot_=door_slot_==10?8:door_slot_+1;played.push_back("DOOR/"+asset_number(door_bank_)+"/"+asset_number(program));return true;
}
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
    if(fade_ticks>600*game_tick_hz)throw std::runtime_error("audio fade too long");
    const auto relative=std::filesystem::path("audio")/archive/(asset_number(id)+".adx");auto file=mods_/relative;
    if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"archives"/archive/(asset_number(id)+".bin");
    auto clip=std::make_shared<PcmClip>(decode_adx(read_asset_file(file,32*1024*1024)));play_pcm(slot,clip,volume,loop);
    if(fade_ticks){slots_[slot].gain=0;slots_[slot].fade=fade_ticks*audio_frames_per_tick;}played.push_back(archive+"/"+asset_number(id));
}
void AudioMixer::stop(unsigned slot,unsigned fade_ticks){if(slot>=slots_.size()||fade_ticks>600*game_tick_hz)throw std::runtime_error("invalid audio slot");auto& s=slots_[slot];if(fade_ticks){s.target=0;s.fade=fade_ticks*audio_frames_per_tick;}else s.clip.reset();}
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
namespace recvx {
float AudioMixer::adx_gain(float volume){const int index=std::clamp(int(-volume),0,127);return std::pow(10.f,AdxVolTbl[index]/200.f);}
unsigned AudioMixer::adx_fade_ticks(int timer){const int count=((timer/100)*30+((timer%100)*6)/10)/2;return unsigned(std::max(count,1));}
void AudioMixer::fade_gain(unsigned slot,float gain,unsigned ticks){auto& s=slots_.at(slot);if(!s.clip)return;const float target=s.volume>0?gain/s.volume:0;if(!ticks){s.gain=s.target=target;s.fade=0;return;}s.target=target;s.fade=ticks*audio_frames_per_tick;}
void AudioMixer::play_bgm_ex(int bgm,int fade_in_rate,int volume){
    // PlayBgmEx2: SetVolumeAdxEx(0,-127,Volume) lifts the limit by 20 below -20.
    const float limit=volume<-20?float(volume+20):0.f;const unsigned ticks=fade_in_rate+1?adx_fade_ticks(fade_in_rate+1):0;
    play(0,"BGM1",unsigned(bgm),adx_gain(limit),true,ticks);current_bgm_=bgm;current_bgm_volume_=volume;
}
void AudioMixer::request_bgm2(int bgm,int volume){if(current_bgm_!=bgm){current_bgm_=bgm;req_fade_bgm_=8;}else if(current_bgm_volume_!=volume)req_fade_bgm_=2;current_bgm_volume_=volume;}
void AudioMixer::stop_bgm(int fade_out_rate){stop(0,fade_out_rate?adx_fade_ticks(fade_out_rate):0);current_bgm_=-1;current_bgm_volume_=-127;}
void AudioMixer::stop_bgm2(){req_fade_bgm_=1;current_bgm_=-1;current_bgm_volume_=-127;}
// RequestSoundFade(2,0xA3|0x40,400): a pending BGM change fades the current track out.
void AudioMixer::room_fade_out(){if(req_fade_bgm_&&active(0))stop(0,adx_fade_ticks(400));req_fade_bgm_=0;}
void AudioMixer::room_fade_in(){
    if((req_fade_bgm_&8)&&current_bgm_>=0)play_bgm_ex(current_bgm_,120,current_bgm_volume_);
    else if(req_fade_bgm_&2)fade_gain(0,adx_gain(float(current_bgm_volume_)),42); // RequestAdxFadeFunctionEx(0,-1,vol,42)
    req_fade_bgm_=0;
}
}
