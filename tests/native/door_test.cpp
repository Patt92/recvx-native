#include "doors.h"
#include "asset_io.h"
#include "binary_view.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
// Positive test of the door demo (door.c semantics): the MPAC parameter block
// sizes, the chained chg_vew validation, and the fade/camera/door procedures
// running every original packet to its own end -- camera travel, door motion,
// sound requests at their original passes and the completion status. No
// original procedure is linked.
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
namespace {
struct Run {
    unsigned ticks=0;
    bool finished=false,drew=false,faded=false,deferred=false;
    float travel=0;
    int node_motion=0,flag_motion=0;
    std::vector<std::pair<unsigned,unsigned>> sounds,commands; // pass, value
    // LightProc1/2: the ambient after the clamp, the point lights that came
    // on, and how far the rate pulled the first light's colour apart.
    bool lit=false;unsigned lights=0;
    std::array<float,3> ambient{};float light_colour=0,light_colour_low=0;
    float light_travel=0;
    // PuruProc1: the system sound numbers and the {attribute, pattern} pairs.
    std::vector<std::pair<unsigned,int>> shake_sounds;
    std::vector<std::pair<unsigned,std::pair<int,int>>> shake_patterns;
};
Run play(DoorSequence& door,unsigned limit=1200){
    Run run;
    bool first_colour=true;
    const auto* nodes=door.state().objP;
    std::vector<int> before,flags_before;
    for(unsigned i=0;i<door.state().nodes;++i){
        flags_before.push_back(int(nodes[i].evalflags));
        for(unsigned k=0;k<3;++k){before.push_back(nodes[i].ang[k]);before.push_back(int(nodes[i].pos[k]*100));}
    }
    while(door.active()&&run.ticks<limit){
        const auto previous=door.state().vew_pos;
        door.tick();++run.ticks;
        const auto now=door.state().vew_pos;
        run.travel+=std::abs(now.x-previous.x)+std::abs(now.y-previous.y)+std::abs(now.z-previous.z);
        if(door.state().status&0x100)run.faded=true;
        if(door.state().status&0x200)run.drew=true;
        if(door.state().status&0x20)run.deferred=true;
        for(const auto sound:door.take_sounds())run.sounds.push_back({run.ticks,sound});
        for(const auto command:door.take_sound_commands())run.commands.push_back({run.ticks,command});
        for(const auto sound:door.take_shake_sounds())run.shake_sounds.push_back({run.ticks,sound});
        for(const auto pattern:door.take_shake_patterns())run.shake_patterns.push_back({run.ticks,pattern});
        const auto& s=door.state();
        if(s.lgt_on){
            run.lit=true;run.ambient={s.lgt_amb[0],s.lgt_amb[1],s.lgt_amb[2]};
            unsigned on=0;for(const auto& light:s.lgt_pnt)if(light.on)++on;
            run.lights=std::max(run.lights,on);
            if(s.lgt_pnt[0].on){
                const float colour=s.lgt_pnt[0].col[0];
                if(first_colour){run.light_colour=run.light_colour_low=colour;first_colour=false;}
                run.light_colour=std::max(run.light_colour,colour);
                run.light_colour_low=std::min(run.light_colour_low,colour);
                run.light_travel+=std::abs(s.lgt_pnt[0].pos.x)+std::abs(s.lgt_pnt[0].pos.y)+std::abs(s.lgt_pnt[0].pos.z);
            }
        }
    }
    run.finished=door.finished();
    std::vector<int> after,flags_after;
    for(unsigned i=0;i<door.state().nodes;++i){
        flags_after.push_back(int(nodes[i].evalflags));
        for(unsigned k=0;k<3;++k){after.push_back(nodes[i].ang[k]);after.push_back(int(nodes[i].pos[k]*100));}
    }
    for(std::size_t i=0;i<after.size();++i)if(after[i]!=before[i])++run.node_motion;
    for(std::size_t i=0;i<flags_after.size();++i)if(flags_after[i]!=flags_before[i])++run.flag_motion;
    return run;
}
bool has_command(const Run& run,unsigned command){
    return std::any_of(run.commands.begin(),run.commands.end(),[&](const auto& e){return e.second==command;});
}
}
int main(int argc,char** argv){try{_Make_SinTable();
    check(DoorRequest{0,0,1,0,0,0}.room_name()=="RM_0010.RDX","door room identity");
    check(DoorRequest{0,0,16,0,0,0}.room_name()=="RM_0160.RDX","two-digit decimal destination");
    check(DoorRequest{0,2,0x1b,0,0,1}.room_name()=="RM_2271.RDX","original decimal room identity");

    // --- MPAC parameter block sizes ---------------------------------------
    {const std::size_t fade[2]={0,0x34};
     const std::size_t view[8]={0,0x50,0x5C,0x94,0x38,0x5C,0x58,0x50};
     const std::size_t door[8]={0,0x50,0x1C,0x40,0x30,0x34,0,0x50};
     // LIGHTPROC_WORK is 0x7C; LightProc2 reads a second block on the pull
     // side. PURUPROC_WORK is 0x14.
     const std::size_t light[3]={0,0x7C,0xF8};
     const std::size_t shake[2]={0,0x14};
     for(unsigned t=0;t<2;++t)check(recvx_door_parameter_size(0,t)==fade[t],"fade block size");
     for(unsigned t=0;t<8;++t)check(recvx_door_parameter_size(1,t)==view[t],"camera block size");
     for(unsigned t=0;t<8;++t)check(recvx_door_parameter_size(2,t)==door[t],"door block size");
     for(unsigned t=0;t<3;++t)check(recvx_door_parameter_size(3,t)==light[t],"light block size");
     for(unsigned t=0;t<2;++t)check(recvx_door_parameter_size(4,t)==shake[t],"shake block size");
     check(!recvx_door_parameter_size(1,8)&&!recvx_door_parameter_size(3,3)&&!recvx_door_parameter_size(4,2)
           &&!recvx_door_parameter_size(5,1),"size reported outside the tables");
     // A light block is only as long as the lights it declares; the second
     // block of LightProc2 still sits a whole 0x7C behind the first.
     std::vector<unsigned char> block(0x7C+0x7C,0);
     *reinterpret_cast<int*>(block.data()+12)=2;
     check(recvx_door_light_extent(block.data(),block.size(),1)==0x10+2*0x24,"a two-light block was not measured");
     *reinterpret_cast<int*>(block.data()+12)=0;
     check(recvx_door_light_extent(block.data(),block.size(),1)==0x10,"an empty light block was not measured");
     check(recvx_door_light_extent(block.data(),0x0F,1)==0,"a light block shorter than its header was accepted");
     *reinterpret_cast<int*>(block.data()+12)=1;
     check(recvx_door_light_extent(block.data(),0x10+0x24-1,1)==0,"a light block cut off inside its light was accepted");
     *reinterpret_cast<int*>(block.data()+12)=4;
     check(recvx_door_light_extent(block.data(),block.size(),1)==0,"a block claiming four lights was accepted");
     *reinterpret_cast<int*>(block.data()+12)=1;
     *reinterpret_cast<int*>(block.data()+0x7C+12)=3;
     check(recvx_door_light_extent(block.data(),block.size(),2)==0x7C+0x10+3*0x24,"the second light block was not measured");
     check(recvx_door_light_extent(block.data(),0x7C+0x10,2)==0,"a cut-off second light block was accepted");
     check(!recvx_door_light_extent(block.data(),block.size(),0)&&!recvx_door_light_extent(block.data(),block.size(),3),
           "a light block outside the two procedures was measured");}

    // --- njClipZ of bhSetDoor ----------------------------------------------
    {float near_clip=0,far_clip=0;
     // dmo_clip holds one case: door type 1 out of the MRY partition whose
     // case runs ViewProc4.
     check(recvx_door_clip(1,RECVX_DOOR_MRY_PARTITION,4,&near_clip,&far_clip),"the dmo_clip case was not found");
     check(near_clip==10.0f&&far_clip==34.0f,"the dmo_clip case did not report its own range");
     check(!recvx_door_clip(1,RECVX_DOOR_MRY_PARTITION,3,&near_clip,&far_clip),"another camera type matched the dmo_clip case");
     check(near_clip==2.0f&&far_clip==20000.0f,"the gameplay default range is not reported");
     check(!recvx_door_clip(2,RECVX_DOOR_MRY_PARTITION,4,&near_clip,&far_clip),"another door type matched the dmo_clip case");
     check(!recvx_door_clip(1,4,4,&near_clip,&far_clip),"another partition matched the dmo_clip case");}

    // --- the chg_vew chain -------------------------------------------------
    {std::vector<unsigned char> block(0x38+0x50,0);
     // A type 4 block that hands over to type 1 must have room for both.
     *reinterpret_cast<int*>(block.data()+0x34)=1;
     check(recvx_door_validate_view(4,block.data(),block.size(),0),"a complete chg_vew chain was rejected");
     check(!recvx_door_validate_view(4,block.data(),0x38+0x10,0),"a chain whose follow-up does not fit was accepted");
     *reinterpret_cast<int*>(block.data()+0x34)=0;
     check(!recvx_door_validate_view(4,block.data(),block.size(),0),"chg_vew 0 has no procedure and must be rejected");
     *reinterpret_cast<int*>(block.data()+0x34)=8;
     check(!recvx_door_validate_view(4,block.data(),block.size(),0),"chg_vew outside 1..7 was accepted");
     check(!recvx_door_validate_view(0,block.data(),block.size(),0),"camera type 0 has no procedure");
     check(!recvx_door_validate_view(8,block.data(),block.size(),0),"camera type outside 0..7 accepted");
     // A ring of type 4 blocks must not be walked forever.
     std::vector<unsigned char> ring(0x38*32,0);
     for(std::size_t p=0;p+0x38<=ring.size();p+=0x38)*reinterpret_cast<int*>(ring.data()+p+0x34)=4;
     check(!recvx_door_validate_view(4,ring.data(),ring.size(),0),"an endless chg_vew chain was accepted");}

    // --- procedure binding --------------------------------------------------
    {RECVX_DOOR_STATE state{};
     check(recvx_door_configure(&state,1,1,1,1,1),"the original fade/camera/door types did not bind");
     check(state.fde_prcP&&state.vew_prcP&&state.dor_prcP&&state.lgt_prcP&&state.pru_prcP,"a procedure stayed unbound");
     check(!recvx_door_configure(&state,0,1,1,1,1),"fade type 0 has no procedure and must not report success");
     // Type 6 only raises an evalflag, so it has a procedure but no block.
     check(recvx_door_configure(&state,1,1,6,1,1)&&state.dor_prcP,"door type 6 did not bind its procedure");
     check(!recvx_door_parameter_size(2,6),"door type 6 reported a parameter block");
     check(recvx_door_configure(&state,1,1,0,1,1)&&!state.dor_prcP,"door type 0 bound a procedure");
     check(!recvx_door_configure(&state,1,8,1,1,1),"camera type outside the table reported success");
     // Both light procedures and the one shake procedure have to resolve, and
     // a packet may leave either family at zero.
     check(recvx_door_configure(&state,1,1,1,2,0)&&state.lgt_prcP&&!state.pru_prcP,"light type 2 without a shake did not bind");
     check(recvx_door_configure(&state,1,1,1,0,1)&&!state.lgt_prcP&&state.pru_prcP,"a shake without a light procedure did not bind");
     check(!recvx_door_configure(&state,1,1,1,3,0),"light type outside the table reported success");
     check(!recvx_door_configure(&state,1,1,1,0,2),"shake type outside the table reported success");
     // dmo_atr 0x40 swaps the packet's shake for the built-in heartbeat one.
     {RECVX_DOOR_STATE doki{};
      check(recvx_door_configure(&doki,1,1,1,1,0),"the heartbeat case did not configure");
      check(!doki.pru_prcP,"the packet already carried a shake procedure");
      recvx_door_select_doki_shake(&doki);
      check(doki.pru_prcP&&doki.pru_type==1,"the heartbeat shake was not selected");}}

    if(argc<2){std::cout<<"No cooked data supplied: the original packets were not played.\n";return 0;}
    const auto root=std::filesystem::u8path(argv[1]);

    // --- every original packet runs its own sequence -----------------------
    unsigned packets=0,with_travel=0,with_motion=0,with_sound=0,with_procedure=0,with_light=0,with_shake=0;
    unsigned shortest=~0u,longest=0;
    for(unsigned type=0;type<105;++type){
        const auto file=root/"archives/MRY"/(asset_number(type+71)+".bin");
        if(!std::filesystem::is_regular_file(file))continue;
        const auto bytes=read_asset_file(file,8*1024*1024);
        if(bytes.size()<32||std::memcmp(bytes.data(),"MPAC",4))continue;
        BinaryView view(bytes);std::size_t p=4+view.u32(4),parameter=0;bool model=false;
        for(unsigned i=0;i<4;++i){const auto size=view.u32(p);if(i==0)model=bytes.at(p+4)=='M'&&bytes.at(p+5)=='D'&&bytes.at(p+6)=='L';if(i==3)parameter=p+4;p+=4+size;}
        if(!model)continue;
        for(unsigned c=0;c<view.u8(parameter+3);++c){
            // PT_WORK slots 2, 3 and 4 name the door, light and shake
            // procedures of this case, each followed by its block offset.
            const unsigned door_type=view.u16(parameter+4+c*20+8);
            const unsigned light_type=view.u16(parameter+4+c*20+12),light_off=view.u16(parameter+4+c*20+14);
            const unsigned shake_type=view.u16(parameter+4+c*20+16),shake_off=view.u16(parameter+4+c*20+18);
            DoorSequence door;door.begin(root,{},DoorRequest{c<<7,0,1,0,type,0});
            check(door.active(),"an original door packet did not start a sequence");
            const auto run=play(door);
            const std::string what="door type "+std::to_string(type)+" case "+std::to_string(c);
            check(run.finished,(what+" never reached the end of its sequence").c_str());
            check(run.ticks>30,(what+" finished in its first passes instead of running the demo").c_str());
            check(door.state().status&0x40,(what+" finished without the completion status").c_str());
            check(!(door.state().status&0x300),(what+" left the fade or the door model on screen").c_str());
            check(run.drew,(what+" never raised the draw status").c_str());
            check(run.faded,(what+" never ran a fade").c_str());
            check(run.deferred,(what+" never released the deferred sound group").c_str());
            // bhControlDoor requests sound command 8 from the start flag and 9
            // once FadeProc1 has reached its last section.
            check(!run.commands.empty()&&run.commands.front().second==8,(what+" did not open with sound command 8").c_str());
            check(run.commands.back().second==9,(what+" did not close with sound command 9").c_str());
            // Command 9 rides on the same status bit that releases the
            // deferred sounds, so it can never precede the opening command.
            check(has_command(run,9)&&run.commands.back().first>run.commands.front().first,
                  (what+" released command 9 before the demo had started").c_str());
            // Type 6 only hides a leaf, type 0 has no door procedure at all;
            // every other procedure has to move its own nodes.
            if(door_type==6)check(run.flag_motion>0,(what+" did not raise the leaf evalflag").c_str());
            else if(door_type)check(run.node_motion>0,(what+" moved no door node").c_str());
            // LightProc1/2: the ambient of the block capped at 0.4, and every
            // light the block declares switched on with its own colour. The
            // rate bhInitDoor leaves behind is one, so the colour is the
            // packet's own value.
            if(light_type){
                check(run.lit,(what+" never set its multi ambient").c_str());
                const unsigned lights=view.u32(parameter+light_off+12);
                check(run.lights==lights,(what+" switched on a different number of point lights").c_str());
                for(unsigned k=0;k<3;++k)
                    check(run.ambient[k]==std::min(view.f32(parameter+light_off+k*4),0.4f),
                          (what+" put in an ambient the block does not hold").c_str());
                if(lights)check(run.light_colour==view.f32(parameter+light_off+16+4)&&run.light_colour_low==run.light_colour,
                                (what+" scaled its first light away from the packet colour").c_str());
                ++with_light;
            }
            else check(!run.lit,(what+" lit itself without a light procedure").c_str());
            // PuruProc1: one system sound and two vibration patterns per
            // trigger, the first after wait_time passes and then every
            // pru_wait+1 passes while loop_time lasts.
            if(shake_type){
                const int wait_time=view.i32(parameter+shake_off),loop_time=view.i32(parameter+shake_off+4);
                const int pattern=view.i32(parameter+shake_off+12),pru_wait=view.i32(parameter+shake_off+16);
                check(!run.shake_sounds.empty(),(what+" requested no shake sound").c_str());
                check(run.shake_patterns.size()==2*run.shake_sounds.size(),
                      (what+" did not raise two vibration patterns per trigger").c_str());
                check(run.shake_sounds.front().first==unsigned(wait_time)+1,
                      (what+" started its shake outside its own wait time").c_str());
                for(const auto& entry:run.shake_sounds)
                    check(entry.second==10,(what+" used a sound outside PruSndTbl").c_str());
                for(const auto& entry:run.shake_patterns)
                    check(entry.second.first==2&&entry.second.second==pattern,
                          (what+" used another vibration attribute or pattern").c_str());
                for(std::size_t i=1;i<run.shake_sounds.size();++i)
                    check(run.shake_sounds[i].first-run.shake_sounds[i-1].first==unsigned(pru_wait)+1,
                          (what+" did not keep its own shake interval").c_str());
                check(int(run.shake_sounds.back().first)<=wait_time+loop_time,
                      (what+" shook after its loop time had run out").c_str());
                ++with_shake;
            }
            else check(run.shake_sounds.empty()&&run.shake_patterns.empty(),
                       (what+" shook without a shake procedure").c_str());
            ++packets;
            if(run.travel>0.5f)++with_travel;
            if(run.node_motion||run.flag_motion)++with_motion;
            if(!run.sounds.empty())++with_sound;
            if(door_type)++with_procedure;
            shortest=std::min(shortest,run.ticks);longest=std::max(longest,run.ticks);
        }
    }
    check(packets>=100,"fewer original door packets than expected were played");
    check(with_travel==packets,"a packet moved its camera nowhere");
    check(with_motion>=with_procedure,"a packet with a door procedure changed nothing on its model");
    check(with_sound==packets,"a packet requested no door sound");
    // Every original case selects a light procedure; the shake is rarer.
    check(with_light==packets,"a packet drew without the light procedure its own case selects");
    check(with_shake>0,"no original packet ran its shake procedure");
    // The lengths come from the packets themselves; they are reported, not
    // asserted against an invented range.
    check(shortest>30,"a packet finished before it could run a demo");

    // --- one packet against its own original passes ------------------------
    {DoorSequence door;door.begin(root,{},DoorRequest{0,0,1,0,0,0});
     const auto run=play(door);
     check(run.ticks==165,("door type 0 ran "+std::to_string(run.ticks)+" passes instead of 165").c_str());
     const std::vector<std::pair<unsigned,unsigned>> sounds{{41,0},{76,2},{164,1}};
     check(run.sounds==sounds,"door type 0 did not request its three sounds at their own passes");
     const std::vector<std::pair<unsigned,unsigned>> commands{{1,8},{72,5},{149,6},{164,9}};
     check(run.commands==commands,"door type 0 did not send its sound commands at their own passes");
     // The closing sound belongs to the deferred group and must not arrive
     // before FadeProc1 released it.
     check(run.sounds.back().first+1==run.ticks,"the closing sound did not land on the last pass before the end");}

    // --- attribute variants pick other parameters --------------------------
    {DoorSequence push,pull;
     push.begin(root,{},DoorRequest{0,0,1,0,0,0});
     pull.begin(root,{},DoorRequest{1,0,1,0,0,0}); // attr 1: status 0x4000, the pull side
     check(pull.state().status&0x4000,"the pull attribute did not reach the status");
     check(!(push.state().status&0x4000),"the push side still carries the pull status");
     // Which packets actually hold two camera sides is data; at least one of
     // the original ones has to start the two sides apart.
     unsigned sided=0;
     for(unsigned type=0;type<105;++type){
         const auto file=root/"archives/MRY"/(asset_number(type+71)+".bin");
         if(!std::filesystem::is_regular_file(file))continue;
         const auto bytes=read_asset_file(file,8*1024*1024);
         if(bytes.size()<32||std::memcmp(bytes.data(),"MPAC",4))continue;
         DoorSequence a,b;
         try{a.begin(root,{},DoorRequest{0,0,1,0,type,0});b.begin(root,{},DoorRequest{1,0,1,0,type,0});}catch(const std::exception&){continue;}
         a.tick();b.tick();
         const auto p=a.state().vew_pos,q=b.state().vew_pos;
         if(p.x!=q.x||p.y!=q.y||p.z!=q.z)++sided;
     }
     check(sided>0,"no original packet starts its pull side anywhere else");
     DoorSequence mirrored;mirrored.begin(root,{},DoorRequest{2,0,1,0,0,0}); // attr 2 mirrors the demo
     check((mirrored.state().status&0xc00)!=(push.state().status&0xc00),"the mirror attribute did not flip the door status");}

    // --- the demo ends through FadeProc1 alone ------------------------------
    // bhSysCallDoordemo (system.c:939) has no pad or flag that cuts the demo
    // short; bhControlDoor keeps returning 1 until FadeProc1 mode 5 sets
    // status 0x40 (door.c:641). The sequence has no abort service, and the
    // completion bit must not appear before the fade's last wait has run out.
    unsigned full_run_ticks=0;
    {DoorSequence door;door.begin(root,{},DoorRequest{0,0,1,0,0,0});
     unsigned ticks=0;bool early=false;
     while(door.active()){door.tick();++ticks;if(door.state().status&0x40){if(door.state().fde_mode<6)early=true;}}
     check(!early,"the completion status appeared before FadeProc1 reached its end");
     check(door.finished()&&door.state().fde_mode==6&&ticks>100,"the demo did not end through FadeProc1");
     check(!door.skipped(),"the undisturbed demo reported a skip");
     full_run_ticks=ticks;}

    // --- the host skip ends the demo early ---------------------------------
    // BEWUSSTE ABWEICHUNG VOM ORIGINAL (doors.h, Nutzerentscheidung 2026-10-13):
    // skip() ist eine Host-Komfortfunktion. Geprueft wird, dass sie genau den
    // regulaeren Abschluss erzeugt -- Statusbit 0x40 wie von FadeProc1, kein
    // aktiver Lauf mehr -- und dabei deutlich vor dem eigenen Ende steht, denn
    // nur dann kann ein Tuerton noch klingen und muss gestoppt werden.
    {DoorSequence door;door.begin(root,{},DoorRequest{0,0,1,0,0,0});
     for(unsigned i=0;i<20;++i)door.tick();
     check(door.active()&&!door.finished()&&!door.skipped(),"the demo ended before it was skipped");
     door.skip();
     check(door.skipped()&&door.finished()&&!door.active(),"the skip did not end the demo");
     check(door.state().status&0x40,"the skip left the completion status clear");
     check(door.ticks()<full_run_ticks,"the skip did not shorten the sequence");
     // A second request starts a clean sequence: the skip must not stick.
     door.begin(root,{},DoorRequest{0,0,1,0,0,0});
     check(door.active()&&!door.skipped()&&!door.finished(),"the skip survived into the next door");
     // The direct transition (type 255) finishes without ever being skipped.
     DoorSequence straight;straight.begin(root,{},DoorRequest{0x80000000u,0,1,0,0,0});
     check(straight.finished()&&!straight.active()&&!straight.skipped(),"a direct transition reported a skip");}

    // --- the heartbeat light and the heartbeat shake ------------------------
    // dmo_atr 0x20 switches DokiProcLgt on: the rectified sine over time*1024
    // pulls the point-light rate between 0.5 and 1.5, and the ambient offset
    // of a half lifts the clamped ambient to 0.4.
    {DoorSequence plain,doki;
     plain.begin(root,{},DoorRequest{0,0,1,0,0,0});
     doki.begin(root,{},DoorRequest{0x20,0,1,0,0,0});
     check(doki.state().status&0x40000000,"the heartbeat attribute did not reach the status");
     const auto a=play(plain),b=play(doki);
     check(a.light_colour==a.light_colour_low,"the steady demo pulsed its light anyway");
     check(b.light_colour>b.light_colour_low,"the heartbeat light never pulsed the point light");
     // The packet colour is 0.5 and the rate runs 0.5..1.5 around it.
     check(b.light_colour>a.light_colour&&b.light_colour_low<a.light_colour_low,
           "the heartbeat rate did not reach past the steady colour");
     check(a.ambient[0]==0.0f&&b.ambient[0]==0.4f,"the heartbeat ambient offset did not reach the clamp");
     check(b.ticks==a.ticks,"the heartbeat light changed the length of the sequence");}
    // dmo_atr 0x40 replaces the packet shake by DkiPru: pattern 13 every 20+1
    // passes from the first one, for 768 passes.
    {DoorSequence door;door.begin(root,{},DoorRequest{0x40,0,1,0,0,0});
     check(door.state().status&0x20000000,"the heartbeat shake attribute did not reach the status");
     check(door.state().status&0x10000000,"the shake pass was not switched on");
     const auto run=play(door);
     check(!run.shake_sounds.empty(),"the heartbeat shake requested no sound");
     check(run.shake_sounds.front().first==1,"the heartbeat shake did not start on the first pass");
     for(const auto& entry:run.shake_sounds)check(entry.second==10,"the heartbeat shake used another sound");
     for(const auto& entry:run.shake_patterns)
         check(entry.second.first==2&&entry.second.second==13,"the heartbeat shake used another pattern");
     for(std::size_t i=1;i<run.shake_sounds.size();++i)
         check(run.shake_sounds[i].first-run.shake_sounds[i-1].first==21,"the heartbeat shake lost its interval");
     check(run.shake_patterns.size()==2*run.shake_sounds.size(),"the heartbeat shake lost a vibration pattern");
     // Door type 0 has no shake of its own, so this one can only be DkiPru.
     DoorSequence plain;plain.begin(root,{},DoorRequest{0,0,1,0,0,0});
     check(play(plain).shake_sounds.empty(),"door type 0 shook without the heartbeat attribute");}

    // --- njClipZ reaches the drawn demo ------------------------------------
    // Door type 1 case 1 is the one original case that runs ViewProc4, so it
    // is the dmo_clip entry; its case 0 runs ViewProc3 and stays at default.
    {DoorSequence narrow,wide;
     narrow.begin(root,{},DoorRequest{0x80,0,1,0,1,0});
     wide.begin(root,{},DoorRequest{0,0,1,0,1,0});
     check(narrow.clip_case(),"the original dmo_clip case did not match its own packet");
     check(narrow.clip_near()==10.0f&&narrow.clip_far()==34.0f,"the dmo_clip case did not narrow the demo");
     check(!wide.clip_case()&&wide.clip_near()==2.0f&&wide.clip_far()==20000.0f,
           "the other case of the same packet was narrowed as well");
     DoorSequence other;other.begin(root,{},DoorRequest{0,0,1,0,0,0});
     check(!other.clip_case(),"another door type matched the dmo_clip case");}

    // --- stair demo: mesh, original projection and image edge, not Kaidan ---
    // RM_0010 -> RM_0020 selects MRY/0072, case 1 (attribute 0x80).
    // The first tread is present in the original CNK stream but lies below
    // the 640x480 image at the initial ViewProc4 pose. Do not "repair" that
    // with a room override or an invented near/far range. The far-plane
    // packet reject hides UPPER steps, independently of this image edge.
    // Original: door.c:191-240/454-489/877-910/991-1075;
    // system.c:429-437; ps2_NaView.c:55-65; ps2_NinjaCnk.c:2431-2536.
    {const auto bytes=read_asset_file(root/"archives/MRY/0072.bin",8*1024*1024);
     BinaryView packet(bytes);std::size_t p=4+packet.u32(4),model=0;
     for(unsigned section=0;section<4;++section){if(!section)model=p+4;p+=4+packet.u32(p);}
     const auto base=model+packet.u16(model+4),objects=base+packet.u32(model+12);
     const auto stair_model=base+packet.u32(objects+52+4),vertices=base+packet.u32(stair_model);
     check(packet.u8(vertices)==41&&packet.u16(vertices+4)==0&&packet.u16(vertices+6)==410,
           "stair demo no longer has its original 410-vertex CNK source");
     auto mesh=decode_model_asset(bytes,model,0);
     check(mesh.object_count==2&&mesh.triangles.size()==328,"stair demo lost original nodes or CNK triangles");
     // Both original objects have identity transforms; obj0 is a modelless
     // root (evalflag 8), obj1 the mesh (evalflag 16). Verify rather than
     // silently projecting raw model vertices as if all assets were identity.
     for(unsigned node=0;node<2;++node){const auto o=objects+node*52;
         for(unsigned axis=0;axis<3;++axis)
             check(packet.f32(o+8+axis*4)==0&&packet.i32(o+20+axis*4)==0&&packet.f32(o+32+axis*4)==1,
                   "stair model object transform is not the original identity");
     }
     check(packet.u32(objects)==8&&packet.u32(objects+4)==0xffffffffu&&packet.u32(objects+52)==16,
           "stair root/mesh object flags changed");
     // Independently read original strip indices and winding: every decoded
     // triangle must carry exactly those original positions, including all
     // lower steps. Count equality alone would miss substituted geometry.
     std::size_t polygon=base+packet.u32(stair_model+4),decoded=0;
     while(packet.u8(polygon)!=255){const unsigned type=packet.u8(polygon);
         if(type<8){polygon+=2;continue;}if(type<16){polygon+=4;continue;}
         const auto end=polygon+(packet.u16(polygon+2)+2)*2;
         if(type>=64){check(type==66,"stair polygon no longer uses original Uvn strips");
             const auto header=packet.u16(polygon+4);std::size_t at=polygon+6;
             for(unsigned strip=0;strip<(header&0x3fff);++strip){const auto bits=packet.u16(at);at+=2;
                 const bool reverse=bits&0x8000;const unsigned count=reverse?0x10000u-bits:bits;
                 std::vector<unsigned> indices;
                 for(unsigned v=0;v<count;++v){indices.push_back(packet.u16(at));at+=6;
                     if(v<2)continue;at+=2*(header>>14);
                     std::array<unsigned,3> index{indices[v-2],indices[v-1],indices[v]};
                     if(bool(v&1)^reverse)std::swap(index[0],index[1]);
                     check(decoded<mesh.triangles.size(),"original stair stream has more triangles than native mesh");
                     const auto& triangle=mesh.triangles[decoded++];check(triangle.object==1,"stair triangle belongs to another object");
                     for(unsigned corner=0;corner<3;++corner){check(index[corner]<410,"stair source index outside original vertex block");
                         const auto source=vertices+8+index[corner]*24;const auto& position=triangle.vertices[corner].position;
                         check(position.x==packet.f32(source)&&position.y==packet.f32(source+4)&&position.z==packet.f32(source+8),
                               "native stair triangle differs from the original CNK indices or object transform");
                     }
                 }
             }
         }
         polygon=end;
     }
     check(decoded==mesh.triangles.size(),"native stair mesh has triangles absent from the original stream");
     // 41 original strips, each ten vertices / eight triangles, not twenty
     // procedural host steps. This also catches loss of the lower mesh half.
     unsigned segments=0,segment_triangles=0;std::uint32_t previous=0;
     Scene first_tread;first_tread.background=0xff000000;
     for(auto triangle:mesh.triangles){
         check(triangle.strip_clip&&triangle.strip_segment,"stair CNK bypasses the original strip clip rule");
         if(triangle.strip_segment!=previous){
             if(previous)check(segment_triangles==8,"stair strip did not emit all ten original vertices");
             previous=triangle.strip_segment;segment_triangles=0;++segments;
         }
         ++segment_triangles;
         if(std::all_of(triangle.vertices.begin(),triangle.vertices.end(),[](const SceneVertex& v){
             return v.position.y==1.5f&&v.position.z>=-3.f&&v.position.z<=0.f;})){
             triangle.textured=false;triangle.color=0xffffffff;triangle.world_lit=true;
             first_tread.triangles.push_back(triangle);
         }
     }
     check(segments==41&&segment_triangles==8,"stair demo lost original strip boundaries");
     check(first_tread.triangles.size()==8,"the first original stair tread is absent from the native mesh");
     // njSetPerspective halves the INTEGER angle (ELF 0x002E2A80: sra a0,1),
     // then uses w/2/tanf. dist is screen projection distance, NOT a dolly.
     NJS_SCREEN screen{500,640,480,320,240};njSetScreen(&screen);njSetPerspective(12743);
     const float projection=_nj_screen_.dist;
     check(std::abs(projection-457.05916f)<.001f,"original door projection distance or angle units changed");
     DoorSequence stairs;stairs.begin(root,{},DoorRequest{0x80,0,2,0,1,0});
     auto camera=[&](){SceneCamera c;c.position=stairs.state().vew_pos;
         for(unsigned axis=0;axis<3;++axis)c.angles[axis]=-stairs.state().vew_ang[axis];
         c.perspective=12743;c.clip_near=stairs.clip_near();c.clip_far=stairs.clip_far();return c;};
     const auto project_tread=[&](const SceneCamera& c){
         NJS_MATRIX view,original;camera_view_matrix(c,view);
         // Actual bhControlDoor draw call, independent of camera_view_matrix.
         njUnitMatrix(&original);njRotateX(&original,32768);
         njRotateZ(&original,-stairs.state().vew_ang[2]);
         njRotateX(&original,-stairs.state().vew_ang[0]);
         njRotateY(&original,-stairs.state().vew_ang[1]);
         njTranslate(&original,-c.position.x,-c.position.y,-c.position.z);
         std::array<float,4> bounds{INFINITY,-INFINITY,INFINITY,-INFINITY}; // y min/max, z min/max
         for(const auto& t:first_tread.triangles)for(const auto& v:t.vertices){
             NJS_POINT3 source=v.position,a,b;njCalcPoint(&view,&source,&a);njCalcPoint(&original,&source,&b);
             check(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)<.0001f,"door model/camera transform differs from original call order");
             const float y=240.f+projection*a.y/a.z;
             bounds[0]=std::min(bounds[0],y);bounds[1]=std::max(bounds[1],y);
             bounds[2]=std::min(bounds[2],a.z);bounds[3]=std::max(bounds[3],a.z);
         }
         return bounds;
     };
     SoftwareRenderer image(640,480);image.set_supersampling(1);image.set_render_threads(1);
     stairs.tick();stairs.tick();const auto early=camera();const auto early_bounds=project_tread(early);
     check(early.position.x==0&&early.position.y==12&&early.position.z==16&&early.angles[0]==0,
           "ViewProc4 no longer starts at the original packet pose");
     check(early_bounds[2]>=10&&early_bounds[3]<=34,"first tread is outside z clip, not merely below the image");
     check(early_bounds[0]>480&&std::abs(early_bounds[0]-492.5853f)<.01f&&std::abs(early_bounds[1]-539.9451f)<.01f,
           "initial first tread does not project below the original screen edge");
     image.draw(first_tread,early);check(image.covered_pixels()==0,"off-image first tread leaked onto the initial door frame");
     // 198 is an original procedure pass, not a new animation time limit:
     // View4 waits 24, View6 waits 96 for angles and interpolates its target.
     while(stairs.ticks()<198)stairs.tick();const auto later=camera();const auto later_bounds=project_tread(later);
     check(std::abs(later.position.z-10.7139f)<.001f&&later.angles[0]==4005,"ViewProc6 stair camera progression changed");
     check(later_bounds[0]>=0&&later_bounds[1]<480&&later_bounds[2]>=10&&later_bounds[3]<=34,
           "original tilt no longer brings the first tread into the image independently of near/far");
     image.draw(first_tread,later);check(image.covered_pixels()>0,"present first stair tread is not drawn when the original camera sees it");
     const auto tread_mask=image.pixels();
     for(unsigned node=0;node<2;++node){const auto& n=stairs.state().objP[node];const auto o=objects+node*52;
         check(n.evalflags==packet.u32(o)&&n.model==packet.u32(o+4),"door sequence changed the static stair mesh binding");
         for(unsigned axis=0;axis<3;++axis)check(n.pos[axis]==0&&n.ang[axis]==0,"door sequence moved a static stair object");
     }
     stairs.draw(image);check(image.covered_pixels()>0&&image.last_clip()[0]==10&&image.last_clip()[1]==34,
           "productive stair demo lost its geometry or original clip range");
     std::size_t lit_tread=0;
     for(std::size_t pixel=0;pixel<tread_mask.size();++pixel)
         if(tread_mask[pixel]!=0xff000000u&&(image.pixels()[pixel]&0xffffffu))++lit_tread;
     check(lit_tread>0,"DoorSequence::draw leaves the visible first-tread region blank");
     std::cout<<"Stair demo: 410 source vertices / 41 strips / 328 triangles; first tread below image at pass 2 (y "
              <<early_bounds[0]<<".."<<early_bounds[1]<<"), visible at pass 198 (y "<<later_bounds[0]<<".."<<later_bounds[1]<<").\n";
    }

    // --- the sequence is actually drawn, and the light reaches the image ---
    {DoorSequence door;door.begin(root,{},DoorRequest{0,0,1,0,0,0});
     for(unsigned i=0;i<60;++i)door.tick();
     SoftwareRenderer image(160,120);
     std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);
     door.draw(image);
     check(std::any_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p!=0xff123456u;}),
           "the door demo drew nothing");
     // The same pass under the heartbeat light has to come out differently:
     // its rate and ambient are not the steady ones.
     DoorSequence lit;lit.begin(root,{},DoorRequest{0x20,0,1,0,0,0});
     for(unsigned i=0;i<60;++i)lit.tick();
     SoftwareRenderer other(160,120);
     std::fill(other.canvas().begin(),other.canvas().end(),0xff123456u);
     lit.draw(other);
     check(image.canvas()!=other.canvas(),"the heartbeat light did not change the drawn demo");}

    // --- the dmo_clip range is effective in the native drawing path ---------
    // A quad at z 60 lies inside the gameplay range and outside the narrowed
    // one, so the range the packet hands over decides whether it is drawn.
    {DoorSequence narrow,wide;
     narrow.begin(root,{},DoorRequest{0x80,0,1,0,1,0});
     wide.begin(root,{},DoorRequest{0,0,1,0,1,0});
     // The renderer flips z into the Ninja view, so a point in front of the
     // camera has negative world z.
     Scene probe;SceneTriangle t{};probe.background=0xff000000;
     t.vertices[0].position={-20,-20,-60};t.vertices[1].position={20,-20,-60};t.vertices[2].position={0,20,-60};
     t.color=0xffffffff;probe.triangles.push_back(t);
     auto shot=[&](const DoorSequence& door){
         SceneCamera camera;camera.clip_near=door.clip_near();camera.clip_far=door.clip_far();
         SoftwareRenderer image(160,120);image.draw(probe,camera);
         return std::count_if(image.canvas().begin(),image.canvas().end(),
                              [](std::uint32_t p){return p!=0xff000000u;});
     };
     check(shot(wide)>0,"the gameplay range clipped a quad inside it away");
     check(shot(narrow)==0,"the narrowed dmo_clip range did not clip a quad behind it");}

    std::cout<<packets<<" original door packets play their own sequence ("<<shortest<<".."<<longest
             <<" passes): camera, door motion, light, sounds and completion verified; "
             <<with_light<<" ran a light procedure, "<<with_shake<<" a shake procedure.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"Door test failed: "<<e.what()<<'\n';return 1;}}
