#pragma once
// effsub1.c:9001 bhEff130 — das fliegende Geschoss von Granatwerfer (Waffen 14..17,
// type 0..3 = Granate/Saeure/Brand/BOW-Gas), Waffe 18 (type 4) und Waffe 20 (type 6).
// Erzeuger ist der Spielerabschuss ueber WpnEffTab[w][2] (player.c:4990-5120); der
// Satz liegt im eff[512]-Pool des Raumbesitzers. Semantikreferenz, nie kompiliert.
#include "effect_sprites.h"
#include "weapon_fly.h"
#include <functional>
namespace recvx {
// Dienste des Raumbesitzers, die bhEff130 ausserhalb des Effektmoduls erreicht. Jeder
// fehlende Dienst wird beim Gebrauch benannt gemeldet; der Ablauf erfindet keinen Ersatz.
struct EffectProjectileServices {
    // pwksub.c:718 bhCheckL2Wall(&lp, flg, &len): Treffer -> sys->apos und sys->ahtp->type.
    std::function<bool(NJS_POINT3 start,NJS_POINT3 vec,unsigned flg,NJS_POINT3& apos,int& atr_type)> l2_wall;
    // weapon.c:1148 bhCheckFlyAtari(&gat, eidx) und :1258 bhSetBowDamage auf sys->ene.
    std::function<int(const RECVX_GA_WORK& gat,int eidx)> fly_atari;
    std::function<void()> bow_damage;
    // ene[index].flg2 |= 0x4 (effsub1.c:9325).
    std::function<void(int index)> enemy_flg2_or;
    // hitchk.c:5561 bhCheckL2Water(&lp, &ps) und pwksub.c bhSetWaterSplash4 (nur sys->st_flg 0x40).
    std::function<bool(NJS_POINT3 start,NJS_POINT3 vec,NJS_POINT3& hit)> l2_water;
    std::function<void(NJS_POINT3 pos,int ay,int type,float sx,float sy,float sz)> water_splash4;
    // CallPlayerWeaponSeEx(&op->px, se, slot).
    std::function<void(NJS_POINT3 pos,int se,int slot)> weapon_se;
    // plp->stflg |= bits (effsub1.c:9380).
    std::function<void(unsigned bits)> player_stflg_or;
    // sys->ef_flg &= ~bits (effsub1.c:9094 u. a.).
    std::function<void(unsigned bits)> effect_flags_clear;
    // rom->lgtp[0] der Explosion (type 6) laeuft ueber EffectWorldHooks::room_light0.
};
void set_effect_projectile_services(EffectProjectileServices services);
void step_eff130(EffectSprite& s,const EffectContext& c,bool& queued);
}
