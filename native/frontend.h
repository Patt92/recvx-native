#pragma once
#include "input.h"
#include "third_person.h"
#include "video_settings.h"
#include "audio_settings.h"
namespace recvx {
// TitleCall: original adv.c TitleCall started (play ADV entry 3, white flash and
// fade to black). NewGame follows once the fade is complete (Mode 17 -> -1).
enum class FrontendCommand {None,TitleCall,NewGame,LoadGame,Quit};
// Title screen = adv.c Adv_BioCvTitle (2048) ported mode by mode: Mode 0..4
// init and 30-frame fade-in (RequestAdvFade(2, GetSamurai(50)) = 1/30), Mode 5..7
// "PRESS START button" plate (DisplayPressStartPlate, 900-frame attract timer),
// Mode 8..10 game-mode plate LOAD GAME / NEW GAME / OPTION (DisplayGameModePlate,
// CheckButton Level 0), Mode 17 TitleCall / OPTION fade, Mode -1 return code.
// All plates are the original ADV/0002 texture 1 quads (SetQuadPos/SetQuadUv2);
// the host draws no text of its own for original entries.
//
// Documented host deviations (PC port, see CLAUDE.md "Hauptmenue 2026-10-13"):
//  * Host save slots replace memory-card availability for LOAD GAME: without
//    saves, CheckButton Flag 0 forces Cursor 1; otherwise Cursor 0 is selectable.
//    BATTLE GAME (ssd_flg & 1, system save) remains off.
//  * ReturnCode 4 (OPTION, Adv_GameOptionScreen) opens the host settings screen
//    (Screen::Controls: key bindings, 3rd-person mod, render quality, MetalFX
//    upscaler / frame generation / post-processing) instead of
//    the original option screen, which is not ported.
//  * ReturnCode 1 (attract demo after the 900-frame timer, ss_flg 0x400200) is
//    not ported: the title re-initialises (Mode 0) as bhReturnTitle would after
//    the demo. attract_count() counts these.
//  * A fourth row QUIT (host, no original counterpart) at PosYDef[0][3] = 384,
//    rendered with the original system font in the plate colours; it leaves
//    via the OPTION path (SE 3, GetSamurai(50) fade) and returns FrontendCommand::Quit.
//  * Keyboard/controller "Confirm" stands for PS2 Start (0x800) and the OK
//    button alike (runtime.cpp binds both bits to one physical button).
class Frontend {
public:
    Frontend(InputConfig& config,std::filesystem::path path):config_(config),path_(std::move(path)){}
    bool active()const{return screen_!=Screen::Hidden;}
    bool settings()const{return screen_==Screen::Controls;}
    bool title()const{return screen_==Screen::Title;}
    bool starting()const{return screen_==Screen::Title&&mode_==17&&gen_flag_;} // TitleCall fade (Mode 17, GenFlag 1)
    int mode()const{return mode_;}                     // ADV_WORK.Mode
    unsigned cursor()const{return cursor_;}            // ADV_WORK.Cursor[0]
    bool press_start()const{return screen_==Screen::Title&&mode_>=5&&mode_<=7;}
    bool menu_ready()const{return screen_==Screen::Title&&mode_==9;} // CheckButton reads the pad
    float fade()const{return fade_rate_;}
    float flush()const{return flush_count_;}
    float flush2()const{return flush_count2_;}
    float timer()const{return timer_;}
    unsigned font_base_color()const{return font_base_color_;}
    unsigned attract_count()const{return attract_count_;}
    // Replays/tests only: finish a running TitleCall fade at once.
    FrontendCommand skip_transition(){if(!starting()||next_return_code_!=2)return FrontendCommand::None;finish_title_task();return FrontendCommand::NewGame;}
    // Replay shortcut "new_game": Start, NEW GAME, TitleCall and the fade in one step.
    std::vector<FrontendCommand> replay_new_game();
    bool capturing()const{return capture_;}
    bool gamepad_page()const{return gamepad_;}
    unsigned selected()const{return screen_==Screen::Title?cursor_:selected_;}
    void hide(){screen_=Screen::Hidden;}
    void show_main(){screen_=Screen::Title;mode_=0;from_title_=0;} // bhReturnTitle: Mode 0, FromTitle 0 (ss_flg 0x100000/0x1000)
    // bup_00.c:425-437 TypewriterExit mit ss_flg 0x200 (Ladebildschirm vom Titel, ohne Laden): tk_flg 0x300008 ->
    // Adv_BioCvTitle Mode 0 mit dem FromTitle 1 von LOAD GAME (adv.c:1953): Mode 4 geht direkt in das Hauptmenue (Mode 8).
    void return_from_load(){screen_=Screen::Title;mode_=0;from_title_=1;}
    FrontendCommand apply(const InputFrame& input);
    FrontendCommand input(MenuInput input,bool gamepad=false);
    void event(const SDL_Event& event);
    void draw(SoftwareRenderer& image,GameUi& ui,std::size_t controllers);
    std::function<void(const InputConfig&)> changed;
    // LOAD GAME ist seit 2026-10-14 keine Abweichung mehr: liegen Schreib-
    // maschinen-Spielstaende vor (FindFirstVmDrive-Pendant), ist Cursor 0
    // waehlbar und laeuft ueber Titel-Returncode 3 in den Ladebildschirm.
    std::function<bool()> has_saves;
    std::function<void(unsigned)> sound;   // CallSystemSe(0, n) / CallSystemSeBasic(n,0,0)
    std::function<void(unsigned)> vibrate; // TitleCall: StartVibrationEx(PortId, 16)
    ThirdPersonCamera* third_person=nullptr; // optional camera mod row in the settings screen
    static constexpr unsigned third_person_row=action_count+3;
    VideoConfig* video=nullptr; // host render quality row
    // Host presentation rows (video.ini): MetalFX upscaler, frame generation and post-processing.
    static constexpr unsigned video_row=action_count+4,frame_rate_row=action_count+5,upscaler_row=action_count+6,frame_generation_row=action_count+7,sharpness_row=action_count+8,post_process_row=action_count+9,audio_row=action_count+10,back_row=action_count+11,row_count=action_count+12;
    std::function<void(const VideoSettings&)> video_changed;
    AudioConfig* audio=nullptr;
    std::function<void(unsigned)> audio_changed;
    static constexpr unsigned title_rows=3;          // CursorMax[0] with ExtraFlag 0: LOAD GAME, NEW GAME, OPTION
    static constexpr unsigned quit_row=title_rows;   // host QUIT row (deviation)
    static constexpr unsigned load_game_cursor=0,new_game_cursor=1,option_cursor=2;
private:
    enum class Screen {Title,Controls,Hidden};
    // What the current Adv_BioCvTitle frame displayed (Display* calls of the
    // mode that ran); draw() renders it so that a mode change inside a frame
    // still shows that frame's plate like the original.
    struct TitleView {enum Kind {None,Bg,PressStart,GameMode} kind=None;bool plate_fade=false;float flush=0,flush2=0;unsigned font_base=0xffffff;unsigned flash=0;bool fade=false;float fade_rate=0;float saver=0;};
    Screen screen_=Screen::Title;
    // ADV_WORK subset
    int mode_=0,next_mode_=0,next_return_code_=0,from_title_=0,gen_flag_=0,extra_flag_=0,fade_type_=0;
    unsigned cursor_=0,cursor_max_=title_rows,font_base_color_=0xffffff,attract_count_=0;
    int mode_command_id_[4]={0,1,2,3};
    float flush_count_=0,flush_speed_=0,flush_count2_=0,flush_speed2_=0,fade_rate_=0,fade_speed_=0,timer_=0,saver_timer_=9000,saver_rate_=0;
    int saver_command_=0;
    bool pad_on_=false,start_pressed_=false,ok_pressed_=false,cancel_pressed_=false;int cursor_moves_=0;bool option_pad_=false;
    FrontendCommand pending_command_=FrontendCommand::None;
    TitleView view_;
    FrontendCommand title_step();
    void reset_adv_system();void request_adv_fade(int type,float speed);void execute_adv_fade();void execute_adv_screen_saver();
    void reset_flush_plate();void flush_plate();void fade_in_plate(int next_mode);void fade_out_plate();
    void title_call(int return_code);void check_button();void title_frame();void finish_title_task();
    void draw_title(SoftwareRenderer& image,GameUi& ui)const;
    // host settings screen
    InputConfig& config_;std::filesystem::path path_;
    bool gamepad_=false,capture_=false,suppress_=false;
    unsigned selected_=0;
    std::array<bool,SDL_CONTROLLER_AXIS_MAX> neutral_{};
    std::string status_;
    void commit(const InputConfig& candidate);
    void arm();
    FrontendCommand controls_input(MenuInput key,bool pad);
};
}
