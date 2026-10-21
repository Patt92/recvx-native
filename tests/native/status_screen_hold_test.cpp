#define SDL_MAIN_HANDLED
// Statusbildschirm-Taskrahmen (StatusTask, native/ui.h): wie lange st_flg 0x8 steht.
//
// Original (Semantikreferenz, nicht gebaut):
//  - bhCheckSubTask (system.c:603-611, Task 7): st_flg |= 0x8, mn_mode0 = 5, sp_flg 0x30,
//    bhSetScreenFade(0xFF000000, 5); bhMainSequence im selben Task 7 ruft bhControlScreenFade
//    (game.c:38-40). Der Fade loescht cb_flg 0x2 in seinem fuenften Steuerdurchlauf (screen.c:50-90).
//  - bhSysCallMonitor Modus 5 (system.c:1953-2025, Task 20): mn_md1 0 -> 1 -> 2 (wartet auf
//    cb_flg 0x2 frei, setzt ts_flg 0x80) -> 3 (SbsTextureInit) -> 4 (ItemTaskCheck-Einstieg,
//    ts_flg 0x200 frei). Erst danach laeuft Task 9 (bhSysCallItemselect, system.c:903).
//  - ItemTaskCheck (sub1.c:3025-3239): nach statusflg 0x80000 (SpriteH, Ende der Ausfahrt)
//    vier taskloop-Durchlaeufe, Abbau mit st_flg &= ~0x8 erst im fuenften.
//  - RM_0090: scd1 @1378/@1472 startet auf Boden 4 in jedem Durchlauf Ebene 0 neu (EvtOn 0,0;
//    Event_exec/Event_init event.c:13001/12956), solange st_flg 0x8/0x10000000/0x40000 frei sind.
//    Ereignis 19 (Ebene 0, scd1 @2110 beim Untersuchen) schlaeft 30 Durchlaeufe (bhSleep/bhSleeping
//    event.c:9100-9129) und schreibt im 31. die Klappe obwp[5] absolut 0 (Common 9).
// Der Pruefstand koppelt dieselben nativen Besitzer in der Reihenfolge von runtime.cpp: Eingabe
// und allgemeiner Statuspass, Kartentask-Kopf (Task 20 des Vorbilds, dann Task 7 mit Fade), Eventpass,
// Kistenpass, Fade/Welt nur ohne Statusbildschirm. Die Platzierung ist eine Diagnoseaufstellung.
#include "input.h"
#include "world.h"
#include "movie.h"
#include "renderer.h"
#include "asset_io.h"
#include "room.h"
#include "box_menu.h"
#include "screen_fade.h"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
namespace {
// Fade und cb_flg 0x2 wie runtime.cpp fade_set/fade_control (screen.c:21-23/50-90).
struct Fade {
    ScreenFade fade;unsigned own=0;unsigned* flags=&own; // sys->cb_flg (im Raum: EventVm flags[10][0])
    bool pending()const{return (*flags&2u)!=0;}
    void set(std::uint32_t argb,float ct){fade.set(argb,ct);*flags|=2u;}
    bool control(){if(pending()&&fade.control()){*flags&=~2u;return true;}return false;}
};
// 1) Der Rahmen allein, gegen den Originalfade.
void frame_alone(){
    {   // Ohne Rahmen (StatusMain-Pruefstand) bleibt alles beim alten Ablauf.
        StatusTask t;check(!t.owned()&&!t.holding(),"ungebundener Rahmen haelt st_flg 0x8");
        check(t.item_task_check(false)==StatusTask::Pass::Suspended,"ungebundener Rahmen meldet StatusMain");
    }
    StatusTask t;t.frame();
    check(t.item_task_check(false)==StatusTask::Pass::Suspended,"Task 9 laeuft vor bhCheckSubTask");
    Fade f;f.set(0xFF000000u,5.0f);t.enter();f.control(); // Task 7 des Eintrittsbildes
    check(t.entering()&&t.game_running()&&t.md1()==0,"bhCheckSubTask beginnt nicht mit mn_md1 0");
    unsigned frame=0,cleared=0,stopped=0,ready=0;
    for(;frame<40&&!t.running();++frame){
        // Task 9 dieses Bildes vor Task 20: StatusMain laeuft nicht, solange Modus 5 laeuft.
        check(t.item_task_check(false)==StatusTask::Pass::Suspended,"StatusMain waehrend bhSysCallMonitor Modus 5");
        const bool pending=f.pending();const unsigned before=t.md1();
        t.monitor(pending);                                   // Task 20
        if(before==2&&pending)check(t.md1()==2&&t.game_running(),"Modus 5 verlaesst mn_md1 2 mit gesetztem cb_flg 0x2");
        if(!t.game_running()&&!stopped)stopped=frame+1;
        if(t.running())ready=frame+1;
        if(t.game_running()&&f.control())cleared=frame+1;     // Task 7 des Folgebildes
    }
    check(t.running(),"ItemTaskCheck-Einstieg erreicht");
    // ts_flg 0x80 im ersten Monitorlauf nach dem fadeloeschenden Task 7, Einstieg zwei Laeufe spaeter.
    check(cleared&&stopped==cleared+1,"ts_flg 0x80 nicht im ersten Monitorlauf nach cb_flg 0x2: Fade "+std::to_string(cleared)+", Stopp "+std::to_string(stopped));
    check(ready==stopped+2,"SbsTextureInit/ItemTaskCheck nicht als mn_md1 3 und 4 nach dem Stopp");
    check(t.item_task_check(false)==StatusTask::Pass::Main,"Task 9 laeuft nach dem Einstieg nicht");
    // taskloop: vier weitere StatusMain-Durchlaeufe nach statusflg 0x80000, Abbau im fuenften.
    for(unsigned n=1;n<=4;++n){check(t.item_task_check(true)==StatusTask::Pass::Main&&t.taskloop()==n,"taskloop "+std::to_string(n));check(t.holding(),"st_flg 0x8 vor taskloop 4 abgebaut");}
    check(t.item_task_check(true)==StatusTask::Pass::Exit&&!t.holding(),"Abbau nicht bei taskloop 4");
    check(t.item_task_check(true)==StatusTask::Pass::Suspended,"Task 9 laeuft nach dem Abbau weiter");
    // Rueckkehr aus der Karte (system.c case 6): ohne Monitorlauf direkt StatusMain, taskloop von vorn.
    t.resume();check(t.running()&&t.taskloop()==0&&t.item_task_check(false)==StatusTask::Pass::Main,"case 6 setzt StatusMain nicht fort");
    t.reset();check(t.owned()&&!t.holding(),"reset loest die Bindung des Rahmens");
}
// 2) Allgemeiner Statuspass (StatusMenu): kein Bild, keine Eingabe vor dem Einstieg; Abbau nach taskloop.
void status_menu(){
    Inventory inv;StatusMenu m;m.task.frame();
    m.input(MenuInput::Open,inv);check(m.active&&!m.screen(),"Statusbildschirm sichtbar vor bhCheckSubTask");
    for(unsigned i=0;i<3;++i)m.tick(inv);
    check(m.active&&!m.screen()&&!m.slide.moving(),"StatusMain (SpriteH) lief vor dem Monitorlauf");
    Fade f;f.set(0xFF000000u,5.0f);m.task.enter();f.control();
    m.input(MenuInput::Open,inv);check(m.active,"Statustaste waehrend Modus 5 schliesst den Schirm");
    unsigned n=0;while(!m.task.running()&&n++<40){m.tick(inv);m.task.monitor(f.pending());if(m.task.game_running())f.control();}
    check(m.screen(),"Statusbildschirm nach dem Einstieg nicht sichtbar");
    n=0;do m.tick(inv);while(m.slide.moving()&&n++<40);
    m.input(MenuInput::Open,inv);check(m.closing,"StatusCancel (pad_ps 0x4000) nach der Einfahrt nicht angenommen");
    n=0;while(m.task.taskloop()==0&&n++<40)m.tick(inv);
    check(m.task.taskloop()==1&&m.active,"taskloop 1 fehlt nach statusflg 0x80000");
    for(unsigned k=2;k<=4;++k){m.tick(inv);check(m.task.taskloop()==k&&m.active,"taskloop "+std::to_string(k));}
    m.tick(inv);check(!m.active&&!m.task.holding(),"Abbau nicht im fuenften ItemTaskCheck");
}
// 3) RM_0090: dieselbe Kopplung wie rm0090_box_test.cpp, ergaenzt um Rahmen, Kiste und Fade.
struct Room {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
    Inventory inventory;StatusMenu menu;MessageDialog dialog;AudioMixer audio;MoviePlayer movie;PlayerAsset body;RoomWorld world;EventVm vm;GameplayInput input;BoxMenu box;Fade fade;
    bool framed=true,was_active=false;
    // Messwerte je Durchlauf
    std::vector<unsigned> st_flg,ev19_alive;
    explicit Room(const std::filesystem::path& data,bool frame_task):bytes(read_asset_file(data/"rooms/RM_0090.RDT",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),movie(data,{},audio),body(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0090"),vm(bytes,inventory,dialog),input(inventory,menu,dialog,&vm),framed(frame_task){
        bind_item_names(read_asset_file(data/"files/SYSMES.ALD",4*1024*1024));
        world.set_player_asset(&body);{const auto id=RoomIdentity::parse("RM_0090.RDX");vm.values[0]=id.stage;vm.values[1]=id.room;}
        vm.binding=[&](const EventCommand& c){if(c.opcode==0x5e){movie.start(c.bytes[1]);return true;}if(c.opcode==0x5f){movie.stop();return true;}return world.bind(c);};
        vm.task_started=[&](unsigned n){world.start_task(n);};vm.animation_frame=[&](unsigned k,unsigned n){return world.frame(k,n);};vm.object_position=[&](unsigned k,unsigned n){return world.position(k,n);};
        vm.enemy_joint_position=[&](unsigned n,unsigned node){return world.enemy_joint_position(n,node);};vm.set_item_position=[&](unsigned n,NJS_POINT3 p){world.set_item_position(n,p);};vm.enemy_disable=[&](unsigned n){world.deactivate_enemy(n);};
        vm.audio_active=[&](unsigned n){return audio.active(n);};vm.movie_active=[&]{return movie.active();};vm.movie_time=[&]{return movie.time_centiseconds();};vm.enemy_dead=[&](unsigned n){return world.enemy_dead(n);};
        input.video_active=[&]{return movie.active();};input.skip_video=[&]{movie.stop();};vm.door=[](const DoorRequest&){return true;};
        // runtime.cpp gameplay_input.interact: bhCheckExmAtari ueber das Ereignismodul
        input.interact=[&]{if(!vm.exm_gate(world.player_mode()&0xffu))return;vm.interact(actor.position,actor.yaw,world.floor_at(actor.position),world.player_front_point(),world.player_front_offset());};
        input.modal_input=[&](MenuInput){return box.active();};
        if(framed)menu.task.frame();
        fade.flags=&vm.flags[10][0];
        audio.mount_sound_afs(vm.values[0]);audio.begin_room_init();vm.initialize();audio.load_room_sounds(vm.values[0],vm.values[1],audio.take_room_sound_case());audio.room_fade_in();
    }
    // runtime.cpp map_task, Statusteil (Integrationspatch): Eintritt mit Fade und erstem
    // bhControlScreenFade, sonst Task 20 des Vorbilds und Task 7 mit bhControlScreenFade.
    void status_frame(){
        if(menu.active&&!was_active){if(framed&&!menu.task.holding()){fade.set(0xFF000000u,5.0f);menu.task.enter();fade.control();}}
        else if(framed&&menu.active&&menu.task.entering()){menu.task.monitor(fade.pending());if(menu.task.game_running())fade.control();}
        was_active=menu.active;
    }
    void tick(unsigned pad_ps=0){
        input.apply({});status_frame();
        vm.menu_open=menu.active;vm.check_floor(actor.position,actor.yaw,world.floor_at(actor.position));vm.control_exm();vm.tick();
        st_flg.push_back(vm.flags[5][0]);ev19_alive.push_back(task(19));
        input.synchronize();box.tick(0,pad_ps,inventory,menu,&vm,audio);menu.sprite_on_off(inventory);
        if(!menu.active)fade.control(); // game.c:40 in bhMainSequence
        if(!menu.active){world.set_simulation_flags(vm.flags[6][0]);world.tick();if(!scene.cameras.empty())world.control_camera(scene,scene.cameras.at(0));vm.flags[4][0]|=world.take_enemy_room_flags();}
        movie.tick();audio.mix();
        for(const auto& e:vm.errors)std::cerr<<e<<"\n";check(vm.errors.empty(),"RM_0090: original event stream blocked");
    }
    void ticks(unsigned n){for(unsigned i=0;i<n;++i)tick();}
    bool task(unsigned event)const{for(const auto& t:vm.task_states())if(t.event==event)return true;return false;}
    bool started(unsigned event,std::size_t from=0)const{for(std::size_t i=from;i<vm.dispatched.size();++i)if(vm.dispatched[i]==event)return true;return false;}
    void place(float x,float z,Angle yaw=0){actor.position={x,0,z};actor.yaw=yaw;world.place_player(actor.position);}
    int flap()const{const auto a=world.model_pose(2,5,1).angles;return a[2];}
};
constexpr int step=(int(5*(65536.0f/360.0f)))>>1; // event.c:12015 addaz
constexpr int opened=30*step;                      // Ereignis 31: FOR 30 x Common 18
struct Run {int flap=0;unsigned held_after_close=0,exit_pass=0,ev19_end=0;bool ev19_with_hold=false;};
Run security_box(const std::filesystem::path& data,bool framed){
    Room r(data,framed);r.place(60,65);r.ticks(2);
    for(unsigned slot=1;slot<12;++slot)r.inventory.slots()[slot]=0;r.inventory.acquire(55,1); // Diagnose: Feuerzeug (Metall)
    const auto mark=r.vm.dispatched.size();
    r.place(34.5f,5.f,16384);r.ticks(2);r.place(44.5f,5.f,0);unsigned n=0;while(!r.started(31,mark)&&n++<10)r.tick();
    check(r.started(31,mark),"Boden 4 mit Metall oeffnet Box 5 nicht (scd1 @1490/@1498)");
    n=0;while((r.task(2)||r.task(31)||r.dialog.active())&&n++<600){r.tick();if(r.dialog.active()&&(r.dialog.turnable()||r.dialog.closable()||r.dialog.choosing())){r.dialog.selection=1;r.vm.confirm_dialog();}}
    r.ticks(2);check(r.flap()==opened,"Klappe obwp[5] vor dem Untersuchen nicht offen");
    check((r.vm.flags[10][0]&0x200u)&&r.vm.values[16]==4,"Spieler steht nicht auf Boden 4");
    // Diagnose: Standort vor der Box wie im Runtime-Replay status-hold-rm0090-earliest-cancel.txt
    // (--state Bild 229: actor 44.5/9.5, Blick 0); die Untersuchungssonde type 4 ist pos - 2*Blick (hitchk.c:4601-4618).
    r.place(44.5f,9.5f,0);
    // Untersuchen (Eingabe am Bildanfang wie replay_input): scd1 @2110 -> Ereignis 19, cb_flg 0x40000.
    const auto examine=r.vm.dispatched.size();r.input.menu_input(MenuInput::Confirm);r.tick();
    check(r.started(19,examine)&&r.task(19),"Untersuchen startet Ereignis 19 nicht");
    check(r.menu.active&&(r.vm.flags[10][0]&0x40000u),"Untersuchen fordert den Sicherheitsbox-Schirm nicht an");
    const std::size_t first=r.st_flg.size();
    // Abbrechen im fruehesten Durchlauf: pad_ps 0x1000 in jedem Kistenpass (ItemBox nimmt ihn erst ohne statusflg 0x40).
    n=0;while(r.menu.active&&n++<200){
        if(framed&&r.menu.task.entering())check(!r.box.active(),"ItemBoxInit waehrend bhSysCallMonitor Modus 5");
        r.tick(0x1000u);}
    check(!r.menu.active,"Sicherheitsbox-Schirm schliesst nicht");
    Run out;out.exit_pass=unsigned(r.st_flg.size()-first);
    r.ticks(40);
    for(std::size_t i=first;i<r.ev19_alive.size();++i)if(r.ev19_alive[i]&&(i+1==r.ev19_alive.size()||!r.ev19_alive[i+1])){out.ev19_end=unsigned(i+1-first);out.ev19_with_hold=(r.st_flg[i+1]&8u)!=0;break;}
    out.flap=r.flap();check(!r.task(19),"Ereignis 19 laeuft noch");
    return out;
}
}
int main(int argc,char** argv){try{
    _Make_SinTable();
    frame_alone();status_menu();
    if(argc<2){std::puts("status task frame: monitor mode 5 / ItemTaskCheck taskloop (no data, RM_0090 skipped)");return 0;}
    const std::filesystem::path data=argv[1];
    if(!std::filesystem::exists(data/"rooms/RM_0090.RDT")){std::puts("status task frame: RM_0090 missing, room part skipped");return 0;}
    // Mit Rahmen: st_flg 0x8 haelt, bis Ereignis 19 sein Common 9 erreicht -> Klappe 0.
    const auto held=security_box(data,true);
    check(held.ev19_with_hold,"Ereignis 19 endet nicht unter st_flg 0x8 (Abbau in Durchlauf "+std::to_string(held.exit_pass)+")");
    check(held.flap==0,"Klappe obwp[5] nach Untersuchen und sofortigem Abbrechen offen: "+std::to_string(held.flap));
    // Gegenstand ohne Rahmen (alter Ablauf: StatusMain sofort, Abbau mit dem SpriteH-Ende):
    // scd1 @1472 verdraengt Ereignis 19 vor seinem Common 9, die Klappe bleibt offen.
    const auto legacy=security_box(data,false);
    check(legacy.flap==opened&&!legacy.ev19_with_hold,"Gegenprobe ohne Rahmen schliesst die Klappe");
    check(held.exit_pass>legacy.exit_pass,"Rahmen verlaengert st_flg 0x8 nicht");
    std::printf("status task frame: st_flg 0x8 held %u passes (without frame %u); event 19 Common 9 at pass %u under st_flg 0x8, obwp[5] flap closed\n",held.exit_pass,legacy.exit_pass,held.ev19_end);
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
