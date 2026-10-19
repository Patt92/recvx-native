#pragma once
#include "actor.h"
namespace recvx {
struct WeaponAsset {
    Scene right,left;
    std::vector<std::optional<MotionClip>> motions;
    static WeaponAsset decode(const std::vector<std::uint8_t>& packet);
    static WeaponAsset load(const std::filesystem::path& data,unsigned player,unsigned weapon,const std::filesystem::path& mods={});
    // models_hidden: sys->obwp[0]/[1].mdflg 0x1, beide Modelle bleiben aus der Zeichenliste.
    void append(Scene& scene,const std::vector<BoneMatrix>& bones,bool right_part2_hidden=false,bool models_hidden=false)const;
};
}
