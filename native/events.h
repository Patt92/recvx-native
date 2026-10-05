#pragma once
#include "inventory.h"
#include "messages.h"
#include "scene.h"
#include <functional>
#include <set>
namespace recvx {
struct EventCommand {unsigned opcode=0,task=0,event=0;std::size_t offset=0;std::vector<std::uint8_t> bytes;};
struct Interaction {unsigned flags=0,type=0,attribute=0,floor=0;NJS_POINT3 position{},extent{};std::array<unsigned,4> parameters{};};
class EventVm {
public:
    EventVm(const std::vector<std::uint8_t>& room,Inventory& inventory,MessageDialog& dialog);
    std::array<std::array<std::uint32_t,32>,17> flags{};
    std::array<unsigned,256> values{};
    std::vector<Interaction> interactions;
    std::vector<EventCommand> pending_bindings;
    std::vector<std::string> errors;
    std::vector<unsigned> dispatched;
    bool menu_open=false;
    unsigned tick_count=0;
    // Renderer/world bindings are distinct from opcode parsing. Unbound
    // commands are retained with exact operands and reported by the runtime.
    std::function<bool(const EventCommand&)> binding;
    std::function<unsigned(unsigned,unsigned)> animation_frame;
    std::function<bool(unsigned)> audio_active;
    std::function<bool()> movie_active;
    void initialize();
    void tick();
    void start(unsigned level,unsigned event);
    bool interact(NJS_POINT3 position,Angle yaw);
    void use(unsigned item);
    void check_floor(NJS_POINT3 position,Angle yaw,unsigned floor=0);
    void confirm_dialog();
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
    std::array<Task,16> tasks_{};
    std::set<std::pair<unsigned,std::size_t>> reported_;
    void run(Task& task,bool scenario=false);
    int instruction(Task& task);
    void world(Task& task,unsigned opcode,std::size_t length);
};
}
