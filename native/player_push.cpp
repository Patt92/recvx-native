#include "player_push.h"
#include <stdexcept>
namespace recvx {
static RECVX_STAIR_AREA atr_wall(const CollisionWall& w){return {(unsigned char)w.flags,(unsigned char)w.type,(unsigned char)w.id,(signed char)w.floor,w.attributes,w.position.x,w.position.y,w.position.z,w.extent.x,w.extent.y,w.extent.z,(unsigned char)w.parameters[0],(unsigned char)w.parameters[1],(unsigned char)w.parameters[2],(unsigned char)w.parameters[3]};}

PlayerPush::PlayerPush(const std::vector<CollisionWall>& walls,float height,const std::array<float,32>& floors,const std::vector<RECVX_PUSH_OBJECT>& objects,const PlayerAsset& asset,const RECVX_PUSH_HOST& host):asset_(asset){
 std::vector<RECVX_STAIR_AREA> atr;for(const auto& w:walls)atr.push_back(atr_wall(w));
 motions_.resize(asset.motions());channels_.resize(asset.motions());angles_.resize(asset.motions());positions_.resize(asset.motions());poses_.resize(asset.bones());
 for(unsigned i=0;i<asset.motions();++i)if(asset.has_motion(i)){const auto& clip=asset.motion_clip(i);auto& ch=channels_[i];auto& ak=angles_[i];auto& pk=positions_[i];ch.resize(clip.positions.size());ak.resize(ch.size());pk.resize(ch.size());for(unsigned n=0;n<ch.size();++n){for(const auto& a:clip.angles[n])ak[n].push_back({{(unsigned short)a[0],(unsigned short)a[1],(unsigned short)a[2]}});for(const auto& p:clip.positions[n])pk[n].push_back({{p.x,p.y,p.z}});ch[n].p[0]=pk[n].empty()?nullptr:pk[n].data();ch[n].p[1]=ak[n].empty()?nullptr:ak[n].data();}motions_[i]={clip.flags,int(ch.size()),int(clip.frames),(void*)&clip,ch.data(),clip.attributes.empty()?nullptr:(unsigned short*)clip.attributes.data()};}
 core_=recvx_push_create(atr.data(),atr.size(),height,floors.data(),objects.data(),objects.size(),&host);if(!core_)throw std::bad_alloc();
 const auto& tr=asset.translation_curve(9);recvx_push_motions(core_,motions_.data(),motions_.size(),tr.data(),tr.size(),poses_.data(),poses_.size());seed(42,0);
}
PlayerPush::~PlayerPush(){recvx_push_destroy(core_);}
void PlayerPush::seed(std::size_t motion,std::int64_t frame){const auto p=asset_.local_pose(motion,frame);const auto nodes=asset_.motion_nodes(poses_.size());for(unsigned n=0;n<poses_.size();++n){const auto& v=p.at((nodes.empty()?n:nodes.at(n)));poses_[n]={{v.position.x,v.position.y,v.position.z},{v.angles[0],v.angles[1],v.angles[2]}};}}
NJS_POINT3 PlayerPush::contact(NJS_POINT3 from,NJS_POINT3 to,int yaw,int floor,bool walking,int health,int weapon,std::size_t motion,std::int64_t frame){if(!state_.active)seed(motion,frame);recvx_push_contact(core_,&state_,from.x,from.y,from.z,to.x,to.y,to.z,yaw,floor,walking,health,weapon);return {state_.x,state_.y,state_.z};}
bool PlayerPush::tick(bool forward,int character,int health,int weapon,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame){if(!active())return false;recvx_push_tick(core_,&state_,forward,character,health,weapon);actor.position={state_.x,state_.y,state_.z};actor.yaw=state_.ay;motion=state_.motion;frame=state_.frame;return true;}
NJS_POINT3 PlayerPush::object(unsigned i)const{float p[3];recvx_push_object(core_,i,p);return {p[0],p[1],p[2]};}
ModelNodeOverrides PlayerPush::pose()const{ModelNodeOverrides out;const auto nodes=asset_.motion_nodes(poses_.size());for(unsigned n=0;n<poses_.size();++n)out[nodes.empty()?n:nodes[n]]={NJS_POINT3{poses_[n].pos[0],poses_[n].pos[1],poses_[n].pos[2]},std::array<Angle,3>{poses_[n].ang[0],poses_[n].ang[1],poses_[n].ang[2]}};return out;}
void PlayerPush::update_object(unsigned i,NJS_POINT3 p,bool hidden){recvx_push_update_object(core_,i,p.x,p.y,p.z,hidden);}
void PlayerPush::update_walls(const std::vector<CollisionWall>& w){std::vector<RECVX_STAIR_AREA> atr;for(const auto& v:w)atr.push_back(atr_wall(v));recvx_push_update_walls(core_,atr.data(),atr.size());}
std::vector<CollisionWall> PlayerPush::walls()const{const RECVX_STAIR_AREA* atr;const unsigned n=recvx_push_walls(core_,&atr);std::vector<CollisionWall> out;for(unsigned i=0;i<n;++i){const auto& a=atr[i];out.push_back({a.flg,a.type,a.id,a.flr_no,a.attr,{a.px,a.py,a.pz},{a.w,a.h,a.d},{a.prm0,a.prm1,a.prm2,a.prm3}});}return out;}

}
