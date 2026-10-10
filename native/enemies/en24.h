#ifndef RECVX_NATIVE_EN24_H
#define RECVX_NATIVE_EN24_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 24 — Baby Black Widow (en24.c:13 "ENEMY: Baby Black Widow").
   Nativ eigenstaendig bestimmt. Semantikreferenz (nur gelesen, mit
   Datei:Zeile belegt):
     en24.c:15   DeadRate[19], :38 CapColTab[3]
     en24.c:49   bhEne24_Mode0[7], :60 bhEne24_BrainType[1],
                 :65 bhEne24_MoveMode2[6]
     en24.c:76   bhEne24            Rahmen eines Passes
     en24.c:114  bhEne24_Init, :155 Brain, :161 BR00
     en24.c:203  bhEne24_Move
     en24.c:279  MV00 (leer), :285 MV01, :307 MV02, :370 MV03,
                 :396 MV04, :458 MV05
     en24.c:485/491/497/503  Nage/Damage/Die/Dummy (alle leer, `void (void)`)
     en24.c:509  bhEne24_CollisionWalls, :517 bhEne24_CollisionLine,
                 :534 bhEne24_DeadCheck
     pwksub.c:133 bhAddSpeed, :664 bhSearchPlayer
     zonzon1.c:433 bhEne_DirTarget, :611 bhEne_AttackHitCheck,
                 :828 bhEne_CallSE, :864 bhArcTan2
     en03.c:5220 bhEne03_Collision (Dienst collision_works)
     eneset.c:906 bhEne_CallocWork
     macros.h:73 EXP0_P3_X/Y/Z, :76 EXP0_ANG, :105 njRandom
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Drei Eigenheiten des Originals, die hier unveraendert erhalten bleiben:

   1. bhEne24_Init setzt epw->mode0 = 6 (en24.c:131), und bhEne24 fuehrt den
      ganzen Pass nur aus, solange mode0 != 6 ist (en24.c:80). Ein frisch
      gesetzter EN24 initialisiert sich also EINMAL und ist danach vollstaendig
      ruhend, bis ein fremder Mechanismus mode0 wieder umstellt (das Original
      erzeugt diese Identitaet als Kind der Mutterspinne, en23.c:492). Das ist
      kein Fehler dieses Ports und wird nicht "repariert".

   2. bhEne24 ruft KEIN bhSetMotion. epw->frm_no ist in dieser Identitaet
      deshalb kein 16.16-Festkommaframe, sondern ein schlichter Zaehler 0..3,
      mit dem der Pass das Modell des VERKNUEPFTEN Works auswaehlt
      (en24.c:103-108: `epw->mlwP = &ep->mdl[epw->frm_no + 2]`). Die Motion
      bewegt sich nie; nur der Modellsatz wechselt je Pass.

   3. bhEne24_Move liest DeadRate[epw->wpnr_no] ohne Bereichspruefung
      (en24.c:222), waehrend weapon.c:1453 wpnr_no = 20 setzt — ausserhalb der
      19 Tabelleneintraege. Der Zugriff ist im Original ein Lesen hinter dem
      Array. Nativ wird er nicht nachgebildet, sondern namentlich als Luecke
      gemeldet; ein geratener Ersatzwert waere eine erfundene Entscheidung.

   Einstieg: bhEne24 (en24.c:76); der Dispatch-Eintrag 0x18 der Tabelle
   (eneset.c:51) gehoert dem Dispatcher-Besitzer und wird hier nur
   beschrieben, nicht angelegt. */
void recvx_en24(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
