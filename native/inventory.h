#pragma once
#include "item_tables.h"
#include <array>
#include <cstdint>
#include <string>
namespace recvx {
struct FloorUseResult {bool closing=false;unsigned message=0;};
struct PickupResult {unsigned mode=0,message=0;bool granted=false,used=false;};
class Inventory {
public:
    Inventory();
    std::array<std::uint32_t,384> items{};
    unsigned player=0,game_mode=0;
    int hp=160,max_hp=160;
    std::uint32_t play_time=0; // sync.c stv_tm, 60-Hz VSync units, room-independent.
    bool poisoned=false,side_pack=false,infected=false;
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
    // sub1.c GetItem: mode 0 begins, modes 1/4 consume a menu answer 100/101.
    PickupResult pickup(unsigned item,unsigned& control,unsigned stage,unsigned room,unsigned mode=0,int answer=-1);
    FloorUseResult use_on_floor(unsigned slot,const std::array<unsigned,4>* parameters,unsigned& control,unsigned stage,unsigned room);
    bool consume(unsigned slot);
    bool heal(unsigned slot);
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
