#pragma once
// Handportierte Sprite-Effektprozeduren des eff[]-Pools aus effsub3.c (Semantikreferenz,
// nie kompiliert): bhEff305 (Staubwolke des Tyrant, effsub3.c:1779) und bhEff306
// (Rauchwolke am Gelenk des Erzeugers, effsub3.c:1944). Sie laufen auf dem vorhandenen
// EffectSprite-Satz des Raumbesitzers; `queued` ist die Mitgliedschaft in sys->ef_trs
// dieses Passes (bhDrawTrsEffect3D zeichnet nur, was drinsteht). Die Registrierung in
// hand_effect_procedures[] uebernimmt native/effect_sprite_procedures.cpp.
#include "effect_sprites.h"
namespace recvx {
void step_eff305(EffectSprite& s,const EffectContext& c,bool& queued); // effsub3.c:1779 bhEff305
void step_eff306(EffectSprite& s,const EffectContext& c,bool& queued); // effsub3.c:1944 bhEff306
}
