#include "gameover.h"
#include "asset_io.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test fuhr den Game-Over-
// Ablauf aus gameover.inc (Phasenfolge, Wartezeiten, gelöschte Originalflags,
// Retry-Ja/Nein, Kampfmodus-Retry und das Punktnetz). Generator und Include
// sind Quarantäne, siehe native/gameover.cpp. Geprüft bleibt nur, dass die
// Lücke sich wie dokumentiert verhält: begin() startet nichts, tick() trifft
// keine Entscheidung, draw() zeichnet nicht.
static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
static void put(std::vector<std::uint8_t>& b,unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);}
static std::vector<std::uint8_t> room(){std::vector<std::uint8_t>b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);put(b,16,64);put(b,28,1100);put(b,102,1400);put(b,1100,16);put(b,1104,18);put(b,1108,80);put(b,1112,110);return b;}
int main(int argc,char** argv){try{_Make_SinTable();auto bytes=room();Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);
    const auto data=argc>1?std::filesystem::path(argv[1]):std::filesystem::path{};AudioMixer audio(data);GameOver over(data);
    for(unsigned mode:{0u,3u}){
        events.set_flag(1,69,0);events.set_flag(1,67,0);events.set_flag(1,70,0);
        over.begin(events,0,int(mode));
        check(!over.active(),"game-over gap claims an active dispatcher");
        check(events.flag(1,69)&&events.flag(1,67)&&events.flag(1,70),"game-over gap cleared original flags without the dispatcher");
        for(unsigned t=0;t<200;++t)check(over.tick(0x800,events,audio)==GameOver::Result::Running,"game-over gap decided Retry or Title");
        check(over.phase()==0&&over.count()==0,"game-over gap reports a phase or counter");
    }
    if(!data.empty()){ // GameUi needs the original menu assets
        SoftwareRenderer image(64,48);std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);
        GameUi ui(data);over.draw(image,ui);
        check(std::all_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p==0xff123456u;}),"game-over gap drew into the frame");}
    std::cout<<"Game-over dispatcher, death scene and retry prompt are a documented gap; nothing is drawn or decided.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
