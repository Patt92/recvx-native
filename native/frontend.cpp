#include "frontend.h"
#include "back_color.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace recvx {
namespace {
// Host settings screen (replaces the unported Adv_GameOptionScreen; see frontend.h).
const char* const labels[]={"Forward","Back","Turn left","Turn right","Run","Inventory","OK / Action","Cancel","Menu up","Menu down","Menu left","Menu right","Skip video","Pause","Quit game","Wireframe","Aim","Attack / Fire","Next target","Select (skip scene)"};
static_assert(sizeof(labels)/sizeof(*labels)==action_count,"one label per input action");
void panel(SoftwareRenderer& image,int x,int y,int w,int h){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i){auto& pixel=image.canvas()[j*image.width()+i];pixel=0xff000000|((pixel&0xfcfcfc)>>2);}}
std::string percent(float value){return std::to_string(int(std::round(value*100)))+" %";}
float samurai(int time){return 1.f/float((time/100)*30+((time%100)*6)/10);} // adv.c:56 GetSamurai
// adv.c:691 AdvEasyDrawTexture for an axis-aligned SetQuadPos/SetQuadUv2 quad:
// texel * BaseColor, alpha = texel alpha * BaseColor alpha. `uw`/`vh` are the
// texel spans the quad's UVs cover (inclusive end clamped to the atlas).
void quad(SoftwareRenderer& image,const SceneTexture& t,int x,int y,int w,int h,int u,int v,int uw,int vh,std::uint32_t base){
    if(!(base>>24))return;auto& dst=image.canvas();
    for(int j=0;j<h;++j)for(int i=0;i<w;++i){const int tx=x+i,ty=y+j;if(tx<0||ty<0||tx>=image.width()||ty>=image.height())continue;
        const unsigned su=std::min(unsigned(u+i*uw/w),t.width-1),sv=std::min(unsigned(v+j*vh/h),t.height-1);const auto c=t.pixels[sv*t.width+su];
        const unsigned alpha=(c>>24)*(base>>24)/255;if(!alpha)continue;auto& d=dst[ty*image.width()+tx];std::uint32_t out=0xff000000;
        for(unsigned shift:{0u,8u,16u}){const auto component=((c>>shift)&255)*((base>>shift)&255)/255;out|=((component*alpha+((d>>shift)&255)*(255-alpha))/255)<<shift;}d=out;}
}
// adv.c:708 AdvEasyDrawTextureS: the same quad again at +3,+3 with colour
// ShadowAlpha<<24 at PosZ-0.001 (behind the plate), so it is painted first.
void quad_s(SoftwareRenderer& image,const SceneTexture& t,int x,int y,int w,int h,int u,int v,int uw,int vh,std::uint32_t base,unsigned shadow_alpha){
    quad(image,t,x+3,y+3,w,h,u,v,uw,vh,shadow_alpha<<24);quad(image,t,x,y,w,h,u,v,uw,vh,base);
}
// njDrawPolygon over (-2,-2)-(642,482) with one colour: AdvDrawFadePolygon and the Mode 17 flash.
void fill(SoftwareRenderer& image,std::uint32_t rgb,unsigned alpha){
    if(!alpha)return;const unsigned k=std::min(255u,alpha);
    for(auto& p:image.canvas()){std::uint32_t out=0xff000000;for(unsigned shift:{0u,8u,16u})out|=((((rgb>>shift)&255)*k+((p>>shift)&255)*(255-k))/255)<<shift;p=out;}
}
}
// ---------------------------------------------------------------- adv.c title task
void Frontend::reset_adv_system(){set_back_color(0); /* adv.c:97 */next_return_code_=0;extra_flag_=0;next_mode_=0;gen_flag_=0;saver_timer_=9000;saver_command_=0;saver_rate_=0;} // adv.c:83 ResetAdvSystem subset, StopAdvScreenSaver(1)
void Frontend::request_adv_fade(int type,float speed){fade_type_=type;fade_speed_=speed;if(type==1)fade_type_=0;else if(type==2)fade_rate_=1;else if(type==3)fade_rate_=0;} // adv.c:384
void Frontend::execute_adv_fade(){ // adv.c:451 ExecuteAdvFadeEx(0)
    if(fade_type_==2){fade_rate_-=fade_speed_;if(fade_rate_<=0){fade_rate_=0;fade_type_=0;}view_.fade=true;view_.fade_rate=fade_rate_;}
    else if(fade_type_==3){fade_rate_+=fade_speed_;if(fade_rate_>=1){fade_rate_=1;fade_type_=0;}view_.fade=true;view_.fade_rate=fade_rate_;}
}
void Frontend::execute_adv_screen_saver(){ // adv.c:510
    if(saver_command_==0){saver_timer_-=1;if(saver_timer_<0){saver_command_=1;saver_rate_=0;}}
    if(saver_command_==1){saver_rate_+=.01f;if(saver_rate_>=.5f)saver_rate_=.5f;}else{saver_rate_-=.05f;if(saver_rate_<=0)saver_rate_=0;}
    view_.saver=saver_rate_;
}
void Frontend::reset_flush_plate(){flush_speed2_=0;flush_count2_=0;} // adv.c:1522
void Frontend::flush_plate(){ // adv.c:1533: the selected plate pulses 255 -> 191 -> 255 over 64 frames
    flush_count2_+=flush_speed2_;
    if(flush_speed2_<0){if(flush_count2_<=-64){flush_count2_=-64;flush_speed2_=2;}}
    else if(flush_count2_>=0){flush_count2_=0;flush_speed2_=-2;}
    const unsigned level=unsigned(int(flush_count2_)+255)&0xff;font_base_color_=level|(level<<16)|(level<<8);
}
void Frontend::fade_in_plate(int next_mode){reset_flush_plate();flush_count_+=flush_speed_;if(flush_count_>255){flush_count_=255;flush_speed_=32;mode_=next_mode;}} // adv.c:1785
void Frontend::fade_out_plate(){reset_flush_plate();flush_count_-=flush_speed_;if(flush_count_<0){flush_count_=0;flush_speed_=32;mode_=next_mode_;}}  // adv.c:1805
void Frontend::title_call(int return_code){ // adv.c:1825 TitleCall(PortId, ReturnCode)
    if(vibrate)vibrate(16);                       // StartVibrationEx(PortId, 16)
    pending_command_=FrontendCommand::TitleCall;  // SetVolumeAdx2(0,0); PlayAdx(0, PatId, 3) -> runtime plays ADV/0003
    request_adv_fade(3,samurai(430));flush_count_=180;next_return_code_=return_code;mode_=17;gen_flag_=1;
}
// adv.c:1849 CheckButton(Level 0, Flag, MaxFlag 0). Flag is CheckMcSelectPortInfoState
// of both ports; host save slots supply that availability. Cursor 0 is skipped
// only without saves (adv.c:1864/1879). MaxFlag 0: no wrap at either end.
void Frontend::check_button(){
    if(pad_on_){saver_timer_=9000;saver_command_=0;}       // CheckAdvScreenSaverStopKey -> StopAdvScreenSaver(0)
    const unsigned first=has_saves&&has_saves()?0u:1u;
    bool connect=true;if(cursor_==0&&first!=0){cursor_=1;connect=false;}
    const unsigned last=cursor_max_;                       // original: CursorMax-1; host: +1 for the QUIT row (deviation)
    if(connect){
        if(cursor_moves_<0){cursor_=std::max(cursor_,first+1)-1;if(sound)sound(2);}  // Rept & 0x1000: Cursor--, skip 0 only with Flag 0
        else if(cursor_moves_>0){cursor_=std::min(cursor_+1,last);if(sound)sound(2);} // Rept & 0x4000 (pad down): Cursor++, clamp
    }
    if(ok_pressed_){ // press & 0x800 (Start) || press & AdvGetOkButton()
        const int command=cursor_==quit_row?4:mode_command_id_[cursor_];
        switch(command){
        case 0: if(sound)sound(3);from_title_=0;title_call(3);break;        // LOAD GAME: system.c:374 ss_flg 0x200, tk_flg 0x308040, typ_md0 0 -> Ladebildschirm
        case 1: from_title_=0;title_call(2);break;                          // NEW GAME: AppMode 1, sys->gm_mode 0, TitleCall(PortId, 2)
        case 2: from_title_=2;next_return_code_=4;mode_=17;if(sound)sound(3);request_adv_fade(3,samurai(50));break; // OPTION: AppMode 0
        case 3: from_title_=3;title_call(6);break;                          // BATTLE GAME (ExtraFlag): sys->gm_mode 3, TitleCall(PortId, 6)
        case 4: from_title_=2;next_return_code_=100;mode_=17;if(sound)sound(3);request_adv_fade(3,samurai(50));break; // host QUIT: OPTION leave path
        }
    }else if(cancel_pressed_){next_mode_=5;mode_=10;if(sound)sound(0);}     // CancelNextMode[0] 5, CancelMode[0] 10
}
void Frontend::title_frame(){ // adv.c:2048 Adv_BioCvTitle, one 30-Hz frame
    view_=TitleView{};
    execute_adv_fade();execute_adv_screen_saver();
    auto display_game_mode_plate=[&](bool flag){view_.kind=TitleView::GameMode;view_.plate_fade=flag;view_.flush=flush_count_;view_.flush2=flush_count2_;view_.font_base=font_base_color_;flush_plate();};
    switch(mode_){
    case 0: // RequestAllStopSoundEx(1,0,0); SetEventVibrationMode(0); ResetAdvSystem(); CursorFlag all 1; Cursor[1..2], CursorMax[1..2] (plates 11..16 are unreachable in this build)
        reset_adv_system();
        // sys->ssd_flg & 1 (BATTLE GAME unlocked by the system save) needs the unported save system: ExtraFlag stays 0.
        if(extra_flag_){cursor_max_=4;mode_command_id_[0]=0;mode_command_id_[1]=1;mode_command_id_[2]=3;mode_command_id_[3]=2;cursor_=from_title_==3?2:from_title_==2?3:0;}
        else{cursor_max_=3;mode_command_id_[0]=0;mode_command_id_[1]=1;mode_command_id_[2]=2;mode_command_id_[3]=3;cursor_=from_title_==2?2:0;}
        mode_=1;break;
    case 1: mode_=2;break; // ss_flg |= 0x20000; Pad.Flag 1; RequestAdvInsideFileEx(2,0): ADV/0002 is read by GameUi::title_texture
    case 2: mode_=3;break; // CheckReadEndAdvInsideFile2(3): the host read completes at once
    case 3: request_adv_fade(2,samurai(50));mode_=4;break; // RequestAllStopSoundEx(0,1,0); texture lists 0/1; AdvTransShadowPalette
    case 4: if(fade_type_==0){if(from_title_==0)mode_=5;else{flush_count_=0;flush_speed_=32;mode_=8;}}view_.kind=TitleView::Bg;break;
    case 5: timer_=900;flush_count_=0;flush_speed_=32;view_.kind=TitleView::PressStart;view_.flush=flush_count_;mode_=6;break; // DepthLevel 0; ss_flg &= ~0x20000
    case 6:
        flush_count_+=flush_speed_;
        if(flush_speed_<0){if(flush_count_<=128){flush_count_=128;flush_speed_=4;}}
        else if(flush_count_>=255){flush_count_=255;flush_speed_=-4;}
        timer_-=1;
        if(timer_<0){request_adv_fade(3,samurai(50));from_title_=0;next_return_code_=1;mode_=17;}
        else if(start_pressed_){if(sound)sound(3);saver_timer_=9000;saver_command_=0;flush_count_=255;flush_speed_=32;next_mode_=8;mode_=7;} // CheckStartButton: CallSystemSeBasic(3,0,0); StopAdvScreenSaver(0)
        view_.kind=TitleView::PressStart;view_.flush=flush_count_;break;
    case 7: view_.kind=TitleView::PressStart;view_.flush=flush_count_;fade_out_plate();break;
    case 8: // DepthLevel 1; ss_flg |= 0x20000; ulFrame 2; (Cursor 0 && no card/save) -> Cursor 1
        if(cursor_==0&&!(has_saves&&has_saves()))cursor_=1;
        fade_in_plate(9);display_game_mode_plate(true);break;
    case 9: check_button();display_game_mode_plate(false);break; // CheckMcSelectPortInfoState alternates ports 0/1; both 0 without a card
    case 10: display_game_mode_plate(true);fade_out_plate();break;
    case 17:
        if(fade_type_==0)mode_=-1;
        else{view_.kind=TitleView::Bg;if(gen_flag_&&flush_count_!=0){view_.flash=unsigned(flush_count_)&0xff;flush_count_-=3;if(flush_count_<0)flush_count_=0;}}
        break;
    }
    if(mode_==-1)finish_title_task();
}
void Frontend::finish_title_task(){ // adv.c:2436 Mode -1: StopVibrationEx; AdvEasyReleaseTexture; StopAdx(0) for 2/3/6; Mode 0; Active 0; return NextReturnCode
    const int code=next_return_code_;mode_=0;gen_flag_=0;
    switch(code){
    case 2: screen_=Screen::Hidden;pending_command_=FrontendCommand::NewGame;break; // system.c:365 bhSysCallFirstmovie case 2 -> opening (runtime: StopAdx(0), new game)
    case 4: screen_=Screen::Controls;selected_=0;gamepad_=option_pad_;status_.clear();break; // case 4 -> Adv_GameOptionScreen: host settings screen (deviation)
    case 1: ++attract_count_;from_title_=0;break;                                           // case 1 -> attract demo (ss_flg 0x400200): not ported, title re-initialises
    case 3: screen_=Screen::Hidden;pending_command_=FrontendCommand::LoadGame;break;         // LOAD GAME -> Schreibmaschinen-Ladebildschirm (Slotwahl)
    case 100: pending_command_=FrontendCommand::Quit;break;                                 // host QUIT
    default: from_title_=0;break;                                                           // 6 (BATTLE GAME): unreachable without ExtraFlag
    }
}
FrontendCommand Frontend::title_step(){pending_command_=FrontendCommand::None;title_frame();pad_on_=start_pressed_=ok_pressed_=cancel_pressed_=false;cursor_moves_=0;return pending_command_;}
std::vector<FrontendCommand> Frontend::replay_new_game(){
    std::vector<FrontendCommand> out;if(screen_!=Screen::Title)return out;
    for(int i=0;i<256&&mode_!=9;++i){if(mode_==6)start_pressed_=true;title_step();}
    if(mode_!=9)return out;
    cursor_=new_game_cursor;ok_pressed_=true;
    for(auto c:{title_step(),skip_transition()})if(c!=FrontendCommand::None)out.push_back(c);
    return out;
}
// ---------------------------------------------------------------- input
FrontendCommand Frontend::input(MenuInput key,bool pad){
    if(!active()||capture_)return FrontendCommand::None;
    if(screen_==Screen::Title){ // latched like Pad.press/Rept; consumed by the next title frame (CheckStartButton / CheckButton)
        pad_on_=true;option_pad_=pad;
        if(key==MenuInput::Up)--cursor_moves_;else if(key==MenuInput::Down)++cursor_moves_;
        else if(key==MenuInput::Confirm)start_pressed_=ok_pressed_=true;else if(key==MenuInput::Cancel)cancel_pressed_=true;
        return FrontendCommand::None;
    }
    return controls_input(key,pad);
}
FrontendCommand Frontend::apply(const InputFrame& frame){
    if(screen_==Screen::Title){
        for(unsigned i=0;i<action_count;++i)if(frame.held[i])pad_on_=true; // Pad.on != 0 (CheckAdvScreenSaverStopKey)
        if(frame.hit(Action::Cancel))input(MenuInput::Cancel,frame.gamepad_input);
        else if(frame.hit(Action::Quit))return FrontendCommand::Quit; // host hotkey; a key bound to Cancel as well (Escape) is Cancel first
        for(auto pair:{std::pair<Action,MenuInput>{Action::Up,MenuInput::Up},{Action::Down,MenuInput::Down}})if(frame.navigation(pair.first))input(pair.second,frame.gamepad_input);
        if(frame.hit(Action::Confirm))input(MenuInput::Confirm,frame.gamepad_input);
        return title_step();
    }
    if(screen_!=Screen::Controls)return FrontendCommand::None;
    if(suppress_){suppress_=false;return FrontendCommand::None;}if(capture_)return FrontendCommand::None;
    if(frame.hit(Action::Cancel))return input(MenuInput::Cancel,frame.gamepad_input);
    if(frame.hit(Action::Quit))return FrontendCommand::Quit;
    for(auto pair:{std::pair<Action,MenuInput>{Action::Up,MenuInput::Up},{Action::Down,MenuInput::Down},{Action::Left,MenuInput::Left},{Action::Right,MenuInput::Right}})if(frame.navigation(pair.first))input(pair.second,frame.gamepad_input);
    if(frame.hit(Action::Confirm))return input(MenuInput::Confirm,frame.gamepad_input);
    return FrontendCommand::None;
}
// ---------------------------------------------------------------- host settings screen
void Frontend::commit(const InputConfig& candidate){try{candidate.save(path_);config_=candidate;if(changed)changed(config_);status_="Saved";}catch(const std::exception&){status_="Saving failed - bindings unchanged";}}
void Frontend::arm(){capture_=true;status_.clear();neutral_.fill(true);
    for(int i=0;i<SDL_NumJoysticks();++i)if(auto* pad=SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i)))
        for(unsigned axis=0;axis<neutral_.size();++axis){const auto value=SDL_GameControllerGetAxis(pad,SDL_GameControllerAxis(axis));if(axis<SDL_CONTROLLER_AXIS_TRIGGERLEFT&&std::abs(int(value))>16000)neutral_[axis]=false;}
}
FrontendCommand Frontend::controls_input(MenuInput key,bool pad){
    if(sound)sound(key==MenuInput::Cancel?0:key==MenuInput::Confirm?3:2);
    auto back_to_title=[&]{screen_=Screen::Title;mode_=0;from_title_=2;status_.clear();}; // leaving the option screen: Mode 0 with FromTitle 2 (cursor on OPTION)
    if(key==MenuInput::Cancel){back_to_title();return FrontendCommand::None;}
    if(key==MenuInput::Up)selected_=(selected_+row_count-1)%row_count;
    if(key==MenuInput::Down)selected_=(selected_+1)%row_count;
    if((key==MenuInput::Left||key==MenuInput::Right||key==MenuInput::Confirm)&&selected_==third_person_row){if(third_person){third_person->toggle();status_=third_person->status();}return FrontendCommand::None;}
    if((key==MenuInput::Left||key==MenuInput::Right||key==MenuInput::Confirm)&&selected_==video_row){if(video){video->cycle(key==MenuInput::Left?-1:1);status_=video->status();if(video_changed)video_changed(video->settings());}return FrontendCommand::None;}
    if(key==MenuInput::Left||key==MenuInput::Right){
        if(selected_==action_count||selected_==action_count+1){auto candidate=config_;const float increment=key==MenuInput::Left?-.05f:.05f;
            if(selected_==action_count){candidate.deadzone=std::clamp(candidate.deadzone+increment,.05f,.95f);candidate.menu_threshold=std::max(candidate.menu_threshold,candidate.deadzone);}
            else candidate.menu_threshold=std::clamp(candidate.menu_threshold+increment,candidate.deadzone,.95f);commit(candidate);
        }else gamepad_=!gamepad_;
    }
    if(key==MenuInput::Confirm){if(selected_<action_count)arm();else if(selected_==action_count+2)commit(InputConfig::defaults());else if(selected_==back_row)back_to_title();}
    return FrontendCommand::None;
}
void Frontend::event(const SDL_Event& event){if(!capture_)return;
    if(event.type==SDL_KEYDOWN&&!event.key.repeat&&event.key.keysym.scancode==SDL_SCANCODE_ESCAPE){capture_=false;suppress_=true;status_="Cancelled";return;}
    InputBinding binding{InputBinding::Kind::Key,0,1};bool accepted=false;
    if(!gamepad_&&event.type==SDL_KEYDOWN&&!event.key.repeat&&event.key.keysym.scancode>0){binding.code=event.key.keysym.scancode;accepted=true;}
    if(gamepad_&&event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.button<SDL_CONTROLLER_BUTTON_MAX){binding={InputBinding::Kind::Button,event.cbutton.button,1};accepted=true;}
    if(gamepad_&&event.type==SDL_CONTROLLERAXISMOTION&&event.caxis.axis<SDL_CONTROLLER_AXIS_MAX){const unsigned axis=event.caxis.axis;const int value=event.caxis.value;
        if(std::abs(value)<10000||axis>=SDL_CONTROLLER_AXIS_TRIGGERLEFT&&value<10000)neutral_[axis]=true;
        if(neutral_[axis]&&(axis<SDL_CONTROLLER_AXIS_TRIGGERLEFT?std::abs(value)>24000:value>24000)){binding={InputBinding::Kind::Axis,int(axis),value>0?1:-1};accepted=true;}
    }
    if(!accepted)return;auto candidate=config_;auto& bindings=candidate.bindings[selected_];
    if(selected_==unsigned(Action::Confirm)||selected_==unsigned(Action::Cancel)){
        auto& opposite=candidate.bindings[unsigned(selected_==unsigned(Action::Confirm)?Action::Cancel:Action::Confirm)];
        const auto before=opposite.size();opposite.erase(std::remove_if(opposite.begin(),opposite.end(),[&](const InputBinding& b){return b.kind==binding.kind&&b.code==binding.code&&b.direction==binding.direction;}),opposite.end());
        if(opposite.size()!=before&&!std::any_of(opposite.begin(),opposite.end(),[&](const InputBinding& b){return (b.kind!=InputBinding::Kind::Key)==gamepad_;}))
            for(const auto& b:bindings)if((b.kind!=InputBinding::Kind::Key)==gamepad_)opposite.push_back(b);
    }
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[&](const InputBinding& value){return (value.kind!=InputBinding::Kind::Key)==gamepad_;}),bindings.end());bindings.push_back(binding);
    capture_=false;suppress_=true;commit(candidate);
}
// ---------------------------------------------------------------- drawing
void Frontend::draw_title(SoftwareRenderer& image,GameUi& ui)const{
    if(view_.kind==TitleView::None){std::fill(image.canvas().begin(),image.canvas().end(),0xff000000);} // Mode 0..3: black back colour only
    else ui.draw_title(image); // DisplayTitleBg (PosZ 0.01/0.15, behind every plate)
    const auto& atlas=ui.title_texture(2);
    if(view_.kind==TitleView::PressStart){ // adv.c:1582 DisplayPressStartPlate: quad (64,304) 512x32, UV (0,32); alpha FadeRate, ShadowAlpha (unsigned)0.5f = 0
        quad(image,atlas,64,304,512,32,0,32,512,32,(unsigned(view_.flush)<<24)|0xffffff);
    }else if(view_.kind==TitleView::GameMode){ // adv.c:1600 DisplayGameModePlate
        static constexpr int pos_y[2][4]={{288,320,352,384},{272,304,336,368}},v_def[2][4]={{64,96,128,224},{64,96,224,128}};
        // PosYDef[1] is the original's placement for four rows (ExtraFlag); the host's
        // fourth row QUIT uses it too so no plate overlaps the copyright plate at y 396.
        const int table=extra_flag_?1:0,rows=cursor_max_+1>3?1:0;
        const unsigned alpha=view_.plate_fade?unsigned(int(view_.flush)+int(view_.flush2))&0xffu:0xffu,shadow=unsigned(view_.flush)&0xffu; // Flag 0: 0xFF000000
        for(unsigned i=0;i<cursor_max_;++i)quad_s(image,atlas,64,pos_y[rows][i],513,33,0,v_def[table][i],513,33,(alpha<<24)|(i==cursor_?view_.font_base:0x7f7f7fu),shadow);
        // Host QUIT row (deviation, documented in frontend.h): original system font, same colours/shadow, centred like the 513-wide plates.
        const std::string text="QUIT";const float scale=1.f;const int x=64+(513-ui.label_width(text,scale))/2,y=extra_flag_?pos_y[1][3]+32:pos_y[rows][3];
        ui.label(image,text,x+3,y+3,scale,shadow<<24);ui.label(image,text,x,y,scale,(alpha<<24)|(cursor_==quit_row?view_.font_base:0x7f7f7fu));
    }
    if(view_.flash)fill(image,0xffffff,view_.flash);                               // Mode 17 GenFlag: white polygon, alpha FlushCount
    if(view_.fade)fill(image,0x000000,unsigned(255.f*view_.fade_rate)&0xffu);      // AdvDrawFadePolygon(0, FadeRate, 0)
    if(view_.saver>0)fill(image,0x00000f,unsigned(255.f*view_.saver)&0xffu);       // ExecuteAdvScreenSaver: AdvDrawFadePolygon(2, SaverRate, 0xF)
}
void Frontend::draw(SoftwareRenderer& image,GameUi& ui,std::size_t controllers){
    if(screen_==Screen::Title){draw_title(image,ui);return;}
    if(screen_!=Screen::Controls)return;
    ui.draw_title(image);
    panel(image,10,10,620,462);ui.label(image,"OPTION",26,14,.9f);ui.label(image,gamepad_?"Controller":"Keyboard",26,44,.65f);
    ui.label(image,std::to_string(controllers)+(controllers==1?" controller connected":" controllers connected"),342,49,.48f);
    for(unsigned i=0;i<row_count;++i){const int y=68+int(i)*14;const auto color=i==selected_?0xffffff70:0xffffffff;
        std::string label,value;if(i<action_count){label=labels[i];value=config_.describe(Action(i),gamepad_);}
        else if(i==action_count){label="Stick dead zone";value=percent(config_.deadzone)+"  left/right";}
        else if(i==action_count+1){label="Menu stick threshold";value=percent(config_.menu_threshold)+"  left/right";}
        else if(i==action_count+2)label="Restore default bindings";
        else if(i==third_person_row){label="3rd-person camera (mod)";value=third_person?third_person->describe()+"  left/right":"Not available";}
        else if(i==video_row){label="Render quality";value=video?video->describe()+"  left/right":"Not available";}else label="Back to title";
        if(i==selected_)for(int yy=y+5;yy<y+12;++yy)for(int x=20;x<24;++x)image.canvas()[yy*640+x]=0xffffff70;ui.label(image,label,32,y,.48f,color);ui.label(image,value,245,y,.48f,color);
    }
    if(capture_){panel(image,28,188,584,96);ui.label(image,gamepad_?"Press a controller button or move a stick":"Press a new keyboard key",48,209,.68f);ui.label(image,"Escape: cancel",48,243,.58f);}
    ui.label(image,status_.empty()?(selected_==action_count||selected_==action_count+1||selected_==third_person_row||selected_==video_row?"Left/Right: change value":"OK: rebind; Left/Right: keyboard/controller"):status_,22,436,.45f);
    ui.label(image,"Back: "+config_.describe(Action::Cancel,false)+" / "+config_.describe(Action::Cancel,true),22,457,.45f);
}
}
