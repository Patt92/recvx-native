#include "gameover.h"
#include "asset_io.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
static void put(std::vector<std::uint8_t>& b,unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);}
static std::vector<std::uint8_t> room(){std::vector<std::uint8_t>b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);put(b,16,64);put(b,28,1100);put(b,102,1400);put(b,1100,16);put(b,1104,18);put(b,1108,80);put(b,1112,110);return b;}
int main(int argc,char** argv){try{_Make_SinTable();auto bytes=room();Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);const auto data=argc>1?std::filesystem::path(argv[1]):std::filesystem::path{};AudioMixer audio(data);GameOver over(data);
    for(unsigned mode:{0u,3u}){
        events.set_flag(1,69,0);events.set_flag(1,67,0);events.set_flag(1,70,0);over.begin(events,0,mode);
        check(over.tick(0,events,audio)==GameOver::Result::Running&&over.active(),"game-over dispatcher start");over.tick(0,events,audio);
        check(!events.flag(1,69)&&!events.flag(1,67)&&!events.flag(1,70),"game-over does not clear original flags");
        for(unsigned t=0;t<71;++t)over.tick(0,events,audio);
        check(over.phase()==1&&over.count()==0,"death scene wait is not 71 ticks");
        for(unsigned t=0;t<51;++t)over.tick(0,events,audio);
        if(mode==0){check(over.phase()==2,"Retry prompt not reached");
            for(unsigned t=0;t<17;++t)over.tick(0,events,audio);
            if(argc>2){GameUi ui(data);SoftwareRenderer image(640,480);over.draw(image,ui);image.save_bmp(std::filesystem::path(argv[2])/"retry-yes.bmp");}
            over.tick(0x800,events,audio);check(over.phase()==3,"Yes does not start retry fade");
            GameOver::Result result=GameOver::Result::Running;for(unsigned t=0;t<12&&result==GameOver::Result::Running;++t)result=over.tick(0,events,audio);
            check(result==GameOver::Result::Retry&&!over.active(),"Yes does not restore retry");
            over.begin(events,0,mode);for(unsigned t=0;t<145;++t)over.tick(0,events,audio);over.tick(8,events,audio);
            if(argc>2){GameUi ui(data);SoftwareRenderer image(640,480);over.draw(image,ui);image.save_bmp(std::filesystem::path(argv[2])/"retry-no.bmp");}
            over.tick(0x800,events,audio);check(over.phase()==1,"No does not continue game-over effect");
            result=GameOver::Result::Running;for(unsigned t=0;t<300&&result==GameOver::Result::Running;++t)result=over.tick(0,events,audio);
            check(result==GameOver::Result::Title&&!over.active(),"No does not return to title");
        }else{GameOver::Result result=GameOver::Result::Running;for(unsigned t=0;t<300&&result==GameOver::Result::Running;++t)result=over.tick(0,events,audio);check(result==GameOver::Result::Retry,"battle mode does not retry automatically");}
    }
    std::cout<<"original gameover.c: delay, cleared flags, Retry Yes/No and battle-mode retry passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
