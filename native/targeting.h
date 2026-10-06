#pragma once
#include "world.h"
#include <set>
namespace recvx {
struct AimSelection {unsigned right=~0u,left=~0u;int yaw=0,right_angle=0,left_angle=0;};
class Targeting {
public:
    AimSelection select(const SceneSpawn& actor,const std::vector<RoomWorld::CombatTarget>& targets,const std::vector<BoneMatrix>& nodes,bool dual,bool next,const RoomWorld* world);
    void reset(){visited_.clear();current_=~0u;dual_mode_=0;}
    static ModelNodeOverrides arms(const PlayerAsset& player,std::size_t motion,std::int64_t frame,int right,int left);
private:
    std::set<unsigned> visited_;
    unsigned current_=~0u,dual_mode_=0;
};
}
