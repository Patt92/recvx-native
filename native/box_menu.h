#pragma once
#include "ui.h"
#include "events.h"
namespace recvx {
class AudioMixer;
// Original ItemBoxChange adapter and settled ItemBox sprite layout.
// The slide in/out (sub1.c SpriteH/StatusCancel) is a StatusSlide (box mode); the scroll animation of the list is still missing.
class BoxMenu {
public:
    void synchronize(Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer* audio=nullptr);
    bool input(MenuInput key,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio);
    void draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& inventory,const StatusMenu& menu)const;
    bool active()const{return active_;}
    unsigned base()const{return base_;}
    // sub1.c ItemBox testmode 2 (the box list scrolls under the fixed row 3).
    unsigned box_index()const{return ips1_;}
    float row_shift()const{return row_shift_;}
    unsigned arrows()const{return selecting_?(arrows_?arrows_:3u):0u;}
    // Slide in/out running: ItemBox ignores the pad while statusflg 0x40 is set.
    bool moving()const{return active_&&slide_.moving();}
    void reset(){active_=false;closing_=false;exit_=exit_pressed_=false;exit_wait_=0;note_=0;selecting_=false;
        ips1_=ipsdisp_=0;number_=flag_=0;wait_=keywait_=0;boxwait_=4;row_shift_=0.f;arrows_=0;}
private:
    // ItemBox testmode 3 (cursor on EXIT, pressed animation) and the non-modal bhSetMessage note.
    bool active_=false,selecting_=false,closing_=false,exit_=false,exit_pressed_=false;
    unsigned exit_wait_=0,note_=0;
    StatusSlide slide_;
    unsigned base_=64;
    // ItemBoxInit: st->ips1/st->ipsdisp are the box index (S_WORK), advanced by
    // the testmode 2 scroll. `number` (+-19) is the two-frame row displacement
    // of ItemBoxIconSet, `flag` (1/2) its compensating +-38 frame, `wait` the
    // 38/|number| animation counter. `row_shift` accumulates the persistent
    // parts_20b pos[1] mutation of ItemBoxIconSet so the draw sees the rows
    // where the original left them.
    unsigned ips1_=0,ipsdisp_=0;int number_=0,flag_=0;unsigned wait_=0,keywait_=0,boxwait_=4;
    float row_shift_=0.f;unsigned arrows_=0;
};
}
