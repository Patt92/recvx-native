#pragma once
#include "item_tables.h"
#include <array>
#include <cstdint>
#include <string>
namespace recvx {
// sub1.c ItemUse: StatusCancel request, bhSetMessage index, return value
// (useon), Use_00's PulseInit + pulseflg bits and the resulting st->hp.
struct ItemUseResult {bool closing=false;unsigned message=0,useon=0,pulse=0,condition=0;};
// sub1.c GetItem: testmode after the call, bhSetMessage index, statusflg 0x100
// (granted), cb_flg 0x400 set by the immediate-use answer, Use_00 pulseflg.
struct PickupResult {unsigned mode=0,message=0;bool granted=false,used=false;unsigned pulse=0;};
// WeaponSet effect on the player work: plp->wpnr_no plus the gm_flg 0x10000000
// bit of item 131. `changed` is false when no WeaponSet ran.
struct WeaponChange {bool changed=false;unsigned wpnr_no=0;bool special=false;};
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
    // sub1.c GetItem: mode 0 begins, modes 1/4 consume a menu answer 100/101,
    // mode 2 is the following pass that reports message 158. `busy` is the
    // original wait in testmode 1..4: swork.statusflg 0x40 (status screen still
    // sliding) or sys->st_flg 0x200 (a message is on screen).
    PickupResult pickup(unsigned item,unsigned& control,unsigned stage,unsigned room,unsigned mode=0,int answer=-1,bool busy=false);
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
    // sub1.c ItemSearch: first pip[1..11] entry with this item id (slot index),
    // 0 when absent.
    unsigned search(unsigned item)const;
    // sub1.c EraseItem: clears the entry, lowers pip[0] when the erased slot
    // precedes the equipped one, then ItemSort.
    void erase(unsigned slot);
    // Last WeaponSet effect of a mutation (GetItem 0x8000, combination bit
    // 0x80, box transfer of the equipped slot). Consumed by the caller.
    WeaponChange take_weapon_change(){const auto c=weapon_change_;weapon_change_={};return c;}
    // sys->gm_flg 0x40000: bhCountBullet emptied the chamber; cleared by the
    // combat owner after a successful bhSearchBullet.
    bool chamber_empty=false;
    // plp->at_flg 0x40000000: infinite ammunition for the current attack.
    bool unlimited_attack=false;
    // sub1.c sakai: first slot behind the wide items (menu cursor boundary).
    unsigned wide_boundary()const;
    // Event-Inventarmutationen (event.c, Opcodes 0x31/0xb2/0xbf/0xc1/0xc2/
    // 0xc7/0xc9). Sie wirken direkt auf sys->itm; ply_wno ist der einzige
    // zusätzliche Zustand (Waffennummer des Partners, bhItemGetGet Item 34).
    void script_item_lost(unsigned item);                                  // bhPlItemLost
    void script_item_give(unsigned owner,unsigned item,unsigned mode);     // bhItemGetGet
    void script_item_lost_ex(unsigned item,unsigned box_slot);             // bhPlItemLostEx
    void script_arms_set(unsigned item);                                   // bhArmsItemSet
    void script_item_give_ex(unsigned owner,unsigned item,unsigned equip,unsigned amount); // bhItemGetGetEx
    void script_item_change(unsigned item,unsigned replacement);           // bhPlItemChangeM
    void script_item_amount(unsigned item,unsigned amount);                // bhPlItemTamaSet
    std::array<std::uint8_t,4> partner_weapon{};                           // sys->ply_wno
private:
    // Original ItemSort over pip[2..11]: wide items first, then the rest; the
    // equipped index pip[0] is left unchanged, exactly as in the original.
    void compact(unsigned* tracked_slot=nullptr);
    // WeaponSet: `previous` is plp->wpnr_no before this mutation. Callers read
    // it with weapon() before touching any slot, because the original calls
    // WeaponSet after a combination has already swapped the entries. No second
    // player state is kept here.
    void weapon_set(unsigned slot,unsigned lt_c,unsigned previous);
    // Use_00 on this inventory (heal/poison/spray); shared by ItemUse and
    // GetItem's immediate-use answer.
    struct HerbUse {bool consumed=false;unsigned message=0,pulse=0,condition=0;};
    HerbUse use_00(unsigned item,unsigned condition,bool message_active);
    WeaponChange weapon_change_{};
};
}
