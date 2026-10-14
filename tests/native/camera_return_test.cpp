#include "world.h"
#include "room.h"
#include "core_random.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static float distance(NJS_POINT3 a,NJS_POINT3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
int main(int argc,char** argv){try{
    SceneCamera from,to;from.position={0,0,0};from.flags=2;from.angles[1]=65530;
    to.position={31,0,0};to.transition_speed=30;to.move_speed=.3f;to.angle_speed=30;to.angles[1]=6;
    ActiveCameraMotion motion;motion.reset(from,9);motion.select(to,3);motion.tick(to);
    check(motion.transitioning()&&motion.clock()==1&&motion.view().position.x==0,"first return tick must retain its outgoing pose");
    motion.tick(to);check(std::abs(motion.view().position.x-1)<.00001f,"return lacks original recursive hct/(spd+1)");
    for(int i=0;i<29;++i)motion.tick(to);
    check(!motion.transitioning()&&motion.clock()==31,"original transition clock boundary changed");
    motion.select(to,4,true);check(!motion.transitioning()&&motion.view().position.x==31,"forced init must snap");
    from.flags=0;to.flags=2;motion.reset(from,0);motion.select(to,1);check(motion.transitioning(),"incoming interpolation flag ignored");
    from.flags=to.flags=0;motion.reset(from,0);motion.select(to,1);check(!motion.transitioning()&&motion.view().position.x==31,"ordinary cut must remain a cut");
    if(argc>1){
        const std::filesystem::path data=argv[1];RoomArchive rooms(data);auto bytes=rooms.load("RM_0090.RDX",64*1024*1024);auto scene=decode_scene(bytes);
        check(scene.cameras[9].flags&2,"announcement camera has no original return flag");
        Inventory inventory;check(inventory.acquire(5,18),"metal fixture inventory");MessageDialog dialog;AudioMixer audio(data);
        auto actor=scene.spawns.at(0);std::size_t clip=42;std::int64_t frame=0;RoomWorld world(bytes,scene,actor,clip,frame,audio,data);EventVm vm(bytes,inventory,dialog);
        vm.values[1]=9;vm.task_started=[&](unsigned level){world.start_task(level);};
        vm.binding=[&](const EventCommand& c){if(c.opcode==0x39){scene.cut_enabled.at(c.bytes.at(2))=c.bytes.at(1)==0;return true;}return world.bind(c);};
        vm.object_position=[&](unsigned kind,unsigned index){return world.position(kind,index);};vm.animation_frame=[&](unsigned kind,unsigned index){return world.frame(kind,index);};
        vm.audio_active=[&](unsigned slot){return audio.active(slot);};vm.audio_stop=[&](unsigned slot){audio.stop(slot);};vm.movie_active=[]{return false;};
        vm.initialize();audio.load_room_sounds(0,9);unsigned cut=0;SceneCamera target=scene.cameras[0];bool announced=false,returned=false,intermediate=false;unsigned release=0;
        for(unsigned tick=0;tick<300;++tick){
            if(tick==60)actor.position={34.5f,0,5};if(tick==100)actor.position={44.5f,0,5};
            if(tick==180||tick==220)vm.confirm_dialog();
            const bool fixed=world.fixed_camera.has_value();const auto previous=world.camera(scene,target);
            vm.values[2]=cut;vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.tick();world.tick();
            if(world.fixed_camera){cut=*world.fixed_camera;target=scene.cameras.at(cut);announced|=cut==9;}
            else if(!world.event_camera){if(auto next=camera_for_position(scene,actor.position,std::int8_t(world.floor_at(actor.position))))cut=*next;target=camera_for_actor(scene,cut,actor.position,std::int8_t(world.floor_at(actor.position)));}
            world.tick_camera(scene,target,cut);audio.mix();
            const auto view=world.camera(scene,target);
            if(fixed&&!world.fixed_camera){release=tick;returned=true;check(world.camera_transitioning()&&world.camera_clock()==1,"original announcement exits without a camera return");check(distance(previous.position,view.position)<.00001f,"release cuts to the gameplay view");std::cout<<"Original return: tick "<<tick<<", cut "<<cut<<", spd "<<target.transition_speed<<"\n";}
            if(returned&&tick==release+1){check(world.camera_clock()==2&&distance(view.position,target.position)>1&&distance(view.position,scene.cameras[9].position)>.01f,"return has no intermediate position");intermediate=true;}
        }
        check(vm.errors.empty()&&announced&&returned&&intermediate&&!world.camera_transitioning(),"announcement camera sequence incomplete");
        check(distance(world.camera(scene,target).position,target.position)<.001f,"return does not converge to gameplay position");
        check(!world.player_hidden&&!(vm.flags[5][0]&4),"camera return did not release gameplay");
        // RM_1020 camera 20 is the original manor-establishing event path:
        // two 60-frame travelling keys followed by its final hold.  bhCamSet
        // must drive the CAM_KEYF_WORK spline rather than leave key 0 static.
        auto manor=rooms.load("RM_1020.RDX",64*1024*1024);auto manor_scene=decode_scene(manor);auto manor_actor=manor_scene.spawns.at(0);std::size_t manor_motion=42;std::int64_t manor_frame=0;AudioMixer manor_audio(data);RoomWorld manor_world(manor,manor_scene,manor_actor,manor_motion,manor_frame,manor_audio,data);
        check(manor_world.bind({0x13,0,0,0,{0x13,0,20,0}}),"RM_1020 original bhCamSet is unbound");const auto manor_start=manor_world.camera(manor_scene,manor_scene.cameras.at(0));
        for(unsigned tick=0;tick<30;++tick)manor_world.tick_camera(manor_scene,manor_scene.cameras.at(0),0);
        const auto manor_mid=manor_world.camera(manor_scene,manor_scene.cameras.at(0));check(distance(manor_start.position,manor_mid.position)>1,"RM_1020 event camera keyframe remains static");
        for(unsigned tick=30;tick<180;++tick)manor_world.tick_camera(manor_scene,manor_scene.cameras.at(0),0);
        const auto manor_final=manor_world.camera(manor_scene,manor_scene.cameras.at(0));check(manor_world.event_camera&&manor_world.event_camera_key()==2&&distance(manor_final.position,{9.5907059f,61.725918f,60.876106f})<.01f,"RM_1020 event camera does not reach its original final key");
        // event.c:4701 bhCamYureSet (RM_0020 Event 7, Explosion): v0 0 setzt cam.ofx/ofy/ofz
        // nacheinander auf (0.01*v1)*(-rand()/-2.1474836E9f), v0 != 0 loescht sie; bhControlCamera
        // verschiebt damit die Sicht (camera.c:36), cam.px selbst bleibt.
        auto yard=rooms.load("RM_0020.RDX",64*1024*1024);auto yard_scene=decode_scene(yard);auto yard_actor=yard_scene.spawns.at(0);std::size_t yard_motion=42;std::int64_t yard_frame=0;AudioMixer yard_audio(data);RoomWorld yard_world(yard,yard_scene,yard_actor,yard_motion,yard_frame,yard_audio,data);
        check(!yard_world.bind({0x5a,0,0,0,{0x5a,0,0x64,0}}),"bhCamYureSet claims a binding without the cam owner");
        NJS_POINT3 offset{};yard_world.set_camera_offset_owner(&offset);
        recvx_core_random_seed(7);const float amp=0.01f*400.0f;
        const float ex=amp*(-recvx_core_random_next()/-2.1474836E9f),ey=amp*(-recvx_core_random_next()/-2.1474836E9f),ez=amp*(-recvx_core_random_next()/-2.1474836E9f);
        recvx_core_random_seed(7);const auto still=yard_world.camera(yard_scene,yard_scene.cameras.at(0));
        check(yard_world.bind({0x5a,0,0,0,{0x5a,0,0x90,0x01}}),"RM_0020 bhCamYureSet unbound");
        check(offset.x==ex&&offset.y==ey&&offset.z==ez&&ex!=ey,"bhCamYureSet offset differs from the original x/y/z rand order");
        const auto shaken=yard_world.camera(yard_scene,yard_scene.cameras.at(0));
        check(shaken.position.x==still.position.x+ex&&shaken.position.y==still.position.y+ey&&shaken.position.z==still.position.z+ez,"cam.ofx does not move the view");
        check(yard_world.bind({0x5a,0,0,0,{0x5a,1,0,0}})&&offset.x==0&&offset.y==0&&offset.z==0,"bhCamYureSet v0 1 does not clear cam.ofx/ofy/ofz");
    }
    std::cout<<"Original active-camera return verified\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
