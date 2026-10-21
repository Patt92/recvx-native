#include "audio.h"
#include "asset_io.h"
#include "binary_view.h"
#include "core_random.h"
#include "audio_sq.h"
#include "audio_pitch.h"
#include "audio_pan.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace recvx {
namespace {
#include "adx_tables.h"
// Host balance for the original 0..127 pan (64 = centre): the far side falls
// linearly to silence. modhsyn's exact SPU pan curve is in the closed IOP module.
void pan_gains(unsigned pan,float& left,float& right){const auto gain=sony_pan_factors(int(pan));left=float(gain.left)/128.f;right=float(gain.right)/128.f;}
// sdShotSetVol/SetVolumeMidi hand vol+128 to the driver; host scale of the closed IOP curve.
float se_volume_gain(float volume){return std::clamp((volume+128)/127.f,0.f,1.f);}
#include "sound_3d_tables.inc"
// GsSlotInfoSe index -> host voice of the binding that plays that SE slot.
unsigned se_host_voice(unsigned se_slot){
    static constexpr unsigned map[20]={18,19,20,21,22,23,6,7,weapon_voice_8,weapon_voice_9,10,5,4,15,14,17,16,2,3,weapon_voice_19};
    if(se_slot>=20)throw std::runtime_error("SE slot outside GsSlotInfoSe[20]");return map[se_slot];
}
unsigned midi_host_voice(unsigned midi){static constexpr unsigned map[8]={24,25,26,8,9,10,11,12};return map[midi];}
}
// Sony HD/BD metadata and SPU-ADPCM format references are listed in docs/AUDIO.md.
PcmClip decode_ps_adpcm(const std::vector<std::uint8_t>& bytes,unsigned rate){
    if(bytes.empty()||bytes.size()%16||bytes.size()>32*1024*1024||rate<8000||rate>96000)throw std::runtime_error("invalid SPU-ADPCM dimensions");
    static constexpr int coefficients[5][2]={{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
    PcmClip out;out.rate=rate;out.channels=1;int previous=0,older=0;bool loop_start=false;
    for(std::size_t p=0;p<bytes.size();p+=16){const unsigned filter=bytes[p]>>4,shift=bytes[p]&15,flags=bytes[p+1];
        if(filter>4||shift>12||flags>7)throw std::runtime_error("invalid SPU-ADPCM frame");
        // Block flags are addresses, not a request to repeat the whole waveform.
        // Bit 2 copies the current address (PSX-SPX "Loop Start"); a later
        // marked block replaces the earlier one, exactly like the SPU register.
        if(flags&4){out.loop_start=out.frames();loop_start=true;}
        for(unsigned i=0;i<28;++i){int nibble=(bytes[p+2+i/2]>>((i&1)*4))&15;if(nibble>=8)nibble-=16;
            const int residual=nibble*(1<<(12-shift));const int predictor=coefficients[filter][0]*previous+coefficients[filter][1]*older;
            const int sample=flags==7?0:std::clamp(residual+(predictor>=0?predictor/64:-((-predictor+63)/64)),-32768,32767);
            out.samples.push_back(std::int16_t(sample));older=previous;previous=sample;
        }if(flags&1){
            // Bit 0 ends, bit 1 repeats instead of releasing (PSX-SPX "Loop
            // End/Repeat"). Flag 7 mutes its own block and loops it silently.
            if(flags&2){if(!loop_start)throw std::runtime_error("SPU repeat without explicit loop start unsupported");out.loop_end=out.frames();}
            break;
        }
    }if(!out.loop_end)out.loop_start=0;return out;
}
namespace {
// One clip per voice the program starts: key<0 (TSNDDRV SE request) the first split, key>=0 (MODHSYN
// note-on) every split whose key range holds the key, in table order.
std::vector<PcmClip> decode_program_voices(const std::vector<std::uint8_t>& bytes,unsigned program,int key){
    BinaryView v(bytes);v.require(0,24);const auto hd=v.u32(0),hd_size=v.u32(4),bd=v.u32(8),bd_size=v.u32(12);v.require(hd,hd_size);v.require(bd,bd_size);
    auto magic=[&](std::size_t p,const char* expected){v.require(p,8);if(std::memcmp(bytes.data()+p,expected,8))throw std::runtime_error("invalid Sony sound-bank section");};
    magic(hd,"IECSsreV");const auto head=hd+v.u32(hd+8);magic(head,"IECSdaeH");
    auto section=[&](unsigned field,const char* name){const auto p=hd+v.u32(head+field);magic(p,name);const auto length=v.u32(p+8),last=v.u32(p+12);if(last>4096)throw std::runtime_error("Sony sound-bank table too large");v.require(p,length);v.require(p+16,(last+1)*4);return p;};
    const auto prog=section(0x14,"IECSgorP"),sets=section(0x18,"IECStesS"),samples=section(0x1c,"IECSlpmS"),vags=section(0x20,"IECSigaV");
    auto entry=[&](std::size_t table,unsigned index,std::size_t size){if(index>v.u32(table+12))throw std::runtime_error("Sony sound-bank index outside table");const auto offset=v.u32(table+16+index*4),length=v.u32(table+8);if(offset>length||size>length-offset)throw std::runtime_error("Sony sound-bank entry outside section");return v.relative(table,offset,size);};
    // Program: +0 split offset, +4 split count, +5 split size, +6 volume, +7 pan.
    // Split: +0 sample set, +2..+4 key low/centre/high, +0x10 volume, +0x11 pan.
    // MIDI note-on (key>=0) is MODHSYN's path. Its program change (MODHSYN.IRX .text 0x75c) keeps no
    // program when the number is above the table's last index (+12), the table entry is 0xffffffff or
    // the program has no split (+4 == 0); note-on (0x5f90) then returns at once without a voice
    // (`beq t0,zero,0x64c8` at 0x5ff0). The sequence itself runs on (MULTSPQ2/0086 bank 2 programs
    // 23/24/27..30 are such empty entries).
    if(key>=0&&(program>v.u32(prog+12)||v.u32(prog+16+std::size_t(program)*4)==0xffffffffu))return {};
    const auto pg=entry(prog,program,0x28);const unsigned splits=v.u8(pg+4),split_size=v.u8(pg+5);
    // A program without splits keys no waveform, so its note-on starts no voice (RM_9340 bank 3
    // programs 60..67: the en01 voice variant add_seno 40 of the mdlver-38 zombie).
    if(!splits)return {};
    if(split_size<0x14)throw std::runtime_error("invalid Sony program split table");
    if(v.u8(pg+12)&1)throw std::runtime_error("Sony phase-inverted program pan not ported");
    const auto zones=v.relative(pg,v.u32(pg),std::size_t(splits)*split_size);
    // MODHSYN note-on split loop (0x6098..0x64a0): split +2 and +4 are compared without bit 7
    // (`and v0,v0,-129`, bit 7 is the crossfade flag read at 0x4680). A key no split covers starts no
    // voice and leaves the sequence running: an original silent note, not an unavailable sound.
    // The loop does not stop at the first hit: every covering split runs its Sset sample loop
    // (0x615c..0x646c) and keys a voice per sample (0x6264 voice, 0x62f0.. setup, 0x63d0 key-on),
    // then steps 20 bytes to the next split (0x6474..0x64a0) until the count runs out.
    std::vector<PcmClip> voices;
    auto voice=[&](std::size_t zone){
    // Note-on skips a split whose Sset index is above the table's last (0x60f0..0x6108) or whose Sset has no
    // sample (0x614c..0x6154), and a sample whose Smpl index is above the last (0x6164..0x6178) or whose
    // Vagi entry is 0xffffffff (0x6200..0x6208); the next split/sample follows.
    if(key>=0&&v.u16(zone)>v.u32(sets+12))return;
    const auto set=entry(sets,v.u16(zone),6);
    if(key>=0&&(!v.u8(set+3)||v.u16(set+4)>v.u32(samples+12)))return;
    if(v.u8(set+3)!=1)throw std::runtime_error("layered system sound unsupported");const auto sample=entry(samples,v.u16(set+4),42);const auto vag=entry(vags,v.u16(sample),8);
    if(key>=0&&v.u32(vag)==0xffffffffu)return;
    // Note-on also gates Sset +1..+2 (0x6120/0x6138) and Smpl +2..+4 &0x7f (0x6190/0x61b0) by velocity; every
    // Sset/Smpl of every bank holds 1..127, so a note-on velocity (1..127) always passes. Others are not ported.
    if(key>=0&&(v.u8(set+1)!=1||v.u8(set+2)!=127||(v.u8(sample+2)&0x7f)!=1||(v.u8(sample+4)&0x7f)!=127))throw std::runtime_error("Sony Sset/Smpl velocity range not ported");
    const auto start=v.u32(vag);std::size_t end=bd_size;for(unsigned i=0;i<=v.u32(vags+12);++i){const auto next=v.u32(entry(vags,i,8));if(next>start)end=std::min(end,std::size_t(next));}
    if(start>=end||end>bd_size)throw std::runtime_error("invalid Sony sound-bank waveform range");
    auto out=decode_ps_adpcm(std::vector<std::uint8_t>(bytes.begin()+bd+start,bytes.begin()+bd+end),v.u16(vag+4));
    const auto attribute=v.u8(vag+6);
    if(attribute==0)out.loop_start=out.loop_end=0; // SCEHD_VAG_1SHOT overrides waveform markers.
    else if(attribute==1){if(!out.loop_end)throw std::runtime_error("Sony looping Vagi without explicit sounding loop bounds unsupported");}
    else throw std::runtime_error("unsupported Sony Vagi attribute");
    /* SdrSeReq/Tsnd_tqreq: the note is the program-header byte +0xB of the HD file (TSNDDRV 0x10D4), velocity 127. */
    const int note=key<0?int(v.u8(pg+11)):key;
    out.sony_pitch=SonyPitch{int(v.u8(sample+11)),note+int(std::int8_t(v.u8(pg+8)))+int(std::int8_t(v.u8(zone+18))),int(std::int8_t(v.u8(pg+9)))+int(std::int8_t(v.u8(sample+12)))+int(std::int8_t(v.u8(zone+19))),v.u16(zone+6),v.u16(zone+8)};
    // MODHSYN Key-On: ADSR1/ADSR2 (+0x12/+0x14) unveraendert, da alle Key-Follow-Tiefen 0 sind;
    // Mischschalter VMIXL/R/EL/ER und Kernwahl aus sampleSpuAttr (+0x29). Siehe audio_spu.h.
    for(unsigned depth:{0x16u,0x18u,0x1au,0x1cu,0x1eu})if(v.u8(sample+depth))throw std::runtime_error("Sony sample key-follow ADSR not ported");
    out.spu=true;out.adsr1=v.u16(sample+0x12);out.adsr2=v.u16(sample+0x14);out.spu_attr=v.u8(sample+0x29);
    // MODHSYN note volume (0x38c8): program +6 * split +0x10 * sample +0x10 * velocity curve, crossfade 0x4680.
    // Velocity curve (Sset +0, 0x8794), split/sample crossfade (bit 7 of +2/+4, 0x4680) and the amplitude LFO
    // (sample +0x28 & 0x70, 0x37cc) are 0 in every bank of the game; others are not ported.
    if(v.u8(set)!=0)throw std::runtime_error("Sony velocity curve not ported");
    if((v.u8(zone+2)|v.u8(zone+4)|v.u8(sample+2)|v.u8(sample+4))&0x80)throw std::runtime_error("Sony split/sample crossfade not ported");
    if(v.u8(sample+0x28)&0x70)throw std::runtime_error("Sony amplitude LFO not ported");
    out.sony_volume=std::uint32_t(v.u8(pg+6))*v.u8(zone+0x10)*v.u8(sample+0x10);
    out.volume=float(v.u8(pg+6))/127.f*float(v.u8(zone+0x10))/127.f;out.pan=unsigned(sony_bank_pan(std::int8_t(v.u8(pg+7)),std::int8_t(v.u8(zone+17)),std::int8_t(v.u8(sample+13)),((note-int(v.u8(zone+15)))*int(std::int8_t(v.u8(zone+14)))+(note-int(v.u8(pg+11)))*int(std::int8_t(v.u8(pg+10))))/12));
    voices.push_back(std::move(out));};
    if(key<0){voice(zones);return voices;}
    for(unsigned i=0;i<splits;++i){const auto z=zones+std::size_t(i)*split_size;if(key>=(v.u8(z+2)&0x7f)&&key<=(v.u8(z+4)&0x7f))voice(z);}
    return voices;
}
}
std::vector<PcmClip> decode_system_note(const std::vector<std::uint8_t>& bytes,unsigned program,int key){
    if(key<0||key>127)throw std::runtime_error("MIDI note-on key outside 0..127");
    return decode_program_voices(bytes,program,key);
}
PcmClip decode_system_sound(const std::vector<std::uint8_t>& bytes,unsigned program,int key){
    auto voices=decode_program_voices(bytes,program,key);
    if(voices.empty())return {};
    // A key several splits cover keys several voices (decode_system_note); one clip cannot stand for them.
    if(voices.size()>1)throw std::runtime_error("Sony note keys several splits; decode_system_note returns its voices");
    return std::move(voices.front());
}
void AudioMixer::play_system(unsigned program){
    if(data_.empty())return; // Resource-free engine fixtures have no system bank.
    // sdfunc.c:1344-1385 CallSystemSeBasic: bit 31 selects ExPlayMidi, bank = (SeNo >> 8) & 15,
    // list = low byte on MIDI slot 2. mcCallMessageTypeSe uses 0x80000242..244 (room MIDI bank 2),
    // not COMMON's single-sample SE table. Do not discard the request as an out-of-range SE program.
    if(program&0x80000000u){play_system_midi(program);return;}
    auto found=system_sounds_.find(program);if(found==system_sounds_.end()){auto file=mods_/"audio/system/COMMON.MLT";if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"files/COMMON.MLT";
        found=system_sounds_.emplace(program,std::make_shared<PcmClip>(decode_system_sound(read_asset_file(file,8*1024*1024),program))).first;}
    if(found->second->frames())play_pcm(effect_slot_,found->second);effect_slot_=effect_slot_==2?3:2;played.push_back("COMMON/"+asset_number(program));
}
std::shared_ptr<PcmClip> AudioMixer::weapon_clip(unsigned bank,unsigned program){
    if(data_.empty())return {};
    auto source=weapon_banks_.find(bank);if(source==weapon_banks_.end()){
        const auto index=read_asset_file(data_/("files/MULTSPQ"+disc_digit()+".IDX"),4096);BinaryView ids(index);unsigned pack=0;bool found=false;
        for(;pack<index.size()/2;++pack){const auto key=ids.u16(pack*2);if(key==0xffff)break;if(key==(0x4000|bank)){found=true;break;}}if(!found)return {};
        auto file=mods_/("audio/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/("archives/MULTSPQ"+disc_digit())/(asset_number(pack)+".bin");const auto bytes=read_asset_file(file,32*1024*1024);BinaryView v(bytes);
        for(unsigned i=0;i<16;++i){const auto p=i*16,offset=v.u32(p);if(!offset)break;const auto size=v.u32(p+4);v.require(offset,size);if(v.u32(p+8)==2&&v.u32(p+12)==1){source=weapon_banks_.emplace(bank,std::vector<std::uint8_t>(bytes.begin()+offset,bytes.begin()+offset+size)).first;break;}}
        if(source==weapon_banks_.end())return {};
    }
    auto found=weapon_sounds_.find({bank,program});if(found==weapon_sounds_.end())found=weapon_sounds_.emplace(std::make_pair(bank,program),std::make_shared<PcmClip>(decode_system_sound(source->second,program))).first;
    return found->second;
}
Sound3D AudioMixer::sound_parameter_3d(SoundPoint p,int mode)const{
    Sound3D out;
    const float dx=camera_pos_.x-p.x,dy=camera_pos_.y-p.y,dz=camera_pos_.z-p.z;
    out.distance=std::sqrt(dx*dx+dy*dy+dz*dz); // njDistanceP2P
    // ELF 0x2936BC: unsigned Val > 510 -> 509; Val 510 reads the zero pad byte.
    int value=int(out.distance);if(unsigned(value)>510u)value=509;
    const int angle=std::int16_t(camera_ay_+int(10430.381f*std::atan2(dx,dz)))&0xffff; // wadGetAngle(&CameraPos, cam.ay, pPos)
    const int index=int((0.0054931640625f*float(angle))/5.0f); // 0..71, see sound_3d_tables.inc
    out.pan=PanTbl360[index];
    std::int8_t volume=std::int8_t(ThreeDVolTbl[value]+PanTbl360Vol[index]);
    if(mode==0)volume=std::int8_t(volume-std::int8_t(current_room_fx_level_)/12);
    out.volume=volume;return out;
}
AudioMixer::SeRequest AudioMixer::setup_se_generic(unsigned se_slot,SoundPoint position)const{
    const auto p=sound_parameter_3d(position,0);const unsigned flags=se_3d_flag(se_slot);
    SeRequest r;r.pan=p.pan;r.volume=p.volume;r.volume_default=(flags&2)!=0;r.pan_default=(flags&4)!=0;return r;
}
void AudioMixer::apply_se_volume(unsigned voice,float volume){
    auto& state=se_volume_.at(voice);
    switch(state.function){
    case 0:state.volume=volume;break;
    case 1:if(volume<state.last)request_se_fade(voice,-1,int(volume),state.remaining);state.volume=volume;break;
    case 2:break;
    }
    if(active(voice)){auto& c=slots_[voice];if(c.shot)shot_volume(c,state.volume);else c.gain=se_volume_gain(state.volume);}
}
void AudioMixer::request_se_fade(unsigned voice,int start,int target,int frames){
    auto& state=se_volume_.at(voice);
    state.volume=float(start==-1?int(state.volume):start);state.last=float(target);state.remaining=frames;
    if(frames)state.speed=-((state.last-state.volume)/float(frames));
    state.function=state.speed<0?1:2;
}
void AudioMixer::sound_sync(){
    // sdSysServer runs first (sdfunc.c:465): a shot requested since the last call leaves as SdrSeReq; every later
    // parameter write reaches TSNDDRV as SdrSeChg.
    for(auto& c:slots_)c.shot_request=false;for(auto& c:releasing_)c.shot_request=false;
    // ExecSoundSynchProgram -> sdSysServer (ps2_sg_sd.c:1680): FxLevelTimer steps EVOL of both cores.
    if(fx_level_timer_){--fx_level_timer_;if(!fx_level_timer_)current_fx_level_=room_fx_target_;else current_fx_level_+=add_fx_level_;
        set_rev(1,5,current_fx_level_);set_rev(2,5,current_fx_level_);}
    auto fade=[&](SoundControl& state,unsigned voice){
        if(!state.function)return;
        state.volume-=state.speed;--state.remaining;
        if(state.remaining<0){state.volume=state.last;if(int(state.volume)==-127)stop(voice);state.function=0;}
        if(active(voice)){auto& c=slots_[voice];if(c.shot)shot_volume(c,state.volume);else c.gain=se_volume_gain(std::int8_t(int(state.volume)));}
    };
    for(unsigned midi=0;midi<8;++midi){const unsigned voice=midi_host_voice(midi);fade(midi_control_[midi],voice);}
    for(unsigned slot=0;slot<20;++slot)fade(se_volume_[se_host_voice(slot)],se_host_voice(slot));
    for(unsigned midi=0;midi<8;++midi){auto& state=midi_control_[midi];if(!state.pan_function)continue;
        state.pan-=state.pan_speed;--state.pan_remaining;if(state.pan_remaining<0){state.pan=state.pan_last;state.pan_function=0;}
        apply_midi_pan(midi,state.pan);}
    for(unsigned slot=0;slot<4;++slot){auto& state=adx_control_[slot];if(!state.function)continue;
        state.volume-=state.speed;--state.remaining;
        if(state.remaining<0){state.volume=state.last;if(int(state.volume)==-127)stop(slot);state.function=0;}
        if(active(slot)&&slots_[slot].volume>0)slots_[slot].gain=adx_gain(state.volume)/slots_[slot].volume;}
    for(unsigned slot=0;slot<20;++slot){const auto voice=se_host_voice(slot);auto& state=se_volume_[voice];if(!state.pan_function)continue;
        state.pan-=state.pan_speed;--state.pan_remaining;if(state.pan_remaining<0){state.pan=state.pan_last;state.pan_function=0;}
        apply_se_pan(voice,state.pan);}
}
void AudioMixer::request_pan(SoundControl& state,int start,int target,int frames){
    state.pan=float(start==-1?int(state.pan):start);state.pan_last=float(target);state.pan_remaining=frames;
    if(frames)state.pan_speed=-(((128.f+state.pan_last)-(128.f+state.pan))/float(frames));state.pan_function=1;
}
void AudioMixer::request_midi_fade(unsigned midi,int start,int target,int frames){
    auto& state=midi_control_.at(midi);state.volume=float(start==-1?int(state.volume):start);state.last=float(target);state.remaining=frames;
    if(frames)state.speed=-((state.last-state.volume)/float(frames));state.function=state.speed<0?1:2;
}
void AudioMixer::request_adx_fade(unsigned slot,int function,int timer){
    auto& state=adx_control_.at(slot);
    int count=((timer/100)*30+((timer%100)*6)/10)/2;if(count==0)count=1;
    state.remaining=count;
    if(function==1)state.last=adx_limit_[slot];
    else if(function==2)state.last=-127.f;
    state.speed=(state.volume-state.last)/float(count);
    state.function=function;
}
void AudioMixer::request_adx_fade_ex(unsigned slot,int start,int target,int frames){
    auto& state=adx_control_.at(slot);state.volume=float(start==-1?int(state.volume):start);state.last=float(target);state.remaining=frames;
    if(frames)state.speed=-((state.last-state.volume)/float(frames));
    state.function=1;
}
void AudioMixer::apply_adx_volume(unsigned slot,float volume){
    adx_control_.at(slot).volume=volume;
    if(active(slot)&&slots_[slot].volume>0)slots_[slot].gain=adx_gain(volume)/slots_[slot].volume;
}
void AudioMixer::apply_adx_volume_ex(unsigned slot,float volume,float max){
    adx_limit_[slot]=max<-20.f?max+20.f:0.f;
    apply_adx_volume(slot,volume);
}
void AudioMixer::apply_midi_volume(unsigned midi,float volume){
    auto& state=midi_control_.at(midi);
    if(state.function==0)state.volume=volume;
    else if(state.function==1){if(volume<state.last)request_midi_fade(midi,-1,int(volume),state.remaining);state.volume=volume;}
    const unsigned voice=midi_host_voice(midi);if(active(voice))slots_[voice].gain=se_volume_gain(std::int8_t(int(state.volume)));
}
void AudioMixer::apply_midi_pan(unsigned midi,float pan){
    auto& state=midi_control_.at(midi);if(!state.pan_function&&state.function==2)return;state.pan=pan;
    const unsigned voice=midi_host_voice(midi);const int encoded=std::int8_t(int(pan));pan_gains(unsigned(std::clamp(encoded/2+64,0,127)),slots_[voice].pan_left,slots_[voice].pan_right);
}
void AudioMixer::restore_midi(unsigned midi,unsigned voice){
    slots_[voice].gain=se_volume_gain(std::int8_t(int(midi_control_.at(midi).volume)));
    const int encoded=std::int8_t(int(midi_control_[midi].pan));pan_gains(unsigned(std::clamp(encoded/2+64,0,127)),slots_[voice].pan_left,slots_[voice].pan_right);
}
unsigned AudioMixer::se_voice(unsigned se_slot){return se_host_voice(se_slot);}
AudioMixer::SoundState AudioMixer::adx_state(unsigned slot)const{
    if(slot>=adx_control_.size())throw std::runtime_error("ADX slot outside AdxTInfo[4]");
    const auto& state=adx_control_[slot];
    return {state.volume,state.pan,state.remaining,state.pan_remaining,state.function,state.pan_function,state.pitch};
}
AudioMixer::SoundState AudioMixer::sound_state(unsigned voice,bool midi)const{
    const auto& state=midi?midi_control_.at(voice):se_volume_.at(voice);
    return {state.volume,state.pan,state.remaining,state.pan_remaining,state.function,state.pan_function,state.pitch};
}
void AudioMixer::apply_se_pan(unsigned voice,float pan){
    se_volume_.at(voice).pan=pan;
    if(!active(voice)||!slots_[voice].clip)return;auto& c=slots_[voice];
    if(c.shot){shot_pan(c,pan);return;}
    const int driver_pan=std::int8_t(int(pan));
    pan_gains(unsigned(std::clamp(int(c.clip->pan)+driver_pan/2,0,127)),c.left,c.right);
}
void AudioMixer::apply_se_pitch(unsigned voice,int pitch){
    se_volume_.at(voice).pitch=pitch;if(!active(voice))return;auto& c=slots_[voice];
    // Tsnd_tqchg (TSNDDRV 0x165c..0x16dc): a change to bend 8192 is stored without a pitch-bend message, so the
    // channel keeps the previous bend; SdrSeReq always sends it. sdShotSetPitch: 3 * pitch + 8192 (u16), 512..16383.
    if(c.shot&&!c.shot_request&&std::clamp(int(std::uint16_t(8192+3*pitch)),512,16383)==8192)return;
    c.pitch=pitch;
}
void AudioMixer::shot_levels(Channel& c){
    // SE note-on velocity is the literal 127 (Tsnd_tqreq), CC11 is never sent to an SE channel (expression 128).
    c.levels=sony_voice_levels(c.clip->sony_volume,127,sony_controller_scale(c.shot_cc7),128,int(c.clip->pan),c.shot_cc10);
    c.left=float(c.levels.left)/16384.f;c.right=float(c.levels.right)/16384.f;
}
void AudioMixer::shot_volume(Channel& c,float volume){
    // sdShotSetVol: SND_WORK.vol (u8) = (Sint8)Volume + 128; SdrSeReq/SdrSeChg pass it as char and mark it valid only
    // when >= 0 (CheckCmdReq/Chg). Invalid: Req uses 127 (TSNDDRV 0xdc4), Chg keeps the old value (0xca0, 0x1508).
    // tsnd_tq_chgreq: CC7 = min(127, 127 * vol / 127).
    const unsigned value=std::uint8_t(int(std::int8_t(int(volume)))+128);
    if(value<128)c.shot_cc7=value;else if(c.shot_request)c.shot_cc7=127;
    shot_levels(c);
}
bool AudioMixer::shot_pan(Channel& c,float pan){
    // sdShotSetPan: (Sint8)Pan / 2 + 64; Tsnd_tqchg stores 64 without a CC10 (0x1588..0x15f4), the channel keeps
    // its pan; SdrSeReq always sends it. tsnd_tq_chgreq: CC10 = clamp(64 + pan - 64, 0, 127).
    const unsigned value=unsigned(int(std::int8_t(int(pan)))/2+64);
    if(c.shot_request||value!=64)c.shot_cc10=value;
    shot_levels(c);return true;
}
void AudioMixer::play_se_request(unsigned voice,std::shared_ptr<PcmClip> clip,const SeRequest& request){
    if(!clip||!clip->frames())return;play_pcm(voice,std::move(clip));
    // ExPlaySe: PanDelayTime -2 -> SetPanSe(0), VolumeDelayTime -2 -> SdcSeDefaultVolume (0, sdcwrap.c:18).
    apply_se_pan(voice,request.pan_default?0:request.pan);apply_se_volume(voice,request.volume_default?0:request.volume);
}
bool AudioMixer::call_player_weapon_se(unsigned bank,int se_no,SoundPoint position,int slot_no,bool syukan){
    const int neo=int((unsigned(se_no)&0xFFFF00FFu)|0x100u); // BankNo 1 of the weapon pack, ListNo (char) neo
    unsigned se_slot=19;
    if(slot_no==0){se_slot=weapon_slot_switch_?9:8;weapon_slot_switch_=!weapon_slot_switch_;}
    auto request=setup_se_generic(se_slot,position);
    if(syukan){request.volume=0;request.volume_default=false;request.pan=0;request.pan_default=false;} // SetSyukanModeSoundParam
    if(!(se_3d_flag(se_slot)&2))request.volume=std::int8_t(request.volume+room_env_.weapon);
    auto clip=weapon_clip(bank,unsigned(neo)&255u);if(!clip)return false;
    play_se_request(se_host_voice(se_slot),clip,request);apply_se_pitch(se_host_voice(se_slot),0);
    played.push_back("ARMS/"+asset_number(bank)+"/"+asset_number(unsigned(neo)&255u)+"/slot"+std::to_string(se_slot));return true;
}
void AudioMixer::set_3d_sound_flag(int type,int slot,unsigned flag){
    auto se=[&](unsigned s){se_flags_.at(s)=flag;if(flag&2)apply_se_volume(se_host_voice(s),0);if(flag&4)apply_se_pan(se_host_voice(s),0);};
    auto in=[&](int count){if(slot<0||slot>=count)throw std::runtime_error("Set3dSoundFlag slot outside original table");};
    switch(type){
    case 0:se(7);break;
    case 1:in(3);se(unsigned(slot)*2+11);se(unsigned(slot)*2+12);break;
    case 2:se(6);break;
    case 3:se(8);se(9);se(19);break;
    case 4:se(10);break;
    case 5:in(6);se(unsigned(slot));break; // DefEne {0..5}
    // SetVolumeMidi2(...,0) clamps vol+128 to 127, the full MIDI voice volume; pan 0 is the part default.
    case 6:in(3);midi_flags_[unsigned(slot)]=flag;if(flag&2)apply_midi_volume(unsigned(slot),0);break;
    case 7:{in(5);static constexpr unsigned def_evt[5]={7,6,5,4,3};const auto midi=def_evt[slot];midi_flags_[midi]=flag;if(flag&2)apply_midi_volume(midi,0);if(flag&4)apply_midi_pan(midi,0);break;}
    case 8:{in(16);object_sounds_[unsigned(slot)].flags=flag;const int voice=object_sound_slot(unsigned(slot));if(voice>=0){const unsigned midi=3+unsigned(voice);if(flag&2)apply_midi_volume(midi,0);if(flag&4)apply_midi_pan(midi,0);}break;}
    case 10:adx_flag_=flag;apply_adx_volume(0,float(current_bgm_volume_));break;
    default:break; // the original switch has no other case
    }
}
void AudioMixer::reset_3d_sound_flag(){
    set_3d_sound_flag(0,0,0);for(int i=0;i<3;++i)set_3d_sound_flag(1,i,0);
    set_3d_sound_flag(2,0,0);set_3d_sound_flag(3,0,0);set_3d_sound_flag(4,0,0);
    for(int i=0;i<6;++i)set_3d_sound_flag(5,i,0);
    for(int i=0;i<3;++i)set_3d_sound_flag(6,i,0);
    // sdfunc.c:1108/1112 as in the original: type 7 over MaxSlotObjectSe, type 8 over MaxSlotEventSe.
    for(int i=0;i<int(max_object_slots_);++i)set_3d_sound_flag(7,i,0);
    for(int i=0;i<int(max_event_slots_);++i)set_3d_sound_flag(8,i,0);
    set_3d_sound_flag(9,0,0);set_3d_sound_flag(10,0,0);
}
void AudioMixer::load_room_sounds(unsigned stage,unsigned room,unsigned variant){
    if(stage>15||room>255||variant>15)throw std::runtime_error("invalid room sound key");
    room_env_={}; // RequestRoomSoundBank: ResetRoomSoundEnvParam
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
        // sdfunc.c:659: type 5 is copied into Room_SoundEnv (RM_SNDENV, 16 bytes).
        if(type==5){if(size<16)throw std::runtime_error("short room sound environment record");const auto* e=reinterpret_cast<const std::int8_t*>(bytes.data()+offset);
            room_env_={e[0],e[1],e[2],e[3],e[4],e[5],e[6],e[7],e[8],e[9],e[10],e[11],e[12]};
            // sdfunc.c:661-664: FxLevelTimer = 16, RoomFxLevel = RoomFxLevel*256, AddFxLevel = (Room-Current)/16.
            fx_level_timer_=16;room_fx_target_=int(room_env_.fx_level)*256;add_fx_level_=(room_fx_target_-current_fx_level_)/16;continue;}
        if(type>2)continue;
        if(type<=1&&bank==3){if(!(request_background_[0]&2)&&!(request_background_[1]&2)&&!(current_background_[0]==-1&&current_background_[1]==-1))continue;
            for(unsigned slot=0;slot<2;++slot)if(active(24+slot)){stop(24+slot);current_background_[slot]=-1;}}
        const unsigned cached=type==2?bank:type==1?0x100u|bank:~0u;
        for(auto it=room_sounds_.begin();it!=room_sounds_.end();)it=std::get<0>(it->first)==cached?room_sounds_.erase(it):std::next(it);
        std::vector<std::uint8_t> section(bytes.begin()+offset,bytes.begin()+offset+size);
        if(type==2){room_banks_[bank]=std::move(section);sound_bank_packs_[bank]=pack;}else if(type==1)midi_programs_[bank]=std::move(section);else midi_sequences_[bank]=std::move(section);}
    if(!terminated)throw std::runtime_error("unterminated room sound pack");room_pack_=pack;
    current_room_fx_level_=room_env_.fx_level; // SetRoomSoundFxLevelEx at the pack terminator; per-slot FxLev is empty on PS2 (ps2_sg_sd.c:542/966)
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
    const auto id=std::make_tuple(bank,program,-1);auto found=room_sounds_.find(id);if(found==room_sounds_.end())found=room_sounds_.emplace(id,std::vector<std::shared_ptr<PcmClip>>{std::make_shared<PcmClip>(decode_system_sound(source->second,program))}).first;
    if(found->second.front()->frames())play_pcm(slot,found->second.front(),1,loop);played.push_back("MULTSPQ"+disc_digit()+"/"+asset_number(sound_bank_packs_.at(bank))+"/"+std::to_string(bank)+"/"+asset_number(program));return true;
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
        const auto block=midi+offset;std::size_t end=s.size();
        for(unsigned index=0;index<=u32(midi+12);++index){const auto next=u32(midi+16+index*4);if(next!=0xffffffff&&next>offset)end=std::min(end,midi+std::size_t(next));}
        const auto decoded=decode_sony_sq(s,block,end);
        const unsigned ppqn=unsigned(s.at(block+4))|unsigned(s.at(block+5))<<8;
        struct Part {int program=-1;unsigned volume=127,expression=127,pan=64;};std::array<Part,16> parts{};
        auto sequence=std::make_shared<Sequence>();bool ended=false;
        double tempo=500000;std::string programs_played;
        for(const auto& input:decoded){
            SequenceEvent event;event.delay=double(input.delta)*tempo*48000/(ppqn*1000000.0);
            const unsigned current=input.status;
            if(current==0xff){const auto type=input.key;
                if(type==0x51)tempo=unsigned(input.meta[0])<<16|unsigned(input.meta[1])<<8|input.meta[2];
                event.status=current;event.key=type;sequence->events.push_back(event);
                if(type==0x2f){ended=true;break;}continue;}
            event.status=current;event.key=input.key;event.value=input.value;
            const unsigned char data[2]={input.key,input.value};auto& part=parts[current&15];
            switch(current&0xf0){case 0xc0:part.program=data[0];break;
                case 0xb0:if(data[0]==7)part.volume=data[1];else if(data[0]==11)part.expression=data[1];else if(data[0]==10)part.pan=data[1];break;
                case 0x90:if(data[1]){
                    if(part.program<0)return fail("note without program");
                    const auto id=std::make_tuple(0x100u|bank,unsigned(part.program),int(data[0]));auto found=room_sounds_.find(id);
                    // MODHSYN keys one voice per covering split (split loop 0x6098..0x64a0); each takes its own
                    // split's volume/pan with the same channel volume/expression/pan and velocity.
                    if(found==room_sounds_.end()){std::vector<std::shared_ptr<PcmClip>> voices;for(auto& clip:decode_system_note(programs->second,unsigned(part.program),int(data[0])))voices.push_back(std::make_shared<PcmClip>(std::move(clip)));found=room_sounds_.emplace(id,std::move(voices)).first;}
                    for(const auto& clip:found->second){if(!clip->frames())continue;Layer layer;layer.clip=clip;pan_gains(unsigned(std::clamp(int(layer.clip->pan)+int(part.pan)-64,0,127)),layer.left,layer.right);
                    const float gain=layer.clip->volume*float(data[1])/127.f*float(part.volume)/127.f*float(part.expression)/127.f;layer.left*=gain;layer.right*=gain;event.layers.push_back(std::move(layer));}
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
bool AudioMixer::event_sound(unsigned slot,unsigned sound,SoundPoint position,int mode){
    if(slot>max_event_slots_||slot>4)return true;const unsigned midi=7-slot;
    if(mode==0){const auto p=sound_parameter_3d(position,0);apply_midi_pan(midi,float(p.pan));apply_midi_volume(midi,float(p.volume));}
    else{apply_midi_pan(midi,0);if(!(midi_flags_[midi]&2))apply_midi_volume(midi,0);}
    const bool played=event_sound(slot,sound);if(played)restore_midi(midi,midi_host_voice(midi));return played;
}
int AudioMixer::object_sound_slot(unsigned object,bool find){
    for(unsigned i=0;i<max_object_slots_;++i)if(object_slots_[i].flag&&object_slots_[i].object==object){if(find)object_slots_[i].find=true;return int(i);}
    return -1;
}
void AudioMixer::regist_object_se(unsigned object,SoundPoint position,int sound,int priority){
    auto& o=object_sounds_.at(object);o.position=position;o.priority=priority;o.type=0;o.slot=-1;o.sound=int((unsigned(sound)&0xFFFF00FFu)|0x200u);o.request=true;
    for(unsigned i=0;i<object_request_count_;++i)if(object_requests_[i]==object)return;
    if(object_request_count_>=object_requests_.size())throw std::runtime_error("ObjectReqList overflow");
    object_requests_[object_request_count_++]=object;
}
void AudioMixer::free_object_se(unsigned object){
    auto& o=object_sounds_.at(object);o.slot=-1;o.request=false;bool shift=false;
    for(unsigned i=0;i<object_request_count_;++i){if(object_requests_[i]==object)shift=true;if(shift)object_requests_[i]=i+1<object_requests_.size()?object_requests_[i+1]:0;}
    if(shift)--object_request_count_;
    const int slot=object_sound_slot(object);if(slot>=0)stop(midi_host_voice(3+unsigned(slot)));
}
void AudioMixer::call_object_sound(unsigned slot,ObjectSound& o,bool start,const std::function<int(SoundPoint,SoundPoint)>& collision){
    const unsigned midi=3+slot,voice=midi_host_voice(midi);
    if(o.type==0){
        if(start){const auto bank=(unsigned(o.sound)>>8)&15u;if(!play_room_sequence(bank,unsigned(o.sound)&255u,voice)){unavailable.push_back("object SE "+std::to_string(o.sound));return;}}
        if(o.flags&2){if(o.flags&8){request_midi_fade(midi,o.volume_fade[0],o.volume_fade[1],o.volume_fade[2]);o.flags&=~8u;}}
        else apply_midi_volume(midi,float(std::int8_t(o.volume+(collision?collision(camera_pos_,o.position):0))));
        if(o.flags&4){if(o.flags&16){request_pan(midi_control_[midi],o.pan_fade[0],o.pan_fade[1],o.pan_fade[2]);o.flags&=~16u;}}
        else apply_midi_pan(midi,float(o.pan));
        if(start)restore_midi(midi,voice);
    }else{
        // CallObjectSe2's Type != 0 branch (sdfunc.c:2635-2644) is unreachable: RegistObjectSe
        // (sdfunc.c:1942) is the only writer of Object.Type and always stores 0. Not implemented.
        if(start)unavailable.push_back("CallObjectSe2 type != 0 (unreachable in the original)");
    }
}
void AudioMixer::exec_object_se(const std::function<int(SoundPoint,SoundPoint)>& collision){
    if(room_initializing_)return;
    for(unsigned i=0;i<object_request_count_;++i){const auto j=object_requests_[i];auto& o=object_sounds_[j];
        const auto p=sound_parameter_3d(o.position,o.type?1:0);o.pan=p.pan;o.volume=p.volume;o.distance=p.distance;
        if(o.type==0)o.volume=std::int8_t(o.volume+room_env_.object);o.slot=-1;o.request=true;}
    // The original bounds both following loops by MaxSlotObjectSe while indexing ObjectReqList.
    for(unsigned i=0;i<max_object_slots_;++i){auto& o=object_sounds_[object_requests_[i]];if(o.request){const int slot=object_sound_slot(object_requests_[i],true);if(slot>=0)o.slot=slot;}}
    for(unsigned i=0;i<max_object_slots_;++i){const auto j=object_requests_[i];auto& o=object_sounds_[j];if(!o.request)continue;
        if(o.slot<0){int slot=-1;for(unsigned k=0;k<max_object_slots_;++k)if(!object_slots_[k].flag){slot=int(k);break;}
            if(slot>=0){call_object_sound(unsigned(slot),o,true,collision);object_slots_[unsigned(slot)]={o.sound,true,false,j};}}
        else{call_object_sound(unsigned(o.slot),o,false,collision);object_slots_[unsigned(o.slot)]={o.sound,true,false,j};}
        o.request=false;}
    for(unsigned i=0;i<max_object_slots_;++i)object_slots_[i].find=false;
}
bool AudioMixer::play_voice(int sound,SoundPoint position,int mode,unsigned fade_rate,bool pause){
    // PlayVoiceEx2 (sdfunc.c:2087) on ADX slot 1; PatId[1] is the VOICE archive.
    if(mode<0||mode>2)throw std::runtime_error("PlayVoiceEx2 mode outside the original switch");
    // GetAdxStatus == 3 is PREP, not PLAYING. Native clips are loaded synchronously, so no preparing stream exists here.
    if(mode==2){ // parameter update of the running voice only, no restart
        const auto p=sound_parameter_3d(position,1);
        adx_control_[1].pan=p.pan;apply_adx_volume(1,float(p.volume));return true;
    }
    if(mode==0){
        const auto p=sound_parameter_3d(position,1); // Mode 1: no RoomFxLevel subtraction
        adx_control_[1].pan=p.pan;apply_adx_volume(1,float(p.volume));
    }else{
        if(fade_rate)request_adx_fade(1,1,int(fade_rate));
        else apply_adx_volume(1,0);
        adx_control_[1].pan=0;
    }
    try{play(1,set_name("VOICE"),unsigned(sound),1,false,0);}catch(const std::exception& error){unavailable.push_back("VOICE/"+std::to_string(sound)+": "+error.what());return false;}
    (void)pause; // ADXT_Pause: jr ra; nop at ELF 0x2cb050.
    if(active(1))slots_[1].gain=adx_gain(adx_control_[1].volume);
    return true;
}
void AudioMixer::continue_voice(){} // ContinueAdx -> ADXT_Pause is empty in the original ELF.
void AudioMixer::stop_voice(unsigned fade_ticks){
    if(fade_ticks){request_adx_fade(1,2,int(fade_ticks));return;} // StopVoice(rate): RequestAdxFadeFunction(1,2,rate)
    stop(1);
}
bool AudioMixer::stop_event_sound(unsigned slot){if(slot<=max_event_slots_&&slot<=4)stop(12-slot);return true;}
void AudioMixer::sequence_step(Channel& channel){
    auto& s=*channel.sequence;
    for(unsigned guard=0;s.until<=0&&guard<4096;++guard){
        if(s.next>=s.events.size()){release_sequence_end(channel);return;}
        const auto& event=s.events[s.next++];const unsigned part=event.status&15;
        // A note in release (key-off) is a separate MODHSYN voice and keeps sounding.
        auto held=[&](const SequenceNote& note){return note.channel==part&&note.key==event.key&&!(note.layer.clip&&note.layer.clip->spu&&note.layer.env.phase==SpuEnvelope::Release);};
        if((event.status&0xf0)==0x90&&event.value){
            s.notes.erase(std::remove_if(s.notes.begin(),s.notes.end(),held),s.notes.end());
            // One MODHSYN voice per layer, keyed in split order: each core choice (0x64f8) sees the voices before it.
            for(const auto& layer:event.layers){s.notes.push_back({layer,part,event.key});auto& on=s.notes.back().layer;if(on.clip&&on.clip->spu)key_on(on.clip,on.env,on.core);}
        }else if((event.status&0xf0)==0x80||((event.status&0xf0)==0x90&&!event.value)){
            // MODHSYN note-off: key-off, the voice runs its sample ADSR2 release.
            for(auto& note:s.notes)if(held(note)){if(note.layer.clip&&note.layer.clip->spu)note.layer.env.key_off();else note.layer.clip.reset();}
            s.notes.erase(std::remove_if(s.notes.begin(),s.notes.end(),[](const SequenceNote& note){return !note.layer.clip;}),s.notes.end());
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
        }else if(event.status==0xff&&event.key==0x2f){release_sequence_end(channel);return;}
        if(s.next<s.events.size())s.until+=s.events[s.next].delay;
    }
    if(s.until<=0)throw std::runtime_error("SQ zero-duration loop/event limit");
}
bool AudioMixer::user_sound_volume(unsigned type,unsigned slot,int start,int target,unsigned frames){
    std::vector<unsigned> voices;
    // SetUserSoundVolume fades a slot only while its Set3dSoundFlag bit 2 is set.
    static constexpr unsigned def_evt[5]={7,6,5,4,3};
    switch(type){
    case 0:if(!(se_flags_[7]&2))return true;voices={7};break;
    case 1:{static constexpr unsigned pairs[3][2]={{4,5},{14,15},{16,17}};if(slot>=3)return false;if(!(se_flags_[slot*2+11]&2))return true;voices={pairs[slot][0],pairs[slot][1]};break;}
    case 2:if(!(se_flags_[6]&2))return true;voices={6};break; // CallPlayerActionSe voice
    case 3:if(!(se_flags_[8]&2))return true;voices={weapon_voice_8,weapon_voice_9,weapon_voice_19};break;
    case 4:if(!(se_flags_[10]&2))return true;voices={se_host_voice(10)};break;
    case 5:if(slot>=6)return false;if(!(se_flags_[slot]&2))return true;voices={18+slot};break;
    case 6:if(slot>=3)return false;if(!(midi_flags_[slot]&2))return true;voices={24+slot};break;
    case 7:if(slot>=5)return false;if(!(midi_flags_[def_evt[slot]]&2))return true;voices={12-slot};break;
    case 8:{if(slot>=16)return false;auto& object=object_sounds_[slot];if(object.flags&2){object.volume_fade={start,target,int(frames)};object.flags|=8;}return true;}
    case 10:if(!(adx_flag_&2))return true;voices={0};break;
    default:return false; // cartridge/object voices need their own work
    }
    for(const auto voice:voices){
        if(type<=5)request_se_fade(voice,start,target,int(frames));
        else if(type==6||type==7)request_midi_fade(type==6?slot:def_evt[slot],start,target,int(frames));
        else if(type==10)request_adx_fade_ex(0,start,target,int(frames));
    }return true;
}
bool AudioMixer::user_sound_pan(unsigned type,unsigned slot,int start,int target,unsigned frames){
    std::vector<unsigned> se;
    switch(type){
    case 0:if(se_flags_[7]&4)se={7};break;
    case 1:if(slot>=3)return false;if(se_flags_[slot*2+11]&4)se={slot*2+11,slot*2+12};break;
    case 2:if(se_flags_[6]&4)se={6};break;
    case 3:if(se_flags_[8]&4)se={8,9,19};break;
    case 4:if(se_flags_[10]&4)se={10};break;
    case 5:if(slot>=6)return false;if(se_flags_[slot]&4)se={slot};break;
    case 7:{if(slot>=5)return false;const unsigned midi=7-slot;if(midi_flags_[midi]&4)request_pan(midi_control_[midi],start,target,int(frames));return true;}
    case 8:{if(slot>=16)return false;auto& object=object_sounds_[slot];if(object.flags&4){object.pan_fade={start,target,int(frames)};object.flags|=16;}return true;}
    default:break;
    }
    for(const auto slot:se)request_pan(se_volume_[se_host_voice(slot)],start,target,int(frames));return true;
}
void AudioMixer::stop_background_sound(unsigned slot,unsigned fade_ticks){if(slot>2)throw std::runtime_error("background sound slot outside original three");stop(24+slot,fade_ticks);current_background_[slot]=-1;}
bool AudioMixer::enemy_sound(unsigned slot,unsigned sound){
    if(slot>=6)throw std::runtime_error("enemy sound slot outside original six voices");
    // sdfunc.c CallEnemySe: force bank 3, preserve the low 8-bit list number.
    return play_room_sound(3,sound&255,18+slot);
}
void AudioMixer::request_enemy_se(unsigned enemy,SoundPoint position,int se_no,int flag,int fade_rate){
    if(enemy>=enemy_info_.size())throw std::runtime_error("RequestEnemySeBasic: EnemyNo outside EnemyInfo[128]");
    // char VolDownTbl[8] = {0,254,252,250,248,247,246,245}; MIPS char is signed.
    static constexpr std::int8_t vol_down[8]={0,-2,-4,-6,-8,-9,-10,-11};
    const unsigned down=(unsigned(se_no)&0xF00000u)>>20;
    // Indizes 8..15 lesen hinter das Feld: VolDownTbl liegt im Rahmen bei sp+0x58 (ELF 0x295144,
    // Rahmen 0x60), sp+0x60.. ist der Rahmen des Aufrufers; RequestEnemySe/RequestEnemySeEx sind
    // Sprungstummel (0x295240/0x295250) ohne eigenen Rahmen, der Inhalt haengt also vom rufenden
    // Gegnerpass ab. Kein SeNo-Literal der Originalquellen (en*.c, zonzon*.c, effsub*.c) erreicht
    // 8..15; der Fall wird gemeldet, nicht geraten.
    if(down>=8){unavailable.push_back("RequestEnemySeBasic VolDownTbl["+std::to_string(down)+"] SE "+std::to_string(se_no));return;}
    auto& e=enemy_info_[enemy];e.pos=position;
    const auto p=sound_parameter_3d(position,0);e.pan=std::int8_t(p.pan);e.vol=std::int8_t(p.volume);e.dist=p.distance;
    e.vol=std::int8_t(e.vol+room_env_.enemy);e.vol=std::int8_t(e.vol+vol_down[down]);
    e.fade=fade_rate;
    const int prio=(se_no&0xF0000)>>16;if(e.prio>prio)e.prio=std::int8_t(prio);
    if(!(se_no&0xF000000)){e.se=se_no;e.req=true;e.call=flag;}else{e.se_v=se_no;e.req_v=true;e.call_v=flag;}
}
void AudioMixer::call_enemy_se_main(unsigned slot,int se_no,int pan,int volume,int flag,int fade_rate){
    const unsigned voice=18+slot;auto& c=slots_[voice];
    if(flag==0){
        // ListNo -1: PlaySe keys nothing; only the running voice's pan and, unless an SE fade runs, volume change.
        if(!se_volume_[voice].function)apply_se_volume(voice,float(volume));apply_se_pan(voice,pan);return;
    }
    stop(voice);
    try{if(!play_room_sound((unsigned(se_no)>>8)&0xF,unsigned(se_no)&255,voice)){unavailable.push_back("enemy SE "+std::to_string(se_no));return;}}
    catch(const std::exception& error){unavailable.push_back("enemy SE "+std::to_string(se_no)+": "+error.what());return;}
    apply_se_pan(voice,pan);
    if(!fade_rate){se_volume_[voice].function=0;apply_se_volume(voice,float(volume));return;}
    if(active(voice)){if(c.shot)shot_volume(c,se_volume_[voice].volume);else c.gain=se_volume_gain(se_volume_[voice].volume);}
    request_se_fade(voice,-127,volume,fade_rate);
}
bool AudioMixer::enemy_se_playing(unsigned enemy,int se_no)const{
    for(const auto& v:enemy_voices_)if(v.flag&&v.enemy==enemy){if(se_no>0)return true;if(v.se==se_no)return true;}return false;
}
void AudioMixer::all_stop_enemy_se(){
    // RequestSeFadeFunction(..., 2, 0xC8): FadeCntMax = ((200/100)*30 + 0)/2, VolLast -127 (sdc.c:693);
    // the same speed RequestSeFadeFunctionEx(-1, -127, FadeCntMax) produces, and Func 2 ends with the stop.
    constexpr int timer=0xC8;int count=((timer/100)*30+((timer%100)*6)/10)/2;if(count==0)count=1;
    for(unsigned i=0;i<6;++i){auto& v=enemy_voices_[i];
        if(v.flag)request_se_fade(18+i,-1,-127,count);else stop(18+i); // DefEne[6] = {0..5}; StopEnemySe(i)
        v.enemy=0;v.se=0;v.flag=false;}
}
void AudioMixer::play_player_voice(int se_no){
    // CallPlayerVoice: SetupSeGenericParm(7, SeNo, &PlayerPos, ...), + VolPlayerVoice unless 3D flag 2.
    auto request=setup_se_generic(7,player_pos_);if(!(se_3d_flag(7)&2))request.volume=std::int8_t(request.volume+room_env_.player_voice);
    try{if(!play_room_sound((unsigned(se_no)>>8)&0xF,unsigned(se_no)&255,7))unavailable.push_back("player voice "+std::to_string(se_no));
        else{apply_se_pan(7,request.pan_default?0:request.pan);apply_se_volume(7,request.volume_default?0:request.volume);}}
    catch(const std::exception& error){unavailable.push_back("player voice "+std::to_string(se_no)+": "+error.what());}
}
void AudioMixer::exec_enemy_se(const std::function<int(SoundPoint,SoundPoint)>& collision){
    if(room_initializing_)return; // StartInitScriptFlag
    for(unsigned i=0;i<6;++i)if(!active(18+i))enemy_voices_[i]={};
    std::vector<unsigned> list;for(unsigned i=0;i<enemy_info_.size();++i)if(enemy_info_[i].req||enemy_info_[i].req_v)list.push_back(i);
    // The original exchange sort swaps on Prio > or, failing that, Dist > (not a Prio-then-Dist key).
    for(std::size_t i=0;i+1<list.size();++i)for(std::size_t j=i+1;j<list.size();++j){
        const auto a=list[i],b=list[j];if(enemy_info_[a].prio>enemy_info_[b].prio||enemy_info_[a].dist>enemy_info_[b].dist){list[i]=b;list[j]=a;}}
    auto serve=[&](unsigned enemy,int se,int call,int attrib){
        int same=0;for(const auto& v:enemy_voices_)if(v.flag&&v.se==se&&v.enemy!=enemy&&call)++same;
        if(same&&same>=((se>>12)&0xF))return; // CheckPlaySameSe
        int slot=-1;for(unsigned i=0;i<6;++i)if(enemy_voices_[i].flag&&enemy_voices_[i].enemy==enemy&&enemy_voices_[i].attrib==attrib){slot=int(i);break;} // SearchPlayingEnemySe
        if(slot<0)for(unsigned i=0;i<6;++i)if(!enemy_voices_[i].flag){slot=int(i);break;} // SearchFreeEnemySeSlot
        if(slot<0)return;
        enemy_voices_[slot]={true,enemy&0xffffu,se,(se&0xF000000)>>24,(se&0xF0000)>>16}; // RegistEnemySlot
        const auto& e=enemy_info_[enemy];call_enemy_se_main(unsigned(slot),se,e.pan,e.vol,call,e.fade);
    };
    for(const auto enemy:list){auto& e=enemy_info_[enemy];
        if(collision)e.vol=std::int8_t(e.vol+collision(camera_pos_,e.pos));
        if(e.req){serve(enemy,e.se,e.call,0);e.req=false;}
        if(e.req_v){serve(enemy,e.se_v,e.call_v,1);e.req_v=false;}
        e.prio=3;}
}
void AudioMixer::stop_enemy_sound(unsigned slot){if(slot>=6)throw std::runtime_error("enemy sound slot outside original six voices");stop(18+slot);}
bool AudioMixer::footstep(unsigned floor,unsigned type,unsigned actor,bool enabled,SoundPoint position,bool syukan){
    if(actor>=foot_switch_.size()||type>3||(type<2&&floor>4))return false;
    auto parameter=sound_parameter_3d(position,0);if(syukan){parameter.pan=0;parameter.volume=0;}
    unsigned se_slot=6;
    if(type<2){foot_switch_[actor]=!foot_switch_[actor];se_slot=11+actor*2+unsigned(foot_switch_[actor]);}
    if(!enabled)return true;
    const unsigned voice=se_host_voice(se_slot),flags=se_3d_flag(se_slot);
    const float old_gain=slots_[voice].gain;
    if(!play_room_sound(2,(type==2?5:floor)&255u,voice))return false;
    slots_[voice].gain=old_gain;
    if(!(flags&2))apply_se_volume(voice,std::int8_t(parameter.volume+room_env_.player_action));
    if(!(flags&4))apply_se_pan(voice,float(parameter.pan));else apply_se_pan(voice,se_volume_[voice].pan);
    static constexpr int pitches[2][4]={{0,256,256,0},{512,768,768,512}};
    const int pitch=type<2?pitches[type][unsigned(4.0f*(-float(recvx_core_random_next())/-2.1474836e9f))&3u]:0;
    apply_se_pitch(voice,pitch);
    return true;
}
bool AudioMixer::cartridge_se(unsigned bank,SoundPoint position,int sound){
    auto request=setup_se_generic(10,position);request.volume=std::int8_t(request.volume-current_room_fx_level_/12);
    if(!(se_3d_flag(10)&2))request.volume=std::int8_t(request.volume+room_env_.cartridge);
    const auto clip=weapon_clip(bank,unsigned(sound)&255u);if(!clip)return false;
    play_se_request(se_host_voice(10),clip,request);
    played.push_back("ARMS/"+asset_number(bank)+"/"+asset_number(unsigned(sound)&255u)+"/slot10");return true;
}
bool AudioMixer::enemy_sound(unsigned slot,unsigned sound,SoundPoint position){
    if(slot>=6)throw std::runtime_error("enemy sound slot outside original six voices");
    auto request=setup_se_generic(slot,position);if(!(se_3d_flag(slot)&2))request.volume=std::int8_t(request.volume+room_env_.enemy);
    if(!enemy_sound(slot,sound))return false;
    apply_se_pan(18+slot,request.pan_default?0:request.pan);apply_se_volume(18+slot,request.volume_default?0:request.volume);return true;
}
void AudioMixer::load_door_sounds(unsigned bank){
    // RequestDoorSoundBank -> LoadSoundPackFile -> SetSoundData (sdc.c:85) only downloads program
    // bank 1; no MIDI port is stopped and MidiHandle[2] keeps its port rotation across doors.
    door_source_.clear();
    for(auto it=room_sounds_.begin();it!=room_sounds_.end();)it=std::get<0>(it->first)==0x101u?room_sounds_.erase(it):std::next(it);door_bank_=bank;
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
    // CallDoorSe (sdfunc.c:1684) -> ExPlayMidi -> PlayMidi -> sdMidiPlay (ps2_sg_sd.c:495): the port
    // MidiHandle[2] currently names is stopped if it still sounds (SdrBgmStop), then the handle
    // rotates 2 -> 8 -> 9 -> 2 and the new list plays on that port. door_slot_ is the named port.
    if(active(door_slot_))stop(door_slot_);
    door_slot_=door_slot_==door_voice_last?door_voice_first:door_slot_+1;
    // RequestInfo (sdfunc.c:1684-1703): Pan/Volume/PitchDelayTime -2 -> ExPlayMidi (sdc.c:230-262)
    // SetPanMidi 0, SetVolumeMidi SdcSeDefaultVolume (0, sdcwrap.c:18), SetPitchMidi 0 sofort;
    // SpeedDelayTime -1 unveraendert; FxLevel DoorFxDef[] = 40 geht an sdMidiSetFxLev, das auf PS2
    // nur den Initstatus zurueckgibt (ps2_sg_sd.c:542). Der frische Kanal (gain 1 = vol 0, Mitte) ist
    // genau dieser Zustand.
    if(!play_room_sequence(1,program,door_slot_)){unavailable.push_back("DOOR/"+asset_number(door_bank_)+"/"+asset_number(program));return false;}
    played.push_back("DOOR/"+asset_number(door_bank_)+"/"+asset_number(program));return true;
}
// Com_ExecRoomFadeIn (sdfunc.c:2905): RequestMidiFadeFunction(2, 2, 250) fades MidiHandle[2], and
// sdMidiSetVol/Volume_Control act on the one port that handle names (ps2_sg_sd.c:670, 1190);
// earlier ports were already stopped by the rotation or have run out on their own.
void AudioMixer::finish_door_sound(){if(active(door_slot_))stop(door_slot_,adx_fade_ticks(250));}
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
// MODHSYN voice allocation (.text 0x64f8): attr&0x30 0x10/0x20 pins the core, otherwise
// the core with more free voices (24 each after init 0x3170), ties to core 0.
unsigned AudioMixer::spu_voices(unsigned core)const{
    unsigned count=0;
    auto add=[&](const std::shared_ptr<PcmClip>& clip,const SpuEnvelope& env,unsigned voice_core){if(clip&&clip->spu&&env.sounding()&&voice_core==core)++count;};
    auto channel=[&](const Channel& c){add(c.clip,c.env,c.core);for(const auto& l:c.layers)add(l.clip,l.env,l.core);if(c.sequence)for(const auto& n:c.sequence->notes)add(n.layer.clip,n.layer.env,n.layer.core);};
    for(const auto& c:slots_)channel(c);
    for(const auto& c:releasing_)channel(c);
    return count;
}
unsigned AudioMixer::spu_core(std::uint8_t attr)const{
    if((attr&0x30)==0x10)return 0;
    if((attr&0x30)==0x20)return 1;
    const unsigned busy0=spu_voices(0),busy1=spu_voices(1);
    const unsigned free0=busy0<24?24-busy0:0,free1=busy1<24?24-busy1:0;
    return free0<free1?1:0;
}
void AudioMixer::key_on(const std::shared_ptr<PcmClip>& clip,SpuEnvelope& env,unsigned& core){
    core=spu_core(clip->spu_attr);
    env.key_on(clip->adsr1,clip->adsr2);
}
// CC 120 (MODHSYN 0x5bf4): ADSR2 := 0xC021, then key-off of every voice of the channel.
// sdShotStop (SdrSeCancel -> Tsnd_tqcancel) and sdMidiStop (port_stop) both end here.
void AudioMixer::release_channel(Channel& channel){
    auto off=[](const std::shared_ptr<PcmClip>& clip,SpuEnvelope& env){if(!clip||!clip->spu||!env.sounding())return false;env.adsr2=0xC021;env.key_off();return true;};
    Channel tail=std::move(channel);channel=Channel{};bool any=false;
    if(off(tail.clip,tail.env))any=true;else tail.clip.reset();
    for(auto& l:tail.layers){if(off(l.clip,l.env))any=true;else l.clip.reset();}
    if(tail.sequence){auto notes=std::make_shared<Sequence>();for(auto& n:tail.sequence->notes)if(off(n.layer.clip,n.layer.env)){notes->notes.push_back(n);any=true;}tail.sequence=std::move(notes);}
    tail.fade=0;tail.released=true;
    if(any)releasing_.push_back(std::move(tail));
}
// End of an SQ track: notes already keyed off finish their release; the transport ends.
void AudioMixer::release_sequence_end(Channel& channel){
    auto ended=std::move(channel.sequence);channel.sequence.reset();
    Channel tail;tail.volume=channel.volume;tail.gain=channel.gain;tail.pan_left=channel.pan_left;tail.pan_right=channel.pan_right;tail.pitch=channel.pitch;tail.released=true;
    tail.sequence=std::make_shared<Sequence>();
    for(const auto& n:ended->notes)if(n.layer.clip&&n.layer.clip->spu&&n.layer.env.phase==SpuEnvelope::Release)tail.sequence->notes.push_back(n);
    if(!tail.sequence->notes.empty())releasing_.push_back(std::move(tail));
}
// TSNDDRV treq_ALL_REV: per core bit, same mode -> EVOLL/EVOLR = depth, otherwise
// sceSdSetEffectAttr(core, {mode, depth, depth, delay, feedback}) and the stored mode.
void AudioMixer::set_rev(unsigned core_mask,unsigned mode,int depth){
    if(mode>=10)throw std::runtime_error("SdrSetRev: mode invalid");
    for(unsigned core=0;core<2;++core){if(!(core_mask&(1u<<core)))continue;
        if(rev_mode_[core]==mode)reverb_[core].set_depth(std::int16_t(depth));
        else{reverb_[core].set_effect(mode,std::int16_t(depth),false);rev_mode_[core]=mode;}}
}
// TSNDDRV iop_main_proc: spucore_init(0/1) clears the mode-0 work area (attr mode 0x100) and
// enables the effect; sdInit (ps2_sg_sd.c:233) then SdrSetRev(0/1, 0) and SdrSetRev(0/1, 5, 0).
void AudioMixer::init_spu(){
    for(auto& r:reverb_)r.set_effect(0,0,true);
    rev_mode_={0,0};
    set_rev(1,0,0);set_rev(2,0,0);
    room_fx_target_=0;fx_level_timer_=0;current_fx_level_=0;add_fx_level_=0;
    set_rev(1,5,0);set_rev(2,5,current_fx_level_);
}
void AudioMixer::play_pcm(unsigned slot,std::shared_ptr<PcmClip> clip,float volume,bool loop){if(slot>=slots_.size()||!clip||!clip->channels||clip->channels>2||clip->rate<8000||clip->rate>96000||clip->samples.size()%clip->channels||!clip->frames()||!std::isfinite(volume)||volume<0||volume>8||(clip->loop_end&&(clip->loop_start>=clip->loop_end||clip->loop_end>clip->frames())))throw std::runtime_error("invalid audio channel/clip");Channel c;c.volume=volume*clip->volume;pan_gains(clip->pan,c.left,c.right);c.clip_rate=clip->rate;c.clip=std::move(clip);c.loop=loop;slots_[slot]=std::move(c);
    auto& voice=slots_[slot];if(voice.clip->spu)key_on(voice.clip,voice.env,voice.core);
    // A Sony bank voice started by play_pcm is an SE shot (ExPlaySe -> SdrSeReq): its level is the MODHSYN register.
    if(voice.clip->spu&&voice.clip->sony_volume){voice.shot=voice.shot_request=true;voice.volume=volume;shot_levels(voice);}}
void AudioMixer::play_layers(unsigned slot,std::vector<Layer> layers,float volume,bool loop){
    if(layers.empty())throw std::runtime_error("audio voice without layers");
    play_pcm(slot,layers.front().clip,volume,loop);auto& c=slots_[slot];c.shot=false;c.volume=volume;c.left=layers.front().left;c.right=layers.front().right;
    for(std::size_t i=1;i<layers.size();++i){const auto& l=layers[i].clip;if(l&&l->spu)key_on(l,layers[i].env,layers[i].core);if(!l||!l->channels||l->channels>2||l->rate<8000||l->rate>96000||!l->frames()||(l->loop_end&&(l->loop_start>=l->loop_end||l->loop_end>l->frames())))throw std::runtime_error("invalid audio layer");c.layers.push_back(layers[i]);}
}
void AudioMixer::play(unsigned slot,const std::string& archive,unsigned id,float volume,bool loop,unsigned fade_ticks){
    if(fade_ticks>600*game_tick_hz)throw std::runtime_error("audio fade too long");
    const auto relative=std::filesystem::path("audio")/archive/(asset_number(id)+".adx");auto file=mods_/relative;
    if(mods_.empty()||!std::filesystem::is_regular_file(file))file=data_/"archives"/archive/(asset_number(id)+".bin");
    auto clip=std::make_shared<PcmClip>(decode_adx(read_asset_file(file,32*1024*1024)));play_pcm(slot,clip,volume,loop&&clip->loop_end>clip->loop_start);
    if(fade_ticks){slots_[slot].gain=0;slots_[slot].fade=fade_ticks*audio_frames_per_tick;}played.push_back(archive+"/"+asset_number(id));
}
void AudioMixer::stop(unsigned slot,unsigned fade_ticks){if(slot>=slots_.size()||fade_ticks>600*game_tick_hz)throw std::runtime_error("invalid audio slot");auto& s=slots_[slot];if(fade_ticks){s.target=0;s.fade=fade_ticks*audio_frames_per_tick;}else release_channel(s);}
bool AudioMixer::active(unsigned slot)const{return slot<slots_.size()&&(bool(slots_[slot].clip)||bool(slots_[slot].sequence));}
// adxwrap.c:650 GetAdxPlayTime: Abspielzeit des Stroms in Hundertstelsekunden. Die Position
// des Kanals zaehlt Quellbilder (position += rate/48000 je Ausgabebild, siehe mix), also gilt
// dieselbe Rechnung wie im Original: SampleCount / SamplingRate * 100. Ein natuerlich zu Ende
// gelaufener Strom behaelt seine Endzeit (nur `clip` wird geloest); erst stop/release setzt den
// Platz und damit die Zeit auf 0 zurueck. Nur die Plaetze 0 und 1 sind im Original angemeldet
// (sdfunc.c:310 RegistAdxStreamEx(2, ..)); fuer jeden anderen gibt es dort kein Handle.
unsigned AudioMixer::adx_play_time(unsigned slot)const{
    if(slot>=2||slot>=slots_.size())return 0;
    const auto& channel=slots_[slot];
    if(!channel.clip_rate)return 0;
    return unsigned(channel.position/double(channel.clip_rate)*100.0);
}
std::vector<std::int16_t> AudioMixer::mix(unsigned frames){if(frames>48000*10)throw std::runtime_error("audio mix too large");std::vector<std::int16_t> out(std::size_t(frames)*2);
    for(unsigned f=0;f<frames;++f){float left=0,right=0;
        // Effect input per SPU2 core (VMIXEL/VMIXER); ADX streams enter dry only (TSNDDRV MMIX 0xffcf/0xffcc).
        std::array<std::array<float,2>,2> wet{};
        auto mix_channel=[&](Channel& channel){if(!channel.clip&&!channel.sequence)return;
        if(channel.fade){channel.gain+=(channel.target-channel.gain)/channel.fade;if(!--channel.fade&&channel.target==0){release_channel(channel);return;}}
        if(channel.sequence&&!channel.released)sequence_step(channel);
        const float gain=channel.volume*channel.gain;
        auto voice=[&](std::shared_ptr<PcmClip>& held,double& position,SpuEnvelope& env,unsigned core,float l,float r,bool loop){auto& clip=*held;
            // MODHSYN voice: ENVX * sample >> 15 at the SPU2 rate; release end turns the voice off.
            float envelope=1;if(clip.spu){if(!env.tick()){held.reset();return;}envelope=float(env.level)/32768.f;}
            double advance=double(clip.rate)/48000;
            if(clip.sony_pitch){const auto& p=*clip.sony_pitch;
                const int encoded=std::clamp(int(std::uint16_t(8192+3*channel.pitch)),512,16383);
                const int bend=encoded-8192;const int fine=p.fine+bend*int(bend<0?p.bend_down:p.bend_up)/8192;
                advance=double(sony_sample_pitch(clip.rate,std::uint16_t(p.root),p.note,fine))/4096;}

            const auto end=loop&&clip.loop_end?clip.loop_end:clip.frames();if(position>=end){if(loop){const auto begin=clip.loop_end?clip.loop_start:0;position=begin+std::fmod(position-end,double(end-begin));}else{held.reset();return;}}
            const auto i=std::size_t(position),next=loop&&i+1>=end?(clip.loop_end?clip.loop_start:0):std::min(i+1,clip.frames()-1);const float fraction=float(position-i);
            auto sample=[&](unsigned ch){const float a=clip.samples[i*clip.channels+ch],b=clip.samples[next*clip.channels+ch];return (a+(b-a)*fraction)*envelope*gain;}; // SPU: sample * ENVX >> 15 precedes the voice volume
            const float sl=sample(0)*l,sr=sample(clip.channels==1?0:1)*r;
            const unsigned attr=clip.spu?clip.spu_attr:3u;
            if(attr&1)left+=sl;
            if(attr&2)right+=sr;
            if(clip.spu){if(attr&4)wet[core][0]+=sl;if(attr&8)wet[core][1]+=sr;}
            position+=advance;};
        if(channel.sequence){for(auto& note:channel.sequence->notes)if(note.layer.clip)voice(note.layer.clip,note.layer.position,note.layer.env,note.layer.core,note.layer.left*channel.pan_left,note.layer.right*channel.pan_right,note.layer.clip->loop_end>note.layer.clip->loop_start);
            channel.sequence->notes.erase(std::remove_if(channel.sequence->notes.begin(),channel.sequence->notes.end(),[](const SequenceNote& n){return !n.layer.clip;}),channel.sequence->notes.end());
            if(!channel.released)channel.sequence->until-=1;return;}
        // A SPU voice loops on its own VAG markers (ADPCM loop-start/repeat flags, kept only for Vagi attribute 1,
        // decode_system_sound) until key-off, whichever request keyed it: ExPlaySe (sdfunc.c CallEnemySeMain,
        // CallSystemSe, ...) has no one-shot override. The sequence notes above already follow the markers.
        auto looping=[&](const std::shared_ptr<PcmClip>& clip){return channel.loop||(clip->spu&&clip->loop_end>clip->loop_start);};
        if(channel.clip)voice(channel.clip,channel.position,channel.env,channel.core,channel.left,channel.right,looping(channel.clip));
        for(auto& layer:channel.layers)if(layer.clip)voice(layer.clip,layer.position,layer.env,layer.core,layer.left,layer.right,looping(layer.clip));
        // A finished first note hands the slot to the next sounding layer.
        if(!channel.clip||!channel.layers.empty()){channel.layers.erase(std::remove_if(channel.layers.begin(),channel.layers.end(),[](const Layer& l){return !l.clip;}),channel.layers.end());if(!channel.clip&&!channel.layers.empty()){const auto next=channel.layers.front();channel.layers.erase(channel.layers.begin());channel.clip=next.clip;channel.position=next.position;channel.left=next.left;channel.right=next.right;channel.env=next.env;channel.core=next.core;}}
        };
        for(auto& channel:slots_)mix_channel(channel);
        for(std::size_t k=0;k<releasing_.size();++k)mix_channel(releasing_[k]);
        releasing_.erase(std::remove_if(releasing_.begin(),releasing_.end(),[](const Channel& c){return !c.clip&&(!c.sequence||c.sequence->notes.empty())&&c.layers.empty();}),releasing_.end());
        for(unsigned core=0;core<2;++core){std::int32_t l=0,r=0;
            reverb_[core].process(std::int16_t(std::clamp(wet[core][0],-32768.f,32767.f)),std::int16_t(std::clamp(wet[core][1],-32768.f,32767.f)),l,r);
            left+=float(l);right+=float(r);}
    out[f*2]=std::int16_t(std::clamp(left,-32768.f,32767.f));out[f*2+1]=std::int16_t(std::clamp(right,-32768.f,32767.f));
    // sync.c bhControlVSync calls ExecSoundFadeManager at 60 Hz, twice per game tick.
    if(++sound_sync_frames_==800){sound_sync_frames_=0;sound_sync();}}
    return out;
}
}
namespace recvx {
float AudioMixer::adx_gain(float volume){const int index=std::clamp(int(-volume),0,127);return std::pow(10.f,AdxVolTbl[index]/200.f);}
unsigned AudioMixer::adx_fade_ticks(int timer){int count=((timer/100)*30+((timer%100)*6)/10)/2;if(count==0)count=1;return unsigned(count);}
void AudioMixer::play_bgm_archive(const std::string& archive,unsigned id,int fade_in_rate,int volume){
    if(fade_in_rate!=-1){apply_adx_volume_ex(0,-127.f,float(volume));request_adx_fade(0,1,fade_in_rate+1);}
    play(0,archive,id,1,true,0);
    apply_adx_volume(0,adx_control_[0].volume);
}
// PlayBgmEx2 (sdfunc.c:2007) und ExecAdxFadeManager im Soundmonitor (sdfunc.c:477) stehen unter
// !(ss_flg & 0x4000000); kein Originalschreiber setzt dieses Bit (alle ss_flg-|=/=-Stellen: 0x1,
// 0x40, 0x100, 0x200, 0x1000, 0x20000, 0x30000, 0x40000, 0x100000, 0x200000, 0x400200, 0x800000,
// 0x80000000), das Tor ist also immer offen.
void AudioMixer::play_bgm_ex(int bgm,int fade_in_rate,int volume){
    play_bgm_archive(set_name("BGM"),unsigned(bgm),fade_in_rate,volume);
    current_bgm_=bgm;current_bgm_volume_=volume;
}
void AudioMixer::play_dead_voice(int player){
    play_bgm_archive("ADV",unsigned(player+8),1,-45);
    current_bgm_=player+8;current_bgm_volume_=-45;
}
void AudioMixer::request_bgm2(int bgm,int volume){if(current_bgm_!=bgm){current_bgm_=bgm;req_fade_bgm_=8;}else if(current_bgm_volume_!=volume)req_fade_bgm_=2;current_bgm_volume_=volume;}
void AudioMixer::stop_bgm(int fade_out_rate){if(fade_out_rate)request_adx_fade(0,2,fade_out_rate);else stop(0);current_bgm_=-1;current_bgm_volume_=-127;}
void AudioMixer::stop_bgm2(){req_fade_bgm_=1;current_bgm_=-1;current_bgm_volume_=-127;}
void AudioMixer::request_midi_fade_timer(unsigned midi,int function,int timer){
    auto& state=midi_control_.at(midi);
    if(function==1){state.volume=-127.f;state.last=0.f;}else if(function==2)state.last=-127.f;
    int count=((timer/100)*30+((timer%100)*6)/10)/2;if(count==0)count=1;
    state.remaining=count;state.speed=(state.volume-state.last)/float(count);state.function=function;
}
void AudioMixer::request_sound_fade(int function,unsigned attributes,int timer){
    auto se=[&](unsigned slot){auto& state=se_volume_[se_host_voice(slot)];if(function==1){state.volume=-127.f;state.last=0.f;}else if(function==2)state.last=-127.f;
        int count=((timer/100)*30+((timer%100)*6)/10)/2;if(count==0)count=1;
        state.remaining=count;state.speed=(state.volume-state.last)/float(count);state.function=function;};
    if(attributes&1){se(11);se(12);}
    if(attributes&2)for(unsigned slot=0;slot<6;++slot)se(slot);
    if(attributes&8)request_midi_fade_timer(0,function,timer);
    if(attributes&16)request_midi_fade_timer(1,function,timer);
    if(attributes&32){for(unsigned slot=0;slot<max_object_slots_;++slot){
            if(function==1)request_midi_fade(3+slot,-127,-64,(timer/100)*30+((timer%100)*6)/10);
            else request_midi_fade_timer(3+slot,function,timer);}
        for(unsigned slot=0;slot<max_event_slots_;++slot)request_midi_fade_timer(7-slot,function,timer);}
    if(attributes&64)request_adx_fade(0,function,timer);
    if(attributes&128)request_adx_fade(1,function,timer);
}
void AudioMixer::room_fade_out(){
    unsigned attributes=0xA3;if(req_fade_bgm_)attributes|=0x40;req_fade_bgm_=0;
    for(unsigned slot=0;slot<2;++slot)if(request_background_[slot]&1)attributes|=8u<<slot;
    request_sound_fade(2,attributes,400);
}
void AudioMixer::room_fade_in(){
    room_initializing_=false;
    if(req_fade_bgm_&8)play_bgm_ex(current_bgm_,120,current_bgm_volume_);
    else if(req_fade_bgm_&2)request_adx_fade_ex(0,-1,current_bgm_volume_,42);
    req_fade_bgm_=0;
    for(unsigned slot=0;slot<2;++slot){
        if(request_background_[slot]&2)background_sound(slot,unsigned(current_background_[slot]),120);
        else if(background_init_[slot].active){const auto req=background_init_[slot];background_sound(slot,req.sound,req.timer);}
        request_background_[slot]=0;background_init_[slot].active=false;
    }
    request_sound_fade(1,0xA3,120);
    request_midi_fade_timer(2,2,250);
}
}
