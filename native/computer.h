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
    explicit ComputerTerminal(std::filesystem::path data):data_(std::move(data)){}
    bool active()const{return active_;}
    // bhSysCall: start when a script requested the terminal.
    bool begin(EventVm& events);
    // One 30 Hz pass. `held`: pad_on direction bits (1 up, 2 down, 4 left, 8 right), `pressed`: pad_ps (0x800 OK, 0x1000 cancel).
    void tick(unsigned held,unsigned pressed,EventVm& events,AudioMixer* audio);
    // Edge input that arrives between ticks (menu_input path); merged into the next tick's pad_ps.
    void press(unsigned bits){pending_|=bits;}
    void draw(SoftwareRenderer& image,const GameUi& ui,const EventVm& events)const;
    void reset(){active_=false;frame_.reset();}
    // bhDrawSpEvtComCharacter: the actor owner answers with the scene of
    // rom->enep[chr_no] posed at the origin with the script's motion/frame.
    // An empty answer is reported as a gap, never drawn as a substitute.
    std::function<std::optional<Scene>(unsigned chr_no,unsigned mtn_no,unsigned frm_no)> character_scene;
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
    std::filesystem::path data_;
    bool active_=false;
    unsigned pending_=0;
    mutable unsigned character_gaps_=0,message_gaps_=0;
    std::unique_ptr<RECVX_COM_FRAME> frame_;
    mutable std::vector<SceneTexture> textures_;
    std::vector<std::string> text_;
    std::array<std::string,2> words_;
    void draw_character(SoftwareRenderer& image)const;
    void capture();
};
}
