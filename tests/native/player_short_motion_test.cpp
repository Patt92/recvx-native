// Motion.c:29/131: bhSetMotion tastet ab, sobald mnwP[mtn_no].datP != NULL ist, und laeuft
// ueber obj_num der MOTION; eine Motion mit weniger Kanaelen als das Modell Knoten hat,
// schreibt nur die ersten obj_num objP, die uebrigen behalten ihren Stand. Wesker (SYSTEM
// 0013) traegt Motion 9 (Warten, PlMtnAct[*][*][6]) mit 20 Kanaelen auf 22 Knoten; der Host
// warf dafuer "bhCPM1_act_bas: player motion 9 missing" (Absturz beim Stehenbleiben).
//
// Gegenproben (je eine Zeile, zurueck und touch):
//   actor.h has_motion: "<=bones()" wieder "==bones()"           -> "Motion 9 (20 Kanaele)"
//   world.cpp player_set_motion_pose: Knotenteil vor set_motion zuruecksetzen -> "Knoten 20/21"
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
static bool same(const ModelPose& a,const ModelPose& b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.angles==b.angles;}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");_Make_SinTable();const std::filesystem::path data=argv[1];
    const auto wesker=PlayerAsset::load(data,13);
    check(wesker.bones()==22,"SYSTEM 0013: 22 Knoten erwartet");
    const auto* clip=wesker.library_motion(9);
    check(clip&&clip->positions.size()==20,"SYSTEM 0013 Motion 9 hat nicht 20 Kanaele");
    check(wesker.has_motion(9),"Motion 9 (20 Kanaele) gilt nicht als vorhanden (datP != NULL)");
    const auto bind=wesker.bind_pose(),pose=wesker.local_pose(9,0);
    check(pose.size()==22,"Pose hat nicht 22 Knoten");
    check(same(pose[20],bind[20])&&same(pose[21],bind[21]),"Knoten 20/21 ohne Kanal weichen von der Ruhelage ab");
    // Knotenteil am Spielerbesitzer: Motion 42 (22 Kanaele) schreibt alle Knoten, Motion 9 danach nur 0..19.
    const auto bytes=read_asset_file(data/"rooms/RM_3080.RDT",64*1024*1024);const auto stage=decode_scene(bytes);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_3080");world.set_player_asset(&wesker);
    world.player_set_motion_pose(42,30*65536,0u,0u,0);
    const auto standing=world.player_local_pose();check(standing.size()==22,"objP nach Motion 42");
    bool moved=false;for(unsigned n=20;n<22;++n)if(!same(standing[n],bind[n]))moved=true;
    check(moved,"Motion 42 Bild 30 bewegt die Knoten 20/21 nicht (Pruefvorrichtung)");
    motion=9;world.player_set_motion_pose(9,0,0u,0u,0);
    const auto waiting=world.player_local_pose();
    check(same(waiting[20],standing[20])&&same(waiting[21],standing[21]),"Knoten 20/21 behalten nach Motion 9 nicht den Stand von Motion 42");
    bool changed=false;for(unsigned n=0;n<20;++n)if(!same(waiting[n],standing[n]))changed=true;
    check(changed,"Motion 9 schreibt die Knoten 0..19 nicht");
    for(const auto& g:world.unbound_enemy_routines())check(g.find("bhSetMotion(plp)")==std::string::npos,"Luecke: "+g);
    std::cout<<"player short motion PASS (SYSTEM 0013 Motion 9: 20 Kanaele auf 22 Knoten)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
