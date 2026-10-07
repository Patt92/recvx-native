#include "player_push.h"
#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{_Make_SinTable();check(argc==2,"data path required");const auto data=std::filesystem::path(argv[1]);auto asset=PlayerAsset::load(data);std::array<float,32> floors{};floors[0]=-10000;floors[1]=-1000;floors[3]=1000;
 for(int yaw:{0,16384,32768,49152}){std::vector<RECVX_PUSH_OBJECT> objects{{4,1,0,0,0,0,0,0}};PlayerPush push({},100,floors,objects,asset,{});SceneSpawn actor;actor.yaw=yaw;actor.position={njSin(yaw)*8.3f,0,njCos(yaw)*8.3f};std::size_t motion=0;std::int64_t frame=5*65536;bool began=false;const auto initial=push.object(0);
 for(int t=0;t<30&&!push.active();++t){auto from=actor.position;asset.locomotion(actor,motion,frame,1,false,0);actor.position=push.contact(from,actor.position,yaw,0,true,0,0,motion,frame);check(std::hypot(push.object(0).x-initial.x,push.object(0).z-initial.z)<1e-6,"crate moved before MTN50");began=push.active();}
 check(began,"original contact count did not start push");
 const auto before=push.pose().at(0).position.value();push.tick(true,0,0,0,actor,motion,frame);const auto first=push.pose().at(0).position.value();const auto target=asset.motion_clip(39).positions[0][0];const float rate=0.000015258789f*(65536-42598);
 check(std::abs(first.x-(before.x+rate*(target.x-before.x)))<1e-5f&&std::abs(first.y-(before.y+rate*(target.y-before.y)))<1e-5f,"original Hokan rate/previous pose lost");
 bool entered49=false,entered50=false;int entryTicks=0;while(push.active()&&entryTicks++<100){push.tick(true,0,0,0,actor,motion,frame);if(motion==49)entered49=true;if(motion==50){entered50=true;break;}check(std::hypot(push.object(0).x-initial.x,push.object(0).z-initial.z)<1e-6,"crate moved during entry");}
 check(entered49&&entered50,"original MTN49 -> MTN50 transition missing");for(int t=0;t<12;++t)push.tick(true,0,0,0,actor,motion,frame);const auto moved=push.object(0);check(std::hypot(moved.x,moved.z)>.1f,"original MTN50 curve did not move crate");
 push.tick(false,0,0,0,actor,motion,frame);check(motion==49&&push.state().phase==7,"release did not reverse MTN49");auto previous=frame;unsigned reverse=0;while(push.active()&&reverse++<200){push.tick(false,0,0,0,actor,motion,frame);if(push.active())check(frame==previous-32768,"exit does not use original half-frame clock");previous=frame;check(std::hypot(push.object(0).x-moved.x,push.object(0).z-moved.z)<1e-6,"crate moved during exit");}check(!push.active()&&!(push.state().status&0x10080),"push did not release control");std::cout<<"Cardinal "<<yaw<<": MTN49/50/reverse49 passed\n";}


 // Each original aspd footprint. A blocker ahead must keep the wall contact
 // counter at zero, so MTN49 is not entered. No custom proximity threshold.
 {const float halfz[8]={4.5f,6.f,3.5f,7.5f,2.5f,5.f,4.5f,5.5f};for(int aspd=0;aspd<8;++aspd){CollisionWall stop{1,0,0,0,0,{-20,0,-halfz[aspd]-2},{40,40,2}};std::vector<RECVX_PUSH_OBJECT> objects{{4,1,aspd,0,0,0,0,0}};PlayerPush push({stop},100,floors,objects,asset,{});SceneSpawn actor;actor.position={0,0,halfz[aspd]+3.8f};actor.yaw=0;std::size_t m=0;std::int64_t f=5*65536;for(unsigned t=0;t<30;++t){auto from=actor.position;asset.locomotion(actor,m,f,1,false,0);actor.position=push.contact(from,actor.position,0,0,true,0,0,m,f);}check(!push.active()&&push.state().contact==0,"blocked aspd entered push");check(push.object(0).z==0,"blocked crate moved");}}
 // A second original movable wall blocks the first one by work index.
 {std::vector<RECVX_PUSH_OBJECT> objects{{4,1,0,0,0,0,0,0},{5,1,0,0,0,0,0,-9.f}};PlayerPush push({},100,floors,objects,asset,{});SceneSpawn actor;actor.position={0,0,8.3f};std::size_t m=0;std::int64_t f=5*65536;for(unsigned t=0;t<30;++t){auto from=actor.position;asset.locomotion(actor,m,f,1,false,0);actor.position=push.contact(from,actor.position,0,0,true,0,0,m,f);}check(!push.active(),"second crate did not block push contact");check(push.object(0).z==0&&push.object(1).z==-9,"crates overlap/move before MTN50");}
 // Original pad release during entry does not interrupt the alignment/49.
 {std::vector<RECVX_PUSH_OBJECT> objects{{4,1,0,0,0,0,0,0}};PlayerPush push({},100,floors,objects,asset,{});SceneSpawn actor;actor.position={0,0,8.3f};std::size_t m=0;std::int64_t f=5*65536;for(unsigned t=0;t<30&&!push.active();++t){auto from=actor.position;asset.locomotion(actor,m,f,1,false,0);actor.position=push.contact(from,actor.position,0,0,true,0,0,m,f);}check(push.active(),"early release fixture did not begin");bool saw50=false;for(unsigned t=0;t<160&&push.active();++t){push.tick(false,0,0,0,actor,m,f);saw50|=m==50;}check(saw50&&!push.active()&&push.object(0).z==0,"entry release bypassed original 50/exit state");}
 std::cout<<"Eight footprints / walls / second crate / entry release passed\n";

 // Contact starts freely; reaching a real wall during MTN50 must invoke the
 // object's original blocker -> mode6 -> reverse49 exit, while forward stays held.
 {CollisionWall stop{1,0,0,0,0,{-20,0,-15},{40,40,2}};std::vector<RECVX_PUSH_OBJECT> objects{{4,1,0,0,0,0,0,0}};PlayerPush push({stop},100,floors,objects,asset,{});SceneSpawn actor;actor.position={0,0,8.3f};std::size_t m=0;std::int64_t f=5*65536;
 for(unsigned t=0;t<30&&!push.active();++t){auto from=actor.position;asset.locomotion(actor,m,f,1,false,0);actor.position=push.contact(from,actor.position,0,0,true,0,0,m,f);}check(push.active(),"wall approach fixture did not begin");bool saw50=false,sawReverse=false;
 for(unsigned t=0;t<400&&push.active();++t){push.tick(true,0,0,0,actor,m,f);saw50|=m==50;sawReverse|=m==49&&push.state().phase==7;check(push.object(0).z-4.5f>=-13.0001f,"box penetrates blocking wall");}
 check(saw50&&sawReverse&&!push.active(),"wall during push did not invoke original exit");check(push.object(0).z<-8,"wall approach did not reach blocker");check(!push.state().unsupported_services,"push invoked an unbound collision service");}
 std::cout<<"Blocking wall during MTN50 -> reverse49 passed\n";
 // Actual room, real object record and regular world/runtime order: world tick
 // must not reset the contact counter before locomotion.
 {unsigned successful=0;for(int yaw:{0,16384,32768,49152}){auto bytes=read_asset_file(data/"rooms/RM_2070.RDT",64*1024*1024);auto scene=decode_scene(bytes);SceneSpawn actor=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(bytes,scene,actor,motion,frame,audio,data);world.set_player_asset(&asset);const auto initial=world.position(2,4).value();actor.position={initial.x+njSin(yaw)*8.3f,initial.y,initial.z+njCos(yaw)*8.3f};actor.yaw=yaw;bool m49=false,m50=false;for(unsigned t=0;t<100;++t){world.tick();if(world.pushing())world.tick_push(true);else{auto from=actor.position;asset.locomotion(actor,motion,frame,1,false,0);actor.position=world.move(from,actor.position,3.5f,16.5f,true);}m49|=motion==49;m50|=motion==50;auto drawn=world.sample_player(asset);check(!drawn.triangles.empty(),"push pose cannot render");}const auto next=world.position(2,4).value();if(m49&&m50&&std::hypot(next.x-initial.x,next.z-initial.z)>.1f)++successful;for(unsigned t=0;t<160&&world.pushing();++t){world.tick();world.tick_push(false);}check(!world.pushing(),"actual room push did not exit");}check(successful>0,"original RM2070 crate never moved through world driver");std::cout<<"Original RM_2070 world push/pose/release passed ("<<successful<<" sides)\n";}
 return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
