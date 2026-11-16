// On-screen touch gamepad: layout and touch ownership (see touch_pad.h for the source and licence).
#include "touch_pad.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace recvx::touch {
namespace {
constexpr double pi=3.14159265358979323846;
double dist(Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y);}
Point side(const Rect& s,bool right,double dx,double y){return {right?s.max_x()-dx:s.min_x()+dx,y};}
bool is_drop(Key k){return k==TriggerL||k==ShoulderL||k==TriggerR||k==ShoulderR;}
bool is_click(Key k){return k==StickL||k==StickR;}
// Keys locked at touch-down, in nxnative's test order (stick clicks only in the 3D layout).
constexpr Key locked_keys[]={TriggerL,ShoulderL,TriggerR,ShoulderR,Back,Start,StickL,StickR};
}
Rect Rect::intersect(const Rect& o)const{
    if(null||o.null)return none();
    const double left=std::max(x,o.x),top=std::max(y,o.y),right=std::min(max_x(),o.max_x()),bottom=std::min(max_y(),o.max_y());
    if(right<left||bottom<top)return none();
    return {left,top,right-left,bottom-top};
}
Rect aspect_fit(Rect b,double aspect){
    double w=b.w,h=b.h;
    if(b.null||w<=0||h<=0||aspect<=0)return {};
    if(w/h>aspect)w=h*aspect;else h=w/aspect;
    return {std::floor(b.mid_x()-w/2),std::floor(b.mid_y()-h/2),w,h};
}
Rect horizontal_division(Rect b,Rect division){
    if(division.null||division.w<=division.h)return Rect::none();
    // Keep a zero-height inactive fold; clip the region to the view during a display handoff.
    const double x=std::max(b.min_x(),division.min_x()),y=std::max(b.min_y(),division.min_y());
    const double right=std::min(b.max_x(),division.max_x()),bottom=std::min(b.max_y(),division.max_y());
    if(right-x<b.w*0.9||bottom<y||y<=b.min_y()||bottom>=b.max_y())return Rect::none();
    return {x,y,right-x,bottom-y};
}
Insets game_safe_insets(Rect bounds,Insets insets,bool foldable){
    return foldable&&bounds.h>bounds.w?Insets{insets.top,0,0,0}:insets;
}
Layout layout(Rect b,Insets insets,bool partially_open,Rect division,double aspect){
    Layout l{aspect_fit(b,aspect),b,b,false,false};
    if(b.empty())return l;
    division=horizontal_division(b,division);
    const bool portrait=b.h>b.w;
    if(!portrait&&(!partially_open||division.null))return l;
    if(division.null)division={b.x,b.mid_y(),b.w,0};
    const Rect safe=b.inset(insets);
    const Rect upper{safe.min_x(),safe.min_y(),safe.w,division.min_y()-safe.min_y()};
    const Rect lower{b.min_x(),division.max_y(),b.w,b.max_y()-division.max_y()};
    if(upper.h<=0||safe.max_y()<=lower.min_y())return l;
    l.game=aspect_fit(upper,aspect);l.picture=upper;l.controls=lower;l.separated=true;
    return l;
}

bool Pad::arrange(Rect bounds,Insets safe_insets,const Layout& layout){
    const bool same=layout_.controls.equals(layout.controls)&&layout_.game.equals(layout.game)&&layout_.picture.equals(layout.picture)
        &&layout_.separated==layout.separated&&layout_.top_safe_only==layout.top_safe_only;
    bounds_=bounds;insets_=safe_insets;
    if(same)return false;
    reset();layout_=layout;
    return true;
}
void Pad::set_full(bool full){if(full==full_)return;reset();full_=full;}
void Pad::set_hidden(bool hidden){if(hidden==hidden_)return;reset();hidden_=hidden;}

// ---- Geometry (points; computed inside the safe area) ----
Rect Pad::safe()const{return layout_.controls.intersect(bounds_.inset(game_safe_insets(bounds_,insets_,layout_.top_safe_only)));}
// The rectangle the bottom-corner controls use: each corner holds one control, so it reaches halfway
// into the side inset (in landscape only the rounded screen corner is there).
Rect Pad::corner()const{
    if(layout_.separated)return safe();
    Insets i=insets_;i.left*=corner_in;i.right*=corner_in;
    return bounds_.inset(i);
}
double Pad::u()const{
    if(layout_.separated){const Rect s=safe();return std::min(std::min(s.h/430.0,s.w/700.0),1.3);}
    return std::min(bounds_.h/430.0,1.3);
}
Point Pad::portrait_thumb_center(bool right)const{const Rect s=safe();return {s.min_x()+s.w*(right?0.72:0.28),s.min_y()+s.h*0.58};}
double Pad::face_gap()const{return face_gap_u*u();}
double Pad::face_r()const{return face_r_u*u();}
double Pad::face_hit_r()const{return std::max(face_r()*1.25,layout_.separated?22.0:0.0);}
Point Pad::face_center()const{
    if(portrait_controls())return portrait_thumb_center(true);
    const Rect s=corner();const double unit=u(),edge=layout_.separated?face_gap()+face_hit_r():0;
    return side(s,true,std::max(face_x*unit,edge),s.max_y()-std::max(pad_y*unit,edge));
}
Point Pad::face_pos(Key key)const{
    const Point c=face_center();const double g=face_gap();
    switch(key){case FaceRight:return {c.x+g,c.y};case FaceBottom:return {c.x,c.y+g};case FaceTop:return {c.x,c.y-g};default:return {c.x-g,c.y};}
}
Point Pad::stick_home()const{
    if(portrait_controls()){Point p=portrait_thumb_center(false);if(full_)p.x-=dpad_r()+6*u();return p;}
    const Rect s=corner();
    return side(s,false,stick_x*u(),s.max_y()-pad_y*u());
}
double Pad::stick_r(int stick)const{return (stick?rstick_r:stick_r_u)*u();}
bool Pad::in_right_area(Point p)const{const Rect& b=layout_.controls;return p.x>=b.max_x()-b.w*rswipe;}
Point Pad::dpad_center()const{
    if(portrait_controls()){Point p=portrait_thumb_center(false);p.x+=stick_r(0)+6*u();return p;}
    const Rect s=corner();
    return side(s,false,low_x*u(),s.max_y()-low_y*u());
}
double Pad::dpad_r()const{return dpad_r_u*u();}
Point Pad::drop_center(Key key)const{
    const Rect s=safe();const double unit=u(),edge=layout_.separated?22:0;const bool z=key==TriggerL||key==TriggerR;
    return side(s,key==TriggerR||key==ShoulderR,std::max((z?zl_x:l_x)*unit,edge),s.min_y()+std::max((z?zl_y:l_y)*unit,edge));
}
double Pad::drop_r(Key key)const{return (key==TriggerL||key==TriggerR?zl_r:l_r)*u();}
Point Pad::click_center(Key key)const{
    const Rect s=safe();const double unit=u(),edge=layout_.separated?22:0;
    return side(s,key==StickR,std::max(click_x*unit,edge),s.min_y()+std::max(zl_y*unit,edge));
}
Rect Pad::pill_rect(Key key)const{ // Back on the left, Start on the right of the menu button
    const Rect s=safe();const double unit=u(),w=pill_w*unit,h=pill_h*unit;
    const Point c{s.mid_x()+(key==Start?pill_dx:-pill_dx)*unit,s.min_y()+std::max(pill_y*unit,layout_.separated?14.0:0.0)};
    return {c.x-w/2,c.y-h/2,w,h};
}
Point Pad::menu_center()const{const Rect s=safe();return {s.mid_x(),s.min_y()+std::max(pill_y*u(),layout_.separated?14.0:0.0)};}
double Pad::key_distance(Key key,Point p)const{
    const double unit=u(),floor_r=layout_.separated?22:0;
    if(is_drop(key))return dist(p,drop_center(key))/std::max(drop_r(key)*1.35,floor_r);
    if(is_click(key))return dist(p,click_center(key))/std::max(click_r*unit*1.35,floor_r);
    const Rect pill=pill_rect(key);
    const Rect r{pill.x-8*unit,pill.y-8*unit,pill.w+16*unit,pill.h+16*unit};
    if(!layout_.separated&&!r.contains(p))return std::numeric_limits<double>::max();
    const double w=std::max(r.w,layout_.separated?28.0:0.0),h=std::max(r.h,layout_.separated?28.0:0.0);
    return std::max(std::fabs(p.x-r.mid_x())/(w/2),std::fabs(p.y-r.mid_y())/(h/2));
}
int Pad::key_at(Point p)const{ // the nearest normalized target wins where the split layout's targets overlap
    int best=-1;double distance=1;
    for(const Key key:locked_keys){
        if(full_only(key)&&!full_)continue;
        const double d=key_distance(key,p);
        if(d<=1&&(best<0||d<distance)){best=int(key);distance=d;if(!layout_.separated)return best;}
    }
    return best;
}
double Pad::menu_distance(Point p)const{return dist(p,menu_center())/std::max(menu_r*u()*1.6,22.0);}
bool Pad::menu_takes(Point p)const{
    const double d=menu_distance(p);if(d>1)return false;
    if(hidden_||(layout_.separated&&!layout_.controls.contains(p)))return true;
    const int hit=key_at(p);
    return hit<0||d<key_distance(Key(hit),p);
}
std::uint32_t Pad::face_at(Point p)const{ // keys under the finger in the diamond (between two keys both count)
    std::uint32_t mask=0;const double r=face_hit_r();
    for(unsigned k=FaceRight;k<=FaceLeft;++k)if(dist(p,face_pos(Key(k)))<=r)mask|=1u<<k;
    return mask;
}
std::uint32_t Pad::dpad_at(Point p)const{ // 8 directions of 45 degrees, diagonals press two keys; a small centre circle is none
    const Point c=dpad_center();const double dx=p.x-c.x,dy=p.y-c.y;
    if(std::hypot(dx,dy)<dpad_r()*0.25)return 0;
    const double a=std::atan2(-dy,dx)*180/pi;std::uint32_t mask=0;
    if(a>-67.5&&a<67.5)mask|=1u<<Right;
    if(a>22.5&&a<157.5)mask|=1u<<Up;
    if(a>112.5||a<-112.5)mask|=1u<<Left;
    if(a<-22.5&&a>-157.5)mask|=1u<<Down;
    return mask;
}
Point Pad::knob_offset(int stick)const{
    if(!stick_touch_[stick])return {};
    const Point h=stick_centre(stick);double dx=stick_now_[stick].x-h.x,dy=stick_now_[stick].y-h.y;
    const double d=std::hypot(dx,dy),radius=stick_r(stick);
    if(d>radius){dx*=radius/d;dy*=radius/d;}
    return {dx,dy};
}
double Pad::face_grab(Point p)const{return dist(p,face_center())/(face_gap()+std::max(face_r()*1.6,face_hit_r()));}
double Pad::dpad_grab_distance(Point p)const{return dist(p,dpad_center())/std::max(dpad_r()*dpad_grab,layout_.separated?22.0:0.0);}
double Pad::stick_grab_distance(Point p)const{return dist(p,stick_home())/std::max(stick_r(0)*stick_grab,layout_.separated?22.0:0.0);}
bool Pad::control_at(Point p)const{
    if(hidden_||(layout_.separated&&!layout_.controls.contains(p)))return false;
    if(key_at(p)>=0)return true;
    if(face_grab(p)<=1)return true;
    if(full_&&dpad_grab_distance(p)<1)return true;
    if(!stick_touch_[0]&&stick_grab_distance(p)<1)return true;
    return full_&&!stick_touch_[1]&&in_right_area(p);
}

// ---- Touch ----
// Touch-down ownership: locked keys first, then whichever of the diamond, the D-pad and the left
// stick has the smallest distance / grab radius, then in the 3D layout the right stick for a touch
// in the right third. The menu button is a view of its own and asks menu_takes.
void Pad::begin(TouchId id,Point p){
    if(!id||hidden_||(layout_.separated&&!layout_.controls.contains(p))){recompute();return;}
    Owner owner;owner.now=p;
    const int hit=key_at(p);
    if(hit>=0){owner.kind=Button;owner.index=unsigned(hit);}
    else{
        double best=1;
        const double f=face_grab(p);if(f<=best){best=f;owner.kind=Face;}
        if(full_){const double d=dpad_grab_distance(p);if(d<best){best=d;owner.kind=Dpad;}}
        if(!stick_touch_[0]){const double s=stick_grab_distance(p);if(s<best){best=s;owner.kind=Stick;owner.index=0;}}
        if(owner.kind==None&&full_&&!stick_touch_[1]&&in_right_area(p)){owner.kind=Stick;owner.index=1;right_centre_=p;}
        if(owner.kind==Face)owner.face=face_at(p);
        else if(owner.kind==Stick){stick_touch_[owner.index]=id;stick_now_[owner.index]=p;}
    }
    if(owner.kind!=None)owners_[id]=owner;
    recompute();
}
void Pad::move(TouchId id,Point p){
    const auto found=owners_.find(id);
    if(found!=owners_.end()){
        Owner& owner=found->second;owner.now=p;
        if(owner.kind==Face)owner.face=face_at(p); // sliding between keys switches keys
        else if(owner.kind==Stick){
            stick_now_[owner.index]=p;
            if(owner.index==1){ // the right stick's centre follows a finger past its radius, so turning back responds at once
                const double d=dist(p,right_centre_),radius=stick_r(1);
                if(d>radius)right_centre_={p.x-(p.x-right_centre_.x)*radius/d,p.y-(p.y-right_centre_.y)*radius/d};
            }
        }
    }
    recompute();
}
void Pad::end(TouchId id){
    owners_.erase(id);
    for(auto& touch:stick_touch_)if(touch==id)touch=0;
    recompute();
}
void Pad::reset(){owners_.clear();stick_touch_.fill(0);recompute();press_edge_=false;} // a release is never a press: drop an edge nobody read yet
void Pad::recompute(){
    std::uint32_t keys=0;
    for(const auto& item:owners_){
        const Owner& owner=item.second;
        if(owner.kind==Button)keys|=1u<<owner.index;
        else if(owner.kind==Face)keys|=owner.face;
        else if(owner.kind==Dpad)keys|=dpad_at(owner.now);
    }
    State next;next.keys=keys;
    for(int i=0;i<2;++i){
        const Point o=knob_offset(i);const double radius=stick_r(i),d=std::hypot(o.x,o.y),dz=radius*dead_zone;
        if(d<=dz)continue;
        const double m=std::min((d-dz)/(radius*full_tilt-dz),1.0)/d; // outside the dead zone: 0 rising to full at the full-tilt radius
        next.axes[i*2]=int(std::lrint(o.x*m*stick_max));next.axes[i*2+1]=int(std::lrint(o.y*m*stick_max)); // rounded so full tilt is exactly 32767
    }
    const unsigned gripping=(stick_touch_[0]?1u:0u)|(stick_touch_[1]?2u:0u);
    if((keys&~pressed_)||(gripping&~gripped_))press_edge_=true;
    pressed_=keys;gripped_=gripping;state_=next;
}
}
