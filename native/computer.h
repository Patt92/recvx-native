#pragma once
#include "computer_core.h"
#include "events.h"
#include "renderer.h"
#include "scene.h"
#include <filesystem>
#include <memory>
namespace recvx {
class AudioMixer;
class GameUi;
// Original special-event computer (sp_evt.c): bhEvtComSet raises cb_flg 0x400000,
// bhSysCall switches to the terminal task, which owns the screen until
// bhExitSpEvtComputer clears the request again. The routines are generated.
class ComputerTerminal {
public:
    explicit ComputerTerminal(std::filesystem::path data):data_(std::move(data)){}
    bool active()const{return active_;}
    // bhSysCall: start when a script requested the terminal.
    bool begin(EventVm& events);
    // One 30 Hz pass. `held`: pad_on direction bits (1 up, 2 down, 4 left, 8 right), `pressed`: pad_ps (0x800 OK, 0x1000 cancel).
    void tick(unsigned held,unsigned pressed,EventVm& events,AudioMixer* audio);
    // Edge input that arrives between ticks (menu_input path); merged into the next tick's pad_ps.
    void press(unsigned bits){pending_|=bits;}
    void draw(SoftwareRenderer& image,const GameUi& ui,const EventVm& events)const;
    void reset(){active_=false;}
    unsigned unsupported_character()const{return frame_?frame_->unsupported_character:0;}
private:
    std::filesystem::path data_;
    bool active_=false;
    unsigned pending_=0;
    std::unique_ptr<RECVX_COM_FRAME> frame_;
    mutable std::vector<SceneTexture> textures_;
};
}
