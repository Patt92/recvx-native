#pragma once
#include "ui.h"
#include "events.h"
#include <functional>
namespace recvx {
class AudioMixer;
// Native ItemBox pad pass, list scroll and SpriteH/StatusCancel slide.
// Inventory::box_change owns ItemBoxChange; GameUi draws the original sprite groups.
class BoxMenu {
public:
    // StatusMain case 4: SpriteH, ItemBox pad branch, then ItemBox tail.
    void tick(unsigned on,unsigned pressed,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio);
    void synchronize(Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer* audio=nullptr);
    bool input(MenuInput key,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio);
    // ItemBox reads pad_on every game tick, including release ticks (sub1.c:7688).
    void pad(unsigned on,unsigned pressed,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio);
    void draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& inventory,const StatusMenu& menu)const;
    // ItemTaskCheck's bhStandPlayerMotion (player.c:1114), answered by the player owner.
    std::function<void()> stand_player;
    unsigned stand_gaps()const{return stand_gaps_;}
    // sub1.c:3105 ruft bhStandPlayerMotion genau einmal; hat der Kistenabbau ihn schon
    // gerufen, darf der allgemeine Statusaustritt (runtime map_task) ihn nicht wiederholen.
    bool take_stood(){const bool v=stood_;stood_=false;return v;}
    bool active()const{return active_;}
    unsigned base()const{return base_;}
    // sub1.c ItemBox testmode 2 (the box list scrolls under the fixed row 3).
    unsigned box_index()const{return ips1_;}
    unsigned cursor_cell()const{return cell_;}
    float row_shift()const{return row_shift_;}
    unsigned arrows()const{return selecting_?(arrows_?arrows_:3u):0u;}
    // Slide in/out running: ItemBox ignores the pad while statusflg 0x40 is set.
    bool moving()const{return active_&&slide_.moving();}
    void reset(){active_=false;closing_=false;closed_=false;exit_=exit_pressed_=false;exit_wait_=0;note_=0;selecting_=false;
        ips1_=ipsdisp_=0;cell_=2;number_=flag_=0;wait_=keywait_=0;boxwait_=4;row_shift_=0.f;arrows_=0;}
private:
    // ItemBox testmode 3 (cursor on EXIT, pressed animation) and the non-modal bhSetMessage note.
    bool active_=false,selecting_=false,closing_=false,exit_=false,exit_pressed_=false;
    // swork.statusflg 0x80000: SpriteH hat die Ausfahrt beendet; ItemBox laeuft nicht mehr
    // (sub1.c:3463), ItemTaskCheck zaehlt taskloop bis zum Abbau (sub1.c:3103/3227).
    bool closed_=false,stood_=false;
    void leave(Inventory& inventory,StatusMenu& menu,EventVm* events);
    unsigned exit_wait_=0,note_=0,stand_gaps_=0;
    void cursor(unsigned on,Inventory& inventory,StatusMenu& menu,AudioMixer& audio);
    StatusSlide slide_;
    unsigned base_=64;
    // ItemBoxInit: st->ips1/st->ipsdisp are the box index (S_WORK), advanced by
    // the testmode 2 scroll. `number` (+-19) is the two-frame row displacement
    // of ItemBoxIconSet, `flag` (1/2) its compensating +-38 frame, `wait` the
    // 38/|number| animation counter. `row_shift` accumulates the persistent
    // parts_20b pos[1] mutation of ItemBoxIconSet so the draw sees the rows
    // where the original left them.
    unsigned ips1_=0,ipsdisp_=0,cell_=2;int number_=0,flag_=0;unsigned wait_=0,keywait_=0,boxwait_=4;
    float row_shift_=0.f;unsigned arrows_=0;
};
}
