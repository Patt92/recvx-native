#pragma once
#include <cstdint>
#include <vector>
namespace recvx {
/* sys->fade_* (types.h:836-850) and the three owners in screen.c:
   bhSetScreenFade (:21), bhControlScreenFade (:50) and bhDrawScreenFade (:97).
   The caller owns sys->cb_flg: set() is bhSetScreenFade without the
   `cb_flg |= 0x2`, control() returns true in the pass that clears it. */
class ScreenFade {
public:
    void set(std::uint32_t argb,float ct);
    bool control();
    /* bhDrawScreenFade: njDrawPolygon2D over the whole 640x480 frame, attr
       0x60 (fill, transparent), GS alpha ((a+1)>>1)/128 under
       njColorBlendingMode(0,8)/(1,6) = SA/ISA. `canvas` is 0xAARRGGBB. */
    void draw(std::vector<std::uint32_t>& canvas,int width,int height)const;
    float an()const{return an_;}
    std::uint32_t previous()const{return pbk_;}      /* sys->fade_pbk */
    void set_previous(std::uint32_t argb){pbk_=argb;} /* system.c:875 `sys->fade_pbk = 0` */
    std::uint32_t target()const{return ap_<<24|rp_<<16|gp_<<8|bp_;}
    /* game.c:111 / system.c:811: the quad is only drawn while fade_an > 0. */
    bool visible()const{return an_>0.f;}
    /* player.c:6355-6358 (bhCPM0_die): sys->fade_an/rn/gn/bn = 0 direkt, ohne bhSetScreenFade. */
    void clear_now(){an_=rn_=gn_=bn_=0.f;}
private:
    unsigned ap_=0,rp_=0,gp_=0,bp_=0;
    float ao_=0,ro_=0,go_=0,bo_=0;
    float an_=0,rn_=0,gn_=0,bn_=0;
    float ct_=0,hkn_=1.f;
    std::uint32_t pbk_=0;
};
}
