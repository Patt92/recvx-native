// EN06 Moth im Originalraum RM_6030, echte RoomWorld-Dienste und das echte
// Raumskript: Ereignis 16 haelt Motte 3 in Common_controll 25 (mode0 5) und
// gibt sie frei, sobald Raumflag 4/0 steht (WHILE !flag, dann 0x24/0x69 17/
// 0x67 8f). Das Setzen dieses Flags, die Spielerlagen und der Treffer sind
// Diagnosevorbedingungen, kein natuerlich gespielter Fortschritt.
// Semantikreferenz en06.c, en06sub.c, effsub2.c:1839.
#include "world.h"
#include "room.h"
#include "en06.h"
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
    explicit Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_6030.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_6030"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
        // room.c:997-1009 bhStartDoorDemo Fall 1 im Bild nach dem Raumladen: sp_flg = -1 (runtime.cpp room_handover).
        events.flags[6][0]=0xffffffffu;
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){events.tick();world.set_simulation_flags(events.flags[6][0]);world.tick();commit();
        // Auch den Zeichenpfad pruefen: reine Simulation erreicht die Atlaspruefung nicht.
        Scene out;world.append(out,{},scene.cameras.at(0));}
    RECVX_ENEMY_WORK& work(unsigned index){auto& t=world.enemy_table();check(index<t.ewk_n&&t.slot[index],"ene[index] fehlt");return *t.slot[index];}
    std::vector<const EffectSprite*> effects(unsigned id){std::vector<const EffectSprite*> out;for(unsigned i=0;;++i){const auto* s=world.effect_state(i);if(!s)break;if((s->flg&1u)&&s->id==id)out.push_back(s);}return out;}
    unsigned count(unsigned id,unsigned flg_mask=1u){unsigned n=0;auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n;++i)if(t.slot[i]&&(t.slot[i]->flg&flg_mask)&&t.slot[i]->id==id)++n;return n;}
    void no_gaps(const char* phase){
        // effsub2.c:1932: der Mittelpunkt der Staubwolke kommt aus nicht gesetzten FPU-Registern (Luecke benannt).
        static const char* allowed[]={"bhEff_E06_Rinpun: op->cspr.c"};
        for(const auto& g:world.unbound_enemy_routines()){bool ok=false;for(const char* a:allowed)if(g.find(a)!=std::string::npos)ok=true;
            if(!ok)throw std::runtime_error(std::string(phase)+": Luecke "+g);}
    }
    // Ereignis 16: Raumflag 4/0 beendet das WHILE vor der Freigabe von Motte 3.
    void release(){events.set_flag(4,0,0);}
};
static const WorldEntity& tree(const RECVX_ENEMY_WORK& w){const auto* e=static_cast<const WorldEntity*>(w.owner);return e->model_owner?*e->model_owner:*e;}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // bhEne06_Init (en06.c:272) im Raumladen fuer die drei Motten (Index 3/4/5).
        Fixture f(data);
        auto& m3=f.work(3);auto* p=recvx_en06_exp0(&m3);
        check(m3.id==6&&p&&m3.exp1&&m3.exp2,"Init: exp0/exp1/exp2");
        check((m3.flg&0x100800u)==0x100800u&&!(m3.flg&0x1006u)&&(m3.flg&0x8000u) /* 0x8000 setzt 0x24 (inaktiv) danach */&&m3.ar==5.0f&&m3.ah==1.0f&&m3.cah==3.0f&&m3.hp==1,"Init: flg 0x100000|0x800, ar/ah, hp 1");
        check(m3.clp_jno[0]==11&&m3.clp_jno[3]==14&&m3.clp_jno[4]==1&&m3.clp_jno[5]==2&&m3.clp_jno[6]==-1&&m3.cpcl&&m3.cpcl[1].z==-6,"Init: clp_jno, CapColTab");
        for(int i=0;i<14;++i){const auto* c=p->parts[i];check(c&&c->id==31&&c->lkwkp==&m3&&c->lkono==i&&(c->mdflg&1u)&&c->mode0==2&&c->exp0,"Fragment: id 31, lkwkp/lkono, Init -> Dummy");}
        const auto* leader=static_cast<RECVX_ENEMY_WORK*>(m3.exp2);
        check(leader->id==6&&(leader->flg&0x01008001u)==0x01008001u&&(leader->mdflg&0x21u)==0x21u&&leader->exp1==m3.exp1,"Anfuehrer: flg 0x01008001, mdflg 0x21, geteiltes exp1");
        // Raumladen in Reihenfolge: Motte 3 findet sich selbst und legt exp1/Anfuehrer an, 4 und 5
        // finden Motte 3 zuerst und teilen beides (en06.c:316-345).
        check(f.work(4).exp1==m3.exp1&&f.work(5).exp1==m3.exp1&&f.work(4).exp2==m3.exp2&&f.work(5).exp2==m3.exp2&&f.count(6)==4,"Init: drei Motten, ein Anfuehrer");
        check(p->shadow>=0&&f.world.effect_state(unsigned(p->shadow))->mdlver==1&&p->car_target==3.0f&&p->ar_target==5.0f,"Init: Schatten mdlver 1, EXP0_F(0x6C/0x70)");
        const auto& t=tree(m3);
        check(!(t.enemy_evalflags.at(t.model_window+1)&8u)&&!(t.enemy_evalflags.at(t.model_window+2)&8u),"Init: objP[1/2].evalflags &= ~8");
        check(m3.mode0==5&&m3.mode1==2,"Raumskript: Common_controll 25 haelt Motte 3 (mode0 5)");
        f.no_gaps("Init");

        // Freigabe durch das Raumskript, dann Flug MV02 mit Schuppenstaub und Anlehnen MV08.
        f.release();
        bool flying=false,dust=false,leaned=false;
        for(int i=0;i<600&&!leaned;++i){f.tick();
            if(m3.mode0==1&&m3.mode2==2&&(m3.flg&0x400118u)==0x400118u)flying=true;
            if(!dust)for(const auto* s:f.effects(260)){dust=true;check(s->type==0&&(s->stflg&0x20u)&&s->tex_id>=1&&s->tex_id<=4&&(s->flg&0x20000000u)&&s->bl_src==8&&s->bl_dst==3,"260: stflg 0x20, tex_id 1..4, Punktfilter, 8/3");}
            if(m3.mode2==0&&flying&&m3.mtn_no==4u){leaned=true;
                check(f.world.effect_state(unsigned(p->shadow))->sy==2.0f,"MV08: Schatten sy -> 2");}
        }
        check(flying&&dust&&leaned,"Freigabe -> MV02 -> Staub -> MV08 -> MV00 nicht vollstaendig");
        f.no_gaps("Flug");
    }
    {
        // Host-Negativtest: absichtlich falscher Texturframe, keine Aenderung
        // an Spieldaten. Der Zeichenfehler darf die Simulation nicht beenden.
        Fixture f(data);f.release();
        EffectSprite* dust=nullptr;
        for(int i=0;i<600&&!dust;++i){
            f.tick();
            for(const auto* s:f.effects(260))if(!s->quads.empty()){
                dust=const_cast<EffectSprite*>(s);break;
            }
        }
        check(dust,"Warnpfad: kein zeichnender Mottenstaub erreicht");
        check(f.world.render_warnings().empty(),"Regulaerer Mottenstaub erzeugt bereits eine Darstellungswarnung");
        const auto saved=dust->quads;
        const auto flags=dust->flg;
        const int hp=f.inv.hp;
        for(auto& q:dust->quads)if(q.textured)q.frame=999999u;
        Scene out;f.world.append(out,{},f.scene.cameras.at(0));
        check(!f.world.render_warnings().empty(),"Ungueltiger Texturframe wurde nicht als Warnung gemeldet");
        bool identified=false;
        for(const auto& warning:f.world.render_warnings())
            if(warning.find("effect=260")!=std::string::npos&&warning.find("frame=999999")!=std::string::npos)identified=true;
        check(identified,"Warnung nennt Effekt und fehlerhaften Frame nicht");
        check(dust->flg==flags&&f.inv.hp==hp,"Zeichenfehler hat Effektaktivitaet oder Spieler-HP veraendert");
        const auto warning_count=f.world.render_warnings().size();
        Scene again;f.world.append(again,{},f.scene.cameras.at(0));
        check(f.world.render_warnings().size()==warning_count,"Identischer Zeichenfehler erzeugt doppelte Warnungen");
        dust->quads=saved;
        for(int i=0;i<30;++i)f.tick();
        check(f.world.render_warnings().size()==warning_count,"Weiterlaufen nach Zeichenfehler erzeugt neue Warnungen");
    }
    {
        // Griff (BR00 :398 -> NG00 :1079) mit Anfuehrer-PlayerControl (:2112) und Mottenei (:1161).
        Fixture f(data);auto& m=f.work(3);auto* p=recvx_en06_exp0(&m);f.release();
        const auto* leader=static_cast<RECVX_ENEMY_WORK*>(m.exp2);
        bool grabbed=false,player_motion=false,egg=false,released=false;int grab_tick=-1;
        for(int i=0;i<1200&&!released;++i){
            if(!grabbed&&m.mode0==1&&m.mode2==2&&(m.flg&0x400000u)){
                // Spieler 5 vor der Motte, 15 darunter, gleiche Blickrichtung.
                const float px=m.px-5.0f*njSin(m.ay),pz=m.pz-5.0f*njCos(m.ay);
                f.actor.position={px,m.py-15.0f,pz};f.actor.yaw=Angle(m.ay);
                RECVX_ENEMY_PLAYER q{};check(f.world.enemy_player_get(q),"Spielerbesitzer fehlt");q.pos={px,m.py-15.0f,pz};q.ay=m.ay;q.flg&=~4u;
                check(f.world.enemy_player_set(q),"Spieler setzen");f.commit();
            }
            const bool before=m.mode0==1;
            f.tick();
            RECVX_ENEMY_PLAYER pl{};f.world.enemy_player_get(pl);
            // BR00 :416: der Griff greift bei EXP0_F(0) < 15 — hier mit dem Spieler 5 voraus, nicht erst darunter.
            if(before&&m.mode0==2)check(p->dist>3.0f&&p->dist<15.0f,"BR00: Griff im Abstand < 15 (Spieler 5 voraus)");
            if(!grabbed&&m.mode0==2&&m.mode3==1){grabbed=true;grab_tick=i;
                check((static_cast<RECVX_EN06_EXP1*>(m.exp1)->flags&1)&&pl.mode0==4&&(pl.flg&0x10004u)==0x10004u&&m.mtn_no==6u,"NG00: exp1 Bit 1, plp mode0 4, flg 0x10004, mtn 6");}
            if(grabbed&&!player_motion&&pl.mode0==4&&pl.mode3==1){player_motion=true;
                check(pl.mtn_enemy==1&&pl.mtn_enemy_work==leader&&pl.mtn_no==20u&&pl.hokan_rate==52428,"PlayerControl: plp->mnwP = Anfuehrer, mtn 20");}
            if(grabbed&&!egg&&f.count(27)){egg=true;
                const unsigned marks=pl.stflg&0x38000000u;check(marks==0x8000000u||marks==0x10000000u||marks==0x20000000u,"NG00: eine stflg-Marke fuer das Ei");}
            if(player_motion&&pl.mode0==1){released=true;
                check(!(static_cast<RECVX_EN06_EXP1*>(m.exp1)->flags&1)&&pl.mtn_enemy==0&&(pl.flg&8u)&&!(pl.flg&0x10004u)&&!(pl.stflg&0x10000u),"PlayerControl: Freigabe am Motionende");}
        }
        check(grabbed&&player_motion&&egg&&released,"Griff -> Spielermotion -> Ei -> Freigabe nicht vollstaendig");
        check(m.mode0==1&&(m.mode2==5||m.mode2==2),"NG00-Ende: MV05 Rueckflug");
        (void)grab_tick;
        // Das abgelegte Ei laeuft als EN27 auf plp->mdl[6] (en06.c:1182-1184: type, mode2 1).
        {auto& t=f.world.enemy_table();bool egg_init=false;for(unsigned k=0;k<t.ewk_n;++k)if(t.slot[k]&&t.slot[k]->id==27&&(t.slot[k]->flg&1u)&&t.slot[k]->lk_player&&t.slot[k]->mtn_no==93u+t.slot[k]->type)egg_init=true;
         check(egg_init,"Ei: bhEne27_Init am Spieler");}
        f.no_gaps("Griff");
    }
    {
        // Schwerer Treffer im Flug (dam > 30): Damage :1195 -> DD03 :1729, zwoelf Fragmente fallen auf owP.
        Fixture f(data);auto& m=f.work(3);auto* p=recvx_en06_exp0(&m);f.release();
        for(int i=0;i<300&&!(m.mode0==1&&m.mode2==2);++i)f.tick();
        check(m.mode0==1&&m.mode2==2,"Flug erreicht");
        m.flg|=4u;m.dam[3]=40;m.djnt_no=3;m.wpnr_no=0;m.comb_flg=0x10u;m.dvx=0;m.dvy=0;m.dvz=1;
        f.tick();
        const auto& t=tree(m);
        check(m.mode0==4&&m.mode2==3&&m.hp<0&&(m.stflg&8u)&&(m.flg&0x8000u)&&!(m.flg&0x78u)&&m.mtn_add==0,"DD03: mode0 4/mode2 3, stflg 8, flg 0x8000");
        check((t.enemy_evalflags.at(t.model_window+1)&8u)&&(t.enemy_evalflags.at(t.model_window+2)&8u),"DD03: objP[1/2].evalflags |= 8");
        for(int i=0;i<12;++i){const auto node=t.model_window+unsigned(recvx_en06_broken_parts[i]);
            check((t.enemy_owp_flg.at(node)&3u)==3u,"DD03: owP[BrokenParts].flg |= 3");
            check(p->parts[i]->mode0==1&&p->parts[i]->mode2==(i<4?1:0),"DD03: Fragment mode0 1, mode2 (i < 4)");}
        check(p->parts[12]->mode0==2&&p->parts[13]->mode0==2,"Fragmente 12/13 bleiben Dummy");
        const auto node0=t.model_window+unsigned(recvx_en06_broken_parts[0]),node5=t.model_window+unsigned(recvx_en06_broken_parts[5]);
        const float y0=t.enemy_owp_mtx.at(node0).value[13],y5=t.enemy_owp_mtx.at(node5).value[13];
        for(int i=0;i<12;++i)f.tick();
        const auto* wing=p->parts[0];const auto* body=p->parts[5];
        check(wing->py<y0&&body->py<y5,"Fragmente fallen");
        check(t.enemy_owp_mtx.at(node0).value[13]==wing->py&&t.enemy_owp_mtx.at(node5).value[13]==body->py,"epw->mtx = owP[BrokenParts[lkono]] des Elternworks");
        // MV00 Fall 2 (en06sub.c:124-165): acht Schritte ang/ct3 legen die Hochachse auf die Bodennormale.
        bool landed=false;for(int i=0;i<200&&!landed;++i){f.tick();if(body->mode3==2&&body->ct3==0)landed=true;}
        check(landed,"MV00: Rumpfstueck landet und kippt in acht Schritten");
        {NJS_MATRIX mm;std::memcpy(mm,t.enemy_owp_mtx.at(node5).value,sizeof mm);const auto* bx=static_cast<const RECVX_EN06S_EXP0*>(body->exp0);
         NJS_VECTOR n{bx->n[0],bx->n[1],bx->n[2]},vd,ov;njCalcVector(&mm,&n,&vd);if(vd.y<0){vd.x=-vd.x;vd.y=-vd.y;vd.z=-vd.z;}
         const int rest=njArcSin(njOuterProduct(&vd,&n,&ov));
         check(n.y>0.9f&&rest<64&&body->py==t.enemy_owp_mtx.at(node5).value[13],"MV00: Hochachse auf der Bodennormalen");}
        f.no_gaps("DD03");
    }
    std::cout<<"EN06 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
