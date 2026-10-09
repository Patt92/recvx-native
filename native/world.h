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
#include "effect_particles.h"
#include "object_itmbox.h"
#include <set>
#include <string>
#include <map>
#include <memory>
namespace recvx {
struct PlayerGroundWork {NJS_POINT3 ground{};NJS_POINT3 water_c1{},water_c2{};float water_radius=0;std::optional<GroundHit> hit;};
struct WorldEntity {
    std::vector<FaceParameter> face_keys;unsigned face_frame=0,face_flags=0;
    // face.c:321 bhSetLip only stores the selected stream in exp0[268];
    // face.c:141 bhControlMask copies it into LIP_WORK.top/ptr while
    // lip->flag == 0. A running stream therefore keeps its own source.
    std::optional<FaceParameter> facial;std::vector<std::uint8_t> lip_stream,lip_current_stream;unsigned lip_bank=0,lip_pos=0,lip_count=0,lip_time=0,lip_cur=0,lip_next=0;bool lip_active=false;
RECVX_SUBPLAYER subplayer{};bool subplayer_bound=false,subplayer_no_motion=false;SceneSpawn transform{};std::array<int,3> angles{};unsigned identity=0,type=0,death_ticks=0,object_flags=0,object_parameter=0;int object_floor=0;int hp=0;bool dead=false,combat_enabled=true;bool hidden=false,inactive=false,frozen=false,scripted=false,has_motion=false,room_motion=true;unsigned motion=0;std::int64_t frame=0,step=65536;bool linked=false;unsigned parent_kind=0,parent_index=0,parent_node=0;NJS_POINT3 link_offset{};std::map<unsigned,bool> hidden_nodes;ModelNodeOverrides node_overrides;std::shared_ptr<RECVX_EN01> en01;unsigned upper_motion=0;bool flinch=false;std::array<int,2> flinch_angles{};int neck_angle=0;
    // bhEneLookFlgSet (event.c): original BH_PWORK flg bit 0x8000. Enemy AI
    // consumers (en01.c) are still pending; the state itself is exact.
    bool look=false;
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
    bool morph=false;float morph_shape=0;unsigned morph_obj_a=0,morph_obj_b=0;};
class RoomWorld {
public:
    RoomWorld(const std::vector<std::uint8_t>& room,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data={},const std::filesystem::path& mods={},const std::string& room_name="RM_0000",unsigned game_mode=0);
    struct CombatTarget {unsigned index,identity;NJS_POINT3 position;float radius,height;int hp;};
    std::vector<CombatTarget> combat_targets()const;
    void combat_difficulty(unsigned mode);
    // weapon.c bhCheckEnemies/bhSetGunFire: assigns one hit (joint damage, hit position/direction) to the en01 work.
    bool damage(unsigned index,int weapon,int amount,unsigned comb_flg,NJS_POINT3 position,NJS_POINT3 direction,int hit_joint=-1);
    // Original en01 routines the damage/death port called while they are still unbound (reported, never hidden).
    const std::set<std::string>& unbound_enemy_routines()const{return unbound_enemy_;}
    std::size_t blood_particles()const{return particles_.active();}
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
    bool player_morph()const{return player_morph_.morph;}float player_morph_shape()const{return player_morph_.morph_shape;}
    const EffectSprite* effect_state(unsigned index)const{return index<sprites_.size()?&sprites_[index].sprite:nullptr;}
    bool effect_visible(unsigned index)const{if(index>=sprites_.size())return false;const auto& w=sprites_[index];const auto atlas=effect_textures_.find(w.sprite.tex_id);return !w.hidden&&w.queued&&!(w.sprite.flg&0x1000000)&&atlas!=effect_textures_.end()&&w.sprite.ani_ct<atlas->second.size();}
    ModelPose model_pose(unsigned kind,unsigned index,unsigned script_node=0)const;
    // sp_evt.c bhDrawSpEvtComCharacter: the special-event computer draws
    // rom->enep[chr_no] into its character window. The original writes the
    // script's mtn_no/frm_no and a zero position/angle into that enemy work
    // and draws the object tree with njCnkEasyDraw (white light from
    // (4,6,10), diffuse 1.0, ambient 0.2). Returns nothing when the room has
    // no such enemy or its motion library lacks the clip; the caller reports
    // that as a gap instead of drawing a substitute.
    std::optional<Scene> computer_character(unsigned chr_no,unsigned motion,unsigned frame_no);
    void set_player_asset(const PlayerAsset* player){player_asset_=player;}
    void set_player_character(unsigned character){player_character_=int(character&3);}
    void set_effect_counter(unsigned yk_ct){effect_yk_ct_=yk_ct;}
    int spawn_player_effect(const EffectSpawn& effect){return spawn_effect(effect,1,0);}
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
    // Zombie grab/bite (needs the AI). Default off; the debug menu switches it.
    void set_enemy_grab(bool enabled){enemy_grab_=enabled;}
    bool enemy_grab()const{return enemy_grab_;}
    // sys->pad_ps while grabbed: newly pressed direction bits (0xf) and buttons (0x2d00); bhEne_LeverCheck shakes the zombie off.
    void set_player_lever(unsigned pad_ps){lever_bits_=pad_ps;}
    bool player_grabbed()const{return grab_driving_;}
    bool player_dead()const{return player_dead_;}
    bool player_killed()const{return player_killed_;}
    void set_player_attack(bool attacking){player_attacking_=attacking;} // plp->at_flg & 2
    void set_player_weapon(unsigned wpnr_no){player_weapon_=wpnr_no;} // plp->wpnr_no for bhControlPlayer effects
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
    void tick_camera(const Scene& stage,const SceneCamera& target,std::size_t cut,bool tracking=true);
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
    void player_footstep(const PlayerAsset& player);
    NJS_POINT3 move(NJS_POINT3 from,NJS_POINT3 to,float radius=3.5f,float height=16.5f,std::optional<bool> walking={});
    void bind_player_collision(EventVm&);
    NJS_POINT3 move_player(NJS_POINT3 from,NJS_POINT3 to,int movement,bool run,int turn=0);
    void check_player_floor(NJS_POINT3 position,Angle yaw,int floor);
private:
    struct Model {unsigned kind,index;std::unique_ptr<PlayerAsset> actor;Scene object;std::size_t source_model=0;std::vector<ModelPose> rest_pose;};
    struct Context {unsigned kind=0,index=0,node=0,mode1=0;bool work=false;std::array<int,3> pos_sign{},ang_sign{};NJS_POINT3 increment{};std::array<int,3> angle_increment{};std::array<std::array<float,3>,4> positions{},angles{};std::array<std::array<float,3>,2> ground{};};
    struct Light {unsigned flags,type,speed,link,index,node,source,clock=0;NJS_POINT3 position,offset,direction,color;float near_range,far_range;};
    struct EventCamera {SceneCamera camera;unsigned frame=0,link,index,node;NJS_POINT3 offset;std::array<std::uint32_t,4> hidden_lights;};
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
    float collision_front_offset_=0;
    NJS_POINT3 collision_previous_probe_{},collision_safe_{};
    bool collision_probe_ready_=false;
    std::optional<std::size_t> collision_step_;
    MotionActor motion_actor();
    std::vector<CollisionWall> motion_steps()const;
    std::vector<CollisionWall> motion_floors()const;
    NJS_POINT3 resolve_player_motion(NJS_POINT3,NJS_POINT3,float,float,std::optional<float>);
    PlayerGroundWork player_ground_{};bool player_ground_ready_=false;
    std::optional<std::pair<unsigned,int>> stair_ground_state_;
    std::vector<unsigned> push_objects_;
    bool push_pose_=false,player_poisoned_=false;
    int push_health()const{return player_hp_>=120?(player_poisoned_?1:0):player_hp_>=30?1:2;}
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
    SceneSpawn& player_;std::size_t& motion_;std::int64_t& frame_;AudioMixer& audio_;
    std::array<Context,16> contexts_{};
    std::map<std::pair<unsigned,unsigned>,WorldEntity> entities_;
    std::set<std::string> unbound_enemy_;
    unsigned enemy_game_mode_=0;
    // bhSetEffectTb from game code: `link_kind` 0 = world position, 2 = enemy joint (WorldEntity index/joint).
    int spawn_effect(const EffectSpawn& spawn,unsigned link_kind=0,unsigned link_index=0);
    void process_effect_output();
    void bomb_damage(const BombRequest& bomb);
    EffectParticles particles_;
    void tick_en01();
    void sync_player_grab();
    std::vector<BoneMatrix> player_joints_now()const;
    const MotionClip& enemy_library_clip(unsigned identity,unsigned index)const;
    void apply_en01_flinch();
    void init_en01(bool hard);
    void init_barrels();
    std::vector<Model> models_;
    struct SpriteWork {EffectSprite sprite;NJS_POINT3 scale;bool hidden=false;unsigned link=0,index=0,node=0;NJS_POINT3 offset{};bool player_effect=false,queued=true,dynamic=false;};
    bool player_attacking_=false;int blast_damage_=0;bool barrels_ready_=false,enemy_ai_=false,enemy_grab_=true,player_enemy_motion_=false,grab_driving_=false,player_dead_=false,player_killed_=false;unsigned lever_bits_=0;int player_hp_=200,player_character_=0;std::map<unsigned,std::size_t> barrel_walls_;
    unsigned player_weapon_=0,effect_yk_ct_=0;bool lighter_=false; // plp->flg 0x1000000, sys->yk_ct
    bool lighter_fire_=false; // sys->st_flg 0x20000000 (bhSetLighterFire)
    void control_player_effects();
    std::vector<SpriteWork> sprites_;std::map<unsigned,std::vector<SceneTexture>> effect_textures_;std::optional<Scene> effect_model0_;
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
    std::vector<CollisionWall> walls_;
    struct FloorSound {CollisionWall area;unsigned sound;};
    std::vector<FloorSound> floor_sounds_;std::vector<CollisionWall> effect_water_room_,effect_water_mobile_;
    std::vector<EnemyArea> enemy_areas_;EnemySpawnContext enemy_spawn_;
    unsigned footstep_slot_=4;
    std::size_t footstep_motion_=~std::size_t(0);
    std::int64_t footstep_frame_=-1;
    float room_height_=0;
    std::array<float,32> floors_{};
    std::vector<EventCameraTrack> event_cameras_;
    unsigned event_camera_key_=0,event_camera_clock_=0;
    bool event_camera_finished_=false;
    SceneCamera event_camera_pose_{};
    ActiveCameraMotion active_camera_;std::vector<SceneCamera> room_cameras_;
    bool camera_force_=false;
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
    std::vector<ModelPose> object_pose(const Model& model,const WorldEntity& state)const;
    std::vector<BoneMatrix> object_matrices(const Model& model,const WorldEntity& entity,const NJS_MATRIX& transform)const;
    ModelPose model_pose_physical(unsigned kind,unsigned index,unsigned physical)const; // objP[physical] of the original model
    std::optional<BoneMatrix> link_matrix(unsigned kind,unsigned index,unsigned node,unsigned depth=0)const;
    NJS_POINT3 link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const;
};
}
