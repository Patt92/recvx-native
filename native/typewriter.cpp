#include "typewriter.h"
namespace recvx {
void Typewriter::open(Mode mode,const std::filesystem::path& saves_dir){
    mode_=mode;slots_=list_save_slots(saves_dir);cursor_=0;
    // Der Original-Speicherbildschirm stellt den Cursor auf den ersten freien
    // Platz, der Ladebildschirm auf den ersten belegten; ohne den jeweiligen
    // Eintrag bleibt er auf dem ersten Slot.
    for(unsigned i=0;i<slots_.size();++i)if((mode==Mode::Save&&!slots_[i].exists)||(mode==Mode::Load&&slots_[i].exists)){cursor_=i;break;}
    active_=true;done_=confirmed_=cancelled_=false;
}
void Typewriter::input(Key key){
    if(!active_||done_||slots_.empty())return;
    switch(key){
    case Key::Up:cursor_=(cursor_+slots_.size()-1)%slots_.size();break;
    case Key::Down:cursor_=(cursor_+1)%slots_.size();break;
    case Key::Confirm:
        // Laden nur aus einem belegten Slot; Speichern in jeden Slot.
        if(mode_==Mode::Load&&!slots_[cursor_].exists)return;
        confirmed_=true;done_=true;break;
    case Key::Cancel:cancelled_=true;done_=true;break;
    }
}
void Typewriter::reset(){active_=false;done_=confirmed_=cancelled_=false;slots_.clear();cursor_=0;}
}
