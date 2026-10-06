#pragma once
#include "weapon_asset.h"
#include "world.h"
#include "targeting.h"
#include "weapon_flash.h"
#include "inventory.h"
#include <set>
namespace recvx {
// WpnTab[w].wp_fps1: flame/muzzle offset in the hand node (player.c).
NJS_POINT3 weapon_fire_offset(unsigned weapon);
// WpnTab[w].snd_wpno / seno0 (CallPlayerWeaponSeEx); false when the weapon has no shot sound.
bool weapon_shot_sound(unsigned weapon,unsigned& bank,unsigned& program);
struct CombatControls {bool aim=false,attack=false;int turn=0,vertical=0;bool next_target=false;};
class CombatController {
public:
    CombatController(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    // Returns true while a weapon pose owns locomotion. Modal/script input cancels it.
    bool tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio);
    void append(Scene& scene,const std::vector<BoneMatrix>& bones,bool scripted=false,const RoomWorld* world=nullptr)const;
    unsigned attacks=0,shots=0,hits=0,empty=0;
    bool aiming()const{return phase_!=Phase::Idle;}
    bool attacking()const{return phase_==Phase::Attack;}
    const AimSelection& targets()const{return aim_;}
    ModelNodeOverrides pose_overrides(const PlayerAsset& player,std::size_t motion,std::int64_t frame)const;
    static float target_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,NJS_POINT3 position,float radius,float height);
private:
    enum class Phase {Idle,Ready,Aim,Attack,Reload};Phase phase_=Phase::Idle;
    std::filesystem::path data_,mods_;std::optional<WeaponAsset> asset_,bare_hands_;
    unsigned weapon_=~0u,player_=~0u,clock_=0;int vertical_=0;bool inhibited_=false;
    std::set<unsigned> struck_;
    const PlayerAsset* body_=nullptr;
    std::vector<WeaponFlash> flashes_;
    Targeting targeting_;AimSelection aim_;int yaw_ticks_=0,right_arm_=0,left_arm_=0,arm_ticks_=0;
    unsigned burst_=0,fire_clock_=0;int left_delay_=-1;bool trigger_latched_=false,manual_aim_=false;
    void select_target(PlayerAsset& player,const SceneSpawn& actor,std::size_t motion,std::int64_t frame,RoomWorld* world,bool next=false);
    void fire(Inventory& inventory,const SceneSpawn& actor,RoomWorld* world,AudioMixer& audio,PlayerAsset& player,std::size_t motion,std::int64_t frame,bool left=false);
    void hit(RoomWorld& world,NJS_POINT3 origin,NJS_POINT3 direction,float range,float radius,bool knife);
};
}
