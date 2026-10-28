#include <optional>
#include "combat.h"
#include "world.h"
#include "effect_shadow.h"
#include "enemies/en11.h"
#include "enemies/en16.h"
#include "enemies/en09.h"
#include "enemies/en09_tables.h"
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
#include "enemies/en13.h"
#include "enemies/en23.h"
#include "enemies/en53.h"
#include "enemies/en02.h"
#include "enemies/en05.h"
#include "enemies/en19.h"
#include "enemies/en29.h"
#include "enemies/en17.h"
#include "enemies/en03_spider.h"
#include "enemies/en03.h"
#include "enemies/en15.h"
#include "enemies/en18.h"
#include "enemies/en21.h"
#include "effect_sprites_internal.h"
#include "effect_rap_camera.h"
#include "effect_rap_blood.h"
#include "effect_rap_claw.h"
#include "enemies/en30.h"
#include "enemies/en06.h"
#include "enemies/en27.h"
#include "enemies/en22.h"
#include "enemies/en26.h"
#include "effect_collision.h"
#include "combat_geometry.h"
#include "wall_capsule.h"
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

// Die EN19-Dienste von control_enemy_works. Sie liegen in einer funktionslokalen
// static, leben also prozessweit; dieser Zeiger merkt sie fuer den Effektpass, der
// mit bhEff349 (effsub3.c:3234) bhEne19_TyBloodSet aufruft. Vor dem ersten Gegnerpass
// NULL: dann meldet en19.c seine Dienstluecke, statt etwas zu erfinden.
const RECVX_EN19_SERVICES* en19_effect_services=nullptr;
// Die T3-Ergaenzung (en03_lib.c) ist prozessweit gebunden und gilt nur
// waehrend des Aufrufs, genau wie in control_enemy_works.
struct EnemyExtScope {
    explicit EnemyExtScope(RECVX_ENEMY_EXT* ext){recvx_enemy_ext_bind(ext);}
    ~EnemyExtScope(){recvx_enemy_ext_bind(nullptr);}
};
WorldEntity& owner_of(RECVX_ENEMY_WORK* w){return *static_cast<WorldEntity*>(w->owner);}
const WorldEntity& owner_of(const RECVX_ENEMY_WORK* w){return *static_cast<const WorldEntity*>(w->owner);}
// mlwP: der Objektbaum, den ein Work liest und schreibt. Ein Kind mit
// `epp->mlwP = epw->mdl` (en01.c:2827) hat keinen eigenen.
WorldEntity& tree_of(WorldEntity& e){return e.model_owner?*e.model_owner:e;}
const WorldEntity& tree_of(const WorldEntity& e){return e.model_owner?*e.model_owner:e;}
// owP[n]/obj_num des Originals im nativen Baum. bhInitMask (face.c:7, room.c:445-449
// fuer id > 90) fuegt ueber fmCnkInitContext Zahn-/Zungen-/Augenobjekte als Kinder ein
// (face_bh.c:535-561, je obj_num++) und legt mit _fmSetObjP (face_bh.c:67) objP und owP
// in Baumreihenfolge neu an; bhCalcTree (MdlPut.c:268) laeuft ueber diese obj_num
// Objekte. owP[n] ist daher der logische Knoten n (model_index), wie bei link_matrix
// und enemy_slot_joint_matrix. objP/owP += k (en01.c:2799, en26.c:1180) verschiebt
// im selben Indexraum. Ohne Maske ist die Abbildung die Identitaet.
std::optional<std::size_t> owp_node(const WorldEntity& t,std::uint32_t joint){
    if(!t.enemy_asset)return std::nullopt;
    try{return std::size_t(t.enemy_asset->model_index(joint+t.model_window));}catch(const std::runtime_error&){return std::nullopt;}
}

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
    // mlwP = &epw->mdl[base] (en13.c:111-184): mdl[model] des Kindes ist mdl[base+model] des Elternworks.
    if(e.model_bank){const unsigned k=e.model_base+model;return k==0?e.model_bank:e.model_bank->model(k);}
    const PlayerAsset* row=e.model_row();if(!row)return nullptr; // mdl[0], auch nach select_model
    return model==0?row:row->model(model);
}
int morph_variant_index(const WorldEntity& e,const PlayerAsset* variant){
    if(!variant)return -1;
    if(e.model_bank){for(unsigned k=e.model_base;k<e.model_bank->model_count();++k)if(morph_variant(e,k-e.model_base)==variant)return int(k-e.model_base);return -1;}
    const PlayerAsset* row=e.model_row();if(!row)return -1;
    if(variant==row)return 0;
    for(unsigned i=1;i<row->model_count();++i)if(row->model(i)==variant)return int(i);
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
// objP von mdl[k] der eigenen Reihe: die Pose des aktiven mlwP oder die abgelegte eines anderen
// mdl[k] (select_model); ein noch unberuehrtes mdl[k] beginnt mit seiner Ruhelage.
static std::vector<ModelPose>* own_model_pose(WorldEntity& e,unsigned k){
    if(k==(e.own_row?e.own_model:0u))return &e.enemy_obj;
    const auto* asset=morph_variant(e,k);if(!asset)return nullptr;
    auto& keep=e.own_models[k];
    if(keep.obj.empty()){keep.obj=asset->bind_pose();keep.owp_flg.assign(keep.obj.size(),0u);keep.owp_mtx.assign(keep.obj.size(),BoneMatrix{});}
    if(k==0&&keep.evalflags.empty())keep.evalflags=asset->node_evalflags();
    return &keep.obj;
}
// en30.c:413/:463 objA->pos[1] = objB->pos[1] = y: obj_a = mdl[a].objP, obj_b = mdl[b].objP.
int en30_morph_node_set_y(RECVX_ENEMY_WORK* w,uint32_t node,float y){
    if(!w)return 0;auto& e=owner_of(w);
    auto* a=own_model_pose(e,e.morph_obj_a);auto* b=own_model_pose(e,e.morph_obj_b);
    if(!a||!b||node>=a->size()||node>=b->size())return 0;
    (*a)[node].position.y=y;(*b)[node].position.y=y;return 1;
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
    // mdl[model].objP[node].pos[1]: die aktive Reihe (mlwP) oder die abgelegte Pose von mdl[model].
    auto* pose=own_model_pose(owner_of(w),model);if(!pose||node>=pose->size())return 0;
    (*pose)[node].position.y=y;return 1;
}
// en30.c:161-164/:809-810: objP[node].evalflags Bit 0x8 je Modell.
int en30_model_node_hidden(RECVX_ENEMY_WORK* w,uint32_t model,uint32_t node,int hidden){
    if(!w)return 0;auto& e=owner_of(w);
    if(model==(e.own_row?e.own_model:0u)){ // mdl[model] ist mlwP
        if(node>=e.enemy_evalflags.size())return 0;
        if(hidden)e.enemy_evalflags[node]|=8u;else e.enemy_evalflags[node]&=~8u;
        return 1;
    }
    if(model==0){auto& flags=e.own_models[0].evalflags;if(flags.empty())flags=e.model_row()->node_evalflags();
        if(node>=flags.size())return 0;if(hidden)flags[node]|=8u;else flags[node]&=~8u;return 1;}
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
    RECVX_EN18_SERVICES t{};t.model_root=en18_model_root;t.select_root=en18_select_root;t.morph_pair=en18_morph_pair;
    // en13.c:1033-1094: die EN13-Routinen, die bhEne18 ruft.
    t.parent_hatch_no=recvx_en13_hatch_no;t.parent_tenta_no=recvx_en13_tenta_no;
    t.parent_put_attacker=recvx_en13_put_attacker;t.parent_tentacle=recvx_en13_tentacle;return t;}();return s;}
RECVX_EN01_SERVICES& en01_services();
const RECVX_EN30_SERVICES& en30_services(){static const RECVX_EN30_SERVICES s=[]{
    RECVX_EN30_SERVICES t{};t.model_node_rest=en30_model_node_rest;t.model_node_set_y=en30_model_node_set_y;
    t.model_node_hidden=en30_model_node_hidden;t.morph_node_set_y=en30_morph_node_set_y;t.morph_sources=en30_morph_sources;
    // rutchk.c:5 bhCheckRoute: derselbe Anbieter wie EN01 (Rueckgabe now oder 0xFF).
    t.route_points=[](const RECVX_ENEMY_VEC* from,const RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* out)->int{return en01_services().route_next(from,to,out);};
    return t;}();return s;}
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
    out.debug_attack_disabled=!enemy_grab_||!enemy_attacks_; // Host-Schalter Gegner-Angriff (Griff) und Gegnerangriffe (alle Wirkungen)
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
    player_lib_seen_motion_=player_enemy_motion_;player_lib_seen_owner_=player_enemy_owner_;player_lib_seen_work_=player_enemy_work_;
    out.wpnr_no=int(player_weapon_);out.ply_id=unsigned(player_character_);out.exp1_flags=*player_head_flags_;
    // player.c:199/708: PlyInfo[sys->ply_id] = {3.5, 16.5/17.5}; car/cah sind
    // ar/ah - 1.0f. ply_id 0 ist Claires Mass, 1..3 das der uebrigen Figuren.
    out.car=2.5f;
    out.cah=(player_character_?17.5f:16.5f)-1.0f;
    return true;
}
bool RoomWorld::enemy_player_set(const RECVX_ENEMY_PLAYER& in){
    if(!player_asset_||!player_hp_owner_||!player_head_flags_)return false;
    if(std::getenv("RECVX_POSITION_TRACE")&&(player_.position.x!=in.pos.x||player_.position.z!=in.pos.z))std::fprintf(stderr,"POSITION_WRITE mode=%u enemy=%u old=%.8g,%.8g,%.8g new=%.8g,%.8g,%.8g flg=%x stflg=%x floor=%d\n",unsigned(in.mode0),unsigned(enemy_table_.enow),player_.position.x,player_.position.y,player_.position.z,in.pos.x,in.pos.y,in.pos.z,in.flg,in.stflg,int(in.flr_no));
    /* Host-Komfortschalter "Gegnerangriffe" (enemy_attacks_, keine Originalmechanik): ein Pass, der den
       Spieler neu in einen gegnergefuehrten Modus (mode0 4 Griff, 5 Wurf/enedam, 6 enedie) nimmt, wird
       bei ausgeschaltetem Schalter vollstaendig verworfen; HP-Abzug, Toetung und neue Giftbits werden
       weiter unten verworfen. Der Gegnerzustand selbst bleibt unangetastet. */
    const unsigned current_mode0=collision_mode_&0xffu;
    const bool driven=in.mode0==4||in.mode0==5||in.mode0==6;
    const bool currently_driven=current_mode0==4||current_mode0==5||current_mode0==6;
    if(!enemy_attacks_&&driven&&!currently_driven)return true;
    player_.position={in.pos.x,in.pos.y,in.pos.z};player_.pitch=in.ax;player_.yaw=in.ay;player_.roll=in.az;
    collision_floor_=in.flr_no;
    /* Host-Schalter "Gegner-Angriff" (debug_attack_disabled == !enemy_grab_):
       Solange er aus ist, verliert der Spieler durch native Gegner kein HP.
       Das ist die gemeinsame Rueckschreibestelle des Spielerworks; andere
       Gegner pruefen denselben Schalter schon an ihrem Angriffsbeginn
       (en01_core.c, en04.c, en07.c BR00 :341, en09_behavior.inc, en10.c,
       en21.c). Diese Sperre allein haelt einen Angriff NICHT auf: Griff,
       Spielermotion, Stimme und Vibration laufen weiter (so bei den
       Fledermaeusen vor dem Tor in en07.c). Sie bleibt nur Rueckhalt fuer
       Module ohne eigenes Tor am Angriffsbeginn. Keine Annahme: nur der
       Abzug wird gesperrt, jede uebrige Spielerfeld-Uebernahme bleibt. */
    const bool player_damage_allowed=(enemy_grab_&&enemy_attacks_)||in.hp>=*player_hp_owner_;
    if(player_damage_allowed)*player_hp_owner_=in.hp;
    std::uint32_t stflg=in.stflg;
    {   // plp->stflg 0x80000 (poison) und 0x200000 (poison2) gehoeren dem Inventarbesitzer
        // (inventory.h:26). Ein Gegner-/Effektpass, der sie neu setzt (effsub2.c:1452
        // bhEff_E03_Acid, en15.c:2572 PoisonAttack), reicht sie dorthin weiter.
        const std::uint32_t before=collision_events_?collision_events_->flags[14][0]:collision_status_;
        // Gegnerangriffe aus: neu gesetzte Giftbits werden verworfen (bestehende bleiben).
        if(!enemy_attacks_)stflg=(stflg&~0x280000u)|(before&0x280000u);
        if((stflg&0x80000u)&&!(before&0x80000u))player_poison_request_=true;
        if((stflg&0x200000u)&&!(before&0x200000u))player_infect_request_=true;
    }
    if(collision_events_){collision_events_->flags[13][0]=in.flg;collision_events_->player_flags=in.flg;
        collision_events_->flags[14][0]=stflg;collision_events_->flags[15][0]=in.flg2;
    }else {collision_flags_=in.flg;collision_status_=stflg;collision_flags2_=in.flg2;}
    player_at_flags_=in.at_flg;
    collision_mode_=unsigned(in.mode0)|(unsigned(in.mode1)<<8)|(unsigned(in.mode2)<<16)|(unsigned(in.mode3)<<24);
    player_ct0_=in.ct0;player_ct1_=in.ct1;player_ayp_=in.ayp;
    motion_=in.mtn_no;frame_=in.frm_no;player_step_=in.mtn_add;
    player_mtn_md_=in.mtn_md;player_hokan_count_=in.hokan_count;player_hokan_rate_=in.hokan_rate;
    // plp->mnwP: nur eine Aenderung der Sicht selbst uebernehmen (siehe player_lib_seen_*).
    if((in.mtn_enemy!=0)!=player_lib_seen_motion_||in.mtn_enemy_owner!=player_lib_seen_owner_||in.mtn_enemy_work!=player_lib_seen_work_){
        player_enemy_motion_=in.mtn_enemy!=0;player_enemy_owner_=in.mtn_enemy_owner;player_enemy_work_=in.mtn_enemy?in.mtn_enemy_work:nullptr;}
    player_lib_seen_motion_=player_enemy_motion_;player_lib_seen_owner_=player_enemy_owner_;player_lib_seen_work_=player_enemy_work_;
    player_weapon_=unsigned(in.wpnr_no);*player_head_flags_=in.exp1_flags;
    // The existing input owner must not run free locomotion during enemy
    // damage/death modes. This is ownership, not a replacement player phase.
    // mode0 5 (bhCPM0_enedam, player.c:6635) ist Zeile fuer Zeile bhCPM0_nage
    // (mode0 4, :6596): der Gegner fuehrt den Spieler (EN19 bhEne19_PlySetDamage,
    // en19.c:3827 setzt plp->mode0 = 5).
    if(driven){enemy_player_driving_=true;grab_driving_=true;player_scripted=true;player_room_motion_=false;player_smove_.active=false;}
    else if(enemy_player_driving_){enemy_player_driving_=false;grab_driving_=false;player_scripted=false;player_enemy_motion_=false;player_enemy_work_=nullptr;player_link_matrix_valid_=false;}
    if(in.hp<0&&player_damage_allowed)player_killed_=true;
    // bhCPM0_enedie (player.c:6711) ist der einzige Leser von plp->flg 0x2 fuer den Game-Over-Start
    // und laeuft nur in mode0 6; bhCPM0_die (mode0 3) setzt flg 0x10002 selbst und startet ueber
    // den Bildwechsel seiner Fallmotion, nicht ueber dieses Bit.
    // Nur plp->flg 0x2, nicht hp < 0: en01 PlyDG00 case 7 (en01.c:7748) setzt es erst am letzten Bild
    // der Spieler-Todesmotion (412/413), bhCPM0_enedie startet dann; en19.c:4385 u.a. ebenso.
    if(player_damage_allowed&&in.mode0==6&&(in.flg&2u)!=0)player_dead_=true;
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
int RoomWorld::set_enemy_shadow(const std::int8_t* jtb,RECVX_ENEMY_WORK* link,int lkono,float sx,float sy,float sz){
    // effect.c:644 bhSetShadow: the first eff[512] slot without flg & 3, zeroed,
    // flg 0x240001, id 1, type 0 with a joint chain (1 without), tex_id -1,
    // scale and its copy, lkwkp/lkono with flg 0x80, jno[] up to the -1.
    const int slot=reserve_effect_slot();if(slot<0)return -1;
    auto& work=sprites_[unsigned(slot)];auto& o=work.sprite;
    o.flg=0x240001u;o.id=1;o.type=jtb?0u:1u;o.tex_id=-1;o.mdlver=0;o.px=o.py=o.pz=0;
    o.sx=o.sxb=sx;o.sy=o.syb=sy;o.sz=o.szb=sz;o.ax=o.ay=o.az=0;o.lkono=lkono;
    if(link){o.lkwkp=link;o.flg|=0x80u;}
    o.jno.fill(0);
    if(!jtb)o.jno[0]=-1;else for(unsigned j=0;j<15;++j){o.jno[j]=jtb[j];if(jtb[j]==-1)break;}
    work.hidden=false;work.queued=false;
    return slot;
}
// effsub3.c:1640 rySetShadow: AllocOworkOne (erster freier eff[]-Platz ab 0, genullt),
// flg 0x8240001, id 304, mlwP = &sys->efm[1] (dieselben Modifier-Modelle wie bhEff001).
// ct0..ct2 halten im Original Zeiger auf owP[obj].mtx[12] des ewP->mlwP; nativ stehen
// dort die Gelenknummern, gelesen wird im Effektpass dieselbe owP-Matrix.
int RoomWorld::ry_set_shadow(RECVX_ENEMY_WORK* w,int obj0,int obj1,int obj2,float off_a,float off_b){
    const int slot=reserve_effect_slot(0);if(slot<0)return -1;
    auto& work=sprites_[unsigned(slot)];auto& o=work.sprite;
    o.flg=0x8240001u;o.id=304;o.type=0;o.tex_id=-1;o.mdlver=0;
    o.sx=o.sy=o.sz=1.0f;o.ax=o.ay=o.az=0;o.px=o.py=o.pz=0;
    o.lkwkp=w;o.lkono=0;o.xn=off_a;o.zn=off_b;o.ct0=obj0;o.ct1=obj1;o.ct2=obj2;
    work.hidden=false;work.queued=false;
    return slot;
}
// effsub3.c:1683 bhEff304.
void RoomWorld::tick_ry_shadow(unsigned slot){
    auto& work=sprites_.at(slot);auto& op=work.sprite;
    work.queued=false;
    WorldEntity* linked=nullptr;
    if(op.lkwkp){auto* w=static_cast<RECVX_ENEMY_WORK*>(op.lkwkp);if(w->owner)linked=static_cast<WorldEntity*>(w->owner);}
    if(!linked)for(auto& entry:entities_)if(entry.second.enemy_bound&&op.lkwkp==&entry.second.enemy){linked=&entry.second;break;}
    if(!linked){unbound_enemy_.insert("bhEff304: lkwkp ohne nativen Besitzer");return;}
    const auto& tree=linked->model_owner?*linked->model_owner:*linked;
    auto point=[&](int obj,NJS_POINT3& out)->bool{
        const auto index=std::size_t(tree.model_window)+unsigned(obj);
        if(obj<0||!tree.enemy_calculated||index>=tree.enemy_owp_mtx.size())return false;
        const auto* m=tree.enemy_owp_mtx[index].value;out={m[12],m[13],m[14]};return true;};
    NJS_POINT3 a,b,c;
    if(!point(op.ct0,a)||!point(op.ct1,b)||!point(op.ct2,c)){unbound_enemy_.insert("bhEff304: owP[obj].mtx ausserhalb des Modells");return;}
    const NJS_POINT3* p0=&a;const NJS_POINT3* p1=&b;
    NJS_POINT3 pp{(p0->x+p1->x)*0.5f,(p0->y+p1->y)*0.5f,(p0->z+p1->z)*0.5f};
    float ra=(p1->x-p0->x)*(p1->x-p0->x);ra+=(p1->z-p0->z)*(p1->z-p0->z);
    float rb=(c.x-pp.x)*(c.x-pp.x);rb+=(c.z-pp.z)*(c.z-pp.z);
    if(rb>ra){std::swap(ra,rb);p1=&pp;p0=&c;}
    ra=njSqrt(ra);rb=njSqrt(rb);
    const NJS_VECTOR vct{p0->x-p1->x,p0->y-p1->y,p0->z-p1->z}; // njSubVector(&vct, p1P)
    op.ay=-int(10430.381f*std::atan2(vct.z,vct.x));
    op.sx=(op.sx+(ra+op.xn))*0.5f;
    op.sz=(op.sz+(rb+op.zn))*0.5f;
    const NJS_POINT3 pos=*p0;
    NJS_POINT3 dlt{(pos.x+p1->x)*0.5f,(pos.y+p1->y)*0.5f,(pos.z+p1->z)*0.5f};
    NJS_POINT3 cur{op.px,op.py,op.pz};
    const float dx=cur.x-dlt.x,dy=cur.y-dlt.y,dz=cur.z-dlt.z; // ps2_NaColi.c njDistanceP2P
    if(njSqrt(dx*dx+dy*dy+dz*dz)<rb){op.px=(op.px+dlt.x)*0.5f;op.py=(op.py+dlt.y)*0.5f;op.pz=(op.pz+dlt.z)*0.5f;}
    else{op.px=dlt.x;op.py=dlt.y;op.pz=dlt.z;}
    if(op.type==0)op.py=ground_.query(NJS_POINT3{op.px,op.py,op.pz});
    const auto& ew=linked->enemy;
    if(!(ew.flg&0x1u))op.flg=0;
    else if((ew.stflg&0x8u)||(ew.stflg&0x1000000u)||(ew.mdflg&0x1u))op.flg|=0x1000000u;
    else{op.flg&=~0x1000000u;work.queued=true;} // sys->ef_mdf[sys->ef_mdfn++] = oP
}
int RoomWorld::en11_light_control(RECVX_ENEMY_WORK* w,int mode){
    // event.c:9912 Sub_controll 148: bhEne11_LightControl(bhCetask->work, v0) ausserhalb des Gegnerpasses.
    EnemyWorldScope scope(this);bind_en11_services();const int result=recvx_en11_light_control(w,mode);recvx_en11_bind(nullptr);return result;
}
void RoomWorld::tick_shadow(unsigned slot){
    auto& work=sprites_.at(slot);
    auto& shadow=work.sprite;
    work.queued=false;
    // op->lkwkp is the BH_PWORK the creator passed to bhSetShadow; its native
    // owner is the entity recvx_enemy_set registered (room work or a child of
    // enemy_children_, which has no entities_ entry).
    if(shadow.lkwkp==&player_shadow_tag_){
        if(!player_asset_)return; // Welt ohne Spielerbesitzer (Werkzeug/Test): kein plp, nichts zu zeichnen
        // effsub1.c bhEff001 an plp: flg/stflg des Spielerworks (EventVm 13/14), stflg 0x1000000
        // aus bhInitModelSet (player_hidden), owP aus dem Spielerbaum (link_matrix 0).
        const unsigned flg=collision_events_?collision_events_->flags[13][0]:1u;
        const unsigned stflg=(collision_events_?collision_events_->flags[14][0]:collision_status_)|(player_hidden?0x1000000u:0u);
        const auto matrix=[&](int joint)->BoneMatrix{const auto m=joint<0?std::optional<BoneMatrix>{}:link_matrix(0,0,unsigned(joint));
            if(!m)throw std::runtime_error("bhEff001: plp->mlwP->owP nicht verfuegbar");return *m;};
        try{
            if((shadow.flg&0x80u)&&(flg&1u)){auto m=matrix(shadow.lkono);NJS_POINT3 offset{shadow.lox,shadow.loy,shadow.loz},position{};njCalcPoint(&m.value,&offset,&position);shadow.px=position.x;shadow.py=position.y;shadow.pz=position.z;}
            ShadowOwnerView view;view.flags=flg;view.status=stflg;view.model_flags=0;view.yaw=player_.yaw;
            if(shadow.mode0==1&&shadow.mdlver==0&&(stflg&0x2000u))view.root_matrix_y=matrix(0).value[13];
            work.queued=step_shadow(shadow,view,presentation_flags_,[&](int joint){const auto m=matrix(joint);return NJS_POINT3{m.value[12],m.value[13],m.value[14]};},[&](NJS_POINT3 position){return ground_.query(position);});
        }catch(const std::runtime_error& error){unbound_enemy_.insert(error.what());}
        return;
    }
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

void RoomWorld::tick_e03_shadow(unsigned slot){
    auto& work=sprites_.at(slot);auto& op=work.sprite;
    work.queued=false;
    switch(op.mode0){
    case 0:op.flg|=0x8000080u;op.flg&=~0x200000u;op.lkono=0;op.mode0=1;break;
    case 1:{
        const auto* owp=static_cast<const RECVX_ENEMY_WORK*>(op.lkwkp);
        if(!owp){unbound_enemy_.insert("bhEff_E03_Shadow: lkwkp fehlt");return;}
        if(!(owp->flg&0x1u)){op.flg=0;return;}
        if((owp->stflg&0x8u)||(owp->mdflg&0x1u)||(owp->stflg&0x1000000u)){op.flg|=0x1000000u;return;}
        op.flg&=~0x1000000u;
        work.queued=true; // sys->ef_mdf[sys->ef_mdfn++] = op
        break;}
    }
}

void RoomWorld::bind_enemy_services(){
    RECVX_ENEMY_SERVICES& s=enemy_services_;std::memset(&s,0,sizeof s);
    s.gap=[](const char* name){enemy_world->unbound_enemy_.insert(name);};
    s.player_status=collision_events_?&collision_events_->flags[14][0]:&collision_status_;
    s.random_next=[]()->int32_t{return recvx_core_random_next();};
    s.player_get=[](RECVX_ENEMY_PLAYER* out)->int{return out&&enemy_world->enemy_player_get(*out)?1:0;};
    s.player_set=[](const RECVX_ENEMY_PLAYER* in){if(!in||!enemy_world->enemy_player_set(*in))enemy_world->unbound_enemy_.insert("player_set: Inventory/EXP1/PlayerAsset nicht gebunden");};
    s.player_joint_matrix=[](uint32_t joint,float* out)->int{BoneMatrix m;if(!enemy_world->enemy_player_joint(joint,m))return 0;std::memcpy(out,m.value,sizeof m.value);return 1;};
    s.player_joint_point=[](uint32_t joint,const RECVX_ENEMY_VEC* local,RECVX_ENEMY_VEC* out)->int{
        BoneMatrix m;if(!enemy_world->enemy_player_joint(joint,m))return 0;
        NJS_POINT3 p{local->x,local->y,local->z},q;njCalcPoint(&m.value,&p,&q);*out={q.x,q.y,q.z};return 1;};
    s.player_motion_frames=[](uint32_t mtn,int enemy)->uint32_t{return enemy_world->enemy_player_motion_frames(mtn,enemy!=0);};
    s.player_damage=[](RECVX_ENEMY_WORK*,const char*,int amount){
        if(enemy_world&&enemy_world->player_hp_owner_&&enemy_world->enemy_attacks_)*enemy_world->player_hp_owner_-=amount; // Host-Schalter Gegnerangriffe
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
    // hitchk.c:12 bhCheckWall(pw): one pass over rom->walp + sys->mwalp
    // with the quantized position, current-side box response, ramps (type 6)
    // and the bhSetDansaLimitAtari walls for stflg 0x20000. The ported pass
    // is geometry_motion.cpp query(direct); the work fields are its view.
    // attr 0x40 writes plp (hitchk.c:258-290), as in bhCheckWallEx above.
    s.check_wall=[](RECVX_ENEMY_WORK* w)->int{
        auto* world=enemy_world;if(!w)return 0;
        MotionActor player=world->motion_actor();
        auto& e=owner_of(w);
        int floor_no=w->flr_no,push_ct=w->psh_ct,push_idx=w->psh_idx;
        unsigned floor_snd=unsigned(w->flr_snd),foot=unsigned(w->footeff);
        MotionActor actor{w->flg,w->stflg,w->flg2,player.control,player.system,player.mode,
            floor_no,push_ct,push_idx,floor_snd,foot,e.enemy_dansa_step,w->ay,false,player.effect29,&player.flags};
        auto walls=world->collision_geometry();auto steps=world->motion_steps();auto floors=world->motion_floors();
        MotionTerrain terrain{walls,steps,floors,world->floors_,world->room_height_};
        NJS_POINT3 p{w->px,w->py,w->pz};
        const bool hit=resolve_motion_wall_direct(terrain,actor,p,w->ar,w->ah);
        w->px=p.x;w->py=p.y;w->pz=p.z;w->flr_no=std::int8_t(floor_no);w->psh_ct=std::int16_t(push_ct);
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
    s.enemy_se=[](uint32_t enemy,const RECVX_ENEMY_VEC* p,int se){
        if(std::getenv("ZOMBIE_SE_DBG")){
            const auto* w=enemy<RECVX_ENEMY_SLOTS?enemy_world->enemy_table_.slot[enemy]:nullptr;
            std::fprintf(stderr,"ZOMBIE_SE enemy_se enemy=%u se=%d inactive=%d stflg=%u\n",enemy,se,(w&&(w->stflg&0x1000000u))?1:0,w?w->stflg:0u);
        }
        enemy_world->audio_.request_enemy_se(enemy,{p->x,p->y,p->z},se,1,0);};
    s.enemy_se_ex=[](uint32_t enemy,const RECVX_ENEMY_VEC* p,int se,int fade){enemy_world->audio_.request_enemy_se(enemy,{p->x,p->y,p->z},se,1,fade);};
    s.enemy_se_playing=[](uint32_t enemy,int se)->int{return enemy_world->audio_.enemy_se_playing(enemy,se)?1:0;};
    s.player_voice=[](int se){enemy_world->audio_.play_player_voice(se);};
    // zonzon1.c:391 -> bhSetBloodPoolLnk, palette = BloodType[id].color.
    s.blood_pool=[](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_VEC* p,int ay,const RECVX_ENEMY_BP_WORK* param){
        if(w->id>=31){enemy_world->unbound_enemy_.insert("bhEne_BloodPool: BloodType[id] ausserhalb");return;}
        enemy_world->particles_.spawn_pool(param,{p->x,p->y,p->z},ay,recvx_t3_blood_type[w->id].color,w);};
    // CallSystemSe(0, SeNo) -> CallSystemSeBasic: bit 31 is the ExPlayMidi path.
    s.system_se=[](int se){if(std::uint32_t(se)&0x80000000u)enemy_world->audio_.play_system_midi(std::uint32_t(se));else enemy_world->unbound_enemy_.insert("CallSystemSeBasic ohne Bit 31");};
    // mlwP->obj_num; waehrend objP/owP += 8 um acht kleiner (en01.c:2801). Nach
    // bhInitMask zaehlt es die eingefuegten Maskenobjekte mit (face_bh.c:541-561).
    s.obj_num=[](const RECVX_ENEMY_WORK* w)->uint32_t{const auto& t=tree_of(owner_of(w));if(!t.enemy_asset)return 0u;const auto n=t.enemy_asset->script_count();return n>t.model_window?std::uint32_t(n-t.model_window):0u;};
    s.motion_frames=[](const RECVX_ENEMY_WORK* w,uint32_t mtn)->uint32_t{const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);return clip?clip->frames:0u;};
    s.motion_obj_num=[](const RECVX_ENEMY_WORK* w,uint32_t mtn)->uint32_t{const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);return clip?std::uint32_t(clip->positions.size()):0u;};
    // Motion.c:21 bhSetMotion auf mlwP->objP/owP ab dem aktuellen Fenster. Ein
    // Kind mit geteiltem mlwP schreibt in den Baum des Elternworks.
    s.set_motion=[](RECVX_ENEMY_WORK* w,int add,uint32_t md)->int{
        auto* world=enemy_world;auto& e=owner_of(w);auto& t=tree_of(e);
        if(!t.enemy_asset){
            // Motion.c:29: ohne mnwP[mtn_no].datP kehrt bhSetMotion sofort mit 0 zurueck; mit
            // Motion, aber genulltem ML_WORK (obj_num 0) schriebe SetMtn* nach objP NULL.
            if(!t.model_absent){world->unbound_enemy_.insert("bhSetMotion: mlwP fehlt (kein Modell)");return 0;}
            const auto* clip=w->mtn_no<512?world->enemy_mn_work(e,w->mtn_no):nullptr;
            if(!clip)return 0;
            // Die Bild-/Hokan-Uhr laeuft wie im Original weiter; die Kanalschreibzugriffe
            // gingen dort nach objP NULL und werden gemeldet, nicht nachgebildet.
            MotionWork m{w->frm_no,w->frm_mode,w->hokan_count,w->mtn_attr,w->hokan_rate};std::string gap;
            std::vector<ModelPose> none;const std::vector<std::uint32_t> no_owp;
            const int r=set_motion(m,add,md,clip,static_cast<const std::int8_t*>(w->mtn_tp),none,no_owp,&gap,w->id,w->mtn_no);
            w->frm_no=m.frm_no;w->frm_mode=m.frm_mode;w->hokan_count=m.hokan_count;w->mtn_attr=m.mtn_attr;
            world->unbound_enemy_.insert("bhSetMotion: Motion ohne Modell (SetMtn schreibt nach objP NULL)");
            return r;}
        if(w->mtn_no>=512){world->unbound_enemy_.insert("bhSetMotion: mtn_no ausserhalb mnwP[512]");return 0;}
        if(t.model_window>t.enemy_obj.size()){world->unbound_enemy_.insert("bhSetMotion: objP-Fenster ausserhalb des Modells");return 0;}
        // Motion.c:131: objP wird bei frm_no (vor dem Vorschub) abgetastet.
        e.sampled_frame=w->frm_no;
        MotionWork m{w->frm_no,w->frm_mode,w->hokan_count,w->mtn_attr,w->hokan_rate};std::string gap;
        const auto first=std::ptrdiff_t(t.model_window);
        std::vector<ModelPose> obj(t.enemy_obj.begin()+first,t.enemy_obj.end());
        const std::vector<std::uint32_t> owp(t.enemy_owp_flg.begin()+first,t.enemy_owp_flg.end());
        const int r=set_motion(m,add,md,world->enemy_mn_work(e,w->mtn_no),static_cast<const std::int8_t*>(w->mtn_tp),obj,owp,&gap,w->id,w->mtn_no);
        std::copy(obj.begin(),obj.end(),t.enemy_obj.begin()+first);
        w->frm_no=m.frm_no;w->frm_mode=m.frm_mode;w->hokan_count=m.hokan_count;w->mtn_attr=m.mtn_attr;
        // bhGetFrameNum clamps to the raw frm_num and bhEne01_ChgWalkMtn shifts
        // that by 16 (en01.c:10297): frm_no == frm_num on entry, one key past
        // the motion in the original. That pass keeps the previous pose here
        // and is reported; the advance below wraps it exactly as Motion.c:106.
        if(!gap.empty()){world->unbound_enemy_.insert(gap);if(std::getenv("RECVX_MTN_DBG")){const auto* c=world->enemy_mn_work(e,w->mtn_no);std::fprintf(stderr,"MTNDBG id=%u mtn=%u frm=%d md=%u obj=%zu owp=%zu window=%u chans=%zu frames=%u gap=%s\n",w->id,w->mtn_no,w->frm_no,md,obj.size(),owp.size(),t.model_window,c?c->positions.size():0u,c?c->frames:0u,gap.c_str());}}
        return r;};
    s.calc_model=[](RECVX_ENEMY_WORK* w){enemy_world->calc_enemy_model(owner_of(w));};
    // room.c:438-444 rechnet den Baum schon beim Raumaufbau (bhKeepObjWork +
    // bhCalcModel); Kinder ohne eigenen Raumaufbau lesen ihren genullten Satz.
    // mlwP->owP[joint].mtx: logischer Knoten (owp_node).
    s.joint_matrix=[](const RECVX_ENEMY_WORK* w,uint32_t joint,float* out)->int{const auto& t=tree_of(owner_of(w));const auto node=owp_node(t,joint);if(!node||*node>=t.enemy_owp_mtx.size())return 0;std::memcpy(out,t.enemy_owp_mtx[*node].value,16*sizeof(float));return 1;};
    s.joint_point=[](const RECVX_ENEMY_WORK* w,uint32_t joint,const RECVX_ENEMY_VEC* local,RECVX_ENEMY_VEC* out)->int{
        const auto& t=tree_of(owner_of(w));const auto node=owp_node(t,joint);if(!node||*node>=t.enemy_owp_mtx.size())return 0;
        NJS_MATRIX m;std::memcpy(m,t.enemy_owp_mtx[*node].value,sizeof m);NJS_POINT3 a{local->x,local->y,local->z},b;njCalcPoint(&m,&a,&b);*out={b.x,b.y,b.z};return 1;};
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
    s.set_shadow=[](const int8_t* jtb,RECVX_ENEMY_WORK* link,int lkono,float sx,float sy,float sz)->int{return enemy_world->set_enemy_shadow(jtb,link,lkono,sx,sy,sz);};
    // en07.c:216 eff[sdw].mdlver = 1 nach bhSetShadow. Ein Slot ausserhalb des
    // Pools kann nur aus einem fehlerhaften Rueckgabewert kommen; das Original
    // schreibt dann mit eff[-1] vor den Pool, nativ wird gemeldet.
    s.effect_set_mdlver=[](int slot,uint32_t mdlver){auto* world=enemy_world;
        if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot].mdlver ausserhalb des Pools");return;}
        world->sprites_[std::size_t(slot)].sprite.mdlver=mdlver;};
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
    // `epw->mlwP = &ep->mdl[k]`: Reihe und Basis des Besitzers, Baum neu aus mdl[k].
    s.link_model=[](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_WORK* owner,uint32_t k)->int{
        auto& e=owner_of(w);const auto& o=tree_of(owner_of(owner));
        const PlayerAsset* bank=o.model_bank?o.model_bank:o.model_row();const unsigned base=(o.model_bank?o.model_base:0u)+k;
        if(!bank)return 0;
        if(&e==&o&&!o.model_bank&&k==0)return 1;                           // mlwP = mdl (eigene Reihe)
        const PlayerAsset* asset=base==0?bank:bank->model(base);
        e.model_owner=nullptr;e.model_bank=bank;e.model_base=base;
        if(!asset){
            if(base>=bank->model_count()){e.model_absent=true;e.enemy_asset=nullptr;e.enemy_obj.clear();e.enemy_evalflags.clear();e.enemy_owp_flg.clear();e.enemy_owp_mtx.clear();e.enemy_calculated=false;return 1;}
            return 0;}
        e.model_absent=false;
        if(e.enemy_asset!=asset){e.enemy_asset=asset;e.enemy_obj=asset->bind_pose();e.enemy_evalflags=asset->node_evalflags();
            e.enemy_owp_flg.assign(e.enemy_obj.size(),0u);e.enemy_owp_mtx.assign(e.enemy_obj.size(),BoneMatrix{});e.enemy_calculated=false;e.morph_obj_a=e.morph_obj_b=0;e.live_model.clear();e.live_models.clear();}
        return 1;};
    // `epw->mlwP = &epw->mdl[k]` der eigenen Reihe (en10.c:51, en71.c:41, en30.c:116/360/402/437/448).
    // Jedes mdl[k] ist ein eigenes ML_WORK: Pose, owP und evalflags des bisherigen bleiben liegen und
    // sind beim Zurueckwechseln wieder da; ein noch nie gewaehltes mdl[k] beginnt mit seinem objP.
    s.select_model=[](RECVX_ENEMY_WORK* w,uint32_t k)->int{
        auto& e=owner_of(w);
        if(e.model_owner||e.model_bank){enemy_world->unbound_enemy_.insert("epw->mlwP = &epw->mdl[k]: Work ohne eigene Modellreihe");return 0;}
        const PlayerAsset* row=e.model_row();if(!row)return 0;
        if(k==(e.own_row?e.own_model:0u))return 1;
        // Ein Kind mit `epp->mlwP = epw->mdl` (model_owner) liest im Original weiter mdl[0], nativ
        // aber den aktiven Baum dieses Works: diese Kombination ist nicht portiert, also melden.
        auto* world=enemy_world;
        for(const auto& c:world->enemy_children_)if(c.model_owner==&e){world->unbound_enemy_.insert("epw->mlwP = &epw->mdl[k] mit Kind auf epw->mdl (geteilter Baum)");return 0;}
        for(const auto& [key,c]:world->entities_)if(c.model_owner==&e){world->unbound_enemy_.insert("epw->mlwP = &epw->mdl[k] mit Kind auf epw->mdl (geteilter Baum)");return 0;}
        const PlayerAsset* next=k==0?row:row->model(k);if(!next)return 0;
        e.own_row=row;
        const unsigned out=e.own_model;auto& keep=e.own_models[out];
        keep.obj=std::move(e.enemy_obj);keep.owp_flg=std::move(e.enemy_owp_flg);keep.owp_mtx=std::move(e.enemy_owp_mtx);keep.calculated=e.enemy_calculated;
        if(out==0)keep.evalflags=std::move(e.enemy_evalflags);
        else{if(e.morph_model_flags.size()<out)e.morph_model_flags.resize(out);e.morph_model_flags[out-1]=std::move(e.enemy_evalflags);}
        const auto at=e.own_models.find(k);
        if(at!=e.own_models.end()){
            e.enemy_obj=std::move(at->second.obj);e.enemy_owp_flg=std::move(at->second.owp_flg);e.enemy_owp_mtx=std::move(at->second.owp_mtx);e.enemy_calculated=at->second.calculated;
            if(k==0)e.enemy_evalflags=std::move(at->second.evalflags);
            e.own_models.erase(at);
        }else{e.enemy_obj=next->bind_pose();e.enemy_owp_flg.assign(e.enemy_obj.size(),0u);e.enemy_owp_mtx.assign(e.enemy_obj.size(),BoneMatrix{});e.enemy_calculated=false;}
        if(k){if(e.morph_model_flags.size()>=k&&!e.morph_model_flags[k-1].empty()){e.enemy_evalflags=std::move(e.morph_model_flags[k-1]);e.morph_model_flags[k-1].clear();}
            else e.enemy_evalflags=next->node_evalflags();}
        else if(e.enemy_evalflags.empty())e.enemy_evalflags=next->node_evalflags();
        e.enemy_asset=next;e.own_model=k;return 1;};
    s.mobile_wall_alloc=[]()->int{auto* world=enemy_world;world->enemy_mobile_walls_.push_back(CollisionWall{});world->update_ground_geometry();return int(world->enemy_mobile_walls_.size()-1);};
    s.mobile_wall_get=[](int index,RECVX_ENEMY_ATR* out)->int{const auto& walls=enemy_world->enemy_mobile_walls_;if(index<0||std::size_t(index)>=walls.size())return 0;
        const auto& a=walls[std::size_t(index)];*out=RECVX_ENEMY_ATR{};out->flg=a.flags;out->type=a.type;out->attr=a.attributes;out->flr_no=a.floor;out->px=a.position.x;out->py=a.position.y;out->pz=a.position.z;out->w=a.extent.x;out->h=a.extent.y;out->d=a.extent.z;
        out->prm0=std::uint8_t(a.parameters[0]);out->prm1=std::uint8_t(a.parameters[1]);out->prm2=std::uint8_t(a.parameters[2]);out->prm3=std::uint8_t(a.parameters[3]);out->id=std::uint8_t(a.id);return 1;};
    s.mobile_wall_set=[](int index,const RECVX_ENEMY_ATR* in){auto* world=enemy_world;auto& walls=world->enemy_mobile_walls_;if(index<0||std::size_t(index)>=walls.size()){world->unbound_enemy_.insert("sys->mwalp: Index ohne Zuteilung");return;}
        walls[std::size_t(index)]={in->flg,in->type,in->id,in->flr_no,in->attr,{in->px,in->py,in->pz},{in->w,in->h,in->d},{in->prm0,in->prm1,in->prm2,in->prm3}};world->update_ground_geometry();};
    // subpl.c:143 bhEne_Event belongs to the existing subplayer apparatus.
    // effect.c:331 bhSetEffect(id, &POINT, lkp, param): eigener Poolplatz mit
    // flg 1, type/mdlver/flr_no 0, Skala 1 und Nullpunkt; ox/oy/oz kommen aus
    // dem POINT des Aufrufers (eneset.c:461 uebergibt dort drei Nullen,
    // en01.c:10814 stattdessen mtx[12..]). Aufrufer: eneset.c:461 (id 17),
    // en01.c:10814 (id 108).
    s.set_effect=[](int id,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* off,RECVX_ENEMY_WORK* link,int param)->int{
        auto* world=enemy_world;EffectSpawn e;
        e.flg=1;e.id=unsigned(id);e.type=0;e.mdlver=0;e.lkono=param;
        if(pos)e.position={pos->x,pos->y,pos->z};
        if(off){e.has_offset=true;e.offset={off->x,off->y,off->z};}
        unsigned kind=0,index=0;
        if(link){if(!world->enemy_work_index(link,index)){world->unbound_enemy_.insert("bhSetEffect: Linkwork ohne Raumfigur (Kindwork)");return -1;}kind=2;e.linked_work=link;}
        return world->spawn_effect(e,kind,index);};
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
        const PlayerAsset* asset=p.model_row()?(slot==0?p.model_row():p.model_row()->model(slot)):nullptr;
        c.model_owner=nullptr;c.enemy_asset=asset;
        // en01.c:2617-2658 epp->mdl[0] = epw->mdl[slot]: hinter mdl_n ist das ML_WORK genullt.
        c.model_absent=!asset&&(p.model_absent||(p.model_row()&&slot>=p.model_row()->model_count()));
        if(c.model_absent){c.enemy_obj.clear();c.enemy_evalflags.clear();c.enemy_owp_flg.clear();c.enemy_owp_mtx.clear();c.enemy_calculated=false;return;}
        if(!asset){c.enemy_obj.clear();c.enemy_evalflags.clear();c.enemy_owp_flg.clear();c.enemy_owp_mtx.clear();enemy_world->unbound_enemy_.insert("bhEne01_Init: epw->mdl[slot] nicht als Modell geladen");return;}
        // epp->mlwP = epp->mdl, mdl[0] = epw->mdl[slot] (struct-Kopie samt texP): die Modellreihe des
        // Elternworks ab slot. bhDrawEnemy (eneset.c:807-857) zeichnet das Kind, sobald mdflg 0x1 frei
        // ist (Kappe :2614, Wurm/Bom/Scope en01sub.c:1360/1857/1924). mdl[1..] ist im Original 0;
        // kein EN01-Kind liest es (kein mdflg 0x2/obj_b), model_bank reicht also nur mdl[0] weiter.
        c.model_bank=p.model_row();c.model_base=slot; // epw->mdl[slot]: die Reihe, nicht mlwP
        c.enemy_obj=asset->bind_pose();c.enemy_evalflags=asset->node_evalflags();c.enemy_owp_flg.assign(c.enemy_obj.size(),0u);c.enemy_owp_mtx.assign(c.enemy_obj.size(),BoneMatrix{});c.enemy_calculated=false;};
    // objP += 8, owP += 8, obj_num -= 8 am geteilten ML_WORK (en01.c:2799-2805).
    en.shift_object_window=[](RECVX_ENEMY_WORK* w,int shift){
        auto& t=tree_of(owner_of(w));
        if(shift)t.model_window+=8u;
        else if(t.model_window>=8u)t.model_window-=8u;
        else enemy_world->unbound_enemy_.insert("objP -= 8 ohne vorheriges += 8");};
    // hitchk.c:2178 bhCheckWallRefAngle: Suche ueber rom->walp + sys->mwalp mit
    // dem zusaetzlichen attr-0x20-Tor (effect_wall_ref_angle) und Antwort wie
    // bhSetWallRefAngle (reflect_effect_wall). Aufrufer en01sub.c:937 (flg 0)
    // und :1118 (flg 0x400); `rec` bleibt bei NULL unberuehrt.
    en.wall_ref_angle=[](RECVX_ENEMY_VEC* pos,uint32_t flg,float ar,float ah,
                         int32_t* ay,RECVX_ENEMY_ATR* rec)->int{
        auto* world=enemy_world;if(!world||!pos||!ay)return 0;
        const auto walls=world->collision_geometry();
        const auto wall=effect_wall_ref_angle(walls,world->room_height_,{pos->x,pos->y,pos->z},flg,ar,ah);
        if(!wall)return 0;
        NJS_POINT3 p{pos->x,pos->y,pos->z};int angle=*ay;
        reflect_effect_wall(*wall,world->room_height_,p,ar,ah,angle);
        pos->x=p.x;pos->y=p.y;pos->z=p.z;*ay=angle;
        if(rec){const auto a=collision_record(*wall);
            rec->flg=a.flg;rec->type=a.type;rec->attr=a.attr;rec->flr_no=(int32_t)a.flr_no;
            rec->px=a.px;rec->py=a.py;rec->pz=a.pz;rec->w=a.w;rec->h=a.h;rec->d=a.d;
            rec->prm0=a.prm0;rec->prm1=a.prm1;rec->prm2=a.prm2;rec->prm3=a.prm3;rec->id=a.id;}
        return 1;};
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
    // PEXP0_F(72/80) = EXP_WORK spx/spz (en01.c:7920, en01sub.c:1529): derselbe
    // Weg wie en19/Hund, der Aktionsbesitzer uebernimmt die Werte nach dem Gegnerpass.
    en.player_exp0_float_set=[](uint32_t offset,float v){enemy_world->player_exp0_writes_.push_back({offset,v});};
    // bhSetMotion(plp, add, ...) aus dem Gegnerpass (en01.c:7943), wie en15.player_set_motion.
    en.player_set_motion=[](RECVX_ENEMY_PLAYER* p,int add)->int{auto* world=enemy_world;
        if(!world->enemy_player_set(*p)){world->unbound_enemy_.insert("bhSetMotion(plp): Spielerbesitzer fehlt");return -2;}
        const MotionClip* clip=nullptr;const int r=world->player_set_motion_now(add,clip);
        world->enemy_player_get(*p);return r;};
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
    // hitchk.c:5532 bhCheckWater(pos): erste Wasserflaeche (type 3, prm0 1) unter pos. Der
    // Aufrufer en01.c:4069 prueft zuvor sys->st_flg 0x40, das bhObj005 im Wasserraum setzt.
    en.check_water=[](const RECVX_ENEMY_VEC* p)->int{float top;return enemy_world->water_top({p->x,p->y,p->z},top)?1:0;};
    // effect.c:735 bhLinkBlood(BH_PWORK*, int, NJS_POINT3*, float): der Originalkoerper ist leer.
    en.link_blood=[](RECVX_ENEMY_WORK*,int,const RECVX_ENEMY_VEC*,float){};
    // hitchk.c:5561 bhCheckL2Water(&line, &gpos): NJS_LINE aus Lage und Vektor; ohne Treffer
    // bleibt out ungeschrieben (die Aufrufer halten den Originaldefekt selbst fest).
    en.water_line=[](const RECVX_ENEMY_VEC* from,const RECVX_ENEMY_VEC* vec,RECVX_ENEMY_VEC* out)->int{NJS_POINT3 hit{};
        if(!enemy_world->l2_water({from->x,from->y,from->z},{vec->x,vec->y,vec->z},hit))return 0;*out={hit.x,hit.y,hit.z};return 1;};
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
        if(p->fields&RECVX_T3_EFF_SHP_CT)s.shp_ct=p->shp_ct;
        if(p->fields&RECVX_T3_EFF_ZN)s.zn=p->zn_val;
        if(p->fields&RECVX_T3_EFF_MODE1){if(s.id==300||s.id==302)world->particles_.set_mode1(unsigned(slot),p->mode1);else s.mode1=p->mode1;}
        if(p->fields&RECVX_T3_EFF_COLOR)for(auto& v:s.tv)v.col=p->color;
        if(p->fields&RECVX_T3_EFF_AY)s.ay=p->ay;
        // effect.c:1329: stflg 0x20 selects txp[0], NOT sys->ef_tn[tex_id].
        if(p->fields&RECVX_T3_EFF_TEXP)world->sprites_[std::size_t(slot)].texture_list=static_cast<const std::vector<SceneTexture>*>(p->texp);};
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
    x.water_line=en.water_line; // zonzon.c:925 bhCheckL2Water, dieselbe Linie (Lage, Vektor)
    x.player_id=[]()->int{return 0;}; // player.c:700 plp->id = 0
    x.player_obj_num=[]()->uint32_t{return uint32_t(enemy_world->player_joint_matrices_.size());};
    // en01.c:3278-3308: damage interrupts an enemy-owned player grab.
    en.player_hurt_state=[](RECVX_ENEMY_WORK*,int knocked_down){
        auto* world=enemy_world;RECVX_ENEMY_PLAYER pl{};
        if(!world->enemy_player_get(pl))return;
        pl.exp1_flags|=4u;
        world->clear_pad_on(0xfu);
        pl.flg=(pl.flg&~0x10000u)|8u;
        pl.stflg&=~0x50000u;pl.at_flg=0;
        pl.mtn_enemy=0;pl.mtn_enemy_work=nullptr;
        if(knocked_down){
            pl.mode0=2;pl.mode2=1;pl.mode3=0;
            const RECVX_ENEMY_VEC dv{world->player_hit_dv_.x,world->player_hit_dv_.y,world->player_hit_dv_.z};
            pl.mode1=recvx_en01_dgc_dir_check(&dv,pl.ay)==0?0:1;
        }else{pl.mode0=2;pl.mode1=2;pl.mode2=0;pl.mode3=0;}
        world->enemy_player_set(pl);
    };
    x.player_hit_set=[](int djnt_no,const RECVX_ENEMY_VEC* dv,const RECVX_ENEMY_VEC* dp){auto* world=enemy_world;world->player_hit_joint_=djnt_no;if(dv)world->player_hit_dv_={dv->x,dv->y,dv->z};if(dp)world->player_hit_dp_={dp->x,dp->y,dp->z};};
    x.player_hit_get=[](int* djnt_no,RECVX_ENEMY_VEC* dv,RECVX_ENEMY_VEC* dp)->int{auto* world=enemy_world;if(djnt_no)*djnt_no=world->player_hit_joint_;if(dv)*dv={world->player_hit_dv_.x,world->player_hit_dv_.y,world->player_hit_dv_.z};if(dp)*dp={world->player_hit_dp_.x,world->player_hit_dp_.y,world->player_hit_dp_.z};return 1;};
    x.model_texp=[](const RECVX_ENEMY_WORK* w)->const void*{
        const auto& t=tree_of(owner_of(w));
        if(!t.enemy_asset){enemy_world->unbound_enemy_.insert("mlwP->texP: Modell fehlt");return nullptr;}
        for(const auto& m:enemy_world->models_)if(m.actor.get()==t.enemy_asset)return &m.object.textures;
        enemy_world->unbound_enemy_.insert("mlwP->texP: Texturliste fehlt");return nullptr;};
    // mdl[k].texP (zonzon1.c:84 bhEne_SetMinceEffect: BloodType[id].mdlno): eigenes texP oder das
    // des letzten texturierten mdl davor (room.c bhSetEneMdl), wie en23 model_texture.
    x.model_texp_index=[](const RECVX_ENEMY_WORK* w,int k)->const void*{
        const auto& e=tree_of(owner_of(w));const PlayerAsset* bank=e.model_bank?e.model_bank:e.model_row();
        if(k<0){enemy_world->unbound_enemy_.insert("mdl[k].texP: Index negativ");return nullptr;}
        const unsigned base=(e.model_bank?e.model_base:0u)+unsigned(k);
        if(!bank){enemy_world->unbound_enemy_.insert("mdl[k].texP: Modell fehlt");return nullptr;}
        for(unsigned i=base;i>0;--i)if(const auto& textures=bank->model_textures(i);!textures.empty())return &textures;
        for(const auto& m:enemy_world->models_)if(m.actor.get()==bank)return &m.object.textures;
        enemy_world->unbound_enemy_.insert("mdl[k].texP: Texturliste fehlt");return nullptr;};
    x.player_joint_matrix=s.player_joint_matrix;
    x.player_voice_free=s.player_voice;
    // zonzon1.c:844/858 bhEne_CallEffectSE / bhEne_CheckPlayEffectSE: die feste
    // Effektstimme 127 ohne Work (RequestEnemySe(127, ..)); EnemyInfo[128] traegt sie.
    x.effect_se=[](uint32_t voice,const RECVX_ENEMY_VEC* p,int se){enemy_world->audio_.request_enemy_se(voice,{p->x,p->y,p->z},se);};
    x.effect_se_playing=[](uint32_t voice,int se)->int{return enemy_world->audio_.enemy_se_playing(voice,se)?1:0;};
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
    // effsub3.c:145 bhSetBloodPoolLnk aus den EN01-Sterbezustaenden (en01.c:9990/9994/10122/10149,
    // en01b.c:763), pal_bnk vom Aufrufer (dort 0); lkwkp bleibt die Referenz auf diesen Work.
    en.blood_pool_lnk=[](RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_VEC* pos,int32_t ay,const RECVX_ENEMY_BP_WORK* tab,int pal_bnk)->int{return enemy_world->particles_.spawn_pool(tab,{pos->x,pos->y,pos->z},ay,pal_bnk,epw);};
    en.nikuhen_effect=[](RECVX_ENEMY_WORK* w,int type,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* v,int tex_id){recvx_t3_set_nikuhen_effect(w,type,pos,v,tex_id);};
    en.nikuhen_effect2=[](RECVX_ENEMY_WORK* w,int type,const RECVX_ENEMY_VEC* pos,int n,int tex_id){recvx_t3_set_nikuhen_effect2(w,type,pos,n,tex_id);};
    en.san_effect=[](RECVX_ENEMY_WORK* w,int jno,const RECVX_EN01_BT* bt){recvx_t3_set_san_effect(w,jno,reinterpret_cast<const RECVX_T3_BT*>(bt));};
    en.water_splash=[](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_VEC* pos,int n,float sx,float sy,float sz){enemy_world->water_splash2(w->py,w->ay,{pos->x,pos->y,pos->z},n,sx,sy,sz);};
    en.set_effect_tb=[](const RECVX_EN01_EF* tmpl,const RECVX_ENEMY_VEC* off,RECVX_ENEMY_WORK* link,int lkono)->int{
        auto* world=enemy_world;const int ay=(tmpl->keep&RECVX_EN01_EF_KEEP_AY)?world->effect_template_ay_:tmpl->ay;world->effect_template_ay_=ay;
        return world->effect_tb(tmpl->flg,tmpl->id,tmpl->type,unsigned(tmpl->mdlver),{tmpl->px,tmpl->py,tmpl->pz},{tmpl->sx,tmpl->sy,tmpl->sz},0,ay,off,link,false,lkono);};
    en.effect_slot_set=[](int slot,uint32_t mask,const RECVX_EN01_EF_SLOT* in){
        auto* world=enemy_world;if(slot<0||std::size_t(slot)>=world->sprites_.size()||!in)return;
        auto& s=world->sprites_[std::size_t(slot)].sprite;
        if(mask&RECVX_EN01_EFS_LINK)s.lkwkp=in->lkwkp;
        if(mask&RECVX_EN01_EFS_VEC){s.xn=in->xn;s.yn=in->yn;s.zn=in->zn;}
        if(mask&RECVX_EN01_EFS_STFLG)s.stflg|=in->stflg_or;
        if(mask&RECVX_EN01_EFS_TEX){s.tex_id=in->tex_id;
            world->sprites_[std::size_t(slot)].texture_list=in->tex_from?static_cast<const std::vector<SceneTexture>*>(world->enemy_ext_.model_texp(in->tex_from)):nullptr;}
        if(mask&RECVX_EN01_EFS_SPD)s.spd=in->spd;
        if(mask&RECVX_EN01_EFS_ANG){s.ax=in->ax;s.ay=in->ay;}
        if(mask&RECVX_EN01_EFS_CT){s.ct0=in->ct0;s.ct1=in->ct1;s.ct3=in->ct3;}
        if(mask&RECVX_EN01_EFS_SHP)s.shp_ct=in->shp_ct;
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
    dog.blood_pool_lnk=[](RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_VEC* pos,int32_t ay,const RECVX_ENEMY_BP_WORK* tab,int pal_bnk)->int{return enemy_world->particles_.spawn_pool(tab,{pos->x,pos->y,pos->z},ay,pal_bnk,epw);};
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
    // sys->ufo_pos/ufo_md/ufo_flg/ufo_oidx (bhEne56 <-> bhEff134).
    s.ufo_get=[](RECVX_ENEMY_VEC* pos,int* md,int* flg,uint32_t* oidx)->int{const auto& u=enemy_world->ufo_;
        if(pos)*pos={u.pos.x,u.pos.y,u.pos.z};if(md)*md=u.md;if(flg)*flg=u.flg;if(oidx)*oidx=u.oidx;return 1;};
    s.ufo_set_pos=[](const RECVX_ENEMY_VEC* pos){enemy_world->ufo_.pos={pos->x,pos->y,pos->z};};
    s.ufo_set_md=[](int md){enemy_world->ufo_.md=md;};
    s.ufo_set_oidx=[](uint32_t oidx){enemy_world->ufo_.oidx=oidx;};
    // flag.c bhCkFlg(sys->ev_flg, no): Flagmatrix 1 des Eventbesitzers.
    s.event_flag=[](uint32_t no)->int{auto* world=enemy_world;if(!world->collision_events_){world->unbound_enemy_.insert("bhCkFlg(sys->ev_flg) ohne EventVm");return 0;}return world->collision_events_->flag(1,no)?1:0;};
    // sys->obwp[no] als O_WRK-Sicht (eneset.c:592-602, :699-780): flg 0x80 = am lkwkp
    // gehaengt (WorldEntity::linked unter dem Raumgegner), lox/loy/loz = link_offset.
    s.object_get=[](uint32_t index,RECVX_ENEMY_OBJECT* out)->int{auto* world=enemy_world;const auto* e=world->state(2,index);if(!e)return 0;
        *out=RECVX_ENEMY_OBJECT{};out->flg=(e->object_flags&~0x80u)|(e->linked?0x80u:0u);out->lkono=int32_t(e->parent_node);
        out->lox=e->link_offset.x;out->loy=e->link_offset.y;out->loz=e->link_offset.z;
        out->px=e->transform.position.x;out->py=e->transform.position.y;out->pz=e->transform.position.z;out->yn=e->object_yn;out->mode1=e->object_mode1;
        if(const auto m=world->link_matrix(2,index,0))out->root={m->value[12],m->value[13],m->value[14]};
        else world->unbound_enemy_.insert("sys->obwp[].mlwP->owP[0] ohne Objektbaum");
        return 1;};
    s.object_set=[](uint32_t index,const RECVX_ENEMY_OBJECT* in,RECVX_ENEMY_WORK* link){auto* world=enemy_world;if(!world->state(2,index)){world->unbound_enemy_.insert("sys->obwp[index] ausserhalb rom->obj_n");return;}
        auto& e=world->entity(2,index);e.object_flags=in->flg;e.object_yn=in->yn;e.object_mode1=in->mode1;e.transform.position={in->px,in->py,in->pz};
        if(in->flg&0x80u){e.linked=true;if(link){e.parent_kind=1;e.parent_index=link->owner_index;}e.parent_node=unsigned(in->lkono);e.link_offset={in->lox,in->loy,in->loz};}
        else e.linked=false;};
    // zonzon1.c:243/354/397 auf dem gemeinsamen T3-Helfer (Effekte 297/252/298).
    s.blood_effect_burst=[](RECVX_ENEMY_WORK* w,int type,int jno,const RECVX_ENEMY_VEC* pos,int flg){const RECVX_T3_LINK lk{w,0};recvx_t3_set_blood_effect_burst(lk,type,jno,pos,flg);};
    s.fire_effect=[](RECVX_ENEMY_WORK* w,int jno,const RECVX_ENEMY_VEC* ofp,float size,int len){recvx_t3_set_fire_effect(w,jno,ofp,size,len);};
    s.acid_effect=[](RECVX_ENEMY_WORK* w,int jno,const RECVX_ENEMY_VEC* ofp,float size){recvx_t3_set_acid_effect(w,jno,ofp,size);};
    // zonzon1.c:52 bhEne_SetMinceEffect / :175 bhEne_SetBloodEffect4: dieselben T3-Wege
    // wie die uebrigen zonzon-Effekte; bisher ungebunden (EN07/10/24/27/30 meldeten sie).
    s.mince_effect=[](RECVX_ENEMY_WORK* w,int type,int num){recvx_t3_set_mince_effect(w,type,num);};
    s.blood_effect4=[](const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* vec,int col,int type1,int type2){recvx_t3_set_blood_effect4(pos,vec,col,type1,type2);};
    // eneset.c:372 bhControlMask(epw): Maske/Lippen des eigenen Raumworks im
    // Gegnerpass. Ohne Maskendaten (pp->mskp NULL) tut das Original nichts.
    s.control_mask=[](RECVX_ENEMY_WORK* w){auto* world=enemy_world;auto& e=owner_of(w);
        for(const auto& m:world->models_)if(m.kind==1&&m.actor&&&world->entity(m.kind,m.index)==&e){if(m.actor->face())world->control_mask(e,*m.actor);return;}};
    // eneset.c:906 bhEne_CallocWork. Room lifetime; the cursor is aligned to
    // 64 before the block, the limit test uses the unaligned cursor.
    s.calloc_work=[](uint32_t size)->void*{auto* world=enemy_world;
        if(world->enemy_exp_used_+size>enemy_exp_budget)return nullptr;
        world->enemy_exp_used_=((world->enemy_exp_used_+63)&~std::size_t(63))+size;
        world->enemy_exp_blocks_.push_back(std::unique_ptr<unsigned char[]>(new unsigned char[size?size:1]()));
        return world->enemy_exp_blocks_.back().get();};
}

void RoomWorld::effect_player_fire(int jno,NJS_POINT3 ofp,float size,int life){
    EnemyWorldScope scope(this);EnemyExtScope ext(&enemy_ext_);const RECVX_T3_LINK lk{nullptr,1};const RECVX_ENEMY_VEC o{ofp.x,ofp.y,ofp.z};
    recvx_t3_set_fire_effect_link(lk,jno,&o,size,life);
}
void RoomWorld::effect_en12_binta(RECVX_ENEMY_WORK* w,int action){
    EnemyWorldScope scope(this);EnemyExtScope ext(&enemy_ext_);
    if(!w||w->id!=12){unbound_enemy_.insert("bhEff_E12_BintaEffControl: lkwkp ist kein EN12-Work");return;}
    recvx_en12_set_fire_binta_effect(w,action);
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
// Motion.c bhSetMotion(plp, add, plp->mtn_md, plp->mtn_tp) auf der Bibliothek, auf die
// plp->mnwP zeigt (die des greifenden Gegners oder mnwPb). Derselbe Lauf fuer den Pass
// in bhControlPlayer (player.c:1502) und fuer Aufrufe aus dem Gegnerpass (en15.c:1977
// SetPlyMtn mit add 0). Rueckgabe wie bhSetMotion (-1/0/1); -2 = Clip fehlt (gemeldet).
int RoomWorld::player_set_motion_now(int add,const MotionClip*& clip){
    clip=nullptr;
    if(player_enemy_motion_)clip=player_enemy_clip();
    else if(player_asset_&&player_asset_->has_motion(motion_))clip=&player_asset_->motion_clip(motion_);
    if(!clip){unbound_enemy_.insert(player_enemy_motion_?"bhSetMotion(plp): mnwP[mtn_no] des Gegners fehlt":"bhSetMotion(plp): mnwPb[mtn_no] fehlt");return -2;}
    // Knoten- und Uhrteil auf dem einen plp->mlwP->objP (player_obj_), wie player_set_motion_pose.
    if(player_obj_.size()!=player_asset_->bones()){player_obj_=player_asset_->bind_pose();player_owp_flags_.assign(player_obj_.size(),0u);}
    MotionWork m{std::int32_t(frame_),0u,player_hokan_count_,0u,player_hokan_rate_};
    std::string gap;
    const int r=set_motion(m,add,player_mtn_md_,clip,nullptr,player_obj_,player_owp_flags_,&gap,0u,unsigned(motion_));
    if(!gap.empty())unbound_enemy_.insert(gap+" (plp)");
    player_obj_motion_=motion_;player_obj_written_=true;player_obj_drives_=true;
    frame_=m.frm_no;player_hokan_count_=m.hokan_count;
    return r;
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
    if(player_set_motion_now(int(player_step_),clip)==-2)return;
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
    if(e.player_motion_library){const auto* c=player_asset_?player_asset_->library_motion(mtn):nullptr;return c&&c->frames?c:nullptr;}
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
    // EN11/EN16 binden sich NICHT hier, sondern in jedem Gegnerpass selbst
    // (control_enemy_works), weil der Pass seine Dienste am Ende wieder loest.
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
            // room.c:438-444: bhKeepObjWork nullt owP, gleich danach bhCalcModel(pwp)
            // auf Bindepose und Weltlage des eben gesetzten Works (vor bhInitMask
            // und vor dem ersten Gegnerpass). Der erste Pass liest also einen
            // berechneten Baum, keinen Nullsatz.
            calc_enemy_model(e);
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

void RoomWorld::check_moth_egg(){
    if(!collision_events_)return;
    EnemyWorldScope scope(this); // bhSetEnemy -> bhSetFloorNum ueber die Gegnerdienste
    const std::uint32_t stflg=collision_events_->flags[14][0];
    static const std::uint32_t marks[3]={0x8000000u,0x10000000u,0x20000000u};
    for(unsigned type=0;type<3;++type){
        if(!(stflg&marks[type]))continue;
        auto& c=enemy_children_.emplace_back();
        RECVX_ENEMY_TEMPLATE t{};t.flg=1u;t.id=27;t.type=std::uint16_t(type);
        // Kein Raumbesitzer: owner_index ~0u (kein Zielindex eines Raumgegners).
        if(!recvx_enemy_set(&enemy_table_,&c.enemy,&t,&c,~0u,&enemy_services_)){enemy_children_.pop_back();unbound_enemy_.insert("bhCheckMothEgg: ene[] voll");continue;}
        c.enemy_bound=true;c.enemy_native=true;c.identity=27;c.model_absent=true;
    }
}

// bhSetEnemy(&ene18/&ene13B, rom->ene_n) mit `mlwP = &epw->mdl[base]` (en13.c:100-184).
// Das Kind gehoert wie ene[] dem Raum; owner_index ist der des Elternworks.
// share: `mlwP = &epw->mdl[10]` derselbe ML_WORK wie ein anderes Kind (en13.c:170).
WorldEntity* RoomWorld::create_enemy_child(RECVX_ENEMY_WORK* parent,const RECVX_ENEMY_TEMPLATE& t,unsigned base,RECVX_ENEMY_WORK* share){
    if(!parent)return nullptr;
    auto& p=tree_owner(owner_of(parent));
    if(!share&&base!=~0u&&!p.model_row()){unbound_enemy_.insert("eneset.c:170 Kind: Elternwork ohne Modellreihe");return nullptr;}
    auto& c=enemy_children_.emplace_back();
    if(!recvx_enemy_set(&enemy_table_,&c.enemy,&t,&c,parent->owner_index,&enemy_services_)){enemy_children_.pop_back();return nullptr;}
    c.enemy_bound=true;c.enemy_native=true;c.identity=t.id;
    if(share){const auto& sharer=owner_of(share);c.model_owner=&tree_owner(owner_of(share));c.model_bank=sharer.model_bank;c.model_base=sharer.model_base;return &c;}
    // bhSetEnemy laesst mlwP NULL (en23.c:492): kein Modell, kein Baum.
    if(base==~0u){c.model_absent=true;return &c;}
    c.model_bank=p.model_row();c.model_base=base; // epw->mdl[base], unabhaengig vom aktiven mlwP
    const PlayerAsset* asset=morph_variant(c,0);
    // Hinter mdl_n ist das ML_WORK genullt (room.c bhSetEneMdl): obj_num 0, kein Fehler.
    // Dasselbe gilt fuer einen Satz INNERHALB der Reihe, der 0 Objekte hat
    // (actor.cpp:493 laesst diesen Platz absichtlich leer) — im Original ein
    // ML_WORK mit obj_num 0. Nur ein gescheiterter Lesevorgang ist eine Luecke.
    c.model_absent=!asset;
    if(!asset&&p.model_row()->model_failed(base)){
        unbound_enemy_.insert("eneset.c:170 Kind: epw->mdl[base] liess sich nicht lesen");return &c;}
    if(!asset)return &c;
    c.enemy_asset=asset;
    if(asset){c.enemy_obj=asset->bind_pose();c.enemy_evalflags=asset->node_evalflags();c.enemy_owp_flg.assign(c.enemy_obj.size(),0u);c.enemy_owp_mtx.assign(c.enemy_obj.size(),BoneMatrix{});}
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
    if(!t.enemy_asset&&!t.model_absent){unbound_enemy_.insert("bhCalcModel: mlwP fehlt (kein Modell)");return;}
    NJS_MATRIX root;
    if(e.linked){
        // event.c:5312 bhObjLinkSetObjEne (0x51) sets the target enemy work's
        // lkwkp to the parent object work (sys->obwp[v0]) and its flg 0x80; the
        // sibling opcodes 0x54 (enemy->enemy) and 0x6f (player->object) do the
        // same for their target. bhCalcModel (MdlPut.c:248-256) then takes
        // lkwkp->mlwP->owP[lkono].mtx as the root, exactly like the flg-0x80
        // branch below. The native keeps the scripted parent as the generic
        // WorldEntity link (parent_kind/index/node), so the parent matrix comes
        // from link_matrix; the object parent has no stored enemy owP of its own.
        const auto parent=link_matrix(e.parent_kind,e.parent_index,e.parent_node);
        if(!parent){unbound_enemy_.insert("bhCalcModel: verknuepftes Elternwerk ohne owP");return;}
        std::memcpy(root,parent->value,sizeof root);
        njTranslate(&root,e.link_offset.x,e.link_offset.y,e.link_offset.z);
        if(w.flg&0x1000u)njUnitRotPortion(&root);
        njRotateXYZ(&root,w.ax,w.ay,w.az);std::memcpy(w.root_mtx,root,sizeof root);
    }else if((w.flg&0x80u)&&w.lk_player){
        // lkwkp = plp (en27.c:84): plp->mlwP->owP[lkono].mtx des letzten Spielerpasses.
        const auto parent=w.lkono>=0?link_matrix(0,0,unsigned(w.lkono)):std::nullopt;
        if(!parent){unbound_enemy_.insert("bhCalcModel: plp->mlwP->owP[lkono] fehlt");return;}
        std::memcpy(root,parent->value,sizeof root);
        njTranslate(&root,w.lox,w.loy,w.loz);
        if(w.flg&0x1000u)njUnitRotPortion(&root);
        njRotateXYZ(&root,w.ax,w.ay,w.az);std::memcpy(w.root_mtx,root,sizeof root);
    }else if(w.flg&0x80u){
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
    // MdlPut.c:286: obj_num 0 — ewP->mtx ist geschrieben, der Baum bleibt leer.
    if(!t.enemy_asset)return;
    if(!t.model_window)t.enemy_owp_mtx=t.enemy_asset->tree_matrices(t.enemy_obj,root,t.enemy_owp_flg,t.enemy_owp_mtx);
    // Mit verschobenem objP/owP laeuft bhCalcTree nur ueber die obj_num Knoten
    // ab dem Fenster und schreibt nur deren owP (MdlPut.c:262-310).
    else if(t.model_window<t.enemy_obj.size())t.enemy_asset->tree_window(t.enemy_obj,root,t.model_window,t.enemy_owp_flg,t.enemy_owp_mtx);
    else {unbound_enemy_.insert("bhCalcModel: objP-Fenster ausserhalb des Modells");return;}
    t.enemy_calculated=true;
    publish_enemy(e);
}

void RoomWorld::calc_enemy_draw_models(){
    EnemyWorldScope scope(this);
    const unsigned st_flg=collision_events_?collision_events_->flags[5][0]:0u;
    for(std::uint32_t i=0;i<enemy_table_.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){auto* w=enemy_table_.slot[i];if(!w||!w->owner)continue;
        if(!(w->mdflg&0x4u)||!recvx_enemy_draw_enabled(w,st_flg))continue;
        auto& e=owner_of(w);if(!e.enemy_native)continue;
        if((w->flg&0x80u)&&!w->lkwkp&&!w->lk_player)continue;
        calc_enemy_model(e);}
    // eneset.c:866 bhDrawEneObject (game.c nach bhDrawEnemy): die en_obj-Works (flg 0x80000000).
    for(std::uint32_t i=0;i<enemy_table_.en_objn&&i<RECVX_ENEMY_OBJECTS;++i){auto* w=enemy_table_.en_obj[i];if(!w||!w->owner)continue;
        if(!(w->flg&0x1u)||(w->stflg&0x41000000u)||!(w->mdflg&0x4u))continue;
        auto& e=owner_of(w);if(e.enemy_native)calc_enemy_model(e);}
}

// weapon.c:1419-1490 bhCheckBombAtari, enemy half only. The table points at
// the real works, including linked children and non-zombie enemy identities.
// watr and owP are the last published enemy-pass results, never a sampled pose.
void RoomWorld::bomb_enemy_damage(const BombRequest& bomb){
    const RECVX_CB_SPHERE blast{{bomb.position.x,bomb.position.y,bomb.position.z},bomb.radius};
    const auto walls=collision_geometry(); // rom->walp, then sys->mwalp
    for(std::uint32_t i=0;i<enemy_table_.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){
        auto* epw=enemy_table_.slot[i];
        if(!epw||!(epw->flg&1u)||!(epw->flg&0x20u)||(epw->flg&2u)||(epw->stflg&0x1000000u))continue;
        if(epw->flg&0x80u){
            if(!epw->lkwkp){unbound_enemy_.insert("bhCheckBombAtari: flg 0x80 ohne lkwkp (weapon.c:1423)");continue;}
            if(epw->lkwkp->stflg&0x1000000u)continue;
        }
        const RECVX_CB_CAPSULE capsule{{epw->watr.c1.x,epw->watr.c1.y,epw->watr.c1.z},
            {epw->watr.c2.x,epw->watr.c2.y,epw->watr.c2.z},epw->watr.r};
        if(!recvx_cb_check_sc(&blast,&capsule))continue;
        RECVX_CB_VEC contact;
        recvx_cb_distance_p2c(&blast.c,&capsule,&contact);
        const RECVX_CB_CAPSULE sight{contact,blast.c,0.1f};
        if(check_c2_wall_n(walls,room_height_,sight,0x400u))continue;

        epw->flg|=4u;epw->flg2|=0x200u;
        epw->dpx=contact.x;epw->dpy=contact.y;epw->dpz=contact.z;
        epw->dvx=contact.x-blast.c.x;epw->dvy=contact.y-blast.c.y;epw->dvz=contact.z-blast.c.z;
        epw->dax=0;epw->day=int(10430.381f*std::atan2(-epw->dvx,-epw->dvz));
        epw->wpnr_no=20;

        // j starts at 1, knr == 0 remains the original sentinel (also when a
        // joint sits exactly on the contact). Respect mlwP's shared window.
        const auto* sv=epw->sv;
        if(!sv||!sv->obj_num||!sv->joint_matrix){
            unbound_enemy_.insert("bhCheckBombAtari: mlwP->owP ohne Gelenkbesitzer (weapon.c:1459)");continue;
        }
        int kno=0;float nearest=0;bool joints_ok=true;
        const auto count=sv->obj_num(epw);
        for(std::uint32_t j=1;j<count;++j){
            float matrix[16];
            if(!sv->joint_matrix(epw,j,matrix)){
                unbound_enemy_.insert("bhCheckBombAtari: owP[j] ausserhalb des Modellbaums (weapon.c:1461)");joints_ok=false;break;
            }
            NJS_VECTOR delta{contact.x-matrix[12],contact.y-matrix[13],contact.z-matrix[14]};
            const float distance=njScalor(&delta);
            if(nearest==0||nearest>distance){nearest=distance;kno=int(j);}
        }
        if(!joints_ok)continue;
        if(unsigned(kno)>=RECVX_ENEMY_DAM){unbound_enemy_.insert("bhCheckBombAtari: dam[kno] ausserhalb dam[64]");continue;}
        epw->djnt_no=kno;
        NJS_VECTOR root{epw->px-blast.c.x,epw->py-blast.c.y,epw->pz-blast.c.z};
        if(njScalor(&root)<0.5f*bomb.radius){epw->dam[kno]+=bomb.damage_max;epw->comb_flg|=0x10u;}
        else{epw->dam[kno]+=bomb.damage_min;epw->comb_flg|=0x20u;}
    }
}

// eneset.c:344 bhControlEnemy over the table; native routines only. The
// legacy en01/en04 store keeps its own tick until C03/C04 replace it.
// en11.c-Dienste fuer den Gegnerpass und fuer Sub_controll 148 (bhEne11_LightControl aus dem Skript).
void RoomWorld::bind_en11_services(){
static RECVX_EN11_SERVICES en11{};
    // en11.c:132 sys->rm_flg &= ~1 (event.c:440 Bank 4 = rm_flg; Bank 1 der Dienstnummer).
    en11.flags_clear=[](uint32_t which,uint32_t bits){auto* world=enemy_world;static const unsigned bank[4]={10,4,5,6};
        if(which<4&&world->collision_events_)world->collision_events_->flags[bank[which]][0]&=~bits;else world->unbound_enemy_.insert("EN11 flags_clear: EventVm nicht gebunden");};
    // en11.c:170-185 bhSetEffectTb(&sys->ef {id 0x105, flg 1, Skala 0}, NULL, epw, 0).
    en11.effect_create=[](const RECVX_EN11_EFFECT_TEMPLATE* t,RECVX_ENEMY_WORK* link)->int{
        return enemy_world->effect_tb(t->flg,int(t->id),int(t->type),t->mdlver,{t->pos.x,t->pos.y,t->pos.z},{t->scale.x,t->scale.y,t->scale.z},0,t->ay,nullptr,link,false,0);};
    // eff[slot] als O_WRK-Sicht: mode0, ct2, ax, axp (effsub2.c:2014 bhEff_E11_SearchLight).
    en11.effect_get=[](int slot,RECVX_EN11_EFFECT* out)->int{auto* world=enemy_world;
        if(slot<0||std::size_t(slot)>=world->sprites_.size()||world->sprites_[std::size_t(slot)].sprite.id!=261)return 0;
        const auto& e=world->sprites_[std::size_t(slot)].sprite;out->mode0=std::uint8_t(e.mode0);out->ct2=e.ct2;out->ax=e.ax;out->axp=e.axp;return 1;};
    en11.effect_set=[](int slot,const RECVX_EN11_EFFECT* in){auto* world=enemy_world;
        if(slot<0||std::size_t(slot)>=world->sprites_.size()||world->sprites_[std::size_t(slot)].sprite.id!=261){world->unbound_enemy_.insert("EN11 eff[slot] ohne bhEff_E11_SearchLight");return;}
        auto& e=world->sprites_[std::size_t(slot)].sprite;e.mode0=in->mode0;e.ct2=in->ct2;e.ax=in->ax;e.axp=in->axp;};
    // mlwP->objP[node].ang (en11.c:1047-1090 bhEne11_CameraSet).
    en11.node_angles_get=[](RECVX_ENEMY_WORK* w,uint32_t node,int32_t out[3])->int{const auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_obj.size())return 0;for(unsigned k=0;k<3;++k)out[k]=t.enemy_obj[node].angles[k];return 1;};
    en11.node_angles_set=[](RECVX_ENEMY_WORK* w,uint32_t node,const int32_t in[3]){auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_obj.size()){enemy_world->unbound_enemy_.insert("EN11 objP[node].ang ausserhalb des Modells");return;}
        for(unsigned k=0;k<3;++k)t.enemy_obj[node].angles[k]=in[k];};
    // mlwP->owP[node].flg (en11.c:956-957 bhEne11_MV09).
    en11.node_work_flags=[](RECVX_ENEMY_WORK* w,uint32_t node,uint32_t set,uint32_t clear){auto& t=tree_of(owner_of(w));node+=t.model_window;
        if(node>=t.enemy_owp_flg.size()){enemy_world->unbound_enemy_.insert("EN11 owP[node].flg ausserhalb des Modells");return;}
        t.enemy_owp_flg[node]=(t.enemy_owp_flg[node]|set)&~clear;};
    en11.se_pan=[](uint32_t enemy,const RECVX_ENEMY_VEC* p,int se){enemy_world->audio_.request_enemy_se(enemy,{p->x,p->y,p->z},se,0,0);}; // zonzon1.c:852 bhEne_SetSEPan
    recvx_en11_bind(&en11);
}
// effect.c:869 bhControlEffect: bhEff349 (effsub3.c:3234) ruft mit bhEne19_TyBloodSet
// Gegnercode auf, das bhGetGroundPosition, mlwP->objP[18] und bhSetEffectTb braucht.
// Im Original ist das derselbe Satz wie im Gegnerpass; nativ sind Weltzeiger und
// EN19-Dienste nur waehrend bhControlEnemy gebunden, also hier fuer den Effektpass.
RoomWorld::EnemyCallbackFrame::EnemyCallbackFrame(RoomWorld* world)
    :previous_world_(enemy_world),previous_en19_(recvx_en19_bound()),previous_ext_(recvx_enemy_ext()),previous_en01_(recvx_en01_context()){
    enemy_world=world;
    recvx_en19_bind(en19_effect_services);
    recvx_enemy_ext_bind(&world->enemy_ext_);
    // Derselbe EN01-Kontext wie in control_enemy_works (sys/rom-Felder dieses Bildes).
    auto ctx=std::make_shared<RECVX_EN01_CONTEXT>();
    ctx->table=&world->enemy_table_;ctx->services=&world->enemy_services_;ctx->en01=&en01_services();
    ctx->game_mode=world->enemy_game_mode_;
    ctx->stg_no=world->collision_events_?std::int32_t(world->collision_events_->values[0]):0;
    ctx->rom_no=world->collision_events_?std::int32_t(world->collision_events_->values[1]):0;
    ctx->stage_valid=world->collision_events_?1u:0u;
    recvx_en01_set_context(ctx.get());en01_ctx_=ctx;
}
RoomWorld::EnemyCallbackFrame::~EnemyCallbackFrame(){
    recvx_en01_set_context(static_cast<const RECVX_EN01_CONTEXT*>(previous_en01_));
    recvx_enemy_ext_bind(const_cast<RECVX_ENEMY_EXT*>(static_cast<const RECVX_ENEMY_EXT*>(previous_ext_)));
    recvx_en19_bind(static_cast<const RECVX_EN19_SERVICES*>(previous_en19_));
    enemy_world=previous_world_;
}
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
    // en14.c:1311 bhEne14_Acid: sys->ef (id 256, flg 1, type 4/6, Lage, Skalierung) ueber
    // bhSetEffectTb; die uebrigen Felder der Vorlage bleiben die des letzten Schreibers.
    // Danach eff[eno] (en14.c:1370-1393): stflg 0x20, txp[0] = mdl[2].texP, tex_id 0,
    // Richtung, Lage, mdlver = se. Die Prozedur selbst ist bhEff_E03_Acid (256).
    static const RECVX_EN14_SERVICES en14=[]{
        RECVX_EN14_SERVICES t=en14_services();
        t.acid_alloc=[](RECVX_ENEMY_WORK*,uint32_t type,const RECVX_ENEMY_VEC* pos,float size)->int{
            auto& ef=effect_detail::request_template();ef.id=256;ef.flg=1;ef.type=type;ef.position={pos->x,pos->y,pos->z};ef.scale={size,size,size};
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;
            spawn.mdlver=ef.mdlver;spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;
            return enemy_world->spawn_effect(spawn);};
        t.acid_setup=[](RECVX_ENEMY_WORK* w,int slot,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* v,int se){
            auto* world=enemy_world;if(slot<0||std::size_t(slot)>=world->sprites_.size())return;
            auto& sw=world->sprites_[std::size_t(slot)];auto& s=sw.sprite;
            s.stflg|=0x20u;s.tex_id=0;s.xn=v->x;s.yn=v->y;s.zn=v->z;s.px=pos->x;s.py=pos->y;s.pz=pos->z;s.mdlver=unsigned(se);
            // mdl[2].texP: eigenes texP oder das des letzten texturierten mdl davor (room.c bhSetEneMdl),
            // wie en23 model_texture. effect.c:1329: stflg 0x20 waehlt txp[0], nicht sys->ef_tn[tex_id].
            const auto& e=tree_of(owner_of(w));const PlayerAsset* bank=e.model_bank?e.model_bank:e.model_row();
            const unsigned base=(e.model_bank?e.model_base:0u)+2u;sw.texture_list=nullptr;
            if(!bank){world->unbound_enemy_.insert("bhEne14_Acid: epw->mdl[2].texP ohne Modell");return;}
            for(unsigned i=base;i>0;--i)if(const auto& textures=bank->model_textures(i);!textures.empty()){sw.texture_list=&textures;return;}
            for(const auto& m:world->models_)if(m.actor.get()==bank){sw.texture_list=&m.object.textures;return;}
            world->unbound_enemy_.insert("bhEne14_Acid: epw->mdl[2].texP ohne Texturliste");};
        // en14.c:952 bhEne14_LookPlayaer: owP[6].flg Bit 2 (mask/value), objP[6].ang, owP[6].mtx und
        // der Drehschluessel mnwP[mtn_no].md2P[6].p[1][frm_no/65536] (wie en12 Kopfwinkel).
        t.joint_flags=[](RECVX_ENEMY_WORK* w,uint32_t joint,uint32_t mask,uint32_t value){
            auto& tr=tree_of(owner_of(w));joint+=tr.model_window;
            if(joint>=tr.enemy_owp_flg.size()){enemy_world->unbound_enemy_.insert("bhEne14_LookPlayaer: owP[joint].flg ausserhalb des Modells");return;}
            tr.enemy_owp_flg[joint]=(tr.enemy_owp_flg[joint]&~mask)|(value&mask);};
        t.node_angles=[](RECVX_ENEMY_WORK* w,uint32_t node,const int32_t* a){auto& tr=tree_of(owner_of(w));node+=tr.model_window;
            if(node>=tr.enemy_obj.size()){enemy_world->unbound_enemy_.insert("bhEne14_LookPlayaer: objP[node].ang ausserhalb des Modells");return;}
            for(unsigned k=0;k<3;++k)tr.enemy_obj[node].angles[k]=a[k];};
        t.motion_angles=[](const RECVX_ENEMY_WORK* w,uint32_t mtn,uint32_t node,uint32_t frame,int32_t* out)->int{
            const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);
            if(!clip||node>=clip->angles.size()||clip->angles[node].empty())return 0;
            const auto& keys=clip->angles[node];const auto& k=keys[std::min<std::size_t>(frame,keys.size()-1)];
            if(frame>=keys.size())enemy_world->unbound_enemy_.insert("bhEne14_LookPlayaer: frm_no hinter frm_num");
            out[0]=k[0];out[1]=k[1];out[2]=k[2];return 1;};
        t.joint_matrix_set=[](RECVX_ENEMY_WORK* w,uint32_t joint,const float* m){en01_services().joint_matrix_set(w,joint,m);};
        t.node_local=en01_services().node_local;
        // hitchk.c:5474 bhCheckFloorEnemy: erste Bodenflaeche type 2 (rom->flrp, sys->mflrp), wie en12.
        t.floor_enemy=[](int flr_no,float px,float pz,RECVX_ENEMY_ATR* out)->int{auto* world=enemy_world;
            for(const auto* table:{&world->room_floors_,&world->effect_water_mobile_})for(const auto& a:*table)
                if((a.flags&1u)&&a.type==2&&a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz&&a.floor==flr_no){if(out)*out=atr_of(a);return 1;}
            return 0;};
        t.game_mode=[](int* out)->int{*out=int(enemy_world->enemy_game_mode_);return 1;};
        // en14.c:213-221: erster eff[0..511] mit flg 1, ohne stflg 0x1000000, id 130, type 4
        // (Spielerwaffe aus WpnEffTab); Handle = Platz + 1, 0 = keiner. Die Suche laeuft gegen den
        // echten Pool: ist kein Strahl (Effekt 130, effsub1.c:9073) angelegt, findet die Suche
        // nichts, wie im Original ohne Strahl.
        t.find_beam=[](uint32_t* handle)->int{auto* world=enemy_world;*handle=0;
            for(std::size_t i=0;i<world->sprites_.size();++i){const auto& e=world->sprites_[i].sprite;
                if((e.flg&1u)&&!(e.stflg&0x1000000u)&&e.id==130&&e.type==4){*handle=uint32_t(i)+1u;break;}}
            return 1;};
        // en14.c:657-664/833-839: O_WRK +0x10 px/py/pz und +0x64 lox/loy/loz (Kopfsicht wie
        // RECVX_EN16_EFF in en16.h; bhEff130 effsub1.c:9073 schiebt px += lox je Pass).
        t.beam_line=[](uint32_t handle,RECVX_ENEMY_VEC* pos,RECVX_ENEMY_VEC* dir)->int{auto* world=enemy_world;
            if(handle==0||std::size_t(handle-1)>=world->sprites_.size())return 0;
            const auto& e=world->sprites_[std::size_t(handle-1)].sprite;
            *pos={e.px,e.py,e.pz};*dir={e.lox,e.loy,e.loz};return 1;};
        return t;}();
    recvx_en14_bind_services([](const RECVX_ENEMY_WORK*)->const RECVX_EN14_SERVICES*{return &en14;});
    recvx_en18_bind(&en18_services());
    recvx_en30_bind(&en30_services());
    const RECVX_EN21_SERVICES en21{
        en01_services().route_next,
        [](int* out)->int{*out=int(enemy_world->enemy_game_mode_);return 1;},
        [](int16_t way){enemy_world->player_way_=way;},
        [](RECVX_ENEMY_WORK* w,int id,int type,const RECVX_ENEMY_VEC* pos,const int32_t* ax,const int32_t* ay,int tex)->int{
            auto& ef=effect_detail::request_template();ef.id=unsigned(id);ef.flg=1;ef.type=unsigned(type);
            ef.position={pos->x,pos->y,pos->z};ef.scale={1,1,1};if(ax)ef.ax=*ax;if(ay)ef.ay=*ay;
            auto* world=enemy_world;
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;
            spawn.mdlver=ef.mdlver;spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;
            const int slot=world->spawn_effect(spawn);
            if(slot>=0){RECVX_T3_EFFPATCH patch{};patch.fields=RECVX_T3_EFF_STFLG_OR|RECVX_T3_EFF_TEXP|RECVX_T3_EFF_TEX_ID;
                patch.stflg_or=0x20;patch.texp=world->enemy_ext_.model_texp(w);patch.tex_id=tex;world->enemy_ext_.effect_patch(slot,&patch);}
            return slot;}};
    {   // EN11 (Spotter) und EN16 (Laserziel): dieselben prozessweiten Dienste wie die
        // T3-Ergaenzung. Sie liegen HIER, weil control_enemy_works sie am Ende wieder
        // loest; vorher band sie nur init_enemy_works, sodass ab dem zweiten Pass keine
        // Dienste mehr gefunden wurden (Fehlluecke "bhEne11: linked bhEff261 work").
        bind_en11_services();
        static RECVX_EN16_SERVICES en16{};
        en16.effect_tb=[](RECVX_ENEMY_WORK* w)->int{return enemy_world->effect_tb(1,270,0,0,{0,0,0},{0,0,0},0,0,nullptr,w,false,0);};
        en16.effect_get=[](int slot,RECVX_EN16_EFF* out)->int{auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()||world->sprites_[std::size_t(slot)].sprite.id!=270)return 0;
            const auto& e=world->sprites_[std::size_t(slot)].sprite;out->mode0=int32_t(e.mode0);out->ct0=e.ct0;
            out->gpx={e.gpx,e.gpy,e.gpz};out->lox={e.lox,e.loy,e.loz};out->atr=e.line_wall?const_cast<CollisionWall*>(&*e.line_wall):nullptr;return 1;};
        en16.effect_mode=[](int slot,int32_t mode0){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()||world->sprites_[std::size_t(slot)].sprite.id!=270){world->unbound_enemy_.insert("EN16 eff[slot] ohne bhEff_E16_LaserSight");return;}
            world->sprites_[std::size_t(slot)].sprite.mode0=unsigned(mode0);};
        en16.effect_step=[](int slot){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size())return;
            auto& work=world->sprites_[std::size_t(slot)];step_effect_sprite(work.sprite,world->effect_context(),&work.queued);};
        en16.floor_enemy=[](int flr_no,float px,float pz,int32_t* prm0)->int{auto* world=enemy_world;
            for(const auto* table:{&world->room_floors_,&world->effect_water_mobile_})for(const auto& a:*table)
                if((a.flags&1u)&&a.type==2&&a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz&&a.floor==flr_no){*prm0=int32_t(a.parameters[0]);return 1;}
            return 0;};
        // hitchk.c:5881 bhCheckWallAttrB89: attr 0x100 aus oder st_flg 1 -> 1; ohne 0x200 -> 0;
        // sonst 0, wenn prm1..prm3 den laufenden Schnitt cam.ncut nennen.
        en16.wall_attr_b89=[](const void* atr,int32_t* prm0)->int{auto* world=enemy_world;const auto& hp=*static_cast<const CollisionWall*>(atr);*prm0=int32_t(hp.parameters[0]);
            if(!world->collision_events_){world->unbound_enemy_.insert("bhCheckWallAttrB89: sys->st_flg/cam.ncut ohne EventVm");return 0;}
            if(!(hp.attributes&0x100u)||(world->collision_events_->flags[5][0]&1u))return 1;
            if(!(hp.attributes&0x200u))return 0;
            const unsigned ncut=world->collision_events_->values[2]&255u;
            for(unsigned i=1;i<4;++i)if((hp.parameters[i]&255u)==ncut)return 0;
            return 1;};
        en16.gun_spark=[](const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* dir,uint32_t src,uint32_t dst,int typ){enemy_world->gun_spark({pos->x,pos->y,pos->z},{dir->x,dir->y,dir->z},src,dst,typ);};
        en16.hiteff13=WpnTab[13].hiteff;
        // en16.c:265 bhSetGunSplash: dieselbe Routine wie der Kampfpass (combat.cpp gun_splash);
        // die im Original uninitialisierte Richtung setzt en16 nach QUALITY.md auf 0.
        en16.gun_splash=[](const RECVX_ENEMY_VEC* g,const RECVX_ENEMY_VEC* pos,int wattr,int wpn_no,int ay){
            // Ausserhalb des Effektpasses belegt bhSetEffectTb sofort einen Platz (wie der Kampfpass, combat.cpp).
            gun_splash({g->x,g->y,g->z},{pos->x,pos->y,pos->z},wattr,wpn_no,ay,
                [](int lkono,const NJS_POINT3* off){const auto& ef=effect_detail::request_template();
                    EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
                    spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=lkono;
                    spawn.has_offset=off!=nullptr;if(off)spawn.offset=*off;
                    enemy_world->spawn_unlinked_effect(spawn);},
                [](const char* name){enemy_world->unbound_enemy_.insert(name);});};
        recvx_en16_bind(&en16);
    }
    recvx_en21_bind(&en21);
    static const RECVX_EN09_DATA en09_data=recvx_en09_default_data();recvx_en09_set_data(&en09_data);
    const auto& common=en01_services();
    const RECVX_EN09_SERVICES en09{
        []()->int{return int(enemy_world->enemy_game_mode_);},common.weapon_flg,common.set_material_color,
        common.node_local,common.node_local_set,common.node_angle_set,common.calc_fix_offset,
        [](RECVX_ENEMY_WORK* w){morph_pair_set(w,0,1);en01_services().set_material_color(w,0xffb2b2b2u);},
        [](uint32_t j,uint32_t bits){auto* world=enemy_world;
            if(j>=world->player_owp_flags_.size()){world->unbound_enemy_.insert("EN09 plp->owP[j].flg");return;}
            world->player_owp_flags_[j]&=~bits;},
        common.search_player,common.route_next,
        [](const RECVX_ENEMY_VEC* pos,uint32_t flags,float r,float h,RECVX_ENEMY_ATR* out)->int{
            const auto hit=enemy_world->collision_contact({pos->x,pos->y,pos->z},flags,r,h);if(!hit)return 0;
            if(out)*out=atr_of(*hit);return 1;},common.enemy_atari_atr,
        [](RECVX_ENEMY_VEC* out)->int{
            auto* world=enemy_world;const MotionClip* clip=world->player_enemy_motion_?world->player_enemy_clip():
                (world->player_asset_&&world->player_asset_->has_motion(world->motion_)?&world->player_asset_->motion_clip(world->motion_):nullptr);
            if(!clip){world->unbound_enemy_.insert("EN09 PlayerLink: mnwP fehlt");return 0;}
            MotionWork m{int32_t(world->frame_),0,world->player_hokan_count_,0,world->player_hokan_rate_};std::string gap;
            set_motion(m,0,world->player_mtn_md_,clip,nullptr,world->player_obj_,world->player_owp_flags_,&gap,0,unsigned(world->motion_));
            world->player_hokan_count_=m.hokan_count;if(!gap.empty()){world->unbound_enemy_.insert(gap);return 0;}
            NJS_MATRIX matrix;njUnitMatrix(&matrix);njTranslate(&matrix,world->player_.position.x,world->player_.position.y,world->player_.position.z);
            njRotateXYZ(&matrix,world->player_.pitch,world->player_.yaw,world->player_.roll);
            if(world->player_obj_.size()<6){world->unbound_enemy_.insert("EN09 PlayerLink: plp Kette 0..5");return 0;}
            for(unsigned j=0;j<6;++j){const auto& node=world->player_obj_[j];njTranslate(&matrix,node.position.x,node.position.y,node.position.z);njRotateXYZ(&matrix,node.angles[0],node.angles[1],node.angles[2]);}
            *out={matrix[12],matrix[13],matrix[14]};return 1;},
        [](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_VEC* ofs,int rot){
            auto* world=enemy_world;auto& ef=effect_detail::request_template();ef.flg=1;ef.id=369;ef.type=1;ef.scale={2,2,2};ef.ax=0;ef.ay=rot;ef.mdlver=0;ef.position.y=ofs->y;
            for(int i=0;i<3;++i){ef.position.x=ofs->x+2*effect_detail::effect_random()-1;ef.position.z=ofs->z+2*effect_detail::effect_random()-1;
                EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.scale=ef.scale;spawn.position=ef.position;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.flr_no=ef.flr_no;
                const int slot=world->spawn_effect(spawn);
                if(slot>=0){RECVX_T3_EFFPATCH p{};p.fields=RECVX_T3_EFF_STFLG_OR|RECVX_T3_EFF_TEXP|RECVX_T3_EFF_TEX_ID|RECVX_T3_EFF_VECTOR|RECVX_T3_EFF_CT3;
                    p.stflg_or=0x20;p.texp=world->enemy_ext_.model_texp(w);p.tex_id=10;p.xn=1+.400000006f*effect_detail::effect_random();p.yn=.05000000075f;p.zn=.8999999762f;p.ct3=i;world->enemy_ext_.effect_patch(slot,&p);}}}};
    recvx_en09_bind(&en09);
    // --- EN12 (en12.h): Blattzugriffe auf vorhandene Besitzer ---------------------
    static RECVX_EN12_SERVICES en12=[]{RECVX_EN12_SERVICES t=en12_services();
        // rutchk.c:5 bhCheckRoute: out nur fuer gueltige Zonen (sonst Original unbestimmt).
        t.route_points=[](const RECVX_ENEMY_VEC* from,const RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* out)->int{
            const auto& e=en01_services();if(e.route_id(from)==0xFF||e.route_id(to)==0xFF){enemy_world->unbound_enemy_.insert("bhCheckRoute(en12): Lage ausserhalb rom->rutp");return 0;}e.route_next(from,to,out);return 1;};
        t.local_node=en01_services().node_local;
        // MN_WORK md2P[node].p[1][frame] (en12.c:1453 Kopfwinkel aus der Motion).
        t.motion_angles=[](const RECVX_ENEMY_WORK* w,uint32_t mtn,uint32_t node,uint32_t frame,int32_t* out)->int{
            const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);
            if(!clip||node>=clip->angles.size()||clip->angles[node].empty())return 0;
            const auto& keys=clip->angles[node];const auto& k=keys[std::min<std::size_t>(frame,keys.size()-1)];
            if(frame>=keys.size())enemy_world->unbound_enemy_.insert("en12 Kopfwinkel: frm_no hinter frm_num");
            out[0]=k[0];out[1]=k[1];out[2]=k[2];return 1;};
        // en12.c:1488-1495: objP[node].ang und owP[node].mtx.
        t.joint_pose_set=[](RECVX_ENEMY_WORK* w,uint32_t node,const int32_t* ang,const float* mtx){
            auto& tr=tree_of(owner_of(w));node+=tr.model_window;
            if(node>=tr.enemy_obj.size()||node>=tr.enemy_owp_mtx.size()){enemy_world->unbound_enemy_.insert("en12 objP/owP[node] ausserhalb des Modells");return;}
            for(unsigned k=0;k<3;++k)tr.enemy_obj[node].angles[k]=ang[k];std::memcpy(tr.enemy_owp_mtx[node].value,mtx,16*sizeof(float));};
        t.fix_offset=[](RECVX_ENEMY_WORK* w,const int8_t* chain,RECVX_ENEMY_VEC* out)->int{en01_services().calc_fix_offset(w,chain,nullptr,out);return 1;};
        // hitchk.c:5474 bhCheckFloorEnemy: erste Bodenflaeche type 2 (rom->flrp, sys->mflrp).
        t.floor_enemy=[](int flr_no,float px,float pz,RECVX_ENEMY_ATR* out)->int{auto* world=enemy_world;
            for(const auto* table:{&world->room_floors_,&world->effect_water_mobile_})for(const auto& a:*table)
                if((a.flags&1u)&&a.type==2&&a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz&&a.floor==flr_no){if(out)*out=atr_of(a);return 1;}
            return 0;};
        t.system_flags_clear=[](uint32_t which,uint32_t bits){auto* world=enemy_world;static const unsigned bank[4]={10,4,5,6};
            if(which<4&&world->collision_events_)world->collision_events_->flags[bank[which]][0]&=~bits;else world->unbound_enemy_.insert("en12 system_flags_clear: EventVm nicht gebunden");};
        // effect.c:436 bhSetEffectTb(&sys->ef, ofs, epw, jno): nur maskierte Felder der
        // gemeinsamen Vorlage sys->ef werden geschrieben, der Rest bleibt stehen.
        t.effect_tb=[](const RECVX_EN12_EFFECT_TEMPLATE* tp,uint32_t mask,RECVX_ENEMY_WORK* link,const RECVX_ENEMY_VEC* ofs,int joint)->int{
            auto& ef=effect_detail::request_template();
            if(mask&RECVX_EN12_EF_ID)ef.id=tp->id;if(mask&RECVX_EN12_EF_FLG)ef.flg=tp->flg;if(mask&RECVX_EN12_EF_TYPE)ef.type=tp->type;if(mask&RECVX_EN12_EF_MDLVER)ef.mdlver=tp->mdlver;
            if(mask&RECVX_EN12_EF_POS)ef.position={tp->pos.x,tp->pos.y,tp->pos.z};
            if(mask&RECVX_EN12_EF_SX)ef.scale.x=tp->scale.x;if(mask&RECVX_EN12_EF_SY)ef.scale.y=tp->scale.y;if(mask&RECVX_EN12_EF_SZ)ef.scale.z=tp->scale.z;
            if(mask&RECVX_EN12_EF_AY)ef.ay=tp->ay;
            return enemy_world->effect_tb(ef.flg,int(ef.id),int(ef.type),ef.mdlver,ef.position,ef.scale,ef.ax,ef.ay,ofs,link,false,joint);};
        t.effect_get=[](int slot,RECVX_EN12_EFFECT_VIEW* v)->int{auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size())return 0;const auto& e=world->sprites_[std::size_t(slot)].sprite;
            v->id=e.id;v->flg=e.flg;v->stflg=e.stflg;v->mode0=std::uint8_t(e.mode0);v->ct0=e.ct0;v->ct1=e.ct1;v->ax=e.ax;v->ay=e.ay;v->tex_id=e.tex_id;
            v->pos={e.px,e.py,e.pz};v->velocity={e.xn,e.yn,e.zn};v->normal={e.aox,e.aoy,e.aoz};v->ofy=e.ofy;v->exp1_slot=e.exp1_slot;return 1;};
        t.effect_set=[](int slot,const RECVX_EN12_EFFECT_VIEW* v){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("en12 eff[slot] ausserhalb");return;}
            auto& w=world->sprites_[std::size_t(slot)];auto& e=w.sprite;
            e.flg=v->flg;e.stflg=v->stflg;e.mode0=v->mode0;e.ct0=v->ct0;e.ct1=v->ct1;e.ax=v->ax;e.ay=v->ay;e.tex_id=v->tex_id;
            e.px=v->pos.x;e.py=v->pos.y;e.pz=v->pos.z;e.xn=v->velocity.x;e.yn=v->velocity.y;e.zn=v->velocity.z;e.aox=v->normal.x;e.aoy=v->normal.y;e.aoz=v->normal.z;e.ofy=v->ofy;e.exp1_slot=v->exp1_slot;
            auto& ent=world->entity(4,unsigned(slot));ent.transform.position={e.px,e.py,e.pz};ent.angles={e.ax,e.ay,e.az};};
        // eff[eno].txp[0] = epw->mdl[model].texP (en12.c:1865).
        t.effect_texture=[](int slot,const RECVX_ENEMY_WORK* w,uint32_t model){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size())return;const auto& owner=owner_of(w);
            for(const auto& m:world->models_)if(m.kind==1&&&world->entity(m.kind,m.index)==&owner){if(const auto* a=model_variant(m,model)){world->sprites_[std::size_t(slot)].texture_list=&a->textures();return;}break;}
            world->unbound_enemy_.insert("en12 mdl[model].texP nicht geladen");};
        t.player_extra_get=[](int32_t* ct1,int32_t* ayp)->int{*ct1=enemy_world->player_ct1_;*ayp=enemy_world->player_ayp_;return 1;};
        t.player_extra_set=[](int32_t ct1,int32_t ayp){enemy_world->player_ct1_=ct1;enemy_world->player_ayp_=ayp;};
        // plp->mnwP: 0 eigene Bibliothek, 1/2 die des Gegnerworks (epw->mnwP bzw. mnwPb; room.c:376 gleich).
        t.player_library=[](const RECVX_ENEMY_WORK* w,uint32_t source){auto* world=enemy_world;
            if(source==0){world->player_enemy_motion_=false;world->player_enemy_work_=nullptr;return;}
            world->player_enemy_motion_=true;world->player_enemy_owner_=w->id;world->player_enemy_work_=const_cast<RECVX_ENEMY_WORK*>(w);};
        // en12.c:1765 bhCheckWall(plp): direkte ATR-Aufloesung am Spielerwork.
        t.player_wall=[](RECVX_ENEMY_PLAYER* p)->int{auto* world=enemy_world;
            MotionActor actor=world->motion_actor();
            auto walls=world->collision_geometry();auto steps=world->motion_steps();auto floors=world->motion_floors();
            MotionTerrain terrain{walls,steps,floors,world->floors_,world->room_height_};
            NJS_POINT3 q{p->pos.x,p->pos.y,p->pos.z};
            const bool hit=resolve_motion_wall_direct(terrain,actor,q,3.5f,world->player_character_?17.5f:16.5f);
            p->pos={q.x,q.y,q.z};return hit?1:0;};
        // plp->mlwP->texP = plp->txp[0]: der native Spieler zeichnet stets txp[0].
        t.player_texture_zero=[](){};
        t.player_material=[](uint32_t rgba){enemy_world->player_material_=rgba;};
        t.object_material=[](uint32_t object,uint32_t,uint32_t rgba){auto* world=enemy_world;if(!world->state(2,object)){world->unbound_enemy_.insert("en12 sys->obwp[n] ausserhalb rom->obj_n");return;}
            auto& e=world->entity(2,object);e.enemy_material_set=true;e.enemy_material_color=rgba;};
        t.player_fire=[](int joint,const RECVX_ENEMY_VEC* ofp,float size,int life){const RECVX_T3_LINK lk{nullptr,1};recvx_t3_set_fire_effect_link(lk,joint,ofp,size,life);};
        return t;}();
    for(auto& pair:entities_)if(pair.first.first==1&&pair.second.enemy_bound&&pair.second.identity==12)
        recvx_en12_bind(&pair.second.enemy,&en12);
    // --- EN13 (en13.h): Kinderzeugung, Modellbesitz, Kamera, Spielerbibliothek ---------
    static const RECVX_EN13_SERVICES en13=[]{RECVX_EN13_SERVICES t{};
        t.create_child=[](RECVX_ENEMY_WORK* parent,const RECVX_ENEMY_TEMPLATE* tmpl,uint32_t base,RECVX_ENEMY_WORK* share)->RECVX_ENEMY_WORK*{
            auto* c=enemy_world->create_enemy_child(parent,*tmpl,base,share);return c?&c->enemy:nullptr;};
        // en13.c:186-191: mnwP/mnwPb des Elternworks; skp[13] liegt nativ im Modell mdl[13].
        t.share_motion=[](RECVX_ENEMY_WORK* child,const RECVX_ENEMY_WORK* parent){auto& c=owner_of(child);const auto& p=owner_of(parent);
            c.motion_library=p.motion_library>=0?p.motion_library:int(p.identity);child->mtn_room=parent->mtn_room;};
        // bhEne13_RestoreObject: objP[i].pos und Vertexstrom wie beim ersten Init gespeichert,
        // also wie geladen (die Raumdaten sind bei jedem Laden unveraendert).
        t.restore_objects=[](RECVX_ENEMY_WORK* w)->int{auto& e=tree_of(owner_of(w));if(e.model_absent)return 1;if(!e.enemy_asset)return 0;
            if(e.morph_obj_a!=0){enemy_world->unbound_enemy_.insert("bhEne13_RestoreObject: mlwP->objP ist nicht mdl[0]");return 0;}
            for(std::size_t i=0;i<e.enemy_obj.size();++i)e.enemy_obj[i].position=e.enemy_asset->bind_position(i);
            e.live_model.clear();return 1;};
        t.select_first_root=[](RECVX_ENEMY_WORK* w)->int{auto& e=owner_of(w);if(!e.enemy_asset&&!e.model_absent)return 0;e.morph_obj_a=0;return 1;};
        // bhEne13_Finish/ScaleModel: objP[ono].pos und p[0]/p[1] des Vertexstroms an Ort und Stelle.
        t.scale_object=[](RECVX_ENEMY_WORK* w,uint32_t ono,const RECVX_ENEMY_VEC* ps,const RECVX_ENEMY_VEC* vs)->int{auto& e=tree_of(owner_of(w));
            if(!e.enemy_asset||ono>=e.enemy_obj.size())return 0;
            if(e.morph_obj_a!=0){enemy_world->unbound_enemy_.insert("bhEne13_ScaleModel: mlwP->objP ist nicht mdl[0]");return 0;}
            auto& pos=e.enemy_obj[ono].position;pos.x*=ps->x;pos.y*=ps->y;pos.z*=ps->z;
            if(e.live_model.empty())e.live_model=e.enemy_asset->live_stream();
            BinaryView v(e.enemy_asset->encoded_model());const auto base=v.u16(4);
            const auto objects=v.relative(base,v.u32(12),e.enemy_asset->bones()*52);
            const auto mdl=v.u32(objects+ono*52+4);if(mdl==0xffffffffu)return 1; // pObj->model == NULL
            const auto vlist=v.u32(v.relative(base,mdl,24));if(vlist==0xffffffffu)return 1;
            const auto chunk=v.relative(base,vlist,64);const unsigned n=v.u16(chunk+6); // HDR_PS usIndexMax
            v.require(chunk+64,std::size_t(n)*32);
            auto scale=[&](std::size_t at,float f){float x;std::memcpy(&x,e.live_model.data()+at,4);x*=f;std::memcpy(e.live_model.data()+at,&x,4);};
            for(unsigned i=0;i<n;++i){const auto p=chunk+64+std::size_t(i)*32;
                scale(p,vs->x);scale(p+4,vs->y);scale(p+8,vs->z);scale(p+16,vs->x);scale(p+20,vs->y);scale(p+24,vs->z);}
            return 1;};
        t.camera_offset=[](float x,float y,float z){auto* world=enemy_world;if(!world->camera_offset_){world->unbound_enemy_.insert("cam.ofx/ofy/ofz ohne Kamerabesitzer");return;}*world->camera_offset_={x,y,z};};
        t.game_mode=[](int* out)->int{*out=int(enemy_world->enemy_game_mode_);return 1;};
        t.player_library=[](const RECVX_ENEMY_WORK* w,uint32_t source){auto* world=enemy_world;
            if(source==0){world->player_enemy_motion_=false;world->player_enemy_work_=nullptr;return;}
            world->player_enemy_motion_=true;world->player_enemy_owner_=w->id;world->player_enemy_work_=const_cast<RECVX_ENEMY_WORK*>(w);};
        // zonzon1.c:611 bhEne_AttackHitCheck(pl, pos, ar) mit einem Gegnerwork als pl.
        t.attack_hit_work=[](RECVX_ENEMY_WORK* pl,const RECVX_ENEMY_VEC* pos,float ar)->int{auto* world=enemy_world;const auto& sv=world->enemy_services_;
            int kno=-1;float ll;
            if(!sv.sphere_capsule(pos,ar,&pl->watr.c1,&pl->watr.c2,pl->watr.r))return 0;
            ll=ar*ar;const uint32_t n=sv.obj_num(pl);
            for(uint32_t i=0;i<n;++i){float m[16];if(!sv.joint_matrix(pl,i,m)){world->unbound_enemy_.insert("bhEne_AttackHitCheck: pl->mlwP->owP[i]");return 0;}
                NJS_POINT3 vec{pos->x-m[12],pos->y-m[13],pos->z-m[14]};const float knr=njScalor2(&vec);
                if(ll>knr){ll=knr;kno=int(i);pl->dvx=-vec.x;pl->dvy=-vec.y;pl->dvz=-vec.z;}}
            if(kno>=0){pl->djnt_no=kno;pl->dpx=pos->x;pl->dpy=pos->y;pl->dpz=pos->z;return 1;}
            return 0;};
        t.node_position=[](const RECVX_ENEMY_WORK* w,uint32_t node,RECVX_ENEMY_VEC* out)->int{float m[16];
            if(!enemy_world->enemy_services_.joint_matrix(w,node,m))return 0;*out={m[12],m[13],m[14]};return 1;};
        return t;}();
    recvx_en13_bind(&en13);
    // --- EN03-Familie (en03_spider.h): Baumdienste am Besitzer des mlwP -----------
    static const RECVX_EN03_SERVICES en03=[]{RECVX_EN03_SERVICES t{};
        t.hide_parts=[](RECVX_ENEMY_WORK* w,uint32_t node,int hide)->int{auto& e=tree_of(owner_of(w));if(!e.enemy_asset)return 0;
            const auto& a=*e.enemy_asset;node+=e.model_window;if(node>=e.enemy_evalflags.size())return 0;
            auto set=[&](std::size_t n){if(hide)e.enemy_evalflags[n]|=8u;else e.enemy_evalflags[n]&=~8u;};
            // en03.c:4990 Sub0/Sub1: child und rekursiv dessen child/sibling.
            std::function<void(std::int32_t)> walk=[&](std::int32_t n){while(n>=0){if(std::size_t(n)>=e.enemy_evalflags.size())return;set(std::size_t(n));walk(a.child(std::size_t(n)));n=a.sibling(std::size_t(n));}};
            set(node);walk(a.child(node));return 1;};
        t.model_flags=[](RECVX_ENEMY_WORK* w,uint32_t and_flg,uint32_t or_flg)->int{auto& e=tree_of(owner_of(w));
            const auto& sv=enemy_world->enemy_services_;const uint32_t n=sv.motion_obj_num?sv.motion_obj_num(w,0):0u; // epw->mnwP->obj_num
            if(!n)return 0;for(uint32_t i=0;i<n;++i){const auto k=e.model_window+i;if(k>=e.enemy_owp_flg.size())return 0;e.enemy_owp_flg[k]=(e.enemy_owp_flg[k]&and_flg)|or_flg;}return 1;};
        t.owp_flag_or=[](RECVX_ENEMY_WORK* w,uint32_t node,uint32_t bits)->int{auto& e=tree_of(owner_of(w));node+=e.model_window;if(node>=e.enemy_owp_flg.size())return 0;e.enemy_owp_flg[node]|=bits;return 1;};
        t.owp_matrix=[](const RECVX_ENEMY_WORK* w,uint32_t node,float* out)->int{const auto& e=tree_of(owner_of(w));node+=e.model_window;if(node>=e.enemy_owp_mtx.size())return 0;std::memcpy(out,e.enemy_owp_mtx[node].value,16*sizeof(float));return 1;};
        t.owp_matrix_set=[](RECVX_ENEMY_WORK* w,uint32_t node,const float* m)->int{auto& e=tree_of(owner_of(w));node+=e.model_window;if(node>=e.enemy_owp_mtx.size())return 0;std::memcpy(e.enemy_owp_mtx[node].value,m,16*sizeof(float));return 1;};
        t.root_angles_set=[](RECVX_ENEMY_WORK* w,const int32_t* a)->int{auto& e=tree_of(owner_of(w));if(e.model_window>=e.enemy_obj.size())return 0;auto& o=e.enemy_obj[e.model_window];o.angles={a[0],a[1],a[2]};return 1;};
        return t;}();
    recvx_en03_bind(&en03);
    // --- EN23 (en23.h) -------------------------------------------------------------
    static const RECVX_EN23_SERVICES en23=[]{RECVX_EN23_SERVICES t{};
        t.create_child=[](RECVX_ENEMY_WORK* parent,const RECVX_ENEMY_TEMPLATE* tmpl)->RECVX_ENEMY_WORK*{auto* c=enemy_world->create_enemy_child(parent,*tmpl,~0u,nullptr);return c?&c->enemy:nullptr;};
        t.route_points=[](const RECVX_ENEMY_VEC* from,const RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* out)->int{return en01_services().route_next(from,to,out);};
        t.game_mode=[](int* out)->int{*out=int(enemy_world->enemy_game_mode_);return 1;};
        t.player_library=en13.player_library;
        t.effect_id=[](int slot,uint32_t id){auto* world=enemy_world;if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot].id ausserhalb");return;}world->sprites_[std::size_t(slot)].sprite.id=id;};
        // mdl[k].texP: eigenes texP oder das des letzten texturierten mdl davor (room.c bhSetEneMdl).
        t.model_texture=[](const RECVX_ENEMY_WORK* w,uint32_t k)->const void*{auto* world=enemy_world;const auto& e=tree_of(owner_of(w));
            const PlayerAsset* bank=e.model_bank?e.model_bank:e.model_row();const unsigned base=(e.model_bank?e.model_base:0u)+k;if(!bank)return nullptr;
            for(unsigned i=base;i>0;--i)if(const auto& textures=bank->model_textures(i);!textures.empty())return &textures;
            for(const auto& m:world->models_)if(m.actor.get()==bank)return &m.object.textures;
            return nullptr;};
        t.effect_position=[](int slot,RECVX_ENEMY_VEC* p,int write)->int{auto* world=enemy_world;if(slot<0||std::size_t(slot)>=world->sprites_.size())return 0;auto& s=world->sprites_[std::size_t(slot)].sprite;
            if(write){s.px=p->x;s.py=p->y;s.pz=p->z;}else *p={s.px,s.py,s.pz};return 1;};
        t.material_color=[](RECVX_ENEMY_WORK* w,uint32_t rgba){en01_services().set_material_color(w,rgba);};
        t.room_flags=[](uint32_t set,uint32_t clear){auto* world=enemy_world;if(!world->collision_events_){world->unbound_enemy_.insert("sys->rm_flg: EventVm nicht gebunden");return;}
            auto& f=world->collision_events_->flags[4][0];f|=set;f&=~clear;};
        t.effect_tb=[](uint32_t id,uint32_t type,uint32_t flg,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* scale)->int{
            auto& ef=effect_detail::request_template();ef.id=id;ef.type=type;ef.flg=flg;ef.position={pos->x,pos->y,pos->z};ef.scale={scale->x,scale->y,scale->z};
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;
            spawn.mdlver=ef.mdlver;spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;
            return enemy_world->spawn_effect(spawn);};
        t.morph_sources=[](RECVX_ENEMY_WORK* w,uint32_t a,uint32_t b){morph_pair_set(w,a,b);};
        return t;}();
    recvx_en23_bind(&en23);
    // --- EN53 (en53.h, en15.c:3439-3745): Nosferatu-Kern auf dem vorhandenen Besitzer --
    // mbp[k] ist mdl[k] des Works (morph_variant); obj_a/obj_b sind morph_obj_a/b.
    // en15.c:3350: nur das Gruenbyte des Ambient-Materials im ersten Kindmodell.
    static const RECVX_EN53_SERVICES en53=[]{RECVX_EN53_SERVICES t{};
        t.child_ambient=[](RECVX_ENEMY_WORK* epw,uint32_t model,uint8_t value)->int{
            auto& e=tree_of(owner_of(epw));const auto* asset=morph_variant(e,model);if(!asset)return 0;
            const auto child=asset->child(0);if(child<0)return 0;
            auto& stream=model?e.live_models[model]:e.live_model;
            if(stream.empty())stream=asset->live_stream();
            BinaryView v(stream);const auto base=v.u16(4);
            const auto objects=v.relative(base,v.u32(12),asset->bones()*52);
            const auto pointer=v.u32(objects+std::size_t(child)*52+4);if(pointer==0xffffffffu)return 0;
            const auto mdl=v.relative(base,pointer,24);const auto plist=v.u32(mdl+4);if(plist==0xffffffffu)return 0;
            const auto at=v.relative(base,plist,2);
            return recvx_en53_change_ambient(stream.data()+at,stream.size()-at,value);};
        t.model_root=[](RECVX_ENEMY_WORK* w,uint32_t model)->int{auto& e=owner_of(w);return morph_variant(e,model)?1:0;};
        t.morph_pair=[](RECVX_ENEMY_WORK* w,uint32_t a,uint32_t b)->int{morph_pair_set(w,a,b);return 1;};
        t.morph_indices=[](const RECVX_ENEMY_WORK* w,uint32_t* a,uint32_t* b)->int{const auto& e=owner_of(w);*a=e.morph_obj_a;*b=e.morph_obj_b;return 1;};
        t.work_texture=[](const RECVX_ENEMY_WORK* w)->const void*{const auto& e=owner_of(w);
            for(const auto& m:enemy_world->models_)if(m.actor.get()==e.enemy_asset)return &m.object.textures;
            return nullptr;};
        return t;}();
    recvx_en53_bind(&en53);
    // --- EN02 (en02.h, Gulp Worm): Blattzugriffe auf vorhandene Besitzer ----------------
    static const RECVX_EN02_SERVICES en02=[]{RECVX_EN02_SERVICES t{};
        // eneset.c:170 bhSetEnemy(&child) und epp->mlwP = &epw->mdl[base] (en02.c:777-803).
        t.create_child=[](RECVX_ENEMY_WORK* parent,const RECVX_ENEMY_TEMPLATE* tmpl,uint32_t base)->RECVX_ENEMY_WORK*{
            auto* c=enemy_world->create_enemy_child(parent,*tmpl,base,nullptr);return c?&c->enemy:nullptr;};
        // en02.c:3160 FallingRock: epp->mlwP = &epw->mdl[k] an einem bestehenden Kind — derselbe
        // Modellreihen-Anschluss wie beim Anlegen (create_enemy_child), nur spaeter.
        t.child_model=[](RECVX_ENEMY_WORK* child,RECVX_ENEMY_WORK* parent,uint32_t base)->int{
            auto& c=owner_of(child);auto& p=tree_of(owner_of(parent));if(!p.model_row())return 0;
            c.model_owner=nullptr;c.model_bank=p.model_row();c.model_base=base;
            const PlayerAsset* asset=morph_variant(c,0);
            c.model_absent=!asset&&base>=p.model_row()->model_count();
            if(!asset){c.enemy_asset=nullptr;c.enemy_obj.clear();c.enemy_evalflags.clear();c.enemy_owp_flg.clear();c.enemy_owp_mtx.clear();c.enemy_calculated=false;return c.model_absent?1:0;}
            if(c.enemy_asset!=asset){c.enemy_asset=asset;c.enemy_obj=asset->bind_pose();c.enemy_evalflags=asset->node_evalflags();c.enemy_owp_flg.assign(c.enemy_obj.size(),0u);c.enemy_owp_mtx.assign(c.enemy_obj.size(),BoneMatrix{});c.enemy_calculated=false;}
            return 1;};
        // obj_a = mdl[base+a].objP, obj_b = mdl[base+b].objP (en02.c:781-782), relativ zur Reihe des Kindes.
        t.morph_sources=[](RECVX_ENEMY_WORK* w,uint32_t a,uint32_t b){morph_pair_set(w,a,b);};
        t.game_mode=[](int* out)->int{*out=int(enemy_world->enemy_game_mode_);return 1;};
        t.room_flags=en23.room_flags;
        t.camera_offset=en13.camera_offset;
        t.player_library=en13.player_library;
        // plp->mdflg &= ~0x1 (en02.c:844): Bit 0x1 ist die Zeichensperre des Spielers (game.c:361),
        // nativ RoomWorld::player_hidden. Andere Bits haben keinen Besitzer.
        t.player_mdflg_clear=[](uint32_t bits)->int{auto* world=enemy_world;if(bits&~1u)return 0;if(bits&1u)world->player_hidden=false;return 1;};
        // hitchk.c:5474 bhCheckFloorEnemy: erste Bodenflaeche type 2 (rom->flrp, dann sys->mflrp);
        // `index` ist die Listenposition — das Original vergleicht den Zeiger (en02.c:3108).
        t.floor_enemy=[](int flr_no,float px,float pz,RECVX_ENEMY_ATR* out,int32_t* index)->int{auto* world=enemy_world;int32_t i=0;
            for(const auto* table:{&world->room_floors_,&world->effect_water_mobile_})for(const auto& a:*table){
                if((a.flags&1u)&&a.type==2&&a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz&&a.floor==flr_no){if(out)*out=atr_of(a);if(index)*index=i;return 1;}
                ++i;}
            return 0;};
        // zonzon1.c:852 bhEne_SetSEPan: RequestEnemySeBasic(sys->enow, pos, se, 0, 0).
        t.se_pan=[](uint32_t enemy,const RECVX_ENEMY_VEC* p,int se){enemy_world->audio_.request_enemy_se(enemy,{p->x,p->y,p->z},se,0,0);};
        t.add_speed=[](RECVX_ENEMY_WORK* w,int mode){en01_services().add_speed(w,mode);};
        // effect.c:436 bhSetEffectTb(&sys->ef, NULL, NULL, 0): nur die maskierten Felder der
        // persistenten Vorlage schreiben (en02.c:2504-2518 / :3010-3016), dann anlegen.
        t.effect_tb=[](uint32_t mask,const RECVX_EN02_EFF* v)->int{auto& ef=effect_detail::request_template();
            if(mask&RECVX_EN02_EF_ID)ef.id=v->id;if(mask&RECVX_EN02_EF_TYPE)ef.type=v->type;if(mask&RECVX_EN02_EF_FLG)ef.flg=v->flg;
            if(mask&RECVX_EN02_EF_MDLVER)ef.mdlver=v->mdlver;if(mask&RECVX_EN02_EF_POS)ef.position={v->pos.x,v->pos.y,v->pos.z};
            if(mask&RECVX_EN02_EF_SCALE)ef.scale={v->sx,v->sy,v->sz};
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;
            spawn.mdlver=ef.mdlver;spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;
            return enemy_world->spawn_effect(spawn);};
        t.effect_set=[](int slot,uint32_t mask,const RECVX_EN02_EFFSET* v){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot] ausserhalb des Pools (en02)");return;}
            auto& s=world->sprites_[std::size_t(slot)].sprite;
            if(mask&RECVX_EN02_EFS_VEC){s.xn=v->xn;s.yn=v->yn;s.zn=v->zn;}
            if(mask&RECVX_EN02_EFS_SCALE){s.sx=v->sx;s.sy=v->sy;s.sz=v->sz;}
            if(mask&RECVX_EN02_EFS_SCALEB){s.sxb=v->sxb;s.syb=v->syb;s.szb=v->szb;}};
        // hitchk.c:12 bhCheckWall auf dem Stapel-Dummy von bhEne02_DD00 (en02.c:1946-1962): flg 0x19,
        // stflg 0, ar 20, ah 1. dmy_ene.flr_no ist im Original uninitialisiert und wird nur fuer
        // Waende mit attr 0x1 gelesen (hitchk.c:87). Ohne solche Wand im Raum ist das Ergebnis davon
        // unabhaengig und die Probe laeuft wie die des Works; sonst wird die Luecke gemeldet. Nach QUALITY.md
        // Bedingung 3 unerreichbar: RM_3091/5830/7220/7230/7231 (alle Gulp-Worm-Raeume) haben keine
        // attr-0x1-Wand (gemessen 2026-10-23).
        t.wall_probe=[](uint32_t flg,uint32_t* stflg,RECVX_ENEMY_VEC* pos,float ar,float ah)->int{auto* world=enemy_world;
            auto walls=world->collision_geometry();
            for(const auto& a:walls)if((a.flags&1u)&&(a.attributes&1u)){world->unbound_enemy_.insert("bhCheckWall(&dmy_ene): dmy_ene.flr_no uninitialisiert, Raum hat attr-0x1-Waende (en02.c:1915)");return 0;}
            MotionActor player=world->motion_actor();
            unsigned f=flg,f2=0u;int floor_no=0,push_ct=0,push_idx=0;unsigned floor_snd=0,foot=0;std::optional<std::size_t> step;
            MotionActor actor{f,*stflg,f2,player.control,player.system,player.mode,floor_no,push_ct,push_idx,floor_snd,foot,step,0,false,player.effect29,&player.flags};
            auto steps=world->motion_steps();auto floors=world->motion_floors();
            MotionTerrain terrain{walls,steps,floors,world->floors_,world->room_height_};
            NJS_POINT3 p{pos->x,pos->y,pos->z};
            (void)resolve_motion_wall_direct(terrain,actor,p,ar,ah);
            pos->x=p.x;pos->y=p.y;pos->z=p.z;
            return 1;};
        return t;}();
    recvx_en02_bind(&en02);
    // --- EN05 (en05.h): Blattzugriffe auf vorhandene Besitzer ---------------------
    static const RECVX_EN05_SERVICES en05=[]{RECVX_EN05_SERVICES t{};
        t.create_child=en23.create_child;   // bhSetEnemy(&ene05_child): mlwP bleibt NULL
        t.route_points=en23.route_points;
        t.game_mode=en23.game_mode;
        // pwksub.c:664 bhSearchPlayer(pp, r) == bhSearchPlayer2 mit pp-Lage und pp->ay.
        t.search_player=[](const RECVX_ENEMY_WORK* w,int32_t r,int32_t* out)->int{const RECVX_ENEMY_VEC p{w->px,w->py,w->pz};return en01_services().search_player(w,&p,w->ay,r,out);};
        t.add_speed=[](RECVX_ENEMY_WORK* w,int r){en01_services().add_speed(w,r);};
        t.fix_offset=[](RECVX_ENEMY_WORK* w,const int8_t* chain,RECVX_ENEMY_VEC* out)->int{en01_services().calc_fix_offset(w,chain,nullptr,out);return 1;};
        t.check_water=[](const RECVX_ENEMY_VEC* p)->int{float top;return enemy_world->water_top({p->x,p->y,p->z},top)?1:0;};
        // pwksub.c:2572 bhSetWaterSplash2: Boden unter (pos.x, pp->py, pos.z), Wasserflaeche,
        // dann drei Effekte id 11 aus der gemeinsamen Vorlage sys->ef (lkono 0/1/2).
        t.water_splash=[](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_VEC* pos,int type,float sx,float sy,float sz){enemy_world->water_splash2(w->py,w->ay,{pos->x,pos->y,pos->z},type,sx,sy,sz);};
        // sys->ef: nur die genannten Felder, der Rest der gemeinsamen Vorlage bleibt (en05.c:5532/5578).
        t.effect_tb=[](uint32_t id,uint32_t type,uint32_t flg,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* scale,int lkono,int set_type)->int{
            auto& ef=effect_detail::request_template();ef.id=id;ef.flg=flg;if(set_type)ef.type=type;ef.position={pos->x,pos->y,pos->z};ef.scale={scale->x,scale->y,scale->z};
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
            spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=lkono;return enemy_world->spawn_effect(spawn);};
        t.effect_own_texture=[](int slot,const RECVX_ENEMY_WORK* w,int tex_id){auto* world=enemy_world;RECVX_T3_EFFPATCH p{};
            p.fields=RECVX_T3_EFF_STFLG_OR|RECVX_T3_EFF_TEXP|RECVX_T3_EFF_TEX_ID;p.stflg_or=0x20;p.texp=world->enemy_ext_.model_texp(w);p.tex_id=tex_id;world->enemy_ext_.effect_patch(slot,&p);};
        t.material_color=en23.material_color;
        // rom->etcp[index].attr (nur der Raumteil; en05.c:1141/2163/2257).
        t.etc_attr=[](uint32_t index,uint32_t set,uint32_t clear,uint32_t* attr)->int{auto* world=enemy_world;auto* ev=world->collision_events_;
            if(!ev||index>=ev->interactions.size())return 0;auto& a=ev->interactions[index].attribute;*attr=a;a|=set;a&=~clear;return 1;};
        t.player_owp_clear=[](uint32_t node,uint32_t bits){auto* world=enemy_world;
            if(node>=world->player_owp_flags_.size()){world->unbound_enemy_.insert("EN05 plp->owP[n].flg");return;}world->player_owp_flags_[node]&=~bits;};
        // plp->txp[2] hat keinen nativen Zeichenbesitzer: benannte Luecke statt Ersatztextur.
        t.player_texture=[](uint32_t)->int{return 0;};
        t.player_poison=[](){if(enemy_world->enemy_attacks_)enemy_world->player_poison_request_=true;}; // Host-Schalter Gegnerangriffe
        t.player_dmlvl=[](int32_t* out)->int{if(!enemy_world->player_dmlvl_bound_)return 0;*out=enemy_world->player_dmlvl_;return 1;};
        t.node_angles=[](RECVX_ENEMY_WORK* w,uint32_t node,int32_t* a,int write)->int{auto& tr=tree_of(owner_of(w));node+=tr.model_window;
            if(node>=tr.enemy_obj.size())return 0;for(unsigned k=0;k<3;++k){if(write)tr.enemy_obj[node].angles[k]=a[k];else a[k]=tr.enemy_obj[node].angles[k];}return 1;};
        t.owp_flags=[](RECVX_ENEMY_WORK* w,uint32_t node,uint32_t and_flg,uint32_t or_flg)->int{auto& tr=tree_of(owner_of(w));node+=tr.model_window;
            if(node>=tr.enemy_owp_flg.size())return 0;tr.enemy_owp_flg[node]=(tr.enemy_owp_flg[node]&and_flg)|or_flg;return 1;};
        t.room_height=[]()->float{return enemy_world->room_height_;};
        return t;}();
    recvx_en05_bind(&en05);
    // --- EN19 (en19.h): Blattzugriffe auf vorhandene Besitzer ---------------------
    static const RECVX_EN19_SERVICES en19=[]{RECVX_EN19_SERVICES t{};
        t.game_mode=en23.game_mode;
        // sys->gm_flg: runtime.cpp fuehrt es als events->flags[11][0] (world.cpp:2351).
        t.game_flags=[](uint32_t* out)->int{auto* world=enemy_world;if(!world->collision_events_)return 0;*out=world->collision_events_->flags[11][0];return 1;};
        t.room_flags=en23.room_flags;
        t.node_local=en01_services().node_local;
        t.node_local_set=en01_services().node_local_set;
        t.node_angle_set=en01_services().node_angle_set;
        t.owp_flags=en05.owp_flags;
        // Motion.c / en19.c:3809: mnwP[mtn_no].atrP[frm_no >> 16]; atrP == NULL -> 0.
        t.motion_attribute=[](const RECVX_ENEMY_WORK* w,uint32_t mtn,uint32_t frame,int32_t* out)->int{
            const auto* clip=enemy_world->enemy_mn_work(owner_of(w),mtn);if(!clip)return 0;
            if(clip->attributes.empty()){*out=0;return 1;}
            if(frame>=clip->attributes.size())return 0;
            *out=int32_t(clip->attributes[frame]);return 1;};
        t.fix_offset=en05.fix_offset;
        t.floor_enemy=en02.floor_enemy;
        t.check_route=en01_services().route_next;
        t.route_id=en01_services().route_id;
        t.wall_ref_angle=[](RECVX_ENEMY_VEC* pos,uint32_t flg,float ar,float ah,int32_t* ay)->int{return en01_services().wall_ref_angle(pos,flg,ar,ah,ay,nullptr);};
        // hitchk.c:12 bhCheckWall(plp) mit plp->ah = 1.0f (en19.c:494-501): derselbe Weg wie
        // en12.player_wall (en12.c:1765), nur mit den uebergebenen ar/ah des Spielerworks.
        t.player_wall=[](RECVX_ENEMY_PLAYER* p,float ar,float ah)->int{auto* world=enemy_world;
            MotionActor actor=world->motion_actor();
            auto walls=world->collision_geometry();auto steps=world->motion_steps();auto floors=world->motion_floors();
            MotionTerrain terrain{walls,steps,floors,world->floors_,world->room_height_};
            NJS_POINT3 q{p->pos.x,p->pos.y,p->pos.z};
            const bool hit=resolve_motion_wall_direct(terrain,actor,q,ar,ah);
            p->pos={q.x,q.y,q.z};return hit?1:0;};
        t.player_exp0_float_set=[](uint32_t offset,float v){enemy_world->player_exp0_writes_.push_back({offset,v});};
        t.player_library=en13.player_library;
        // effect.c:436 bhSetEffectTb(&sys->ef, ofs, link, joint): nur die maskierten Felder der
        // persistenten Vorlage schreiben (sz bleibt in PlyDmg042 vom Vorgaenger, en19.c:4095), dann anlegen.
        t.effect_tb=[](uint32_t mask,const RECVX_EN19_EF* v,const RECVX_ENEMY_VEC* ofs,RECVX_ENEMY_WORK* link,int joint)->int{auto* world=enemy_world;auto& ef=effect_detail::request_template();
            if(mask&RECVX_EN19_EF_FLG)ef.flg=v->flg;if(mask&RECVX_EN19_EF_ID)ef.id=v->id;if(mask&RECVX_EN19_EF_TYPE)ef.type=v->type;
            if(mask&RECVX_EN19_EF_MDLVER)ef.mdlver=v->mdlver;if(mask&RECVX_EN19_EF_FLR)ef.flr_no=v->flr_no;
            if(mask&RECVX_EN19_EF_POS)ef.position={v->pos.x,v->pos.y,v->pos.z};
            if(mask&RECVX_EN19_EF_SX)ef.scale.x=v->sx;if(mask&RECVX_EN19_EF_SY)ef.scale.y=v->sy;if(mask&RECVX_EN19_EF_SZ)ef.scale.z=v->sz;
            if(mask&RECVX_EN19_EF_AX)ef.ax=v->ax;if(mask&RECVX_EN19_EF_AY)ef.ay=v->ay;
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
            spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=joint;
            if(ofs){spawn.has_offset=true;spawn.offset={ofs->x,ofs->y,ofs->z};}
            unsigned kind=0,index=0;
            if(link){if(!world->enemy_work_index(link,index)){world->unbound_enemy_.insert("bhSetEffectTb: Linkwork ohne Raumfigur (en19)");return -1;}kind=2;spawn.linked_work=link;}
            return world->spawn_effect(spawn,kind,index);};
        // effsub3.c:1640 rySetShadow -> bhEff304 (eff[]-Pool, Modifier-Schatten ueber sys->efm[1]).
        t.shadow=[](RECVX_ENEMY_WORK* w,int obj0,int obj1,int obj2,float off_a,float off_b)->int{return enemy_world->ry_set_shadow(w,obj0,obj1,obj2,off_a,off_b);};
        // effsub3.c:2137 bhSetRapEff: AllocOworkOne = erster freier eff[]-Platz ab 0, genullt
        // (reserve_effect_slot(0)); die Prozedur schreibt flg 1/id und ihren free4-Satz selbst.
        // Rueckgabe ist der Platz (das Original gibt den OR_WORK* zurueck); -1 = Pool voll.
        t.cam_shake=[](const RECVX_EN19_PMA* prm)->int{auto* world=enemy_world;const int slot=world->reserve_effect_slot(0);if(slot<0)return -1;
            const EffectPma30a p{prm->type,prm->ang_fst,prm->add_ax,prm->x_rang,prm->x_rate,prm->add_ay,prm->y_rang,prm->y_rate};
            auto& work=world->sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
            if(!start_eff30a(work.sprite,p)){work.sprite.flg=0;return -1;}
            work.sprite.slot=slot;world->entity(4,unsigned(slot)).hidden=false;return slot;};
        // en19.c:250-261: bhSetRapEff(349) mit fnc_prcP = bhEne19_TyBloodSet und EB_WORK{ewP, fwP, time}
        // im free-Bereich; nativ liegt time im Zustand (eb_time) und das Argument ist das Work.
        t.blood_callback=[](RECVX_ENEMY_WORK* w)->int{auto* world=enemy_world;const int slot=world->reserve_effect_slot(0);if(slot<0)return 0;
            auto& work=world->sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
            if(!start_eff349(work.sprite,[](void* a){recvx_en19_ty_blood_set(static_cast<RECVX_ENEMY_WORK*>(a));},w)){work.sprite.flg=0;return 0;}
            work.sprite.slot=slot;world->entity(4,unsigned(slot)).hidden=false;return 1;};
        // effsub3.c:2430 bhSetEffSpark (eigene Eff308Prm[2], rom->lgtp[2]) auf einem frischen Platz.
        t.spark=[](const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* dir,uint32_t src,uint32_t dst,int typ)->int{auto* world=enemy_world;const int slot=world->reserve_effect_slot(0);if(slot<0)return -1;
            auto& work=world->sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
            if(!start_eff_spark(work.sprite,{pos->x,pos->y,pos->z},{dir->x,dir->y,dir->z},src,dst,typ)){work.sprite.flg=0;return -1;}
            work.sprite.slot=slot;world->entity(4,unsigned(slot)).hidden=false;return slot;};
        // effsub3.c:2748 rySetEffBlood / :3247 rySetEffBlood2: mtxP = owP[joint].mtx des Links (Spieler
        // kind 0, Gegner kind 1 mit Raumindex), je Pass lebend gelesen.
        t.blood=[](RECVX_T3_LINK lk,int joint,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* dir,int typ)->int{auto* world=enemy_world;
            unsigned kind=0,index=0;if(lk.w){if(!world->enemy_work_index(lk.w,index)){world->unbound_enemy_.insert("rySetEffBlood: Linkwork ohne Raumfigur");return -1;}kind=1;}
            const int slot=world->reserve_effect_slot(0);if(slot<0)return -1;
            auto& work=world->sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
            if(!start_eff309(work.sprite,kind,index,unsigned(joint),{pos->x,pos->y,pos->z},{dir->x,dir->y,dir->z},typ)){work.sprite.flg=0;return -1;}
            work.sprite.slot=slot;world->entity(4,unsigned(slot)).hidden=false;return slot;};
        t.blood2=[](RECVX_T3_LINK lk,int joint,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* dir,int typ)->int{auto* world=enemy_world;
            unsigned kind=0,index=0;if(lk.w){if(!world->enemy_work_index(lk.w,index)){world->unbound_enemy_.insert("rySetEffBlood2: Linkwork ohne Raumfigur");return -1;}kind=1;}
            const int slot=world->reserve_effect_slot(0);if(slot<0)return -1;
            auto& work=world->sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
            if(!start_eff30b(work.sprite,kind,index,unsigned(joint),{pos->x,pos->y,pos->z},{dir->x,dir->y,dir->z},typ)){work.sprite.flg=0;return -1;}
            work.sprite.slot=slot;world->entity(4,unsigned(slot)).hidden=false;return slot;};
        // en19.c:4780 bhEne19_SetLeftClaw: PRM_WRK{owP[base].mtx, texP, objP[obj].model, (-pos.x, pos.y, pos.z),
        // (ang.x, 32768 - ang.y, -ang.z)} aus dem mlwP->objP des Works; das Modell zeichnet der Raumbesitzer
        // (append_claw_model) aus dem Gegnerasset, der Satz haelt nur Verweise und die Matrix.
        t.left_claw=[](RECVX_ENEMY_WORK* w,uint32_t base,uint32_t obj)->int{auto* world=enemy_world;unsigned index=0;
            if(!world->enemy_work_index(w,index)){world->unbound_enemy_.insert("bhEne19_SetLeftClaw: Work ohne Raumfigur");return -1;}
            const auto& tree=tree_of(owner_of(w));const auto node=std::size_t(tree.model_window)+obj;
            if(node>=tree.enemy_obj.size()){world->unbound_enemy_.insert("bhEne19_SetLeftClaw: objP[obj] ausserhalb des Modells");return -1;}
            const auto& o=tree.enemy_obj[node];
            const float pos[3]={-o.position.x,o.position.y,o.position.z};const int ang[3]={o.angles[0],32768-o.angles[1],-o.angles[2]};
            const int slot=world->reserve_effect_slot(0);if(slot<0)return -1;
            auto& work=world->sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
            if(!start_eff30c(work.sprite,index,unsigned(tree.model_window)+base,unsigned(node),pos,ang)){work.sprite.flg=0;return -1;}
            work.sprite.slot=slot;world->entity(4,unsigned(slot)).hidden=false;return slot;};
        // fwP->clw0P->mtxP (en19.c:2331/2455): R0_WK.mtxP des 312-Satzes, solange er lebt.
        t.claw_matrix=[](int handle,float* out16)->int{auto* world=enemy_world;if(handle<0||std::size_t(handle)>=world->sprites_.size())return 0;
            const auto& s=world->sprites_[std::size_t(handle)].sprite;if(!(s.flg&1u)||s.id!=312||!s.claw30c)return 0;
            std::copy_n(&s.claw30c->mtx[0],16,out16);return 1;};
        // en19.c:4801 bhEne19_SetClawPlane: EFFPRM_WORK.mtxP lebend — der 312-Satz (claw_slot) oder owP[joint] des Works.
        t.claw_plane=[](RECVX_ENEMY_WORK* w,int claw_slot,uint32_t joint,int col,int tim,float src_x,float dst_x)->int{auto* world=enemy_world;
            EffectMatrixRef ref;
            if(claw_slot>=0){ref.kind=1;ref.slot=claw_slot;}
            else{unsigned index=0;if(!world->enemy_work_index(w,index)){world->unbound_enemy_.insert("bhEne19_SetClawPlane: Work ohne Raumfigur");return -1;}
                ref.kind=0;ref.link_kind=1;ref.link_index=index;ref.node=unsigned(tree_of(owner_of(w)).model_window)+joint;}
            const int slot=world->reserve_effect_slot(0);if(slot<0)return -1;
            auto& work=world->sprites_[std::size_t(slot)];work.dynamic=true;work.queued=false;work.scale={1,1,1};
            if(!start_eff307(work.sprite,ref,col,tim,src_x,dst_x)){work.sprite.flg=0;return -1;}
            // en19.c:4816/4818: texP = ewP->mlwP->texP, tex_id 4 — Textur Nr. 4 der Gegner-Texturliste,
            // nicht sys->ef_tn[4]. Der Zeichenpass loest die Kategorie 0x80000000|Raumindex auf diese Liste auf.
            {unsigned owner_index=0;if(!world->enemy_work_index(w,owner_index)){world->unbound_enemy_.insert("bhEne19_SetClawPlane: Work ohne Raumfigur");work.sprite.flg=0;return -1;}
             work.sprite.plane307->tex_category=0x80000000u|owner_index;work.sprite.plane307->tex_frame=4;}
            work.sprite.slot=slot;world->entity(4,unsigned(slot)).hidden=false;return slot;};
        return t;}();
    recvx_en19_bind(&en19);
    // Dieselbe Dienststruktur braucht der Effektpass fuer den bhEff349-Rueckruf
    // (RoomWorld::EnemyCallbackFrame). Sie hat statische Lebensdauer, der Zeiger bleibt gueltig.
    en19_effect_services=&en19;
    // --- EN29 (en29.h): Tentakel, dieselben Besitzer wie EN19/EN23 ----------------
    static const RECVX_EN29_SERVICES en29=[]{RECVX_EN29_SERVICES t{};
        t.room_flags=en23.room_flags;
        t.shadow=en19.shadow;
        // en29.c:294-298 oP->mdlver = 1, oP->type = 1, oP->sy = 6.0f auf dem rySetShadow-Platz.
        t.shadow_fields=[](int slot,uint32_t mdlver,uint32_t type,float sy){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot] ausserhalb des Pools (en29)");return;}
            auto& o=world->sprites_[std::size_t(slot)].sprite;o.mdlver=mdlver;o.type=type;o.sy=sy;};
        // en29.c:316/382 fwP->sdw_owkP->stflg Bit 0x1000000 (bhControlEffect ueberspringt den Satz, effect.c:776).
        t.shadow_hidden=[](int slot,int hidden){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot] ausserhalb des Pools (en29)");return;}
            auto& o=world->sprites_[std::size_t(slot)].sprite;if(hidden)o.stflg|=0x1000000u;else o.stflg&=~0x1000000u;};
        t.create_child=en23.create_child;   // bhSetEnemy(&En29DmgDat): mlwP bleibt NULL
        // en29.c:1750 enP->mlwP = ewP->mlwP: das Kapselkind liest dieselben owP wie der Koerper.
        t.share_model=[](RECVX_ENEMY_WORK* child,const RECVX_ENEMY_WORK* parent)->int{auto& c=owner_of(child);auto& owner=enemy_world->tree_owner(owner_of(const_cast<RECVX_ENEMY_WORK*>(parent)));
            if(&c==&owner)return 0;c.model_owner=&owner;c.model_bank=owner.model_bank;c.model_base=owner.model_base;c.model_absent=false;return 1;};
        t.effect_tb=en19.effect_tb;t.blood=en19.blood;t.blood2=en19.blood2;
        t.player_library=en19.player_library;
        return t;}();
    recvx_en29_bind(&en29);
    // --- EN26 (en26.h): Anatomist Zombie; Effektvorlage, Effektslot und Objektfenster ----
    static const RECVX_EN26_SERVICES en26=[]{RECVX_EN26_SERVICES t{};
        // effect.c:436 bhSetEffectTb(&sys->ef, ofs, epw, obj): bhEne26_LinkFireEffect beschreibt
        // nur flg/id/type/sz/px/py/pz/ay (en26.c:816-823) und sx/sy (:835) der persistenten
        // Vorlage; mdlver, flr_no und ax bleiben vom letzten Schreiber.
        t.effect_tb=[](const RECVX_EN26_EF* v,const RECVX_ENEMY_VEC* ofs,RECVX_ENEMY_WORK* link,int joint)->int{auto* world=enemy_world;auto& ef=effect_detail::request_template();
            ef.flg=v->flg;ef.id=unsigned(v->id);ef.type=unsigned(v->type);ef.position={v->px,v->py,v->pz};ef.scale={v->sx,v->sy,v->sz};ef.ay=v->ay;
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
            spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=joint;
            if(ofs){spawn.has_offset=true;spawn.offset={ofs->x,ofs->y,ofs->z};}
            unsigned kind=0,index=0;
            if(link){if(!world->enemy_work_index(link,index)){world->unbound_enemy_.insert("bhSetEffectTb: Linkwork ohne Raumfigur (en26)");return -1;}kind=2;spawn.linked_work=link;}
            return world->spawn_effect(spawn,kind,index);};
        // eff[eno].zn / eff[eno].ct3 (en26.c:840-841, :847-848).
        t.effect_zn_ct3=[](int slot,float zn,int32_t ct3){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot] ausserhalb des Pools (en26)");return;}
            auto& s=world->sprites_[std::size_t(slot)].sprite;s.zn=zn;s.ct3=ct3;};
        // objP += n, owP += n, obj_num -= n am ML_WORK des Works (en26.c:1180-1190).
        t.shift_object_window=[](RECVX_ENEMY_WORK* w,int32_t nodes)->int{auto& tr=tree_of(owner_of(w));
            if(nodes>=0){tr.model_window+=unsigned(nodes);return 1;}
            if(tr.model_window>=unsigned(-nodes)){tr.model_window-=unsigned(-nodes);return 1;}
            enemy_world->unbound_enemy_.insert("objP -= n ohne vorheriges += n (en26)");return 0;};
        return t;}();
    recvx_en26_bind(&en26);
    // --- EN22 (en22.h): Albinoid Adult; Blitzeffekte, Lichtplatz 2, Blitzmodell-Materialien ----
    static const RECVX_EN22_SERVICES en22=[]{RECVX_EN22_SERVICES t{};
        // effect.c:436 bhSetEffectTb(&sys->ef, ofs, link, lkono): en22.c beschreibt flg/id/type/px..pz/
        // sx..sz/ay (:2617-2633, :2663-2675, :2833-2842), ax und mdlver nur in SetDengekiEffect2.
        t.effect_tb=[](const RECVX_EN22_EF* v,const RECVX_ENEMY_VEC* ofs,RECVX_ENEMY_WORK* link,int link_player,int lkono)->int{auto* world=enemy_world;auto& ef=effect_detail::request_template();
            ef.flg=v->flg;ef.id=unsigned(v->id);ef.type=unsigned(v->type);ef.position={v->px,v->py,v->pz};ef.scale={v->sx,v->sy,v->sz};ef.ay=v->ay;
            if(v->mask&RECVX_EN22_EF_AX)ef.ax=v->ax;if(v->mask&RECVX_EN22_EF_MDLVER)ef.mdlver=v->mdlver;
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
            spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=lkono;
            if(ofs){spawn.has_offset=true;spawn.offset={ofs->x,ofs->y,ofs->z};}
            if(link_player)return world->spawn_effect(spawn,1,0); // lkwkp = plp (en22.c:2830)
            unsigned index=0;
            if(link){if(!world->enemy_work_index(link,index)){world->unbound_enemy_.insert("bhSetEffectTb: Linkwork ohne Raumfigur (en22)");return -1;}spawn.linked_work=link;return world->spawn_effect(spawn,2,index);}
            return world->spawn_effect(spawn);};
        // eff[eno]-Nachfelder (en22.c:2637-2641, :2680-2688, :2814-2816, :2846-2849). TEX vor MODEL:
        // SetDengekiEffect2 setzt txp[0] = mlwP->texP des gewaehlten mdl.
        t.effect_slot=[](int slot,uint32_t mask,const RECVX_EN22_EFSLOT* in){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()||!in){world->unbound_enemy_.insert("eff[slot] ausserhalb des Pools (en22)");return;}
            auto& work=world->sprites_[std::size_t(slot)];auto& s=work.sprite;
            if(mask&RECVX_EN22_EFS_STFLG)s.stflg|=in->stflg_or;
            if(mask&RECVX_EN22_EFS_TEX){s.tex_id=in->tex_id;
                if(in->tex_from){unsigned index=0;if(!world->enemy_work_index(in->tex_from,index)){world->unbound_enemy_.insert("txp[0] = epw->mlwP->texP: Work ohne Raumfigur (en22)");return;}
                    work.texture_list=static_cast<const std::vector<SceneTexture>*>(world->enemy_ext_.model_texp(in->tex_from));s.txp_category=0x80000000u|index;}}
            if(mask&RECVX_EN22_EFS_CT3)s.ct3=in->ct3;
            if(mask&RECVX_EN22_EFS_ZN)s.zn=in->zn;
            if(mask&RECVX_EN22_EFS_ANG){s.ax=in->ax;s.ay=in->ay;s.az=in->az;}
            if(mask&RECVX_EN22_EFS_FLR)s.flr_no=in->flr_no;
            if(mask&RECVX_EN22_EFS_MODEL){unsigned index=0;
                if(!in->model_of||!world->enemy_work_index(in->model_of,index)){world->unbound_enemy_.insert("mlwP = &epw->mdl[n]: Work ohne Raumfigur (en22)");return;}
                s.model_enemy=int(index);s.model_mdl=unsigned(in->model);
                work.texture_list=static_cast<const std::vector<SceneTexture>*>(world->enemy_ext_.model_texp_index(in->model_of,in->model));}};
        // en22.c:661/:1715: op->flg != 0 && op->id == 353 && op->lkwkp == epw -> op->mode0 = 4.
        t.effect_light_off=[](int slot,RECVX_ENEMY_WORK* w){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot] ausserhalb des Pools (en22)");return;}
            auto& s=world->sprites_[std::size_t(slot)].sprite;if(s.flg!=0&&s.id==353&&s.lkwkp==w)s.mode0=4;};
        // rom->lgtp[2]: LGT_WORK flg/type/aspd/lkflg/lkno/lkono/lsrc, p/l, rgb, nr/fr, mode/ct0/ct1.
        t.light_get=[](RECVX_EN22_LIGHT* out)->int{auto* world=enemy_world;if(world->lights_.size()<3)return 0;const auto& l=world->lights_[2];
            out->flg=l.flags;out->aspd=int32_t(l.speed);out->lsrc=int32_t(l.source);out->type=int32_t(l.type);
            out->r=l.color.x;out->g=l.color.y;out->b=l.color.z;out->nr=l.near_range;out->fr=l.far_range;
            out->lkflg=int32_t(l.link);out->lkno=int32_t(l.index);out->lkono=int32_t(l.node);
            out->lx=l.offset.x;out->ly=l.offset.y;out->lz=l.offset.z;out->px=l.position.x;out->py=l.position.y;out->pz=l.position.z;
            out->mode=int32_t(l.mode);out->ct0=int32_t(l.clock);out->ct1=l.ct1;return 1;};
        t.light_set=[](const RECVX_EN22_LIGHT* in){auto* world=enemy_world;if(world->lights_.size()<3){world->unbound_enemy_.insert("rom->lgtp[2] fehlt (en22)");return;}auto& l=world->lights_[2];
            l.flags=in->flg;l.speed=unsigned(in->aspd);l.source=unsigned(in->lsrc);l.type=unsigned(in->type);
            l.color={in->r,in->g,in->b};l.near_range=in->nr;l.far_range=in->fr;
            l.link=unsigned(in->lkflg);l.index=unsigned(in->lkno);l.node=unsigned(in->lkono);
            l.offset={in->lx,in->ly,in->lz};l.position={in->px,in->py,in->pz};
            l.mode=unsigned(in->mode);l.clock=unsigned(in->ct0);l.ct1=in->ct1;l.color_sampled=false;};
        // epw->mdl[mdl].objP[obj].model->plist im beschreibbaren Strom des Works (wie en53 child_ambient).
        t.model_plist=[](RECVX_ENEMY_WORK* w,int mdl,int obj,uint8_t** plist,size_t* size)->int{
            if(mdl<0||obj<0)return 0;auto& e=tree_of(owner_of(w));const auto* asset=morph_variant(e,unsigned(mdl));
            if(!asset||unsigned(obj)>=asset->bones())return 0;
            auto& stream=mdl?e.live_models[unsigned(mdl)]:e.live_model;
            if(stream.empty())stream=asset->live_stream();
            BinaryView v(stream);const auto base=v.u16(4);
            const auto objects=v.relative(base,v.u32(12),asset->bones()*52);
            const auto pointer=v.u32(objects+std::size_t(obj)*52+4);if(pointer==0xffffffffu)return 0;
            const auto model=v.relative(base,pointer,24);const auto pl=v.u32(model+4);if(pl==0xffffffffu)return 0;
            const auto at=v.relative(base,pl,2);
            *plist=stream.data()+at;*size=stream.size()-at;return 1;};
        // pwksub.c:2572 bhSetWaterSplash2(plp, pos, ...) (en22.c:2127): py/ay des Spielers.
        t.player_water_splash=[](const RECVX_ENEMY_VEC* pos,int type,float sx,float sy,float sz){auto* world=enemy_world;
            RECVX_ENEMY_PLAYER p;if(!world->enemy_player_get(p)){world->unbound_enemy_.insert("bhSetWaterSplash2(plp): Spielerbesitzer nicht gebunden");return;}
            world->water_splash2(p.pos.y,p.ay,{pos->x,pos->y,pos->z},type,sx,sy,sz);};
        return t;}();
    recvx_en22_bind(&en22);
    // --- EN17 (en17.h): Monster Steve und seine zwei Kettenwerke (Arme) -----------------
    static const RECVX_EN17_SERVICES en17=[]{RECVX_EN17_SERVICES t{};
        // bhEne17_SetLinkWork (en17.c:879): bhSetEnemy(flg 0x80A1, id) mit lkwkp/lkono und mlwP = &epw->mdl[mdl_no].
        t.create_child=[](RECVX_ENEMY_WORK* parent,uint32_t lnk_obj,uint32_t mdl_no,uint32_t id)->RECVX_ENEMY_WORK*{
            RECVX_ENEMY_TEMPLATE tmpl{};tmpl.flg=0x80A1u;tmpl.id=uint16_t(id);
            auto* c=enemy_world->create_enemy_child(parent,tmpl,mdl_no,nullptr);if(!c)return nullptr;
            auto& enemy=c->enemy;enemy.lkwkp=parent;enemy.lkono=int32_t(lnk_obj);enemy.lox=enemy.loy=enemy.loz=0.0f;
            c->motion_library=owner_of(parent).motion_library>=0?owner_of(parent).motion_library:int(owner_of(parent).identity);
            enemy.mtn_room=parent->mtn_room;return &enemy;};
        t.set_call_func=[](uint32_t id,RECVX_ENEMY_PROC proc){recvx_enemy_set_call_func(&enemy_world->enemy_dispatch_,id,proc);};
        // en17.c:861-863: mlwP->objP = *mbp, obj_a = mbp[0], obj_b = mbp[1].
        t.select_morph_pair=[](RECVX_ENEMY_WORK* w){morph_pair_set(w,0,1);};
        t.check_route=[](const RECVX_ENEMY_VEC* from,const RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* out)->int{return en01_services().route_next(from,to,out);};
        t.calc_fix_offset=en01_services().calc_fix_offset;
        // npCollisionCheckCC (njplus.c) ueber denselben Kampfanbieter wie npCollisionCheckSC.
        t.capsule_capsule=[](const RECVX_ENEMY_CAPSULE* a,const RECVX_ENEMY_CAPSULE* b)->int{
            const RECVX_CB_CAPSULE ca{{a->c1.x,a->c1.y,a->c1.z},{a->c2.x,a->c2.y,a->c2.z},a->r};
            const RECVX_CB_CAPSULE cb{{b->c1.x,b->c1.y,b->c1.z},{b->c2.x,b->c2.y,b->c2.z},b->r};
            return recvx_cb_check_cc(&ca,&cb);};
        // en17.c:1676 bhEne17_CameraControl schreibt nur cam.ofy; ofx/ofz bleiben vom letzten Schreiber stehen.
        t.camera_ofy=[](float y){auto* world=enemy_world;if(!world->camera_offset_){world->unbound_enemy_.insert("cam.ofx/ofy/ofz ohne Kamerabesitzer");return;}world->camera_offset_->y=y;};
        t.camera_clear=[](){auto* world=enemy_world;if(!world->camera_offset_){world->unbound_enemy_.insert("cam.ofx/ofy/ofz ohne Kamerabesitzer");return;}*world->camera_offset_={0.0f,0.0f,0.0f};};
        t.player_library=en13.player_library;
        // plp->axp / plp->spd: spd ist das Spielerwerk der nativen Aktion; axp haelt der Besitzer ausser dem Originalblock.
        t.player_extra_get=[](int32_t* axp,float* spd)->int{*axp=enemy_world->player_axp_;*spd=enemy_world->player_spd_;return 1;};
        t.player_extra_set=[](int32_t axp,float spd){enemy_world->player_axp_=axp;enemy_world->player_spd_=spd;};
        // effsub4.c:19/117/199 bhEff_SetPtcl/Ptcl2/Ptcl2V: die Partikelsaetze 365/366 am Effektpool
        // des Raumbesitzers (RoomWorld::set_ptcl/set_ptcl2/set_ptcl2v).
        t.eff_ptcl=[](RECVX_ENEMY_WORK* w,const RECVX_ENEMY_VEC* pos,int kind){enemy_world->set_ptcl(w,{pos->x,pos->y,pos->z},kind);};
        t.eff_ptcl2=[](RECVX_ENEMY_WORK*,const RECVX_ENEMY_VEC* pos,const float* mtx16){enemy_world->set_ptcl2({pos->x,pos->y,pos->z},mtx16);};
        t.eff_ptcl2v_player=[](const RECVX_ENEMY_VEC* pos,RECVX_ENEMY_VEC* v,int kind){
            NJS_POINT3 dv{v->x,v->y,v->z};enemy_world->set_ptcl2v({pos->x,pos->y,pos->z},&dv,kind);
            v->x=dv.x;v->y=dv.y;v->z=dv.z;}; // njUnitVector(dv) wirkt auf den Vektor des Aufrufers
        // en17.c:1689 bhEne17_AfterimageAxEffect: Effekt 364 mit mlwP = &epw->mdl[5].
        t.afterimage=[](RECVX_ENEMY_WORK* w,const float* mtx16,const RECVX_ENEMY_VEC* ofs,uint32_t argb){
            enemy_world->en17_afterimage(w,mtx16,{ofs->x,ofs->y,ofs->z},argb);};
        // en17.c:1852 bhEne17_SetLight: rom->lgtp[index]. Das Original schreibt flg/aspd/lsrc/type,
        // Farbe, nr/fr, light = NULL, lkflg/lkno/lkono, Lage und ct0/mode — NICHT lx/ly/lz und
        // nicht ct1; die bleiben am Lichtsatz stehen, wie ihn der letzte Schreiber hinterliess.
        t.set_light=[](uint32_t index,const RECVX_EN17_LIGHT* in){auto* world=enemy_world;
            if(world->lights_.size()<=std::size_t(index)){world->unbound_enemy_.insert("rom->lgtp["+std::to_string(index)+"] fehlt (en17)");return;}
            auto& l=world->lights_[std::size_t(index)];
            l.flags=unsigned(in->flg);l.speed=unsigned(in->aspd);l.source=unsigned(in->lsrc);l.type=unsigned(in->type);
            l.color={in->r,in->g,in->b};l.near_range=in->nr;l.far_range=in->fr;
            l.link=unsigned(in->lkflg);l.index=unsigned(in->lkno);l.node=unsigned(in->lkono);
            l.position={in->px,in->py,in->pz};
            l.mode=unsigned(in->mode);l.clock=unsigned(in->ct0);
            l.color_sampled=false;}; // lp->light = NULL
        // zonzon.c:466 bhEne_SetBlood2(plp, type, ofs, ry) am Spielerwork; djnt_no setzt en17.c.
        t.player_blood2=[](int type,RECVX_ENEMY_VEC* ofs,int ry){recvx_t3_set_blood2_player(type,ofs,ry);};
        // en17.c:655/1533 mlwP->objP->pos[0] = pos[2] = 0 am Gegner- bzw. Spielerbaum: die
        // Wurzelverschiebung der Motion wird in x/z verworfen (MtnTblPlay setzt sie selbst).
        t.model_root_zero_xz=[](RECVX_ENEMY_WORK* w){auto& e=tree_of(owner_of(w));
            if(e.enemy_obj.empty()){enemy_world->unbound_enemy_.insert("mlwP->objP[0] ohne Objektbaum (en17)");return;}
            e.enemy_obj[0].position.x=0.0f;e.enemy_obj[0].position.z=0.0f;};
        t.player_root_zero_xz=[](){auto* world=enemy_world;
            if(world->player_obj_.empty()){world->unbound_enemy_.insert("plp->mlwP->objP[0] ohne Objektbaum (en17)");return;}
            world->player_obj_[0].position.x=0.0f;world->player_obj_[0].position.z=0.0f;};
        return t;}();
    recvx_en17_bind(&en17);
    // --- EN03 (en03.h, Schwarze Witwe): Zustandsmaschine auf dem vorhandenen Besitzer --
    // Alle Dienste sind bereits an anderer Stelle gebundene Anbieter derselben Bedeutung;
    // kein neuer Ersatzpfad. create_child legt die 8 Beinkinder (mlwP NULL) und die 15
    // EN24-Kinder (mdl[1]) an.
    static const RECVX_EN03_ID_SERVICES en03id=[]{RECVX_EN03_ID_SERVICES t{};
        t.create_child=[](RECVX_ENEMY_WORK* parent,const RECVX_ENEMY_TEMPLATE* tmpl,uint32_t base)->RECVX_ENEMY_WORK*{
            auto* c=enemy_world->create_enemy_child(parent,*tmpl,base,nullptr);return c?&c->enemy:nullptr;};
        t.route_points=en23.route_points;
        t.game_mode=en23.game_mode;
        t.effect_id=en23.effect_id;
        t.effect_tb=en23.effect_tb;
        t.material_color=en23.material_color;
        t.player_library=en13.player_library;
        t.player_wall=en19.player_wall;
        t.effect_position=en23.effect_position;
        // plp->dvx/dvy/dvz: derselbe Abnehmer wie der Effektdienst h.player_dv.
        t.player_dv_set=[](const RECVX_ENEMY_VEC* dv){auto* world=enemy_world;world->player_hit_dv_={dv->x,dv->y,dv->z};};
        return t;}();
    recvx_en03_id_bind(&en03id);
    // --- EN06 (en06.h): Motte, Anfuehrer und Fluegelfragmente auf den EN03-Besitzerwegen ----
    static const RECVX_EN06_SERVICES en06=[]{RECVX_EN06_SERVICES t{};
        // eneset.c:170 ohne Modell (en06.c:337/351/1182): mlwP bleibt NULL; der Anfuehrer (id 6)
        // spielt ueber seine Identitaet dieselbe Bibliothek emtp[6] wie die Motte.
        t.create_child=[](RECVX_ENEMY_WORK* parent,const RECVX_ENEMY_TEMPLATE* tmpl)->RECVX_ENEMY_WORK*{
            auto* c=enemy_world->create_enemy_child(parent,*tmpl,~0u,nullptr);return c?&c->enemy:nullptr;};
        t.set_call_func=[](uint32_t id,RECVX_ENEMY_PROC proc){recvx_enemy_set_call_func(&enemy_world->enemy_dispatch_,id,proc);};
        t.model_flags=en03.model_flags;
        t.owp_flag_or=en03.owp_flag_or;
        t.owp_matrix=en03.owp_matrix;
        t.owp_matrix_set=en03.owp_matrix_set;
        // ((BH_PWORK*)EXP0_I(0x84))->sy: BH_PWORK.sy und O_WRK.sy liegen beide bei 0x3B4 (types.h:169/:330).
        t.effect_sy=[](int slot,float* sy,int write)->int{auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size())return 0;auto& w=world->sprites_[std::size_t(slot)];
            if(write){w.sprite.sy=*sy;w.scale.y=*sy;}else *sy=w.sprite.sy;return 1;};
        // en06.c:2183-2194: id/flg/type/sx/sy/sz/ay/px/pz/py der persistenten Vorlage; mdlver/flr_no/ax bleiben.
        t.effect_tb=[](uint32_t id,uint32_t flg,uint32_t type,const RECVX_ENEMY_VEC* pos,const RECVX_ENEMY_VEC* scale,int32_t ay)->int{
            auto& ef=effect_detail::request_template();ef.id=id;ef.flg=flg;ef.type=type;ef.scale={scale->x,scale->y,scale->z};ef.ay=ay;ef.position={pos->x,pos->y,pos->z};
            EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
            spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=0;return enemy_world->spawn_effect(spawn);};
        // eff[eno].stflg |= 0x20, txp[0] = epw->mdl[mdl].texP, tex_id (en06.c:2197-2199).
        t.effect_texture=[](int slot,RECVX_ENEMY_WORK* w,uint32_t mdl,int32_t tex_id){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()){world->unbound_enemy_.insert("eff[slot] ausserhalb des Pools (en06)");return;}
            auto& work=world->sprites_[std::size_t(slot)];work.sprite.stflg|=0x20u;work.sprite.tex_id=tex_id;
            work.texture_list=static_cast<const std::vector<SceneTexture>*>(world->enemy_ext_.model_texp_index(w,int(mdl)));
            work.sprite.txp_category=kEffectOwnTextureList;};
        t.player_wall=en19.player_wall;
        t.material_color=en23.material_color;
        return t;}();
    recvx_en06_bind(&en06);
    // --- EN27 (en27.h): Mottenei auf dem Spielerasset (plp->mdl[6]/[7], sys->plmthp, lkwkp = plp) ----
    static const RECVX_EN27_SERVICES en27=[]{RECVX_EN27_SERVICES t{};
        // hitchk.c:5480 / en27.c:87: rom->flrp, dann sys->mflrp, in Originalreihenfolge.
        t.floor_count=[]()->uint32_t{auto* world=enemy_world;world->enemy_floor_cache_=world->motion_floors();return uint32_t(world->enemy_floor_cache_.size());};
        t.floor_get=[](uint32_t i,RECVX_ENEMY_ATR* out)->int{const auto& c=enemy_world->enemy_floor_cache_;if(i>=c.size())return 0;*out=atr_of(c[i]);return 1;};
        // en27.c:101/:160 fp->prm2 = v: rom->flrp im Ereignisbesitzer, sys->mflrp (Schiebeflaechen) hat keinen Schreibweg.
        t.floor_set_prm2=[](uint32_t i,uint8_t v){auto* world=enemy_world;
            const std::size_t rom=world->collision_events_?world->collision_events_->floor_areas().size():0u;
            if(i<rom&&world->collision_events_->set_floor_parameter(i,2,v)){if(i<world->enemy_floor_cache_.size())world->enemy_floor_cache_[i].parameters[2]=v;return;}
            world->unbound_enemy_.insert("ATR_WORK.prm2: sys->mflrp ohne Schreibweg (en27)");};
        t.use_player_motion=[](RECVX_ENEMY_WORK* w)->int{auto& e=owner_of(w);if(!enemy_world->player_asset_)return 0;e.player_motion_library=true;e.enemy.mtn_room=0;return 1;};
        // epw->mlwP = &plp->mdl[k] (en27.c:62/:199): Modellreihe des Spielers ab k, frischer objP/owP-Satz.
        t.use_player_model=[](RECVX_ENEMY_WORK* w,uint32_t k)->int{auto* world=enemy_world;auto& e=owner_of(w);
            const PlayerAsset* bank=world->player_asset_;const PlayerAsset* asset=bank?(k?bank->model(k):bank):nullptr;if(!asset)return 0;
            e.model_owner=nullptr;e.model_window=0;e.model_bank=bank;e.model_base=k;e.model_absent=false;e.enemy_asset=asset;
            e.enemy_obj=asset->bind_pose();e.enemy_evalflags=asset->node_evalflags();e.enemy_owp_flg.assign(e.enemy_obj.size(),0u);e.enemy_owp_mtx.assign(e.enemy_obj.size(),BoneMatrix{});
            e.enemy_calculated=false;e.morph_obj_a=e.morph_obj_b=0;e.live_model.clear();e.live_models.clear();return 1;};
        // epw->skp[8] = plp->skp[7] (en27.c:64). Der native Teilsatz plp->mdl[k] traegt die SKIN-Daten,
        // die bhReadPlayerData (dread.c:52) ihm als plp->skp[k] zuordnet. Das Ei zeichnet mdl[6] mit
        // mdl_no 0 (skp[0] NULL, mdl[6] hat keinen Skin) und nach dem Schluepfen mdl[7] mit mdl_no 8,
        // also skp[8] == plp->skp[7] = der eigene Skin von mdl[7]. Nur diese Zuordnung ist belegt.
        t.copy_player_skin=[](RECVX_ENEMY_WORK*,uint32_t dst,uint32_t src)->int{auto* world=enemy_world;
            const auto* m=world->player_asset_?world->player_asset_->model(src):nullptr;
            if(dst!=8u||src!=7u||!m){world->unbound_enemy_.insert("skp[dst] = plp->skp[src]: nur skp[8] = plp->skp[7] belegt");return 0;}
            const auto* egg=world->player_asset_->model(6);
            if(egg&&!egg->encoded_skin().empty()){world->unbound_enemy_.insert("plp->mdl[6] mit Skin, aber skp[0] NULL (en27)");return 0;}
            return 1;};
        t.link_player=[](RECVX_ENEMY_WORK* w)->int{w->lkwkp=nullptr;w->lk_player=1;return 1;};
        // zonzon1.c:243 bhEne_SetBloodEffectBurst(plp, type, jno, pos, flg) mit plp->dv (en27.c:340-344).
        t.player_blood_burst=[](int type,int jno,const RECVX_ENEMY_VEC* pos,int flg,const RECVX_ENEMY_VEC* dir)->int{auto* world=enemy_world;
            world->player_hit_dv_={dir->x,dir->y,dir->z};const RECVX_T3_LINK lk={nullptr,1};recvx_t3_set_blood_effect_burst(lk,type,jno,pos,flg);return 1;};
        return t;}();
    recvx_en27_bind(&en27);
    // --- EN15 (en15.h, Nosferatu): Griff, Wurf, Gift auf den vorhandenen Besitzern -------
    static const RECVX_EN15_SERVICES en15=[]{RECVX_EN15_SERVICES t{};
        // plp->dvx/dvy/dvz, dax/day, spd, pxb: Felder des Spielerbesitzers, die
        // RECVX_ENEMY_PLAYER nicht fuehrt. spd geht ueber take_player_speed an die Aktion zurueck.
        t.player_dyn_get=[](RECVX_EN15_PLAYER_DYN* d)->int{auto* world=enemy_world;
            d->dv={world->player_hit_dv_.x,world->player_hit_dv_.y,world->player_hit_dv_.z};
            d->dax=world->player_dax_;d->day=world->player_day_;d->spd=world->player_spd_;
            d->pos_b={world->player_prev_pos_.x,world->player_prev_pos_.y,world->player_prev_pos_.z};return 1;};
        t.player_dyn_set=[](const RECVX_EN15_PLAYER_DYN* d){auto* world=enemy_world;
            world->player_hit_dv_={d->dv.x,d->dv.y,d->dv.z};world->player_dax_=d->dax;world->player_day_=d->day;
            if(world->player_spd_!=d->spd){world->player_spd_=d->spd;world->player_spd_written_=true;}};
        // bhSetMotion(plp, add, ...) aus dem Gegnerpass (en15.c:1977).
        t.player_set_motion=[](RECVX_ENEMY_PLAYER* p,int add)->int{auto* world=enemy_world;
            if(!world->enemy_player_set(*p)){world->unbound_enemy_.insert("bhSetMotion(plp): Spielerbesitzer fehlt");return -2;}
            const MotionClip* clip=nullptr;const int r=world->player_set_motion_now(add,clip);
            world->enemy_player_get(*p);return r;};
        t.player_library=en13.player_library;
        t.calc_fix_offset=en01_services().calc_fix_offset;
        t.room_point=[](uint32_t i,RECVX_ENEMY_VEC* out)->int{const auto& pp=enemy_world->positions_;if(i>=pp.size())return 0;*out={pp[i].x,pp[i].y,pp[i].z};return 1;};
        t.model_root=en53.model_root;
        t.morph_pair=en53.morph_pair;
        t.game_mode=en23.game_mode;
        // 0 gm_flg (flags[11]), 1 st_flg (flags[5]), 2 pt_flg (Darstellungsbesitzer), 3 ts_flg (flags[12]).
        t.sys_flags=[](uint32_t which)->uint32_t{auto* world=enemy_world;
            if(which==2)return world->presentation_flags_;
            static const unsigned bank[4]={11,5,0,12};
            if(which>=4||!world->collision_events_){world->unbound_enemy_.insert("sys->gm_flg/st_flg/ts_flg: EventVm nicht gebunden");return 0;}
            return world->collision_events_->flags[bank[which]][0];};
        t.sys_flags_set=[](uint32_t which,uint32_t set,uint32_t clear){auto* world=enemy_world;
            if(which==2){world->presentation_flags_=(world->presentation_flags_&~clear)|set;return;}
            static const unsigned bank[4]={11,5,0,12};
            if(which>=4||!world->collision_events_){world->unbound_enemy_.insert("sys->gm_flg/st_flg/ts_flg: EventVm nicht gebunden");return;}
            auto& f=world->collision_events_->flags[bank[which]][0];f=(f&~clear)|set;};
        t.event_flag_set=[](uint32_t no,int on){auto* world=enemy_world;
            if(!world->collision_events_){world->unbound_enemy_.insert("bhStFlg/bhCrFlg(sys->ev_flg) ohne EventVm");return;}
            world->collision_events_->set_flag(1,no,on?0u:1u);};
        t.effect_tb=[](uint32_t mask,const RECVX_EN15_EF* v,const RECVX_ENEMY_VEC* ofs,RECVX_ENEMY_WORK* link,int joint)->int{
            RECVX_EN19_EF e{};uint32_t m=0;
            if(mask&RECVX_EN15_EF_FLG){m|=RECVX_EN19_EF_FLG;e.flg=v->flg;}
            if(mask&RECVX_EN15_EF_ID){m|=RECVX_EN19_EF_ID;e.id=v->id;}
            if(mask&RECVX_EN15_EF_TYPE){m|=RECVX_EN19_EF_TYPE;e.type=v->type;}
            if(mask&RECVX_EN15_EF_MDLVER){m|=RECVX_EN19_EF_MDLVER;e.mdlver=v->mdlver;}
            if(mask&RECVX_EN15_EF_POS){m|=RECVX_EN19_EF_POS;e.pos=v->pos;}
            if(mask&RECVX_EN15_EF_SX){m|=RECVX_EN19_EF_SX;e.sx=v->sx;}
            if(mask&RECVX_EN15_EF_SY){m|=RECVX_EN19_EF_SY;e.sy=v->sy;}
            if(mask&RECVX_EN15_EF_SZ){m|=RECVX_EN19_EF_SZ;e.sz=v->sz;}
            if(mask&RECVX_EN15_EF_AX){m|=RECVX_EN19_EF_AX;e.ax=v->ax;}
            if(mask&RECVX_EN15_EF_AY){m|=RECVX_EN19_EF_AY;e.ay=v->ay;}
            return en19.effect_tb(m,&e,ofs,link,joint);};
        // eff[eno]-Nachfelder (en15.c:2349-2354/2376-2407): txp[0] = epw->mlwP->texP.
        t.effect_patch=[](int slot,RECVX_ENEMY_WORK* w,const RECVX_EN15_EFFPATCH* p){auto* world=enemy_world;
            if(slot<0||std::size_t(slot)>=world->sprites_.size()||!p)return;
            auto& work=world->sprites_[std::size_t(slot)];auto& s=work.sprite;
            if(p->fields&RECVX_EN15_P_STFLG)s.stflg|=0x20u;
            if(p->fields&RECVX_EN15_P_TEXP)work.texture_list=static_cast<const std::vector<SceneTexture>*>(world->enemy_ext_.model_texp(w));
            if(p->fields&RECVX_EN15_P_TEXID)s.tex_id=p->tex_id;
            if(p->fields&RECVX_EN15_P_MODE3)s.mode3=p->mode3;
            if(p->fields&RECVX_EN15_P_OWNER)s.owner_work=w;
            if(p->fields&RECVX_EN15_P_VEC){s.xn=p->vec.x;s.yn=p->vec.y;s.zn=p->vec.z;}
            if(p->fields&RECVX_EN15_P_TVXY)for(unsigned k=0;k<4;++k){s.tv[k].x=p->tv[k][0];s.tv[k].y=p->tv[k][1];}};
        return t;}();
    recvx_en15_bind(&en15);
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
    // Skriptauftraege dieses Bildes (0x98/0xC3), bevor die Gegner laufen.
    run_pending_sand();
    const unsigned enemy_sp_flg = enemy_ai_ ? simulation_flags_ : (simulation_flags_ & ~2u);
    recvx_enemy_control(&enemy_table_,&enemy_dispatch_,enemy_sp_flg,&enemy_services_);
    recvx_en04_set_context(nullptr);
    recvx_en01_set_context(nullptr);
    recvx_en14_bind_services(nullptr);
    recvx_en18_bind(nullptr);
    recvx_en13_bind(nullptr);
    recvx_en23_bind(nullptr);
    recvx_en02_bind(nullptr);
    recvx_en05_bind(nullptr);
    recvx_en19_bind(nullptr);
    recvx_en29_bind(nullptr);
    recvx_en26_bind(nullptr);
    recvx_en22_bind(nullptr);
    recvx_en06_bind(nullptr);
    recvx_en27_bind(nullptr);
    recvx_en17_bind(nullptr);
    recvx_en03_bind(nullptr);
    recvx_en03_id_bind(nullptr);
    recvx_en15_bind(nullptr);
    recvx_en53_bind(nullptr);
    recvx_en30_bind(nullptr);
    recvx_en21_bind(nullptr);
    recvx_en09_bind(nullptr);recvx_en09_set_data(nullptr);
    recvx_enemy_ext_bind(nullptr);recvx_en11_bind(nullptr);recvx_en16_bind(nullptr);
}

// pwksub.c:39-56 bhCalcLinkModel(pp): ueber ene[0..ewk_n) jedes Work mit
// flg 0x1 und 0x80, dessen lkwkp pp ist und dessen mlwP gesetzt ist, erhaelt
// bhCalcModel. Die weiteren Schleifen (pwksub.c:58-84: O_WRK der Raumobjekte
// und Items, eff[512]) haben hier keinen gespeicherten Zustand: verknuepfte
// Objekte nehmen ihre Matrix beim Zeichnen aus link_matrix des Elternwerks
// und Effektsprites ueber ihr lkwkp; nur Gegnerworks fuehren gespeicherte owP.
void RoomWorld::calc_enemy_link_model(WorldEntity& e){
    const RECVX_ENEMY_WORK* parent=&e.enemy;
    auto linked=[&](WorldEntity& c){const auto& w=c.enemy;return &c!=&e&&c.enemy_bound&&(w.flg&1u)&&(w.flg&0x80u)&&w.lkwkp==parent&&tree_owner(c).enemy_asset!=nullptr;};
    for(auto& pair:entities_)if(pair.first.first==1&&linked(pair.second))calc_enemy_model(pair.second);
    for(auto& c:enemy_children_)if(linked(c))calc_enemy_model(c);
}

// event.c:11070-11094 (Common 25), :11600-11628 (Common 24) und :11941-11969
// (Common 34): switch(v0) ueber den ct1-Wert des Skripts. 0: SetMotion +
// CalcModel + CalcLinkModel, 1: nichts, 2: SetMotion, 3: CalcModel,
// 4: CalcLinkModel, 5: SetMotion + CalcModel, 6: SetMotion + CalcLinkModel,
// 7: CalcModel + CalcLinkModel. bhSetMotion(work, 0, 0, 0) schreibt den Key
// bei frm_no ohne Vorschub; ohne mode-Bit 2 gilt NormalTbl (Motion.c:148),
// der mtn_tp des Works bleibt ungenutzt.
void RoomWorld::enemy_script_evaluate(WorldEntity& e,unsigned mode){
    EnemyWorldScope scope(this);
    const bool motion=mode==0||mode==2||mode==5||mode==6;
    const bool model=mode==0||mode==3||mode==5||mode==7;
    const bool link=mode==0||mode==4||mode==6||mode==7;
    if(motion&&enemy_services_.set_motion)(void)enemy_services_.set_motion(&e.enemy,0,0u);
    if(model)calc_enemy_model(e);
    if(link)calc_enemy_link_model(e);
}

// event.c:4302 bhEffBloodSet: v0 ist der Raumindex in rom->enep, v1 zugleich
// epw->djnt_no und der Gelenkpunkt von bhSetEffectTb, die drei u16 sind der
// Trefferversatz mit den Vorzeichenbits von v2; danach Typ, Flags und der
// Gradwinkel (Bit 1 der Flags negiert ihn). Den Kern (zonzon.c:466
// bhEne_SetBlood2 mit bhSetEffectTb/bhSetEffParticle) traegt recvx_t3_set_blood2.
bool RoomWorld::script_blood_set(unsigned index,unsigned joint,int type,unsigned flags,int turn,NJS_POINT3 offset){
    const auto* known=state(1,index);if(!known||!known->enemy_bound)return false;
    EnemyWorldScope scope(this);
    EnemyExtScope ext(&enemy_ext_);
    auto& work=entity(1,index);
    work.enemy.djnt_no=int(joint);
    RECVX_ENEMY_VEC ofs{offset.x,offset.y,offset.z};
    int ry=int(turn);if(flags&2)ry=-ry;
    recvx_t3_set_blood2(&work.enemy,type,&ofs,int(std::int16_t(float(std::int16_t(ry))*65536.0f/360.0f)));
    return true;
}

// event.c:4110 bhEffectSandSet (Skriptbefehl 0x98): bhEne02_SetSandEffect(ene[enep[v0].wrk_no],
// &pos, typ). Die Lage ist entweder die Weltlage aus dem Skript oder ein Versatz, der in x und z
// zur Gegnerlage kommt; y bleibt in beiden Faellen der Skriptwert (:4207).
bool RoomWorld::script_sand_effect(unsigned index,NJS_POINT3 offset,bool relative,int type){
    const auto* known=state(1,index);if(!known||!known->enemy_bound)return false;
    auto& work=entity(1,index);
    if(relative){offset.x+=work.enemy.px;offset.z+=work.enemy.pz;}
    SandRequest r;r.work=&work.enemy;r.pos=offset;r.type=type;
    pending_sand_.push_back(r);
    return true;
}
// event.c:8577 bhEffectSandSetMatsumoto (Skriptbefehl 0xC3): bhEne02_SetSandEffectEV(eno, t1, t2)
// liest ene[eno].px direkt, der erste Wert ist also der Werkplatz von sys->ene.
bool RoomWorld::script_sand_effect_ev(unsigned work,int type1,int type2){
    if(work>=enemy_table_.ewk_n||work>=RECVX_ENEMY_SLOTS)return false;
    auto* w=enemy_table_.slot[work];if(!w)return false;
    SandRequest r;r.work=w;r.type=type1;r.type2=type2;r.ev=true;
    pending_sand_.push_back(r);
    return true;
}
// pwksub.c:455-457 / player.c:7132-7134: epp->mlwP->owP[node].mtx des Slots `slot`. owP-Indizes
// des Originals sind logische Knoten (model_index, wie link_matrix/enemy_joint_position); ein
// objP/owP-Fenster (en01.c:2799-2805) verschiebt den Zeiger im selben Indexraum.
std::optional<BoneMatrix> RoomWorld::enemy_slot_joint_matrix(unsigned slot,unsigned node)const{
    if(slot>=enemy_table_.ewk_n||slot>=RECVX_ENEMY_SLOTS)return {};
    const auto* w=enemy_table_.slot[slot];if(!w||!w->owner)return {};
    const auto& t=tree_of(owner_of(w));
    if(!t.enemy_calculated||!t.enemy_asset)return {};
    unsigned physical=0;try{physical=t.enemy_asset->model_index(node+t.model_window);}catch(const std::runtime_error&){return {};}
    if(physical>=t.enemy_owp_mtx.size())return {};
    return t.enemy_owp_mtx[physical];
}
// Laeuft im Gegnerpass, solange die EN02-Dienste gebunden sind.
void RoomWorld::run_pending_sand(){
    if(pending_sand_.empty())return;
    auto requests=std::move(pending_sand_);pending_sand_.clear();
    for(const auto& r:requests){
        if(!r.work)continue;
        if(r.ev){recvx_en02_set_sand_effect_ev(r.work,r.type,r.type2);continue;}
        const RECVX_ENEMY_VEC pos{r.pos.x,r.pos.y,r.pos.z};
        recvx_en02_set_sand_effect(r.work,&pos,r.type);
    }
}

// --- bhEff_SetPtcl / bhEff_SetPtcl2 / bhEff_SetPtcl2V (effsub4.c:19/35/117/199) -------------
// Die drei Erzeuger von EN17. Jeder legt mit bhSetEffect(365|366) einen Satz an und dahinter mit
// bhEff_AllocOwork (effsub4.c:3028) einen zweiten eff[512]-Platz als Hilfsarbeitssatz, in dem die
// Punkte, Vektoren und (bei 365) Groessen liegen. Laeuft bhEff_AllocOwork leer, folgt das Original
// einem NULL-Zeiger; nativ wird das gemeldet und der Satz endet.
// Der gemeinsame Kopf: bhSetEffect(id, &ps, NULL, 0) mit ps.ox/oy/oz = 0 (effsub4.c:49-55),
// dahinter bhEff_AllocOwork. nullptr heisst: kein Platz, der Satz entsteht nicht.
RoomWorld::SpriteWork* RoomWorld::begin_ptcl(unsigned id,NJS_POINT3 pos,const char* what){
    EffectSpawn spawn;spawn.flg=1;spawn.id=id;spawn.type=0;spawn.flr_no=0;spawn.mdlver=0;
    spawn.position=pos;spawn.scale={1.0f,1.0f,1.0f};spawn.ax=0;spawn.ay=0;spawn.lkono=0;
    const int slot=spawn_effect(spawn);
    if(slot<0)return nullptr;                                  // bhSetEffect == -1: der Erzeuger laesst es
    auto& work=sprites_[std::size_t(slot)];
    if(effect_detail::acquire_extra_work(work.sprite))return &work;
    unbound_enemy_.insert(std::string(what)+": bhEff_AllocOwork ohne freien eff[512]-Platz (effsub4.c:3028, das Original folgt NULL)");
    work.sprite.flg=0;return nullptr;
}
void RoomWorld::set_ptcl(RECVX_ENEMY_WORK* w,NJS_POINT3 pos,int tex_id){
    // :24 time = (rand() % 4) * 4, danach drei Saetze mit je vier Bildern mehr Verzoegerung.
    int time=int(recvx_core_random_next()%4)*4;
    for(int i=0;i<3;++i){set_ptcl1(w,pos,tex_id,time);time+=4;}
}
void RoomWorld::set_ptcl1(RECVX_ENEMY_WORK* w,NJS_POINT3 pos,int tex_id,int wcnt){
    auto* set=begin_ptcl(365,pos,"bhEff_SetPtcl1");
    if(!set)return;
    auto& s=set->sprite;
    auto* row=effect_detail::extra_rows<EffectPtcl365Work>(s,"bhEff_SetPtcl1");
    // :68/69 op->txp[0] = epw->mlwP->texP, op->tex_id = tex_id; :75 op->ct3 = wcnt.
    unsigned index=0;
    if(!enemy_work_index(w,index))unbound_enemy_.insert("bhEff_SetPtcl1: txp[0] = epw->mlwP->texP ohne Raumfigur");
    else{set->texture_list=static_cast<const std::vector<SceneTexture>*>(enemy_ext_.model_texp(w));s.txp_category=0x80000000u|index;}
    s.tex_id=tex_id;s.ct3=wcnt;
    row->num=40;                                               // :77 effect->dtbl.num
    // :87-89 njUnitMatrix(NULL), njRotateY(NULL, epw->ay); njCalcPoint dreht den Startvektor.
    NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,w->ay);
    for(int i=0;i<40;++i){                                     // :91-112, je Punkt drei rand()
        row->vtx_pos[i].x=pos.x+(-0.5f+effect_detail::effect_random());
        row->vtx_pos[i].y=pos.y;
        row->vtx_pos[i].z=pos.z+(-0.5f+effect_detail::effect_random());
        NJS_POINT3 ps{0.0f,-(0.2f+(1.5f*effect_detail::effect_random())),0.0f};
        row->size[i]=-0.15f*ps.y;
        if(row->size[i]<0.1f)row->size[i]=0.1f;
        njCalcPoint(&m,&ps,&row->add_vec[i]);
    }
}
void RoomWorld::set_ptcl2(NJS_POINT3 pos,const float* mtx16){
    auto* set=begin_ptcl(366,pos,"bhEff_SetPtcl2");
    if(!set)return;
    auto& s=set->sprite;
    auto* row=effect_detail::extra_rows<EffectPtcl366Work>(s,"bhEff_SetPtcl2");
    s.ct3=0;s.type=0;                                          // :151/153
    row->num=16;                                               // :157
    // :167-170 njSetMatrix(NULL, mtx), njUnitTransPortion(NULL), njRotateZ(NULL, -16384):
    // nur der Drehanteil der Armmatrix, um Z gekippt.
    NJS_MATRIX m;std::copy(mtx16,mtx16+16,m);njUnitTransPortion(&m);njRotateZ(&m,-16384);
    for(int i=0;i<16;++i){                                     // :172-192, je Punkt drei rand()
        row->vtx_pos[i]=row->vtx_pos2[i]=pos;
        const int ang=int(((recvx_core_random_next()%16)*4096)&0xFFFFu);
        const int ang2=int(recvx_core_random_next()%5461)+5461;
        const float spd=1.0f+(2.0f*effect_detail::effect_random());
        NJS_POINT3 v{-njSin(ang)*(spd*njCos(ang2)),spd*njSin(ang2),-njCos(ang)*(spd*njCos(ang2))};
        njCalcPoint(&m,&v,&row->add_vec[i]);
    }
}
void RoomWorld::set_ptcl2v(NJS_POINT3 pos,NJS_POINT3* dv,int wcnt){
    auto* set=begin_ptcl(366,pos,"bhEff_SetPtcl2V");
    if(!set)return;
    auto& s=set->sprite;
    auto* row=effect_detail::extra_rows<EffectPtcl366Work>(s,"bhEff_SetPtcl2V");
    s.ct3=wcnt;s.type=1;                                       // :233/235
    row->num=16;                                               // :239
    for(int i=0;i<16;++i){                                     // :249-274, je Punkt drei rand()
        row->vtx_pos[i]=row->vtx_pos2[i]=pos;
        const int ang1=int(recvx_core_random_next()%7281)-3640;
        const int ang2=int(recvx_core_random_next()%7281)-3640;
        const float spd=1.0f+(2.0f*effect_detail::effect_random());
        njUnitVector(dv);                                      // :260 normiert den Vektor des Aufrufers
        NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,0,ang2,-ang1);
        njCalcPoint(&m,dv,&row->add_vec[i]);
        row->add_vec[i].x*=spd;row->add_vec[i].y*=spd;row->add_vec[i].z*=spd;
    }
}
// en17.c:1689 bhEne17_AfterimageAxEffect: ein 364-Satz je Nachbild des Beils. Die Lage ist
// mtx * ofs, die Winkel kommen aus derselben Matrix, gezeichnet wird objP[1] von epw->mdl[5]
// mit dessen Textur, und flr_no 1 faerbt die Materialien mit argb (npSetAllMatColor).
void RoomWorld::en17_afterimage(RECVX_ENEMY_WORK* w,const float* mtx16,NJS_POINT3 ofs,std::uint32_t argb){
    NJS_MATRIX m;std::copy(mtx16,mtx16+16,m);
    NJS_POINT3 pos;njCalcPoint(&m,&ofs,&pos);
    // :1692-1701 sys->ef: id 364, type 0, flg 1, Skalierung 1, ax/ay 0, mdlver 1.
    auto& ef=effect_detail::request_template();
    ef.id=364;ef.type=0;ef.flg=1;ef.scale={1.0f,1.0f,1.0f};ef.ax=0;ef.ay=0;ef.mdlver=1;ef.position=pos;
    EffectSpawn spawn;spawn.flg=ef.flg;spawn.id=ef.id;spawn.type=ef.type;spawn.flr_no=ef.flr_no;spawn.mdlver=ef.mdlver;
    spawn.position=ef.position;spawn.scale=ef.scale;spawn.ax=ef.ax;spawn.ay=ef.ay;spawn.lkono=0;
    const int slot=spawn_effect(spawn);
    if(slot<0){
        // :1705-1710 prueft `eno == -1` und darin `eno >= 512`, kehrt also NICHT um und schreibt
        // eff[-1]; nativ wird das gemeldet statt vor den Pool zu schreiben.
        unbound_enemy_.insert("bhEne17_AfterimageAxEffect: eff[-1] original out-of-bounds (en17.c:1705)");
        return;
    }
    auto& work=sprites_[std::size_t(slot)];auto& s=work.sprite;
    s.stflg|=0x20u;                                            // :1712 txp[0] statt sys->ef_tlist
    s.tex_id=0;                                                // :1713
    unsigned index=0;
    if(!enemy_work_index(w,index))unbound_enemy_.insert("bhEne17_AfterimageAxEffect: mlwP = &epw->mdl[5] ohne Raumfigur");
    else{
        s.model_enemy=int(index);s.model_mdl=5u;               // :1714 mlwP = &epw->mdl[5]
        work.texture_list=static_cast<const std::vector<SceneTexture>*>(enemy_ext_.model_texp_index(w,5)); // :1715 txp[0] = mlwP->texP
    }
    // :1716-1718 Winkel aus der Nachbildmatrix (njRotateEx-Reihenfolge Z, Y, X in bhEff_ModelDraw).
    s.ax=int(10430.381f*std::atan2(m[6],m[10]));
    s.ay=int(10430.381f*std::asin(-m[2]));
    s.az=int(10430.381f*std::atan2(m[1],m[0]));
    s.flr_no=1;                                                // :1719 npSetAllMatColor
    s.tv[0].col=argb;                                          // :1720 nur der erste Eckpunkt
}
}

namespace recvx {
void RoomWorld::run_in_enemy_scope(void (*fn)(RECVX_ENEMY_WORK*),RECVX_ENEMY_WORK* w){EnemyWorldScope scope(this);fn(w);}
}
