#define SDL_MAIN_HANDLED
// RM_0090 Security Boxen: obwp[5] (Kamera 9, Boden 4) und obwp[14] (Kamera 8,
// Boden 3) tragen beide die Kennung 37, sind aber zwei getrennte O_WRK mit je
// eigenem ML_WORK: room.c:126-133 ruft bhSetObjMdl fuer jedes Objekt mit seinem
// eigenen Modellblock auf (RDT 0x8e040 bzw. 0x9fd80), bhMlbBinRealize
// (binfunc.c:4) realisiert objP an Ort und Stelle. Die Klappe ist objP[1]
// (Modellknoten 1 bei (-1.85, 7, 0)), der Deckel objP[2]; kein Raumskript
// schreibt den Deckel.
//
// Schreiber der Klappe im Original (Raumskript RM_0090; "scd1 @n" = Byteoffset ab Beginn von
// scd1, im EVTRACE der Runtime erscheint derselbe Befehl bei n + 1960):
//   Ereignis 30/31: bhLoadWork 2,14,1 bzw. 2,5,1; FOR 30 { Common 13 (0,0,0),
//                   Common 6 (0,0,5), Common 18 } -> addaz = (int)(5*65536/360)>>1
//                   = 455 je Durchlauf relativ (event.c:12015, :12310), 30 x 455 = 13650.
//   Ereignis 18/19: Sleep 30, dann bhLoadWork 2,14,1 bzw. 2,5,1; Common 13; Common 9
//                   (0,0,0) absolut (event.c:12136).
//   Ereignis 7:     beide Klappen absolut 0 (Alarmruecksetzung).
//   scd1 @1936:     Boden 4 betreten und rm_flg 0x0a = 0 -> obwp[14] Klappe absolut 0
//                   (nicht obwp[5]); @1980 Boden 3 mit rm_flg 0x0a -> Ereignis 30 + 32.
//   scd1 @1378:     Boden 4 (cb_flg 0x200, flr_idx 4), ev_flg 0x35 = 0, st_flg 0x8/
//                   0x10000000/0x40000 = 0 -> in JEDEM Durchlauf EvtOn(0,0); mit rm_flg 1 = 0
//                   und rm_flg 2 = 1 (Metall, Ereignis 0) zusaetzlich Ereignis 2 (Kamera 9)
//                   und 31 auf Ebene 3.
// Der Test prueft, dass jede dieser Schreibstellen genau ihren Work trifft. Eine
// geteilte Knoten-/Motion-/Vertexsicht der beiden gleichen Box-Objekte oder ein
// falscher Index faellt an der jeweils anderen Box auf.
#include "input.h"
#include "world.h"
#include "movie.h"
#include "renderer.h"
#include "asset_io.h"
#include "room.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
namespace {
// Wie tests/native/event_lifecycle_test.cpp: dieselbe Kopplung wie runtime.cpp.
struct Room {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
    Inventory inventory;StatusMenu menu;MessageDialog dialog;AudioMixer audio;MoviePlayer movie;PlayerAsset body;RoomWorld world;EventVm vm;GameplayInput input;
    explicit Room(const std::filesystem::path& data):bytes(read_asset_file(data/"rooms/RM_0090.RDT",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0090"),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm){
        bind_item_names(read_asset_file(data/"files/SYSMES.ALD",4*1024*1024));
        world.set_player_asset(&body);{const auto id=RoomIdentity::parse("RM_0090.RDX");vm.values[0]=id.stage;vm.values[1]=id.room;}
        vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
        vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};
        vm.enemy_joint_position=[&](unsigned n,unsigned node){return world.enemy_joint_position(n,node);};vm.set_item_position=[&](unsigned n,NJS_POINT3 p){world.set_item_position(n,p);};vm.enemy_disable=[&](unsigned n){world.deactivate_enemy(n);};
        vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
        input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
        audio.mount_sound_afs(vm.values[0]);audio.begin_room_init();vm.initialize();audio.load_room_sounds(vm.values[0],vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
    }
    void tick(){input.apply({});vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.tick();input.synchronize();world.set_simulation_flags(vm.flags[6][0]);world.tick();
        if(!scene.cameras.empty())world.control_camera(scene,scene.cameras.at(0));vm.flags[4][0]|=world.take_enemy_room_flags();movie.tick();audio.mix();
        for(const auto& e:vm.errors)std::cerr<<e<<"\n";check(vm.errors.empty(),"RM_0090: original event stream blocked");}
    void ticks(unsigned n){for(unsigned i=0;i<n;++i)tick();}
    bool task(unsigned event)const{for(const auto& t:vm.task_states())if(t.event==event)return true;return false;}
    // Seit `from` (Index in vm.dispatched) gestartete Ereignisse.
    bool started(unsigned event,std::size_t from=0)const{for(std::size_t i=from;i<vm.dispatched.size();++i)if(vm.dispatched[i]==event)return true;return false;}
    // Diagnoseplatzierung (kein Laufweg): Spieler auf eine Bodenflaeche des Raums setzen.
    void place(float x,float z,Angle yaw=0){actor.position={x,0,z};actor.yaw=yaw;world.place_player(actor.position);}
    // Diagnose: Inventar leeren (Taschenplaetze 1..11, die bhPlItemCheck event.c:1368 liest).
    void strip_items(){for(unsigned slot=1;slot<12;++slot)inventory.slots()[slot]=0;}
    std::array<int,3> flap(unsigned object)const{return world.model_pose(2,object,1).angles;}
    std::array<int,3> lid(unsigned object)const{return world.model_pose(2,object,2).angles;}
};
constexpr int step=(int(5*(65536.0f/360.0f)))>>1; // event.c:12015 addaz
constexpr int opened=30*step;                      // FOR 30 x Common 18
static std::string text(const std::array<int,3>& a){return "("+std::to_string(a[0])+","+std::to_string(a[1])+","+std::to_string(a[2])+")";}
static void expect_flap(const Room& r,unsigned object,int z,const char* where){
    const auto a=r.flap(object);
    check(a[0]==0&&a[1]==0&&a[2]==z,std::string(where)+": obwp["+std::to_string(object)+"] Klappe "+text(a)+" statt (0,0,"+std::to_string(z)+")");
    const auto l=r.lid(object);check(l[0]==0&&l[1]==0&&l[2]==0,std::string(where)+": obwp["+std::to_string(object)+"] Deckel bewegt "+text(l));
}
// Ein Ereignis auf seiner Originalebene starten und bis zu seinem Ende laufen lassen.
static void run_event(Room& r,unsigned level,unsigned event,const char* why){
    r.vm.start(level,event);unsigned n=0;r.tick();while(r.task(event)&&n++<200)r.tick();check(!r.task(event),std::string(why)+": Ereignis "+std::to_string(event)+" endet nicht");
}
}
int main(int argc,char** argv){try{
    if(argc<2)return 0;_Make_SinTable();const std::filesystem::path data=argv[1];
    // Neutraler Standort im Detektorgang: keine der Bodenflaechen 3..6 (scd1 @820/@990/@1378/@1548).
    const NJS_POINT3 neutral{60,0,65};
    {   // 1) Beide Boxen sind eigene Works mit gleicher Ruhelage.
        Room r(data);r.place(neutral.x,neutral.z);r.ticks(2);
        const auto* a=r.world.state(2,5);const auto* b=r.world.state(2,14);
        check(a&&b&&a!=b,"obwp[5]/obwp[14] ohne getrennte native Works");
        check(a->identity==37&&b->identity==37,"RM_0090 Security-Box-Kennung 37 erwartet");
        expect_flap(r,5,0,"Ruhelage");expect_flap(r,14,0,"Ruhelage");
        // 2) Ereignis 31 (Box 5 oeffnen) trifft nur obwp[5]; Ereignis 30 nur obwp[14].
        run_event(r,3,31,"Box 5 oeffnen");expect_flap(r,5,opened,"nach Ereignis 31");expect_flap(r,14,0,"nach Ereignis 31");
        run_event(r,3,30,"Box 14 oeffnen");expect_flap(r,14,opened,"nach Ereignis 30");expect_flap(r,5,opened,"nach Ereignis 30");
        // 3) Common 18 ist relativ zu objP (event.c:12310): ein zweites Oeffnen ohne
        //    Schliessen addiert erneut 75 Grad - Originalsemantik, kein Host-Klemmen.
        run_event(r,3,31,"Box 5 zweimal");expect_flap(r,5,2*opened,"Ereignis 31 zweimal");expect_flap(r,14,opened,"Ereignis 31 zweimal");
        // 4) Schliessen: Ereignis 19 nur obwp[5], Ereignis 18 nur obwp[14] (absolut 0 nach 30 Bildern).
        run_event(r,0,19,"Box 5 schliessen");expect_flap(r,5,0,"nach Ereignis 19");expect_flap(r,14,opened,"nach Ereignis 19");
        run_event(r,0,18,"Box 14 schliessen");expect_flap(r,14,0,"nach Ereignis 18");expect_flap(r,5,0,"nach Ereignis 18");
        // 5) Ereignis 7 (Alarmruecksetzung) setzt beide zurueck.
        run_event(r,3,31,"Box 5 fuer Ereignis 7");run_event(r,3,30,"Box 14 fuer Ereignis 7");
        r.vm.start(0,7);r.ticks(2);expect_flap(r,5,0,"nach Ereignis 7");expect_flap(r,14,0,"nach Ereignis 7");
    }
    {   // 6) scd1 @1936: Boden 4 ohne Metall schliesst obwp[14] (nicht obwp[5]) und setzt rm_flg 0x0a;
        //    @1980: Boden 3 mit rm_flg 0x0a startet Ereignis 30 (obwp[14]) und 32.
        Room r(data);r.place(neutral.x,neutral.z);r.ticks(2);
        run_event(r,3,30,"Box 14 vorher offen");run_event(r,3,31,"Box 5 vorher offen");
        r.strip_items(); // ohne Metall: Ereignis 0 laesst rm_flg 2 frei
        const auto mark=r.vm.dispatched.size();
        r.place(44.5f,10.f,32768);r.ticks(3);
        check(r.vm.flag(4,10),"scd1 @1966: rm_flg 0x0a nach Boden 4 nicht gesetzt");
        expect_flap(r,14,0,"Boden 4 (scd1 @1960)");expect_flap(r,5,opened,"Boden 4 ohne Metall (scd1 @1960)");
        check(!r.started(2,mark)&&!r.started(31,mark),"Boden 4 ohne Metall startet die Box-5-Oeffnung");
        r.place(72.f,95.f);r.ticks(2);r.place(74.f,104.f);r.ticks(2);
        check(r.started(32,mark),"scd1 @1998: Ereignis 32 nach Rueckkehr auf Boden 3 fehlt");
        unsigned n=0;while(r.task(30)&&n++<100)r.tick();
        expect_flap(r,14,opened,"Boden 3 mit rm_flg 0x0a (Ereignis 30)");expect_flap(r,5,opened,"Boden 3 mit rm_flg 0x0a");
    }
    {   // 7) Rueckweg mit Metall: Boden 6 (rm_flg 1 = 0, kein Alarm solange rm_flg 2 noch 0),
        //    dann Boden 4 -> Ereignis 0 setzt rm_flg 2, scd1 @1490/@1498 startet 2 (Kamera 9) und 31.
        Room r(data);r.place(neutral.x,neutral.z);r.ticks(2);
        r.strip_items();r.inventory.acquire(55,1); // Diagnose: Feuerzeug (Metall, ev0 bhPlItemCheck 0x37) ohne Zwischenstopp in Boden 3/4
        const auto mark=r.vm.dispatched.size();
        r.place(34.5f,5.f,16384);r.ticks(2);check(!r.vm.flag(1,0x35),"Boden 6 mit veraltetem rm_flg 2 loest Alarm aus");
        r.place(44.5f,5.f,0);unsigned n=0;while(!r.started(31,mark)&&n++<10)r.tick();
        check(r.started(2,mark)&&r.started(31,mark),"Boden 4 mit Metall oeffnet Box 5 nicht (scd1 @1490/@1498)");
        n=0;while(r.task(31)&&n++<100)r.tick();
        expect_flap(r,5,opened,"Rueckweg mit Metall");expect_flap(r,14,0,"Rueckweg mit Metall (scd1 @1960)");
        // 8) Untersuchen ohne Statusbildschirm: scd1 @1472 ersetzt in jedem Durchlauf auf Boden 4
        //    Ebene 0 durch Ereignis 0 (Event_exec -> Event_init, event.c:13001/12956), solange
        //    st_flg 0x8/0x10000000/0x40000 frei sind. Ereignis 19 (Ebene 0, Sleep 30) erreicht
        //    sein Common 9 dann nicht - die Klappe bleibt offen. Originalablauf, kein Host-Fehler.
        //    Zuerst die Kamera-9-Szene (Ereignis 2, Meldung) beenden: erst danach laeuft der
        //    Spielerpass wieder und haelt cb_flg 0x200 fuer Boden 4 (hitchk.c:5297).
        n=0;while((r.task(2)||r.dialog.active())&&n++<600){r.tick();if(r.dialog.active()&&(r.dialog.turnable()||r.dialog.closable()||r.dialog.choosing())){r.dialog.selection=1;r.vm.confirm_dialog();}}
        check(!r.task(2)&&!r.dialog.active(),"Kamera-9-Szene (Ereignis 2) endet nicht");
        r.ticks(2);check((r.vm.flags[10][0]&0x200u)&&r.vm.values[16]==4,"Spieler steht nach Ereignis 2 nicht auf Boden 4");
        r.vm.start(0,19);r.ticks(40);
        check(!r.task(19),"Ereignis 19 laeuft trotz scd1-Neustart von Ebene 0 weiter");
        expect_flap(r,5,opened,"Ereignis 19 von scd1 @1472 verdraengt");
    }
    std::puts("RM_0090 security boxes: obwp[5]/obwp[14] node writes of events 7/18/19/30/31 and scd1 @1936/@1980/@1498 hit only their own work");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
