// Skriptgesteuerte Gegner vor ihrer Initialisierung (docs/OPEN_ITEMS.md, RM_7110 EN11/Hunter).
//
// Original: bhInitEvent (event.c:339-352) ruft bhControlEnemy VOR scd0 und dem ersten Schedulerpass;
// beim Raumladen ist sys->sp_flg ~0 (room.c:545), also laeuft fuer jedes Work mit flg & 1 die
// Identitaetsroutine einmal mit mode0 0 und initialisiert es (en11.c:58 bhEne11_Mode0[0] = bhEne11_Init,
// eneset.c:377). Schreibt danach ein Skript mode0 = 5 (bhInitMotionPause event.c:1866, Common_controll
// 24/34 Selektor 1 mit Byte 5), laeuft bhEne_Event (subpl.c:143) auf einem initialisierten Work.
//
// Geprueft in RM_7110 mit echten Raum- und Skriptdaten (Diagnosevorbedingung: Fixture ohne Durchspielen):
//  1. Nach EventVm::initialize sind alle EN11-Works initialisiert (exp0 mit initialized), bevor ein
//     Skriptbefehl sie angefasst haben kann.
//  2. Ein nachtraeglich gesetztes mode0 5 (wie bhInitMotionPause) fuehrt in den Ereignispfad, ohne dass
//     die Fehlinitialisierungs-Luecke ("missing initialized exp0") gemeldet wird.
//  3. Gegenprobe: ohne den Initialpass (initialize_enemies leer) meldet derselbe Ablauf die Luecke.
//  4. Alle Raeume: nach dem Initialpass steht kein Work mit flg & 1 mehr auf mode0 0 - ausser den drei
//     Originalwaechtern, die ihre Mode-Tabelle gar nicht aufrufen: EN29 mit type 8 (en29.c:166),
//     EN03 unter flg 0x4000000 (en03.c:484, Spieler-Kontrollpfad) und EN06 unter flg 0x1000000 (en06.c:231).
#include "world.h"
#include "room.h"
#include "en11.h"
#include "ninja_math.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
static void initialize_room(RoomWorld& world,EventVm& events,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,
                            Inventory& inv,std::uint32_t& head,NJS_POINT3& camera){
    world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
    world.set_enemy_ai(true);world.set_enemy_grab(true);world.set_camera_offset_owner(&camera);
    world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
    events.binding=[&world](const EventCommand& c){return world.bind(c);};
}
// Ein Work, dessen Identitaetsroutine bei mode0 0 gar nicht in ihre Mode-Tabelle verzweigt.
static bool original_guard(const RECVX_ENEMY_WORK* w){
    return (w->id==29&&w->type==8)||(w->id==3&&(w->flg&0x4000000u))||(w->id==6&&(w->flg&0x1000000u));}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data,bool initial_pass):bytes(RoomArchive(data).load("RM_7110.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_7110"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_enemy_ai(true);world.set_enemy_grab(true);world.set_camera_offset_owner(&camera);
        world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        if(!initial_pass)events.initialize_enemies=nullptr; // Gegenprobe: bhInitEvent ohne bhControlEnemy
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    void tick(){world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        world.set_simulation_flags(events.flags[6][0]);world.tick();}
    std::vector<RECVX_ENEMY_WORK*> en11(){std::vector<RECVX_ENEMY_WORK*> out;auto& t=world.enemy_table();
        for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==11)out.push_back(t.slot[i]);return out;}
    bool gap(const char* text){for(const auto& g:world.unbound_enemy_routines())if(g.find(text)!=std::string::npos)return true;return false;}
};
static bool initialized(const RECVX_ENEMY_WORK* w){return w->exp0&&static_cast<const RECVX_EN11_STATE*>(w->exp0)->initialized;}
int main(int argc,char** argv){try{
    if(argc<2)return 0;
    _Make_SinTable();const std::filesystem::path data=argv[1];
    {   Fixture f(data,true);
        const auto works=f.en11();check(!works.empty(),"RM_7110 hat kein EN11-Work");
        for(auto* w:works)check(initialized(w),"bhInitEvent: EN11 nach dem Initialpass (event.c:352) nicht initialisiert");
        // Skript setzt mode0 5 / mode3 4 wie bhInitMotionPause (event.c:1866-1867) auf dem ersten EN11-Work.
        works[0]->mode0=5;works[0]->mode3=4;
        for(int i=0;i<10;++i)f.tick();
        check(!f.gap("missing initialized exp0"),"mode0 5 nach dem Initialpass meldet eine Fehlinitialisierung");
        check(!f.gap("bhEne_Event"),"bhEne_Event ohne Besitzer");
        std::printf("RM_7110: %zu EN11-Works vor scd0 initialisiert, mode0 5 laeuft in bhEne_Event\n",works.size());
    }
    {   Fixture f(data,false);
        const auto works=f.en11();check(!works.empty(),"Gegenprobe: kein EN11-Work");
        check(!initialized(works[0]),"Gegenprobe: EN11 ohne Initialpass schon initialisiert");
        works[0]->mode0=5;works[0]->mode3=4;
        for(int i=0;i<3;++i)f.tick();
        check(f.gap("missing initialized exp0"),"Gegenprobe: ohne Initialpass bleibt die Luecke unsichtbar");
        std::printf("Gegenprobe ohne bhControlEnemy in bhInitEvent: Luecke gemeldet\n");
    }
    {   unsigned rooms=0,works=0,guarded=0;std::string findings;
        PlayerAsset player=PlayerAsset::load(data);AudioMixer audio(data);
        for(const auto& entry:std::filesystem::directory_iterator(data/"rooms")){
            if(entry.path().extension()!=".RDT")continue;const auto name=entry.path().stem().string();
            auto bytes=RoomArchive(data).load(name+".RDX",64*1024*1024);auto scene=decode_scene(bytes);if(scene.spawns.empty())continue;
            auto actor=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;Inventory inv;MessageDialog dialog;std::uint32_t head=0;NJS_POINT3 camera{};
            RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},name);EventVm events(bytes,inv,dialog);
            initialize_room(world,events,player,actor,motion,frame,inv,head,camera);
            // Zustand nach dem Initialpass, vor scd0 (event.c:352-354).
            auto pass=events.initialize_enemies;events.initialize_enemies=[&]{pass();auto& t=world.enemy_table();
                for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){auto* w=t.slot[i];if(!w||!(w->flg&1u))continue;++works;
                    if(w->mode0!=0)continue;if(original_guard(w)){++guarded;continue;}
                    findings+=name+" slot "+std::to_string(i)+" id "+std::to_string(w->id)+"; ";}};
            events.initialize();++rooms;
        }
        check(findings.empty(),"nach dem Initialpass nicht initialisiert: "+findings);
        std::printf("%u Raeume: %u Works initialisiert, %u unter Originalwaechtern (EN29 type 8, EN03/EN06 Spielerpfad)\n",rooms,works-guarded,guarded);
    }
    std::cout<<"scripted enemy init PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<"scripted enemy init: "<<ex.what()<<'\n';return 1;}}
