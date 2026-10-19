#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "inspection.h"
#include "binary_view.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <filesystem>
using namespace recvx;
// Komponentenpruefung des Greif-und-Dreh-Gegenstands: objitm.c:1648 bhObj007
// (Typ-7-Objekt, nur RM_0150 Objekt 5, die Eisenstatue) zusammen mit der
// Spieleraktion player.c:4336 bhCPM2_act_rpsh (mode2 22). Ereignis 16 hebt die
// Statue auf py 10 (bhCommonCtr 07), danach erkennt Fall 1 den gehenden
// Spieler, Fall 2/3 dreht die Statue bis |(short)ay| < 182 und setzt ev_flg 2.
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    if(argc<2)return 0;
    _Make_SinTable();
    const std::filesystem::path data=argv[1];
    AudioMixer audio(data);
    auto bytes=read_asset_file(data/"rooms/RM_0150.RDT",64*1024*1024);
    auto scene=decode_scene(bytes);
    auto player=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;
    RoomWorld world(bytes,scene,player,motion,frame,audio,data);
    auto asset=PlayerAsset::load(data);world.set_player_asset(&asset);
    Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);
    vm.binding=[&](const EventCommand& c){return world.bind(c);};
    vm.task_started=[&](unsigned l){world.start_task(l);};
    vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};
    world.bind_player_collision(vm);
    vm.initialize(); // scd0 stellt die Statue (Objekt 5) auf [30,0,40], ay 180
    const auto& statue_before=*world.state(2,5);
    check(statue_before.type==7&&statue_before.transform.position.y<1.0f,"RM_0150 Objekt 5 ist nicht die Typ-7-Statue am Boden");
    check(!world.turning(),"Drehaktion startet ohne Greifen");
    // Ereignis 16 (objitm-Skriptpfad, hier direkt gebunden): bhLoadWork Objekt 5,
    // bhCommonCtr 0d/0b/0c/07 -> [30,10,40], ay 0.
    auto cmd=[&](std::vector<std::uint8_t> b){EventCommand c;c.opcode=b[0];c.bytes=std::move(b);return world.bind(c);};
    check(cmd({0x65,0x02,0x05,0x00}),"bhLoadWork Objekt 5 bindet nicht");
    check(cmd({0x69,0x0d,0x00,0x01,0x00,0x00}),"Common 0d bindet nicht");
    check(cmd({0x69,0x0b,0x00,0xb4,0x00,0x00}),"Common 0b bindet nicht"); // bay 1 -> ay -180 (0x8000)
    check(cmd({0x69,0x0c,0x00,0x00,0x00,0x00}),"Common 0c bindet nicht");
    check(cmd({0x69,0x07,0xb8,0x0b,0xe8,0x03,0xa0,0x0f}),"Common 07 (py 10) bindet nicht");
    const auto& statue=*world.state(2,5);
    check(statue.transform.position.y>=1.0f,"Statue nicht auf py 10 gehoben");
    check(int(statue.angles[1])==-32768,"Statuenwinkel nicht auf -180 Grad gesetzt");
    // Spieler seitlich an die Statue (z < 40 = Vorderseite, x 32 neben der Linie,
    // Blick zur Linie = ay 16384), gehend. bhObj007:1707/1736 verlangen die
    // Annaeherung senkrecht zur Frontlinie der Statue.
    world.set_simulation_flags(0x4u); // sp_flg 0x4: bhControlObjItm laeuft (game.c)
    player.position={32,0,36};player.yaw=16384;
    world.set_player_mode(3u|(0u<<8)|(3u<<16)|(0u<<24)); // mode0 3, mode2 3 (gehen)
    world.set_pad(0x1u,0x1u); // pad_on & 1 = vorwaerts (pad.c: analog vorwaerts 0x21)
    unsigned turn_frames=0;
    for(unsigned t=0;t<120&&!world.turning();++t){
        // der gehende Spieler stoesst je Bild in den Greifbereich (len2 < ar),
        // bhObj007 Fall 1 schnappt ihn zurueck und zaehlt ct0 hoch.
        player.position={33,0,36};player.yaw=16384;
        world.tick();++turn_frames;
    }
    check(world.turning(),"bhObj007 Fall 1 hat den gehenden Spieler nicht gegriffen (mode2 22)");
    check(((world.player_mode()>>16)&0xffu)==22u,"Greifen setzt plp->mode2 nicht auf 0x16");
    // Drehen: Vorwaerts halten, bis die Statue am Ziel (|ay| < 182) ist und ev_flg 2 faellt.
    const int start_ay=int(world.state(2,5)->angles[1]);
    bool completed=false;unsigned guard=0;
    bool rotated_visibly=false;
    for(unsigned t=0;t<600&&!completed;++t){
        world.tick_turn(true);
        world.tick();
        {const auto& s=*world.state(2,5);if(int(s.angles[1])!=start_ay&&int(s.angles[1])!=0&&int(s.transform.yaw)==int(Angle(s.angles[1])))rotated_visibly=true;}
        if(vm.flag(1,2))completed=true;
        if(++guard>600)break;
    }
    check(rotated_visibly,"Zeichenwurzel (transform.yaw) folgt op->ay waehrend des Drehens nicht");
    check(completed,"Drehen erreicht ev_flg 2 nicht");
    const int end_ay=int(world.state(2,5)->angles[1]);
    check(end_ay==0&&start_ay!=end_ay,"Statue dreht sich nicht auf 0");
    check(world.state(2,5)->object007_mode0==4,"Statue nicht im Fertigzustand mode0 4");
    // Ausklang: bhObj007 setzte plp->mode3 6, rpsh Fall 6/7 spult zurueck und gibt frei.
    for(unsigned t=0;t<200&&world.turning();++t){world.tick_turn(false);world.tick();}
    check(!world.turning(),"Drehaktion endet nicht");
    std::cout<<"turn_object: Statue von "<<start_ay<<" auf "<<end_ay<<" gedreht, ev_flg 2, "<<turn_frames<<" Greifbilder.\n";
}catch(const std::exception& e){std::cerr<<"turn_object_test: "<<e.what()<<"\n";return 1;}
return 0;}
