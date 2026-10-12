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
// native/effect_collision.cpp und native/effect_sprites.cpp. Boden und
// Wandkontakt, Linien und Reflexion sind jetzt positiv geprüft. Effekt-
// prozeduren bleiben getrennte Lückenverträge, kein Wirkungsnachweis.
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{_Make_SinTable();
    const std::vector<CollisionWall> walls{{1,0,0,0,0,{0,0,0},{10,10,10},{}}};
    check(effect_wall(walls,20,{.01f,2,5},0x400,.1f,.1f).has_value(),"native even wall contact missing");
    NJS_POINT3 pos{.01f,2,5};int ay=8192;
    reflect_effect_wall(walls[0],20,pos,.1f,.1f,ay);
    check(pos.x==-.1f&&pos.y==2&&pos.z==5&&ay==-8192,"native box reflection did not project and reflect x");
    NJS_POINT3 end{5,0,5},normal{};
    check(effect_line(walls,20,{-5,0,5},end,normal,0,-1).has_value()&&end.x==0&&normal.x==-1,
          "native line query did not clip at the box's left face");
    std::size_t index=0;
    check(original_ground_position(walls,20,{5,5,5},&index)==-1000&&index==std::size_t(-1),
          "probe below box top incorrectly reports ground");
    check(original_ground_position(walls,20,{5,11,5},&index)==10&&index==0,
          "native box-top ground or hit index missing");
    {GroundCollision ground;ground.update(walls,20);
     check(ground.query({5,5,5})==-1000&&!ground.hit(),"below-top ground incorrectly reports a hit");
     check(ground.query({5,11,5})==10&&ground.hit(),"persistent native ground not bound");}
    set_effect_walls(&walls,20);set_effect_context({0,1,0,0,0,0,8});
    set_explosion_effect({-2,2,5},{0,0,1});set_explosion({-2,2,5},{0,0,1});
    const auto request=take_effect_output();
    check(request.spawns.empty()&&request.sounds.empty()&&request.bombs.empty()&&!request.light,
          "explosion gap requested effects, sounds, blast damage or a light");
    // Noch nicht portierte Prozeduren: sie duerfen weder laufen noch zeichnen.
    for(const unsigned id:{15u,16u,23u,24u,25u,29u,200u,201u,231u,232u}){ // bhEff000 (Patronenhuelse) und bhEff005 (Blutspritzer, effsub1.c:2099, Gegner-Angriffskette) sind gebunden
        check(!supported_effect_sprite(id),"an effect procedure claims to be bound");
        EffectSprite sprite;EffectSpawn spawn;spawn.id=id;initialize_effect_sprite(sprite,spawn);
        bool queued=true;
        check(!step_effect_sprite(sprite,&queued)&&!queued,"effect gap ran a procedure or queued geometry");
    }
    // Portiert (siehe tests/native/effect_room_sprites_test.cpp): diese
    // Prozeduren muessen gebunden melden, damit die Luecke nicht zurueckfaellt.
    for(const unsigned id:{122u,123u,125u,126u,181u,182u,218u,230u})
        check(supported_effect_sprite(id),"a ported room effect procedure reports itself unbound");
    check(common_effect_categories().size()==20&&common_effect_categories()[8]==std::pair<unsigned,unsigned>{25,1},"native ef_info mapping differs from the original table");
    check(common_effect_textures_offset({})==std::size_t(-1),"effect texture offset gap returns a guessed offset");
    set_effect_walls(nullptr,0);
    std::cout<<"Native ground/contact/line/reflection passed; explosion/effect procedures remain documented gaps.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
