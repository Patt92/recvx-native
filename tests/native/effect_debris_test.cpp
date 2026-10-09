#include "effect_sprites.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test prüfte die ATR-
// Kollisionstypen 0..7 aus effect_collision.inc und die Trümmer-/Rauchspur
// (bhSetExplosionEffect, bhEff024/025) aus effect_sprites.inc. Beide
// Generatoren und Includes sind Quarantäne, siehe
// native/effect_collision.cpp und native/effect_sprites.cpp. Geprüft bleibt
// nur, dass die Lücken sich wie dokumentiert verhalten: keine Treffer, keine
// Reflexion, keine Effektanforderung und keine gebundene Prozedur.
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{_Make_SinTable();
    const std::vector<CollisionWall> walls{{1,0,0,0,0,{0,0,0},{10,10,10},{}}};
    check(!effect_wall(walls,20,{.01f,2,5},0x400,.1f,.1f),"wall probe gap reports a guessed hit");
    NJS_POINT3 pos{.01f,2,5};int ay=8192;
    reflect_effect_wall(walls[0],20,pos,.1f,.1f,ay);
    check(pos.x==.01f&&pos.y==2&&pos.z==5&&ay==8192,"wall reflection gap moved the position or angle");
    NJS_POINT3 end{5,0,5},normal{};
    check(!effect_line(walls,20,{-5,0,5},end,normal,0,-1),"line query gap reports a guessed hit");
    std::size_t index=0;
    check(original_ground_position(walls,20,{5,5,5},&index)==-1000&&index==std::size_t(-1),
          "ground query gap does not report the original no-ground sentinel");
    {GroundCollision ground;ground.update(walls,20);
     check(ground.query({5,5,5})==-1000&&!ground.hit(),"persistent ground gap reports a height or htp");}
    set_effect_walls(&walls,20);set_effect_context({0,1,0,0,0,0,8});
    set_explosion_effect({-2,2,5},{0,0,1});set_explosion({-2,2,5},{0,0,1});
    const auto request=take_effect_output();
    check(request.spawns.empty()&&request.sounds.empty()&&request.bombs.empty()&&!request.light,
          "explosion gap requested effects, sounds, blast damage or a light");
    for(const unsigned id:{0u,5u,9u,15u,16u,23u,24u,25u,29u,122u,200u,201u,218u,230u,231u,232u}){
        check(!supported_effect_sprite(id),"an effect procedure claims to be bound");
        EffectSprite sprite;EffectSpawn spawn;spawn.id=id;initialize_effect_sprite(sprite,spawn);
        bool queued=true;
        check(!step_effect_sprite(sprite,&queued)&&!queued,"effect gap ran a procedure or queued geometry");
    }
    check(common_effect_categories().empty(),"effect categories gap invented ef_info entries");
    check(common_effect_textures_offset({})==std::size_t(-1),"effect texture offset gap returns a guessed offset");
    set_effect_walls(nullptr,0);
    std::cout<<"Collision and effect procedures report their documented gaps; no substitute hits or spawns.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
