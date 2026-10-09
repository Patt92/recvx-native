#include "player_hair.h"
/* PORTIERUNGSLÜCKE (Policy 2026-10-09): Der Inhalt dieser Datei bestand aus
 * bhObjClpn und bhCalcHair, die tools/generate_player_hair.py als
 * player_hair.inc aus den Originalquellen erzeugt hat. Generator und Include
 * liegen in archived/FORBIDDEN_DONTUSE/ und dürfen weder gelesen noch
 * ausgeführt werden. Mit dem Include ist auch der hier gehaltene eigene
 * Spieler-/Objekt-Arbeitssatz (BH_PWORK/O_WRK/ML_WORK plus sys->pletcp)
 * entfallen, den die Policy zusätzlich als separaten Work-Owner verbietet.
 *
 * Zu leisten: die Haarkette von obwp[2] (lkmtab-Objekt 1211, bhJumpObject2[11])
 * als eigene native Mechanik bestimmen — Segmentkette an Kopfknoten 5, die
 * Verlaufspuffer aus mode0 0/1, die Rücksetzbedingung plp->flg2 0x2 und die
 * Winkelbildung je Segment. Nicht aus einem Generator über die Originalquellen.
 *
 * Solange das offen ist, liefert recvx_hair_tick den dokumentierten
 * Neutralwert: keine Winkel und der Rückgabewert 0 ("auf diesem Frame wurde
 * nichts simuliert"), den runtime.cpp bereits auswertet.
 *
 * Sichtbare Folge: Claires Pferdeschwanz (und der von Spieler 3) hat keine
 * Physik mehr. Das Haarmodell bleibt in seiner Bindepose am Kopfknoten
 * stehen, statt der Kopfbewegung nachzulaufen. Kein geratener Ersatzwert und
 * keine nachgebaute Federsimulation. */
int recvx_hair_tick(const RECVX_HAIR_INPUT* input,int angles[4][2]){
    int i;
    (void)input;
    for(i=0;i<4;++i){angles[i][0]=0;angles[i][1]=0;}
    return 0;
}
