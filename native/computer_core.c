#include "computer_core.h"
#include <string.h>
/* PORTIERUNGSLUECKE (Policy 2026-10-09): Der gesamte Terminalcode aus sp_evt.c
   (bhKeepSpEvtComputer, bhControlSpEvtComputer, bhControlComEvtScript mit den
   acht Skripten, Soft-Keyboard, Fensterautomat, Quad-Aufbau, Zeichenliste,
   bhExitSpEvtComputer sowie COM_EVT_WORK) kam aus computer.inc, erzeugt von
   tools/generate_computer.py. Generator und Include sind Quarantaene
   (archived/FORBIDDEN_DONTUSE/) und duerfen weder gelesen noch ausgefuehrt
   werden. Mit den Prozeduren entfaellt auch die ganze Host-Bruecke, die hier
   nur dafuer existierte (sys/rom-Teilwerk, njDrawQuadTexture-Sammler,
   bhDispMessage, CallSystemSe, syRtc*, bhSetMemPvpTexture).

   Zu leisten: Skriptinterpreter, Fensterzustaende, Tastatur und Zeichenausgabe
   des Computerterminals als eigenen nativen Mechanismus - nicht ueber einen
   Generator auf die Originalquellen.

   Solange das offen ist, meldet recvx_com_tick einen leeren Frame und sofort
   das Ende des Tasks (Rueckgabe 0 mit finished). Der Aufrufer fuehrt daraufhin
   wie bhExitSpEvtComputer st_flg 0x4000000 und cb_flg 0x400000 zurueck, sodass
   das Eventskript weiterlaeuft und nicht am Terminal haengt. Der Frame bleibt
   leer: keine Zeichenbefehle, keine Toene, keine Ereignisflags, keine Meldung.
   Kein geratener Bildschirminhalt und keine ersetzte Skriptfolge.

   Sichtbare Folge: Das Terminal (3D-Scanner in RM_0090, Passwortterminal)
   zeigt nichts und schliesst im selben Tick wieder. Alles, was die Skripte
   setzen - Scannerablauf, Deckeloeffnen, ev_flg 0x3d/0x3e des Passworts,
   bhStandPlayerMotion und die Terminaltoene - findet nicht statt. */
void recvx_com_begin(unsigned number,const RECVX_COM_SYSTEM* system){
    (void)number;(void)system;
}
int recvx_com_tick(const RECVX_COM_INPUT* input,RECVX_COM_FRAME* frame){
    (void)input;
    if(!frame)return 0;
    memset(frame,0,sizeof *frame);
    /* Kein Task vorhanden: inaktiv und beendet, damit der Aufrufer die
       originalen Austrittsflags setzt. sp_flg -1 wie bhExitSpEvtComputer. */
    frame->active=0;frame->finished=1;frame->system.sp_flg=~0u;
    return 0;
}
