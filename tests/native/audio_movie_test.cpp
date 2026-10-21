#include "audio.h"
#include "movie.h"
#include "asset_io.h"
#include "binary_view.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "movie_fixture.inc"
#include "weapon_tables.h"
#include "audio_pitch.h"
#include "audio_pan.h"
namespace recvx {
#include "combat_tables_data.inc"
}
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static int reject_count=0;template<class F> static void reject(F f){bool rejected=false;++reject_count;try{f();}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("malformed media accepted #"+std::to_string(reject_count));}
static void le(std::vector<std::uint8_t>& b,unsigned n){for(unsigned i=0;i<4;++i)b.push_back(std::uint8_t(n>>(i*8)));}
// SPU2 envelope, MODHSYN key-off/CC 120, core choice, TSNDDRV/libsd HALL (audio_spu.h).
static void spu_envelope_reverb(const std::filesystem::path& root){
    // Hand-computed from the SPU ADSR rules: ADSR1 0x80ff = exponential attack shift 0 step 0,
    // decay shift 15, sustain level 15; ADSR2 0x5fe6 = sustain rate 0x7f (static), release exp shift 6.
    SpuEnvelope env;env.key_on(0x80ff,0x5fe6);
    const std::int32_t attack[4]={14336,28672,32256,32767};
    for(auto expected:attack){check(env.tick()&&env.level==expected,"attack +7<<11, then /4 above 0x6000");}
    check(env.phase==SpuEnvelope::Decay,"attack ends at 0x7fff");
    check(env.tick()&&env.phase==SpuEnvelope::Sustain&&env.level==32767,"decay target (15+1)<<11 is above ENVX");
    for(int i=0;i<1000;++i)check(env.tick()&&env.level==32767,"rate 0x7f never steps");
    env.key_off();check(env.tick()&&env.level==32511,"exponential release -8<<5 scaled by level");
    unsigned ticks=1;while(env.tick())++ticks;
    check(env.level==0&&env.phase==SpuEnvelope::Stopped&&ticks>100&&ticks<4000,"release ends at 0 and stops the voice");
    // CC 120: ADSR2 0xC021 -> exponential release shift 1 (-8<<10 per tick scaled by level).
    SpuEnvelope cut;cut.key_on(0x80ff,0x5fe6);for(int i=0;i<8;++i)cut.tick();
    cut.adsr2=0xC021;cut.key_off();check(cut.tick()&&cut.level==24575,"CC 120 release step");
    unsigned cut_ticks=1;while(cut.tick())++cut_ticks;check(cut_ticks<200,"CC 120 release is short");
    // Linear increase with shift > 11 steps only every 2^(shift-11) ticks.
    SpuEnvelope slow;slow.key_on(std::uint16_t(13<<10),0x5fe6);
    for(int i=0;i<3;++i)check(slow.tick()&&slow.level==0,"shift 13 waits four ticks");
    check(slow.tick()&&slow.level==7,"shift 13 step +7");

    // LIBSD.IRX .data: reverb sizes at +0x38, presets at +0x64 (0x44 bytes each).
    const auto libsd=read_asset_file(root/"files/PS2_DATA/MODULES/LIBSD.IRX",1024*1024);BinaryView elf(libsd);
    const auto shoff=elf.u32(0x20);const unsigned shentsize=elf.u16(0x2e);const auto data=elf.u32(shoff+6*shentsize+16);
    for(unsigned mode=0;mode<10;++mode){check(spu_reverb_size(mode)==elf.u32(data+0x38+mode*4),"reverb size table differs from LIBSD.IRX");
        for(unsigned r=0;r<32;++r)check(spu_reverb_preset(mode)[r]==elf.u16(data+0x64+mode*0x44+r*2),"reverb preset differs from LIBSD.IRX");}
    check(spu_reverb_preset(5)[0]==0x01a5&&spu_reverb_size(5)==0x15bc,"HALL preset");

    // A MODHSYN voice from the original bank: sampleSpuAttr 0x1f (dry+effect L/R, core 0).
    const auto pack=read_asset_file(root/"archives/MULTSPQ1/0002.bin",32*1024*1024);BinaryView spq(pack);std::vector<std::uint8_t> programs;
    for(unsigned i=0;i<16&&spq.u32(i*16);++i)if(spq.u32(i*16+8)==1&&spq.u32(i*16+12)==2){const auto offset=spq.u32(i*16),size=spq.u32(i*16+4);programs={pack.begin()+offset,pack.begin()+offset+size};break;}
    const auto clip=std::make_shared<PcmClip>(decode_system_sound(programs,6,60));
    check(clip->spu&&clip->spu_attr==0x1f&&clip->adsr1==0xa1ff&&clip->adsr2==0x5fea,"sample ADSR/attr from HD +0x12/+0x14/+0x29");

    // Core choice: 0x2f pins core 1, no preference picks the core with more free voices.
    {AudioMixer m;auto core1=std::make_shared<PcmClip>(*clip);core1->spu_attr=0x2f;auto any=std::make_shared<PcmClip>(*clip);any->spu_attr=0x0f;
        // SE host voices: synthetic TQ programs, keyed at the ATick of the sdSysServer that sends them (frame 800).
        m.play_pcm(2,core1,1,true);m.play_pcm(3,core1,1,true);m.mix(800);check(m.spu_voices(1)==2&&m.spu_voices(0)==0,"attr 0x20 pins core 1");
        m.play_pcm(4,any,1,true);m.mix(800);check(m.spu_voices(0)==1,"no preference -> core with more free voices");
        m.play_pcm(5,any,1,true);m.mix(800);check(m.spu_voices(0)==2&&m.spu_voices(1)==2,"tie -> core 0");
        m.play_pcm(6,any,1,true);m.mix(800);check(m.spu_voices(0)==3,"tie -> core 0 again");
        m.play_pcm(7,any,1,true);m.mix(800);check(m.spu_voices(1)==3,"core 1 has more free voices");}

    // sdShotStop/sdMidiStop -> CC 120: the slot is free at once, the voice releases (0xC021).
    {AudioMixer m;m.play_pcm(12,clip,1,true);m.mix(2000);check(m.spu_voices(0)==1,"looping voice sounding");
        m.stop(12);check(!m.active(12)&&m.spu_voices(0)==1,"stop frees the slot, the voice is in release");
        const auto tail=m.mix(400);check(std::any_of(tail.begin(),tail.begin()+8,[](std::int16_t v){return v!=0;}),"release tail audible");
        check(m.spu_voices(0)==0,"CC 120 release finished");}

    // Room pack 0 (stage 0 room 0): RoomFxLevel 16 -> CurrentFxLevel ramps over 16 VSyncs to 4096.
    AudioMixer wet(root),dry(root);wet.load_room_sounds(0,0);dry.load_room_sounds(0,0);
    check(wet.current_fx_level()==0,"FX ramp starts at the previous CurrentFxLevel");
    wet.mix(800);check(wet.current_fx_level()==256&&wet.reverb(0).depth()==256&&wet.reverb(1).depth()==256,"AddFxLevel (4096-0)/16 per VSync on both cores");
    dry.mix(800);wet.mix(15*800);dry.mix(15*800);check(wet.current_fx_level()==4096&&wet.reverb(0).depth()==4096,"FxLevelTimer end sets RoomFxLevel*256");
    auto nosend=std::make_shared<PcmClip>(*clip);nosend->spu_attr=0x13;
    wet.play_pcm(12,clip,1,true);dry.play_pcm(12,nosend,1,true);
    const auto a=wet.mix(48000),b=dry.mix(48000);bool differs=false;for(std::size_t i=0;i<a.size();++i)differs|=a[i]!=b[i];
    check(differs,"VMIXEL/VMIXER voice feeds the HALL");
    wet.stop(12);dry.stop(12);const auto ta=wet.mix(24000),tb=dry.mix(24000);
    bool wet_tail=false,dry_silent=true;for(std::size_t i=4000;i<ta.size();++i){wet_tail|=ta[i]!=0;dry_silent&=tb[i]==0;}
    check(wet_tail&&dry_silent,"HALL tail continues after the voice ended");
    std::cout<<"SPU2 ADSR, MODHSYN key-off/CC 120/core choice, LIBSD HALL presets and RoomFxLevel EVOL passed.\n";
}
// MODHSYN register levels of a sequence note against the same voice at unit gain (clip volume 1, pan 64): the SQ voice
// plays sample * ENVX * VOLL/16384; both outputs truncate to int16, so they agree within 2.
static bool scaled_like(const std::vector<std::int16_t>& actual,std::size_t offset,const std::vector<std::int16_t>& reference,std::size_t ref_offset,SonyLevels lv,SonyLevels ref){
    for(std::size_t i=offset;i<actual.size()&&i-offset+ref_offset<reference.size();++i){const auto u=reference[i-offset+ref_offset];
        const float gain=(i&1)?float(lv.right)/float(ref.right):float(lv.left)/float(ref.left);if(std::abs(float(actual[i])-float(u)*gain)>2.f)return false;}
    return true;
}
// Synthetic MODHSYN voice: full bank volume, centred, ADSR1 0x00ff / ADSR2 0x1fc0 (3-tick attack, static sustain,
// 2-tick release), dry L/R on core 0 (sampleSpuAttr 0x13).
static std::shared_ptr<PcmClip> spu_clip(std::vector<std::int16_t> samples,unsigned rate=48000,bool loop=false,std::uint8_t attr=0x13){
    auto c=std::make_shared<PcmClip>();c->rate=rate;c->channels=1;c->samples=std::move(samples);c->spu=true;c->spu_attr=attr;c->adsr1=0x00ff;c->adsr2=0x1fc0;
    c->sony_volume=127u*127u*127u;c->pan=64;c->volume=1;if(loop){c->loop_start=0;c->loop_end=c->frames();}return c;}
static std::size_t first_sound(const std::vector<std::int16_t>& v){for(std::size_t i=0;i<v.size();++i)if(v[i])return i;return v.size();}
// IOP timing of a request issued right after a sync: sdSysServer at frame 800 sends it, the TSNDDRV ATick of that frame
// keys an SE (TQ) or starts the MIDI port, whose first sequence tick runs at the next ATick (1000): 4 SQ ticks per ATick.
static constexpr std::size_t se_start=800,midi_tick0=1000;
static void sample_loops(const std::filesystem::path& root){
    // Original MULTSPQ1/0002 bank 2/list 6: program 6/key 60, velocity 127,
    // tempo 500000/PPQN 480. NoteOn tick 7, NoteOff tick 7687 (50 frames/tick).
    const auto pack=read_asset_file(root/"archives/MULTSPQ1/0002.bin",32*1024*1024);BinaryView spq(pack);std::vector<std::uint8_t> programs;
    for(unsigned i=0;i<16&&spq.u32(i*16);++i)if(spq.u32(i*16+8)==1&&spq.u32(i*16+12)==2){const auto offset=spq.u32(i*16),size=spq.u32(i*16+4);spq.require(offset,size);programs={pack.begin()+offset,pack.begin()+offset+size};break;}
    const auto clip=std::make_shared<PcmClip>(decode_system_sound(programs,6,60));
    check(clip->rate==32000&&clip->loop_start==3*28&&clip->loop_end==2159*28,"original Vagi loop bounds missing/wrong");
    // Both mixers carry the same room environment (SPU2 HALL on both cores, sdInit).
    AudioMixer sequence(root),reference(root),dry;sequence.load_room_sounds(0,2);reference.load_room_sounds(0,2);check(sequence.event_sound(0,0x206),"original loop sample SQ missing");auto unit=std::make_shared<PcmClip>(*clip);unit->volume=1;unit->pan=64;reference.play_pcm(12,unit,1,true);dry.play_pcm(12,unit,1,true);
    check(sequence.current_fx_level()==0&&sequence.reverb(0).mode()==5&&sequence.reverb(1).mode()==5,"sdInit HALL mode / initial CurrentFxLevel");
    const auto actual=sequence.mix(120000),pcm=reference.mix(120000),plain=dry.mix(120000);
    // NoteOn tick 7: BgmReq at frame 800, port start at that ATick, ticks 5..8 at the ATick of frame 1200.
    const std::size_t note_at=midi_tick0+200;
    // The ADPCM start is quiet for a few samples: both voices key at an ATick boundary, 1000 frames apart.
    check(first_sound(actual)>=note_at*2&&first_sound(pcm)>=200*2&&first_sound(actual)-first_sound(pcm)==(note_at-200)*2,"SQ note must start at the ATick that passes tick 7");
    const auto levels=sequence.sequence_levels(12),unit_levels=reference.sequence_levels(12);check(levels.size()==1&&unit_levels.size()==1&&levels[0].left>0&&levels[0].left==levels[0].right,"SQ note register level missing");
    check(scaled_like(actual,note_at*2,pcm,200*2,levels[0],unit_levels[0]),"SQ sample loop differs from explicit waveform loop/intro timing");
    // MULTSPQ1/0002 carries RoomFxLevel 0: EVOL stays 0 and the result equals the dry mix.
    check(sequence.current_fx_level()==0&&reference.current_fx_level()==0,"RoomFxLevel 0 room changed EVOL");
    for(std::size_t i=0;i<plain.size();++i)check(pcm[i]==plain[i],"EVOL 0 hall changed the dry mix");
    check(std::any_of(actual.begin()+100000*2,actual.end(),[](auto n){return n!=0;}),"SQ note silent after waveform end");
    sequence.mix(7687*50-120000);const auto released=sequence.mix(100);
    // MODHSYN NoteOff = key-off: the voice runs the sample's ADSR2 0x5fea release (exponential,
    // shift 10), it does not stop at the NoteOff tick.
    check(std::any_of(released.begin(),released.end(),[](auto n){return n!=0;}),"NoteOff cut the voice instead of its ADSR release");
    const auto restarted=sequence.mix(2000);check(std::any_of(restarted.begin(),restarted.end(),[](auto n){return n!=0;}),"NRPN transport did not restart note after NoteOff");
    // StopNativeEventSe -> StopMidi -> SdrBgmStop at the next sdSysServer: TSNDDRV port_stop -> sceMidi_MidiPlaySwitch off
    // -> CC 123 (key-off, the sample's ADSR2 release), the EE sees the port free two frames later.
    sequence.stop_event_sound(0);sequence.mix(2400);check(!sequence.active(12),"sample loop survives slot stop");
    sequence.mix(24000);check(sequence.spu_voices(0)+sequence.spu_voices(1)==0,"CC 123 key-off: the voice must end its ADSR2 release");
    check(sequence.event_sound(0,0x206),"loop sample reentry failed");sequence.mix(120000);
    sequence.set_3d_sound_flag(7,0,2);check(sequence.user_sound_volume(7,0,0,-127,1),"RequestMidiFadeFunctionEx(DefEvt[0], 0, -127, 1)");
    sequence.mix(800*5);check(!sequence.active(12),"MIDI fade to -127 must StopMidi at FadeCntMax < 0");
    check(sequence.event_sound(0,0x206),"loop sample second reentry failed");sequence.mix(120000);sequence.load_room_sounds(0,0);
    // Room 0 has RoomFxLevel 16: the HALL buffer (filled while EVOL was 0) becomes audible as EVOL
    // ramps, so the stopped voice itself is checked, not silence.
    sequence.mix(2400);check(!sequence.active(12),"sample loop survives room replacement");
    sequence.mix(24000);check(sequence.spu_voices(0)+sequence.spu_voices(1)==0,"room replacement: the stopped port's voice must end its release");
    // Change only Vagi's documented attribute in a copied bank: a one-shot
    // retains exactly the decoded PCM, but never acquires sample-loop bounds.
    BinaryView hd(programs);const auto base=hd.u32(0),head=base+hd.u32(base+8),vags=base+hd.u32(head+0x20),vag=vags+hd.u32(vags+16+7*4);
    auto shot=programs;shot.at(vag+6)=0;const auto one=decode_system_sound(shot,6,60);
    check(!one.loop_start&&!one.loop_end&&one.samples==clip->samples,"Vagi one-shot did not override ADPCM loop markers");
    shot.at(vag+6)=2;reject([&]{decode_system_sound(shot,6,60);});
    // An outer BG/channel loop must not turn a one-shot SQ note into a loop.
    // Original bank 2/list 0 has a one-shot program; compare both slot policies.
    const auto short_clip=std::make_shared<PcmClip>(decode_system_sound(programs,0,60));check(!short_clip->loop_end,"original one-shot unexpectedly loops");
    auto short_unit=std::make_shared<PcmClip>(*short_clip);short_unit->volume=1;short_unit->pan=64;
    AudioMixer short_reference;short_reference.play_pcm(12,short_unit);const auto short_pcm=short_reference.mix(120000);const auto short_lv=short_reference.sequence_levels(12);check(short_lv.size()==1,"diagnostic one-shot level missing");
    for(bool outer:{false,true}){AudioMixer short_sequence(root);short_sequence.load_room_sounds(0,2);check(short_sequence.play_room_sequence(2,0,12,outer),"original one-shot SQ missing");
        // NoteOn tick 5: the ATick of frame 1200 (ticks 5..8).
        auto out=short_sequence.mix(midi_tick0+201);const auto lv=short_sequence.sequence_levels(12);check(lv.size()==1&&lv[0].left>0,"one-shot SQ note register level missing");
        check(std::all_of(out.begin(),out.begin()+(midi_tick0+200)*2,[](auto n){return n==0;}),"one-shot SQ sounded before its NoteOn");
        const auto rest=short_sequence.mix(120000-(midi_tick0+201));out.insert(out.end(),rest.begin(),rest.end());
        check(scaled_like(out,(midi_tick0+200)*2,short_pcm,200*2,lv[0],short_lv[0]),"outer SQ slot loop repeats a one-shot waveform");short_sequence.mix(300000);check(!short_sequence.active(12),"one-shot SQ end retained slot");}
    // ExPlaySe keys a voice without any slot loop (CallEnemySeMain, CallSystemSe, ...); the SPU still
    // follows the VAG loop markers until key-off, like the SQ note above (en02.c:1040, Gulp Worm 70400).
    {AudioMixer se,looped;se.play_pcm(12,clip);looped.play_pcm(12,clip,1,true);
        check(se.mix(120000)==looped.mix(120000)&&se.active(12),"ExPlaySe voice ignored the VAG loop markers");
        AudioMixer one;one.play_pcm(12,short_clip);one.mix(300000);check(!one.active(12),"one-shot ExPlaySe voice looped");}
    // Original terminal-7 Vagi: a silent self-loop, not a guessed full loop.
    const auto terminal=read_asset_file(root/"archives/MULTSPQ1/0022.bin",32*1024*1024);BinaryView term(terminal);
    for(unsigned i=0;i<16&&term.u32(i*16);++i)if(term.u32(i*16+8)==1&&term.u32(i*16+12)==2){
        const auto offset=term.u32(i*16),size=term.u32(i*16+4);std::vector<std::uint8_t> bank(terminal.begin()+offset,terminal.begin()+offset+size);BinaryView bankv(bank);
        const auto b=bankv.u32(0),h=b+bankv.u32(b+8),v=b+bankv.u32(h+0x20),p=v+bankv.u32(v+16+5*4),data=bankv.u32(8)+bankv.u32(p);
        const auto silent=decode_ps_adpcm(std::vector<std::uint8_t>(bank.begin()+data,bank.begin()+data+7*16),43000);
        check(silent.loop_start==6*28&&silent.loop_end==7*28&&std::all_of(silent.samples.begin()+6*28,silent.samples.end(),[](auto n){return n==0;}),"terminal-7 silent loop bounds wrong");break;
    }
    check(sequence.sequence_unavailable.empty(),"sample-loop sequence reported unavailable");
}
// sdfunc.c:847 Get3DSoundParameter. Expectations derived by hand from the
// original tables: Ang = cam.ay + 10430.381*atan2(cam.x-src.x, cam.z-src.z).
static void sound_3d_core(){
    {AudioMixer fade;auto clip=spu_clip(std::vector<std::int16_t>(20000,12700));
     fade.play_pcm(weapon_voice_8,clip);fade.set_3d_sound_flag(3,0,2);fade.user_sound_volume(3,0,0,-127,2);
     // RequestSeFadeFunctionEx(.., 0, -127, 2): VolSpeed 63.5. VSync 1 (after SdrSeReq with the handle's old vol 127):
     // Volume -63.5 -> sdShotSetVol((Sint8)-63) = byte 65; VSync 2: SdrSeChg(65), Volume -127 (byte 1); VSync 3:
     // SdrSeChg(1), FadeCntMax < 0 -> StopSe (SdrSeCancel); VSync 4 sends it, the ATick cancels; VSync 5 sees it free.
     const auto s0=fade.sound_state(weapon_voice_8);check(s0.fade==2&&s0.volume_count==2&&int(s0.volume)==0,"RequestSeFadeFunctionEx(.., 0, -127, 2)");
     fade.mix(800);const auto s1=fade.sound_state(weapon_voice_8);check(int(s1.volume)==-63&&s1.volume_count==1,"SE fade first VSync must subtract VolSpeed (-63.5 -> driver byte -63)");
     check(fade.voice_levels(weapon_voice_8).left==sony_voice_levels(127u*127u*127u,127,sony_controller_scale(127),128,64,64).left,"SdrSeReq carries the handle's volume before the fade step (127)");
     fade.mix(800);check(fade.active(weapon_voice_8)&&fade.sound_state(weapon_voice_8).volume_count==0,"SE fade must not stop at FadeCntMax == 0");
     check(fade.voice_levels(weapon_voice_8).left==sony_voice_levels(127u*127u*127u,127,sony_controller_scale(65),128,64,64).left,"SdrSeChg(65) must reach MODHSYN as CC7 65 at the ATick of that VSync");
     fade.mix(800);const auto s3=fade.sound_state(weapon_voice_8);check(s3.fade==0&&int(s3.volume)==-127,"SE fade must stop at FadeCntMax < 0");
     fade.mix(800*3);check(!fade.active(weapon_voice_8),"StopSe must reach the IOP (SdrSeCancel at the next sdSysServer, cleared at its ATick, reported with the following SdrSetIopData)");}
    {AudioMixer fade;fade.set_3d_sound_flag(3,0,2);fade.user_sound_volume(3,0,0,-100,4);fade.mix(1600);
     auto clip=spu_clip(std::vector<std::int16_t>(2000,12700));fade.play_pcm(weapon_voice_8,clip);fade.set_3d_sound_flag(3,0,2);
     // Two VSyncs of the fade: Volume -50 -> byte 78 on the handle; SetVolumeSe(0) with FadeFunc 2 changes nothing; SdrSeReq(78).
     fade.mix(800);check(fade.voice_levels(weapon_voice_8).left==sony_voice_levels(127u*127u*127u,127,sony_controller_scale(78),128,64,64).left,"SE fade on an idle slot must advance and retain its volume when sound starts");}
    AudioMixer m;m.set_sound_monitor({0,0,0},0,{});
    auto at=[&](float x,float y,float z,int mode=0){return m.sound_parameter_3d({x,y,z},mode);};
    const auto ahead=at(0,0,-10);check(ahead.pan==0&&ahead.volume==0&&ahead.distance==10,"Ang 0: PanTbl360[0]/ThreeDVolTbl[10]");
    // Single precision: 10430.381f*atan2f = 32768.0088 / 16384.0044 (above the integer for EE truncation too).
    const auto back=at(0,0,10);check(back.pan==4&&back.volume==-34,"Ang 32768 must use index 36 (pan 4, vol -34)");
    const auto left=at(-10,0,0);check(left.pan==-30&&left.volume==-8,"Ang 16384 must use index 18 (pan -30, vol -8)");
    const auto right=at(10,0,0);check(right.pan==26&&right.volume==0,"Ang 49153 must use index 54 (pan 26, vol 0)");
    check(at(0,0,-600).volume==-87&&at(0,0,-509.5f).volume==-87,"distance above 510 clamps to ThreeDVolTbl[509]");
    check(at(0,0,-510.5f).volume==0,"distance 510 reads the original zero pad byte after ThreeDVolTbl (ELF 0x2936BC)");
    m.set_sound_monitor({0,0,0},16384,{});const auto turned=at(0,0,-10);check(turned.pan==-30&&turned.volume==-8,"cam.ay 16384 must give index 18");
    m.set_sound_monitor({0,0,0},-1,{});const auto wrap=at(0,0,-10);check(wrap.pan==0&&wrap.volume==0,"index 71 reads the original zero bytes after PanTbl360/PanTbl360Vol");
    m.set_sound_monitor({5,1,-3},0,{});check(at(5,1,-13).volume==0&&at(5,1,-13).distance==10,"CameraPos is the listener origin");
    m.set_3d_sound_flag(3,0,6);check(m.se_3d_flag(8)==6&&m.se_3d_flag(9)==6&&m.se_3d_flag(19)==6,"Set3dSoundFlag type 3 covers slots 8/9/19");
    m.set_3d_sound_flag(1,2,2);check(m.se_3d_flag(15)==2&&m.se_3d_flag(16)==2,"Set3dSoundFlag type 1 covers slot*2+11/12");
    reject([&]{m.set_3d_sound_flag(5,6,2);});
    m.reset_3d_sound_flag();for(unsigned s=0;s<20;++s)check(!m.se_3d_flag(s),"Reset3dSoundFlag leaves an SE flag");
}
static std::pair<int,int> peaks(AudioMixer& m,unsigned frames=6000){int l=0,r=0;const auto out=m.mix(frames);for(std::size_t i=0;i+1<out.size();i+=2){l=std::max(l,std::abs(int(out[i])));r=std::max(r,std::abs(int(out[i+1])));}return {l,r};}
// sdfunc.c:1546 CallPlayerWeaponSeEx with original weapon/room packs.
static void weapon_se(const std::filesystem::path& root){
    const unsigned bank=unsigned(WpnTab[3].snd_wpno);const int se=WpnTab[3].seno0;
    auto shot=[&](SoundPoint at,int slot=0,bool syukan=false,std::function<void(AudioMixer&)> setup={}){AudioMixer m(root);m.load_room_sounds(0,2);m.set_sound_monitor({0,0,0},0,{});if(setup)setup(m);
        check(m.call_player_weapon_se(bank,se,at,slot,syukan),"original weapon SE missing");return peaks(m);};
    {AudioMixer m(root);m.load_room_sounds(0,2);m.set_sound_monitor({0,0,0},0,{});
        check(m.call_player_weapon_se(bank,se,{0,0,-10},0,false)&&m.active(weapon_voice_8)&&m.weapon_se_slot_switch()==1,"SlotNo 0 must start on SE slot 8");
        check(m.call_player_weapon_se(bank,se,{0,0,-10},0,false)&&m.active(weapon_voice_9)&&m.active(weapon_voice_8)&&m.weapon_se_slot_switch()==0,"second shot must alternate to slot 9 without cutting slot 8");
        check(m.call_player_weapon_se(bank,se,{0,0,-10},2,false)&&m.active(weapon_voice_19)&&m.weapon_se_slot_switch()==0,"SlotNo != 0 must use slot 19 and keep the switch");
        check(!m.active(6),"weapon SE must not take the action voice 6");
        check(m.user_sound_volume(3,0,-127,-127,0)&&m.active(weapon_voice_8),"SetUserSoundVolume must not fade weapon slots without 3D flag 2");
        m.set_3d_sound_flag(3,0,2);m.user_sound_volume(3,0,-127,-127,0);m.mix(800*4);check(!m.active(weapon_voice_8)&&!m.active(weapon_voice_9)&&!m.active(weapon_voice_19),"SetUserSoundVolume type 3 with flag 2 must reach slots 8/9/19");}
    const auto near=shot({0,0,-10}),far=shot({0,0,-600});
    check(near.first>0&&std::abs(double(far.first)/near.first-41.0/127.0)<0.01,"distance attenuation must follow ThreeDVolTbl[509] (-87 -> 41/127)");
    const auto left=shot({-10,0,0}),right=shot({10,0,0});check(left.first>left.second&&right.second>right.first,"PanTbl360 must place the shot left/right");
    check(shot({0,0,-600},0,true)==near,"SetSyukanModeSoundParam must zero pan and volume");
    check(shot({0,0,-600},0,false,[](AudioMixer& m){m.set_3d_sound_flag(3,0,2);})==near,"3D flag 2 must use SdcSeDefaultVolume");
    {const auto centred=shot({-10,0,0},0,false,[](AudioMixer& m){m.set_3d_sound_flag(3,0,4);});
     check(std::abs(double(centred.first)/centred.second-double(near.first)/near.second)<0.01&&centred.first<near.first,"3D flag 4 must centre the pan but keep the angle volume");}
    {AudioMixer m(root);m.load_room_sounds(0,0);check(m.room_fx_level()==16&&m.sound_parameter_3d({0,0,-10},0).volume==-1&&m.sound_parameter_3d({0,0,-10},1).volume==0,"RM_0000 RoomFxLevel 16 must subtract 1 in mode 0 only");
        m.load_room_sounds(1,2);check(m.room_fx_level()==48&&m.sound_parameter_3d({0,0,-10},0).volume==-4,"RM_1020 RoomFxLevel 48 must subtract 4");}
    auto voice=[&](SoundPoint player){AudioMixer m(root);m.load_room_sounds(0,2);m.load_player_sounds(0);m.set_sound_monitor({0,0,0},0,player);m.play_player_voice(1026);return peaks(m);};
    const auto close=voice({0,0,-10}),distant=voice({0,0,-600});check(close.first>0&&std::abs(double(distant.first)/close.first-41.0/127.0)<0.01,"CallPlayerVoice must use PlayerPos through Get3DSoundParameter");
}
// sdfunc.c:1708 RequestEnemySeBasic / :2371 ExecEnemySeManager / :2301 CallEnemySeMain.
// SE 0xF300: bank 3 list 0, MaxSeCount 15, Prio 0, Attrib 0, VolDownTbl[0].
static void enemy_se(const std::filesystem::path& root){
    constexpr int se=0xF300; // constexpr: MSVC verbietet lokale Variablen als Lambda-Standardargument (C2587)
    auto mixer=[&](){auto m=std::make_unique<AudioMixer>(root);m->load_room_sounds(0,2);m->set_sound_monitor({0,0,0},0,{});return m;};
    auto one=[&](SoundPoint at,int se_no=se,int fade=0,std::function<int(SoundPoint,SoundPoint)> wall={}){auto m=mixer();m->request_enemy_se(0,at,se_no,1,fade);m->exec_enemy_se(wall);check(m->active(18),"original enemy SE not served on voice 18");return peaks(*m,fade?400:6000);};
    const auto near=one({0,0,-10}),far=one({0,0,-600});
    check(near.first>0&&std::abs(double(far.first)/near.first-41.0/128.0)<0.01,"enemy SE must use Get3DSoundParameter distance (ThreeDVolTbl[509] -87)");
    const auto left=one({-10,0,0}),right=one({10,0,0});check(left.first>left.second&&right.second>right.first,"enemy SE must use PanTbl360");
    check(std::abs(double(one({0,0,-10},se|0x400000).first)/near.first-120.0/128.0)<0.01,"VolDownTbl[4] must lower by 8");
    {auto m=mixer();m->request_enemy_se(0,{0,0,-10},se|0x800000,1,0);m->exec_enemy_se();check(!m->active(18)&&!m->unavailable.empty(),"VolDownTbl index 8 has no original value and must be reported");}
    check(std::abs(double(one({0,0,-10},se,0,[](SoundPoint,SoundPoint){return -0x1C;}).first)/near.first-100.0/128.0)<0.01,"CheckCollision4Sound -0x1C must lower the volume");
    {auto plain=mixer(),faded=mixer();plain->request_enemy_se(0,{0,0,-10},se,1,0);faded->request_enemy_se(0,{0,0,-10},se,1,30);plain->exec_enemy_se();faded->exec_enemy_se();plain->mix(1600);faded->mix(1600);
     // VolumeDelayTime -1: SdrSeReq carries the handle's previous volume; the first fade step (-127 + step) reaches
     // TSNDDRV with the SdrSeChg of the next sdSysServer.
     check(peaks(*faded,400).first*4<peaks(*plain,400).first,"FadeRate must start the voice at -0x7F (RequestSeFadeFunctionEx)");}
    {auto m=mixer();m->request_enemy_se(0,{-10,0,0},se,1,0);m->exec_enemy_se();peaks(*m,200);
     m->request_enemy_se(0,{10,0,0},se,0,0);m->exec_enemy_se();const auto moved=peaks(*m,4000);
     check(m->active(18)&&moved.second>moved.first,"Flag 0 (bhEne_SetSEPan) must update the running voice's pan without a new note");}
    {auto m=mixer();m->set_disc(0);m->room_fade_out();m->begin_room_init();m->request_enemy_se(0,{0,0,-10},se,1,0);m->exec_enemy_se();check(!m->active(18),"StartInitScriptFlag must hold enemy SE");
     m->room_fade_in();m->exec_enemy_se();check(m->active(18),"request must survive the init script");}
    {// First requests keep the BSS Prio 0; after one pass every served entry is Prio 3.
     auto m=mixer();for(unsigned e=0;e<7;++e)m->request_enemy_se(e,{0,0,-10},se,1,0);m->exec_enemy_se();m->mix(800);for(unsigned v=18;v<24;++v)m->stop(v);m->mix(800*3); /* StopSe after the request left, reported free after the ATick */
     // Enemy 0 has the lower priority but is farther: the original exchange sort still swaps it behind.
     m->request_enemy_se(0,{0,0,-100},se,1,0);for(unsigned e=1;e<7;++e)m->request_enemy_se(e,{0,0,-10},se|0x10000,1,0);m->exec_enemy_se();
     check(!m->enemy_se_playing(0,1)&&m->enemy_se_playing(6,1),"ExecEnemySeManager must sort with Prio > || Dist > (enemy 0 loses the sixth voice)");}
    {// sdfunc.c AllStopEnemySe: EnemySlotInfo cleared at once, the registered voice fades out with
     // RequestSeFadeFunction(..., 2, 200) = FadeCntMax 30 sound syncs (800 frames each), then the stop.
     auto m=mixer();m->request_enemy_se(0,{0,0,-10},se,1,0);m->request_enemy_se(1,{0,0,-10},se,1,0);m->exec_enemy_se();m->mix(800);m->stop(19);m->mix(800*3);m->exec_enemy_se(); /* CheckPlaySe frees slot 1 once the IOP reports it */
     m->all_stop_enemy_se();const auto fade=m->sound_state(18,false);
     check(!m->enemy_se_playing(0,1)&&m->active(18)&&fade.fade==2&&fade.volume_count==30,"AllStopEnemySe must clear the slot and fade (Func 2, FadeCntMax 30), not cut");
     check(m->sound_state(19,false).fade==0,"a voice without a running SE takes StopEnemySe, not a fade");}
}
// sdfunc.c:1447 CallPlayerFootStepSeEx, :1583 CallYakkyouSe, :1925-2004/2669 object SE, :895 pan manager, :1830 CallNativeEventSe.
static void shared_3d_consumers(const std::filesystem::path& root){
    auto mixer=[&](){auto m=std::make_unique<AudioMixer>(root);m->load_room_sounds(0,2);m->set_sound_monitor({0,0,0},0,{0,0,-10});return m;};
    {auto near=mixer(),far=mixer();near->footstep(0,0,0,true,{0,0,-10});far->footstep(0,0,0,true,{0,0,-600});
     const auto a=peaks(*near),b=peaks(*far);check(a.first>0&&b.first<a.first,"footstep must use Get3DSoundParameter and its distance");
     auto flag=mixer();flag->footstep(0,0,0,false,{0,0,-10});check(!flag->active(4),"Flag 0 must not request a footstep");
     auto alternate=mixer();alternate->footstep(0,0,0,true,{0,0,-10});alternate->footstep(0,0,0,true,{0,0,-10});check(alternate->active(4)&&alternate->active(5),"foot slots 11/12 must alternate");
     auto action=mixer();check(action->footstep(5,3,0,true,{0,0,-10})&&action->active(6),"CallPlayerActionSe SeNo > 4 must not be rejected by the floor gate");
     check(!mixer()->footstep(5,1,0,true,{0,0,-10}),"only walk/run footsteps are limited to floor types 0..4");}
    {auto m=mixer();m->set_3d_sound_flag(0,0,4);m->user_sound_pan(0,0,0,100,2);
     check(m->sound_state(AudioMixer::se_voice(7)).pan_function==1,"SetUserSoundPan needs Set3dSoundFlag bit 4");
     auto none=mixer();none->user_sound_pan(0,0,0,100,2);check(none->sound_state(AudioMixer::se_voice(7)).pan_function==0,"SetUserSoundPan without flag bit 4 must not start a pan fade");
     m->mix(800*3);check(m->sound_state(AudioMixer::se_voice(7)).pan==100&&m->sound_state(AudioMixer::se_voice(7)).pan_function==0,"pan fade must end at PanLast after Count < 0");}
    {auto m=mixer();m->begin_room_init();m->regist_object_se(2,{0,0,-10},0x0305,0);m->exec_object_se();check(!m->active(8),"ExecObjectSeManager must hold during the init script");
     m->room_fade_in();m->exec_object_se();check(m->active(8)||!m->sequence_unavailable.empty(),"registered object SE must be requested after the init script");
     m->mix(800);m->free_object_se(2);m->mix(800*3);check(!m->active(8),"FreeObjectSe must stop the object's MIDI slot (SdrBgmStop at the next sdSysServer)");}
}
// sdfunc.c:2087 PlayVoiceEx2 / :2160 StopVoice / :2154 ContinuePlayVoice on ADX slot 1.
static void adx_voice(const std::filesystem::path& root){
    {AudioMixer m(root);m.set_sound_monitor({0,0,0},0,{0,0,-10});
     check(m.play_voice(0,{0,0,-10},0,0,false)&&m.active(1),"PlayVoiceEx2 mode 0 must start the VOICE slot");
     const auto pcm=m.mix(400);check(std::any_of(pcm.begin(),pcm.end(),[](auto v){return v!=0;}),"voice slot silent");
     const auto plays=m.played.size();
     check(m.play_voice(0,{0,0,-600},2,0,false),"mode 2 must report the parameter update");
     check(m.active(1)&&m.played.size()==plays,"mode 2 must retain the playing voice without a new request");}
    {AudioMixer normal(root),wait(root);normal.play_voice(0,{0,0,-10},0,0,false);wait.play_voice(0,{0,0,-10},0,0,true);
     check(normal.mix(800)==wait.mix(800),"VoiceWait must not invent a pause: ADXT_Pause is jr ra;nop in the ELF");
     wait.continue_voice();check(normal.mix(800)==wait.mix(800),"ContinuePlayVoice must retain the playback cursor");}
    {AudioMixer m(root);m.play_voice(0,{0,0,-10},0,0,false);m.stop_voice(30);m.mix(1600*8);
     check(!m.active(1)&&m.adx_state(1).volume==-127,"StopVoice(rate) must fade out to -127 and release the voice");
     check(m.play_voice(0,{0,0,-10},1,30,false),"mode 1 must start the voice");
     const auto begin=m.mix(1600);check(std::all_of(begin.begin(),begin.end(),[](auto v){return v==0;}),"mode 1 fade-in must start from the residual -127 volume");
     m.mix(1600*6);const auto late=m.mix(1600);check(std::any_of(late.begin(),late.end(),[](auto v){return v!=0;}),"mode 1 fade-in never became audible");
     m.stop_voice(0);check(!m.active(1),"StopVoice(0) must release immediately");}
    {AudioMixer m(root);check(!m.play_voice(60000,{0,0,-10},0,0,false)&&!m.unavailable.empty(),"a missing VOICE program must be reported, not silent");}
}
static void bgm_fade_contract(const std::filesystem::path& root){
    AudioMixer m(root);m.play_bgm_ex(0,120,-45);
    check(m.adx_state(0).volume==-127&&m.adx_state(0).volume_count==21&&m.adx_state(0).fade==1,"BGM start must install RequestAdxFadeFunction(121) to limit -25");
    m.mix(799);check(m.adx_state(0).volume==-127,"ADX fade must not interpolate before VSync");
    m.mix(1);check(std::abs(m.adx_state(0).volume-(-127.f+102.f/21.f))<.0001f&&m.adx_state(0).volume_count==20,"ADX first VSync must advance the float volume");
    m.mix(800*20);check(m.active(0)&&m.adx_state(0).volume_count==0&&m.adx_state(0).fade==1,"ADX fade must stay active at Count==0");
    m.mix(800);check(m.adx_state(0).volume==-25&&m.adx_state(0).fade==0,"ADX fade must finish at Count<0");
    m.stop_bgm(400);check(m.adx_state(0).volume_count==60,"BGM stop timer400 must use60 VSyncs, not60 game ticks");
    m.mix(800*60);check(m.active(0)&&m.adx_state(0).fade==2,"BGM stop must retain the voice through Count0");
    m.mix(800);check(!m.active(0)&&m.adx_state(0).volume==-127,"BGM stop must release at sync61");
    m.play_bgm_ex(0,120,-45);m.mix(800*3);const auto retained=m.adx_state(0);
    m.play_bgm_ex(0,-1,-10);check(m.adx_state(0).volume==retained.volume&&m.adx_state(0).volume_count==retained.volume_count&&m.adx_state(0).fade==retained.fade,"FadeInRate -1 must retain volume/limit/fade");
    m.stop_bgm(99);check(m.adx_state(0).volume==retained.volume&&m.adx_state(0).volume_count==29,"ordinary fade must retain fractional volume and timer99 count29");
    m.stop_bgm(100);check(m.adx_state(0).volume_count==15,"timer100 uses original discontinuous integer conversion");
    m.play_bgm_ex(0,120,-45);m.mix(800);const auto fraction=m.adx_state(0).volume;
    m.set_3d_sound_flag(10,0,2);m.user_sound_volume(10,0,-1,-30,42);
    check(m.adx_state(0).volume==-45&&m.adx_state(0).volume_count==42,"Set3dSoundFlag10 restores CurrentBgmVolume; Ex fade has its own frame count");
    m.mix(800);m.user_sound_volume(10,0,-1,-50,2);
    check(m.adx_state(0).volume==float(int(-45.f+15.f/42.f)),"ADX Ex sentinel must truncate the current float volume toward zero");
    (void)fraction;
}
// MODHSYN.IRX note-on (.text 0x5f90): the split loop (0x6098..0x64a0) does not leave after a hit. Every split
// whose key range (+2..+4 without bit 7) holds the key runs its Sset sample loop (0x615c..0x646c) and keys a
// voice (0x6264 voice, 0x63d0 key-on), then the loop steps to the next split. RM_1100 (MULTSPQ1 pack 1100)
// bank 2 seq 0 is that room's bhEasySESet type 7 (CallNativeEventSe slot 0, SeNo 512): programs 0/1 each hold
// two splits over keys 12..119 with pan 0 and pan 127, keyed together at key 60. Before only the first split
// sounded (right channel silent). The sequence must equal the sum of the two packs that leave one split each.
static void midi_multi_split(const std::filesystem::path& root){
    const auto index=read_asset_file(root/"files/MULTSPQ1.IDX",4096);BinaryView ids(index);
    unsigned pack=0;while(pack<index.size()/2&&ids.u16(pack*2)!=1100)++pack;check(pack<index.size()/2,"RM_1100 sound pack missing");
    const auto bytes=read_asset_file(root/"archives/MULTSPQ1"/(asset_number(pack)+".bin"),32*1024*1024);BinaryView spq(bytes);
    std::size_t bank_at=0,bank_size=0;
    for(unsigned i=0;i<16&&spq.u32(i*16);++i)if(spq.u32(i*16+8)==1&&spq.u32(i*16+12)==2){bank_at=spq.u32(i*16);bank_size=spq.u32(i*16+4);spq.require(bank_at,bank_size);break;}
    check(bank_size!=0,"RM_1100 MIDI program bank 2 missing");
    const std::vector<std::uint8_t> programs(bytes.begin()+std::ptrdiff_t(bank_at),bytes.begin()+std::ptrdiff_t(bank_at+bank_size));
    BinaryView hd(programs);const std::size_t vers=hd.u32(0),head=vers+hd.u32(vers+8),prog=vers+hd.u32(head+0x14);
    auto zones=[&](unsigned program){const std::size_t pg=prog+hd.u32(prog+16+program*4);check(hd.u8(pg+4)==2&&hd.u8(pg+5)==20,"RM_1100 program is not the two-split pair");return pg+hd.u32(pg);};
    for(unsigned program:{0u,1u}){const auto voices=decode_system_note(programs,program,60);
        check(voices.size()==2&&voices[0].frames()&&voices[1].frames()&&voices[0].pan<voices[1].pan,"RM_1100 key 60 does not key one voice per covering split");
        reject([&]{decode_system_sound(programs,program,60);});}
    const auto mods=std::filesystem::temp_directory_path()/("recvx-multisplit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    // keep < 0: the original pack; otherwise the other split of programs 0/1 covers no key (+2 = 127, +4 = 0).
    auto run=[&](int keep,unsigned& voices){
        std::filesystem::remove_all(mods);
        if(keep>=0){auto copy=bytes;for(unsigned program:{0u,1u}){const auto z=bank_at+zones(program)+std::size_t(1-keep)*20;copy.at(z+2)=127;copy.at(z+4)=0;}
            std::filesystem::create_directories(mods/"audio/MULTSPQ1");std::ofstream(mods/"audio/MULTSPQ1"/(asset_number(pack)+".bin"),std::ios::binary).write(reinterpret_cast<const char*>(copy.data()),std::streamsize(copy.size()));}
        AudioMixer m(root,mods);m.load_room_sounds(1,10);check(m.room_fx_level()==0,"RM_1100 RoomFxLevel changed: the HALL would no longer be silent");
        check(m.event_sound(0,512)&&m.active(12)&&m.sequence_unavailable.empty(),"RM_1100 type-7 sequence not played");
        auto pcm=m.mix(midi_tick0+96*50);voices=m.spu_voices(0)+m.spu_voices(1);const auto rest=m.mix(48000*2);pcm.insert(pcm.end(),rest.begin(),rest.end());return pcm;};
    unsigned both=0,left=0,right=0;const auto full=run(-1,both),first=run(0,left),second=run(1,right);std::filesystem::remove_all(mods);
    check(left==2&&right==2&&both==4,"RM_1100: two notes must key two voices each (one per covering split)");
    check(std::any_of(second.begin(),second.end(),[](auto n){return n!=0;}),"RM_1100 second split silent");
    // Voices add in the float mix before one int16 conversion: at most 1 LSB apart from the summed single-split outputs.
    for(std::size_t i=0;i<full.size();++i)check(std::abs(int(full[i])-int(first[i])-int(second[i]))<=1,"RM_1100 sequence is not the sum of its two splits");
}
int main(int argc,char** argv){try{
    sound_3d_core();
    check(sony_pan_factors(32).right==62&&sony_pan_factors(96).left==62&&sony_pan_factors(64).left==128&&sony_pan_factors(64).right==128,"original Sony integer pan factors");
    check(sony_bank_pan(64,64,64,0)==64&&sony_bank_pan(-64,64,32,0)==32,"original program/split/sample signed bank pan");
    check(sony_note_pitch(60,0,60,0)==4096&&sony_note_pitch(60,0,48,0)==2048&&sony_note_pitch(60,0,72,0)==8192,"original Sony octave pitch table");
    check(sony_note_pitch(60,0,60,64)==4216&&sony_note_pitch(60,0,59,0)==3866,"original Sony fine/semitone pitch table");
    check(sony_sample_pitch(48000,60,60,-1)==4094&&sony_sample_pitch(32000,60,60,0)==2730,"negative fine normalization and sample-rate scaling");
    if(argc>1){weapon_se(std::filesystem::u8path(argv[1]));enemy_se(std::filesystem::u8path(argv[1]));shared_3d_consumers(std::filesystem::u8path(argv[1]));adx_voice(std::filesystem::u8path(argv[1]));bgm_fade_contract(std::filesystem::u8path(argv[1]));std::cout<<"Weapon SE slots 8/9/19, enemy SE manager, Get3DSoundParameter, 3D flags, syukan, room FX and player voice position passed.\n";}
    if(argc>2&&std::string(argv[2])=="sample-loops"){sample_loops(std::filesystem::u8path(argv[1]));std::cout<<"Original Sony sample loops, NoteOff, NRPN, stop, fade, room replacement and Vagi attributes passed.\n";return 0;}
    if(argc>1){sample_loops(std::filesystem::u8path(argv[1]));spu_envelope_reverb(std::filesystem::u8path(argv[1]));}
    static_assert(game_tick_hz==30&&audio_frames_per_tick==1600);
    {AudioMixer clock;auto second=std::make_shared<PcmClip>();second->rate=48000;second->channels=1;second->samples.assign(48000,100);clock.play_pcm(0,second);unsigned produced=0;for(unsigned tick=0;tick<30;++tick){auto samples=clock.mix();produced+=samples.size()/2;check(samples.front()==100&&samples.back()==100,"audio clock loses or advances samples");}check(produced==48000,"30 engine ticks do not represent one audio second");clock.mix(1);check(!clock.active(0),"one-second voice survives 30 engine ticks");}
    std::vector<std::uint8_t> spu(16);spu[0]=12;spu[1]=1;spu[2]=0xf1;auto ps=decode_ps_adpcm(spu,32000);check(ps.channels==1&&ps.frames()==28&&ps.samples[0]==1&&ps.samples[1]==-1,"SPU nibble order/sign/shift");
    {std::vector<std::uint8_t> blocks(4*16);for(unsigned i=0;i<4;++i){blocks[i*16]=12;std::fill_n(blocks.begin()+i*16+2,14,std::uint8_t((i+1)*17));}blocks[17]=6;blocks[49]=3;
        const auto loop=decode_ps_adpcm(blocks,48000);check(loop.loop_start==28&&loop.loop_end==112&&loop.frames()==112&&loop.samples.back()==4,"SPU explicit loop bounds/end block missing");
        AudioMixer wave;wave.play_pcm(0,std::make_shared<PcmClip>(loop),1,true);const auto output=wave.mix(200);for(unsigned i=0;i<200;++i){const auto at=i<112?i:28+(i-112)%84;check(output[i*2]==loop.samples[at]&&output[i*2+1]==loop.samples[at],"waveform loop repeats intro or omits end block");}
        blocks[49]=1;const auto shot=decode_ps_adpcm(blocks,48000);check(!shot.loop_end&&shot.frames()==112,"normal SPU end creates a loop");blocks[17]=0;blocks[49]=3;reject([&]{decode_ps_adpcm(blocks,48000);});}
    auto malformed=spu;malformed[0]=0x50;reject([&]{decode_ps_adpcm(malformed,32000);});malformed=spu;malformed.pop_back();reject([&]{decode_ps_adpcm(malformed,32000);});
    if(argc>1){ // CORE character banks: original CallPlayerVoice bank 4, slot 7; replacement and room retention.
        const auto root=std::filesystem::u8path(argv[1]);
        for(unsigned disc:{0u,1u}){AudioMixer voices(root);voices.set_disc(disc);voices.load_room_sounds(0,2);
            std::vector<std::int16_t> previous;
            for(unsigned player=0;player<4;++player){check(voices.load_player_sounds(player),"original CORE bank missing");voices.mix(800*3);check(!voices.active(7),"character change keeps old voice");
                voices.play_player_voice(1026);check(voices.active(7),"bite voice not routed to slot 7");
                const auto pcm=voices.mix(1600);check(std::any_of(pcm.begin(),pcm.end(),[](auto n){return n!=0;}),"CORE bite voice silent");
                if(!previous.empty())check(pcm!=previous,"character change retains cached CORE sample");previous=pcm;
                const auto route=voices.played.back();voices.load_room_sounds(0,9);voices.play_player_voice(1025);
                check(voices.active(7)&&voices.played.back().substr(0,voices.played.back().rfind('/'))==route.substr(0,route.rfind('/')),"room change loses CORE bank/provenance");
                const auto death=voices.mix(1600);check(std::any_of(death.begin(),death.end(),[](auto n){return n!=0;}),"CORE death voice silent");}
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
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto bank=read_asset_file(root/"files/COMMON.MLT",8*1024*1024);for(unsigned id=0;id<11;++id){const auto sound=decode_system_sound(bank,id);check(sound.rate==32000&&sound.frames()>0&&std::any_of(sound.samples.begin(),sound.samples.end(),[](auto value){return value!=0;}),"original system sound silent/invalid");}reject([&]{decode_system_sound(bank,11);});auto truncated=bank;truncated.resize(256);reject([&]{decode_system_sound(truncated,0);});AudioMixer effects(root);effects.play_system(2);check(effects.active(2)&&effects.played.back()=="COMMON/0002","original cursor sound not routed");effects.load_room_sounds(0,0);for(unsigned id=0;id<5;++id){check(effects.play_room_sound(2,id,4),"original floor bank missing");const auto output=effects.mix(1600);check(std::any_of(output.begin(),output.end(),[](auto value){return value!=0;}),"original floor sound silent");}check(effects.played.back()=="MULTSPQ1/0000/2/0004","floor sound route mismatch");effects.load_room_sounds(0,1);effects.mix(800*3); /* the replaced TQ bank cancels its entries at the next ATick, reported two frames later */check(!effects.active(4)&&effects.play_room_sound(2,2,5)&&effects.played.back()=="MULTSPQ1/0001/2/0002","room sound reload incorrect");check(!effects.play_room_sound(15,0),"missing bank fabricated");check(!effects.play_room_sound(2,100)&&!effects.unavailable.empty(),"a program above the TSNDDRV table must be dropped and reported");}

    // ps2_McSaveFile.c:577/CallSystemSeBasic: typing uses MIDI bank 2 lists 66..68, not COMMON SE, on MidiHandle[2]
    // (first sdMidiPlay rotates port 2 -> 8, host voice 28).
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);for(unsigned id:{0x80000242u,0x80000243u,0x80000244u}){
        AudioMixer typing(root),reference(root);typing.load_room_sounds(0,1);reference.load_room_sounds(0,1);
        reference.play_system_midi(id);typing.play_system(id);
        check(typing.active(typing.door_voice())&&typing.door_voice()==28&&typing.played.back()=="SYSTEM/MIDI/2/"+std::to_string(id&255u),"typewriter high-bit sound not routed to system MIDI");
        const auto pcm=typing.mix(8000);check(pcm==reference.mix(8000),"typewriter system dispatch differs from ExPlayMidi path");
        check(std::any_of(pcm.begin(),pcm.end(),[](auto sample){return sample!=0;}),"typewriter MIDI PCM is silent");
        check(typing.sequence_unavailable.empty(),"typewriter MIDI list unavailable");}}

    if(argc>1){AudioMixer enemies(std::filesystem::u8path(argv[1]));enemies.load_room_sounds(0,2);for(unsigned slot=0;slot<6;++slot)check(enemies.enemy_sound(slot,0)&&enemies.active(18+slot),"original enemy bank or independent slot missing");check(enemies.played.back()=="MULTSPQ1/0002/3/0000","enemy bank/list selection incorrect");enemies.mix(800);enemies.stop_enemy_sound(2);enemies.mix(800*3);for(unsigned slot=0;slot<6;++slot)check(enemies.active(18+slot)==(slot!=2),"enemy sound stop steals another voice");const auto samples=enemies.mix(100);check(std::any_of(samples.begin(),samples.end(),[](auto n){return n!=0;}),"enemy PCM silent");/* The original leaves a room through Com_ExecRoomFadeOut: RequestSoundFade(2, 0xA3, 400) fades the enemy voices (attribute 2) over 60 VSyncs and StopSe ends them at -127; the bank load itself stops nothing on the EE. */enemies.room_fade_out();enemies.mix(800*64);check(!enemies.active(18),"Com_ExecRoomFadeOut must fade and stop the enemy voices");enemies.load_room_sounds(0,0);reject([&]{enemies.enemy_sound(6,0);});}
    // RM_9340 bank 3 programs 60..67 carry no split. The mdlver-38 zombie (ene01_typ_tbl type 18,
    // add_seno 40) requests list 63 there: an original silent voice, not an unavailable sound.
    if(argc>1){AudioMixer silent(std::filesystem::u8path(argv[1]));silent.set_disc(1);silent.load_room_sounds(9,34);
        silent.request_enemy_se(3,{},0x0101131c);silent.exec_enemy_se();check(silent.active(18),"RM_9340 zombie voice 28 missing");
        silent.request_enemy_se(1,{},0x0101133f);silent.exec_enemy_se();check(silent.unavailable.empty(),"split-less enemy program reported unavailable");
        // TSNDDRV keys the TQ entry (its table is built from split 0's Sset index whatever the split count), MODHSYN
        // starts no voice: the EE sees the slot busy, nothing sounds on it.
        silent.mix(800*2);check(silent.played.back()=="MULTSPQ2/0083/3/0063"&&silent.sequence_levels(19).empty()&&silent.unavailable.empty(),"split-less enemy program not served silently");}
    // RM_9370 (MULTSPQ2 pack 9370) MIDI bank 2, the sequences of event 11's bhEasySESet type 7 (CallNativeEventSe):
    // MODHSYN.IRX program change (.text 0x75c) keeps no program for an empty table entry (0xffffffff: programs
    // 23/24/27..30) or a number above the last index (39), note-on (0x5f90) then starts no voice; the split loop
    // (0x6098) compares key bounds &0x7f and a key no split covers starts no voice either (program 11: key 60 only).
    // The sequence runs on in its MIDI slot. Before: seq 11 "no split for key", seq 23/28/29 "entry outside section".
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto index=read_asset_file(root/"files/MULTSPQ2.IDX",4096);BinaryView ids(index);
        unsigned pack=0;while(pack<index.size()/2&&ids.u16(pack*2)!=9370)++pack;check(pack<index.size()/2,"RM_9370 sound pack missing");
        const auto bytes=read_asset_file(root/"archives/MULTSPQ2"/(asset_number(pack)+".bin"),32*1024*1024);BinaryView spq(bytes);std::vector<std::uint8_t> programs;
        for(unsigned i=0;i<16&&spq.u32(i*16);++i)if(spq.u32(i*16+8)==1&&spq.u32(i*16+12)==2){const auto offset=spq.u32(i*16),size=spq.u32(i*16+4);spq.require(offset,size);programs={bytes.begin()+offset,bytes.begin()+offset+size};break;}
        check(!programs.empty(),"RM_9370 MIDI program bank 2 missing");
        check(decode_system_sound(programs,11,60).frames()>0,"RM_9370 bank 2 program 11 key 60 (its split) silent");
        check(decode_system_sound(programs,11,61).frames()==0,"key outside every split of program 11 keyed a voice");
        for(const unsigned empty:{23u,24u,27u,28u,29u,30u})check(decode_system_sound(programs,empty,60).frames()==0,"empty program entry keyed a voice");
        check(decode_system_sound(programs,40,60).frames()==0,"program above the table's last index keyed a voice");
        AudioMixer room(root);room.set_disc(1);room.load_room_sounds(9,37);
        check(room.event_sound(0,523)&&room.active(12)&&room.played.back()=="MULTSPQ2/"+asset_number(pack)+"/2/seq11/prog11","RM_9370 seq 11 not played");
        const auto sounding=room.mix(48000);check(std::any_of(sounding.begin(),sounding.end(),[](auto n){return n!=0;}),"RM_9370 seq 11 key 60 silent");
        for(const unsigned seq:{23u,28u,29u}){AudioMixer quiet(root);quiet.set_disc(1);quiet.load_room_sounds(9,37);
            check(quiet.event_sound(0,0x200|seq)&&quiet.active(12)&&quiet.played.back()=="MULTSPQ2/"+asset_number(pack)+"/2/seq"+std::to_string(seq)+"/prog"+std::to_string(seq),"RM_9370 sequence on an empty program not run");
            const auto pcm=quiet.mix(4800);check(std::all_of(pcm.begin(),pcm.end(),[](auto n){return n==0;}),"empty program sounded");
            check(quiet.sequence_unavailable.empty()&&quiet.unavailable.empty(),"empty program reported unavailable");}
        check(room.sequence_unavailable.empty()&&room.unavailable.empty(),"RM_9370 seq 11 reported unavailable");}
    if(argc>1)midi_multi_split(std::filesystem::u8path(argv[1]));

    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);const auto index=read_asset_file(root/"files/MULTSPQ1.IDX",4096);BinaryView ids(index);
        // COMMON SQ bank 1 uses key 60 and velocities 120/110/120/120.
        // Compare actual stereo PCM against the independently decoded HD split,
        // including a room load before the closing sample is first mixed.
        for(unsigned bank:{2u,7u}){unsigned pack=0;while(pack<index.size()/2&&ids.u16(pack*2)!=(0x8000|bank))++pack;check(pack<index.size()/2,"original door SPQ not found");const auto bytes=read_asset_file(root/"archives/MULTSPQ1"/(asset_number(pack)+".bin"),32*1024*1024);BinaryView spq(bytes);std::vector<std::uint8_t> programs;
            for(unsigned i=0;i<16&&spq.u32(i*16);++i)if(spq.u32(i*16+8)==1&&spq.u32(i*16+12)==1){const auto offset=spq.u32(i*16),size=spq.u32(i*16+4);spq.require(offset,size);programs={bytes.begin()+offset,bytes.begin()+offset+size};break;}
            for(unsigned list=0;list<(bank==2?2u:3u);++list){AudioMixer door(root),reference;door.load_door_sounds(bank);check(door.play_door_sound(list),"COMMON door sequence unresolved");auto clip=std::make_shared<PcmClip>(decode_system_sound(programs,list,60));clip->volume=1;clip->pan=64;reference.play_pcm(27,clip);door.load_room_sounds(0,6);const std::size_t note_at=midi_tick0+200;auto actual=door.mix(note_at+1);const auto lv=door.sequence_levels(door.door_voice());const auto rest=door.mix(1600-1);actual.insert(actual.end(),rest.begin(),rest.end());const auto pcm=reference.mix(1600);const auto ref_lv=reference.sequence_levels(27); // COMMON: NoteOn tick 6 -> the ATick of frame 1200 (ticks 5..8)
                check(lv.size()==1&&ref_lv.size()==1,"door note register level missing");
                // MODHSYN: velocity 120/110 enters the note level, (bank * velocity) >> 14 (0x38c8); the decoded split is centred.
                const auto bank_volume=decode_system_sound(programs,list,60).sony_volume;const auto want=sony_voice_levels(bank_volume,list==1?110:120,sony_controller_scale(sony_midi_cc7(127,127)),128,64,64);
                check(lv[0].left==want.left&&lv[0].right==want.right,(std::string("door register level ")+std::to_string(lv[0].left)+" != "+std::to_string(want.left)).c_str());
                check(std::all_of(actual.begin(),actual.begin()+note_at*2,[](auto value){return value==0;}),"door note before its tick");
                check(std::any_of(actual.begin(),actual.end(),[](auto value){return value!=0;}),"door PCM silent after destination load");check(scaled_like(actual,note_at*2,pcm,200*2,lv[0],ref_lv[0]),"door key/velocity/program/pan/timing differs from COMMON SQ");}
        }
        // sdMidiPlay rotates MidiHandle[2] to the next port before it plays, so the closing voice
        // is the port the handle names afterwards; Com_ExecRoomFadeIn fades that port over 250.
        AudioMixer tail(root);tail.load_door_sounds(7);const unsigned before=tail.door_voice();check(tail.play_door_sound(2)&&tail.door_voice()!=before&&tail.active(tail.door_voice()),"closing door voice missing");const unsigned closing_voice=tail.door_voice();tail.load_room_sounds(0,7);tail.room_fade_in();check(tail.active(closing_voice),"room completion cuts closing door voice immediately");const auto closing=tail.mix(1600);check(std::any_of(closing.begin(),closing.end(),[](auto value){return value!=0;}),"closing door fade starts silent");tail.mix(48000*2);check(!tail.active(closing_voice),"completed closing voice not released");
    }
    if(argc>1){AudioMixer detector(std::filesystem::u8path(argv[1]));detector.load_room_sounds(0,9);detector.custom_midi_slots(1,4);
        check(detector.event_sound(4,0x202),"original detector SQ missing");
        for(unsigned cycle=0;cycle<5;++cycle){const auto pcm=detector.mix(60000);check(detector.active(8)&&std::any_of(pcm.begin(),pcm.end(),[](auto sample){return sample!=0;}),"Sony NRPN alarm loop stopped or remained silent");}
        check(detector.stop_event_sound(4),"script alarm stop ignored");detector.mix(2400);check(!detector.active(8)&&!detector.iop().sequence_playing(3),"SdrBgmStop must stop MODMIDI port 3 and free the EE slot");
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
