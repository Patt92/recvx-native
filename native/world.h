#pragma once
#include "player_push.h"
#include "actor.h"
#include "audio.h"
#include "events.h"
#include "renderer.h"
#include "collision.h"
#include "effect_collision.h"
#include "geometry_motion.h"
#include "effect_sprites.h"
#include "enemy_spawn.h"
#include "subplayer.h"
#include "en01_damage.h"
#include "enemy_work.h"
#include "enemy_dispatch.h"
#include "en03_collision.h"
#include "effect_particles.h"
#include "weapon_flash.h"
#include "player_combat_core.h"
#include "object_itmbox.h"
#include "view_clip.h"
#include <deque>
#include <set>
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <functional>
namespace recvx {
struct PlayerGroundWork {NJS_POINT3 ground{};NJS_POINT3 water_c1{},water_c2{};float water_radius=0;std::optional<GroundHit> hit;};
struct WorldEntity {
    std::vector<FaceParameter> face_keys;unsigned face_frame=0,face_flags=0;
    // face.c:321 bhSetLip only stores the selected stream in exp0[268];
    // face.c:141 bhControlMask copies it into LIP_WORK.top/ptr while
    // lip->flag == 0. A running stream therefore keeps its own source.
    std::optional<FaceParameter> facial;std::vector<std::uint8_t> lip_stream,lip_current_stream;unsigned lip_bank=0,lip_pos=0,lip_count=0,lip_time=0,lip_cur=0,lip_next=0;bool lip_active=false;
bool subplayer_bound=false,subplayer_no_motion=false;SceneSpawn transform{};std::array<int,3> angles{};unsigned identity=0,type=0,death_ticks=0,object_flags=0,object_parameter=0;int object_floor=0;
    // O_WRK.hide[0..3] (objitm.c:200-203 bhSetObject, :262-265 bhSetItem): die
    // Schnittnummern, in denen ein Werk mit flg 0x40000000 gezeichnet wird.
    // 255 steht in den Originaldaten fuer "kein Schnitt".
    std::array<std::uint8_t,4> object_hide{{255,255,255,255}};
    // O_WRK fields of a room object (kind 2) that enemy routines read and write
    // (en04.c:1100 door branch, :3955 wire shake): flg2, mode1, mode3, ct0, ayp.
    unsigned object_flg2=0,object_mode1=0,object_mode3=0;int object_ct0=0,object_ayp=0;int hp=0;bool dead=false,combat_enabled=true;bool hidden=false,inactive=false,frozen=false,scripted=false,has_motion=false,room_motion=true;unsigned motion=0;std::int64_t frame=0,step=65536;bool linked=false;unsigned parent_kind=0,parent_index=0,parent_node=0;NJS_POINT3 link_offset{};std::map<unsigned,bool> hidden_nodes;ModelNodeOverrides node_overrides;std::shared_ptr<RECVX_EN01> en01;unsigned upper_motion=0;bool flinch=false;std::array<int,2> flinch_angles{};int neck_angle=0;
    // bhEneLookFlgSet (event.c): original BH_PWORK flg bit 0x8000. Enemy AI
    // consumers (en01.c) are still pending; the state itself is exact.
    bool look=false;
    // Common_controll 41 (event.c:10346): original BH_PWORK flg bit 0x40000.
    // Its readers are family-specific (en01.c:10498, en05.c:4877, en09.c:7574,
    // en12.c:1601, en17.c:681, en26.c:1270) and the player foot-fix gate
    // (player.c:1539, ported in player_stairs.c:918); there is no generic
    // model-link reader in MdlPut.c/objitm.c. The bit is stored exactly here;
    // the per-family consumer is the open contract (see work report).
    bool model_link=false;
    // bhSetMotion samples frm_no before advancing it. Event conditions read
    // the advanced clock; rendering and joint controls use the sampled pose.
    std::optional<std::int64_t> sampled_frame;
    unsigned motion_mode3=0; // O_WRK: only bhSetObj/ItmMotion advances mode 1.
    bool motion_slow=false; // Common 24/25/34 mtn_md bit 8, separate from MTN flags.
    std::int64_t pose_frame()const{return scripted&&sampled_frame?*sampled_frame:frame;}
    // objP of the original work. motion/room_motion/frame/has_motion above are
    // mtn_no/frm_no, the state the control pass will sample next;
    // bhSetMotion/bhCalcModel write the object tree, and they only run while
    // that pass is enabled. Everything that draws or reads a pose therefore
    // uses the committed values, so a work written while the pass is paused
    // (sp_flg bit 0x2 clear, e.g. the special-event computer) leaves the
    // drawn tree alone until the pass returns.
    bool pose_set=false,pose_room=true,pose_motion_valid=false;
    unsigned pose_mtn=0;std::int64_t pose_frm=0;
    void commit_pose(){pose_set=true;pose_mtn=motion;pose_room=room_motion;pose_motion_valid=has_motion;pose_frm=pose_frame();}
    unsigned posed_motion()const{return pose_set?pose_mtn:motion;}
    bool posed_room_motion()const{return pose_set?pose_room:room_motion;}
    bool posed_has_motion()const{return pose_set?pose_motion_valid:has_motion;}
    std::int64_t posed_frame()const{return pose_set?pose_frm:pose_frame();}
    // bhObjAspdSet: object work aspd, a light parameter in objitm.c
    // (emissive intensity 0.1*aspd and ob_spc/special object selection).
    unsigned aspd=0;
    // bhCyoutenHenkei family: mdflg bit 2 plus shp_ct (0..1000) and, for
    // bhCyoutenHenkeiSetEX, the original mbp model indices obj_a/obj_b.
    // Vertex interpolation/rendering is pending; the shape clock is exact.
    bool morph=false;float morph_shape=0;unsigned morph_obj_a=0,morph_obj_b=0;
    // objP[node].evalflags of the realized models beyond the Work's own
    // (mbp[0], whose live array is enemy_evalflags). The blend reads the live
    // array of both sources, not the immutable asset's.
    std::vector<std::vector<std::uint32_t>> morph_model_flags;
    const std::vector<std::uint32_t>* morph_live_flags(unsigned model)const{
        return model&&model<=morph_model_flags.size()&&!morph_model_flags[model-1].empty()?&morph_model_flags[model-1]:nullptr;
    }
    // C02: the original BH_PWORK of a room enemy (eneset.c:170 bhSetEnemy).
    // It lives here; RoomWorld's enemy table only points at it. enemy_native:
    // bhJumpEnemy[id] has a native routine, so this work (not the fields
    // above) is the authority during the enemy pass (enemy_work.h, B).
    RECVX_ENEMY_WORK enemy{};bool enemy_bound=false,enemy_native=false;const PlayerAsset* enemy_asset=nullptr; // mlwP source (models_ is fixed after load)
    // mlwP of that work: objP pos/ang (bhSetMotion), objP evalflags, owP
    // flg/mtx (bhCalcTree). enemy_calculated: bhCalcModel has run once.
    std::vector<ModelPose> enemy_obj;std::vector<std::uint32_t> enemy_evalflags,enemy_owp_flg;std::vector<BoneMatrix> enemy_owp_mtx;bool enemy_calculated=false;
    // Verknuepfte Kindworks (bhEne01_SetLinkEnemy, en01.c:2815). model_owner:
    // `epp->mlwP = epw->mdl` — das Kind liest und schreibt den Objektbaum des
    // Elternworks, es hat keinen eigenen. model_window: objP/owP += 8 und
    // obj_num -= 8 am geteilten ML_WORK (en01.c:2799/10347/3148); der Wert gehoert
    // deshalb dem Baumbesitzer. motion_library: `epp->mnwP = epw->mnwP`
    // (en01.c:2829), die Identitaet, deren emtp das Kind abspielt; -1 = eigene.
    WorldEntity* model_owner=nullptr;unsigned model_window=0;int motion_library=-1;
    // npSetAllMatColor(mlwP->objP, obj_num, color) (njplus.c:1375) patcht in
    // JEDEM Objekt des Baums die Materialchunks 17..23: B/G/R werden immer
    // geschrieben, A nur, wenn der Modellwert nicht 0 ist. Der Wert gehoert
    // wie der Baum dem model_owner.
    bool enemy_material_set=false;std::uint32_t enemy_material_color=0;
    // Entity values as the last publish left them; a difference before the
    // next pass is a script write that recvx_enemy_adopt takes over.
    struct EnemyMirror {NJS_POINT3 position{};std::array<int,3> angles{};int yaw=0,floor=0;unsigned motion=0;bool room_motion=false;std::int64_t frame=0,step=0;int hp=0;bool hidden=false,inactive=false,look=false;} enemy_mirror{};};
class RoomWorld {
public:
    RoomWorld(const std::vector<std::uint8_t>& room,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data={},const std::filesystem::path& mods={},const std::string& room_name="RM_0000",unsigned game_mode=0);
    struct CombatTarget {unsigned index,identity;NJS_POINT3 position;float radius,height;int hp;};
    std::vector<CombatTarget> combat_targets()const;
    void combat_difficulty(unsigned mode);
    void sound_monitor();
    // weapon.c bhCheckEnemies/bhSetGunFire: assigns one hit (joint damage, hit position/direction) to the en01 work.
    bool damage(unsigned index,int weapon,int amount,unsigned comb_flg,NJS_POINT3 position,NJS_POINT3 direction,int hit_joint=-1);
    // Original en01 routines the damage/death port called while they are still unbound (reported, never hidden).
    const std::set<std::string>& unbound_enemy_routines()const{return unbound_enemy_;}
    std::size_t blood_particles()const{return particles_.active();}
    // event.c:4302 bhEffBloodSet: epw->djnt_no und bhEne_SetBlood2 am Gegnerwork.
    bool script_blood_set(unsigned index,unsigned joint,int type,unsigned flags,int turn,NJS_POINT3 offset);
    bool enemy_dead(unsigned index)const;
    struct JointHit {int joint;NJS_POINT3 point;};
    std::optional<JointHit> enemy_capsule_hit(unsigned index,NJS_POINT3 start,NJS_POINT3 end,float radius);
    void set_enemy_health(unsigned index,int hp);
    // bhEneSetCk: original BH_PWORK stflg 0x1000000 (not drawn, no hit/sight checks).
    void deactivate_enemy(unsigned index);
    bool step_clear(NJS_POINT3 probe,float radius=3.5f,float height=16.5f)const;
    // Native bhCheckWallType / bhCollisionCheckLine/2/3 over the existing
    // room-then-mobile geometry. These are not bhCheckWall movement or
    // bhCheckL2Wall (which has a different capsule/inset contract).
    std::optional<CollisionWall> collision_contact(NJS_POINT3 probe,unsigned flags,float radius,float height)const;
    std::optional<CollisionWall> collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal)const;
    std::optional<CollisionWall> collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,bool omit_attribute4=false)const;
    float shot_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range)const{return wall_ray_distance(collision_geometry(),room_height_,origin,direction,range,floor_at(origin));}
    float combat_wall_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,unsigned flags=0x400)const{return wall_ray_distance(collision_geometry(),room_height_,origin,direction,range,-32768,flags);}
    // bhGetGroundPosition for the original attack path; same collision set as movement.
    float combat_ground(NJS_POINT3 probe);
    std::optional<GroundHit> ground_hit()const{return ground_.hit();}
    const PlayerGroundWork* player_ground_work()const{return player_ground_ready_?&player_ground_:nullptr;}
    void player_stair_ground_state(unsigned status,int mode0){stair_ground_state_=std::make_pair(status,mode0);}
    void clear_player_stair_ground_state(){stair_ground_state_.reset();}
    const std::vector<SceneTexture>* weapon_flash_textures()const{const auto at=effect_textures_.find(3);return at==effect_textures_.end()?nullptr:&at->second;}
    bool bind(const EventCommand& command);
    void set_simulation_flags(unsigned flags){simulation_flags_=flags;}
    const WorldEntity* state(unsigned kind,unsigned index)const;
    // bhGameOverSet: the room script requested the death sequence (original
    // gov_md0 driver of gameover.c is pending).
    bool game_over_requested()const{return game_over_;}
    // bhCyoutenHenkei family on the player work (bhCyoutenHenkeiSetEX type 0).
    bool player_enemy_motion()const{return player_enemy_motion_;}
    // en07.c:2190/2221: plp->mnwP is the controlled work's library while a grab
    // owns the player, and plp->mtn_no then indexes that library. The host frame
    // count must follow the same source as the sampled clip.
    unsigned player_motion_frames()const;
    bool player_morph()const{return player_morph_.morph;}float player_morph_shape()const{return player_morph_.morph_shape;}
    const EffectSprite* effect_state(unsigned index)const{return index<sprites_.size()?&sprites_[index].sprite:nullptr;}
    bool effect_visible(unsigned index)const;
    EffectContext effect_context()const;
    void set_presentation_flags(unsigned flags){presentation_flags_=flags;} // sys->pt_flg, native draw owner
    unsigned presentation_flags()const{return presentation_flags_;}
    // game.c:196-224 / :115-139 (bhCheckExmAtari, cb_flg 0x8): exm_attr 0x2 also drops
    // rom->lgtp[1].flg 0x2; neither restore path sets it again.
    void clear_exm_light(){if(lights_.size()>1)lights_[1].flags&=~2u;}
    // hitchk.c:4819: cam.flg 0x20 (cut.c bhSetCut/bhSetFixedCut take it from the cut's
    // flg 0x400; bhSetEventCamera/bhSetEventFixedCut clear it) arms ef_flg 0x10.
    void mark_exm_cut_effect(){if(!event_camera&&(cut_fog_flags_&0x400u))effect_flags_|=0x10u;}
    void set_fixed_cut(unsigned cno);                       // cut.c:448 bhSetFixedCut
    void set_event_camera(unsigned cno,unsigned kno,bool lock); // cut.c:1644 / :1726
    // cut.c:12 bhCheckCut(1): bhSetCut(cam.ncut, 1) for the player's current cut.
    void return_to_cut(){event_camera=false;fixed_camera.reset();camera_force_=true;}
    // sys->ef_flg. system.c:195 initializes it to 0; its only original writer
    // is the mn_md1 room-load sequence (system.c:1830/1884), which no native
    // owner drives yet, so bit 0 stays clear until one binds it.
    void set_effect_flags(unsigned flags){effect_flags_=flags;}
    unsigned effect_flags()const{return effect_flags_;}
    struct LightState {unsigned flags,type,clock,link,index,node;NJS_POINT3 position,color;float near_range,far_range;};
    std::optional<LightState> light_state(unsigned index)const;
    ModelPose model_pose(unsigned kind,unsigned index,unsigned script_node=0)const;
    // sp_evt.c bhDrawSpEvtComCharacter: the special-event computer draws
    // rom->enep[chr_no] into its character window. The original writes the
    // script's mtn_no/frm_no and a zero position/angle into that enemy work
    // and draws the object tree with njCnkEasyDraw (white light from
    // (4,6,10), diffuse 1.0, ambient 0.2). Returns nothing when the room has
    // no such enemy or its motion library lacks the clip; the caller reports
    // that as a gap instead of drawing a substitute.
    std::optional<Scene> computer_character(unsigned chr_no,unsigned motion,unsigned frame_no);
    void set_player_asset(const PlayerAsset* player){player_asset_=player;if(player){player_obj_=player->bind_pose();player_owp_flags_.assign(player_obj_.size(),0);}}
    void set_player_character(unsigned character){player_character_=int(character&3);}
    // C02: transient enemy view of the existing player. Runtime binds its
    // Inventory HP and EXP1 head flags by address; neither is copied here.
    void bind_enemy_player(int& hp,std::uint32_t& head_flags){player_hp_owner_=&hp;player_head_flags_=&head_flags;}
    bool enemy_player_get(RECVX_ENEMY_PLAYER& out)const;
    bool enemy_player_set(const RECVX_ENEMY_PLAYER& in);
    unsigned enemy_player_motion_frames(unsigned motion,bool from_enemy)const;
    // sys->ene / sys->ewk_n: die eine Gegnertabelle des Raumlaufs. Der
    // Kampfpass liest sie ueber den Dienst `enemies` (pwksub.c:244
    // bhSearchNearEnemy, playpch.c:318 bhSearchEnemy, weapon.c:614
    // bhCheckGunAtari); sie haelt nur Zeiger auf die vorhandenen Entities und
    // wird vom Gegnerpass desselben Besitzers gefuellt.
    RECVX_ENEMY_TABLE& enemy_table(){return enemy_table_;}
    void commit_player_joints(const std::vector<BoneMatrix>& nodes){player_joint_matrices_=nodes;}
    // bhCalcModel(plp) (player.c:1517) runs on mlwP->objP after the mode routine
    // and bhControlPlayerHead, so the owP it commits carries the script node
    // writes and the crate pose as well.  The owner of the committed matrices
    // uses this to sample exactly that object tree.
    ModelNodeOverrides player_pose_overrides()const{auto edits=overrides(0,0);if(push_pose_current())for(const auto& v:push_->pose())edits[v.first]=v.second;return edits;}
    bool enemy_player_joint(unsigned joint,BoneMatrix& out)const;
    std::vector<ModelPose> player_local_pose()const;
    void set_pad_clear_callback(std::function<void(uint32_t)> cb){pad_clear_cb_=std::move(cb);}
    void clear_pad_on(uint32_t bits){if(pad_clear_cb_)pad_clear_cb_(bits);}
    void set_effect_counter(unsigned yk_ct){effect_yk_ct_=yk_ct;}
    int spawn_player_effect(const EffectSpawn& effect){return spawn_effect(effect,1,0);}
    // bhSetEffectTb for bhSetGunFire requests (bhEff003); link_kind 1 = player, 2 = enemy.
    int spawn_weapon_flash(const RECVX_CB_EFFECT& effect,unsigned link_kind,unsigned link_index);
    void set_gun_light(const RECVX_CB_LIGHT& light); // rom->lgtp[0] from bhSetGunFire
    // hitchk.c bhCheckExmAtari, type 4 with attr 0x2: obwp[object] with flg 1
    // becomes type 100 (mode0..mode3 cleared). Returns false when flg 1 is clear.
    bool start_item_box(unsigned object);
    // bhControlObject for every type-100 room object: bhJumpObject[100] = bhObjItmBox.
    void control_item_boxes(unsigned& cb_flg,unsigned& player_stflg);
    // bhSetExplosion: the effect works, flash light and sound; `view` is the camera direction (cam.vx/vy/vz).
    void explode(NJS_POINT3 position,NJS_POINT3 view);
    // Blast damage dealt to the player by bhCheckBombAtari since the last call (hp points).
    int take_blast_damage(){const auto value=blast_damage_;blast_damage_=0;return value;}
    // Zombie AI (debug menu). Off keeps idle zombies standing; damage reactions still run.
    void set_enemy_ai(bool enabled){enemy_ai_=enabled;}
    bool enemy_ai()const{return enemy_ai_;}
    // The hp the host shows is clamped at 0; a negative plp->hp (the original death condition) stays until the host revives the player.
    void set_player_poisoned(bool poisoned){player_poisoned_=poisoned;}
    void set_player_hp(int hp){if(player_killed_&&hp<=0)return;if(hp>0){player_killed_=false;player_dead_=false;}player_hp_=hp;}
    int player_health()const{return player_hp_owner_?*player_hp_owner_:player_hp_;}
    // Zombie grab/bite (needs the AI). Default off; the debug menu switches it.
    void set_enemy_grab(bool enabled){enemy_grab_=enabled;}
    bool enemy_grab()const{return enemy_grab_;}
    // sys->pad_ps while grabbed: newly pressed direction bits (0xf) and buttons (0x2d00); bhEne_LeverCheck shakes the zombie off.
    void set_player_lever(unsigned pad_ps){lever_bits_=pad_ps;}
    unsigned player_lever()const{return lever_bits_;}
    bool player_grabbed()const{return grab_driving_;}
    bool player_dead()const{return player_dead_;}
    bool player_killed()const{return player_killed_;}
    void set_player_attack(bool attacking){player_attacking_=attacking;player_at_flags_=(player_at_flags_&~2u)|(attacking?2u:0u);} // plp->at_flg & 2
    void set_player_weapon(unsigned wpnr_no){player_weapon_=wpnr_no;} // plp->wpnr_no for bhControlPlayer effects
    void set_player_way(std::int16_t way){player_way_=way;} // plp->way, written by the combat pass (player.c:1707/4992)
    void set_player_speed(float spd){player_spd_=spd;} // plp->spd of the native action pass
    // StartVibrationEx(AtrbId, VibNo) requests of enemies, zonzon helpers and
    // event opcode 0x9f; SetEventVibrationMode of opcode 0x9e. The runtime owns
    // the vibration unit and drains these once per tick.
    std::vector<std::pair<int,int>> take_vibration_requests(){auto out=std::move(vibration_requests_);vibration_requests_.clear();return out;}
    std::optional<int> take_vibration_mode(){auto out=vibration_mode_request_;vibration_mode_request_.reset();return out;}
    // PEXP0_F(offset) writes by enemy grab routines (en04.c:3448/3449); the
    // player's EXP_WORK floats live with the runtime's action work.
    std::vector<std::pair<unsigned,float>> take_player_exp0_writes(){auto out=std::move(player_exp0_writes_);player_exp0_writes_.clear();return out;}
    void start_task(unsigned level);
    void tick();
    unsigned take_enemy_room_flags(){const auto flags=enemy_spawn_.room_flags;enemy_spawn_.room_flags=0;return flags;}
    Scene sample_player(const PlayerAsset& player,std::vector<BoneMatrix>* nodes=nullptr,const ModelNodeOverrides& combat={})const;
    void append(Scene& scene,const std::vector<BoneMatrix>& player_nodes={},const SceneCamera& active={})const;
    std::size_t export_textures(const std::filesystem::path& output,const std::string& room_name)const;
    SceneCamera camera(const Scene& stage,const SceneCamera& active)const;
    void overlay(SoftwareRenderer& image)const;
    unsigned frame(unsigned kind,unsigned index)const;
    std::optional<NJS_POINT3> position(unsigned kind,unsigned index)const;
    bool player_hidden=false,player_scripted=false,player_action=false;
    bool game_over_=false; // bhGameOverSet request; death driver pending
    WorldEntity player_morph_{}; // bhCyoutenHenkei on the player work
    bool event_camera=false;
    unsigned event_camera_index=0;
    std::optional<unsigned> fixed_camera;
    unsigned event_camera_key()const{return event_camera_key_;}
    unsigned event_camera_clock()const{return event_camera_clock_;} // cam.ct0
    bool event_camera_paused()const{return event_camera_paused_;}   // cam.mode0 == 3
    bool event_camera_finished()const{return event_camera_finished_;} // cam.mode0 == 2
    // bhSetCut (cut.c:281): clip, hide tables and the cam.fog_* hand-over. The
    // bhSetFixedCut clip call passes cam.ncut, its fog comes from cno separately.
    void initialize_camera_clip(unsigned cut,bool hand_over_fog=true);
    ViewClip camera_clip()const{return installed_clip_;}
    void tick_camera(const Scene& stage,const SceneCamera& target,std::size_t cut,bool tracking=true);
    struct FogView {bool enabled;std::uint32_t color;float near_range,far_range;int ct;};
    FogView fog_view()const{return {fog_.enabled,fog_color_,cam_fog_near_,cam_fog_far_,fog_ct_};}
    bool camera_transitioning()const{return active_camera_.transitioning();}
    unsigned camera_clock()const{return active_camera_.clock();}
    int floor_at(NJS_POINT3 position)const;
    const RECVX_PUSH_STATE* push_state()const{return push_?&push_->state():nullptr;}
    bool pushing()const{return push_&&push_->active();}
    bool tick_push(bool forward);
    // bhObj001 stores climb / descend ETC records in sys->metcp.  Unlike the
    // room ETC table, these records move with their O_WRK.
    std::vector<RECVX_STAIR_AREA> push_steps();
    std::vector<RECVX_STAIR_AREA> push_floors();
    void initialize_player_spawn(); // bhSetRoom: POS height selects rom->grand
    const std::array<float,32>& floor_heights()const{return floors_;} // rom->grand
    unsigned floor_sound(NJS_POINT3 position,bool special=false)const;
    unsigned floor_sound(int floor,float x,float z,bool special=false)const;
    // hitchk.c bhCheckWater: the first original type-3 water area with prm0 == 1
    // that contains the probe, room flrp before bhObj001's mflrp. `top` then
    // receives fp->py + fp->h, the surface the original splash is placed on.
    bool water_top(NJS_POINT3 position,float& top)const;
    void player_footstep(const PlayerAsset& player);
    void player_foot_sound(const PlayerAsset& player,unsigned node,unsigned type);
    bool search_near_enemy_b(NJS_POINT3 pos,int ay,int ar,float len)const;
    NJS_POINT3 player_safe_position()const{return collision_safe_;}
    void set_player_safe_position(NJS_POINT3 p){collision_safe_=p;}
    // Host-Diagnose-Teleport (Replay "position", Debug): der Spieler steht danach so, als
    // waere er schon dort gewesen. Wie die Original-Teleports der Aktionsenden
    // (player.c:3095-3099: px = spx, bpx = bpxb = px) folgen spx/spz und die Vorsonde bpxb der
    // neuen Lage; sonst waehlt bhCheckWallEx die Austrittsseite nach der alten Sonde.
    // Ende einer Stufen-/Treppenaktion: bpx = bpxb = px, bpy = bpyb = 12.5 + py, bpz = bpzb = pz
    // (player.c:3099/3294/3474/3692). Ohne das vergleicht der erste Koerperpass nach der
    // Landung gegen die Vorsonde der Ausgangsebene.
    void set_player_previous_probe(NJS_POINT3 p){collision_previous_probe_={p.x,p.y+12.5f,p.z};}
    void place_player(NJS_POINT3 p){collision_safe_=p;collision_previous_probe_={p.x,p.y+12.5f,p.z};collision_front_offset_=collision_front_target_=0.f;collision_probe_ready_=true;}
    // bhControlPlayer player.c:1578-1591 und :1649-1654: der Spielerpass fuehrt
    // EXP_WORK arn an arp nach und schreibt fpx/bpx/spx aus der aktuellen Lage.
    // `arp` ist der Wert, den bhCPM2_act_* dieses Bildes hinterlassen hat.
    void settle_player_body(NJS_POINT3& position,float arp);
    void finish_player_pass(NJS_POINT3& position,Angle yaw);
    bool player_safe_position_ready()const{return collision_probe_ready_;}
    void report_enemy_gap(const char* name){unbound_enemy_.insert(name);}
    NJS_POINT3 move(NJS_POINT3 from,NJS_POINT3 to,float radius=3.5f,float height=16.5f,std::optional<bool> walking={});
    void bind_player_collision(EventVm&);
    NJS_POINT3 move_player(NJS_POINT3 from,NJS_POINT3 to,int movement,bool run,int turn=0);
    void check_player_floor(NJS_POINT3 position,Angle yaw,int floor);
    // *(int*)&plp->mode0 of the collision-owned player work (mode0..mode3,
    // little endian). bhCheckWallEx writes 2/0x102 for a fire wall.
    unsigned player_mode()const{return collision_mode_;}
    void set_player_mode(unsigned mode){collision_mode_=mode;}
    // Free player motion with an explicit EXP_WORK arp (bhCPM0_damage).
    NJS_POINT3 move_player_arp(NJS_POINT3 from,NJS_POINT3 to,float arp){return resolve_player_motion(from,to,3.5f,player_character_?17.5f:16.5f,arp);}
private:
    struct Model {unsigned kind,index;std::unique_ptr<PlayerAsset> actor;Scene object;std::size_t source_model=0;std::vector<ModelPose> rest_pose;};
    // bhSetEneMdl realizes one model string per MDL block of the entity's asset
    // list; that order is the mbp[] the morph opcodes index. mbp[0] is the Work's
    // own model, the blocks load_models kept follow as mbp[1..].
    // nullptr when the entity has no such variant, which the original would read
    // as a NULL mlwP->objP.
    static const PlayerAsset* model_variant(const Model& model,unsigned index){
        if(!model.actor)return nullptr;
        return index==0?model.actor.get():model.actor->model(index);
    }
    /* BH_SCEWORK mode1/mode2 (types.h:1953) */
    struct Context {unsigned kind=0,index=0,node=0,mode1=0,mode2=0;unsigned mtn_md=0,mdflg=0;int hokan_rate=0;bool work=false;std::array<int,3> pos_sign{},ang_sign{};NJS_POINT3 increment{};std::array<int,3> angle_increment{};std::array<std::array<float,3>,4> positions{},angles{};std::array<std::array<float,3>,2> ground{};};
    struct Light {unsigned flags,type,speed,link,index,node,source,clock=0;NJS_POINT3 position,offset,direction,color;float near_range,far_range;NJS_POINT3 working_color{};bool color_sampled=false;};
    // CAM_KEYF_WORK: flg (0x8 = this key carries fog), fog_col/fog_nr/fog_fr.
    struct EventCamera {SceneCamera camera;unsigned frame=0,link,index,node;NJS_POINT3 offset;std::array<std::uint32_t,4> hidden_lights;unsigned flags=0;std::uint32_t fog_color=0;float fog_near=0,fog_far=0;};
    struct EventCameraTrack {std::vector<EventCamera> keys;unsigned next=0;};
    std::vector<std::uint8_t> room_;std::size_t header_;unsigned simulation_flags_=~0u;
    std::unique_ptr<PlayerPush> push_;
    GroundCollision ground_; // room-owned original sys->htp, shared by ground consumers
    // Collision-owned fields of the existing native player, not another
    // player position/work. In production, flags/status/control/system refer
    // directly to EventVm's existing banks. Fallbacks serve World-only users.
    EventVm* collision_events_=nullptr;
    unsigned collision_flags_=0x919,collision_status_=0x40000000,collision_flags2_=0;
    unsigned collision_control_=0,collision_system_=0,collision_mode_=1;
    unsigned collision_floor_sound_=0,collision_foot_effect_=0;
    int collision_floor_=0,collision_push_count_=0,collision_push_index_=0,collision_floor_index_=0;
    float collision_front_offset_=0,collision_front_target_=0; // EXP_WORK arn/arp
    NJS_POINT3 collision_previous_probe_{},collision_safe_{};
    bool collision_probe_ready_=false;
    // Der Koerperteil von bhControlPlayer laeuft im Original genau einmal je
    // Bild (player.c:1578-1655). Im Host besitzt ihn der Bewegungspass; ohne
    // Bewegungspass uebernimmt ihn die bhCheckFloorP-Stelle.
    bool collision_body_pass_=false;
    void player_body_pass(NJS_POINT3& position,Angle yaw);
    std::optional<std::size_t> collision_step_;
    MotionActor motion_actor();
    std::vector<CollisionWall> motion_steps()const;
    std::vector<CollisionWall> motion_floors()const;
    NJS_POINT3 resolve_player_motion(NJS_POINT3,NJS_POINT3,float,float,std::optional<float>,std::optional<Angle> yaw={});
    PlayerGroundWork player_ground_{};bool player_ground_ready_=false;
    std::optional<std::pair<unsigned,int>> stair_ground_state_;
    std::vector<unsigned> push_objects_;
    bool push_pose_=false,player_poisoned_=false;
    int push_health()const{return player_health()>=120?(player_poisoned_?1:0):player_health()>=30?1:2;}
    bool push_pose_current()const{return push_&&push_pose_&&!player_scripted&&!player_enemy_motion_&&motion_==std::size_t(push_->state().motion)&&frame_==push_->state().frame;}
    void ensure_push();
    void sync_push_objects();
    void update_push();
    void update_ground_geometry();
    void update_player_ground_work();
    std::vector<CollisionWall> collision_geometry()const;
    const PlayerAsset* player_asset_=nullptr;
    bool player_room_motion_=false,player_frozen_=false;
    std::int64_t player_step_=65536;
    int* player_hp_owner_=nullptr;std::uint32_t* player_head_flags_=nullptr;
    int player_ct0_=0,player_ct1_=0,player_ayp_=0,player_hokan_rate_=0;
    unsigned player_mtn_md_=0,player_hokan_count_=0,player_at_flags_=0,player_enemy_owner_=0;
    const RECVX_ENEMY_WORK* player_enemy_work_=nullptr; // plp->mnwP: the grabbing work
    unsigned player_frm_mode_=0,player_mtn_attr_=0;
    std::vector<ModelPose> player_obj_;std::vector<unsigned> player_owp_flags_;
    bool enemy_player_driving_=false;
    // owP belongs to the player model and is published by its control pass,
    // not by rendering. Enemy joint queries read the previous committed tree.
    std::vector<BoneMatrix> player_joint_matrices_;
    BoneMatrix player_link_matrix_{};
    bool player_link_matrix_valid_=false;
    std::function<void(uint32_t)> pad_clear_cb_;
    SceneSpawn& player_;std::size_t& motion_;std::int64_t& frame_;AudioMixer& audio_;
    std::array<Context,16> contexts_{};
    std::map<std::pair<unsigned,unsigned>,WorldEntity> entities_;
    mutable std::set<std::string> unbound_enemy_; // const owP/pose services report their gaps too
    unsigned enemy_game_mode_=0;
    // bhSetEffectTb from game code: `link_kind` 0 = world position, 2 = enemy joint (WorldEntity index/joint).
    int spawn_effect(const EffectSpawn& spawn,unsigned link_kind=0,unsigned link_index=0);
    void gun_fire(unsigned kind,unsigned index,const BoneMatrix& joint,unsigned wno,unsigned jno,unsigned hand,int ang);
    void process_effect_output();
    int reserve_effect_slot(unsigned start=0);
    void tick_shadow(unsigned slot);
    void write_particle_slot(unsigned slot,unsigned flags,unsigned id);
    void bomb_damage(const BombRequest& bomb);
    EffectParticles particles_;
    void tick_en01();
    // C02 enemy pass on the native works (eneset.c:344 bhControlEnemy) and
    // the room-load half (room.c:114 bhSetEneMdl -> bhSetEnemy).
    RECVX_ENEMY_TABLE enemy_table_{};RECVX_ENEMY_DISPATCH enemy_dispatch_{};RECVX_ENEMY_SERVICES enemy_services_{};
    std::vector<std::unique_ptr<unsigned char[]>> enemy_exp_blocks_;std::size_t enemy_exp_used_=0;
    // sys->mwalp entries an enemy allocated (bhEne00 eneset.c:427, en25.c:58 ...).
    // game.c:48-51 runs bhControlEnemy before bhControlObjItm, so the first
    // pass puts them ahead of the bhObj001 entries PlayerPush owns.
    std::vector<CollisionWall> enemy_mobile_walls_;
    // T3 service supplement (en03_lib.h RECVX_ENEMY_EXT): the original
    // rom->walp + sys->mwalp order for the shared en03.c:5220 wall pass.
    RECVX_ENEMY_EXT enemy_ext_{};std::vector<CollisionWall> enemy_wall_cache_;
    // Kindworks aus bhSetEnemy(&zob_lkmtab, ...) (en01.c:2817): Raumlebensdauer
    // wie ene[], stabile Adressen, weil die Tabelle nur Zeiger haelt.
    std::deque<WorldEntity> enemy_children_;
    WorldEntity* create_enemy_child(RECVX_ENEMY_WORK* parent);
    void bind_enemy_services();
    void init_enemy_works();
    void control_enemy_works();
    void control_enemy_player_motion();
    const MotionClip* player_enemy_clip()const;
    // mlwP eines Works: der Objektbaum, den er liest und schreibt. Ein Kind
    // mit `epp->mlwP = epw->mdl` (en01.c:2827) hat keinen eigenen.
    WorldEntity& tree_owner(WorldEntity& e)const{return e.model_owner?*e.model_owner:e;}
    const WorldEntity& tree_owner(const WorldEntity& e)const{return e.model_owner?*e.model_owner:e;}
    void adopt_enemy_script(WorldEntity& e);
    void publish_enemy(WorldEntity& e);
    const MotionClip* enemy_mn_work(const WorldEntity& e,std::uint32_t mtn_no)const;
    // Ist fuer diesen Work ueberhaupt ein Clip vorhanden? Szenenfiguren wie
    // Identitaet 91 in RM_9301 haben in ihrer RDT keinen emtp-Eintrag (room.c
    // legt dann eine genullte mnwP an): es laeuft dort kein bhSetMotion, also
    // ueberschreibt auch nichts einen Skript-Schreibzugriff auf objP.
    bool enemy_motion_available(const WorldEntity& e,unsigned mtn,bool room)const;
    void calc_enemy_model(WorldEntity& e);
    // pwksub.c:39 bhCalcLinkModel(pp): bhCalcModel fuer jedes aktive Work mit
    // flg 0x80, dessen lkwkp pp ist.
    void calc_enemy_link_model(WorldEntity& e);
    // event.c:11070 (Common 25), :11600 (Common 24), :11941 (Common 34): der
    // ct1-Schalter waehlt nach dem Modusaufbau die sofortige Auswertung
    // bhSetMotion(work,0,0,0) / bhCalcModel / bhCalcLinkModel.
    void enemy_script_evaluate(WorldEntity& e,unsigned mode);
    void sync_player_grab();
    // player.c:1600-1664, the tail of bhControlPlayer that bhCtrPly_mode0 does
    // not gate: the bps probe, the body bhCheckWallEx and bhCheckFloorP also run
    // in the two enemy-driven player modes (bhCPM0_nage/bhCPM0_enedie).
    void control_player_grab_collision();
    std::vector<BoneMatrix> player_joints_now()const;
    const MotionClip& enemy_library_clip(unsigned identity,unsigned index)const;
    void apply_en01_flinch();
    void init_en01(bool hard);
    void init_barrels();
    std::vector<Model> models_;
    struct SpriteWork {EffectSprite sprite;NJS_POINT3 scale;bool hidden=false;unsigned link=0,index=0,node=0;NJS_POINT3 offset{};bool player_effect=false,queued=true,dynamic=false;
        // lkwkp = &hp->flg of a room/enemy ATR work (bhEff029): its place in rom->walp + sys->mwalp.
        std::optional<std::size_t> atr_link;std::optional<WeaponFlash> weapon;};
    bool player_attacking_=false;int blast_damage_=0;bool barrels_ready_=false,enemy_ai_=true,enemy_grab_=true,player_enemy_motion_=false,grab_driving_=false,player_dead_=false,player_killed_=false;unsigned lever_bits_=0;int player_hp_=200,player_character_=0;std::int16_t player_way_=0;std::map<unsigned,std::size_t> barrel_walls_;
    float player_spd_=0; // plp->spd
    // player.c:1366-1387 bhControlPlayer: pxb/pyb/pzb of the previous pass and
    // EXP_WORK nlxb/nlyb/nlzb, the last settled root position (owP[0]).
    NJS_POINT3 player_prev_pos_{},player_last_tick_pos_{},player_nl_{};bool player_prev_valid_=false;
    // plp->djnt_no / dvx..dvz / dpx..dpz written by enemy hit routines.
    int player_hit_joint_=-1;NJS_POINT3 player_hit_dv_{},player_hit_dp_{};
    // plp->mlwP->objP[node] writes of the enemy pass; the next player motion
    // write replaces them, so they are removed at the next world tick.
    std::vector<unsigned> player_transient_nodes_;
    std::vector<std::pair<int,int>> vibration_requests_;std::optional<int> vibration_mode_request_;
    std::vector<std::pair<unsigned,float>> player_exp0_writes_;
    std::vector<CollisionWall> enemy_floor_cache_,enemy_step_cache_;
    int effect_template_ay_=0; // sys->ef.ay carried between EN01 writers (RECVX_EN01_EF_KEEP_AY)
    bool enemy_work_index(const RECVX_ENEMY_WORK* w,unsigned& index)const;
    int effect_tb(unsigned flg,int id,int type,unsigned mdlver,NJS_POINT3 position,NJS_POINT3 scale,int ax,int ay,const RECVX_ENEMY_VEC* ofs,RECVX_ENEMY_WORK* link_w,bool link_player,int lkono);
    // bhSetEffect (effect.c:331) / bhSetEffectTb ohne die Prozedurpruefung.
    int allocate_effect(const EffectSpawn& spawn,unsigned link_kind,unsigned link_index);
    unsigned player_weapon_=0,effect_yk_ct_=0,presentation_flags_=~0u; // system.c initializes pt_flg to ~0u
    unsigned effect_flags_=0; // system.c:195 sys->ef_flg = 0
    void control_player_effects();
    std::vector<CollisionWall> effect_walls_; // rom->walp + sys->mwalp as bhCheckWallType sees them
    std::vector<SpriteWork> sprites_;std::map<unsigned,std::vector<SceneTexture>> effect_textures_;std::optional<Scene> effect_model0_;
    // event.c:220 bhPushEffectWork/:234 bhPopEffectWork sichern den ganzen
    // eff[512]-Satz (hier sprites_ plus die Partikelnutzlast des Besitzers) und
    // stellen ihn auf Anforderung wieder her. Das Original kopiert in einen
    // Speicherblock und ersetzt ihn vollstaendig, deshalb ist auch hier genau
    // ein Schnappschuss aktiv.
    struct EffectPushState {std::vector<SpriteWork> works;std::shared_ptr<const EffectParticles::State> particles;};
    std::optional<EffectPushState> effect_push_;
    std::vector<std::shared_ptr<const ShadowGeometry>> shadow_models_;
    std::vector<MotionClip> room_motions_;
    std::vector<std::vector<FaceParameter>> face_sequences_;std::vector<unsigned> face_sequence_ids_;std::vector<std::vector<std::uint8_t>> lip_sequences_;
    mutable std::map<unsigned,MotionClip> room_motion_segments_;
    mutable std::map<const MotionClip*,MotionClip> slow_motion_clips_;
    struct MotionLibrary {std::vector<std::pair<std::size_t,unsigned>> entries;mutable std::map<unsigned,MotionClip> decoded,segmented;};
    std::map<unsigned,MotionLibrary> enemy_motions_;
    // Work clip (mtn_no): what the control pass samples next.
    const MotionClip& entity_motion(const WorldEntity& entity)const;
    // Object-tree clip (objP): what the last control pass actually wrote.
    const MotionClip& posed_motion_clip(const WorldEntity& entity)const;
    const MotionClip& motion_clip(const WorldEntity& entity,unsigned motion,bool room_motion)const;
    const MotionClip& entity_motion_data(const WorldEntity& entity,unsigned motion,bool room_motion)const;
    // Der verknuepfte Oberkoerperwork (bhEne01_SetLinkEnemy, en01.c:2815) teilt
    // sich Objektbaum und owP mit dem Hauptwork; sein bhSetMotion laeuft mit
    // objP/owP += 8 und obj_num -= 8 auf den Knoten 8.. (en01.c:2799-2805,
    // :10347-10353, :3145-3160). Unterer und oberer Teil liegen als getrennte
    // Clips vor: mnwP[mtn] mit 8 Kanaelen und mnwP[mtn + 0xc8] mit 10
    // (en01_walk_mtn, en01.c:1118); im Raumvorrat ist das Paar (mtn, mtn+1).
    // Ein Entity hat nur EINEN Objektbaum, deshalb fuehrt der Host beide
    // Fenster in einem Clip zusammen. Die Kanalordnung bleibt die des
    // Originals: mtn_tp = en01_flipTree (en01.c:2400) bildet Kanal k auf Knoten
    // flipTree[k] ab, und flipTree[8..17] == 8 + flipTree2[0..9] (en01.c:2407).
    // nullptr, wenn kein Paar vorliegt; der Aufrufer behaelt dann den unteren
    // Clip (kein Ersatzclipp, keine erfundene Oberkoerperpose).
    const MotionClip* segmented_clip(const MotionClip& lower,const MotionClip& upper,std::map<unsigned,MotionClip>& cache,unsigned key)const;
    std::vector<CollisionWall> walls_;
    struct FloorSound {CollisionWall area;unsigned sound;};
    std::vector<FloorSound> floor_sounds_;std::vector<CollisionWall> effect_water_room_,effect_water_mobile_;
    std::vector<EnemyArea> enemy_areas_;EnemySpawnContext enemy_spawn_;
    std::vector<CollisionWall> route_areas_;std::vector<std::uint8_t> route_table_; // rom->rutp / rom->ruttp (rutchk.c)
    unsigned footstep_slot_=4;
    std::size_t footstep_motion_=~std::size_t(0);
    std::int64_t footstep_frame_=-1;
    float room_height_=0;
    std::array<float,32> floors_{};
    std::vector<EventCameraTrack> event_cameras_;
    unsigned event_camera_key_=0,event_camera_clock_=0;
    bool event_camera_finished_=false;
    // cam.mode0 of bhControlEventCamera (cut.c:1986): 0 = init pass (event_camera_init_),
    // 1 = travelling, 2 = final hold (event_camera_finished_), 3 = held by
    // bhCamPauseSet (event_camera_paused_, event.c:1032; the switch has no case 3).
    bool event_camera_init_=false,event_camera_paused_=false;
    // cam.ips/ian/ipf/icr: the 20 Overhauser control slots of bhInitEventCamera (cut.c:1845).
    std::array<float,60> event_ips_{},event_ian_{},event_ipf_{},event_icr_{};
    struct EventCameraLock {unsigned flg=0,no=0,ono=0;NJS_POINT3 offset{};};
    std::array<const EventCamera*,20> event_camera_control_keys()const;
    EventCameraLock event_camera_lock_fields(unsigned track,unsigned slot)const; // raw CAM_KEYF_WORK slot; kfp[1] past keyf_n reads the following slot
    NJS_POINT3 event_camera_lock_position(const EventCamera& key); // cut.c:2391 bhGetEvtCamLockPosition
    void event_camera_lock_angles(const EventCamera& key,std::int16_t& ax,std::int16_t& ay); // cut.c:2248 / :1682
    void init_event_camera();       // cut.c:1845 bhInitEventCamera
    void check_event_camera_lock(); // cut.c:2221 bhCheckEvtCamLockPosition
    void control_event_camera();    // cut.c:1986 bhControlEventCamera
    SceneCamera event_camera_pose_{};
    // Native camera owner's installed njClipZ and last GameNear/GameFar.
    // Flag/key mutations alone do not invoke either original clip routine.
    ViewClip installed_clip_{},room_clip_{};
    std::optional<unsigned> clip_cut_;
    bool restore_camera_clip_=false;
    ActiveCameraMotion active_camera_;std::vector<SceneCamera> room_cameras_;
    bool camera_force_=false;
    // Fog owner (sync.c:143-255 bhControlEOR): cam.fog_* as handed over by
    // bhSetCut/bhSetFixedCut/bhSetEventCamera, sys->fog_ct/fog_cngct, the
    // installed njGenerateFogTable3 state and the njSetFogColor colour.
    std::uint32_t cam_fog_color_=0;float cam_fog_near_=0,cam_fog_far_=0;unsigned cut_fog_flags_=0; // rom->cutp[cam.ncut].cam[camver].flg
    int fog_ct_=0,fog_cngct_=2; // system.c:156: 3 with ss_flg bit 1 (bhChangeHWSetting), else 2; the host has no ss_flg bit 1 owner
    FogState fog_;std::uint32_t fog_color_=0;
    std::uint32_t script_fog_color_=0; // sys->fog_col (bhFogColorSet 0x4c), read while st_flg 0x100000
    void cut_fog_handover(const SceneCamera& ci,bool set_cut); // cut.c:349-377 bhSetCut / :463-484 bhSetFixedCut
    void control_fog(); // sync.c:143-255
    void install_fog(); // njSetFogColor + njGenerateFogTable3 + njSetFogTable + njFogEnable
    std::vector<Light> lights_,event_lights_;
    std::vector<NJS_POINT3> positions_;
    mutable std::map<std::pair<unsigned,unsigned>,std::vector<BoneMatrix>> actor_nodes_;
    std::array<NJS_POINT3,4> ambient_{};
    std::array<unsigned,4> ambient_slots_{};
    float fade_=0,fade_target_=0,fade_step_=0;unsigned fade_color_=0;
    std::uint32_t background_=0,background_target_=0;unsigned background_delay_=0;bool background_pending_=false;
    std::map<unsigned,ItemBoxObjectWork> item_boxes_; // O_WRK mode0/ct0 of type-100 objects
    WorldEntity& entity(unsigned kind,unsigned index);
    const Model& model(unsigned kind,unsigned index)const;
    unsigned model_index(unsigned kind,unsigned index,unsigned script_node)const;
    const ModelNodeOverrides& overrides(unsigned kind,unsigned index)const;
    bool entity_hidden(unsigned kind,unsigned index)const;
    // objitm.c:562 (bhDrawObjItm, Items), :659 (bhDrawObject) und :688
    // (bhDrawSpObject) teilen dieselbe Bedingung: ein Werk mit flg 0x40000000
    // wird nur gezeichnet, wenn sys->gm_flg & 0x40, cam.flg & 0x46 oder
    // cam.ncut einem seiner vier hide[]-Eintraege entspricht.
    bool object_cut_hidden(const WorldEntity& state)const;
    std::vector<ModelPose> object_pose(const Model& model,const WorldEntity& state)const;
    std::vector<BoneMatrix> object_matrices(const Model& model,const WorldEntity& entity,const NJS_MATRIX& transform)const;
    ModelPose model_pose_physical(unsigned kind,unsigned index,unsigned physical)const; // objP[physical] of the original model
    std::optional<BoneMatrix> link_matrix(unsigned kind,unsigned index,unsigned node,unsigned depth=0)const;
    NJS_POINT3 link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const;
};
}
