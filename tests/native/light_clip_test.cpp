/* light.c:789-808 bhControlLight: Bildschirm-Clipping der Punktlichter (lsrc 4).
   Original: ohne Ereigniskamera (st_flg 0x1000000 aus cam.flg 0x2, :355) und mit ci->flg 0x10 am
   laufenden Schnitt ci = rom->cutp[cam.ncut].cam[cam.camver] (:376) wird die Lichtlage mit
   njProjectScreen(cam.mtx) auf den Bildschirm gerechnet (ps2_NaMatrix.c:1572: x*dist/z + Offset,
   NaViewScreenMatrix = diag(1.174, 1, 1)); liegt sie ausserhalb von
   [-clp .. 640+clp] x [-clp .. 480+clp] um die Bildmitte mit clp = 2 * (unsigned char)ci->frame
   (:800-806), faellt das Licht weg, OHNE einen der drei Punktlichtplaetze (lct 2..4) zu belegen (:832).
   Erwartungen kommen aus diesen Originalzeilen und einer eigenen Projektionsrechnung, nicht aus dem
   nativen Zeichenpass.

   Gegenprobe (jeweils eine Aenderung, Test muss rot werden, danach zurueck und touch):
   - world.cpp point_light_clipped: clp = 1 * frame statt 2 * frame                -> "Rand 2*frame"
   - world.cpp point_light_clipped: Fenster 320 statt 640 (x-Grenze)                -> "sichtbares Licht"
   - world.cpp: Clipping auch unter Ereigniskamera (event_camera-Tor entfernen)     -> "Ereigniskamera"
   - world.cpp: Clipping vor dem Platzzaehler zaehlen (return erst nach push)       -> "Platz"
   - world.cpp clip_key: flg 0x10 nicht pruefen                                      -> "ohne flg 0x10" */
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "binary_view.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
// Lichtbeitrag am Pruefpunkt: N.L/|L| = 1/sqrt(3) (Ecke (-1,-1,-1) zur Lampe, Normale +Y) bei Farbe 1 innerhalb nr.
static void lit(float value,const char* why){if(!(value>0.3f))throw std::runtime_error(std::string(why)+" (Licht "+std::to_string(value)+")");}
static void dark(float value,const char* why){if(!(value==0.0f))throw std::runtime_error(std::string(why)+" (Licht "+std::to_string(value)+")");}

namespace{
struct Fixture{
    std::vector<std::uint8_t> bytes;std::size_t header=0,lights=0,cut0=0;unsigned light_count=0;
    explicit Fixture(const std::filesystem::path& data){
        bytes=read_asset_file(data/"rooms/RM_0000.RDT",64*1024*1024);
        const BinaryView v(bytes);header=inspect_room(bytes).room_offset;
        lights=v.u32(header+4);light_count=v.u32(header+0x84);const auto ev=v.u32(header+0x3c);
        check(light_count>=4&&v.u32(header+0xb4)>0&&v.u32(header+0xac)>33,"Lichtclip: Tabellen fehlen");
        for(unsigned i=0;i<12;++i)f32(header+0x3bc+i*4,0);                  // Umgebung aus: nur Lichtbeitrag messen
        for(unsigned i=0;i<light_count;++i)u32(lights+i*0xe0,0);           // alle Raumlichter aus
        for(unsigned i=0;i<v.u32(header+0xb4);++i)u32(ev+i*0xe0,0);        // alle Ereignislichter aus
        cut0=v.u32(header)+0x20;                                             // rom->cutp[0].cam[0] (CAM_WRK)
        // Ereigniskamera 33: Key 0 laesst Licht 0 leuchten (Maskenbit 0x80000000 geloescht), Key 1 haelt.
        const auto cam=v.u32(header+0x34)+33*0x808;
        for(unsigned k=0;k<2;++k){const auto p=cam+8+k*0x80;bytes.at(p+2)=6;bytes.at(p+3)=0;for(unsigned j=0;j<4;++j)u32(p+56+j*4,0xffffffffu);u32(p+56,0x7fffffffu);}
    }
    void u32(std::size_t p,std::uint32_t x){for(unsigned j=0;j<4;++j)bytes.at(p+j)=std::uint8_t(x>>(8*j));}
    void f32(std::size_t p,float x){std::uint32_t bits;std::memcpy(&bits,&x,4);u32(p,bits);}
    void cut_flags(bool clip,std::uint8_t frame){const BinaryView v(bytes);std::uint16_t f=v.u16(cut0);f=clip?std::uint16_t(f|0x10u):std::uint16_t(f&~0x10u);bytes.at(cut0)=std::uint8_t(f);bytes.at(cut0+1)=std::uint8_t(f>>8);bytes.at(cut0+2)=frame;}
    // LGT_WORK: flg 3, type 0, aspd 0, kein Link, lsrc 4, Lage p, Farbe 1, nr 10, fr 20. Raumlichter ab
    // Index 4 (rom->lgtp[0..3] sind die Laufzeitplaetze, room.c/world.cpp read_lights loescht dort flg 2);
    // event = true schreibt rom->evlp[i] (unter cam.flg 0x2 der Lichtsatz, Maske der Ereigniskamera).
    void point_light(unsigned i,NJS_POINT3 p,bool event=false){const auto q=(event?BinaryView(bytes).u32(header+0x3c):lights)+i*0xe0;u32(q,3);u32(q+4,0);u32(q+8,0);u32(q+12,0);u32(q+16,0);u32(q+20,0);u32(q+24,4);
        f32(q+28,p.x);f32(q+32,p.y);f32(q+36,p.z);f32(q+40,0);f32(q+44,0);f32(q+48,0);
        f32(q+76,1);f32(q+80,1);f32(q+84,1);f32(q+88,10);f32(q+92,20);u32(q+116,0);u32(q+120,0);
        // w*-Felder (light.c:703-717, der vom Original gespeicherte Pass; ohne sp_flg 0x80 zeichnet er sie): wpx/wvx/wr/wnr/wfr.
        f32(q+136,p.x);f32(q+140,p.y);f32(q+144,p.z);f32(q+148,0);f32(q+152,0);f32(q+156,0);f32(q+172,1);f32(q+176,1);f32(q+180,1);f32(q+184,10);f32(q+188,20);}
};
// Eigene Bezugsrechnung (ps2_NaMatrix.c:1572 / ps2_NaView.c:55): Bildschirmlage relativ zur Mitte.
struct ScreenRef{float x,y;};
static ScreenRef project(const SceneCamera& cam,NJS_POINT3 world){
    NJS_MATRIX m;camera_view_matrix(cam,m);NJS_POINT3 v;njCalcPoint(&m,&world,&v);
    const float dist=320.0f/std::tan(0.0000958738f*float(cam.perspective/2));
    return {1.174000025f*v.x*dist/v.z,v.y*dist/v.z};
}
// Weltpunkt, der in Kamerasicht bei (vx, vy, vz) liegt.
// Umkehrung der Kameramatrix (scene.cpp camera_view_matrix: S(1,-1,-1) T(0,0,-d) Rz Rx Ry T(-p)),
// rueckwaerts aufgebaut; project() bestaetigt die Lage unten.
static NJS_POINT3 unproject(const SceneCamera& cam,NJS_POINT3 view){
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,cam.position.x,cam.position.y,cam.position.z);
    njRotateY(&m,-cam.angles[1]);njRotateX(&m,-cam.angles[0]);njRotateZ(&m,-cam.angles[2]);
    njTranslate(&m,0,0,cam.distance);njScale(&m,1,-1,-1);
    NJS_POINT3 w;njCalcPoint(&m,&view,&w);
    return w;
}
struct Probe{float light=0;};
static float sample(const Fixture& fx,const std::filesystem::path& data,NJS_POINT3 at,const std::function<void(RoomWorld&,const Scene&)>& prepare,const SceneCamera& view,const WorldDrawPass& pass={}){
    const auto stage=decode_scene(fx.bytes);auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;
    AudioMixer audio(data);RoomWorld world(fx.bytes,stage,actor,motion,frame,audio,data,{},"RM_0000");
    prepare(world,stage);
    Scene probe;SceneTriangle t;t.chunk_ambient=true;t.ambient_material={0,0,0};
    t.vertices={SceneVertex{{at.x-1,at.y-1,at.z-1}},SceneVertex{{at.x+1,at.y-1,at.z-1}},SceneVertex{{at.x,at.y-1,at.z+1}}};
    for(auto& p:t.vertices)p.normal={0,1,0};probe.triangles.push_back(t);
    world.append(probe,{},view,pass);return probe.triangles.at(0).vertices[0].light.x;
}
}

int main(int argc,char** argv){
    try{
        if(argc<2)throw std::runtime_error("usage: light_clip_test <data>");
        const std::filesystem::path data=argv[1];
        _Make_SinTable(); // main.c: Ninja-Sinustabelle vor jeder Matrixrechnung (auch der Bezugsrechnung hier)
        const Fixture base(data);
        const auto stage0=decode_scene(base.bytes);const SceneCamera cam=stage0.cameras.at(0);
        // Lichtlage: in Kamerasicht 450 Bildpunkte rechts der Mitte (ausserhalb 320, innerhalb 320+clp ab clp > 130),
        // 10 Einheiten vor der Kamera; die Bezugsrechnung bestaetigt die Lage unabhaengig vom Zeichenpass.
        const float z=10.0f,dist=320.0f/std::tan(0.0000958738f*float(cam.perspective/2));
        const NJS_POINT3 side=unproject(cam,{450.0f*z/(dist*1.174000025f),0.0f,z});
        const NJS_POINT3 centre=unproject(cam,{0.0f,0.0f,z});
        // Dieselbe Bildlage (450 Bildpunkte seitlich) in 40 Einheiten Tiefe: ausserhalb fr 20 des Pruefpunkts in der Mitte.
        const NJS_POINT3 far_side=unproject(cam,{450.0f*40.0f/(dist*1.174000025f),0.0f,40.0f});
        {const auto s=project(cam,side);if(!(std::fabs(std::fabs(s.x)-450.0f)<0.5f&&std::fabs(s.y)<0.5f))throw std::runtime_error("Lichtclip: Bezugsrechnung der Seitenlage: "+std::to_string(s.x)+" "+std::to_string(s.y));
         const auto f=project(cam,far_side);check(std::fabs(std::fabs(f.x)-450.0f)<0.5f&&std::fabs(f.y)<0.5f,"Lichtclip: Bezugsrechnung der fernen Seitenlage");
         const auto c=project(cam,centre);if(!(std::fabs(c.x)<0.5f&&std::fabs(c.y)<0.5f))throw std::runtime_error("Lichtclip: Bezugsrechnung der Mittellage: "+std::to_string(c.x)+" "+std::to_string(c.y));}
        const auto none=[](RoomWorld&,const Scene&){};
        const auto run=[&](bool clip,std::uint8_t frame,NJS_POINT3 at,const std::function<void(RoomWorld&,const Scene&)>& prepare,const SceneCamera& view,const WorldDrawPass& pass={}){
            Fixture fx(data);fx.cut_flags(clip,frame);fx.point_light(4,at);return sample(fx,data,at,prepare,view,pass);};
        // 1. Ohne ci->flg 0x10 leuchtet das Licht ausserhalb des Bildes (light.c:793: kein Fenstertest).
        lit(run(false,0,side,none,cam),"ohne flg 0x10 muss das Punktlicht ausserhalb des Bildes leuchten");
        // 2. Mit flg 0x10 und frame 0 (clp 0): 450 > 320 -> weg.
        dark(run(true,0,side,none,cam),"sichtbares Licht ausserhalb des Fensters muss unter flg 0x10 wegfallen");
        // 3. Rand 2*frame: frame 60 -> clp 120 -> Grenze 440 < 450 weg; frame 70 -> clp 140 -> Grenze 460 >= 450 bleibt.
        dark(run(true,60,side,none,cam),"Rand 2*frame: frame 60 (clp 120) darf 450 Bildpunkte nicht einschliessen");
        lit(run(true,70,side,none,cam),"Rand 2*frame: frame 70 (clp 140) muss 450 Bildpunkte einschliessen");
        lit(run(true,255,side,none,cam),"Rand 2*frame: frame 255 (clp 510) muss das Licht behalten");
        // 4. Ein Licht in der Bildmitte bleibt unter flg 0x10 / frame 0.
        lit(run(true,0,centre,none,cam),"Licht in der Bildmitte darf nicht wegfallen");
        // 5. Platzzaehler: drei weggefallene Punktlichter belegen keinen der drei Plaetze (:832 lct++ nur nach dem Fenstertest),
        //    das vierte Punktlicht in der Bildmitte zeichnet.
        {Fixture fx(data);fx.cut_flags(true,0);for(unsigned i=4;i<7;++i)fx.point_light(i,far_side);fx.point_light(7,centre);
         lit(sample(fx,data,centre,none,cam),"Platz: weggefallene Punktlichter duerfen keinen Lichtplatz belegen");
         Fixture all(data);all.cut_flags(false,0);for(unsigned i=4;i<7;++i)all.point_light(i,far_side);all.point_light(7,centre);
         dark(sample(all,data,centre,none,cam),"Platz: Gegenkontrolle, drei sichtbare Punktlichter fuellen lct 2..4");}
        // 6. Ereigniskamera (cam.flg 0x2 -> st_flg 0x1000000): kein Fenstertest (:791).
        const auto event_camera=[](RoomWorld& w,const Scene&){w.set_event_camera(33,0,true);};
        {Fixture fx(data);fx.cut_flags(true,0);fx.point_light(0,side,true); // evlp[0], Key 0 der Kamera 33 laesst es leuchten
         const auto evcam=decode_scene(fx.bytes).cameras.at(0); // Lichtkamera egal: unter cam.flg 0x2 kein Fenstertest
         lit(sample(fx,data,side,event_camera,evcam),"Ereigniskamera: light.c:791 verzichtet auf den Fenstertest");}
        // Monitorbild (game.c:63): screen.c:947 setzt gm_flg 0x80, der Pass zeichnet mit dem Einzellicht
        // (njCnkEasyDrawObject) - der Multi-Lichtsatz und sein Fenstertest sind dort nicht sichtbar.
        std::cout<<"light_clip ok\n";return 0;
    }catch(const std::exception& e){std::cerr<<"light_clip_test: "<<e.what()<<"\n";return 1;}
}
