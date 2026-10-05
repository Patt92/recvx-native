#pragma once
#include "inventory.h"
#include "scene.h"
#include <memory>
namespace recvx {
// Original ItemModelCheck/MessageWait/Action/Zoom routines, with a host adapter.
class ItemInspection {
public:
    struct State;
    ItemInspection();
    void check(Inventory& inventory,unsigned slot,std::array<Angle,3> angles);
    void answer(Inventory& inventory,bool yes);
    void cancel(Inventory& inventory,unsigned slot);
    void tick(Inventory& inventory);
    bool question()const;
    bool running()const;
    bool message_pending()const;
    unsigned take_message();
    unsigned source()const;
    unsigned action_frame()const;
    unsigned model()const;
    unsigned item()const;
    unsigned phase()const;
    unsigned completed_file()const;
    float zoom()const;
    std::array<Angle,3> spin()const;
private:std::shared_ptr<State> state_;
};
unsigned inventory_name_id(std::uint32_t packed);
unsigned inspection_model_id(unsigned item);
unsigned inspection_action(unsigned item);
std::array<Angle,3> inspection_default_angles(unsigned item);
ModelGeometryOptions inspection_geometry(const std::vector<std::uint8_t>& packet,unsigned item,unsigned frame=0);
unsigned original_file_number(unsigned item,unsigned stage=0,unsigned room=0);
unsigned original_file_pages(unsigned file);
unsigned original_file_message(unsigned file,unsigned page);
unsigned original_file_wallpaper(unsigned file);
}
