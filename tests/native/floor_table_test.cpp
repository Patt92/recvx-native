// rom->flrp hat im Original genau einen Besitzer. Die Skriptbefehle 0x0c (event.c:1749
// bhFlrAtariSet) und 0x7c (:5804 bhFlrAtariSet2) schreiben ihn; bhCheckFloorSound,
// bhCheckWater, bhCheckL2Water und bhCheckFloorEnemy lesen ihn. Vorher hielt RoomWorld drei
// getrennte Kopien, 0x7c erreichte keine davon und 0x0c nicht die des Effekt-/Gegnerbodens.
#include "world.h"
#include "room.h"
#include "inspection.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    explicit Fixture(const std::filesystem::path& data,const char* room,const std::function<void(EventVm&)>& before_init={}):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();if(before_init)before_init(events);events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){events.tick();world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    // Rohsatz n der Raumtabelle (header 0x20/0xa0), unabhaengig von beiden Laufzeitsichten.
    unsigned raw_prm0(unsigned n)const{BinaryView v(bytes);const auto info=inspect_room(bytes);return v.u8(v.u32(info.room_offset+0x20)+n*36+32);}
    unsigned raw_type(unsigned n)const{BinaryView v(bytes);const auto info=inspect_room(bytes);return v.u8(v.u32(info.room_offset+0x20)+n*36+1);}
    EventCommand command(unsigned op,std::vector<std::uint8_t> b){EventCommand c;c.opcode=op;c.bytes=std::move(b);return c;}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {   // RM_0070: das Init-Skript scd0 schreibt Bodenflaeche 2 ueber 0x7c. @12e bhIfelCk mit
        // bhCk(1, 0x74, 1): Flag frei -> @142 prm0 1 (wie der Rohsatz), Flag gesetzt -> @1be prm0 2.
        Fixture f(data,"RM_0070",[](EventVm& e){e.set_flag(1,0x74,0);});
        const auto& a=f.events.floor_areas().at(2);
        const float cx=a.position.x+a.extent.x*0.5f,cz=a.position.z+a.extent.z*0.5f;
        check(f.raw_type(2)==1&&f.raw_prm0(2)==1,"RM_0070: Rohsatz 2 ist Bodenklang 1");
        for(int i=0;i<8;++i)f.tick();
        const unsigned event_prm0=f.events.floor_areas().at(2).parameters[0];
        const unsigned world_sound=f.world.floor_sound(a.floor,cx,cz);
        std::cout<<"RM_0070: Flaeche 2 roh prm0 "<<f.raw_prm0(2)<<", Eventsicht "<<event_prm0<<", Weltsicht "<<world_sound<<"\n";
        check(event_prm0==2,"RM_0070: scd0 hat mit gesetztem Flag 1/0x74 nicht den 0x7c-Zweig @1be genommen");
        check(world_sound==2,"RM_0070: bhCheckFloorSound liest nicht die vom Skript geschriebene Flaeche");
        // Dieselben Skriptbytes beider Zweige (scd0 @142/@1be) ueber den Weltbesitzer.
        check(f.world.bind(f.command(0x7c,{0x7c,2,0,0,2,0,0,0,1,0}))&&f.world.floor_sound(a.floor,cx,cz)==2,"RM_0070: 0x7c prm0 2 erreicht bhCheckFloorSound nicht");
        check(f.world.bind(f.command(0x7c,{0x7c,2,0,0,1,0,0,0,1,0}))&&f.world.floor_sound(a.floor,cx,cz)==1,"RM_0070: 0x7c prm0 1 erreicht bhCheckFloorSound nicht");
        // type 0 entzieht die Flaeche bhCheckFloorSound (nur type 1 zaehlt).
        check(f.world.bind(f.command(0x7c,{0x7c,2,0,0,2,0,0,0,0,0}))&&f.world.floor_sound(a.floor,cx,cz)==0,"RM_0070: 0x7c type 0 laesst die Klangflaeche stehen");
    }
    {   // RM_3060: 0x0c schaltet die Wasserflaeche ab; Wasserabfragen und Effektboden folgen.
        Fixture f(data,"RM_3060");
        int water=-1;const auto& floors=f.events.floor_areas();
        for(unsigned i=0;i<floors.size();++i)if((floors[i].flags&1)&&floors[i].type==3&&floors[i].parameters[0]==1){water=int(i);break;}
        check(water>=0,"RM_3060: keine Wasserflaeche");
        const auto& a=floors[unsigned(water)];const float top=a.position.y+a.extent.y,cx=a.position.x+a.extent.x*0.5f,cz=a.position.z+a.extent.z*0.5f;
        float t=0;NJS_POINT3 hit{};
        check(f.world.water_top({cx,top-0.01f,cz},t)&&f.world.l2_water({cx,top+10,cz},{0,-20,0},hit),"RM_3060: Wasser vor 0x0c");
        check(f.world.bind(f.command(0x0c,{0x0c,std::uint8_t(water),1,0})),"RM_3060: 0x0c nicht gebunden");
        check(!f.world.water_top({cx,top-0.01f,cz},t),"RM_3060: bhCheckWater sieht die abgeschaltete Flaeche");
        check(!f.world.l2_water({cx,top+10,cz},{0,-20,0},hit),"RM_3060: bhCheckL2Water sieht die abgeschaltete Flaeche");
        check(f.world.bind(f.command(0x0c,{0x0c,std::uint8_t(water),0,0}))&&f.world.water_top({cx,top-0.01f,cz},t),"RM_3060: 0x0c schaltet nicht wieder ein");
        // 0x7c macht die Wasserflaeche zur Klangflaeche: bhCheckWater nicht mehr, Bodenklang prm0.
        check(f.world.bind(f.command(0x7c,{0x7c,std::uint8_t(water),0,0,4,0,0,0,1,0})),"RM_3060: 0x7c nicht gebunden");
        check(!f.world.water_top({cx,top-0.01f,cz},t)&&f.world.floor_sound(a.floor,cx,cz)==4,"RM_3060: 0x7c type 1 / prm0 4 nicht uebernommen");
    }
    std::cout<<"floor table PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
