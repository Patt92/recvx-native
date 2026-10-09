#pragma once
#include "inventory.h"
#include "scene.h"
#include <memory>
namespace recvx {
unsigned file_page_reduction(const std::array<std::uint32_t,32>& event_flags);
// SelectFile/SelectTag own the rotation and register animation clocks.
class FileFolder {
public:
    FileFolder();
    void open(Inventory& inventory,bool bonus=false,bool automatic=false);
    void step(Inventory& inventory,unsigned pad=0);
    void return_from_reader();
    unsigned take_sound();
    bool take_reader();
    bool take_navigation();
    bool moving()const;
    bool registers()const;
    bool arrows()const;
    float arrow_brightness(unsigned index)const;
    unsigned folder()const;
    unsigned file()const;
    unsigned clock()const;
    bool automatic()const;
    Scene scene(const std::vector<std::uint8_t>& packet,const Inventory& inventory)const;
private:
    // SearchTag and the two rotation phases of SelectFile/SelectTag.
    bool search_tag(const Inventory& inventory,int direction);
    void rotate_selected(int step);
    struct State;
    std::shared_ptr<State> state_;
};
}
