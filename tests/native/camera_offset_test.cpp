// cam.ofx/ofy/ofz (camera.c:36/49/81 bhControlCamera/bhGetCameraPosition) in echten Raeumen:
// alle Originalschreiber am gemeinsamen Besitzer der Runtime (RoomWorld::set_camera_offset_owner).
// Schreiber laut grep ueber src/ps2/veronica/prog/*.c: en02.c:3083-3097 (bhEne02_CameraControl),
// en13.c:1111-1125 (en13_world_test), en17.c:1676-1683 (bhEne17_CameraControl), event.c:4717-4725
// (bhCamYureSet, Opcode 0x5a), effsub3.c:3177/3220 (bhEff30a, effect_rap_camera_test/en19_world_test);
// Ruecksetzen: camera.c:8 bhInitCamera (room.c:33 bei jedem Raumladen, system.c:1396 Spielstart).
// Zustandsschreibungen unten sind Diagnosevorbedingungen und als solche markiert, kein
// natuerlich gespielter Fortschritt. Semantikreferenz, nie kompiliert.
#include "world.h"
#include "room.h"
#include "en02.h"
#include "en17.h"
#include "effect_rap_camera.h"
#include "core_random.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data,const char* room):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        world.set_camera_offset_owner(&camera);
        commit();
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    RECVX_ENEMY_WORK& enemy(unsigned id){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==id)return *t.slot[i];throw std::runtime_error("Originalgegner "+std::to_string(id)+" fehlt");}
    void no_camera_gap(const char* phase){for(const auto& g:world.unbound_enemy_routines())check(g.find("cam.of")==std::string::npos,std::string(phase)+": "+g);}
};
// -rand() / -2.1474836e9f (en17.c:1676, event.c:4717) und -2147483648.0f (en02.c:3083) sind derselbe Float.
static float ee_unit(int r){return -r/-2.1474836E9f;}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    check(-2.1474836E9f==-2147483648.0f,"Divisor der drei Schreiber ist ein Float");
    {
        // event.c:4701 bhCamYureSet (Opcode 0x5a, vier Byte): v0 = Byte 1, v1 = u16 ab Byte 2.
        // v0 == 0: ofx, ofy, ofz in dieser Reihenfolge je ein rand(); sonst alle drei 0 (:4723-4725).
        Fixture f(data,"RM_9280");
        recvx_core_random_seed(0x5a5a);const int r1=recvx_core_random_next(),r2=recvx_core_random_next(),r3=recvx_core_random_next();
        recvx_core_random_seed(0x5a5a);
        f.camera={7,7,7};
        EventCommand c;c.opcode=0x5a;c.bytes={0x5a,0x00,0x90,0x01}; // v1 = 0x0190 = 400 -> 0.01f*400
        check(f.world.bind(c),"0x5a nicht gebunden");
        const float a=0.01f*400;
        check(f.camera.x==a*ee_unit(r1)&&f.camera.y==a*ee_unit(r2)&&f.camera.z==a*ee_unit(r3),"bhCamYureSet: ofx/ofy/ofz = (0.01f*v1)*rand-Anteil in Reihenfolge x,y,z");
        check(recvx_core_random_next()==[]{recvx_core_random_seed(0x5a5a);recvx_core_random_next();recvx_core_random_next();recvx_core_random_next();return recvx_core_random_next();}(),"bhCamYureSet: genau drei rand()");
        c.bytes={0x5a,0x01,0x90,0x01};check(f.world.bind(c),"0x5a v0 1");
        check(f.camera.x==0&&f.camera.y==0&&f.camera.z==0,"bhCamYureSet v0 != 0: alle drei 0");
        f.no_camera_gap("bhCamYureSet");
    }
    {
        // en17.c:1674-1685 bhEne17_CameraControl schreibt unter exp0[2] & 0x800 nur cam.ofy;
        // ofx/ofz bleiben stehen (kein Schreiber dort). Erst wenn EXP0_F(24) <= 0.01 ist, nullt er
        // alle drei und loescht 0x800. Aufruf nur bei mode0 < 5 (en17.c:373), und nur fuer nicht
        // gehaltene Gegner (eneset.c:362 stflg 0x41000000).
        Fixture f(data,"RM_9280");f.tick();auto& e=f.enemy(17);auto* x0=static_cast<RECVX_EN17_EXP0*>(e.exp0);
        check(x0&&e.mode0<5,"EN17 Init");
        // DIAGNOSEVORBEDINGUNG: Flag 0x800 und EXP0_F(24) gesetzt, ofx/ofy/ofz = 1/2/3 als Stand
        // eines anderen Schreibers.
        f.camera={1,2,3};x0->flags|=0x800u;x0->camera_shake.amplitude=1.0f;f.tick();
        check(f.camera.x==1&&f.camera.y==2&&f.camera.z==3&&x0->camera_shake.amplitude==1.0f,"eneset.c:362: gehaltener Steve (stflg 0x1000000) schreibt nicht");
        // DIAGNOSEVORBEDINGUNG: stflg 0x1000000 geloest wie in en17_world_test (RM_9280 haelt Steve
        // bis zum Skript); EXP0_F(24) = 0.005 <= 0.01 -> Nullzweig im ersten freien Pass (Motion 0,
        // keine Ausloesung en17.c:587-612).
        e.stflg&=~0x01000000u;check(e.mtn_no!=15,"erster freier Pass ohne Motion 15");
        x0->camera_shake.amplitude=0.005f;f.tick();
        check(f.camera.x==0&&f.camera.y==0&&f.camera.z==0,"en17.c:1681-1683: alle drei 0");
        check(!(x0->flags&0x800u)&&x0->camera_shake.amplitude==0.005f,"en17.c:1684: 0x800 geloescht, EXP0_F(24) unveraendert");
        f.camera={4,5,6};f.tick();
        if(!(x0->flags&0x800u))check(f.camera.x==4&&f.camera.y==5&&f.camera.z==6,"ohne 0x800 kein Schreiben");
        // Natuerlicher Lauf: Motion 15 setzt bei frm 0/48/100/144 0x800 und EXP0_F(24) = 1.0
        // (en17.c:609-612); bhEne17_CameraControl laeuft danach im selben Pass (:373 nach :363-372).
        f.camera={1,2,3};int armed=0,decayed=0;float amp=x0->camera_shake.amplitude;bool flag=x0->flags&0x800u;
        for(int i=0;i<120;++i){f.tick();const float now=x0->camera_shake.amplitude;
            if(!(x0->flags&0x800u)){
                if(flag){check(amp<=0.01f,"0x800 nur nach Unterschreiten geloescht");break;}
                check(f.camera.x==1&&f.camera.y==2&&f.camera.z==3,"ohne 0x800 kein Schreiben");continue;}
            // Ausloesung: 1.0 (en17.c:611) oder Bitmuster 0x40000000 = 2.0f (:591-606), danach *0.8.
            if((now==0.8f||now==1.6f)&&!(flag&&amp*0.8f==now)){++armed;check(std::fabs(f.camera.y)<=now/0.8f/2,"en17.c:1676 nach Ausloesung: |ofy| <= EXP0_F(24)/2");}
            else{check(flag&&now==amp*0.8f,"en17.c:1677: EXP0_F(24) *= 0.8 ("+std::to_string(amp)+" -> "+std::to_string(now)+")");
                check(std::fabs(f.camera.y)<=amp/2,"en17.c:1676: |ofy| <= EXP0_F(24)/2");++decayed;}
            check(f.camera.x==1&&f.camera.z==3,"bhEne17_CameraControl: cam.ofx/ofz bleiben stehen (en17.c:1676 schreibt nur ofy)");
            amp=now;flag=true;}
        std::cout<<"  EN17 Motion 15: "<<armed<<" Ausloesungen, "<<decayed<<" Abklingpaesse\n";
        check(armed>=2&&decayed>=10,"EN17: natuerliches Wackeln in Motion 15");
        f.no_camera_gap("EN17");
    }
    {
        // Reihenfolge game.c:48/50: bhControlEnemy vor bhControlEffect. Ein bhEff30a-Typ-1-Satz
        // (effsub3.c:3177, wie Eff30aPrm2[0]) nullt nach dem EN17-Schreiber im selben Pass.
        // DIAGNOSEVORBEDINGUNG: der Satz wird direkt im Raumpool angelegt.
        Fixture f(data,"RM_9280");f.tick();auto& e=f.enemy(17);auto* x0=static_cast<RECVX_EN17_EXP0*>(e.exp0);e.stflg&=~0x01000000u;
        const int slot=f.world.reserve_effect_slot(0);check(slot>=0,"Effektplatz");
        check(start_eff30a(const_cast<EffectSprite&>(*f.world.effect_state(unsigned(slot))),EffectPma30a{1,0,0,0,0,0,0,0}),"start_eff30a");
        f.camera={1,2,3};x0->flags|=0x800u;x0->camera_shake.amplitude=1.0f;f.tick();
        check(x0->camera_shake.amplitude==0.8f,"EN17 hat im Pass geschrieben");
        check(f.camera.x==0&&f.camera.y==0&&f.camera.z==0,"bhEff30a Typ 1 nach bhControlEnemy: cam.of* = 0");
        f.no_camera_gap("Reihenfolge");
    }
    {
        // en02.c:968-993 bhEne02_MV00 Fall 0: flg |= 0x180000 (enthaelt 0x80000), EXP0_F(36) = 0.3.
        // bhEne02_CameraControl (en02.c:741 nach PlayerControl, :3081) wackelt deshalb im natuerlichen
        // Lauf unter dem Boden in jedem Pass mit 0.3 ohne Abklingen (0x80000-Zweig ohne *= 0.9).
        Fixture f(data,"RM_5830");auto& e=f.enemy(2);auto* s=static_cast<RECVX_EN02_STATE*>(e.exp0);
        int moved=0;for(int i=0;i<8;++i){f.tick();
            if(e.mode0==1&&e.mode2==0&&(e.flg&0x80000u)){
                check(s->shake==0.3f,"EXP0_F(36) = 0.3 bleibt im 0x80000-Zweig");
                check(std::fabs(f.camera.x)<=0.15f&&std::fabs(f.camera.y)<=0.15f&&std::fabs(f.camera.z)<=0.15f,"en02.c:3083-3085: |of*| <= 0.3/2");
                if(f.camera.x!=0&&f.camera.y!=0&&f.camera.z!=0)++moved;}}
        check(moved>0,"EN02 MV00: cam.ofx/ofy/ofz im natuerlichen Lauf geschrieben");
        // en02.c:3087-3093: ohne 0x80000 schreiben, solange > 0.01, dann *= 0.9; sonst alle drei 0 (:3097).
        // DIAGNOSEVORBEDINGUNG: 0x80000 geloescht (MV00 Fall 1 setzt es nicht neu).
        e.flg&=~0x80000u;float amp=s->shake;int writes=0;while(amp>0.01f){amp*=0.9f;++writes;}
        int passes=0;bool zero=false;
        for(;passes<writes+1;++passes){
            check(e.mode0==1&&e.mode2==0&&!(e.flg&0x80000u),"EN02 verlaesst MV00 waehrend der Abklingprobe");
            const float before=s->shake;f.tick();
            if(before>0.01f){check(s->shake==before*0.9f,"EXP0_F(36) *= 0.9 (en02.c:3093)");
                check(std::fabs(f.camera.x)<=before/2&&std::fabs(f.camera.y)<=before/2&&std::fabs(f.camera.z)<=before/2,"en02.c:3089-3091");}
            else{zero=f.camera.x==0&&f.camera.y==0&&f.camera.z==0;check(s->shake==before,"Nullzweig aendert EXP0_F(36) nicht");}}
        check(zero,"en02.c:3097: nach "+std::to_string(writes)+" Abklingpaessen alle drei 0");
        // en02.c:3076: mode0 5 (bhEne_Event) schreibt nichts. DIAGNOSEVORBEDINGUNG: nur mode0 auf 5.
        f.camera={4,5,6};e.flg|=0x80000u;e.mode0=5;e.mode1=0;f.tick();
        check(f.camera.x==4&&f.camera.y==5&&f.camera.z==6,"en02.c:3076: mode0 5 laesst cam.of* stehen");
        f.no_camera_gap("EN02");
    }
    std::cout<<"camera offset writers (event 0x5a, EN17, EN02, Reihenfolge Gegner/Effekt) PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
