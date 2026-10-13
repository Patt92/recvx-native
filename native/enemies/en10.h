#ifndef RECVX_NATIVE_EN10_H
#define RECVX_NATIVE_EN10_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 10 — Ameise A. Nativ eigenstaendig bestimmt. Semantikreferenz:
   en10.c (:12 Mode0[6], :22 MoveMode2[3], :30 bhEne10, :94 bhEne10_Init,
   :141 bhEne10_CheckPos, :159 bhEne10_Brain, :188 bhEne10_Move, :253 MV00
   (leer), :259 MV01, :297 MV02, :354 bhEne10_Nage, :417/423 leere
   Damage/Die), zonzon1.c:433 (bhEne_DirTarget), :828 (bhEne_CallSE), :175
   (bhEne_SetBloodEffect4), pwksub.c:133 (bhAddSpeed), ps2_NaColi.c:65
   (njDistanceP2P), macros.h:109 (njArcTan2), hitchk.c (bhCheckWall,
   bhCheckPlayer), effect.c:436 (bhSetEffectTb), vibman.c
   (StartVibrationEx), eneset.c:906 (bhEne_CallocWork), Motion.c
   (bhSetMotion), MdlPut.c:240 (bhCalcModel) und subpl.c:143 (bhEne_Event).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Die Ameise hat hp 1 und stirbt an jedem Kontakt. Ihr EXP0-Block ist
   0x24 Byte gross (en10.c:100) und wird in Original-Byteoffsets gelesen:
     +0x00 unsigned char   Teiler des Modellwechsels (0..2, en10.c:37-49)
     +0x01 unsigned char   gewaehltes Modell 0..3 (en10.c:43-51)
     +0x04 int             Drehschritt je Pass (en10.c:133/271/286/342/405)
     +0x08 float px        Heimatpunkt x (en10.c:113)
     +0x0C float py        Heimatpunkt y (:114; wird nie gelesen)
     +0x10 float pz        Heimatpunkt z (:115)
     +0x14 float           Abstand zum Heimatpunkt (:149)
     +0x18 float           Abstand zum Spieler (:119/154)
     +0x1C int             Winkel zum Spieler (:155; :120 setzt das Feld als
                           float 0, :155 schreibt es als int — beide Zugriffe
                           bleiben erhalten)
     +0x20 unsigned char   Sperrzeit nach einem Biss (UC-Lesen :85/165,
                           C-Schreiben :121/399; alle 256 Bytewerte bleiben erhalten)
     +0x21 char            wird in Init auf 0 gesetzt (:122) und sonst nie
                           benutzt
   Das Modell wechselt jeden dritten Pass (epw->mlwP = &epw->mdl[n]); nativ
   ist mdl_no die Auswahl der Modellwurzel, der Zeichenpfad muss sie
   auswerten (wie bei Identitaet 71).

   Einstieg: bhEne10 (en10.c:30); der Dispatch-Eintrag 0xA der Tabelle
   (eneset.c:51) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en10(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
