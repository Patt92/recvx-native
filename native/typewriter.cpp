#include "typewriter.h"
#include <algorithm>
#include <stdexcept>
namespace recvx {
void Typewriter::open(Mode mode,const std::filesystem::path& saves_dir){
    reset();mode_=mode;slots_=list_save_slots(saves_dir);active_=true;
    // Ohne Memory-Karten-Auswahl (Plattformanpassung) startet der Bildschirm
    // direkt im SelectFile nach dem Verzeichnis-Scan. Beide Originalpfade setzen
    // lSelectFileNumber auf null, nicht auf den ersten freien/belegten Slot.
    // SaveScreen:620,691–712,1080; LoadScreen:345,497,569–590,659.
}
unsigned Typewriter::message()const{
    switch(phase_){
    case Phase::Confirm:return mode_==Mode::Load?459:(slots_[cursor_].exists||slots_[cursor_].damaged)?483:482;
    case Phase::NoSave:return 458;
    case Phase::Io:case Phase::Typing:return mode_==Mode::Save?484:460;
    case Phase::Success:return 485;
    case Phase::Error:return mode_==Mode::Save?486:461;
    case Phase::SpecialSave:return 253; // DispMessageSelect case 8, ps2_SaveScreen.c:116
    default:return 0;
    }
}
void Typewriter::move(int step){
    // mcMoveFileSelectWindowCursor (McSaveFile:596–622) plus SelectFile's
    // five-line cursor clamp. Scroll one record, wrap 0 <-> 14 / top 0 <-> 10.
    const int next=int(cursor_)+step;
    if(next<0){cursor_=14;top_=10;}
    else if(next>14){cursor_=0;top_=0;}
    else{cursor_=unsigned(next);if(cursor_<top_)--top_;else if(cursor_>=top_+5)++top_;}
    sound_=2;
}
void Typewriter::choose(unsigned pressed){
    if(phase_==Phase::Files){
        if(pressed&0x800u){
            if(mode_==Mode::Load&&slots_[cursor_].damaged){phase_=Phase::Error;sound_=1;}
            else if(mode_==Mode::Load&&!slots_[cursor_].exists){phase_=Phase::NoSave;sound_=1;}
            else{phase_=Phase::Confirm;selection_=mode_==Mode::Save&&(slots_[cursor_].exists||slots_[cursor_].damaged)?1:0;sound_=3;}
        }else if(pressed&0x1000u){phase_=Phase::Exit;cancelled_=true;timer_=1;sound_=0;}
        return;
    }
    if(phase_==Phase::NoSave){if(pressed&0x1800u){phase_=Phase::Files;sound_=3;}return;}
    if(phase_==Phase::Error){if(pressed&0x1800u){phase_=Phase::Files;sound_=3;}return;}
    if(phase_==Phase::SpecialSave){
        // ExecuteStateSaveScreenSpecialSave (ps2_SaveScreen.c:563–604): OK on 1 or
        // Cancel returns to the file list; OK on 0 sets usExitFlag directly, so
        // the Exit state does not re-read the still-set ev_flg 56.
        if(pressed&3u){selection_^=1;sound_=2;}
        else if(pressed&0x800u){sound_=3;if(selection_){phase_=Phase::Files;cursor_=top_=0;}else done_=true;}
        else if(pressed&0x1000u){sound_=0;phase_=Phase::Files;cursor_=top_=0;}
        return;
    }
    if(phase_!=Phase::Confirm)return;
    if(pressed&3u){selection_^=1;sound_=2;return;}
    if(pressed&0x800u){
        sound_=3;
        if(selection_)phase_=Phase::Files;
        else{phase_=Phase::Io;io_ready_=false;}
    }else if(pressed&0x1000u){
        sound_=0;
        phase_=Phase::Files;
    }
}
void Typewriter::input(Key key){
    if(!active_||done_)return;
    if(phase_==Phase::Files&&(key==Key::Up||key==Key::Down))move(key==Key::Up?-1:1);
    else choose(key==Key::Up?1u:key==Key::Down?2u:key==Key::Confirm?0x800u:0x1000u);
}
void Typewriter::control(unsigned pad_on,unsigned pad_ps,bool special_save){
    if(!active_||done_)return;
    switch(phase_){
    case Phase::Files:
        if(!(pad_ps&0x1800u)){
            if(pad_on&0xfu){if(key_wait0_>4)++key_wait1_;else ++key_wait0_;}
            else key_wait0_=key_wait1_=0;
            if(key_wait1_>=3||key_wait0_==1){key_wait1_=0;if(pad_on&1u)move(-1);else if(pad_on&2u)move(1);}
        }else choose(pad_ps);
        timer_=(timer_+1)%60;
        break;
    case Phase::Io:io_ready_=true;break;
    case Phase::Typing:
        // Original drawing advances the string once per pass; no host-I/O
        // latency is invented. McSaveFile:571–586,847–860; SaveScreen:1402–1426.
        if(write_count_>write_max_){save_complete_=true;phase_=Phase::Success;timer_=20;write_count_=0;write_timer_=-6;}
        else{
            drawn_count_=write_count_;
            if(write_timer_%6==0){
                const unsigned player=slots_[cursor_].player;
                if(write_count_>2&&write_count_!=10-player&&write_count_!=14-player)
                    sound_=(write_count_==9-player||write_count_==13-player)?0x80000243u:0x80000242u;
                ++write_count_;
            }else if(write_count_>=write_max_){sound_=0x80000244u;++write_count_;}
            ++write_timer_;
        }
        break;
    case Phase::Success:if(timer_)--timer_;else{phase_=Phase::Exit;timer_=1;}break;
    case Phase::Exit:
        // ExecuteStateSaveScreenExit (ps2_SaveScreen.c:536–549): with ev_flg 56 set
        // the special-save question opens (sSelectCur 1) instead of leaving.
        if(special_save){phase_=Phase::SpecialSave;selection_=1;break;}
        if(timer_&&!--timer_)done_=true;break;
    default:choose(pad_ps);break;
    }
}
void Typewriter::io_complete(bool success,const std::filesystem::path& saves_dir){
    if(!io_pending())throw std::logic_error("typewriter I/O completion without request");
    io_ready_=false;
    if(!success){phase_=Phase::Error;return;}
    if(mode_==Mode::Load){confirmed_=true;phase_=Phase::Exit;timer_=1;return;}
    slots_=list_save_slots(saves_dir);
    const auto& s=slots_[cursor_];
    // Type_Space, ps2_McSaveFile.c:13–17 (original data, not a timed approximation).
    constexpr unsigned lengths[]={28,29,29,28,28,29,29,28,28,29,27,30,28,29,29,29,29,28};
    if(!s.exists||s.save_room<1||s.save_room>18||s.player>1)
        throw std::runtime_error("typewriter: original Type_Space requires a known save room and Claire/Chris metadata");
    write_max_=lengths[s.save_room-1]-s.player;write_count_=drawn_count_=0;write_timer_=6;
    confirmed_=true;phase_=Phase::Typing;
}
void Typewriter::reset(){
    active_=done_=confirmed_=cancelled_=io_ready_=save_complete_=false;slots_.clear();cursor_=top_=selection_=timer_=write_count_=drawn_count_=write_max_=0;
    key_wait0_=key_wait1_=0;write_timer_=6;sound_=0xffffffffu;phase_=Phase::Files;
}
}
