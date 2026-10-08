#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace recvx {
// One njDrawTextureH vertex from the original routines: screen position/depth,
// UV normalised to the 512x512 status atlas (gbix 200004), base colour
// (modulates the texel, alpha included) and offset colour (added to RGB).
struct PulseVertex {float x,y,u,v;std::uint32_t base,offset;float z;};
using PulseQuad=std::array<PulseVertex,4>;
// Host owner of the original StatusMain condition monitor (sub1.c PulseInit,
// PulseMain, PulseAnim, PulseFadeAnim, PulseHealAnim, PulsePoisonHealAnim and
// Pulse00). One tick() equals one original StatusMain frame.
class StatusPulse {
public:
    // Condition index as StatusInit/Use_00 compute it: 0 Fine, 1/2 Caution,
    // 3 Danger, 4 Poison.
    static unsigned condition(int hp,bool poisoned){ // poisoned: plp->stflg & 0x280000
        return poisoned?4:hp>=120?0:hp>=60?1:hp>=30?2:3;}
    void open(unsigned condition);                        // StatusInit + PulseInit
    // Use_00 after a cure: st->hp, then PulseInit and the captured pulseflg
    // bits (0x40 heal, 0x80 poison cure) plus statusflg 0x20.
    void begin_cure(unsigned condition,unsigned pulseflg);
    void tick(float center_x=0,float center_y=100);       // PulseMain at cen_pos[2]
    unsigned condition()const{return hp_;}
    bool healing()const{return statusflg_&0x20;}
    unsigned heal_mode()const;                            // 0 none, 1 heal sweep, 2 poison cure
    const std::vector<PulseQuad>& quads()const{return quads_;}
private:
    alignas(8) std::array<std::uint8_t,0x120> work_{};   // original PULSE_WORK
    unsigned hp_=0,statusflg_=0;bool opened_=false;
    std::vector<PulseQuad> quads_;
    template<class F> void run(F&& routine);
};
}
