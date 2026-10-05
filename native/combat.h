#pragma once
#include "weapon_asset.h"
#include "world.h"
#include "inventory.h"
#include <set>
namespace recvx {
struct CombatControls {bool aim=false,attack=false;int turn=0,vertical=0;};
class CombatController {
public:
    CombatController(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    // Returns true while a weapon pose owns locomotion. Modal/script input cancels it.
    bool tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio);
    void append(Scene& scene,const std::vector<BoneMatrix>& bones,bool scripted=false)const;
    unsigned attacks=0,shots=0,hits=0,empty=0;
    bool aiming()const{return phase_!=Phase::Idle;}
    static float target_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,NJS_POINT3 position,float radius,float height);
private:
    enum class Phase {Idle,Ready,Aim,Attack,Reload};Phase phase_=Phase::Idle;
    std::filesystem::path data_,mods_;std::optional<WeaponAsset> asset_,bare_hands_;
    unsigned weapon_=~0u,player_=~0u,clock_=0;int vertical_=0;bool inhibited_=false;
    std::set<unsigned> struck_;
    void hit(RoomWorld& world,NJS_POINT3 origin,NJS_POINT3 direction,float range,float radius,bool knife);
};
}
