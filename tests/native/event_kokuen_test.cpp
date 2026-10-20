// event.c:1986 bhEffectKokuenSet (Skriptbefehl 0x97): Effekt 2 type 1, Lage =
// bhEtask[v0].work->mlwP->objP->pos (LOKALE Lage von objP[0]) + u16/100 in x/z, y nur u16/100.
// Diagnose: RM_9110 Objekt 10 (Weltlage x 3.41, z -5.53 aus der Raumdatei) wird ueber
// bhLoadWork Fall 2 (0x68, event.c:12938-12945) Task 1 mit model_cno 3 zugewiesen; dann setzt
// Task 0 den Rauch mit Versatz 1.00/2.00/3.00. Das Original addiert weder die Weltlage px/pz des
// Works noch einen model_cno-Knoten.
#include "world.h"
#include "room.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
static bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");_Make_SinTable();const std::filesystem::path data=argv[1];
    auto bytes=RoomArchive(data).load("RM_9110.RDX",64*1024*1024);auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);auto player=PlayerAsset::load(data);
    Inventory inv;MessageDialog dialog;EventVm events(bytes,inv,dialog);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_9110");world.set_player_asset(&player);world.bind_player_collision(events);
    const auto* object=world.state(2,10);check(object,"RM_9110 Objekt 10 fehlt");
    const auto root=world.model_pose(2,10,0).position,place=object->transform.position;
    check(!near(root.x,place.x)&&!near(root.z,place.z),"Vorbedingung: lokale Wurzellage und Weltlage muessen sich unterscheiden");
    check(world.bind({0x68,0,0,0,{0x68,2,0,1,10,3}}),"0x68 Fall 2 nicht gebunden");
    check(world.bind({0x97,0,0,0,{0x97,1,100,0,200,0,44,1}}),"0x97 nicht gebunden");
    const EffectSprite* smoke=nullptr;for(unsigned i=0;const auto* e=world.effect_state(i);++i)if((e->flg&1u)&&e->id==2)smoke=e;
    check(smoke&&smoke->type==1&&near(smoke->sx,4)&&near(smoke->sy,4)&&near(smoke->sz,4),"Effekt 2 type 1 mit Groesse 4 fehlt");
    check(near(smoke->px,1.0f+root.x)&&near(smoke->py,2.0f)&&near(smoke->pz,3.0f+root.z),
          "Rauch muss bei Versatz + objP[0].pos liegen (x "+std::to_string(smoke->px)+", z "+std::to_string(smoke->pz)+")");
    {   // event.c:7159 bhEffUVSet2 (0xAA). Echte Bytes: RM_9180, Offset 424 hinter der Ereignistabelle,
        // Raumeffekt 1 = bhEffFil (92), UV (0,292)-(257,357) in 1/512. pvp = op->pv als NJS_TEXTUREH_VTX.
        auto room=RoomArchive(data).load("RM_9180.RDX",64*1024*1024);auto stage=decode_scene(room);auto spawn=stage.spawns.at(0);
        std::size_t m=42;std::int64_t f=0;RoomWorld w(room,stage,spawn,m,f,audio,data,{},"RM_9180");
        const auto base=inspect_room(room).event_offset;const BinaryView v(room);
        check(v.u8(base+424)==0xAA&&v.u8(base+425)==1,"RM_9180: 0xAA nicht an der erwarteten Stelle");
        const auto* fil=w.effect_state(1);check(fil&&fil->id==92,"RM_9180 Raumeffekt 1 ist nicht bhEffFil");
        const auto before=*fil;
        check(w.bind({0xAA,0,0,base+424,{room.begin()+long(base+424),room.begin()+long(base+434)}}),"0xAA nicht gebunden");
        const float u0=0,v0=292/512.0f,u1=257/512.0f,v1=357/512.0f;
        check(near(fil->pv[3],u0)&&near(fil->pv[4],v0)&&near(fil->pv[10],u1)&&near(fil->pv[11],v0),"H-Eintraege 0/1 muessen pv[0].col, pv[1].x, pv[2].z/col treffen");
        check(near(fil->tv[0].y,u0)&&near(fil->tv[0].z,v1)&&near(fil->tv[1].z,u1)&&near(fil->tv[1].u,v1),"H-Eintraege 2/3 muessen tv[0].y/z und tv[1].z/u treffen");
        check(near(fil->tv[0].u,before.tv[0].u)&&near(fil->tv[0].v,before.tv[0].v)&&near(fil->tv[1].v,before.tv[1].v)&&near(fil->tv[2].u,before.tv[2].u)&&near(fil->tv[3].v,before.tv[3].v)&&near(fil->tv[0].x,before.tv[0].x),
              "0xAA darf die uebrigen tv-Felder nicht beruehren");
        // RM_1050 Raumeffekt 1 = bhEff111: dort fuehrt der Port die H-Eckpunkte in tv[i].u/v.
        auto room2=RoomArchive(data).load("RM_1050.RDX",64*1024*1024);auto stage2=decode_scene(room2);auto spawn2=stage2.spawns.at(0);
        RoomWorld w2(room2,stage2,spawn2,m,f,audio,data,{},"RM_1050");const auto* mon=w2.effect_state(1);check(mon&&mon->id==111,"RM_1050 Raumeffekt 1 ist nicht bhEff111");
        check(w2.bind({0xAA,0,0,0,{0xAA,1,0,0,0x24,1,1,1,0x65,1}}),"0xAA (bhEff111) nicht gebunden");
        check(near(mon->tv[0].u,u0)&&near(mon->tv[0].v,v0)&&near(mon->tv[1].u,u1)&&near(mon->tv[1].v,v0)&&near(mon->tv[2].u,u0)&&near(mon->tv[2].v,v1)&&near(mon->tv[3].u,u1)&&near(mon->tv[3].v,v1),
              "bhEff111: H-Eckpunkte muessen in tv[i].u/v landen");
    }
    std::cout<<"bhEffectKokuenSet/bhEffUVSet2 PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
