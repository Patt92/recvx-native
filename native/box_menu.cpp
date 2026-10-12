#include "box_menu.h"
#include "audio.h"
#include <algorithm>
namespace recvx {
void BoxMenu::synchronize(Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer* audio){
    if(!events){reset();return;}
    if(active_&&!menu.active){reset();return;}
    if(!active_&&menu.active&&events->flags[10][0]&0x1c0000){
        active_=true;closing_=false;selecting_=false;ips1_=ipsdisp_=0;number_=flag_=0;wait_=keywait_=0;boxwait_=4;row_shift_=0.f;arrows_=0;
        base_=events->flags[10][0]&0x80000?192:events->flags[10][0]&0x100000?224:64;
        menu.commands=menu.examining=menu.combining=menu.files=false;menu.selected=2;
        menu.skip_slide();slide_.open(true); // ItemBoxInit: statusflg 0x2000, sprites slide in
        return;
    }
    if(!active_)return;
    // SpriteH ends the opening with SE 7 (CallSystemSe(1,7)); the closing end only hands control back.
    if(exit_pressed_){if(exit_wait_)--exit_wait_;else{exit_pressed_=false;exit_=false;slide_.cancel();closing_=true;}} // ItemBox testmode 3: StatusCancel after the press
    const bool closed=slide_.tick();
    if(const auto sound=slide_.take_sound();sound!=0xffffffff&&audio)audio->play_system(sound);
    // sub1.c ItemBox tail (7688-7751) with ItemBoxIconSet's persistent parts_20b
    // mutation: while testmode 2 a pending one-row scroll displaces the rows by
    // `number` (+-19) each frame; once `wait` passes 38/|number| the direction
    // flag adds the full 38 px in the same frame and the box index advances.
    // st->flag is cleared at the top of every ItemBox pass, so the shift lives
    // exactly one frame. SE 2 per completed step (CallSystemSe(0,2)).
    if(active_&&selecting_){
        flag_=0;
        if(number_){
            ++wait_;
            const unsigned tmp=unsigned(38/std::abs(number_));
            if(wait_>tmp){
                if(number_>0){--ips1_;flag_=1;}else{++ips1_;flag_=2;}
                if(audio)audio->play_system(2);
                ips1_&=base_==64?0x7Fu:0x1Fu;ipsdisp_=ips1_;
                number_=0;wait_=0;
            }
        }
        row_shift_+=float(number_);                     // ItemBoxIconSet: pos[1] += number
        if(flag_==1)row_shift_-=38.f;else if(flag_==2)row_shift_+=38.f;
        arrows_=number_>0?1u:number_<0?2u:keywait_?arrows_:0u;
    }
    if(closed&&closing_){ // SpriteH set statusflg 0x80000: hand control back to the script
        menu.reset(inventory);reset();events->menu_open=false;events->flags[10][0]&=~0x1c0000u;events->flags[12][0]|=0x200u;
        // ItemTaskCheck: bhStandPlayerMotion clears plp->stflg 0x18000 and the pad lock; the next
        // bhControlPlayerPad clears st_flg 0x4 set by bhCheckExmAtari for the lid.
        events->flags[14][0]&=~0x18000u;events->flags[5][0]&=~4u;
    }
}
bool BoxMenu::input(MenuInput key,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio){
    if(!active_)return false;
    if(menu.message.active()){menu.input(key,inventory);if(menu.feedback_sound!=0xffffffff)audio.play_system(menu.feedback_sound);return true;}
    if(slide_.moving()||exit_pressed_)return true; // statusflg 0x40 / pressed EXIT: no pad input
    if(key==MenuInput::Open){slide_.cancel();closing_=true;audio.play_system(0);return true;} // StatusCancel + SE 0
    // ItemBox testmode 3: the cursor rests on the EXIT button.
    if(exit_){
        if(key==MenuInput::Down||key==MenuInput::Right){exit_=false;menu.selected=2;audio.play_system(2);}
        else if(key==MenuInput::Cancel){exit_=false;menu.selected=2;audio.play_system(0);}
        else if(key==MenuInput::Confirm){exit_pressed_=true;exit_wait_=4;audio.play_system(0);}
        return true;
    }
    // testmode 1 cancel: StatusCancel; testmode 2 cancel: back to the item cursor. Both clear the refusal text.
    // sub1.c ItemBox testmode 2 gates Confirm and Cancel behind `st->wait == 0`
    // (7606): while the one-row scroll animation is pending, both presses are
    // ignored and only the tail advances the box index.
    if(selecting_&&wait_)return true;
    if(key==MenuInput::Cancel){note_=0;if(!selecting_){slide_.cancel();closing_=true;}else selecting_=false;audio.play_system(0);return true;}
    if(key==MenuInput::Confirm){
        if(!selecting_){selecting_=true;note_=0;number_=0;wait_=0;keywait_=0;boxwait_=4;arrows_=0;audio.play_system(3);}
        else{unsigned message=0;inventory.box_change(menu.selected,ips1_,base_,menu.stage,menu.room,&message);audio.play_system(3);
            // ItemBoxChange clears statusflg 0x100000 (stay in the list) when it refuses and sets a bhSetMessage text.
            note_=message;if(!message)selecting_=false;}
        return true;
    }
    if(selecting_){
        // sub1.c ItemBox testmode 2 pad block (7455-7604). Up/Down (0x1/0x2)
        // request the one-row displacement; the left/right shoulder scroll
        // (0x80/0x10) moves five entries and repeats with keywait>10 then
        // boxwait>3. The host delivers a held direction as repeated MenuInput
        // events (input.hpp repeat_delay/repeat_interval), so the counters
        // advance per event instead of per held frame; every other key resets
        // them like `sys->pad_on == 0` (7688).
        const auto count=base_==64?128:32;
        if(key==MenuInput::Up||key==MenuInput::Down){
            const int direction=key==MenuInput::Up?1:-1;   // 0x1 up, 0x2 down
            if(!number_)number_=19*direction;
            arrows_=direction>0?1u:2u;keywait_=0;boxwait_=4;note_=0;
            return true;
        }
        if(key==MenuInput::Left||key==MenuInput::Right){
            const int direction=key==MenuInput::Right?5:-5; // 0x10 down, 0x80 up
            if(keywait_>10){++boxwait_;if(boxwait_>3){boxwait_=0;ips1_=(ips1_+count+direction)%count;ipsdisp_=ips1_;note_=0;audio.play_system(2);}}
            else{++keywait_;if(keywait_==1){ips1_=(ips1_+count+direction)%count;ipsdisp_=ips1_;note_=0;audio.play_system(2);}}
            arrows_=direction>0?2u:1u;
            return true;
        }
        keywait_=0;boxwait_=4;
        return true;
    }
    // Item cursor (CursorMove-like, listcsr_2 = cell). The status bar is not reachable here: Up leaves through EXIT.
    unsigned cell=1;if(menu.selected>1){cell=2;for(unsigned i=2;i<menu.selected;++i){const auto id=Inventory::id(inventory.slots()[i]);cell+=(id&&(itemdata[id].type&0x100))?2:1;}}
    const bool security=base_!=64;
    if(key==MenuInput::Up&&(cell==1||cell<4)){
        if(security&&cell!=1){menu.selected=1;audio.play_system(2);}   // listcsr_0 = listcsr_2 = 1
        else{exit_=true;audio.play_system(0);}                          // testmode 3
        note_=0;return true;
    }
    if(key==MenuInput::Left&&cell<=2)return true;
    const auto before=menu.selected;menu.input(key,inventory);if(menu.selected!=before)note_=0;
    if(menu.feedback_sound!=0xffffffff)audio.play_system(menu.feedback_sound);
    return true;
}
void BoxMenu::draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& inventory,const StatusMenu& menu)const{
    if(active_)ui.draw_box(image,inventory,menu,base_,ips1_,selecting_,&slide_,exit_?(exit_pressed_?2:1):0,note_,row_shift_,arrows());
}
}
