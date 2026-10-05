#pragma once
#include "actor.h"
namespace recvx {
struct WeaponAsset {
    Scene right,left;
    std::vector<std::optional<MotionClip>> motions;
    static WeaponAsset decode(const std::vector<std::uint8_t>& packet);
    static WeaponAsset load(const std::filesystem::path& data,unsigned player,unsigned weapon,const std::filesystem::path& mods={});
    void append(Scene& scene,const std::vector<BoneMatrix>& bones)const;
};
}
