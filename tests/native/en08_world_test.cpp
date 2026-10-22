// EN08 Fisch in RM_4000 (Originalraum, erster Abnahmeraum), echte RoomWorld-Dienste.
// Semantikreferenz en08.c: MV00 (:85) kreist mit Wand- (:131) und Linientest (:139),
// MV01 (:168) schwimmt an, MV02 (:263) dreht ab. Geprueft: jeder Fisch verlaesst
// mode [0,0,0,0], bewegt sich, durchlaeuft die Bewegungszweige und meldet keine eigene Luecke.
#include "world.h"
#include "room.h"
#include "ninja_math.h"
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_4000.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_4000"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_enemy_ai(true);world.set_camera_offset_owner(&camera);
        world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    void tick(){world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        world.set_simulation_flags(events.flags[6][0]);world.tick();}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();
    Fixture f(argv[1]);f.tick();
    std::vector<unsigned> fish;
    for(unsigned i=0;i<32;++i){const auto* e=f.world.state(1,i);if(e&&e->enemy_bound&&e->enemy.id==8)fish.push_back(i);}
    check(fish.size()==3,"RM_4000 ohne die drei Originalfische");
    std::vector<NJS_POINT3> start;for(unsigned i:fish){const auto& w=f.world.state(1,i)->enemy;start.push_back({w.px,w.py,w.pz});
        check(w.mode0==1&&(w.flg&0x8018u)==0x8018u&&w.ar==5.0f,"bhEne08_Init (en08.c:45-77) nicht gelaufen");}
    std::set<unsigned> branches;
    for(int t=0;t<240;++t){f.tick();for(unsigned i:fish)branches.insert(f.world.state(1,i)->enemy.mode2);}
    for(std::size_t k=0;k<fish.size();++k){const auto& w=f.world.state(1,fish[k])->enemy;
        check(w.px!=start[k].x||w.pz!=start[k].z,"Fisch "+std::to_string(fish[k])+" bewegt sich nicht (bhAddSpeed)");}
    check(branches.count(0)&&branches.count(1)&&branches.count(2),"MV00/MV01/MV02 nicht alle durchlaufen");
    for(const auto& g:f.world.unbound_enemy_routines())
        if(g.find("Ene08")!=std::string::npos||g.find("bhCheckWallType")!=std::string::npos||g.find("bhCollisionCheckLine")!=std::string::npos||g.find("en03.c:5220")!=std::string::npos)
            throw std::runtime_error("EN08-Luecke: "+g);
    std::cout<<"EN08 RM_4000: drei Fische, MV00/MV01/MV02 durchlaufen, Bewegung, keine EN08-Luecke PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
