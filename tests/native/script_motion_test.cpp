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
    std::cout<<"Original RM_0050 trigger/event 12, frame 60/64 offsets, final pose before hide, fractional sampling, pause and loop passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
