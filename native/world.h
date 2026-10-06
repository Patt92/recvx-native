#pragma once
#include "actor.h"
#include "audio.h"
#include "events.h"
#include "renderer.h"
#include "collision.h"
#include "effect_sprites.h"
#include "enemy_spawn.h"
#include "subplayer.h"
#include "en01_damage.h"
#include <set>
#include <string>
#include <map>
#include <memory>
namespace recvx {
struct WorldEntity {RECVX_SUBPLAYER subplayer{};bool subplayer_bound=false,subplayer_no_motion=false;SceneSpawn transform{};std::array<int,3> angles{};unsigned identity=0,type=0,death_ticks=0;int hp=0;bool dead=false,combat_enabled=true;bool hidden=false,inactive=false,frozen=false,scripted=false,has_motion=false,room_motion=true;unsigned motion=0;std::int64_t frame=0,step=65536;bool linked=false;unsigned parent_kind=0,parent_index=0,parent_node=0;NJS_POINT3 link_offset{};std::map<unsigned,bool> hidden_nodes;ModelNodeOverrides node_overrides;std::shared_ptr<RECVX_EN01> en01;};
class RoomWorld {
public:
    RoomWorld(const std::vector<std::uint8_t>& room,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data={},const std::filesystem::path& mods={},const std::string& room_name="RM_0000");
    struct CombatTarget {unsigned index,identity;NJS_POINT3 position;float radius,height;int hp;};
    std::vector<CombatTarget> combat_targets()const;
    void combat_difficulty(unsigned mode);
    // weapon.c bhCheckEnemies/bhSetGunFire: assigns one hit (joint damage, hit position/direction) to the en01 work.
    bool damage(unsigned index,int weapon,int amount,unsigned comb_flg,NJS_POINT3 position,NJS_POINT3 direction);
    // Original en01 routines the damage/death port called while they are still unbound (reported, never hidden).
    const std::set<std::string>& unbound_enemy_routines()const{return unbound_enemy_;}
    bool enemy_dead(unsigned index)const;
    void set_enemy_health(unsigned index,int hp);
    // bhEneSetCk: original BH_PWORK stflg 0x1000000 (not drawn, no hit/sight checks).
    void deactivate_enemy(unsigned index);
    bool step_clear(NJS_POINT3 probe,float radius=3.5f,float height=16.5f)const;
    float shot_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range)const{return wall_ray_distance(walls_,room_height_,origin,direction,range,floor_at(origin));}
    float combat_wall_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,unsigned flags=0x400)const{return wall_ray_distance(walls_,room_height_,origin,direction,range,-32768,flags);}
    const std::vector<SceneTexture>* weapon_flash_textures()const{const auto at=effect_textures_.find(3);return at==effect_textures_.end()?nullptr:&at->second;}
    bool bind(const EventCommand& command);
    const WorldEntity* state(unsigned kind,unsigned index)const;
    const EffectSprite* effect_state(unsigned index)const{return index<sprites_.size()?&sprites_[index].sprite:nullptr;}
    bool effect_visible(unsigned index)const{if(index>=sprites_.size())return false;const auto& w=sprites_[index];const auto atlas=effect_textures_.find(w.sprite.tex_id);return !w.hidden&&w.queued&&!(w.sprite.flg&0x1000000)&&atlas!=effect_textures_.end()&&w.sprite.ani_ct<atlas->second.size();}
    ModelPose model_pose(unsigned kind,unsigned index,unsigned script_node=0)const;
    void set_player_asset(const PlayerAsset* player){player_asset_=player;}
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
    bool event_camera=false;
    unsigned event_camera_index=0;
    std::optional<unsigned> fixed_camera;
    unsigned event_camera_key()const{return event_camera_key_;}
    void tick_camera(const Scene& stage,const SceneCamera& target,std::size_t cut,bool tracking=true);
    bool camera_transitioning()const{return active_camera_.transitioning();}
    unsigned camera_clock()const{return active_camera_.clock();}
    int floor_at(NJS_POINT3 position)const;
    const std::array<float,32>& floor_heights()const{return floors_;} // rom->grand
    unsigned floor_sound(NJS_POINT3 position,bool special=false)const;
    void player_footstep(const PlayerAsset& player);
    NJS_POINT3 move(NJS_POINT3 from,NJS_POINT3 to,float radius=3.5f,float height=16.5f)const;
private:
    struct Model {unsigned kind,index;std::unique_ptr<PlayerAsset> actor;Scene object;std::size_t source_model=0;std::vector<ModelPose> rest_pose;};
    struct Context {unsigned kind=0,index=0,node=0,mode1=0;std::array<int,3> pos_sign{},ang_sign{};NJS_POINT3 increment{};std::array<int,3> angle_increment{};std::array<std::array<float,3>,4> positions{},angles{};std::array<std::array<float,3>,2> ground{};};
    struct Light {unsigned flags,type,speed,link,index,node,source,clock=0;NJS_POINT3 position,offset,direction,color;float near_range,far_range;};
    struct EventCamera {SceneCamera camera;unsigned link,index,node;NJS_POINT3 offset;std::array<std::uint32_t,4> hidden_lights;};
    std::vector<std::uint8_t> room_;std::size_t header_;
    const PlayerAsset* player_asset_=nullptr;
    bool player_room_motion_=false,player_frozen_=false;
    std::int64_t player_step_=65536;
    SceneSpawn& player_;std::size_t& motion_;std::int64_t& frame_;AudioMixer& audio_;
    std::array<Context,16> contexts_{};
    std::map<std::pair<unsigned,unsigned>,WorldEntity> entities_;
    std::set<std::string> unbound_enemy_;
    void tick_en01();
    void init_en01(bool hard);
    std::vector<Model> models_;
    struct SpriteWork {EffectSprite sprite;NJS_POINT3 scale;bool hidden=false;unsigned link=0,index=0,node=0;NJS_POINT3 offset{};bool player_effect=false,queued=true;};
    bool player_attacking_=false;
    unsigned player_weapon_=0;bool lighter_=false; // plp->flg 0x1000000
    bool lighter_fire_=false; // sys->st_flg 0x20000000 (bhSetLighterFire)
    void control_player_effects();
    std::vector<SpriteWork> sprites_;std::map<unsigned,std::vector<SceneTexture>> effect_textures_;
    std::vector<MotionClip> room_motions_;
    mutable std::map<unsigned,MotionClip> room_motion_segments_;
    struct MotionLibrary {std::vector<std::pair<std::size_t,unsigned>> entries;mutable std::map<unsigned,MotionClip> decoded,segmented;};
    std::map<unsigned,MotionLibrary> enemy_motions_;
    const MotionClip& entity_motion(const WorldEntity& entity)const;
    std::vector<CollisionWall> walls_;
    struct FloorSound {CollisionWall area;unsigned sound;};
    std::vector<FloorSound> floor_sounds_;
    std::vector<EnemyArea> enemy_areas_;EnemySpawnContext enemy_spawn_;
    unsigned footstep_slot_=4;
    std::size_t footstep_motion_=~std::size_t(0);
    std::int64_t footstep_frame_=-1;
    float room_height_=0;
    std::array<float,32> floors_{};
    std::vector<std::vector<EventCamera>> event_cameras_;
    unsigned event_camera_key_=0;
    ActiveCameraMotion active_camera_;std::vector<SceneCamera> room_cameras_;
    bool camera_force_=false;
    std::vector<Light> lights_,event_lights_;
    std::vector<NJS_POINT3> positions_;
    mutable std::map<std::pair<unsigned,unsigned>,std::vector<BoneMatrix>> actor_nodes_;
    std::array<NJS_POINT3,4> ambient_{};
    std::array<unsigned,4> ambient_slots_{};
    float fade_=0,fade_target_=0,fade_step_=0;unsigned fade_color_=0;
    std::uint32_t background_=0,background_target_=0;unsigned background_delay_=0;bool background_pending_=false;
    WorldEntity& entity(unsigned kind,unsigned index);
    const Model& model(unsigned kind,unsigned index)const;
    unsigned model_index(unsigned kind,unsigned index,unsigned script_node)const;
    const ModelNodeOverrides& overrides(unsigned kind,unsigned index)const;
    bool entity_hidden(unsigned kind,unsigned index)const;
    std::vector<BoneMatrix> object_matrices(const Model& model,const WorldEntity& entity,const NJS_MATRIX& transform)const;
    std::optional<BoneMatrix> link_matrix(unsigned kind,unsigned index,unsigned node,unsigned depth=0)const;
    NJS_POINT3 link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const;
};
}
