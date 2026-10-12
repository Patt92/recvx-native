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
struct CombatOriginal;
class CombatController {
public:
    CombatController(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    // Returns true while a weapon pose owns locomotion. Modal/script input cancels it.
    bool tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio,EventVm* events=nullptr);
    void append(Scene& scene,const std::vector<BoneMatrix>& bones,bool scripted=false,const RoomWorld* world=nullptr)const;
    unsigned attacks=0,shots=0,hits=0,empty=0;
    bool aiming()const;
    unsigned original_mode()const;
    unsigned original_phase()const;
    bool attacking()const;
    std::int16_t aim_yaw()const; // plp->way of the combat player work
    const AimSelection& targets()const{return aim_;}
    ModelNodeOverrides pose_overrides(const PlayerAsset& player,std::size_t motion,std::int64_t frame)const;
    static float target_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,NJS_POINT3 position,float radius,float height);
    // Originalroutinen, die der Kampfkern in diesem Raum gemeldet hat, weil
    // ihr Dienst keinen gebundenen Besitzer hat (nie verdeckt).
    const std::set<std::string>& unbound_routines()const;
private:
    void publish_pose(PlayerAsset& player,std::size_t motion,std::int64_t frame,const SceneSpawn& actor);
    std::shared_ptr<CombatOriginal> original_;
    std::filesystem::path data_,mods_;std::optional<WeaponAsset> asset_,bare_hands_;
    unsigned weapon_=~0u,player_=~0u;int vertical_=0;bool inhibited_=false;
    std::set<unsigned> struck_;
    const PlayerAsset* body_=nullptr;
    AimSelection aim_;
};
}
