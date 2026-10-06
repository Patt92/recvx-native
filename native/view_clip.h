#pragma once
namespace recvx {
// Original njClipZ ranges as positive view-space distances (Ninja uses negative z).
struct ViewClip {float near=2.0f,far=20000.0f;};
// bhChangeViewClipRM (cut.c bhSetCut): room camera `cut` of the active room.
// cam.flg 0x40 (near -1) is not modelled by the host camera yet.
ViewClip room_view_clip(unsigned stage,unsigned room,unsigned cut);
// bhChangeViewClip (bhCamSet mode 0): event camera `evc_no`. Gameplay has
// ts_flg 0x200 set (near -2/far -20000 default); the map/status screens clear it.
ViewClip event_view_clip(unsigned stage,unsigned room,unsigned rcase,unsigned evc_no);
}
