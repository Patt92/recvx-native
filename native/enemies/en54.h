#ifndef RECVX_NATIVE_EN54_H
#define RECVX_NATIVE_EN54_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 54 — Gepanzerter Ritter A (Schwert und mittlerer Schild).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en54.c. Kein
   Originalkoerper, kein Generator, keine zweite Workwelt.

   Einstieg: bhEne54 (en54.c:45); der Dispatch-Eintrag bhJumpEnemy[54]
   (eneset.c:84) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en54(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
