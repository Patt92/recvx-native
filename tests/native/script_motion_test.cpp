#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "binary_view.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static EventCommand command(std::initializer_list<std::uint8_t> bytes){EventCommand c;c.bytes=bytes;c.opcode=*bytes.begin();return c;}
int main(int argc,char** argv){try{
    if(argc<2){std::cout<<"Supply cooked data for original script motion regression.\n";return 0;}
    _Make_SinTable();const std::filesystem::path data=argv[1];
    const auto bytes=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);
    const auto scene=decode_scene(bytes);auto player=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;
    AudioMixer sound(data);RoomWorld world(bytes,scene,player,motion,frame,sound,data,{},"RM_0050");
    auto asset=PlayerAsset::load(data);world.set_player_asset(&asset);
    Inventory inventory;MessageDialog dialog;EventVm vm(bytes,inventory,dialog);
    std::vector<unsigned> starts;bool active=false,wrapped_visible=false,hidden=false;float previous=0;unsigned samples=0;
    vm.binding=[&](const EventCommand& c){if(c.event==12&&c.opcode==0x69&&(c.bytes[1]==24||c.bytes[1]==34)){starts.push_back(c.bytes.size()==10?unsigned(c.bytes[8])|(unsigned(c.bytes[9])<<8):0);active=true;previous=0;}return world.bind(c);};
    vm.task_started=[&](unsigned level){world.start_task(level);};
    vm.animation_frame=[&](unsigned kind,unsigned index){return world.frame(kind,index);};
    vm.initialize();player.position={-248,0,54}; // Diagnostic placement inside the original trigger, no event/flag rewrites.
    for(unsigned tick=0;tick<220;++tick){
        vm.check_floor(player.position,player.yaw,world.floor_at(player.position));vm.tick();world.tick();sound.mix();
        const auto& e=*world.state(1,2);
        // Der Raum blendet die Figur nach dem Ziehen mit bhInitModelSet (0x24)
        // aus; das ist das Original-stflg 0x1000000 und damit das Zeichengate
        // (entity_hidden), nicht das rohe hidden-Feld.
        const auto drawn=!e.hidden&&!e.inactive;
        if(active&&drawn){const auto root=world.model_pose(1,2).position;check(root.x<-220&&root.x>-250,"drag leaves the original motion's position range");
            if(previous)check(root.x>=previous-.001f,"drag returns towards its starting position within a motion section");
            previous=root.x;++samples;
            if(e.frame==0){check(e.pose_frame()==84*65536,"visible wrap sampled frame zero instead of final frame 84");check(root.x>-231,"last visible drag frame flashes back to the ground position");wrapped_visible=true;}
        }
        if(active&&!drawn)hidden=true;
    }
    check(vm.errors.empty(),"original drag trigger/script is blocked");check(starts==std::vector<unsigned>{0,60,64},"Common 34 lost the original frame 60/64 offsets or restarted the event");
    check(samples>90&&wrapped_visible&&hidden,"original drag/hide sequence not fully exercised");
    // Same shared mechanism: start frame, fractional slow sampling, pause, and
    // loop boundary must work without room-specific rules.
    // Die Drag-Szene hat die Figur mit bhInitModelSet deaktiviert (stflg 0x1000000);
    // bhControlEnemy ueberspringt solche Works (eneset.c:360). Fuer die folgenden
    // Pruefungen wird sie mit demselben Originalbefehl wieder aktiviert.
    world.bind(command({0x24,1,2,1}));
    world.bind(command({0x65,1,2,0}));world.bind(command({0x69,23,0,0}));
    world.bind(command({0x69,34,3,1,0,1,5,6,65,0}));world.tick();
    const auto a=world.model_pose(1,2).position.x;world.tick();const auto half=world.model_pose(1,2).position.x;
    // A script command writes the work; the object tree follows in the next
    // control pass (bhSetMotion), which samples frm_no before advancing it.
    world.bind(command({0x69,40,66,0}));world.tick();const auto b=world.model_pose(1,2).position.x;
    check(std::abs(half-(a+b)*.5f)<.0001f,"mtn_md bit 8 does not interpolate half-frame root motion");
    // event.c:11971-11997 Common 23 schaltet nur flg 0x118 am Work; es ist keine
    // Bewegungspause (die ist bhMotionPauseSet, unten). Die Motion laeuft weiter.
    world.bind(command({0x69,23,1,0}));world.tick();
    check((world.state(1,2)->enemy.flg&0x118u)==0x118u,"Common 23 (1) does not set work flg 0x118");
    check(world.model_pose(1,2).position.x!=b,"Common 23 (1) froze the work motion like a host pause");
    world.bind(command({0x69,23,0,0}));world.bind(command({0x69,34,1,1,0,1,5,6,84,0}));world.tick();
    check(world.frame(1,2)==0&&world.state(1,2)->pose_frame()==84*65536,"next loop clock and last rendered pose are not independent");
    world.tick();check(world.state(1,2)->pose_frame()==0,"intentional animation loop does not resume frame zero on the next tick");
    // event.c:1791 bhMotionPauseSet schreibt nur mode3 (4 bzw. 1) an Work und
    // bei id 1 an exp1. Fuer diesen EN01-Work (mode0 5, ct2 6 = mv00_subpl2_0)
    // treibt bhEne01_SetMtn (en01.c:2528/10322) bhSetMotion unabhaengig von
    // mode3 weiter: die Uhr laeuft im Original auch "pausiert". Eine echte
    // Pause zeigt der Spielerwork unten.
    {
        const auto& d=*world.state(1,2);
        check(world.bind(command({0x2b,0})),"actor motion pause is unbound");
        const auto* upper=static_cast<const RECVX_ENEMY_WORK*>(d.enemy.exp1);
        check(d.enemy.mode3==4&&upper&&upper->mode3==4,"MotionPauseSet (0) does not write mode3 4 to work and exp1");
        const auto before=world.frame(1,2);world.tick();
        check(world.frame(1,2)!=before,"EN01 bhEne01_SetMtn stopped under mode3 4 (not original)");
        world.bind(command({0x2b,2}));check(d.enemy.mode3==4,"undefined pause mode changed mode3");
        world.bind(command({0x2b,1}));check(d.enemy.mode3==1&&upper->mode3==1,"MotionPauseSet (1) does not write mode3 1 to work and exp1");
    }
    world.bind(command({0x65,0,0,0}));world.bind(command({0x69,34,1,0,0,0,0,6,1,0}));
    world.tick();const auto player_clock=frame;
    world.bind(command({0x2b,0}));for(unsigned i=0;i<7;++i)world.tick();check(frame==player_clock,"MotionPauseSet ignores the player work");
    world.bind(command({0x2b,1}));world.tick();check(frame!=player_clock,"MotionPauseSet does not resume the player");
    {const auto other=read_asset_file(data/"rooms/RM_0070.RDT",64*1024*1024);const auto stage=decode_scene(other);auto spawn=stage.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld actors(other,stage,spawn,m,f,sound);
        BinaryView v(other);const auto h=inspect_room(other).room_offset,table=v.u32(h+8);unsigned enemy=0;
        while(enemy<v.u32(h+0x88)&&v.u16(table+enemy*36+4)!=1)++enemy;
        check(enemy<v.u32(h+0x88),"second room has no original zombie");
        actors.set_simulation_flags(~0u);actors.tick(); // bhEne01_Init (Pass 1) legt exp0/exp1 an; Common 34 setzt sie voraus (event.c:11300)
        actors.bind(command({0x65,1,std::uint8_t(enemy),0}));check(actors.bind(command({0x69,34,1,1,1,1,5,6,20,0})),"Common 34 on the second room's zombie not bound");
        bool loop=false;for(unsigned tick=0;tick<500;++tick){const auto before=actors.frame(1,enemy);actors.tick();check(actors.state(1,enemy)->pose_frame()==before,"second room samples the advanced clock");if(actors.frame(1,enemy)==0){loop=true;break;}}
        check(loop,"second original room motion did not reach its loop boundary");Scene rendered;actors.append(rendered,{},stage.cameras[0]);check(!rendered.triangles.empty(),"last pose in second room is not drawable");
    }
    // bhMotionPosSetEnePly (0x7d, event.c:5844) on the real RM_0050 zombie: yaw is the original signed-16 node sum + 0x8000,
    // Y is rom->grand[v1+1], the player root Y angle is cleared and a missing enemy work keeps the task at the command.
    {auto stage=decode_scene(bytes);auto spawn=stage.spawns.at(0);std::size_t m=0;std::int64_t f=0;AudioMixer s2(data);RoomWorld w(bytes,stage,spawn,m,f,s2,data,{},"RM_0050");
        auto player_asset=PlayerAsset::load(data);w.set_player_asset(&player_asset);
        check(!w.bind(command({0x7d,60,0,0})),"bhMotionPosSetEnePly without enemy work must stay pending");
        w.bind(command({0x65,1,2,0}));w.tick();
        int sum=w.model_pose(1,2,0).angles[1];for(unsigned n=1;n<5;++n)sum-=w.model_pose(1,2,n).angles[1];
        check(w.bind(command({0x7d,2,0,0})),"bhMotionPosSetEnePly must bind");
        check(spawn.yaw==std::int16_t(std::int16_t(sum)+32768),"bhMotionPosSetEnePly yaw must be sum of node Y angles + 0x8000");
        check(w.model_pose(0,0,0).angles[1]==0,"bhMotionPosSetEnePly must clear the player root Y angle");
        check(spawn.position.y==w.floor_heights()[1],"bhMotionPosSetEnePly Y must be rom->grand[v1+1]");}
    // bhLoadWork (event.c:12713): nur Gegner/Objekte lesen model_cno aus Byte 3.
    // Reale Item-Drehung aus RM_4010: 65 03 03 01 waehlt trotzdem objP[0].
    for(const char* name:{"RM_4010","RM_4011","RM_8010"}){
        const auto room=read_asset_file(data/(std::string("rooms/")+name+".RDT"),64*1024*1024);
        const auto stage=decode_scene(room);auto spawn=stage.spawns.at(0);std::size_t m=42;std::int64_t f=0;
        AudioMixer a(data);RoomWorld w(room,stage,spawn,m,f,a,data,{},name);
        const auto base=inspect_room(room).event_offset;
        const unsigned off=std::string(name)=="RM_4010"?0x9ee:std::string(name)=="RM_4011"?0xc88:0x7de;
        auto real_command=[&](unsigned rel,unsigned size){EventCommand c;c.bytes.assign(room.begin()+base+off+rel,room.begin()+base+off+rel+size);c.opcode=c.bytes[0];return c;};
        const auto select=real_command(0,4);check(select.bytes==std::vector<std::uint8_t>({0x65,3,3,1}),"original item selector changed");
        const auto root=w.model_pose(3,3,0),child=w.model_pose(3,3,1);
        check(w.bind(select)&&w.bind(real_command(8,6))&&w.bind(real_command(14,6)),"original item rotation setup unbound");
        for(unsigned i=0;i<20;++i)check(w.bind(real_command(20,3)),"original item rotation unbound");
        check(w.model_pose(3,3,0).angles[0]==root.angles[0]-20*(int(18*(65536.f/360.f))>>1),"bhLoadWork item must rotate node zero, ignoring byte 3");
        check(w.model_pose(3,3,1).angles==child.angles,"bhLoadWork item rotated a child instead of the root");
    }
    {
        // Synthetische Gegenpruefung: Spieler/Effect ignorieren Byte 3, Objekt behaelt es.
        world.bind(command({0x65,0,0,7}));world.bind(command({0x69,12,0,0,0,0}));
        const auto before=world.model_pose(0,0,7).position;
        check(world.bind(command({0x69,8,100,0,200,0,44,1})),"player root write unbound");
        const auto root=world.model_pose(0,0,0).position;
        check(root.x==1&&root.y==2&&root.z==3,"bhLoadWork player must select node zero");
        const auto after=world.model_pose(0,0,7).position;
        check(before.x==after.x&&before.y==after.y&&before.z==after.z,"player selector touched operand node");
        world.bind(command({0x65,4,0,255}));
        world.bind(command({0x69,26,0,0,100,0,200,0,44,1}));
        world.bind(command({0x69,26,1,0,100,0,200,0,44,1}));
        check(world.bind(command({0x69,30,0})),"effect root interpolation must not request a model node");
        const auto* effect=world.effect_state(0);
        check(effect&&effect->px==1&&effect->py==2&&effect->pz==3,"effect selector did not write effect work position");
        world.bind(command({0x65,2,15,1}));
        const auto object_root=world.model_pose(2,15,0).position;
        check(world.bind(command({0x69,8,100,0,200,0,44,1})),"object child write unbound");
        const auto object_child=world.model_pose(2,15,1).position;
        check(object_child.x==1&&object_child.y==2&&object_child.z==3,"object selector must retain byte 3");
        const auto unchanged=world.model_pose(2,15,0).position;
        check(unchanged.x==object_root.x&&unchanged.y==object_root.y&&unchanged.z==object_root.z,"object child write touched root");
    }
    {
        // bhLoadWorkEx (event.c:12808, 0xB5): dieselbe Auswahl, aber in bhEtask[0], nicht im
        // Kontext des laufenden Tasks. Real in RM_4010/4011 als `b5 00 00 01` hinter den
        // Item-Drehschleifen; hier Gegner/Objekt-Faelle, damit der Zielkontext messbar ist.
        auto on_task=[&](unsigned task,std::initializer_list<std::uint8_t> bytes){EventCommand c;c.bytes=bytes;c.opcode=*bytes.begin();c.task=task;return c;};
        // Task 3 waehlt sich das Objekt 15/Knoten 1; ein 0xB5 auf Task 3 darf das nicht aendern,
        // sondern muss Ebene 0 auf den Gegner 2 setzen (model_cno aus Byte 3).
        check(world.bind(on_task(3,{0x65,2,15,1})),"task 3 selector unbound");
        check(world.bind(on_task(3,{0xb5,1,2,1})),"bhLoadWorkEx unbound");
        const auto before_t3=world.model_pose(2,15,1).position;
        check(world.bind(on_task(3,{0x69,8,100,0,100,0,100,0})),"task 3 write unbound");
        const auto t3=world.model_pose(2,15,1).position;
        check(t3.x==1&&t3.y==1&&t3.z==1,"bhLoadWorkEx must not touch the running task's context");
        (void)before_t3;
        // Ebene 0 zeigt jetzt auf Gegner 2, Knoten 1: ein Schreibvorgang auf Task 0 landet dort.
        world.bind(on_task(0,{0x69,12,0,0,0,0}));
        const auto enemy_before=world.model_pose(1,2,1).position;
        check(world.bind(on_task(0,{0x69,8,200,0,200,0,200,0})),"task 0 write unbound");
        const auto enemy_after=world.model_pose(1,2,1).position;
        check(enemy_after.x==2&&enemy_after.y==2&&enemy_after.z==2&&(enemy_before.x!=2||enemy_before.z!=2),"bhLoadWorkEx must select into bhEtask[0]");
        // Item ueber 0xB5: Byte 3 wird wie bei 0x65 ignoriert (event.c:12880).
        check(world.bind(on_task(5,{0xb5,3,0,1})),"bhLoadWorkEx item unbound");
        check(world.bind(on_task(0,{0x69,8,44,1,44,1,44,1})),"task 0 item write unbound");
        const auto item_root=world.model_pose(3,0,0).position;
        check(item_root.x==3&&item_root.y==3&&item_root.z==3,"bhLoadWorkEx item must select node zero");
        // Art > 4 faellt wie im Original durch den switch: nichts gebunden, Kontext unveraendert.
        check(!world.bind(on_task(5,{0xb5,7,0,0})),"bhLoadWorkEx kind 7 must stay unbound");
    }
    std::cout<<"Original script motion, bhLoadWork and bhLoadWorkEx node selection passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
