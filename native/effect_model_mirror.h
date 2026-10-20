#pragma once
#include "effect_sprites.h"
#include "ninja_matrix.h"
#include "binary_view.h"
#include "chunk_blend.h"
#include <algorithm>
#include <utility>

namespace recvx::effect_model_detail {
// effsub4.c:3319: st_flg 0x100 schedules a SECOND bhDrawEffect at the owner
// (game.c:77-89/427), not here. Unlike bhPutModel (MdlPut.c:57/89), this
// direct model call never duplicates itself and tests the EFFECT's mdflg.
inline bool visible(const EffectSprite& s,unsigned gm_flg){
    return !((s.flg|s.stflg)&0x1000000u)&&!((gm_flg&0x4000u)&&(s.mdflg&0x40u));
}
// effsub4.c:3358-3374; njRotateEx(...,0), ps2_NaMatrix.c:1759.
// No pose sampling, node hierarchy or billboard correction: objP[mdlver].model
// is drawn directly. A linked caller supplies the ALREADY CALCULATED owP matrix.
inline bool matrix(const EffectSprite& s,NJS_MATRIX& out,const float* linked=nullptr){
    if((s.flg&0x200000u)||!(s.flg&0x80u)){
        njUnitMatrix(&out);njTranslate(&out,s.px,s.py,s.pz);
        njRotateZ(&out,s.az);njRotateY(&out,s.ay);njRotateX(&out,s.ax);
    }else{
        if(!linked)return false;
        std::copy_n(linked,16,out);njTranslate(&out,s.lox,s.loy,s.loz);
        njRotateXYZ(&out,s.ax,s.ay,s.az);
    }
    njScale(&out,s.sx,s.sy,s.sz);return true;
}
// Native metadata for njCnkSimpleMultiDrawModel (MODE 3, not Simple MODE 2).
// The existing Scene/renderer owns cam.mtx * njMirror; do not pre-reflect vertices.
inline void prepare(Scene& object,const EffectSprite& s,bool reflected){
    route_model_texture_cache(object.triangles,1); // SimpleMulti, same strip routing as bhPutModel draw_tp 1
    for(auto& t:object.triangles){
        t.light_category=5;t.material_phase=reflected?10u:6u;
        t.after_modifiers=reflected;t.camera_mask=false;t.mirror=reflected?3:0;
        t.texture_filter=(s.flg&0x20000000u)?SceneTextureFilter::Point:SceneTextureFilter::Linear;
    }
    for(auto& w:object.ambient_writes){w.phase=reflected?10u:6u;w.camera_mask=false;}
}
// bhEff_ModelDraw does NOT install bl_src/bl_dst (8/10 is only work state).
// njCnkCbBa/Cm* (ps2_NinjaCnk.c:867/931ff) write even WITHOUT a strip after
// them. Reading the last triangle loses trailing or material-only writes.
inline void finish_blend(const std::vector<std::uint8_t>& stream,unsigned object,std::pair<unsigned,unsigned>* blend){
    if(!blend)return;
    BinaryView v(stream);const auto base=v.u16(4);const auto count=v.u16(6);
    if(object>=count)throw std::runtime_error("bhEff_ModelDraw: objP outside table");
    const auto nodes=v.relative(base,v.u32(12),std::size_t(count)*52);
    const auto pointer=v.u32(nodes+std::size_t(object)*52+4);if(pointer==0xffffffffu)return;
    const auto model=v.relative(base,pointer,24);const auto list=v.u32(model+4);if(list==0xffffffffu)return;
    std::size_t at=v.relative(base,list,2);
    for(unsigned guard=0;guard<65536;++guard){
        const unsigned head=v.u8(at);if(head==255)return;
        if(head==1||(head>=17&&head<=23)){
            const unsigned bits=v.u8(at+1);const auto pair=chunk_blend_modes((bits>>3)&7u,bits&7u);
            *blend={unsigned(pair.first),unsigned(pair.second)};
        }
        if(head<8)at+=2;
        else if(head<16){v.require(at,4);at+=4;}
        else{const auto size=std::size_t(v.u16(at+2)+2)*2;v.require(at,size);at+=size;}
    }
    throw std::runtime_error("bhEff_ModelDraw: unterminated material list");
}
}
