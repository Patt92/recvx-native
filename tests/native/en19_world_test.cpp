// EN19 Tyrant T-078 in RM_5840 (Originalraum mit Typ 2, in dem der Tyrant ohne
// Skriptsperre laeuft; RM_1122 haelt ihn mit stflg 0x1000000, RM_3230/RM_5010 setzen
// ihn per Skript), echte RoomWorld-Dienste. Zustands- und Lageschreibungen unten
// sind Diagnosevorbedingungen, kein natuerlich gespielter Fortschritt.
// Semantikreferenz en19.c.
#include "world.h"
#include "room.h"
#include "en19.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data,const char* room="RM_5840"):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        world.set_camera_offset_owner(&camera); // cam.ofx/ofy/ofz (bhEff30a, Effekt 310)
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit(); /* player.c:840 bhSetPlayer: bhCalcModel(plp) vor Event_init (runtime.cpp) */
        events.initialize();
    }
    // bhCalcModel(plp) je Bild (runtime.cpp nach world->tick()): plp->watr/owP fuer
    // bhEne19_AttackHitCheck und die Spielergelenke der Blut-/Staubeffekte.
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    RECVX_ENEMY_WORK& tyrant(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==19)return *t.slot[i];throw std::runtime_error("original RM_5840 tyrant missing");}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    // Nur die Luecken, die nicht EN19-Logik sind: die unportierten effsub3.c-Effekte
    // (304 Schatten, 305 Staub, 307 Klauenflaeche, 308 Funken, 309/311 Blut, 310 Kamera,
    // 312 Klaue, 349 Rueckruf) und die Sprites 303/306/298 aus bhEne19_SetDmgEffect.
    void no_en19_gaps(const char* phase){
        static const char* allowed[]={"bhEff30","bhEff349","bhEff298","bhEff303","bhEff305","bhEff306","bhEff304","bhEff307","bhEff308","bhEff309"};
        for(const auto& g:world.unbound_enemy_routines()){bool ok=false;for(const char* a:allowed)if(g.find(a)!=std::string::npos)ok=true;
            if(!ok){std::cerr<<phase<<": unerwartete Luecke: "<<g<<'\n';throw std::runtime_error("EN19 meldet eine Luecke");}}
    }
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // bhEne19_Init (en19.c:285): Koerper, Tabellen, Aktion 0, Typ-2-Statusbits. Der erste
        // Gegnerpass (Init) laeuft beim Raumaufbau (player.c:840 vor Event_init), also vor dem ersten tick().
        Fixture f(data);auto& e=f.tyrant();
        check(e.exp0&&e.mode0==1&&e.mode1==0&&e.mode2==0&&e.mode3==0&&e.exp2==nullptr,"bhEne19_Init: mode0 1 / exp2 NULL");
        check(e.type==2,"RM_5840: Tyrant ist nicht Typ 2");
        check(e.ar==5.0f&&e.ah==4.0f&&e.aw==0&&e.ad==0&&e.car==6.5f&&e.cah==4.0f,"bhEne19_Init: ar/ah/car/cah");
        check(e.hp==300||e.hp==420||e.hp==540||e.hp==900,"bhEne19_Init: hp aus En19HpTbl[gm_mode][2]");
        check(e.cpcl==recvx_en19_cap_col&&e.mtn_tp==recvx_en19_flp_tbl,"bhEne19_Init: cpcl / mtn_tp");
        check((e.flg&0x178u)==0x178u&&!(e.flg&0x2u)&&(e.flg2&1u)&&(e.stflg&0x4000u)&&(e.flg&0x800u),"bhEne19_Init: flg 0x178, flg2 1, stflg 0x4000, Schatten 0x800");
        check(e.clp_jno[0]==5&&e.clp_jno[1]==17&&e.clp_jno[2]==9&&e.clp_jno[3]==1&&e.clp_jno[4]==25&&e.clp_jno[5]==21&&e.clp_jno[6]==-1,"bhEne19_Init: clp_jno");
        check(!(e.mdflg&0x20u)&&e.lok_jno==4,"bhEne19_Init: mdflg/lok_jno");
        const auto* s=static_cast<const RECVX_EN19_STATE*>(e.exp0);
        check(s->ewP==&e&&s->watr_top==4&&s->watr_rad==3.0f&&s->ct_pinch==3600&&s->hp_bak==e.hp,"bhEne19_Init: exp0 Felder");
        check(s->ovl_rte==1.5f&&s->e0aP==recvx_en19_eff30a_prm2,"bhEne19_Init: En19OvlTbl[2] / Eff30aTbl[2]");
        check(s->atk_rng_a_near==12.0f&&s->atk_rng_b_near==13.0f&&s->atk_rng_a_far==14.0f&&s->atk_rng_b_far==15.0f,"bhEne19_Init: En19AtkRng[2]");
        check((std::uint32_t(s->status)&0xE0000000u)==0xE0000000u,"bhEne19_Init: status 0xE0000000 fuer Typ 2");
        // bhEne19_ActionChange(0) (en19.c:413) findet KEINEN Eintrag: act_now ist im genullten
        // Block 0 == act_dst, also weder Schluessel 0x0000 noch der Ersatzschluessel (-1, 0)
        // (en19.c:2699-2702). Der Original-Init laesst mtn_no/frm_no/mtn_add 0 und ohne
        // status 0x8000; erst der erste Brain-Pass waehlt eine Aktion ueber (-1, act).
        if(!(s->act_now==0&&e.mtn_no==0&&e.frm_no==0&&e.mtn_add==0&&e.hokan_count==0&&!(s->status&0x8000)&&s->chg_rte==0)){
            std::cerr<<"ActionChange(0): act_now="<<s->act_now<<" mtn="<<e.mtn_no<<" frm="<<e.frm_no<<" mtn_md="<<e.mtn_md<<" hokan="<<e.hokan_count<<" add="<<e.mtn_add<<" chg_rte="<<s->chg_rte<<" status=0x"<<std::hex<<s->status<<std::dec<<'\n';
            throw std::runtime_error("bhEne19_ActionChange(0) darf keinen Eintrag finden");}
        check(s->ply_act.act_tblP==recvx_en19_ply_act_tbl&&s->ply_act.tbl_num==17&&s->ply_act.p_act_now==-1,"bhEne_PlyActionInit");
        // Bereits im ersten Pass die ClwPos aus objP[10..13].pos (en19.c:375).
        check(f.world.state(1,0)&&std::fabs(s->ClwPos[0].x-f.world.state(1,0)->enemy_obj.at(10).position.x)<1e-6f,"bhEne19_Init: ClwPos[0] = objP[10].pos");
        f.no_en19_gaps("Init");
        // Zweiter Pass (bhEne19 :239): exp2 = -1, Effektanlage 310/312 (Luecken), dann Move.
        // Erster Move-Pass: bhEne19 :239 setzt exp2 = -1; Br02 waehlt ueber (-1, act) Walk 5
        // (En19ActTbl 65285: Motion 4/Mv04b, hkn_cnt 8 -> 7 nach bhSetMotion im selben Pass) oder
        // Rotate 8 (65288: Motion 8, hkn_cnt 16 -> 15); mtn_md 0x20, flag 0x80000 -> status 0x8000.
        f.tick();
        if(!(e.exp2!=nullptr&&e.mode0==1&&((s->act_now==5&&e.mtn_no==4u&&e.hokan_count==7u)||(s->act_now==8&&e.mtn_no==8u&&e.hokan_count==15u))&&e.mtn_md==0x20u&&e.mtn_add==65536&&(s->status&0x8000)&&s->chg_rte==65536)){
            std::cerr<<"Move 1: act_now="<<s->act_now<<" mtn="<<e.mtn_no<<" mtn_md="<<e.mtn_md<<" hokan="<<e.hokan_count<<" add="<<e.mtn_add<<" chg_rte="<<s->chg_rte<<" status=0x"<<std::hex<<s->status<<std::dec<<" exp2="<<e.exp2<<'\n';
            throw std::runtime_error("bhEne19 erster Move-Pass");}
    }
    {
        // bhEne19_Move / Br02 (en19.c:430/1539): der Tyrant sucht den Spieler (Walk 5 oder Rotate 8),
        // die Aktionsuhr laeuft (mtn_rte), Attribute/Schritte werden gelesen (snd_no -> RequestEnemySe).
        Fixture f(data);f.tick();auto& e=f.tyrant();auto* s=static_cast<RECVX_EN19_STATE*>(e.exp0);
        const float x0=e.px,z0=e.pz;const int ay0=e.ay;
        bool walked=false,rotated=false;
        for(int i=0;i<300;++i){f.tick();if(s->act_now==5)walked=true;if(s->act_now==8)rotated=true;}
        check(e.mode0==1,"Move: mode0 verlassen");
        check(walked||rotated,"Br02: weder Walk (5) noch Rotate (8) gewaehlt");
        check(e.frm_no!=0||s->mtn_rte!=0,"ActionMain: Motionuhr steht");
        check(e.px!=x0||e.pz!=z0||e.ay!=ay0,"Br02: der Tyrant bewegt sich nicht");
        check(f.world.state(1,0)->enemy_calculated,"bhCalcModel (CalcEnemy) nie gelaufen");
        check(e.watr.r==3.0f,"CalcEnemy: watr.r");
        check(s->br_mde0==0,"Br02: br_mde0 verlassen ohne Schaden");
        f.no_en19_gaps("Move");
    }
    {
        // bhEne19_DmgCheck / CheckDmgLvl2 (en19.c:3193/4743): Treffer mit hp < 0 -> TY_DMG_LVL4,
        // Br02 BR1_DAMAGE -> Aktion 18 (flg &= ~0x60), dann DOWN (19) und DIE (2, flg 0x2).
        // Trefferfelder wie weapon.c (A04 ist nicht gebunden): dam[4], flg 0x4, comb_flg 0x10, Waffe 3.
        Fixture f(data);f.tick();auto& e=f.tyrant();auto* s=static_cast<RECVX_EN19_STATE*>(e.exp0);
        for(int i=0;i<5;++i)f.tick();
        e.hp=5;e.wpnr_no=3;e.comb_flg|=0x10u;e.dam[4]=10;e.djnt_no=4;e.flg|=0x4u; /* weapon.c: dam[j], djnt_no, comb_flg 0x10, flg 0x4 */
        f.tick();
        // Br02 BR1_DAMAGE FIRST mit dmg_lvl >= 3 (en19.c:1875): flg &= ~0x60, Aktion 18 (En19ActTbl
        // 65298 -> Motion 17), LOOP1; dmg_lvl bleibt in diesem Zweig stehen (nur der <3-Zweig nullt).
        if(!(e.hp==-5&&s->dmg_lvl==5&&s->br_mde1==6&&s->br_mde2==2&&s->act_now==18&&e.mtn_no==17&&!(e.flg&0x60u)&&!(e.flg&0x4u))){
            std::cerr<<"DmgCheck: hp="<<e.hp<<" dmg_lvl="<<s->dmg_lvl<<" br="<<s->br_mde0<<'/'<<s->br_mde1<<'/'<<s->br_mde2<<" act="<<s->act_now<<" mtn="<<e.mtn_no<<" flg=0x"<<std::hex<<e.flg<<std::dec<<'\n';
            for(const auto& g:f.world.unbound_enemy_routines())std::cerr<<"  gap: "<<g<<'\n';
            throw std::runtime_error("DmgCheck/Br02 BR1_DAMAGE");
        }
        check(s->dmg_obj==4,"DmgCheck: dmg_obj = djnt_no");
        int down=-1,die=-1;
        for(int i=0;i<600;++i){f.tick();if(down<0&&s->act_now==19)down=i;if(die<0&&s->act_now==2)die=i;if(die>=0&&e.mtn_add==0)break;}
        check(down>=0,"Br02 BR0_DOWN: Aktion 19 nie erreicht");
        check(die>=0&&(e.flg&0x2u)&&s->br_mde0==3,"Br02 BR0_DIE: Aktion 2 / flg 0x2");
        check(e.mtn_add==0,"Br02 BR0_DIE LOOP0: mtn_add 0");
        f.no_en19_gaps("Damage");
    }
    {
        // Angriff (Br02 BR1_ATTACK -> Mv08/Mv09 -> bhEne19_PlySetDamage -> PlyMoveMain/PlyDmg042):
        // Diagnosevorbedingung: Spieler 7 Einheiten vor dem Tyrant (Vorwaertsrichtung -sin/-cos ay).
        Fixture f(data);f.tick();auto& e=f.tyrant();auto* s=static_cast<RECVX_EN19_STATE*>(e.exp0);
        RECVX_ENEMY_PLAYER p{};check(f.world.enemy_player_get(p),"Spielerbesitzer fehlt");
        p.pos.x=e.px-7.0f*njSin(Angle(e.ay));p.pos.z=e.pz-7.0f*njCos(Angle(e.ay));p.ay=e.ay+32768;
        const int hp0=p.hp;check(f.world.enemy_player_set(p),"Spieler setzen");
        bool attacked=false,hit=false,returned=false;int hit_at=-1;
        for(int i=0;i<600;++i){f.tick();
            if(s->act_now==9||s->act_now==10)attacked=true;
            RECVX_ENEMY_PLAYER q{};f.world.enemy_player_get(q);
            if(!hit&&(s->ply_act.p_status&1)){hit=true;hit_at=i;
                check(q.mode0==5&&(q.flg&0x10004u)&&(q.stflg&0x44000u)==0x44000u&&q.hp==hp0-40,"PlySetDamage: plp mode0 5 / flg 0x10004 / stflg 0x44000 / hp - 40");}
            if(hit&&!(s->ply_act.p_status&1)&&q.mode0==1){returned=true;check(q.mtn_enemy==0&&!(q.flg&0x10004u)&&!(q.stflg&0x54000u),"PlyDmgRtn: Spieler zurueck (mnwPb, Flags geloescht)");break;}
        }
        if(!(attacked&&hit&&returned)){
            std::cerr<<"Angriff: attacked="<<attacked<<" hit="<<hit<<" (Pass "<<hit_at<<") returned="<<returned<<" act="<<s->act_now<<" br="<<s->br_mde0<<'/'<<s->br_mde1<<'/'<<s->br_mde2<<" status=0x"<<std::hex<<s->status<<std::dec<<" tgt_dst="<<s->tgt_dst<<" tgt_ang="<<s->tgt_ang<<" p_mode="<<s->ply_act.p_mode<<'\n';
            for(const auto& g:f.world.unbound_enemy_routines())std::cerr<<"  gap: "<<g<<'\n';
            throw std::runtime_error("Angriffskette Mv08/09 -> PlySetDamage -> PlyDmgRtn");
        }
        f.no_en19_gaps("Angriff");
    }
    std::cout<<"EN19 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
