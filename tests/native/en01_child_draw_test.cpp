// eneset.c:797 bhDrawEnemy fuer die EN01-Kinder mit eigener Modellreihe (Original RM_3160).
// bhEne01_Init legt fuer mdlver 5/6/22/35..38 die Kappe an (en01.c:2612-2624): SetLinkEnemy setzt
// mdflg 0x1|0x20 (:2819/2820), die Kappe loescht 0x1 wieder (:2614), mlwP = epp->mdl mit
// mdl[0] = epw->mdl[2] (bzw. [3] bei mdlver 22). bhDrawEnemy zeichnet jeden ene[]-Work, dessen
// Tore offen sind und dessen mlwP->objP nicht NULL ist, solange mdflg 0x1 frei ist (:807-857).
// Das mdflg-0x1-Setzen unten ist eine Diagnosevorbedingung (Gegenprobe des Tors), kein Spielablauf.
#include "world.h"
#include "room.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data,const char* room):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(false);
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();}
    std::size_t drawn(){Scene s;world.append(s,{},SceneCamera{});return s.triangles.size();}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    Fixture f(data,"RM_3160");
    for(int i=0;i<3;++i)f.tick();
    const WorldEntity* cap=nullptr;
    for(const auto& c:f.world.enemy_children())if(c.enemy_bound&&c.enemy.id==0x22)cap=&c;
    check(cap,"RM_3160: keine EN01-Kappe (id 0x22, en01.c:2613)");
    const auto& w=cap->enemy;
    const auto* parent=static_cast<const RECVX_ENEMY_WORK*>(w.lkwkp);
    check(parent&&parent->id==1,"Kappe: lkwkp ist kein EN01-Work (en01.c:2821)");
    const unsigned mv=parent->mdlver;
    check(mv==5||mv==6||mv==22||(mv>=35&&mv<=38),"Kappe: Eltern-mdlver ausserhalb 5/6/22/35..38 (en01.c:2612)");
    check(!(w.mdflg&1u)&&(w.mdflg&0x20u),"Kappe: mdflg 0x1 nicht frei bzw. 0x20 fehlt (en01.c:2614/2820)");
    // mlwP = epp->mdl, mdl[0] = epw->mdl[2] bzw. [3] (en01.c:2617-2621).
    const unsigned slot=mv==22?3u:2u;
    check(cap->model_bank&&!cap->model_owner&&cap->model_base==slot,"Kappe: Modellreihe nicht epw->mdl[slot]");
    check(cap->enemy_asset&&cap->enemy_asset==cap->model_bank->model(slot),"Kappe: mdl[0] ist nicht epw->mdl[slot]");
    check(cap->enemy_calculated,"Kappe: bhCalcModel (en01sub.c:997) lief nicht");
    // bhDrawEnemy: mit freiem mdflg 0x1 kommt das Kappenmodell hinzu, mit gesetztem nicht.
    const auto with_cap=f.drawn();
    auto* mw=const_cast<RECVX_ENEMY_WORK*>(&w);
    mw->mdflg|=1u;const auto without=f.drawn();mw->mdflg&=~1u;
    check(with_cap>without,"bhDrawEnemy: die Kappe wird nicht gezeichnet (eneset.c:813 mdflg 0x1-Tor)");
    check(f.drawn()==with_cap,"bhDrawEnemy: Zeichnung nicht wiederholbar");
    for(const auto& g:f.world.unbound_enemy_routines())if(g.find("eneset.c:797")!=std::string::npos)throw std::runtime_error("Luecke "+g);
    std::cout<<"EN01 child draw PASS (Kappe mdlver "<<mv<<", slot "<<slot<<", "<<(with_cap-without)<<" Dreiecke)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
