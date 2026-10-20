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
// RECVX_CB_EFFECT.sys_ef: bhSetEffectTb(&sys->ef, ...) schreibt die gemeinsame Vorlage.
void sys_ef_request(EffectSpawn& spawn,bool with_ax);
struct CombatControls {bool aim=false,attack=false;int turn=0,vertical=0;bool next_target=false;int pad_dx=0,pad_dy=0;};
struct CombatOriginal;
class CombatController {
public:
    CombatController(std::filesystem::path data={},std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    // Returns true while a weapon pose owns locomotion. Modal/script input cancels it.
    bool tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio,EventVm* events=nullptr);
    void append(Scene& scene,const std::vector<BoneMatrix>& bones,const RoomWorld* world=nullptr)const;
    unsigned attacks=0,shots=0,hits=0,empty=0;
    void set_weapon_part2_hidden(bool hidden){right_part2_hidden_=hidden;} // obwp[0].objP[2].evalflags 0x8
    // obwp[0]/[1].mdflg 0x1: bhPlyHandChange (event.c:7413/7441) und gameover.c:304 schreiben beide
    // gemeinsam; bhSetWeapon (weapon.c:130) nullt das O_WRK beim Laden einer neuen Waffe.
    void set_weapon_models_hidden(bool hidden){weapon_models_hidden_=hidden;}
    bool weapon_models_hidden()const{return weapon_models_hidden_;}
    bool aiming()const;
    unsigned original_mode()const;
    unsigned original_phase()const;
    bool attacking()const;
    std::int16_t aim_yaw()const; // plp->way of the combat player work
    // Gruende der Zielsuche. Reine Zaehler des Kerns, wie shots/hits ueber die
    // Lebensdauer der Kampfpässe aufsummiert, damit sie auch nach dem
    // Wegstecken lesbar bleiben.
    struct AimDiagnostics {int near_calls=0,near_seen=0,near_filter=0,near_cone=0,near_wall=0,near_found=0,gun_seen=0,gun_filter=0;float wall_probe[3][4]{};unsigned wall_probe_n=0;};
    AimDiagnostics aim_diagnostics()const{return aim_history_;}
    const AimSelection& targets()const{return aim_;}
    ModelNodeOverrides pose_overrides(const PlayerAsset& player,std::size_t motion,std::int64_t frame)const;
    static float target_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,NJS_POINT3 position,float radius,float height);
    // Originalroutinen, die der Kampfkern in diesem Raum gemeldet hat, weil
    // ihr Dienst keinen gebundenen Besitzer hat (nie verdeckt).
    const std::set<std::string>& unbound_routines()const;
private:
    void accumulate_aim(const RECVX_COMBAT_RESULT& result);
    void publish_pose(PlayerAsset& player,std::size_t motion,std::int64_t frame,const SceneSpawn& actor,RoomWorld* world);
    std::shared_ptr<CombatOriginal> original_;
    AimDiagnostics aim_history_{};
    const void* aim_core_=nullptr;
    RECVX_COMBAT_RESULT aim_seen_{};
    std::filesystem::path data_,mods_;std::optional<WeaponAsset> asset_;
    unsigned weapon_=~0u,player_=~0u;int vertical_=0;bool inhibited_=false,right_part2_hidden_=false,weapon_models_hidden_=false;
    std::set<unsigned> struck_;
    // Waffen ohne nativen Kampfweg, die gezogen/abgefeuert werden sollten (benannte Luecke),
    // und die fuer unbound_routines() zusammengefuehrte Sicht.
    std::set<std::string> unsupported_;mutable std::set<std::string> reported_;
    const PlayerAsset* body_=nullptr;
    AimSelection aim_;
};
}
