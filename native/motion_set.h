#pragma once
#include <optional>
#include "actor.h"
#include <cstdint>
#include <string>
#include <vector>

namespace recvx {
// Motion.c:21 bhSetMotion on a native object tree. `obj` is mlwP->objP
// (pos/ang per physical node), `owp_flg` is mlwP->owP[i].flg (0x2: angle
// kept, 0x4: node skipped, 0x8: FastHokan blends in YZX order). `clip` is
// mnwP[mtn_no]; nullptr is the original datP == NULL case, which leaves the
// work untouched and returns 0. `datP` is mtn_tp; without mode 0x2 the
// original uses NormalTbl (identity) instead.
//
// Disassembly check (SLUS_201.84, 0x12F510..0x130D88): only
// SetMtnNormalHokan consumes a motion channel for an owP-0x4 node; every
// other variant advances datP alone. The hokan Euler flip writes
// ang[0]+0x8000, 0x8000-ang[1], ang[2]+0x8000.
struct MotionWork {
    std::int32_t frm_no=0;
    std::uint32_t frm_mode=0,hokan_count=0,mtn_attr=0;
    std::int32_t hokan_rate=0;
};
// Returns the original -1/0/1. A state the original would read outside its
// tables (frame/channel/node beyond the data) is not executed; `gap` then
// names it and the frame clock still follows Motion.c:79-115.
// `motion_id`/`motion_no` werden nur in Meldungen gefuehrt (der Eigentuemer
// der Bibliothek und der Motionindex).
int set_motion(MotionWork& work,int add,unsigned mode,const MotionClip* clip,const std::int8_t* datP,
               std::vector<ModelPose>& obj,const std::vector<std::uint32_t>& owp_flg,std::string* gap=nullptr,
               unsigned motion_id=0,unsigned motion_no=0);
}
