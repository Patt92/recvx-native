// Schnitt-Beschraenkung der Raumobjekte und Raumitems.
//
// objitm.c:659 (bhDrawObject), :688 (bhDrawSpObject) und :562 (bhDrawObjItm,
// Items) pruefen woertlich dieselbe Bedingung, bevor bhPutModel laeuft:
//   (!(op->flg & 0x40000000)) || (sys->gm_flg & 0x40) || (cam.flg & 0x46)
//   || cam.ncut == op->hide[0..3]
// Die vier hide[]-Bytes kommen aus ETTY_WORK (objitm.c:200-203 bhSetObject,
// :262-265 bhSetItem); bhSetEneMdl kopiert sie nicht, Gegner kennen die
// Bedingung also nicht (eneset.c bhDrawEnemy/bhDrawEneObject).
//
// Geprueft wird mit den Originaldaten von RM_0090: dort traegt Objekt 20
// (id 786, flg 0xC0000001, hide [7,255,255,255]) die Leuchtflaeche ueber der
// Tuer und wurde vorher in jedem Schnitt gezeichnet.
#include "../../native/world.h"
#include "../../native/asset_io.h"
#include "../../native/binary_view.h"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}

// Zaehlt die gezeichneten Dreiecke einer Werkart. RoomWorld::append setzt
// light_category auf die Werkart (game.c:318).
static unsigned drawn(RoomWorld& world,const Scene& stage,std::size_t cut,unsigned kind){
    world.tick_camera(stage,stage.cameras.at(cut),cut);
    Scene out;std::vector<BoneMatrix> nodes;world.append(out,nodes,stage.cameras.at(cut));
    unsigned n=0;for(const auto& t:out.triangles)if(t.light_category==kind)++n;
    return n;
}

int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("usage: recvx-object-cut-tests <exported-native-data>");
    _Make_SinTable();
    const std::filesystem::path data=std::filesystem::u8path(argv[1]);
    const auto room=read_asset_file(data/"rooms/RM_0090.RDT",64*1024*1024);
    const auto stage=decode_scene(room);
    auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0090");
    world.tick();

    // 1) hide[] stammt wirklich aus den vier Bytes am ETTY_WORK-Satzende.
    const BinaryView v(room);const auto header=v.u32(16);
    const auto objp=v.u32(header+12),obj_count=v.u32(header+0x8c);
    const auto itmp=v.u32(header+16),item_count=v.u32(header+0x90);
    unsigned restricted=0;
    for(unsigned i=4;i<obj_count;++i){
        const auto* state=world.state(2,i);check(state,"room object work missing");
        for(unsigned j=0;j<4;++j)check(state->object_hide[j]==v.u8(objp+i*36+32+j),"ETTY_WORK hide[] not copied into the object work");
        if(v.u32(objp+i*36)&0x40000000u)++restricted;
    }
    for(unsigned i=0;i<item_count;++i){
        const auto* state=world.state(3,i);check(state,"room item work missing");
        for(unsigned j=0;j<4;++j)check(state->object_hide[j]==v.u8(itmp+i*36+32+j),"ETTY_WORK hide[] not copied into the item work");
    }
    check(restricted==7,"RM_0090 no longer carries its seven cut-restricted objects");
    check(world.state(2,20)->object_flags==0xC0000001u,"RM_0090 object 20 flags differ");
    check(world.state(2,20)->object_hide[0]==7&&world.state(2,20)->object_hide[1]==255,"RM_0090 object 20 hide list differs");
    check(world.state(2,19)->object_hide[0]==255,"an unrestricted object acquired a hide entry");
    // bhSetEneMdl kopiert hide[] nicht; der Gegnerwork behaelt den Initialwert.
    for(unsigned i=0;i<5;++i)if(const auto* e=world.state(1,i))for(unsigned j=0;j<4;++j)check(e->object_hide[j]==255,"enemy work received a hide list");

    // 2) Zeichenentscheidung. In RM_0090 nennt keine hide[]-Liste Schnitt 1;
    //    Schnitt 0 gehoert Objekt 14 (hide [0,8,6]), Schnitt 7 Objekt 20.
    const auto neutral=drawn(world,stage,1,2);
    const auto cut0=drawn(world,stage,0,2);
    const auto cut7=drawn(world,stage,7,2);
    if(!(neutral<cut0&&neutral<cut7))
        std::cerr<<"kind-2 Dreiecke: Schnitt1="<<neutral<<" Schnitt0="<<cut0<<" Schnitt7="<<cut7<<'\n';
    check(neutral>0,"the unrestricted room objects stopped drawing");
    check(neutral<cut0,"the cut-0 object is missing from its own cut");
    check(neutral<cut7,"the cut-7 object is missing from its own cut");
    // Objekt 20 ist ein einzelner Streifen mit zwei Dreiecken (ein Knoten,
    // CsUvh mit einem Strip der Laenge 4).
    check(cut7-neutral==2,"the cut-7 object does not contribute exactly its original quad");
    // Die Beschraenkung gilt nur fuer Objekte/Items, nicht fuer Gegner.
    check(drawn(world,stage,1,1)==drawn(world,stage,7,1),"the cut restriction reached the enemy works");

    // 3) objitm.c:664-676 mdflg 0x10 (ETTY flg 0x80000000, :164): unter st_flg 0x2 (Nebel an) laeuft bhPutModel
    //    zwischen njFogDisable und njFogEnable. Objekt 20 (flg 0xC0000001) ist das einzige Objekt mit Bit 31 in
    //    RM_0090 und zeichnet nur in Schnitt 7; sein Streifen (zwei Dreiecke) muss dort ohne Nebel stehen, alle
    //    anderen Objekte mit Nebel. Ohne st_flg 0x2 (:665 zweite Bedingung) bleibt auch Objekt 20 nebelig.
    //    Gegenprobe: world.cpp put_model_mdflg ohne 0x10 bzw. Nebeltor ohne st_flg 0x2 -> rot.
    {
        Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
        // Mit gebundener Event-VM liest object_cut_hidden cam.ncut aus events->values[2] (bhCmpB key 2), wie runtime.cpp.
        const auto unfogged=[&](std::size_t cut){events.values[2]=std::int32_t(cut);world.tick_camera(stage,stage.cameras.at(cut),cut);
            Scene out;std::vector<BoneMatrix> nodes;world.append(out,nodes,stage.cameras.at(cut));
            unsigned off=0,on=0;for(const auto& t:out.triangles)if(t.light_category==2){if(t.fogged)++on;else ++off;}
            return std::make_pair(off,on);};
        events.flags[5][0]|=0x2u;
        const auto fog_on=unfogged(7);
        check(fog_on.first==2,"mdflg 0x10: object 20 must draw its two triangles without fog while st_flg 0x2 is set");
        check(fog_on.second==cut7-2,"mdflg 0x10: the other objects of cut 7 must keep the room fog");
        check(unfogged(1).first==0,"mdflg 0x10: cut 1 (object 20 hidden) has no unfogged object");
        events.flags[5][0]&=~0x2u;
        check(unfogged(7).first==0,"mdflg 0x10: without st_flg 0x2 bhDrawObject does not touch the fog (objitm.c:665)");
        events.flags[5][0]|=0x2u;
    }

    std::cout<<"Original objitm.c cut restriction (flg 0x40000000 / hide[0..3]) passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
