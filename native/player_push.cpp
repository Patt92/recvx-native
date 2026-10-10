#include "player_push.h"
#include <stdexcept>
namespace recvx {
static RECVX_STAIR_AREA atr_wall(const CollisionWall& w){return {(unsigned char)w.flags,(unsigned char)w.type,(unsigned char)w.id,(signed char)w.floor,w.attributes,w.position.x,w.position.y,w.position.z,w.extent.x,w.extent.y,w.extent.z,(unsigned char)w.parameters[0],(unsigned char)w.parameters[1],(unsigned char)w.parameters[2],(unsigned char)w.parameters[3]};}
static CollisionWall collision_wall(const RECVX_STAIR_AREA& a){return {a.flg,a.type,a.id,a.flr_no,a.attr,{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}};}

PlayerPush::PlayerPush(float room_height,const std::array<float,32>& grand,const std::vector<RECVX_PUSH_OBJECT>& objects,const PlayerAsset& asset,const RECVX_PUSH_HOST& host):asset_(asset){
 core_=recvx_push_create(room_height,grand.data(),objects.data(),unsigned(objects.size()),&host);if(!core_)throw std::bad_alloc();
 std::vector<unsigned> frames(asset.motions());for(unsigned i=0;i<frames.size();++i)frames[i]=asset.has_motion(i)?asset.motion_frames(i):0;
 // PlyTrsZ[4][0..2] is curve 9 for every dmlvl (player.c:189).
 const auto& curve=asset.translation_curve(9);
 recvx_push_motions(core_,frames.data(),unsigned(frames.size()),curve.data(),unsigned(curve.size()));
}
PlayerPush::~PlayerPush(){recvx_push_destroy(core_);}
void PlayerPush::update_walls(const std::vector<CollisionWall>& prefix){std::vector<RECVX_STAIR_AREA> atr;atr.reserve(prefix.size());for(const auto& w:prefix)atr.push_back(atr_wall(w));recvx_push_update_walls(core_,atr.data(),unsigned(atr.size()));}
void PlayerPush::update_object(unsigned i,NJS_POINT3 p){recvx_push_update_object(core_,i,p.x,p.y,p.z);}
void PlayerPush::control(unsigned cb_flg,int psh_idx,unsigned stflg){state_.wall=psh_idx;recvx_push_control(core_,&state_,cb_flg,psh_idx,stflg);}
void PlayerPush::store_walls(const std::vector<CollisionWall>& combined,std::size_t base){
 const unsigned n=recvx_push_wall_count(core_);if(base+n>combined.size())throw std::runtime_error("player wall pass lost bhObj001 entries");
 std::vector<unsigned> attr(n);for(unsigned i=0;i<n;++i)attr[i]=combined[base+i].attributes;recvx_push_wall_attributes(core_,attr.data(),n);
}
bool PlayerPush::begin(int psh_ct,const SceneSpawn& actor,int dmlvl,int wpntp,unsigned& flg,unsigned& stflg,unsigned& st_flg){
 auto next=state_;next.flags=flg;next.status=stflg;next.st_flg=st_flg;
 if(!recvx_push_begin(&next,psh_ct,actor.position.x,actor.position.y,actor.position.z,actor.yaw&65535,dmlvl,wpntp))return false;
 state_=next;flg=state_.flags;stflg=state_.status;st_flg=state_.st_flg;return true;
}
bool PlayerPush::tick(bool forward,unsigned& flg,unsigned& stflg,unsigned& st_flg,unsigned& flg2,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame){
 if(!active())return false;
 state_.flags=flg;state_.status=stflg;state_.st_flg=st_flg;state_.flg2=flg2;
 state_.x=actor.position.x;state_.y=actor.position.y;state_.z=actor.position.z;
 recvx_push_tick(core_,&state_,forward?1:0);
 flg=state_.flags;stflg=state_.status;st_flg=state_.st_flg;flg2=state_.flg2;
 actor.position={state_.x,state_.y,state_.z};actor.yaw=state_.ay;
 if(asset_.has_motion(std::size_t(state_.motion))){motion=std::size_t(state_.motion);frame=state_.frame;}
 return true;
}
NJS_POINT3 PlayerPush::object(unsigned i)const{float p[3];recvx_push_object(core_,i,p);return {p[0],p[1],p[2]};}
std::vector<CollisionWall> PlayerPush::walls()const{const RECVX_STAIR_AREA* atr;const unsigned n=recvx_push_walls(core_,&atr);std::vector<CollisionWall> out;out.reserve(n);for(unsigned i=0;i<n;++i)out.push_back(collision_wall(atr[i]));return out;}
std::vector<RECVX_STAIR_AREA> PlayerPush::steps()const{const RECVX_STAIR_AREA* areas;const unsigned n=recvx_push_steps(core_,&areas);return {areas,areas+n};}
std::vector<RECVX_STAIR_AREA> PlayerPush::floors()const{const RECVX_STAIR_AREA* areas;const unsigned n=recvx_push_floors(core_,&areas);return {areas,areas+n};}
ModelNodeOverrides PlayerPush::pose()const{
 ModelNodeOverrides out;
 if(!(state_.mtn_md&8)||!asset_.has_motion(std::size_t(state_.motion)))return out;
 // Motion.c bhSetMotion: mode & 8 selects SetMtnSlow whatever the clip's own
 // 0x20 bit says; every channel is sampled between its two keys.
 auto clip=asset_.motion_clip(std::size_t(state_.motion));clip.interpolate=true;
 const auto pose=asset_.local_pose_clip(clip,state_.frame,{},{});
 for(unsigned n=0;n<pose.size();++n)out[n]={pose[n].position,std::array<Angle,3>{pose[n].angles[0],pose[n].angles[1],pose[n].angles[2]}};
 return out;
}
}
