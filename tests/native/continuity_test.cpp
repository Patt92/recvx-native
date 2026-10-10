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
    // Treppen-, Leiter- und Stufenaktionen (Auftrag B01, native/player_stairs.c):
    // die sechs Originalroutinen kdu/kdd/dnu/dnd/hsu/hsd werden je einmal
    // vollstaendig durchlaufen.  Der Host liefert wie die Laufzeit
    // Motionlaengen, Knotenlagen, Grundhoehen und die Schrittdienste.
    std::array<float,32> floors{};floors[2]=0;floors[3]=9;
    struct Host {unsigned feet=0,actions=0,effects=0,water=0,vibration=0,kaidan=0,head_release=0,badflag=0,last_node=0,probe_node=0;
                 int burst=0,splash_up=0,splash_dn=0,sound=-1;float bits=0;};
    Host h;
    RECVX_STAIR_HOST services{&h,
        /* Testfixture fuer die Wurzelbewegung: die Originalmotionen 31/33/35/37
           heben und 32/34/36/38 senken die Figur ueber ihre Laenge um die
           doppelte Stufentiefe (2*prm2 = 16 bei den geprueften Flaechen).  Das
           ist nicht die Originalkurve, sondern nur ihre Form; die echten
           Schluessel liest die Laufzeit aus dem MTN-Bundle.  Eine feste Hoehe
           waere falsch: kdd beendet sich ueber py <= area.py - 2*prm2. */
        [](void*,unsigned motion,int frame,float pos[3]){
            const bool down=motion>=32u&&motion<=38u&&(motion%2u)==0u;
            pos[0]=0.0f;pos[1]=(down?-16.0f:16.0f)*(float(frame)/26.0f);pos[2]=0.0f;return 1;},
        /* Originallaenge, nicht geschaetzt: die Ruhe-/Stufenmotionen 31/33/35/37
           haben in allen fuenf Spielerkoerpern 27 Bilder, 33/37 haben 31; alle
           PlKDU-Schwellen (6..24) liegen darunter.  Gemessen aus
           data/archives/SYSTEM/0010..0014.bin. */
        [](void*,unsigned){return 27u;},
        [](void* p,unsigned node,float out[3]){static_cast<Host*>(p)->last_node=node;out[0]=5;out[1]=0;out[2]=7;},
        [](void*,int floor,float,float,int){return floor;},
        [](void* p,int sound,int type,int flag){auto& h=*static_cast<Host*>(p);++h.feet;if(type>=2){++h.actions;h.sound=sound;}if(!flag)++h.badflag;},
        [](void* p,const float*,float* top){auto& h=*static_cast<Host*>(p);++h.water;h.probe_node=h.last_node;*top=1.5f;return 1;},
        [](void* p,const RECVX_STAIR_EFFECT* e){auto& h=*static_cast<Host*>(p);++h.effects;
            if(e->id==108){if(e->lkono==10){++h.splash_up;h.bits=e->offset[0];}else ++h.splash_dn;}else if(e->id==11)++h.burst;},
        [](void* p,int,int){++static_cast<Host*>(p)->vibration;},
        [](void* p,int){++static_cast<Host*>(p)->kaidan;},
        /* bhCPM2_act_* case 0: EXP1_I(0) &= ~0x4; |= 0x1E0 (B06 head control). */
        [](void* p){++static_cast<Host*>(p)->head_release;},
        [](void*,float py){return py<4.5f?0:1;},
        floors.data(),0};
    /* recvx_stairs_tick liefert genau in dem Bild 0, in dem der originale
       Abschlussblock die Arbeit zurueckgegeben hat; jedes andere Bild laeuft
       weiter.  Ein 0 ohne Freigabe waere ein Fehler. */
    /* player.c bhCPM1_act_bas (1957-1991) setzt rtspd vor jedem Actionbild.
       Waehrend der Aktion traegt plp->stflg 0x80010010 bzw. 0x80010020, also
       gilt der erste Zweig: dmlvl 2 -> 0.8, sonst 1.0.  Die Laufzeit setzt den
       Wert genauso (native/runtime.cpp); der Host muss ihn wie status/flg
       vorhalten, sonst dreht die Giernaeherung nie. */
    auto seed=[&](RECVX_STAIR_STATE& s){s.rtspd=s.dmlvl==2?0.8f:1.0f;};
    auto run_stairs=[&](RECVX_STAIR_STATE& s){int ticks=0;
        while(s.active&&ticks<400){if(recvx_stairs_tick(&s,&services)==0){check(!s.active,"stair routine stopped without releasing the player work");break;}++ticks;}
        check(!s.active&&ticks<400,"stair action never handed the player back");};
    // (1) Einsprungszustand nach hitchk.c bhCheckExmAtari: case 1 (4682)
    // setzt 0x80010010, case 2 (4750) und die danf-Sonde (4949) setzen
    // 0x80010020; sys->st_flg 0x4 setzen nur case 1 (4686) und case 2
    // (4757), die danf-Sonde nicht.
    for(int action:{14,15,20,21}){
        RECVX_STAIR_STATE s{};RECVX_STAIR_AREA a{};const float py=(action==15||action==21)?9.f:0.f;
        a.type=1;a.py=py;a.prm2=8;a.w=40;a.d=40;
        seed(s);
        check(recvx_stairs_begin(&s,&a,action,floors.data(),0,py,0,0,int(py),0,0)==1,"stair begin refused a type-1 area");
        check(s.active&&s.mode0==1&&s.mode2==action&&(s.status&0x80010010u)==0x80010010u,"stair begin does not match bhCheckExmAtari case 1/2");
        check((s.flg&0x10400u)==0x10400u&&!(s.flg&0x110u)&&(s.st_flg&4u),"stair begin does not set the original player switches");
        // HOKAN|MTN_ATTR bleiben offen; HEADWORK ist seit B06 gebunden und wird
        // nur noch gemeldet, wenn der Host release_head_look nicht stellt.
        check((s.unsupported_services&0x280u)==0x280u,"stair begin stopped reporting its unbound services");
        check(!(s.unsupported_services&RECVX_STAIR_UNBOUND_HEADWORK),"stair begin must not pre-seed the bound head work");}
    for(int action:{16,17}){
        RECVX_STAIR_STATE s{};RECVX_STAIR_AREA a{};const float py=action==16?0.f:9.f;
        a.type=2;a.py=py;
        seed(s);
        recvx_stairs_begin(&s,&a,action,floors.data(),0,py,0,0,int(py),0,0);
        check(s.active&&(s.status&0x80010020u)==0x80010020u,"stair begin does not match the danf branch");
        check((s.st_flg&4u)==(action==17?0u:4u),"danf branch does not leave sys->st_flg 0x4 alone");
        RECVX_STAIR_STATE first=s;
        check(recvx_stairs_tick(&first,&services)!=0&&(first.st_flg&4u),"stair tick does not maintain sys->st_flg 0x4");}
    // (2) Vollstaendiger Ablauf.  Aufwaerts enden kdu/hsu auf der oberen
    // Grundhoehe, abwaerts kdd/hsd auf der unteren; alle vier setzen die
    // Ruhemotion PlMtnAct[wpntp][dmlvl][0] und geben die Arbeit frei.
    struct Expected {int action,floor;float area_py,end_py;bool steps;};
    for(const Expected& e:{Expected{14,1,0.f,9.f,true},Expected{20,1,0.f,9.f,false},Expected{15,0,9.f,0.f,true},Expected{21,0,9.f,0.f,false}}){
        RECVX_STAIR_STATE s{};RECVX_STAIR_AREA a{};a.type=1;a.py=e.area_py;a.prm2=8;a.w=40;a.d=40;
        seed(s);
        recvx_stairs_begin(&s,&a,e.action,floors.data(),0,e.area_py,0,0,int(e.area_py),0,0);
        h.feet=0;h.actions=0;h.kaidan=0;h.head_release=0;
        run_stairs(s);
        check(s.mode0==1&&s.motion==42&&s.floor==e.floor&&s.py==e.end_py&&s.py==floors[e.floor+2],"stair action did not return to the original floor table");
        check((s.flg&0x118u)==0x118u&&!(s.flg&0xD0400u)&&!(s.status&0x80010030u),"stair action did not release its original switches");
        check((s.flg2&1u)&&h.kaidan,"stair action did not open its hokan work or clear the kaidan mark");
        // B06: der mode3-0-Block gibt das Blickziel auf (EXP1_I(0) &= ~0x4; |= 0x1E0).
        check(h.head_release==1,"stair action did not release the head look target");
        check(!(s.unsupported_services&RECVX_STAIR_UNBOUND_HEADWORK),"head work still reported unbound with a bound service");
        check((h.feet>0)==e.steps,"stair action changed its original step sounds");}
    // (3) Danf-Stufe mit gesetztem st_flg 0x40: bhCheckWater an Knoten 19
    // (ply_id 0), bhSetEffect 108 mit lkono 15 und sechs bhSetEffectTb 11
    // (zwei Haelften a drei Drehungen), danach CallPlayerActionSe 522.
    {RECVX_STAIR_STATE s{};RECVX_STAIR_AREA a{};a.type=2;a.py=9;a.prm2=8;a.w=40;a.d=40;
        seed(s);
        recvx_stairs_begin(&s,&a,17,floors.data(),0,9,0,0,1,0,0);
        s.st_flg|=0x40u;
        h.water=0;h.vibration=0;h.effects=0;h.burst=0;h.splash_dn=0;h.actions=0;h.sound=-1;h.probe_node=0;
        run_stairs(s);
        check(h.vibration==1&&h.water==1&&h.probe_node==19,"danf step did not probe the original water node");
        check(h.splash_dn==1&&h.burst==6&&h.effects==7,"danf splash does not carry the original effect set");
        check(h.actions==1&&h.sound==522,"danf splash did not request CallPlayerActionSe 522");
        check((s.unsupported_services&RECVX_STAIR_UNBOUND_EFFECT_OFF)!=0,"danf splash stopped reporting the unbound effect offset");}
    // (4) dnu reicht plp->ax/ay/az als Bitmuster an bhSetEffect 108 weiter
    // ((POINT*)&plp->px ueberlagert ax/ay/az), nicht als Winkel.
    {RECVX_STAIR_STATE s{};RECVX_STAIR_AREA a{};a.type=1;a.prm2=4;
        seed(s);
        recvx_stairs_begin(&s,&a,16,floors.data(),0,0,0,0,0,0,0);
        s.status|=0x100000u;s.ax=0x3F800000;
        h.splash_up=0;h.bits=0;
        for(int ticks=0;s.active&&h.splash_up==0&&ticks<40;++ticks)recvx_stairs_tick(&s,&services);
        check(h.splash_up==1&&h.bits==1.0f,"dnu effect offset is not the raw plp->ax bit pattern");}
    check(!h.badflag,"stair action requested a step sound with ListNo -1");

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
    }
    std::cout<<"Decimal rooms, original Subpl dispatch, sound-case/fades and the six stair/ladder/step actions passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
