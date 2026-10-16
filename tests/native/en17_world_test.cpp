// EN17 Monster Steve in RM_9280 (Originalraum; stflg 0x1000000 haelt ihn bis das Skript
// ihn freigibt), echte RoomWorld-Dienste. Lage- und Zustandsschreibungen unten sind
// Diagnosevorbedingungen, kein natuerlich gespielter Fortschritt. Semantikreferenz en17.c.
#include "world.h"
#include "room.h"
#include "en17.h"
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
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    explicit Fixture(const std::filesystem::path& data,const char* room="RM_9280"):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    RECVX_ENEMY_WORK& steve(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==17)return *t.slot[i];throw std::runtime_error("Original-Steve fehlt");}
    void no_gaps(const char* phase){
        // Nicht gebundene Effektdienste (Partikel 365/366, Nachbild 364, Licht, Spielerblut) sind benannte Luecken.
        static const char* allowed[]={"bhEff_SetPtcl","bhEff365","bhEff366","bhEne17_AfterimageAxEffect","rom->lgtp[2]","bhEne_SetBlood2","bhEff369","bhEff370","bhEff301"};
        for(const auto& g:world.unbound_enemy_routines()){bool ok=false;for(const char* a:allowed)if(g.find(a)!=std::string::npos)ok=true;
            if(!ok)throw std::runtime_error(std::string(phase)+": Luecke "+g);}
    }
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // bhEne17_Init (en17.c:789): Koerper, Tabellen, zwei Kettenwerke, Schatten.
        Fixture f(data);auto& e=f.steve();auto* x0=static_cast<RECVX_EN17_EXP0*>(e.exp0);
        check(x0&&e.exp1&&e.mode0==1&&e.mode1==1&&e.mode2==0&&e.mode3==0,"Init: mode0 1/mode1 1");
        check(e.hp==250&&e.ar==8.0f&&e.car==10.0f&&e.lok_jno==4,"Init: hp 250, ar 8, car 10, lok_jno 4");
        {const auto* tree=static_cast<const WorldEntity*>(e.owner);const auto& m=tree->enemy_owp_mtx.at(5).value;check(e.ah==e.cah&&std::fabs(e.ah-(m[13]-e.py))<1e-3f,"CalcEnemy (Init): cah = ah = owP[5].y - py");}
        check((e.flg&0x878u)==0x878u&&!(e.flg&2u),"Init: flg 0x78 | 0x800 (Schatten), ohne 0x2");
        check((x0->flags&0x11u)==0x11u,"Init: exp0.flags 0x11");
        check(e.clp_jno[0]==5&&e.clp_jno[1]==1&&e.clp_jno[2]==12&&e.clp_jno[3]==8&&e.clp_jno[4]==21&&e.clp_jno[5]==16&&e.clp_jno[6]==-1,"Init: clp_jno");
        auto* larm=static_cast<RECVX_ENEMY_WORK*>(e.exp2);auto* rarm=static_cast<RECVX_ENEMY_WORK*>(e.exp3);
        check(larm&&rarm&&larm->id==0x1F&&rarm->id==0x20&&larm->lkwkp==&e&&rarm->lkwkp==&e&&larm->lkono==0xD&&rarm->lkono==9,"SetLinkWork: zwei Kettenwerke 0x1F/0x20 an Gelenk 13/9");
        check((larm->flg&0x80A1u)==0x80A1u,"SetLinkWork: flg 0x80A1");
        f.no_gaps("Init");
    }
    {
        // Move/Brain: Steve in Reichweite -> Brain00 waehlt MV03/MV04 (mode2 3/4), dann Angriff mit Spielerkontakt.
        Fixture f(data);f.tick();auto& e=f.steve();auto* x0=static_cast<RECVX_EN17_EXP0*>(e.exp0);
        e.stflg&=~0x01000000u; // Skript-Freigabe: Diagnosevorbedingung (RM_9280 Event startet den Kampf)
        RECVX_ENEMY_PLAYER p{};check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");
        p.pos.x=e.px-14.0f*njSin(Angle(e.ay));p.pos.z=e.pz-14.0f*njCos(Angle(e.ay));p.ay=e.ay+32768;
        const int hp0=p.hp;check(f.world.enemy_player_set(p),"Spieler setzen");
        bool moved=false,attacked=false,hit=false;
        for(int i=0;i<600;++i){f.tick();
            if(e.mode2==3||e.mode2==4||e.mode2==5)moved=true;
            if(e.mtn_no==15||e.mtn_no==16||e.mtn_no==11||e.mtn_no==2)attacked=true;
            RECVX_ENEMY_PLAYER q{};f.world.enemy_player_get(q);
            if(q.hp<hp0){hit=true;break;}
        }
        if(!(moved&&attacked)){
            std::cerr<<"MV: moved="<<moved<<" attacked="<<attacked<<" mode="<<int(e.mode0)<<'/'<<int(e.mode1)<<'/'<<int(e.mode2)<<'/'<<int(e.mode3)<<" mtn="<<e.mtn_no<<" dist="<<x0->player_distance<<" stflg=0x"<<std::hex<<e.stflg<<std::dec<<'\n';
            for(const auto& g:f.world.unbound_enemy_routines())std::cerr<<"  gap: "<<g<<'\n';
            throw std::runtime_error("Brain00 -> MV03/04 -> Angriffsmotion");
        }
        f.no_gaps("Bewegung/Angriff");
        check(hit,"MV03/04: der Angriff trifft den Spieler in Reichweite (PlayerDGCheck)");
    }
    {
        // Treffer: DmgChk (en17.c:425): CalcDamage setzt total_dam und comb_flg, DamageAdd (:487) zieht
        // KEINE hp ab (das tut der Waffenpfad), wechselt das erste Bit von EXP0_UC(1) und aktiviert
        // ChgDmgMode (:454): Waffe 3 hat nm_act 4 (>= 4U) -> kein Wechsel in mode0 3.
        Fixture f(data);f.tick();auto& e=f.steve();auto* x0=static_cast<RECVX_EN17_EXP0*>(e.exp0);e.stflg&=~0x01000000u;f.tick();
        const int hp0=e.hp;
        e.flg|=4u;e.djnt_no=5;e.dpx=e.px;e.dpy=e.py+10;e.dpz=e.pz;e.dvx=0;e.dvy=0;e.dvz=1;e.wpnr_no=3;e.comb_wep=3;e.comb_flg=0x10u;
        for(auto& d:e.dam)d=0;e.dam[5]=20;
        const unsigned flags0=x0->flags;
        f.tick();
        check(e.hp==hp0,"DamageAdd: hp wird vom Waffenpfad abgezogen, nicht von EN17");
        check(e.total_dam==20&&e.comb_wep==3,"CalcDamage: total_dam 20 (correct 0), comb_wep 3");
        check(!(e.flg&4u)&&e.dam[5]==0,"bhEne17: flg 0x4 und dam[] werden am Ende des Passes geloescht (en17.c:364)");
        check(x0->damage_toggle==1&&x0->effect_timer>=0,"DamageAdd: EXP0_UC(1) wechselt 0 -> 1");
        (void)flags0;
        // Zweiter Treffer: EXP0_UC(1) ist 1 -> flags |= 0x200, UC(1) = 0 (en17.c:433).
        e.flg|=4u;e.djnt_no=5;e.dvz=1;e.wpnr_no=3;e.comb_flg=0x10u;for(auto& d:e.dam)d=0;e.dam[5]=20;
        f.tick();
        check(x0->damage_toggle==0&&(x0->flags&0x200u),"DmgChk: zweiter Treffer setzt flags 0x200, UC(1) = 0");
        // Waffe 2 (nm_act 4) bleibt in Move; Waffe 6 (nm_act 2) und hp >= 0 wechselt mode0 auf 3.
        e.flg|=4u;e.wpnr_no=6;e.comb_wep=6;e.comb_flg=0x10u;for(auto& d:e.dam)d=0;e.dam[5]=20;
        f.tick();
        check(e.mode0==3||e.mode2==0,"ChgDmgMode: nm_act 2 (< 4) -> mode0 3 (Damage)");
        f.no_gaps("Treffer");
    }
    std::cout<<"EN17 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
