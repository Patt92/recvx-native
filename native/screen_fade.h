#pragma once
#include <cstdint>
#include <vector>
namespace recvx {
/* sys->fade_* (types.h:836-850) and the three owners in screen.c:
   bhSetScreenFade (:21), bhControlScreenFade (:50) and bhDrawScreenFade (:97).
   The caller owns sys->cb_flg: set() is bhSetScreenFade without the
   `cb_flg |= 0x2`, control() returns true in the pass that clears it.
   set_screen_fade()/control_screen_fade() below carry that bit. */
class ScreenFade {
public:
    void set(std::uint32_t argb,float ct);
    bool control();
    /* bhDrawScreenFade: njDrawPolygon2D over the whole 640x480 frame, attr
       0x60 (fill, transparent), GS alpha ((a+1)>>1)/128 under
       njColorBlendingMode(0,8)/(1,6) = SA/ISA. `canvas` is 0xAARRGGBB. */
    void draw(std::vector<std::uint32_t>& canvas,int width,int height)const;
    float an()const{return an_;}
    float rn()const{return rn_;}
    float gn()const{return gn_;}
    float bn()const{return bn_;}
    float ao()const{return ao_;}                     /* sys->fade_ao, Ausgangswert des laufenden Fades */
    float hkn()const{return hkn_;}                   /* sys->fade_hkn */
    float ct()const{return ct_;}                     /* sys->fade_ct */
    std::uint32_t previous()const{return pbk_;}      /* sys->fade_pbk */
    void set_previous(std::uint32_t argb){pbk_=argb;} /* system.c:875 `sys->fade_pbk = 0` */
    std::uint32_t target()const{return ap_<<24|rp_<<16|gp_<<8|bp_;}
    /* game.c:111 / system.c:811: the quad is only drawn while fade_an > 0. */
    bool visible()const{return an_>0.f;}
    /* player.c:6362-6365 / 6469-6472 (bhCPM0_die): sys->fade_an/rn/gn/bn = 0 direkt, ohne bhSetScreenFade. */
    void clear_now(){set_current(0.f,0.f,0.f,0.f);}
    /* Direkte Schreiber von sys->fade_an/rn/gn/bn ohne bhSetScreenFade: room.c:524-528
       (bhFinishRoom unter ss_flg 0x100: 255/0/0/0) und bhCPM0_die (clear_now). */
    void set_current(float an,float rn,float gn,float bn){an_=an;rn_=rn;gn_=gn;bn_=bn;}
private:
    unsigned ap_=0,rp_=0,gp_=0,bp_=0;
    float ao_=0,ro_=0,go_=0,bo_=0;
    float an_=0,rn_=0,gn_=0,bn_=0;
    float ct_=0,hkn_=1.f;
    std::uint32_t pbk_=0;
};
/* Das Original hat genau einen Fade-Zustand: die Felder sys->fade_* (types.h:836-850)
   im prozessweiten sys. Alle Tasks teilen ihn - Spieltask (game.c:40/111), Statusbildschirm
   (system.c:611/921), Karte (system.c:628/806/844, map.c:334/540), Tuer (room.c:950/1000),
   Schreibmaschine (system.c:579, bup_00.c:466), Game Over (gameover.c:122-564), Skript
   (event.c:3215 bhFadeSet), Effekte (effsub1b.c bhEff134/bhEff138) und der Spielstart
   (room.c:524-530). Wie Db.clear (back_color.h) ist er deshalb prozessweit: kein Task besitzt
   eine eigene Kopie. Beim Start steht er auf 0 (sys ist statischer Speicher). */
ScreenFade& system_screen_fade();
/* bhSetScreenFade auf dem prozessweiten Zustand mit `sys->cb_flg |= 0x2` (screen.c:23). */
void set_screen_fade(std::uint32_t& cb_flg,std::uint32_t argb,float ct);
/* game.c:38-41 / system.c:804-807: `if (sys->cb_flg & 0x2) bhControlScreenFade();`; die
   Steuerung loescht cb_flg 0x2 in dem Pass, der auf das Ziel springt (screen.c:67/89). */
void control_screen_fade(std::uint32_t& cb_flg);
}
