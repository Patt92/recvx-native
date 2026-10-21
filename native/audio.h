#pragma once
#include <array>
#include "timing.h"
#include "audio_spu.h"
#include "audio_pan.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <map>
#include <string>
#include <tuple>
#include <vector>
namespace recvx {
inline constexpr unsigned audio_voice_count=33; // 24..26: CallBackGroundSe slots 0..2, 27..29: CallDoorSe, 30..32: SE slots 8/9/19
inline constexpr unsigned door_voice_first=27,door_voice_last=29;
// sdfunc.c CallPlayerWeaponSeEx uses SE slots 8/9 (SlotNo 0, alternating) and 19.
inline constexpr unsigned weapon_voice_8=30,weapon_voice_9=31,weapon_voice_19=32;
struct SoundPoint {float x=0,y=0,z=0;};
// Get3DSoundParameter outputs: RequestInfo.Pan/Volume are signed chars, Dist a float.
struct Sound3D {int pan=0,volume=0;float distance=0;};
// RM_SNDENV (sdfunc.h:6): the 16-byte type-5 record of a room sound pack.
struct RoomSoundEnv {std::int8_t player_voice=0,player_action=0,weapon=0,cartridge=0,enemy=0,bg0=0,bg1=0,object=0,event=0,fx_level=0,fx_prog=0,max_event_slots=0,max_object_slots=0;};
// volume/pan: original Sony HD program (+6/+7) and split (+0x10/+0x11) values,
// 0..127 each; ADX clips keep 127/64 (unity, centre).
struct SonyPitch {int root=60,note=60,fine=0;unsigned bend_down=0,bend_up=0;};
// spu: Sony-HD-Sample (MODHSYN-Stimme) mit ADSR1/ADSR2 (+0x12/+0x14) und sampleSpuAttr (+0x29).
struct PcmClip {unsigned rate=0,channels=0;std::optional<SonyPitch> sony_pitch;bool spu=false;std::uint16_t adsr1=0,adsr2=0;std::uint8_t spu_attr=0;std::size_t loop_start=0,loop_end=0;std::vector<std::int16_t> samples;float volume=1;unsigned pan=64;std::uint32_t sony_volume=0;std::size_t frames()const{return channels?samples.size()/channels:0;}};
PcmClip decode_ps_adpcm(const std::vector<std::uint8_t>& bytes,unsigned rate);
// key<0 selects the first split; otherwise the split whose key range holds `key`. For key>=0 (MODHSYN
// note-on) a missing/empty program or a key no split covers yields an empty clip: no voice, as in the driver.
// A key several splits cover throws: use decode_system_note.
PcmClip decode_system_sound(const std::vector<std::uint8_t>& bank,unsigned program,int key=-1);
// MODHSYN note-on (.text 0x5f90, split loop 0x6098..0x64a0): one clip per voice the note keys, i.e. per
// split whose key range holds `key`, in split-table order; empty when no voice starts.
std::vector<PcmClip> decode_system_note(const std::vector<std::uint8_t>& bank,unsigned program,int key);
PcmClip decode_adx(const std::vector<std::uint8_t>& bytes);
class AudioMixer {
public:
    explicit AudioMixer(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){init_spu();}
    // Hall-Zustand eines SPU2-Kerns und die gemeinsame CurrentFxLevel (EVOL beider Kerne).
    const SpuReverb& reverb(unsigned core)const{return reverb_.at(core);}
    int current_fx_level()const{return current_fx_level_;}
    // Anzahl klingender MODHSYN-Stimmen eines Kerns, einschliesslich ausklingender.
    unsigned spu_voices(unsigned core)const;
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
    // sdfunc.c ExecSoundSystemMonitor: CameraPos = cam.wpx/wpy/wpz, PlayerPos = plp->px/py/pz; cam.ay feeds wadGetAngle.
    void set_sound_monitor(SoundPoint camera,int camera_ay,SoundPoint player){camera_pos_=camera;camera_ay_=camera_ay;player_pos_=player;}
    // sdfunc.c:847 Get3DSoundParameter(&CameraPos, pPos, ..., Mode).
    Sound3D sound_parameter_3d(SoundPoint source,int mode)const;
    // sdfunc.c:940 Set3dSoundFlag / :1088 Reset3dSoundFlag (event 0x76 / 0x87).
    void set_3d_sound_flag(int type,int slot,unsigned flag);
    void reset_3d_sound_flag();
    unsigned se_3d_flag(unsigned se_slot)const{return se_slot<se_flags_.size()?se_flags_[se_slot]:0u;}
    const RoomSoundEnv& room_sound_env()const{return room_env_;}
    int room_fx_level()const{return current_room_fx_level_;}
    // sdfunc.c:1546 CallPlayerWeaponSeEx. weapon_bank is the WpnTab snd_wpno pack
    // that supplies SE bank 1; syukan is sys->gm_flg & 0x01000000.
    bool call_player_weapon_se(unsigned weapon_bank,int se_no,SoundPoint position,int slot_no,bool syukan);
    // adxwrap.c:650 GetAdxPlayTime(SlotNo): ((double)SampleCount / SamplingRate) * 100, also
    // Hundertstelsekunden ab Streambeginn. sdfunc.c:310 meldet genau zwei ADX-Stroeme an
    // (Platz 0 Musik, Platz 1 Stimme); fuer jeden anderen Platz hat das Original kein Handle.
    unsigned adx_play_time(unsigned slot)const;
    unsigned weapon_se_slot_switch()const{return weapon_slot_switch_;}
    void load_room_sounds(unsigned stage,unsigned room,unsigned variant=0);
    // RequestPlayerVoiceSoundBank: CORE key player|0xfff0; the pack supplies SE bank 4.
    bool load_player_sounds(unsigned player);
    // sdfunc.c Set/GetRoomSoundCaseNo: consumed once by the next room bank request.
    void set_room_sound_case(unsigned variant){room_sound_case_=variant;}
    unsigned take_room_sound_case(){const auto value=room_sound_case_;room_sound_case_=0;return value;}
    bool play_room_sound(unsigned bank,unsigned program,unsigned slot=4,bool loop=false);
    // CallBackGroundSeEx/StopBackGroundSeEx: bank 3, original slots 0..2 (looping ambience).
    bool background_sound(unsigned slot,unsigned sound,unsigned timer=0);
    // Com_StartInitScript: ResetEnemySeInfo (empty) and ResetObjectSeInfo (memset ObjectSlotInfo/ObjectInfo, MaxObjectReqList = 0).
    void begin_room_init(){room_initializing_=true;custom_midi_slots(1,4);object_slots_={};object_sounds_={};object_request_count_=0;}
    void request_background_sound(unsigned slot,int sound);
    // ExPlayMidi: Sony SQ transport, tempo/PPQN, running status, shortened
    // note-offs and NRPN loops. Notes resolve through the original program bank.
    // SPU2 ADSR and reverb follow MODHSYN/LIBSD (audio_spu.h); output levels are not a bit-exact IOP mix.
    // CallSystemSe with bit 31 (ExPlayMidi, MIDI slot 2): bank (SeNo/256)&15, list is a char (low byte).
    bool play_system_midi(unsigned se_number);
    // sdfunc.c:1400 StopSystemSe: StopMidi(2) (host slot 13).
    void stop_system_midi(){stop(13);}
    bool play_room_sequence(unsigned bank,unsigned list,unsigned slot,bool loop=false);
    void custom_midi_slots(unsigned objects,unsigned events);
    bool event_sound(unsigned slot,unsigned sound);
    bool stop_event_sound(unsigned slot);
    std::vector<std::string> sequence_unavailable;
    void stop_background_sound(unsigned slot,unsigned fade_ticks=0);
    bool user_sound_volume(unsigned type,unsigned slot,int start,int target,unsigned frames);
    bool enemy_sound(unsigned slot,unsigned sound);
    void stop_enemy_sound(unsigned slot);
    // sdfunc.c:1708 RequestEnemySeBasic(EnemyNo, pPos, SeNo, Flag, FadeRate): RequestEnemySe/Ex pass
    // Flag 1, zonzon1.c bhEne_SetSEPan Flag 0. ExecEnemySeManager serves the six enemy voices (18..23).
    void request_enemy_se(unsigned enemy,SoundPoint position,int se_no,int flag=1,int fade_rate=0);
    bool enemy_se_playing(unsigned enemy,int se_no)const;
    // sdfunc.c AllStopEnemySe (bhCheckSubTask, system.c:602/617/634): a registered enemy voice fades out
    // (RequestSeFadeFunction(DefEne[i], 2, 200)), a free one is stopped; EnemySlotInfo is cleared.
    // StopVibrationEx belongs to the caller's VibrationUnit.
    void all_stop_enemy_se();
    // sdfunc.c:2087 PlayVoiceEx2/PlayVoiceEx/PlayVoice/StopVoice/ContinuePlayVoice on ADX slot 1 (VOICE archive).
    // Mode 0 = 3D start, 1 = centred fade-in start, 2 = parameter update; ADXT_Pause is empty in the original ELF.
    bool play_voice(int sound,SoundPoint position,int mode,unsigned fade_rate,bool pause);
    void continue_voice();
    void stop_voice(unsigned fade_ticks);
    // sdfunc.c CallPlayerVoice: slot 7, bank (SeNo/256)&0xF, list SeNo. Undecodable programs are reported, never fatal.
    void play_player_voice(int se_no);
    // sdfunc.c:2371 ExecEnemySeManager; collision is the room's CheckCollision4Sound(CameraPos, pPos): 0 or -0x1C.
    void exec_enemy_se(const std::function<int(SoundPoint camera,SoundPoint source)>& collision={});
    bool footstep(unsigned floor,unsigned type,unsigned actor,bool enabled,SoundPoint position,bool syukan=false);
    // sdfunc.c:1536 CallPlayerFootStepSe: Id 0, position is PlayerPos.
    bool player_footstep_se(unsigned floor,unsigned type,bool enabled=true,bool syukan=false){return footstep(floor,type,0,enabled,player_pos_,syukan);}
    bool player_action_se(int sound,bool enabled=true,bool syukan=false){return footstep(unsigned(sound),3,0,enabled,player_pos_,syukan);}
    bool cartridge_se(unsigned weapon_bank,SoundPoint position,int sound);
    bool enemy_sound(unsigned slot,unsigned sound,SoundPoint position);
    bool event_sound(unsigned slot,unsigned sound,SoundPoint position,int mode);
    bool user_sound_pan(unsigned type,unsigned slot,int start,int target,unsigned frames);
    void regist_object_se(unsigned object,SoundPoint position,int sound,int priority);
    void free_object_se(unsigned object);
    void exec_object_se(const std::function<int(SoundPoint,SoundPoint)>& collision={});
    struct SoundState {float volume=0,pan=0;int volume_count=0,pan_count=0,fade=0,pan_function=0,pitch=0;};
    SoundState sound_state(unsigned voice,bool midi=false)const;
    SoundState adx_state(unsigned slot)const;

    static unsigned se_voice(unsigned se_slot);
    void load_door_sounds(unsigned bank);
    bool play_door_sound(unsigned program);
    void finish_door_sound(); // Com_ExecRoomFadeIn: RequestMidiFadeFunction(2, 2, 250) on the port MidiHandle[2] names
    unsigned door_voice()const{return door_slot_;} // the port MidiHandle[2] currently names (sdMidiPlay rotation 2 -> 8 -> 9)
    void stop(unsigned slot,unsigned fade_ticks=0);
    bool active(unsigned slot)const;
    std::vector<std::int16_t> mix(unsigned frames=audio_frames_per_tick);
    // Original BGM control (sdfunc.c PlayBgmEx/PlayBgm2/StopBgm/StopBgm2,
    // Com_ExecRoomFadeOut/In; adxwrap.c volume and fade units) on ADX slot 0.
    // The request state persists across rooms like the original globals.
    void play_bgm_ex(int bgm,int fade_in_rate,int volume);
    // adv.c CallPlayerDeadVoice: PlayBgmEx2(PatId[3], player + 8, 1, -45) --
    // the ADV archive on the BGM slot, not a looping track.
    void play_dead_voice(int player);
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
    // MODHSYN VOLL/VOLR of an SE shot voice (0 for other voices).
    std::shared_ptr<const PcmClip> voice_clip(unsigned voice)const{return voice<slots_.size()?slots_[voice].clip:nullptr;}
    SonyLevels voice_levels(unsigned voice)const{return voice<slots_.size()&&slots_[voice].shot&&slots_[voice].clip?slots_[voice].levels:SonyLevels{};}
    std::vector<std::string> played,unavailable;
private:
    unsigned disc_=0;
    // A voice slot plays one or more layers (simultaneous SQ notes); fades act on the slot.
    struct Layer {std::shared_ptr<PcmClip> clip;double position=0;float left=1,right=1;SpuEnvelope env;unsigned core=0;};
    struct SequenceNote {Layer layer;unsigned channel=0,key=0;};
    struct SequenceEvent {double delay=0;unsigned status=0,key=0,value=0;std::vector<Layer> layers;}; // note-on: one layer per MODHSYN voice (split)
    struct SequenceLoop {unsigned number=0;std::size_t next=0;unsigned remaining=0;bool counted=false;};
    struct Sequence {std::vector<SequenceEvent> events;std::vector<SequenceNote> notes;std::vector<SequenceLoop> loops;std::size_t next=0;double until=0;unsigned nrpn=0;};
    // clip_rate ueberlebt das natuerliche Clipende (dort wird nur `clip` geloest), damit
    // adx_play_time danach noch die erreichte Zeit melden kann; release_channel setzt das
    // ganze Werk zurueck und damit auch die Rate.
    struct Channel {std::shared_ptr<PcmClip> clip;unsigned clip_rate=0;double position=0;float volume=1,gain=1,target=1;unsigned fade=0;bool loop=false;float left=1,right=1,pan_left=1,pan_right=1;int pitch=0;std::vector<Layer> layers;std::shared_ptr<Sequence> sequence;SpuEnvelope env;unsigned core=0;bool released=false;
        // TSNDDRV SE shot (Tsnd_tqreq/Tsnd_tqchg -> tsnd_tq_chgreq CC7/CC10/bend): `shot_request` until the next
        // sdSysServer (sound_sync) carries SdrSeReq, afterwards SdrSeChg; levels = MODHSYN VOLL/VOLR.
        bool shot=false,shot_request=false;unsigned shot_cc7=127,shot_cc10=64;SonyLevels levels{};};
    // MODHSYN-Stimmen nach Key-Off (CC 120 von sdShotStop/sdMidiStop, Fade-Ende, Sequenzende):
    // der Slot ist frei, die Stimmen klingen bis zum Release-Ende aus.
    std::vector<Channel> releasing_;
    void key_on(const std::shared_ptr<PcmClip>& clip,SpuEnvelope& env,unsigned& core);
    void shot_levels(Channel& c);void shot_volume(Channel& c,float volume);bool shot_pan(Channel& c,float pan);
    unsigned spu_core(std::uint8_t attr)const;
    void release_channel(Channel& channel);
    void release_sequence_end(Channel& channel);
    // Zwei SPU2-Kerne mit je eigenem Hall (TSNDDRV spucore_init(0/1), SdrSetRev Kernmaske).
    std::array<SpuReverb,2> reverb_{};std::array<unsigned,2> rev_mode_{};
    void set_rev(unsigned core_mask,unsigned mode,int depth);
    // sdfunc.c RoomFxLevel/FxLevelTimer/CurrentFxLevel/AddFxLevel (sdSysServer, ps2_sg_sd.c:1680).
    int room_fx_target_=0,fx_level_timer_=0,current_fx_level_=0,add_fx_level_=0;
    void init_spu();
    void sequence_step(Channel& channel);
    void play_layers(unsigned slot,std::vector<Layer> layers,float volume,bool loop);
    // Native shot/event voices are separate from CRI movie/ADX channels.
    std::array<Channel,audio_voice_count> slots_{};
    std::map<unsigned,std::shared_ptr<PcmClip>> system_sounds_;
    unsigned effect_slot_=2;
    unsigned max_object_slots_=1,max_event_slots_=4;
    int current_bgm_=-1,current_bgm_volume_=-127,req_fade_bgm_=0;
    void play_bgm_archive(const std::string& archive,unsigned id,int fade_in_rate,int volume);
    void request_sound_fade(int function,unsigned attributes,int timer);
    void request_midi_fade_timer(unsigned midi,int function,int timer);
    std::map<unsigned,std::vector<std::uint8_t>> room_banks_,midi_programs_,midi_sequences_; // type 2 SE / type 1 program / type 0 SQ
    std::map<std::tuple<unsigned,unsigned,int>,std::vector<std::shared_ptr<PcmClip>>> room_sounds_; // bank (0x100|bank: MIDI), program, key -> voices
    unsigned room_pack_=0;
    std::map<unsigned,unsigned> sound_bank_packs_; // provenance of independently replaced SE banks
    unsigned room_sound_case_=0;
    bool room_initializing_=false;
    std::array<int,3> current_background_{{-1,-1,-1}};
    std::array<unsigned,3> request_background_{};
    // types.h Enemy (EnemyInfo[128]) and EnemySlot (EnemySlotInfo[6]): BSS globals, zero at boot and
    // never reset (ResetEnemySeInfo is empty), so a first request keeps Prio 0 until it is served.
    struct EnemyInfo {SoundPoint pos;float dist=0;int se=0,se_v=0,fade=0;bool req=false,req_v=false;int call=0,call_v=0;std::int8_t prio=0,pan=0,vol=0;};
    struct EnemyVoice {bool flag=false;unsigned enemy=0;int se=0,attrib=0,prio=0;};
    std::array<EnemyInfo,128> enemy_info_{};std::array<EnemyVoice,6> enemy_voices_{};
    void call_enemy_se_main(unsigned slot,int se_no,int pan,int volume,int flag,int fade_rate);
    struct BackgroundRequest {bool active=false;unsigned sound=0,timer=0;};
    std::array<BackgroundRequest,3> background_init_{};
    std::array<bool,3> foot_switch_{};
    unsigned door_bank_=0,door_slot_=door_voice_first;
    std::vector<std::uint8_t> door_source_;
    std::map<unsigned,std::vector<std::uint8_t>> weapon_banks_;
    std::map<std::pair<unsigned,unsigned>,std::shared_ptr<PcmClip>> weapon_sounds_;
    std::shared_ptr<PcmClip> weapon_clip(unsigned bank,unsigned program);
    // SetupSeGenericParm + ExPlaySe for an SE voice: pan/volume per RequestInfo.
    struct SeRequest {int pan=0,volume=0;bool pan_default=false,volume_default=false;};
    SeRequest setup_se_generic(unsigned se_slot,SoundPoint position)const;
    void play_se_request(unsigned voice,std::shared_ptr<PcmClip> clip,const SeRequest& request);
    void apply_se_volume(unsigned voice,float volume);
    void apply_se_pan(unsigned voice,float pan);
    struct SoundControl {float volume=0,speed=0,last=0,pan=0,pan_speed=0,pan_last=0;int remaining=0,function=0,pan_remaining=0,pan_function=0,pitch=0;};
    std::array<SoundControl,audio_voice_count> se_volume_{};
    std::array<SoundControl,8> midi_control_{};
    // ADXT_INFO (adxwrap.c): Volume/VolSpeed/FadeCntMax/LimitMaxVol/Pan of the four ADX slots.
    // Pan is state only in the original (SetPanAdx2 stores it), so it is not applied to the output.
    std::array<SoundControl,4> adx_control_{};float adx_limit_[4]={};
    void apply_midi_volume(unsigned midi,float volume);
    void apply_midi_pan(unsigned midi,float pan);
    void request_midi_fade(unsigned midi,int start,int target,int frames);
    void request_pan(SoundControl& state,int start,int target,int frames);
    void request_adx_fade(unsigned slot,int function,int timer);
    void request_adx_fade_ex(unsigned slot,int start,int target,int frames);
    void apply_adx_volume(unsigned slot,float volume);
    void apply_adx_volume_ex(unsigned slot,float volume,float max);
    void apply_se_pitch(unsigned voice,int pitch);
    void restore_midi(unsigned midi,unsigned voice);
    struct ObjectSound {SoundPoint position;float distance=0;int sound=0,slot=-1,priority=0,type=0,pan=0,volume=0;bool request=false;unsigned flags=0;std::array<int,3> volume_fade{},pan_fade{};};
    struct ObjectSlot {int sound=0;bool flag=false,find=false;unsigned object=0;};
    std::array<ObjectSound,16> object_sounds_{};
    std::array<ObjectSlot,3> object_slots_{};
    std::array<unsigned,16> object_requests_{};unsigned object_request_count_=0;
    int object_sound_slot(unsigned object,bool find=false);
    void call_object_sound(unsigned slot,ObjectSound& sound,bool start,const std::function<int(SoundPoint,SoundPoint)>& collision);
    unsigned sound_sync_frames_=0;
    void request_se_fade(unsigned voice,int start,int target,int frames);
    void sound_sync();
    SoundPoint camera_pos_{},player_pos_{};int camera_ay_=0;
    RoomSoundEnv room_env_{};int current_room_fx_level_=0;
    std::array<unsigned,20> se_flags_{};std::array<unsigned,8> midi_flags_{};std::array<unsigned,16> object_flags_{};unsigned adx_flag_=0;
    unsigned weapon_slot_switch_=0;
    std::filesystem::path data_,mods_;
};
}
