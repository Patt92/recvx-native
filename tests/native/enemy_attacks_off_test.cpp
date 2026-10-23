// Host-Komfortschalter "Gegnerangriffe" (RoomWorld::set_enemy_attacks), keine
// Originalmechanik: aus = kein HP-Verlust, kein Gift, keine Griff-/Wurf-
// uebernahme, keine Toetung des Spielers durch Gegner oder Gegnereffekte; die
// Gegner-KI laeuft weiter. Geprueft am zentralen Spielerpass
// RoomWorld::enemy_player_set (so schreiben die Gegnerroutinen und die Effekte
// bhEff_E03_Acid/E15_Poison ueber h.player_set), am Hunter-Jagdlauf in RM_5590
// (en05.c, natuerlicher Treffer binnen 120 Passes, Gegenprobe mit Schalter an)
// und am Mottengriff in RM_6030 (en06.c BR00 -> NG00, mode0 4).
// Zustandsschreibungen sind Diagnosevorbedingungen, kein gespielter Fortschritt.
#include "world.h"
#include "room.h"
#include "en06.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data,const char* room,int character,bool attacks)
        :bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),
         player(character?PlayerAsset::load(data,character):PlayerAsset::load(data)),
         world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_enemy_ai(true);world.set_enemy_grab(true);world.set_enemy_attacks(attacks);
        commit();events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
        events.flags[6][0]=0xffffffffu; // room.c:997-1009 bhStartDoorDemo Fall 1 (runtime.cpp room_handover)
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){events.tick();world.set_simulation_flags(events.flags[6][0]);world.set_player_hp(inv.hp);world.tick();commit();}
    RECVX_ENEMY_WORK& work(unsigned index){auto& t=world.enemy_table();check(index<t.ewk_n&&t.slot[index],"ene[index] fehlt");return *t.slot[index];}
    unsigned mode0(){RECVX_ENEMY_PLAYER p{};check(world.enemy_player_get(p),"Spielerbesitzer fehlt");return p.mode0;}
    unsigned stflg(){RECVX_ENEMY_PLAYER p{};check(world.enemy_player_get(p),"Spielerbesitzer fehlt");return p.stflg;}
    void no_gaps(const char* phase,const char* module){
        static const char* allowed[]={"bhEff_E06_Rinpun: op->cspr.c"}; // effsub2.c:1932 (benannte Luecke, en06_world_test)
        for(const auto& g:world.unbound_enemy_routines()){bool ok=false;for(const char* a:allowed)if(g.find(a)!=std::string::npos)ok=true;
            if(!ok&&g.find(module)!=std::string::npos)throw std::runtime_error(std::string(phase)+": Luecke "+g);}
    }
};
// Direkter zentraler Pfad: so schreiben Gegnerroutinen (en19.c:3827 ply_set_damage,
// en15.c PoisonAttack) und Effekte (effsub2.c:1452 bhEff_E03_Acid) den Spieler.
static void direct_pass(const std::filesystem::path& data){
    for(int attacks=1;attacks>=0;--attacks){
        Fixture f(data,"RM_6030",0,attacks!=0);f.tick();
        const int hp=f.inv.hp;check(hp>0,"Inventar-HP nicht gebunden");
        RECVX_ENEMY_PLAYER p{};check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");
        check(p.debug_attack_disabled==(attacks==0),"debug_attack_disabled folgt dem Schalter nicht");
        // Treffer ohne Modusuebernahme: hp -= 20, Gift 0x80000 (bhEff_E03_Acid), Gift2 0x200000 (en15).
        RECVX_ENEMY_PLAYER q=p;q.flg|=4u;q.mode0=2;q.mode2=1;q.mode3=0;q.hp-=20;q.stflg|=0x80000u|0x200000u;
        check(f.world.enemy_player_set(q),"Spieler setzen");
        const bool poison=f.world.take_player_poison(),infect=f.world.take_player_infect();
        if(attacks){check(f.inv.hp==hp-20,"an: HP-Abzug kam nicht beim Inventar an");check(poison&&infect,"an: Giftbits kamen nicht an");
            check((f.stflg()&0x280000u)==0x280000u,"an: stflg-Giftbits nicht uebernommen");check(f.mode0()==2,"an: mode0 2 nicht uebernommen");}
        else{check(f.inv.hp==hp,"aus: HP wurde gesenkt");check(!poison&&!infect,"aus: Gift wurde weitergereicht");
            check((f.stflg()&0x280000u)==0,"aus: stflg-Giftbits uebernommen");check(f.mode0()==2,"aus: Trefferreaktion (mode0 2) darf weiterlaufen");}
        // Wurf/Griff: en19.c:3827 mode0 5, hp -= dmg; en01/en06 mode0 4.
        check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");q=p;q.flg|=0x10004u;q.stflg|=0x10000u;q.mode0=5;q.mode1=0;q.mode2=0;q.mode3=0;q.hp-=30;q.pos.x+=7.0f;
        check(f.world.enemy_player_set(q),"Spieler setzen");
        if(attacks){check(f.mode0()==5&&f.world.player_scripted&&f.inv.hp==hp-50,"an: Wurfuebernahme/HP fehlt");}
        else{check(f.mode0()==2&&!f.world.player_scripted&&f.inv.hp==hp,"aus: Wurfuebernahme oder HP-Abzug kam durch");
            RECVX_ENEMY_PLAYER r{};f.world.enemy_player_get(r);check(r.pos.x==p.pos.x,"aus: Griffpass hat die Spielerlage verschoben");}
        // Toetung: hp < 0, mode0 6 mit flg 2 (bhCPM0_enedie).
        check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");q=p;q.mode0=6;q.flg|=2u;q.hp=-1;
        check(f.world.enemy_player_set(q),"Spieler setzen");
        if(attacks)check(f.world.player_killed()&&f.world.player_dead()&&f.inv.hp==-1,"an: Toetung kam nicht durch");
        else check(!f.world.player_killed()&&!f.world.player_dead()&&f.inv.hp==hp,"aus: Toetung kam durch");
        // Heilung (hp hoeher) bleibt auch bei ausgeschaltetem Schalter erlaubt (Inventarbesitzer).
        check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");q=p;q.hp=p.hp;check(f.world.enemy_player_set(q),"Spieler setzen");
        // Dienst s.player_damage (enemy_work.h:538) ist nur innerhalb des Gegnerpasses
        // gebunden (enemy_world-Scope) und hat derzeit keinen Aufrufer; nicht hier pruefbar.
    }
}
// Natuerlicher Jagdlauf RM_5590 (zwei Hunter): mit Schalter an verliert der Spieler
// binnen 120 Passes HP (en05_world_test), mit Schalter aus nicht; die Hunter laufen weiter.
static void hunter_hunt(const std::filesystem::path& data){
    int hp_on=-1;
    for(int attacks=1;attacks>=0;--attacks){
        Fixture f(data,"RM_5590",10,attacks!=0);f.tick();
        auto& h=f.work(0);check(h.id==5,"RM_5590 Hunter fehlt");
        const float ax=h.px,az=h.pz;const int hp=f.inv.hp;bool attacked=false,poison=false,driven=false;
        for(int i=0;i<120;++i){f.tick();
            for(unsigned k=0;k<2;++k){const auto m=f.work(k).mode0;if(m==2||m==3)attacked=true;}
            if(f.world.take_player_poison()||f.world.take_player_infect())poison=true;
            const unsigned m0=f.mode0();if(m0==4||m0==5||m0==6)driven=true;}
        check(std::fabs(h.px-ax)+std::fabs(h.pz-az)>1.0f,"Hunter 0 hat sich nicht bewegt (KI muss weiterlaufen)");
        f.no_gaps("Hunter",attacks?"en05":"en05");
        if(attacks){check(f.inv.hp<hp,"Gegenprobe an: kein Hunter-Treffer kam beim HP an");hp_on=hp-f.inv.hp;}
        else{check(f.inv.hp==hp,"aus: Hunter hat HP gesenkt");check(!poison,"aus: Gift wurde weitergereicht");check(!driven,"aus: Spieler wurde uebernommen");}
        (void)attacked;
    }
    std::cout<<"RM_5590 Hunter: HP-Verlust mit Schalter an "<<hp_on<<", mit Schalter aus 0\n";
}
// Mottengriff RM_6030 (en06_world_test): mit Schalter aus greift die Motte den
// Spieler nicht (mode0 4 bleibt aus, kein HP-Verlust, kein Gift), die Motte fliegt weiter.
static void moth_grab(const std::filesystem::path& data){
    bool grabbed_on=false;
    for(int attacks=1;attacks>=0;--attacks){
        Fixture f(data,"RM_6030",0,attacks!=0);auto& m=f.work(3);f.events.set_flag(4,0,0);
        bool grabbed=false,placed=false;const int hp=f.inv.hp;bool poison=false;float moved=0;
        for(int i=0;i<1200&&!grabbed;++i){
            if(!placed&&m.mode0==1&&m.mode2==2&&(m.flg&0x400000u)){placed=true;
                const float px=m.px-5.0f*njSin(m.ay),pz=m.pz-5.0f*njCos(m.ay);
                f.actor.position={px,m.py-15.0f,pz};f.actor.yaw=Angle(m.ay);
                RECVX_ENEMY_PLAYER q{};check(f.world.enemy_player_get(q),"Spielerbesitzer fehlt");q.pos={px,m.py-15.0f,pz};q.ay=m.ay;q.flg&=~4u;
                check(f.world.enemy_player_set(q),"Spieler setzen");f.commit();}
            const float bx=m.px,bz=m.pz;f.tick();moved+=std::fabs(m.px-bx)+std::fabs(m.pz-bz);
            if(f.world.take_player_poison()||f.world.take_player_infect())poison=true;
            if(f.mode0()==4)grabbed=true;}
        check(placed,"Motte 3 erreichte den Flug nicht");
        f.no_gaps("Motte","en06");
        if(attacks){grabbed_on=grabbed;check(grabbed,"Gegenprobe an: Mottengriff (mode0 4) kam nicht zustande");}
        else{check(!grabbed,"aus: Mottengriff hat den Spieler uebernommen");check(f.inv.hp==hp,"aus: HP gesenkt");check(!poison,"aus: Gift weitergereicht");
            check(moved>1.0f,"aus: Motte bewegt sich nicht mehr (KI muss weiterlaufen)");}
    }
    (void)grabbed_on;
}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    direct_pass(data);hunter_hunt(data);moth_grab(data);
    std::cout<<"enemy_attacks off: zentraler Spielerpass, Hunter RM_5590 und Mottengriff RM_6030 ohne Wirkung; Gegenprobe an bestanden.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
