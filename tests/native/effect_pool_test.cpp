#include "effect_particles.h"
#include "effect_sprites.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test prüfte die Blutlache und
// die Emitterlebensdauer aus effect_particles.inc sowie bhEff029 aus
// effect_sprites.inc. Beide Generatoren und Includes sind Quarantäne, siehe
// native/effect_particles.cpp und native/effect_sprites.cpp. Geprüft bleiben
// die Slotwahl original_effect_slot (reiner Host-Code, flg & 3) und dass die
// Lücken sich wie dokumentiert verhalten: kein Werk, keine Geometrie, und die
// fehlenden Originalroutinen werden über unsupported() gemeldet.
struct BloodTable {NJS_POINT3 offset;int spread_direction;float spread_position,speed;float stretch_speed[5];int stretch_direction[5];};
static const BloodTable blood{{0,.1f,0},0,0,.06f,{.5f,.7f,.4f,1,1},{1,3,8,11,15}};
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static void tick(EffectParticles& p){p.tick([](const ParticleLink&,NJS_POINT3 at){return at;},[](NJS_POINT3){return false;});}
int main(){try{_Make_SinTable();
    check(original_effect_slot({})==0,"empty eff[512] pool did not select slot 0");
    check(original_effect_slot({1u,2u,0x80u,3u})==2,"bhSetEffectTb did not choose the first slot with !(flg & 3)");
    std::vector<unsigned> full_effect_pool(512,1u);check(original_effect_slot(full_effect_pool)==-1,"full eff[512] pool did not report allocation failure");
    {EffectSprite effect;EffectSpawn request;request.id=29;initialize_effect_sprite(effect,request);
     check(!supported_effect_sprite(29),"bhEff029 claims to be bound");
     check(!step_effect_sprite(effect),"bhEff029 gap ran a procedure");}
    {EffectParticles particles;
     particles.spawn(18,{},{},{0,0,1},0xffff0000);
     particles.spawn_mk2(18,{},{},{0,0,1},0xffff0000,0xff000000);
     particles.spawn_pool(&blood,{10,0,30},0,0);
     tick(particles);tick(particles);
     check(particles.active()==0,"particle gap reports a live emitter");
     Scene scene;particles.append_pools(scene,0);particles.append(scene,SceneCamera{},0);
     check(scene.triangles.empty()&&scene.lines.empty(),"particle gap produced geometry without a procedure");
     const auto& missing=particles.unsupported();
     check(missing.count("bhSetEffParticle")&&missing.count("bhSetEffParticleMk2")&&missing.count("bhSetBloodPoolLnk"),
           "particle gap hides the missing original routines instead of reporting them");}
    {bool rejected=false;EffectParticles particles;try{particles.spawn(19,{},{},{0,0,1},0);}catch(const std::runtime_error&){rejected=true;}
     check(rejected,"particle type outside PtclTbl accepted");}
    std::cout<<"Effect slot selection passed; particles, blood pools and bhEff029 are documented gaps.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
