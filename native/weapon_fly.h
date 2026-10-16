#ifndef RECVX_NATIVE_WEAPON_FLY_H
#define RECVX_NATIVE_WEAPON_FLY_H
/* ===========================================================================
   Eigenstaendige native Fassung zweier Trefferroutinen aus weapon.c, die nicht
   der Spielerkampf selbst, sondern ein fliegendes Geschoss aufruft (effsub1.c
   bhEff130, Granatwerfer und Bogengeschuetz). Semantikreferenz, kein
   Originalkoerper: weapon.c:1148 bhCheckFlyAtari, weapon.c:1258 bhSetBowDamage.
   Sie laufen auf derselben Gegnertabelle (sys->ene / sys->ewk_n) wie
   native/player_combat_core.c und nutzen dieselbe Kampfgeometrie.
   =========================================================================== */
#include "combat_geometry.h"
#include "enemies/enemy_work.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* GA_WORK (types.h:2221), Feldfolge wie im Original. */
typedef struct {
    float px, py, pz, vx, vy, vz, gx, gy, gz;
    uint32_t at_flg;
    int32_t wpn_no;
    float r, l, rn, rmax;
    int32_t ax, ay;
} RECVX_GA_WORK;

/* Dienste des Raumbesitzers. c2_wall_n ist bhCheckC2WallN (pwksub.c:2172) auf
   rom->walp + sys->mwalp; gap meldet einen nicht erreichbaren Originaldienst
   namentlich. dam_near/mid/far sind EneDamNear/Mid/Far[31][22] (weapon.c). */
typedef struct {
    void* user;
    int32_t (*c2_wall_n)(void* user, const RECVX_CB_CAPSULE* cap, uint32_t flg);
    void (*gap)(void* user, const char* original);
    const int16_t (*dam_near)[22];
    const int16_t (*dam_mid)[22];
    const int16_t (*dam_far)[22];
} RECVX_FLY_SERVICES;

/* weapon.c:1148 bhCheckFlyAtari: Kugel (px, r) gegen jede Gegnerkapsel watr; bei
   freier Sicht (Kapsel r 0.1 vom Fusspunkt zur Kugelmitte, bhCheckC2WallN 0x400)
   bucht sie flg 0x4, dp/dv/da, wpnr_no, das naechste Gelenk und den Schaden nach
   at_flg 0x80 (fern) bzw. Abstand zum Abschusspunkt g (< 15 nah, sonst mittel).
   eidx ist ein ausgenommener Index. Liefert den LETZTEN getroffenen Index, -1 sonst. */
int32_t recvx_check_fly_atari(RECVX_ENEMY_TABLE* table, const RECVX_GA_WORK* gap, int32_t eidx,
                              const RECVX_FLY_SERVICES* services);
/* weapon.c:1258 bhSetBowDamage: jeder Gegner mit EneDamFar[id][17] != 0 bekommt einen
   Treffer in der Mitte seiner watr (dv 0,1,0), wpnr_no 17 und (int)(0.5 * hp) Schaden. */
void recvx_set_bow_damage(RECVX_ENEMY_TABLE* table, const RECVX_FLY_SERVICES* services);

#ifdef __cplusplus
}
#endif
#endif
