// On-screen touch gamepad: where the picture and the controls go, and which control a touch holds.
// Host adaptation, no game logic. Pure functions of the view's bounds, its safe area and a foldable
// phone's fold, so the host tests run them without a device (tests/native/touch_pad_test.cpp);
// native/ios_touch_pad.mm draws the controls and feeds an SDL virtual game controller from State.
//
// Ported from nxnative (https://github.com/seedds/nxnative), dm/ios/Sources/DmGamepad.m and
// DmLayout.h: the layout constants, the touch-down ownership rules, the stick curve and the
// portrait / half-folded split are its own.
// nxnative's licence, verbatim:
//   MIT License
//
//   Copyright (c) 2026 seedds
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files (the "Software"), to deal
//   in the Software without restriction, including without limitation the rights
//   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
//
//   The above copyright notice and this permission notice shall be included in all
//   copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//   SOFTWARE.
#pragma once
#include <array>
#include <cstdint>
#include <map>
namespace recvx::touch {
// The SDL virtual controller the touch gamepad feeds carries this vendor / product pair, so the
// input manager can tell it from a real controller (input.h).
constexpr std::uint16_t virtual_vendor=0x5243,virtual_product=0x5450;
struct Point {double x=0,y=0;};
struct Insets {double top=0,left=0,bottom=0,right=0;};
struct Rect {
    double x=0,y=0,w=0,h=0;bool null=false; // null: no rectangle at all (CGRectNull)
    static Rect none(){Rect r;r.null=true;return r;}
    double min_x()const{return x;} double max_x()const{return x+w;} double mid_x()const{return x+w/2;}
    double min_y()const{return y;} double max_y()const{return y+h;} double mid_y()const{return y+h/2;}
    bool empty()const{return null||w<=0||h<=0;}
    bool contains(Point p)const{return !null&&p.x>=x&&p.x<x+w&&p.y>=y&&p.y<y+h;}
    bool equals(const Rect& o)const{return null==o.null&&(null||(x==o.x&&y==o.y&&w==o.w&&h==o.h));}
    Rect inset(Insets i)const{return null?*this:Rect{x+i.left,y+i.top,w-i.left-i.right,h-i.top-i.bottom};}
    Rect intersect(const Rect& o)const;
};

// ---- Where the picture and the controls go ----
// game: the picture's shape fitted into its area. picture: that whole area (the full view in the
// overlay layout, the part above the fold or the midline in the split one); recvx hands it to the
// SDL view, whose renderer fits the picture itself. controls: where the controls are laid out.
// separated: the split layout, picture above the controls.
struct Layout {Rect game,picture,controls;bool separated=false,top_safe_only=false;};
Rect aspect_fit(Rect bounds,double aspect); // largest centered rectangle of width / height = aspect
// A foldable's fold as the system reports it, clipped to the view; none() unless it crosses the view
// from side to side (90 % of the width) strictly inside its height.
Rect horizontal_division(Rect bounds,Rect division);
// A foldable held upright keeps only the top inset: the fold, not the edges, bounds the controls.
Insets game_safe_insets(Rect bounds,Insets insets,bool foldable);
// Landscape: the controls lie over the picture (overlay), unless the hinge is half open over a fold
// that crosses the view. Portrait: always split, at the fold or at the midline.
Layout layout(Rect bounds,Insets insets,bool partially_open,Rect division,double aspect);

// ---- The controls ----
// Keys by position, in nxnative's order (its A B X Y are right, bottom, top, left).
enum Key:unsigned {FaceRight,FaceBottom,FaceTop,FaceLeft,StickL,StickR,ShoulderL,ShoulderR,TriggerL,TriggerR,Start,Back,Left,Up,Right,Down,KeyCount};
constexpr int stick_max=32767;
// keys: bit per Key. axes: left x, left y, right x, right y; +y is down, as SDL reports a stick.
struct State {std::uint32_t keys=0;std::array<int,4> axes{};};

class Pad {
public:
    using TouchId=std::uintptr_t;
    // The view changed: a finger held across a pose change must not leave a key stuck, so a
    // different layout releases every touch. Returns whether it did.
    bool arrange(Rect bounds,Insets safe_insets,const Layout& layout);
    void set_full(bool full); // the 3D layout: D-pad, stick clicks and the undrawn right stick
    bool full()const{return full_;}
    void set_hidden(bool hidden); // a physical controller is connected: no control takes a touch
    bool hidden()const{return hidden_;}

    void begin(TouchId id,Point p);
    void move(TouchId id,Point p);
    void end(TouchId id);
    void reset();
    const State& state()const{return state_;}
    // One short feedback per press edge (a key newly down, a stick newly gripped); reading clears it.
    bool take_press_edge(){const bool edge=press_edge_;press_edge_=false;return edge;}

    // Geometry, in view points; the drawing and the menu button's hit test read it.
    const Layout& current_layout()const{return layout_;}
    double u()const; // 1 = a view 430 points tall (iPhone Pro Max landscape)
    Rect safe()const;
    bool portrait_controls()const{return layout_.separated&&bounds_.h>bounds_.w;}
    Point face_center()const;
    Point face_pos(Key key)const;
    double face_r()const;
    Point stick_home()const;
    double stick_r(int stick)const;
    bool stick_held(int stick)const{return stick_touch_[stick]!=0;}
    Point knob_offset(int stick)const; // from the stick centre, clamped to the stick radius
    Point dpad_center()const;
    double dpad_r()const;
    Point drop_center(Key key)const;
    double drop_r(Key key)const;
    Point click_center(Key key)const;
    Rect pill_rect(Key key)const;
    Point menu_center()const;
    int key_at(Point p)const; // a key locked at touch-down (shoulders, triggers, Back / Start, stick clicks), or -1
    bool menu_takes(Point p)const; // the menu button's hit circle, unless a locked key's target is nearer
    bool control_at(Point p)const; // a touch-down here would be taken by a control
    static bool full_only(Key key){return key==StickL||key==StickR||key>=Left;}

    // Layout constants, in u; x from the outer edge, the right half mirrors (DmGamepad.m).
    static constexpr double zl_x=44,zl_y=40,zl_r=26,l_x=44,l_y=108,l_r=24,drop_tip=1.45,corner_in=0.5,
        face_x=81,pad_y=82,face_gap_u=50,face_r_u=27,pill_dx=52,pill_y=22,pill_w=50,pill_h=28,menu_r=15,
        stick_x=81,stick_r_u=77,knob_r=30,stick_grab=1.4,full_tilt=0.8,dead_zone=0.1,
        low_x=231,low_y=64,dpad_r_u=48,arm_w=17,arm_in=11,dpad_grab=1.3,rstick_r=60,rswipe=1.0/3,click_x=102,click_r=17;
private:
    enum Kind:unsigned {None,Stick,Dpad,Face,Button};
    struct Owner {Kind kind=None;unsigned index=0;Point now;std::uint32_t face=0;};
    Rect bounds_;Insets insets_;Layout layout_;
    bool full_=true,hidden_=false,press_edge_=false;
    std::map<TouchId,Owner> owners_;
    std::array<TouchId,2> stick_touch_{};std::array<Point,2> stick_now_{};
    Point right_centre_; // the right stick's centre: where its touch landed, dragged along past its radius
    std::uint32_t pressed_=0;unsigned gripped_=0;
    State state_;
    Rect corner()const;
    Point portrait_thumb_center(bool right)const;
    Point stick_centre(int stick)const{return stick?right_centre_:stick_home();}
    double face_gap()const;
    double face_hit_r()const;
    bool in_right_area(Point p)const;
    double key_distance(Key key,Point p)const;
    double menu_distance(Point p)const;
    double face_grab(Point p)const;
    double dpad_grab_distance(Point p)const;
    double stick_grab_distance(Point p)const;
    std::uint32_t face_at(Point p)const;
    std::uint32_t dpad_at(Point p)const;
    void recompute();
};
}
