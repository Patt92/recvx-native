/* objitm.c:496-618 bhDrawObjItm: Lichtumschaltung der Items und ihre Nachwirkung.
   Erwartungen kommen aus den Originalzeilen und eigenen Bezugsrechnungen (light.c:1067-1078
   bhGetLightVector, screen.c:971-990, light.c:279-328), nicht aus dem nativen Zeichenpass:

   1. objitm.c:403-407 bhControlObjItm (Items): der Glanzzaehler laeuft nur ohne stflg 0x1000000
      (bhInitModelSet 0x24, event.c:2874-2884); mdflg 0x1 (bhSetDispObj 0x20, event.c:2320-2327)
      sperrt erst das Zeichnen (objitm.c:560), nicht den Zaehler und nicht den rand()-Aufruf.
   2. objitm.c:569-585: mdflg 0x80 -> Licht 1 = bhGetLightVector(57344, ayp, 0), Farbe 0.1*aspd;
      Umgebung amb_itm (:545-546). ayp ist 0 im ersten Glanzbild (:426-428) und waechst um 8192 (:436).
   3. Das Licht 1 des letzten Items, das die Tore bis :562 passiert, bleibt fuer bhDrawEffect stehen
      (game.c:342-348, Umgebung amb_chr :344-345): Effektmodelle (bhEff_ModelDraw, njCnkSimpleMultiDrawModel)
      sehen dann den Glanz. Nicht mit ef_flg 0x4 (effect.c:924-927 bhSetHalfLight), nicht ohne
      pt_flg 0x8 (:550), nicht in den Albinoid-Raeumen (game.c:339-342, bhDrawEffect vor bhDrawObjItm).
   4. Monitorbild (screen.c:947/987: gm_flg 0x80, st_flg 0x20): bhDrawObjItm laesst bhSetLight/
      bhSetHalfLight aus (:507/:520/:542), es gilt das Einzellicht aus screen.c:971-990 - in Stufe 7
      Farbe 0.3/0.3/1.0, Intensitaet 4.0, Umgebung 0.5, Richtung 0.35/-0.15/0.5.
   5. Augenkamera (gm_flg 0xC0, cut.c:2645-2646): bhDrawSpObject mit op->draw_tp 0 setzt nur den
      EasyMulti-Satz (objitm.c:698-705), bhPutModel zeichnet aber mit dem Einzellicht (MdlPut.c:33-37),
      das bhSetHalfLight -> bhSetEasyDirLight(0.6) (light.c:1003, :321-327) hinterliess.

   Raumdaten: RM_6020 Item 9 (letztes Item, aspd 5, flg 1), Items 0/1 ebenfalls aspd 5;
   RM_7100 (Stufe 7 Raum 10, Albinoid) Item 2 aspd 5; RM_7030 Monitor (bhEff111 Raumeffekt 3, Schnitt 5);
   RM_1050 Monitor (Schnitt 6, Stufe 1); RM_4000 Objekt 8 (Typ 5, aspd 7, draw_tp 0 -> ob_spc).
   Alle Raumlichter (rom->lgtp, rom->evlp) stehen auf flg 0, die vier Umgebungsplaetze tragen
   unterscheidbare Werte und amb_rom/chr/obj/itm zeigen vertauscht auf sie (Pruefaufbau, kein Spielzustand).

   Gegenproben (jeweils eine Aenderung in native/world.cpp, Test muss rot werden, danach zurueck und touch):
   - control_item_glint: e.inactive -> e.hidden                                   -> "Zaehler"
   - item_glint_vector: njRotateX(&m,57344) -> njRotateX(&m,8192)                 -> "Glanzrichtung"
   - item_light_leak immer false                                                   -> "Nachwirkung"
   - easy_screen: Stufe-7-Zweig entfernt                                           -> "Monitor Stufe 7"
   - ob_spc draw_tp 0 unter gm_flg 0x80: mode=2 mit light=aspd (alter Stand)       -> "Augenkamera ob_spc"
   - item_light_leak ohne !albinoid_early                                          -> "Albinoid"
   - item_light_leak ohne !effect_half_light                                       -> "ef_flg 0x4"
   Alle sieben am 2026-10-20 rot gemessen (privater Link unter $TMPDIR, Bibliothek ohne world.cpp.o). */
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "binary_view.h"
#include "events.h"
#include "inventory.h"
#include "messages.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
static bool approx(float a,float b){return std::fabs(a-b)<2e-4f;}

namespace{
// Umgebungsplatz k: r = 0.1(k+1), g = 0.05(k+1), b = 0.02(k+1); amb_rom 3, amb_chr 0, amb_obj 1, amb_itm 2.
constexpr unsigned kAmbRom=3,kAmbChr=0,kAmbObj=1,kAmbItm=2;
NJS_POINT3 ambient_slot(unsigned k){return {0.1f*float(k+1),0.05f*float(k+1),0.02f*float(k+1)};}

struct Room{
    std::vector<std::uint8_t> bytes;std::size_t header=0;std::string name;
    Room(const std::filesystem::path& data,const std::string& room):name(room){
        bytes=read_asset_file(data/"rooms"/(room+".RDT"),64*1024*1024);
        const BinaryView v(bytes);header=inspect_room(bytes).room_offset;
        const auto lights=v.u32(header+4),light_count=v.u32(header+0x84),ev=v.u32(header+0x3c),ev_count=v.u32(header+0xb4);
        for(unsigned i=0;i<light_count;++i)u32(lights+i*0xe0,0);
        for(unsigned i=0;i<ev_count;++i)u32(ev+i*0xe0,0);
        bytes.at(header+0x3b8)=kAmbRom;bytes.at(header+0x3b9)=kAmbChr;bytes.at(header+0x3ba)=kAmbObj;bytes.at(header+0x3bb)=kAmbItm;
        for(unsigned k=0;k<4;++k){const auto a=ambient_slot(k);f32(header+0x3bc+k*4,a.x);f32(header+0x3cc+k*4,a.y);f32(header+0x3dc+k*4,a.z);}
    }
    void u32(std::size_t p,std::uint32_t x){for(unsigned j=0;j<4;++j)bytes.at(p+j)=std::uint8_t(x>>(8*j));}
    void f32(std::size_t p,float x){std::uint32_t bits;std::memcpy(&bits,&x,4);u32(p,bits);}
    std::uint32_t item_flags(unsigned i)const{const BinaryView v(bytes);return v.u32(v.u32(header+16)+i*36);}
    int item_aspd(unsigned i)const{const BinaryView v(bytes);return std::int16_t(v.u16(v.u32(header+16)+i*36+30));}
    unsigned item_count()const{return BinaryView(bytes).u32(header+0x90);}
    const std::uint8_t* object(unsigned i)const{const BinaryView v(bytes);return &bytes.at(v.u32(header+12)+i*36);}
};
struct World{
    Scene stage;SceneSpawn actor;std::size_t motion=0;std::int64_t frame=0;AudioMixer audio;RoomWorld world;
    Inventory inventory;MessageDialog dialog;EventVm events;
    World(const Room& r,const std::filesystem::path& data):stage(decode_scene(r.bytes)),actor(stage.spawns.at(0)),audio(data),
        world(r.bytes,stage,actor,motion,frame,audio,data,{},r.name),events(r.bytes,inventory,dialog){world.bind_player_collision(events);world.set_simulation_flags(0x4u);}
    bool command(std::vector<std::uint8_t> b){EventCommand c;c.opcode=b.at(0);c.bytes=std::move(b);return world.bind(c);}
    void disp(unsigned item,bool shown){check(command({0x20,std::uint8_t(item),3,std::uint8_t(shown?1:0)}),"0x20 bhSetDispObj Item nicht gebunden");}
    void init_model(unsigned item,bool active){check(command({0x24,3,std::uint8_t(item),std::uint8_t(active?1:0)}),"0x24 bhInitModelSet Item nicht gebunden");}
};
// light.c:1067-1078 bhGetLightVector(xr, yr, zr): Einheitsmatrix, RotZ, RotY, RotX, Punkt (0, 0, -1).
NJS_POINT3 light_vector(int xr,int yr,int zr){NJS_MATRIX m;njUnitMatrix(&m);njRotateZ(&m,zr);njRotateY(&m,yr);njRotateX(&m,xr);NJS_POINT3 p{0,0,-1.0f},out;njCalcPoint(&m,&p,&out);return out;}
NJS_VECTOR unit_or_face(const SceneTriangle& t,const SceneVertex& vx){
    NJS_VECTOR n=vx.normal;
    if(n.x*n.x+n.y*n.y+n.z*n.z<.0001f){NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z},
        b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z};
        n={a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
    const float l=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);return {n.x/l,n.y/l,n.z/l};
}
// ps2_NinjaCnk.c Mehrfachlicht, Richtungslicht ohne Punktlichter: Umgebung * Material + Farbe * max(0, -v.n).
NJS_POINT3 multi_directional(NJS_POINT3 ambient,NJS_POINT3 material,NJS_POINT3 color,NJS_POINT3 v,NJS_VECTOR n){
    const float f=std::max(0.f,-(v.x*n.x+v.y*n.y+v.z*n.z));
    return {std::clamp(ambient.x*material.x+color.x*f,0.f,1.f),std::clamp(ambient.y*material.y+color.y*f,0.f,1.f),std::clamp(ambient.z*material.z+color.z*f,0.f,1.f)};
}
// Einzellicht (ps2_NinjaCnk.c:252-276): fC = -v (Kameraraum), fI, Umgebung fR/fG/fB.
NJS_POINT3 easy_light(NJS_VECTOR n_view,NJS_POINT3 v,NJS_POINT3 color,float intensity,float ambient){
    const float f=std::max(0.f,-(n_view.x*v.x+n_view.y*v.y+n_view.z*v.z))*intensity;
    return {std::clamp(ambient+f*color.x,0.f,1.f),std::clamp(ambient+f*color.y,0.f,1.f),std::clamp(ambient+f*color.z,0.f,1.f)};
}
bool same(NJS_POINT3 a,NJS_POINT3 b){return approx(a.x,b.x)&&approx(a.y,b.y)&&approx(a.z,b.z);}
std::string str(NJS_POINT3 p){return "("+std::to_string(p.x)+","+std::to_string(p.y)+","+std::to_string(p.z)+")";}

// Effektmodell-Probe (light_category 5 wie effect_model_mirror prepare, ungespiegelt, Material 1).
SceneTriangle effect_probe(NJS_VECTOR n){
    SceneTriangle t;t.camera_mask=false;t.light_category=5;t.mirror=0;t.chunk_ambient=false;
    t.vertices={SceneVertex{{0,0,0}},SceneVertex{{1,0,0}},SceneVertex{{0,0,1}}};for(auto& v:t.vertices)v.normal=n;return t;
}
NJS_POINT3 probe_light(World& w,const SceneTriangle& probe,const SceneCamera& cam,const WorldDrawPass& pass={}){
    Scene out;out.triangles.push_back(probe);w.world.append(out,{},cam,pass);return out.triangles.at(0).vertices[0].light;
}
// Tickt bis zum ersten Glanzbild des Items (ct0 1 -> 2, objitm.c:422-431); hoechstens 30+60+2 Bilder.
void tick_to_glint(World& w,unsigned item){
    for(unsigned t=0;t<100;++t){w.world.tick();if(w.world.state(3,item)->item_glint)return;}
    throw std::runtime_error("Item "+std::to_string(item)+" erreicht den Glanz nicht innerhalb von 100 Bildern");
}
}

int main(int argc,char** argv){
    try{
        if(argc<2)throw std::runtime_error("usage: item_draw_light_test <data>");
        const std::filesystem::path data=argv[1];
        _Make_SinTable();
        const Room rm6020(data,"RM_6020");
        check(rm6020.item_count()==10&&rm6020.item_aspd(9)==5&&(rm6020.item_flags(9)&0x40000001u)==1u&&rm6020.item_aspd(8)==0,"RM_6020: Item 9 ist nicht das letzte Glanzitem (aspd 5, flg 1)");
        const SceneCamera cam6020=decode_scene(rm6020.bytes).cameras.at(0);

        // 1. Zaehlertor: stflg 0x1000000 haelt ihn an, mdflg 0x1 nicht (objitm.c:407).
        {World w(rm6020,data);w.init_model(9,false);w.world.tick();
         check(w.world.state(3,9)->item_ct0==0,"Zaehler: Item mit stflg 0x1000000 (0x24) darf bhControlObjItm nicht durchlaufen");
         check(w.world.state(3,0)->item_ct0==1,"Zaehler: Item 0 (aspd 5) muss im ersten Bild ct0 = 1 setzen (default-Zweig :448-454)");}
        {World w(rm6020,data);w.disp(9,false);w.world.tick();
         check(w.world.state(3,9)->item_ct0==1,"Zaehler: mdflg 0x1 (0x20) sperrt nur das Zeichnen, der Glanzzaehler laeuft weiter");
         w.init_model(9,true);w.disp(9,true);}

        // 2. Glanz des Items selbst: nur Item 9 zeichnet (0..8 per 0x20 aus, ihr Zaehler laeuft weiter).
        {World w(rm6020,data);for(unsigned i=0;i<9;++i)w.disp(i,false);
         tick_to_glint(w,9);
         const auto* s=w.world.state(3,9);
         check(s->item_ct0==2&&s->item_ayp==0,"Glanz: erstes Glanzbild muss ct0 2 und ayp 0 tragen (objitm.c:426-431)");
         for(unsigned frame=0;frame<9;++frame){
            s=w.world.state(3,9);
            const bool glint=frame<8;
            check(s->item_glint==glint,"Glanz: mdflg 0x80 genau acht Bilder (Bild "+std::to_string(frame)+")");
            if(glint)check(s->item_ayp==int(8192*frame),"Glanz: ayp += 8192 je Bild (objitm.c:436)");
            const NJS_POINT3 vec=light_vector(57344,s->item_ayp,0);const float its=0.1f*5.0f;
            Scene out;w.world.append(out,{},cam6020);
            unsigned items=0,lit=0;
            for(const auto& t:out.triangles){if(t.light_category!=3)continue;++items;
                const NJS_POINT3 material=t.chunk_ambient?t.ambient_material:NJS_POINT3{1,1,1};
                for(const auto& vx:t.vertices){const auto n=unit_or_face(t,vx);
                    const auto expect=glint?multi_directional(ambient_slot(kAmbItm),material,{its,its,its},vec,n):multi_directional(ambient_slot(kAmbItm),material,{0,0,0},vec,n);
                    if(!same(vx.light,expect))throw std::runtime_error("Glanzrichtung/amb_itm: Bild "+std::to_string(frame)+" Licht "+str(vx.light)+" erwartet "+str(expect));
                    if(glint&&-(vec.x*n.x+vec.y*n.y+vec.z*n.z)>0.05f)++lit;}}
            check(items>0,"Glanz: Item 9 zeichnet nicht");
            if(glint)check(lit>0,"Glanz: kein Eckpunkt zeigt zum Glanzlicht (Bild "+std::to_string(frame)+")");
            w.world.tick();}}

        // 3. Nachwirkung auf bhDrawEffect: Probe mit Normale -vec zeigt genau zum Glanzlicht.
        {World w(rm6020,data);tick_to_glint(w,9);
         const auto* s=w.world.state(3,9);const NJS_POINT3 vec=light_vector(57344,s->item_ayp,0);
         const float len=std::sqrt(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z);const NJS_VECTOR n{-vec.x/len,-vec.y/len,-vec.z/len};
         const auto chr=ambient_slot(kAmbChr);const float its=0.5f;
         const auto leak=probe_light(w,effect_probe(n),cam6020);
         check(same(leak,{std::min(1.f,chr.x+its*len),std::min(1.f,chr.y+its*len),std::min(1.f,chr.z+its*len)}),"Nachwirkung: Effektmodell sieht das Glanzlicht des letzten Items nicht: "+str(leak));
         // Gegenprobe ohne Glanzrichtung: die Normale +vec bekommt nichts vom Glanz.
         check(same(probe_light(w,effect_probe({-n.x,-n.y,-n.z}),cam6020),chr),"Nachwirkung: abgewandte Normale darf nur amb_chr tragen");
         // ef_flg 0x4: bhSetHalfLight vor den ef_fnc-Modellen (effect.c:924-927).
         w.world.set_effect_flags(0x4u);check(same(probe_light(w,effect_probe(n),cam6020),chr),"Nachwirkung: ef_flg 0x4 setzt Licht 1 zurueck");w.world.set_effect_flags(0);
         // pt_flg ohne 0x8: keine Itemschleife (:550), Licht 1 bleibt bei bhSetLight (:542).
         w.world.set_presentation_flags(~0x8u);check(same(probe_light(w,effect_probe(n),cam6020),chr),"Nachwirkung: ohne pt_flg 0x8 schreibt kein Item Licht 1");w.world.set_presentation_flags(~0u);
         // Item 9 per 0x20 aus: letztes Item mit Licht ist 8 (ohne Glanz) -> :587-604 bhSetLight-Zustand.
         w.disp(9,false);check(same(probe_light(w,effect_probe(n),cam6020),chr),"Nachwirkung: ohne Glanz des letzten Items gilt der bhSetLight-Zustand");
         w.disp(9,true);check(same(probe_light(w,effect_probe(n),cam6020),leak),"Nachwirkung: nach 0x20 v2 1 zeichnet Item 9 wieder mit Glanz");}
        // Albinoid-Raum RM_7100 (Stufe 7, Raum 10): bhDrawEffect laeuft vor bhDrawObjItm, keine Nachwirkung.
        {const Room rm7100(data,"RM_7100");check(rm7100.item_aspd(2)==5&&rm7100.item_count()==5,"RM_7100: Item 2 traegt nicht aspd 5");
         World w(rm7100,data);w.disp(3,false);w.disp(4,false); // Item 2 ist damit das letzte gezeichnete Item
         tick_to_glint(w,2);const auto* s=w.world.state(3,2);const NJS_POINT3 vec=light_vector(57344,s->item_ayp,0);
         const float len=std::sqrt(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z);
         const auto rom=ambient_slot(kAmbRom); // Albinoid: Umgebung amb_rom (system.c:1647-1657, world.cpp category 0)
         check(same(probe_light(w,effect_probe({-vec.x/len,-vec.y/len,-vec.z/len}),decode_scene(rm7100.bytes).cameras.at(0)),rom),"Nachwirkung: im Albinoid-Raum zeichnet bhDrawEffect vor bhDrawObjItm");}

        // 4. Monitorbild: Einzellicht aus screen.c:971-990.
        const auto monitor=[&](const std::string& name,unsigned cut,NJS_POINT3 v,NJS_POINT3 color,float intensity,float ambient,const char* why){
            const Room r(data,name);World w(r,data);const SceneCamera cam=decode_scene(r.bytes).cameras.at(cut);
            NJS_MATRIX view;camera_view_matrix(cam,view);
            unsigned graded=0; // Faelle mit Richtungsanteil unterhalb der Klemmung: pruefen v, Farbe und Intensitaet
            for(const NJS_VECTOR n:{NJS_VECTOR{0,1,0},NJS_VECTOR{0,0,1},NJS_VECTOR{1,0,0},NJS_VECTOR{0,-1,0},NJS_VECTOR{0,0,-1},NJS_VECTOR{-1,0,0},
                                    NJS_VECTOR{0.6f,0.8f,0},NJS_VECTOR{0,0.6f,-0.8f},NJS_VECTOR{-0.6f,0,0.8f},NJS_VECTOR{0.8f,-0.6f,0}}){
                NJS_VECTOR c;NJS_VECTOR in=n;njCalcVector(&view,&in,&c);const auto expect=easy_light(c,v,color,intensity,ambient);
                if(expect.x>ambient+0.01f&&expect.x<0.99f)++graded;
                SceneTriangle probe=effect_probe(n);probe.light_category=1; // Probe wie light_clip_test: Kategorie 1
                const auto got=probe_light(w,probe,cam,WorldDrawPass{true,cut});
                if(!same(got,expect))throw std::runtime_error(std::string(why)+": "+str(got)+" erwartet "+str(expect));
                SceneTriangle room=probe;room.camera_mask=true; // Raummodell (njCnkEasyDrawObject unter gm_flg 0x80)
                const auto got_room=probe_light(w,room,cam,WorldDrawPass{true,cut});
                if(!same(got_room,expect))throw std::runtime_error(std::string(why)+" (Raum): "+str(got_room)+" erwartet "+str(expect));}
            check(graded>0,std::string(why)+": keine Probe mit Richtungsanteil unter der Klemmung");};
        monitor("RM_7030",5,{0.35f,-0.15f,0.5f},{0.3f,0.3f,1.0f},4.0f,0.5f,"Monitor Stufe 7");
        monitor("RM_1050",6,{0,0.3f,0.7f},{1,1,1},1.0f,0.2f,"Monitor Stufe 1");

        // 5. Augenkamera: ob_spc draw_tp 0 (RM_4000 Objekt 8) zeichnet mit dem Einzellicht von bhSetHalfLight.
        {const Room rm4000(data,"RM_4000");const auto* o=rm4000.object(8);
         check(o[6]==5&&std::int16_t(o[30]|(o[31]<<8))==7&&o[11]==0,"RM_4000: Objekt 8 ist nicht Typ 5 / aspd 7 / draw_tp 0");
         World w(rm4000,data);w.world.tick(); // bhControlObjItm baut ob_spc (sp_flg 0x4)
         check(w.world.state(2,8)->object_list==3,"RM_4000: Objekt 8 steht nicht in ob_spc");
         const SceneCamera cam=decode_scene(rm4000.bytes).cameras.at(0);NJS_MATRIX view;camera_view_matrix(cam,view);
         const auto ob_spc=[&](){Scene out;w.world.append(out,{},cam);std::vector<SceneTriangle> t;for(const auto& x:out.triangles)if(!x.camera_mask&&x.material_phase==4)t.push_back(x);return t;};
         // Ohne Augenkamera (Gegenkontrolle des bestehenden Pfads): halber Satz ohne Raumlichter + lgtp[7] voll, amb_chr.
         {const auto tris=ob_spc();check(!tris.empty(),"RM_4000: Objekt 8 zeichnet nicht");}
         w.events.flags[11][0]|=0xC0u; // gm_flg 0x40|0x80 (bhInitPlEyeCamera, cut.c:2645-2646); Testaufstellung
         const auto tris=ob_spc();check(!tris.empty(),"Augenkamera ob_spc: Objekt 8 zeichnet nicht");
         for(const auto& t:tris)for(const auto& vx:t.vertices){NJS_VECTOR n=unit_or_face(t,vx),c;njCalcVector(&view,&n,&c);
            const auto expect=easy_light(c,{0,0.3f,0.7f},{1,1,1},0.6f,0.2f);
            if(!same(vx.light,expect))throw std::runtime_error("Augenkamera ob_spc: "+str(vx.light)+" erwartet bhSetEasyDirLight(0.6) "+str(expect));}}

        std::cout<<"item_draw_light ok\n";return 0;
    }catch(const std::exception& e){std::cerr<<"item_draw_light_test: "<<e.what()<<"\n";return 1;}
}
