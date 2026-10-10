#ifndef RECVX_NATIVE_EN71_H
#define RECVX_NATIVE_EN71_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 71 — Ameise C. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en71.c. Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Einstieg: bhEne71 (en71.c:23); der Dispatch-Eintrag bhJumpEnemy[71]
   (eneset.c:84) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en71(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
