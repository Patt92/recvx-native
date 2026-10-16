// EN15 Nosferatu im Originalraum RM_5800, echte RoomWorld-Dienste.
// Semantikreferenz src/ps2/veronica/prog/en15.c. Die Spielerlagen unten sind
// Diagnosevorbedingungen, kein natuerlich gespielter Fortschritt.
#include "world.h"
#include "room.h"
#include "en15.h"
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
    explicit Fixture(const std::filesystem::path& data):Fixture(data,"RM_5800",true){}
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    RECVX_ENEMY_WORK* nosferatu(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==15)return t.slot[i];return nullptr;}
    void report(const char* phase){
        std::cerr<<phase<<": Luecken:\n";
        for(const auto& g:world.unbound_enemy_routines())std::cerr<<"  "<<g<<'\n';
    }
    // Der Nosferatu-Kern EN53 meldet in diesem Raum seine eigene, getrennte Luecke (child_ambient).
    void no_gaps(const char* phase){
        for(const auto& g:world.unbound_enemy_routines()){
            if(g.find("EN53")!=std::string::npos)continue;
            report(phase);throw std::runtime_error(std::string(phase)+": unerwartete Luecke "+g);
        }
    }
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        Fixture f(data);
        auto* e=f.nosferatu();check(e!=nullptr,"RM_5800: Nosferatu (id 15) fehlt");
        std::cout<<"  vorher: mode "<<int(e->mode0)<<" stflg 0x"<<std::hex<<e->stflg<<" flg 0x"<<e->flg<<std::dec<<" hp "<<e->hp<<'\n';
        f.tick();
        auto* s=static_cast<RECVX_EN15_STATE*>(e->exp0);
        std::cout<<"  nach 1 Pass: mode "<<int(e->mode0)<<'/'<<int(e->mode1)<<" exp0 "<<(s?1:0)<<" mtn "<<e->mtn_no<<" frm "<<e->frm_no<<'\n';
        check(s!=nullptr,"Init: exp0 (100 Byte) angelegt");
        check(e->mode0==1,"Init: mode0 1");
        check(e->hp==600||e->hp==360,"Init: hp 600 (gm_mode 2: 360)");
        check(e->ar==3.8f&&e->ah==20.0f&&e->car==2.5f&&e->cah==20.0f,"Init: ar/ah/car/cah");
        check(e->clp_jno[0]==4&&e->clp_jno[1]==20&&e->clp_jno[2]==23&&e->clp_jno[3]==-1&&e->lok_jno==4,"Init: clp_jno/lok_jno");
        check((e->flg&0x178u)==0x178u&&(e->flg&0x800u),"Init: flg 0x178|0x800");
        check(s->hold==0&&s->poison==0,"Init: Haltebit und Giftzaehler 0");
        check(e->watr.r==2.5f,"Pass: watr r 2.5 aus owP[4]/owP[20,23]");
        f.no_gaps("Init");
    }
    {
        // Wurfkette (en15.c:540 Stand -> :1240 Throw): Spieler 4 Einheiten vor dem
        // Becken (owP[1]) in Blickrichtung. Stand waehlt mode0 2 / mtn 5; bei Bild 22
        // greift Throw (plp->mode0 4, Haltebit, mode3 5 HoldPlayer), bei 55 FlyingPlayer
        // (mode3 6), bei 78 der Wurf (mode3 0 DrivePlayer) mit Rueckstoss 3.
        Fixture f(data);f.tick();
        auto* e=f.nosferatu();check(e!=nullptr,"Nosferatu fehlt");
        auto* s=static_cast<RECVX_EN15_STATE*>(e->exp0);
        const auto* ent=static_cast<const WorldEntity*>(e->owner);const auto& tree=ent->model_owner?*ent->model_owner:*ent;
        const auto* m=tree.enemy_owp_mtx.at(tree.model_window+1).value;
        RECVX_ENEMY_PLAYER p{};check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");
        const float fx=-std::sin(e->ay*(3.14159265f/32768.0f)),fz=-std::cos(e->ay*(3.14159265f/32768.0f));
        p.pos.x=m[12]+4.0f*fx;p.pos.z=m[14]+4.0f*fz;check(f.world.enemy_player_set(p),"Spieler setzen");
        bool threw=false,grabbed=false,flying=false,driven=false;int max_mode0=0;
        for(int i=0;i<300;++i){
            f.tick();
            if(e->mode0==2)threw=true;
            RECVX_ENEMY_PLAYER q{};f.world.enemy_player_get(q);
            if(q.mode0>max_mode0)max_mode0=q.mode0;
            if((s->hold&1)&&e->mode3==5&&q.mode0==4)grabbed=true;
            if((s->hold&1)&&e->mode3==6)flying=true;
            if(flying&&(s->hold&1)&&e->mode3==0)driven=true;
            if(driven)break;
        }
        std::cout<<"  Wurf: threw="<<threw<<" grabbed="<<grabbed<<" flying="<<flying<<" driven="<<driven<<" mode0="<<int(e->mode0)<<'/'<<int(e->mode1)<<" mtn "<<e->mtn_no<<" frm "<<(e->frm_no>>16)<<" max plp mode0 "<<max_mode0<<'\n';
        if(!(threw&&grabbed&&flying&&driven)){f.report("Wurf");throw std::runtime_error("Wurfkette Stand -> Throw -> Hold -> Flying -> Drive");}
        f.no_gaps("Wurf");
        // DrivePlayer (Flug mit plp->dv, Aufprall) -> SlidePlayer (bhAddSpeed mit
        // plp->spd *= 0.94) -> StandupPlayer (mnwPb zurueck, Haltebit frei) oder der
        // Absturz ueber den Raumrand (FallingPlayer -> FallDiePlayer).
        std::string seq;int last=-1;bool released=false;
        for(int i=0;i<600;++i){
            f.tick();
            if((s->hold&1)&&int(e->mode3)!=last){last=int(e->mode3);seq+=std::to_string(last);}
            if(!(s->hold&1)){released=true;break;}
        }
        RECVX_ENEMY_PLAYER q{};f.world.enemy_player_get(q);
        std::cout<<"  Fuehrung: mode3-Folge "<<seq<<" released="<<released<<" plp mode0 "<<int(q.mode0)<<" hp "<<q.hp<<'\n';
        check(released,"Fuehrung: das Haltebit wird wieder frei");
        check(seq.size()>=2&&seq[0]=='0'&&seq[1]=='1',"Fuehrung: DrivePlayer -> SlidePlayer");
        if(seq.back()=='2'){
            check(q.mode0==1&&!(q.flg&0x10004u),"StandupPlayer: plp->mode0 1, flg 0x10004 frei");
        }else{
            // AbleToFall (rom->posp[1..4]) hinter dem Plattformrand: FallingPlayer -> FallDiePlayer.
            check(seq.find("34")!=std::string::npos,"Sturz: FallingPlayer (3) -> FallDiePlayer (4)");
            check(q.mode0==6&&q.hp<0&&(q.flg&2u),"FallDiePlayer: plp->mode0 6, hp < 0, flg 2");
            check((f.events.flags[12][0]&0x4000u)!=0,"FallDiePlayer: sys->ts_flg 0x4000");
        }
        f.no_gaps("Fuehrung");
    }
    {
        // Treffer (en15.c:1564 CheckDamage): hp sinkt, bhEne_SetPoison legt drei Saetze 397 an
        // (Typ 1 Quelle, zwei Typ-0-Fluegel) mit exp0 = epw; bhEff_E15_Poison laeuft danach.
        Fixture f(data);f.tick();
        auto* e=f.nosferatu();check(e!=nullptr,"Nosferatu fehlt");
        const auto* ent=static_cast<const WorldEntity*>(e->owner);const auto& tree=ent->model_owner?*ent->model_owner:*ent;
        const auto* m=tree.enemy_owp_mtx.at(tree.model_window+5).value;
        for(auto& d:e->dam)d=0;
        e->dam[5]=20;e->djnt_no=5;e->wpnr_no=4;e->comb_flg=0x10u;e->flg2=0;
        e->dpx=m[12];e->dpy=m[13];e->dpz=m[14];e->dvx=0;e->dvy=0;e->dvz=1;
        e->flg|=0x4u;
        const int hp0=e->hp;
        f.tick();
        check(!(e->flg&0x4u),"CheckDamage: Trefferbit geloescht");
        check(e->hp<hp0,"CheckDamage: hp sinkt");
        unsigned poison=0,owned=0,types=0;
        for(unsigned i=0;i<512;++i){const auto* sp=f.world.effect_state(i);if(!sp||!(sp->flg&3u)||sp->id!=397)continue;++poison;if(sp->owner_work==e)++owned;types|=1u<<sp->type;}
        std::cout<<"  Gift: "<<poison<<" Saetze 397, "<<owned<<" mit exp0 = epw, Typen 0x"<<std::hex<<types<<std::dec<<'\n';
        check(poison>=3&&owned==poison&&(types&3u)==3u,"bhEne_SetPoison: drei Saetze 397 (Typ 1 und 0) am Werk");
        for(int i=0;i<20;++i)f.tick();
        f.no_gaps("Gift");
    }
    {
        // Gegenprobe: ohne Gegnerpass kein Init.
        Fixture f(data,"RM_5800",false);
        auto* e=f.nosferatu();check(e!=nullptr,"Nosferatu fehlt");
        for(int i=0;i<5;++i)f.tick();
        check(e->exp0==nullptr&&e->mode0==0,"Gegenprobe: ohne Gegnerpass kein Init");
    }
    std::cout<<"EN15 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
