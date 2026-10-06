#include "effect_sprites.h"
#include <stdexcept>
namespace recvx {
namespace {
using O_WRK=EffectSprite;
struct EFF5UV {int u,v;};
struct EFF5UVTBL {EFF5UV* pTblTop;int lTblMax,lTexID,lAniCt,pad,w,h;};
struct OriginalSpriteQueue {unsigned ef_trsn=0;O_WRK* ef_trs[512]{};unsigned st_flg=0,pt_flg=1;} queue;
auto* sys=&queue;
struct OriginalPlayer {unsigned flg=0,stflg=0,mdflg=0;} player_work;
auto* plp=&player_work;
struct UV_WORK {float u,v,xs,ys;};
struct EF_WORK {unsigned flg;unsigned short id,type;short flr_no;unsigned short mdlver;float px,py,pz,sx,sy,sz;short ay,ax;};
struct LGT_WK {unsigned flg,type;int aspd,lkflg,lkno,lkono,lsrc;float px,py,pz,lx,ly,lz,vx,vy,vz,spc,dif,amb,r,g,b,nr,fr;int iang,oang,ax,ay,az;};
// The platform libc random sequence is not matched yet. It only seeds atlas phase.
int effect_random(){static unsigned state=1;state=state*1103515245+12345;return int((state>>1)&0x7fffffff);}
#include "effect_sprites.inc"
const LGT_WK& lighter_light_table(){return lighter_light;}
}
void set_effect_context(const EffectContext& c){queue.st_flg=c.st_flg;queue.pt_flg=c.pt_flg;player_work={c.player_flg,c.player_stflg,c.player_mdflg};}
LighterSprite lighter_sprite(){
    // bhSetEffectTb: copy flg/id/type and the scale; the procedure initializes the rest.
    LighterSprite out;out.sprite.flg=lighter_effect.flg;out.sprite.id=lighter_effect.id;out.sprite.type=lighter_effect.type;out.sprite.ax=lighter_effect.ax;
    out.scale={lighter_effect.sx,lighter_effect.sy,lighter_effect.sz};return out;
}
OriginalLight lighter_light(){const auto& l=lighter_light_table();return {l.flg,l.type,l.aspd,l.lkflg,l.lkno,l.lkono,l.lsrc,{l.px,l.py,l.pz},{l.lx,l.ly,l.lz},{l.vx,l.vy,l.vz},l.spc,l.dif,l.amb,{l.r,l.g,l.b},l.nr,l.fr};}
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& data){
    auto u32=[&](std::size_t at){if(at+4>data.size())throw std::runtime_error("effect data truncated");return std::uint32_t(data[at])|data[at+1]<<8|data[at+2]<<16|std::uint32_t(data[at+3])<<24;};
    std::size_t dp=4;std::uint32_t sz=u32(0);
    for(const int flg:effect_info_flags){if(flg==-1)break;if(flg&1){if(sz>data.size()-dp)throw std::runtime_error("effect model outside data");dp+=sz;sz=u32(dp);dp+=4;}}
    return (dp+31)&~std::size_t(31);
}
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(){std::vector<std::pair<unsigned,unsigned>> out;unsigned first=0;for(std::size_t i=0;effect_info_flags[i]!=-1;++i){out.push_back({first,unsigned(effect_info_anims[i])});first+=unsigned(effect_info_anims[i]);}return out;}
bool supported_effect_sprite(unsigned id){return id==9||id==122||id==200||id==201||id==218||id==230;}
bool step_effect_sprite(EffectSprite& sprite,bool* queued){queue.ef_trsn=0;if(sprite.id==9)bhEff009(&sprite);else if(sprite.id==122)bhEff122(&sprite);else if(sprite.id==200)bhEff200(&sprite);else if(sprite.id==201)bhEff201(&sprite);else if(sprite.id==218)bhEff218(&sprite);else if(sprite.id==230)bhEff230(&sprite);else return false;if(queued)*queued=queue.ef_trsn>0;return true;}
void append_effect_sprite(Scene& scene,const EffectSprite& s,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture){
    if(s.flg&0x1000000)return;
    // bhDrawTrsEffect3D: translation, UnitRotPortion for flag 0x100000,
    // Z rotation, scale. Invert the room view rotation to place the billboard.
    NJS_MATRIX basis;njUnitMatrix(&basis);
    if(s.flg&0x100000){njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);njRotateZ(&basis,s.az);}
    else njRotateXYZ(&basis,s.ax,s.ay,s.az);
    std::array<SceneVertex,4> vertices;
    for(unsigned i=0;i<4;++i){NJS_POINT3 point{s.tv[i].x*scale.x,s.tv[i].y*scale.y,s.tv[i].z*scale.z};njCalcVector(&basis,&point,&point);point.x+=position.x;point.y+=position.y;point.z+=position.z;vertices[i]={point,s.tv[i].u,s.tv[i].v,{}};}
    for(auto indices:{std::array<unsigned,3>{0,1,2},std::array<unsigned,3>{1,3,2}}){SceneTriangle triangle;for(unsigned i=0;i<3;++i)triangle.vertices[i]=vertices[indices[i]];triangle.color=s.tv[0].col;triangle.texture=texture;triangle.textured=true;triangle.camera_mask=false;triangle.world_lit=true;triangle.blend=SceneBlend::Additive;triangle.depth_write=false;scene.triangles.push_back(triangle);}
}
}
