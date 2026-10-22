// EN03 Schwarze Witwe im Originalraum RM_6090, echte RoomWorld-Dienste.
// Semantikreferenz src/ps2/veronica/prog/en03.c. Die Spielerlagen unten sind
// Diagnosevorbedingungen, kein natuerlich gespielter Fortschritt.
#include "world.h"
#include "room.h"
#include "en03.h"
#include "ninja_math.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data,const char* room,bool ai):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(ai);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
    }
    explicit Fixture(const std::filesystem::path& data):Fixture(data,"RM_6090",true){}
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    // Der EN03-Koerperwork; der Anfuehrerwork traegt flg 0x4000000.
    RECVX_ENEMY_WORK& spider(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==3&&!(t.slot[i]->flg&0x4000000u))return *t.slot[i];throw std::runtime_error("EN03 fehlt");}
    RECVX_ENEMY_WORK* leader(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==3&&(t.slot[i]->flg&0x4000000u))return t.slot[i];return nullptr;}
    unsigned spiders(){unsigned n=0;auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==3&&!(t.slot[i]->flg&0x4000000u)&&(t.slot[i]->flg&1u))++n;return n;}
    unsigned count(unsigned id){unsigned n=0;auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==id&&(t.slot[i]->flg&1u))++n;return n;}
    const float* joint(RECVX_ENEMY_WORK& e,unsigned node){const auto* ent=static_cast<const WorldEntity*>(e.owner);const auto& tree=ent->model_owner?*ent->model_owner:*ent;return tree.enemy_owp_mtx.at(tree.model_window+node).value;}
    void report(const char* phase){
        std::cerr<<phase<<": Luecken:\n";
        for(const auto& g:world.unbound_enemy_routines())std::cerr<<"  "<<g<<'\n';
    }
    void no_gaps(const char* phase){
        // In RM_6090 meldet EN03 selbst keine Luecke; alles andere ist ein Fund.
        if(!world.unbound_enemy_routines().empty()){report(phase);throw std::runtime_error(std::string(phase)+": unerwartete Luecke");}
    }
};
// en03.c:1318/1332/1334: Richtung und Dauer sind getrennte RNG-Zuege.
static int rng_calls=0, rng_first=0, rng_second=0;
static int32_t audit_rng(){return rng_calls++ == 0 ? rng_first : rng_second;}
static void mv02_rng(const std::filesystem::path& data){
    for(int direction=0;direction<2;++direction){
        Fixture f(data);auto& w=f.spider();
        auto services=*w.sv;const auto* saved=w.sv;services.random_next=audit_rng;
        w.sv=&services;w.mode0=1;w.mode1=0;w.mode2=2;w.mode3=0;
        rng_first=direction;rng_second=direction?0:5;rng_calls=0;
        {RoomWorld::EnemyCallbackFrame scope(&f.world);recvx_en03(&w);}
        w.sv=saved;
        check(rng_calls==2,"MV02 muss genau zwei RNG-Zuege ausfuehren");
        check(w.mtn_no==unsigned(6+direction),"MV02 Motion folgt erstem RNG-Zug");
        check(w.ayp==(direction?819:-819),"MV02 Drehrichtung folgt erstem, nicht zweitem RNG-Zug");
        check(w.ct0==rng_second-1,"MV02 Dauer folgt zweitem RNG-Zug und Postdekrement");
    }
}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    mv02_rng(data);
    {
        // en03.c:1646-1659: vorwaerts (0,0,1) kreuz Ziel (1,0,0)
        // ergibt +Y und Winkel 16384 / 12 = 1365 vor der Rotation.
        Fixture f(data);auto& w=f.spider();
        auto* s=static_cast<RECVX_EN03_STATE*>(w.exp0);
        njUnitMatrix(reinterpret_cast<NJS_MATRIX*>(s->h.m));
        s->dir={1.0f,0.0f,0.0f};s->h.target={12.0f,0.0f,0.0f};
        w.mode0=1;w.mode1=0;w.mode2=6;w.mode3=0;
        {RoomWorld::EnemyCallbackFrame scope(&f.world);recvx_en03(&w);}
        check(w.ayp==1365,"MV06 Kreuzprodukt nutzt Vorwaertsachse, Vorzeichen Hochachse");
    }
    {
        // bhEne03_Init (en03.c:625): Work, exp0/exp1, Anfuehrer, acht Beinkinder,
        // fuenfzehn EN24-Kinder, Schatten 258, clp_jno, Kapselfelder.
        Fixture f(data);auto& e=f.spider();
        check(e.mode0==1&&e.mode1==1&&e.mode2==0&&e.mode3==0,"Init: Modus verlaesst [0,0,0,0] nach [1,1,0,0]");
        auto* s=static_cast<RECVX_EN03_STATE*>(e.exp0);
        check(s!=nullptr&&e.exp1!=nullptr,"Init: exp0 und exp1 angelegt");
        check(e.ar==6.0f&&e.ah==6.0f&&e.car==6.0f&&e.cah==6.0f,"Init: ar/ah/car/cah 6");
        check(e.hp>0&&s->hp2>0,"Init: hp und hp2 aus ENE03_HITPOINT");
        check(s->h.dist==999.0f&&s->h.ang==0&&s->h.seen==0&&s->h.surface==0,"Init: dist 999, ang 0, seen 0, surface 0");
        check(s->h.target.x==0.0f&&s->h.target.y==0.0f&&s->h.target.z==0.0f,"Init: Ziel 0");
        check(s->timer>=60&&s->timer<=149&&s->climb_timer==0,"Init: ct 60..149, Kletteruhr 0");
        check(s->h.wall>=-1,"Init: Wandindex gesetzt");
        check(e.clp_jno[0]==1&&e.clp_jno[1]==5&&e.clp_jno[2]==12&&e.clp_jno[3]==30&&
              e.clp_jno[4]==9&&e.clp_jno[5]==27&&e.clp_jno[6]==32&&e.clp_jno[7]==34,"Init: clp_jno");
        check(e.cpcl!=nullptr,"Init: Kapselfeldtabelle");
        check((e.flg&0x800u)&&(e.flg&0x1078u)==0x1078u,"Init: flg 0x1078|0x800, auf 16 Bit beschnitten");
        check(e.mtn_add==65536&&e.hokan_rate==65536,"Init: mtn_add/hokan_rate 65536");
        // RM_6090 stellt drei EN03 (ein ETTY-Satz je Spinne); jedes Init legt
        // seine eigenen acht Beinkinder und fuenfzehn EN24-Kinder an.
        check(f.spiders()>=1&&f.count(31)==8u*f.spiders()&&f.count(24)==15u*f.spiders(),"Init: Kinder je EN03-Work");
        // en03.c:686-704: alle Koerper teilen den ersten Anfuehrer via exp2.
        check(f.count(3)==f.spiders()+1u,"Init: genau ein gemeinsamer Anfuehrer");
        auto& table=f.world.enemy_table();
        for(unsigned i=0;i<table.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){
            auto* body=table.slot[i];
            if(!body||body->id!=3||!(body->flg&1u)||(body->flg&0x4000000u))continue;
            auto* state=static_cast<RECVX_EN03_STATE*>(body->exp0);
            check(body->exp2==f.leader()&&state->leader==f.leader(),"Init: exp2 teilt Anfuehrer");
        }
        for(int i=0;i<8;++i){
            auto* c=s->legs[i];
            check(c!=nullptr&&c->id==31&&(c->flg&1u),"Init: Beinkind vorhanden");
            check(c->lkwkp==&e&&c->lkono==(i>=4?1:0),"Init: Beinkind verknuepft, lkono je Seite");
            check((c->mdflg&0x1u)&&(c->mdflg&0x20u),"Init: Beinkind versteckt");
            check(c->exp1==&s->leg_link[i]&&s->leg_link[i].owner==&e,"Init: Beinkind exp1 zeigt auf den owP-Knoten");
        }
        for(int i=0;i<15;++i)check(s->babies[i]!=nullptr&&s->babies[i]->id==24,"Init: EN24-Kind vorhanden");
        auto* lead=s->leader;
        check(lead!=nullptr&&lead->id==3&&(lead->flg&0x4000000u),"Init: Anfuehrerwork mit flg 0x4000000");
        check((lead->mdflg&0x1u)&&(lead->mdflg&0x20u),"Init: versteckter Anfuehrerwork");
        check(lead->exp1==e.exp1,"Init: Anfuehrer teilt exp1");
        check(((RECVX_EN03_EXP1*)e.exp1)->timer>=60&&((RECVX_EN03_EXP1*)e.exp1)->timer<=89,"Init: EXP1-Timer 60..89");
        check((e.frm_no&0xffff)==0,"Init: erster Frame 0");
        f.no_gaps("Init");
        f.tick();
        check(e.frm_no>=65536,"Move: Motion laeuft (frm_no schreitet fort)");
        f.no_gaps("Move");
    }
    {
        // Die Zustandsmaschine verlaesst [1,1,0,0]: MV00 wartet ct0 = rand%63+40
        // Passe und waehlt dann aus action_table[type][rand%4] einen der Zweige
        // {1,1,2,2}. Ohne Spieler bleibt der Work in diesen Zweigen.
        Fixture f(data);
        bool moved=false;unsigned at=0;
        for(unsigned i=0;i<400;++i){
            f.tick();auto& e=f.spider();
            if(e.mode1==1&&(e.mode2==1||e.mode2==2)){moved=true;at=i;break;}
        }
        if(!moved){f.report("MV00");throw std::runtime_error("MV00: kein Zweigwechsel innerhalb 400 Passe");}
        auto& e=f.spider();
        auto* s=static_cast<RECVX_EN03_STATE*>(e.exp0);
        check(s->h.surface==0,"MV00: Flaeche 0 am Boden");
        const float* j0=f.joint(e,0);
        float first[16];float px=e.px,pz=e.pz;std::memcpy(first,j0,sizeof first);
        for(unsigned i=0;i<200;++i)f.tick();
        const float* j1=f.joint(e,0);
        check(std::memcmp(first,j1,sizeof first)!=0,"Motion: Gelenkmatrix weicht von der Bind-Pose ab");
        check(e.px!=px||e.pz!=pz,"MV01/MV03: bhEne03_GoAHead bewegt den Work");
        check(e.mtn_no!=0&&e.hokan_rate==32768,"MV01/MV03: Clip und Einblendrate gesetzt");
        f.no_gaps("Zustandswechsel");
        std::cout<<"  MV00 waehlt Zweig "<<unsigned(e.mode2)<<" nach "<<at<<" Passen\n";
    }
    {
        // Gegenprobe: ohne Gegnerpass laeuft der Work nicht — er bleibt in
        // [0,0,0,0], legt kein exp0 an und erzeugt keine Kinder. Wird der Pass
        // danach zugeschaltet, laeuft Init und derselbe Work wechselt den Modus.
        Fixture f(data,"RM_6090",false);
        auto& e=f.spider();
        check(e.mode0==0&&e.mode1==0&&e.mode2==0&&e.mode3==0,"Gegenprobe: Modus bleibt [0,0,0,0]");
        check(e.exp0==nullptr,"Gegenprobe: kein exp0 ohne Gegnerpass");
        check(f.spiders()==3&&f.count(31)==0&&f.count(24)==0,"Gegenprobe: keine Kinder ohne Gegnerpass");
        for(unsigned i=0;i<5;++i)f.tick();
        check(e.mode0==0&&e.exp0==nullptr,"Gegenprobe: auch nach fuenf Passen kein Init");
        f.world.set_enemy_ai(true);
        f.tick();
        check(e.mode0==1&&e.exp0!=nullptr,"Gegenprobe: mit Gegnerpass laeuft Init");
    }
    std::cout<<"EN03 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
