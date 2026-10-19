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
#include <set>
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
    std::vector<const EffectSprite*> effects(unsigned id){std::vector<const EffectSprite*> out;
        for(unsigned i=0;const auto* s=world.effect_state(i);++i)if((s->flg&1u)&&s->id==id)out.push_back(s);return out;}
    // EN17 hat auf den hier gelaufenen Wegen keine offene Luecke mehr; die frueher
    // zugelassenen bhEff369/370/301 sind portiert (effect_sprites.cpp:1073 bzw. die
    // Prozedurtabelle), ihre Namen kamen aus einem aelteren Stand.
    void no_gaps(const char* phase){
        for(const auto& g:world.unbound_enemy_routines())throw std::runtime_error(std::string(phase)+": Luecke "+g);
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
        // bhEff_SetPtcl (365, Staub der Tritte), bhEne17_AfterimageAxEffect (364, Beil-Nachbild),
        // bhEff_SetPtcl2V (366 type 1) und bhEne_SetBlood2(plp) (5) auf dem natuerlichen Weg.
        int saw365=0,saw364=0,saw366v=0,saw_blood=0;
        std::uint32_t afterimage_cols=0;
        for(int i=0;i<600;++i){f.tick();
            if(e.mode2==3||e.mode2==4||e.mode2==5)moved=true;
            if(e.mtn_no==15||e.mtn_no==16||e.mtn_no==11||e.mtn_no==2)attacked=true;
            for(const auto* s:f.effects(365)){++saw365;
                check(s->extra!=nullptr&&s->ct3>=-1&&s->ct3<=20,"bhEff_SetPtcl1: Hilfsarbeitssatz und ct3 0..20");
                check(s->tex_id==8,"bhEff_SetPtcl(epw, pos, 8): tex_id 8");}
            for(const auto* s:f.effects(364)){++saw364;
                check(s->flr_no==1&&s->mdlver==1&&s->model_mdl==5&&(s->stflg&0x20u)&&s->tex_id==0,"Nachbild: flr_no 1, mdlver 1, mdl[5], stflg 0x20, tex_id 0");
                check((s->tv[0].col&0x00FFFFFFu)==0x00FFFFFFu,"Nachbild: tv[0].col = (i << 28) + 0x20FFFFFF");
                afterimage_cols|=1u<<((s->tv[0].col>>28)&0xFu);}
            for(const auto* s:f.effects(366))if(s->type==1){++saw366v;
                check(s->extra!=nullptr&&s->bl_src==8&&s->bl_dst==3,"bhEff_SetPtcl2V: Hilfsarbeitssatz, Blend 8/3");
                check(s->tv[0].col==0xFF600000u||s->ct1>4,"bhEff_Sub366 type 1: Eckfarbe 0xFF600000 (keine Verschiebung)");
                if(s->mode2==1)check(s->quads.size()==16,"bhEff_SetPtcl2V: sechzehn Punkte");
                // Erster Bewegungspass: poly[0] - poly[2] ist add_vec + (0, g, 0) mit g = -0.2.
                // njUnitVector(dv) normiert die Richtung, der Betrag ist deshalb spd aus [1, 3].
                if(s->mode0==2&&s->ct1==1)for(const auto& q:s->quads){
                    if(q.vertices[0].y==0.0f)continue;               // am Boden abgeschnitten (:1960)
                    const float dx=q.vertices[0].x-q.vertices[2].x,dy=(q.vertices[0].y-q.vertices[2].y)+0.2f,dz=q.vertices[0].z-q.vertices[2].z;
                    const float len=std::sqrt(dx*dx+dy*dy+dz*dz);
                    check(len>0.99f&&len<3.01f,"bhEff_SetPtcl2V: Betrag der Startgeschwindigkeit 1..3 (njUnitVector)");}}
            for(const auto* s:f.effects(5))if(s->link_kind==0){++saw_blood;
                // bhEne_SetBlood2(pl, 2, &ps, 0) nach pl->djnt_no = 3 (en17.c:1653): Gelenk 3.
                check(s->lkono==3&&s->type==2,"bhEne_SetBlood2(plp): lkono 3 (djnt_no), type 2");}
            // bhEne_SetVibration(1) = StartVibrationEx(1, 0xA) (zonzon.c:1357).
            for(const auto& v:f.world.take_vibration_requests())check(v.first==1&&v.second==10,"bhEne_SetVibration(1): StartVibrationEx(1, 0xA)");
            RECVX_ENEMY_PLAYER q{};f.world.enemy_player_get(q);
            if(q.hp<hp0){hit=true;break;}
        }
        check(saw365>0,"bhEff_SetPtcl: kein 365-Satz im Angriff");
        check(saw364>0,"bhEne17_AfterimageAxEffect: kein 364-Satz in Motion 15/16");
        // Die sieben Nachbilder eines Bildes tragen 0x20FFFFFF + (i << 28), i = 0..6: Alpha 0x2..0x8.
        check((afterimage_cols&0x1FCu)==0x1FCu,"Nachbild: sieben verschiedene Alphastufen");
        check(saw366v>0,"bhEff_SetPtcl2V: kein 366-Satz am Spielertreffer");
        check(saw_blood>0,"bhEne_SetBlood2(plp): kein Bluteffekt am Spieler");
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
    {
        // Klingentreffer (en17.c:385): das Bit 0x20000000 in exp0 setzt der Waffenpfad
        // (recvx_en17_blade_hit) — hier als Diagnosevorbedingung. Danach vier bhEff_SetPtcl2
        // an der Klingenspitze, rom->lgtp[2] und die Stimme.
        Fixture f(data);f.tick();auto& e=f.steve();e.stflg&=~0x01000000u;f.tick();f.tick();
        const std::size_t before=f.effects(366).size();
        recvx_en17_blade_hit(&e);f.tick();
        // Die Klingenspitze des Passes, in dem der Zweig lief (exp2 owP[1], (0,0,-17.5)).
        RECVX_ENEMY_VEC tip{};check(recvx_en17_blade_tip(&e,&tip),"Klingenspitze (exp2 owP[1])");
        auto sparks=f.effects(366);
        std::size_t fresh=0;for(const auto* s:sparks)if(s->type==0)++fresh;
        check(fresh>=4,"bhEff_SetPtcl2: vier Funkensaetze am Klingentreffer");
        check(sparks.size()>=before+4,"bhEff_SetPtcl2: Saetze wurden angelegt");
        for(const auto* s:sparks)if(s->type==0){
            check(s->extra!=nullptr&&s->bl_src==8&&s->bl_dst==10,"bhEff_Sub366 type 0: Hilfsarbeitssatz, Blend 8/10");
            check(s->mode0==2&&s->mode2==1&&s->ct1==1,"bhEff_Sub366: ct3 0 laeuft im ersten Pass bis mode0 2 durch");
            // :1906 tv[0..1].col = -0xC0, :1979 der erste Pass zieht 0x800 bzw. 0x2000 ab.
            check(s->tv[0].col==0xFFFFFF40u-0x800u&&s->tv[1].col==0xFFFFFF40u-0x2000u,"bhEff_Sub366 type 0: Eckfarben");
            check(s->quads.size()==16,"bhEff_LineDraw: sechzehn Dreiecke");
            const auto& q=s->quads.front();
            check(q.pn==3&&!q.textured&&q.gouraud&&q.world_space&&!q.fogged&&q.source==8&&q.destination==10,
                  "bhEff_LineDraw: njDrawPolygon3DEx(poly, 3, 1) ohne Textur/Nebel, eigene Eckfarben, Blend 8/10");
            check(q.vertices[1].x==q.vertices[0].x+0.2f&&q.vertices[1].y==q.vertices[0].y+0.3f&&q.vertices[1].z==q.vertices[0].z+0.2f,
                  "bhEff_LineDraw: poly[1] = poly[0] + (0.2, 0.3, 0.2)");
            check(q.vertices[0].col==s->tv[0].col&&q.vertices[1].col==s->tv[0].col&&q.vertices[2].col==s->tv[1].col,
                  "bhEff_LineDraw: Eckfarben tv[0], tv[0], tv[1]");
            break;}
        // en17.c:1852 bhEne17_SetLight an rom->lgtp[2].
        const auto light=f.world.light_state(2);
        check(light.has_value(),"rom->lgtp[2] fehlt");
        check(light->flags==3u&&light->type==101u&&light->speed==8u,"SetLight: flg 3, type 101, aspd 8");
        check(light->color.x==6.0f&&light->color.y==6.0f&&light->color.z==6.0f,"SetLight: rgb 6");
        check(light->near_range==10.0f&&light->far_range==80.0f,"SetLight: nr 10, fr 80");
        // lkflg/lkno/lkono und mode schreibt SetLight auf 0; ct0 hat der Lichtpass desselben
        // Bildes danach schon weitergezaehlt (Typ 101 mit aspd 8).
        check(light->link==0u&&light->index==0u&&light->node==0u&&light->mode==0u,"SetLight: lkflg/lkno/lkono/mode 0");
        // Der Armwork laeuft im Gegnerpass nach Steve und rechnet seine owP danach neu; die
        // oben gelesene Spitze ist deshalb eine Armaktualisierung neuer als die des Zweigs.
        check(std::fabs(light->position.x-tip.x)<1.0f&&std::fabs(light->position.y-tip.y)<1.0f&&std::fabs(light->position.z-tip.z)<1.0f,"SetLight: Lage an der Klingenspitze");
        f.no_gaps("Klingentreffer");
        // Das Beil-Nachbild zeichnet objP[1] von epw->mdl[5]; flr_no 1 faerbt die Materialien mit
        // tv[0].col (npSetAllMatColor), die sieben Nachbilder also mit Alpha 0x20..0x80 auf 0xFFFFFF.
        e.type=0;e.mtn_no=16;e.frm_no=20*65536;f.tick();
        check(f.effects(364).size()>=7,"Nachbild: sieben Saetze je Bild");
        f.tick();                       // erst der Effektpass danach reiht sie in sys->ef_fnc ein
        auto shots=f.effects(364);check(shots.size()>=7,"Nachbild: Saetze im zweiten Pass");
        {
            auto draw=[&]{Scene out;f.world.append(out,{},SceneCamera{});return out;};
            const Scene with=draw();
            for(const auto* b:shots)const_cast<EffectSprite*>(b)->stflg|=0x1000000u;   // :3319 verdeckt den Zeichenaufruf
            const Scene without=draw();
            for(const auto* b:shots)const_cast<EffectSprite*>(b)->stflg&=~0x1000000u;
            check(with.triangles.size()>without.triangles.size(),"bhEff_ModelDraw: Nachbild zeichnet keine Dreiecke");
            unsigned lit=0;std::set<unsigned> shades;
            for(const auto& t:with.triangles)if(t.light_category==4){++lit;
                const unsigned alpha=(t.color>>24)&0xFFu;
                if((t.color&0x00FFFFFFu)==0x00FFFFFFu&&alpha>=0x20u&&alpha<=0x80u)shades.insert(alpha);}
            check(lit>0,"bhEff_ModelDraw: keine ef_fnc-Modelldreiecke");
            // Jedes der sieben Nachbilder faerbt sein Modell mit dem eigenen argb: mehrere
            // Alphastufen zugleich im Bild. Ohne npSetAllMatColor waere es nur die eine
            // Materialfarbe des Modells.
            check(shades.size()>=2,"npSetAllMatColor: die Nachbilder tragen nur eine Materialfarbe");
        }
        f.no_gaps("Nachbild");
    }
    std::cout<<"EN17 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
