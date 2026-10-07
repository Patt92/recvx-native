#include "effect_particles.h"
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
    {EffectParticles upstairs;upstairs.spawn_pool(&blood,{40,20,70},0,0);tick(upstairs);tick(upstairs);check_pool(upstairs,20.11f);
     EffectParticles downstairs;downstairs.spawn_pool(&blood,{10,0,30},0,0);tick(downstairs);tick(downstairs);check_pool(downstairs,.11f);check_pool(upstairs,20.11f);
     downstairs.spawn(18,{}, {},{0,0,1},0xffff0000);tick(downstairs);check_pool(upstairs,20.11f);}
    // Repeated room destruction must free both effect and extension works.
    for(unsigned room=0;room<300;++room){EffectParticles next;next.spawn_pool(&blood,{0,0,0},0,0);tick(next);tick(next);check_pool(next,.11f);}
    std::cout<<"Blood pools isolated across simultaneous worlds and 300 room lifetimes.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
