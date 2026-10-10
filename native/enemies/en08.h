#ifndef RECVX_NATIVE_EN08_H
#define RECVX_NATIVE_EN08_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 8 — Fisch. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en08.c (:18 Mode0[6], :28 MoveMode2[3], :36 bhEne08, :45 bhEne08_Init,
   :81 bhEne08_Move, :85 bhEne08_MV00, :168 bhEne08_MV01, :263 bhEne08_MV02,
   :310/315/320 leere Nage/Damage/Die), zonzon1.c:529
   (bhEne_CheckEnemiesBall), pwksub.c:133 (bhAddSpeed) und macros.h:105-109
   (njRandom/bhRandomAngle/njArcSin/njArcTan2).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Einstieg: bhEne08 (en08.c:36); der Dispatch-Eintrag bhJumpEnemy[8]
   (eneset.c:84) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en08(RECVX_ENEMY_WORK* w);

/* ---------------------------------------------------------------------------
   Geteilter Helfer der Teilaufgabe T4, Original zonzon1.c:529
   bhEne_CheckEnemiesBall: stossen sich zwei Koerperkapseln (car) ab, schiebt
   dieser Aufruf den laufenden Work heraus und markiert den Gegner mit
   stflg 0x2 (bzw. 0x4, wenn der laufende Work der Spieler ist).
   Benutzer: en08.c:42, en06.c:253, en21.c:124.
   Die Originalbibliothek zonzon1.c ist der T3-Bibliothek zugeschlagen (T3
   deklariert diese eine Funktion dort aber nicht); bis der Integrator
   entscheidet, liefert T4 diese eine Fassung in en08.c, damit die drei
   Aufrufer nicht auf einen noch nicht vergebenen Namen warten.
   --------------------------------------------------------------------------- */
void recvx_t4_enemies_ball(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
