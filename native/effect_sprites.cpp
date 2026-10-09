#include "effect_sprites.h"
#include "chunk_blend.h"
#include "binary_view.h"
#include "core_random.h"
#include "ninja_math.h"
#include <algorithm>
#include <stdexcept>
namespace recvx {
// Host-Typen der Effektverknüpfung; EffectSprite hält nur Zeiger darauf.
struct EffectLinkNode {NJS_MATRIX mtx{};};
struct EffectModelWork {EffectLinkNode* owP=nullptr;};
// E03-Basis: ef_info/WpnEffTab/lgttab sind reine Daten; Flamme und Licht
// sind eigenständige native Mechanismen auf dem vorhandenen Besitzer.
// PORTIERUNGSLÜCKE: Andere Effektprozeduren, Explosion/Blut/Trümmer und
// Metalldetektor bleiben ungebunden. Keine Generator-/Originalkernbrücke.
namespace {
EffectOutput pending;
// effect.c ef_info: {model block present, animation textures}. Terminal
// {-1,0} is represented by the end of the native data array.
constexpr std::pair<unsigned,unsigned> categories[]={{1,9},{1,0},{0,2},{0,4},{0,4},{0,4},{0,2},{1,0},{0,1},{0,2},{0,3},{0,5},{0,0},{1,0},{0,0},{0,0},{0,0},{0,2},{0,1},{0,1}};
}
void set_effect_context(const EffectContext& context){(void)context;}
LighterSprite lighter_sprite(){
    EffectSpawn spawn;spawn.id=9;spawn.type=2;spawn.scale={.5f,.5f,.5f};
    LighterSprite out;initialize_effect_sprite(out.sprite,spawn);out.scale=spawn.scale;return out;
}
OriginalLight lighter_light(){
    // lgttab[1]. bhSetLightTab replaces its tabular direction with
    // bhGetLightVector(0,0,0), i.e. (0,0,-1).
    return {1,4,30,1,0,9,4,{0,0,0},{.3f,0,-1.5f},{0,0,-1},0,0,0,{3.16f,2.06f,.7f},8,400};
}
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& data){
    if(data.empty())return std::size_t(-1);
    const BinaryView bytes(data);std::size_t cursor=0;
    // Follow exactly the model-bearing categories, not a magic offset or a
    // byte search for PVP. Each model size is followed by the next size word.
    for(const auto [model,frames]:categories)if(model){
        const auto size=bytes.u32(cursor);cursor+=4;
        if(size&0x80000000u)throw std::runtime_error("common effect model chain ends before ef_info");
        bytes.require(cursor,size);cursor+=size;
    }
    bytes.require(cursor,4);cursor+=4; // word fetched after the final model
    const auto offset=(cursor+31)&~std::size_t(31);bytes.require(offset,4);return offset;
}
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(){
    std::vector<std::pair<unsigned,unsigned>> out;unsigned first=0;
    for(const auto [model,frames]:categories){out.emplace_back(first,frames);first+=frames;}return out;
}
bool supported_effect_sprite(unsigned id){return id==9;}
bool step_effect_sprite(EffectSprite& sprite,bool* queued){
    if(supported_effect_sprite(sprite.id))throw std::runtime_error("flame step requires native flag context");
    if(queued)*queued=false;return false;
}
bool step_effect_sprite(EffectSprite& s,const EffectContext& c,bool* queued){
    if(queued)*queued=false;if(!supported_effect_sprite(s.id))return false;
    if(!s.mode0){
        s.flg|=0x4100000;s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;
        s.flame_compact_uv=s.type>=1&&s.type<=3;s.tex_id=s.flame_compact_uv?8:9;
        for(unsigned i=0;i<4;++i){auto& v=s.tv[i];v.x=(i&1)?1.f:-1.f;v.y=(i&2)?0.f:-2.f;v.z=0;v.col=0xffe0e0e0;}
        s.mode0=1;
    }
    if(s.type==3&&!(c.st_flg&0x20000000u)){s.flg=0;return true;}
    if(s.type==2){
        if(!(c.player_flg&0x1000000u)){s.flg=0;return true;}
        if(!(c.pt_flg&1u)||(c.player_stflg&0x1000000u)||(c.player_mdflg&1u))return true;
        s.flg&=~0x1000000u;
    }
    if(s.ct0==8)s.ct0=0;
    if(s.ct0<0||s.ct0>7)throw std::runtime_error("flame UV cursor outside native bank");
    const float edge=s.flame_compact_uv?.0625f:.25f;
    const float u=float(s.flame_compact_uv?s.ct0:s.ct0%4)*edge;
    const float v=s.flame_compact_uv?.8125f:float(s.ct0/4)*edge;
    for(unsigned i=0;i<4;++i){s.tv[i].u=u+((i&1)?edge:0);s.tv[i].v=v+((i&2)?edge:0);}
    ++s.ct0;if(queued)*queued=true;return true;
}
void step_light_color(unsigned type,unsigned speed,unsigned& clock,NJS_POINT3 color,NJS_POINT3& working,bool running){
    if(!running)return;
    working=color;if(type<1||type>6)return;
    const unsigned shape=(type-1)%3;const float wave=njSin(clock)*(shape==0?.25f:shape==1?.5f:1.f);
    if(shape==2)working={wave*color.x,wave*color.y,wave*color.z};
    else {const float base=shape==0?.75f:.5f;working={wave*color.x+color.x*base,wave*color.y+color.y*base,wave*color.z+color.z*base};}
    unsigned delta=speed<<8;
    if(type>=4)delta=unsigned(float(delta)*(-recvx_core_random_next()/-2.1474836e9f));
    clock=(clock+delta)&0x7fff;
}
void set_effect_walls(const std::vector<CollisionWall>* walls,float height){(void)walls;(void)height;}
void set_effect_ground(GroundCollision* ground,const std::vector<CollisionWall>* room,const std::vector<CollisionWall>* mobile){(void)ground;(void)room;(void)mobile;}
void set_effect_link_matrix(unsigned node,const float* matrix){
    // Die Knotenmatrix hat ohne Effektprozeduren keinen Abnehmer.
    (void)node;(void)matrix;
}
void set_explosion(NJS_POINT3 position,NJS_POINT3 view){(void)position;(void)view;}
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view){(void)position;(void)view;}
EffectOutput take_effect_output(){EffectOutput out=std::move(pending);pending=EffectOutput{};return out;}
// bhSetEffectTb: copy flg/id/type/mdlver/flr_no, position, scale (also kept in sxb..), angles and the joint offset.
void initialize_effect_sprite(EffectSprite& s,const EffectSpawn& e){
    s=EffectSprite{};s.flg=e.flg;s.id=e.id;s.type=e.type;s.tex_id=-1;s.mdlver=e.mdlver;s.flr_no=e.flr_no;s.px=e.position.x;s.py=e.position.y;s.pz=e.position.z;
    s.sx=s.sxb=e.scale.x;s.sy=s.syb=e.scale.y;s.sz=s.szb=e.scale.z;s.ax=e.ax;s.ay=e.ay;s.az=0;s.lkono=e.lkono;
    s.lkwkp=e.linked_work;
    if(e.has_offset){s.lox=e.offset.x;s.loy=e.offset.y;s.loz=e.offset.z;}
}
// effect.c:bhSetEffectTb scans the shared eff[512] pool and accepts the first
// slot whose flg has neither allocation bit set. Missing tail entries are zeroed.
int original_effect_slot(const std::vector<unsigned>& flags){for(unsigned i=0;i<512;++i)if(i>=flags.size()||!(flags[i]&3u))return int(i);return -1;}
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,unsigned>& bases){for(const auto& q:sprite.quads){const auto base=bases.find(q.category);if(base==bases.end())throw std::runtime_error("original custom-effect texture missing");EffectSprite quad;quad.id=302;quad.flg=0x100000;quad.tv=q.vertices;quad.bl_src=int(q.source);quad.bl_dst=int(q.destination);append_effect_sprite(scene,quad,q.center,{1,1,1},camera,base->second);}}
void append_effect_sprite(Scene& scene,const EffectSprite& s,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture,const float* linked_matrix){
    if((s.flg|s.stflg)&0x1000000)return;
    // bhDrawTrsEffect3D: translation, UnitRotPortion for flag 0x100000,
    // Z rotation, scale. Invert the room view rotation to place the billboard.
    NJS_MATRIX basis;njUnitMatrix(&basis);
    if(s.flg&0x100000){njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);njRotateZ(&basis,s.az);}
    else {if((s.flg&0x80)&&!(s.flg&0x200000)){if(!linked_matrix)throw std::runtime_error("linked effect draw missing native node matrix");std::copy(linked_matrix,linked_matrix+16,basis);}njRotateXYZ(&basis,s.ax,s.ay,s.az);}
    std::array<SceneVertex,4> vertices;
    for(unsigned i=0;i<4;++i){NJS_POINT3 point{s.tv[i].x*scale.x,s.tv[i].y*scale.y,s.tv[i].z*scale.z};njCalcVector(&basis,&point,&point);point.x+=position.x;point.y+=position.y;point.z+=position.z;vertices[i]={point,s.tv[i].u,s.tv[i].v,{}};}
    for(auto indices:{std::array<unsigned,3>{0,1,2},std::array<unsigned,3>{1,3,2}}){SceneTriangle triangle;for(unsigned i=0;i<3;++i)triangle.vertices[i]=vertices[indices[i]];triangle.color=s.tv[0].col;triangle.texture=texture;triangle.textured=true;triangle.camera_mask=false;triangle.world_lit=true;triangle.depth_write=false;
        // Effects bound with their original njColorBlendingMode pair (hit blood, explosion). The older atlas sprites keep the
        // additive approximation they were verified with until their pairs are checked against the original look.
        if(s.id==5||s.id==9||s.id==23||s.id==302){triangle.blend=SceneBlend::Ninja;triangle.blend_register=ninja_blend_register(unsigned(s.bl_src),unsigned(s.bl_dst));}else triangle.blend=SceneBlend::Additive;
        if(s.id==9)triangle.texture_filter=(s.flg&0x20000000u)?SceneTextureFilter::Point:SceneTextureFilter::Linear;
        scene.triangles.push_back(triangle);}
}
}
