#pragma once
#include "save_slots.h"
#include <filesystem>
#include <vector>
namespace recvx {
// Native owner of the save/load screen (bup_00.c / ps2_SaveScreen.c /
// ps2_LoadScreen.c). The existing host files replace the card I/O only.
// Die Memory-Karten-Auswahl (SelectCard, Port 0/1) ist entfernt: sie ist
// Konsolenverhalten des Originalkerns; der Host hat genau einen Speicherort,
// deshalb fuehrt Laden/Speichern direkt in die Slotliste.
class Typewriter {
public:
    enum class Mode {Save,Load};
    enum class Key {Up,Down,Confirm,Cancel};
    // SpecialSave: SaveScreen state 18 (ps2_SaveScreen.c:552–604), entered from
    // Exit while ev_flg 56 is set; message 253/466, cursor at (256,359).
    enum class Phase {Files,Confirm,NoSave,Io,Typing,Success,Error,Exit,SpecialSave};
    void open(Mode mode,const std::filesystem::path& saves_dir);
    bool active()const{return active_;}
    Mode mode()const{return mode_;}
    Phase phase()const{return phase_;}
    const std::vector<SaveSlotInfo>& slots()const{return slots_;}
    unsigned cursor()const{return cursor_;}
    unsigned top()const{return top_;}
    unsigned row()const{return cursor_-top_;}
    unsigned selection()const{return selection_;}
    unsigned write_count()const{return write_count_;}
    // mcDisplayFileSelectWindow (McSaveFile:569-586) zeichnet slDispWriteMes, bevor es
    // im selben Durchlauf erhoeht wird; das ist der Stand, den die Zeile zeigt.
    unsigned drawn_count()const{return drawn_count_;}
    bool arrows()const{return phase_==Phase::Files&&(timer_%30)<20;}
    bool done()const{return done_;}
    bool confirmed()const{return confirmed_;}
    bool cancelled()const{return cancelled_;}
    unsigned slot()const{return slots_.empty()?0:slots_[cursor_].slot;}
    unsigned message()const;
    bool io_pending()const{return phase_==Phase::Io&&io_ready_;}
    void io_complete(bool success,const std::filesystem::path& saves_dir);
    unsigned take_sound(){const auto s=sound_;sound_=0xffffffffu;return s;}
    bool take_save_complete(){const bool complete=save_complete_;save_complete_=false;return complete;}
    void input(Key key);
    // One screen pass: held-pad repeat for Files, pressed directions for choices.
    // special_save is bhCkFlg(sys->ev_flg,56), read by ExecuteStateSaveScreenExit
    // (ps2_SaveScreen.c:538) each pass; the load screen never passes it.
    void control(unsigned pad_on,unsigned pad_ps,bool special_save=false);
    void reset();
private:
    void move(int step);
    void choose(unsigned pressed);
    unsigned key_wait0_=0,key_wait1_=0,timer_=0,selection_=0;
    unsigned top_=0,write_count_=0,drawn_count_=0,write_max_=0,sound_=0xffffffffu;
    int write_timer_=6;
    bool active_=false,done_=false,confirmed_=false,cancelled_=false,io_ready_=false,save_complete_=false;
    Mode mode_=Mode::Save;Phase phase_=Phase::Files;unsigned cursor_=0;
    std::vector<SaveSlotInfo> slots_;
};
}
