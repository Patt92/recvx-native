#pragma once
#include "ui.h"
#include "events.h"
namespace recvx {
class AudioMixer;
// Host screen adapter for the original ItemBoxChange procedure. Window slides
// and animated box sprites are not represented by this initial host view.
class BoxMenu {
public:
    void synchronize(StatusMenu& menu,EventVm* events);
    bool input(MenuInput key,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio);
    void draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& inventory,const StatusMenu& menu)const;
    bool active()const{return active_;}
    unsigned base()const{return base_;}
    void reset(){active_=false;}
private:
    bool active_=false,selecting_=false;
    unsigned base_=64,index_=0;
};
}
