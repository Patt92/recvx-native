#ifndef RECVX_NATIVE_EN17_H
#define RECVX_NATIVE_EN17_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 17 — nativ eigenstaendig bestimmt aus der Semantikreferenz
   en17.c (bhEne17 und die zugehoerigen Routinen, Tabellen en17.c). Kein
   Originalkoerper, kein Generator, keine zweite Workwelt.

   Kettenwerke: en17.c:826/:828 legt bhEne17_SetLinkWork ueber
   bhSetEnemy mit einer 9-Byte-Vorlage (flg 0x80A1, id 0x1F bzw. 0x20) zwei
   EIGENE Works an, die das Modell epw->mdl[4] (linker Arm) bzw. mdl[3]
   uebernehmen, die Motionquelle epw->mnwP teilen und per
   bhEne_SetCallFunc auf bhEne17LArm/bhEne17RArm umgehaengt werden. Der
   Zeiger steht im Original in exp2/exp3 als BH_PWORK*; nativ zeigt er auf
   den RECVX_ENEMY_WORK des Kettenwerks. Die Gelenkmatrizen sind — wie im
   Original — erst nach bhCalcModel des Kettenwerks gueltig. */
void recvx_en17(RECVX_ENEMY_WORK* w);

/* Klingenspitze des linken Arms: njCalcPoint(((O_WRK*)epw->exp2)->
   mlwP->owP[1].mtx, (0,0,-17.5)). Verbraucher: en54.c:108-116 und
   en55.c:108-116 (CollCheck der gepanzerten Ritter). Rueckgabe 0, wenn das
   Kettenwerk oder seine Gelenkmatrix nicht bestimmbar ist. */
int recvx_en17_blade_tip(const RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* out);

/* ((unsigned int*)epw->exp0)[2] |= 0x20000000 — EXP0_I(0x8) des Gegners 17.
   en54.c:120 / en55.c:120 setzen das Bit; en17.c:385 liest es und
   en17.c:397 loescht es beim Verarbeiten. */
void recvx_en17_blade_hit(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
