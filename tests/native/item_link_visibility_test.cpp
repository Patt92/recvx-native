// objitm.c:314-323 bhControlObjItm: ein gelinktes OBJEKT (flg 0x80) uebernimmt stflg 0x1000000 seines lkwkp.
// Fuer ITEMS gibt es diese Uebernahme nicht (Itemschleife :402-462, Zeichentor :553-562 nur op->stflg/flg/mdflg):
// ein an einen abgeschalteten Gegner gelinktes Item bleibt sichtbar. Aufstellung RM_0070 wie das Raumskript
// (bhObjLinkSetEneItem 0x53 00 01 01 00: Gegner 0 traegt Item 1 an Knoten 1); das Abschalten des Gegners ist
// eine Diagnosevorbedingung.
#include "world.h"
#include "room.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    const auto bytes=RoomArchive(data).load("RM_0070.RDX",64*1024*1024);const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);PlayerAsset player=PlayerAsset::load(data,10);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0070");world.set_player_asset(&player);
    auto triangles=[&]{Scene s;world.append(s,{},SceneCamera{});return s.triangles.size();};
    check(world.bind({0x53,0,0,0,{0x53,0,1,1,0,0,0,0,0,0,0,0}}),"bhObjLinkSetEneItem unbound");
    world.deactivate_enemy(0);
    const auto linked_item_visible=triangles();
    world.deactivate_item(1);
    const auto item_off=triangles();
    check(linked_item_visible>item_off,"item linked to an inactive enemy must still be drawn (objitm.c:553-562)");
    std::cout<<"item link visibility PASS ("<<linked_item_visible-item_off<<" item triangles)\n";return 0;
}catch(const std::exception& e){std::cerr<<"item link visibility: "<<e.what()<<'\n';return 1;}}
