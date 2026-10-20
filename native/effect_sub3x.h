#pragma once
// Handportierte Effektprozeduren aus effsub4.c (bhJumpEffect4, effect.c:37; Semantikreferenz,
// nie kompiliert, kein Funktionskoerper kopiert): bhEff_Sub357 (:1235), bhEff_Sub360 (:1241),
// bhEff_Sub361 (:1335), bhEff_Sub368 (:2075) und bhEff_Sub398 (:2722). Sie laufen auf dem
// vorhandenen EffectSprite-Satz des Raumbesitzers; `queued` ist die Mitgliedschaft in
// sys->ef_trs dieses Passes (bhDrawTrsEffect3D zeichnet nur, was drinsteht).
// Nicht hier: bhEff302/bhEff304 (effsub3.c:1107/1683) haben bereits eigene Besitzer
// (native/effect_particles.cpp bzw. RoomWorld::tick_ry_shadow) und bhEff_Sub367 (:2007) hat
// im Original keinen Erzeuger fuer exp0 (EFFSUB367_WORK) und op->func; siehe effect_sub3x.cpp.
#include "effect_sprites.h"
namespace recvx {
void step_eff357(EffectSprite& s,const EffectContext& c,bool& queued); // effsub4.c:1235 bhEff_Sub357
void step_eff360(EffectSprite& s,const EffectContext& c,bool& queued); // effsub4.c:1241 bhEff_Sub360
void step_eff361(EffectSprite& s,const EffectContext& c,bool& queued); // effsub4.c:1335 bhEff_Sub361
void step_eff368(EffectSprite& s,const EffectContext& c,bool& queued); // effsub4.c:2075 bhEff_Sub368
void step_eff398(EffectSprite& s,const EffectContext& c,bool& queued); // effsub4.c:2722 bhEff_Sub398
}
