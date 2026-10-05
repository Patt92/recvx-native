#include "status_pulse.h"
#include <cstring>
namespace recvx {
#include "status_pulse.inc"
static_assert(sizeof(pulse::PULSE_WORK)==0x120,"original PULSE_WORK layout");
template<class F> void StatusPulse::run(F&& routine){
    std::memcpy(&pulse::pulsework,work_.data(),sizeof(pulse::PULSE_WORK));pulse::swork={hp_,statusflg_};
    routine();
    std::memcpy(work_.data(),&pulse::pulsework,sizeof(pulse::PULSE_WORK));hp_=pulse::swork.hp;statusflg_=pulse::swork.statusflg;
}
void StatusPulse::open(unsigned condition){hp_=condition;statusflg_=0;opened_=true;quads_.clear();run([]{pulse::PulseInit();});}
void StatusPulse::cured(unsigned condition,unsigned cure,bool resistant_poison){
    if(!opened_)open(condition);
    // Use_00: st->hp follows st->abc, then the heal or poison-cure sweep starts.
    hp_=condition;const unsigned heal=cure&15,poison=cure&0xf0;
    run([&]{
        if(heal){pulse::PulseInit();pulse::pulsework.pulseflg|=0x40;pulse::swork.statusflg|=0x20;}
        else if(poison&&!resistant_poison){pulse::PulseInit();pulse::pulsework.pulseflg|=0x80;pulse::swork.statusflg|=0x20;}
    });
}
unsigned StatusPulse::heal_mode()const{if(!healing())return 0;pulse::PULSE_WORK w;std::memcpy(&w,work_.data(),sizeof w);return w.pulseflg&0x40?1:w.pulseflg&0x80?2:0;}
void StatusPulse::tick(float center_x,float center_y){
    if(!opened_)return;
    std::vector<std::array<pulse::NJS_TEXTUREH_VTX,4>> raw;
    run([&]{pulse::cen_pos[2][0]=center_x;pulse::cen_pos[2][1]=center_y;pulse::captured=&raw;pulse::PulseMain();pulse::captured=nullptr;});
    quads_.clear();for(const auto& q:raw){PulseQuad out;for(unsigned i=0;i<4;++i)out[i]={q[i].x,q[i].y,q[i].u,q[i].v,q[i].bcol,q[i].ocol};quads_.push_back(out);}
}
}
