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
    // PORTIERUNGSLÜCKE (Policy 2026-10-09): Hier wurde die Begleitfiguren-
    // Steuerung aus subplayer.inc geprüft (Erkennung der Begleit-IDs,
    // em60_init und beide Bewegungs-Dispatchtabellen). Generator und Include
    // sind Quarantäne, siehe native/subplayer.c. Geprüft bleibt nur, dass die
    // Lücke sich wie dokumentiert verhält: keine Figur wird als Begleitfigur
    // erkannt, Init und Tick verändern die Arbeit nicht.
    check(!recvx_is_subplayer(91)&&!recvx_is_subplayer(93)&&!recvx_is_subplayer(54),"subplayer gap must not classify any identity");
    {RECVX_SUBPLAYER sub{};sub.id=93;recvx_subplayer_init(&sub);
     check(!sub.mode0&&!sub.mtn_no&&!sub.mtn_add&&!sub.mode3,"subplayer gap initialized a work");
     check(!recvx_subplayer_tick(&sub)&&!sub.advanced,"subplayer gap dispatched a routine");}
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
        // RM_1000's type-6 ramp is a bhGetGroundPosition candidate. The
        // original player loop records it in gpy after bhCheckWallEx; normal
        // movement must not lift py while crossing the ramp.
        auto ramp_bytes=read_asset_file(data/"rooms/RM_1000.RDT",64*1024*1024);auto ramp_scene=decode_scene(ramp_bytes);auto ramp_player=ramp_scene.spawns.at(2);RoomWorld ramp_world(ramp_bytes,ramp_scene,ramp_player,motion,frame,audio,data,{},"RM_1000");const auto ramp_crossing=ramp_world.move({110,0,81},{110,0,79});check(ramp_crossing.y==0,"bhGetGroundPosition ramp query incorrectly changes player height");
        // PORTIERUNGSLÜCKE (Policy 2026-10-09): Hier liefen die originalen
        // Typ-2-Stufenaktionen (dnu/dnd) aus player_stairs.inc mit echten
        // Claire-Motionlängen. Generator und Include sind Quarantäne, siehe
        // native/player_stairs.c. Geprüft bleibt nur, dass der Treppen-Task
        // die Lücke wie dokumentiert meldet: der Spieler bleibt auf seiner
        // Position und Etage, es wird keine Motion und kein Schritt erzeugt,
        // und die Steuerung ist nach einem Tick wieder frei.
        std::array<float,32> floors{};floors[3]=9;
        struct Host {unsigned feet=0;} h;
        RECVX_STAIR_HOST services{&h,[](void*,unsigned,int,float*){return 0;},[](void*,unsigned){return 0u;},[](void*,unsigned,float* pos){pos[0]=pos[1]=pos[2]=0;},[](void*,int floor,float,float,int){return floor;},[](void* p,int){++static_cast<Host*>(p)->feet;},nullptr,floors.data(),0};
        for(unsigned down=0;down<2;++down){RECVX_STAIR_STATE s{};RECVX_STAIR_AREA a{};a.type=2;a.prm0=std::uint8_t(down);
            recvx_stairs_begin(&s,&a,floors.data(),0,down?9.f:0.f,0,0,down?1:0,0,0);
            check(s.active&&s.py==(down?9.f:0.f)&&s.floor==(down?1:0),"stair gap moved the player or changed its floor");
            check(!recvx_stairs_tick(&s,&services)&&!s.active&&!s.unsupported_services,"stair gap did not release control on its first tick");}
        check(!h.feet,"stair gap requested a footstep sound");
    }
    std::cout<<"Decimal rooms, original Subpl dispatch, sound-case/fades and short-step actions passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
