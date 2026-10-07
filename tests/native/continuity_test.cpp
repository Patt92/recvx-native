#include "room.h"
#include "world.h"
#include "asset_io.h"
#include "binary_view.h"
#include "player_stairs.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace recvx;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    _Make_SinTable();
    const auto id=RoomIdentity::parse("RM_5820.RDX");check(id.stage==5&&id.room==82&&id.variant==0&&id.name()=="RM_5820.RDX","decimal room identity roundtrip");
    bool rejected=false;try{RoomIdentity::parse("RM_21B1.RDX");}catch(const std::exception&){rejected=true;}check(rejected,"hexadecimal name accepted");
    check(recvx_is_subplayer(91)&&recvx_is_subplayer(93)&&!recvx_is_subplayer(54)&&!recvx_is_subplayer(71),"original enemy dispatcher lost");
    RECVX_SUBPLAYER sub{};sub.id=93;recvx_subplayer_init(&sub);check(sub.mtn_no==0&&sub.mtn_add==32768&&sub.mode3==3,"original NPC initialization missing");recvx_subplayer_tick(&sub);check(!sub.advanced,"NPC init pause advanced without a routine");
    sub.mode3=0;sub.mode1=7;sub.ct2=0;recvx_subplayer_tick(&sub);check(sub.mtn_no==7&&sub.advanced&&sub.hokan_count==8,"default event motion dispatch");
    sub.mode0=5;sub.mode3=0;recvx_subplayer_tick(&sub);check(!sub.advanced&&sub.spd==0,"scene-controlled pose advances twice");
    sub.mode0=1;sub.mode3=0;sub.ct2=1;sub.ct0=10000<<16;sub.ct3=50;recvx_subplayer_tick(&sub);check(sub.advanced&&sub.ay==-182&&sub.px>0&&sub.pz<0,"original target turn/movement not bound");
    AudioMixer mixer;auto clip=std::make_shared<PcmClip>();clip->rate=48000;clip->channels=1;clip->samples.assign(32000,10000);mixer.play_pcm(24,clip,.5f,true);check(mixer.user_sound_volume(6,0,0,-64,0),"BG volume category unbound");auto samples=mixer.mix(1);check(std::abs(samples[0]-2519)<=1,"fade erased note velocity/program gain");check(mixer.user_sound_volume(6,0,-1,-127,2),"BG fade missing");mixer.mix(3200);check(!mixer.active(24),"fade does not release the voice");mixer.set_room_sound_case(1);check(mixer.take_room_sound_case()==1&&mixer.take_room_sound_case()==0,"sound variant not consumed once");
    if(argc>1){const auto data=std::filesystem::path(argv[1]);
        // bhSetRoom normalizes POS.py before the player/event initialization.
        // Exercise offset POS heights and upper floors using original resources.
        for(const auto* name:{"RM_0040","RM_0090","RM_0050","RM_0020","RM_0070"}){
            const auto b=read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024);
            const auto s=decode_scene(b);
            for(const auto& spawn:s.spawns){
                auto p=spawn;std::size_t m=42;std::int64_t f=0;AudioMixer a;
                RoomWorld w(b,s,p,m,f,a);const auto expected=w.floor_heights().at(w.floor_at(spawn.position)+2);
                w.initialize_player_spawn();
                check(p.position.y==expected&&p.position.x==spawn.position.x&&p.position.z==spawn.position.z&&p.yaw==spawn.yaw,"room spawn not initialized from original floor before presentation");
                const auto y=p.position.y;w.tick();check(p.position.y==y,"first world tick changes initialized spawn height");
                if(std::string(name)=="RM_0090")check(spawn.position.y==1&&p.position.y==0,"security passage floating spawn regression");
            }
        }
        // The original security room allocates zeroed built-in motion work;
        // scene release must retain the final room pose instead of decoding it.
        auto bytes=read_asset_file(data/"rooms/RM_0040.RDT",64*1024*1024);auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;RoomWorld world(bytes,scene,player,motion,frame,audio);
        for(unsigned npc=0;npc<2;++npc){const auto* state=world.state(1,npc);check(state&&state->subplayer_bound&&!state->has_motion&&!state->room_motion&&state->motion==0,"zeroed original NPC motion work not retained");}
        EventCommand select{0x65,0,0,0,{0x65,1,0,0}};world.bind(select);world.bind({0x69,0,0,0,{0x69,24,1,1,0,0,0,0}});world.tick();check(world.frame(1,0)==65536,"shared NPC event motion did not advance");world.bind({0x67,0,0,0,{0x67,0x90}});const auto frozen=world.frame(1,0);world.tick();check(world.frame(1,0)==frozen&&!world.state(1,0)->scripted,"NPC scene release changes its retained pose");
        // bhInitEvent runs before the platform requests the soundbank. Decoding
        // the world must not consume a case number intended for that request.
        AudioMixer ordered_audio(data);ordered_audio.set_room_sound_case(1);RoomWorld decoded(bytes,scene,player,motion,frame,ordered_audio,data,{},"RM_0040");check(ordered_audio.take_room_sound_case()==1,"world constructor consumes sound case before Event_init");
        // Background requests retain a playing room-independent voice when
        // the next original room requests the same sequence; init starts defer.
        AudioMixer ambience(data);ambience.load_room_sounds(0,3,0);ambience.begin_room_init();check(ambience.background_sound(0,0),"original ambient init request missing");check(!ambience.active(24),"ambient started before original room initialization completed");ambience.room_fade_in();check(ambience.active(24),"deferred original ambient did not start");const auto plays=ambience.played.size();ambience.load_room_sounds(0,5,0);check(ambience.active(24),"room bank reload cuts off ambient voice");ambience.begin_room_init();ambience.request_background_sound(0,0x300); /* bhBGSeOn2 SeNo as the room scripts store it */ambience.room_fade_in();check(ambience.played.size()==plays,"unchanged original ambient request restarted sequence");// CheckSpecialBank changes only bank-3 MIDI sections; an absent section
        // is not an original stop request, even when the destination is indoors.
        for(unsigned room:{6u,7u}){ambience.load_room_sounds(0,room,0);ambience.begin_room_init();ambience.room_fade_in();check(ambience.active(24),"unrequested ambient stop invented for an absent bank");}
        ambience.stop_background_sound(0);check(!ambience.active(24),"original ambient stop failed");ambience.load_room_sounds(0,5,0);ambience.begin_room_init();ambience.request_background_sound(0,0x300);ambience.room_fade_in();check(ambience.active(24),"original outdoor request does not restart after explicit stop");
        // Player-init stflg 0x40000000 admits the original attr-4 stair volume.
        auto stairs_bytes=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);auto stairs_scene=decode_scene(stairs_bytes);auto stair_player=stairs_scene.spawns.at(0);RoomWorld stairs_world(stairs_bytes,stairs_scene,stair_player,motion,frame,audio);const auto reachable=stairs_world.move({-186,0,86},{-186,0,86});check(reachable.x>-189.3f&&reachable.x<-183.3f&&reachable.y==0,"original player-init status missing from shared collision path");
        // Original type-2 rise/drop driver with actual Claire motion lengths.
        auto body=PlayerAsset::load(data,10);struct Host {const PlayerAsset* body;RECVX_STAIR_STATE* state;unsigned feet=0;} h{&body,nullptr};std::array<float,32> floors{};floors[3]=9;
        RECVX_STAIR_HOST services{&h,[](void*,unsigned,int,float*){return 0;},[](void* p,unsigned m){return static_cast<Host*>(p)->body->motion_frames(m);},[](void* p,unsigned,float* pos){const auto& s=*static_cast<Host*>(p)->state;pos[0]=s.px;pos[1]=s.py;pos[2]=s.pz;},[](void* p,int){++static_cast<Host*>(p)->feet;},nullptr,floors.data(),0};
        for(unsigned down=0;down<2;++down){RECVX_STAIR_STATE s{};h.state=&s;RECVX_STAIR_AREA a{};a.type=2;a.prm0=down;recvx_stairs_begin(&s,&a,floors.data(),0,down?9:0,0,0,down?1:0,0,0);unsigned ticks=0;bool actual_motion=false;while(s.active&&ticks++<300){recvx_stairs_tick(&s,&services);actual_motion|=s.motion==(down?22:21);}check(!s.active&&actual_motion&&s.py==(down?0:9)&&s.floor==(down?0:1),"original short step action does not restore correct floor/control");}check(h.feet>=4,"short step contacts silent");
    }
    std::cout<<"Decimal rooms, original Subpl dispatch, sound-case/fades and short-step actions passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
