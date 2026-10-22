#pragma once
// IOP-Seite der PS2-Klangausgabe: TSNDDRV.IRX (Befehlsthread, SE-Warteschlange "TQ", BGM-Ports), MODMIDI.IRX
// (Sequencer), MODHSYN.IRX (16 Ports x 16 Kanaele, 2 x 24 Stimmen) und die 48 SPU2-Stimmen. Nachgebaut aus der
// eigenen Disassemblierung der Module (nur Semantikreferenz, siehe docs/AUDIO.md "IOP-Takt und Stimmenpool");
// nichts davon wird geladen oder ausgefuehrt.
//
// Zeitbasis: TSNDDRV-ATick 240 Hz (Hardware-Timer 0x25800 bei 36,864 MHz = 200 Ausgabebilder bei 48 kHz). Ein
// ATick laeuft TsndTQ_ATick -> sceMidi_ATick -> sceHSyn_ATick (Nachrichten der Ports 0..15, Key-Off/Key-On-Stapel,
// Stimmentick) -> BGM-Nacharbeit (Portlautstaerke, HsMsg-Pan). Der Befehlsthread (TsndFunc) arbeitet einen vom EE
// gesendeten Stapel der Reihe nach ab und wartet bei DelayATick(n) auf das Ende von n ATicks.
#include "audio_clip.h"
#include "audio_spu.h"
#include "audio_pan.h"
#include "audio_sq.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
namespace recvx {
using AudioBank=std::shared_ptr<const std::vector<std::uint8_t>>;

// One EE sndque entry (ps2_snddrv.c). vol/pan/pitch < 0: the CheckCmdReq/Chg bit of that field is clear.
struct IopCommand {
    enum Kind:std::uint8_t {SeReq,SeChg,SeCancel,SeAllStop,BgmReq,BgmStop,BgmChg,Rev,SetIopData,OutputMode};
    Kind kind=SetIopData;
    unsigned port=0,channel=0,program=0;
    int vol=-1,pan=-1,pitch=-1;
    unsigned core=0,mode=0;int depth=0;
};

class IopSound {
public:
    static constexpr unsigned cores=2,core_voices=24,voice_count=48,hsyn_ports=16,tq_ports=6,bgm_ports=10;
    IopSound();
    // treq_LOAD of SE bank p (TQ port p, MODHSYN port 10+p): TSNDDRV rebuilds its per-program SE table (0x6600..0x6bc4).
    void load_se_bank(unsigned bank,AudioBank data);
    // MIDI bank b: programs (HD/BD) and sequences (SQ). BgmReq attaches them to a BGM port (sceHSyn_Load, snd_load_sqinit).
    void load_midi_bank(unsigned bank,AudioBank programs,AudioBank sequences);
    void push(const IopCommand& command){queue_.push_back(command);}
    void send();       // SdrSendReq: the queued commands reach the TsndFunc thread, which starts at once
    void atick();      // one TSNDDRV ATick; the RPC thread resumes afterwards when its DelayATick ends
    // One 48 kHz output frame of the 48 SPU2 voices: dry sum (VMIXL/VMIXR) and effect input per core (VMIXEL/VMIXER).
    void mix(float& left,float& right,std::array<std::array<float,2>,2>& wet);
    // SdrSetIopData snapshot (0x2F), read by the EE in get_iopsnd_info: TQ busy masks and BGM ports in use.
    struct Status {std::array<std::uint16_t,tq_ports> se{};std::uint16_t midi=0;};
    const Status& status()const{return status_;}
    // Diagnostic note on MODHSYN port 15 channel `channel` with an explicit clip (synthetic test clips, not a game path).
    // Synthetic SE table entry (tests): program `program` of TQ bank `bank` plays `clip` through the normal TQ/MODHSYN path.
    void load_se_clip(unsigned bank,unsigned program,std::shared_ptr<const PcmClip> clip);
    void diagnostic_note(unsigned port,unsigned channel,std::shared_ptr<const PcmClip> clip,unsigned velocity=127);
    void diagnostic_stop(unsigned port,unsigned channel); // CC 120 on that channel
    bool diagnostic_busy(unsigned port,unsigned channel)const;
    std::function<void(unsigned core_mask,unsigned mode,int depth)> on_rev;
    std::vector<std::string> unavailable;
    // --- diagnostics ---
    struct VoiceView {unsigned core=0,index=0,state=0,port=0,channel=0,key=0,velocity=0;bool pending=false,sounding=false;std::uint16_t envx=0;SonyLevels levels;std::shared_ptr<const PcmClip> clip;};
    std::vector<VoiceView> voices(int port=-1,int channel=-1)const; // allocated MODHSYN voices (pending and active)
    unsigned sounding(unsigned core)const;        // SPU voices with a running envelope
    unsigned allocated(unsigned core)const;       // MODHSYN voices not on the free list
    std::array<unsigned,cores> allocation_peak()const{return peak_;}
    std::uint64_t aticks()const{return aticks_;}
    int channel_bend(unsigned port,unsigned channel)const{return hsyn_.at(port).channel.at(channel).bend;}
    unsigned sequence_cc7(unsigned port,unsigned channel)const{return seq_.at(port).seq_cc7.at(channel);}
    unsigned midi_port_volume(unsigned port)const{return seq_.at(port).volume;}
    bool sequence_playing(unsigned port)const{return seq_.at(port).playing;}
    std::uint32_t sequence_tick(unsigned port)const{return seq_.at(port).tick;}
    unsigned tq_timer(unsigned port,unsigned channel)const{return tq_.at(port).entry.at(channel).timer;}
    struct SeInfo {std::uint32_t offset=0,length=0xffff;std::uint16_t rate=0;std::uint8_t attr=0,note=0;std::shared_ptr<const PcmClip> clip;}; // clip: synthetic test entry
    const std::vector<SeInfo>& se_table(unsigned port)const{return tq_.at(port).table;}
    bool rpc_waiting()const{return wait_>0;}
    bool mono()const{return mono_;}
private:
    // ---- SPU2 ----
    struct Hw {std::shared_ptr<const PcmClip> clip;double position=0,advance=1;SpuEnvelope env;bool endx=false,ended=false;std::uint16_t voll=0,volr=0;};
    std::array<Hw,voice_count> hw_{};
    // ---- MODHSYN ----
    struct Voice {unsigned core=0,index=0;std::uint16_t state=0,envx=0,prio=0;std::uint8_t type=255,c75=0;int port=-1,channel=-1,id=255,pan=0,bend=0,list=0,chlist=-1;unsigned key=0,velocity=0;bool mono_pan=false;std::shared_ptr<const PcmClip> clip;};
    bool mono_=false; // MODHSYN 0x1f88, consumed when 0x4534 recalculates a voice's pan factors
    struct Channel {std::uint8_t cc7=128,cc11=128,nrpn_msb=255,nrpn_lsb=255;int pan=0,bend=0,program=-1;bool pedal=false;std::array<std::deque<int>,3> list;};
    struct Message {enum Kind:std::uint8_t {NoteOn,NoteOff,Control,Program,Bend,Pan,VoiceBend,ClipOn} kind=NoteOn;unsigned channel=0,a=0,b=0;int id=255;std::shared_ptr<const PcmClip> clip;};
    struct Port {std::array<Channel,16> channel;unsigned count=0,limit=48,prio=0,master=256;int bank=-1;bool se=false;std::vector<Message> queue;};
    std::array<Voice,voice_count> voice_{};
    std::array<Port,hsyn_ports> hsyn_{};
    std::array<std::deque<int>,cores> free_,pending_,active_; // front = list head +0 side (0x320c inserts there)
    std::array<std::uint32_t,cores> kon_{},koff_{};
    bool gc_flag_=false; // aa24: set by every voice tick, consumed by the first note-on that finds no free voice
    std::array<unsigned,cores> peak_{};
    void hs_message(unsigned port,const Message& m);
    void hs_note_on(unsigned port,unsigned channel,unsigned key,unsigned velocity,int id,const std::vector<std::shared_ptr<const PcmClip>>& clips);
    void hs_note_off(unsigned port,unsigned channel,unsigned key,int id);
    void hs_control(unsigned port,unsigned channel,unsigned control,unsigned value);
    void hs_all_sound_off(unsigned port,unsigned channel); // CC 120 (0x5bf4)
    void hs_all_notes_off(unsigned port,unsigned channel); // CC 123 (0x5ab4)
    void hs_pedal(unsigned port,unsigned channel,unsigned value); // CC 64 (0x5974)
    void hs_pan(unsigned port,unsigned channel,unsigned pan); // HsMsg 0xfd/1 (0x0ce4)
    void hs_voice_bend(unsigned port,unsigned channel,int offset); // HsMsg 0xfd/2 (0x0f1c)
    int alloc_free(std::uint8_t attr);
    int steal_active(unsigned prio,std::uint8_t attr);
    int steal_pending(unsigned prio,std::uint8_t attr);
    bool steal_port(unsigned port,unsigned prio);
    void free_voice(int v);    // 0x5cd4
    void cancel_voice(int v);  // 0x5840
    void detach(int v);        // remove from channel and core list
    void collect(bool snapshot); // 0x4a8c (snapshot = +18) / tick part of 0x47d8
    void key_batch();          // 0x32c4
    void voice_tick();         // 0x47d8
    void update_hw(int v);     // 0x3ef8 pitch and 0x40f0 VOLL/VOLR of an active voice
    SonyLevels levels(const Voice& v)const;
    std::vector<std::shared_ptr<const PcmClip>> resolve(int port,unsigned program,unsigned key,unsigned velocity);
    std::map<std::tuple<const void*,unsigned,unsigned,unsigned>,std::vector<std::shared_ptr<const PcmClip>>> clip_cache_;
    AudioBank port_bank(int port)const;
    // ---- MODMIDI ----
    struct Seq {
        bool playing=false,selected=false;std::vector<SonySqEvent> events;std::size_t next=0;
        std::uint32_t tick=0,next_tick=0,frac=0,step=0,ppqn=480,tempo=500000,speed=256;
        std::array<std::uint8_t,16> seq_cc7{};unsigned volume=128;std::uint16_t used=0;
        std::uint8_t nrpn_msb=255,data=255;
        struct Loop {std::uint8_t id=255,count=255;std::uint32_t tick=0;std::size_t next=0;};
        std::array<Loop,8> loops{};int sq_bank=-1;unsigned list=0;
    };
    std::array<Seq,bgm_ports> seq_{};
    void midi_tick(unsigned port);
    bool midi_events(unsigned port);   // 0xf78: false at end of track
    void midi_send(unsigned port,const Message& m); // 0xda4: also marks the channel used
    void midi_location(unsigned port); // sceMidi_MidiSetLocation(port, 0) / reset 0x1994
    void midi_volume(unsigned port,unsigned volume); // sceMidi_MidiSetVolume(port, 255, volume)
    void midi_switch(unsigned port,bool on); // sceMidi_MidiPlaySwitch
    void midi_step(Seq& s);            // 0x948
    // ---- TSNDDRV ----
    struct TqEntry {std::uint32_t flags=0;std::uint16_t timer=0,pitch=8192,def_bend=8192,note=0;std::uint8_t program=0,channel=0,vol=127,pan=64,def_vol=127,def_pan=64;};
    struct TqPort {std::array<TqEntry,16> entry{};std::uint16_t active=0;bool loaded=false;std::vector<SeInfo> table;};
    std::array<TqPort,tq_ports> tq_{};
    struct Bgm {std::uint8_t flags=0,vol=0,pan=64,bend_msb=32,bend_lsb=0;int hsyn_bank=-1;};
    std::array<Bgm,bgm_ports> bgm_{};
    std::uint16_t midi_info_=0;
    std::map<unsigned,AudioBank> se_banks_,midi_programs_,midi_sequences_;
    void tq_atick();
    void bgm_atick();
    void tq_request(const IopCommand& c);
    void tq_change(const IopCommand& c);
    void tq_cancel(const IopCommand& c);
    // RPC thread
    std::deque<IopCommand> queue_,rpc_;
    unsigned wait_=0,stage_=0;
    void process();
    bool run(const IopCommand& c); // false: waiting on DelayATick
    Status status_;
    std::uint64_t aticks_=0;
};
}
