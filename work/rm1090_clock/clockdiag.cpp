// Diagnose RM_1090 Uhrenraetsel (Gegner 0, id 61, Modell 0x4c5c4): Ablauf L/R/Reset
// ueber die originalen Raumskripte und Geometrieanalyse der Knoten um das Drehzentrum.
// Kein Produktivcode; Flags per DIAG_FLAGS sind Diagnosevorbedingungen.
#define SDL_MAIN_HANDLED
#include "input.h"
#include "world.h"
#include "movie.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "binary_view.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Room {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
 Inventory inventory;StatusMenu menu;MessageDialog dialog;AudioMixer audio;MoviePlayer movie;PlayerAsset body;RoomWorld world;EventVm vm;GameplayInput input;
 unsigned ps=0,on=0;
 Room(const std::filesystem::path& data,const char* name):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm){
  world.set_player_asset(&body);vm.values[1]=0;if(std::getenv("DIAG_AI")){world.bind_player_collision(vm);world.set_enemy_ai(true);world.set_enemy_grab(true);std::printf("enemy AI on\n");}
  vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
  vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};
  vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
  vm.pad_state=[&]{return std::pair<unsigned,unsigned>(ps,on);};
  vm.enemy_node_ax_get=[&](unsigned e,unsigned n,int& a){return world.enemy_node_ax(e,n,a);};vm.enemy_node_ax_set=[&](unsigned e,unsigned n,int a){world.set_enemy_node_ax(e,n,a);};
  vm.object_ay_get=[&](unsigned i,int& a){return world.object_ay(i,a);};vm.object_ay_set=[&](unsigned i,int a){world.set_object_ay(i,a);};
  input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
  if(const char* f=std::getenv("DIAG_FLAGS")){std::string s=f;size_t p=0;while(p<s.size()){auto c=s.find(',',p);auto item=s.substr(p,c==std::string::npos?std::string::npos:c-p);p=c==std::string::npos?s.size():c+1;unsigned m=0,b=0;if(std::sscanf(item.c_str(),"%u:%u",&m,&b)==2){vm.set_flag(m,b,0);std::printf("preset flag %u:%u\n",m,b);}}}
  audio.begin_room_init();vm.initialize();
  std::printf("pending bindings after init: %zu [",vm.pending_bindings.size());for(const auto& c:vm.pending_bindings)std::printf("%02x/%02x ",c.opcode,c.bytes.size()>1?c.bytes[1]:0);std::printf("]\n");
  for(const auto& g:world.unbound_enemy_routines())std::printf("gap: %s\n",g.c_str());
  audio.load_room_sounds(0,vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
 }
 void tick(){input.apply({});vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.tick();input.synchronize();world.set_simulation_flags(vm.flags[6][0]);world.tick();vm.flags[4][0]|=world.take_enemy_room_flags();movie.tick();audio.mix();ps=0;if(!vm.errors.empty()){std::cerr<<"VM errors: ";for(auto& e:vm.errors)std::cerr<<e<<" | ";std::cerr<<"\n";throw std::runtime_error("original event stream blocked");}}
 std::string tasks(){std::string o;for(const auto& t:vm.task_states()){o+="ev"+std::to_string(t.event)+"@L"+std::to_string(t.level)+":"+std::to_string(t.offset)+(t.blocked?"(blocked)":"")+" ";}return o;}
 int ax(unsigned node){int a=0;if(!world.enemy_node_ax(0,node,a))return -99999;return int(std::int16_t(a));}
 void state(const char* tag){std::printf("%s: tasks=[%s] n6=%d(%.1fdeg) n21=%d(%.1fdeg) dialog=%d choices=%u sel=%u cine=%d f4:4=%d f4:7=%d pend=%zu\n",tag,tasks().c_str(),ax(6),ax(6)*360.0/65536,ax(21),ax(21)*360.0/65536,dialog.active()?int(dialog.index()):-1,dialog.choices(),dialog.selection,(vm.flags[5][0]&4)?1:0,vm.flag(4,4)?1:0,vm.flag(4,7)?1:0,vm.pending_bindings.size());}
};
// Winkelabdeckung je Knoten um das Drehzentrum (Knoten 1 z=-9, Knoten 6/21 y=19.04); 0 Grad = 12 Uhr (+y), positiv Richtung +z.
static void digits(const std::vector<std::uint8_t>& bytes,std::size_t model,std::size_t texture){
 auto scene=decode_model_asset(bytes,model,texture);std::printf("textures: %zu\n",scene.textures.size());
 for(unsigned tex:{10u,13u,9u,15u,14u,16u,11u,12u,18u}){if(tex>=scene.textures.size())continue;const auto& t=scene.textures[tex];std::printf("tex %u %ux%u\n",tex,t.width,t.height);
  unsigned lo=255,hi=0;for(std::size_t i=0;i<t.pixels.size();++i){const auto px=t.pixels[i];if((px>>24)<64)continue;const unsigned lum=((px&0xff)+((px>>8)&0xff)+((px>>16)&0xff))/3;lo=std::min(lo,lum);hi=std::max(hi,lum);}
  const unsigned rows=32,cols=32;for(unsigned r=0;r<rows;++r){std::string line;for(unsigned c=0;c<cols;++c){const unsigned x=c*t.width/cols,y=r*t.height/rows;const auto px=t.pixels[y*t.width+x];const unsigned a=px>>24,lum=((px&0xff)+((px>>8)&0xff)+((px>>16)&0xff))/3;const unsigned q=hi>lo?(lum-lo)*4/(hi-lo+1):0;line+=a<64?' ':".:-=+*#%"[std::min((lum-lo)*8/(hi-lo+1),7u)];}std::printf("  |%s| lo=%u hi=%u\n",line.c_str(),lo,hi);}}
}
static void hole(const std::vector<std::uint8_t>& bytes,std::size_t model,const std::vector<int>& degs6){
 BinaryView v(bytes);const auto base=v.relative(model,v.u16(model+4),0);const unsigned count=v.u16(model+6);const auto nodes=v.relative(base,v.u32(model+12),count*52);
 const float cy=19.04f,cz=-9.0f;
 auto centre=[&](const Scene& sc,unsigned obj,float& ang){float y=0,z=0;unsigned n=0;for(const auto& t:sc.triangles){if(t.object!=obj)continue;for(const auto& vt:t.vertices){y+=vt.position.y-cy;z+=vt.position.z-cz;++n;}}if(!n)return false;ang=std::atan2(z/n,y/n)*180.0f/3.14159265f;return true;};
 for(int d6:degs6){ModelGeometryOptions o;for(unsigned i=0;i<count;++i){const auto p=nodes+i*52;ModelPose mp;mp.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};mp.angles={v.i32(p+20),v.i32(p+24),v.i32(p+28)};o.pose.push_back(mp);}
  o.pose[6].angles[0]=int(d6*182.04445f);o.pose[21].angles[0]=int(-20*182.04445f);auto sc=decode_model_asset(bytes,model,0,o);
  float h=0;centre(sc,23,h);int best=-1;float bestd=999;std::string all;for(unsigned obj=7;obj<=14;++obj){float a=0;if(!centre(sc,obj,a))continue;float d=std::fabs(std::remainder(a-h,360.0f));all+=" o"+std::to_string(obj)+"@"+std::to_string(int(std::lround(a)));if(d<bestd){bestd=d;best=int(obj);}}
  std::printf("n6=%4d: window at %.1f deg -> obj %d (%.1f deg off);%s\n",d6,h,best,bestd,all.c_str());}
}
static void dumptex(const std::vector<std::uint8_t>& bytes,std::size_t model,std::size_t texture,const char* path){
 auto scene=decode_model_asset(bytes,model,texture);FILE* f=std::fopen(path,"wb");if(!f)throw std::runtime_error("cannot write texture dump");
 for(unsigned tex=0;tex<scene.textures.size();++tex){const auto& t=scene.textures[tex];std::uint32_t hdr[3]={tex,t.width,t.height};std::fwrite(hdr,4,3,f);for(std::size_t i=0;i<t.pixels.size();++i){const std::uint32_t px=t.pixels[i];std::fwrite(&px,4,1,f);}}
 std::fclose(f);std::printf("dumped %zu textures\n",scene.textures.size());
}
static void geometry(const std::vector<std::uint8_t>& bytes,std::size_t model,int deg6,int deg21,const char* tag){
 BinaryView v(bytes);const auto base=v.relative(model,v.u16(model+4),0);const unsigned count=v.u16(model+6);const auto nodes=v.relative(base,v.u32(model+12),count*52);
 ModelGeometryOptions o;for(unsigned i=0;i<count;++i){const auto p=nodes+i*52;ModelPose mp;mp.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};mp.angles={v.i32(p+20),v.i32(p+24),v.i32(p+28)};o.pose.push_back(mp);}
 o.pose[6].angles[0]=int(deg6*182.04445f);o.pose[21].angles[0]=int(deg21*182.04445f);
 auto scene=decode_model_asset(bytes,model,0,o);
 std::printf("--- %s n6=%d n21=%d: %zu triangles\n",tag,deg6,deg21,scene.triangles.size());
 if(std::getenv("DIAG_UV"))for(unsigned obj=7;obj<=14;++obj){float u0=9,u1=-9,v0=9,v1=-9;int tex=-1;for(const auto& t:scene.triangles){if(t.object!=obj)continue;tex=t.texture;for(const auto& vt:t.vertices){u0=std::min(u0,vt.u);u1=std::max(u1,vt.u);v0=std::min(v0,vt.v);v1=std::max(v1,vt.v);}}std::printf("digit obj %2u tex=%d u[%.3f,%.3f] v[%.3f,%.3f]\n",obj,tex,u0,u1,v0,v1);}
 for(unsigned obj:{22u,23u})if(std::getenv("DIAG_UV")){float u0=9,u1=-9,v0=9,v1=-9;int tex=-1;for(const auto& t:scene.triangles){if(t.object!=obj)continue;tex=t.texture;for(const auto& vt:t.vertices){u0=std::min(u0,vt.u);u1=std::max(u1,vt.u);v0=std::min(v0,vt.v);v1=std::max(v1,vt.v);}}std::printf("obj %2u tex=%d u[%.3f,%.3f] v[%.3f,%.3f]\n",obj,tex,u0,u1,v0,v1);}
 const float cy=19.04f,cz=-9.0f;const int B=36;
 for(unsigned obj=0;obj<count;++obj){unsigned tris=0;int bins[B]={};float minx=1e9,maxx=-1e9,rmin=1e9,rmax=0;
  for(const auto& t:scene.triangles){if(t.object!=obj)continue;++tris;float ty=0,tz=0;for(const auto& vert:t.vertices){const auto& p=vert.position;minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);ty+=p.y-cy;tz+=p.z-cz;const float r=std::sqrt((p.y-cy)*(p.y-cy)+(p.z-cz)*(p.z-cz));rmin=std::min(rmin,r);rmax=std::max(rmax,r);}
   ty/=3;tz/=3;const float r=std::sqrt(ty*ty+tz*tz);if(r<0.3f)continue;float th=std::atan2(tz,ty)*180.0f/3.14159265f;int b=int(std::floor((th+180.0f)/(360.0f/B)));if(b<0)b=0;if(b>=B)b=B-1;bins[b]++;}
  if(!tris)continue;
  std::printf("obj %2u tris=%4u x[%5.2f,%5.2f] r[%5.2f,%5.2f] tri-centres per 10deg from -180 (12h at col 18):",obj,tris,minx,maxx,rmin,rmax);
  for(int b=0;b<B;++b)std::printf("%s%d",b==18?" |":" ",bins[b]);std::printf("\n");}
}
int main(int argc,char** argv){try{
 if(argc<2)return 1;_Make_SinTable();const std::filesystem::path data=argv[1];
 std::vector<std::uint8_t> raw=read_asset_file(data/"rooms"/"RM_1090.RDT",64*1024*1024);
 if(argc>3&&std::string(argv[2])=="digits"){digits(raw,0x4c5c4,std::size_t(std::strtoul(argv[3],nullptr,0)));return 0;}
 if(argc>4&&std::string(argv[2])=="dumptex"){dumptex(raw,0x4c5c4,std::size_t(std::strtoul(argv[3],nullptr,0)),argv[4]);return 0;}
 if(argc>2&&std::string(argv[2])=="hole"){hole(raw,0x4c5c4,{-20,160,-65,115,-155,180,0});return 0;}
 if(argc>2&&std::string(argv[2])=="geom"){geometry(raw,0x4c5c4,0,0,"rest");geometry(raw,0x4c5c4,-20,-20,"init");geometry(raw,0x4c5c4,20,20,"plus20");return 0;}
 Room r(data,"RM_1090");
 const auto* e=r.world.state(1,0);std::printf("enemy0 identity=%u scripted=%d native=%d bound=%d calculated=%d objs=%zu\n",e->identity,e->scripted,e->enemy_native,e->enemy_bound,e->enemy_calculated,e->enemy_obj.size());
 for(unsigned t=0;t<240;++t)r.tick();
 r.state("after init");for(const auto& g:r.world.unbound_enemy_routines())std::printf("gap after init: %s\n",g.c_str());{const auto* e=r.world.state(1,0);std::printf("enemy0 work mode=[%u,%u,%u,%u] mtn_no=%u frm_no=%d calculated=%d owp_flg6=%u owp_flg21=%u\n",unsigned(e->enemy.mode0),unsigned(e->enemy.mode1),unsigned(e->enemy.mode2),unsigned(e->enemy.mode3),unsigned(e->enemy.mtn_no),int(e->enemy.frm_no),e->enemy_calculated,e->enemy_owp_flg.size()>21?e->enemy_owp_flg[6]:99u,e->enemy_owp_flg.size()>21?e->enemy_owp_flg[21]:99u);}
 unsigned target=argc>2?unsigned(std::atoi(argv[2])):~0u;bool opened=false;
 for(unsigned i=0;i<r.vm.interactions.size()&&!opened;++i){if(target!=~0u&&i!=target)continue;const auto& a=r.vm.interactions[i];if(a.type!=3)continue;
  for(unsigned yaw=0;yaw<65536&&!opened;yaw+=16384){float rad=yaw*6.28318530718f/65536;float probe=6.f;r.actor.position={a.position.x+a.extent.x*.5f+probe*std::sin(rad),a.position.y,a.position.z+a.extent.z*.5f+probe*std::cos(rad)};r.actor.yaw=yaw;
   if(r.vm.interact(r.actor.position,yaw,a.floor)){for(unsigned t=0;t<5;++t)r.tick();r.state("after interact");if(r.dialog.active())opened=true;}}}
 check(opened,"no interaction opened a dialog");
 auto answer=[&](unsigned sel,const char* tag){unsigned waited=0;while(!(r.dialog.active()&&r.dialog.choices())&&waited<600){if(r.dialog.active()){r.input.menu_input(MenuInput::Confirm);r.ps=0x200|0x800;}r.tick();++waited;}check(r.dialog.active()&&r.dialog.choices(),"choice dialog missing");
  std::printf("choices after %u ticks: %s\n",waited,message_text(r.dialog.page()).c_str());r.dialog.selection=sel;r.vm.values[7]=sel;r.input.menu_input(MenuInput::Confirm);r.ps=0x200|0x800;r.tick();r.state(tag);};
 const std::string plan=argc>3?argv[3]:"L,RESET";
 size_t pos=0;while(pos<=plan.size()){auto nx=plan.find(',',pos);std::string step=plan.substr(pos,nx==std::string::npos?std::string::npos:nx-pos);pos=nx==std::string::npos?plan.size()+1:nx+1;
  unsigned sel=step=="L"?0:step=="R"?1:2;answer(sel,("answered "+step).c_str());
  for(unsigned t=0;t<400;++t){if(r.dialog.active()&&!r.dialog.choices()&&t>2){r.input.menu_input(MenuInput::Confirm);r.ps=0x200|0x800;}r.tick();if(t%20==19)r.state(("  "+step+" t="+std::to_string(t)).c_str());
   if(!(r.vm.flags[5][0]&4)&&!r.dialog.active()){r.state(("  "+step+" released at t="+std::to_string(t)).c_str());break;}
   if(r.dialog.active()&&r.dialog.choices()&&t>30){r.state(("  "+step+" dialog back at t="+std::to_string(t)).c_str());break;}}
 }
 r.state("final");
 return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 2;}}
