#pragma once
#include "actor.h"
#include "collision.h"
#include "player_push_core.h"
namespace recvx {
class PlayerPush {
public:
 PlayerPush(const std::vector<CollisionWall>&,float,const std::array<float,32>&,const std::vector<RECVX_PUSH_OBJECT>&,const PlayerAsset&,const RECVX_PUSH_HOST&);
 ~PlayerPush();
 PlayerPush(const PlayerPush&)=delete;PlayerPush& operator=(const PlayerPush&)=delete;
 NJS_POINT3 contact(NJS_POINT3 from,NJS_POINT3 to,int yaw,int floor,bool walking,int health,int weapon,std::size_t motion,std::int64_t frame);
 bool tick(bool forward,int character,int health,int weapon,SceneSpawn&,std::size_t&,std::int64_t&);
 bool active()const{return state_.active;}
 const RECVX_PUSH_STATE& state()const{return state_;}
 void enabled(unsigned flags){recvx_push_enabled(core_,flags);}
 void update_object(unsigned i,NJS_POINT3 p,bool hidden);
 void terrain(const std::vector<RECVX_STAIR_AREA>&,const std::vector<RECVX_STAIR_AREA>&);
 void update_walls(const std::vector<CollisionWall>&);
 std::vector<CollisionWall> walls()const;
 std::vector<RECVX_STAIR_AREA> steps()const;
 std::vector<RECVX_STAIR_AREA> floors()const;
 NJS_POINT3 object(unsigned i)const;
 ModelNodeOverrides pose()const;
private:
 const PlayerAsset& asset_;RECVX_PUSH_CORE* core_=nullptr;RECVX_PUSH_STATE state_{};
 std::vector<RECVX_PUSH_MOTION> motions_;
 std::vector<std::vector<RECVX_PUSH_CHANNEL>> channels_;
 std::vector<std::vector<std::vector<RECVX_PUSH_ANGLE>>> angles_;
 std::vector<std::vector<std::vector<RECVX_PUSH_POSITION>>> positions_;
 std::vector<RECVX_PUSH_POSE> poses_;
 void seed(std::size_t,std::int64_t);
};
}
