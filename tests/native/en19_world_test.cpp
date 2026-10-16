// EN19 Tyrant T-078 in RM_5840 (Originalraum mit Typ 2, in dem der Tyrant ohne
// Skriptsperre laeuft; RM_1122 haelt ihn mit stflg 0x1000000, RM_3230/RM_5010 setzen
// ihn per Skript), echte RoomWorld-Dienste. Zustands- und Lageschreibungen unten
// sind Diagnosevorbedingungen, kein natuerlich gespielter Fortschritt.
// Semantikreferenz en19.c.
#include "world.h"
#include "room.h"
#include "en19.h"
#include "effect_rap_camera.h"
#include "effect_rap_claw.h"
#include "combat.h"
#include "inventory.h"
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
    // Stand 2026-10-16: die ganze Effektkette des Tyrant ist portiert und gebunden
    // (298, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 349). Keine Ausnahme mehr:
    // jede gemeldete Luecke ist ab hier ein Fehler.
    void no_en19_gaps(const char* phase){
        for(const auto& g:world.unbound_enemy_routines()){
            std::cerr<<phase<<": unerwartete Luecke: "<<g<<'\n';throw std::runtime_error("EN19 meldet eine Luecke");}
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
    {
        // bhEne19 zweiter Pass (en19.c:239-275): exp2 = -1, dann die OR_WORK-Saetze des Typ-2-Tyrant
        // (status 0xE0000000): 310 mit Eff30aPrm2[0] (Typ 1: cam.ofx = 0 je Pass), 349 mit
        // fnc_prcP = bhEne19_TyBloodSet, drei gespiegelte Klauen 312 an owP[17] (clw0/-/clw2).
        Fixture f(data);f.tick();auto& e=f.tyrant();auto* s=static_cast<RECVX_EN19_STATE*>(e.exp0);
        f.tick();
        unsigned n310=0,n349=0,n312=0;const EffectSprite* claw0=nullptr;
        for(unsigned i=0;i<512;++i){const auto* sp=f.world.effect_state(i);if(!sp||!(sp->flg&1u))continue;
            if(sp->id==310){++n310;check(sp->shake30a&&sp->shake30a->prm_a.type==1,"310: Eff30aPrm2[0] ist Typ 1");}
            if(sp->id==349){++n349;check(sp->callback349&&sp->callback349->arg==&e,"349: fnc_prcP-Argument ist das Work");}
            if(sp->id==312){++n312;if(int(i)==s->clw0)claw0=sp;}}
        std::cout<<"  Effektanlage: 310="<<n310<<" 349="<<n349<<" 312="<<n312<<" clw0="<<s->clw0<<" clw2="<<s->clw2<<" cam_clr="<<s->cam_clr<<'\n';
        // en19.c:243/248/264: 310 bei status 0x20000000, 349 bei 0x10000000, Klauen bei 0x80000000.
        // Der Typ-2-Tyrant (status 0xE0000000) legt kein 349 an; RM_5010 (anderer Typ) tut es.
        const unsigned expect349=(std::uint32_t(s->status)&0x10000000u)?1u:0u;
        check(n310==1&&n349==expect349&&n312==3,"bhEne19 :243-275: ein 310, 349 nach status 0x10000000, drei 312");
        check(s->clw0>=0&&s->clw2>=0&&s->cam_clr>=0&&claw0&&claw0->claw30c,"Handles clw0/clw2/cam_clr");
        // bhEff30c (effsub3.c:3549): mtx = owP[17].mtx * T(-pos.x, pos.y, pos.z) * Rxyz(ax, 32768 - ay, -az) von objP[10].
        const auto* ent=f.world.state(1,0);check(ent&&ent->enemy_calculated,"Tyrant-Baum");
        NJS_MATRIX m;std::copy_n(ent->enemy_owp_mtx.at(17).value,16,m);const auto& o=ent->enemy_obj.at(10);
        njTranslate(&m,-o.position.x,o.position.y,o.position.z);njRotateXYZ(&m,o.angles[0],32768-o.angles[1],-o.angles[2]);
        for(unsigned k=0;k<16;++k)check(std::fabs(m[k]-claw0->claw30c->mtx[k])<1e-4f,"bhEff30c: R0_WK.mtxP weicht von owP[17]*T*R ab");
        check(claw0->claw30c->obj==10&&claw0->claw30c->enemy_index==0,"bhEff30c: objP[10] des Tyrant");
        // Der Typ-1-Satz haelt cam.ofx auf 0, solange kein Wackelsatz dazukommt.
        f.camera={1,2,3};f.tick();check(f.camera.x==0&&f.camera.y==0&&f.camera.z==0,"bhEff30a Typ 1: cam.ofx/ofy/ofz = 0 je Pass");
        f.no_en19_gaps("Effektanlage");
    }
    {
        // Kamerawackel als Diagnosevorbedingung: bhSetRapEff(310, &e0aP[1]) wie Mv10 (en19.c:2257) direkt
        // ueber den gebundenen Dienst; der Typ-1-Satz (frueherer Platz) nullt zuerst, der Typ-2-Satz addiert
        // danach (Poolreihenfolge effect.c:776), also ist cam.ofx nach dem Pass ungleich 0 und klingt ab.
        Fixture f(data);f.tick();f.tick();
        int slot=-1;for(unsigned i=0;i<512;++i){const auto* sp=f.world.effect_state(i);if(sp&&(sp->flg&1u)&&sp->id==310)slot=int(i);}
        check(slot>=0,"Typ-1-Satz fehlt");
        const int shake=f.world.reserve_effect_slot(0);check(shake>slot,"der Wackelsatz liegt hinter dem Rueckstellsatz");
        auto& sp=const_cast<EffectSprite&>(*f.world.effect_state(unsigned(shake)));
        const EffectPma30a prm{2,16384,8192,1.0f,0.9f,8192,2.0f,0.94f}; /* Eff30aPrm2[1] */
        check(start_eff30a(sp,prm),"start_eff30a");
        bool moved=false;int life=0;
        for(int i=0;i<200;++i){f.tick();const auto* w=f.world.effect_state(unsigned(shake));if(f.camera.x!=0||f.camera.y!=0||f.camera.z!=0)moved=true;if(w&&(w->flg&1u))life=i+1;else break;}
        std::cout<<"  Wackeln: bewegt="<<moved<<" Lebensdauer="<<life<<" Paesse\n";
        check(moved,"bhEff30a Typ 2 veraendert cam.ofx/ofy/ofz");
        // Abklingen 2*0.94^n + 0.9^n < 0.01 erstmals bei n = 86 (geometrische Folge, nicht aus dem
        // Code): 85 Paesse lebt der Satz noch, der 86. gibt ihn frei (life zaehlt die lebenden).
        check(life==85,"bhEff30a: Freigabe im 86. Pass");
        f.tick();check(f.camera.x==0&&f.camera.y==0&&f.camera.z==0,"nach dem Wackeln haelt der Typ-1-Satz die Kamera wieder auf 0");
    }
    {
        // bhEne19_TyBloodSet -> bhEff303 (effsub3.c:1507) im echten Raumpool.
        // DIAGNOSEVORBEDINGUNG: RM_5840 traegt Typ 2 (status 0xE0000000) und damit kein
        // Bit 0x10000000; das setzt der Test vor dem ersten Pass, damit die Anlage den
        // 349-Satz wie beim Typ-1-Tyrant anlegt (en19.c:248). Danach laeuft die Kette
        // 349 -> recvx_en19_ty_blood_set -> 303 ohne weiteren Eingriff.
        Fixture f(data);auto& e=f.tyrant();auto* s=static_cast<RECVX_EN19_STATE*>(e.exp0);
        s->status|=0x10000000;
        f.tick();
        unsigned rueckruf=0;for(unsigned i=0;i<512;++i){const auto* sp=f.world.effect_state(i);if(sp&&(sp->flg&1u)&&sp->id==349)++rueckruf;}
        check(rueckruf==1,"bhEne19: status 0x10000000 legt genau einen 349-Satz an");
        // eb_time startet bei 0 (en19.c:249), also fordert der erste Rueckruf sofort ein 303 an.
        int slot=-1;for(unsigned i=0;i<512;++i){const auto* sp=f.world.effect_state(i);if(sp&&(sp->flg&1u)&&sp->id==303)slot=int(i);}
        check(slot>=0,"bhEne19_TyBloodSet: kein 303 im Raumpool");
        const auto* fleck=f.world.effect_state(unsigned(slot));
        check(fleck->type>=4u&&fleck->type<=7u,"bhEff303: type (rnd & 3) + 4 (en19.c:3637)");
        check(fleck->flg==0x200001u&&fleck->tex_id==5&&fleck->bl_src==8&&fleck->bl_dst==6,"bhEff303: flg/Gruppe/Blend im Raumpool");
        check(fleck->ax==16384&&fleck->mode0==1,"bhEff303: ax + 16384, mode0 1 nach dem ersten Pass");
        check(s->eb_time>=18&&s->eb_time<=33,"bhEne19_TyBloodSet: eb_time = SetPrm[0][0] + rnd (18..33)");
        // Der Fleck wachst und wird gezeichnet; in den naechsten Paessen kommt keiner dazu,
        // solange eb_time laeuft.
        const int wartet=s->eb_time;
        for(int i=0;i<3;++i)f.tick();
        unsigned flecken=0;for(unsigned i=0;i<512;++i){const auto* sp=f.world.effect_state(i);if(sp&&(sp->flg&1u)&&sp->id==303)++flecken;}
        check(flecken==1&&s->eb_time==wartet-3,"bhEne19_TyBloodSet :3654: eb_time zaehlt ab, kein zweiter Fleck");
        check(f.world.effect_state(unsigned(slot))->sx>0,"bhEff303: mode 1 laesst die Flaeche wachsen");
        check(f.world.effect_visible(unsigned(slot)),"bhEff303: der Fleck wird nicht gezeichnet (Gruppe 5 / ef_trs)");
        // Nach Ablauf von eb_time folgt der naechste Fleck.
        for(int i=0;i<wartet+1;++i)f.tick();
        flecken=0;for(unsigned i=0;i<512;++i){const auto* sp=f.world.effect_state(i);if(sp&&(sp->flg&1u)&&sp->id==303)++flecken;}
        check(flecken>=2,"bhEne19_TyBloodSet: nach eb_time kommt kein weiterer Fleck");
        f.no_en19_gaps("TyBloodSet");
    }
    {
        // bhEne19_SetDmgEffect Zeile 5 {2, 131072, 32, 298} -> bhEff_E00_AcidGenerator (effsub2.c:623)
        // ueber den ECHTEN Treffer: Granatwerfer mit Saeuremunition ist Waffe 15 (sub1.c:4489-4492,
        // Item 6 mit Flag 0x1000). VORBEDINGUNG: der geladene Werfer kommt per Originalaufnahme
        // acquire(6, n, 0x1000) ins Inventar und wird ausgeruestet — dasselbe, was ein Fund und
        // WeaponSet tun; kein Eingriff in Gegner- oder Trefferfelder. Danach zielt und feuert der
        // native CombatController (player_combat_core.c) Bild fuer Bild wie die Runtime.
        Fixture f(data);f.tick();auto& e=f.tyrant();auto* s=static_cast<RECVX_EN19_STATE*>(e.exp0);
        check(f.inv.acquire(6,6,0x1000),"Granatwerfer (Saeure) nicht aufgenommen");
        unsigned slot=0;for(unsigned i=1;i<12;++i)if(Inventory::id(f.inv.slots()[i])==6)slot=i;
        check(slot&&f.inv.equip(slot)&&f.inv.weapon()==15u,"WeaponSet: Saeure-Granatwerfer ist nicht Waffe 15");
        CombatController combat(data);
        const int hp_vor=e.hp;int slot298=-1;unsigned frame_hit=0;
        for(unsigned i=0;i<600&&slot298<0;++i){
            f.world.set_player_weapon(f.inv.weapon());f.world.set_player_way(combat.aim_yaw());f.world.set_player_attack(combat.attacking());
            f.tick();
            const CombatControls in{true,(i%40)==20,0,0,false};
            combat.tick(in,false,f.inv,f.player,f.actor,f.motion,f.frame,&f.world,f.audio,&f.events);
            for(unsigned k=0;k<512;++k){const auto* sp=f.world.effect_state(k);if(sp&&(sp->flg&1u)&&sp->id==298){slot298=int(k);frame_hit=i;break;}}
        }
        std::cout<<"  Saeuregranate: Schuesse="<<combat.shots<<" Treffer="<<combat.hits<<" hp "<<hp_vor<<"->"<<e.hp<<" comb_wep="<<e.comb_wep<<" 298-Bild="<<frame_hit<<'\n';
        // Stand 2026-10-16: der Granatwerfer (Waffen 14..17, Roadmap A05) hat keinen nativen
        // Kampfweg. Dann muss der Controller das benennen und es darf weder Schuss noch 298
        // geben. Sobald A05 portiert ist, entfaellt die Meldung und die volle Kette unten greift.
        const std::string a05="CombatController: Waffe 15 (WpnTab[15].flg 0x20000404) ohne nativen Kampfweg (weapon.c/player.c)";
        if(combat.unbound_routines().count(a05)){
            check(combat.shots==0&&e.hp==hp_vor&&slot298<0,"Waffe 15 ohne Kampfweg darf nichts ausloesen");
            std::cout<<"  OFFEN (A05): "<<a05<<'\n';
        }else{
        for(const auto& g:combat.unbound_routines())std::cout<<"  Kampfluecke: "<<g<<'\n';
        check(combat.shots>0,"der Kampfkern hat nicht geschossen");
        check(e.comb_wep==15u,"recvx_t3_calc_damage: comb_wep ist nicht Waffe 15 (en03_lib.c:1314)");
        check(slot298>=0,"bhEne19_SetDmgEffect Zeile 5: kein 298 nach echtem Saeuretreffer");
        const auto* quelle=f.world.effect_state(unsigned(slot298));
        check(quelle->type==32u,"bhEff_E00_AcidGenerator: type = effP[2] (32 Ausgaben)");
        check(std::fabs(quelle->sxb-2.0f)<1e-3f&&std::fabs(quelle->szb-2.0f)<1e-3f,"298: Skalierung 0.000015258789 * 131072 = 2.0");
        check(quelle->link_kind==1&&quelle->lkono==int(e.djnt_no),"298 :3482: bhSetEffectTb an ewP, Gelenk djnt_no");
        (void)s;
        // Die Quelle schiebt nach ihrer Wartezeit 257 nach (effsub2.c:663/:676).
        for(int i=0;i<8;++i)f.tick();
        unsigned saeure=0;for(unsigned k=0;k<512;++k){const auto* sp=f.world.effect_state(k);if(sp&&(sp->flg&1u)&&sp->id==257)++saeure;}
        check(saeure>=2,"298: nach der Wartezeit entstehen keine 257 im Raumpool");
        f.no_en19_gaps("SetDmgEffect 298 (echter Treffer)");
        }
    }
    std::cout<<"EN19 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
