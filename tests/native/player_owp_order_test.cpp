// Reihenfolge-Nachweis fuer plp->mlwP->owP im Effektpass: RM_9100 (EN11-Suchscheinwerfer,
// bhEff261 -> bhEff_E11_CheckCollisionPlayer, effsub2.c:2296) und die EN05-Kind-Kopfsonde
// (en05.c:1021, plp->mlwP->owP[4]). Echte RoomWorld-Dienste, Diagnosezustand, kein Durchspielen.
//
// Original: game.c:48-50 bhControlEnemy -> bhControlPlayer (player.c:1688 bhCalcModel(plp))
// -> bhControlEffect; vor Event_init rechnet bhSetPlayer den Baum (player.c:840). Die Runtime
// bildet beides ab (native/runtime.cpp: commit_player_joints vor events->initialize() und in
// head_hair_step nach world->tick()). Eine RoomWorld-Fixture, die diese beiden Schritte
// auslaesst, hat ein leeres player_joint_matrices_; effect_player_hit (world.cpp) meldet dann
// "bhEff_E11_CheckCollisionPlayer(plp): owP vor bhCalcModel" und bhEne05s
// "plp->mlwP->owP[4].mtx (en05s)". Beides ist ein Fixture-Fehler, keine Engine-Luecke.
#include "world.h"
#include "room.h"
#include "ninja_math.h"
#include <cstdio>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    const bool calc_model;
    Fixture(const std::filesystem::path& data,bool calc):bytes(RoomArchive(data).load("RM_9100.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_9100"),events(bytes,inv,dialog),calc_model(calc){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_enemy_ai(true);world.set_enemy_grab(true);world.set_camera_offset_owner(&camera);
        // player.c:840 bhSetPlayer: bhCalcModel(plp) vor Event_init (runtime.cpp vor events->initialize()).
        if(calc_model)commit();
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    void commit(){world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));}
    // Runtime-Bildfolge: world->tick() (Gegner- und Effektpass), danach head_hair_step mit
    // bhCalcModel(plp) -> commit_player_joints. Der Effektpass sieht damit den Baum des
    // vorigen Bildes (Original: denselben Bildes, game.c:49 vor :50) — eine benannte
    // Reihenfolge-Luecke, aber kein leeres owP.
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();if(calc_model)commit();}
};
static const char* kEff="bhEff_E11_CheckCollisionPlayer(plp): owP vor bhCalcModel";
static const char* kEn05s="plp->mlwP->owP[4].mtx (en05s)";
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {   // Wie die Runtime: Baum vor Event_init und nach jedem Bild festgeschrieben.
        Fixture f(data,true);
        check(f.world.player_committed_joints().size()==f.player.bones(),"owP: obj_num != Spielerknoten");
        for(int i=0;i<40;++i)f.tick();
        bool bad=false;
        for(const auto& g:f.world.unbound_enemy_routines()){
            std::printf("Luecke mit bhCalcModel(plp): %s\n",g.c_str());
            if(g==kEff||g==kEn05s||g.find("owP vor bhCalcModel")!=std::string::npos)bad=true;
        }
        check(!bad,"owP-Luecke trotz festgeschriebenem Spielerbaum");
        std::printf("RM_9100 mit bhCalcModel(plp): keine owP-Luecke nach 40 Passes (%zu Knoten)\n",f.world.player_committed_joints().size());
    }
    {   // Ursachenbeleg: dieselbe Fixture ohne die beiden Schritte meldet genau diese Luecken.
        Fixture f(data,false);
        for(int i=0;i<40;++i)f.tick();
        const auto& gaps=f.world.unbound_enemy_routines();
        check(gaps.count(kEff)==1,"ohne bhCalcModel(plp): Effektpass-Meldung fehlt");
        check(gaps.count(kEn05s)==1,"ohne bhCalcModel(plp): bhEne05s-Meldung fehlt");
        std::printf("RM_9100 ohne bhCalcModel(plp): beide owP-Luecken gemeldet (Ursachenbeleg)\n");
    }
    std::cout<<"player owP order PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
