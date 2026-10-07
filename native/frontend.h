#pragma once
#include "input.h"
#include "third_person.h"
#include "video_settings.h"
namespace recvx {
// TitleCall: original adv.c TitleCall started (play ADV entry 3, white flash and
// fade to black). NewGame follows once the fade is complete (Mode 17 -> -1).
enum class FrontendCommand {None,TitleCall,NewGame,Quit};
class Frontend {
public:
    Frontend(InputConfig& config,std::filesystem::path path):config_(config),path_(std::move(path)){}
    bool active()const{return screen_!=Screen::Hidden;}
    bool settings()const{return screen_==Screen::Controls;}
    bool starting()const{return screen_==Screen::Starting;}
    float fade()const{return fade_rate_;}
    float flush()const{return flush_count_;}
    // Replays/tests only: finish the title transition at once.
    FrontendCommand skip_transition(){if(screen_!=Screen::Starting)return FrontendCommand::None;hide();return FrontendCommand::NewGame;}
    bool capturing()const{return capture_;}
    bool gamepad_page()const{return gamepad_;}
    unsigned selected()const{return selected_;}
    void hide(){screen_=Screen::Hidden;}
    void show_main(){screen_=Screen::Main;selected_=0;} // host game-over placeholder: back to the title menu
    FrontendCommand apply(const InputFrame& input);
    FrontendCommand input(MenuInput input,bool gamepad=false);
    void event(const SDL_Event& event);
    void draw(SoftwareRenderer& image,GameUi& ui,std::size_t controllers);
    std::function<void(const InputConfig&)> changed;
    std::function<void(unsigned)> sound;
    ThirdPersonCamera* third_person=nullptr; // optional camera mod row in the settings screen
    static constexpr unsigned third_person_row=action_count+3;
    VideoConfig* video=nullptr; // host render quality row
    static constexpr unsigned video_row=action_count+4,back_row=action_count+5,row_count=action_count+6;
    std::function<void(const VideoSettings&)> video_changed;
private:
    enum class Screen {Main,Controls,Starting,Hidden};
    float flush_count_=0,fade_rate_=0;
    FrontendCommand advance_title_call();
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
