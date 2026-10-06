#pragma once
#include "scene.h"
namespace recvx {
struct WeaponFlash {
    unsigned flg=0,mode0=0,type=0,mdlver=0,ct0=0,ct1=0,flr_no=2,tex_id=3,ani_ct=0,pn=4;
    int bl_src=8,bl_dst=10;float sx=1,sy=1,sz=1,spd=1;void* func=nullptr;
    struct Vertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t col=~0u;} vertex;
    Vertex* tvp=&vertex;float pv[16]{};int jno[8]{};NJS_MATRIX matrix{};
    // Generated original bhEff003; queued is retained for this tick's presentations.
    bool queued=false;
    void tick();
    void append(Scene& scene,unsigned texture)const;
};
}
