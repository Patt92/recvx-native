#include "view_clip.h"
#include <iostream>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test prüfte die Clip-Bereiche
// aus view_clip.inc. Generator und Include sind Quarantäne; die Bereiche sind
// bis zur nativen Neubestimmung nicht vorhanden (siehe native/view_clip.cpp).
// Geprüft bleibt nur, dass beide Abfragen den dokumentierten Ninja-Standard
// liefern und keinen geratenen Ersatzwert.
int main(){
    const auto event=event_view_clip(0,2,0,4),room=room_view_clip(0,0,1);
    const bool ok=event.near==2.0f&&event.far==20000.0f&&room.near==2.0f&&room.far==20000.0f;
    if(!ok){std::cerr<<"view clip gap does not report the documented default\n";return 1;}
    std::cout<<"View clip gap reports the Ninja default; per-camera ranges are not ported.\n";return 0;}
