// EN11 Spotter in RM_9100 (Originalraum mit Identitaet 11), echte RoomWorld-Dienste.
// Prueft die Bindung der EN11-Dienste je Gegnerpass: vorher band sie nur
// init_enemy_works, und control_enemy_works loeste sie am Ende des ersten Passes
// wieder — ab dem zweiten Pass fand bhEne11_effect_read keine Dienste mehr und
// meldete "bhEne11: linked bhEff261 work". Diagnosezustand, kein Durchspielen.
#include "world.h"
#include "room.h"
#include "en11.h"
#include "ninja_math.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_9100.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_9100"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_enemy_ai(true);world.set_enemy_grab(true);world.set_camera_offset_owner(&camera);
        // player.c:840 bhCalcModel(plp) vor Event_init und :1688 je Bild — die Runtime schreibt den
        // Spielerbaum so fest (runtime.cpp:450/493); ohne diesen Schritt liest der Effektpass
        // (bhEff_E11_CheckCollisionPlayer) einen leeren owP-Satz.
        world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    void tick(){world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        world.set_simulation_flags(events.flags[6][0]);world.tick();}
};
// eff[slot] des EN11-Zustands (EXP0_P(0x60), en11.h:15).
static int slot_of(const RECVX_ENEMY_WORK* w){int s=0;std::memcpy(&s,static_cast<const char*>(w->exp0)+0x60,sizeof s);return s;}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    Fixture f(data);
    // 40 Passes: der Fehler trat ab dem zweiten Pass auf, ist also nach dem ersten
    // laengst sichtbar — und der Effektslot muss danach noch derselbe 261 sein.
    for(int i=0;i<40;++i)f.tick();
    auto& t=f.world.enemy_table();
    int found=0;
    for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){
        RECVX_ENEMY_WORK* w=t.slot[i];if(!w||w->id!=11||!w->exp0)continue;++found;
        const int slot=slot_of(w);
        check(slot>=0,"EN11: kein Effektslot angelegt");
        const auto* e=f.world.effect_state(unsigned(slot));
        check(e!=nullptr,"EN11: Effektslot ausserhalb des Pools");
        check(e->id==261,"EN11: Slot traegt nicht bhEff_E11_SearchLight");
        check((e->flg&3u)!=0u,"EN11: Effektslot wurde freigegeben");
    }
    check(found>0,"EN11 work fehlt in RM_9100");
    // Die drei frueheren Fehlluecken (bhEne11: linked bhEff261 work, bhEne11_BR00,
    // bhEne11_MV09) duerfen nicht mehr auftreten; ebenso wenig die owP-Meldungen, die
    // nur eine Fixture ohne bhCalcModel(plp) erzeugt (tests/native/player_owp_order_test.cpp).
    for(const auto& g:f.world.unbound_enemy_routines()){
        std::cerr<<"unerwartete Luecke: "<<g<<'\n';throw std::runtime_error("EN11 meldet eine Luecke");
    }
    std::printf("EN11: %d Works, Effektslot(s) 261 leben nach 40 Passes\n",found);
    std::cout<<"EN11 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
