#pragma once
#include "computer_core.h"
#include "events.h"
#include "renderer.h"
#include "scene.h"
#include <filesystem>
#include <functional>
#include <array>
#include <memory>
#include <string>
#include <optional>
namespace recvx {
class AudioMixer;
class GameUi;
// Original special-event computer (sp_evt.c): bhEvtComSet raises cb_flg 0x400000,
// bhSysCall switches to the terminal task, which owns the screen until
// bhExitSpEvtComputer clears the request again. The window automaton, script
// interpreter, soft keyboard and quad list are native (computer_core.c).
class ComputerTerminal {
public:
    explicit ComputerTerminal(std::filesystem::path data,std::filesystem::path mods={}):data_(std::move(data)),mods_(std::move(mods)){}
    bool active()const{return active_;}
    // bhCheckSubTask (system.c:582-596) has started the terminal; bhSysCallCompEvent (Task 17) runs only
    // once bhSysCallMonitor mn_mode0 7 (system.c:2084-2147) has waited for the 5-frame fade to black
    // (cb_flg 0x2) and cleared ts_flg 0x20000. Until then Task 7 keeps the room on screen under the fade.
    bool running()const{return active_&&monitor_==0;}
    unsigned monitor_step()const{return monitor_;}
    // bhSysCall: start when a script requested the terminal.
    bool begin(EventVm& events);
    // bhSysCallMonitor mn_mode0 7, once per frame after the game task (main.c task order 7 < 20).
    void monitor(EventVm& events);
    // One 30 Hz pass. `held`: pad_on direction bits (1 up, 2 down, 4 left, 8 right), `pressed`: pad_ps (0x800 OK, 0x1000 cancel).
    void tick(unsigned held,unsigned pressed,EventVm& events,AudioMixer* audio);
    // Edge input that arrives between ticks (menu_input path); merged into the next tick's pad_ps.
    void press(unsigned bits){pending_|=bits;}
    void draw(SoftwareRenderer& image,const GameUi& ui,const EventVm& events)const;
    void reset(){active_=false;monitor_=0;frame_.reset();}
    // bhDrawSpEvtComCharacter: the actor owner answers with the scene of
    // rom->enep[chr_no] posed at the origin with the script's motion/frame.
    // An empty answer is reported as a gap, never drawn as a substitute.
    std::function<std::optional<Scene>(unsigned chr_no,unsigned mtn_no,unsigned frm_no)> character_scene;
    // bhExitSpEvtComputer ends with bhStandPlayerMotion; the player owner answers it.
    std::function<void()> stand_player;
    unsigned stand_gaps()const{return stand_gaps_;}
    const RECVX_COM_FRAME* frame()const{return frame_.get();}
    unsigned character_gaps()const{return character_gaps_;}
    // bhDispMessage indexes rom->mesp without a bound; a room whose table
    // is shorter than the script's mes_no has no caption to draw.
    unsigned message_gaps()const{return message_gaps_;}
    // Rows of the original text grid as plain text and the two keyboard words,
    // kept from the last pass of the task so a finished run stays inspectable.
    const std::vector<std::string>& text()const{return text_;}
    const std::array<std::string,2>& words()const{return words_;}
private:
    std::filesystem::path data_,mods_;
    bool active_=false;
    unsigned monitor_=0; // mn_md1 of bhSysCallMonitor mn_mode0 7; 0 = Task 17 runs
    unsigned pending_=0;
    mutable unsigned character_gaps_=0,message_gaps_=0;
    unsigned stand_gaps_=0;
    std::unique_ptr<RECVX_COM_FRAME> frame_;
    mutable std::vector<SceneTexture> textures_;
    mutable bool textures_mods_=false;
    std::vector<std::string> text_;
    std::array<std::string,2> words_;
    void draw_character(SoftwareRenderer& image)const;
    void capture();
};
}
