#include "audio.h"
#include "asset_io.h"
#include "binary_view.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace recvx {
namespace {
#include "adx_tables.h"
// Host balance for the original 0..127 pan (64 = centre): the far side falls
// linearly to silence. modhsyn's exact SPU pan curve is in the closed IOP module.
void pan_gains(unsigned pan,float& left,float& right){left=pan<=64?1.f:float(127-std::min(pan,127u))/63.f;right=pan>=64?1.f:float(pan)/64.f;}
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
PcmClip decode_system_sound(const std::vector<std::uint8_t>& bytes,unsigned program,int key){
    BinaryView v(bytes);v.require(0,24);const auto hd=v.u32(0),hd_size=v.u32(4),bd=v.u32(8),bd_size=v.u32(12);v.require(hd,hd_size);v.require(bd,bd_size);
    auto magic=[&](std::size_t p,const char* expected){v.require(p,8);if(std::memcmp(bytes.data()+p,expected,8))throw std::runtime_error("invalid Sony sound-bank section");};
    magic(hd,"IECSsreV");const auto head=hd+v.u32(hd+8);magic(head,"IECSdaeH");
    auto section=[&](unsigned field,const char* name){const auto p=hd+v.u32(head+field);magic(p,name);const auto length=v.u32(p+8),last=v.u32(p+12);if(last>4096)throw std::runtime_error("Sony sound-bank table too large");v.require(p,length);v.require(p+16,(last+1)*4);return p;};
    const auto prog=section(0x14,"IECSgorP"),sets=section(0x18,"IECStesS"),samples=section(0x1c,"IECSlpmS"),vags=section(0x20,"IECSigaV");
    auto entry=[&](std::size_t table,unsigned index,std::size_t size){if(index>v.u32(table+12))throw std::runtime_error("Sony sound-bank index outside table");const auto offset=v.u32(table+16+index*4),length=v.u32(table+8);if(offset>length||size>length-offset)throw std::runtime_error("Sony sound-bank entry outside section");return v.relative(table,offset,size);};
    // Program: +0 split offset, +4 split count, +5 split size, +6 volume, +7 pan.
    // Split: +0 sample set, +2..+4 key low/centre/high, +0x10 volume, +0x11 pan.
    const auto pg=entry(prog,program,0x28);const unsigned splits=v.u8(pg+4),split_size=v.u8(pg+5);
    // A program without splits keys no waveform, so its note-on starts no voice (RM_9340 bank 3
    // programs 60..67: the en01 voice variant add_seno 40 of the mdlver-38 zombie).
    if(!splits)return {};
    if(split_size<0x12)throw std::runtime_error("invalid Sony program split table");
    auto zone=v.relative(pg,v.u32(pg),std::size_t(splits)*split_size);
    if(key>=0){unsigned i=0;for(;i<splits;++i){const auto z=zone+std::size_t(i)*split_size;if(key>=v.u8(z+2)&&key<=v.u8(z+4)){zone=z;break;}}if(i==splits)throw std::runtime_error("Sony program has no split for key");}
    const auto set=entry(sets,v.u16(zone),6);
    if(v.u8(set+3)!=1)throw std::runtime_error("layered system sound unsupported");const auto sample=entry(samples,v.u16(set+4),2);const auto vag=entry(vags,v.u16(sample),8);
    const auto start=v.u32(vag);std::size_t end=bd_size;for(unsigned i=0;i<=v.u32(vags+12);++i){const auto next=v.u32(entry(vags,i,8));if(next>start)end=std::min(end,std::size_t(next));}
    if(start>=end||end>bd_size)throw std::runtime_error("invalid Sony sound-bank waveform range");
    auto out=decode_ps_adpcm(std::vector<std::uint8_t>(bytes.begin()+bd+start,bytes.begin()+bd+end),v.u16(vag+4));
    out.volume=float(v.u8(pg+6))/127.f*float(v.u8(zone+0x10))/127.f;out.pan=unsigned(std::clamp(int(v.u8(pg+7))+int(v.u8(zone+0x11))-64,0,127));
    return out;
}
void AudioMixer::play_system(unsigned program){
    if(data_.empty())return; // Resource-free engine fixtures have no system bank.
    auto found=system_sounds_.find(program);if(found==system_sounds_.end()){auto file=mods_/"audio/system/COMMON.MLT";if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"files/COMMON.MLT";
        found=system_sounds_.emplace(program,std::make_shared<PcmClip>(decode_system_sound(read_asset_file(file,8*1024*1024),program))).first;}
    if(found->second->frames())play_pcm(effect_slot_,found->second);effect_slot_=effect_slot_==2?3:2;played.push_back("COMMON/"+asset_number(program));
}
bool AudioMixer::play_weapon_sound(unsigned bank,unsigned program){
    if(data_.empty())return false;
    auto source=weapon_banks_.find(bank);if(source==weapon_banks_.end()){
        const auto index=read_asset_file(data_/("files/MULTSPQ"+disc_digit()+".IDX"),4096);BinaryView ids(index);unsigned pack=0;bool found=false;
        for(;pack<index.size()/2;++pack){const auto key=ids.u16(pack*2);if(key==0xffff)break;if(key==(0x4000|bank)){found=true;break;}}if(!found)return false;
        auto file=mods_/("audio/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/("archives/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");const auto bytes=read_asset_file(file,32*1024*1024);BinaryView v(bytes);
        for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset)break;const auto size=v.u32(p+4);v.require(offset,size);if(v.u32(p+8)==2&&v.u32(p+12)==1){source=weapon_banks_.emplace(bank,std::vector<std::uint8_t>(bytes.begin()+offset,bytes.begin()+offset+size)).first;break;}}
        if(source==weapon_banks_.end())return false;
    }
    auto found=weapon_sounds_.find({bank,program});if(found==weapon_sounds_.end())found=weapon_sounds_.emplace(std::make_pair(bank,program),std::make_shared<PcmClip>(decode_system_sound(source->second,program))).first;
    if(found->second->frames())play_pcm(6,found->second);played.push_back("ARMS/"+asset_number(bank)+"/"+asset_number(program));return true;
}
void AudioMixer::load_room_sounds(unsigned stage,unsigned room,unsigned variant){
    if(stage>15||room>255||variant>15)throw std::runtime_error("invalid room sound key");
    stop(4);stop(5);for(unsigned slot=14;slot<24;++slot)stop(slot);for(unsigned slot=8;slot<13;++slot)stop(slot); /* CallDoorSe (MIDI slot 2) survives the room load */
    const auto index=read_asset_file(data_/("files/MULTSPQ"+disc_digit()+".IDX"),4096);BinaryView ids(index);const auto key=stage*1000+room*10+variant;
    unsigned pack=0;bool found=false;for(;pack<index.size()/2;++pack){const auto id=ids.u16(pack*2);if(id==0xffff)break;if(id==key){found=true;break;}}
    if(!found)return;
    auto file=mods_/("audio/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/("archives/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");
    const auto bytes=read_asset_file(file,32*1024*1024);BinaryView v(bytes);bool terminated=false;
    for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset){terminated=true;break;}const auto size=v.u32(p+4),type=v.u32(p+8),bank=v.u32(p+12);v.require(offset,size);
        // Type 2 is the original SDE shot bank (HD/BD wrapper); type 0/1
        // MIDI sequence/program and type 5 environment records remain separate.
        if(bank>15&&type<=2)throw std::runtime_error("invalid room sound bank");
        // LoadSoundPackFile/SetSoundData replace only the banks this pack carries; others keep their
        // previous data. CheckSpecialBank keeps the bank-3 BG sequence/program (rain) unless a BG
        // change is pending or no BG plays; then StopThePsgSound stops MIDI slots 0/1 first.
        if(type>2)continue;
        if(type<=1&&bank==3){if(!(request_background_[0]&2)&&!(request_background_[1]&2)&&!(current_background_[0]==-1&&current_background_[1]==-1))continue;
            for(unsigned slot=0;slot<2;++slot)if(active(24+slot)){stop(24+slot);current_background_[slot]=-1;}}
        const unsigned cached=type==2?bank:type==1?0x100u|bank:~0u;
        for(auto it=room_sounds_.begin();it!=room_sounds_.end();)it=std::get<0>(it->first)==cached?room_sounds_.erase(it):std::next(it);
        std::vector<std::uint8_t> section(bytes.begin()+offset,bytes.begin()+offset+size);
        if(type==2){room_banks_[bank]=std::move(section);sound_bank_packs_[bank]=pack;}else if(type==1)midi_programs_[bank]=std::move(section);else midi_sequences_[bank]=std::move(section);}
    if(!terminated)throw std::runtime_error("unterminated room sound pack");room_pack_=pack;
}
bool AudioMixer::load_player_sounds(unsigned player){
    if(player>3)throw std::runtime_error("invalid player voice bank");
    // Replacing the character must not retain a voice or a decoded sample from the old CORE pack.
    stop(7);room_banks_.erase(4);sound_bank_packs_.erase(4);
    for(auto it=room_sounds_.begin();it!=room_sounds_.end();)it=std::get<0>(it->first)==4?room_sounds_.erase(it):std::next(it);
    if(data_.empty())return false;
    const auto index=read_asset_file(data_/("files/MULTSPQ"+disc_digit()+".IDX"),4096);BinaryView ids(index);
    unsigned pack=0;bool found=false;
    for(;pack<index.size()/2;++pack){const auto key=ids.u16(pack*2);if(key==0xffff)break;if(key==(0xfff0u|player)){found=true;break;}}
    if(!found){unavailable.push_back("CORE/"+asset_number(player)+": pack missing");return false;}
    auto file=mods_/("audio/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");
    if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/("archives/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");
    const auto bytes=read_asset_file(file,32*1024*1024);BinaryView v(bytes);bool loaded=false,terminated=false;
    for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset){terminated=true;break;}
        const auto size=v.u32(p+4);v.require(offset,size);
        if(v.u32(p+8)==2&&v.u32(p+12)==4){room_banks_[4]=std::vector<std::uint8_t>(bytes.begin()+offset,bytes.begin()+offset+size);sound_bank_packs_[4]=pack;loaded=true;}}
    if(!terminated)throw std::runtime_error("unterminated player sound pack");
    if(!loaded)unavailable.push_back("CORE/"+asset_number(player)+": SE bank 4 missing");
    return loaded;
}
bool AudioMixer::play_room_sound(unsigned bank,unsigned program,unsigned slot,bool loop){
    if(slot<3||slot>=slots_.size())throw std::runtime_error("invalid room effect channel");
    const auto source=room_banks_.find(bank);if(source==room_banks_.end())return false;
    const auto id=std::make_tuple(bank,program,-1);auto found=room_sounds_.find(id);if(found==room_sounds_.end())found=room_sounds_.emplace(id,std::make_shared<PcmClip>(decode_system_sound(source->second,program))).first;
    if(found->second->frames())play_pcm(slot,found->second,1,loop);played.push_back("MULTSPQ"+disc_digit()+"/"+asset_number(sound_bank_packs_.at(bank))+"/"+std::to_string(bank)+"/"+asset_number(program));return true;
}
bool AudioMixer::background_sound(unsigned slot,unsigned sound,unsigned timer){
    if(slot>2)throw std::runtime_error("background sound slot outside original three");
    if(room_initializing_){background_init_[slot]={true,sound,timer};return true;}
    if(!play_room_sequence(3,sound&255,24+slot,true))return false;
    current_background_[slot]=int((sound&0xffff00ffu)|0x300u); // CallBackGroundSeEx: SeNo = (SeNo & ~0xff00) | 0x300
    if(timer){auto& c=slots_[24+slot];c.gain=0;c.target=1;c.fade=adx_fade_ticks(int(timer))*audio_frames_per_tick;}
    return true;
}
void AudioMixer::request_background_sound(unsigned slot,int sound){
    if(slot>2)throw std::runtime_error("background sound request outside original slots");
    if(sound<0){request_background_[slot]=1;current_background_[slot]=-1;}
    else if(current_background_[slot]!=sound){request_background_[slot]|=2;current_background_[slot]=sound;}
}
bool AudioMixer::play_room_sequence(unsigned bank,unsigned list,unsigned slot,bool loop){
    const auto sq=midi_sequences_.find(bank);const auto programs=midi_programs_.find(bank);
    auto fail=[&](const char* why){sequence_unavailable.push_back("MULTSPQ"+disc_digit()+"/"+asset_number(room_pack_)+"/"+std::to_string(bank)+"/seq"+std::to_string(list)+": "+why);return false;};
    if(sq==midi_sequences_.end()||programs==midi_programs_.end())return fail("bank missing");
    const auto& s=sq->second;auto u32=[&](std::size_t at)->std::uint32_t{if(at+4>s.size())throw std::runtime_error("SQ truncated");return std::uint32_t(s[at])|s[at+1]<<8|s[at+2]<<16|std::uint32_t(s[at+3])<<24;};
    try{
        // IECSsreV, IECSuqeS (+20: Midi chunk offset from section start), IECSidiM (+12: last index, +16: offsets).
        if(s.size()<64||std::memcmp(s.data()+16,"IECSuqeS",8))return fail("no Sequ chunk");
        const std::size_t midi=u32(16+20);if(midi+16>s.size()||std::memcmp(s.data()+midi,"IECSidiM",8))return fail("no Midi chunk");
        if(list>u32(midi+12))return fail("sequence outside table");const auto offset=u32(midi+16+list*4);if(offset==0xffffffff)return fail("empty sequence");
        // SCEI Midi data block: event offset, PPQN; compression headers are
        // separate. Note-off has one data byte (unlike Standard MIDI).
        const auto block=midi+offset;const auto event_offset=u32(block);
        if(event_offset!=6)return fail("compressed SQ not bound");
        const unsigned ppqn=unsigned(s.at(block+4))|unsigned(s.at(block+5))<<8;
        if(!ppqn)return fail("zero PPQN");
        struct Part {int program=-1;unsigned volume=127,expression=127,pan=64;};std::array<Part,16> parts{};
        auto sequence=std::make_shared<Sequence>();
        std::size_t p=block+event_offset;unsigned char status=0;bool delta=true,ended=false;
        double tempo=500000;std::string programs_played;
        auto vlq=[&](){unsigned value=0;for(int i=0;i<4;++i){const auto c=s.at(p++);value=(value<<7)|(c&127);if(!(c&128))return value;}throw std::runtime_error("SQ VLQ too long");};
        for(unsigned guard=0;guard<100000&&p<s.size();++guard){
            SequenceEvent event;event.delay=delta?double(vlq())*tempo*48000/(ppqn*1000000.0):0;
            delta=true;const auto c=s.at(p);unsigned char current=status;
            if(c&0x80){current=c;++p;if(current!=0xff)status=current;}
            if(current==0xff){const auto type=s.at(p++);const auto length=vlq();
                if(length>s.size()-p)throw std::runtime_error("SQ meta outside data");
                if(type==0x51){if(length!=3)return fail("invalid tempo");tempo=unsigned(s[p])<<16|unsigned(s[p+1])<<8|s[p+2];if(!tempo)return fail("zero tempo");}
                p+=length;event.status=current;event.key=type;sequence->events.push_back(event);
                if(type==0x2f){ended=true;break;}continue;}
            if(current<0x80||current>=0xf0)return fail("unsupported SQ event");
            event.status=current;
            const unsigned size=(current&0xf0)==0x80||(current&0xf0)==0xc0||(current&0xf0)==0xd0?1:2;
            unsigned char data[2]{};for(unsigned k=0;k<size;++k)data[k]=s.at(p++);
            if(data[size-1]&0x80){delta=false;data[size-1]&=0x7f;}
            event.key=data[0];event.value=data[1];auto& part=parts[current&15];
            switch(current&0xf0){case 0xc0:part.program=data[0];break;
                case 0xb0:if(data[0]==7)part.volume=data[1];else if(data[0]==11)part.expression=data[1];else if(data[0]==10)part.pan=data[1];break;
                case 0x90:if(data[1]){
                    if(part.program<0)return fail("note without program");
                    const auto id=std::make_tuple(0x100u|bank,unsigned(part.program),int(data[0]));auto found=room_sounds_.find(id);
                    if(found==room_sounds_.end())found=room_sounds_.emplace(id,std::make_shared<PcmClip>(decode_system_sound(programs->second,unsigned(part.program),int(data[0])))).first;
                    if(found->second->frames()){auto& layer=event.layer;layer.clip=found->second;pan_gains(unsigned(std::clamp(int(layer.clip->pan)+int(part.pan)-64,0,127)),layer.left,layer.right);
                    const float gain=layer.clip->volume*float(data[1])/127.f*float(part.volume)/127.f*float(part.expression)/127.f;layer.left*=gain;layer.right*=gain;}
                    if(programs_played.find("/prog"+std::to_string(part.program))==std::string::npos)programs_played+="/prog"+std::to_string(part.program);
                }break;
                default:break;}
            sequence->events.push_back(std::move(event));}
        if(!ended)return fail("SQ has no end");
        if(programs_played.empty())return fail("no program/note");
        Channel channel;channel.sequence=std::move(sequence);channel.sequence->until=channel.sequence->events.front().delay;channel.loop=loop;
        slots_.at(slot)=std::move(channel);
        played.push_back("MULTSPQ"+disc_digit()+"/"+asset_number(room_pack_)+"/"+std::to_string(bank)+"/seq"+std::to_string(list)+programs_played);return true;
    }catch(const std::exception& e){return fail(e.what());}
}
void AudioMixer::custom_midi_slots(unsigned objects,unsigned events){
    if(objects>3||objects+events>5){objects=1;events=4;}
    max_object_slots_=objects;max_event_slots_=events;
}
bool AudioMixer::play_system_midi(unsigned se){
    // StopFadeMidi(2) then PlayMidi(2, bank, (char)list); host slot 13 stands for MIDI slot 2.
    stop(13);
    const auto bank=(se>>8)&15,list=se&255;
    if(!play_room_sequence(bank,list,13)){unavailable.push_back("SYSTEM/MIDI/0x"+std::to_string(se));return false;}
    played.push_back("SYSTEM/MIDI/"+std::to_string(bank)+"/"+std::to_string(list));return true;
}
bool AudioMixer::event_sound(unsigned slot,unsigned sound){
    // Original CallNativeEventSe's upper-bound comparison includes SlotNo==MaxSlotEventSe.
    if(slot>max_event_slots_||slot>4)return true;
    return play_room_sequence(2,sound&255,12-slot);
}
bool AudioMixer::stop_event_sound(unsigned slot){if(slot<=max_event_slots_&&slot<=4)stop(12-slot);return true;}
void AudioMixer::sequence_step(Channel& channel){
    auto& s=*channel.sequence;
    for(unsigned guard=0;s.until<=0&&guard<4096;++guard){
        if(s.next>=s.events.size()){channel.sequence.reset();return;}
        const auto& event=s.events[s.next++];const unsigned part=event.status&15;
        if((event.status&0xf0)==0x90&&event.value){
            s.notes.erase(std::remove_if(s.notes.begin(),s.notes.end(),[&](const auto& note){return note.channel==part&&note.key==event.key;}),s.notes.end());
            s.notes.push_back({event.layer,part,event.key});
        }else if((event.status&0xf0)==0x80||((event.status&0xf0)==0x90&&!event.value)){
            s.notes.erase(std::remove_if(s.notes.begin(),s.notes.end(),[&](const auto& note){return note.channel==part&&note.key==event.key;}),s.notes.end());
        }else if((event.status&0xf0)==0xb0){
            // Sony SQ loop controls: NRPN 99=0/1, Data Entry 6=number,
            // Data Entry LSB 38=count (0=infinite). Original room resources
            // supply these markers; no room IDs or alarm-specific loop rule.
            if(event.key==99)s.nrpn=event.value;
            else if(event.key==6&&s.nrpn==0){s.loops.push_back({event.value,s.next,0,false});}
            else if(event.key==38&&s.nrpn==1&&!s.loops.empty()){
                auto& loop=s.loops.back();if(!loop.counted){loop.remaining=event.value;loop.counted=true;}
                if(!event.value||loop.remaining>1){if(event.value)--loop.remaining;s.next=loop.next;}else s.loops.pop_back();
            }
        }else if(event.status==0xff&&event.key==0x2f){channel.sequence.reset();return;}
        if(s.next<s.events.size())s.until+=s.events[s.next].delay;
    }
    if(s.until<=0)throw std::runtime_error("SQ zero-duration loop/event limit");
}
bool AudioMixer::user_sound_volume(unsigned type,unsigned slot,int start,int target,unsigned frames){
    std::vector<unsigned> voices;
    switch(type){
    case 1:{static constexpr unsigned pairs[3][2]={{4,5},{14,15},{16,17}};if(slot>=3)return false;voices={pairs[slot][0],pairs[slot][1]};break;}
    case 2:voices={6};break; // host weapon voice
    case 5:if(slot>=6)return false;voices={18+slot};break;
    case 6:if(slot>=3)return false;voices={24+slot};break;
    case 7:if(slot>=5)return false;voices={12-slot};break;
    case 10:voices={0};break;
    default:return false; // object/action/special voices need their own work
    }
    auto gain=[&](int volume){return type==10?adx_gain(float(volume)):std::clamp(float(volume+128)/127.f,0.f,1.f);};
    for(const auto voice:voices){auto& c=slots_[voice];if(!active(voice))continue;
        // Request*FadeFunctionEx -1 retains the current volume. Frame is an
        // original scheduler count, not a centisecond timer.
        if(type==10){if(start!=-1)c.gain=c.volume>0?gain(start)/c.volume:0;fade_gain(voice,gain(target),frames);}
        else{if(start!=-1)c.gain=gain(start);c.target=target==-127?0:gain(target);c.fade=frames*audio_frames_per_tick;if(!frames)c.gain=c.target;}
        if(!frames&&target==-127){c.clip.reset();c.layers.clear();c.sequence.reset();}
    }return true;
}
void AudioMixer::stop_background_sound(unsigned slot,unsigned fade_ticks){if(slot>2)throw std::runtime_error("background sound slot outside original three");stop(24+slot,fade_ticks);current_background_[slot]=-1;}
bool AudioMixer::enemy_sound(unsigned slot,unsigned sound){
    if(slot>=6)throw std::runtime_error("enemy sound slot outside original six voices");
    // sdfunc.c CallEnemySe: force bank 3, preserve the low 8-bit list number.
    return play_room_sound(3,sound&255,18+slot);
}
void AudioMixer::request_enemy_se(unsigned enemy,int se_no,int fade_rate){
    auto& r=enemy_requests_[enemy];r.fade=fade_rate;const int prio=(se_no&0xF0000)>>16;if(r.prio>prio)r.prio=prio;
    if(!(se_no&0xF000000)){r.se=se_no;r.req=true;}else{r.se_v=se_no;r.req_v=true;}
}
bool AudioMixer::enemy_se_playing(unsigned enemy,int se_no)const{
    for(const auto& v:enemy_voices_)if(v.flag&&v.enemy==enemy){if(se_no>0)return true;if(v.se==se_no)return true;}return false;
}
void AudioMixer::play_player_voice(int se_no){
    try{if(!play_room_sound((unsigned(se_no)>>8)&0xF,unsigned(se_no)&255,7))unavailable.push_back("player voice "+std::to_string(se_no));}
    catch(const std::exception& error){unavailable.push_back("player voice "+std::to_string(se_no)+": "+error.what());}
}
void AudioMixer::exec_enemy_se(){
    // Free voices whose sound ended, then serve the requests by priority (lower first), as ExecEnemySeManager does.
    for(unsigned i=0;i<6;++i)if(enemy_voices_[i].flag&&!active(18+i))enemy_voices_[i]={};
    std::vector<unsigned> order;for(const auto& pair:enemy_requests_)if(pair.second.req||pair.second.req_v)order.push_back(pair.first);
    std::stable_sort(order.begin(),order.end(),[&](unsigned a,unsigned b){return enemy_requests_[a].prio<enemy_requests_[b].prio;});
    auto serve=[&](unsigned enemy,int se,int attrib){
        int same=0;for(const auto& v:enemy_voices_)if(v.flag&&v.se==se&&v.enemy!=enemy)++same;
        if(same&&same>=((se>>12)&0xF))return; // CheckPlaySameSe
        int slot=-1;for(unsigned i=0;i<6;++i)if(enemy_voices_[i].flag&&enemy_voices_[i].enemy==enemy&&enemy_voices_[i].attrib==attrib){slot=int(i);break;}
        if(slot<0)for(unsigned i=0;i<6;++i)if(!enemy_voices_[i].flag){slot=int(i);break;}
        if(slot<0)return;
        enemy_voices_[slot]={true,enemy,se,(se&0xF000000)>>24,(se&0xF0000)>>16};
        stop(18+slot);
        // CallEnemySeMain: bank (SeNo>>8)&0xF, list SeNo. A program the host cannot decode is reported, never fatal.
        try{if(!play_room_sound((unsigned(se)>>8)&0xF,unsigned(se)&255,18+unsigned(slot)))unavailable.push_back("enemy SE "+std::to_string(se));}
        catch(const std::exception& error){unavailable.push_back("enemy SE "+std::to_string(se)+": "+error.what());}
    };
    for(const auto enemy:order){auto& r=enemy_requests_[enemy];if(r.req){serve(enemy,r.se,0);r.req=false;}if(r.req_v){serve(enemy,r.se_v,1);r.req_v=false;}r.prio=3;}
}
void AudioMixer::stop_enemy_sound(unsigned slot){if(slot>=6)throw std::runtime_error("enemy sound slot outside original six voices");stop(18+slot);}
bool AudioMixer::footstep(unsigned floor,unsigned type,unsigned actor,bool enabled){
    if(actor>=foot_switch_.size()||type>3||floor>4)return false;
    static constexpr unsigned pairs[3][2]={{4,5},{14,15},{16,17}};
    unsigned slot=6;if(type<2){foot_switch_[actor]=!foot_switch_[actor];slot=pairs[actor][foot_switch_[actor]?0:1];}
    if(!enabled){stop(slot);return true;}
    return play_room_sound(2,type==2?5:floor,slot);
}
void AudioMixer::load_door_sounds(unsigned bank){
    for(unsigned slot=door_voice_first;slot<=door_voice_last;++slot)stop(slot);door_source_.clear();
    for(auto it=room_sounds_.begin();it!=room_sounds_.end();)it=std::get<0>(it->first)==0x101u?room_sounds_.erase(it):std::next(it);door_bank_=bank;door_slot_=door_voice_first;
    if(data_.empty())return;const auto bytes=read_asset_file(data_/("files/MULTSPQ"+disc_digit()+".IDX"),4096);BinaryView ids(bytes);unsigned pack=0;
    for(;pack<bytes.size()/2;++pack){if(ids.u16(pack*2)==(0x8000|bank))break;if(ids.u16(pack*2)==0xffff)return;}if(pack==bytes.size()/2)return;
    auto file=mods_/("audio/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/("archives/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");const auto data=read_asset_file(file,32*1024*1024);BinaryView v(data);
    for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset)break;const auto size=v.u32(p+4);v.require(offset,size);if(v.u32(p+8)==1&&v.u32(p+12)==1){door_source_={data.begin()+offset,data.begin()+offset+size};break;}}
    // InitSoundDriver downloads COMMON.MLT once. Its third block is MIDI
    // sequence bank 1 (four door lists), retained when DOOR SPQ replaces only
    // program bank 1. Resolve velocity, key and program through that SQ.
    auto common=mods_/"audio/system/COMMON.MLT";if(mods_.empty()||!std::filesystem::is_regular_file(common))common=data_/"files/COMMON.MLT";
    const auto combined=read_asset_file(common,8*1024*1024);BinaryView cv(combined);const auto seq=cv.u32(16),length=cv.u32(20);cv.require(seq,length);
    midi_sequences_[1]={combined.begin()+seq,combined.begin()+seq+length};midi_programs_[1]=door_source_;

}
bool AudioMixer::play_door_sound(unsigned program){
    if(door_source_.empty())return false;
    program&=255;
    if(!play_room_sequence(1,program,door_slot_)){unavailable.push_back("DOOR/"+asset_number(door_bank_)+"/"+asset_number(program));return false;}
    door_slot_=door_slot_==door_voice_last?door_voice_first:door_slot_+1;played.push_back("DOOR/"+asset_number(door_bank_)+"/"+asset_number(program));return true;
}
void AudioMixer::finish_door_sound(){for(unsigned slot=door_voice_first;slot<=door_voice_last;++slot)stop(slot,adx_fade_ticks(250));}
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
    // ADX v3 embedded loop points, as consumed by ADXT_StartAfs.
    if(start>=50&&be32(24)){out.loop_start=be32(28);out.loop_end=be32(36);if(out.loop_start>=out.loop_end||out.loop_end>frames)throw std::runtime_error("invalid ADX loop points");}
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
void AudioMixer::play_pcm(unsigned slot,std::shared_ptr<PcmClip> clip,float volume,bool loop){if(slot>=slots_.size()||!clip||!clip->channels||clip->channels>2||clip->rate<8000||clip->rate>96000||clip->samples.size()%clip->channels||!clip->frames()||!std::isfinite(volume)||volume<0||volume>8||(clip->loop_end&&(clip->loop_start>=clip->loop_end||clip->loop_end>clip->frames())))throw std::runtime_error("invalid audio channel/clip");Channel c;c.volume=volume*clip->volume;pan_gains(clip->pan,c.left,c.right);c.clip=std::move(clip);c.loop=loop;slots_[slot]=std::move(c);}
void AudioMixer::play_layers(unsigned slot,std::vector<Layer> layers,float volume,bool loop){
    if(layers.empty())throw std::runtime_error("audio voice without layers");
    play_pcm(slot,layers.front().clip,volume,loop);auto& c=slots_[slot];c.volume=volume;c.left=layers.front().left;c.right=layers.front().right;
    for(std::size_t i=1;i<layers.size();++i){const auto& l=layers[i].clip;if(!l||!l->channels||l->channels>2||l->rate<8000||l->rate>96000||!l->frames()||(l->loop_end&&(l->loop_start>=l->loop_end||l->loop_end>l->frames())))throw std::runtime_error("invalid audio layer");c.layers.push_back(layers[i]);}
}
void AudioMixer::play(unsigned slot,const std::string& archive,unsigned id,float volume,bool loop,unsigned fade_ticks){
    if(fade_ticks>600*game_tick_hz)throw std::runtime_error("audio fade too long");
    const auto relative=std::filesystem::path("audio")/archive/(asset_number(id)+".adx");auto file=mods_/relative;
    if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"archives"/archive/(asset_number(id)+".bin");
    auto clip=std::make_shared<PcmClip>(decode_adx(read_asset_file(file,32*1024*1024)));play_pcm(slot,clip,volume,loop&&clip->loop_end>clip->loop_start);
    if(fade_ticks){slots_[slot].gain=0;slots_[slot].fade=fade_ticks*audio_frames_per_tick;}played.push_back(archive+"/"+asset_number(id));
}
void AudioMixer::stop(unsigned slot,unsigned fade_ticks){if(slot>=slots_.size()||fade_ticks>600*game_tick_hz)throw std::runtime_error("invalid audio slot");auto& s=slots_[slot];if(fade_ticks){s.target=0;s.fade=fade_ticks*audio_frames_per_tick;}else{s.clip.reset();s.layers.clear();s.sequence.reset();}}
bool AudioMixer::active(unsigned slot)const{return slot<slots_.size()&&(bool(slots_[slot].clip)||bool(slots_[slot].sequence));}
std::vector<std::int16_t> AudioMixer::mix(unsigned frames){if(frames>48000*10)throw std::runtime_error("audio mix too large");std::vector<std::int16_t> out(std::size_t(frames)*2);
    for(unsigned f=0;f<frames;++f){float left=0,right=0;for(auto& channel:slots_){if(!channel.clip&&!channel.sequence)continue;
        if(channel.fade){channel.gain+=(channel.target-channel.gain)/channel.fade;if(!--channel.fade&&channel.target==0){channel.clip.reset();channel.layers.clear();channel.sequence.reset();continue;}}
        if(channel.sequence)sequence_step(channel);
        const float gain=channel.volume*channel.gain;
        auto voice=[&](std::shared_ptr<PcmClip>& held,double& position,float l,float r){auto& clip=*held;
            const auto end=channel.loop&&clip.loop_end?clip.loop_end:clip.frames();if(position>=end){if(channel.loop){const auto begin=clip.loop_end?clip.loop_start:0;position=begin+std::fmod(position-end,double(end-begin));}else{held.reset();return;}}
            const auto i=std::size_t(position),next=channel.loop&&i+1>=end?(clip.loop_end?clip.loop_start:0):std::min(i+1,clip.frames()-1);const float fraction=float(position-i);
            auto sample=[&](unsigned ch){const float a=clip.samples[i*clip.channels+ch],b=clip.samples[next*clip.channels+ch];return (a+(b-a)*fraction)*gain;};left+=sample(0)*l;right+=sample(clip.channels==1?0:1)*r;position+=double(clip.rate)/48000;};
        if(channel.sequence){for(auto& note:channel.sequence->notes)if(note.layer.clip)voice(note.layer.clip,note.layer.position,note.layer.left,note.layer.right);channel.sequence->until-=1;continue;}
        if(channel.clip)voice(channel.clip,channel.position,channel.left,channel.right);
        for(auto& layer:channel.layers)if(layer.clip)voice(layer.clip,layer.position,layer.left,layer.right);
        // A finished first note hands the slot to the next sounding layer.
        if(!channel.clip||!channel.layers.empty()){channel.layers.erase(std::remove_if(channel.layers.begin(),channel.layers.end(),[](const Layer& l){return !l.clip;}),channel.layers.end());if(!channel.clip&&!channel.layers.empty()){const auto next=channel.layers.front();channel.layers.erase(channel.layers.begin());channel.clip=next.clip;channel.position=next.position;channel.left=next.left;channel.right=next.right;}}
    }out[f*2]=std::int16_t(std::clamp(left,-32768.f,32767.f));out[f*2+1]=std::int16_t(std::clamp(right,-32768.f,32767.f));}return out;
}
}
namespace recvx {
float AudioMixer::adx_gain(float volume){const int index=std::clamp(int(-volume),0,127);return std::pow(10.f,AdxVolTbl[index]/200.f);}
unsigned AudioMixer::adx_fade_ticks(int timer){const int count=((timer/100)*30+((timer%100)*6)/10)/2;return unsigned(std::max(count,1));}
void AudioMixer::fade_gain(unsigned slot,float gain,unsigned ticks){auto& s=slots_.at(slot);if(!active(slot))return;const float target=s.volume>0?gain/s.volume:0;if(!ticks){s.gain=s.target=target;s.fade=0;return;}s.target=target;s.fade=ticks*audio_frames_per_tick;}
void AudioMixer::play_bgm_ex(int bgm,int fade_in_rate,int volume){
    // PlayBgmEx2: SetVolumeAdxEx(0,-127,Volume) lifts the limit by 20 below -20.
    const float limit=volume<-20?float(volume+20):0.f;const unsigned ticks=fade_in_rate+1?adx_fade_ticks(fade_in_rate+1):0;
    play(0,set_name("BGM"),unsigned(bgm),adx_gain(limit),true,ticks);current_bgm_=bgm;current_bgm_volume_=volume;
}
void AudioMixer::request_bgm2(int bgm,int volume){if(current_bgm_!=bgm){current_bgm_=bgm;req_fade_bgm_=8;}else if(current_bgm_volume_!=volume)req_fade_bgm_=2;current_bgm_volume_=volume;}
void AudioMixer::stop_bgm(int fade_out_rate){stop(0,fade_out_rate?adx_fade_ticks(fade_out_rate):0);current_bgm_=-1;current_bgm_volume_=-127;}
void AudioMixer::stop_bgm2(){req_fade_bgm_=1;current_bgm_=-1;current_bgm_volume_=-127;}
// RequestSoundFade(2,0xA3|0x40,400): a pending BGM change fades the current track out.
void AudioMixer::room_fade_out(){if(req_fade_bgm_&&active(0))stop(0,adx_fade_ticks(400));req_fade_bgm_=0;for(unsigned slot=0;slot<2;++slot)if(request_background_[slot]&1)stop(24+slot,adx_fade_ticks(400));}
void AudioMixer::room_fade_in(){
    if((req_fade_bgm_&8)&&current_bgm_>=0)play_bgm_ex(current_bgm_,120,current_bgm_volume_);
    else if(req_fade_bgm_&2)fade_gain(0,adx_gain(float(current_bgm_volume_)),42); // RequestAdxFadeFunctionEx(0,-1,vol,42)
    req_fade_bgm_=0;
    room_initializing_=false;
    for(unsigned slot=0;slot<2;++slot){
        if(request_background_[slot]&2)background_sound(slot,unsigned(current_background_[slot]),120);
        else if(background_init_[slot].active){const auto req=background_init_[slot];background_sound(slot,req.sound,req.timer);}
        request_background_[slot]=0;background_init_[slot].active=false;
    }
}
}
