#include "view_clip.h"
namespace recvx {
namespace {
struct ViewClipRow {int room,rcase,evc_no;float near,far;};
#include "view_clip.inc"
}
ViewClip room_view_clip(unsigned stage,unsigned room,unsigned cut){
    ViewClip clip;if(stage>=10)return clip;
    for(const auto* row=room_view_clip_st[stage];row->room>=0;++row)if(unsigned(row->room)==room&&unsigned(row->evc_no)==cut){clip={-row->near,-row->far};break;}
    return clip;
}
ViewClip event_view_clip(unsigned stage,unsigned room,unsigned rcase,unsigned evc_no){
    ViewClip clip;if(stage>=10)return clip;
    for(const auto* row=event_view_clip_st[stage];row->room>=0;++row)if(unsigned(row->room)==room&&unsigned(row->rcase)==rcase&&unsigned(row->evc_no)==evc_no){clip={-row->near,-row->far};break;}
    return clip;
}
}
