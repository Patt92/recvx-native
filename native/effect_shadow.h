#pragma once
#include "effect_sprites.h"
#include <algorithm>

namespace recvx {
// Read-only inputs from the existing linked owner for this pass.
// No persistent player/enemy mirror and no second effect allocation.
struct ShadowOwnerView {
    unsigned flags=0,status=0,model_flags=0;
    int yaw=0;
    // effsub1.c:636 casts O_WORK to O_WRK. types.h places pyb
    // at 0x44, which aliases O_WORK.mtx[13] (0x10 + 13*4).
    // This is the current first model-node translation, not prior Y.
    float root_matrix_y=0;
};

// effect.c link update precedes this procedure. The caller supplies actual
// owP joint translations and the shared ground query, never root fallbacks.
template<class JointPosition,class GroundPosition>
bool step_shadow(EffectSprite& s,const ShadowOwnerView& owner,
                 unsigned presentation,JointPosition joint,GroundPosition ground){
    if(s.mode0==0){
        s.flg|=0x08000000u;
        s.mode0=1;
        return false; // Init does not enter ef_mdf.
    }
    if(s.mode0!=1)return false;
    s.ay=owner.yaw;
    if(s.mdlver==0){
        if(owner.status&0x2000u)s.py=owner.root_matrix_y;
        else s.py=ground(NJS_POINT3{s.px,s.py,s.pz});
    }
    if(s.type==0){
        float low_x=100.f,low_z=100.f,high_x=-100.f,high_z=-100.f;
        NJS_MATRIX rotation;
        njUnitMatrix(&rotation);
        njRotateY(&rotation,-s.ay);
        for(const auto number:s.jno){
            if(number==-1)break;
            const auto point=joint(int(number));
            NJS_POINT3 relative{point.x-s.px,0,point.z-s.pz},local{};
            njCalcPoint(&rotation,&relative,&local);
            low_x=std::min(low_x,local.x);
            low_z=std::min(low_z,local.z);
            high_x=std::max(high_x,local.x);
            high_z=std::max(high_z,local.z);
        }
        // Preserve the original two consecutive comparisons, including
        // behaviour for unusual base scales, rather than std::clamp.
        s.sx=std::min(std::max(.8f*(high_x-low_x),s.sxb),2.f*s.sxb);
        s.sz=std::min(std::max(.8f*(high_z-low_z),s.szb),2.f*s.szb);
    }
    if(!(owner.flags&1u)){
        s.flg=0;
        return false;
    }
    const bool hidden=(owner.status&0x8u)||(owner.status&0x01000000u)
        ||(owner.model_flags&1u)
        ||((owner.status&0x40000000u)&&!(presentation&1u))
        ||(!(owner.status&0x40000000u)&&!(presentation&2u));
    if(hidden){
        s.flg|=0x01000000u;
        return false;
    }
    s.flg&=~0x01000000u;
    return true;
}
}
