#include "view_clip.h"
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Die njClipZ-Bereiche kamen aus
// view_clip.inc, erzeugt von tools/generate_view_clip.py. Generator und
// Include sind nach archived/FORBIDDEN_DONTUSE/ verschoben und dürfen weder
// gelesen noch ausgeführt werden.
//
// Zu leisten: die Clip-Bereiche von bhChangeViewClip / bhChangeViewClipRM als
// eigene native Datenquelle bestimmen — nicht aus einem Generator über die
// Originalquellen. Solange das offen ist, liefern beide Abfragen den
// dokumentierten Ninja-Spielstandard (near 2 / far 20000, ts_flg 0x200), und
// kamera-spezifische Bereiche fehlen.
//
// Sichtbare Folge: Kameras mit abweichendem near — etwa die Hubschrauberszene
// (Kamera 4, near 25) — zeigen wieder nahe Geometrie im Bild, weil ihr
// engerer Bereich nicht vorliegt. Kein geratener Ersatzwert.
ViewClip room_view_clip(unsigned stage,unsigned room,unsigned cut){
    (void)stage;(void)room;(void)cut;
    return {};
}
ViewClip event_view_clip(unsigned stage,unsigned room,unsigned rcase,unsigned evc_no){
    (void)stage;(void)room;(void)rcase;(void)evc_no;
    return {};
}
}
