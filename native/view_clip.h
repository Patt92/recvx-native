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
}
