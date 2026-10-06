#include "effect_sprites.h"
#include <stdexcept>
namespace recvx {
namespace {
using O_WRK=EffectSprite;
struct EFF5UV {int u,v;};
struct EFF5UVTBL {EFF5UV* pTblTop;int lTblMax,lTexID,lAniCt,pad,w,h;};
struct OriginalSpriteQueue {unsigned ef_trsn=0;O_WRK* ef_trs[512]{};} queue;
auto* sys=&queue;
// The platform libc random sequence is not matched yet. It only seeds atlas phase.
int effect_random(){static unsigned state=1;state=state*1103515245+12345;return int((state>>1)&0x7fffffff);}
#include "effect_sprites.inc"
}
bool step_effect_sprite(EffectSprite& sprite){queue.ef_trsn=0;if(sprite.id==200)bhEff200(&sprite);else if(sprite.id==201)bhEff201(&sprite);else if(sprite.id==218)bhEff218(&sprite);else return false;return true;}
void append_effect_sprite(Scene& scene,const EffectSprite& s,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture){
    if(!s.type||(s.flg&0x1000000))return;
    // bhDrawTrsEffect3D: translation, UnitRotPortion for flag 0x100000,
    // Z rotation, scale. Invert the room view rotation to place the billboard.
    NJS_MATRIX basis;njUnitMatrix(&basis);njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);njRotateZ(&basis,s.az);
    std::array<SceneVertex,4> vertices;
    for(unsigned i=0;i<4;++i){NJS_POINT3 point{s.tv[i].x*scale.x,s.tv[i].y*scale.y,s.tv[i].z*scale.z};njCalcVector(&basis,&point,&point);point.x+=position.x;point.y+=position.y;point.z+=position.z;vertices[i]={point,s.tv[i].u,s.tv[i].v,{}};}
    for(auto indices:{std::array<unsigned,3>{0,1,2},std::array<unsigned,3>{1,3,2}}){SceneTriangle triangle;for(unsigned i=0;i<3;++i)triangle.vertices[i]=vertices[indices[i]];triangle.color=s.tv[0].col;triangle.texture=texture;triangle.textured=true;triangle.camera_mask=false;triangle.world_lit=true;triangle.blend=SceneBlend::Additive;triangle.depth_write=false;scene.triangles.push_back(triangle);}
}
}
