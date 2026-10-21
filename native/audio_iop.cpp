#include "audio_iop.h"
#include "audio_pitch.h"
#include "binary_view.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace recvx {
namespace {
// sceMidi_Init(ctx, 4167) (TSNDDRV 0x39a4): MODMIDI 0x15c keeps 4167 << 7 as the per-ATick increment.
constexpr std::uint32_t midi_atick_increment=4167u<<7;
// MODHSYN 0x33a0: a pending voice keys on once the SPU reports ENVX below 1000.
constexpr std::int32_t key_on_envx=1000;
// TSNDDRV TsndTQ_ATick visits TQ ports 0..4 (`slti v1,v0,5`, 0x20f8) although requests accept 0..5.
constexpr unsigned tq_ticked_ports=5;
void list_remove(std::deque<int>& list,int v){const auto it=std::find(list.begin(),list.end(),v);if(it==list.end())throw std::logic_error("MODHSYN voice list out of step");list.erase(it);}
}

IopSound::IopSound(){
    // MODHSYN init (0x3110..0x3178): voices 0..23 of each core appended to its free list (0x3228); free counts 24.
    for(unsigned core=0;core<cores;++core)for(unsigned i=0;i<core_voices;++i){const int v=int(core*core_voices+i);voice_[v].core=core;voice_[v].index=i;voice_[v].list=0;free_[core].push_back(v);}
    for(unsigned p=0;p<hsyn_ports;++p)hsyn_[p].se=p>=bgm_ports;
    for(auto& s:seq_)s.seq_cc7.fill(127);
}

// ---------------------------------------------------------------------------------------------------- banks
void IopSound::load_se_bank(unsigned bank,AudioBank data){
    if(bank>=tq_ports)throw std::runtime_error("SE bank outside the six TQ ports");
    auto& t=tq_[bank];t.table.clear();t.loaded=bool(data);
    // The decoded-clip cache is keyed by the bank buffer; a replaced buffer's entries go (its address may be reused).
    if(const auto old=se_banks_.find(bank);old!=se_banks_.end()&&old->second)for(auto it=clip_cache_.begin();it!=clip_cache_.end();)it=std::get<0>(it->first)==static_cast<const void*>(old->second.get())?clip_cache_.erase(it):std::next(it);
    se_banks_[bank]=data;
    // Host policy for a replaced SE bank: its TQ port stops (Tsnd_tq_portstop, the cancel bit of every busy entry, CC 120
    // at the next ATick); the original load thread overwrites the SPU RAM the voices read.
    for(auto& e:t.entry)if(e.flags&1)e.flags|=2;
    if(!data)return;
    // TSNDDRV treq_LOAD (0x6600..0x6bc4): per program the Vagi entry named by split 0's Sset index (the module uses
    // that index for the Smpl and Vagi tables too): Vagi offset/rate/attribute, Smpl +0x0B (root note) and the length
    // 420 * size / rate in ATicks, size = next Vagi offset - this one (the last one ends at the Head's BD size).
    const auto& b=*data;BinaryView v(b);v.require(0,16);const std::size_t hd=v.u32(0);v.require(hd,0x30);
    const std::size_t head=hd+16;
    const std::size_t prog=hd+v.u32(head+0x14),smpl=hd+v.u32(head+0x1c),vagi=hd+v.u32(head+0x20);
    const std::uint32_t bd_size=v.u32(head+0x10),prog_last=v.u32(prog+12),vagi_last=v.u32(vagi+12);
    std::vector<std::uint32_t> size(std::size_t(vagi_last)+1);
    for(std::uint32_t i=0;i<=vagi_last;++i){const auto at=v.u32(vagi+16+i*4);const auto here=v.u32(vagi+at);
        const auto next=i<vagi_last?v.u32(vagi+v.u32(vagi+16+(i+1)*4)):bd_size;size[i]=next-here;}
    t.table.resize(std::size_t(prog_last)+1);
    for(std::uint32_t i=0;i<=prog_last;++i){auto& e=t.table[i];const auto off=v.u32(prog+16+i*4);
        if(off==0xffffffffu)continue; // zeros, length 0xffff
        const std::size_t program=prog+off;const std::size_t split0=program+v.u32(program);
        const unsigned idx=v.u16(split0);
        if(idx>vagi_last)continue;
        const std::size_t ventry=vagi+v.u32(vagi+16+idx*4);
        e.offset=v.u32(ventry);e.rate=v.u16(ventry+4);e.attr=v.u8(ventry+6);
        e.note=v.u8(smpl+v.u32(smpl+16+idx*4)+11);
        if(!e.rate)throw std::runtime_error("TSNDDRV SE table: Vagi rate 0 (divu break)");
        e.length=std::uint32_t(420u*size[idx])/e.rate;
    }
}
void IopSound::load_midi_bank(unsigned bank,AudioBank programs,AudioBank sequences){
    // Host policy for a replaced MIDI bank: every BGM port bound to it stops (port_stop type 2: play switch off,
    // CC 64/123 to its channels), as the EE's own StopThePsgSound does for bank 3.
    for(unsigned p=0;p<bgm_ports;++p)if(bgm_[p].hsyn_bank==int(bank)){if(seq_[p].playing)midi_switch(p,false);bgm_[p].flags&=~(1|4|0x40|0x80);midi_info_&=std::uint16_t(~(1u<<p));}
    if(const auto old=midi_programs_.find(bank);old!=midi_programs_.end()&&old->second)for(auto it=clip_cache_.begin();it!=clip_cache_.end();)it=std::get<0>(it->first)==static_cast<const void*>(old->second.get())?clip_cache_.erase(it):std::next(it);
    if(programs)midi_programs_[bank]=std::move(programs);else midi_programs_.erase(bank);
    if(sequences)midi_sequences_[bank]=std::move(sequences);else midi_sequences_.erase(bank);
}
AudioBank IopSound::port_bank(int port)const{
    if(port<0)return {};
    // A MODHSYN port reads the IOP buffer of its bank, which a later load of that bank number overwrites in place.
    if(hsyn_[port].se){const auto f=se_banks_.find(unsigned(port)-bgm_ports);return f==se_banks_.end()?AudioBank{}:f->second;}
    if(hsyn_[port].bank<0)return {};
    const auto f=midi_programs_.find(unsigned(hsyn_[port].bank));return f==midi_programs_.end()?AudioBank{}:f->second;
}
void IopSound::load_se_clip(unsigned bank,unsigned program,std::shared_ptr<const PcmClip> clip){
    if(bank>=tq_ports||program>127||!clip)throw std::runtime_error("synthetic SE entry outside the TQ table");
    auto& t=tq_[bank];t.loaded=true;if(t.table.size()<=program)t.table.resize(program+1);
    auto& e=t.table[program];e.rate=std::uint16_t(clip->rate);e.attr=clip->loop_end>clip->loop_start?1:0;e.note=60;
    // 420 * ADPCM bytes / rate ATicks: 16 bytes per 28 samples.
    e.length=std::uint32_t(420u*std::uint32_t(clip->frames()*16/28))/std::max<unsigned>(1,clip->rate);e.clip=std::move(clip);
}
std::vector<std::shared_ptr<const PcmClip>> IopSound::resolve(int port,unsigned program,unsigned key){
    if(port>=int(bgm_ports)){const auto& t=tq_[unsigned(port)-bgm_ports].table;if(program<t.size()&&t[program].clip)return {t[program].clip};}
    const auto bank=port_bank(port);if(!bank)return {};
    const auto id=std::make_tuple(static_cast<const void*>(bank.get()),program,key);
    auto found=clip_cache_.find(id);
    if(found==clip_cache_.end()){std::vector<std::shared_ptr<const PcmClip>> clips;
        try{for(auto& c:decode_system_note(*bank,program,int(key)))clips.push_back(std::make_shared<const PcmClip>(std::move(c)));}
        catch(const std::exception& e){unavailable.push_back("MODHSYN note-on port "+std::to_string(port)+" program "+std::to_string(program)+" key "+std::to_string(key)+": "+e.what());}
        found=clip_cache_.emplace(id,std::move(clips)).first;}
    return found->second;
}

// ---------------------------------------------------------------------------------------------------- RPC thread
void IopSound::send(){
    for(auto& c:queue_)rpc_.push_back(c);queue_.clear();
    process();
}
void IopSound::process(){
    while(!rpc_.empty()&&!wait_){if(!run(rpc_.front()))return;rpc_.pop_front();stage_=0;}
}
bool IopSound::run(const IopCommand& c){
    switch(c.kind){
    case IopCommand::SeReq:tq_request(c);return true;
    case IopCommand::SeChg:tq_change(c);return true;
    case IopCommand::SeCancel:tq_cancel(c);return true;
    case IopCommand::SeAllStop: // Tsnd_tq_allstop: Tsnd_tq_portstop for ports 5..0 (every busy entry gets the cancel bit)
        for(unsigned p=tq_ports;p-->0;)for(auto& e:tq_[p].entry)if(e.flags&1)e.flags|=2;return true;
    case IopCommand::Rev:if(on_rev)on_rev(c.core,c.mode,c.depth);return true;
    case IopCommand::SetIopData: // 0x2F: the status block the EE reads at its next sdSysServer
        for(unsigned p=0;p<tq_ports;++p)status_.se[p]=tq_[p].active;status_.midi=midi_info_;return true;
    case IopCommand::BgmChg:{
        // treq_BGM 0x27 (0x5c90..0x5e88), only for a port in use (+0 & 1).
        if(c.port>=bgm_ports)return true;auto& b=bgm_[c.port];if(!(b.flags&1))return true;
        const auto vol=std::uint8_t(c.vol);
        if(b.vol!=vol){b.vol=vol;b.flags|=0x20;}else b.flags&=~0x20;
        if(std::uint8_t(c.pan)==64){b.pan=64;b.flags&=~0x40;}else{b.pan=std::uint8_t(c.pan);b.flags|=0x40;}
        // SdrBgmChg packs the 14-bit pitch as two 7-bit bytes (((p & 0x3F80) << 1) | (p & 0x7F), ps2_snddrv.c:362);
        // TSNDDRV tests byte 7 == 32 and byte 6 == 0 (0x5dfc), i.e. the unsplit 0x2000. The EE's centre 8192 arrives as
        // 00 40, so the port keeps flag 0x80 and +4/+5 = 0x40/0x00, which the ATick sends as HsMsg 2 every tick.
        const auto packed=std::uint16_t(((c.pitch&0x3F80)<<1)|(c.pitch&0x7F));const std::uint8_t lo=std::uint8_t(packed&0xff),hi=std::uint8_t(packed>>8);
        if(hi==32&&lo==0){b.bend_msb=32;b.bend_lsb=0;b.flags&=~0x80;}else{b.bend_msb=hi;b.bend_lsb=lo;b.flags|=0x80;}
        return true;}
    case IopCommand::BgmStop:{
        // treq_BGM 0x21 (0x57ac..0x5910): play switch off while playing, flags/midi_info cleared, DelayATick(1).
        if(c.port>=bgm_ports)return true;
        if(stage_==0){if(seq_[c.port].playing)midi_switch(c.port,false);
            auto& b=bgm_[c.port];b.flags&=~(1|0x40|0x80);midi_info_&=std::uint16_t(~(1u<<c.port));b.flags&=~2;
            stage_=1;wait_=1;return false;}
        return true;}
    case IopCommand::BgmReq:{
        // treq_BGM 0x20 (0x4f98..0x57a4). Bank 0..3 with a loaded HD and BD, else the request is dropped.
        if(c.port>=bgm_ports||c.program>=4)return true;
        auto& s=seq_[c.port];auto& b=bgm_[c.port];
        if(stage_==0){
            if(!midi_programs_.count(c.program)||!midi_sequences_.count(c.program))return true;
            if(s.playing){midi_switch(c.port,false);stage_=1;wait_=2;return false;} // 0x50cc..0x5158: DelayATick(2)
            stage_=1;}
        if(stage_==1){
            // sceHSyn_Load when the port names another bank (0x5160..0x526c), snd_load_sqinit likewise (0x5270..0x533c).
            if(b.hsyn_bank!=int(c.program)){b.hsyn_bank=int(c.program);hsyn_[c.port].bank=int(c.program);}
            s.sq_bank=int(c.program);
            // sceMidi_SelectMidi(port, block) (0x53a0): the SQ's Midi table entry; a missing one ends the request.
            const auto& sq=*midi_sequences_.at(c.program);
            auto u32=[&](std::size_t at)->std::uint32_t{if(at+4>sq.size())throw std::runtime_error("SQ truncated");return std::uint32_t(sq[at])|sq[at+1]<<8|sq[at+2]<<16|std::uint32_t(sq[at+3])<<24;};
            s.selected=false;s.events.clear();
            try{
                if(sq.size()<64||std::memcmp(sq.data()+16,"IECSuqeS",8))throw std::runtime_error("no Sequ chunk");
                const std::size_t midi=u32(16+20);if(midi+16>sq.size()||std::memcmp(sq.data()+midi,"IECSidiM",8))throw std::runtime_error("no Midi chunk");
                const unsigned list=c.channel;
                if(list>u32(midi+12))throw std::runtime_error("sequence outside table");const auto offset=u32(midi+16+list*4);if(offset==0xffffffffu)throw std::runtime_error("empty sequence");
                const std::size_t block=midi+offset;std::size_t end=sq.size();
                for(unsigned i=0;i<=u32(midi+12);++i){const auto next=u32(midi+16+i*4);if(next!=0xffffffffu&&next>offset)end=std::min(end,midi+std::size_t(next));}
                s.events=decode_sony_sq(sq,block,end);
                s.ppqn=unsigned(sq.at(block+4))|unsigned(sq.at(block+5))<<8;if(!s.ppqn)throw std::runtime_error("PPQN 0 (divu break)");
                s.list=list;s.selected=true;
            }catch(const std::exception& e){unavailable.push_back("BgmReq port "+std::to_string(c.port)+" bank "+std::to_string(c.program)+" list "+std::to_string(c.channel)+": "+e.what());return true;}
            midi_location(c.port);
            midi_volume(c.port,unsigned(std::uint8_t(c.vol)));
            stage_=2;wait_=1;return false; // DelayATick(1) (0x5664)
        }
        // 0x566c..0x57a4: play switch on, +3 = vol, +6 = block, flags (clear 2, set 4 and 1), midi_info bit.
        midi_switch(c.port,true);
        b.vol=std::uint8_t(c.vol);b.flags&=~2;b.flags|=4|1;midi_info_|=std::uint16_t(1u<<c.port);
        return true;}
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------- TSNDDRV TQ
void IopSound::tq_request(const IopCommand& c){
    // Tsnd_tqreq (0xfa4): port < 6, a loaded bank, program <= the table's last index.
    if(c.port>=tq_ports)return;auto& t=tq_[c.port];if(!t.loaded||c.program>=t.table.size())return;
    const unsigned ch=c.channel&0x7f;if(ch>=16)return;
    auto& e=t.entry[ch];const auto& info=t.table[c.program];
    e.note=info.note;
    e.timer=info.attr?std::uint16_t(0xffff):std::uint16_t((info.length<<4)+112);
    e.program=std::uint8_t(c.program);e.channel=std::uint8_t(ch);
    // Dispatcher defaults when a field is absent (0xdc4/0xe00/0xe70): vol 127, pan 64, pitch 8192.
    e.vol=std::uint8_t(c.vol<0?127:c.vol);e.pan=std::uint8_t(c.pan<0?64:c.pan);e.pitch=std::uint16_t(c.pitch<0?8192:c.pitch);
    e.def_vol=127;e.def_pan=64;e.def_bend=8192;
    t.active|=std::uint16_t(1u<<ch);
    e.flags|=0x20|0x40|0x80|4|1;e.flags&=~2u;
}
void IopSound::tq_change(const IopCommand& c){
    // Tsnd_tqchg (0x13a0): every busy entry with the same program and channel.
    if(c.port>=tq_ports)return;auto& t=tq_[c.port];
    for(unsigned i=16;i-->0;){auto& e=t.entry[i];if(!(e.flags&1)||e.program!=(c.program&0x7f)||e.channel!=(c.channel&0x7f))continue;
        if(c.vol>=0){e.vol=std::uint8_t(c.vol);e.flags|=0x20;}
        if(c.pan>=0){if(c.pan==64){e.pan=64;e.flags&=~0x40u;}else{e.pan=std::uint8_t(c.pan);e.flags|=0x40;}}
        if(c.pitch>=0){if(c.pitch==8192){e.pitch=8192;e.flags&=~0x80u;}else{e.pitch=std::uint16_t(c.pitch);e.flags|=0x80;}}}
}
void IopSound::tq_cancel(const IopCommand& c){
    // Tsnd_tqcancel (0x1770): entry [port][channel] gets the cancel bit while busy.
    if(c.port>=tq_ports)return;const unsigned ch=c.channel&0x7f;if(ch>=16)return;auto& e=tq_[c.port].entry[ch];if(e.flags&1)e.flags|=2;
}
void IopSound::tq_atick(){
    for(unsigned p=0;p<tq_ticked_ports;++p){auto& t=tq_[p];if(!t.active)continue;const unsigned hp=bgm_ports+p;auto& q=hsyn_[hp].queue;
        for(unsigned ch=0;ch<16;++ch){if(!(t.active&(1u<<ch)))continue;auto& e=t.entry[ch];
            auto control=[&](unsigned cc,unsigned value){Message m;m.kind=Message::Control;m.channel=ch;m.a=cc;m.b=value;q.push_back(m);};
            if(e.flags&2){control(120,0);e.flags&=~2u;if(!(e.flags&4)){e.flags&=~1u;t.active&=std::uint16_t(~(1u<<ch));e.timer=0;continue;}}
            if(e.flags&4){
                control(7,e.def_vol&0x7f);
                Message pc;pc.kind=Message::Program;pc.channel=ch;pc.a=e.program&0x7f;q.push_back(pc);
                Message on;on.kind=Message::NoteOn;on.channel=ch;on.a=e.note&0x7f;on.b=127;on.id=1;q.push_back(on); // HsMsg fd 10 ch note 1 7f
                e.flags&=~4u;}
            if(e.timer!=0xffff){const unsigned dec=e.pitch>>9;e.timer=dec<e.timer?std::uint16_t(e.timer-dec):std::uint16_t(0);}
            if(e.timer==0){control(120,0);e.flags&=~1u;t.active&=std::uint16_t(~(1u<<ch));continue;}
            // tsnd_tq_chgreq (0x1ce8)
            if(e.flags&0x20){e.flags&=~0x20u;control(7,std::min(127u,unsigned(e.def_vol)*e.vol/127u));}
            if(e.flags&0x40){e.flags&=~0x40u;control(10,unsigned(std::clamp(int(e.def_pan)+int(std::int8_t(e.pan))-64,0,127)));}
            if(e.flags&0x80){e.flags&=~0x80u;const int bend=std::clamp(int(e.def_bend)+int(e.pitch)-8192,0,16383);
                Message m;m.kind=Message::Bend;m.channel=ch;m.a=unsigned(bend&0x7f);m.b=unsigned(bend>>7);q.push_back(m);}
        }}
}
void IopSound::bgm_atick(){
    // TSNDDRV ATick after sceHSyn_ATick (0x2590..0x2cd8); its messages wait for the next MODHSYN tick.
    for(unsigned p=0;p<bgm_ports;++p){if(!(midi_info_&(1u<<p)))continue;auto& b=bgm_[p];
        if(!seq_[p].playing&&!(b.flags&4)){b.flags&=~(1|0x40|0x80);midi_info_&=std::uint16_t(~(1u<<p));continue;}
        if(b.flags&4)b.flags&=~(4|2);
        if(b.flags&0x20){midi_volume(p,b.vol);b.flags&=~0x20;}
        if(b.flags&0x40)for(unsigned ch=0;ch<16;++ch){Message m;m.kind=Message::Pan;m.channel=ch;m.a=b.pan;hsyn_[p].queue.push_back(m);}
        if(b.flags&0x80)for(unsigned ch=0;ch<16;++ch){Message m;m.kind=Message::VoiceBend;m.channel=ch;m.a=b.bend_lsb;m.b=b.bend_msb;hsyn_[p].queue.push_back(m);}
    }
}

// ---------------------------------------------------------------------------------------------------- MODMIDI
void IopSound::midi_step(Seq& s){s.step=std::uint32_t(((std::uint32_t(s.tempo)<<7)/s.ppqn)<<8)/s.speed;}
void IopSound::midi_location(unsigned port){
    // sceMidi_MidiSetLocation(port, 0) (0x1bf0): tempo 500000, speed 256, PPQN from the block, loop table and the
    // channel controller store cleared, sequence CC7 127 (0x1994), first delta read (0x9b8).
    auto& s=seq_[port];s.tempo=500000;s.speed=256;s.tick=0;s.frac=0;s.next=0;
    s.seq_cc7.fill(127);s.nrpn_msb=255;s.data=255;for(auto& l:s.loops)l={};
    midi_step(s);s.next_tick=s.events.empty()?0:std::uint32_t(s.events.front().delta);
}
void IopSound::midi_send(unsigned port,const Message& m){
    // 0xda4: the channel is marked used (+54); default routing sends it to the MODHSYN port of the same number.
    seq_[port].used|=std::uint16_t(1u<<m.channel);hsyn_[port].queue.push_back(m);
}
void IopSound::midi_volume(unsigned port,unsigned volume){
    // sceMidi_MidiSetVolume(port, 255, vol) (0x1d64): port volume +50; a playing port resends CC7 of all 16 channels.
    auto& s=seq_[port];s.volume=volume;
    if(s.playing)for(unsigned ch=0;ch<16;++ch){Message m;m.kind=Message::Control;m.channel=ch;m.a=7;m.b=sony_midi_cc7(s.seq_cc7[ch],s.volume);midi_send(port,m);}
}
void IopSound::midi_switch(unsigned port,bool on){
    auto& s=seq_[port];
    if(on){if(s.selected)s.playing=true;return;} // 0x18c4 case 1; 0x1620 sends nothing after a location reset
    if(!s.playing)return;
    // 0x14bc: CC64 0 and CC123 to every channel used since the last stop, then the used mask is cleared.
    for(unsigned ch=0;ch<16;++ch)if(s.used&(1u<<ch)){Message m;m.kind=Message::Control;m.channel=ch;m.a=64;m.b=0;midi_send(port,m);m.a=123;midi_send(port,m);}
    s.used=0;s.playing=false;
}
bool IopSound::midi_events(unsigned port){
    auto& s=seq_[port];
    while(s.tick>=s.next_tick){
        if(s.next>=s.events.size())return false;
        const auto& e=s.events[s.next++];const unsigned type=e.status&0xf0,ch=e.status&15;bool jumped=false;
        if(e.status==0xff){
            if(e.key==0x2f)return false;
            if(e.key==0x51&&e.meta.size()>=3){s.tempo=std::uint32_t(e.meta[0])<<16|std::uint32_t(e.meta[1])<<8|e.meta[2];if(!s.tempo)throw std::runtime_error("SQ tempo 0");midi_step(s);}
        }else{
            Message m;m.channel=ch;
            if(type==0x80||type==0x90){m.kind=type==0x80||!e.value?Message::NoteOff:Message::NoteOn;m.a=e.key;m.b=e.value;midi_send(port,m);}
            else if(type==0xc0){m.kind=Message::Program;m.a=e.key;midi_send(port,m);}
            else if(type==0xe0){m.kind=Message::Bend;m.a=e.key;m.b=e.value;midi_send(port,m);}
            else if(type==0xb0){
                m.kind=Message::Control;m.a=e.key;m.b=e.value;
                if(e.key==7){s.seq_cc7[ch]=std::uint8_t(e.value&0x7f);m.b=sony_midi_cc7(s.seq_cc7[ch],s.volume);} // 0x1238 / 0xf30
                else if(e.key==32){unavailable.push_back("MODMIDI CC32 output routing not ported");}
                else if(e.key==99){s.nrpn_msb=std::uint8_t(e.value);s.data=255;} // 0x6c4
                else if(e.key==6){
                    s.data=std::uint8_t(e.value); // 0x6d0..0x6dc (+52 in the delay slot)
                    if(s.nrpn_msb==0){ // loop start: reuse the slot with this id or the first free one (0x6e4..0x7a0)
                        auto it=std::find_if(s.loops.begin(),s.loops.end(),[&](const Seq::Loop& l){return l.id==e.value;});
                        if(it==s.loops.end())it=std::find_if(s.loops.begin(),s.loops.end(),[](const Seq::Loop& l){return l.id==255;});
                        if(it!=s.loops.end()){it->id=std::uint8_t(e.value);it->count=255;it->tick=s.next_tick;it->next=s.next;}
                        else unavailable.push_back("MODMIDI loop table full");}
                }else if(e.key==38&&s.nrpn_msb==1&&s.data!=255){ // loop end (0x7cc..0x900)
                    const auto it=std::find_if(s.loops.begin(),s.loops.end(),[&](const Seq::Loop& l){return l.id==s.data;});
                    if(it!=s.loops.end()){
                        if(it->count==255)it->count=std::uint8_t(e.value);
                        if(e.value&&it->count==0)it->count=255; // the last pass falls through
                        else{s.tick=it->tick;s.next_tick=it->tick;s.next=it->next;jumped=true;if(e.value)--it->count;}
                    }else unavailable.push_back("MODMIDI loop end without start");
                }
                midi_send(port,m);
            }else seq_[port].used|=std::uint16_t(1u<<ch); // aftertouch: sent, MODHSYN has no handler (0x1268)
        }
        (void)jumped;
        // The next delta is read from the (possibly reloaded) data pointer and added to +24.
        if(s.next<s.events.size())s.next_tick+=std::uint32_t(s.events[s.next].delta);
    }
    return true;
}
void IopSound::midi_tick(unsigned port){
    // 0x1570: frac += 4167 << 7; events of the current tick; while frac >= step: frac -= step, tick++, events.
    auto& s=seq_[port];std::uint32_t acc=s.frac+midi_atick_increment;
    for(;;){
        if(!midi_events(port)){midi_switch(port,false);s.frac=0;return;}
        if(acc<s.step)break;acc-=s.step;++s.tick;
    }
    s.frac=acc;
}

// ---------------------------------------------------------------------------------------------------- MODHSYN
void IopSound::detach(int v){
    auto& w=voice_[v];
    if(w.chlist>=0){list_remove(hsyn_[w.port].channel[w.channel].list[w.chlist],v);w.chlist=-1;}
    if(w.list==1)list_remove(pending_[w.core],v);else if(w.list==2)list_remove(active_[w.core],v);else if(w.list==0)list_remove(free_[w.core],v);
    w.list=-1;
    if(w.port>=0&&hsyn_[w.port].count)--hsyn_[w.port].count;
}
void IopSound::free_voice(int v){
    // 0x5cd4: off the channel and core lists, state 0, port count -1, onto the free list (0x320c).
    auto& w=voice_[v];detach(v);w.state=0;free_[w.core].push_front(v);w.list=0;w.id=255;w.type=255;w.port=-1;w.channel=-1;
}
void IopSound::cancel_voice(int v){
    // 0x5840: a voice that never keyed on goes back to the free list; the caller already took it off its channel list.
    free_voice(v);
}
int IopSound::alloc_free(std::uint8_t attr){
    // 0x64f8: attr & 0x30 == 0x10 core 0, 0x20 core 1, else the core with more free voices (tie core 0); its +4 end.
    const unsigned a=attr&0x30;const unsigned core=a==0x10?0u:a==0x20?1u:(free_[0].size()<free_[1].size()?1u:0u);
    if(free_[core].empty())return -1;const int v=free_[core].back();free_[core].pop_back();voice_[v].list=-1;return v;
}
int IopSound::steal_active(unsigned prio,std::uint8_t attr){
    // 0x659c: cores 1 then 0 (mask 0xa7c0 {0x10, 0x20}), active list from its +4 end; class state & 0x300 lower wins
    // (only with priority <= the new one), then lower priority, then lower ENVX snapshot +18; first found on ties.
    unsigned mask=attr&0x30;if(!mask)mask=0x30;
    int best=-1;unsigned best_class=0x300,best_prio=prio,best_envx=0xffff;
    for(int core=1;core>=0;--core){if(!(mask&(core?0x20u:0x10u)))continue;
        for(auto it=active_[core].rbegin();it!=active_[core].rend();++it){const auto& w=voice_[*it];if(w.type!=0)continue;
            const unsigned cls=w.state&0x300u;
            if(cls<best_class){if(prio<w.prio)continue;best_class=cls;best_prio=w.prio;best_envx=w.envx;best=*it;}
            else if(cls==best_class){if(w.prio==best_prio){if(w.envx<best_envx){best_envx=w.envx;best=*it;}}else if(w.prio<best_prio){best_prio=w.prio;best_envx=w.envx;best=*it;}}
        }}
    if(best<0)return -1;
    auto& w=voice_[best];koff_[w.core]|=1u<<w.index;detach(best);w.state=0;w.id=255;w.type=255;return best;
}
int IopSound::steal_pending(unsigned prio,std::uint8_t attr){
    // 0x67a0: lowest priority (< new + 1) on the pending lists, cores 1 then 0, from the +4 end; no key-off.
    unsigned mask=attr&0x30;if(!mask)mask=0x30;int best=-1;unsigned best_prio=prio+1;
    for(int core=1;core>=0;--core){if(!(mask&(core?0x20u:0x10u)))continue;
        for(auto it=pending_[core].rbegin();it!=pending_[core].rend();++it){const auto& w=voice_[*it];if(w.type!=0)continue;if(w.prio<best_prio){best_prio=w.prio;best=*it;}}}
    if(best<0)return -1;auto& w=voice_[best];detach(best);w.state=0;w.id=255;w.type=255;return best;
}
bool IopSound::steal_port(unsigned port,unsigned prio){
    // 0x5e0c: priority <= the new one, released lists first, then keyed and pedal lists, channels 0..15, from the
    // +8 end (the last one found wins ties). The victim gets ADSR2 0xC021 at once, a key-off and is freed.
    auto& P=hsyn_[port];int best=-1;unsigned bp=prio;
    for(unsigned ch=0;ch<16;++ch)for(const int v:P.channel[ch].list[2])if(voice_[v].prio<=bp){bp=voice_[v].prio;best=v;}
    if(best<0)for(unsigned ch=0;ch<16;++ch)for(unsigned l=0;l<2;++l)for(const int v:P.channel[ch].list[l])if(voice_[v].prio<=bp){bp=voice_[v].prio;best=v;}
    if(best<0)return false;
    auto& w=voice_[best];koff_[w.core]|=1u<<w.index;hw_[best].env.adsr2=0xC021;free_voice(best);return true;
}
void IopSound::hs_note_on(unsigned port,unsigned ch,unsigned key,unsigned velocity,int id,const std::vector<std::shared_ptr<const PcmClip>>& clips){
    auto& P=hsyn_[port];auto& C=P.channel[ch];
    for(const auto& clip:clips){
        if(!clip||!clip->frames()||!clip->spu)continue;
        const unsigned prio=clip->priority+P.prio;
        if(P.count>=P.limit&&!steal_port(port,prio))return; // 0x6250: no voice in the port ends the note-on (0x64c8)
        int v=alloc_free(clip->spu_attr);
        if(v<0&&gc_flag_){gc_flag_=false;collect(true);v=alloc_free(clip->spu_attr);} // 0x6278..0x62b0
        if(v<0)v=steal_active(prio,clip->spu_attr);
        if(v<0)v=steal_pending(prio,clip->spu_attr);
        if(v<0)continue;
        auto& w=voice_[v];
        w.state=0x208;w.port=int(port);w.channel=int(ch);w.key=key;w.velocity=velocity;w.id=id;w.prio=std::uint16_t(prio);w.envx=0;w.type=0;w.clip=clip;w.c75=0;w.pan=C.pan;w.bend=C.bend;
        ++P.count;
        C.list[0].push_front(v);w.chlist=0;       // 0x63d0 (0x3264)
        pending_[w.core].push_front(v);w.list=1;  // 0x6418 (0x320c)
        peak_[w.core]=std::max(peak_[w.core],allocated(w.core));
    }
}
void IopSound::hs_note_off(unsigned port,unsigned ch,unsigned key,int id){
    // 0x5694: keyed list from its +12 end; pedal -> pedal list, never keyed -> freed, else key-off and released list.
    auto& C=hsyn_[port].channel[ch];auto& a=C.list[0];
    for(std::size_t i=a.size();i-->0;){const int v=a[i];auto& w=voice_[v];if(w.key!=key||!(w.id==id||id==255))continue;
        a.erase(a.begin()+std::ptrdiff_t(i));w.chlist=-1;w.state&=~0x300;
        if(C.pedal){w.state|=0x100;C.list[1].push_front(v);w.chlist=1;}
        else if(w.state&8)cancel_voice(v);
        else{koff_[w.core]|=1u<<w.index;C.list[2].push_front(v);w.chlist=2;}}
}
void IopSound::hs_all_sound_off(unsigned port,unsigned ch){
    // 0x5bf4: every voice of the three lists, keyed or not: key-off, ADSR2 0xC021 at once, freed.
    auto& C=hsyn_[port].channel[ch];
    for(unsigned l=0;l<3;++l)while(!C.list[l].empty()){const int v=C.list[l].back();auto& w=voice_[v];koff_[w.core]|=1u<<w.index;hw_[v].env.adsr2=0xC021;free_voice(v);}
}
void IopSound::hs_all_notes_off(unsigned port,unsigned ch){
    // 0x5ab4: keyed and pedal lists from the +12 end; never keyed -> freed, else key-off into the released list.
    auto& C=hsyn_[port].channel[ch];
    for(unsigned l=0;l<2;++l)while(!C.list[l].empty()){const int v=C.list[l].back();auto& w=voice_[v];C.list[l].pop_back();w.chlist=-1;
        if(w.state&8){cancel_voice(v);continue;}
        w.state&=~0x300;koff_[w.core]|=1u<<w.index;C.list[2].push_front(v);w.chlist=2;}
}
void IopSound::hs_pedal(unsigned port,unsigned ch,unsigned value){
    // 0x5974: on stores the value; off after on releases the pedal list from its +8 end.
    auto& C=hsyn_[port].channel[ch];if(value){C.pedal=true;return;}if(!C.pedal)return;C.pedal=false;
    while(!C.list[1].empty()){const int v=C.list[1].front();auto& w=voice_[v];C.list[1].pop_front();w.chlist=-1;
        if(w.state&8){cancel_voice(v);continue;}
        koff_[w.core]|=1u<<w.index;C.list[2].push_front(v);w.chlist=2;w.state&=~0x300;}
}
void IopSound::hs_pan(unsigned port,unsigned ch,unsigned pan){
    // 0x0ce4 for id 127 / any key: every voice of the channel takes pan - 64 (below -63: -63); the channel CC10 stays.
    const int offset=std::max(int(pan)-64,-63);
    for(auto& l:hsyn_[port].channel[ch].list)for(const int v:l)voice_[v].pan=offset;
}
void IopSound::hs_voice_bend(unsigned port,unsigned ch,int offset){
    // 0x0f1c for id 127 / any key: every voice of the channel takes the bend offset (0x4098); channel +38 stays.
    for(auto& l:hsyn_[port].channel[ch].list)for(const int v:l)voice_[v].bend=offset;
}
void IopSound::hs_control(unsigned port,unsigned ch,unsigned cc,unsigned value){
    auto& C=hsyn_[port].channel[ch];
    switch(cc){
    case 7:C.cc7=std::uint8_t(sony_controller_scale(value));break;   // 0x978
    case 11:C.cc11=std::uint8_t(sony_controller_scale(value));break; // 0x9d4
    case 10:{const int offset=std::max(int(value)-64,-63); // 0x0c18: only a change moves the voices
        if(offset!=C.pan){C.pan=offset;for(auto& l:C.list)for(const int v:l)voice_[v].pan=offset;}break;}
    case 64:hs_pedal(port,ch,value);break;
    case 120:hs_all_sound_off(port,ch);break;
    case 123:hs_all_notes_off(port,ch);break;
    case 99:C.nrpn_msb=std::uint8_t(value);C.nrpn_lsb=255;break; // 0x11d4
    case 98:C.nrpn_lsb=std::uint8_t(value);break;                // 0x11e8
    case 6: // 0x11f0: NRPN 2/x and 3/x act; every data entry resets the NRPN state
        if(C.nrpn_lsb!=255&&(C.nrpn_msb==2||C.nrpn_msb==3))unavailable.push_back("MODHSYN NRPN "+std::to_string(C.nrpn_msb)+"/"+std::to_string(C.nrpn_lsb)+" not ported");
        C.nrpn_msb=C.nrpn_lsb=255;break;
    case 0:case 1:case 2:case 5:case 65:case 84:case 121:
        unavailable.push_back("MODHSYN CC"+std::to_string(cc)+" not ported");break;
    default:break; // other controllers have no handler in the 0xa470 table
    }
}
void IopSound::hs_message(unsigned port,const Message& m){
    auto& C=hsyn_[port].channel[m.channel];
    switch(m.kind){
    case Message::NoteOn:if(!m.b){hs_note_off(port,m.channel,m.a,255);break;}hs_note_on(port,m.channel,m.a,m.b,m.id,C.program<0?std::vector<std::shared_ptr<const PcmClip>>{}:resolve(int(port),unsigned(C.program),m.a));break;
    case Message::ClipOn:hs_note_on(port,m.channel,m.a,m.b,m.id,{m.clip});break;
    case Message::NoteOff:hs_note_off(port,m.channel,m.a,m.id);break;
    case Message::Control:hs_control(port,m.channel,m.a,m.b);break;
    case Message::Program:C.program=int(m.a);break; // 0x75c; an invalid program keys no voice (decode yields none)
    case Message::Bend:{const int offset=int((m.b<<7)|m.a)-8192; // 0x7f8: only a change moves the voices
        if(offset!=C.bend){C.bend=offset;for(auto& l:C.list)for(const int v:l)voice_[v].bend=offset;}break;}
    case Message::Pan:hs_pan(port,m.channel,m.a);break;
    case Message::VoiceBend:hs_voice_bend(port,m.channel,int((m.b<<7)+m.a)-8192);break;
    }
}
SonyLevels IopSound::levels(const Voice& w)const{
    const auto& C=hsyn_[w.port].channel[w.channel];
    return sony_voice_levels_offset(w.clip->sony_volume,w.velocity,C.cc7,C.cc11,int(w.clip->pan),w.pan);
}
void IopSound::update_hw(int v){
    auto& w=voice_[v];auto& h=hw_[v];if(!w.clip)return;
    const auto lv=levels(w);h.voll=lv.left;h.volr=lv.right;
    const auto& clip=*w.clip;h.advance=double(clip.rate)/48000;
    if(clip.sony_pitch){const auto& p=*clip.sony_pitch;const int fine=p.fine+w.bend*int(w.bend<0?p.bend_down:p.bend_up)/8192;
        h.advance=double(sony_sample_pitch(clip.rate,std::uint16_t(p.root),p.note,fine))/4096;}
}
void IopSound::collect(bool snapshot){
    // 0x4a8c (snapshot: ENVX into +18) and the first part of the voice tick 0x47d8: ENVX/ENDX per active voice.
    for(unsigned core=0;core<cores;++core)for(std::size_t i=active_[core].size();i-->0;){
        const int v=active_[core][i];auto& w=voice_[v];const auto& h=hw_[v];
        const auto envx=std::uint16_t(std::clamp(h.env.level,0,0x7fff));const bool endx=h.endx;
        if(snapshot)w.envx=envx;
        bool release=false;
        if(w.state&0x300){
            if(w.c75){w.c75=endx?std::uint8_t(w.c75-1):std::uint8_t(0);if(snapshot)w.envx=0x7fff;continue;}
            release=endx&&envx==0;
        }else release=envx==0;
        if(release)free_voice(v);
    }
}
void IopSound::key_batch(){
    // 0x32c4: KOFF masks of both cores, then per core the pending list from its +4 end: ENVX < 1000 -> key-on
    // setup (0x36b4) and onto the active list (0x320c), then the KON mask.
    for(unsigned core=0;core<cores;++core){if(koff_[core])for(unsigned i=0;i<core_voices;++i)if(koff_[core]&(1u<<i))hw_[core*core_voices+i].env.key_off();koff_[core]=0;}
    for(unsigned core=0;core<cores;++core){
        for(std::size_t i=pending_[core].size();i-->0;){const int v=pending_[core][i];auto& w=voice_[v];auto& h=hw_[v];
            if(h.env.level>=key_on_envx)continue;
            h.clip=w.clip;h.position=0;h.endx=false;h.ended=false;h.env.key_on(w.clip->adsr1,w.clip->adsr2); // KON clears ENDX
            w.pan=hsyn_[w.port].channel[w.channel].pan;   // 0x39a8: +86 has bit 7 -> channel CC10
            w.bend=hsyn_[w.port].channel[w.channel].bend; // 0x3a84: voice +40 has bit 15 -> channel bend
            w.c75=3;w.state&=~8u;
            update_hw(v);
            pending_[core].erase(pending_[core].begin()+std::ptrdiff_t(i));active_[core].push_front(v);w.list=2;
            kon_[core]|=1u<<w.index;
        }
        kon_[core]=0;
    }
}
void IopSound::voice_tick(){
    // 0x47d8: with the collection flag set the ENVX/ENDX pass of every active voice, then pitch and VOLL/VOLR.
    if(gc_flag_)collect(false);
    for(unsigned core=0;core<cores;++core)for(const int v:active_[core])update_hw(v);
    gc_flag_=true;
}
void IopSound::atick(){
    ++aticks_;
    tq_atick();
    for(unsigned p=0;p<bgm_ports;++p)if(seq_[p].playing)midi_tick(p);
    // sceHSyn_ATick (0x1798): input ports 0..15 in order (0..9 MODMIDI ports, 10..15 TQ ports), each FIFO.
    for(unsigned p=0;p<hsyn_ports;++p){auto queue=std::move(hsyn_[p].queue);hsyn_[p].queue.clear();for(const auto& m:queue)hs_message(p,m);}
    key_batch();
    voice_tick();
    bgm_atick();
    // End of the ATick: the event flag wakes a DelayATick in the TsndFunc thread.
    if(wait_&&!--wait_)process();
}

// ---------------------------------------------------------------------------------------------------- SPU2
void IopSound::mix(float& left,float& right,std::array<std::array<float,2>,2>& wet){
    for(unsigned v=0;v<voice_count;++v){auto& h=hw_[v];if(!h.clip||h.ended)continue;const auto& clip=*h.clip;
        // ENVX * sample >> 15 precedes the voice volume; a silent voice still advances (ENDX for MODHSYN).
        float envelope=0;if(h.env.tick())envelope=float(h.env.level)/32768.f;
        const bool loop=clip.loop_end>clip.loop_start;
        const auto end=loop?clip.loop_end:clip.frames();
        if(h.position>=end){
            if(loop){h.endx=true;const auto begin=clip.loop_start;h.position=begin+std::fmod(h.position-end,double(end-begin));}
            else{h.endx=true;h.ended=true;h.env.phase=SpuEnvelope::Stopped;h.env.level=0;continue;} // end without repeat: muted
        }
        if(envelope!=0){
            const auto i=std::size_t(h.position),next=loop&&i+1>=end?clip.loop_start:std::min(i+1,clip.frames()-1);const float fraction=float(h.position-double(i));
            auto sample=[&](unsigned ch){const float a=clip.samples[i*clip.channels+ch],b=clip.samples[next*clip.channels+ch];return (a+(b-a)*fraction)*envelope;};
            const float sl=sample(0)*float(h.voll)/16384.f,sr=sample(clip.channels==1?0:1)*float(h.volr)/16384.f;
            const unsigned attr=clip.spu_attr,core=v/core_voices;
            if(attr&1)left+=sl;
            if(attr&2)right+=sr;
            if(attr&4)wet[core][0]+=sl;
            if(attr&8)wet[core][1]+=sr;
        }
        h.position+=h.advance;
    }
}

// ---------------------------------------------------------------------------------------------------- diagnostics
void IopSound::diagnostic_note(unsigned port,unsigned channel,std::shared_ptr<const PcmClip> clip,unsigned velocity){
    Message m;m.kind=Message::ClipOn;m.channel=channel&15;m.a=60;m.b=velocity;m.clip=std::move(clip);hsyn_.at(port).queue.push_back(m);
}
void IopSound::diagnostic_stop(unsigned port,unsigned channel){Message m;m.kind=Message::Control;m.channel=channel&15;m.a=120;hsyn_.at(port).queue.push_back(m);}
bool IopSound::diagnostic_busy(unsigned port,unsigned channel)const{
    for(const auto& m:hsyn_.at(port).queue)if(m.channel==(channel&15)&&m.kind==Message::ClipOn)return true;
    for(const auto& l:hsyn_[port].channel[channel&15].list)if(!l.empty())return true;return false;
}
std::vector<IopSound::VoiceView> IopSound::voices(int port,int channel)const{
    std::vector<VoiceView> out;
    for(unsigned core=0;core<cores;++core)for(const auto* list:{&pending_[core],&active_[core]})for(auto it=list->rbegin();it!=list->rend();++it){
        const auto& w=voice_[*it];if((port>=0&&w.port!=port)||(channel>=0&&w.channel!=channel))continue;
        VoiceView view;view.core=w.core;view.index=w.index;view.state=w.state;view.port=unsigned(w.port);view.channel=unsigned(w.channel);view.key=w.key;view.velocity=w.velocity;
        view.pending=list==&pending_[core];const auto& h=hw_[*it];view.sounding=!view.pending&&h.env.sounding();view.envx=std::uint16_t(std::clamp(h.env.level,0,0x7fff));
        view.levels=view.pending?SonyLevels{}:SonyLevels{h.voll,h.volr};view.clip=w.clip;out.push_back(view);}
    return out;
}
unsigned IopSound::sounding(unsigned core)const{unsigned n=0;for(unsigned i=0;i<core_voices;++i){const auto& h=hw_[core*core_voices+i];if(h.clip&&!h.ended&&h.env.sounding())++n;}return n;}
unsigned IopSound::allocated(unsigned core)const{return unsigned(core_voices-free_[core].size());}
}
