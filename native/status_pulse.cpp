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
void StatusPulse::begin_cure(unsigned condition,unsigned pulseflg){
    if(!opened_)open(condition);
    hp_=condition;
    run([&]{pulse::PulseInit();pulse::pulsework.pulseflg|=pulseflg;pulse::swork.statusflg|=0x20;});
}
unsigned StatusPulse::heal_mode()const{if(!healing())return 0;pulse::PULSE_WORK w;std::memcpy(&w,work_.data(),sizeof w);return w.pulseflg&0x40?1:w.pulseflg&0x80?2:0;}
void StatusPulse::tick(float center_x,float center_y){
    if(!opened_)return;
    std::vector<std::array<pulse::NJS_TEXTUREH_VTX,4>> raw;
    run([&]{pulse::cen_pos[2][0]=center_x;pulse::cen_pos[2][1]=center_y;pulse::captured=&raw;pulse::PulseMain();pulse::captured=nullptr;});
    quads_.clear();for(const auto& q:raw){PulseQuad out;for(unsigned i=0;i<4;++i)out[i]={q[i].x,q[i].y,q[i].u,q[i].v,q[i].bcol,q[i].ocol,q[i].z};quads_.push_back(out);}
}
}
