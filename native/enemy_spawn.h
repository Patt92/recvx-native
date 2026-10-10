#pragma once
#include "scene.h"
#include <functional>
#include <cstdlib>
namespace recvx {
// One ATR_WORK floor area (types.h:500). The host fills these from the room's
// floor table (world.cpp:113); only type 2 with flg bit 0 takes part in enemy
// activation, exactly as bhEne_EnemyAtariCheck requires.
struct EnemyArea {
    unsigned flg=0,type=0;int flr_no=0;
    float px=0,pz=0,w=0,d=0;
    unsigned char prm0=0,prm1=0,prm2=0,prm3=0;
};
struct EnemySpawn {
    unsigned char id=0;int flr_no=0;
    float px=0,py=0,pz=0;int ay=0;unsigned type=0;
    // bhEne04 movement mode (epw->mode2). Only the running/attacking modes 2/3
    // let an already-active dog (type 0) trigger a type-4 door area
    // (bhEne04_MVType00). The host feeds this once the running AI (C04) owns
    // mode2; while it stays 0 the type-4 branch is correctly inert.
    unsigned mode2=0;
};
// Shared state the original keeps as en04.c file-scope statics, reset per room
// in bhEne04_Init. The RoomWorld owns one context per room, so these fields
// reset with it. contact is en04_contact_flg, areas is en04_atari_flg (a bit
// mask over the trigger's prm3). room_flags collects the sys->rm_flg bits the
// activation raises (bit prm3+16); the host ORs them into the event flags.
// rand is the shared random source: the original calls rand() directly, so the
// default is std::rand. The host may replace it with its own stream; the exact
// call order is documented at each use in enemy_spawn.cpp.
struct EnemySpawnContext {
    unsigned contact=0,areas=0,room_flags=0;
    std::function<int()> rand=[]{return std::rand();};
    // Type-4 door trigger (bhEne04_MVType00): the area's prm2 indexes
    // sys->obwp[]. The host owns that object work; the activation reports the
    // door it wants opened through these fields instead of writing obwp itself.
    // door_open is raised once per trigger, only when the target object is not
    // already moving (the original's `obwp[prm2].mode1 == 0` gate). door_angle
    // carries ang_tbl[prm3]; door_sound carries bhEne04_SePlay's 70415.
    bool door_open=false;int door_angle=0,door_sound=0;
};
// Returns an original type 1/2 activation, with original trap placement.
// Running/attacking AI after activation is a separate, incomplete binding.
bool activate_enemy_spawn(EnemySpawn& enemy,NJS_POINT3 player,int floor,std::vector<EnemyArea>& areas,EnemySpawnContext& context);
}
