#include "combat.h"
#include "weapon_tables.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test prüfte den gesamten
// Spielerkampf gegen die erzeugten Originalmodule player_combat.inc und
// targeting.inc — Ziehen/Zielen/Schuss/Rückstoß aller Feuerwaffen, die
// Munitionsbuchung, das automatische Nachladen, Mündungsfeuer und
// Hülsenauswurf, den Messerhieb, die Zielaufschaltung einschließlich
// Doppelwaffen und die Treffer an echten Raumgegnern. Generatoren und
// Includes sind Quarantäne, siehe native/player_combat_core.c und
// native/targeting.cpp.
//
// Diese Abdeckung muss zurückkehren, sobald der Spielerkampf und die
// Zielaufschaltung eigenständig nativ nachgebaut sind. Geprüft bleibt hier:
//  - die weiterhin eigenständig portierten Host-Abfragen (Trefferstrecke
//    gegen eine Kapsel, Geschossweg gegen die Wandtabelle, die
//    Mündungsfeuer-Lebensdauer) und die Originaltabellen,
//  - dass die Zielaufschaltung ihre Lücke wie dokumentiert meldet.
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{_Make_SinTable();
    const NJS_POINT3 start{0,11,20},dir{0,0,-1};
    check(CombatController::target_distance(start,dir,200,{0,0,0},3.8f,18.2f)<20,"target hit missing");
    check(CombatController::target_distance(start,dir,200,{20,0,0},3.8f,18.2f)==200,"off-axis hit");
    check(CombatController::target_distance({0,40,20},dir,200,{0,0,0},3.8f,18.2f)==200,"height ignored");
    std::vector<CollisionWall> walls={{1,1,0,0,0,{-5,0,8},{10,20,2}}};
    check(wall_ray_distance(walls,20,start,dir,200)==10,"bullet crosses wall");walls[0].flags=0;check(wall_ray_distance(walls,20,start,dir,200)==200,"disabled wall blocks shot");
    WeaponFlash flash;flash.flg=0x1001;flash.tick();check(flash.queued&&flash.ct0==1,"muzzle first tick absent");flash.tick();check(flash.queued&&flash.ct0==2,"muzzle second tick absent");flash.tick();check(!flash.queued&&!flash.flg,"muzzle persists beyond original lifetime");
    walls[0].flags=1;check(wall_ray_distance(walls,20,start,dir,200,-32768,0x400)==200,"original odd wall type blocks firearm");walls[0].type=0;check(wall_ray_distance(walls,20,start,dir,200,-32768,0x400)==10,"solid wall does not block firearm");walls[0].attributes=0x2000000;check(wall_ray_distance(walls,20,start,dir,200,-32768,0x40000400)==200&&wall_ray_distance(walls,20,start,dir,200,-32768,0x400)==10,"sight-transparent wall also lets bullets through");
    check(WpnTab[3].snd_wpno==0&&(WpnTab[3].seno0&255)==5&&EneDamNear[1][3]==18&&EneDamNear[1][2]==10,"original combat constants changed");
    // Lücke: die Zielaufschaltung erkennt kein Ziel und dreht den Spieler nicht.
    Targeting targeting;const std::vector<RoomWorld::CombatTarget> lock_targets={{0,1,{-6,0,-25},3.8f,18.2f,100},{1,1,{6,0,-28},3.8f,18.2f,100},{2,1,{0,0,-60},3.8f,18.2f,100}};
    const SceneSpawn lock_actor{{0,0,0},12345};
    for(bool dual:{false,true})for(bool next:{false,true}){
        const auto lock=targeting.select(lock_actor,lock_targets,{},dual,next,nullptr);
        check(lock.right==~0u&&lock.left==~0u,"lock-on gap acquired a target");
        check(lock.yaw==lock_actor.yaw&&!lock.right_angle&&!lock.left_angle,"lock-on gap turned the player or bent an arm");
    }
    targeting.reset();
    std::cout<<"Host hit/wall queries, muzzle lifetime and the original tables remain ported; player combat and lock-on report their documented gaps.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
