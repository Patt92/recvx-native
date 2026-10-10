#include "view_clip.h"
namespace recvx {
// PORTIERUNG (Policy 2026-10-09, native Neubestimmung): Die njClipZ-Bereiche
// sind direkt aus den `static VIEW_CLIP`-Tabellen von bhChangeViewClip und
// bhChangeViewClipRM in src/ps2/veronica/prog/event.c übernommen — nicht aus
// dem Quarantäne-Generator. Beide Funktionen halten zehn tabellenförmige
// Arrays pro Stage (ViewClipStage[stg_no]) und laufen sie linear bis zum
// ersten Schlüsseltreffer; das raum<0-Sentinel ist hier in die Feldlänge
// aufgelöst. near/far stehen im Original als negatives View-Space-z und werden
// beim Rückgabe in die positiven Abstände der Headerdokumentation gekehrt.
//
// Schlüssel: bhChangeViewClip nach (room, rcase, evc_no), bhChangeViewClipRM
// nach (room, evc_no), ohne Rcase-Vergleich. Event-Fehltreffer hängen vom
// bestehenden ts_flg ab: Bit 0x200 -> 2/20000, sonst 1/99. Ein Tabellen-
// treffer überschreibt beide Standards. Raum-Fehltreffer liefern 2/20000;
// Monitoransichten überspringen die Tabelle vollständig und liefern 1/20000.
// Kein PS2-OT-/ICE-Zustand wird als Renderer oder Work nachgebildet.
namespace {
struct Row { int room,rcase,evc; float near,far; };

// bhChangeViewClip: ViewClipStage[stage], Schlüssel (room, rcase, evc_no).
const Row ev0[] = { {2,0,1,-24.0f,-5000.0f}, {2,0,4,-25.0f,-20000.0f}, {2,0,12,-9.0f,-20000.0f}, {3,0,3,-1.0f,-20000.0f}, {3,0,23,-1.0f,-20000.0f}, {3,0,29,-10.0f,-20000.0f}, {5,0,1,-1.0f,-20000.0f} };
const Row ev1[] = { {0,2,8,-1.0f,-20000.0f}, {0,2,33,-10.0f,-20000.0f}, {2,0,5,-1.0f,-20000.0f}, {2,0,8,-4.0f,-20000.0f}, {2,0,11,-1.0f,-20000.0f}, {2,0,15,-1.9f,-20000.0f}, {2,0,17,-1.0f,-20000.0f}, {2,0,34,-1.1f,-20000.0f}, {5,0,16,-20.0f,-20000.0f}, {5,0,20,-1.0f,-20000.0f}, {5,0,21,-1.0f,-20000.0f}, {5,0,23,-1.0f,-20000.0f}, {5,0,29,-1.5f,-20000.0f}, {12,2,13,-1.0f,-20000.0f} };
const Row ev2[] = { {3,0,12,-5.0f,-20000.0f}, {4,0,8,-1.0f,-20000.0f}, {5,0,0,-1.0f,-20000.0f}, {5,0,1,-10.0f,-20000.0f}, {5,0,7,-1.0f,-20000.0f}, {5,0,8,-1.0f,-20000.0f}, {5,0,19,-1.8f,-20000.0f}, {5,0,20,-1.0f,-20000.0f}, {5,0,22,-1.0f,-20000.0f} };
const Row ev3[] = { {0,0,3,-1.0f,-20000.0f}, {0,0,5,-1.0f,-20000.0f}, {0,0,7,-1.0f,-20000.0f}, {0,0,31,-1.0f,-20000.0f}, {0,0,40,-1.0f,-20000.0f}, {1,0,2,-1.0f,-20000.0f}, {2,0,9,-1.01f,-5000.0f}, {8,0,7,-1.0f,-20000.0f}, {10,0,18,-1.0f,-20000.0f}, {10,0,21,-1.7f,-20000.0f}, {13,0,28,-20.799999f,-20000.0f}, {14,0,7,-5.0f,-20000.0f}, {14,0,8,-1.1f,-20000.0f}, {23,0,11,-1.0f,-20000.0f}, {24,0,4,-1.0f,-20000.0f}, {24,0,5,-1.0f,-20000.0f}, {24,0,6,-1.0f,-20000.0f}, {24,0,9,-60.0f,-20000.0f}, {24,0,11,-1.5f,-20000.0f}, {24,0,28,-26.0f,-20000.0f} };
const Row ev4[] = { {1,0,8,-10.0f,-1000.0f}, {4,0,3,-20.0f,-20000.0f} };
const Row ev5[] = { {0,1,1,-1.0f,-20000.0f}, {0,1,9,-10.0f,-20000.0f}, {0,1,11,-60.0f,-20000.0f}, {0,1,17,-20.0f,-20000.0f}, {0,1,19,-10.0f,-20000.0f}, {0,1,21,-1.0f,-20000.0f}, {1,0,26,-1.5599999f,-20000.0f}, {1,0,30,-1.2f,-20000.0f}, {1,0,32,-1.01f,-20000.0f} };
const Row ev6[] = { {0,0,10,-10.0f,-20000.0f}, {0,0,8,-1.0f,-20000.0f}, {0,0,40,-44.0f,-20000.0f}, {4,0,17,-1.0f,-20000.0f}, {4,0,18,-1.0f,-20000.0f}, {4,0,20,-1.0f,-20000.0f}, {4,0,21,-1.0f,-20000.0f}, {4,0,35,-1.0f,-20000.0f}, {5,1,13,-1.2f,-20000.0f}, {6,0,7,-1.0f,-20000.0f}, {10,0,16,-1.1f,-20000.0f}, {10,0,17,-5.0f,-20000.0f}, {10,0,19,-1.0f,-20000.0f}, {10,0,30,-1.01f,-20000.0f}, {10,0,32,-550.0f,-20000.0f} };
const Row ev7[] = { {7,0,0,-1.0f,-20000.0f}, {7,0,1,-5.0f,-20000.0f}, {7,0,4,-20.0f,-20000.0f}, {7,0,15,-1.5f,-20000.0f}, {7,0,26,-1.0f,-20000.0f}, {8,0,2,-1.01f,-20000.0f}, {10,0,5,-1.0f,-20000.0f}, {24,0,2,-1.0f,-20000.0f} };
const Row ev8[] = { {3,0,0,-1.01f,-20000.0f}, {3,0,4,-35.0f,-20000.0f}, {3,0,9,-1.1f,-20000.0f} };
const Row ev9[] = { {5,0,2,-1.0f,-20000.0f}, {5,0,4,-1.0f,-20000.0f}, {5,0,8,-1.0f,-20000.0f}, {5,0,14,-10.0f,-20000.0f}, {5,0,34,-1.0f,-20000.0f}, {9,0,4,-1.1f,-20000.0f}, {9,0,6,-27.0f,-20000.0f}, {9,0,16,-35.0f,-8000.0f}, {9,0,28,-125.0f,-20000.0f}, {9,0,43,-1.0f,-20000.0f}, {9,0,58,-5.0f,-20000.0f}, {9,0,61,-30.0f,-20000.0f}, {16,0,0,-10.0f,-1000.0f}, {28,0,10,-1.01f,-170.0f}, {28,0,34,-2.0f,-183.0f}, {29,0,3,-1.2f,-20000.0f}, {32,1,41,-1.01f,-20000.0f}, {34,0,13,-150.0f,-20000.0f}, {34,0,27,-1.0f,-20000.0f}, {35,0,11,-10.0f,-20000.0f}, {35,0,20,-1.0f,-20000.0f}, {35,0,26,-1.01f,-20000.0f}, {35,0,41,-1.2f,-20000.0f}, {35,0,48,-1.0f,-20000.0f}, {37,0,33,-1.0f,-20000.0f}, {37,0,62,-1.0f,-20000.0f} };
const Row* const ev_stage[]={ ev0,ev1,ev2,ev3,ev4,ev5,ev6,ev7,ev8,ev9 };
const unsigned ev_count[]={ 7,14,9,20,2,9,15,8,3,26 };

// bhChangeViewClipRM: ViewClipStage[stage], Schlüssel (room, evc_no); rcase
// gehört nicht zum Suchschlüssel.
const Row rm0[] = { {0,0,1,-10.0f,-1000.0f}, {1,0,0,-1.5f,-20000.0f}, {1,0,1,-1.01f,-20000.0f}, {2,0,4,-4.0f,-20000.0f}, {2,1,4,-4.0f,-20000.0f}, {10,0,1,-8.72f,-20000.0f}, {10,0,2,-5.0f,-20000.0f}, {16,0,2,-6.0f,-20000.0f} };
const Row rm1[] = { {2,0,6,-10.0f,-1000.0f}, {4,0,0,-10.0f,-1000.0f}, {4,0,1,-22.0f,-20000.0f}, {5,0,1,-20.0f,-20000.0f}, {11,0,2,-10.0f,-20000.0f} };
const Row rm2[] = { {2,0,0,-10.0f,-1000.0f}, {2,0,2,-10.0f,-1000.0f}, {3,0,2,-3.1199999f,-20000.0f}, {4,0,0,-10.0f,-2000.0f}, {5,0,1,-10.0f,-1000.0f} };
const Row rm3[] = { {7,0,0,-3.5f,-20000.0f}, {8,0,5,-1.0f,-20000.0f}, {10,0,8,-9.0f,-20000.0f}, {15,0,1,-1.0f,-600.0f} };
const Row rm4[] = { {0,0,2,-4.0f,-20000.0f}, {4,0,1,-22.0f,-20000.0f}, {4,0,2,-22.0f,-20000.0f}, {4,0,3,-15.0f,-20000.0f} };
const Row rm5[] = { {0,0,1,-10.0f,-1000.0f}, {1,0,0,-1.1f,-20000.0f}, {51,0,2,-10.0f,-20000.0f} };
const Row rm6[] = { {0,0,1,-44.0f,-20000.0f}, {3,0,0,-1.0f,-1000.0f}, {3,0,1,-1.4f,-20000.0f}, {4,0,0,-20.0f,-20000.0f}, {9,0,2,-25.5f,-1000.0f}, {9,0,3,-25.5f,-1000.0f} };
const Row rm7[] = { {9,0,1,-15.0f,-20000.0f}, {15,0,1,-10.0f,-20000.0f}, {16,0,3,-10.0f,-20000.0f} };
const Row rm8[] = { {0,0,2,-4.0f,-20000.0f} };
const Row rm9[] = { {0,0,1,-44.0f,-20000.0f}, {3,0,0,-1.0f,-20000.0f}, {3,0,1,-1.0f,-20000.0f}, {4,0,0,-20.0f,-20000.0f}, {5,0,1,-10.0f,-1000.0f}, {5,0,2,-10.0f,-20000.0f}, {8,0,2,-11.33f,-20000.0f}, {12,0,1,-3.4f,-20000.0f}, {14,0,0,-10.0f,-20000.0f}, {14,0,2,-12.0f,-20000.0f}, {15,0,3,-4.0f,-1000.0f}, {22,0,0,-5.0f,-20000.0f} };
const Row* const rm_stage[]={ rm0,rm1,rm2,rm3,rm4,rm5,rm6,rm7,rm8,rm9 };
const unsigned rm_count[]={ 8,5,5,4,4,3,6,3,1,12 };

// Linearer Scan wie im Original: erster Schlüsseltreffer gewinnt, sonst null.
// `with_rcase` wählt den Drei-Schlüssel-Vergleich der Event- gegen den
// Zwei-Schlüssel der Raumtabelle.
const Row* find(const Row* table,unsigned count,unsigned room,unsigned rcase,unsigned evc,bool with_rcase){
    for(unsigned i=0;i<count;++i){const Row& r=table[i];
        if(unsigned(r.room)==room&&r.evc==int(evc)&&(!with_rcase||unsigned(r.rcase)==rcase))return &r;}
    return nullptr;}
}
ViewClip monitor_view_clip(){return {1.0f,20000.0f};}
ViewClip restored_view_clip(ViewClip room_clip,unsigned display_flags,unsigned game_flags){
    if(!(display_flags&0x200u))return {1.0f,99.0f};
    return (game_flags&0x40u)?monitor_view_clip():room_clip;
}
ViewClip room_view_clip(unsigned stage,unsigned room,unsigned cut,bool monitor){
    if(monitor)return monitor_view_clip();
    if(stage>=10)return {};
    const Row* r=find(rm_stage[stage],rm_count[stage],room,0,cut,false);
    return r?ViewClip{-r->near,-r->far}:ViewClip{};}
ViewClip event_view_clip(unsigned stage,unsigned room,unsigned rcase,unsigned evc_no,unsigned display_flags){
    const ViewClip fallback=(display_flags&0x200u)?ViewClip{}:ViewClip{1.0f,99.0f};
    if(stage>=10)return fallback;
    const Row* r=find(ev_stage[stage],ev_count[stage],room,rcase,evc_no,true);
    return r?ViewClip{-r->near,-r->far}:fallback;}
}
