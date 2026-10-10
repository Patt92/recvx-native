#ifndef RECVX_NATIVE_EN20_H
#define RECVX_NATIVE_EN20_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 20 — Ameise B. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en20.c (:10 Mode0[6], :20 MoveMode2[8], :33 bhEne20, :41 bhEne20_Init,
   :85 bhEne20_Move, :94 MV00, :140 MV01, :196 MV02, :228 MV03, :260 MV04,
   :292 MV05, :324 MV06, :366 MV07, :408/414/420 leere Nage/Damage/Die),
   pwksub.c:133 (bhAddSpeed), eneset.c:906 (bhEne_CallocWork), Motion.c
   (bhSetMotion), MdlPut.c:240 (bhCalcModel) und subpl.c:143 (bhEne_Event).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Nage, Damage und Die sind im Original LEERE Koerper (en20.c:408-423) und
   haben dort sogar die Signatur `void (void)`; sie bleiben leer. Die
   Identitaet wird von Init und den acht Bewegungszustaenden getragen: MV00
   ist der Ruhezustand (Motion 0) und wuerfelt in MV00/MV06/MV07, MV01 ist
   das Laufen (Motion 7) und wuerfelt in MV00/MV01/MV02/MV03, MV02 und MV03
   sind je ein kurzer Laufabschnitt zurueck nach MV01, MV04/MV05 spielen
   Motion 5 bzw. 6 ab, MV06 die Folge 3 -> 4 (doppelte Laenge) und MV07 die
   Folge 2 -> 1. MV04 und MV05 sind von keinem anderen Zustand aus
   erreichbar; das Original schaltet mode2 nie auf 4 oder 5, erreichbar sind
   sie nur ueber einen fremden Setzer (z.B. ein Ereignisskript).

   Der Zufall ist der originale `rand() % n` (en20.c:109/119/155/168), nicht
   njRandom; die Modulo-Form bleibt erhalten.

   Einstieg: bhEne20 (en20.c:33); der Dispatch-Eintrag 0x14 der Tabelle
   (eneset.c:51) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en20(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
