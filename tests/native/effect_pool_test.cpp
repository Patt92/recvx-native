#include "effect_particles.h"
#include "effect_sprites.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
// BP_WORK layout and values from types.h / en01.c en01_BldTbl.
struct BloodTable {NJS_POINT3 offset;int spread_direction;float spread_position,speed;float stretch_speed[5];int stretch_direction[5];};
static const BloodTable blood{{0,.1f,0},0,0,.06f,{.5f,.7f,.4f,1,1},{1,3,8,11,15}};
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static void tick(EffectParticles& p){p.tick([](const ParticleLink&,NJS_POINT3 at){return at;},[](NJS_POINT3){return false;});}
static void check_pool(const EffectParticles& p,float height){Scene s;p.append_pools(s,0);check(s.triangles.size()==14,"world adopted another world's blood pool");for(const auto& t:s.triangles)for(const auto& v:t.vertices)check(std::abs(v.position.y-height)<.0001f,"blood pool retained another world's height");}
int main(){try{_Make_SinTable();
    check(original_effect_slot({})==0,"empty eff[512] pool did not select slot 0");
    check(original_effect_slot({1u,2u,0x80u,3u})==2,"bhSetEffectTb did not choose the first slot with !(flg & 3)");
    std::vector<unsigned> full_effect_pool(512,1u);check(original_effect_slot(full_effect_pool)==-1,"full eff[512] pool did not report allocation failure");
    {int linked_work[2]={0,0};EffectSprite effect;EffectSpawn request;request.id=29;request.linked_work=linked_work;initialize_effect_sprite(effect,request);check(supported_effect_sprite(29),"bhEff029 is not bound");for(unsigned i=0;i<31;++i){step_effect_sprite(effect);check(effect.flg==1&&linked_work[1]==0,"bhEff029 completed before its original 32-frame cycle");}step_effect_sprite(effect);check(effect.flg==0&&(linked_work[1]&0x40),"bhEff029 did not set linked work bit and release its slot on frame 32");}
    {EffectParticles upstairs;upstairs.spawn_pool(&blood,{40,20,70},0,0);tick(upstairs);tick(upstairs);check_pool(upstairs,20.11f);
     EffectParticles downstairs;downstairs.spawn_pool(&blood,{10,0,30},0,0);tick(downstairs);tick(downstairs);check_pool(downstairs,.11f);check_pool(upstairs,20.11f);
     downstairs.spawn(18,{}, {},{0,0,1},0xffff0000);tick(downstairs);check_pool(upstairs,20.11f);}
    // Repeated room destruction must free both effect and extension works.
    for(unsigned room=0;room<300;++room){EffectParticles next;next.spawn_pool(&blood,{0,0,0},0,0);tick(next);tick(next);check_pool(next,.11f);}
    std::cout<<"Blood pools isolated across simultaneous worlds and 300 room lifetimes.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
