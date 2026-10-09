#include "doors.h"
#include "asset_io.h"
#include "binary_view.h"
#include <algorithm>
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
};
Run play(DoorSequence& door,unsigned limit=1200){
    Run run;
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
     for(unsigned t=0;t<2;++t)check(recvx_door_parameter_size(0,t)==fade[t],"fade block size");
     for(unsigned t=0;t<8;++t)check(recvx_door_parameter_size(1,t)==view[t],"camera block size");
     for(unsigned t=0;t<8;++t)check(recvx_door_parameter_size(2,t)==door[t],"door block size");
     check(!recvx_door_parameter_size(1,8)&&!recvx_door_parameter_size(3,1),"size reported outside the tables");}

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
     check(recvx_door_configure(&state,1,1,1),"the original fade/camera/door types did not bind");
     check(state.fde_prcP&&state.vew_prcP&&state.dor_prcP,"a procedure stayed unbound");
     check(!recvx_door_configure(&state,0,1,1),"fade type 0 has no procedure and must not report success");
     // Type 6 only raises an evalflag, so it has a procedure but no block.
     check(recvx_door_configure(&state,1,1,6)&&state.dor_prcP,"door type 6 did not bind its procedure");
     check(!recvx_door_parameter_size(2,6),"door type 6 reported a parameter block");
     check(recvx_door_configure(&state,1,1,0)&&!state.dor_prcP,"door type 0 bound a procedure");
     check(!recvx_door_configure(&state,1,8,1),"camera type outside the table reported success");}

    if(argc<2){std::cout<<"No cooked data supplied: the original packets were not played.\n";return 0;}
    const auto root=std::filesystem::u8path(argv[1]);

    // --- every original packet runs its own sequence -----------------------
    unsigned packets=0,with_travel=0,with_motion=0,with_sound=0,with_procedure=0;
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
            // PT_WORK slot 2 names the door procedure of this case.
            const unsigned door_type=view.u16(parameter+4+c*20+8);
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

    // --- skip ends the demo at once ----------------------------------------
    {DoorSequence door;door.begin(root,{},DoorRequest{0,0,1,0,0,0});
     for(unsigned i=0;i<20;++i)door.tick();
     check(door.active()&&!door.finished(),"the demo ended before it was skipped");
     door.skip();
     check(door.skipped()&&door.finished()&&!door.active(),"the skip did not end the demo");
     check(door.state().status&0x40,"the skip left the completion status clear");}

    // --- the sequence is actually drawn ------------------------------------
    {DoorSequence door;door.begin(root,{},DoorRequest{0,0,1,0,0,0});
     for(unsigned i=0;i<60;++i)door.tick();
     SoftwareRenderer image(160,120);
     std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);
     door.draw(image);
     check(std::any_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p!=0xff123456u;}),
           "the door demo drew nothing");}

    std::cout<<packets<<" original door packets play their own sequence ("<<shortest<<".."<<longest
             <<" passes): camera, door motion, sounds and completion verified.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"Door test failed: "<<e.what()<<'\n';return 1;}}
