#pragma once
#include <SDL.h>
#include "ui.h"
#include "events.h"
#include <array>
#include <filesystem>
#include <map>
#include <functional>
#include <string>
#include <vector>
namespace recvx {
enum class Action {Forward,Back,TurnLeft,TurnRight,Run,Inventory,Confirm,Cancel,Up,Down,Left,Right,Skip,Quit,Wireframe,Count};
constexpr unsigned action_count=unsigned(Action::Count);
struct InputFrame {
    bool gamepad_input=false;
    std::array<bool,action_count> held{},pressed{},repeat{};
    bool down(Action a)const{return held[unsigned(a)];}
    bool hit(Action a)const{return pressed[unsigned(a)];}
    bool navigation(Action a)const{return hit(a)||repeat[unsigned(a)];}
    int move()const{return int(down(Action::Forward))-int(down(Action::Back));}
    int turn()const{return int(down(Action::TurnRight))-int(down(Action::TurnLeft));}
};
struct InputBinding {enum class Kind {Key,Button,Axis};Kind kind;int code;int direction=1;};
struct InputConfig {
    std::array<std::vector<InputBinding>,action_count> bindings;
    float deadzone=.25f,menu_threshold=.55f;
    unsigned repeat_delay=18,repeat_interval=5;
    std::string describe(Action action,bool gamepad)const;
    void save(const std::filesystem::path& path)const;
    static InputConfig defaults();
    static InputConfig read(const std::filesystem::path& path);
    static void write_defaults(const std::filesystem::path& path);
};
class InputManager {
public:
    explicit InputManager(InputConfig config);
    ~InputManager();
    InputManager(const InputManager&)=delete;
    InputManager& operator=(const InputManager&)=delete;
    void event(const SDL_Event& event);
    InputFrame tick();
    void configure(InputConfig config);
    std::size_t controllers()const{return pads_.size();}
private:
    struct Pad {SDL_GameController* handle=nullptr;std::array<Sint16,SDL_CONTROLLER_AXIS_MAX> axes{};std::array<Uint8,SDL_CONTROLLER_BUTTON_MAX> buttons{};};
    InputConfig config_;
    std::array<bool,SDL_NUM_SCANCODES> keys_{};
    std::map<SDL_JoystickID,Pad> pads_;
    std::array<bool,action_count> held_{},pending_{};
    std::array<unsigned,action_count> age_{};
    bool focused_=true,last_gamepad_=false;
    void open(int index);
    void refresh(Pad& pad);
    void evaluate(bool edges=true);
};
// Used by both the runtime and controller integration tests. All input sources
// take the same cinematic/dialogue/menu path before any movement is allowed.
class GameplayInput {
public:
    GameplayInput(Inventory& inventory,StatusMenu& menu,MessageDialog& dialog,EventVm* events):inventory_(inventory),menu_(menu),dialog_(dialog),events_(events){}
    void menu_input(MenuInput key);
    bool apply(const InputFrame& frame); // true means an explicit quit action
    std::function<void()> interact,confirm_transition;
    std::function<bool()> transition_active,video_active;
    std::function<void()> skip_video;
    void set_events(EventVm* events){events_=events;inventory_grace_=0;inventory_pending_=false;}
private:
    Inventory& inventory_;StatusMenu& menu_;MessageDialog& dialog_;EventVm* events_;
    unsigned inventory_grace_=0;
    bool inventory_pending_=false;
};
}
