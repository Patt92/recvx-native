#pragma once
#include "actor.h"
#include <array>
#include <map>
#include <optional>
namespace recvx {
struct WeaponAsset {
    Scene right,left;
    std::vector<std::optional<MotionClip>> motions;
    // Objektbaum des rechten Modells (sys->obwp[0].mlwP->objP: pos/ang je Objekt und child/sibling), fuer
    // bhCalcModel(obwp[0]) in bhActionWeapon (weapon.c:369). Leer ohne rechtes Modell.
    struct Node {ModelPose pose;std::int32_t child=-1,sibling=-1;};
    std::vector<Node> right_nodes;
    // MdlPut.c:240 bhCalcModel mit flg 0x80 (an plp): owP[node] = owP_plp[lkono] * T(lo) * Rxyz(ax, ay, az) und
    // darunter bhCalcTree ueber right_nodes, wobei `overrides` einzelne objP-Lagen ersetzt (Schlitten/Trommel).
    std::optional<std::array<float,16>> right_node_matrix(const NJS_MATRIX& hand,int ax,int ay,int az,unsigned node,const std::map<unsigned,ModelPose>& overrides={})const;
    static WeaponAsset decode(const std::vector<std::uint8_t>& packet);
    static WeaponAsset load(const std::filesystem::path& data,unsigned player,unsigned weapon,const std::filesystem::path& mods={});
    // models_hidden: sys->obwp[0]/[1].mdflg 0x1, beide Modelle bleiben aus der Zeichenliste.
    void append(Scene& scene,const std::vector<BoneMatrix>& bones,bool right_part2_hidden=false,bool models_hidden=false)const;
};
}
