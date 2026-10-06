#include "enemy_spawn.h"
#include <stdexcept>
namespace recvx {
namespace {
using BH_PWORK=EnemySpawn;using ATR_WORK=EnemyArea;
struct Room {int flr_n;ATR_WORK* flrp;} room;auto* rom=&room;
struct System {int mflr_n=0;ATR_WORK* mflrp=nullptr;unsigned rm_flg=0;} system;auto* sys=&system;
BH_PWORK player;auto* plp=&player;
unsigned en04_contact_flg=0,en04_atari_flg=0;
unsigned random_state=1;
int spawn_random(){random_state=random_state*1103515245+12345;return int((random_state>>1)&0x7fffffff);}
const int kaidan_ang[4]={0,49152,32768,16384};
#include "enemy_spawn.inc"
}
bool activate_enemy_spawn(EnemySpawn& enemy,NJS_POINT3 position,int floor,std::vector<EnemyArea>& areas,EnemySpawnContext& context){
    for(const auto& area:areas)if(area.prm0==4&&area.prm1==7&&area.prm2>=4)throw std::runtime_error("enemy spawn direction outside original four angles");
    room={int(areas.size()),areas.data()};player={};player.px=position.x;player.py=position.y;player.pz=position.z;player.flr_no=floor;system={};random_state=context.random;en04_contact_flg=context.contact;en04_atari_flg=context.areas;
    bool activated=false;
    if(enemy.type==1){const auto* player_area=bhEne_EnemyAtariCheck(reinterpret_cast<NJS_POINT3*>(&player.px),floor,enemy.id,1);if(player_area){const auto* enemy_area=bhEne_EnemyAtariCheck(reinterpret_cast<NJS_POINT3*>(&enemy.px),enemy.flr_no,enemy.id,2);activated=enemy_area&&enemy_area->prm3==player_area->prm3;}}
    else if(enemy.type==2){if(en04_contact_flg&1){bhEne04_AtariCheck2(&enemy);activated=true;}else activated=bhEne04_AtariCheck(&enemy)!=0;}
    context.contact=en04_contact_flg;context.areas=en04_atari_flg;context.room_flags|=system.rm_flg;context.random=random_state;if(activated)enemy.type=0;return activated;
}
}
