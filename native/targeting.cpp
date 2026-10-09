#include "targeting.h"
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Diese Datei band bhSearchNearEnemy,
// bhSearchNextEnemy, bhCalcLockEneYR, SetLockOnDirection und MixSetToJointRot
// ein, die tools/generate_targeting.py als targeting.inc aus pwksub.c und
// playpch.c erzeugt hat. Generator und Include liegen in
// archived/FORBIDDEN_DONTUSE/ und dürfen weder gelesen noch ausgeführt
// werden. Mit dem Include ist auch der hier gehaltene eigene Spieler-/
// Gegner-Arbeitssatz (BH_PWORK/PP_WORK/Suchliste) entfallen, den die Policy
// zusätzlich als separaten Work-Owner verbietet.
//
// Zu leisten (als eigene native Mechanik, nicht aus einem Generator über die
// Originalquellen): die Zielsuche über die Gegnerarbeiten einschließlich
// Sichtprüfung gegen Wände, Entfernungs- und Winkelfenster, die
// Weiterschaltung auf das nächste Ziel samt der bereits besuchten Gegner, die
// Blickrichtung zum aufgeschalteten Ziel und die Aufteilung auf zwei Ziele
// für Doppelwaffen; dazu die Armwinkelverteilung über die Gelenkkette
// (Knoten 1/2/6/7 bzw. 1/2/10/11).
//
// Solange das offen ist, meldet select den dokumentierten Neutralwert: kein
// Ziel (right/left bleiben ~0u), keine Armwinkel und die unveränderte
// Blickrichtung des Akteurs. arms liefert keine Knotenüberschreibungen.
//
// Sichtbare Folge: Die automatische Zielaufschaltung fehlt vollständig. Beim
// Zielen wird kein Gegner markiert, der Spieler dreht sich nicht zum Gegner,
// und Doppelwaffen richten ihre Arme nicht auf zwei Ziele aus. Kein
// geratener Ersatzwert und keine nachgebaute Zielheuristik.
AimSelection Targeting::select(const SceneSpawn& actor,const std::vector<RoomWorld::CombatTarget>& targets,const std::vector<BoneMatrix>& nodes,bool dual,bool next,const RoomWorld* world){
    (void)targets;(void)nodes;(void)dual;(void)next;(void)world;
    AimSelection out;out.yaw=actor.yaw;return out;
}
ModelNodeOverrides Targeting::arms(const PlayerAsset& player,std::size_t motion,std::int64_t frame,int right,int left){
    (void)player;(void)motion;(void)frame;(void)right;(void)left;
    return {};
}
}
