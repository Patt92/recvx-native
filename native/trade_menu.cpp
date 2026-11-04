#include "trade_menu.h"
#include <algorithm>
namespace recvx {
unsigned TradeMenu::slot(const Inventory& inventory,unsigned cell){
    unsigned at=0;
    for(unsigned slot=2;slot<inventory.capacity()+2;++slot){const auto id=Inventory::id(inventory.slots()[slot]);const unsigned width=id<156&&(itemdata[id].type&0x100u)?2u:1u;
        if(cell<at+width)return slot;at+=width;}
    return inventory.capacity()+1;
}
bool TradeMenu::open(const RoomWorld& world){
    if(!world.player2_trade_ready())return false;
    active_=changed_=true;selecting_=actions_=false;action_cursor_=0;message_=0;pane_=0;cells_={0,0};note_.clear();sound_=3;return true;
}
void TradeMenu::apply(const InputFrame& frame,Inventory& primary,RoomWorld& world){
    if(!active_)return;
    if(!world.player2_trade_ready()){close();return;}
    if(frame.hit(Action::Trade)||frame.hit(Action::Inventory)){close();sound_=0;return;}
    if(frame.hit(Action::Aim)){item_actions(primary,world);return;}
    if(frame.hit(Action::Cancel)){input(MenuInput::Cancel,primary,world);return;}
    if(frame.hit(Action::Confirm)){input(MenuInput::Confirm,primary,world);return;}
    for(const auto [action,key]:{std::pair{Action::Up,MenuInput::Up},std::pair{Action::Down,MenuInput::Down},std::pair{Action::Left,MenuInput::Left},std::pair{Action::Right,MenuInput::Right}})
        if(frame.navigation(action)){input(key,primary,world);break;}
}
void TradeMenu::item_actions(Inventory&,RoomWorld& world){
    message_=0;
    if(!active_||!world.player2_trade_ready()){if(active_)close();return;}
    if(actions_){actions_=false;note_.clear();message_=0;sound_=0;return;}
    if(selecting_||pane_!=1){note_="Select a partner item first.";sound_=1;return;}
    const auto& equipment=world.player2_progress()->equipment;
    const auto id=Inventory::id(equipment.slots()[slot(equipment,cells_[1])]);
    if(!id||id>=156||(!(itemdata[id].type&1u)&&(itemdata[id].type&0x5fu)!=8u)){note_="Select a partner weapon or healing item.";sound_=1;return;}
    actions_=true;action_cursor_=0;message_=0;note_.clear();sound_=3;
}
void TradeMenu::input(MenuInput key,Inventory& primary,RoomWorld& world){
    if(!active_)return;
    if(!world.player2_trade_ready()){close();return;}
    const auto* progress=world.player2_progress();
    const Inventory* inventories[]={&primary,&progress->equipment};
    if(key==MenuInput::Open){close();sound_=0;return;}
    if(actions_){
        if(key==MenuInput::Cancel){actions_=false;note_.clear();sound_=0;return;}
        const auto at=slot(*inventories[1],cells_[1]);const auto id=Inventory::id(inventories[1]->slots()[at]);
        const bool reload=reload_option(*inventories[1],at);const unsigned count=reload?3u:2u;
        if(key==MenuInput::Up||key==MenuInput::Down){action_cursor_=(action_cursor_+count+(key==MenuInput::Down?1:-1))%count;sound_=2;return;}
        if(key!=MenuInput::Confirm)return;
        actions_=false;if(action_cursor_==count-1){sound_=0;return;}
        if(reload&&action_cursor_==1){const bool changed=world.reload_player2_item(at,primary.game_mode);sound_=changed?3:1;note_=changed?"Partner weapon reloaded.":"No matching ammunition or magazine full.";return;}
        if(id&&id<156&&(itemdata[id].type&1u)){
            const bool equipped=inventories[1]->slots()[0]==at;
            const bool changed=world.equip_player2_item(at);sound_=changed?3:1;note_=changed?(equipped?"Partner weapon unequipped.":"Partner weapon equipped."):"Partner item unavailable.";
        }else{
            const auto result=world.use_player2_item(at,primary.game_mode);sound_=result&&result->pulse?3:1;
            if(result){message_=result->message;note_=result->pulse?"Partner healing item used.":"This item cannot be used now.";}else note_="Partner item unavailable.";
        }
        return;
    }
    message_=0;
    if(key==MenuInput::Cancel){if(selecting_){selecting_=false;pane_=source_pane_;cells_[pane_]=source_cell_;note_.clear();}else close();sound_=0;return;}
    if(key==MenuInput::Confirm){
        if(!selecting_){source_pane_=pane_;source_cell_=cells_[pane_];source_slot_=slot(*inventories[pane_],source_cell_);selecting_=true;pane_=1-pane_;note_.clear();sound_=3;return;}
        const auto result=world.trade_player2(primary,source_pane_!=0,source_slot_,slot(*inventories[pane_],cells_[pane_]));
        sound_=result.changed()?3u:1u;
        switch(result.code){
        case Inventory::TradeResult::Transferred:note_="Items transferred.";selecting_=false;break;
        case Inventory::TradeResult::Empty:note_="Both slots are empty.";break;
        case Inventory::TradeResult::NoSpace:note_="Not enough space.";break;
        case Inventory::TradeResult::Restricted:note_="This item cannot be traded.";break;
        case Inventory::TradeResult::Unavailable:close();break;
        default:note_="Exchange refused. Items unchanged.";break;
        }
        return;
    }
    auto& cell=cells_[pane_];const auto max=inventories[pane_]->capacity();
    if(key==MenuInput::Up){if(cell>=2)cell-=2;}
    else if(key==MenuInput::Down){if(cell+2<max)cell+=2;}
    else if(key==MenuInput::Right){if(!(cell&1u))++cell;else if(!selecting_){pane_=1-pane_;cells_[pane_]=std::min(cell-1,inventories[pane_]->capacity()-1);}}
    else if(key==MenuInput::Left){if(cell&1u)--cell;else if(!selecting_){pane_=1-pane_;cells_[pane_]=std::min(cell+1,inventories[pane_]->capacity()-1);}}
    note_.clear();sound_=2;
}
void TradeMenu::draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& primary,const RoomWorld& world)const{
    if(active_&&world.player2_progress())ui.draw_trade(image,primary,world.player2_progress()->equipment,world.player2_mode()==Player2Mode::Npc,pane_,cells_,selecting_,source_pane_,source_cell_,message_?std::string(" "):actions_?std::string("OK: confirm action    Back: cancel"):note_);
    if(!active_)return;
    if(message_){auto text=message_text(ui.system_message(message_));while(!text.empty()&&ui.label_width(text,.43f)>584)text.pop_back();ui.label(image,text,26,426,.43f,0xffd4dba8,.43f);}
    if(actions_&&world.player2_progress()){
        const auto& equipment=world.player2_progress()->equipment;const auto at=slot(equipment,cells_[1]);const auto id=Inventory::id(equipment.slots()[at]);
        const std::string command=id<156&&(itemdata[id].type&1u)?(equipment.slots()[0]==at?"UNEQUIP":"EQUIP"):"USE";
        const int x=std::max(0,(image.width()-640)/2)+350;
        const bool reload=reload_option(equipment,at);const unsigned count=reload?3u:2u;
        for(int y=200;y<200+int(count)*37+18;++y)for(int dx=0;dx<234;++dx)image.canvas()[y*image.width()+x+dx]=0xff16262d;
        for(unsigned i=0;i<count;++i){const auto label=i==0?command:i==count-1?std::string("BACK"):std::string("RELOAD");ui.label(image,label,x+18,215+int(i)*37,.7f,action_cursor_==i?0xffe5edbc:0xff718c92,.7f);}
    }
}
}
