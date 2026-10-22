// EN10/EN71 (Ameisen) in RM_9170, dem einzigen Originalraum beider Identitaeten, auf echten
// RoomWorld-Diensten. en10.c:51 `epw->mlwP = &epw->mdl[EXP0_UC(0x1)]` (alle drei Passes weiter)
// und en71.c:41 `epw->mlwP = &epw->mdl[mode3]` waehlen ein ML_WORK der eigenen Reihe. Geprueft:
// der Work rechnet und zeichnet mdl[k], jedes mdl[k] behaelt seinen objP-Zustand ueber den
// Wechsel, mdl_no (Skin-Index, MdlPut.c:48) bleibt unberuehrt, und es entsteht keine Luecke.
#include "world.h"
#include "room.h"
#include "ninja_math.h"
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_9170.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_9170"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_enemy_ai(true);world.set_camera_offset_owner(&camera);
        world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    void tick(){world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        world.set_simulation_flags(events.flags[6][0]);world.tick();}
};
static unsigned selected(const WorldEntity& e){
    if(e.enemy.id==10){unsigned char k=0;std::memcpy(&k,static_cast<const char*>(e.enemy.exp0)+1,1);return k;} // EXP0_UC(0x1)
    return e.enemy.mode3;
}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    Fixture f(data);
    f.tick();
    std::vector<unsigned> ants;
    for(unsigned i=0;i<32;++i){const auto* e=f.world.state(1,i);if(e&&e->enemy_bound&&(e->enemy.id==10||e->enemy.id==71)&&e->enemy.exp0)ants.push_back(i);}
    check(ants.size()>=2,"RM_9170 ohne EN10/EN71-Works");
    // Marke in mdl[0] eines EN10-Works (Flipbook ohne bhSetMotion, en10.c): sie muss den Ausflug
    // nach mdl[1..3] ueberstehen. EN71 tastet je Pass eine Motion in mdl[mode3] ab.
    unsigned probe=~0u;for(unsigned i:ants)if(f.world.state(1,i)->enemy.id==10){probe=i;break;}
    check(probe!=~0u,"RM_9170 ohne EN10-Work");
    auto* marked=const_cast<WorldEntity*>(f.world.state(1,probe));
    const PlayerAsset* row=marked->model_row();check(row&&row->model_count()==4,"Ameise ohne vier mdl[]");
    for(int i=0;i<40&&selected(*marked)!=0;++i)f.tick();
    check(selected(*marked)==0&&marked->enemy_asset==row,"Ameise kehrt nicht nach mdl[0] zurueck");
    marked->enemy_obj.at(0).position.y=12.5f;
    bool left=false,back=false;unsigned switches=0,last=0;
    for(int i=0;i<60;++i){
        f.tick();
        for(unsigned index:ants){
            const auto& e=*f.world.state(1,index);if(!e.enemy_bound||!e.enemy.exp0||!(e.enemy.flg&1u))continue;
            const unsigned k=selected(e);
            const PlayerAsset* expect=k==0?e.model_row():e.model_row()->model(k);
            check(e.enemy_asset==expect,"Work "+std::to_string(index)+" rechnet nicht mdl["+std::to_string(k)+"]");
            check(e.drawn_root()==k&&(k==0||e.own_row),"Work "+std::to_string(index)+" zeichnet nicht mlwP");
            check(e.enemy.mdl_no==0,"mdl_no (Skin-Index) wurde als Modellwahl beschrieben");
            check(e.enemy_obj.size()==expect->bones(),"objP passt nicht zu mdl[k]");
        }
        const unsigned k=selected(*marked);if(k!=last){++switches;last=k;}
        if(k!=0)left=true;
        if(left&&k==0){back=true;check(marked->enemy_obj.at(0).position.y==12.5f,"mdl[0] verlor seinen objP-Zustand beim Wechsel");break;}
    }
    check(left&&back&&switches>=2,"kein vollstaendiger mdl[]-Umlauf in 60 Passes");
    for(const auto& g:f.world.unbound_enemy_routines())
        if(g.find("mlwP")!=std::string::npos||g.find("Modellreihe")!=std::string::npos)throw std::runtime_error("Luecke: "+g);
    std::cout<<"EN10/EN71 RM_9170: mlwP = &mdl[k] auf eigener Reihe, Zustand je mdl[k] erhalten, mdl_no unberuehrt PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
