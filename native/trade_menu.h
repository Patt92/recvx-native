#pragma once
#include "ui.h"
#include "input.h"
#include "world.h"
namespace recvx {
// Optional host window. Stores cursor state only; inventories remain with their
// existing session owners, and RoomWorld validates every transaction.
class TradeMenu {
public:
    bool active()const{return active_;}
    bool actions()const{return actions_;}
    bool selecting()const{return selecting_;}
    unsigned pane()const{return pane_;}
    unsigned cell(unsigned pane)const{return cells_.at(pane);}
    unsigned source_pane()const{return source_pane_;}
    unsigned source_cell()const{return source_cell_;}
    const std::string& note()const{return note_;}
    bool open(const RoomWorld& world);
    void close(){active_=selecting_=actions_=false;changed_=true;}
    void reset(){active_=selecting_=actions_=changed_=false;cells_={0,0};pane_=source_pane_=source_cell_=0;source_slot_=2;sound_=~0u;action_cursor_=0;message_=0;note_.clear();}
    bool take_changed(){const bool value=changed_;changed_=false;return value;}
    unsigned take_sound(){const auto value=sound_;sound_=~0u;return value;}
    void item_actions(Inventory& primary,RoomWorld& world);
    void input(MenuInput input,Inventory& primary,RoomWorld& world);
    void apply(const InputFrame& frame,Inventory& primary,RoomWorld& world);
    void draw(SoftwareRenderer& image,const GameUi& ui,const Inventory& primary,const RoomWorld& world)const;
    static unsigned slot(const Inventory& inventory,unsigned cell);
private:
    bool active_=false,selecting_=false,changed_=false,actions_=false;
    unsigned message_=0,action_cursor_=0;
    static bool reload_option(const Inventory& inventory,unsigned slot){return inventory.slots()[0]==slot&&inventory.weapon()>=3;}
    unsigned pane_=0,source_pane_=0,source_cell_=0,source_slot_=2,sound_=~0u;
    std::array<unsigned,2> cells_{};
    std::string note_;
};
}
