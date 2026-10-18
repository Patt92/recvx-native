// sys->pt_flg als Zeichentor auf dem produktiven Besitzer RoomWorld::append (presentation_flags_).
// Original: game.c:318-370 bhAllDrawModel (Raummodell rom->mdl.objP nur mit pt_flg 0x20, :328;
// bhDrawEneObject/bhDrawEnemy 0x2, :336/353; bhDrawEffect 0x10, :339/347; bhPutModel(plp) 0x1, :357),
// game.c:381 bhAllEasyDrawModel (Monitorbild, Raummodell 0x20, :391) und objitm.c:497 bhDrawObjItm
// (ob_nlg/ob_hlg/ob_spc nur mit 0x4, :503; Raumitems nur mit 0x8, :550). Alle Tore lesen pt_flg
// beim Zeichnen. RM_8001: bhEff138 setzt beim Eintritt pt_flg = 0x10 (effsub1b.c:1641-1645),
// aktiviert ueber den Skriptbefehl 0x91 bhEffModeSet (event.c:6519) wie im Raumskript; dann bleibt
// vom Raum nur der Hintergrund (rom->bak_col 0, :1646-1652) und bhDraw138 (2D, nicht Teil dieser Szene).
#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "renderer.h"
#include "screen_fade.h"
#include "events.h"
#include "room.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
namespace {
struct Counts{std::size_t room=0,room_ambient=0,objects=0,items=0,enemies=0,pixels=0;};
struct Rig {
    std::vector<std::uint8_t> room;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
    AudioMixer audio;Inventory inventory;MessageDialog dialog;EventVm events;RoomWorld world;ScreenFade fade;unsigned slot=0;
    explicit Rig(const std::filesystem::path& data):room(read_asset_file(data/"rooms/RM_8001.RDT",64*1024*1024)),scene(decode_scene(room)),actor(scene.spawns.at(0)),
        audio(data),events(room,inventory,dialog),world(room,scene,actor,motion,frame,audio,data,{},"RM_8001"){
        // runtime.cpp: stg_no/rom_no/rom_case aus dem Raumnamen (events->values[0/1/21]), dann bhChangeViewClipRM.
        const auto id=RoomIdentity::parse("RM_8001");events.values[0]=id.stage;events.values[1]=id.room;events.values[21]=id.variant;
        world.bind_player_collision(events);world.initialize_camera_clip(0);
        // runtime.cpp fade_set/fade_control: bhSetScreenFade setzt cb_flg 0x2, bhControlScreenFade loescht es.
        world.set_screen_fade_hooks([this](std::uint32_t argb,float ct){fade.set(argb,ct);events.flags[10][0]|=0x2u;},[this]{return fade.previous();});
        world.set_stand_player_hook([]{});
        bool found=false;for(unsigned i=0;const auto* e=world.effect_state(i);++i)if(e->id==138){slot=i;found=true;break;}
        check(found,"RM_8001 hat keinen Raumeffekt 138");
    }
    const EffectSprite& op()const{return *world.effect_state(slot);}
    void step(unsigned ps=0){
        if((events.flags[10][0]&0x2u)&&fade.control())events.flags[10][0]&=~0x2u;
        world.set_simulation_flags(events.flags[6][0]);
        const unsigned p=(events.flags[6][0]&0x20u)?ps:0u;world.set_pad(p,p);world.tick();}
    // runtime.cpp compose: scene.triangles = stage_geometry (das dekodierte rom->mdl.objP), dann
    // world->append(scene, nodes, world->camera(...)) mit njClipZ/Volumen des installierten Schnitts.
    Counts draw(const std::string& dump={},const WorldDrawPass& pass={}){
        Scene s=scene;auto view=world.camera(scene,scene.cameras.at(0));
        const auto clip=world.camera_clip();view.clip_near=clip.near;view.clip_far=clip.far;
        const auto volume=world.clip_volume();view.clip_volume_x=volume.x;view.clip_volume_y=volume.y;
        world.append(s,{},view,pass);Counts c;
        // world.cpp append: material_phase en_obj 1, Objekte 1 + object_list (2..4), Items 5, Gegner 7.
        for(const auto& t:s.triangles){if(t.camera_mask){++c.room;continue;}
            if(t.material_phase>=2&&t.material_phase<=4)++c.objects;else if(t.material_phase==5)++c.items;else if(t.material_phase==7)++c.enemies;}
        for(const auto& w:s.ambient_writes)if(w.camera_mask)++c.room_ambient;
        SoftwareRenderer image(640,480);image.draw(s,view);
        // Nur RGB vergleichen: der installierte Clear (njSetBackColor) traegt sein Originalalpha, der SSAA-Resolve
        // schreibt Alpha 0xFF (bekannte Zielalpha-Luecke); die Raumfrage ist eine Farbfrage.
        for(const auto p:image.pixels())if((p&0xffffffu)!=(s.background&0xffffffu))++c.pixels;
        // runtime.cpp: nach image.draw folgt die ef_fnc-Schleife (bhDraw138, effect.c:929) nur fuer das Belegbild.
        if(!dump.empty()){world.draw_fnc_huds(image);image.save_bmp(dump);}
        return c;}
};
}
int main(int argc,char** argv){try{
    check(argc>=2,"original data directory required; this test must not silently skip its resource checks");
    _Make_SinTable();const std::filesystem::path data=argv[1];
    const std::filesystem::path out=argc>=3?std::filesystem::path(argv[2]):std::filesystem::path{};
    const auto dump=[&](const char* name){return out.empty()?std::string{}:(out/name).string();};
    Rig r(data);
    std::size_t stage_ambient=0;for(const auto& w:r.scene.ambient_writes)if(w.camera_mask)++stage_ambient;
    // system.c:193 pt_flg = ~0: alles zeichnet, das Raummodell vollstaendig.
    check(r.world.presentation_flags()==~0u,"pt_flg beginnt nicht mit ~0 (system.c:193)");
    for(int i=0;i<3;++i)r.step();
    const auto all=r.draw(dump("rm8001-pt-ffffffff.bmp"));
    std::cout<<"pt ~0: room "<<all.room<<"/"<<r.scene.triangles.size()<<" objects "<<all.objects<<" items "<<all.items<<" enemies "<<all.enemies<<" pixels "<<all.pixels<<'\n';
    check(all.room==r.scene.triangles.size()&&all.room>0&&all.room_ambient==stage_ambient,"pt_flg 0x20: Raummodell nicht vollstaendig");
    check(all.objects>0&&all.items>0&&all.pixels>0,"Ausgangsbild ohne Objekte/Items/Raumpixel");
    // Diagnoseaufstellung am Besitzer (je ein Bit geloescht): jedes Tor trifft genau seine Gruppe.
    const auto only=[&](unsigned clear,const char* what){r.world.set_presentation_flags(~0u&~clear);const auto c=r.draw();r.world.set_presentation_flags(~0u);
        std::cout<<"pt ~0x"<<std::hex<<clear<<std::dec<<": room "<<c.room<<" objects "<<c.objects<<" items "<<c.items<<" enemies "<<c.enemies<<'\n';
        check(c.room==(clear==0x20u?0u:all.room)&&c.room_ambient==(clear==0x20u?0u:all.room_ambient),std::string(what)+": Raummodell");
        check(c.objects==(clear==0x4u?0u:all.objects),std::string(what)+": Objektlisten");
        check(c.items==(clear==0x8u?0u:all.items),std::string(what)+": Raumitems");
        check(c.enemies==(clear==0x2u?0u:all.enemies),std::string(what)+": Gegner");};
    only(0x20u,"game.c:328 Raummodell");only(0x4u,"objitm.c:503 Objektlisten");only(0x8u,"objitm.c:550 Raumitems");only(0x2u,"game.c:353 Gegner");
    // game.c:391 bhAllEasyDrawModel (Monitorbild) prueft dasselbe Raumtor.
    {r.world.set_presentation_flags(~0u&~0x20u);const auto c=r.draw({},{true,0});r.world.set_presentation_flags(~0u);
     check(c.room==0&&c.room_ambient==0,"game.c:391 bhAllEasyDrawModel zeichnet das Raummodell ohne pt_flg 0x20");
     const auto d=r.draw({},{true,0});check(d.room==all.room,"Monitorpass ohne Raummodell trotz pt_flg 0x20");}
    // Original-Aktivierung: Skriptbefehl 0x91 bhEffModeSet -> bhEff138 mode0 0 setzt pt_flg = 0x10.
    check(r.world.bind({0x91,0,0,0,{0x91,std::uint8_t(r.slot),1,0}}),"0x91 bhEffModeSet erreicht bhEff138 nicht");
    r.step();
    check(r.op().mode0==1&&r.world.presentation_flags()==0x10u,"bhEff138 setzt pt_flg nicht auf 0x10");
    const auto puzzle=r.draw(dump("rm8001-oil-puzzle-pt-10.bmp"));
    std::cout<<"oil puzzle pt 0x10: room "<<puzzle.room<<" objects "<<puzzle.objects<<" items "<<puzzle.items<<" enemies "<<puzzle.enemies<<" pixels "<<puzzle.pixels<<'\n';
    check(puzzle.room==0&&puzzle.room_ambient==0,"Oelraetsel: Raummodell zeichnet trotz pt_flg 0x10 (game.c:328)");
    check(puzzle.objects==0&&puzzle.items==0&&puzzle.enemies==0,"Oelraetsel: Objekte/Items/Gegner zeichnen trotz pt_flg 0x10");
    check(puzzle.pixels==0,"Oelraetsel: Raumszene liefert noch Pixel ueber rom->bak_col");
    // Abbruch (0x1000, effsub1b.c:1772-1779) -> mode0 10/11 -> pt_flg = op->ct2 (:2107): der Raum kehrt zurueck.
    r.step(0x1000);
    for(unsigned i=0;i<200&&r.op().mode0!=0;++i)r.step();
    check(r.op().mode0==0&&r.world.presentation_flags()==~0u,"pt_flg nach dem Abbruch nicht wiederhergestellt");
    const auto back=r.draw(dump("rm8001-after-puzzle.bmp"));
    check(back.room==all.room&&back.room_ambient==all.room_ambient&&back.objects==all.objects&&back.items==all.items&&back.pixels>0,"Raum/Objekte/Items nach dem Raetsel nicht zurueck");
    std::cout<<"pt_flg draw gates passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
