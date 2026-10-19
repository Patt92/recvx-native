// cut.c:1803 bhSetEventHideObjLgt schreibt die Objekt- und Lichtmasken eines Keys EINMAL in
// rom->mdl.objP[].evalflags und rom->evlp[].flg; sie bleiben dort bis zum naechsten Aufruf.
// Gerufen wird sie bei bhSetEventCamera (:1692), im Initpass (:2020) und nach jedem Keywechsel
// (:2136) — NICHT bei dem Keywechsel, der in das Endhalten fuehrt: cut.c:2124 bricht mit `break`
// vor dem Aufruf ab. In der Haltephase steht die Kamera also auf dem letzten Key, die Masken
// aber noch auf dem vorletzten.
//
// Geprueft an RM_0000 Spur 33 (vier Keys, nxt_no 0): dort unterscheiden sich die Objektmasken
// des letzten und des vorletzten Keys deutlich. In den ausgelieferten Raumdaten betrifft das
// 383 von 1362 endenden Spuren in 82 Raeumen.
#include "world.h"
#include "room.h"
#include "asset_io.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
// Diagnostische Aufstellung: RM_0000 liefert die Tabellen; Lichtfelder, Masken und Keyzeiten
// werden gezielt gesetzt, nicht als natuerlich erreichter Raumzustand ausgegeben. Kein zweiter
// Lichtbesitzer: Steuerung, Kameramaske und Zeichnung laufen alle durch RoomWorld.
static void light_masks(const std::filesystem::path& data){
    const auto original=read_asset_file(data/"rooms/RM_0000.RDT",64*1024*1024);
    for(unsigned type:{100u,101u,13u,0u}){
        auto bytes=original;
        const auto u32=[&](std::size_t p,std::uint32_t x){for(unsigned j=0;j<4;++j)bytes.at(p+j)=std::uint8_t(x>>(8*j));};
        const auto f32=[&](std::size_t p,float x){std::uint32_t bits;std::memcpy(&bits,&x,4);u32(p,bits);};
        const BinaryView v(bytes);const auto h=inspect_room(bytes).room_offset;
        const auto lights=v.u32(h+4),ev=v.u32(h+0x3c),cam=v.u32(h+0x34)+33*0x808;
        check(v.u32(h+0x84)>=4&&v.u32(h+0xb4)>0,"Lichtpruefung: Tabellen fehlen");
        for(unsigned i=0;i<12;++i)f32(h+0x3bc+i*4,0); // Umgebung aus: nur der Lichtbeitrag wird gemessen.
        for(unsigned i=0;i<v.u32(h+0x84);++i)u32(lights+i*0xe0,0);
        for(unsigned i=0;i<v.u32(h+0xb4);++i)u32(ev+i*0xe0,0);
        // evlp[0]: Typ 100 zwei Paesse, 101/13 aspd 64 ueberschreiten 16383 im ersten Pass.
        // Typ 0 prueft Linktod ueber plp->flg; maskiert wird nur Bit 2, niemals Aktivbit 1.
        u32(ev,1);u32(ev+4,type);u32(ev+8,64);u32(ev+12,type==0?1:0);u32(ev+20,0);u32(ev+24,4);
        f32(ev+28,0);f32(ev+32,3);f32(ev+36,0);
        f32(ev+40,0);f32(ev+44,3);f32(ev+48,0);
        f32(ev+76,.3f);f32(ev+80,.1f);f32(ev+84,.05f);f32(ev+88,10);f32(ev+92,20);
        u32(ev+116,0);u32(ev+120,0);
        for(unsigned k=0;k<4;++k){const auto p=cam+8+k*0x80;
            bytes.at(p+2)=6;bytes.at(p+3)=0;
            for(unsigned j=0;j<4;++j)u32(p+56+j*4,0xffffffffu);
            // Key 0 sichtbar, Key 1 verdeckt, Key 2 sichtbar, Key 3 verdeckt (Endhalten).
            if(k==0||k==2)u32(p+56,0x7fffffffu);}
        const auto stage=decode_scene(bytes);auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;
        AudioMixer audio(data);RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_0000");
        Inventory inv;MessageDialog dlg;EventVm events(bytes,inv,dlg);world.bind_player_collision(events);
        events.flags[13][0]|=1u;world.set_simulation_flags(~0u);
        const auto lamp=[&]{return world.light_state(0,true).value();};
        const auto sample=[&]{Scene probe;SceneTriangle t;t.chunk_ambient=true;t.ambient_material={0,0,0};
            t.vertices={SceneVertex{{-1,0,-1}},SceneVertex{{1,0,-1}},SceneVertex{{0,0,1}}};
            for(auto& p:t.vertices)p.normal={0,1,0};probe.triangles.push_back(t);
            world.append(probe,{},stage.cameras.at(0));return probe.triangles.at(0).vertices[0].light.x;};
        unsigned camera_passes=0;
        const auto camera_pass=[&]{check(++camera_passes<100,"Lichtpruefung: Kamera erreicht den erwarteten Key nicht");world.tick_camera(stage,stage.cameras.at(0),0);};
        check(lamp().flags==1,"Lichtpruefung: Ausgangsflag");
        world.set_event_camera(33,0,true);
        check(lamp().flags==3,"Kamera setzen: Lichtmaske muss sofort in flg 2 stehen");
        camera_pass(); // Init wiederholt die Maske, danach KEIN Aufruf bis zum Keywechsel.
        check(world.event_camera_key()==0,"Lichtpruefung: Init wechselte zu frueh den Key");
        world.control_lights();
        if(type==100){check(lamp().mode==1&&lamp().flags==3&&sample()>.1f,"Typ 100: erster Pass fehlt");world.control_lights();}
        if(type==0){events.flags[13][0]&=~1u;world.control_lights();events.flags[13][0]|=1u;}
        check((lamp().flags&(type==13?1u:2u))==0,"Lichtsteuerung muss das Originalbit loeschen");
        if(type==100||type==101)check(sample()>.01f,"Erloschenes Licht muss im loeschenden Pass noch zeichnen");
        world.control_lights();
        {const auto value=sample();check(std::fabs(value)<1e-6f,"Ereignislicht leuchtet nach dem Ausbrennen/Linktod weiter: Typ "+std::to_string(type)+", flg "+std::to_string(lamp().flags)+", Licht "+std::to_string(value));}
        for(unsigned i=0;i<2;++i){camera_pass();world.control_lights();
            check(world.event_camera_key()==0&&std::fabs(sample())<1e-6f,"Licht wird ohne Maskenaufruf wieder eingeschaltet");}
        while(world.event_camera_key()==0)camera_pass();
        check(world.event_camera_key()==1&&!(lamp().flags&2u),"Key 1 muss das Licht verdecken");
        world.control_lights();check(std::fabs(sample())<1e-6f,"Verdeckter Key zeichnet Ereignislicht");
        while(world.event_camera_key()==1)camera_pass();
        check(world.event_camera_key()==2&&(lamp().flags&2u),"Key 2 muss flg 2 erneut setzen");
        if(type==100||type==101){check(lamp().flags==3,"Maske darf Aktivbit nicht loeschen");
            const auto before=lamp().clock;world.control_lights();
            if(type==100)check(sample()>.001f,"Neuer Maskenaufruf schaltet ausgebranntes Licht nicht ein");
            else check(lamp().clock==before+(64u<<8),"Typ 101 muss nach neuem Maskenaufruf weiterlaufen, ohne Uhrreset");}
        else {check(!(lamp().flags&1u),"Maske darf Aktivbit nach Typ 13/Linktod nicht wiederherstellen");
            world.control_lights();check(std::fabs(sample())<1e-6f,"Maske erweckt inaktives Licht");}
        const auto held_flags=lamp().flags;
        while(!world.event_camera_finished())camera_pass();
        check(lamp().flags==held_flags,"Endhalten darf Key-3-Maske nicht schreiben");
        // Ein erneuter Kameraaufruf schreibt dieselbe sichtbare Maske auch ohne Keywechsel.
        world.set_event_camera(33,0,false);check(lamp().flags&2u,"Fixed cut muss Lichtmaske sofort schreiben");
        check(lamp().mode==(type==100?1u:0u),"Maskenaufruf darf Lichtmodus nicht zuruecksetzen");
    }
}
int main(int argc,char** argv){try{
    if(argc<2)return 0;
    _Make_SinTable();
    const std::filesystem::path data=argv[1];
    const auto bytes=read_asset_file(data/"rooms/RM_0000.RDT",64*1024*1024);
    const auto stage=decode_scene(bytes);
    auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_0000");
    Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);
    world.bind_player_collision(events);

    // Die Masken der Spur direkt aus den Raumdaten lesen (Layout wie native/world.cpp:149).
    const BinaryView v(bytes);
    const auto room=inspect_room(bytes).room_offset;
    const auto cameras=v.u32(room+0x34);
    const unsigned track=33;
    const auto base=cameras+track*0x808;
    check(v.u16(base+4)==0,"RM_0000 Spur 33: erwartet eine endende Spur (nxt_no 0)");
    const unsigned keys=v.u16(base+6);
    check(keys==4,"RM_0000 Spur 33: erwartet vier Keys");
    // SceneCamera::hidden_objects fuehrt sechzehn Woerter; das RDT liefert acht (world.cpp:149).
    const auto hidden=[&](unsigned k){decltype(SceneCamera{}.hidden_objects) out{};
        for(unsigned j=0;j<8;++j)out[j]=v.u32(base+8+k*0x80+24+j*4);return out;};
    const auto last=hidden(keys-1),previous=hidden(keys-2);
    check(last!=previous,"RM_0000 Spur 33: die beiden letzten Keys muessten verschiedene Objektmasken haben");

    // bhSetEventCamera (Skriptbefehl 0x13, Modus 0): Spur 33 ab Key 0.
    check(world.bind({0x13,0,0,0,std::vector<std::uint8_t>{0x13,0x00,std::uint8_t(track),0x00}}),
          "0x13 bhSetEventCamera nicht gebunden");
    check(world.event_camera,"Ereigniskamera laeuft nicht");
    const auto pose=[&]{return world.camera(stage,stage.cameras.at(0));};
    check(pose().hidden_objects==hidden(0),"Nach bhSetEventCamera gelten die Masken von Key 0");

    // Durchlaufen bis in die Haltephase.
    unsigned passes=0;
    while(!world.event_camera_finished()&&passes<2000){world.tick_camera(stage,stage.cameras.at(0),0);++passes;}
    check(world.event_camera_finished(),"Die Spur erreicht das Endhalten nicht");
    check(world.event_camera_key()==keys-1,"Im Endhalten steht die Kamera auf dem letzten Key");

    // Kern der Pruefung: die Masken bleiben auf dem vorletzten Key.
    const auto held=pose().hidden_objects;
    check(held==previous,"Endhalten: die Masken muessen die des vorletzten Keys bleiben");
    check(held!=last,"Endhalten: die Masken duerfen nicht auf den letzten Key springen");

    // Sie bleiben auch ueber weitere Bilder stehen.
    for(unsigned i=0;i<30;++i)world.tick_camera(stage,stage.cameras.at(0),0);
    check(pose().hidden_objects==previous,"Endhalten: die Masken wandern spaeter doch noch");

    {
        // bhSetEventCamera ruft die Maskenfunktion mit SEINEM Key (cut.c:1692) — ein Einstieg
        // mitten in der Spur gilt also sofort, nicht erst nach dem naechsten Keywechsel.
        RoomWorld fresh(bytes,stage,actor,motion,frame,audio,data,{},"RM_0000");
        Inventory inv2;MessageDialog dlg2;EventVm ev2(bytes,inv2,dlg2);
        fresh.bind_player_collision(ev2);
        check(fresh.bind({0x13,0,0,0,std::vector<std::uint8_t>{0x13,0x00,std::uint8_t(track),0x02}}),
              "0x13 mit Key 2 nicht gebunden");
        // Erst der Kamerapass legt die Maske aus dem gemerkten Key in die Pose; vorher traegt
        // sie noch die Direktzuweisung aus bhSetEventCamera selbst.
        fresh.tick_camera(stage,stage.cameras.at(0),0);
        check(fresh.event_camera_key()==2,"Ein Pass darf den Key noch nicht wechseln");
        const auto entered=fresh.camera(stage,stage.cameras.at(0)).hidden_objects;
        check(entered==hidden(2),"Einstieg auf Key 2: die Masken dieses Keys gelten sofort");
        check(entered!=hidden(0),"Einstieg auf Key 2 darf nicht die Masken von Key 0 tragen");
    }
    light_masks(data);
    std::cout<<"event camera hide masks ok\n";return 0;
}catch(const std::exception& e){std::cerr<<"event camera hide masks: "<<e.what()<<'\n';return 1;}}
