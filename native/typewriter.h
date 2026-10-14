#pragma once
#include "save_slots.h"
#include <filesystem>
#include <vector>
namespace recvx {
// ControlTypewriter (bup_00.c:52) als Plattformanpassung: die Originalaufgabe
// 15 (system.c:1224 bhSysCallTypewriter) laeuft, sobald der Raum das Bit 15 von
// sys->ts_flg loescht, und zeigt den Speicher-/Ladebildschirm mit der
// Slotauswahl. typ_md0 laeuft Init(0) -> Main(1) -> Exit(2); hier ohne
// Textur-/Streaming-Schritte, die Slotwahl und die Memory-Card-Schnittstelle
// (Host-Datei je Slot, save_slots.h) bleiben erhalten.
class Typewriter {
public:
    enum class Mode {Save,Load};
    enum class Key {Up,Down,Confirm,Cancel};
    void open(Mode mode,const std::filesystem::path& saves_dir);
    bool active()const{return active_;}
    Mode mode()const{return mode_;}
    const std::vector<SaveSlotInfo>& slots()const{return slots_;}
    unsigned cursor()const{return cursor_;}
    // usExitFlag: der Bildschirm ist verlassen (bestaetigt oder abgebrochen).
    bool done()const{return done_;}
    bool confirmed()const{return confirmed_;}
    bool cancelled()const{return cancelled_;}
    unsigned slot()const{return slots_.empty()?0:slots_[cursor_].slot;} // 1..kSaveSlots
    void input(Key key);
    void reset();
private:
    bool active_=false,done_=false,confirmed_=false,cancelled_=false;
    Mode mode_=Mode::Save;unsigned cursor_=0;
    std::vector<SaveSlotInfo> slots_;
};
}
