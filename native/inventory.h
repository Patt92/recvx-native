#pragma once
#include "item_tables.h"
#include <array>
#include <cstdint>
#include <string>
namespace recvx {
class Inventory {
public:
    Inventory();
    std::array<std::uint32_t,384> items{};
    unsigned player=0,game_mode=0;
    int hp=160,max_hp=160;
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
    bool acquire(unsigned item,unsigned quantity,unsigned flags=0);
    bool consume(unsigned slot);
    bool heal(unsigned slot);
    int combination_rule(unsigned first,unsigned second)const;
    bool combine(unsigned first,unsigned second,unsigned* message=nullptr,unsigned* result_slot=nullptr);
    bool transform(unsigned slot,unsigned expected,unsigned replacement);
    bool deposit(unsigned slot,unsigned box=64);
    bool withdraw(unsigned index,unsigned box=64);
    std::string condition()const;
private:
    void compact(unsigned* tracked_slot=nullptr);
};
}
