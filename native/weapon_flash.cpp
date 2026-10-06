#include "weapon_flash.h"
#include <algorithm>
#include "chunk_blend.h"
namespace recvx {
namespace {
using O_WRK=WeaponFlash;
using NJS_TEXTURE_VTX=WeaponFlash::Vertex;
struct UV_WORK {float u,v,xs,ys;};
struct Queue {O_WRK* ef_fnc[512]{};unsigned ef_fncn=0;unsigned ef_tn[32]{};} queue;
auto* sys=&queue;
thread_local Scene* output=nullptr;
thread_local unsigned texture_index=0,texture_base=0;
void njSetTextureNum(unsigned texture){texture_index=texture_base+texture;}
thread_local int blend_source=8,blend_dest=10;
void bhDrawWeaponEffect(){}
int effect_random(){return 0;} // Not used by type 0, the bound muzzle variant.
void njDrawTexture3DEx(NJS_TEXTURE_VTX* vertices,unsigned,int){
    NJS_MATRIX matrix;njGetMatrix(&matrix);
    for(auto indices:{std::array<unsigned,3>{0,1,2},std::array<unsigned,3>{1,3,2}}){SceneTriangle t;t.texture=texture_index;t.textured=true;t.camera_mask=false;t.color=vertices[0].col;t.world_lit=true;t.depth_write=false;t.blend=SceneBlend::Ninja;t.blend_register=ninja_blend_register(blend_source,blend_dest);
        for(unsigned i=0;i<3;++i){const auto& v=vertices[indices[i]];NJS_POINT3 point{v.x,v.y,v.z};njCalcPoint(&matrix,&point,&point);t.vertices[i]={point,v.u,v.v,{}};}output->triangles.push_back(t);}
}
#include "weapon_flash.inc"
}
void WeaponFlash::tick(){tvp=&vertex;queue.ef_fncn=0;bhEff003(this);queued=queue.ef_fncn>0;}
void WeaponFlash::append(Scene& scene,unsigned texture)const{
    if(!queued||!flg||type!=0)return;auto flash=*this;flash.tvp=&flash.vertex;
    output=&scene;texture_base=texture;texture_index=texture;blend_source=bl_src;blend_dest=bl_dst;
    njPushMatrix(nullptr);njSetMatrix(nullptr,&flash.matrix);draw_muzzle(&flash);njPopMatrix(1);output=nullptr;
}
}
