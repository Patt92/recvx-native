// EN01 Explosionseffekte im Originalraum RM_0080: bhEne01_ExpHeadEffect2 (en01.c:10967) legt
// bhEff_Sub356 an (effsub4.c:1189, Rueckrufe NikuhenEffect/NeckBloodEffect2), bhEne01_ExpLegEffect
// (en01.c:11012) vierundzwanzig bhEff_Sub363 (effsub4.c:1667). Der Aufruf der beiden EN01-Routinen
// ist die Diagnosevorbedingung (statt eines Schrotflintentreffers).
#include "world.h"
#include "room.h"
#include "en01_core.h"
#include "en03_lib.h"
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
    explicit Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_0080.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0080"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    RECVX_ENEMY_WORK& zombie(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n;++i)if(t.slot[i]&&t.slot[i]->id==1&&!(t.slot[i]->flg&0x80u)&&t.slot[i]->owner)return *t.slot[i];throw std::runtime_error("Zombie fehlt");}
    std::vector<const EffectSprite*> effects(unsigned id){std::vector<const EffectSprite*> out;for(unsigned i=0;const auto* s=world.effect_state(i);++i)if((s->flg&1u)&&s->id==id)out.push_back(s);return out;}
    unsigned live(){unsigned n=0;for(unsigned i=0;const auto* s=world.effect_state(i);++i)if(s->flg&1u)++n;return n;}
    void no_gaps(const char* phase){for(const auto& g:world.unbound_enemy_routines())if(g.find("bhEff")!=std::string::npos||g.find("Nikuhen")!=std::string::npos||g.find("NeckBlood")!=std::string::npos)throw std::runtime_error(std::string(phase)+": Luecke "+g);}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        Fixture f(data);f.tick();auto& z=f.zombie();z.dvx=0;z.dvy=1;z.dvz=0;
        f.world.run_enemy_callback([&]{recvx_en01_exp_head_effect2(&z,0);});
        auto neck=f.effects(356);check(neck.size()==1&&neck[0]->lkwkp==&z,"ExpHeadEffect2: ein 356 am Zombie");
        const auto* s=neck[0];const unsigned before=f.live();
        f.tick();f.tick(); // Pass 0 -> 1, Pass 1: NikuhenEffect (4 Fetzen)
        const unsigned after_niku=f.live();check(after_niku>=before+4,"Sub356 Pass 1: vier Fleischfetzen");
        f.tick();f.tick(); // Pass 2, Pass 3: NeckBloodEffect2(0)
        check(s->mode0==4&&s->ct0==24&&f.live()>after_niku,"Sub356 Pass 3: Halsblut Stufe 0, ct0 24");
        for(int i=0;i<25;++i)f.tick();
        check(s->mode0==5&&s->ct0==12,"Sub356: Stufe 1 nach 25 Passes, ct0 12");
        for(int i=0;i<13;++i)f.tick();
        check(!(s->flg&1u)||s->id!=356u,"Sub356: Stufe 2 und Ende");
        f.no_gaps("ExpHeadEffect2");
    }
    {
        Fixture f(data);f.tick();auto& z=f.zombie();
        f.world.run_enemy_callback([&]{recvx_en01_exp_leg_effect(&z,13);});
        auto chunks=f.effects(363);check(chunks.size()==24,"ExpLegEffect: 24 Stuecke 363");
        f.tick();
        for(const auto* c:chunks)check(c->type==3&&(c->stflg&0x20u)&&c->bl_src==8&&c->bl_dst==3&&c->ct0==7&&c->ct1>=4&&c->ct1<8&&c->ofx==0.8f,"Sub363 Init: type 3, Blend 8/3, ct0 8-1, ct1 rand%4+4");
        const auto* c=chunks[0];const float x0=c->px,y0=c->py,z0=c->pz,spd0=c->spd;
        f.tick();
        f.world.set_presentation_flags(0x1fu);
        check(f.world.effect_visible(unsigned(c->slot)),"Sub363: ef_trs-Satz mit txp[0] sichtbar");
        check(c->spd==spd0*0.8f&&(c->px!=x0||c->pz!=z0||c->py!=y0),"Sub363: spd *= 0.8, Lage folgt (xn,yn,zn)*spd");
        bool gone=false;for(int i=0;i<40&&!gone;++i){f.tick();gone=!(c->flg&1u)||c->id!=363u;}
        check(gone,"Sub363: Alpha blendet ab und der Satz endet");
        f.no_gaps("ExpLegEffect");
    }
    {
        // zonzon1.c:354 bhEne_SetFireEffect -> bhEff_E00_Fire (252) am Zombiegelenk 3, ef_tn[8].
        Fixture f(data);f.tick();auto& z=f.zombie();const RECVX_ENEMY_VEC ofp{0,0,0};
        f.world.run_enemy_callback([&]{recvx_t3_set_fire_effect(&z,3,&ofp,1.0f,10);});
        auto fire=f.effects(252);check(fire.size()==1&&(fire[0]->flg&0x80u)&&fire[0]->lkwkp==&z&&fire[0]->ct0==10,"SetFireEffect: 252 am Gelenk, ct0 10");
        f.tick();f.world.set_presentation_flags(0x1fu);
        check(fire[0]->tex_id==8&&f.world.effect_visible(unsigned(fire[0]->slot)),"252: ef_trs mit ef_tn[8] sichtbar");
        f.no_gaps("SetFireEffect");
    }
    std::cout<<"EN01 Exp effects PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
