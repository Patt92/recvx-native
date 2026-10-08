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
 // objitm.c bhObj001: only aspd 0 and 6 set pn and allocate the two mobile
 // type-2 ETC records used by bhCheckExmAtari/bhCheckDansa.  The lower one
 // starts DNU (prm0 0); the upper one starts DND (prm0 1).
 {for(int aspd=0;aspd<8;++aspd){std::vector<RECVX_PUSH_OBJECT> objects{{4,1,aspd,3,0,10,0,20}};PlayerPush push({},100,floors,objects,asset,{});const auto areas=push.steps(),floor_areas=push.floors();const bool climbable=aspd==0||aspd==6;check(areas.size()==(climbable?2u:0u)&&floor_areas.size()==(climbable?1u:0u),"bhObj001 climb ETC/floor allocation differs by aspd");if(climbable){check(areas[0].type==2&&areas[0].prm0==0&&areas[0].flr_no==0,"crate lower ascent ETC differs from bhObj001");check(areas[1].type==2&&areas[1].prm0==1&&areas[1].flr_no==0,"crate upper descent ETC differs from bhObj001");check(floor_areas[0].type==1&&floor_areas[0].prm0==3&&floor_areas[0].flr_no==0&&floor_areas[0].px==1.5f&&floor_areas[0].pz==11.5f,"crate mflrp sound area differs from bhObj001");check(areas[0].px==5.5f&&areas[0].pz==15.5f&&areas[1].px==5.5f&&areas[1].pz==15.5f,"crate ETC does not follow original footprint");push.update_object(0,{30,0,40},false);push.contact({0,0,0},{0,0,0},0,0,false,0,0,0,0);const auto moved=push.steps(),moved_floors=push.floors();check(moved[0].px==25.5f&&moved[0].pz==35.5f&&moved[1].px==25.5f&&moved[1].pz==35.5f&&moved_floors[0].px==21.5f&&moved_floors[0].pz==31.5f,"mobile crate ETC/mflrp does not follow O_WRK position");}}}
 // bhCheckWallType: negative type-7 h is a plane threshold, not a box.
 {RECVX_STAIR_AREA wall{1,7,0,0,1,-4.5f,9,-4.5f,9,-1,9,0,0,0,0};unsigned attr=0;
  check(!recvx_push_probe(&wall,1,100,0,9.1f,0,3.5f,16.5f,0x119,&attr),"upper climb probe treated type7 as a volume");
  check(recvx_push_probe(&wall,1,100,0,7.9f,0,3.5f,16.5f,0x119,&attr)&&attr==1,"type7 plane intersection lost");
  wall.type=1;wall.h=1;check(!recvx_push_probe(&wall,1,100,0,9,0,3.5f,16.5f,0x400,&attr),"original odd-type 0x400 skip lost");}
 // Original floor state -> temporary Dansa walls -> removal after collision.
 // Both walk and run must stay on the upper surface; contiguous upper ETCs
 // remain traversable and do not acquire an artificial barrier at the seam.
 {auto heights=floors;heights[3]=9;heights[4]=1000;
  for(bool walking:{false,true})for(int yaw:{0,16384,32768,49152}){PlayerPush push({},100,heights,{{4,1,0,0,0,0,0,0}},asset,{});NJS_POINT3 pos{0,9,0};
   for(unsigned t=0;t<100;++t){auto to=pos;to.x-=.5f*njSin(yaw);to.z-=.5f*njCos(yaw);pos=push.contact(pos,to,yaw,1,walking,0,0,42,0);check(pos.y==9&&std::abs(pos.x)<=4.5f&&std::abs(pos.z)<=4.5f,"walk/run falls off original upper ETC");check(push.walls().size()==2,"temporary Dansa walls leaked into mobile wall table");}
   check(push.state().status&0x20000,"upper floor state not retained");check(!push.state().unsupported_services,"Dansa limit service remains unbound");}
  PlayerPush joined({},100,heights,{{4,1,0,0,0,0,0,0},{5,1,0,0,0,9,0,0}},asset,{});NJS_POINT3 pos{0,9,0};
  for(unsigned t=0;t<22;++t){auto to=pos;to.x+=.5f;pos=joined.contact(pos,to,49152,1,true,0,0,42,0);}
  check(pos.x>9&&pos.y==9,"adjacent original upper ETCs were blocked at their seam");
 }
 // Actual RM0100 objects 5/6/7: floor 2 / height 13, four directions.
 {auto bytes=read_asset_file(data/"rooms/RM_0100.RDT",64*1024*1024);auto scene=decode_scene(bytes);
  for(unsigned box:{5u,6u,7u})for(int yaw:{0,16384,32768,49152}){SceneSpawn actor=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;AudioMixer audio(data);RoomWorld world(bytes,scene,actor,m,f,audio,data);world.set_player_asset(&asset);const auto center=world.position(2,box).value();actor.position={center.x,13,center.z};actor.yaw=yaw;
   for(unsigned t=0;t<100;++t){world.tick();auto from=actor.position,to=from;to.x-=.5f*njSin(yaw);to.z-=.5f*njCos(yaw);actor.position=world.move(from,to,3.5f,16.5f,true);check(actor.position.y==13,"RM0100 crate walk drops to room ground");}
  }
 }
 std::cout<<"Original Dansa edge / run / adjacent ETC / RM0100 three crates passed\n";
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
