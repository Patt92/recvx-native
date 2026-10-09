#include "door_core.h"
/* PORTIERUNGSLUECKE (Policy 2026-10-09): Die Tuerdemo-Prozeduren (door.c
   FadeProc1, ViewProc1..7, DoorProc1..7 samt ViewProcTbl und VectorMove) kamen
   aus door_procedures.inc, die Parameterstrukturen (FADEPROC_WORK,
   VIEWPROC1..7_WORK, DOORPROC1..7_WORK) aus door_structs.inc. Beide wurden von
   tools/generate_door_core.py erzeugt; Generator und Includes sind Quarantaene
   (archived/FORBIDDEN_DONTUSE/) und duerfen weder gelesen noch ausgefuehrt
   werden. Damit fehlen hier auch die Strukturgroessen, nicht nur die Koerper.

   Zu leisten: die Tuerdemo (Blende, Kamerafahrt, Tuerbewegung) und die Groessen
   der drei Parameterblocks als eigene native Mechanismen bestimmen - nicht ueber
   einen Generator auf die Originalquellen.

   Solange das offen ist:
   - recvx_door_parameter_size meldet 0 Byte, weil keine Blockgroesse vorliegt.
     Aufrufer verlangen dadurch keinen Mindestumfang der MPAC-Parameter.
   - recvx_door_configure bindet keine Prozedur und meldet das mit 0.
   - recvx_door_validate_view prueft nur noch den Typbereich 0..7; die verkettete
     Pruefung der chg_vew-Folge braucht die Blockgroessen.
   - recvx_door_step hat nichts auszufuehren und meldet sofort den
     Abschlussstatus 0x40, den bhControlDoorDemo am Ende der Demo setzt. Das ist
     kein Ersatz fuer die Sequenz, sondern der dokumentierte Leerlauf, damit ein
     Raumwechsel nicht dauerhaft in der Tuer haengen bleibt.

   Sichtbare Folge: Tueren haben keine Oeffnungs-/Schliesssequenz mehr. Es gibt
   keine Blende, keine Kamerafahrt, keine Tuerfluegelanimation und - weil die
   Statusbits 16..23 nie gesetzt werden - auch keine Tuer-SE. Der Raumwechsel
   schneidet hart um. Keine geratenen Zeiten, Winkel oder Soundzeitpunkte. */
size_t recvx_door_parameter_size(unsigned family,unsigned type){
    (void)family;(void)type;return 0;
}
int recvx_door_configure(RECVX_DOOR_STATE* state,unsigned fade,unsigned view,unsigned door){
    (void)fade;(void)view;(void)door;
    if(!state)return 0;
    state->fde_prcP=0;state->vew_prcP=0;state->dor_prcP=0;
    return 0;
}
int recvx_door_validate_view(unsigned type,const unsigned char* parameters,size_t bytes,unsigned depth){
    (void)parameters;(void)bytes;(void)depth;
    return type<=7;
}
void recvx_door_step(RECVX_DOOR_STATE* state){
    if(!state||(state->status&0x40))return;
    state->status|=0x40;
}
