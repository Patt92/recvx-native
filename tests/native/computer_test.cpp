#include "computer.h"
#include "events.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test fuhr den Terminal-
// Skriptinterpreter aus computer.inc (alle acht Skripte, Soft-Keyboard,
// Fensterautomat, Quadliste, Raummeldungen, Passwortvergleich). Generator und
// Include sind Quarantäne, siehe native/computer_core.c. Geprüft bleiben der
// Eventbefehl bhEvtComSet, der den Task weiter anfordert, und dass die Lücke
// sich wie dokumentiert verhält: ein leerer Frame, sofortiges Ende und die
// originalen Austrittsflags.
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static std::vector<std::uint8_t> event_fixture(const std::vector<std::uint8_t>& script){std::vector<std::uint8_t> b(1600);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(n>>(i*8));};float version=1.73f;std::memcpy(b.data(),&version,4);put(16,64);put(28,1100);put(64+0x38,1400);put(1100,12);put(1104,14);put(1108,80);std::copy(script.begin(),script.end(),b.begin()+1180);return b;}
int main(){try{
    // bhEvtComSet raises cb_flg 0x400000 with com_num; the script is not blocked and keeps its place.
    {auto bytes=event_fixture({0x8d,4,0xff,0});Inventory inv;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.start(0,0);vm.tick();
     check(vm.errors.empty(),"bhEvtComSet reported as unimplemented");check((vm.flags[10][0]&0x400000)&&vm.com_number==4,"bhEvtComSet did not request the terminal");}
    // Every original script number must end in the documented gap, not hang.
    for(unsigned number=0;number<8;++number){
        const RECVX_COM_SYSTEM before{0,0x400000,0xffffffff};recvx_com_begin(number,&before);
        RECVX_COM_FRAME frame{};frame.active=1;frame.draw_count=7;
        const int running=recvx_com_tick(nullptr,&frame);
        check(!running&&!frame.active&&frame.finished,"terminal gap does not report an immediately finished task");
        check(!frame.draw_count&&!frame.sound_count&&!frame.flag_count&&!frame.message_visible&&!frame.stand_player,
              "terminal gap produced drawing, sound, flags or a message without a procedure");
        check(frame.system.sp_flg==0xffffffffu,"terminal gap does not report the original bhExitSpEvtComputer sp_flg");
    }
    std::cout<<"bhEvtComSet still requests the terminal; its script interpreter is a documented gap.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Computer test failed: "<<e.what()<<'\n';return 1;}}
