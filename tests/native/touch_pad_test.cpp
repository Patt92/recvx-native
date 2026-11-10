// Touch gamepad: layout (overlay, portrait and half-folded split), control anchors, touch ownership
// and the input manager's handling of the virtual controller it feeds. The layout and ownership cases
// follow nxnative's Duo harness (dm/ios/tests/duo/Tests/DuoHarnessTests.m), run here on touch_pad.cpp.
#define SDL_MAIN_HANDLED
#include "input.h"
#include "touch_pad.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
using namespace recvx::touch;
static void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static bool near(double a,double b,double tolerance=0.5){return std::fabs(a-b)<=tolerance;}
static bool same(const Rect& a,const Rect& b){return a.equals(b);}
static std::vector<std::string> failures;
static void section(const char* name,const std::function<void()>& body){try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}}
constexpr double wide=16.0/9.0;
static std::uint32_t bit(Key key){return 1u<<key;}
// The split layout with its controls trimmed to the safe area, as the harness lays its pad out.
static Layout portrait_controls(Rect bounds,Insets insets,Rect division){
    Layout l=layout(bounds,insets,false,division,wide);const Rect safe=bounds.inset(insets);const double top=l.controls.min_y();
    l.controls={safe.min_x(),top,safe.w,std::max(0.0,safe.max_y()-top)};
    return l;
}
static Pad pad_for(Rect bounds,const Layout& l,bool full=true){Pad pad;pad.set_full(full);pad.arrange(bounds,{},l);return pad;}
static double unit(const Layout& l,Rect bounds){return l.separated?std::min(std::min(l.controls.h/430.0,l.controls.w/700.0),1.3):std::min(bounds.h/430.0,1.3);}

static void layout_cases(){
    const Rect landscape{0,0,844,390};
    const Layout overlay=layout(landscape,{},false,Rect::none(),wide);
    check(!overlay.separated&&same(overlay.controls,landscape)&&same(overlay.picture,landscape),"landscape is not the overlay layout");
    check(near(overlay.game.w/overlay.game.h,wide,0.001)&&same(overlay.game,aspect_fit(landscape,wide)),"overlay picture is not fitted");

    const Rect portrait{0,0,390,844};const Insets safe{59,0,34,0};const Rect division{0,400,390,24};
    const Layout separated=layout(portrait,safe,false,division,wide);
    check(separated.separated,"portrait ignores a valid horizontal fold");
    check(near(separated.game.min_y(),119,1)&&separated.game.max_y()<=division.min_y(),"portrait picture is not above the fold");
    check(same(separated.controls,{0,424,390,420}),"portrait controls are not below the fold");
    check(same(separated.picture,{0,59,390,341}),"portrait picture area is not the safe part above the fold");
    check(separated.game.max_y()<=separated.controls.min_y(),"picture and controls overlap");
    const Layout clipped=layout(portrait,safe,false,{-50,400,500,24},wide);
    check(clipped.separated&&near(clipped.controls.min_y(),424,0.001)&&near(clipped.controls.w,portrait.w,0.001),"a fold wider than the view does not split its visible part");
    const Layout flat=layout(portrait,safe,false,{0,300,390,0},wide);
    check(flat.separated&&near(flat.controls.min_y(),300,0.001)&&flat.game.max_y()<=300,"a zero-height fold is not used as the split");
    check(layout(portrait,safe,true,division,wide).separated,"half-open portrait does not split");
    check(layout(portrait,safe,false,Rect::none(),wide).separated,"portrait without a fold does not split at the midline");
    check(layout(portrait,safe,true,{500,400,500,24},wide).separated,"a fold outside the view breaks the portrait split");
    check(layout(portrait,safe,false,{180,0,24,844},wide).separated,"a vertical fold breaks the portrait split");

    const Rect sizes[]={{0,0,390,844},{0,0,320,568}};const Insets insets[]={{59,0,34,0},{50,0,24,0}};
    for(int i=0;i<2;++i){
        const Rect b=sizes[i];const double split=b.mid_y();const Rect inner=b.inset(insets[i]);
        const Layout l=layout(b,insets[i],false,Rect::none(),wide);
        check(l.separated&&same(l.controls,{0,split,b.w,b.h-split}),"midline split differs");
        check(near(l.game.w/l.game.h,wide,0.001)&&l.game.min_y()>=inner.min_y()&&l.game.max_y()<=split,"midline picture leaves its area");
        const Layout classic=layout(b,insets[i],false,Rect::none(),4.0/3.0); // the game's own 4:3 picture
        check(near(classic.game.w/classic.game.h,4.0/3.0,0.001)&&classic.game.min_y()>=classic.picture.min_y()&&classic.game.max_y()<=classic.picture.max_y()+0.001,"4:3 picture leaves its area");
    }

    check(!layout(landscape,{},false,{0,180,844,24},wide).separated,"a fully open landscape foldable leaves the overlay layout");
    const Layout tabletop=layout(landscape,{},true,{0,180,844,24},wide);
    check(tabletop.separated&&near(tabletop.game.max_y(),180,1)&&near(tabletop.controls.min_y(),204,0.001),"half-open landscape does not split at the fold");
    check(same(tabletop.picture,{0,0,844,180}),"half-open landscape picture area is not the upper half");
    check(same(layout(portrait,safe,false,Rect::none(),wide).controls,{0,422,390,422}),"portrait without a hinge status does not split at the midline");
    check(same(horizontal_division(landscape,{0,0,844,24}),Rect::none())&&same(horizontal_division(landscape,{0,366,844,24}),Rect::none()),"a fold on the view's edge counts as a division");
    const Insets upright=game_safe_insets(portrait,{59,10,34,12},true),flatInsets=game_safe_insets(landscape,{0,59,21,59},true);
    check(upright.top==59&&upright.left==0&&upright.bottom==0&&upright.right==0&&flatInsets.left==59&&flatInsets.bottom==21,"foldable safe insets differ");
}

static void landscape_controls(){
    const Rect bounds{0,0,844,390};const Layout l=layout(bounds,{},false,Rect::none(),wide);
    Pad pad=pad_for(bounds,l);const double u=unit(l,bounds);
    check(near(pad.u(),u,0.0001)&&near(pad.stick_home().x,81*u)&&near(pad.stick_home().y,390-82*u),"landscape stick anchor differs");
    check(near(pad.face_center().x,844-81*u)&&near(pad.face_center().y,390-82*u),"landscape face anchor differs");
    const Point stick{81*u,390-82*u},dpad{231*u,390-64*u},face=pad.face_pos(FaceRight);
    check(near(pad.dpad_center().x,dpad.x)&&near(pad.dpad_center().y,dpad.y),"landscape D-pad anchor differs");
    pad.begin(1,stick);pad.begin(2,{dpad.x+0.6*48*u,dpad.y});pad.begin(3,face);pad.begin(4,{bounds.mid_x(),bounds.mid_y()});
    pad.move(1,{stick.x+30*u,stick.y});
    check(pad.state().axes[0]>0&&pad.state().axes[1]==0,"left stick lost its touch among four");
    check(pad.state().keys==(bit(Right)|bit(FaceRight)),"D-pad and face keys do not keep separate ownership");
    check(pad.state().axes[2]==0&&pad.state().axes[3]==0,"a touch on the picture outside the right third moved the right stick");
    for(Pad::TouchId id=1;id<=4;++id)pad.end(id);
    check(pad.state().keys==0&&pad.state().axes[0]==0,"released touches leave input held");
}

static void portrait_controls_cases(){
    struct Case {Rect bounds;Insets insets;Rect division;};
    const Case cases[]={{{0,0,669,951},{62,0,34,0},{0,455.5,669,40}},{{0,0,390,844},{59,0,34,0},Rect::none()},{{0,0,320,568},{50,0,24,0},Rect::none()}};
    for(const auto& c:cases){
        const Layout l=portrait_controls(c.bounds,c.insets,c.division);
        check(l.separated&&l.game.max_y()<=l.controls.min_y(),"portrait case has no separate controls panel");
        Pad pad=pad_for(c.bounds,l);const Rect lower=l.controls;const double u=unit(l,c.bounds);
        const Point left{lower.min_x()+0.28*lower.w,lower.min_y()+0.58*lower.h},right{lower.min_x()+0.72*lower.w,left.y};
        check(near(pad.stick_home().x,left.x-(48*u+6*u))&&near(pad.stick_home().y,left.y),"3D portrait stick is not left of the D-pad at the left anchor");
        check(near(pad.face_center().x,right.x)&&near(pad.face_center().y,right.y),"portrait face diamond is not at the right anchor");
        const Point dpad{left.x+(77*u+6*u),left.y};
        check(near(dpad.x-48*u-(pad.stick_home().x+pad.stick_r(0)),12*u),"stick and D-pad lose their 12 unit gap");
        pad.begin(1,{dpad.x+0.6*48*u,dpad.y});check(pad.state().keys==bit(Right),"portrait D-pad target is not at its position");pad.end(1);
        check(pad.stick_home().x-pad.stick_r(0)>=lower.min_x()-0.5&&pad.stick_home().x+pad.stick_r(0)<=lower.max_x()+0.5,"portrait stick leaves the controls panel");
        check(pad.face_center().x-77*u>=lower.min_x()-0.5&&pad.face_center().x+77*u<=lower.max_x()+0.5,"portrait face targets leave the controls panel");
    }
    {   // 2D: the stick itself sits on the left anchor and there is no D-pad
        const Rect bounds{0,0,390,844};const Layout l=portrait_controls(bounds,{59,0,34,0},Rect::none());
        Pad pad=pad_for(bounds,l,false);const Rect lower=l.controls;const double u=unit(l,bounds);
        const Point left{lower.min_x()+0.28*lower.w,lower.min_y()+0.58*lower.h};
        check(near(pad.stick_home().x,left.x)&&near(pad.stick_home().y,left.y),"2D portrait stick is not at the left anchor");
        pad.begin(1,left);pad.move(1,{left.x+30*u,left.y});check(pad.state().axes[0]>0,"2D portrait stick does not drive the left stick");pad.end(1);
        pad.begin(2,{left.x+(77+6+0.6*48)*u,left.y});check(!(pad.state().keys&bit(Right)),"2D layout exposes the D-pad");pad.end(2);
    }
    {   // 3D on an unfolded foldable: stick, D-pad and a face key at once; a touch on the picture is nobody's
        const Rect bounds{0,0,669,951};const Layout l=portrait_controls(bounds,{62,0,34,0},{0,455.5,669,40});
        Pad pad=pad_for(bounds,l);const Rect lower=l.controls;const double u=unit(l,bounds);
        const Point left{lower.min_x()+0.28*lower.w,lower.min_y()+0.58*lower.h},stick{left.x-54*u,left.y},dpad{left.x+83*u,left.y};
        pad.begin(1,stick);pad.begin(2,{dpad.x+0.6*48*u,dpad.y});pad.begin(3,{lower.min_x()+0.72*lower.w+50*u,left.y});pad.begin(4,{l.game.mid_x(),l.game.mid_y()});
        pad.move(1,{stick.x+30*u,stick.y});
        check(pad.state().axes[0]>0&&pad.state().keys==(bit(Right)|bit(FaceRight)),"portrait controls do not keep separate ownership");
        check(!pad.control_at({l.game.mid_x(),l.game.mid_y()}),"a touch on the picture is taken by a control");
        for(Pad::TouchId id=1;id<=4;++id)pad.end(id);
        check(pad.state().keys==0,"portrait release leaves keys held");
    }
}

static void hit_targets(){
    const Rect bounds{0,0,390,844};const Layout l=layout(bounds,{},false,Rect::none(),wide);
    Pad pad=pad_for(bounds,l);const double u=unit(l,bounds);
    check(near(u,390.0/700.0,0.001),"small split unit differs");
    const auto press=[&](Point p){pad.begin(9,p);const auto keys=pad.state().keys;pad.end(9);return keys;};
    // 44 point targets around artwork that keeps its scaled size: probe just inside, inward from each face key.
    const Key faces[]={FaceRight,FaceBottom,FaceTop,FaceLeft};const Point inward[]={{-21.5,0},{0,-21.5},{0,21.5},{21.5,0}};
    for(int i=0;i<4;++i){
        const Point c=pad.face_pos(faces[i]);
        check(pad.face_r()*2<44,"face artwork grew with its target");
        check(press({c.x+inward[i].x,c.y+inward[i].y})==bit(faces[i]),"face key misses its 44 point target");
    }
    const Key corner[]={TriggerL,ShoulderL,TriggerR,ShoulderR,StickL,StickR};const Point outward[]={{0,-21.5},{0,21.5},{0,-21.5},{0,21.5},{21.5,0},{-21.5,0}};
    for(int i=0;i<6;++i){
        const Key k=corner[i];const Point c=k==StickL||k==StickR?pad.click_center(k):pad.drop_center(k);
        check(press({c.x+outward[i].x,c.y+outward[i].y})==bit(k),"corner key misses its 44 point target");
    }
    for(const Key k:{Back,Start}){const Rect pill=pad.pill_rect(k);check(pill.h<28&&press({pill.mid_x(),pill.mid_y()+13.25})==bit(k),"Back / Start miss their 28 point target");}
    const Point menu=pad.menu_center();
    check(Pad::menu_r*u*2<28&&pad.menu_takes({menu.x,menu.y+13.7}),"menu button does not answer outside its drawn circle");

    {   // sliding across the diamond switches keys, between two keys both are held
        const Point a=pad.face_pos(FaceRight),b=pad.face_pos(FaceBottom);
        pad.begin(1,a);check(pad.state().keys==bit(FaceRight),"face touch-down misses its key");
        pad.move(1,{(a.x+b.x)/2,(a.y+b.y)/2});check(pad.state().keys==(bit(FaceRight)|bit(FaceBottom)),"between two face keys both are not held");
        pad.move(1,b);check(pad.state().keys==bit(FaceBottom),"sliding to the next face key does not switch");
        pad.end(1);check(pad.state().keys==0,"face release leaves a key held");
    }
    {   // overlapping locked targets: the nearest normalized one wins and stays owned
        const Point lt=pad.drop_center(TriggerL),l3=pad.click_center(StickL);
        pad.begin(1,{lt.x+(l3.x-lt.x)*0.6,lt.y});check(pad.state().keys==bit(StickL),"nearest of two overlapping locked targets does not win");
        pad.move(1,lt);check(pad.state().keys==bit(StickL),"a locked key changes owner while the finger moves");
        pad.end(1);
    }
    {   // from Back to the menu button the owner changes once; a Back touch slid onto the menu stays Back
        const Rect pill=pad.pill_rect(Back);const Point back{pill.mid_x(),pill.mid_y()};
        check(!pad.menu_takes(back)&&pad.menu_takes(menu),"Back and the menu button do not own their centres");
        bool last=false;int flips=0;
        for(int i=0;i<=40;++i){const bool now=pad.menu_takes({back.x+(menu.x-back.x)*i/40.0,back.y+(menu.y-back.y)*i/40.0});if(now!=last){++flips;last=now;}}
        check(flips==1,"owner between Back and the menu button changes more than once");
        pad.begin(1,back);pad.move(1,menu);check(pad.state().keys==bit(Back),"a Back touch slid onto the menu button is lost");pad.end(1);
    }
}

static void sticks_and_state(){
    const Rect bounds{0,0,932,430};const Layout l=layout(bounds,{},false,Rect::none(),wide);
    Pad pad=pad_for(bounds,l);const double radius=pad.stick_r(0);const Point home=pad.stick_home();
    check(near(pad.u(),1.0,0.0001),"a 430 point tall view is not one unit");
    pad.begin(1,home);check(pad.take_press_edge()&&!pad.take_press_edge(),"gripping the stick is not one press edge");
    pad.move(1,{home.x+radius*0.05,home.y});check(pad.state().axes[0]==0,"stick dead zone is not dead");
    pad.move(1,{home.x+radius*0.8,home.y});check(pad.state().axes[0]==stick_max&&pad.state().axes[1]==0,"full tilt at 0.8 of the ring is not exactly the maximum");
    pad.move(1,{home.x,home.y-radius*3});check(pad.state().axes[1]==-stick_max&&near(pad.knob_offset(0).y,-radius,0.001),"finger up is not stick up, or the knob leaves the ring");
    pad.move(1,{home.x-radius*0.45,home.y});check(pad.state().axes[0]<0&&pad.state().axes[0]>-stick_max,"partial tilt is not partial");
    check(!pad.take_press_edge(),"moving a held stick is a press edge");
    pad.end(1);check(pad.state().axes[0]==0&&!pad.stick_held(0),"stick release leaves it tilted");

    const Point free{bounds.w*0.8,bounds.h*0.45}; // right third, on no control
    check(pad.control_at(free)&&pad.key_at(free)<0,"right third does not offer the right stick");
    pad.begin(2,free);check(pad.state().axes[2]==0&&pad.state().axes[3]==0,"right stick is not centred where its touch lands");
    pad.move(2,{free.x+30,free.y});check(pad.state().axes[2]>0,"right stick does not follow a drag");
    pad.move(2,{free.x+300,free.y});check(pad.state().axes[2]==stick_max,"right stick is not at full tilt past its radius");
    pad.move(2,{free.x+300-pad.stick_r(1)*0.5,free.y});check(pad.state().axes[2]<stick_max,"right stick centre did not follow the finger: turning back does not respond at once");
    pad.end(2);
    pad.set_full(false);check(!pad.control_at(free),"2D layout has a right stick");
    pad.begin(2,free);pad.move(2,{free.x+60,free.y});check(pad.state().axes[2]==0,"2D layout moved the right stick");pad.end(2);
    const Point click=pad.click_center(StickL);pad.begin(3,click);check(!(pad.state().keys&bit(StickL)),"2D layout has stick clicks");pad.end(3);
    pad.set_full(true);

    const Point dpad=pad.dpad_center();const double arm=pad.dpad_r()*0.7,diagonal=arm*std::sqrt(0.5);
    pad.begin(4,{dpad.x+diagonal,dpad.y-diagonal});check(pad.state().keys==(bit(Up)|bit(Right)),"D-pad diagonal is not two keys");
    pad.move(4,{dpad.x,dpad.y+arm});check(pad.state().keys==bit(Down),"D-pad does not follow the finger");
    pad.move(4,dpad);check(pad.state().keys==0,"D-pad centre presses a direction");
    pad.end(4);

    pad.begin(5,pad.drop_center(ShoulderR));pad.begin(6,pad.face_pos(FaceBottom));
    check(pad.state().keys==(bit(ShoulderR)|bit(FaceBottom)),"aim and attack cannot be held together");
    check(!pad.arrange(bounds,{},l)&&pad.state().keys!=0,"an unchanged layout released the keys");
    check(pad.arrange({0,0,430,932},{},layout({0,0,430,932},{},false,Rect::none(),wide))&&pad.state().keys==0,"a pose change leaves keys held");
    pad.arrange(bounds,{},l);
    pad.begin(7,pad.face_pos(FaceTop));pad.set_hidden(true);
    check(pad.state().keys==0,"hiding the gamepad leaves a key held");
    pad.begin(8,pad.face_pos(FaceTop));check(pad.state().keys==0&&!pad.control_at(pad.face_pos(FaceTop)),"a hidden gamepad takes touches");pad.end(8);
    check(pad.menu_takes(pad.menu_center()),"the menu button is gone while the gamepad hides");
    pad.set_hidden(false);pad.begin(8,pad.face_pos(FaceTop));check(pad.state().keys==bit(FaceTop),"the gamepad does not come back");pad.end(8);

    // Safe area: the top row stays inside it, the bottom corners reach halfway into the side inset.
    const Insets notch{0,59,21,59};Pad inset;inset.arrange(bounds,notch,l);
    check(near(inset.drop_center(TriggerL).x,59+44)&&near(inset.stick_home().x,59*0.5+81)&&near(inset.stick_home().y,430-21-82),"safe area anchors differ");
}

// ---- The virtual controller in the input manager ----
static void pump(InputManager& a,InputManager* b=nullptr){SDL_GameControllerUpdate();SDL_Event event;while(SDL_PollEvent(&event)){a.event(event);if(b)b->event(event);}}
static SDL_Joystick* attach(bool touch_pad){
    SDL_VirtualJoystickDesc desc;SDL_zero(desc);
    desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;desc.naxes=SDL_CONTROLLER_AXIS_MAX;desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;
    if(touch_pad){desc.vendor_id=InputManager::touch_pad_vendor;desc.product_id=InputManager::touch_pad_product;desc.name="RECVX Touch";}
    const int index=SDL_JoystickAttachVirtualEx(&desc);check(index>=0&&SDL_IsGameController(index),"virtual controller attach failed");
    SDL_Joystick* joystick=SDL_JoystickOpen(index);check(joystick,"virtual joystick open failed");
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,SDL_JOYSTICK_AXIS_MIN);SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,SDL_JOYSTICK_AXIS_MIN);
    return joystick;
}
static void virtual_controller(){
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");check(SDL_Init(SDL_INIT_GAMECONTROLLER)==0,"SDL controller init failed");
    struct Quit{~Quit(){SDL_Quit();}} quit;
    const int present=SDL_NumJoysticks(); // real controllers on this machine take slots of their own
    SDL_Joystick* touch_pad=attach(true); // attached first: it must still leave slot 0 to the first real controller
    const auto config=InputConfig::defaults();
    {InputManager merged(config);pump(merged);
        check(merged.touch_pad(),"touch gamepad not recognised");
        check(merged.controllers()==std::size_t(present),"touch gamepad counts as a connected controller");
        merged.tick();
        SDL_JoystickSetVirtualButton(touch_pad,SDL_CONTROLLER_BUTTON_A,1);pump(merged);check(merged.tick().hit(Action::Confirm),"touch gamepad A is not Confirm");
        SDL_JoystickSetVirtualButton(touch_pad,SDL_CONTROLLER_BUTTON_A,0);pump(merged);merged.tick();
        check(!merged.tick().down(Action::Run),"a resting touch trigger holds Run");
        SDL_JoystickSetVirtualAxis(touch_pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,SDL_JOYSTICK_AXIS_MAX);pump(merged);check(merged.tick().down(Action::Run),"touch RT is not Run");
        SDL_JoystickSetVirtualAxis(touch_pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,SDL_JOYSTICK_AXIS_MIN);pump(merged);
        SDL_JoystickSetVirtualAxis(touch_pad,SDL_CONTROLLER_AXIS_LEFTY,-stick_max);pump(merged);{const auto frame=merged.tick();check(frame.move()==1&&frame.gamepad_input,"touch stick up is not forward");}
        SDL_JoystickSetVirtualAxis(touch_pad,SDL_CONTROLLER_AXIS_LEFTY,0);pump(merged);merged.tick();
    }
    if(present){std::cout<<"touch gamepad slots: skipped, "<<present<<" real controller(s) attached to this machine\n";return;}
    SDL_Joystick* first=attach(false);SDL_Joystick* second=attach(false);
    InputManager player1(config,{true,0}),player2(config,{false,1});pump(player1,&player2);
    check(player1.controllers()==2&&player2.controllers()==2&&player1.touch_pad(),"two controllers and the touch gamepad are not seen");
    player1.tick();player2.tick();
    const auto press=[&](SDL_Joystick* joystick,bool down){SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,down?1:0);pump(player1,&player2);};
    press(touch_pad,true);check(player1.tick().hit(Action::Confirm)&&!player2.tick().hit(Action::Confirm),"touch gamepad is not player 1's alone");press(touch_pad,false);player1.tick();player2.tick();
    press(first,true);check(player1.tick().hit(Action::Confirm)&&!player2.tick().hit(Action::Confirm),"the first real controller is not player 1: the touch gamepad took its slot");press(first,false);player1.tick();player2.tick();
    press(second,true);check(!player1.tick().hit(Action::Confirm)&&player2.tick().hit(Action::Confirm),"the second real controller is not player 2");press(second,false);
}

int main(){
    section("layout",layout_cases);
    section("landscape controls",landscape_controls);
    section("portrait controls",portrait_controls_cases);
    section("hit targets",hit_targets);
    section("sticks and state",sticks_and_state);
    section("virtual controller",virtual_controller);
    for(const auto& failure:failures)std::cerr<<"touch pad: "<<failure<<'\n';
    if(failures.empty())std::cout<<"touch pad: layout, ownership and virtual controller verified\n";
    return failures.empty()?0:1;
}
