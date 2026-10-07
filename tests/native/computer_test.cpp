#include "computer.h"
#include "events.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static std::vector<std::uint8_t> event_fixture(const std::vector<std::uint8_t>& script){std::vector<std::uint8_t> b(1600);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(n>>(i*8));};float version=1.73f;std::memcpy(b.data(),&version,4);put(16,64);put(28,1100);put(64+0x38,1400);put(1100,12);put(1104,14);put(1108,80);std::copy(script.begin(),script.end(),b.begin()+1180);return b;}
// Drives the generated terminal like bhSysCallCompEvent: OK every `period` ticks, no direction input.
static unsigned run_terminal(unsigned number,unsigned period,unsigned limit,RECVX_COM_FRAME* last=nullptr){
    const RECVX_COM_SYSTEM before{0,0x400000,0xffffffff};recvx_com_begin(number,&before);
    static RECVX_COM_FRAME frame;
    check((before.st_flg|0x4000000)!=0,"entry state");
    for(unsigned tick=0;tick<limit;++tick){
        const RECVX_COM_INPUT input{0,period&&tick%period==period-1?0x800u:0u,0};
        const bool running=recvx_com_tick(&input,&frame);
        check(frame.draw_count<=RECVX_COM_MAX_DRAW,"terminal draw list overflow");
        if(!running){if(last)*last=frame;return tick;}
    }
    return limit;
}
int main(){try{
    // bhEvtComSet raises cb_flg 0x400000 with com_num; the script is not blocked and keeps its place.
    {auto bytes=event_fixture({0x8d,4,0xff,0});Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.start(0,0);vm.tick();
     check(vm.errors.empty(),"bhEvtComSet reported as unimplemented");check((vm.flags[10][0]&0x400000)&&vm.com_number==4,"bhEvtComSet did not request the terminal");}
    // Every original script (test, 1..7) initialises, runs and draws within the quad budget.
    for(unsigned number=0;number<8;++number){
        const RECVX_COM_SYSTEM before{0,0x400000,0xffffffff};recvx_com_begin(number,&before);static RECVX_COM_FRAME frame;
        for(unsigned tick=0;tick<300;++tick){const RECVX_COM_INPUT input{0,tick%20==19?0x800u:0u,0};recvx_com_tick(&input,&frame);check(frame.draw_count<=RECVX_COM_MAX_DRAW,"draw overflow");
            for(unsigned i=0;i<frame.draw_count;++i)check(frame.draw[i].texture<3,"terminal texture page outside SYSTEM entry 2");}
    }
    // Scanner (script 4): the OK waits finish the sequence and bhExitSpEvtComputer restores the work flags.
    RECVX_COM_FRAME exit{};const auto ticks=run_terminal(4,40,4000,&exit);
    check(ticks<4000,"scanner terminal never leaves the computer task");
    check(exit.finished&&exit.stand_player,"bhExitSpEvtComputer did not finish with bhStandPlayerMotion");
    check(!(exit.system.cb_flg&0x400000)&&!(exit.system.st_flg&0x4000000)&&exit.system.sp_flg==0xffffffffu,"terminal exit flags");
    // Without OK the scanner script stays on its first wait (command 0xe2) and does not finish.
    check(run_terminal(4,0,400)==400,"scanner finished without any confirmation");
    // Messages 5 and 7 are room messages shown by command 0xf1 with their line count.
    {const RECVX_COM_SYSTEM before{0,0x400000,0xffffffff};recvx_com_begin(4,&before);static RECVX_COM_FRAME frame;bool seen5=false,seen7=false;
     for(unsigned tick=0;tick<2000;++tick){const RECVX_COM_INPUT input{0,tick%20==19?0x800u:0u,0};if(!recvx_com_tick(&input,&frame))break;
        if(frame.message_visible&&frame.message_number==5&&frame.message_lines==2)seen5=true;if(frame.message_visible&&frame.message_number==7&&frame.message_lines==2)seen7=true;}
     check(seen5&&seen7,"scanner room messages 5 and 7 were not requested");}
    // Password terminal (script 7): the soft keyboard types 1971, Enter compares it with message 3 and sets ev flag 0x3d (0x3e for a wrong word).
    for(const bool right:{true,false}){
        const RECVX_COM_SYSTEM before{0,0x400000,0xffffffff};recvx_com_begin(7,&before);static RECVX_COM_FRAME frame;
        auto tick=[&](unsigned held,unsigned pressed){const RECVX_COM_INPUT input{held,pressed,0};const bool running=recvx_com_tick(&input,&frame);return running;};
        std::vector<unsigned> flags;auto collect=[&]{for(unsigned i=0;i<frame.flag_count;++i)flags.push_back(frame.flags[i]);};
        auto idle=[&](unsigned n){for(unsigned i=0;i<n;++i){tick(0,0);collect();}};
        auto press=[&]{tick(0,0x800);collect();idle(2);};
        auto step=[&](unsigned bits){tick(bits,0);collect();idle(2);}; // pad_on direction for one tick; the original repeats after eight
        idle(80);press();idle(60); // intro text, OK at the first wait, then the keyboard opens
        step(1);step(4);                       // cursor (1,1) -> up -> left: key '1'
        press();                               // 1
        for(unsigned i=0;i<8;++i)step(8);      // to '9' (x=8)
        press();                               // 9
        step(4);step(4);press();               // 7
        if(right){for(unsigned i=0;i<6;++i)step(4);}else{step(8);step(8);} // '1' again, or '9' for a wrong word
        press();
        for(unsigned i=0;i<(right?9u:1u);++i)step(8);step(2);step(2); // Enter key (x=9, y=2)
        press();
        for(unsigned i=0;i<600;++i){if(!tick(0,i%30==29?0x800u:0u))break;collect();}
        bool saw_ok=false,saw_bad=false;for(auto f:flags){saw_ok|=f==0x3d;saw_bad|=f==0x3e;}
        check(right?(saw_ok&&!saw_bad):(saw_bad&&!saw_ok),"password terminal compared the typed word wrongly");
    }
    std::cout<<"Computer terminal tests passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Computer test failed: "<<e.what()<<'\n';return 1;}}
