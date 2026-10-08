#include "box_menu.h"
#include "audio.h"
#include <algorithm>
namespace recvx {
void BoxMenu::synchronize(Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer* audio){
    if(!events){reset();return;}
    if(active_&&!menu.active){reset();return;}
    if(!active_&&menu.active&&events->flags[10][0]&0x1c0000){
        active_=true;closing_=false;selecting_=false;index_=0;base_=events->flags[10][0]&0x80000?192:events->flags[10][0]&0x100000?224:64;
        menu.commands=menu.examining=menu.combining=menu.files=false;menu.selected=2;
        menu.skip_slide();slide_.open(true); // ItemBoxInit: statusflg 0x2000, sprites slide in
        return;
    }
    if(!active_)return;
    // SpriteH ends the opening with SE 7 (CallSystemSe(1,7)); the closing end only hands control back.
    if(exit_pressed_){if(exit_wait_)--exit_wait_;else{exit_pressed_=false;exit_=false;slide_.cancel();closing_=true;}} // ItemBox testmode 3: StatusCancel after the press
    const bool closed=slide_.tick();
    if(const auto sound=slide_.take_sound();sound!=0xffffffff&&audio)audio->play_system(sound);
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
    if(key==MenuInput::Cancel){note_=0;if(!selecting_){slide_.cancel();closing_=true;}else selecting_=false;audio.play_system(0);return true;}
    if(key==MenuInput::Confirm){
        if(!selecting_){selecting_=true;note_=0;audio.play_system(3);}
        else{unsigned message=0;inventory.box_change(menu.selected,index_,base_,menu.stage,menu.room,&message);audio.play_system(3);
            // ItemBoxChange clears statusflg 0x100000 (stay in the list) when it refuses and sets a bhSetMessage text.
            note_=message;if(!message)selecting_=false;}
        return true;
    }
    if(selecting_){const auto count=base_==64?128:32;const int delta=key==MenuInput::Up?-1:key==MenuInput::Down?1:key==MenuInput::Left?-5:key==MenuInput::Right?5:0;if(delta){index_=(index_+count+delta)%count;note_=0;audio.play_system(2);}return true;}
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
    if(active_)ui.draw_box(image,inventory,menu,base_,index_,selecting_,&slide_,exit_?(exit_pressed_?2:1):0,note_);
}
}
