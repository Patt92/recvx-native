#include <optional>
#include "world.h"
#include "effect_shadow.h"
#include "enemies/en11.h"
#include "binary_view.h"
#include "core_random.h"
#include "motion_set.h"
#include "subplayer.h"
#include "enemy_dispatch.h"
#include "enemies/en01_core.h"
#include "enemies/en04.h"
#include "enemies/en04_tables.h"
#include "enemies/en12.h"
#include "enemies/en14.h"
#include "enemies/en18.h"
#include "enemies/en30.h"
#include "combat_geometry.h"
#include "weapon_tables.h"
#include <algorithm>
#include <cmath>
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
// mlwP: der Objektbaum, den ein Work liest und schreibt. Ein Kind mit
// `epp->mlwP = epw->mdl` (en01.c:2827) hat keinen eigenen.
WorldEntity& tree_of(WorldEntity& e){return e.model_owner?*e.model_owner:e;}
const WorldEntity& tree_of(const WorldEntity& e){return e.model_owner?*e.model_owner:e;}

// --- F03: Morph-Auswahl der portierten Gegner-Zustandsmaschinen -------------
// njplus.c npCalcMorphing blendet obj_a gegen obj_b. Beide sind realisierte
// Modellstrings des Works: mbp[k] und mdl[k].objP sind derselbe Eintrag
// (room.c bhSetEneMdl: epp->mbp[mdl_n] = epp->mlwP->objP). Im Original liegen
// obj_a/obj_b am BH_PWORK; nativ ist das WorldEntity der Work-Zustand.
// en01.c:2749 / en12.c:800 / en14.c:457 / en30.c:176 waehlen (0,1);
// en18 haelt stattdessen Modellhandles in seinem exp0-Block.
void morph_pair_set(RECVX_ENEMY_WORK* w,unsigned a,unsigned b){
    if(!w)return;auto& e=owner_of(w);e.morph_obj_a=a;e.morph_obj_b=b;
}
const PlayerAsset* morph_variant(const WorldEntity& e,unsigned model){
    if(!e.enemy_asset)return nullptr;
    return model==0?e.enemy_asset:e.enemy_asset->model(model);
}
int morph_variant_index(const WorldEntity& e,const PlayerAsset* variant){
    if(!e.enemy_asset||!variant)return -1;
    if(variant==e.enemy_asset)return 0;
    for(unsigned i=1;i<e.enemy_asset->model_count();++i)if(e.enemy_asset->model(i)==variant)return int(i);
    return -1;
}
void en01_select_morph_pair(RECVX_ENEMY_WORK* w){morph_pair_set(w,0,1);}
void en12_morph_sources(RECVX_ENEMY_WORK* w,uint32_t a,uint32_t b){morph_pair_set(w,a,b);}
void en14_morph_models(RECVX_ENEMY_WORK* w,uint32_t a,uint32_t b){morph_pair_set(w,a,b);}
int en30_morph_sources(RECVX_ENEMY_WORK* w,uint32_t a,uint32_t b){morph_pair_set(w,a,b);return 1;}
// en30.c:415/:461/:829: obj_a[node].pos[1] = obj_b[node].pos[1] = y. obj_a ist
// mlwP->objP = mdl[0] und traegt die Pose, die bhCalcTree in owP legt; mdl[1]
// wird nie bhCalcTree unterzogen und geht nur ueber npTransform in die
// Rohstroemung ein.
int en30_morph_node_set_y(RECVX_ENEMY_WORK* w,uint32_t node,float y){
    if(!w)return 0;auto& e=owner_of(w);
    if(node>=e.enemy_obj.size())return 0;
    e.enemy_obj[node].position.y=y;return 1;
}
// en30.c:147-153: mdl[model].objP[i].pos fuer i < obj_num. Vor dem ersten
// bhCalcModel ist objP die realisierte Ruhelage des Modells.
uint32_t en30_model_node_rest(const RECVX_ENEMY_WORK* w,uint32_t model,RECVX_ENEMY_VEC* out,uint32_t max){
    if(!w||!out)return 0;const auto& e=owner_of(w);const auto* asset=morph_variant(e,model);
    if(!asset)return 0;const std::size_t n=std::min<std::size_t>(max,asset->bones());
    for(std::size_t i=0;i<n;++i){const auto p=asset->bind_position(i);out[i]={p.x,p.y,p.z};}
    return uint32_t(n);
}
int en30_model_node_set_y(RECVX_ENEMY_WORK* w,uint32_t model,uint32_t node,float y){
    if(!w)return 0;
    // Nur mdl[0] traegt die gezeichnete Pose; mdl[1] hat nativ keinen
    // eigenen Baum, der geschrieben werden koennte.
    if(model!=0)return 0;
    auto& e=owner_of(w);
    if(node>=e.enemy_obj.size())return 0;
    e.enemy_obj[node].position.y=y;return 1;
}
// en30.c:161-164/:809-810: objP[node].evalflags Bit 0x8 je Modell.
int en30_model_node_hidden(RECVX_ENEMY_WORK* w,uint32_t model,uint32_t node,int hidden){
    if(!w)return 0;auto& e=owner_of(w);
    if(model==0){
        if(node>=e.enemy_evalflags.size())return 0;
        if(hidden)e.enemy_evalflags[node]|=8u;else e.enemy_evalflags[node]&=~8u;
        return 1;
    }
    const auto* asset=morph_variant(e,model);if(!asset)return 0;
    if(e.morph_model_flags.size()<model)e.morph_model_flags.resize(model);
    auto& flags=e.morph_model_flags[model-1];
    if(flags.empty())flags=asset->node_evalflags();
    if(node>=flags.size())return 0;
    if(hidden)flags[node]|=8u;else flags[node]&=~8u;return 1;
}
// en18.c:425-427 haelt die objP von mdl[0..2] in exp0; das Handle ist nativ der
// stabile Assetzeiger. select_root aendert NUR mlwP->objP.
void* en18_model_root(const RECVX_ENEMY_WORK* w,uint32_t slot){
    if(!w)return nullptr;
    const auto* asset=morph_variant(owner_of(w),slot);
    return reinterpret_cast<void*>(const_cast<PlayerAsset*>(asset));
}
int en18_select_root(RECVX_ENEMY_WORK* w,void* root){
    if(!w)return 0;auto& e=owner_of(w);
    const auto index=morph_variant_index(e,static_cast<const PlayerAsset*>(root));
    if(index<0)return 0;
    e.morph_obj_a=unsigned(index);return 1;
}
int en18_morph_pair(RECVX_ENEMY_WORK* w,void* root_a,void* root_b){
    if(!w)return 0;auto& e=owner_of(w);
    const auto a=morph_variant_index(e,static_cast<const PlayerAsset*>(root_a));
    const auto b=morph_variant_index(e,static_cast<const PlayerAsset*>(root_b));
    if(a<0||b<0)return 0;
    e.morph_obj_a=unsigned(a);e.morph_obj_b=unsigned(b);return 1;
}
// Ein Satz je Modul. Alle nicht gesetzten Dienste bleiben NULL; die
// Mechanismen melden sie dann namentlich als Luecke, wie bisher.
const RECVX_EN12_SERVICES& en12_services(){static const RECVX_EN12_SERVICES s=[]{
    RECVX_EN12_SERVICES t{};t.morph_sources=en12_morph_sources;return t;}();return s;}
const RECVX_EN14_SERVICES& en14_services(){static const RECVX_EN14_SERVICES s=[]{
    RECVX_EN14_SERVICES t{};t.morph_models=en14_morph_models;return t;}();return s;}
const RECVX_EN18_SERVICES& en18_services(){static const RECVX_EN18_SERVICES s=[]{
    RECVX_EN18_SERVICES t{};t.model_root=en18_model_root;t.select_root=en18_select_root;t.morph_pair=en18_morph_pair;return t;}();return s;}
const RECVX_EN30_SERVICES& en30_services(){static const RECVX_EN30_SERVICES s=[]{
    RECVX_EN30_SERVICES t{};t.model_node_rest=en30_model_node_rest;t.model_node_set_y=en30_model_node_set_y;
    t.model_node_hidden=en30_model_node_hidden;t.morph_node_set_y=en30_morph_node_set_y;t.morph_sources=en30_morph_sources;return t;}();return s;}
const RECVX_EN14_SERVICES* en14_resolver(const RECVX_ENEMY_WORK*){return &en14_services();}
// Die Kind-/Baumdienste brauchen den Weltbesitzer und werden deshalb in
// RoomWorld::bind_enemy_services nachgetragen.
RECVX_EN01_SERVICES& en01_services(){static RECVX_EN01_SERVICES s=[]{
    RECVX_EN01_SERVICES t{};t.select_morph_pair=en01_select_morph_pair;return t;}();return s;}
// EN04 shares the original routines EN01 already reaches (en04.h: "denselben
// Anbieter binden"): npSetAllMatColor and the local objP pos/ang of its own
// tree. Without set_material_color bhEne04_Init (en04.c:993) stopped before
// mode0 = 1 and re-ran every pass, rolling a new hp from hp_tbl each time.
// Entries without an existing provider stay NULL and are reported by name.
RECVX_EN04_SERVICES& en04_services(){static RECVX_EN04_SERVICES s{};return s;}
RECVX_ENEMY_ATR atr_of(const RECVX_STAIR_AREA& a){
    RECVX_ENEMY_ATR out{};out.flg=a.flg;out.type=a.type;out.attr=a.attr;out.flr_no=a.flr_no;out.px=a.px;out.py=a.py;out.pz=a.pz;out.w=a.w;out.h=a.h;out.d=a.d;
    out.prm0=a.prm0;out.prm1=a.prm1;out.prm2=a.prm2;out.prm3=a.prm3;out.id=a.id;return out;
}
RECVX_ENEMY_ATR atr_of(const CollisionWall& a){
    RECVX_ENEMY_ATR out{};out.flg=a.flags;out.type=a.type;out.attr=a.attributes;out.flr_no=a.floor;out.px=a.position.x;out.py=a.position.y;out.pz=a.position.z;out.w=a.extent.x;out.h=a.extent.y;out.d=a.extent.z;
    out.prm0=std::uint8_t(a.parameters[0]);out.prm1=std::uint8_t(a.parameters[1]);out.prm2=std::uint8_t(a.parameters[2]);out.prm3=std::uint8_t(a.parameters[3]);out.id=std::uint8_t(a.id);return out;
}
// zonzon.c:402 bhEne_PosCheck(px, pz, x, z, w, h).
bool ene_pos_check(float px,float pz,float x,float z,float w,float h){
    return x<=px&&!(x+w<px)&&z<=pz&&!(z+h<pz);
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
    if(!enemy_grab_)out.flg|=2u;
    out.flg2=collision_events_?collision_events_->flags[15][0]:collision_flags2_;
    out.stflg=collision_events_?collision_events_->flags[14][0]:collision_status_;
    if(player_hidden||event_camera||(player_scripted&&!grab_driving_))out.stflg|=0x80000000u;
    out.at_flg=player_at_flags_;
    out.mode0=std::uint8_t(collision_mode_);out.mode1=std::uint8_t(collision_mode_>>8);
    out.mode2=std::uint8_t(collision_mode_>>16);out.mode3=std::uint8_t(collision_mode_>>24);
    out.ct0=player_ct0_;out.ct1=player_ct1_;out.ayp=player_ayp_;
    out.mtn_no=unsigned(motion_);out.frm_no=std::int32_t(frame_);out.mtn_add=std::int32_t(player_step_);
    out.mtn_md=player_mtn_md_;out.hokan_count=player_hokan_count_;out.hokan_rate=player_hokan_rate_;
    out.mtn_enemy=player_enemy_motion_?1:0;out.mtn_enemy_owner=player_enemy_owner_;out.mtn_enemy_work=player_enemy_work_;
    out.wpnr_no=int(player_weapon_);out.ply_id=unsigned(player_character_);out.exp1_flags=*player_head_flags_;
    // player.c:199/708: PlyInfo[sys->ply_id] = {3.5, 16.5/17.5}; car/cah sind
    // ar/ah - 1.0f. ply_id 0 ist Claires Mass, 1..3 das der uebrigen Figuren.
    out.car=2.5f;
    out.cah=(player_character_?17.5f:16.5f)-1.0f;
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
    player_enemy_motion_=in.mtn_enemy!=0;player_enemy_owner_=in.mtn_enemy_owner;player_enemy_work_=in.mtn_enemy?in.mtn_enemy_work:nullptr;
    player_weapon_=unsigned(in.wpnr_no);*player_head_flags_=in.exp1_flags;
    // The existing input owner must not run free locomotion during enemy
    // damage/death modes. This is ownership, not a replacement player phase.
    const bool driven=in.mode0==4||in.mode0==6;
    if(driven){enemy_player_driving_=true;grab_driving_=true;player_scripted=true;player_room_motion_=false;}
    else if(enemy_player_driving_){enemy_player_driving_=false;grab_driving_=false;player_scripted=false;player_enemy_motion_=false;player_enemy_work_=nullptr;player_link_matrix_valid_=false;}
    if(in.hp<0)player_killed_=true;
    if((in.flg&2u)!=0||(in.mode0==6&&in.hp<0))player_dead_=true;
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


// effect.c:802 link update, then effsub1.c:599 bhEff001.
// Use the linked owner's actual model window; never substitute a root pose.
void RoomWorld::tick_shadow(unsigned slot){
    auto& work=sprites_.at(slot);
    auto& shadow=work.sprite;
    work.queued=false;
    // op->lkwkp is the BH_PWORK the creator passed to bhSetShadow; its native
    // owner is the entity recvx_enemy_set registered (room work or a child of
    // enemy_children_, which has no entities_ entry).
    WorldEntity* linked=nullptr;
    if(shadow.lkwkp){auto* w=static_cast<RECVX_ENEMY_WORK*>(shadow.lkwkp);if(w->owner)linked=static_cast<WorldEntity*>(w->owner);}
    if(!linked)for(auto& entry:entities_){
        if(entry.second.enemy_bound&&shadow.lkwkp==&entry.second.enemy){
            linked=&entry.second;
            break;
        }
    }
    if(!linked){
        unbound_enemy_.insert("bhEff001: linked native owner missing");
        return;
    }
    auto& owner=linked->enemy;
    auto& tree=linked->model_owner?*linked->model_owner:*linked;
    const auto matrix=[&](int joint)->const BoneMatrix&{
        if(joint<0||!tree.enemy_calculated)
            throw std::runtime_error("bhEff001: linked model matrix not available");
        const auto index=std::size_t(tree.model_window)+unsigned(joint);
        if(index>=tree.enemy_owp_mtx.size())
            throw std::runtime_error("bhEff001: joint outside linked model window");
        return tree.enemy_owp_mtx[index];
    };
    try{
        // Validate required matrices before changing work or shared ground hit.
        if((shadow.flg&0x80u)&&(owner.flg&1u))
            (void)matrix(shadow.lkono);
        if(shadow.mode0==1){
            if(shadow.mdlver==0&&(owner.stflg&0x2000u))(void)matrix(0);
            if(shadow.type==0){
                for(const auto joint:shadow.jno){
                    if(joint==-1)break;
                    (void)matrix(int(joint));
                }
            }
        }
        if((shadow.flg&0x80u)&&(owner.flg&1u)){
            NJS_MATRIX transform;
            const auto& source=matrix(shadow.lkono);
            std::copy_n(source.value,16,transform);
            NJS_POINT3 offset{shadow.lox,shadow.loy,shadow.loz},position{};
            njCalcPoint(&transform,&offset,&position);
            shadow.px=position.x;shadow.py=position.y;shadow.pz=position.z;
        }
        ShadowOwnerView view;
        view.flags=owner.flg;view.status=owner.stflg;
        view.model_flags=owner.mdflg;view.yaw=owner.ay;
        if(shadow.mode0==1&&shadow.mdlver==0&&(owner.stflg&0x2000u))
            view.root_matrix_y=matrix(0).value[13];
        const auto joint_position=[&](int joint){
            const auto& m=matrix(joint);
            return NJS_POINT3{m.value[12],m.value[13],m.value[14]};
        };
        const auto ground_position=[&](NJS_POINT3 position){
            return ground_.query(position);
        };
        work.queued=step_shadow(shadow,view,presentation_flags_,
                                joint_position,ground_position);
    }catch(const std::runtime_error& error){
        unbound_enemy_.insert(error.what());
    }
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
    s.player_damage=[](RECVX_ENEMY_WORK*,const char*,int amount){
        if(enemy_world&&enemy_world->player_hp_owner_)*enemy_world->player_hp_owner_-=amount;
    };
    s.lever_check=[]()->int{
        if(!enemy_world)return 0;
        const unsigned ps=enemy_world->player_lever();
        int dec=0;
        if(ps&0xfu)dec=4;
        if(ps&0x2d00u)dec+=3;
        return dec;
    };
    s.player_clear_offset=[](){};
    // bhCheckWallType (hitchk.c) over the room-then-mobile geometry.
    s.wall_type=[](const RECVX_ENEMY_VEC* p,uint32_t flg,float r,float h)->int{return enemy_world->collision_contact({p->x,p->y,p->z},flg,r,h)?1:0;};
    // hitchk.c:641 bhCheckWallEx fuer einen Gegnerwork (zonzon.c:1222
    // bhEne_CollisionCheckWall2): derselbe geordnete Durchlauf wie fuer den
    // Spieler (native/geometry_motion.cpp). flg/stflg/flg2/Lage/Etage/Treffer-
    // felder kommen aus dem Gegnerwork, der Zweig attr 0x40 liest und schreibt
    // im Original den Spielerwork (hitchk.c:250-295) — dafuer player_flags.
    s.wall_move=[](RECVX_ENEMY_WORK* w,RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* prev,float r,float h,uint32_t)->int{
        auto* world=enemy_world;if(!w)return 0;
        MotionActor player=world->motion_actor();
        std::optional<std::size_t> step;
        int floor_no=w->flr_no,push_ct=w->psh_ct,push_idx=w->psh_idx;
        unsigned floor_snd=unsigned(w->flr_snd),foot=unsigned(w->footeff);
        MotionActor actor{w->flg,w->stflg,w->flg2,player.control,player.system,player.mode,
            floor_no,push_ct,push_idx,floor_snd,foot,step,w->ay,false,player.effect29,&player.flags};
        auto walls=world->collision_geometry();auto steps=world->motion_steps();auto floors=world->motion_floors();
        MotionTerrain terrain{walls,steps,floors,world->floors_,world->room_height_};
        NJS_POINT3 to{pos->x,pos->y,pos->z};const NJS_POINT3 from{prev->x,prev->y,prev->z};
        const bool hit=resolve_motion_wall(terrain,actor,to,from,r>0?r:w->ar,h>0?h:w->ah);
        pos->x=to.x;pos->y=to.y;pos->z=to.z;
        return hit?1:0;};
    // hitchk.c:5391 bhCheckDansa: rom->etcp zuerst, dann sys->metcp. Der Host
    // fuehrt dieselbe Reihenfolge wie bhCheckExmAtari (EventVm::interactions,
    // danach die beweglichen Saetze von bhObj001). Geprueft wird nur der
    // Fusspunkt px/pz, nicht die Kapsel, und nur Typ 2 mit prm0 != 0 bei
    // freiem stflg 0x30 (hitchk.c:5400-5424).
    s.check_dansa=[](RECVX_ENEMY_WORK* w){
        auto* world=enemy_world;if(!w)return;
        const float px=w->px,pz=w->pz;
        w->stflg&=~0x20000u;
        auto test=[&](unsigned flg,unsigned type,int flr_no,float x,float z,float ew,float ed,unsigned prm0){
            if(!(flg&1u)||type!=2u||flr_no!=int(w->flr_no))return;
            if(!(x<=px&&x+ew>=px&&z<=pz&&z+ed>=pz))return;
            if(!(w->stflg&0x30u)&&prm0)w->stflg|=0x20000u;};
        if(world->collision_events_)for(const auto& a:world->collision_events_->interactions)
            test(a.flags,a.type,a.floor,a.position.x,a.position.z,a.extent.x,a.extent.z,a.parameters[0]);
        for(const auto& a:world->push_steps())
            test(a.flg,a.type,int(a.flr_no),a.px,a.pz,a.w,a.d,a.prm0);};
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
    // njplus.c:498 npCollisionCheckSC through the combat port's provider.
    s.sphere_capsule=[](const RECVX_ENEMY_VEC* c,float r,const RECVX_ENEMY_VEC* c1,const RECVX_ENEMY_VEC* c2,float cr)->int{
        const RECVX_CB_SPHERE sph{{c->x,c->y,c->z},r};const RECVX_CB_CAPSULE cap{{c1->x,c1->y,c1->z},{c2->x,c2->y,c2->z},cr};
        return recvx_cb_check_sc(&sph,&cap);};
    // sdfunc.c RequestEnemySe/Ex (RequestEnemySeBasic Flag 1), ChechPlayEnemySe, CallPlayerVoice.
    s.enemy_se=[](uint32_t enemy,const RECVX_ENEMY_VEC* p,int se){enemy_world->audio_.request_enemy_se(enemy,{p->x,p->y,p->z},se,1,0);};
    s.enemy_se_ex=[](uint32_t enemy,const RECVX_ENEMY_VEC* p,int se,int fade){enemy_world->audio_.request_enemy_se(enemy,{p->x,p->y,p->z},se,1,fade);};
    s.enemy_se_playing=[](uint32_t enemy,int se)->int{return enemy_world->audio_.enemy_se_playing(enemy,se)?1:0;};
    s.player_voice=[](int se){enemy_world->audio_.play_player_voice(se);};
    // CallSystemSe(0, SeNo) -> CallSystemSeBasic: bit 31 is the ExPlayMidi path.
    s.system_se=[](int se){if(std::uint32_t(se)&0x80000000u)enemy_world->audio_.play_system_midi(std::uint32_t(se));else enemy_world->unbound_enemy_.insert("CallSystemSeBasic ohne Bit 31");};
    // mlwP->obj_num; waehrend objP/owP += 8 um acht kleiner (en01.c:2801).
    s.obj_num=[](const RECVX_ENEMY_WORK* w)->uint32_t{const auto& t=tree_of(owner_of(w));if(!t.enemy_asset)return 0u;const auto n=t.enemy_asset->bones();return n>t.model_window?std::uint32_t(n-t.model_window):0u;};
    s.motion_frames=[](const RECVX_ENEMY_WORK* w,uint32_t mtn)->uint32_t{const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);return clip?clip->frames:0u;};
    s.motion_obj_num=[](const RECVX_ENEMY_WORK* w,uint32_t mtn)->uint32_t{const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);return clip?std::uint32_t(clip->positions.size()):0u;};
    // Motion.c:21 bhSetMotion auf mlwP->objP/owP ab dem aktuellen Fenster. Ein
    // Kind mit geteiltem mlwP schreibt in den Baum des Elternworks.
    s.set_motion=[](RECVX_ENEMY_WORK* w,int add,uint32_t md)->int{
        auto* world=enemy_world;auto& e=owner_of(w);auto& t=tree_of(e);
        if(!t.enemy_asset){world->unbound_enemy_.insert("bhSetMotion: mlwP fehlt (kein Modell)");return 0;}
        if(w->mtn_no>=512){world->unbound_enemy_.insert("bhSetMotion: mtn_no ausserhalb mnwP[512]");return 0;}
        if(t.model_window>t.enemy_obj.size()){world->unbound_enemy_.insert("bhSetMotion: objP-Fenster ausserhalb des Modells");return 0;}
        MotionWork m{w->frm_no,w->frm_mode,w->hokan_count,w->mtn_attr,w->hokan_rate};std::string gap;
        const auto first=std::ptrdiff_t(t.model_window);
        std::vector<ModelPose> obj(t.enemy_obj.begin()+first,t.enemy_obj.end());
        const std::vector<std::uint32_t> owp(t.enemy_owp_flg.begin()+first,t.enemy_owp_flg.end());
        const int r=set_motion(m,add,md,world->enemy_mn_work(e,w->mtn_no),static_cast<const std::int8_t*>(w->mtn_tp),obj,owp,&gap);
        std::copy(obj.begin(),obj.end(),t.enemy_obj.begin()+first);
        w->frm_no=m.frm_no;w->frm_mode=m.frm_mode;w->hokan_count=m.hokan_count;w->mtn_attr=m.mtn_attr;
        // bhGetFrameNum clamps to the raw frm_num and bhEne01_ChgWalkMtn shifts
        // that by 16 (en01.c:10297): frm_no == frm_num on entry, one key past
        // the motion in the original. That pass keeps the previous pose here
        // and is reported; the advance below wraps it exactly as Motion.c:106.
        if(!gap.empty()){world->unbound_enemy_.insert(gap+" (id "+std::to_string(w->id)+", mtn "+std::to_string(w->mtn_no)+")");if(std::getenv("RECVX_MTN_DBG")){const auto* c=world->enemy_mn_work(e,w->mtn_no);std::fprintf(stderr,"MTNDBG id=%u mtn=%u frm=%d md=%u obj=%zu owp=%zu window=%zu chans=%zu frames=%u gap=%s\n",w->id,w->mtn_no,w->frm_no,md,obj.size(),owp.size(),t.model_window,c?c->positions.size():0u,c?c->frames:0u,gap.c_str());}}
        return r;};
    s.calc_model=[](RECVX_ENEMY_WORK* w){enemy_world->calc_enemy_model(owner_of(w));};
    // Before the first bhCalcModel the original reads the owP that
    // bhMlbBinRealize zeroed (the realize above assigns the same zero set), so
    // a not-yet-calculated tree is the original's state, not a gap.
    s.joint_matrix=[](const RECVX_ENEMY_WORK* w,uint32_t joint,float* out)->int{const auto& t=tree_of(owner_of(w));joint+=t.model_window;if(joint>=t.enemy_owp_mtx.size())return 0;std::memcpy(out,t.enemy_owp_mtx[joint].value,16*sizeof(float));return 1;};
    s.joint_point=[](const RECVX_ENEMY_WORK* w,uint32_t joint,const RECVX_ENEMY_VEC* local,RECVX_ENEMY_VEC* out)->int{
        const auto& t=tree_of(owner_of(w));joint+=t.model_window;if(joint>=t.enemy_owp_mtx.size())return 0;
        NJS_MATRIX m;std::memcpy(m,t.enemy_owp_mtx[joint].value,sizeof m);NJS_POINT3 a{local->x,local->y,local->z},b;njCalcPoint(&m,&a,&b);*out={b.x,b.y,b.z};return 1;};
    // MN_WORK md2P[0].p[0] / p[1] of a motion on the integer frame.
    s.motion_root_position=[](const RECVX_ENEMY_WORK* w,uint32_t mtn,uint32_t frame,RECVX_ENEMY_VEC* out)->int{
        const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);if(!clip||clip->positions.empty()||clip->positions[0].empty()||frame>=clip->positions[0].size())return 0;
        const auto& k=clip->positions[0][frame];*out={k.x,k.y,k.z};return 1;};
    s.motion_root_angles=[](const RECVX_ENEMY_WORK* w,uint32_t mtn,uint32_t frame,int32_t* out)->int{
        const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);if(!clip||clip->angles.empty()||clip->angles[0].empty()||frame>=clip->angles[0].size())return 0;
        const auto& k=clip->angles[0][frame];out[0]=k[0];out[1]=k[1];out[2]=k[2];return 1;};
    s.model_root_position=[](const RECVX_ENEMY_WORK* w,RECVX_ENEMY_VEC* out)->int{const auto& t=tree_of(owner_of(w));if(t.model_window>=t.enemy_obj.size())return 0;const auto& p=t.enemy_obj[t.model_window].position;*out={p.x,p.y,p.z};return 1;};
    s.model_node_hidden=[](RECVX_ENEMY_WORK* w,uint32_t node,int hidden){auto& t=tree_of(owner_of(w));node+=t.model_window;if(node>=t.enemy_evalflags.size()){enemy_world->unbound_enemy_.insert("mlwP->objP[node].evalflags ausserhalb des Modells");return;}if(hidden)t.enemy_evalflags[node]|=8u;else t.enemy_evalflags[node]&=~8u;};
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
    // effect.c:644 bhSetShadow: the first eff[512] slot without flg & 3, zeroed,
    // flg 0x240001, id 1, type 0 with a joint chain (1 without), tex_id -1,
    // scale and its copy, lkwkp/lkono with flg 0x80, jno[] up to the -1.
    // F05 advances this same slot in tick_shadow. Modifier rendering is
    // separate from the texture-atlas sprite path and remains to be bound.
    s.set_shadow=[](const int8_t* jtb,RECVX_ENEMY_WORK* link,int lkono,float sx,float sy,float sz)->int{
        auto* world=enemy_world;const int slot=world->reserve_effect_slot();if(slot<0)return -1;
        auto& work=world->sprites_[unsigned(slot)];auto& o=work.sprite;
        o.flg=0x240001u;o.id=1;o.type=jtb?0u:1u;o.tex_id=-1;o.mdlver=0;o.px=o.py=o.pz=0;
        o.sx=o.sxb=sx;o.sy=o.syb=sy;o.sz=o.szb=sz;o.ax=o.ay=o.az=0;o.lkono=lkono;
        if(link){o.lkwkp=link;o.flg|=0x80u;}
        o.jno.fill(0);
        if(!jtb)o.jno[0]=-1;else for(unsigned j=0;j<15;++j){o.jno[j]=jtb[j];if(jtb[j]==-1)break;}
        work.hidden=false;work.queued=false;
        return slot;};
    // hitchk.c:5712 bhCheckPlayer(pp): Kontakt Gegner gegen Spieler. Der
    // Spielerwork liegt beim vorhandenen Spielerbesitzer (get/set); die
    // Rueckmeldung ist stflg 0x4 am Gegner und stflg 0x2 am Spieler.
    s.check_player=[](RECVX_ENEMY_WORK* w)->int{
        auto* world=enemy_world;if(!w)return 0;
        if(!(w->flg&0x8u)||(w->stflg&0x40000000u)||(w->flg2&0x1u)||!(w->flg&0x40u))return 1;
        RECVX_ENEMY_PLAYER pl;if(!world->enemy_player_get(pl))return 0;
        float ppx,ppy,ppz;
        /* plp->aox/aoy/aoz sind im Original ausser der Nullung player.c:713
           nirgends geschrieben, also 0. */
        if(!(pl.flg&0x8u)){
            // hitchk.c:5727: ohne plp->flg 0x8 (bhCPM2_act_die loescht es sechs
            // Frames vor Ende der Sterbemotion, player.c:3917) gilt die zuletzt
            // verfolgte Wurzellage EXP_WORK nlxb/nlyb/nlzb (player.c:1379-1387),
            // die RoomWorld::tick wie bhControlPlayer fortschreibt.
            ppx=world->player_nl_.x;ppy=world->player_nl_.y;ppz=world->player_nl_.z;
        }else{ppx=pl.pos.x;ppy=pl.pos.y;ppz=pl.pos.z;}
        const float ey=w->py+w->aoy;
        const float px=ppx-(w->px+w->aox),pz=ppz-(w->pz+w->aoz);
        const float ln=std::sqrt(px*px+pz*pz);
        float car=pl.car+w->car;
        if(!(ln<car&&ppy<=ey+w->cah&&ppy+pl.cah>=ey))return 1;
        if((pl.flg2&0x1u)||(w->flg2&0x40u)){
            const int r=int(10430.381f*std::atan2(px,pz));
            float sine,cosine;njSinCos(r,&sine,&cosine);
            w->px=(ppx-car*sine)-w->aox;w->pz=(ppz-car*cosine)-w->aoz;
        }else{
            car=0.5f*(car-ln);
            NJS_VECTOR vec{px,0,pz};njUnitVector(&vec);
            const float dx=vec.x*car,dz=vec.z*car;
            pl.pos.x+=dx;pl.pos.z+=dz;
            w->px-=dx;w->pz-=dz;
        }
        w->stflg|=0x4u;pl.stflg|=0x2u;
        world->enemy_player_set(pl);
        return 1;};
    // hitchk.c:5788 bhCheckEnemies(pp): Kontakt Gegner gegen Gegner ueber die
    // ganze ene-Liste; Trefferflaechen werden gemittelt (hitchk.c:5840-5860).
    s.check_enemies=[](RECVX_ENEMY_WORK* w)->int{
        auto* world=enemy_world;if(!w)return 0;
        if(w->flg2&0x1u)return 1;
        const float ex0=w->px+w->aox,ey0=w->py+w->aoy,ez0=w->pz+w->aoz;
        std::vector<std::pair<float,float>> push;
        for(std::uint32_t i=0;i<world->enemy_table_.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){
            auto* enep=world->enemy_table_.slot[i];
            if(!enep)continue;
            // Original vergleicht die Zeiger mit ~0x80000000 maskiert; das ist
            // dieselbe Identitaetspruefung, hier direkt.
            if(!(enep->flg&0x1u)||!(enep->flg&0x8u)||(enep->stflg&0x1000000u))continue;
            if(enep==w)continue;
            /* hitchk.c:5813: `(pp != plp) || (enep->flg & 0x40)`. Der Aufrufer
               ist hier immer ein Gegnerwork (der Spielerwork gehoert dem
               Spielerbesitzer), die Bedingung ist damit erfuellt. */
            if(enep->flg&0x80u)if(!enep->lkwkp||(static_cast<RECVX_ENEMY_WORK*>(enep->lkwkp)->stflg&0x1000000u))continue;
            const float ex1=enep->px+enep->aox,ey1=enep->py+enep->aoy,ez1=enep->pz+enep->aoz;
            const float px=ex0-ex1,pz=ez0-ez1;
            const float ln=std::sqrt(px*px+pz*pz);
            const float car=w->car+enep->car;
            if(!(ln<car&&ey1<=ey0+w->cah&&ey1+enep->cah>=ey0))continue;
            const int r=int(10430.381f*std::atan2(ex1-ex0,ez1-ez0));
            float sine,cosine;njSinCos(r,&sine,&cosine);
            push.push_back({(ex1-car*sine)-w->aox,(ez1-car*cosine)-w->aoz});
            /* hitchk.c:5836: `if (pp == plp) enep->stflg |= 0x4; else |= 0x2;`
               Der Aufrufer ist nie der Spielerwork, also immer 0x2. */
            enep->stflg|=0x2u;}
        if(push.empty())return 1;
        if(push.size()==1){w->px=push[0].first;w->pz=push[0].second;return 1;}
        float sx=0,sz=0;for(const auto& p:push){sx+=p.first;sz+=p.second;}
        w->px=sx/float(push.size());w->pz=sz/float(push.size());
        return 1;};
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
        const auto& e=tree_of(owner_of(w));const auto a=std::size_t(std::int8_t(j1))+e.model_window,b=std::size_t(std::int8_t(j2))+e.model_window;
        if(!e.enemy_calculated||a>=e.enemy_owp_mtx.size()||b>=e.enemy_owp_mtx.size()){enemy_world->unbound_enemy_.insert("bhEne_SetWeponAtr: owP ausserhalb/noch nicht berechnet");return;}
        const float* m1=e.enemy_owp_mtx[a].value;const float* m2=e.enemy_owp_mtx[b].value;
        w->watr.c1={m1[12],m1[13],m1[14]};w->watr.c2={m2[12],m2[13],m2[14]};w->watr.r=r;};
    // en03.c:5220 bhEne03_Collision: one wall pass for every enemy routine.
    s.collision_works=[](RECVX_ENEMY_WORK* w)->int{return recvx_en03_collision(w,nullptr);};
    // --- EN01-Kindworks und Baumzugriffe (en01_work.h) ---------------------
    auto& en=en01_services();
    // npSetAllMatColor(mlwP->objP, mlwP->obj_num, color) (njplus.c:1375): der
    // Wert gilt allen Objekten des Baums, also dem Baumbesitzer; angewendet
    // wird er beim Zeichnen (RoomWorld::append).
    en.set_material_color=[](RECVX_ENEMY_WORK* w,uint32_t color){
        if(!w)return;auto& t=tree_of(owner_of(w));t.enemy_material_set=true;t.enemy_material_color=color;};
    // bhEne01_SetLinkEnemy (en01.c:2817): bhSetEnemy(&zob_lkmtab, ...) belegt
    // den ersten freien ene[]-Platz; epp->mnwP = epw->mnwP (:2829).
    en.child_create=[](RECVX_ENEMY_WORK* parent,uint32_t,int32_t)->RECVX_ENEMY_WORK*{
        auto* child=enemy_world->create_enemy_child(parent);return child?&child->enemy:nullptr;};
    // bhEne_SetCallFunc (eneset.c:933): bhJumpEnemy[no] = func. Die Routinen
    // der Kindfamilie stehen in en01sub.c (ids en01.c:2605-2669).
    en.register_child_dispatch=[](uint32_t id){
        RECVX_ENEMY_PROC proc=nullptr;
        switch(id){
        case 0x1f:proc=recvx_en01_head;break;
        case 0x20:proc=recvx_en01_arm;break;
        case 0x21:proc=recvx_en01_leg;break;
        case 0x22:proc=recvx_en01_cap;break;
        case 0x23:proc=recvx_en01_worm;break;
        case 0x24:proc=recvx_en01_bom;break;
        case 0x25:proc=recvx_en01_scope;break;
        case 0x26:proc=recvx_en01_parent;break;
        default:enemy_world->unbound_enemy_.insert("bhEne_SetCallFunc: unbekannte Kind-id");return;}
        recvx_enemy_set_call_func(&enemy_world->enemy_dispatch_,id,proc);};
    // epp->mlwP = epw->mdl (en01.c:2827) bzw. epp->mlwP = epp->mdl mit
    // epp->mdl[0] = epw->mdl[slot] (Kappe/Wurm/Bom/Scope, :2617-2658).
    en.child_model=[](RECVX_ENEMY_WORK* child,RECVX_ENEMY_WORK* parent,uint32_t slot,int){
        auto& c=owner_of(child);auto& p=tree_of(owner_of(parent));
        if(slot==RECVX_EN01_CHILD_SHARE_PARENT){c.model_owner=&p;c.enemy_asset=nullptr;c.enemy_obj.clear();c.enemy_evalflags.clear();c.enemy_owp_flg.clear();c.enemy_owp_mtx.clear();return;}
        const PlayerAsset* asset=p.enemy_asset?(slot==0?p.enemy_asset:p.enemy_asset->model(slot)):nullptr;
        c.model_owner=nullptr;c.enemy_asset=asset;
        if(!asset){c.enemy_obj.clear();c.enemy_evalflags.clear();c.enemy_owp_flg.clear();c.enemy_owp_mtx.clear();enemy_world->unbound_enemy_.insert("bhEne01_Init: epw->mdl[slot] nicht als Modell geladen");return;}
        c.enemy_obj=asset->bind_pose();c.enemy_evalflags=asset->node_evalflags();c.enemy_owp_flg.assign(c.enemy_obj.size(),0u);c.enemy_owp_mtx.assign(c.enemy_obj.size(),BoneMatrix{});c.enemy_calculated=false;};
    // objP += 8, owP += 8, obj_num -= 8 am geteilten ML_WORK (en01.c:2799-2805).
    en.shift_object_window=[](RECVX_ENEMY_WORK* w,int shift){
        auto& t=tree_of(owner_of(w));
        if(shift)t.model_window+=8u;
        else if(t.model_window>=8u)t.model_window-=8u;
        else enemy_world->unbound_enemy_.insert("objP -= 8 ohne vorheriges += 8");};
    en.node_local=[](const RECVX_ENEMY_WORK* w,uint32_t node,RECVX_ENEMY_VEC* pos,int32_t* ang3)->int{
        const auto& t=tree_of(owner_of(w));node+=t.model_window;if(node>=t.enemy_obj.size())return 0;
        const auto& o=t.enemy_obj[node];if(pos)*pos={o.position.x,o.position.y,o.position.z};
        if(ang3){ang3[0]=o.angles[0];ang3[1]=o.angles[1];ang3[2]=o.angles[2];}return 1;};
    en.node_local_clear_xz=[](RECVX_ENEMY_WORK* w,uint32_t node){
        auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_obj.size()){enemy_world->unbound_enemy_.insert("mlwP->objP[node].pos ausserhalb des Modells");return;}
        t.enemy_obj[node].position.x=0;t.enemy_obj[node].position.z=0;};
    en.node_local_set=[](RECVX_ENEMY_WORK* w,uint32_t node,uint32_t axis,float v){
        auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_obj.size()||axis>2){enemy_world->unbound_enemy_.insert("mlwP->objP[node].pos ausserhalb des Modells");return;}
        auto& pos=t.enemy_obj[node].position;if(axis==0)pos.x=v;else if(axis==1)pos.y=v;else pos.z=v;};
    en.node_angle_get=[](const RECVX_ENEMY_WORK* w,uint32_t node,uint32_t axis)->int32_t{
        const auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_obj.size()||axis>2){enemy_world->unbound_enemy_.insert("mlwP->objP[node].ang ausserhalb des Modells");return 0;}
        return t.enemy_obj[node].angles[axis];};
    en.node_angle_set=[](RECVX_ENEMY_WORK* w,uint32_t node,uint32_t axis,int32_t ang){
        auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_obj.size()||axis>2){enemy_world->unbound_enemy_.insert("mlwP->objP[node].ang ausserhalb des Modells");return;}
        t.enemy_obj[node].angles[axis]=ang;};
    // EN04 reaches the same three originals through its own contract.
    auto& dog=en04_services();
    dog.set_material_color=en.set_material_color;
    dog.node_transform=en.node_local;
    dog.node_ang_set=en.node_angle_set;
    dog.node_pos_set=[](RECVX_ENEMY_WORK* w,uint32_t node,uint32_t axis,float v){
        auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_obj.size()||axis>2){enemy_world->unbound_enemy_.insert("mlwP->objP[node].pos ausserhalb des Modells");return;}
        auto& p=t.enemy_obj[node].position;(axis==0?p.x:axis==1?p.y:p.z)=v;};
    // owP[joint].mtx direkt setzen bzw. owP[joint].flg |= bits
    // (MdlPut.c:296 bhCalcTree liest flg 0x1 als "Matrix behalten").
    en.joint_matrix_set=[](RECVX_ENEMY_WORK* w,uint32_t joint,const float* in16)->int{
        auto& t=tree_of(owner_of(w));joint+=t.model_window;
        if(joint>=t.enemy_owp_mtx.size()){enemy_world->unbound_enemy_.insert("mlwP->owP[joint].mtx ausserhalb des Modells");return 0;}
        std::memcpy(t.enemy_owp_mtx[joint].value,in16,16*sizeof(float));return 1;};
    en.joint_flags_or=[](RECVX_ENEMY_WORK* w,uint32_t joint,uint32_t bits){
        auto& t=tree_of(owner_of(w));joint+=t.model_window;
        if(joint>=t.enemy_owp_flg.size()){enemy_world->unbound_enemy_.insert("mlwP->owP[joint].flg ausserhalb des Modells");return;}
        t.enemy_owp_flg[joint]|=bits;};
    // epw->mtx: die Wurzelmatrix des Works nach bhCalcModel (MdlPut.c:240). Der
    // Work haelt sie in root_mtx; ein Kind traegt die des eigenen Aufrufs.
    en.work_matrix=[](const RECVX_ENEMY_WORK* w,float* out16)->int{
        if(!w)return 0;std::memcpy(out16,w->root_mtx,sizeof(w->root_mtx));return 1;};
    // plp->mlwP->objP[node].pos[0] = pos[2] = 0 (en01sub.c:1532-1534): der
    // Spielerbaum gehoert dem vorhandenen Spielerbesitzer.
    // Der Spielerbaum gehoert dem Spielerbesitzer; der Knotenwert gilt bis zum
    // naechsten bhSetMotion des Spielers (RoomWorld::tick loescht ihn).
    en.player_node_clear_xz=[](uint32_t node){
        auto* world=enemy_world;if(!world->player_asset_){world->unbound_enemy_.insert("plp->mlwP->objP[node].pos (Spielerbaum)");return;}
        const unsigned physical=world->player_asset_->model_index(node);
        auto pose=world->model_pose_physical(0,0,physical);pose.position.x=0;pose.position.z=0;
        world->entity(0,0).node_overrides[physical].position=pose.position;world->player_transient_nodes_.push_back(physical);};
    // plp->mtx — die Wurzelmatrix des Spielers (en01sub.c:1466/1527/1594). Der
    // Spielerbaum gehoert dem vorhandenen Besitzer (link_matrix kind 0).
    en.player_matrix=[](float* out16)->int{
        if(enemy_world&&enemy_world->player_link_matrix_valid_){
            std::memcpy(out16,enemy_world->player_link_matrix_.value,16*sizeof(float));
            return 1;
        }
        const auto m=enemy_world->link_matrix(0,0,0);if(!m)return 0;
        std::memcpy(out16,m->value,16*sizeof(float));return 1;};
    en.player_link_pose=[](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_VEC* world_point,int32_t ay,
                           const RECVX_EN01_XZ* local_offset,RECVX_ENEMY_VEC* out_delta)->int{
        auto* world=enemy_world;
        if(!world||!w||!world_point||!out_delta)return 0;
        world->player_.position.x=world_point->x;
        world->player_.position.z=world_point->z;
        world->player_.yaw=ay&0xffff;
        const NJS_POINT3 opos=world->player_link_matrix_valid_?
            NJS_POINT3{world->player_link_matrix_.value[12],world->player_link_matrix_.value[13],world->player_link_matrix_.value[14]}:
            NJS_POINT3{world->player_.position.x,world->player_.position.y,world->player_.position.z};
        NJS_POINT3 pos;
        if(local_offset){
            NJS_POINT3 ps{local_offset->x,0.0f,local_offset->z};
            NJS_MATRIX m;njUnitMatrix(&m);
            njTranslate(&m,world->player_.position.x,world->player_.position.y,world->player_.position.z);
            njRotateXYZ(&m,world->player_.pitch,world->player_.yaw,world->player_.roll);
            njCalcPoint(&m,&ps,&pos);
            NJS_MATRIX mtx;njUnitMatrix(&mtx);
            njTranslate(&mtx,pos.x,pos.y,pos.z);
            njRotateXYZ(&mtx,world->player_.pitch,world->player_ayp_,world->player_.roll);
            std::memcpy(world->player_link_matrix_.value,mtx,sizeof mtx);
            world->player_link_matrix_valid_=true;
        }else{
            pos={world->player_.position.x,world->player_.position.y,world->player_.position.z};
        }
        MotionActor actor=world->motion_actor();
        auto walls=world->collision_geometry();auto steps=world->motion_steps();auto floors=world->motion_floors();
        MotionTerrain terrain{walls,steps,floors,world->floors_,world->room_height_};
        NJS_POINT3 to{pos.x,pos.y,pos.z};const NJS_POINT3 from{opos.x,opos.y,opos.z};
        const float ar=3.5f;const float ah=world->player_character_?17.5f:16.5f;
        resolve_motion_wall(terrain,actor,to,from,ar,ah);
        out_delta->x=to.x-pos.x;out_delta->y=to.y-pos.y;out_delta->z=to.z-pos.z;
        return 1;
    };
    en.pad_on_clear=[](uint32_t bits){
        if(enemy_world)enemy_world->clear_pad_on(bits);
    };
    // sdfunc.c:3401 StartVibrationEx(AtrbId, VibNo): the runtime's vibration
    // unit services the request (VibP envelope, pad motor).
    s.vibration=[](int atrb_id,int vib_no){if(enemy_world)enemy_world->vibration_requests_.push_back({atrb_id,vib_no});};
    // pwksub.c:664 bhSearchPlayer in the bhSearchPlayer2 form (en01b.c:922):
    // the cone is built on the shared matrix, so vc2 carries ay + r. The
    // player owner is the bound runtime player; without it the service
    // returns 0 and the caller reports the gap (no invented -1).
    en.search_player=[](const RECVX_ENEMY_WORK*,const RECVX_ENEMY_VEC* pos,int32_t ay,int32_t r,int32_t* out)->int{
        auto* world=enemy_world;if(!world||!world->player_asset_||!world->player_hp_owner_||!pos||!out)return 0;
        if(r==0)r=32768;
        NJS_VECTOR vc1{0,0,-1.0f},vc0{},vc2{};NJS_MATRIX m;njUnitMatrix(&m);
        njRotateY(&m,ay);njCalcVector(&m,&vc1,&vc0);
        njRotateY(&m,r);njCalcVector(&m,&vc1,&vc2);
        const float er=-njInnerProduct(&vc0,&vc2);
        const auto& pp=world->player_.position;
        vc1={pos->x-pp.x,pos->y-pp.y,pos->z-pp.z};njUnitVector(&vc1);
        const float a=njInnerProduct(&vc0,&vc1);
        if(a<er){const float dx=pp.x-pos->x,dz=pp.z-pos->z;int yr=int(10430.381f*std::atan2(-dx,-dz));if(yr==-1)yr=-2;*out=yr;}
        else *out=-1;
        return 1;};
    // zonzon.c:373 bhEne_EnemyAtariCheck: rom->flrp then sys->mflrp (the
    // motion_floors order), flg&1, type 2, bhEne_PosCheck, flr_no, prm0/prm1.
    en.enemy_atari_atr=[](const RECVX_ENEMY_VEC* pos,int flr_no,uint8_t id,uint8_t type,RECVX_ENEMY_ATR* rec)->int{
        for(const auto& a:enemy_world->motion_floors()){
            if(!(a.flags&1u)||a.type!=2u||!ene_pos_check(pos->x,pos->z,a.position.x,a.position.z,a.extent.x,a.extent.z))continue;
            if(a.floor!=flr_no||a.parameters[0]!=id||a.parameters[1]!=type)continue;
            if(rec)*rec=atr_of(a);return 1;}
        return 0;};
    en.enemy_atari=[](const RECVX_ENEMY_VEC* pos,int flr_no,uint8_t id,uint8_t type)->int{
        return en01_services().enemy_atari_atr(pos,flr_no,id,type,nullptr);};
    // hitchk.c:5503 bhCheckFloorEffect: same list, type 3, inclusive edges.
    en.floor_effect=[](int flr_no,float px,float pz,RECVX_ENEMY_ATR* rec)->int{
        for(const auto& a:enemy_world->motion_floors()){
            if(!(a.flags&1u)||a.type!=3u)continue;
            if(!(a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz)||a.floor!=flr_no)continue;
            if(rec)*rec=atr_of(a);return 1;}
        return 0;};
    // player.c:1758 plp->watr: c1 = owP[4], c2 = owP[0], r 2.5 — read from the
    // joints the runtime commits after bhCalcModel(plp).
    en.player_capsule=[](RECVX_ENEMY_CAPSULE* out)->int{
        BoneMatrix a,b;if(!enemy_world->enemy_player_joint(4,a)||!enemy_world->enemy_player_joint(0,b))return 0;
        out->c1={a.value[12],a.value[13],a.value[14]};out->c2={b.value[12],b.value[13],b.value[14]};out->r=2.5f;return 1;};
    // weapon.c WpnTab[wpnr_no].flg (native/combat_tables_data.inc).
    en.weapon_flg=[](uint32_t wpnr_no,uint32_t* out)->int{if(wpnr_no>=23u)return 0;*out=WpnTab[wpnr_no].flg;return 1;};
    // pwksub.c:144 bhGetFrameNum(fnm_old, fnm_new, fno_now): fno_now / 65536 is
    // the original's integer division, the result is clamped to fnm_new.
    en.frame_num=[](uint32_t fnm_old,uint32_t fnm_new,int32_t fno_now)->int32_t{
        float fold=float(fnm_old);const float fnew=float(fnm_new);
        fold=fnew*((100.0f/fold)*float(fno_now/65536));
        int frm=int(fold/100.0f)*65536;
        if(frm<0)frm=0;
        if(unsigned(frm)>fnm_new)frm=int(fnm_new);
        return frm;};
    // Motion.c:868 bhCalcFixOffset(ewP, datP, offP, rtnP): the foot chain of
    // the current local pose (mlwP->objP) stacked on the work's root (or the
    // parent joint for flg 0x80), minus where owP[last].mtx of the previous
    // bhCalcModel put the same offset.
    en.calc_fix_offset=[](RECVX_ENEMY_WORK* w,const int8_t* tree,const RECVX_ENEMY_VEC* local,RECVX_ENEMY_VEC* out){
        auto* world=enemy_world;if(!w||!tree||!out)return;
        const auto& t=tree_of(owner_of(w));
        static const RECVX_ENEMY_VEC off_zero{0,0,0};if(!local)local=&off_zero;
        NJS_MATRIX m;
        if(w->flg&0x80u){
            auto* parent=(RECVX_ENEMY_WORK*)w->lkwkp;
            if(!parent){world->unbound_enemy_.insert("bhCalcFixOffset: lkwkp fehlt (Motion.c:883)");return;}
            const auto& pt=tree_of(owner_of(parent));const auto joint=std::size_t(w->lkono)+pt.model_window;
            if(!pt.enemy_calculated||joint>=pt.enemy_owp_mtx.size()){world->unbound_enemy_.insert("bhCalcFixOffset: lkwkp->mlwP->owP[lkono].mtx");return;}
            std::memcpy(m,pt.enemy_owp_mtx[joint].value,sizeof m);
            njTranslate(&m,w->lox,w->loy,w->loz);njRotateXYZ(&m,w->ax,w->ay,w->az);
        }else{
            njUnitMatrix(&m);njTranslate(&m,w->px,w->py,w->pz);njRotateXYZ(&m,w->ax,w->ay,w->az);
        }
        int last=-1;
        for(const int8_t* node=tree;*node!=-1;++node){
            const auto index=std::size_t(*node)+t.model_window;
            if(index>=t.enemy_obj.size()){world->unbound_enemy_.insert("bhCalcFixOffset: mlwP->objP[node] ausserhalb des Modells");return;}
            const auto& o=t.enemy_obj[index];
            njTranslate(&m,o.position.x,o.position.y,o.position.z);njRotateXYZ(&m,o.angles[0],o.angles[1],o.angles[2]);
            last=*node;}
        if(last<0){world->unbound_enemy_.insert("bhCalcFixOffset: leere Knotenkette");return;}
        const auto owp=std::size_t(last)+t.model_window;
        if(!t.enemy_calculated||owp>=t.enemy_owp_mtx.size()){world->unbound_enemy_.insert("bhCalcFixOffset: mlwP->owP[last].mtx");return;}
        NJS_POINT3 off{local->x,local->y,local->z},dst,src;
        njCalcPoint(&m,&off,&dst);
        NJS_MATRIX prev;std::memcpy(prev,t.enemy_owp_mtx[owp].value,sizeof prev);
        njCalcPoint(&prev,&off,&src);
        *out={dst.x-src.x,dst.y-src.y,dst.z-src.z};};
    // rutchk.c:47 bhCheckRouteID(ps): first rom->rutp box containing the point.
    en.route_id=[](const RECVX_ENEMY_VEC* ps)->int{
        const auto& zones=enemy_world->route_areas_;
        for(std::size_t i=0;i<zones.size();++i){const auto& a=zones[i];
            if(a.position.x<=ps->x&&a.position.x+a.extent.x>=ps->x&&a.position.y<=ps->y&&a.position.y+a.extent.y>=ps->y&&a.position.z<=ps->z&&a.position.z+a.extent.z>=ps->z)return int(i);}
        return 0xFF;};
    // rutchk.c:5 bhCheckRoute(ps0, ps1, ps2): rom->ruttp[rut_n * nxt + rid]
    // names the zone to cross next; ps2 is its centre.
    en.route_next=[](const RECVX_ENEMY_VEC* from,const RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* out)->int{
        auto* world=enemy_world;const auto& en=en01_services();
        const int rid=en.route_id(from);if(rid==0xFF)return 0xFF;
        const int nxt=en.route_id(to);if(nxt==0xFF)return 0xFF;
        if(rid==nxt){*out=*to;return 0xFF;}
        const auto n=world->route_areas_.size();const auto slot=n*std::size_t(nxt)+std::size_t(rid);
        if(slot>=world->route_table_.size()){world->unbound_enemy_.insert("rom->ruttp: Tabelle kuerzer als rut_n*rut_n");return 0xFF;}
        const unsigned now=world->route_table_[slot];
        if(now>=n){world->unbound_enemy_.insert("rom->ruttp: Zonenindex ausserhalb rom->rutp");return 0xFF;}
        const auto& rp=world->route_areas_[now];
        *out={rp.position.x+0.5f*rp.extent.x,rp.position.y+0.5f*rp.extent.y,rp.position.z+0.5f*rp.extent.z};
        return int(now);};
    // pwksub.c:133 bhAddSpeed(pp, r): step spd along ay + r.
    en.add_speed=[](RECVX_ENEMY_WORK* w,int r){
        const int angle=(w->ay+r)&0xFFFF;
        w->px-=w->spd*njSin(angle);w->pz-=w->spd*njCos(angle);};
    // zonzon.c:209 bhEne_GetTranslateMtn (variant 1) and :251
    // bhEne_GetTranslateMtn2 (variant 2): the root key of mnwP[mtn_no] on
    // `frm` (delta to the previous key after frame 0), y dropped for mode 0;
    // variant 1 moves the work through its own root matrix, variant 2 only
    // stores the step length in spd. Both run only when mtn_add != 0 or the
    // work is not already standing on that frame.
    en.translate_motion=[](RECVX_ENEMY_WORK* w,uint32_t variant,int32_t frm){
        auto* world=enemy_world;const auto& s=enemy_world->enemy_services_;
        if(!(w->mtn_add!=0||(w->frm_no/65536)!=frm))return;
        if(frm<0){world->unbound_enemy_.insert("bhEne_GetTranslateMtn: frm < 0");return;}
        RECVX_ENEMY_VEC key{},prev{};
        if(!s.motion_root_position(w,w->mtn_no,uint32_t(frm),&key)){world->unbound_enemy_.insert("bhEne_GetTranslateMtn: md2P->p[0][frm] ausserhalb der Motion");return;}
        if(frm!=0){
            if(!s.motion_root_position(w,w->mtn_no,uint32_t(frm-1),&prev)){world->unbound_enemy_.insert("bhEne_GetTranslateMtn: md2P->p[0][frm-1]");return;}
            key.x-=prev.x;key.y-=prev.y;key.z-=prev.z;}
        /* bhEne01_SetMtn passes mode 0 at every call site (en01.c:10359/10411/10417). */
        key.y=0;
        if(variant==1u){
            NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,w->px,w->py,w->pz);njRotateXYZ(&m,w->ax,w->ay,w->az);
            NJS_POINT3 k{key.x,key.y,key.z},out;njCalcPoint(&m,&k,&out);
            w->px=out.x;w->py=out.y;w->pz=out.z;
            w->py=float(int(100.0f*(0.001f+w->py)))/100.0f;
        }else if(variant==2u){
            NJS_VECTOR k{key.x,key.y,key.z};w->spd=njScalor(&k);
        }else world->unbound_enemy_.insert("bhEne_GetTranslateMtn: unbekannte Variante");};
    // zonzon.c:310 bhKaidanAtrCheck(epw, len, &idx): probe len before the
    // work over rom->etcp then sys->metcp (the motion_steps order), type 1,
    // attr without 0x400000, same floor, then the inner band test by prm1.
    en.stair_area_atr=[](RECVX_ENEMY_WORK* w,float len,RECVX_ENEMY_ATR* rec,int* index)->int{
        float px=w->px-len*njSin(w->ay),pz=w->pz-len*njCos(w->ay);
        const auto steps=enemy_world->motion_steps();
        for(std::size_t i=0;i<steps.size();++i){const auto& a=steps[i];
            if(!(a.flags&1u)||a.type!=1u)continue;
            if(!(a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz)||(a.attributes&0x400000u)||a.floor!=w->flr_no)continue;
            const float bx=0.5f+a.position.x,bz=0.5f+a.position.z,bx2=a.position.x+a.extent.x-0.5f,bz2=a.position.z+a.extent.z-0.5f;
            const bool hit=(a.parameters[1]==0||a.parameters[1]==2)?(bx<=w->px&&bx2>=w->px):(bz<=w->pz&&bz2>=w->pz);
            if(!hit)continue;
            if(rec)*rec=atr_of(a);if(index)*index=int(i);return 1;}
        return 0;};
    // hitchk.c:4965 bhSetUseKaidanFlag / :5003 bhClrUseKaidanFlag on the
    // room etcp owner (EventVm interactions); the partner is idx+-1 by prm0
    // for prm3 == 0xFF, else rom->etcp[prm3]. Mobile metcp entries belong to
    // bhObj001 and are reported, not written.
    s.stair_use=[](int index,int used){
        auto* world=enemy_world;auto* ev=world->collision_events_;
        if(!ev){world->unbound_enemy_.insert("bhSetUseKaidanFlag: rom->etcp ohne EventVm-Besitzer");return;}
        auto& list=ev->interactions;const int room_n=int(list.size());
        if(index<0){world->unbound_enemy_.insert("bhSetUseKaidanFlag: Index ohne Flaeche");return;}
        // rom->etcp[idx] or sys->metcp[idx - rom->etc_n]; the mobile entries
        // belong to bhObj001 (PlayerPush) and are marked through its owner.
        auto mark=[&](int idx,bool on){
            if(idx<0)return false;
            if(idx<room_n){auto& a=list[std::size_t(idx)];on?a.attribute|=0x400000u:a.attribute&=~0x400000u;return true;}
            if(!world->push_)return false;
            const auto steps=world->push_->steps();const int mobile=idx-room_n;
            if(mobile>=int(steps.size()))return false;
            world->push_->mark_step(unsigned(mobile),on);return true;};
        auto record=[&](int idx,RECVX_ENEMY_ATR& out)->bool{
            if(idx<0)return false;
            if(idx<room_n){const auto& a=list[std::size_t(idx)];out=RECVX_ENEMY_ATR{};out.prm0=std::uint8_t(a.parameters[0]);out.prm3=std::uint8_t(a.parameters[3]);return true;}
            if(!world->push_)return false;const auto steps=world->push_->steps();const int mobile=idx-room_n;
            if(mobile>=int(steps.size()))return false;out=atr_of(steps[std::size_t(mobile)]);return true;};
        RECVX_ENEMY_ATR a;
        if(!record(index,a)||!mark(index,used!=0)){world->unbound_enemy_.insert("bhSetUseKaidanFlag: Flaeche ausserhalb rom->etcp + sys->metcp");return;}
        // hitchk.c:4974/5013: prm3 == 0xFF names the neighbour by prm0, else rom->etcp[prm3].
        const int partner=a.prm3==0xFF?(a.prm0==0?index+1:index-1):int(a.prm3);
        if(!mark(partner,used!=0))world->unbound_enemy_.insert("bhSetUseKaidanFlag: Partnerflaeche ausserhalb rom->etcp + sys->metcp");};
    // plp->way, pushed by the runtime from the combat player work.
    en.player_way=[](int16_t* out)->int{if(!enemy_world->player_hp_owner_)return 0;*out=enemy_world->player_way_;return 1;};
    dog.pad_on_clear=en.pad_on_clear;
    RECVX_ENEMY_EXT& x=enemy_ext_;std::memset(&x,0,sizeof x);
    x.pad_on_clear=en.pad_on_clear;
    x.pad_ps=[]()->uint32_t{return enemy_world?enemy_world->player_lever():0u;};
    x.gap=[](const char* name){enemy_world->unbound_enemy_.insert(name);};
    x.set_call_func=[](uint32_t id,RECVX_ENEMY_PROC proc){recvx_enemy_set_call_func(&enemy_world->enemy_dispatch_,id,proc);};
    x.vibration=s.vibration;
    // --- effect.c bhSetEffectTb / eff[eno] patches / effsub3.c bhSetEffParticle ---
    // effect.c:331 bhSetEffect(id, &POINT, lkp, param): eigener Poolplatz mit
    // flg 1, type/mdlver/flr_no 0, Skala 1 und Nullpunkt; ox/oy/oz kommen aus
    // dem POINT des Aufrufers (eneset.c:461 uebergibt dort drei Nullen).
    x.set_effect=[](int id,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* off,RECVX_ENEMY_WORK* link,int param)->int{
        auto* world=enemy_world;EffectSpawn e;
        e.flg=1;e.id=unsigned(id);e.type=0;e.mdlver=0;e.lkono=param;
        if(pos)e.position={pos->x,pos->y,pos->z};
        if(off){e.has_offset=true;e.offset={off->x,off->y,off->z};}
        unsigned kind=0,index=0;
        if(link){if(!world->enemy_work_index(link,index)){world->unbound_enemy_.insert("bhSetEffect: Linkwork ohne Raumfigur (Kindwork)");return -1;}kind=2;e.linked_work=link;}
        return world->spawn_effect(e,kind,index);};
    // One spawn path for the zonzon helpers (en03_lib), EN01's sys->ef template
    // and EN04. A link to an enemy work resolves to its room entity (sprite link
    // kind 2, particle kind 1); a player link is kind 1 / kind 0; children of
    // enemy_children_ have no entity and are reported.
    x.effect_tb=[](const RECVX_T3_EFF* ef,const RECVX_ENEMY_VEC* ofs,RECVX_T3_LINK link,int joint)->int{
        return enemy_world->effect_tb(ef->flg,ef->id,ef->type,unsigned(ef->mdlver),{ef->px,ef->py,ef->pz},{ef->sx,ef->sy,ef->sz},ef->ax,ef->ay,ofs,link.w,link.player!=0,joint);};
    x.effect_patch=[](int slot,const RECVX_T3_EFFPATCH* p){
        auto* world=enemy_world;if(slot<0||std::size_t(slot)>=world->sprites_.size()||!p)return;
        auto& s=world->sprites_[std::size_t(slot)].sprite;
        if(p->fields&RECVX_T3_EFF_STFLG_OR)s.stflg|=p->stflg_or;
        if(p->fields&RECVX_T3_EFF_TEX_ID)s.tex_id=p->tex_id;
        if(p->fields&RECVX_T3_EFF_VECTOR){s.xn=p->xn;s.yn=p->yn;s.zn=p->zn;}
        if(p->fields&RECVX_T3_EFF_CT01){s.ct0=p->ct0;s.ct1=p->ct1;}
        if(p->fields&RECVX_T3_EFF_CT3)s.ct3=p->ct3;
        if(p->fields&RECVX_T3_EFF_SHP_CT)world->unbound_enemy_.insert("eff[].shp_ct (kein Feld am nativen Effektwork)");
        if(p->fields&RECVX_T3_EFF_ZN)s.zn=p->zn_val;
        if(p->fields&RECVX_T3_EFF_MODE1){if(s.id==300||s.id==302)world->particles_.set_mode1(unsigned(slot),p->mode1);else s.mode1=p->mode1;}
        if(p->fields&RECVX_T3_EFF_COLOR)for(auto& v:s.tv)v.col=p->color;
        if(p->fields&RECVX_T3_EFF_AY)s.ay=p->ay;
        /* RECVX_T3_EFF_TEXP: eff[].txp[0] names the model texture list; the
           native draw addresses textures by tex_id, so the pointer has no
           consumer here. */};
    x.effect_particle=[](RECVX_T3_LINK lk,int joint,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* vec,uint32_t color,int kind)->int{
        auto* world=enemy_world;ParticleLink link;
        if(lk.player){link.linked=true;link.kind=0;link.index=0;link.node=unsigned(joint);}
        else if(lk.w){unsigned i;if(!world->enemy_work_index(lk.w,i)){world->unbound_enemy_.insert("bhSetEffParticle: Linkwork ohne Raumfigur");return -1;}link.linked=true;link.kind=1;link.index=i;link.node=unsigned(joint);}
        if(kind<0||kind>=19){world->unbound_enemy_.insert("bhSetEffParticle: typ_no ausserhalb PtclTbl");return -1;}
        return world->particles_.spawn(unsigned(kind),link,{pos->x,pos->y,pos->z},{vec->x,vec->y,vec->z},color);};
    x.effect_particle_mk2=[](RECVX_T3_LINK lk,int joint,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* vec,uint32_t src_col,uint32_t dst_col,int typ_no)->int{
        auto* world=enemy_world;ParticleLink link;
        if(lk.player){link.linked=true;link.kind=0;link.index=0;link.node=unsigned(joint);}
        else if(lk.w){unsigned i;if(!world->enemy_work_index(lk.w,i)){world->unbound_enemy_.insert("bhSetEffParticleMk2: Linkwork ohne Raumfigur");return -1;}link.linked=true;link.kind=1;link.index=i;link.node=unsigned(joint);}
        if(typ_no<0||typ_no>=19){world->unbound_enemy_.insert("bhSetEffParticleMk2: typ_no ausserhalb PtclTbl");return -1;}
        return world->particles_.spawn_mk2(unsigned(typ_no),link,{pos->x,pos->y,pos->z},{vec->x,vec->y,vec->z},src_col,dst_col);};
    x.dansa=s.check_dansa;
    x.node_transform=en.node_local;
    x.player_way=[]()->int{return enemy_world->player_way_;};
    x.player_watr=en.player_capsule;
    x.player_id=[]()->int{return 0;}; // player.c:700 plp->id = 0
    x.player_obj_num=[]()->uint32_t{return uint32_t(enemy_world->player_joint_matrices_.size());};
    x.player_hit_set=[](int djnt_no,const RECVX_ENEMY_VEC* dv,const RECVX_ENEMY_VEC* dp){auto* world=enemy_world;world->player_hit_joint_=djnt_no;if(dv)world->player_hit_dv_={dv->x,dv->y,dv->z};if(dp)world->player_hit_dp_={dp->x,dp->y,dp->z};};
    x.player_hit_get=[](int* djnt_no,RECVX_ENEMY_VEC* dv,RECVX_ENEMY_VEC* dp)->int{auto* world=enemy_world;if(djnt_no)*djnt_no=world->player_hit_joint_;if(dv)*dv={world->player_hit_dv_.x,world->player_hit_dv_.y,world->player_hit_dv_.z};if(dp)*dp={world->player_hit_dp_.x,world->player_hit_dp_.y,world->player_hit_dp_.z};return 1;};
    x.player_joint_matrix=s.player_joint_matrix;
    x.player_get=s.player_get;x.player_set=s.player_set;x.sphere_capsule=s.sphere_capsule;
    x.flr_count=[]()->int{auto* world=enemy_world;world->enemy_floor_cache_=world->motion_floors();return int(world->enemy_floor_cache_.size());};
    x.flr_at=[](uint32_t i,RECVX_ENEMY_ATR* out)->int{const auto& c=enemy_world->enemy_floor_cache_;if(i>=c.size())return 0;*out=atr_of(c[i]);return 1;};
    x.etc_count=[]()->int{auto* world=enemy_world;world->enemy_step_cache_=world->motion_steps();return int(world->enemy_step_cache_.size());};
    x.etc_at=[](uint32_t i,RECVX_ENEMY_ATR* out)->int{const auto& c=enemy_world->enemy_step_cache_;if(i>=c.size())return 0;*out=atr_of(c[i]);return 1;};
    x.ene_count=s.enemy_count;x.ene_at=s.enemy_at;
    x.ene_is_player=[](uint32_t)->int{return 0;}; // the player is no RECVX_ENEMY_WORK in the native table
    x.player_body=[](RECVX_T3_PLAYER_BODY* out)->int{RECVX_ENEMY_PLAYER pl;if(!enemy_world->enemy_player_get(pl))return 0;out->pos=pl.pos;out->aio={0,0,0};out->car=pl.car;out->flg=pl.flg;out->stflg=pl.stflg;return 1;};
    x.player_stflg_or=[](uint32_t bits){auto* world=enemy_world;if(world->collision_events_)world->collision_events_->flags[14][0]|=bits;else world->collision_status_|=bits;};
    // --- EN01: the zonzon/effect helpers it reaches through its own contract ---
    en.calc_damage=[](RECVX_ENEMY_WORK* w,const RECVX_EN01_COMBWEP* wep,const RECVX_EN01_COMBJOINT* joint){recvx_t3_calc_damage(w,reinterpret_cast<const RECVX_T3_COMBWEP*>(wep),reinterpret_cast<const RECVX_T3_COMBJOINT*>(joint));};
    en.set_blood=[](RECVX_ENEMY_WORK* w,uint8_t type,const RECVX_EN01_BT* bt){recvx_t3_set_blood(w,type,reinterpret_cast<const RECVX_T3_BT*>(bt));};
    en.set_blood2=[](RECVX_ENEMY_WORK* w,uint8_t type,const RECVX_ENEMY_VEC* ofp,int16_t ry){RECVX_ENEMY_VEC ofs=*ofp;recvx_t3_set_blood2(w,type,&ofs,ry);};
    en.set_blood3=[](RECVX_ENEMY_WORK* w,int lnk_obj,const RECVX_ENEMY_VEC* ofs,const RECVX_ENEMY_VEC* dv,int n,int tex_id,int type,int wcnt){recvx_t3_set_blood3(w,lnk_obj,ofs,dv,n,tex_id,type,wcnt);};
    en.set_blood4=[](RECVX_ENEMY_WORK* w,int lnk_obj,const RECVX_ENEMY_VEC* ofs,int tex_id,int type){recvx_t3_set_blood4(w,lnk_obj,ofs,tex_id,type);};
    en.nikuhen_effect=[](RECVX_ENEMY_WORK* w,int type,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* v,int tex_id){recvx_t3_set_nikuhen_effect(w,type,pos,v,tex_id);};
    en.nikuhen_effect2=[](RECVX_ENEMY_WORK* w,int type,const RECVX_ENEMY_VEC* pos,int n,int tex_id){recvx_t3_set_nikuhen_effect2(w,type,pos,n,tex_id);};
    en.san_effect=[](RECVX_ENEMY_WORK* w,int jno,const RECVX_EN01_BT* bt){recvx_t3_set_san_effect(w,jno,reinterpret_cast<const RECVX_T3_BT*>(bt));};
    en.set_effect_tb=[](const RECVX_EN01_EF* tmpl,const RECVX_ENEMY_VEC* off,RECVX_ENEMY_WORK* link,int lkono)->int{
        auto* world=enemy_world;const int ay=(tmpl->keep&RECVX_EN01_EF_KEEP_AY)?world->effect_template_ay_:tmpl->ay;world->effect_template_ay_=ay;
        return world->effect_tb(tmpl->flg,tmpl->id,tmpl->type,unsigned(tmpl->mdlver),{tmpl->px,tmpl->py,tmpl->pz},{tmpl->sx,tmpl->sy,tmpl->sz},0,ay,off,link,false,lkono);};
    en.effect_slot_set=[](int slot,uint32_t mask,const RECVX_EN01_EF_SLOT* in){
        auto* world=enemy_world;if(slot<0||std::size_t(slot)>=world->sprites_.size()||!in)return;
        auto& s=world->sprites_[std::size_t(slot)].sprite;
        if(mask&RECVX_EN01_EFS_LINK)s.lkwkp=in->lkwkp;
        if(mask&RECVX_EN01_EFS_VEC){s.xn=in->xn;s.yn=in->yn;s.zn=in->zn;}
        if(mask&RECVX_EN01_EFS_STFLG)s.stflg|=in->stflg_or;
        if(mask&RECVX_EN01_EFS_TEX)s.tex_id=in->tex_id; /* txp[0]: see effect_patch */
        if(mask&RECVX_EN01_EFS_SPD)s.spd=in->spd;
        if(mask&RECVX_EN01_EFS_ANG){s.ax=in->ax;s.ay=in->ay;}
        if(mask&RECVX_EN01_EFS_CT){s.ct0=in->ct0;s.ct1=in->ct1;s.ct3=in->ct3;}
        if(mask&RECVX_EN01_EFS_SHP)world->unbound_enemy_.insert("eff[].shp_ct (kein Feld am nativen Effektwork)");
        if(mask&RECVX_EN01_EFS_AZ)s.az=in->az;
        if(mask&RECVX_EN01_EFS_CT3)s.ct3=in->ct3;
        if(mask&RECVX_EN01_EFS_EXP0)s.exp0=reinterpret_cast<unsigned char*>(in->exp0);};
    // --- EN04 (en04.h RECVX_EN04_SERVICES): same providers as EN01/T3 ---------
    dog.flr_count=[]()->uint32_t{return uint32_t(enemy_world->enemy_ext_.flr_count());};
    dog.flr_at=[](uint32_t i,RECVX_ENEMY_ATR* out)->int{return enemy_world->enemy_ext_.flr_at(i,out);};
    dog.stair_area_atr=en.stair_area_atr;
    // rutchk.c:5 bhCheckRoute: `out` is written for a valid zone pair and for the
    // same-zone case; otherwise the original leaves pos uninitialised.
    dog.check_route=[](const RECVX_ENEMY_VEC* from,const RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* out)->int{
        const auto& e=en01_services();if(e.route_id(from)==0xFF||e.route_id(to)==0xFF)return 0;e.route_next(from,to,out);return 1;};
    dog.collision_check_wall2=[](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_VEC* ps,const RECVX_ENEMY_VEC* ops,RECVX_ENEMY_VEC* pd,float ar,float ah)->int{return recvx_t3_collision_check_wall2(w,ps,ops,pd,ar,ah);};
    dog.calc_damage=[](RECVX_ENEMY_WORK* w,const RECVX_EN04_COMBWEP* wep,const RECVX_EN04_COMBJOINT* joint){recvx_t3_calc_damage(w,reinterpret_cast<const RECVX_T3_COMBWEP*>(wep),reinterpret_cast<const RECVX_T3_COMBJOINT*>(joint));};
    dog.weapon_flg=en.weapon_flg;
    dog.set_blood=[](RECVX_ENEMY_WORK* w,uint8_t type,const RECVX_EN04_BT* bt){recvx_t3_set_blood(w,type,reinterpret_cast<const RECVX_T3_BT*>(bt));};
    dog.set_blood2=[](RECVX_ENEMY_WORK* w,uint8_t type,RECVX_ENEMY_VEC* ofs,int32_t ry){recvx_t3_set_blood2(w,type,ofs,ry);};
    dog.player_set_blood=[](uint8_t type,const RECVX_EN04_BT* bt){recvx_t3_set_blood_player(type,reinterpret_cast<const RECVX_T3_BT*>(bt));};
    dog.player_set_blood2=[](uint8_t type,RECVX_ENEMY_VEC* ofs,int32_t ry){recvx_t3_set_blood2_player(type,ofs,ry);};
    dog.set_dfire_effect=[](RECVX_ENEMY_WORK* w,int no,const RECVX_EN04_BT* bt,int type)->int{return recvx_t3_set_dfire_effect(w,no,reinterpret_cast<const RECVX_T3_BT*>(bt),type);};
    dog.set_san_effect=[](RECVX_ENEMY_WORK* w,int no,const RECVX_EN04_BT* bt)->int{return recvx_t3_set_san_effect(w,no,reinterpret_cast<const RECVX_T3_BT*>(bt));};
    dog.nikuhen_effect2=[](RECVX_ENEMY_WORK* w,int type,const RECVX_ENEMY_VEC* pos,int n,int tex_id){recvx_t3_set_nikuhen_effect2(w,type,pos,n,tex_id);};
    // effsub3.c:145 bhSetBloodPoolLnk: BP_WORK layout shared with EffectParticles::spawn_pool.
    dog.blood_pool_lnk=[](RECVX_ENEMY_WORK*,const RECVX_ENEMY_VEC* pos,int32_t ay,const RECVX_ENEMY_BP_WORK* tab,int pal_bnk)->int{enemy_world->particles_.spawn_pool(tab,{pos->x,pos->y,pos->z},ay,pal_bnk);return 0;};
    dog.player_hit_check_parts=[](const RECVX_ENEMY_VEC* pos){RECVX_T3_LINK lk;lk.w=nullptr;lk.player=1;recvx_t3_hit_check_parts(lk,pos);};
    dog.player_hit_set=x.player_hit_set;dog.player_hit_get=x.player_hit_get;
    dog.player_capsule=en.player_capsule;
    dog.player_node_pos_set=[](uint32_t node,uint32_t axis,float v){
        auto* world=enemy_world;if(!world->player_asset_||axis>2){world->unbound_enemy_.insert("plp->mlwP->objP[node].pos (Spielerbaum)");return;}
        const unsigned physical=world->player_asset_->model_index(node);
        auto& edits=world->entity(0,0).node_overrides;auto pose=world->model_pose_physical(0,0,physical);
        const auto at=edits.find(physical);if(at!=edits.end()&&at->second.position)pose.position=*at->second.position;
        (axis==0?pose.position.x:axis==1?pose.position.y:pose.position.z)=v;
        edits[physical].position=pose.position;world->player_transient_nodes_.push_back(physical);};
    dog.player_exp0_float_set=[](uint32_t offset,float v){enemy_world->player_exp0_writes_.push_back({offset,v});};
    // sys->obwp[] / rom->obj_n: the room objects are the kind-2 entities.
    dog.object_count=[]()->uint32_t{uint32_t n=0;for(const auto& pair:enemy_world->entities_)if(pair.first.first==2&&pair.first.second+1>n)n=pair.first.second+1;return n;};
    dog.object_get=[](uint32_t index,RECVX_EN04_OBJECT* out)->int{const auto* e=enemy_world->state(2,index);if(!e)return 0;
        *out=RECVX_EN04_OBJECT{};out->flg=e->object_flags;out->flg2=e->object_flg2;out->id=uint16_t(e->identity);out->mode1=uint8_t(e->object_mode1);out->mode3=uint8_t(e->object_mode3);out->ct0=e->object_ct0;
        out->px=e->transform.position.x;out->py=e->transform.position.y;out->pz=e->transform.position.z;out->ay=e->transform.yaw;out->ayp=e->object_ayp;return 1;};
    dog.object_set=[](uint32_t index,const RECVX_EN04_OBJECT* in){auto* world=enemy_world;if(!world->state(2,index)){world->unbound_enemy_.insert("sys->obwp[index] ausserhalb rom->obj_n");return;}
        auto& e=world->entity(2,index);e.object_flags=in->flg;e.object_flg2=in->flg2;e.object_mode1=in->mode1;e.object_mode3=in->mode3;e.object_ct0=in->ct0;e.transform.position={in->px,in->py,in->pz};e.transform.yaw=in->ay;e.object_ayp=in->ayp;
        // The consumers of these O_WRK fields (objitm.c bhObj052 wire shake, the
        // door object's ayp/mode3 drive) are not ported; the values are kept.
        world->unbound_enemy_.insert("sys->obwp Schreibzugriff: Objektprozedur (objitm.c) nicht portiert");};
    dog.player_speed=[](float* out)->int{*out=enemy_world->player_spd_;return 1;};
    // en04.c:914-933: bhEne_CalcPartsPos(plp, ply_tree{0..5}, parts_num) from the
    // player root, ops from owP[previous_joint] of the last pass and plp->pyb,
    // then bhEne_CollisionCheckWall2(plp, ps, ops, pd, ar, ah).
    dog.player_collision_parts=[](uint32_t parts_num,uint32_t previous_joint,RECVX_ENEMY_VEC* delta)->int{
        auto* world=enemy_world;if(!world->player_asset_||parts_num>6)return 0;
        if(previous_joint>=world->player_joint_matrices_.size())return 0;
        NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,world->player_.position.x,world->player_.position.y,world->player_.position.z);njRotateXYZ(&m,world->player_.pitch,world->player_.yaw,world->player_.roll);
        for(uint32_t i=0;i<parts_num;++i){const auto pose=world->model_pose(0,0,i);njTranslate(&m,pose.position.x,pose.position.y,pose.position.z);njRotateXYZ(&m,pose.angles[0],pose.angles[1],pose.angles[2]);}
        NJS_POINT3 to{m[12],world->player_.position.y,m[14]};
        const auto& prev=world->player_joint_matrices_[previous_joint].value;const NJS_POINT3 from{prev[12],world->player_prev_pos_.y,prev[14]};
        MotionActor actor=world->motion_actor();auto walls=world->collision_geometry();auto steps=world->motion_steps();auto floors=world->motion_floors();
        MotionTerrain terrain{walls,steps,floors,world->floors_,world->room_height_};
        const NJS_POINT3 ps=to;const float ar=3.5f,ah=world->player_character_?17.5f:16.5f;
        resolve_motion_wall(terrain,actor,to,from,ar,ah);
        *delta={to.x-ps.x,to.y-ps.y,to.z-ps.z};return 1;};
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

bool RoomWorld::enemy_work_index(const RECVX_ENEMY_WORK* w,unsigned& index)const{
    if(!w)return false;
    for(const auto& pair:entities_)if(pair.first.first==1&&&pair.second.enemy==w){index=pair.first.second;return true;}
    return false;
}
// effect.c:436 bhSetEffectTb(efp, off, lkp, lkono) for every enemy-side caller:
// the EF_WORK template fields, the joint offset and the link target.
int RoomWorld::effect_tb(unsigned flg,int id,int type,unsigned mdlver,NJS_POINT3 position,NJS_POINT3 scale,int ax,int ay,const RECVX_ENEMY_VEC* ofs,RECVX_ENEMY_WORK* link_w,bool link_player,int lkono){
    EffectSpawn e;e.flg=flg;e.id=unsigned(id);e.type=unsigned(type);e.mdlver=mdlver;e.position=position;e.scale=scale;e.ax=ax;e.ay=ay;e.lkono=lkono;
    if(ofs){e.has_offset=true;e.offset={ofs->x,ofs->y,ofs->z};}
    unsigned kind=0,index=0;
    if(link_player)kind=1;
    else if(link_w){if(!enemy_work_index(link_w,index)){unbound_enemy_.insert("bhSetEffectTb: Linkwork ohne Raumfigur (Kindwork)");return -1;}kind=2;e.linked_work=link_w;}
    return spawn_effect(e,kind,index);
}
const MotionClip* RoomWorld::player_enemy_clip()const{
    // en07.c:2190/2221 hands the player work the library of the grabbing enemy:
    // plp->mnwP = epw->mnwP, and room.c:375 sets epw->mnwP = sys->emtp[epw->id].
    // mtn_enemy_owner is therefore the identity, and the native library table is
    // keyed the same way; the room-motion form of bhSetEneMtn lives on the work
    // of that identity and is honoured by enemy_mn_work.
    // plp->mnwP = epw->mnwP: the grabbing work itself. Two works of one id
    // can hold different mnwP (one in the bhSetEneMtn room form), so the
    // work pointer decides first and the identity only stands in for it.
    if(player_enemy_work_&&player_enemy_work_->owner){const auto& e=*static_cast<const WorldEntity*>(player_enemy_work_->owner);if(e.enemy_bound)return enemy_mn_work(e,unsigned(motion_));}
    for(const auto& pair:entities_){
        if(pair.first.first!=1||!pair.second.enemy_bound)continue;
        if(player_enemy_owner_!=0&&pair.second.identity!=player_enemy_owner_)continue;
        return enemy_mn_work(pair.second,unsigned(motion_));
    }
    return nullptr;
}
void RoomWorld::control_enemy_player_motion(){
    // player.c:1502 inside bhControlPlayer (game.c:48/49: after bhControlEnemy):
    // bhSetMotion(plp, mtn_add, mtn_md, mtn_tp) runs on whichever library
    // plp->mnwP points at — the grabbing enemy's (PlyDG00 case 0..3) or the
    // player's own again once mnwPb is restored (case 4/5 waits for
    // hokan_count == 0). The frame clock, the frm_num wrap and the hokan
    // countdown are Motion.c:79-115; the pose itself is sampled by the host.
    if(!enemy_player_driving_||player_frozen_||!(simulation_flags_&1u))return;
    const MotionClip* clip=nullptr;
    if(player_enemy_motion_)clip=player_enemy_clip();
    else if(player_asset_&&player_asset_->has_motion(motion_))clip=&player_asset_->motion_clip(motion_);
    if(!clip){unbound_enemy_.insert(player_enemy_motion_?"bhSetMotion(plp): mnwP[mtn_no] des Gegners fehlt":"bhSetMotion(plp): mnwPb[mtn_no] fehlt");return;}
    {
        MotionWork m{std::int32_t(frame_),0u,player_hokan_count_,0u,player_hokan_rate_};
        std::vector<ModelPose> obj=player_asset_->local_pose_clip(*clip,frame_); /* mlwP->objP scratch: the host samples the pose */
        const std::vector<std::uint32_t> owp(obj.size(),0u);
        std::string gap;
        (void)set_motion(m,int(player_step_),player_mtn_md_,clip,nullptr,obj,owp,&gap);
        if(!gap.empty())unbound_enemy_.insert(gap+" (plp)");
        frame_=m.frm_no;player_hokan_count_=m.hokan_count;
    }
    const unsigned total=clip->frames;
    if(total>0){
        const unsigned current=unsigned(frame_/65536);
        if(current>=total-1u){
            if(collision_events_)collision_events_->flags[13][0]|=0x400000u;
            collision_flags_|=0x400000u;
        }else{
            if(collision_events_)collision_events_->flags[13][0]&=~0x400000u;
            collision_flags_&=~0x400000u;
        }
    }
}
unsigned RoomWorld::player_motion_frames()const{
    // One plp->mtn_no indexes whichever library plp->mnwP points at; while a
    // grab owns the player that is the controlled work's, not the player's.
    if(player_enemy_motion_){const auto* clip=player_enemy_clip();return clip?clip->frames:0u;}
    return player_asset_&&player_asset_->has_motion(motion_)?player_asset_->motion_frames(motion_):0u;
}

const MotionClip* RoomWorld::enemy_mn_work(const WorldEntity& e,std::uint32_t mtn)const{
    // mnwP is sys->rmthp (bhEneMotionSet) or the work's own emtp[id]; ein
    // Kind spielt die mnwP seines Elternworks (en01.c:2829). Der Oberkoerper
    // des Zombies ist ein eigener Kindwork mit eigenem mtn_no (mtn + 0xc8),
    // der ueber objP/owP += 8 in denselben Baum schreibt (en01.c:10347) —
    // deshalb liefert mnwP[mtn] hier genau den einen Clip, ohne Zusammenfuehrung.
    if(e.enemy.mtn_room){if(mtn<room_motions_.size()&&room_motions_[mtn].frames)return &room_motions_[mtn];return nullptr;}
    const unsigned identity=e.motion_library>=0?unsigned(e.motion_library):e.identity;
    const auto library=enemy_motions_.find(identity);
    if(library==enemy_motions_.end()||mtn>=library->second.entries.size()||!library->second.entries[mtn].second)return nullptr;
    return &enemy_library_clip(identity,mtn);
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
    en01_ctx.en01 = &en01_services();
    en01_ctx.game_mode = enemy_game_mode_;
    en01_ctx.stg_no = collision_events_ ? std::int32_t(collision_events_->values[0]) : 0;
    en01_ctx.rom_no = collision_events_ ? std::int32_t(collision_events_->values[1]) : 0;
    en01_ctx.stage_valid = collision_events_ ? 1u : 0u;
    recvx_en01_set_context(&en01_ctx);
    const RECVX_EN04_CONTEXT en04_ctx{&en04_services(),enemy_game_mode_};
    recvx_en04_set_context(&en04_ctx);
    recvx_en04_set_data(recvx_en04_default_data());
    recvx_enemy_table_init(&enemy_table_);   // bhInitEnemy
    recvx_enemy_dispatch_init(&enemy_dispatch_);
    {   // EN11: nur der Anbieter, der am vorhandenen Besitzer existiert. flags_clear, effect_*, node_* bleiben NULL,
        // also benannte Luecken (bhEne11_Init meldet sie), keine Ersatzwerte.
        static RECVX_EN11_SERVICES en11{};
        en11.se_pan=[](uint32_t enemy,const RECVX_ENEMY_VEC* p,int se){enemy_world->audio_.request_enemy_se(enemy,{p->x,p->y,p->z},se,0,0);}; // zonzon1.c:852 bhEne_SetSEPan
        recvx_en11_bind(&en11);}
    enemy_exp_blocks_.clear();enemy_exp_used_=0;enemy_mobile_walls_.clear();enemy_children_.clear();
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

bool RoomWorld::enemy_motion_available(const WorldEntity& e,unsigned mtn,bool room)const{
    if(room)return mtn<room_motions_.size()&&room_motions_[mtn].frames!=0;
    const unsigned identity=e.motion_library>=0?unsigned(e.motion_library):e.identity;
    const auto library=enemy_motions_.find(identity);
    return library!=enemy_motions_.end()&&mtn<library->second.entries.size()&&library->second.entries[mtn].second!=0;
}

// en01.c:1093 zob_lkmtab = {flg 0x81, id 1, sonst 0}; bhSetEnemy belegt damit
// den ersten freien ene[]-Platz (en01.c:2817). Das Kind gehoert zum selben
// Raumgegner wie sein Elternwork (owner_index fuer die Zielwahl). mnwP des
// Kindes ist die des Elternworks (:2829).
WorldEntity* RoomWorld::create_enemy_child(RECVX_ENEMY_WORK* parent){
    if(!parent)return nullptr;
    const auto& p=owner_of(parent);
    auto& c=enemy_children_.emplace_back();
    RECVX_ENEMY_TEMPLATE t{};t.flg=0x81u;t.id=1;
    if(!recvx_enemy_set(&enemy_table_,&c.enemy,&t,&c,parent->owner_index,&enemy_services_)){enemy_children_.pop_back();return nullptr;}
    c.enemy_bound=true;c.enemy_native=true;c.identity=1;
    c.motion_library=p.motion_library>=0?p.motion_library:int(p.identity);
    return &c;
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
    // bhCalcTree(ewP->mtx, ewP->mlwP): der Baum ist der des mlwP — bei einem
    // Kind mit geteiltem mlwP der des Elternworks, ab dessen aktuellem Fenster.
    auto& t=e.model_owner?*e.model_owner:e;
    if(!t.enemy_asset){unbound_enemy_.insert("bhCalcModel: mlwP fehlt (kein Modell)");return;}
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
    if(!t.model_window)t.enemy_owp_mtx=t.enemy_asset->tree_matrices(t.enemy_obj,root,t.enemy_owp_flg,t.enemy_owp_mtx);
    // Mit verschobenem objP/owP laeuft bhCalcTree nur ueber die obj_num Knoten
    // ab dem Fenster und schreibt nur deren owP (MdlPut.c:262-310).
    else if(t.model_window<t.enemy_obj.size())t.enemy_asset->tree_window(t.enemy_obj,root,t.model_window,t.enemy_owp_flg,t.enemy_owp_mtx);
    else {unbound_enemy_.insert("bhCalcModel: objP-Fenster ausserhalb des Modells");return;}
    t.enemy_calculated=true;
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
    // F03: die Dienste der portierten Gegner-Zustandsmaschinen, die obj_a/obj_b
    // waehlen. Die Modulglobale gelten nur waehrend dieses Passes, wie die
    // T3-Ergaenzung oben. recvx_en12_bind haengt am einzelnen Work, weil es
    // dessen exp0-Block anlegt.
    recvx_en14_bind_services(en14_resolver);
    recvx_en18_bind(&en18_services());
    recvx_en30_bind(&en30_services());
    for(auto& pair:entities_)if(pair.first.first==1&&pair.second.enemy_bound&&pair.second.identity==12)
        recvx_en12_bind(&pair.second.enemy,&en12_services());
    RECVX_EN01_CONTEXT en01_ctx{};
    en01_ctx.table = &enemy_table_;
    en01_ctx.services = &enemy_services_;
    en01_ctx.en01 = &en01_services();
    en01_ctx.game_mode = enemy_game_mode_;
    // sys->stg_no / sys->rom_no: dieselben Felder, die bhSv (event.c:813/816,
    // Schluessel 0/1) und der Spielstand schreiben — hier also Werte[0]/[1].
    // Ohne EventVm ist der Satz nicht belegt und wird als Luecke gemeldet.
    en01_ctx.stg_no = collision_events_ ? std::int32_t(collision_events_->values[0]) : 0;
    en01_ctx.rom_no = collision_events_ ? std::int32_t(collision_events_->values[1]) : 0;
    en01_ctx.stage_valid = collision_events_ ? 1u : 0u;
    recvx_en01_set_context(&en01_ctx);
    const RECVX_EN04_CONTEXT en04_ctx{&en04_services(),enemy_game_mode_};
    recvx_en04_set_context(&en04_ctx);
    const unsigned enemy_sp_flg = enemy_ai_ ? simulation_flags_ : (simulation_flags_ & ~2u);
    recvx_enemy_control(&enemy_table_,&enemy_dispatch_,enemy_sp_flg,&enemy_services_);
    recvx_en04_set_context(nullptr);
    recvx_en01_set_context(nullptr);
    recvx_en14_bind_services(nullptr);
    recvx_en18_bind(nullptr);
    recvx_en30_bind(nullptr);
    recvx_enemy_ext_bind(nullptr);recvx_en11_bind(nullptr);
}
}
