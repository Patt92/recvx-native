// Raumobjekte (obwp) bekommen ihr objP nur ueber bhSetMotion: Common 24/34 mit v0 0/2/5/6
// (event.c:11940-11967) oder bhSetObjMotion (event.c:2257, Opcode 0x50). objitm.c ruft nie
// bhSetMotion. Ein Common mit anderem v0 beschreibt nur mtn_no/frm_no des Works.
//
// Befund RM_0090 (Sicherheitsbox obwp[14] mit hochstehendem Deckel): scd1 (bhScenarioCheck,
// event.c:367-383) laeuft mit bhCetask = &bhEtask[15] (letzter Schleifenschritt von
// bhEventScheduler2, event.c:13032) und setzt dort bei Offset 3910 `65 02 0e 01` (Box 14,
// Knoten 1). Die Fensterzombie-Ereignisse laufen auf Ebene 15 (scd1 `14 00 0f 51..53/60..62`)
// und setzen nach bhSleep weiter `69 22 00 01 02 01 01 06 ..` (Common 34, Raummotion 2,
// v0 1) auf dem Work der Ebene 15. Faellt dazwischen der scd1-Block, trifft das Common die
// Box. Im Original bleibt objP unberuehrt (v0 1, kein bhSetObjMotion fuer obwp[14]); nativ
// tastete die Box ab da Raummotion 2 (8 Kanaele auf 3 Knoten) ab.
//
// Echte Bytes aus RM_0090. Gegenproben (je eine Zeile, zurueck und touch):
//   world.cpp Common-Pfad: v0-Bedingung entfernt (immer commit)      -> "Box 14"
//   world.cpp advance_entity_motions: Objekte wieder jeden Takt      -> "Box 14"
//   world.cpp 0x50: commit_pose entfernt                              -> "bhSetObjMotion"
#include "world.h"
#include "room.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
static bool gap(const RoomWorld& w,const char* t){for(const auto& g:w.unbound_enemy_routines())if(g.find(t)!=std::string::npos)return true;return false;}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");_Make_SinTable();const std::filesystem::path data=argv[1];
    auto bytes=RoomArchive(data).load("RM_0090.RDX",64*1024*1024);auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    Inventory inv;MessageDialog dialog;EventVm events(bytes,inv,dialog);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0090");world.bind_player_collision(events);
    const auto base=inspect_room(bytes).event_offset;const BinaryView v(bytes);
    const auto command=[&](unsigned task,std::size_t at,std::initializer_list<std::uint8_t> expect){
        std::size_t i=0;for(const auto b:expect){check(v.u8(base+at+i)==b,"RM_0090: Skriptbytes nicht an Offset "+std::to_string(at));++i;}
        return EventCommand{expect.begin()[0],task,0,at,{bytes.begin()+long(base+at),bytes.begin()+long(base+at+expect.size())}};};
    const auto* box=world.state(2,14);check(box&&box->identity==37,"RM_0090 obwp[14] ist nicht die Sicherheitsbox (id 37)");
    // Ein Takt, damit die Objektposen festgehalten sind.
    world.set_simulation_flags(~0u);world.tick();
    check(!box->posed_has_motion(),"Box 14 traegt vor dem Ablauf eine Motion");
    // scd1 Offset 3910 auf Ebene 15, dann das Fensterzombie-Common der Ebene 15 (ev 87 @12664).
    check(world.bind(command(15,3910,{0x65,0x02,0x0e,0x01})),"bhLoadWork 2/14/1 nicht gebunden");
    check(world.bind(command(15,12664,{0x69,0x22,0x00,0x01,0x02,0x01,0x01,0x06,0x02,0x00})),"Common 34 nicht gebunden");
    // Das Work traegt die Motion (event.c:11905-11926), objP nicht.
    check(box->has_motion&&box->motion==2&&box->room_motion,"Common 34 hat mtn_no 2 nicht ins Work geschrieben");
    for(int t=0;t<5;++t)world.tick();
    check(!box->posed_has_motion(),"Box 14: Common 34 mit v0 1 schreibt objP (Original ruft kein bhSetMotion)");
    Scene drawn=scene;world.append(drawn);
    check(!gap(world,"Motion mit mehr Kanaelen"),"Box 14 tastet die Zombiemotion ab (Original schreibt objP nicht)");
    // Positiv: Common 24 mit v0 0 auf obwp[8] (ev0 @1940/1944) schreibt objP sofort,
    // bhSetObjMotion (ev0 @1938, 0x50 08) danach je Aufruf.
    const auto* scanner=world.state(2,8);check(scanner!=nullptr,"obwp[8] fehlt");
    check(world.bind(command(0,1940,{0x65,0x02,0x08,0x00})),"bhLoadWork 2/8 nicht gebunden");
    check(world.bind(command(0,1944,{0x69,0x18,0x01,0x02,0x06,0x00,0x00,0x00})),"Common 24 nicht gebunden");
    check(scanner->posed_has_motion()&&scanner->posed_motion()==6,"Common 24 v0 0: bhSetMotion schreibt objP nicht sofort");
    const auto before=scanner->posed_frame();
    EventCommand obj=command(0,1938,{0x50,0x08});obj.simulation_flags=~0u;
    // Common 24 Selektor 1 laesst mode3; bhSetObjMotion verlangt mode3 1 (event.c:2262).
    auto& work=const_cast<WorldEntity&>(*scanner);work.motion_mode3=1; // Diagnosevorgabe wie das Skript ueber 0x2b
    check(world.bind(obj),"bhSetObjMotion nicht gebunden");check(world.bind(obj),"bhSetObjMotion nicht gebunden");
    check(scanner->posed_frame()!=before,"bhSetObjMotion: objP folgt dem Frame nicht");
    std::cout<<"object motion commit PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
