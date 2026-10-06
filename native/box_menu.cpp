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
        slide_.open(); // ItemBoxInit: statusflg 0x2000, sprites slide in
        return;
    }
    if(!active_)return;
    // SpriteH ends the opening with SE 7 (CallSystemSe(1,7)); the closing end only hands control back.
    const auto event=slide_.tick();
    if(event==BoxSlide::Event::Opened&&audio)audio->play_system(7);
    if(event==BoxSlide::Event::Closed&&closing_){ // SpriteH set statusflg 0x80000: hand control back to the script
        menu.reset(inventory);reset();events->menu_open=false;events->flags[10][0]&=~0x1c0000u;events->flags[12][0]|=0x200u;
    }
}
bool BoxMenu::input(MenuInput key,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio){
    if(!active_)return false;
    if(menu.message.active()){menu.input(key,inventory);if(menu.feedback_sound!=0xffffffff)audio.play_system(menu.feedback_sound);return true;}
    if(slide_.moving())return true; // statusflg 0x40: no pad input while the sprites slide
    if(key==MenuInput::Open||(key==MenuInput::Cancel&&!selecting_)){slide_.cancel();closing_=true;audio.play_system(0);return true;} // StatusCancel + SE 0
    if(key==MenuInput::Cancel){selecting_=false;audio.play_system(0);return true;}
    if(key==MenuInput::Confirm){if(!selecting_){selecting_=true;audio.play_system(3);}else{unsigned message=0;const bool okay=inventory.box_change(menu.selected,index_,base_,menu.stage,menu.room,&message);audio.play_system(okay?3:1);if(message)menu.show_message(message);if(okay)selecting_=false;}return true;}
    if(selecting_){const auto count=base_==64?128:32;const int delta=key==MenuInput::Up?-1:key==MenuInput::Down?1:key==MenuInput::Left?-5:key==MenuInput::Right?5:0;if(delta){index_=(index_+count+delta)%count;audio.play_system(2);}}
    else{menu.input(key,inventory);if(menu.feedback_sound!=0xffffffff)audio.play_system(menu.feedback_sound);}return true;
}
void BoxMenu::draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& inventory,const StatusMenu& menu)const{
    if(active_)ui.draw_box(image,inventory,menu,base_,index_,selecting_,&slide_);
}
}
