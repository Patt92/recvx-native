#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace recvx {
// One njDrawTextureH vertex: screen position/depth, UV normalised to the
// 512x512 status atlas (gbix 200004), base colour (modulates the texel, alpha
// included) and offset colour (added to RGB).
struct PulseVertex {float x,y,u,v;std::uint32_t base,offset;float z;};
using PulseQuad=std::array<PulseVertex,4>;
// Zustandsmonitor des Statusbildschirms (EKG), eigenständig nativ nach der
// Semantik von sub1.c PulseInit/PulseMain/PulseAnim/PulseFadeAnim/
// PulseHealAnim/PulsePoisonHealAnim/Pulse00. Ein tick() entspricht einem
// StatusMain-Durchlauf.
class StatusPulse {
public:
    // Condition index as StatusInit/Use_00 compute it: 0 Fine, 1/2 Caution,
    // 3 Danger, 4 Poison.
    static unsigned condition(int hp,bool poisoned){ // poisoned: plp->stflg & 0x280000
        return poisoned?4:hp>=120?0:hp>=60?1:hp>=30?2:3;}
    void open(unsigned condition);                        // StatusInit + PulseInit
    // Use_00 nach einer Heilung: st->hp, dann PulseInit und pulseflg (0x40
    // Heilung, 0x80 Gegengift) samt statusflg 0x20.
    void begin_cure(unsigned condition,unsigned pulseflg);
    void tick(float center_x=0,float center_y=100);       // PulseMain at cen_pos[2]
    unsigned condition()const{return hp_;}
    bool healing()const{return statusflg_&0x20;}
    unsigned heal_mode()const;                            // 0 none, 1 heal sweep, 2 poison cure
    const std::vector<PulseQuad>& quads()const{return quads_;}
private:
    // PULSE_WORK (sub1.h, 0x120 Byte) als native Felder.
    struct Point {float x=0,y=0;};
    struct Work {
        std::array<Point,5> pos_00{},pos_01{},pos_02{};
        Point heal_pos00{},heal_pos01{};
        std::array<std::int16_t,5> uv_00{},uv_01{};      // NJS_COLOR.tex.u (Sint16)
        std::array<std::uint32_t,5> col00{},col01{};      // NJS_COLOR.color
        std::array<std::uint32_t,5> ocol00{},ocol01{};
        unsigned pulseflg=0,fadeflg=0,count=0;
    };
    void init();                                          // PulseInit
    void anim();                                          // PulseAnim
    void fade();                                          // PulseFadeAnim
    void heal(float x,float y);                           // PulseHealAnim
    void poison_heal(float x,float y);                    // PulsePoisonHealAnim
    void trace(float x,float y,int u,int v);              // Pulse00
    Work work_{};
    unsigned hp_=0,statusflg_=0;bool opened_=false;
    std::vector<PulseQuad> quads_;
};
}
