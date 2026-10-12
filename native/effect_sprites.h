#pragma once
#include "scene.h"
#include <optional>
#include <set>
#include <functional>
#include <memory>
#include <map>
#include "collision.h"
namespace recvx {
struct EffectModelWork;
// Fields consumed by the original bhEff200/201/218 procedures and draw path.
struct EffectSprite {
    unsigned id=0,type=0,mode0=0,mode1=0,flg=0,stflg=0,ani_ct=0;bool polygon_queued=false;
    // O_WRK.mdflg (offset 0x84). The draw pass reads bit 0x40 together with
    // gm_flg 0x4000 (effect.c:1301); AllocOwork zeroes it and no original
    // effect procedure writes it, so only a future owner can set it.
    unsigned mdflg=0;
    int ayp=0;float yn=0;void* func=nullptr;
    int ax=0,ay=0,az=0,lkono=0,ct0=0,ct1=0,ct2=0,ct3=0,tex_id=-1,bl_src=8,bl_dst=6;
    // O_WRK.exp0 as the room procedures use it: the UV table latched at init.
    const void* uv=nullptr;
    // bhSetEffectTb/bhSetEffect fields used by the generated procedures (px..pz must stay contiguous).
    unsigned mdlver=0;int flr_no=0;float px=0,py=0,pz=0,gpx=0,gpy=0,gpz=0,lox=0,loy=0,loz=0,xn=0,zn=0,sx=1,sy=1,sz=1,sxb=1,syb=1,szb=1,spd=0;void* lkwkp=nullptr;EffectModelWork* mlwP=nullptr;
    // O_WRK.jno[15]: the joint chain bhSetShadow copies up to its -1 terminator.
    std::array<std::int8_t,15> jno{};
    struct Vertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t col=0;};
    std::array<Vertex,4> tv{{{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0}}};
    struct Quad {NJS_POINT3 center;std::array<Vertex,4> vertices;unsigned category,source,destination;};
    std::shared_ptr<std::array<unsigned,312>> extra;std::vector<Quad> quads;
    unsigned char* exp0=nullptr;
    bool flame_compact_uv=false; // bhEff009 latches its UV bank at initialization.
};
// Read-only per-pass values from native owners; never persisted as a sys/plp work.
// gm_flg: bhSetRoom (room.c:471) clears bit 0 on every room load and no other
// original game source writes it, so the bhEff125 gate stays open after a load.
struct EffectContext {unsigned st_flg=0,pt_flg=1,player_flg=0,player_stflg=0,player_mdflg=0,cb_flg=0,sp_flg=8,yk_ct=0,gm_flg=0;};
void set_effect_context(const EffectContext& context);
void set_effect_walls(const std::vector<CollisionWall>* walls,float room_height);
// bhCheckWater(&px): first containing water area; top receives py + h.
void set_effect_water(std::function<bool(NJS_POINT3,float&)> probe);
void set_effect_ground(class GroundCollision* ground,const std::vector<CollisionWall>* water_room,const std::vector<CollisionWall>* water_mobile);
void set_effect_link_matrix(unsigned node,const float* matrix);
// effsub1.c:4218 bhEff017: the effect watches the EN00 work it was spawned
// with (bhSetEffect(17, &null_point, ep, 0), eneset.c:461) and clears
// sys->mwalp[work->ct0].flg bit 0 as soon as that work dies
// (stflg 0x1000000, event.c bhEneSetCk). The room owner resolves the entry in
// its own mobile wall list, because sys->mwalp is not an effect-side table.
void set_effect_mobile_wall_clear(std::function<void(int index)> clear);
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view);
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,unsigned>& texture_bases);
// bhControlPlayer lighter (wpnr_no 1): WpnEffTab[1][0] and lgttab[1].
struct LighterSprite {EffectSprite sprite;NJS_POINT3 scale;};
LighterSprite lighter_sprite();
struct OriginalLight {unsigned flg,type;int aspd,lkflg,lkno,lkono,lsrc;NJS_POINT3 position,offset,direction;float spc,dif,amb;NJS_POINT3 color;float near_range,far_range;};
OriginalLight lighter_light();
// bhSetEffectTb / bhSetEffect: a request to start one effect work (the host owns the slots).
struct EffectSpawn {unsigned flg=1,id=0,type=0;int flr_no=0;unsigned mdlver=0;NJS_POINT3 position{},scale{1,1,1};int ax=0,ay=0,lkono=0;bool has_offset=false;NJS_POINT3 offset{};void* linked_work=nullptr;};
struct BombRequest {NJS_POINT3 position;float radius;int damage_max,damage_min;}; // bhCheckBombAtari
// CallYakkyouSe(&op->px, SeNo): the owner resolves the arms bank and plays it on SE slot 10.
struct EffectRoomSound {NJS_POINT3 position;int sound;};
struct EffectOutput {std::vector<EffectSpawn> spawns;std::vector<unsigned> sounds;std::vector<EffectRoomSound> room_sounds;std::vector<BombRequest> bombs;std::optional<OriginalLight> light;std::set<unsigned> unsupported;};
// Fills the effect work exactly as bhSetEffectTb copies an EF_WORK.
void initialize_effect_sprite(EffectSprite& sprite,const EffectSpawn& spawn);
// bhSetExplosion (weapon.c): sound 534, flash light, effects 16/23/24. `view` is cam.vx/vy/vz.
void set_explosion(NJS_POINT3 position,NJS_POINT3 view);
EffectOutput take_effect_output();
// bhInitEffect: SYSTEM entry 1 holds the ef_info models, then the common effect PVP list.
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& system_entry);
// sys->ef_tn: first texture of each ef_info category (bhDrawTrsEffect3D uses ef_tn[tex_id]+ani_ct).
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(); // {first texture, frames}
// Runs the independent native procedure; `queued` reports whether it entered the translucent list this frame
// (bhDrawTrsEffect3D only draws queued work).
bool step_effect_sprite(EffectSprite& sprite,bool* queued=nullptr);
// Per-pass view of authoritative native flags, not a stored sys/player copy.
bool step_effect_sprite(EffectSprite& sprite,const EffectContext& context,bool* queued=nullptr);
// light.c types 1..6: working RGB is sampled before advancing the clock.
void step_light_color(unsigned type,unsigned speed,unsigned& clock,NJS_POINT3 color,NJS_POINT3& working,bool running);
bool supported_effect_sprite(unsigned id);
// Room texture groups (room.c:305-329 ef_tn) a ported procedure addresses
// through tex_id. Their atlases keep the decoded Ninja alpha, because the
// ported effects draw with their own njColorBlendingMode pair.
bool ported_effect_atlas(unsigned group);
// effect.c:bhSetEffectTb scans the shared eff[512] pool and accepts the first
// slot whose flg has neither allocation bit set. Missing tail entries are zeroed.
int original_effect_slot(const std::vector<unsigned>& flags);

void append_effect_sprite(Scene& scene,const EffectSprite& sprite,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture,const float* linked_matrix=nullptr);
}
