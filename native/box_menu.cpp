#include "box_menu.h"
#include "audio.h"
#include <algorithm>
namespace recvx {
void BoxMenu::synchronize(StatusMenu& menu,EventVm* events){if(!events){reset();return;}if(active_&&!menu.active){reset();return;}if(!active_&&menu.active&&events->flags[10][0]&0x1c0000){active_=true;selecting_=false;index_=0;base_=events->flags[10][0]&0x80000?192:events->flags[10][0]&0x100000?224:64;menu.commands=menu.examining=menu.combining=menu.files=false;menu.selected=2;}}
bool BoxMenu::input(MenuInput key,Inventory& inventory,StatusMenu& menu,EventVm* events,AudioMixer& audio){
    if(!active_)return false;
    if(menu.message.active()){menu.input(key,inventory);if(menu.feedback_sound!=0xffffffff)audio.play_system(menu.feedback_sound);return true;}
    if(key==MenuInput::Open||(key==MenuInput::Cancel&&!selecting_)){menu.reset(inventory);reset();if(events){events->menu_open=false;events->flags[10][0]&=~0x1c0000u;events->flags[12][0]|=0x200u;}audio.play_system(0);return true;}
    if(key==MenuInput::Cancel){selecting_=false;audio.play_system(0);return true;}
    if(key==MenuInput::Confirm){if(!selecting_){selecting_=true;audio.play_system(3);}else{unsigned message=0;const bool okay=inventory.box_change(menu.selected,index_,base_,menu.stage,menu.room,&message);audio.play_system(okay?3:1);if(message)menu.show_message(message);if(okay)selecting_=false;}return true;}
    if(selecting_){const auto count=base_==64?128:32;const int delta=key==MenuInput::Up?-1:key==MenuInput::Down?1:key==MenuInput::Left?-5:key==MenuInput::Right?5:0;if(delta){index_=(index_+count+delta)%count;audio.play_system(2);}}
    else{menu.input(key,inventory);if(menu.feedback_sound!=0xffffffff)audio.play_system(menu.feedback_sound);}return true;
}
void BoxMenu::draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& inventory,const StatusMenu& menu)const{
    if(!active_||menu.message.active())return;auto& pixels=image.canvas();for(int y=40;y<438;++y)for(int x=28;x<436;++x)pixels[y*image.width()+x]=0xff0d1518;
    ui.label(image,base_==64?"ITEM BOX":"SECURITY BOX",44,48,.65f);const unsigned count=base_==64?128:32;
    for(unsigned row=0;row<6;++row){const auto index=(index_+count+row-2)%count,item=inventory.items[base_+index];const bool selected=selecting_&&row==2;
        if(selected)for(int y=85+int(row)*38;y<119+int(row)*38;++y)for(int x=40;x<423;++x)pixels[y*image.width()+x]=0xff344949;
        ui.label(image,(selected?"> ":"  ")+std::to_string(index+1)+"  "+(Inventory::id(item)?ui.item_name(Inventory::id(item)):"---"),44,92+int(row)*38,.5f);
        if(Inventory::id(item)&&Inventory::quantity(item)>1)ui.label(image,std::to_string(Inventory::quantity(item)),377,92+int(row)*38,.5f);
    }
    // The existing original inventory pane remains on the right. The left
    // text list is a host view until ItemBox sprites/window motion are ported.
    ui.label(image,selecting_?"OK: exchange   Back: inventory":"Choose inventory item, then OK",40,376,.48f);
    (void)menu;
}
}
