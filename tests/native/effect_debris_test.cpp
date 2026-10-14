#include "effect_sprites.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
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
    // weapon.c:1812/1706 bhSetExplosionEffect und bhSetExplosion: erst der Feuerball 23
    // mit zwei Truemmern 24, dann (nur bhSetExplosion) Ton 534, Blitzlicht rom->lgtp und
    // der Zuender 16, gefolgt von denselben 23/24. Kein bhCheckBombAtari hier - das laeuft
    // erst, wenn bhEff016 seine lkono Passes abgezaehlt hat.
    set_explosion_effect({-2,2,5},{0,0,1});set_explosion({-2,2,5},{0,0,1});
    const auto request=take_effect_output();
    const std::vector<unsigned> want{23,24,24,16,23,24,24};
    check(request.spawns.size()==want.size()&&std::equal(want.begin(),want.end(),request.spawns.begin(),
          [](unsigned id,const EffectSpawn& s){return s.id==id;}),
          "bhSetExplosion/bhSetExplosionEffect spawn order differs from weapon.c:1706/1812");
    check(request.sounds==std::vector<unsigned>{534u},"bhSetExplosion did not request CallPlayerActionSe(534)");
    check(request.light&&request.light->flg==3&&request.light->type==101&&request.light->aspd==8&&
          request.light->lkflg==0&&request.light->lsrc==4&&request.light->near_range==30.0f&&
          request.light->far_range==200.0f&&request.light->color.x==4.5f&&request.light->color.y==3.0f&&
          request.light->color.z==1.5f,"bhSetExplosion flash light differs from weapon.c:1712");
    check(request.bombs.empty(),"bhSetExplosion requests blast damage before bhEff016 ticks");
    // Der Zuender bhEff016 (effsub1.c) ruft nach lkono Passes bhCheckBombAtari(20,150,100).
    {EffectSprite fuse;initialize_effect_sprite(fuse,request.spawns[3]);
     check(fuse.id==16&&fuse.lkono==16,"bhEff016 fuse lkono differs from bhSetEffect(16,...,16)");
     for(int i=0;i<16;++i)step_effect_sprite(fuse,nullptr);
     const auto after=take_effect_output();
     check(after.bombs.size()==1&&after.bombs[0].radius==20.0f&&after.bombs[0].damage_max==150&&
           after.bombs[0].damage_min==100,
           "bhEff016 did not request bhCheckBombAtari(20,150,100)");}
    // Noch nicht portierte Prozeduren: sie duerfen weder laufen noch zeichnen.
    // 15/29/231 kommen aus den von Hand portierten Originalprozeduren (native/effect_sprite_procedures.cpp).
    // 200/201/232 sind seit 2026-10-14 von Hand portiert (Gruppe 2); 204/221/229 bleiben offen.
    // 16/22/24/25/27 sind seit den Gruppen 3 und 4 (2026-10-14) portiert; 221 seit
    // Gruppe 6; 229 (Muendungsfeuer der Doppelwaffe, effsub5.c) seit 2026-10-14 von Hand
    // portiert. Offen bleiben hier 23/204.
    // Portiert (siehe tests/native/effect_room_sprites_test.cpp): diese
    // Prozeduren muessen gebunden melden, damit die Luecke nicht zurueckfaellt.
    for(const unsigned id:{122u,123u,125u,126u,181u,182u,218u,229u,230u})
        check(supported_effect_sprite(id),"a ported room effect procedure reports itself unbound");
    check(common_effect_categories().size()==20&&common_effect_categories()[8]==std::pair<unsigned,unsigned>{25,1},"native ef_info mapping differs from the original table");
    check(common_effect_textures_offset({})==std::size_t(-1),"effect texture offset gap returns a guessed offset");
    set_effect_walls(nullptr,0);
    std::cout<<"Native ground/contact/line/reflection and bhSetExplosion(Effect) requests passed; other effect procedures remain documented gaps.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
