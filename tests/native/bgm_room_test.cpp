#include "world.h"
#include "asset_io.h"
#include "room.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct Room {
 std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn player;std::size_t motion=42;std::int64_t frame=0;
 RoomWorld world;Inventory inventory;MessageDialog dialog;EventVm vm;AudioMixer& audio;
 Room(const std::filesystem::path& data,const char* name,AudioMixer& mixer):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),player(scene.spawns.at(0)),world(bytes,scene,player,motion,frame,mixer,data),vm(bytes,inventory,dialog),audio(mixer){
  const auto id=RoomIdentity::parse(std::string(name)+".RDX");vm.values[0]=id.stage;vm.values[1]=id.room;
  vm.binding=[&](const EventCommand& c){return world.bind(c);};vm.task_started=[&](unsigned l){world.start_task(l);};vm.animation_frame=[&](unsigned k,unsigned i){return world.frame(k,i);};
  vm.door=[](const DoorRequest&){return true;};audio.begin_room_init();vm.initialize();audio.load_room_sounds(id.stage,id.room,audio.take_room_sound_case());audio.room_fade_in();
 }
 void step(unsigned n){for(unsigned i=0;i<n;++i){vm.tick();world.tick();audio.mix();}}
 bool door(unsigned index){const auto& a=vm.interactions.at(index);check(a.type==0||a.type==3,"fixture is not an original door interaction");const float probe=a.type==0?4.5f:6.f;
  for(unsigned yaw=0;yaw<65536;yaw+=16384){const unsigned direction=(yaw+8192)&0xc000;
   if((direction==32768&&(a.attribute&0x400))||(direction==16384&&(a.attribute&0x800))||(direction==0&&(a.attribute&0x1000))||(direction==49152&&(a.attribute&0x2000)))continue;
   const float r=yaw*6.28318530718f/65536;NJS_POINT3 at{a.position.x+a.extent.x*.5f+probe*std::sin(r),a.position.y,a.position.z+a.extent.z*.5f+probe*std::cos(r)};
   if(vm.interact(at,yaw,a.floor))return true;
  }return false;
 }
};
int main(int argc,char** argv){try{
 if(argc<2)return 0;_Make_SinTable();const std::filesystem::path data=argv[1];
 for(unsigned event:{15u,16u})for(unsigned exit:{0u,1u,2u}){
  {AudioMixer dog_audio(data);Room dog(data,"RM_0050",dog_audio);dog.vm.start(0,event);
   // The original event sets rm_flg 4 on entry and clears it before ending.
   // Wait for that state instead of assuming it survives 100 host ticks.
   for(unsigned tick=0;tick<100&&!dog.vm.flag(4,4)&&dog.vm.errors.empty();++tick)dog.step(1);
   check(dog.vm.errors.empty(),"original dog BGM event is blocked");
   check(dog_audio.current_bgm()==49&&dog_audio.active(0),"original dog event did not start combat BGM 49");check(dog.vm.flag(4,4),"dog event did not set combat music state");dog.step(100);check(dog.vm.errors.empty()&&!dog.vm.flag(4,4),"dog event did not clear its original music state at completion");}
  AudioMixer audio(data);audio.play_bgm_ex(49,0,-45);Room room(data,"RM_0050",audio);room.vm.set_flag(4,4,0);
  check(room.door(exit),"original RM_0050 door interaction failed");check(room.vm.values[20]==exit&&(room.vm.flags[10][0]&0x100),"door omits original interaction index/flags");
  if(exit==0)room.vm.tick();check(room.vm.flags[10][0]&1,"DoorCall omits cb_flg 1");room.vm.finish_room_exit();
  if(exit==0)check(!(room.vm.flags[10][0]&0x100),"DoorCall aborted the remaining original scenario commands");
  check(room.vm.errors.empty(),"RM_0050 exit scenario blocked");
  std::cout<<"RM_0050 original dog event "<<event<<" / exit "<<exit<<": BGM start and door interaction passed.\n";
 }
 {AudioMixer audio(data);Room room(data,"RM_0060",audio);check(audio.current_bgm()==0&&audio.active(0),"RM_0060 init did not start BGM 0");check(room.door(9),"RM_0060 exit 9 failed");room.vm.finish_room_exit();check(audio.bgm_request()==1,"RM_0060 original exit omitted BGM stop");audio.room_fade_out();for(unsigned t=0;t<60;++t)audio.mix();check(!audio.active(0),"RM_0060 music survives original exit");std::cout<<"RM_0060 original exit 9 stops BGM 0.\n";}
 {AudioMixer audio(data);audio.play_bgm_ex(49,0,-30);Room room(data,"RM_0030",audio);check(audio.current_bgm()==49&&audio.active(0),"room without change request discards retained BGM");audio.room_fade_out();for(unsigned t=0;t<61;++t)audio.mix();check(audio.active(0),"unrequested BGM faded at room exit");
  auto sfx=std::make_shared<PcmClip>();sfx->rate=48000;sfx->channels=1;sfx->samples.assign(100,100);audio.play_pcm(6,sfx,1,true);
  Room next(data,"RM_0060",audio);check(audio.current_bgm()==0&&audio.active(0),"destination init did not replace BGM");check(audio.active(6),"replacing BGM stopped separate SFX");const auto plays=audio.played.size();audio.begin_room_init();audio.request_bgm2(0,-45);audio.room_fade_in();check(audio.played.size()==plays,"identical BGM request restarts current music");std::cout<<"Shared music retention, replacement, identical requests and separate SFX passed.\n";
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
