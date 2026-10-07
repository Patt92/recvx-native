#include "world.h"
#include "room.h"
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
    }
    std::cout<<"Original active-camera return verified\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
