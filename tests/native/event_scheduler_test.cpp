// Isolierte Bytecode-Fixtures fuer event.c:9098-9257 und 12956-13095.
// Keine Originalfunktionskoerper, keine Spielverlaufs-/Raumabnahme.
#include "events.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace recvx;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void put(std::vector<std::uint8_t>& b,std::size_t p,unsigned value){for(unsigned i=0;i<4;++i)b.at(p+i)=std::uint8_t(value>>(i*8));}
std::vector<std::uint8_t> fixture(const std::vector<std::uint8_t>& first,
                                const std::vector<std::uint8_t>& second={},
                                const std::vector<std::uint8_t>& scan={0},
                                const std::vector<std::uint8_t>& init={0}){
    std::vector<std::uint8_t> b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);
    put(b,16,64);put(b,28,512);put(b,64+0x38,1400);
    put(b,512,16);put(b,516,80);put(b,520,160);put(b,524,320);
    std::copy(init.begin(),init.end(),b.begin()+528);
    std::copy(scan.begin(),scan.end(),b.begin()+592);
    std::copy(first.begin(),first.end(),b.begin()+672);
    std::copy(second.begin(),second.end(),b.begin()+832);
    put(b,1400,1);put(b,1404,8);b[1408]=b[1409]=255;return b;
}
}
int main(){unsigned failures=0,checks=0;
    auto test=[&](const char* name,const std::function<void()>& body){++checks;try{body();std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    test("bhSv 16 writes player floor without corrupting floor trigger index",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({8,16,2,0,0xff,0}),inv,dialog);
        unsigned floor=0;vm.values[16]=7;vm.set_player_floor=[&](unsigned value){floor=value;};
        vm.start(0,0);vm.tick();check(floor==2&&vm.values[16]==7&&vm.errors.empty(),"player floor and floor trigger index aliased");});
    for(unsigned tail:{0xfdu,0xf3u})test(tail==0xfd?"EWHILE nonzero":"EWHILE2 nonzero",[&]{
        // bhCk returns mode ^ flag = 2. Both EWHILE variants must loop.
        auto b=fixture({0xfc,8,4,1,100,0,0,2,0x15,0,0,0,0xfe,std::uint8_t(tail),0xff,0});
        Inventory inv;MessageDialog dialog;EventVm vm(b,inv,dialog);unsigned calls=0;
        vm.binding=[&](const EventCommand&){++calls;return true;};vm.start(0,0);vm.tick();vm.tick();
        check(vm.errors.empty()&&calls==2,"return 2 was treated as false");});
    test("SLEEP zero wraps once",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xf8,0xf9,0,0,0xff,0}),inv,dialog);
        vm.start(0,0);vm.tick();check(vm.task_states().at(0).delay==0xffffffffu,"zero decremented twice");});
    test("SLEEP ignores padding opcode",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xf8,0xd5,1,0,5,1,100,0,0,0,0xff,0}),inv,dialog);
        vm.start(0,0);vm.tick();check(vm.task_states().at(0).offset==164,"bhSleep dispatched its padding byte");
        vm.tick();check(vm.flag(1,100),"sleep did not resume after four bytes");});
    test("SLEEP counter survives Event_init",[]{
        const std::vector<std::uint8_t> next={0xfc,8,4,1,100,0,0,1,0x15,0,0,0,0xfe,0xfb,0,0xff,0};
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xf8,0xf9,3,0,0xff,0},next),inv,dialog);unsigned calls=0;
        vm.binding=[&](const EventCommand&){++calls;return true;};vm.start(0,0);vm.tick();vm.start(0,1);
        vm.tick();vm.tick();vm.tick();check(vm.errors.empty()&&calls==2&&vm.task_states().empty(),"WHILE/NEXT lost retained cnt[0]=2");});
    test("Event_exec self replacement retains current cursor",[]{
        Inventory inv;MessageDialog dialog;
        EventVm vm(fixture({0x14,0,0,1,5,1,100,0,0,0,0xff,0},{5,1,101,0,0,0,0xff,0}),inv,dialog);
        vm.start(0,0);vm.tick();check(vm.errors.empty()&&vm.flag(1,100)&&!vm.flag(1,101),"Event_init changed bhScePtr");});
    test("scd0 uses task zero counters",[]{
        // scd0 FOR leaves cnt2=cnt3=3 in bhEtask[0]; Event_init preserves both.
        Inventory inv;MessageDialog dialog;
        EventVm vm(fixture({0x15,0,0,0,0xff,0},{},{0},{0xfa,0,3,0,0}),inv,dialog);unsigned count=0;
        vm.binding=[&](const EventCommand& c){count=c.loop_remaining;return true;};vm.initialize();vm.start(0,0);
        vm.flags[6][0]|=0x10;vm.tick();check(vm.errors.empty()&&count==3,"scd0 wrote a temporary task");});
    test("scd1 keeps task fifteen work",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xfa,0,7,0,0xfe,0xff,0},{},{0x15,0,0,0,0}),inv,dialog);
        unsigned count=0;vm.binding=[&](const EventCommand& c){count=c.loop_remaining;return true;};
        vm.start(15,0);vm.tick();vm.tick();check(vm.errors.empty()&&count==7,"scd1 lost last scheduler task's cnt2");});
    test("initialize resets active tasks and event timer",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xfe,0xfe,0xff,0}),inv,dialog);
        vm.start(3,0);vm.tick();vm.initialize();check(vm.task_states().empty()&&vm.event_timer==0,"bhInitEvent retained old status/timer");});
    test("out-of-range instruction is diagnosed",[]{
        auto b=fixture({});put(b,520,unsigned(b.size()-1-512));b.back()=0xf4;
        Inventory inv;MessageDialog dialog;EventVm vm(b,inv,dialog);vm.start(0,0);vm.tick();
        check(vm.errors.size()==1&&vm.task_states().at(0).blocked,"missing bounded diagnostic");});
    test("four original counter slots",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xfa,0,1,0,0xfa,0,1,0,0xfa,0,1,0,0xfa,0,1,0,0xf8,0xf9,2,0}),inv,dialog);
        vm.start(0,0);vm.tick();check(vm.errors.size()==1,"cnt[4] silently became extra host storage");});
    test("PlyDpos operand layout",[]{
        // event.c:8182-8210: unused v0, sign v1, target v2, step v3, unused v4.
        Inventory inv;MessageDialog dialog;
        EventVm vm(fixture({0xbe,0xa5,0,90,30,0x5a,0xff,0}),inv,dialog);unsigned yaw=0;
        vm.player_yaw=[&]{return yaw;};vm.set_player_yaw=[&](unsigned value){yaw=value;};
        vm.start(0,0);vm.tick();check(vm.errors.empty()&&yaw==5461,"PlyDpos read sign/target/step one byte early");});
    test("SLEEP count survives room change",[]{
        Inventory inv;MessageDialog a,b;
        EventVm old(fixture({0xf8,0xf9,3,0,0xff,0}),inv,a);old.start(0,0);old.tick();
        EventVm next(fixture({0xfc,8,4,1,100,0,0,1,0x15,0,0,0,0xfe,0xfb,0,0xff,0}),inv,b);
        next.inherit_progress(old);next.initialize();unsigned calls=0;
        next.binding=[&](const EventCommand&){++calls;return true;};next.flags[6][0]|=0x10;next.start(0,0);
        next.tick();next.tick();next.tick();check(next.errors.empty()&&calls==2&&next.task_states().empty(),"room load cleared cnt[0]");});
    test("full task table replaces slot fifteen",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xfe},{0xfe}),inv,dialog);
        for(unsigned i=0;i<16;++i)vm.start(i,0);vm.start(255,1);const auto state=vm.task_states();
        check(state.size()==16&&state.back().level==15&&state.back().event==1,"automatic level changed original replacement order");});
    test("task kill leaves current pass running",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xbc,0,5,1,100,0,0,0,0xfe}),inv,dialog);
        vm.start(0,0);vm.tick();check(vm.errors.empty()&&vm.flag(1,100)&&vm.task_states().empty(),"status killed bhScePtr's current pass");});
    test("Poison2Cr clears saved Claire only",[]{
        Inventory inv;inv.player=1;inv.infected=true;inv.ply_stflg[0]=0x40280000u;MessageDialog dialog;
        EventVm vm(fixture({0xae,0,0xff,0}),inv,dialog);vm.start(0,0);vm.tick();
        check(inv.infected&&inv.ply_stflg[0]==0x40080000u,"cleared live player's infection instead of ply_stflg[0]");
        inv.switch_player(0);check(!inv.infected&&inv.poisoned,"Claire restored the uncleared infection");});
    test("MovieTime undefined selector returns raw time",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({0xac,0,0,0,6,0,5,1,100,0,0,0,0xff,0}),inv,dialog);
        vm.movie_active=[]{return true;};vm.movie_time=[]{return 500u;};vm.start(0,0);vm.tick();
        check(vm.errors.empty()&&vm.flag(1,100),"original no-default path was rejected");});
    for(unsigned frame:{0xffffffffu,0u})test(frame?"FrameCheck signed current":"FrameCheck signed target",[&]{
        Inventory inv;MessageDialog dialog;
        EventVm vm(fixture({0x38,0,0,0,0,std::uint8_t(frame?0:128),5,1,100,0,0,0,0xff,0}),inv,dialog);
        vm.animation_frame=[&](unsigned,unsigned){return frame;};vm.start(0,0);vm.tick();
        check(vm.errors.empty()&&vm.flag(1,100)==bool(frame),"frame condition compared unsigned values");});
    test("Ck floor selector uses low byte before bit mask",[]{
        Inventory inv;MessageDialog dialog;EventVm vm(fixture({4,10,22,4,9,0,5,1,100,0,0,0,0xff,0}),inv,dialog);
        vm.set_flag(10,22,0);vm.values[16]=3;vm.start(0,0);vm.tick();
        check(vm.errors.empty()&&!vm.flag(1,100),"Ck skipped the flr_idx guard for a high-bit alias");});
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
