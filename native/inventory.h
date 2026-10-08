#pragma once
#include "item_tables.h"
#include <array>
#include <cstdint>
#include <string>
namespace recvx {
// sub1.c ItemUse: StatusCancel request, bhSetMessage index, return value
// (useon), Use_00's PulseInit + pulseflg bits and the resulting st->hp.
struct ItemUseResult {bool closing=false;unsigned message=0,useon=0,pulse=0,condition=0;};
struct PickupResult {unsigned mode=0,message=0;bool granted=false,used=false;unsigned pulse=0;};
class Inventory {
public:
    Inventory();
    std::array<std::uint32_t,384> items{};
    unsigned player=0,game_mode=0;
    int hp=160,max_hp=160;
    std::uint32_t play_time=0; // sync.c stv_tm, 60-Hz VSync units, room-independent.
    bool poisoned=false,side_pack=false,infected=false;
    std::int16_t spray_count=0; // sys->spray_ct, raised by Use_00 for item 20
    // plp->stflg bits held by the inventory: 0x80000 poison, 0x200000 poison2.
    unsigned player_stflg()const{return (poisoned?0x80000u:0u)|(infected?0x200000u:0u);}
    void set_player_stflg(unsigned stflg){poisoned=stflg&0x80000u;infected=stflg&0x200000u;}
    std::uint32_t* slots(){return items.data()+player*16;}
    const std::uint32_t* slots()const{return items.data()+player*16;}
    unsigned capacity()const{return side_pack?10:8;}
    static unsigned id(std::uint32_t item){return (item>>16)&255;}
    static unsigned quantity(std::uint32_t item){return item&65535;}
    static std::uint32_t pack(unsigned id,unsigned quantity){return (id<<16)|quantity;}
    unsigned occupied()const;
    bool contains(unsigned item)const;
    unsigned weapon()const;
    bool equip(unsigned slot);
    bool reload(); // sub1.c bhSearchBullet / Combi_99 / ItemSort.
    bool count_bullet(); // weapon.c bhCountBullet; signed 16-bit count, preserves ID/flags.
    bool acquire(unsigned item,unsigned quantity,unsigned flags=0);
    // Host debug catalog: 256 + file index grants itm[383], normal IDs grant
    // items. File-only room models cannot be injected into inventory slots.
    bool debug_acquire(unsigned choice,unsigned quantity);
    // Compatibility for the old host's duplicate file pickups, only when an
    // associated file is already owned. Never invents file ownership.
    unsigned repair_owned_file_items();
    void erase_read_manual(bool automatic); // FileExit01 -> ItemSearch/EraseItem -> ItemSort
    // sub1.c GetItem: mode 0 begins, modes 1/4 consume a menu answer 100/101.
    PickupResult pickup(unsigned item,unsigned& control,unsigned stage,unsigned room,unsigned mode=0,int answer=-1);
    // Original ItemUse for every item type. `parameters` is the active floor
    // trigger (cb_flg 0x200), `condition` the status screen's st->hp/st->abc.
    ItemUseResult use_item(unsigned slot,const std::array<unsigned,4>* parameters,unsigned& control,unsigned stage,unsigned room,unsigned condition,bool message_active=false);
    int combination_rule(unsigned first,unsigned second)const;
    bool combine(unsigned first,unsigned second,unsigned* message=nullptr,unsigned* result_slot=nullptr);
    bool transform(unsigned slot,unsigned expected,unsigned replacement);
    bool deposit(unsigned slot,unsigned box=64);
    bool withdraw(unsigned index,unsigned box=64);
    bool box_change(unsigned slot,unsigned index,unsigned box,unsigned stage,unsigned room,unsigned* message=nullptr);
    std::string condition()const;
private:
    void compact(unsigned* tracked_slot=nullptr);
};
}
