#pragma once
#include "actor.h"
#include "audio.h"
#include "events.h"
#include "renderer.h"
#include "collision.h"
#include <map>
#include <memory>
namespace recvx {
struct WorldEntity {SceneSpawn transform{};std::array<int,3> angles{};unsigned identity=0,death_ticks=0;int hp=0;bool dead=false;bool hidden=false,inactive=false,frozen=false,scripted=false,has_motion=false,room_motion=true;unsigned motion=0;std::int64_t frame=0,step=65536;bool linked=false;unsigned parent_kind=0,parent_index=0,parent_node=0;NJS_POINT3 link_offset{};std::map<unsigned,bool> hidden_nodes;};
class RoomWorld {
public:
    RoomWorld(const std::vector<std::uint8_t>& room,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data={},const std::filesystem::path& mods={},const std::string& room_name="RM_0000");
    struct CombatTarget {unsigned index,identity;NJS_POINT3 position;float radius,height;int hp;};
    std::vector<CombatTarget> combat_targets()const;
    void combat_difficulty(unsigned mode);
    bool damage(unsigned index,int amount);
    bool enemy_dead(unsigned index)const;
    // bhEneSetCk: original BH_PWORK stflg 0x1000000 (not drawn, no hit/sight checks).
    void deactivate_enemy(unsigned index);
    float shot_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range)const{return wall_ray_distance(walls_,room_height_,origin,direction,range,floor_at(origin));}
    bool bind(const EventCommand& command);
    void set_player_asset(const PlayerAsset* player){player_asset_=player;}
    void tick();
    Scene sample_player(const PlayerAsset& player,std::vector<BoneMatrix>* nodes=nullptr)const;
    void append(Scene& scene,const std::vector<BoneMatrix>& player_nodes={})const;
    std::size_t export_textures(const std::filesystem::path& output,const std::string& room_name)const;
    SceneCamera camera(const Scene& stage,const SceneCamera& active)const;
    void overlay(SoftwareRenderer& image)const;
    unsigned frame(unsigned kind,unsigned index)const;
    bool player_hidden=false,player_scripted=false;
    bool event_camera=false;
    unsigned event_camera_index=0;
    unsigned event_camera_key()const{return event_camera_key_;}
    int floor_at(NJS_POINT3 position)const;
    unsigned floor_sound(NJS_POINT3 position,bool special=false)const;
    void player_footstep(const PlayerAsset& player);
    NJS_POINT3 move(NJS_POINT3 from,NJS_POINT3 to,float radius=3.5f,float height=16.5f)const;
private:
    struct Model {unsigned kind,index;std::unique_ptr<PlayerAsset> actor;Scene object;};
    struct Context {unsigned kind=0,index=0,node=0;std::array<int,3> pos_sign{},ang_sign{};NJS_POINT3 increment{};std::array<int,3> angle_increment{};std::array<std::array<float,3>,4> positions{},angles{};};
    struct Light {unsigned flags,type,speed,link,index,node,source,clock=0;NJS_POINT3 position,offset,direction,color;float near_range,far_range;};
    struct EventCamera {SceneCamera camera;unsigned link,index,node;NJS_POINT3 offset;std::array<std::uint32_t,4> hidden_lights;};
    std::vector<std::uint8_t> room_;std::size_t header_;
    const PlayerAsset* player_asset_=nullptr;
    bool player_room_motion_=false,player_frozen_=false;
    std::int64_t player_step_=65536;
    SceneSpawn& player_;std::size_t& motion_;std::int64_t& frame_;AudioMixer& audio_;
    std::array<Context,16> contexts_{};
    std::map<std::pair<unsigned,unsigned>,WorldEntity> entities_;
    std::vector<Model> models_;
    std::vector<MotionClip> room_motions_;
    struct MotionLibrary {std::vector<std::pair<std::size_t,unsigned>> entries;mutable std::map<unsigned,MotionClip> decoded,segmented;};
    std::map<unsigned,MotionLibrary> enemy_motions_;
    const MotionClip& entity_motion(const WorldEntity& entity)const;
    std::vector<CollisionWall> walls_;
    struct FloorSound {CollisionWall area;unsigned sound;};
    std::vector<FloorSound> floor_sounds_;
    unsigned footstep_slot_=4;
    std::size_t footstep_motion_=~std::size_t(0);
    std::int64_t footstep_frame_=-1;
    float room_height_=0;
    std::array<float,32> floors_{};
    std::vector<std::vector<EventCamera>> event_cameras_;
    unsigned event_camera_key_=0;
    std::vector<Light> lights_,event_lights_;
    std::vector<NJS_POINT3> positions_;
    mutable std::map<std::pair<unsigned,unsigned>,std::vector<BoneMatrix>> actor_nodes_;
    std::array<NJS_POINT3,4> ambient_{};
    std::array<unsigned,4> ambient_slots_{};
    float fade_=0,fade_target_=0,fade_step_=0;unsigned fade_color_=0;
    std::uint32_t background_=0,background_target_=0;unsigned background_delay_=0;bool background_pending_=false;
    WorldEntity& entity(unsigned kind,unsigned index);
    bool entity_hidden(unsigned kind,unsigned index)const;
    NJS_POINT3 link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const;
};
}
