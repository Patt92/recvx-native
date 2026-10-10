#include "gameover.h"
#include "asset_io.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
// Positive test of the game-over sequence (gameover.c semantics): the four
// dispatcher phases, the pass counts of bhInitGameOver/bhMainGameOver, the
// cleared original flags, the continue prompt with its alpha ramp and choice,
// Retry and the return to the title, and the death scene with its explosion
// table. The native mechanism in gameover.cpp is driven directly; no original
// procedure is linked.
static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
static void put(std::vector<std::uint8_t>& b,unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);}
static std::vector<std::uint8_t> room(){std::vector<std::uint8_t>b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);put(b,16,64);put(b,28,1100);put(b,102,1400);put(b,1100,16);put(b,1104,18);put(b,1108,80);put(b,1112,110);return b;}
namespace {
struct Session {
    std::vector<std::uint8_t> bytes=room();
    Inventory inventory;MessageDialog dialog;
    EventVm events{bytes,inventory,dialog};
    AudioMixer audio;
    GameOver over;
    unsigned passes=0;
    GameOver::Result last=GameOver::Result::Running;
    explicit Session(const std::filesystem::path& data):audio(data),over(data){}
    GameOver::Result step(unsigned pad=0){last=over.tick(pad,events,audio);++passes;return last;}
    // Runs until `stop` holds, returning how many passes that took.
    template<typename Stop> unsigned run(unsigned limit,Stop stop,unsigned pad=0){
        for(unsigned i=0;i<limit;++i){
            if(stop())return i;
            step(pad);
        }
        check(stop(),"the game over did not reach the expected state");
        return limit;
    }
    bool played(const std::string& name)const{
        return std::find(audio.played.begin(),audio.played.end(),name)!=audio.played.end();
    }
};
}
int main(int argc,char** argv){try{
    _Make_SinTable();
    const auto data=argc>1?std::filesystem::path(argv[1]):std::filesystem::path{};

    // --- dispatcher, init flags and the pass counts of the opening ---------
    {Session s(data);
     for(unsigned bit:{69u,67u,70u})s.events.set_flag(1,bit,0);
     s.events.event_deadline_mode=2;s.events.event_deadline_ticks=900;
     s.over.begin(s.events,0,0,0,0,nullptr);
     check(s.over.active(),"bhSelectContinue did not start the dispatcher");
     s.step(); // bhInitGameOver
     check(!s.events.flag(1,69)&&!s.events.flag(1,67)&&!s.events.flag(1,70),
           "bhInitGameOver did not clear the three original timer flags");
     check(s.events.flags[12][0]&0x100u,"bhInitGameOver did not raise ts_flg 0x100");
     check(!(s.events.flags[10][0]&0x4u),"bhInitGameOver did not clear cb_flg 0x4");
     check(!s.events.event_deadline_mode&&!s.events.event_deadline_ticks,
           "bhInitGameOver left the event timer running");
     check(s.over.phase()==0,"the main pass did not start in mode 0");
     const auto opening=s.run(200,[&]{return s.over.phase()!=0;});
     check(opening==71,("mode 0 held for "+std::to_string(opening)+" passes instead of 71").c_str());
     check(s.over.phase()==1,"mode 0 did not hand over to the mesh");
     const auto mesh=s.run(200,[&]{return s.over.phase()!=1;});
     check(mesh==51,("the mesh ran "+std::to_string(mesh)+" passes before the prompt instead of 51").c_str());
     check(s.over.phase()==2,"the mesh did not open the continue prompt");
     // The prompt fades in by 16 alpha steps.
     check(s.over.count()==0,"the prompt started with a visible alpha");
     const auto ramp=s.run(60,[&]{return s.over.count()>=0xFF;});
     check(ramp==16,("the prompt alpha ramp took "+std::to_string(ramp)+" passes instead of 16").c_str());}

    // --- the choice: left/right move it, Yes retries -----------------------
    {Session s(data);
     s.over.begin(s.events,0,0,0,0,nullptr);
     s.run(300,[&]{return s.over.phase()==2&&s.over.count()>=0xFF;});
     const auto before=s.audio.played.size();
     s.step(0x8u); // right -> No
     check(s.audio.played.size()>before,"moving the choice played no cursor sound");
     s.step(0x4u); // left -> Yes
     s.step(0x800u); // confirm Yes
     check(s.over.phase()==3,"confirming Yes did not start the retry fade");
     const auto fade=s.run(60,[&]{return s.last==GameOver::Result::Retry;});
     check(s.last==GameOver::Result::Retry,"Yes did not reach bhPopGameData");
     check(fade>5&&fade<30,("the retry fade took "+std::to_string(fade)+" passes").c_str());
     check(!s.over.active(),"the dispatcher stayed active after the retry");
     check(s.events.flags[12][0]&0x4000u,"bhPopGameData did not set ts_flg 0x4000");}

    // --- No plays the death voice and runs out into the title --------------
    {Session s(data);
     s.over.begin(s.events,0,0,0,0,nullptr);
     s.run(300,[&]{return s.over.phase()==2&&s.over.count()>=0xFF;});
     s.step(0x8u);                 // choose No
     s.step(0x800u);               // confirm
     check(s.over.phase()==1,"declining did not return to the mesh");
     if(!data.empty())check(s.played("ADV/0008"),"declining did not play the original death voice");
     const auto out=s.run(400,[&]{return s.last==GameOver::Result::Title;});
     check(s.last==GameOver::Result::Title,"the declined game over did not return to the title");
     check(out>100,"the mesh did not run its remaining passes before the title");
     check(!s.over.active(),"the dispatcher stayed active after the title return");}

    // --- battle mode (gm_mode 3) has no prompt and exits into a retry ------
    {Session s(data);
     s.over.begin(s.events,1,3,0,0,nullptr);
     s.step();
     check(!(s.events.flags[11][0]&0x80000000u),"battle mode did not clear gm_flg 0x80000000");
     s.run(200,[&]{return s.over.phase()==1;});
     s.run(400,[&]{return s.last!=GameOver::Result::Running;});
     check(s.last==GameOver::Result::Retry,"battle mode did not retry without a prompt");
     if(!data.empty())check(s.played("ADV/0009"),"battle mode played no death voice for player 1");}

    // --- death scene: the original explosion table and its dying motion ----
    {Session s(data);
     s.over.begin(s.events,0,0,0,0,nullptr);
     s.step();                                   // bhInitGameOver
     s.events.flags[11][0]|=0x400u;              // the self-destruct death
     s.run(200,[&]{return s.over.phase()!=0;});
     check(s.over.phase()==10,"gm_flg 0x400 did not start the death scene");
     check(s.events.flags[6][0]==0x6Cu,"the death scene did not take sp_flg 0x6C");
     check((s.events.flags[5][0]&0x5u)==0x5u,"the death scene did not set st_flg 0x1/0x4");
     check(s.events.flags[11][0]&0x200080u,"the death scene did not set gm_flg 0x200080");
     if(!data.empty()){
         GameUi ui(data);SoftwareRenderer image(320,240);s.over.draw(image,ui);
         // gameover.c:244 explicitly installs 2/20000. Setting cam.flg 0x40
         // later is not itself a call of bhChangeViewClipRM.
         check(image.last_clip()[0]==2.f&&image.last_clip()[1]==20000.f,
               "death scene lost the explicitly installed original clip range");
     }
     // gov_etb fires at 40, 54, 68, 82, 96 and 110; the fifth one starts the
     // dying motion, its voice and the white flash. The scene hands back after
     // pass 150.
     const auto scene=s.run(300,[&]{return s.over.phase()!=10;});
     check(scene==151,("the death scene ran "+std::to_string(scene)+" passes instead of 151").c_str());
     check(s.over.blasts()==6,"the death scene did not fire all six gov_etb explosions");
     check(s.over.phase()==1,"the death scene did not hand back to the mesh");
     check(s.events.flags[6][0]==0x20u,"the death scene did not restore sp_flg 0x20");
     if(!data.empty())check(s.played("ADV/0008"),"the death scene played no death voice");}

    // --- the screen is actually drawn ---------------------------------------
    if(!data.empty()){
        Session s(data);GameUi ui(data);
        s.over.begin(s.events,0,0,0,0,nullptr);
        s.run(200,[&]{return s.over.phase()==1;});
        SoftwareRenderer image(320,240);
        std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);
        s.over.draw(image,ui);
        check(std::any_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p!=0xff123456u;}),
              "the game-over mesh drew nothing");
        s.run(200,[&]{return s.over.phase()==2&&s.over.count()>=0xFF;});
        std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);
        s.over.draw(image,ui);
        check(std::any_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p!=0xff123456u;}),
              "the continue prompt drew nothing");
    } else std::cout<<"No cooked data supplied: drawing and the original voices were not exercised.\n";

    std::cout<<"Game over: dispatcher phases, timings, cleared flags, prompt, retry, title return and death scene verified.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"Game-over test failed: "<<e.what()<<'\n';return 1;}}
