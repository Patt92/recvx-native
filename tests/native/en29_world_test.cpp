// EN29 Tentakel im Originalraum RM_9000 (Typ 0 und 1), echte RoomWorld-Dienste.
// Spielerlagen unten sind Diagnosevorbedingungen, kein natuerlich gespielter
// Fortschritt. Semantikreferenz en29.c.
#include "world.h"
#include "room.h"
#include "en29.h"
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
    explicit Fixture(const std::filesystem::path& data,const char* room="RM_9000"):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    RECVX_ENEMY_WORK& tentacle(unsigned type){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==29&&t.slot[i]->type==type)return *t.slot[i];throw std::runtime_error("Original-Tentakel fehlt");}
    unsigned children(){unsigned n=0;auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==29&&t.slot[i]->type==8&&(t.slot[i]->flg&1u))++n;return n;}
    void no_gaps(const char* phase){
        // Nicht portierte Effektprozeduren der Treffereffekte (309/311 Blut, 301) sind erlaubt.
        static const char* allowed[]={"bhEff309","bhEff311","bhEff301","rySetEffBlood"};
        for(const auto& g:world.unbound_enemy_routines()){bool ok=false;for(const char* a:allowed)if(g.find(a)!=std::string::npos)ok=true;
            if(!ok)throw std::runtime_error(std::string(phase)+": Luecke "+g);}
    }
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // bhEne29_Init (en29.c:178): Koerper, Kapselkinder, Trefferflaeche, Schatten 304, Aktion 30.
        Fixture f(data);auto& e=f.tentacle(0);auto* s=static_cast<RECVX_EN29_STATE*>(e.exp0);
        check(s&&e.mode0==1&&e.hp==160&&e.ar==3.0f&&e.ah==4.0f&&e.car==6.0f&&e.cah==4.0f,"Init: mode0 1, hp 160, ar/ah/car/cah");
        check((e.flg&0x958u)==0x958u&&!(e.flg&2u)&&(e.flg2&1u)&&e.lok_jno==16&&e.clp_jno[0]==9&&e.clp_jno[1]==15&&e.clp_jno[2]==19&&e.clp_jno[3]==-1,"Init: flg 0x158|0x800, flg2, clp_jno, lok_jno");
        check(s->bas_obj==1&&s->atr_obj==4&&s->atk_obj0==12&&s->atk_obj1==18&&s->atk_rng0==6.5f&&s->atk_rng1==6.5f,"Init: TypPrm[0]");
        check(f.children()==10,"SetDmgEne: fuenf Kapselkinder je Tentakel (zwei Tentakel)");
        unsigned ring=0;for(auto* d=s->dmg_eneP->nextP;d!=s->dmg_eneP;d=d->nextP){check(d->ewP&&d->ewP->type==8,"Ring: Kapselwerk Typ 8");++ring;}
        check(ring==5&&s->dmg_eneP->nextP==&s->DmgWrk[5],"SetDmgEne: Ring rueckwaerts, letzter Satz zuerst");
        check(s->ene_act.act_tblP&&s->ene_act.tbl_num==40&&s->ply_act.tbl_num==2&&s->ply_act.p_act_now==-1,"ActionInit/PlyActionInit");
        check(s->sdw_owk>=0,"rySetShadow fehlt");
        const auto* sh=f.world.effect_state(unsigned(s->sdw_owk));
        check(sh&&sh->id==304&&sh->mdlver==1&&sh->type==1&&sh->sy==6.0f&&sh->ct0==1&&sh->ct1==3&&sh->ct2==5,"Schatten: id 304, mdlver 1, type 1, sy 6, Gelenke 1/3/5");
        f.tick();
        check(s->ene_act.act_now==0&&e.mtn_no==0&&e.mtn_add==65536,"Br00: erste Aktion 0 (Schluessel 30 << 8 | 0)");
        check((s->status&0x40)&&!(sh->stflg&0x1000000u),"Move: Startbits und Schatten sichtbar");
        const auto f0=e.frm_no;f.tick();check(e.frm_no==f0+65536,"ActionMain: bhSetMotion schreitet fort");
        f.no_gaps("Init/Move");
    }
    {
        // Angriff: Spieler an atk_obj0 (Gelenk 12) -> sts 0x200 -> ACT_001 -> Mv01 -> PlySetDamage(2).
        Fixture f(data);f.tick();auto& e=f.tentacle(0);auto* s=static_cast<RECVX_EN29_STATE*>(e.exp0);
        const auto* ent=static_cast<const WorldEntity*>(e.owner);const auto& tree=ent->model_owner?*ent->model_owner:*ent;
        const auto* m=tree.enemy_owp_mtx.at(tree.model_window+12).value;

        RECVX_ENEMY_PLAYER p{};check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");
        p.pos.x=m[12];p.pos.y=m[13];p.pos.z=m[14];const int hp0=p.hp;check(f.world.enemy_player_set(p),"Spieler setzen");
        bool attacked=false,hit=false,knock_end=false;int hit_at=-1;
        for(int i=0;i<400;++i){f.tick();
            if(s->ene_act.act_now==1)attacked=true;
            RECVX_ENEMY_PLAYER q{};f.world.enemy_player_get(q);
            if(!hit&&(s->p_status&1)){hit=true;hit_at=i;
                check(q.hp==hp0-5&&q.mode0==2&&q.mode2==2&&(q.flg&0x210004u)==0x210004u&&s->p_br_mde0==2,"PlySetDamage: hp - 5, mode0 2/mode2 2, flg 0x210004");
                check(std::fabs(s->ply_dmg.x*s->ply_dmg.x+s->ply_dmg.z*s->ply_dmg.z-1.0f)<1e-4f||!(s->ply_dmg.x==0&&s->ply_dmg.z==0),"AttackHitCheck: Stossrichtung");}
            if(hit&&!(s->p_status&1)){knock_end=true;break;}
        }
        if(!(attacked&&hit&&knock_end)){
            std::cerr<<"Angriff: attacked="<<attacked<<" hit="<<hit<<" (Pass "<<hit_at<<") end="<<knock_end<<" act="<<s->ene_act.act_now<<" br="<<s->br_mde0<<'/'<<s->br_mde1<<" status=0x"<<std::hex<<s->status<<std::dec<<'\n';
            for(const auto& g:f.world.unbound_enemy_routines())std::cerr<<"  gap: "<<g<<'\n';
            throw std::runtime_error("Angriffskette Mv01 -> PlySetDamage -> PlyMoveMain");
        }
        f.no_gaps("Angriff");
    }
    {
        // Treffer an einer Kapsel: CheckDmgEne uebernimmt djnt_no/Lage/Richtung an den Koerper,
        // DmgCheck senkt hp und fuehrt Br00 in BR0_DAMAGE (Aktion 2).
        Fixture f(data);f.tick();auto& e=f.tentacle(0);auto* s=static_cast<RECVX_EN29_STATE*>(e.exp0);
        auto* c=s->dmg_eneP->nextP->ewP;
        c->flg|=0x4u;c->djnt_no=10;c->dpx=e.px;c->dpy=e.py+5;c->dpz=e.pz;c->dvx=0;c->dvy=0;c->dvz=1;c->wpnr_no=2;c->comb_flg=0x10u;
        for(auto& d:c->dam)d=0;c->dam[0]=20;
        const int hp0=e.hp;f.tick();
        check(!(c->flg&0x4u),"CheckDmgEne: Kapsel-Trefferbit geloescht");
        check(e.djnt_no==10&&e.wpnr_no==2,"CheckDmgEne: Treffer an den Koerper uebergeben");
        check(e.hp<=hp0&&s->br_mde0==2,"DmgCheck/Br00: BR0_DAMAGE");
        f.no_gaps("Treffer");
    }
    std::cout<<"EN29 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
