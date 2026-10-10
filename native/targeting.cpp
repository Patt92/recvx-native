#include "targeting.h"
namespace recvx {
// Die Zielaufschaltung ist portiert — aber NICHT hier. Sie liegt im
// Spielerkampfkern (native/player_combat_core.c, Paket A01-A04) auf der einen
// Kampfsicht des Spielerworks und auf den RECVX_ENEMY_WORKs der gemeinsamen
// Gegnertabelle:
//   playpch.c:318  bhSearchEnemy        sortierte Sichtliste in pp.sch_lst
//   playpch.c:401  SetLockOnDirection   pp.lck_tbl, Armwinkel arm_dl/arm_dr
//   playpch.c:1174 bhCPM2_SearchPch     Zielwechsel der Doppelwaffe
//   playpch.c:150  MixSetToJointRot     Gelenkkette 1/2/6/7 bzw. 1/2/10/11
//   pwksub.c:170   bhCalcLockEneYR      Nachdrehen mit Schulterversatz
//   pwksub.c:218   bhSearchNearEnemy    :506 bhSearchNextEnemy
//
// Diese Hostklasse ist der Rest der frueheren, aus einem Generator gespeisten
// Zielauswahl. Sie hat keinen produktiven Aufrufer mehr und haelt bewusst
// KEINE zweite Ziel- oder Besuchtliste: eine zweite Zielquelle ist genau der
// Fehler, den die Policy verbietet. Sie bleibt nur stehen, weil
// tests/native/combat_test.cpp ihre Rueckgabe prueft; der Integrator-Patch
// (work/native_port_A01_A04_combat.patch, Abschnitt 8) entfernt beides.
//
// Das aufgeschaltete Ziel steht im Kampf: CombatController::targets() liefert
// AimSelection aus pp.lck_tbl (Hostindex ueber RECVX_ENEMY_WORK.owner_index)
// und den Armwinkeln pp.obj_r_ang/obj_l_ang.
AimSelection Targeting::select(const SceneSpawn& actor,const std::vector<RoomWorld::CombatTarget>& targets,const std::vector<BoneMatrix>& nodes,bool dual,bool next,const RoomWorld* world){
    (void)targets;(void)nodes;(void)dual;(void)next;(void)world;
    AimSelection out;out.yaw=actor.yaw;return out;
}
ModelNodeOverrides Targeting::arms(const PlayerAsset& player,std::size_t motion,std::int64_t frame,int right,int left){
    (void)player;(void)motion;(void)frame;(void)right;(void)left;
    return {};
}
}
