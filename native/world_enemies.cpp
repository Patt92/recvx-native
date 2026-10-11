#include <optional>
#include "world.h"
#include "binary_view.h"
#include "core_random.h"
#include "motion_set.h"
#include "subplayer.h"
#include "enemy_dispatch.h"
#include "enemies/en01_core.h"
#include "enemies/en04.h"
#include "enemies/en04_tables.h"
#include <cstring>

// C02 — the shared enemy work at its existing owner. RoomWorld/WorldEntity
// stay the only owner: every RECVX_ENEMY_WORK is a member of its WorldEntity,
// the table below only points at those members, and the services are bound
// to the native providers that already exist (collision, ground, motion
// library, asset tree, audio, RNG, EventVm flag banks). A service without
// such a provider stays NULL; the mechanisms then report the original
// routine by name and change nothing (enemy_work.h).
//
// Semantics references: room.c:114-126 (bhSetEneMdl -> bhSetEnemy in room
// order), eneset.c:344 bhControlEnemy, MdlPut.c:240 bhCalcModel/bhCalcTree,
// Motion.c:21 bhSetMotion, zonzon1.c:591 bhEne_SetWeponAtr, eneset.c:906
// bhEne_CallocWork, pwksub.c:112 bhCheckFloorNum.
namespace recvx {
namespace {
// The service table is plain C function pointers without a user argument.
// The world that runs a room load or an enemy pass installs itself here for
// exactly that call.
RoomWorld* enemy_world=nullptr;
struct EnemyWorldScope {
    RoomWorld* previous;
    explicit EnemyWorldScope(RoomWorld* w):previous(enemy_world){enemy_world=w;}
    ~EnemyWorldScope(){enemy_world=previous;}
};
WorldEntity& owner_of(RECVX_ENEMY_WORK* w){return *static_cast<WorldEntity*>(w->owner);}
const WorldEntity& owner_of(const RECVX_ENEMY_WORK* w){return *static_cast<const WorldEntity*>(w->owner);}
RECVX_ENEMY_ATR atr_of(const RECVX_STAIR_AREA& a){
    RECVX_ENEMY_ATR out{};out.flg=a.flg;out.type=a.type;out.attr=a.attr;out.flr_no=a.flr_no;out.px=a.px;out.py=a.py;out.pz=a.pz;out.w=a.w;out.h=a.h;out.d=a.d;
    out.prm0=a.prm0;out.prm1=a.prm1;out.prm2=a.prm2;out.prm3=a.prm3;out.id=a.id;return out;
}
bool same_wall(const CollisionWall& a,const CollisionWall& b){
    return a.flags==b.flags&&a.type==b.type&&a.id==b.id&&a.floor==b.floor&&a.attributes==b.attributes&&a.parameters==b.parameters&&
        a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.extent.x==b.extent.x&&a.extent.y==b.extent.y&&a.extent.z==b.extent.z;
}
constexpr std::size_t enemy_exp_budget=0xC40000; // eneset.c:910
}

// C02 player view: no stored RECVX_ENEMY_PLAYER. The fields below refer to
// SceneSpawn, the Runtime motion clock, EventVm and Inventory/head owners.
// Sources: en07.c:2393 PlayerControl, player.c:1502, en01.c PlyDG00/01.
bool RoomWorld::enemy_player_get(RECVX_ENEMY_PLAYER& out)const{
    if(!player_asset_||!player_hp_owner_||!player_head_flags_)return false;
    out={};out.pos={player_.position.x,player_.position.y,player_.position.z};
    out.ax=player_.pitch;out.ay=player_.yaw;out.az=player_.roll;out.flr_no=collision_floor_;
    out.hp=*player_hp_owner_;
    out.flg=collision_events_?collision_events_->flags[13][0]:collision_flags_;
    out.flg2=collision_events_?collision_events_->flags[15][0]:collision_flags2_;
    out.stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
    out.at_flg=player_at_flags_;
    out.mode0=std::uint8_t(collision_mode_);out.mode1=std::uint8_t(collision_mode_>>8);
    out.mode2=std::uint8_t(collision_mode_>>16);out.mode3=std::uint8_t(collision_mode_>>24);
    out.ct0=player_ct0_;out.ct1=player_ct1_;out.ayp=player_ayp_;
    out.mtn_no=unsigned(motion_);out.frm_no=std::int32_t(frame_);out.mtn_add=std::int32_t(player_step_);
    out.mtn_md=player_mtn_md_;out.hokan_count=player_hokan_count_;out.hokan_rate=player_hokan_rate_;
    out.mtn_enemy=player_enemy_motion_?1:0;out.mtn_enemy_owner=player_enemy_owner_;
    out.wpnr_no=int(player_weapon_);out.ply_id=unsigned(player_character_);out.exp1_flags=*player_head_flags_;
    return true;
}
bool RoomWorld::enemy_player_set(const RECVX_ENEMY_PLAYER& in){
    if(!player_asset_||!player_hp_owner_||!player_head_flags_)return false;
    player_.position={in.pos.x,in.pos.y,in.pos.z};player_.pitch=in.ax;player_.yaw=in.ay;player_.roll=in.az;
    collision_floor_=in.flr_no;*player_hp_owner_=in.hp;
    if(collision_events_){collision_events_->flags[13][0]=in.flg;collision_events_->player_flags=in.flg;
        collision_events_->flags[14][0]=in.stflg;collision_events_->flags[15][0]=in.flg2;
    }else {collision_flags_=in.flg;collision_status_=in.stflg;collision_flags2_=in.flg2;}
    player_at_flags_=in.at_flg;
    collision_mode_=unsigned(in.mode0)|(unsigned(in.mode1)<<8)|(unsigned(in.mode2)<<16)|(unsigned(in.mode3)<<24);
    player_ct0_=in.ct0;player_ct1_=in.ct1;player_ayp_=in.ayp;
    motion_=in.mtn_no;frame_=in.frm_no;player_step_=in.mtn_add;
    player_mtn_md_=in.mtn_md;player_hokan_count_=in.hokan_count;player_hokan_rate_=in.hokan_rate;
    player_enemy_motion_=in.mtn_enemy!=0;player_enemy_owner_=in.mtn_enemy_owner;
    player_weapon_=unsigned(in.wpnr_no);*player_head_flags_=in.exp1_flags;
    // The existing input owner must not run free locomotion during enemy
    // damage/death modes. This is ownership, not a replacement player phase.
    const bool driven=in.mode0==4||in.mode0==6;
    if(driven){enemy_player_driving_=true;grab_driving_=true;player_scripted=true;player_room_motion_=false;}
    else if(enemy_player_driving_){enemy_player_driving_=false;grab_driving_=false;player_scripted=false;}
    if(in.hp<0)player_killed_=true;
    return true;
}
unsigned RoomWorld::enemy_player_motion_frames(unsigned mtn,bool from_enemy)const{
    if(!player_asset_)return 0;
    if(!from_enemy)return player_asset_->has_motion(mtn)?player_asset_->motion_frames(mtn):0;
    // PlayerControl queries the new enemy library before its final player_set.
    // enow therefore supplies the current caller, not a constant zombie ID.
    const auto i=enemy_table_.enow;
    if(i>=RECVX_ENEMY_SLOTS||!enemy_table_.slot[i])return 0;
    const auto* clip=enemy_mn_work(owner_of(enemy_table_.slot[i]),mtn);
    return clip?clip->frames:0;
}
bool RoomWorld::enemy_player_joint(unsigned joint,BoneMatrix& out)const{
    if(joint>=player_joint_matrices_.size())return false;
    out=player_joint_matrices_[joint];return true;
}

void RoomWorld::bind_enemy_services(){
    RECVX_ENEMY_SERVICES& s=enemy_services_;std::memset(&s,0,sizeof s);
    s.gap=[](const char* name){enemy_world->unbound_enemy_.insert(name);};
    s.random_next=[]()->int32_t{return recvx_core_random_next();};
    s.player_get=[](RECVX_ENEMY_PLAYER* out)->int{return out&&enemy_world->enemy_player_get(*out)?1:0;};
    s.player_set=[](const RECVX_ENEMY_PLAYER* in){if(!in||!enemy_world->enemy_player_set(*in))enemy_world->unbound_enemy_.insert("player_set: Inventory/EXP1/PlayerAsset nicht gebunden");};
    s.player_joint_matrix=[](uint32_t joint,float* out)->int{BoneMatrix m;if(!enemy_world->enemy_player_joint(joint,m))return 0;std::memcpy(out,m.value,sizeof m.value);return 1;};
    s.player_joint_point=[](uint32_t joint,const RECVX_ENEMY_VEC* local,RECVX_ENEMY_VEC* out)->int{
        BoneMatrix m;if(!enemy_world->enemy_player_joint(joint,m))return 0;
        NJS_POINT3 p{local->x,local->y,local->z},q;njCalcPoint(&m.value,&p,&q);*out={q.x,q.y,q.z};return 1;};
    s.player_motion_frames=[](uint32_t mtn,int enemy)->uint32_t{return enemy_world->enemy_player_motion_frames(mtn,enemy!=0);};
    // bhCheckWallType (hitchk.c) over the room-then-mobile geometry.
    s.wall_type=[](const RECVX_ENEMY_VEC* p,uint32_t flg,float r,float h)->int{return enemy_world->collision_contact({p->x,p->y,p->z},flg,r,h)?1:0;};
    // bhCollisionCheckLine (1), Line2 (2) and Line3 (3). `id` is the 1-based
    // place of the hit surface in the room-then-mobile list, the native form
    // of the original EXP0_ATR pointer comparison.
    s.line=[](uint32_t variant,const RECVX_ENEMY_VEC* from,RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* normal,uint32_t flg,int floor,RECVX_ENEMY_ATR* rec,uint32_t* id)->int{
        auto* world=enemy_world;NJS_POINT3 start{from->x,from->y,from->z},end{to->x,to->y,to->z},n{};std::optional<CollisionWall> hit;
        if(variant==1)hit=world->collision_line(start,end,n);
        else if(variant==2||variant==3)hit=world->collision_line(start,end,n,flg,floor,variant==3);
        else {world->unbound_enemy_.insert("bhCollisionCheckLine: unbekannte Variante");return 0;}
        if(!hit)return 0;
        *to={end.x,end.y,end.z};if(normal)*normal={n.x,n.y,n.z};
        if(rec)*rec=atr_of(collision_record(*hit));
        if(id){*id=0;const auto geometry=world->collision_geometry();for(std::size_t i=0;i<geometry.size();++i)if(same_wall(geometry[i],*hit)){*id=std::uint32_t(i+1);break;}}
        return 1;};
    s.ground=[](const RECVX_ENEMY_VEC* p)->float{return enemy_world->combat_ground({p->x,p->y,p->z});};
    s.floor_num=[](float py)->int{return enemy_world->floor_at({0,py,0});};
    s.grand=[](uint32_t i)->float{return i<enemy_world->floors_.size()?enemy_world->floors_[i]:0.f;};
    // sdfunc.c RequestEnemySe/Ex, ChechPlayEnemySe, CallPlayerVoice. The
    // AudioMixer has no Get3DSoundParameter input; the position is reported.
    s.enemy_se=[](uint32_t enemy,const RECVX_ENEMY_VEC*,int se){enemy_world->unbound_enemy_.insert("RequestEnemySeBasic:Get3DSoundParameter (Lage)");enemy_world->audio_.request_enemy_se(enemy,se);};
    s.enemy_se_ex=[](uint32_t enemy,const RECVX_ENEMY_VEC*,int se,int fade){enemy_world->unbound_enemy_.insert("RequestEnemySeBasic:Get3DSoundParameter (Lage)");enemy_world->audio_.request_enemy_se(enemy,se,fade);};
    s.enemy_se_playing=[](uint32_t enemy,int se)->int{return enemy_world->audio_.enemy_se_playing(enemy,se)?1:0;};
    s.player_voice=[](int se){enemy_world->audio_.play_player_voice(se);};
    // CallSystemSe(0, SeNo) -> CallSystemSeBasic: bit 31 is the ExPlayMidi path.
    s.system_se=[](int se){if(std::uint32_t(se)&0x80000000u)enemy_world->audio_.play_system_midi(std::uint32_t(se));else enemy_world->unbound_enemy_.insert("CallSystemSeBasic ohne Bit 31");};
    s.obj_num=[](const RECVX_ENEMY_WORK* w)->uint32_t{const auto& e=owner_of(w);return e.enemy_asset?std::uint32_t(e.enemy_asset->bones()):0u;};
    s.motion_frames=[](const RECVX_ENEMY_WORK* w,uint32_t mtn)->uint32_t{const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);return clip?clip->frames:0u;};
    s.motion_obj_num=[](const RECVX_ENEMY_WORK* w,uint32_t mtn)->uint32_t{const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);return clip?std::uint32_t(clip->positions.size()):0u;};
    s.set_motion=[](RECVX_ENEMY_WORK* w,int add,uint32_t md)->int{
        auto* world=enemy_world;auto& e=owner_of(w);
        if(!e.enemy_asset){world->unbound_enemy_.insert("bhSetMotion: mlwP fehlt (kein Modell)");return 0;}
        if(w->mtn_no>=512){world->unbound_enemy_.insert("bhSetMotion: mtn_no ausserhalb mnwP[512]");return 0;}
        MotionWork m{w->frm_no,w->frm_mode,w->hokan_count,w->mtn_attr,w->hokan_rate};std::string gap;
        const int r=set_motion(m,add,md,world->enemy_mn_work(e,w->mtn_no),static_cast<const std::int8_t*>(w->mtn_tp),e.enemy_obj,e.enemy_owp_flg,&gap);
        w->frm_no=m.frm_no;w->frm_mode=m.frm_mode;w->hokan_count=m.hokan_count;w->mtn_attr=m.mtn_attr;
        if(!gap.empty())world->unbound_enemy_.insert(gap);
        return r;};
    s.calc_model=[](RECVX_ENEMY_WORK* w){enemy_world->calc_enemy_model(owner_of(w));};
    s.joint_matrix=[](const RECVX_ENEMY_WORK* w,uint32_t joint,float* out)->int{const auto& e=owner_of(w);if(!e.enemy_calculated||joint>=e.enemy_owp_mtx.size())return 0;std::memcpy(out,e.enemy_owp_mtx[joint].value,16*sizeof(float));return 1;};
    s.joint_point=[](const RECVX_ENEMY_WORK* w,uint32_t joint,const RECVX_ENEMY_VEC* local,RECVX_ENEMY_VEC* out)->int{
        const auto& e=owner_of(w);if(!e.enemy_calculated||joint>=e.enemy_owp_mtx.size())return 0;
        NJS_MATRIX m;std::memcpy(m,e.enemy_owp_mtx[joint].value,sizeof m);NJS_POINT3 a{local->x,local->y,local->z},b;njCalcPoint(&m,&a,&b);*out={b.x,b.y,b.z};return 1;};
    // MN_WORK md2P[0].p[0] / p[1] of a motion on the integer frame.
    s.motion_root_position=[](const RECVX_ENEMY_WORK* w,uint32_t mtn,uint32_t frame,RECVX_ENEMY_VEC* out)->int{
        const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);if(!clip||clip->positions.empty()||clip->positions[0].empty()||frame>=clip->positions[0].size())return 0;
        const auto& k=clip->positions[0][frame];*out={k.x,k.y,k.z};return 1;};
    s.motion_root_angles=[](const RECVX_ENEMY_WORK* w,uint32_t mtn,uint32_t frame,int32_t* out)->int{
        const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);if(!clip||clip->angles.empty()||clip->angles[0].empty()||frame>=clip->angles[0].size())return 0;
        const auto& k=clip->angles[0][frame];out[0]=k[0];out[1]=k[1];out[2]=k[2];return 1;};
    s.model_root_position=[](const RECVX_ENEMY_WORK* w,RECVX_ENEMY_VEC* out)->int{const auto& e=owner_of(w);if(e.enemy_obj.empty())return 0;const auto& p=e.enemy_obj[0].position;*out={p.x,p.y,p.z};return 1;};
    s.model_node_hidden=[](RECVX_ENEMY_WORK* w,uint32_t node,int hidden){auto& e=owner_of(w);if(node>=e.enemy_evalflags.size()){enemy_world->unbound_enemy_.insert("mlwP->objP[node].evalflags ausserhalb des Modells");return;}if(hidden)e.enemy_evalflags[node]|=8u;else e.enemy_evalflags[node]&=~8u;};
    s.enemy_count=[]()->uint32_t{return enemy_world->enemy_table_.ewk_n;};
    s.enemy_at=[](uint32_t i)->RECVX_ENEMY_WORK*{return i<RECVX_ENEMY_SLOTS?enemy_world->enemy_table_.slot[i]:nullptr;};
    s.enemy_now=[]()->uint32_t{return enemy_world->enemy_table_.enow;};
    // event.c:440 flag banks: 10 cb_flg, 4 rm_flg, 5 st_flg, 6 sp_flg.
    s.system_flags=[](uint32_t which)->uint32_t{auto* world=enemy_world;static const unsigned bank[4]={10,4,5,6};
        if(which>=4){world->unbound_enemy_.insert("system_flags: unbekannte Flagbank");return 0;}
        if(world->collision_events_)return world->collision_events_->flags[bank[which]][0];
        if(which==3)return world->simulation_flags_;
        world->unbound_enemy_.insert("system_flags: EventVm nicht gebunden");return 0;};
    s.system_flags_or=[](uint32_t which,uint32_t bits){auto* world=enemy_world;static const unsigned bank[4]={10,4,5,6};
        if(which<4&&world->collision_events_)world->collision_events_->flags[bank[which]][0]|=bits;else world->unbound_enemy_.insert("system_flags_or: EventVm nicht gebunden");};
    s.explode=[](const RECVX_ENEMY_VEC* p){enemy_world->explode({p->x,p->y,p->z},{});};
    // sys->mwalp/mwal_n: one more mobile ATR surface (no bound in the original).
    s.mobile_wall_alloc=[]()->int{auto* world=enemy_world;world->enemy_mobile_walls_.push_back(CollisionWall{});world->update_ground_geometry();return int(world->enemy_mobile_walls_.size()-1);};
    s.mobile_wall_get=[](int index,RECVX_ENEMY_ATR* out)->int{const auto& walls=enemy_world->enemy_mobile_walls_;if(index<0||std::size_t(index)>=walls.size())return 0;
        const auto& a=walls[std::size_t(index)];*out=RECVX_ENEMY_ATR{};out->flg=a.flags;out->type=a.type;out->attr=a.attributes;out->flr_no=a.floor;out->px=a.position.x;out->py=a.position.y;out->pz=a.position.z;out->w=a.extent.x;out->h=a.extent.y;out->d=a.extent.z;
        out->prm0=std::uint8_t(a.parameters[0]);out->prm1=std::uint8_t(a.parameters[1]);out->prm2=std::uint8_t(a.parameters[2]);out->prm3=std::uint8_t(a.parameters[3]);out->id=std::uint8_t(a.id);return 1;};
    s.mobile_wall_set=[](int index,const RECVX_ENEMY_ATR* in){auto* world=enemy_world;auto& walls=world->enemy_mobile_walls_;if(index<0||std::size_t(index)>=walls.size()){world->unbound_enemy_.insert("sys->mwalp: Index ohne Zuteilung");return;}
        walls[std::size_t(index)]={in->flg,in->type,in->id,in->flr_no,in->attr,{in->px,in->py,in->pz},{in->w,in->h,in->d},{in->prm0,in->prm1,in->prm2,in->prm3}};world->update_ground_geometry();};
    // subpl.c:143 bhEne_Event belongs to the existing subplayer apparatus.
    s.event=recvx_subplayer_enemy_event;
    // zonzon1.c:591 bhEne_SetWeponAtr: capsule from two owP matrices.
    s.weapon_atr=[](RECVX_ENEMY_WORK* w,int j1,int j2,float r){
        const auto& e=owner_of(w);const auto a=std::size_t(std::int8_t(j1)),b=std::size_t(std::int8_t(j2));
        if(!e.enemy_calculated||a>=e.enemy_owp_mtx.size()||b>=e.enemy_owp_mtx.size()){enemy_world->unbound_enemy_.insert("bhEne_SetWeponAtr: owP ausserhalb/noch nicht berechnet");return;}
        const float* m1=e.enemy_owp_mtx[a].value;const float* m2=e.enemy_owp_mtx[b].value;
        w->watr.c1={m1[12],m1[13],m1[14]};w->watr.c2={m2[12],m2[13],m2[14]};w->watr.r=r;};
    // en03.c:5220 bhEne03_Collision: one wall pass for every enemy routine.
    s.collision_works=[](RECVX_ENEMY_WORK* w)->int{return recvx_en03_collision(w,nullptr);};
    RECVX_ENEMY_EXT& x=enemy_ext_;std::memset(&x,0,sizeof x);
    x.gap=[](const char* name){enemy_world->unbound_enemy_.insert(name);};
    x.set_call_func=[](uint32_t id,RECVX_ENEMY_PROC proc){recvx_enemy_set_call_func(&enemy_world->enemy_dispatch_,id,proc);};
    // rom->walp[0..wal_n-1] then sys->mwalp[0..mwal_n-1]; wal_count takes the
    // snapshot the following wal_at loop reads.
    x.wal_count=[]()->int{auto* world=enemy_world;world->enemy_wall_cache_=world->collision_geometry();return int(world->enemy_wall_cache_.size());};
    x.wal_at=[](uint32_t i,RECVX_T3_ATR* out)->int{const auto& walls=enemy_world->enemy_wall_cache_;if(i>=walls.size())return 0;const auto& a=walls[i];
        *out=RECVX_T3_ATR{};out->flg=std::uint8_t(a.flags);out->type=std::uint8_t(a.type);out->id=std::uint8_t(a.id);out->flr_no=std::int8_t(a.floor);out->attr=a.attributes;
        out->px=a.position.x;out->py=a.position.y;out->pz=a.position.z;out->w=a.extent.x;out->h=a.extent.y;out->d=a.extent.z;
        out->prm0=std::uint8_t(a.parameters[0]);out->prm1=std::uint8_t(a.parameters[1]);out->prm2=std::uint8_t(a.parameters[2]);out->prm3=std::uint8_t(a.parameters[3]);return 1;};
    x.room_height=[]()->float{return enemy_world->room_height_;};
    // eneset.c:906 bhEne_CallocWork. Room lifetime; the cursor is aligned to
    // 64 before the block, the limit test uses the unaligned cursor.
    s.calloc_work=[](uint32_t size)->void*{auto* world=enemy_world;
        if(world->enemy_exp_used_+size>enemy_exp_budget)return nullptr;
        world->enemy_exp_used_=((world->enemy_exp_used_+63)&~std::size_t(63))+size;
        world->enemy_exp_blocks_.push_back(std::unique_ptr<unsigned char[]>(new unsigned char[size?size:1]()));
        return world->enemy_exp_blocks_.back().get();};
}

const MotionClip* RoomWorld::enemy_mn_work(const WorldEntity& e,std::uint32_t mtn)const{
    // mnwP is sys->rmthp (bhEneMotionSet) or the work's own emtp[id].
    if(e.enemy.mtn_room){if(mtn<room_motions_.size()&&room_motions_[mtn].frames)return &room_motions_[mtn];return nullptr;}
    const auto library=enemy_motions_.find(e.identity);
    if(library==enemy_motions_.end()||mtn>=library->second.entries.size()||!library->second.entries[mtn].second)return nullptr;
    return &enemy_library_clip(e.identity,mtn);
}

// room.c:114-126 walks rom->enep in order and calls bhSetEneMdl, whose first
// act is bhSetEnemy; the slot index therefore follows the room order.
void RoomWorld::init_enemy_works(){
    EnemyWorldScope scope(this);
    bind_enemy_services();
    static const auto en01_d = recvx_en01_default_data();
    recvx_en01_set_data(&en01_d);
    RECVX_EN01_CONTEXT en01_ctx{};
    en01_ctx.table = &enemy_table_;
    en01_ctx.services = &enemy_services_;
    en01_ctx.game_mode = enemy_game_mode_;
    recvx_en01_set_context(&en01_ctx);
    recvx_en04_set_data(recvx_en04_default_data());
    recvx_enemy_table_init(&enemy_table_);   // bhInitEnemy
    recvx_enemy_dispatch_init(&enemy_dispatch_);
    enemy_exp_blocks_.clear();enemy_exp_used_=0;enemy_mobile_walls_.clear();
    BinaryView v(room_);const auto ene_count=v.u32(header_+0x88),enep=v.u32(header_+8);
    for(unsigned i=0;i<ene_count;++i){const auto p=enep+i*36;if(!(v.u32(p)&1))continue;
        auto& e=entity(1,i);RECVX_ENEMY_TEMPLATE t{};
        t.flg=v.u32(p);t.id=v.u16(p+4);t.type=v.u16(p+6);t.flr_no=std::int8_t(v.u8(p+8));t.mdlver=v.u8(p+9);
        t.px=v.f32(p+12);t.py=v.f32(p+16);t.pz=v.f32(p+20);t.ay=std::int16_t(v.u16(p+28));
        if(!recvx_enemy_set(&enemy_table_,&e.enemy,&t,&e,i,&enemy_services_)){unbound_enemy_.insert("eneset.c:170 ene[128] voll");continue;}
        e.enemy_bound=true;e.enemy_asset=nullptr;
        for(const auto& m:models_)if(m.kind==1&&m.index==i&&m.actor){e.enemy_asset=m.actor.get();break;}
        // bhMlbBinRealize: objP starts as the model's own node data; owP is zeroed.
        if(e.enemy_asset){e.enemy_obj=e.enemy_asset->bind_pose();e.enemy_evalflags=e.enemy_asset->node_evalflags();e.enemy_owp_flg.assign(e.enemy_obj.size(),0u);e.enemy_owp_mtx.assign(e.enemy_obj.size(),BoneMatrix{});
            // room.c:445-449 ruft fuer jeden Raumgegner mit id > 90 bhInitMask.
            // face.c:52-60 setzt dort mlwP->owP[6..14].flg |= 0x4 (ohne mskp
            // nur owP[6..10], face.c:21-25). Motion.c:131 SetMtnNormal
            // ueberspringt genau diese Knoten, sodass die Motionkanaele erst
            // dahinter weiterzaehlen; ohne die Markierung landen die Kanaele
            // der Arme und Beine auf den maskengesteuerten Kopfknoten und die
            // Gliedmassen behalten ihre Bindepose.
            // Die von fmCnkInitContext eingefuegten Tochterknoten (tooth/tang/
            // eyes) liegen hier nicht in objP, sondern nur in der logischen
            // Knotenliste des Modells; die Originalindizes 6..14 werden daher
            // ueber dieselbe Zuordnung auf ihre Modellknoten abgebildet.
            if(t.id>90){const unsigned last=e.enemy_asset->face()?14u:10u;
                for(unsigned logical=6;logical<=last;++logical){
                    unsigned physical=0;
                    try{physical=e.enemy_asset->model_index(logical);}
                    catch(const std::runtime_error&){unbound_enemy_.insert("bhInitMask: owP[6..14] ausserhalb des Modellbaums");break;}
                    if(physical<e.enemy_owp_flg.size())e.enemy_owp_flg[physical]|=4u;}}}
        e.enemy_native=t.id<RECVX_ENEMY_IDS&&enemy_dispatch_.jump[t.id]!=nullptr;
        if(e.enemy_native)publish_enemy(e);
    }
}

// Script writes land in the entity (bind()); before the next pass the
// changed groups are taken over through the one allowed direction.
void RoomWorld::adopt_enemy_script(WorldEntity& e){
    auto& m=e.enemy_mirror;RECVX_ENEMY_PUBLISH in;recvx_enemy_publish(&e.enemy,&in);std::uint32_t mask=0;
    const auto& p=e.transform.position;
    if(p.x!=m.position.x||p.y!=m.position.y||p.z!=m.position.z||e.transform.yaw!=m.yaw||e.angles[0]!=m.angles[0]||e.angles[2]!=m.angles[2]||e.object_floor!=m.floor){
        in.px=p.x;in.py=p.y;in.pz=p.z;in.ax=e.angles[0];in.ay=e.transform.yaw!=m.yaw?e.transform.yaw:e.angles[1];in.az=e.angles[2];in.flr_no=std::int8_t(e.object_floor);mask|=RECVX_ENEMY_ADOPT_POSE;}
    if(e.motion!=m.motion||e.room_motion!=m.room_motion||e.frame!=m.frame||e.step!=m.step){
        in.mtn_no=e.motion;in.mtn_room=e.room_motion?1:0;in.frm_no=std::int32_t(e.frame);in.mtn_add=std::int32_t(e.step);mask|=RECVX_ENEMY_ADOPT_MOTION;}
    if(e.hp!=m.hp){in.hp=e.hp;mask|=RECVX_ENEMY_ADOPT_HP;}
    // bhEneRenderSet mdflg 0x200, bhEneSetCk stflg 0x1000000, bhEneLookFlgSet flg 0x8000.
    if(e.hidden!=m.hidden||e.inactive!=m.inactive||e.look!=m.look){
        if(e.hidden!=m.hidden){if(e.hidden)in.mdflg|=0x200u;else in.mdflg&=~0x200u;}
        if(e.inactive!=m.inactive){if(e.inactive)in.stflg|=0x1000000u;else in.stflg&=~0x1000000u;}
        if(e.look!=m.look){if(e.look)in.flg|=0x8000u;else in.flg&=~0x8000u;}
        mask|=RECVX_ENEMY_ADOPT_FLAGS;}
    if(mask)recvx_enemy_adopt(&e.enemy,&in,mask);
    m.position=p;m.angles=e.angles;m.yaw=e.transform.yaw;m.floor=e.object_floor;m.motion=e.motion;m.room_motion=e.room_motion;m.frame=e.frame;m.step=e.step;m.hp=e.hp;m.hidden=e.hidden;m.inactive=e.inactive;m.look=e.look;
}

// enemy_work.h B: one publish per pass, where the original calls bhCalcModel.
void RoomWorld::publish_enemy(WorldEntity& e){
    RECVX_ENEMY_PUBLISH out;recvx_enemy_publish(&e.enemy,&out);
    e.transform.position={out.px,out.py,out.pz};e.transform.yaw=out.ay;e.angles={out.ax,out.ay,out.az};e.object_floor=out.flr_no;e.type=out.type;e.hp=out.hp;
    if(!e.scripted){e.motion=out.mtn_no;e.room_motion=out.mtn_room!=0;e.frame=out.frm_no;e.step=out.mtn_add;}
    e.dead=(out.flg&2)!=0;e.inactive=(out.stflg&0x1000000u)!=0;e.hidden=(out.mdflg&0x200u)!=0;e.look=(out.flg&0x8000u)!=0;
    auto& m=e.enemy_mirror;m.position=e.transform.position;m.angles=e.angles;m.yaw=e.transform.yaw;m.floor=e.object_floor;m.motion=e.motion;m.room_motion=e.room_motion;m.frame=e.frame;m.step=e.step;m.hp=e.hp;m.hidden=e.hidden;m.inactive=e.inactive;m.look=e.look;
}

// MdlPut.c:240 bhCalcModel: flg 0x80 hangs below lkwkp's owP[lkono], flg
// 0x1000 alone keeps the existing root matrix (enemy_work.h root_mtx).
void RoomWorld::calc_enemy_model(WorldEntity& e){
    auto& w=e.enemy;
    if(!e.enemy_asset){unbound_enemy_.insert("bhCalcModel: mlwP fehlt (kein Modell)");return;}
    NJS_MATRIX root;
    if(w.flg&0x80u){
        if(!w.lkwkp){unbound_enemy_.insert("bhCalcModel: flg 0x80 ohne lkwkp (Erzeugerluecke)");return;}
        const auto& parent=owner_of(w.lkwkp);
        if(!parent.enemy_calculated||w.lkono<0||std::size_t(w.lkono)>=parent.enemy_owp_mtx.size()){unbound_enemy_.insert("bhCalcModel: lkwkp->owP[lkono] noch nicht berechnet");return;}
        std::memcpy(root,parent.enemy_owp_mtx[std::size_t(w.lkono)].value,sizeof root);
        njTranslate(&root,w.lox,w.loy,w.loz);
        if(w.flg&0x1000u)njUnitRotPortion(&root);
        njRotateXYZ(&root,w.ax,w.ay,w.az);std::memcpy(w.root_mtx,root,sizeof root);
    }else if(!(w.flg&0x1000u)){
        njUnitMatrix(&root);njTranslate(&root,w.px,w.py,w.pz);njRotateXYZ(&root,w.ax,w.ay,w.az);std::memcpy(w.root_mtx,root,sizeof root);
    }else std::memcpy(root,w.root_mtx,sizeof root);
    e.enemy_owp_mtx=e.enemy_asset->tree_matrices(e.enemy_obj,root,e.enemy_owp_flg,e.enemy_owp_mtx);
    e.enemy_calculated=true;
    publish_enemy(e);
}

// eneset.c:344 bhControlEnemy over the table; native routines only. The
// legacy en01/en04 store keeps its own tick until C03/C04 replace it.
void RoomWorld::control_enemy_works(){
    EnemyWorldScope scope(this);
    for(auto& pair:entities_)if(pair.first.first==1&&pair.second.enemy_bound)adopt_enemy_script(pair.second);
    // The T3 supplement is a process-wide binding in en03_lib.c; it is only
    // valid while this world runs its pass.
    recvx_enemy_ext_bind(&enemy_ext_);
    RECVX_EN01_CONTEXT en01_ctx{};
    en01_ctx.table = &enemy_table_;
    en01_ctx.services = &enemy_services_;
    en01_ctx.game_mode = enemy_game_mode_;
    recvx_en01_set_context(&en01_ctx);
    recvx_enemy_control(&enemy_table_,&enemy_dispatch_,simulation_flags_,&enemy_services_);
    recvx_en01_set_context(nullptr);
    recvx_enemy_ext_bind(nullptr);
}
}
