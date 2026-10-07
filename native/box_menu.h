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
    // Slide in/out running: ItemBox ignores the pad while statusflg 0x40 is set.
    bool moving()const{return active_&&slide_.moving();}
    void reset(){active_=false;closing_=false;exit_=exit_pressed_=false;exit_wait_=0;note_=0;}
private:
    // ItemBox testmode 3 (cursor on EXIT, pressed animation) and the non-modal bhSetMessage note.
    bool active_=false,selecting_=false,closing_=false,exit_=false,exit_pressed_=false;
    unsigned exit_wait_=0,note_=0;
    StatusSlide slide_;
    unsigned base_=64,index_=0;
};
}
