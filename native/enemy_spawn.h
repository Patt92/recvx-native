#pragma once
#include "scene.h"
namespace recvx {
struct EnemyArea {
    unsigned flg=0,type=0;int flr_no=0;
    float px=0,pz=0,w=0,d=0;
    unsigned char prm0=0,prm1=0,prm2=0,prm3=0;
};
struct EnemySpawn {
    unsigned char id=0;int flr_no=0;
    float px=0,py=0,pz=0;int ay=0;unsigned type=0;
};
struct EnemySpawnContext {unsigned contact=0,areas=0,room_flags=0,random=1;};
// Returns an original type 1/2 activation, with original trap placement.
// Running/attacking AI after activation is a separate, incomplete binding.
bool activate_enemy_spawn(EnemySpawn& enemy,NJS_POINT3 player,int floor,std::vector<EnemyArea>& areas,EnemySpawnContext& context);
}
