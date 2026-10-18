#pragma once
namespace recvx {
// Original njClipZ ranges as positive view-space distances (Ninja uses negative z).
struct ViewClip {float near=2.0f,far=20000.0f;};
// bhChangeViewClipRM (cut.c bhSetCut): room camera `cut` of the active room.
// Monitor view (cam.flg 0x40) bypasses every room-table entry.
ViewClip room_view_clip(unsigned stage,unsigned room,unsigned cut,bool monitor=false);
ViewClip monitor_view_clip();
// bhChangeViewClip (bhCamSet mode 0): event camera `evc_no`. Gameplay has
// ts_flg 0x200 set (near -2/far -20000 default); the map/status screens clear it.
ViewClip event_view_clip(unsigned stage,unsigned room,unsigned rcase,unsigned evc_no,unsigned display_flags=0x200u);
// bhCamSet mode 1 installs a range after bhCheckCut(1). GameNear/GameFar
// belong to the last room-clip invocation, not to the last event camera.
ViewClip restored_view_clip(ViewClip room_clip,unsigned display_flags,unsigned game_flags);
// _Make_ClipVolume (ps2_NaView.c:298). x/y ist ClipVolume.x/y, die halbe Breite/Hoehe des
// VU1-Clipvolumens in Bildpunkten bei Projektion dist: _Make_ClipMatrix (ps2_NaView.c:233)
// setzt ClipMatrix2 m[0][0] = dist/x, m[1][1] = dist/y, CLIPw prueft also |x*dist/z| <= x.
// Sind beide Werte negativ, bleibt ClipVolume 320/240 und ClipDispW/H werden -x/2, -y/2;
// sonst sind ClipDispW/H 2047. display_w/h haben nativ keinen Leser: einziger Originalleser
// von ClipDispW ist die Anzeigebereichspruefung von DoubleMain (ps2_vu1.vsm:26/35/105,
// |fSx|,|fSy| <= ClipDispW/AspectH), ClipDispH liest kein Mikroprogramm. Die native
// Streifenregel folgt _Check_DisplayAreaPoint (ps2_Vu1Strip.c:766) mit festen Grenzen.
struct ClipVolume {float x=320.0f,y=240.0f,display_w=2047.0f,display_h=2047.0f;};
ClipVolume make_clip_volume(float x,float y);
// bhChangeClipVolumeRM (event.c:13647, cut.c:292/538 neben bhChangeViewClipRM unter ts_flg
// 0x200): Schluessel (room, cam.ncut), ohne rcase und ohne Monitor-Ausnahme.
ClipVolume room_clip_volume(unsigned stage,unsigned room,unsigned cut);
// bhChangeClipVolume (event.c:13560, event.c:965 / cut.c:1797 nach bhChangeViewClip, ohne
// ts_flg-Bedingung): Schluessel (room, rcase, evc_no).
ClipVolume event_clip_volume(unsigned stage,unsigned room,unsigned rcase,unsigned evc_no);
}
