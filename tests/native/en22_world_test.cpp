// EN22 Albinoid Adult im Originalraum RM_7100, echte RoomWorld-Dienste.
// Spielerlagen/-boden und das Freigeben des Werks ueber 0x24 sind
// Diagnosevorbedingungen, kein natuerlich gespielter Fortschritt.
// Semantikreferenz en22.c, effsub4.c:837/958/1774.
#include "world.h"
#include "room.h"
#include "binary_view.h"
#include "en22.h"
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
    explicit Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_7100.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_7100"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    // 0x24 (kind 1, Satz 0, an): der Raum gibt den Albinoid frei.
    void activate(){EventCommand c;c.opcode=0x24;c.bytes={0x24,1,0,1};check(world.bind(c),"0x24 nicht gebunden");}
    RECVX_ENEMY_WORK& albinoid(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==22)return *t.slot[i];throw std::runtime_error("Original-Albinoid fehlt");}
    std::vector<const EffectSprite*> effects(unsigned id){std::vector<const EffectSprite*> out;for(unsigned i=0;;++i){const auto* s=world.effect_state(i);if(!s)break;if((s->flg&1u)&&s->id==id)out.push_back(s);}return out;}
    void player_floor(int floor){RECVX_ENEMY_PLAYER q{};check(world.enemy_player_get(q),"Spielerbesitzer fehlt");q.flr_no=std::uint8_t(floor);check(world.enemy_player_set(q),"Spieler setzen");}
    void no_gaps(const char* phase){
        // Die fruehere Raumluecke von RM_7100 (bhObj005 Nachbarzelle) ist nach QUALITY.md behoben.
        for(const auto& g:world.unbound_enemy_routines())throw std::runtime_error(std::string(phase)+": Luecke "+g);
    }
};
// epw->mdl[k].objP[1].model->plist im Strom des Works (wie die Bindung model_plist).
static std::uint32_t material_word(const RECVX_ENEMY_WORK& e,unsigned k,int no,int* head=nullptr){
    const auto* ent=static_cast<const WorldEntity*>(e.owner);const auto& tree=ent->model_owner?*ent->model_owner:*ent;
    const auto at=tree.live_models.find(k);check(at!=tree.live_models.end()&&!at->second.empty(),"mdl[k]: beschreibbarer Strom fehlt");
    const auto* asset=tree.enemy_asset->model(k);check(asset&&asset->bones()>1,"mdl[k].objP[1] fehlt");
    const auto& stream=at->second;BinaryView v(stream);const auto base=v.u16(4);
    const auto objects=v.relative(base,v.u32(12),asset->bones()*52);
    const auto model=v.relative(base,v.u32(objects+52+4),24);const auto plist=v.relative(base,v.u32(model+4),2);
    std::size_t color=0;int h=0;check(recvx_en22_material_chunk(stream.data()+plist,stream.size()-plist,no,&color,&h)!=0,"Materialchunk no fehlt");
    if(head)*head=h;std::uint32_t word;std::memcpy(&word,stream.data()+plist+color,4);return word;
}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // recvx_en22_material_chunk (en22.c:2416): 64..66/56..58 mit Groessenwort, 8 mit Zusatzwort,
        // 17..23 gezaehlte Materialien (Farbe hinter dem Groessenwort), 255 endet.
        const std::uint8_t plist[]={8,0,0x11,0, 17,0,2,0,1,2,3,4,5,6,7,8, 64,0,1,0,9,9, 23,0,2,0,0xAA,0xBB,0xCC,0xDD,0,0,0,0, 255,0};
        std::size_t at=0;int head=0;
        check(recvx_en22_material_chunk(plist,sizeof plist,0,&at,&head)&&at==8&&head==17,"Material 0 hinter Kopf 8");
        check(recvx_en22_material_chunk(plist,sizeof plist,1,&at,&head)&&at==26&&head==23,"Material 1 hinter Streifen 64");
        check(!recvx_en22_material_chunk(plist,sizeof plist,2,&at,&head),"Endchunk 255");
    }
    {
        // bhEne22_Init (en22.c:911) im Raumladen; 0x24 haelt das Werk inaktiv, bis der Raum es freigibt.
        Fixture f(data);auto& e=f.albinoid();auto* p=recvx_en22_exp0(&e);
        check(p&&e.mode0==1&&e.mode1==1&&e.mode2==0&&e.mode3==0,"Init: mode 1/1/0/0");
        check(e.hp==250&&e.ar==7.0f&&e.ah==6.0f&&e.car==7.0f&&e.cah==6.0f,"Init: hp 250, ar/ah/car/cah");
        check((e.flg&0x878u)==0x878u&&!(e.flg&2u)&&e.lok_jno==2&&e.clp_jno[0]==-1&&(e.mdflg&0x20u),"Init: flg 0x78|0x800, lok_jno 2, clp_jno, mdflg 0x20");
        check((p->flags&0x41u)==0x41u&&p->light_effect==-1,"Init: EXP0_I(0x8) |= 0x41, exp0+0x4C NULL");
        check(e.stflg&0x1000000u,"Raumskript: Albinoid inaktiv");
        f.tick();check(e.mode3==0,"inaktiv: kein Move");

        // Erster Move: shock_wait 0 -> SetElectricShockEffect(3) (fuenf 354), SetLight(4, (0,5,0), 0).
        f.activate();f.tick();
        const auto arcs=f.effects(354);check(arcs.size()==5,"ShockEffect 3: fuenf 354");
        for(const auto* s:arcs){
            check(s->link_kind==1&&(s->flg&0x80u)&&(s->stflg&0x20u)&&s->tex_id==6&&s->txp_category==0x80000000u&&s->zn==1.5f,"354: am Werk, txp[0] = mlwP->texP, tex_id 6, zn 1.5");
            check(s->bl_src==8&&s->bl_dst==10&&s->mode1==0,"bhEff_Sub354 mode0 0: Blend 8/10, mode1 = type 0");}
        check(p->shock_wait>=59&&p->shock_wait<=88&&p->hit_window==14,"Move: shock_wait rand % 30 + 60, hit_window 15 (je - 1)");
        auto l=f.world.light_state(2);
        check(l&&l->flags==3u&&l->type==0u&&l->link==2u&&l->index==0u&&l->node==3u,"SetLight: lgtp[2] flg 3, type 0, lkflg 2, lkno idx_ct, lkono 3");
        check(l->mode==1u&&l->speed==8u&&l->clock==2048u&&l->near_range==20.0f&&l->far_range==40.0f&&l->color.z==0.0f,"CtrLight Modus 0 -> 1: aspd 8, ct0 += 2048, rgb = sin(0)");
        check(p->flags&0x80000u,"SetLight: EXP0_I(0x8) |= 0x80000");
        f.no_gaps("Init/Move");

        // Spieler im Wasser (flr 0) und weiter als 40: Brain00 -> MV04 (mtn 9) mit bhEne22_SparkEffect.
        bool colors0=false,light353=false,models=false,ramp=false,end=false;
        for(int i=0;i<300&&!end;++i){f.player_floor(0);f.tick();
            if(e.mtn_no!=9)continue;const int frm=e.frm_no/65536;
            if(frm==1&&!colors0){ // SparkEffect frm 0 lief im Pass davor: ChgDengekiColor(0xB2B2B2) auf 4i/4i+1.
                unsigned written=0;
                // Die Blitzmodelle tragen 40 Materialchunks: no >= 40 erreicht den Endchunk und kehrt ohne Schreiben zurueck.
                for(unsigned k=1;k<=6;++k)for(int i2=0;i2<10;++i2)for(int d=0;d<2;++d){int head=0;const auto w=material_word(e,k,4*i2+d,&head);
                    if(head==17||head==21||head==19||head==23){++written;check(w==0x00B2B2B2u,"ChgDengekiColor: B,G,R,A von 0xB2B2B2");}}
                check(written>0,"ChgDengekiColor: kein beschriebenes Material");
                check(p->light_effect==-1&&p->spark_step==0,"SparkEffect frm 0: exp0+0x4C NULL, EXP0_I(0x18) 0");colors0=true;}
            if(frm==12&&!light353){const auto lights=f.effects(353);
                check(lights.size()==1&&lights[0]->link_kind==1&&lights[0]->tex_id==5&&(lights[0]->stflg&0x20u)&&lights[0]->ax==16384&&p->light_effect>=0&&(p->flags&0x1000u),"frm 11: bhEne22_SetElectricLightEffect (353, tex_id 5, ax 16384)");
                l=f.world.light_state(2);check(l&&l->mode==3u&&l->speed==4u&&l->color.z==0.0f,"SetLight(type 1): Modus 2 -> 3, aspd 4");light353=true;}
            if(frm==31&&!models){const auto bolts=f.effects(364);check(bolts.size()==8,"SetDengekiEffect2: acht 364");
                for(const auto* s:bolts)check(s->model_enemy==0&&s->model_mdl>=1&&s->model_mdl<=6&&s->mdlver==1&&s->flr_no==0&&(s->stflg&0x20u)&&s->tex_id==0&&s->ax==0&&s->az==0&&s->type==1,"364: mlwP = &epw->mdl[1..6], mdlver 1, flr_no 0");
                for(std::size_t k=1;k<bolts.size();++k)check(((bolts[k]->ay-bolts[k-1]->ay)&0xFFFF)==8192,"364: ay je + NJM_DEG_ANG(45)");
                // ulpMatAdrTbl (en22.c:2386): ein Wort an no = (2i + 20) % 40; Schritt 0 -> argb[0] = eff_col[0].
                for(unsigned k=1;k<=6;++k){check(material_word(e,k,20)==0x60B2B2FFu&&material_word(e,k,21)==0x60B2B2FFu,"GetDengekiColorAddr: argb[0] = 0x60B2B2FF");
                    check(material_word(e,k,22)==0x00B2B2FFu,"argb[1] = 0xB2B2FF");}
                // effsub4.c:3306 bhEff_ModelDraw: die acht Blitzmodelle zeichnen in der ef_fnc-Liste.
                {auto draw=[&]{Scene out;f.world.append(out,{},SceneCamera{});return out;};
                 const Scene with=draw();
                 for(const auto* b:bolts)const_cast<EffectSprite*>(b)->stflg|=0x1000000u;
                 const Scene without=draw();
                 for(const auto* b:bolts)const_cast<EffectSprite*>(b)->stflg&=~0x1000000u;
                 unsigned lit=0;for(std::size_t k=without.triangles.size();k<with.triangles.size();++k)if(with.triangles[k].light_category==4)++lit;
                 check(with.triangles.size()>without.triangles.size()&&lit>0,"bhEff_ModelDraw: Blitzmodelle fehlen im Bild");
                 // Materialfarbe aus dem beschriebenen Strom (GetDengekiColorAddr, Schritt 0: 0x60B2B2FF).
                 bool written=false;for(std::size_t k=0;k<with.triangles.size();++k)if(with.triangles[k].light_category==4&&with.triangles[k].color==0x60B2B2FFu)written=true;
                 check(written,"bhEff_ModelDraw: Materialfarbe aus live_models[k] fehlt");
                 for(const auto& g:f.world.unbound_enemy_routines())check(g.find("bhEff_ModelDraw")==std::string::npos,"bhEff_ModelDraw meldet eine Luecke");}
                models=true;}
            if(frm==40&&!ramp){l=f.world.light_state(2);check(l&&l->mode==5u&&l->color.z==6.375f,"CtrLight Modus 5: rgb bleibt, nr/fr wachsen");ramp=true;}
            if(frm==71){check(f.effects(353).empty()&&!(p->flags&0x80000u),"Ende: 353 abgeblendet, Licht aus");end=true;}
        }
        check(colors0&&light353&&models&&ramp&&end,"SparkEffect-Kette (mtn 9) nicht vollstaendig");
        f.no_gaps("SparkEffect");
    }
    {
        // PlyerHitCheck (en22.c:1053): Spieler 6 vor dem Werk auf seinem Boden, im Schlagfenster.
        Fixture f(data);auto& e=f.albinoid();auto* p=recvx_en22_exp0(&e);f.activate();
        const NJS_POINT3 at{e.px+6.0f,e.py,e.pz};f.actor.position=at;f.commit();
        RECVX_ENEMY_PLAYER q{};check(f.world.enemy_player_get(q),"Spielerbesitzer fehlt");q.pos={at.x,at.y,at.z};q.flr_no=e.flr_no;q.flg&=~6u;check(f.world.enemy_player_set(q),"Spieler setzen");
        const int hp0=f.inv.hp;f.tick();
        RECVX_ENEMY_PLAYER r{};f.world.enemy_player_get(r);
        check(f.inv.hp==hp0-30&&r.mode0==4&&(r.flg&4u)&&(p->flags&0x40000u)&&p->hit_window==0,"Treffer: hp - 30, mode0 4, flg 0x4, hit_window 0");
        unsigned on_player=0;for(const auto* s:f.effects(354))if(s->link_kind==0){++on_player;check(s->txp_category==0x80000000u&&s->tex_id==6,"ShockEffect2: txp[0] = epw->mlwP->texP");}
        check(on_player==4,"ShockEffect2: vier 354 am Spieler");
        const auto l=f.world.light_state(2);check(l&&l->link==0u&&l->position.x==at.x&&l->position.y==10.0f&&l->position.z==at.z,"SetLight(-1): an der Spielerlage, y 10");
        f.no_gaps("Treffer");
    }
    std::cout<<"EN22 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
