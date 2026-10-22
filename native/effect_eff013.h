#pragma once
// effsub1.c:3352 bhEff013 — der Pfeil des Bogengewehrs (Waffe 10, mdlver 0) bzw. der
// Sprengpfeil (Waffe 19, mdlver 1). Erzeuger ist der Spielerabschuss ueber WpnEffTab[w][2]
// (player.c:5005). Dienste: EffectProjectileServices (effect_eff130.h). Semantikreferenz,
// nie kompiliert.
#include "effect_sprites.h"
namespace recvx {
void step_eff013(EffectSprite& s,const EffectContext& c,bool& queued);
// effsub1.c:22 bhEffDmy: op->flg = 0. effect_sprites.cpp hand_procedure fuehrt alle 143
// Plaetze (darunter 139..142); bhEff013 legt je Flugpass einen Satz 140 an, der so endet.
void step_eff_dummy(EffectSprite& s,const EffectContext& c,bool& queued);
}
