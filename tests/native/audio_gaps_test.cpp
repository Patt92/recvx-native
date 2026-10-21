// Offene sdfunc.c-Ledgerzeilen am Originalstand nachgeprueft (2026-10-21):
//  - Set3dSoundFlag Typ 6/7/8 (sdfunc.c:1020-1070): Flagwort, SetVolumeMidi2(..,0,0) / SetPanMidi
//    bei Flag 0x2/0x4, Objekt-SE ueber ObjectInfo[].Flag.
//  - SetUserSoundVolume Typ 6/7/8 (sdfunc.c:1123-1190): RequestMidiFadeFunctionEx nur unter Flag 0x2
//    (sdc.c:652: VolSpeed = -((Last - Start)/Frame), Func 1 bei VolSpeed < 0 = steigend, sonst 2), Objekt: VolFadeP + Flag 0x8,
//    verbraucht in CallObjectSe2 (sdfunc.c:2635-2644) beim naechsten Managerlauf des Objekts.
//  - ExecSoundFadeManager/ExecAdxFadeManager laufen in sdSysServer je VSync (60 Hz); nativ
//    sound_sync alle 800 Bilder zu 48 kHz. Gegner-SE mit FadeRate (CallEnemySeMain sdfunc.c:2305:
//    RequestSeFadeFunctionEx(Slot, -127, Vol, FadeRate)).
//  - CallDoorSe (sdfunc.c:1684): PanDelayTime/VolumeDelayTime/PitchDelayTime -2 -> ExPlayMidi
//    (sdc.c:230-262) setzt Pan 0, SdcSeDefaultVolume (0, sdcwrap.c:18) und Pitch 0 sofort;
//    DoorFxDef/FxLevel laeuft in sdMidiSetFxLev (ps2_sg_sd.c:542), das auf PS2 nur den Initstatus
//    zurueckgibt. Jeder CallDoorSe startet also mit vollem Pegel in der Mitte, auch nach einem
//    laufenden Fade des vorigen Tuerports.
//  - RequestEnemySeBasic (sdfunc.c:1708): VolDownTbl[8] liegt im Rahmen bei sp+0x58 (ELF 0x295144),
//    (SeNo & 0xF00000) >> 20 ueber 7 liest sp+0x60.. = den Rahmen des Aufrufers (RequestEnemySe/Ex
//    sind Sprungstummel 0x295240/0x295250 ohne eigenen Rahmen). Kein SeNo-Literal in en*.c/zonzon*.c/
//    effsub*.c erreicht 8..15; der native Dienst meldet den Fall statt zu raten.
//
// Gegenproben (je eine Zeile, zurueck und touch):
//   audio.cpp user_sound_volume Typ 7: Flagpruefung entfernt        -> "Typ 7 ohne Flag"
//   audio.cpp call_object_sound: `o.flags&=~8u` entfernt            -> "Objektfade einmalig"
//   audio.cpp play_door_sound: stop(door_slot_) entfernt            -> "Tuer nach Fade"
//   audio.cpp request_enemy_se: VolDownTbl-Grenze 8 auf 16          -> "VolDownTbl[12]"
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
static bool reported(const AudioMixer& m,const char* text){for(const auto& u:m.unavailable)if(u.find(text)!=std::string::npos)return true;return false;}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");const std::filesystem::path root=std::filesystem::u8path(argv[1]);
    const unsigned event_voice=12; // MIDI DefEvt[0] = 7 -> Hoststimme 12 (midi_host_voice)
    // --- Set3dSoundFlag / SetUserSoundVolume Typ 7 (Ereignis-MIDI) -------------------------------
    {AudioMixer m(root);m.load_room_sounds(0,2);m.set_sound_monitor({0,0,0},0,{});
     check(m.event_sound(0,0x206)&&m.active(event_voice),"RM_0002 Ereignis-SE 0x206 nicht gestartet");
     check(m.user_sound_volume(7,0,0,-127,2)&&m.sound_state(7,true).fade==0,"Typ 7 ohne Flag 0x2 darf keinen MIDI-Fade starten");
     m.mix(800*4);check(m.active(event_voice),"Typ 7 ohne Flag: Stimme darf nicht enden");
     m.set_3d_sound_flag(7,0,2);
     check(m.user_sound_volume(7,0,0,-127,2),"Typ 7 mit Flag 0x2 abgelehnt");
     const auto s=m.sound_state(7,true);check(s.fade==2&&s.volume_count==2&&int(s.volume)==0,"RequestMidiFadeFunctionEx(7, 0, -127, 2): VolSpeed = -((-127-0)/2) > 0 -> Func 2, FadeCntMax 2, Volume 0");
     m.mix(800*4);check(!m.active(event_voice),"MIDI-Fade auf -127 muss die Stimme nach FadeCntMax < 0 stoppen (sdc.c:778)");}
    {AudioMixer m(root);m.load_room_sounds(0,2);m.set_sound_monitor({0,0,0},0,{});
     // Flag 0x4 Typ 7: SetPanMidi(DefEvt[0], 0, 0) -> Pan 0; danach pan-Fade nur ueber SetUserSoundPan (bit 4).
     m.set_3d_sound_flag(7,0,4);check(int(m.sound_state(7,true).pan)==0,"Typ 7 Flag 0x4: SetPanMidi(..,0,0)");
     check(m.user_sound_volume(7,0,0,-127,2)&&m.sound_state(7,true).fade==0,"Typ 7 nur Flag 0x4 darf keinen Lautstaerkefade starten");}
    // --- Typ 6 (Hintergrund-MIDI DefBg[0..2] = MIDI 0..2): Flag und Fade ohne laufende Stimme -----
    {AudioMixer m(root);
     check(m.user_sound_volume(6,1,-10,-127,3)&&m.sound_state(1,true).fade==0,"Typ 6 ohne Flag 0x2 darf keinen Fade starten");
     m.set_3d_sound_flag(6,1,2);check(m.user_sound_volume(6,1,-10,-127,3),"Typ 6 mit Flag abgelehnt");
     const auto s=m.sound_state(1,true);check(s.fade==2&&s.volume_count==3&&int(s.volume)==-10,"RequestMidiFadeFunctionEx(DefBg[1], -10, -127, 3): fallend -> Func 2");
     check(m.sound_state(0,true).fade==0&&m.sound_state(2,true).fade==0,"Typ 6 Slot 1 darf nur MIDI 1 anfassen");
     bool rejected=false;try{m.set_3d_sound_flag(6,3,2);}catch(const std::exception&){rejected=true;}check(rejected,"DefBg[3] liegt ausserhalb der Originaltabelle");}
    // --- Typ 8 (Objekt-SE): ObjectInfo[].Flag, VolFadeP, Verbrauch in CallObjectSe2 ---------------
    {AudioMixer m(root);m.load_room_sounds(0,2);m.set_sound_monitor({0,0,0},0,{});
     m.regist_object_se(2,{0,0,-10},0x0206,0);m.exec_object_se();
     check(m.active(8),"Objekt-SE 0x206 (RegistObjectSe: (SeNo & ~0xff00) | 0x200 -> Bank 2 Liste 6) nicht gestartet");
     check(m.user_sound_volume(8,2,0,-127,3)&&m.sound_state(3,true).fade==0,"Typ 8 ohne ObjectInfo.Flag 0x2 darf nichts eintragen");
     m.set_3d_sound_flag(8,2,2);check(m.user_sound_volume(8,2,0,-127,3),"Typ 8 mit Flag abgelehnt");
     check(m.sound_state(3,true).fade==0,"SetUserSoundVolume Typ 8 schreibt nur VolFadeP/Flag 0x8, kein sofortiger Fade (sdfunc.c:1176)");
     // CallObjectSe2 (sdfunc.c:2573): erst der naechste Managerlauf fuer dieses Objekt setzt den Fade ab.
     m.regist_object_se(2,{0,0,-10},0x0206,0);m.exec_object_se();
     {const auto s=m.sound_state(3,true);check(s.fade==2&&s.volume_count==3,"Objekt-Fade RequestMidiFadeFunctionEx(DefObj[0], 0, -127, 3) fehlt");}
     m.mix(800);m.regist_object_se(2,{0,0,-10},0x0206,0);m.exec_object_se();check(m.sound_state(3,true).volume_count==2,"Objektfade einmalig: Flag 0x8 muss nach dem Verbrauch geloescht sein (sdfunc.c:2597)");
     m.mix(800*5);check(!m.active(8),"Objekt-MIDI-Fade auf -127 muss die Stimme stoppen");}
    // --- Gegner-SE: VolDownTbl-Rand und 60-Hz-Fade --------------------------------------------------
    {AudioMixer m(root);m.load_room_sounds(0,2);m.set_sound_monitor({0,0,0},0,{});
     // VolDownTbl[7] = 245 (char -11): Vol 0 + VolEnemySe + (-11); Index 12 liest den Aufruferrahmen.
     m.request_enemy_se(1,{0,0,-10},0x00700300);check(!reported(m,"VolDownTbl"),"Index 7 ist ein Originaleintrag");
     m.request_enemy_se(2,{0,0,-10},0x00C00300);check(reported(m,"VolDownTbl[12]"),"VolDownTbl[12] (Rahmen des Aufrufers) nicht gemeldet");
     m.exec_enemy_se();
     check(m.active(18)&&!m.active(19),"nur der Eintrag mit Originalindex darf eine Stimme belegen");
     // RequestEnemySeEx(.., FadeRate 10): CallEnemySeMain (sdfunc.c:2345) -> RequestSeFadeFunctionEx(Slot, -127, Vol, 10); VolSpeed < 0 -> Func 1 (steigend, sdc.c:726).
     m.request_enemy_se(3,{0,0,-10},0x00000301,1,10);m.exec_enemy_se();
     const auto before=m.sound_state(AudioMixer::se_voice(1));check(m.active(19)&&before.fade==1&&before.volume_count==10&&int(before.volume)==-127,"Gegner-SE-Fade: VolSpeed = -((Vol+127)/10) < 0 -> Func 1, FadeCntMax 10, Start -127");
     m.mix(800*5);const auto half=m.sound_state(AudioMixer::se_voice(1));check(half.fade==1&&half.volume_count==5&&int(half.volume)>-127,"SE-Fade schreitet je 800 Bilder (60 Hz) um einen Schritt");
     m.mix(800*6);const auto done=m.sound_state(AudioMixer::se_voice(1));check(done.fade==0&&int(done.volume)>-127,"SE-Fade endet nach FadeCntMax + 1 Schritten auf VolLast");}
    // --- CallDoorSe: voller Pegel und Mitte unabhaengig vom Fade des vorigen Ports -----------------
    // Ohne Raumhall (load_room_sounds setzt EVOL/Reverb), damit nur der Tuerport verglichen wird.
    {AudioMixer door(root),fresh(root);door.load_door_sounds(7);fresh.load_door_sounds(7);
     check(door.play_door_sound(2),"Tuerliste 2 nicht spielbar");door.finish_door_sound(); // RequestMidiFadeFunction(2, 2, 250): 45 Schritte
     door.mix(800*20);check(door.active(door.door_voice()),"Tuerport waehrend des Fades verstummt zu frueh");
     check(door.play_door_sound(1)&&fresh.play_door_sound(1),"Tuerliste 1 nicht spielbar");
     // sdMidiPlay stoppt den alten Port per Key-off; dessen SPU-Release klingt im Original wie hier
     // noch aus, daher Vergleich ueber Spitzenpegel je Kanal statt bitgleich: voller Pegel, Mitte.
     const auto a=door.mix(2400),b=fresh.mix(2400);
     auto peaks=[](const std::vector<std::int16_t>& out){int l=0,r=0;for(std::size_t i=0;i+1<out.size();i+=2){l=std::max(l,std::abs(int(out[i])));r=std::max(r,std::abs(int(out[i+1])));}return std::make_pair(l,r);};
     const auto pa=peaks(a),pb=peaks(b);
     check(pa.first>1000&&pa==pb,"Tuer nach Fade: CallDoorSe (VolumeDelayTime -2 = SdcSeDefaultVolume 0) muss den vollen Pegel des frischen Ports haben");
     check(pa.first==pa.second,"Tuer nach Fade: CallDoorSe (PanDelayTime -2) muss in der Mitte spielen");
    }
    std::cout<<"audio gaps PASS (Set3dSoundFlag 6/7/8, SetUserSoundVolume 6/7/8, Gegner-SE-Fade, VolDownTbl, CallDoorSe)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
