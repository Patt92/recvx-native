#include "effect_particles.h"
#include "core_random.h"
#include "effect_sprites.h"
#include "chunk_blend.h"
#include <algorithm>
#include "ninja_matrix.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace recvx {
namespace {
using ::NJS_MATRIX;
struct BH_PWORK {unsigned stflg=0;};
// The fields of the original O_WRK that bhSetEffParticle/bhEff302 touch.
struct O_WRK {unsigned flg;void* func;unsigned short id,type;int tex_id;unsigned mtn_attr,mtn_no;unsigned char* lkwkp;int lkono;float lox,loy,loz,px,py,pz,xn,yn,zn,sx,sy,sz;int ax,ay,az,mode0,mode1,ct0,pn,bl_src,bl_dst,ani_ct,mdlver,flr_no;unsigned char* exp0;unsigned char* exp1;void* txp[1];void* mlwP;void* pvp;void* tvp;float pv[4];NJS_MATRIX* mtx;unsigned char mtxbuf[64];};
struct EffectSystem {int ef_tlist=0;unsigned ef_fncn=0,ef_trsn=0,pt_flg=2,bl_ct=0;O_WRK* ef_fnc[512]{};O_WRK* ef_trs[512]{};void* efm[512]{};};
std::function<bool(NJS_POINT3)> wall_probe;
/* bhCheckWallType(&pos, 0, 0.2f, 0): a wall touching the probe stops the pool from spreading there. */
void* bhCheckWallType(NJS_POINT3* pos,unsigned,float,float){return wall_probe&&wall_probe(*pos)?pos:nullptr;}
thread_local EffectSystem* sys=nullptr;
NJS_MATRIX lcmat_storage;
NJS_MATRIX* lcmat=&lcmat_storage;
unsigned owk_scn_noG=0;
// Host pool replacing eff[512]: blocks are free while (flg & 3) == 0, exactly the original test.
struct Block {alignas(16) unsigned char data[0x4E0];};
using BlockPool=std::vector<std::unique_ptr<Block>>;
thread_local BlockPool* active_pool=nullptr;
struct EffectScope {
    EffectSystem* previous_system=sys;BlockPool* previous_pool=active_pool;
    EffectScope(EffectSystem& state,BlockPool& blocks){sys=&state;active_pool=&blocks;}
    ~EffectScope(){sys=previous_system;active_pool=previous_pool;}
};
O_WRK* AllocOwork(){
    auto& pool=*active_pool;
    for(auto& block:pool){auto* o=reinterpret_cast<O_WRK*>(block->data);if(!(o->flg&3)){std::memset(block->data,0,sizeof block->data);o->flg=2;return o;}}
    if(pool.size()>=512)return nullptr;
    pool.push_back(std::make_unique<Block>());auto* o=reinterpret_cast<O_WRK*>(pool.back()->data);o->flg=2;return o;
}
O_WRK* AllocOworkOne(){return AllocOwork();}
int rand(){return recvx_core_random_next();}
struct {NJS_MATRIX mtx;} cam;
Scene* line_scene=nullptr;
void njDrawLine3D(void*,int,unsigned);
#include "effect_particles.inc"
void njDrawLine3D(void* raw,int count,unsigned){auto* p=static_cast<NJS_POINT3COL*>(raw);if(!line_scene)return;auto* points=static_cast<NJS_POINT3*>(p->p);auto* colors=static_cast<NJS_COLOR*>(p->col);for(int i=0;i<count;++i)line_scene->lines.push_back({{points[i*2],points[i*2+1]},{colors[i*2].color,colors[i*2+1].color}});}
}
struct EffectParticles::Impl {
    // bhInitEffect clears eff[512] and bl_ct on room initialization. Each
    // RoomWorld owns its works, including the reserved extension blocks.
    EffectSystem system;BlockPool blocks;
    struct Emitter {O_WRK* op;ParticleLink link;NJS_POINT3 offset;};
    std::vector<Emitter> emitters;std::set<std::string> unsupported;
    struct Pool {O_WRK* op;bool queued=false;};std::vector<Pool> pools;
};
EffectParticles::EffectParticles():impl_(std::make_unique<Impl>()){}
EffectParticles::~EffectParticles()=default;
EffectParticles::EffectParticles(EffectParticles&&) noexcept=default;
EffectParticles& EffectParticles::operator=(EffectParticles&&) noexcept=default;
void EffectParticles::spawn(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned color){
    if(type>=19)throw std::runtime_error("particle type outside PtclTbl");
    EffectScope scope(impl_->system,impl_->blocks);
    std::set<O_WRK*> before;for(auto& e:impl_->emitters)before.insert(e.op);
    bhSetEffParticle(link.linked?reinterpret_cast<BH_PWORK*>(1):nullptr,int(link.node),&offset,&direction,color,int(type));
    // bhSetEffParticle starts one O_WRK per PtclDat entry (id 302); register every new one.
    for(auto& block:impl_->blocks){auto* o=reinterpret_cast<O_WRK*>(block->data);if((o->flg&3)&&o->id==302&&!before.count(o)&&o->exp0)impl_->emitters.push_back({o,link,offset});}
}
// bhSetEffParticleMk2: identical emitter, but the destination colour is passed
// in instead of being derived from the source colour (bhEne_SetBloodEffect4).
void EffectParticles::spawn_mk2(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned source_color,unsigned destination_color){
    if(type>=19)throw std::runtime_error("particle type outside PtclTbl");
    EffectScope scope(impl_->system,impl_->blocks);
    std::set<O_WRK*> before;for(auto& e:impl_->emitters)before.insert(e.op);
    bhSetEffParticleMk2(link.linked?reinterpret_cast<BH_PWORK*>(1):nullptr,int(link.node),&offset,&direction,source_color,destination_color,int(type));
    for(auto& block:impl_->blocks){auto* o=reinterpret_cast<O_WRK*>(block->data);if((o->flg&3)&&o->id==302&&!before.count(o)&&o->exp0)impl_->emitters.push_back({o,link,offset});}
}
void EffectParticles::tick(const std::function<NJS_POINT3(const ParticleLink&,NJS_POINT3)>& resolve,const std::function<bool(NJS_POINT3)>& wall){
    EffectScope scope(impl_->system,impl_->blocks);
    sys->ef_fncn=0;wall_probe=wall;
    for(auto& emitter:impl_->emitters){auto* o=emitter.op;if(!(o->flg&3))continue;
        if(emitter.link.linked){const auto p=resolve(emitter.link,emitter.offset);o->px=p.x;o->py=p.y;o->pz=p.z;}
        bhEff302(o);}
    for(auto& pool_entry:impl_->pools){sys->ef_trsn=0;bhEff300(pool_entry.op);pool_entry.queued=sys->ef_trsn>0;}
    impl_->emitters.erase(std::remove_if(impl_->emitters.begin(),impl_->emitters.end(),[](const auto& e){return !(e.op->flg&3);}),impl_->emitters.end());
}
void EffectParticles::spawn_pool(const void* table,NJS_POINT3 position,int direction,int palette){
    EffectScope scope(impl_->system,impl_->blocks);
    std::set<O_WRK*> known;for(auto& p:impl_->pools)known.insert(p.op);
    if(!bhSetBloodPoolLnk(nullptr,&position,direction,const_cast<BP_WORK*>(static_cast<const BP_WORK*>(table)),palette))return;
    for(auto& block:impl_->blocks){auto* o=reinterpret_cast<O_WRK*>(block->data);if((o->flg&1)&&o->id==300&&!known.count(o))impl_->pools.push_back({o});}
}
std::size_t EffectParticles::active()const{return impl_->emitters.size()+impl_->pools.size();}
const std::set<std::string>& EffectParticles::unsupported()const{return impl_->unsupported;}
void EffectParticles::append_pools(Scene& scene,unsigned texture_base)const{
    // bhDrawTrsEffect3D on a 16-vertex strip lying on the floor (px,py,pz + tv x/z), texture category 17 frame `ani_ct`.
    for(const auto& entry:impl_->pools){const auto* o=entry.op;if(!entry.queued)continue;const auto* e=reinterpret_cast<const E17_WORK*>(o->exp0);if(!e)continue;
        std::array<SceneVertex,16> v;for(unsigned i=0;i<16;++i)v[i]={{o->px+e->tv_buf[i].x,o->py+e->tv_buf[i].y,o->pz+e->tv_buf[i].z},e->tv_buf[i].u,e->tv_buf[i].v,{}};
        for(unsigned i=0;i+2<16;++i){SceneTriangle t;t.vertices={v[i],v[i+1],v[i+2]};t.color=0xffffffff;t.texture=std::uint16_t(texture_base+unsigned(o->ani_ct));t.textured=true;t.camera_mask=false;t.world_lit=true;t.blend=SceneBlend::Ninja;t.blend_register=ninja_blend_register(unsigned(o->bl_src),unsigned(o->bl_dst));t.depth_write=false;scene.triangles.push_back(t);}
    }
}
void EffectParticles::append(Scene& scene,const SceneCamera& camera,unsigned texture_base)const{
    EffectScope scope(impl_->system,impl_->blocks);
    for(const auto& emitter:impl_->emitters){const auto* o=emitter.op;const auto* e=reinterpret_cast<const E02_WORK*>(o->exp0);if(!e)continue;
        const auto draw=unsigned(reinterpret_cast<std::intptr_t>(o->func));
        if(draw==2){njUnitMatrix(&cam.mtx);line_scene=&scene;bhEff_PtclLineDraw(const_cast<O_WRK*>(o));line_scene=nullptr;continue;}
        if(draw!=1)continue;
        int stage=e->stg_stt;
        for(int n=e->stg_num;n>0;--n,++stage){
            const float scale=e->stg_scl[stage];const NJS_POINT3* points=e->stg_buf[stage];if(!points)continue;
            EffectSprite sprite;sprite.id=302;sprite.flg=0x100000;sprite.bl_src=8;sprite.bl_dst=6;sprite.ani_ct=unsigned(e->stg_tim[stage]&3);
            for(auto& vertex:sprite.tv)vertex.col=std::uint32_t(e->stg_col[stage]);
            // njPtclDrawSprite: half-size `scale` world units around each projected point.
            for(unsigned i=0;i<16;++i)append_effect_sprite(scene,sprite,points[i],{scale,scale,1},camera,texture_base+sprite.ani_ct);
        }
    }
}
}
