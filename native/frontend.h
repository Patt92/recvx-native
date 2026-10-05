#pragma once
#include "input.h"
#include "third_person.h"
namespace recvx {
enum class FrontendCommand {None,NewGame,Quit};
class Frontend {
public:
    Frontend(InputConfig& config,std::filesystem::path path):config_(config),path_(std::move(path)){}
    bool active()const{return screen_!=Screen::Hidden;}
    bool settings()const{return screen_==Screen::Controls;}
    bool capturing()const{return capture_;}
    bool gamepad_page()const{return gamepad_;}
    unsigned selected()const{return selected_;}
    void hide(){screen_=Screen::Hidden;}
    FrontendCommand apply(const InputFrame& input);
    FrontendCommand input(MenuInput input,bool gamepad=false);
    void event(const SDL_Event& event);
    void draw(SoftwareRenderer& image,GameUi& ui,std::size_t controllers);
    std::function<void(const InputConfig&)> changed;
    std::function<void(unsigned)> sound;
    ThirdPersonCamera* third_person=nullptr; // optional camera mod row in the settings screen
    static constexpr unsigned third_person_row=action_count+3;
private:
    enum class Screen {Main,Controls,Hidden};
    Screen screen_=Screen::Main;
    InputConfig& config_;std::filesystem::path path_;
    bool gamepad_=false,capture_=false,suppress_=false;
    unsigned selected_=0;
    std::array<bool,SDL_CONTROLLER_AXIS_MAX> neutral_{};
    std::string status_;
    void commit(const InputConfig& candidate);
    void arm();
};
void draw_skip_hint(SoftwareRenderer& image,const GameUi& ui,const InputConfig& config);
}
