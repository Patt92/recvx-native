#pragma once
#include "inventory.h"
#include "messages.h"
#include "doors.h"
#include "scene.h"
#include <functional>
#include <set>
namespace recvx {
struct EventCommand {unsigned opcode=0,task=0,event=0;std::size_t offset=0;std::vector<std::uint8_t> bytes;};
struct Interaction {unsigned flags=0,type=0,attribute=0;int floor=0;NJS_POINT3 position{},extent{};std::array<unsigned,4> parameters{};};
class EventVm {
public:
    EventVm(const std::vector<std::uint8_t>& room,Inventory& inventory,MessageDialog& dialog);
    std::array<std::array<std::uint32_t,32>,17> flags{};
    std::array<unsigned,256> values{};
    std::vector<Interaction> interactions;
    std::vector<EventCommand> pending_bindings;
    std::vector<std::string> errors;
    std::vector<unsigned> dispatched;
    bool menu_open=false,status_requested=false;
    unsigned tick_count=0;
    // Renderer/world bindings are distinct from opcode parsing. Unbound
    // commands are retained with exact operands and reported by the runtime.
    std::function<bool(const EventCommand&)> binding;
    std::function<bool(unsigned)> enemy_dead;
    std::function<unsigned(unsigned,unsigned)> animation_frame;
    std::function<bool(unsigned)> audio_active;
    std::function<void(unsigned)> audio_stop;
    std::function<Message(unsigned,unsigned)> item_message;
    std::function<bool()> movie_active;
    std::function<bool(const DoorRequest&)> door;
    void inherit_progress(const EventVm& previous);
    void initialize();
    void tick();
    void start(unsigned level,unsigned event);
    bool interact(NJS_POINT3 position,Angle yaw,int floor=0);
    bool use(unsigned item,unsigned* message=nullptr);
    void check_floor(NJS_POINT3 position,Angle yaw,int floor=0);
    void confirm_dialog();
    void cancel_dialog();
    void choose_dialog(int direction);
    bool skip_cinematic();
    bool flag(unsigned matrix,unsigned index)const;
    void set_flag(unsigned matrix,unsigned index,unsigned mode);
private:
    struct Loop {std::size_t condition=0,body=0;unsigned count=0;bool repeat=false;};
    struct Task {bool active=false,blocked=false;unsigned event=0,level=0,delay=0;std::size_t pc=0;std::vector<std::size_t> branches;std::vector<Loop> loops;};
    std::vector<std::uint8_t> room_;
    std::uint32_t random_=1; // Deterministic host RNG; PS2 libc sequence is not yet matched.
    std::size_t base_=0,scd0_=0,scd1_=0,messages_=0;
    std::vector<std::size_t> scripts_;
    std::vector<Interaction> floors_;
    Inventory& inventory_;MessageDialog& dialog_;
    bool opening_hint_=false;
    unsigned pickup_item_=0,pickup_model_=0;
    std::vector<unsigned> room_items_;
    void hide_item(unsigned index);
    std::array<Task,16> tasks_{};
    std::set<std::pair<unsigned,std::size_t>> reported_;
    void run(Task& task,bool scenario=false);
    int instruction(Task& task);
    void world(Task& task,unsigned opcode,std::size_t length);
};
}
