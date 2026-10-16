// Der Kampfkern arbeitet auf dem einen Spielerwork: plp->stflg liegt in Eventbank 14, die
// auch der Aktionspfad (player_action.c) und die Gegner (world_enemies.cpp, en05/en09/en17:
// stflg 0x400 = zielt) lesen. Vorher hielt die Kampfsicht eine eigene Kopie; das Zielbit
// erreichte die Gegner nie.
#include "world.h"
#include "room.h"
#include "combat.h"
#include "inventory.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    CombatController combat;
    Fixture(const std::filesystem::path& data,const char* room="RM_5840"):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog),combat(data){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
        check(inv.acquire(6,6,0x1000),"Granatwerfer nicht aufgenommen");
        unsigned slot=0;for(unsigned i=1;i<12;++i)if(Inventory::id(inv.slots()[i])==6)slot=i;
        check(slot&&inv.equip(slot),"Granatwerfer nicht ausgeruestet");
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    bool step(bool aim){world.set_player_weapon(inv.weapon());world.set_simulation_flags(events.flags[6][0]);world.tick();commit();
        return combat.tick(CombatControls{aim,false,0,0,false},false,inv,player,actor,motion,frame,&world,audio,&events);}
    unsigned stflg()const{return events.flags[14][0];}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {   // player.c:1789: Zielen setzt plp->stflg 0x400; das Freigeben (player.c:1834) loescht es.
        Fixture f(data);
        check(!(f.stflg()&0x400u),"stflg 0x400 vor dem Zielen");
        bool active=false;for(int i=0;i<4;++i)active=f.step(true)||active;
        check(active,"Kampfpass nicht aktiv");
        check(f.stflg()&0x400u,"Zielen erreicht plp->stflg (Eventbank 14) nicht: Gegner sehen kein Zielen");
        check(f.stflg()&0x40000000u,"stflg 0x40000000 (bhInitPlayer) verloren");
        for(int i=0;i<40&&(f.stflg()&0x400u);++i)f.step(false);
        check(!(f.stflg()&0x400u),"Freigabe loescht stflg 0x400 nicht");
    }
    std::cout<<"combat player work PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
