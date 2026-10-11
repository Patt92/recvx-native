#include "effect_particles.h"
#include "effect_sprites.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09, Stand 2026-10-11): Dieser Test prüfte die
// Blutlache und die Emitterlebensdauer aus effect_particles.inc sowie bhEff029
// aus effect_sprites.inc. bhEff029 bleibt Lücke (Generator und Include sind
// Quarantäne, siehe native/effect_sprites.cpp); die Teilchenarbeiten sind
// dagegen nativ bestimmt (effsub3.c) und werden in
// tests/native/effect_particles_test.cpp positiv gegen PtclTbl/PtclDat
// geprüft. Hier bleibt der Lebensdauer-Aspekt: ein Satz Emitterarbeiten wird
// angelegt, altert über die Ticks und zeichnet Geometrie, statt als fehlende
// Originalroutine gemeldet zu werden.
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
     // Portiert (effsub3.c): je ein Emitterwerk aus spawn und spawn_mk2 plus
     // die Lache aus spawn_pool. Die Stufenzahlen gegen PtclTbl/Eff302Prm
     // prueft effect_particles_test.cpp positiv; hier bleibt die Lebensdauer.
     check(particles.active()==3,"ported particle emitters or the blood pool did not reserve their works");
     Scene scene;particles.append_pools(scene,0);particles.append(scene,SceneCamera{},0);
     check(!scene.triangles.empty(),"a ported particle stage produced no geometry");
     check(particles.unsupported().empty(),"a ported particle routine still reported itself unsupported");
     // bhEff302 setzt bei stg_num == 0 oP->flg = 0 und gibt die Plaetze des
     // Emitters frei; die Lache (bhSetBloodPoolLnk) laeuft weiter. Der
     // letzte Schritt faellt spaetestens nach 60 Dispatches.
     for(int i=0;i<60;++i)tick(particles);
     check(particles.active()==1,"the emitter did not release its pool slots while the blood pool survives");}
    {bool rejected=false;EffectParticles particles;try{particles.spawn(19,{},{},{0,0,1},0);}catch(const std::runtime_error&){rejected=true;}
     check(rejected,"particle type outside PtclTbl accepted");}
    std::cout<<"Effect slot selection passed; particles, blood pools and bhEff029 are documented gaps.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
