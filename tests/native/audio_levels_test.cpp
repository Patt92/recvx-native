// SE-Shot-Pegel nach TSNDDRV.IRX/MODHSYN.IRX (nur Lesereferenz, eigene Disassemblierung 2026-10-21):
//  - Notenpegel (MODHSYN 0x38c8): +22 = (Programm +6 * Split +0x10 * Sample +0x10 * Velocity) >> 14;
//    Velocity-Kurve 0, kein Crossfade (0x4680), kein Amplituden-LFO in allen Spielbaenken.
//  - CC7/CC11 (0x978/0x9d4): (Wert << 7) / 127; Stimme +24 = (CC7 * CC11 * +22) >> 14 (0x426c);
//    Ausgabe 0x40f0: VOLL/VOLR = min(16383, (Panfaktor * ((+24 * 256) >> 8)) >> 7).
//  - Pan (CC10 0x0c18, 0x4534): CC10 - 64, Klemme -63..63, plus Bankpan - 64; >= 63 links stumm, <= -63 rechts.
//  - TSNDDRV: SdrSeReq setzt fehlende Lautstaerke auf 127 (0xdc4); SdrSeChg behaelt sie (0xca0/0x1508),
//    speichert Pan 64 ohne CC10 (0x1588) und Bend 8192 ohne Pitchbend (0x1670). EE: SdrSeReq/Chg
//    melden vol als char nur gueltig, wenn (Sint8)Vol + 128 < 128 (CheckCmdReq/Chg, macros.h:24).
//
// Gegenproben (je eine Zeile, zurueck und touch):
//   audio_pan.cpp CC10-Untergrenze und 0x4534-Klemme beide -64 (einzeln verdeckt je die andere) -> "Bankpan 127 + CC10 0"
//   audio.cpp decode_program_voices: Sample +0x10 aus sony_volume entfernt  -> "Sample +0x10"
//   audio.cpp shot_volume: `else if(c.shot_request)` -> `else`               -> "Chg behaelt CC7"
//   audio.cpp shot_pan: Bedingung `value!=64` entfernt                       -> "Chg Pan 64"
//   audio.cpp sound_sync: shot_request-Ruecksetzen entfernt                  -> "Chg behaelt CC7"
//   audio_pan.cpp sony_midi_cc7: Faktor 128 entfernt                         -> "MODMIDI 0xf30"
//   audio.cpp sync_midi_port: CC7-Erneuerung bei Portlautstaerke entfernt     -> "Fadestufe"
//   audio.cpp mix: atick() nicht aufgerufen                                  -> "ATick 400"
//   audio.cpp atick: auch bei EE-Pan 64 Offset 0 setzen                      -> "EE-Pan 64"
//   audio.cpp play_room_sequence: bend.fill(0) beim Req                      -> "SdrBgmReq ... Bend"
//   audio.cpp background_sound: Slot 2 auf 24+slot statt rotate_midi2        -> "Hintergrund-Slot 2"
//   audio.cpp key_on: Poolmeldung entfernt                                   -> "25. Stimme"
#include "audio.h"
#include "audio_pan.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
static bool same(SonyLevels a,SonyLevels b){return a.left==b.left&&a.right==b.right;}
static std::string text(SonyLevels l){return std::to_string(l.left)+"/"+std::to_string(l.right);}
// EE -> TSNDDRV: (Sint8)Volume + 128 als char, gueltig nur unter 128.
static unsigned req_cc7(float volume){const unsigned v=std::uint8_t(int(std::int8_t(int(volume)))+128);return v<128?v:127;}
static unsigned cc10(float pan){return unsigned(int(std::int8_t(int(pan)))/2+64);}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");const std::filesystem::path root=std::filesystem::u8path(argv[1]);
    // --- Registerarithmetik ---------------------------------------------------------------------
    check(sony_controller_scale(127)==128&&sony_controller_scale(64)==64&&sony_controller_scale(1)==1&&sony_controller_scale(0)==0,"CC7/CC11-Skalierung (v << 7) / 127");
    const std::uint32_t full=127u*127u*127u;
    {const auto l=sony_voice_levels(full,127,128,128,64,64);check(l.left==15877&&l.right==15877,"voller Pegel 15877/15877, ist "+text(l));}
    {const auto l=sony_voice_levels(full,64,128,128,64,64);check(l.left==8001&&l.right==8001,"Velocity 64: (2048383 * 64) >> 14 = 8001, ist "+text(l));}
    {const auto l=sony_voice_levels(full,127,sony_controller_scale(64),128,64,64);check(l.left==7938&&l.right==7938,"CC7 64: (64 * 128 * 15877) >> 14 = 7938, ist "+text(l));}
    {const auto l=sony_voice_levels(full,127,128,128,64,127);check(l.left==0&&l.right==15877,"CC10 127 -> links stumm, ist "+text(l));}
    {const auto l=sony_voice_levels(full,127,128,128,64,0);check(l.left==15877&&l.right==0,"CC10 0 -> rechts stumm, ist "+text(l));}
    {const auto l=sony_voice_levels(full,127,128,128,64,96);check(l.left==(((63-32)<<7)/63*15877>>7)&&l.right==15877,"CC10 96: L = ((63 - 32) << 7) / 63, ist "+text(l));}
    // CC10 0 ergibt -64 und wird auf -63 geklemmt: mit Bankpan 127 liegt die Stimme genau in der Mitte.
    {const auto l=sony_voice_levels(full,127,128,128,127,0);check(l.left==15877&&l.right==15877,"Bankpan 127 + CC10 0 muss mittig sein, ist "+text(l));}
    // --- Raumbank mit Sample-Lautstaerke 100 (RM_0021 = MULTSPQ1/0003, Bank 2 Programm 0: 80/127/100) ---
    {AudioMixer m(root);m.load_room_sounds(0,2,1);m.set_sound_monitor({0,0,0},0,{});
     m.request_enemy_se(0,{0,0,-10},0x00000200);m.exec_enemy_se();
     const auto clip=m.voice_clip(18);check(m.active(18)&&clip,"Gegner-SE Bank 2 Liste 0 nicht gestartet");
     check(clip->sony_volume==80u*127u*100u,"Sample +0x10 (100) fehlt im Notenpegel: "+std::to_string(clip->sony_volume));
     const auto s=m.sound_state(18);
     const auto expect=sony_voice_levels(80u*127u*100u,127,sony_controller_scale(req_cc7(s.volume)),128,int(clip->pan),cc10(s.pan));
     check(same(m.voice_levels(18),expect),"Req-Pegel "+text(m.voice_levels(18))+" statt "+text(expect));}
    // --- SdrSeReq gegen SdrSeChg: ungueltige Lautstaerke, Pan 64 ------------------------------------
    auto start=[&](AudioMixer& m){m.load_room_sounds(0,2,1);m.set_sound_monitor({0,0,0},0,{});m.request_enemy_se(0,{-300,0,-300},0x00000200);m.exec_enemy_se();check(m.active(18),"Gegner-SE fern nicht gestartet");};
    {AudioMixer m(root);start(m);const auto s=m.sound_state(18);
     check(req_cc7(s.volume)<127,"Vorbedingung: ferne Quelle muss gedaempft sein (Volume "+std::to_string(int(s.volume))+")");
     check(cc10(s.pan)!=64,"Vorbedingung: seitliche Quelle braucht Pan != 64");
     const auto before=m.voice_levels(18);m.mix(800); // sdSysServer: der Req ist uebertragen
     m.set_3d_sound_flag(5,0,2); // SetVolumeSe(DefEne[0], 0): (Sint8)0 + 128 = 128 -> char -128, ungueltig
     check(int(m.sound_state(18).volume)==0,"Set3dSoundFlag 0x2 schreibt Volume 0");
     check(same(m.voice_levels(18),before),"Chg behaelt CC7: "+text(m.voice_levels(18))+" statt "+text(before));
     m.set_3d_sound_flag(5,0,4); // SetPanSe(DefEne[0], 0): 0 / 2 + 64 = 64 -> Tsnd_tqchg ohne CC10
     check(same(m.voice_levels(18),before),"Chg Pan 64 darf den Kanal-Pan nicht aendern: "+text(m.voice_levels(18)));}
    {AudioMixer m(root);start(m);const auto clip=m.voice_clip(18);
     m.set_3d_sound_flag(5,0,6); // vor dem ersten sdSysServer: Teil des SdrSeReq
     const auto expect=sony_voice_levels(clip->sony_volume,127,128,128,int(clip->pan),64);
     check(same(m.voice_levels(18),expect),"Req mit ungueltiger Lautstaerke = 127 und Pan 64: "+text(m.voice_levels(18))+" statt "+text(expect));}
    // --- MIDI: MODMIDI CC7 * Portlautstaerke, HsMsg-Pan, Bend je Port, MidiHandle[2] -----------------------------------
    check(sony_midi_cc7(127,127)==126&&sony_midi_cc7(64,127)==63&&sony_midi_cc7(127,128)==127&&sony_midi_cc7(127,1)==0,"MODMIDI 0xf30: min(127, CC7 * 128 * vol >> 14)");
    {// COMMON-Tuerliste 1: Velocity 110, Sequenz-CC7 127, Mitte. Com_ExecRoomFadeIn faedet MidiInfo[2]; jede Stufe geht
     // ueber sdMidiSetVol/SdrBgmChg als Portlautstaerke an MODMIDI und erneuert den CC7 aller Kanaele.
     AudioMixer m(root);m.load_door_sounds(7);check(m.play_door_sound(1),"Tuerliste 1");m.mix(301);
     const auto full=m.sequence_levels(m.door_voice());check(full.size()==1&&full[0].left==full[0].right,"Tuernote fehlt");
     m.room_fade_in();m.mix(800*10);const auto st=m.sound_state(2,true);const unsigned u=unsigned(std::clamp(int(std::int8_t(int(st.volume)))+128,0,127));
     check(u<127&&st.fade==2,"MidiInfo[2]-Fade laeuft nicht");
     const auto faded=m.sequence_levels(m.door_voice());check(faded.size()==1,"Tuernote nach Fade");
     // Mitte, CC11 128: VOLL = (128 * ((CC7 * 128 * Note) >> 14)) >> 7; Notenpegel aus dem vollen Register zurueckgerechnet.
     const unsigned cc_full=sony_controller_scale(sony_midi_cc7(127,127)),cc_faded=sony_controller_scale(sony_midi_cc7(127,u));
     std::uint32_t note=0;for(std::uint32_t n=1;n<65536&&!note;++n)if(((128u*((cc_full*128u*n)>>14))>>7)==full[0].left)note=n;
     check(note>0,"Notenpegel nicht rekonstruierbar");
     const std::uint32_t expect=(128u*((cc_faded*128u*note)>>14))>>7;
     check(faded[0].left==expect&&faded[0].right==expect,"Fadestufe u="+std::to_string(u)+" ueber MODMIDI-CC7: "+text(faded[0])+" statt "+std::to_string(expect));}
    {// HsMsg-Pan: CallNativeEventSe Mode 0 mit seitlicher Quelle setzt das EE-Pan != 64. Die Note (Takt 350) startet mit
     // dem Kanal-CC10 (keine Sequenz setzt CC10: Mitte); erst der naechste 240-Hz-ATick (alle 200 Bilder) setzt alle
     // Stimmen des Ports auf Pan - 64. Ein spaeteres EE-Pan 64 schickt nichts mehr: die Stimmen behalten den Pan.
     AudioMixer m(root);m.load_room_sounds(0,2);m.set_sound_monitor({0,0,0},0,{});
     check(m.event_sound(0,0x206,{200,0,0},0),"Ereignis-SE 0x206 Mode 0");m.mix(360);
     const auto start=m.sequence_levels(12);check(start.size()==1&&start[0].left==start[0].right,"Note vor dem ATick muss mittig sein: "+(start.empty()?std::string("-"):text(start[0])));
     m.mix(50);const auto moved=m.sequence_levels(12);check(moved.size()==1&&moved[0].left!=moved[0].right,"ATick 400 muss den HsMsg-Pan setzen: "+(moved.empty()?std::string("-"):text(moved[0])));
     m.set_3d_sound_flag(7,0,4); // SetPanMidi(DefEvt[0], 0, 0): EE-Pan 64
     m.mix(800);const auto kept=m.sequence_levels(12);check(kept.size()==1&&same(kept[0],moved[0]),"EE-Pan 64 darf den Stimmenpan nicht zuruecksetzen: "+(kept.empty()?std::string("-"):text(kept[0])));}
    {// Bend bleibt am Port: RM_1140 Bank 2 Liste 1 endet auf Kanal 0 mit Bend 8396 (+204); MODHSYN setzt Kanaele nur beim
     // Modulstart zurueck (0x6e0), SdrBgmReq nicht. Ereignisslot 0 = MidiHandle[7] = Port 7.
     AudioMixer m(root);m.load_room_sounds(1,14,0);m.set_sound_monitor({0,0,0},0,{});check(m.event_sound(0,1),"RM_1140 Liste 1");
     for(int i=0;i<1200&&m.midi_port_bend(7,0)!=204;++i)m.mix(480);check(m.midi_port_bend(7,0)==204,"Bendende +204 fehlt: "+std::to_string(m.midi_port_bend(7,0)));
     check(m.stop_event_sound(0)&&!m.active(12),"StopNativeEventSe");
     check(m.event_sound(0,0)&&m.midi_port_bend(7,0)==204,"SdrBgmReq darf den MODHSYN-Bend nicht zuruecksetzen");}
    {// MidiHandle[2]: Hintergrund-Slot 2, System-MIDI und CallDoorSe teilen die Portrotation 2 -> 8 -> 9; jedes sdMidiPlay
     // stoppt den bisher benannten Port. Der Host-Tuer-Skip stoppt nur Tuerlisten.
     AudioMixer m(root);m.load_room_sounds(0,3,0);m.load_door_sounds(7);check(m.background_sound(2,0),"Hintergrund-Slot 2");
     const unsigned bg=m.door_voice();check(bg==28&&m.active(bg),"Hintergrund-Slot 2 muss auf MidiHandle[2] (Port 8) spielen");
     check(m.play_door_sound(1),"Tuerliste 1");const unsigned door=m.door_voice();check(door==29&&m.active(door)&&!m.active(bg),"CallDoorSe muss den Port von Hintergrund-Slot 2 stoppen");
     check(m.background_sound(2,0),"Hintergrund-Slot 2 erneut");const unsigned again=m.door_voice();m.stop_door_voices();
     check(m.active(again)&&!m.active(door),"Tuer-Skip darf nur Tuerlisten stoppen");}
    {// MODHSYN-Stimmenpool: 24 Stimmen je Kern; Kern 0 erzwungen (Sample-SPU-Attribut 0x10). Die 25. Stimme muss als
     // nicht portierter Raub gemeldet werden, 24 nicht.
     AudioMixer m;auto clip=std::make_shared<PcmClip>();clip->rate=48000;clip->channels=1;clip->samples.assign(48000,1000);clip->spu=true;clip->spu_attr=0x13;clip->adsr1=0x00ff;clip->adsr2=0x1fc0;
     for(unsigned v=0;v<24;++v)m.play_pcm(v,clip);check(m.spu_voices(0)==24&&m.unavailable.empty(),"24 Stimmen auf Kern 0 duerfen nichts melden");
     m.play_pcm(24,clip);check(!m.unavailable.empty()&&m.unavailable.back().find("voice pool full on core 0")!=std::string::npos,"25. Stimme auf Kern 0 nicht gemeldet");}
    std::cout<<"audio levels PASS (MODHSYN VOLL/VOLR, Sample +0x10, CC7/CC10, SdrSeReq/SdrSeChg, MODMIDI-CC7, HsMsg-Pan, Bend, MidiHandle[2])\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
