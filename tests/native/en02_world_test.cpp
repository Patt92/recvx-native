// EN02 Gulp Worm in RM_5830 (Originalraum, in dem der Wurm ohne Skriptsperre laeuft;
// in RM_3091/RM_7220 haelt ihn das Raumskript in mode0 5), echte
// RoomWorld-Dienste. Zustandsschreibungen unten sind Diagnosevorbedingungen,
// kein natuerlich gespielter Fortschritt. Semantikreferenz en02.c / en02sub.c.
#include "world.h"
#include "room.h"
#include "en02.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_5830.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_5830"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        world.set_camera_offset_owner(&camera); // cam.ofx/ofy/ofz (bhEne02_CameraControl)
        // player.c:840 bhSetPlayer: bhCalcModel(plp) vor Event_init, wie die Runtime (commit_player_joints).
        world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    RECVX_ENEMY_WORK& worm(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==2)return *t.slot[i];throw std::runtime_error("original RM_5830 gulp worm missing");}
    void tick(){world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));world.set_simulation_flags(events.flags[6][0]);world.tick();}
    // Nur die Luecken, die nicht EN02-Logik sind: die beiden unportierten Sandsprites und
    // die dokumentierte Original-UB (ang in SetSandEffectMain Fall 8).
    void no_en02_gaps(const char* phase){
        static const std::set<std::string> allowed={"bhEff253","bhEff254","en02.c:2452 ang uninitialisiert (bhEne02_SetSandEffectMain Fall 8)"};
        for(const auto& g:world.unbound_enemy_routines())if(!allowed.count(g)){std::cerr<<phase<<": unerwartete Luecke: "<<g<<'\n';throw std::runtime_error("EN02 meldet eine Luecke");}
    }
};
static const WorldEntity& ent(const RECVX_ENEMY_WORK* w){return *static_cast<const WorldEntity*>(w->owner);}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // bhEne02_Init (en02.c:747): Kinder, Morphquellen, mobiler ATR-Eintrag, Startzustand.
        Fixture f(data);f.tick();auto& e=f.worm();
        check(e.exp0&&e.mode0==1&&e.mode2==0,"bhEne02_Init: mode0 1 / mode2 0");
        check((e.flg&0x100018u)==0x100018u&&!(e.flg&0x400000u)&&(e.flg2&1u),"bhEne02_Init: flg 0x100018, flg2 1");
        check(!(e.mdflg&0x20u)&&e.lok_jno==11&&e.clp_jno[0]==11&&e.clp_jno[7]==1,"bhEne02_Init: mdflg/lok_jno/clp_jno");
        check(e.hp==300||e.hp==200,"bhEne02_Init: hp aus sys->gm_mode");
        // car wird im selben Pass wieder 0, weil Init flg 0x100000 setzt (en02.c:729-733).
        check(e.ar==15.0f&&e.ah==10.0f&&e.cah==10.0f&&e.car==0.0f,"bhEne02_Init: ar/ah/cah, car 0 unter dem Boden");
        const auto* s=static_cast<const RECVX_EN02_STATE*>(e.exp0);
        check(s->attack_ct==1&&s->warp_floor==-1&&s->car_target==10.0f&&s->wall_index>=0,"bhEne02_Init: exp0 Felder");
        check(s->mouth&&s->mouth->id==31&&s->mouth->type==0&&s->mouth->lkwkp==&e,"Maul-Kind (type 0) fehlt");
        check(s->ring&&s->ring->id==31&&s->ring->type==1&&s->ring->lkwkp==&e,"Ring-Kind (type 1) fehlt");
        for(unsigned i=0;i<RECVX_EN02_ROCKS;++i)check(s->rocks[i]&&s->rocks[i]->id==31&&s->rocks[i]->type==2&&s->rocks[i]->lkwkp==&e,"Felsen-Kind (type 2) fehlt");
        // epp->mlwP = &epw->mdl[5]/[4]/[1] (en02.c:780/788/797): Modellreihe des Elternworks.
        check(ent(s->mouth).model_bank&&ent(s->mouth).model_base==5,"Maul: mlwP ist nicht mdl[5]");
        check(ent(s->ring).model_bank&&ent(s->ring).model_base==4,"Ring: mlwP ist nicht mdl[4]");
        check(ent(s->rocks[0]).model_bank&&ent(s->rocks[0]).model_base==1,"Felsen: mlwP ist nicht mdl[1]");
        // obj_a = mdl[5].objP, obj_b = mdl[6].objP (en02.c:781-782) relativ zur Reihe des Kindes.
        check(ent(s->mouth).morph_obj_a==0&&ent(s->mouth).morph_obj_b==1,"Maul: obj_a/obj_b nicht mdl[5]/mdl[6]");
        // en02sub.c:45 Init der Kinder laeuft im selben Pass: mdflg 0x4|0x1, flg2 1, Maul car/cah.
        check((s->mouth->mdflg&0x5u)==0x5u&&(s->mouth->flg2&1u)&&s->mouth->car==10.0f&&s->mouth->cah==20.0f&&s->mouth->mode0==1,"bhEne02sub_Init (Maul)");
        f.no_en02_gaps("Init");
    }
    {
        // MV00 (en02.c:968): der Wurm folgt unter dem Boden, nach ct0 Passes uebernimmt das Brain.
        Fixture f(data);f.tick();auto& e=f.worm();
        const float x0=e.px,z0=e.pz;
        for(int i=0;i<80;++i)f.tick();
        check(e.mode0==1&&(e.mode2==0||e.mode2==1||e.mode2==2||e.mode2==3),"MV00/Brain: unerwarteter mode2");
        check(e.px!=x0||e.pz!=z0,"MV00: der Wurm bewegt sich nicht");
        check(!(f.events.flags[4][0]&1u)||e.mode2>=2,"MV00 muss sys->rm_flg 0x1 loeschen, solange er unter dem Boden ist");
        f.no_en02_gaps("MV00");
    }
    {
        // MV02 (en02.c:1056): Auftauchen setzt rm_flg 1, Motion 14, Maul oeffnet bei Bild 3 (frm_no 196608).
        Fixture f(data);f.tick();auto& e=f.worm();auto* s=static_cast<RECVX_EN02_STATE*>(e.exp0);
        e.mode1=0;e.mode2=2;e.mode3=0;f.tick();
        check(e.mode3==1&&e.mtn_no==14&&(e.flg&0x40u)&&!(e.flg&0x100000u),"MV02 Fall 0: mtn 14 / flg 0x40 / 0x100000 geloescht");
        check(f.events.flags[4][0]&1u,"MV02 muss sys->rm_flg 0x1 setzen");
        int opened=-1;for(int i=0;i<8&&opened<0;++i){f.tick();if(s->mouth->mode2==1)opened=i;}
        check(opened>=0,"MV02: Maul (Kind mode2 1) oeffnet nicht bei Bild 3");
        for(int i=0;i<3;++i)f.tick();
        check(!(s->mouth->mdflg&1u)&&(s->mouth->mdflg&2u)&&s->mouth->shp_ct>0.0f,"bhEne02sub_MV01: Maul sichtbar, Morph laeuft (shp_ct)");
        f.no_en02_gaps("MV02");
    }
    {
        // DD00 (en02.c:1908): Tod an der Oberflaeche — Motion 11, Felskragen (mobiler ATR) ab Bild 158.
        Fixture f(data);f.tick();auto& e=f.worm();
        e.hp=-1;e.mode0=4;e.mode1=0;e.mode2=0;e.mode3=0;f.tick();
        // hokan_count 20 wird noch im selben Pass von bhSetMotion (en02.c:723) auf 19 gezaehlt.
        if(!(e.mode3==1&&e.mtn_no==11&&e.hokan_count==19&&(e.flg&0x40u)&&!(e.flg&0x10u))){
            std::cerr<<"DD00 Fall 0: mode="<<int(e.mode0)<<'/'<<int(e.mode1)<<'/'<<int(e.mode2)<<'/'<<int(e.mode3)<<" mtn="<<e.mtn_no<<" hokan="<<e.hokan_count<<" flg=0x"<<std::hex<<e.flg<<std::dec<<'\n';
            for(const auto& g:f.world.unbound_enemy_routines())std::cerr<<"  gap: "<<g<<'\n';
            throw std::runtime_error("DD00 Fall 0");
        }
        for(int i=0;i<400&&e.mode3<2;++i)f.tick();
        check(e.mode3==2&&e.mtn_add==0&&!(e.flg&0x48u),"DD00: Kadaverzustand nicht erreicht");
        check(e.flg&0x100000u,"DD00: nach Bild 350 wieder unter dem Boden (flg 0x100000)");
        f.no_en02_gaps("DD00");
    }
    {
        // Skriptbefehle 0x98 bhEffectSandSet (event.c:4110) und 0xC3 bhEffectSandSetMatsumoto
        // (:8577). Die Bytes sind wie im Skriptstrom gebaut (0x98 ist 12 Byte lang: v0, die
        // Vorzeichenbits v1, die Lageart v2, drei u16-Betraege in Hundertsteln und der Typ,
        // der als ein Byte gelesen wird, waehrend der Zeiger um zwei weiterrueckt).
        Fixture f(data);f.tick();auto& e=f.worm();
        unsigned index=0;{auto& t=f.world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]==&e)index=i;}
        const auto count=[&](unsigned id){unsigned n=0;for(unsigned i=0;const auto* s2=f.world.effect_state(i);++i)if((s2->flg&1u)&&s2->id==id)++n;return n;};
        const auto each=[&](unsigned id,const std::function<void(const EffectSprite&)>& fn){
            for(unsigned i=0;const auto* s2=f.world.effect_state(i);++i)if((s2->flg&1u)&&s2->id==id)fn(*s2);};
        check(count(253)==0&&count(254)==0,"vor dem Befehl liegen keine Sandsaetze");
        // 1. Lageart 0 (Weltlage) mit x negativ und Typ 2. bhEne02_SetSandEffectMain Fall 2
        //    (en02.c:2291) legt drei Sprites mit Radius 6, dann zweimal je einen Sprite mit
        //    Radius 8 und drei Partikel: fuenf Saetze 253 und sechs 254.
        const std::vector<std::uint8_t> absolute{0x98,std::uint8_t(index),0x01,0x00,
            0xD2,0x04, 0xF4,0x01, 0x15,0x03, 0x02,0xFF};   // -12.34, 5.00, 7.89, Typ 2
        check(f.world.bind({0x98,0,0,0,absolute}),"0x98 bhEffectSandSet nicht gebunden");
        f.tick();   // der Gegnerpass desselben Bildes fuehrt den Auftrag aus
        check(count(253)==5&&count(254)==6,"0x98 Typ 2: fuenf Sprites 253 und sechs Partikel 254");
        each(253,[&](const EffectSprite& s2){
            check(std::fabs(s2.px+12.34f)<8.01f,"0x98: x folgt dem Vorzeichenbit 0x1 nicht");
            check(std::fabs(s2.pz-7.89f)<8.01f,"0x98: z ohne Vorzeichenbit muss positiv bleiben");
            check(s2.py>=5.0f-1e-3f&&s2.py<=19.01f,"0x98: y ausserhalb der Streuung von Fall 2");});
    }
    {
        // 2. Lageart 1: x und z kommen zur Gegnerlage, y bleibt der reine Skriptwert (:4207).
        Fixture f(data);f.tick();auto& e=f.worm();
        unsigned index=0;{auto& t=f.world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]==&e)index=i;}
        // Diagnosevorbedingung: der Wurm steht in RM_5830 auf y 0, damit traegt die Pruefung
        // unten nicht, dass y den Skriptwert behaelt. Eine Hoehe ungleich 0 macht sie messbar.
        e.py=4.0f;
        const float wx=e.px,wz=e.pz;
        const std::vector<std::uint8_t> relative{0x98,std::uint8_t(index),0x00,0x01,
            0x64,0x00, 0xC8,0x00, 0x2C,0x01, 0x00,0x00};   // 1.00, 2.00, 3.00, Typ 0
        check(f.world.bind({0x98,0,0,0,relative}),"0x98 relative Lage nicht gebunden");
        f.tick();
        unsigned n=0;
        for(unsigned i=0;const auto* s2=f.world.effect_state(i);++i){
            if(!(s2->flg&1u)||s2->id!=253)continue;++n;
            check(std::fabs(s2->px-(wx+1.0f))<8.01f,"0x98 relativ: x nicht um die Gegnerlage versetzt");
            check(std::fabs(s2->pz-(wz+3.0f))<8.01f,"0x98 relativ: z nicht um die Gegnerlage versetzt");
            check(s2->py>=2.0f-1e-3f&&s2->py<=10.01f,"0x98 relativ: y darf die Gegnerhoehe nicht aufnehmen");}
        check(n==6,"0x98 Typ 0: sechs Sprites 253");
    }
    {
        // 3. 0xC3 nimmt den Werkplatz von sys->ene und die Gegnerlage selbst (en02.c:2258).
        Fixture f(data);f.tick();auto& e=f.worm();
        unsigned work=0;{auto& t=f.world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]==&e)work=i;}
        const float wx=e.px,wz=e.pz;
        const std::vector<std::uint8_t> ev{0xC3,std::uint8_t(work),0x00,0x00};
        check(f.world.bind({0xC3,0,0,0,ev}),"0xC3 bhEffectSandSetMatsumoto nicht gebunden");
        f.tick();
        unsigned n=0;
        for(unsigned i=0;const auto* s2=f.world.effect_state(i);++i){
            if(!(s2->flg&1u)||s2->id!=253)continue;++n;
            check(std::fabs(s2->px-wx)<8.01f&&std::fabs(s2->pz-wz)<8.01f,"0xC3: Sprites nicht an der Gegnerlage");}
        check(n==6,"0xC3 Typ 0: sechs Sprites 253");
        // Ein Raumindex ausserhalb der Tabelle bleibt ungebunden statt zu raten.
        check(!f.world.bind({0x98,0,0,0,std::vector<std::uint8_t>{0x98,0x7F,0x00,0x00,0,0,0,0,0,0,0,0}}),"0x98: unbekannter Gegner muss ungebunden bleiben");
        check(!f.world.bind({0xC3,0,0,0,std::vector<std::uint8_t>{0xC3,0x7F,0x00,0x00}}),"0xC3: unbekannter Werkplatz muss ungebunden bleiben");
        f.no_en02_gaps("Sandbefehle");
    }
    std::cout<<"EN02 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
