#pragma once
#include "scene.h"
#include <functional>
namespace recvx {
struct WeaponFlash {
    unsigned flg=0,mode0=0,type=0,mdlver=0,ct0=0,ct1=0,flr_no=2,tex_id=3,ani_ct=0,pn=4;
    int bl_src=8,bl_dst=10;float sx=1,sy=1,sz=1;mutable float spd=1;
    struct Vertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t col=~0u;} vertex;
    float pv[16]{};int jno[8]{};mutable NJS_MATRIX matrix{};
    NJS_POINT3 position{},offset{};std::array<Angle,3> angles{};
    unsigned stflg=0,mdflg=0;
    // Draw binding, not another actor/world: current linked node matrix.
    const NJS_MATRIX* linked_matrix=nullptr;
    unsigned linked_node=~0u;
    // Type 3 requires the native port of bhCheckL2Wall (flags 0x400).
    // A missing binding diagnoses a gap, never substitutes a ray heuristic.
    using WallQuery=std::function<std::optional<float>(NJS_POINT3,NJS_POINT3,unsigned)>;
    // Queued is retained for this tick's presentations.
    bool queued=false;
    void tick();
    unsigned texture_count()const{return type==0||type==5?ct0:type==3||ct0>1?2:1;}
    void append(Scene& scene,unsigned texture,unsigned game_flags=0,const WallQuery& wall={},const NJS_MATRIX* node=nullptr)const;
};
}
