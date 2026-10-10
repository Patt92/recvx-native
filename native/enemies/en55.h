#ifndef RECVX_NATIVE_EN55_H
#define RECVX_NATIVE_EN55_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 55 — Gepanzerter Ritter B (Lanze und grosser Schild).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en55.c. Kein
   Originalkoerper, kein Generator, keine zweite Workwelt.

   Einstieg: bhEne55 (en55.c:45); der Dispatch-Eintrag bhJumpEnemy[55]
   (eneset.c:84) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en55(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
