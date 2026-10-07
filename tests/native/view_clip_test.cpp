#include "view_clip.h"
#include <iostream>
using namespace recvx;
int main(){
    // event.c bhChangeViewClip ViewClipSt0 {2,0,4,-25,-20000}; default -2/-20000.
    const auto heli=event_view_clip(0,2,0,4),other=event_view_clip(0,2,0,5),rcase=event_view_clip(0,2,1,4);
    // bhChangeViewClipRM ViewClipSt0 {0,0,1,-10,-1000} (rcase not compared).
    const auto cut=room_view_clip(0,0,1),plain=room_view_clip(0,0,0);
    const bool ok=heli.near==25&&heli.far==20000&&other.near==2&&rcase.near==2&&cut.near==10&&cut.far==1000&&plain.near==2&&room_view_clip(12,0,0).near==2;
    if(!ok){std::cerr<<"view clip tables differ from event.c\n";return 1;}
    std::cout<<"Original njClipZ tables for event and room cameras verified.\n";return 0;}
