#pragma once
#include <array>
#include "timing.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <tuple>
#include <vector>
namespace recvx {
inline constexpr unsigned audio_voice_count=30; // 24..26: CallBackGroundSe slots 0..2, 27..29: CallDoorSe
inline constexpr unsigned door_voice_first=27,door_voice_last=29;
// volume/pan: original Sony HD program (+6/+7) and split (+0x10/+0x11) values,
// 0..127 each; ADX clips keep 127/64 (unity, centre).
struct PcmClip {unsigned rate=0,channels=0;std::size_t loop_start=0,loop_end=0;std::vector<std::int16_t> samples;float volume=1;unsigned pan=64;std::size_t frames()const{return channels?samples.size()/channels:0;}};
PcmClip decode_ps_adpcm(const std::vector<std::uint8_t>& bytes,unsigned rate);
// key<0 selects the first split; otherwise the split whose key range holds `key`.
PcmClip decode_system_sound(const std::vector<std::uint8_t>& bank,unsigned program,int key=-1);
PcmClip decode_adx(const std::vector<std::uint8_t>& bytes);
class AudioMixer {
public:
    explicit AudioMixer(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    // MountSoundAfs: sys->ss_flg bit 0 selects the disc-1 or disc-2 sound archive set.
    void set_disc(unsigned disc){disc_=disc&1;}
    // system.c:1602 — before a room loads, bhSysCallMonitor hands over to the
    // disc change task whenever the mounted set does not match the stage
    // (`!(ss_flg & 1) && stg_no >= 6` or `(ss_flg & 1) && stg_no < 6`).
    // bhSysCallDiscChange -> Adv_ChangeDiscScreen (adv.c:3715) flips bit 0 and
    // MountSoundAfs (sdfunc.c:362) swaps the digit in BGM?.AFS, VOICE?.AFS,
    // MULTSPQ?.AFS and ITEM?.AFS. ss_flg 0x400000, the only suppression, is
    // never set in the original sources, so the mounted set always matches the
    // stage by the time the room's scripts request a BGM.
    static unsigned original_disc_for_stage(unsigned stage){return stage>=6?1u:0u;}
    void mount_sound_afs(unsigned stage){set_disc(original_disc_for_stage(stage));}
    unsigned disc()const{return disc_;}
    std::string disc_digit()const{return disc_?"2":"1";}
    std::string set_name(const std::string& base)const{return base+disc_digit();}
    void play(unsigned slot,const std::string& archive,unsigned id,float volume=1,bool loop=false,unsigned fade_ticks=0);
    void play_pcm(unsigned slot,std::shared_ptr<PcmClip> clip,float volume=1,bool loop=false);
    void play_system(unsigned program);
    bool play_weapon_sound(unsigned weapon_bank,unsigned program);
    void load_room_sounds(unsigned stage,unsigned room,unsigned variant=0);
    // RequestPlayerVoiceSoundBank: CORE key player|0xfff0; the pack supplies SE bank 4.
    bool load_player_sounds(unsigned player);
    // sdfunc.c Set/GetRoomSoundCaseNo: consumed once by the next room bank request.
    void set_room_sound_case(unsigned variant){room_sound_case_=variant;}
    unsigned take_room_sound_case(){const auto value=room_sound_case_;room_sound_case_=0;return value;}
    bool play_room_sound(unsigned bank,unsigned program,unsigned slot=4,bool loop=false);
    // CallBackGroundSeEx/StopBackGroundSeEx: bank 3, original slots 0..2 (looping ambience).
    bool background_sound(unsigned slot,unsigned sound,unsigned timer=0);
    void begin_room_init(){room_initializing_=true;custom_midi_slots(1,4);} // Com_StartInitScript
    void request_background_sound(unsigned slot,int sound);
    // ExPlayMidi: Sony SQ transport, tempo/PPQN, running status, shortened
    // note-offs and NRPN loops. Notes resolve through the original program bank.
    // Pitch/ADSR/reverb are still incomplete; this is not a bit-exact IOP synth.
    // CallSystemSe with bit 31 (ExPlayMidi, MIDI slot 2): bank (SeNo/256)&15, list is a char (low byte).
    bool play_system_midi(unsigned se_number);
    bool play_room_sequence(unsigned bank,unsigned list,unsigned slot,bool loop=false);
    void custom_midi_slots(unsigned objects,unsigned events);
    bool event_sound(unsigned slot,unsigned sound);
    bool stop_event_sound(unsigned slot);
    std::vector<std::string> sequence_unavailable;
    void stop_background_sound(unsigned slot,unsigned fade_ticks=0);
    bool user_sound_volume(unsigned type,unsigned slot,int start,int target,unsigned frames);
    bool enemy_sound(unsigned slot,unsigned sound);
    void stop_enemy_sound(unsigned slot);
    // sdfunc.c RequestEnemySeBasic / ChechPlayEnemySe / ExecEnemySeManager over the six enemy voices (18..23).
    void request_enemy_se(unsigned enemy,int se_no,int fade_rate=0);
    bool enemy_se_playing(unsigned enemy,int se_no)const;
    // sdfunc.c CallPlayerVoice: slot 7, bank (SeNo/256)&0xF, list SeNo. Undecodable programs are reported, never fatal.
    void play_player_voice(int se_no);
    void exec_enemy_se();
    bool footstep(unsigned floor,unsigned type,unsigned actor,bool enabled=true);
    void load_door_sounds(unsigned bank);
    bool play_door_sound(unsigned program);
    void finish_door_sound(); // original MIDI slot 2: fade after the destination is visible
    void stop(unsigned slot,unsigned fade_ticks=0);
    bool active(unsigned slot)const;
    std::vector<std::int16_t> mix(unsigned frames=audio_frames_per_tick);
    // Original BGM control (sdfunc.c PlayBgmEx/PlayBgm2/StopBgm/StopBgm2,
    // Com_ExecRoomFadeOut/In; adxwrap.c volume and fade units) on ADX slot 0.
    // The request state persists across rooms like the original globals.
    void play_bgm_ex(int bgm,int fade_in_rate,int volume);
    void request_bgm2(int bgm,int volume);
    void stop_bgm(int fade_out_rate);
    void stop_bgm2();
    void room_fade_out(); // sound command 2 at door start
    void room_fade_in();  // sound command 1 once the new room is running
    int current_bgm()const{return current_bgm_;}
    int current_bgm_volume()const{return current_bgm_volume_;}
    int bgm_request()const{return req_fade_bgm_;}
    static float adx_gain(float volume);          // AdxVolTbl, 0.1 dB steps
    static unsigned adx_fade_ticks(int timer);    // RequestAdxFadeFunction2 count
    std::vector<std::string> played,unavailable;
private:
    unsigned disc_=0;
    // A voice slot plays one or more layers (simultaneous SQ notes); fades act on the slot.
    struct Layer {std::shared_ptr<PcmClip> clip;double position=0;float left=1,right=1;};
    struct SequenceNote {Layer layer;unsigned channel=0,key=0;};
    struct SequenceEvent {double delay=0;unsigned status=0,key=0,value=0;Layer layer;};
    struct SequenceLoop {unsigned number=0;std::size_t next=0;unsigned remaining=0;bool counted=false;};
    struct Sequence {std::vector<SequenceEvent> events;std::vector<SequenceNote> notes;std::vector<SequenceLoop> loops;std::size_t next=0;double until=0;unsigned nrpn=0;};
    struct Channel {std::shared_ptr<PcmClip> clip;double position=0;float volume=1,gain=1,target=1;unsigned fade=0;bool loop=false;float left=1,right=1;std::vector<Layer> layers;std::shared_ptr<Sequence> sequence;};
    void sequence_step(Channel& channel);
    void play_layers(unsigned slot,std::vector<Layer> layers,float volume,bool loop);
    // Native shot/event voices are separate from CRI movie/ADX channels.
    std::array<Channel,audio_voice_count> slots_{};
    std::map<unsigned,std::shared_ptr<PcmClip>> system_sounds_;
    unsigned effect_slot_=2;
    unsigned max_object_slots_=1,max_event_slots_=4;
    int current_bgm_=-1,current_bgm_volume_=-127,req_fade_bgm_=0;
    void fade_gain(unsigned slot,float gain,unsigned ticks);
    std::map<unsigned,std::vector<std::uint8_t>> room_banks_,midi_programs_,midi_sequences_; // type 2 SE / type 1 program / type 0 SQ
    std::map<std::tuple<unsigned,unsigned,int>,std::shared_ptr<PcmClip>> room_sounds_; // bank (0x100|bank: MIDI), program, key
    unsigned room_pack_=0;
    std::map<unsigned,unsigned> sound_bank_packs_; // provenance of independently replaced SE banks
    unsigned room_sound_case_=0;
    bool room_initializing_=false;
    std::array<int,3> current_background_{{-1,-1,-1}};
    std::array<unsigned,3> request_background_{};
    struct EnemyRequest {bool req=false,req_v=false;int se=0,se_v=0,fade=0;int prio=3;};
    struct EnemyVoice {bool flag=false;unsigned enemy=0;int se=0,attrib=0,prio=0;};
    std::map<unsigned,EnemyRequest> enemy_requests_;std::array<EnemyVoice,6> enemy_voices_{};
    struct BackgroundRequest {bool active=false;unsigned sound=0,timer=0;};
    std::array<BackgroundRequest,3> background_init_{};
    std::array<bool,3> foot_switch_{};
    unsigned door_bank_=0,door_slot_=door_voice_first;
    std::vector<std::uint8_t> door_source_;
    std::map<unsigned,std::vector<std::uint8_t>> weapon_banks_;
    std::map<std::pair<unsigned,unsigned>,std::shared_ptr<PcmClip>> weapon_sounds_;
    std::filesystem::path data_,mods_;
};
}
