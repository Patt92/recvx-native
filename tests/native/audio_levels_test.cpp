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
//   audio_pan.cpp sony_voice_levels: Klemme -63 auf -64                     -> "Bankpan 127 + CC10 0"
//   audio.cpp decode_program_voices: Sample +0x10 aus sony_volume entfernt  -> "Sample +0x10"
//   audio.cpp shot_volume: `else if(c.shot_request)` -> `else`               -> "Chg behaelt CC7"
//   audio.cpp shot_pan: Bedingung `value!=64` entfernt                       -> "Chg Pan 64"
//   audio.cpp sound_sync: shot_request-Ruecksetzen entfernt                  -> "Chg behaelt CC7"
#include "audio.h"
#include "audio_pan.h"
#include <cstdint>
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
    std::cout<<"audio levels PASS (MODHSYN VOLL/VOLR, Sample +0x10, CC7/CC10, SdrSeReq/SdrSeChg)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
