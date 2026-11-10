// On-screen touch gamepad for iOS/iPadOS: a transparent window over SDL's. Host adaptation, no game logic.
// Layout and touch ownership are touch_pad.cpp (ported from nxnative's DmGamepad.m / DmLayout.h; the
// licence is in touch_pad.h); this file draws the controls as nxnative does and feeds an SDL virtual
// game controller, so controls.ini, the OPTION rebinding screen and the menus see an ordinary pad.
//   Two layouts, picked in the menu the gear at the top centre opens and kept in NSUserDefaults:
//   2D: triggers above the shoulders in the top corners, Back and Start at the top centre, the left
//     stick bottom left, the face diamond bottom right (sliding a finger between keys switches keys).
//   3D: plus a D-pad inward of the left stick, L3 / R3 in the top row and a right stick that is not
//     drawn (a touch in the right third of the screen on no control holds it).
//   Keys are labelled by position in Xbox names, as the game's prompts and controls.ini name them.
//   One CAShapeLayer per control: a press only swaps the fill, a stick only moves its knob.
//   The gamepad (all but the gear) hides while a real controller is connected.
//   Landscape: the controls lie over the picture. Portrait, or a foldable phone half open: the picture
//   sits above the controls, and the SDL view is placed on that part (ios_scene.mm ios_fit_view).
#import <UIKit/UIKit.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
#import <CoreText/CoreText.h>
#include <TargetConditionals.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <cmath>
#include <iostream>
#include "ios_touch_pad.h"
#include "touch_pad.h"

namespace touch=recvx::touch;
using touch::Key;

static CGPoint cg(touch::Point p){return CGPointMake(p.x,p.y);}
static touch::Point pt(CGPoint p){return {p.x,p.y};}
static CGRect cg(const touch::Rect& r){return r.null?CGRectNull:CGRectMake(r.x,r.y,r.w,r.h);}
static touch::Rect rc(CGRect r){return CGRectIsNull(r)?touch::Rect::none():touch::Rect{r.origin.x,r.origin.y,r.size.width,r.size.height};}
static touch::Insets ins(UIEdgeInsets i){return {i.top,i.left,i.bottom,i.right};}

static NSString* const k_layout_key=@"RECVXTouchLayout";     // NSUserDefaults: 0 = 2D, 1 = 3D
static NSString* const k_feedback_key=@"RECVXTouchFeedback"; // NSUserDefaults: touch feedback on / off
static const double k_picture_aspect=4.0/3.0;                // Layout::game only; the SDL view takes Layout::picture

// ---- The SDL virtual controller ----
static SDL_Joystick* g_joystick;
static void publish_state(const touch::State& s){
    if(!g_joystick)return;
    static const struct {Key key;SDL_GameControllerButton button;} buttons[]={
        {touch::FaceRight,SDL_CONTROLLER_BUTTON_B},{touch::FaceBottom,SDL_CONTROLLER_BUTTON_A},{touch::FaceTop,SDL_CONTROLLER_BUTTON_Y},{touch::FaceLeft,SDL_CONTROLLER_BUTTON_X},
        {touch::StickL,SDL_CONTROLLER_BUTTON_LEFTSTICK},{touch::StickR,SDL_CONTROLLER_BUTTON_RIGHTSTICK},{touch::ShoulderL,SDL_CONTROLLER_BUTTON_LEFTSHOULDER},{touch::ShoulderR,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
        {touch::Start,SDL_CONTROLLER_BUTTON_START},{touch::Back,SDL_CONTROLLER_BUTTON_BACK},
        {touch::Left,SDL_CONTROLLER_BUTTON_DPAD_LEFT},{touch::Up,SDL_CONTROLLER_BUTTON_DPAD_UP},{touch::Right,SDL_CONTROLLER_BUTTON_DPAD_RIGHT},{touch::Down,SDL_CONTROLLER_BUTTON_DPAD_DOWN}};
    for(const auto& b:buttons)SDL_JoystickSetVirtualButton(g_joystick,b.button,Uint8((s.keys>>b.key)&1u));
    static const SDL_GameControllerAxis sticks[]={SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY};
    for(int i=0;i<4;++i)SDL_JoystickSetVirtualAxis(g_joystick,sticks[i],Sint16(s.axes[i]));
    // A trigger rests at the axis minimum (SDL reads the whole axis range as 0..1).
    SDL_JoystickSetVirtualAxis(g_joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,(s.keys>>touch::TriggerL)&1u?SDL_JOYSTICK_AXIS_MAX:SDL_JOYSTICK_AXIS_MIN);
    SDL_JoystickSetVirtualAxis(g_joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,(s.keys>>touch::TriggerR)&1u?SDL_JOYSTICK_AXIS_MAX:SDL_JOYSTICK_AXIS_MIN);
}

// ---- Game rumble on the phone's haptic engine ----
// The runtime rumbles every controller it accepts (InputManager::rumble); for the virtual one SDL calls
// back here. One looping continuous haptic whose intensity follows the level; a device without a haptic
// engine (iPad, simulator) does nothing.
static CHHapticEngine* g_engine;
static id<CHHapticAdvancedPatternPlayer> g_player;
static bool g_rumbling;
static void rumble_stop(){
    if(g_rumbling){[g_player stopAtTime:CHHapticTimeImmediate error:nil];g_rumbling=false;}
}
static void rumble_set(float intensity){
    if(intensity<=0){rumble_stop();return;}
    if(!CHHapticEngine.capabilitiesForHardware.supportsHaptics)return;
    @try{
        NSError* error=nil;
        if(!g_engine){
            g_engine=[[CHHapticEngine alloc] initAndReturnError:&error];if(!g_engine)return;
            g_engine.playsHapticsOnly=YES;
            g_engine.stoppedHandler=^(CHHapticEngineStoppedReason reason){(void)reason;dispatch_async(dispatch_get_main_queue(),^{g_engine=nil;g_player=nil;g_rumbling=false;});};
            g_engine.resetHandler=^{dispatch_async(dispatch_get_main_queue(),^{g_engine=nil;g_player=nil;g_rumbling=false;});};
            if(![g_engine startAndReturnError:&error]){g_engine=nil;return;}
        }
        if(!g_player){
            CHHapticEvent* event=[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                parameters:@[[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1],[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:0.3f]]
                relativeTime:0 duration:10];
            CHHapticPattern* pattern=[[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];if(!pattern)return;
            g_player=[g_engine createAdvancedPlayerWithPattern:pattern error:&error];if(!g_player)return;
            g_player.loopEnabled=YES;
        }
        CHHapticDynamicParameter* level=[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:fminf(intensity,1) relativeTime:0];
        [g_player sendParameters:@[level] atTime:CHHapticTimeImmediate error:nil];
        if(!g_rumbling)g_rumbling=[g_player startAtTime:CHHapticTimeImmediate error:&error];
    }@catch(NSException* exception){(void)exception;g_engine=nil;g_player=nil;g_rumbling=false;} // an engine that stops under a call raises
}
static int SDLCALL rumble_callback(void* userdata,Uint16 low,Uint16 high){
    (void)userdata;const float intensity=float(low>high?low:high)/65535.f;
    if(NSThread.isMainThread)rumble_set(intensity);else dispatch_async(dispatch_get_main_queue(),^{rumble_set(intensity);});
    return 0;
}

// ---- The view ----
@interface RECVXTouchPad:UIView
-(BOOL)menuTakes:(CGPoint)p;
-(touch::Layout)measuredLayout;
-(void)releaseAll;
@end
// The gear: a UIButton whose menu UIKit draws. Its hit circle is larger than the drawn one and a key
// nearer to the touch wins (Back / Start sit beside it), so it asks the pad.
@interface RECVXTouchMenuButton:UIButton
@end
@implementation RECVXTouchMenuButton
-(BOOL)pointInside:(CGPoint)p withEvent:(UIEvent*)event{(void)event;RECVXTouchPad* pad=(RECVXTouchPad*)self.superview;return [pad menuTakes:[self convertPoint:p toView:pad]];}
@end
static __weak RECVXTouchPad* g_pad;
static __weak UIWindow* g_sdl_window; // SDL's window, below the gamepad's

static UIColor *k_idle,*k_on,*k_line,*k_ring_fill,*k_text,*k_text_on;
static CAShapeLayer* shape(CALayer* parent){
    CAShapeLayer* l=[CAShapeLayer layer];
    l.fillColor=k_idle.CGColor;l.strokeColor=k_line.CGColor;l.lineWidth=1.5;
    l.shadowColor=UIColor.blackColor.CGColor;l.shadowOpacity=0.2;l.shadowRadius=3;l.shadowOffset=CGSizeMake(0,1);
    [parent addSublayer:l];return l;
}
static CATextLayer* label(CALayer* parent,NSString* text){
    CATextLayer* t=[CATextLayer layer];
    t.string=text;t.alignmentMode=kCAAlignmentCenter;t.foregroundColor=k_text.CGColor;
    [parent addSublayer:t];return t;
}
static CGPathRef circle_path(CGPoint c,CGFloat r){return CGPathCreateWithEllipseInRect(CGRectMake(c.x-r,c.y-r,2*r,2*r),NULL);}
static CGPathRef drop_path(CGPoint c,CGFloat r,CGFloat tipAngle,CGFloat round){ // teardrop: circle + two tangents meeting at a slightly rounded tip
    CGFloat d=r*touch::Pad::drop_tip,th=acos(r/d);
    CGPoint tip=CGPointMake(c.x+d*cos(tipAngle),c.y+d*sin(tipAngle));
    CGPoint p1=CGPointMake(c.x+r*cos(tipAngle+th),c.y+r*sin(tipAngle+th)),p2=CGPointMake(c.x+r*cos(tipAngle-th),c.y+r*sin(tipAngle-th));
    CGMutablePathRef p=CGPathCreateMutable();
    CGPathMoveToPoint(p,NULL,p2.x,p2.y);
    CGPathAddArcToPoint(p,NULL,tip.x,tip.y,p1.x,p1.y,round);
    CGPathAddArc(p,NULL,c.x,c.y,r,tipAngle+th,tipAngle-th+2*M_PI,false);
    CGPathCloseSubpath(p);return p;
}
static CGPathRef arm_path(CGPoint c,CGFloat R,CGFloat w,CGFloat inner,CGFloat angle,CGFloat round){ // one D-pad arm: an upward pentagon (tip toward the centre) turned by angle
    CGPoint v[5]={{-w,-R},{w,-R},{w,-(inner+w)},{0,-inner},{-w,-(inner+w)}};
    CGFloat cs=cos(angle),sn=sin(angle);
    for(int i=0;i<5;i++)v[i]=CGPointMake(c.x+v[i].x*cs-v[i].y*sn,c.y+v[i].x*sn+v[i].y*cs);
    CGMutablePathRef p=CGPathCreateMutable();
    CGPathMoveToPoint(p,NULL,(v[4].x+v[0].x)/2,(v[4].y+v[0].y)/2);
    for(int i=0;i<5;i++)CGPathAddArcToPoint(p,NULL,v[i].x,v[i].y,v[(i+1)%5].x,v[(i+1)%5].y,round);
    CGPathCloseSubpath(p);return p;
}
static void set_path(CAShapeLayer* l,CGPathRef p){l.path=p;l.shadowPath=p;CGPathRelease(p);}
static void place_label(CATextLayer* t,CGPoint c,CGFloat size){
    CTFontRef font=CTFontCreateUIFontForLanguage(kCTFontUIFontEmphasizedSystem,size,NULL);
    t.font=font;t.fontSize=size;
    CGFloat h=CTFontGetAscent(font)+CTFontGetDescent(font)+CTFontGetLeading(font),w=size*4;
    t.frame=CGRectMake(c.x-w/2,c.y-h/2,w,h);
    CFRelease(font);
}
static bool is_drop(unsigned k){return k==touch::TriggerL||k==touch::ShoulderL||k==touch::TriggerR||k==touch::ShoulderR;}
static bool is_click(unsigned k){return k==touch::StickL||k==touch::StickR;}
static const Key k_locked[]={touch::TriggerL,touch::ShoulderL,touch::TriggerR,touch::ShoulderR,touch::Back,touch::Start,touch::StickL,touch::StickR};

@implementation RECVXTouchPad{
    touch::Pad _pad;
    CAShapeLayer* _key[touch::KeyCount];CATextLayer* _label[touch::KeyCount]; // one layer per key
    CAShapeLayer *_ring,*_knob,*_knobInner;                                   // the left stick (the right one is not drawn)
    CALayer* _controls;               // every control layer: hidden while a real controller is connected
    RECVXTouchMenuButton* _menu;
    UIImpactFeedbackGenerator* _feedback; // one short feedback per press edge
    std::uint32_t _shownKeys;BOOL _stickShown;
    BOOL _partiallyOpen;NSInteger _hingeStatus; // a foldable phone's hinge (UIHingeInteraction, iOS 27.1)
    NSString* _layoutLog;
}
+(BOOL)requiresConstraintBasedLayout{return YES;}
-(void)updateConstraints{
#if __has_include(<UIKit/UIHingeInteraction.h>)
    if(@available(iOS 27.1,*)){ // reading here registers UIKit's automatic reserved-region invalidation
        [self reservedRegionsOfKind:UIViewReservedRegionKind.divisionRegionKind options:UIViewReservedRegionQueryOptionsIncludeInactive];
        [self setNeedsLayout];
    }
#endif
    [super updateConstraints];
}
-(instancetype)initWithFrame:(CGRect)frame{
    if((self=[super initWithFrame:frame])){
        self.multipleTouchEnabled=YES;self.opaque=NO;self.backgroundColor=UIColor.clearColor;
        static dispatch_once_t once;
        dispatch_once(&once,^{ // each colour carries its own low opacity; lowering the view's alpha would add an off-screen pass
            k_idle=[UIColor colorWithWhite:1 alpha:0.14];k_on=[UIColor colorWithWhite:1 alpha:0.45];k_line=[UIColor colorWithWhite:1 alpha:0.38];
            k_ring_fill=[UIColor colorWithWhite:1 alpha:0.06];k_text=[UIColor colorWithWhite:1 alpha:0.6];k_text_on=[UIColor colorWithWhite:0 alpha:0.7];
        });
        NSUserDefaults* defaults=NSUserDefaults.standardUserDefaults;
        NSNumber* savedLayout=[defaults objectForKey:k_layout_key];
        _pad.set_full(savedLayout?savedLayout.integerValue==1:true);
        NSNumber* savedFeedback=[defaults objectForKey:k_feedback_key];
        [self setTouchFeedback:savedFeedback?savedFeedback.boolValue:YES save:NO];
        _controls=[CALayer layer];[self.layer addSublayer:_controls];
        _ring=shape(_controls);_ring.fillColor=k_ring_fill.CGColor;
        _knob=shape(_controls);
        _knobInner=[CAShapeLayer layer];_knobInner.fillColor=nil;_knobInner.strokeColor=k_line.CGColor;_knobInner.lineWidth=1.5;
        [_knob addSublayer:_knobInner];
        static NSString* const names[touch::KeyCount]={@"B",@"A",@"Y",@"X",@"L3",@"R3",@"LB",@"RB",@"LT",@"RT",@"Start",@"Back"}; // the D-pad arms carry no label
        for(unsigned k=0;k<touch::KeyCount;k++){
            _key[k]=shape(_controls);
            if(names[k])_label[k]=label(_key[k],names[k]);
        }
        _menu=[RECVXTouchMenuButton buttonWithType:UIButtonTypeCustom];
        _menu.backgroundColor=k_idle;_menu.layer.borderColor=k_line.CGColor;_menu.layer.borderWidth=1.5;
        _menu.layer.shadowColor=UIColor.blackColor.CGColor;_menu.layer.shadowOpacity=0.2;_menu.layer.shadowRadius=3;_menu.layer.shadowOffset=CGSizeMake(0,1);
        _menu.tintColor=k_text;_menu.accessibilityLabel=@"Touch controls";
        __weak RECVXTouchPad* weakSelf=self;
        UIDeferredMenuElement* items=[UIDeferredMenuElement elementWithUncachedProvider:^(void(^done)(NSArray<UIMenuElement*>*)){ // built each time it opens, so it shows the current choices
            RECVXTouchPad* pad=weakSelf;done(pad?[pad menuItems]:@[]);
        }];
        _menu.menu=[UIMenu menuWithTitle:@"" children:@[items]];_menu.showsMenuAsPrimaryAction=YES;
        [_menu addAction:[UIAction actionWithHandler:^(UIAction* action){(void)action;[weakSelf releaseAll];}] forControlEvents:UIControlEventMenuActionTriggered]; // the menu opens: release every key first
        [self addSubview:_menu];
        [self applyLayoutChoice];
        NSNotificationCenter* center=NSNotificationCenter.defaultCenter;
        [center addObserver:self selector:@selector(padsChanged:) name:GCControllerDidConnectNotification object:nil];
        [center addObserver:self selector:@selector(padsChanged:) name:GCControllerDidDisconnectNotification object:nil];
        [center addObserver:self selector:@selector(resigned:) name:UIApplicationWillResignActiveNotification object:nil]; // a key held while switching away gets no release
        [self padsChanged:nil];
#if __has_include(<UIKit/UIHingeInteraction.h>)
        if(@available(iOS 27.1,*)){
            [self addInteraction:[[UIHingeInteraction alloc] initWithUpdateHandler:^(UIHingeInteraction* interaction,UIHingeInteractionUpdate* update){
                (void)interaction;[weakSelf hingeChanged:update.hinge];
            }]];
        }
#endif
    }
    return self;
}
#if __has_include(<UIKit/UIHingeInteraction.h>)
-(void)hingeChanged:(UIHinge*)hinge API_AVAILABLE(ios(27.1)){
    const NSInteger status=hinge?hinge.status:UIHingeStatusUnknown;
    if(_hingeStatus!=status){_hingeStatus=status;std::cout<<"Touch controls: hinge status "<<long(status)<<", angle "<<(hinge?hinge.angle:0)<<std::endl;}
    _partiallyOpen=status==UIHingeStatusPartiallyOpen;
    [self setNeedsUpdateConstraints];[self setNeedsLayout];
}
#endif
-(void)traitCollectionDidChange:(UITraitCollection*)previous{[super traitCollectionDidChange:previous];[self setNeedsLayout];}
-(void)safeAreaInsetsDidChange{[super safeAreaInsetsDidChange];[self setNeedsLayout];}
-(void)resigned:(NSNotification*)note{(void)note;[self releaseAll];rumble_stop();}

// ---- The gear's menu ----
-(NSArray<UIMenuElement*>*)menuItems{
    __weak RECVXTouchPad* weakSelf=self;
    NSMutableArray<UIMenuElement*>* layouts=[NSMutableArray array];
    NSArray<NSString*>* names=@[@"2D",@"3D"];
    for(NSInteger l=0;l<2;l++){
        UIAction* pick=[UIAction actionWithTitle:names[l] image:nil identifier:nil handler:^(UIAction* action){(void)action;[weakSelf setPadLayout:l];}];
        pick.state=(_pad.full()?1:0)==l?UIMenuElementStateOn:UIMenuElementStateOff;
        [layouts addObject:pick];
    }
    UIMenu* layout=[UIMenu menuWithTitle:@"Controller layout" image:nil identifier:nil options:UIMenuOptionsSingleSelection children:layouts];
    const BOOL on=_feedback!=nil;
    UIAction* feedback=[UIAction actionWithTitle:@"Touch feedback" image:nil identifier:nil handler:^(UIAction* action){(void)action;[weakSelf setTouchFeedback:!on save:YES];}];
    feedback.state=on?UIMenuElementStateOn:UIMenuElementStateOff;
    return @[[UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[layout,feedback]]];
}
-(void)setPadLayout:(NSInteger)layout{ // the menu's pick: no key stays held, save, lay out again
    _pad.set_full(layout==1);[self publish];
    [NSUserDefaults.standardUserDefaults setObject:@(layout) forKey:k_layout_key];
    [self applyLayoutChoice];[self setNeedsLayout];
}
-(void)setTouchFeedback:(BOOL)on save:(BOOL)save{
    if(!on)_feedback=nil;
    else if(!_feedback){_feedback=[[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];[_feedback prepare];}
    if(save)[NSUserDefaults.standardUserDefaults setBool:on forKey:k_feedback_key];
}
-(void)applyLayoutChoice{ // show / hide the controls of the current layout
    [CATransaction begin];[CATransaction setDisableActions:YES];
    for(unsigned k=0;k<touch::KeyCount;k++)_key[k].hidden=touch::Pad::full_only(Key(k))&&!_pad.full();
    [CATransaction commit];
}
-(void)padsChanged:(NSNotification*)note{ // a real controller hides the gamepad; the gear stays
    (void)note;BOOL real=NO;
    for(GCController* controller in GCController.controllers)if(controller.extendedGamepad)real=YES;
#if TARGET_OS_SIMULATOR
    real=NO; // the simulator always reports a built-in 'MFi Gamepad', which would keep the gamepad hidden there
#endif
    if(real!=_pad.hidden()){
        _pad.set_hidden(real);[self publish];
        std::cout<<"Touch controls: "<<(real?"hidden, a controller is connected":"shown")<<std::endl;
    }
    [CATransaction begin];[CATransaction setDisableActions:YES];_controls.hidden=real;[CATransaction commit];
}

// ---- Layout ----
-(touch::Layout)measuredLayout{ // from the view as it is now: bounds, safe area, hinge and fold
    const CGRect b=self.bounds;CGRect division=CGRectNull;
#if __has_include(<UIKit/UIHingeInteraction.h>)
    if(@available(iOS 27.1,*)){
        for(UIViewReservedRegion* region in [self reservedRegionsOfKind:UIViewReservedRegionKind.divisionRegionKind options:UIViewReservedRegionQueryOptionsIncludeInactive]){
            const touch::Rect candidate=touch::horizontal_division(rc(b),rc(region.frame));
            if(CGRectIsNull(division)&&!candidate.null)division=cg(candidate);
        }
    }
#endif
    const bool foldPortrait=b.size.height>b.size.width&&(_hingeStatus!=0||!CGRectIsNull(division));
    touch::Layout layout=touch::layout(rc(b),touch::game_safe_insets(rc(b),ins(self.safeAreaInsets),foldPortrait),_partiallyOpen,rc(division),k_picture_aspect);
    layout.top_safe_only=foldPortrait;
    return layout;
}
-(void)layoutSubviews{
    [super layoutSubviews];
    const touch::Layout layout=[self measuredLayout];
    if(_pad.arrange(rc(self.bounds),ins(self.safeAreaInsets),layout))[self publish];
    NSString* line=[NSString stringWithFormat:@"%@, hinge %ld%s, picture %@, controls %@",layout.separated?@"split":@"overlay",(long)_hingeStatus,_partiallyOpen?" (half open)":"",
                    NSStringFromCGRect(cg(layout.picture)),NSStringFromCGRect(cg(layout.controls))];
    if(![line isEqualToString:_layoutLog]){_layoutLog=line;std::cout<<"Touch controls: layout "<<line.UTF8String<<std::endl;}
    UIView* picture=g_sdl_window.rootViewController.view; // the SDL view: the whole window in the overlay layout, the part above the controls in the split one
    if(picture&&!CGRectEqualToRect(picture.frame,cg(layout.picture))){picture.frame=cg(layout.picture);[picture setNeedsLayout];}
    const CGFloat u=_pad.u();
    [CATransaction begin];[CATransaction setDisableActions:YES];
    const CGFloat scale=MAX(self.traitCollection.displayScale,1);
    for(unsigned k=0;k<touch::KeyCount;k++)if(_label[k]&&_label[k].contentsScale!=scale){_label[k].contentsScale=scale;[_label[k] setNeedsDisplay];}
    _controls.frame=self.layer.bounds;
    for(unsigned k=touch::FaceRight;k<=touch::FaceLeft;k++){const CGPoint c=cg(_pad.face_pos(Key(k)));set_path(_key[k],circle_path(c,_pad.face_r()));place_label(_label[k],c,20*u);}
    for(const Key k:k_locked){
        if(is_drop(k)){
            const bool right=k==touch::TriggerR||k==touch::ShoulderR;const CGPoint c=cg(_pad.drop_center(k));
            set_path(_key[k],drop_path(c,_pad.drop_r(k),right?-M_PI_4:-3*M_PI_4,4*u));place_label(_label[k],c,15*u); // tip toward the near top corner
        }else if(is_click(k)){
            const CGPoint c=cg(_pad.click_center(k));set_path(_key[k],circle_path(c,touch::Pad::click_r*u));place_label(_label[k],c,13*u);
        }else{
            const CGRect r=cg(_pad.pill_rect(k));
            set_path(_key[k],CGPathCreateWithRoundedRect(r,r.size.height/2,r.size.height/2,NULL));place_label(_label[k],CGPointMake(CGRectGetMidX(r),CGRectGetMidY(r)),12*u);
        }
    }
    static const struct {Key key;CGFloat angle;} arms[]={{touch::Up,0},{touch::Right,M_PI_2},{touch::Down,M_PI},{touch::Left,-M_PI_2}}; // the same arm in 90 degree steps
    for(const auto& arm:arms)set_path(_key[arm.key],arm_path(cg(_pad.dpad_center()),_pad.dpad_r(),touch::Pad::arm_w*u,touch::Pad::arm_in*u,arm.angle,6*u));
    const CGFloat mr=touch::Pad::menu_r*u;const CGPoint mc=cg(_pad.menu_center());
    _menu.frame=CGRectMake(mc.x-mr,mc.y-mr,2*mr,2*mr);_menu.layer.cornerRadius=mr;
    _menu.layer.shadowPath=[UIBezierPath bezierPathWithOvalInRect:_menu.bounds].CGPath;
    [_menu setImage:[UIImage systemImageNamed:@"gearshape.fill" withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:13*u weight:UIImageSymbolWeightSemibold]] forState:UIControlStateNormal];
    set_path(_ring,circle_path(cg(_pad.stick_home()),_pad.stick_r(0)));
    const CGFloat k=touch::Pad::knob_r*u;_knob.bounds=CGRectMake(0,0,2*k,2*k);
    set_path(_knob,circle_path(CGPointMake(k,k),k));
    CGPathRef inner=circle_path(CGPointMake(k,k),k*0.62);_knobInner.frame=_knob.bounds;_knobInner.path=inner;CGPathRelease(inner);
    [CATransaction commit];
    [self refresh];
}
-(void)refresh{ // pressed state to fill, the stick to its knob position
    const std::uint32_t all=_pad.state().keys;
    [CATransaction begin];[CATransaction setDisableActions:YES];
    for(unsigned k=0;k<touch::KeyCount;k++){const bool on=(all>>k)&1u;_key[k].fillColor=(on?k_on:k_idle).CGColor;_label[k].foregroundColor=(on?k_text_on:k_text).CGColor;}
    const touch::Point home=_pad.stick_home(),offset=_pad.knob_offset(0);
    _knob.position=CGPointMake(home.x+offset.x,home.y+offset.y);_knob.fillColor=(_pad.stick_held(0)?k_on:k_idle).CGColor;
    [CATransaction commit];
}

// ---- Touch ----
-(BOOL)menuTakes:(CGPoint)p{return _pad.menu_takes(pt(p));}
-(UIView*)hitTest:(CGPoint)p withEvent:(UIEvent*)event{ // hidden: only the gear is here, every other touch passes on to SDL's window
    UIView* view=[super hitTest:p withEvent:event];
    return view==self&&_pad.hidden()?nil:view;
}
-(void)publish{
    const touch::State& state=_pad.state();
    publish_state(state);
    if(_pad.take_press_edge()){[_feedback impactOccurredWithIntensity:0.6];[_feedback prepare];}
    const BOOL stick=_pad.stick_held(0); // only the left stick is drawn
    if(state.keys!=_shownKeys||stick||_stickShown)[self refresh]; // also on the stick's release, so the knob returns to the centre
    _shownKeys=state.keys;_stickShown=stick;
}
static touch::Pad::TouchId touch_id(UITouch* t){return touch::Pad::TouchId((__bridge void*)t);}
-(void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;for(UITouch* t in touches)_pad.begin(touch_id(t),pt([t locationInView:self]));[self publish];}
-(void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;for(UITouch* t in touches)_pad.move(touch_id(t),pt([t locationInView:self]));[self publish];}
-(void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;for(UITouch* t in touches)_pad.end(touch_id(t));[self publish];}
-(void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;for(UITouch* t in touches)_pad.end(touch_id(t));[self publish];}
-(void)releaseAll{_pad.reset();[self publish];}
@end

// The gamepad has a window of its own above SDL's: UIKit presents the gear's menu from a view controller, and
// the gamepad must cover the whole screen while the SDL view, in the split layout, covers only the part above
// the controls. The window passes on every touch its views do not take, and never becomes the key window.
@interface RECVXTouchPadController:UIViewController
@end
@implementation RECVXTouchPadController
-(void)loadView{self.view=[[RECVXTouchPad alloc] initWithFrame:CGRectZero];}
-(BOOL)prefersHomeIndicatorAutoHidden{return YES;}
-(UIRectEdge)preferredScreenEdgesDeferringSystemGestures{return UIRectEdgeAll;} // edge gestures need two swipes, the controls hug the edges
-(BOOL)prefersStatusBarHidden{return YES;}
-(UIInterfaceOrientationMask)supportedInterfaceOrientations{return UIInterfaceOrientationMaskAll;} // Info.plist narrows it
@end
@interface RECVXTouchWindow:UIWindow
@end
@implementation RECVXTouchWindow
-(UIView*)hitTest:(CGPoint)p withEvent:(UIEvent*)event{UIView* view=[super hitTest:p withEvent:event];return view==self?nil:view;}
-(BOOL)canBecomeKeyWindow{return NO;} // SDL's window keeps the keyboard and controller focus
@end
static RECVXTouchWindow* g_window;

// ios_scene.mm ios_fit_view: where the SDL view belongs in its window; CGRectNull without a touch gamepad.
extern "C" CGRect recvx_ios_touch_picture_frame(UIWindow* window){
    RECVXTouchPad* pad=g_pad;
    if(!pad||!window||window!=g_sdl_window)return CGRectNull;
    return cg([pad measuredLayout].picture);
}

namespace recvx {
void ios_touch_pad_attach(SDL_Window* window){
    if(g_pad)return;
    SDL_SysWMinfo info;SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(window,&info)||info.subsystem!=SDL_SYSWM_UIKIT){std::cerr<<"Touch controls unavailable: no UIKit window\n";return;}
    UIWindow* uiwindow=info.info.uikit.window;
    SDL_VirtualJoystickDesc desc;SDL_zero(desc);
    desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes=SDL_CONTROLLER_AXIS_MAX;desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;
    desc.vendor_id=touch::virtual_vendor;desc.product_id=touch::virtual_product;desc.name="RECVX Touch";
    desc.Rumble=rumble_callback;
    const int index=SDL_JoystickAttachVirtualEx(&desc);
    if(index<0){std::cerr<<"Touch controls unavailable: "<<SDL_GetError()<<'\n';return;}
    if(!(g_joystick=SDL_JoystickOpen(index))){std::cerr<<"Touch controls unavailable: "<<SDL_GetError()<<'\n';SDL_JoystickDetachVirtual(index);return;} // leave no orphan device behind
    publish_state(touch::State{}); // triggers to their rest value before the first touch
    uiwindow.backgroundColor=UIColor.blackColor; // behind the picture and the controls in the split layout
    g_sdl_window=uiwindow;
    g_window=uiwindow.windowScene?[[RECVXTouchWindow alloc] initWithWindowScene:uiwindow.windowScene]:[[RECVXTouchWindow alloc] initWithFrame:uiwindow.frame];
    g_window.windowLevel=uiwindow.windowLevel+1;g_window.backgroundColor=UIColor.clearColor;
    g_window.rootViewController=[RECVXTouchPadController new];
    RECVXTouchPad* pad=(RECVXTouchPad*)g_window.rootViewController.view;g_pad=pad;
    g_window.hidden=NO;
    [pad setNeedsLayout];[pad layoutIfNeeded];
}
}
