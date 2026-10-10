#include "object_itmbox.h"
#include "ninja_math.h"
namespace recvx {
// Independent native type-100 controller; objitm.c bhObjItmBox is only
// the semantics reference. WorldEntity owns the lid transform. Mode zero
// performs the first opening step; the menu request follows the threshold
// crossing on a separate pass. Menu ownership pauses the object task.
void control_item_box_object(ItemBoxObjectWork& work,unsigned& cb_flg,unsigned& player_stflg,std::vector<unsigned>& sounds){
    if(work.mode0==3){player_stflg&=~0x10000u;work.ax=0;work.type=0;return;}
    if(work.mode0==2){cb_flg|=0x40000u;work.mode0=3;return;}
    if(work.mode0>3)return;
    if(work.mode0==0){work.ct0=0;work.mode0=1;sounds.push_back(0x80000240u);}
    work.ct0+=384;
    const float degrees=80.0f*njSin(work.ct0);
    work.ax=-(int(182.04445f*degrees)&0xffff);
    if(work.ct0>15360)work.mode0=2;
}
}
