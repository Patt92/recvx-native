#pragma once
#include "scene.h"
namespace recvx {
// Fields consumed by the original bhEff200/201/218 procedures and draw path.
struct EffectSprite {
    unsigned id=0,type=0,mode0=0,mode1=0,flg=0,ani_ct=0;
    int ax=0,az=0,ct0=0,tex_id=-1,bl_src=8,bl_dst=6;
    struct Vertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t col=0;};
    std::array<Vertex,4> tv{{{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0}}};
};
bool step_effect_sprite(EffectSprite& sprite);
void append_effect_sprite(Scene& scene,const EffectSprite& sprite,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture);
}
