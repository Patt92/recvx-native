#include "ui.h"
#include "events.h"
#include "asset_io.h"
#include "binary_view.h"
#include "item_asset.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>
namespace recvx {
namespace {
struct NJS_TEXANIM {int sx,sy,cx,cy,u1,v1,u2,v2,texid,attr;};
struct Color {float a,r,g,b;};
struct PARTS {float pos[3];int ang,atr,anim,cen_no,parts_num;float move;int setnum;Color col;int color;};
struct S_WORK {unsigned itemid=0,maincsr=0;}; // BulletSet does not access it
struct UiSystem {const unsigned* ev_flg;};static thread_local UiSystem* sys=nullptr;
static bool bhCkFlg(const unsigned* flags,unsigned bit){return (flags[bit/32]&(0x80000000u>>(bit%32)))!=0;}
// Reine Statusmenü-Datentabellen (NJS_TEXANIM/PARTS/Font). Die zugehörigen
// Mechaniken sind hier eigenständig nativ: Mengenanzeige (GameUi::quantity,
// Semantik BulletSet) und Befehlsfenster (item_command_parts, Semantik
// ItemCommand testmode 0 + TrigerSet + Cursorschleife).
#include "ui_tables_data.inc"
// Befehlsfenster parts_06b nach ItemCommand testmode 0: alle 18 Teile
// sichtbar (atr |= 0x20), dann je Itemtyp wieder aus. Waffen: Beschriftung 13
// = Ausrüsten (anim 7); außer der Schrotflinte (Item 10) entfällt Teil 17
// (Salvenmarke). Andere Items: Beschriftung 13 = Benutzen (anim 8), Teile 16
// und 17 entfallen. TrigerSet setzt Rahmenhöhe und Wiederholzahl; bei Item 10
// zeigt Teil 17 je nach ev_flg 74 anim 12 (Salve an) oder 11. Die Zeilen
// 13..13+max färbt die Cursorschleife: ausgewählt atr|4, color|8, Farbe
// (0.125,0.5,1), sonst weiß.
std::array<PARTS,18> item_command_parts(unsigned item,unsigned cursor,bool burst){
    std::array<PARTS,18> parts;std::copy_n(parts_06b,18,parts.begin());
    for(auto& p:parts)p.atr|=0x20;
    const bool weapon=item<156&&(itemdata[item].type&1);
    if(weapon){parts[13].anim=7;if(item!=10)parts[17].atr&=~0x20;}
    else{parts[13].anim=8;parts[16].atr&=~0x20;parts[17].atr&=~0x20;}
    if(weapon){
        if(item==10){
            parts[2].pos[1]=parts[3].pos[1]=148;parts[5].pos[1]=184;parts[7].pos[1]=180;
            parts[8].setnum=3;parts[9].setnum=parts[10].setnum=4;
            parts[17].anim=burst?12:11;
        }else{
            parts[2].pos[1]=parts[3].pos[1]=116;parts[5].pos[1]=152;parts[7].pos[1]=148;
            parts[8].setnum=2;parts[9].setnum=parts[10].setnum=3;
            parts[11].atr&=~0x20;parts[12].atr&=~0x20;
        }
    }else{
        parts[2].pos[1]=parts[3].pos[1]=84;parts[5].pos[1]=120;parts[7].pos[1]=116;
        parts[8].setnum=1;parts[9].setnum=parts[10].setnum=2;
        parts[11].atr&=~0x20;parts[12].atr&=~0x20;
    }
    const unsigned max=weapon?(item==10?4u:3u):2u;
    for(unsigned n=0;n<=max&&13+n<18;++n){
        auto& p=parts[13+n];
        if(n!=cursor){p.atr&=~0x4;p.color&=~0x8;p.col={1,1,1,1};}
        else{p.atr|=0x4;p.color|=0x8;p.col={1,0.125f,0.5f,1};}
    }
    return parts;
}
const NJS_TEXANIM* const tables[]={parts_00,parts_01,parts_02,parts_03,parts_04,parts_05,parts_11,parts_07,parts_08,parts_09,parts_10,parts_11,parts_12,parts_13,parts_14,parts_15,parts_16,parts_11,nullptr,parts_19,parts_20,nullptr,parts_22};
constexpr int centers[12][2]={{0,0},{28,40},{0,100},{352,40},{520,40},{436,120},{28,332},{328,120},{60,132},{436,40},{28,40},{452,120}};
void rectangle(SoftwareRenderer& image,int x,int y,int w,int h,std::uint32_t color){auto& p=image.canvas();for(int j=std::max(0,y);j<std::min(image.height(),y+h);++j)for(int i=std::max(0,x);i<std::min(image.width(),x+w);++i)p[j*image.width()+i]=color;}
void blit_pixels(SoftwareRenderer& image,const SceneTexture& texture,int x,int y,int w,int h,int u1,int v1,int u2,int v2,std::uint32_t tint=0xffffffff,bool flip=false,int angle=0){
    if(w<=0||h<=0||u1<0||v1<0||u2>int(texture.width)||v2>int(texture.height)||u2<u1||v2<v1)return;
    const float a=float(angle)*3.14159265f/180,cs=std::cos(a),sn=std::sin(a);
    auto& dst=image.canvas();for(int j=0;j<h;++j)for(int i=0;i<w;++i){
        const int tx=x+int(std::round(cs*i-sn*j)),ty=y+int(std::round(sn*i+cs*j));if(tx<0||ty<0||tx>=image.width()||ty>=image.height())continue;
        const unsigned u=unsigned(u1+(flip?(w-1-i):i)*(u2-u1)/w),v=unsigned(v1+j*(v2-v1)/h);if(u>=texture.width||v>=texture.height)continue;
        const auto c=texture.pixels[v*texture.width+u];const unsigned alpha=(c>>24)*(tint>>24)/255;if(!alpha)continue;
        auto& d=dst[ty*image.width()+tx];std::uint32_t out=0xff000000;
        for(unsigned shift:{0u,8u,16u}){const auto component=((c>>shift)&255)*((tint>>shift)&255)/255;out|=((component*alpha+((d>>shift)&255)*(255-alpha))/255)<<shift;}d=out;
    }
}
void blit(SoftwareRenderer& image,const SceneTexture& texture,int x,int y,int w,int h,int u1,int v1,int u2,int v2,std::uint32_t tint=0xffffffff,bool flip=false,int angle=0){
    blit_pixels(image,texture,x,y,w,h,u1*int(texture.width)/256,v1*int(texture.height)/256,u2*int(texture.width)/256,v2*int(texture.height)/256,tint,flip,angle);
}
std::uint32_t tint(Color c){return (unsigned(std::clamp(c.a,0.f,1.f)*255)<<24)|(unsigned(std::clamp(c.r,0.f,1.f)*255)<<16)|(unsigned(std::clamp(c.g,0.f,1.f)*255)<<8)|unsigned(std::clamp(c.b,0.f,1.f)*255);}
// njDrawTextureH strip of an axis-aligned rectangle: bilinear vertex colours,
// texel * base colour + offset colour, alpha from texel and base alpha.
void pulse_quad(SoftwareRenderer& image,const SceneTexture& texture,const PulseQuad& q){
    if(texture.pixels.empty())return;
    float x0=q[0].x,x1=q[0].x,y0=q[0].y,y1=q[0].y;for(const auto& v:q){x0=std::min(x0,v.x);x1=std::max(x1,v.x);y0=std::min(y0,v.y);y1=std::max(y1,v.y);}
    if(x1-x0<.5f||y1-y0<.5f)return;
    auto corner=[&](bool right,bool bottom)->const PulseVertex&{for(const auto& v:q)if((v.x>(x0+x1)/2)==right&&(v.y>(y0+y1)/2)==bottom)return v;return q[0];};
    const auto &a=corner(false,false),&b=corner(true,false),&c=corner(false,true),&d=corner(true,true);
    auto lerp=[](float l,float r,float t){return l+(r-l)*t;};
    auto channel=[&](std::uint32_t (PulseVertex::*field),unsigned shift,float s,float t){auto f=[&](const PulseVertex& v){return float((v.*field>>shift)&255);};return lerp(lerp(f(a),f(b),s),lerp(f(c),f(d),s),t);};
    auto& dst=image.canvas();
    for(int y=std::max(0,int(std::ceil(y0)));y<std::min(image.height(),int(std::ceil(y1)));++y)for(int x=std::max(0,int(std::ceil(x0)));x<std::min(image.width(),int(std::ceil(x1)));++x){
        const float s=(x+.5f-x0)/(x1-x0),t=(y+.5f-y0)/(y1-y0);
        const float u=lerp(lerp(a.u,b.u,s),lerp(c.u,d.u,s),t),v=lerp(lerp(a.v,b.v,s),lerp(c.v,d.v,s),t);
        const unsigned tu=std::min(texture.width-1,unsigned(std::max(0.f,u*texture.width))),tv=std::min(texture.height-1,unsigned(std::max(0.f,v*texture.height)));
        const auto texel=texture.pixels[tv*texture.width+tu];
        const float alpha=float(texel>>24)/255*channel(&PulseVertex::base,24,s,t)/255;if(alpha<=0)continue;
        auto& out=dst[y*image.width()+x];std::uint32_t color=0xff000000;
        for(unsigned shift:{0u,8u,16u}){const float value=std::min(255.f,float((texel>>shift)&255)*channel(&PulseVertex::base,shift,s,t)/255+channel(&PulseVertex::offset,shift,s,t));
            color|=unsigned(std::clamp(value*alpha+float((out>>shift)&255)*(1-alpha),0.f,255.f))<<shift;}
        out=color;
    }
}
// Grid cell (listcsr_2) of pip index `slot`: wide items fill a row and sit on its odd cell.
unsigned grid_cell(const Inventory& inv,unsigned slot){if(slot<=1)return 1;unsigned cell=2;for(unsigned i=2;i<12;++i){const auto id=Inventory::id(inv.slots()[i]);const bool wide=id&&(itemdata[id].type&0x100);if(i==slot)return wide?cell+1:cell;cell+=wide?2:1;}return cell;}
unsigned last_slot(const Inventory& inv){unsigned cells=0,last=1;for(unsigned slot=2;slot<12&&cells<inv.capacity();++slot){const auto id=Inventory::id(inv.slots()[slot]);cells+=id&&(itemdata[id].type&0x100)?2:1;last=slot;}return last;}
}
std::vector<std::string> StatusMenu::actions(const Inventory& inventory)const{
    const auto item=Inventory::id(inventory.slots()[selected]);if(!item)return {};
    if(itemdata[item].type&1){std::vector<std::string> list={"Equip","Examine","Combine","Use"};if(item==10)list.push_back(shotgun_burst&&shotgun_burst()?"Single":"Burst");return list;}
    return {"Use","Examine","Combine"};
}
void StatusMenu::reset(Inventory& inventory){
    // Discard transient prompts/actions before a debug mutation can reorder slots.
    if(examining&&!inspection.running()&&selected<12&&Inventory::id(inventory.slots()[selected]))inspection.cancel(inventory,selected);
    auto progress=std::move(file_progress);auto bonus=std::move(file_bonus);auto manual=std::move(file_manual_read);auto source=std::move(message_source);auto burst=std::move(shotgun_burst);auto toggle=std::move(toggle_shotgun_burst);const auto old_stage=stage,old_room=room;
    *this=StatusMenu{};file_progress=std::move(progress);file_bonus=std::move(bonus);file_manual_read=std::move(manual);message_source=std::move(source);shotgun_burst=std::move(burst);toggle_shotgun_burst=std::move(toggle);stage=old_stage;room=old_room;
}
void StatusMenu::show_message(unsigned index){if(!message_source)throw std::runtime_error("inventory message source is not bound");message.show(message_source(index),index);}
void StatusMenu::close_examine(Inventory& inventory){inspection.cancel(inventory,selected);examining=false;opening_item=opening_frame=0;}
void StatusMenu::inspect(Inventory& inventory){
    inspection.check(inventory,selected,{examine_pitch,examine_yaw,examine_roll});
    if(inspection.message_pending())show_message(inspection.take_message());
    opening_item=inspection.running()?inspection.source():0;opening_frame=0;
}
void StatusMenu::open_file(Inventory& inventory,unsigned file,bool list){
    inventory.items[383]|=1u<<file;files=true;file_list=list;file_number=file;file_page=file_display_page=0;file_item_source=false;file_exit_selected=false;file_scroll=file_wait=0;file_x=68;commands=false;
    file_transition=file_entering=false;
    if(list){file_folder.open(inventory,file_bonus&&file_bonus(),true);file_entering=true;slide.change_view();nav=false;maincsr=1;}
}
void StatusMenu::acquire_file(Inventory& inventory,unsigned file){
    active=true;commands=examining=combining=nav=false;file_acquisition=true;open_file(inventory,file);
}
void StatusMenu::finish_inspection(Inventory& inventory){
    if(inspection.completed_file()<24){open_file(inventory,inspection.completed_file());file_item_source=true;examining=false;inspection=ItemInspection{};}
}
void StatusMenu::combine(Inventory& inventory,unsigned first,unsigned second){
    unsigned index=0,destination=0;if(inventory.combine(first,second,&index,&destination)){combining=false;selected=destination;}
    else{feedback_sound=1;if(index)show_message(index);}
}
void StatusMenu::tick(Inventory& inventory){
    // fileview.c PageScroll: accelerating outgoing/incoming page, eight-frame
    // capture wait and two-frame pad wait. The text changes between the passes.
    if(files&&!file_list){
        if(file_wait)--file_wait;
        else if(file_scroll){
            const bool forward=file_scroll==1||file_scroll==3;
            file_x+=forward?-int(file_move):int(file_move);file_move*=2;
            if(file_scroll<=2&&file_move>640){file_scroll=forward?3:4;file_x=forward?708:-572;file_move=10;file_display_page=file_page;file_wait=8;}
            else if((file_scroll==3&&file_x<0)||(file_scroll==4&&file_x>0)){file_scroll=0;file_x=68;file_move=10;file_wait=2;}
        }
    }
    if(active&&files&&file_list&&!file_transition&&!slide.moving()){
        file_folder.step(inventory);file_number=file_folder.file();
        if(const auto sound=file_folder.take_sound();sound!=~0u)feedback_sound=sound;
        if(file_folder.take_reader()){file_transition=true;slide.file_out();file_exit_selected=false;file_wait=file_scroll=0;}
    }
    if(active&&nav&&!slide.moving()){
        if(nav_wait)--nav_wait;
        else if(nav_pressed){ // MainCommand: the action runs once the pressed animation (wait) ended
            nav_pressed=false;
            if(maincsr==0)begin_close();                                    // StatusCancel
            else if(maincsr==1){files=file_list=true;file_transition=false;file_entering=true;slide.change_view();file_page=0;commands=examining=combining=nav=false;file_folder.open(inventory,file_bonus&&file_bonus());file_number=file_folder.file();}
            else if(maincsr==3)nav=false;                                   // back to CursorMove
        }
    }
    if(!active){slide_open_=false;closing=false;}
    else{
        if(!slide_open_){slide=StatusSlide{};slide.open(false);slide_open_=true;closing=false;}
        const bool closed=slide.tick();
        if(file_entering&&slide.open1()==23&&slide.open2()==46)file_entering=false;
        if(const auto sound=slide.take_sound();sound!=0xffffffff)slide_sound=sound;
        if(closed&&file_transition){file_transition=false;file_list=false;slide=StatusSlide{};file_scroll=3;file_x=708;file_move=10;file_wait=0;file_display_page=file_page=0;}else if(closed&&closing){active=false;closing=false;slide_open_=false;}
    }
    if(commands&&command_frames<4)++command_frames;else if(erase_frames)--erase_frames;
    // StatusInit runs PulseInit on every opening; StatusMain then runs PulseMain once per frame.
    const auto condition=StatusPulse::condition(inventory.hp,(inventory.player_stflg()&0x280000u)!=0);
    if(!active)pulse_open_=false;else{if(!pulse_open_||(!pulse.healing()&&pulse.condition()!=condition)){pulse.open(condition);pulse_open_=true;}pulse.tick();}
    const bool had_message=message.active();message.tick();if(had_message&&!message.active())finish_inspection(inventory);
    if(!inspection.running())return;
    inspection.tick(inventory);opening_frame=inspection.action_frame();opening_item=inspection.running()?inspection.source():0;
    if(const auto angles=inspection.take_display_reset()){examine_pitch=(*angles)[0];examine_yaw=(*angles)[1];examine_roll=(*angles)[2];}
    if(inspection.message_pending())show_message(inspection.take_message());
}
bool StatusMenu::input(MenuInput key,Inventory& inventory){
    if(active&&(closing||file_transition||slide.moving()))return true; // statusflg 0x40/0x4: no pad input while the screen slides
    const bool was_active=active;
    const bool handled=input_unslid(key,inventory);
    if(was_active&&!active){active=true;begin_close();} // every close goes through StatusCancel
    return handled;
}
void StatusMenu::begin_close(){if(!active||closing)return;if(!slide_open_){active=false;return;}slide.cancel();closing=true;}
void StatusMenu::abort_close(){if(!closing)return;closing=false;slide=StatusSlide{};}
void StatusMenu::skip_slide(){slide=StatusSlide{};slide_open_=true;closing=false;}
bool StatusMenu::input_unslid(MenuInput key,Inventory& inventory){
    // sub1.c sound semantics (CallSystemSe ignores its first argument):
    // ItemTaskCheck opens with SE 3, decide (pad 0x800) SE 3, cancel/close
    // (0x1000/0x4000, StatusCancel) SE 0, cursor moves SE 2 only when the
    // cursor actually moves, invalid combination SE 1.
    struct Snapshot {bool active,commands,examining,combining,files,file_list,message;unsigned selected,command,file_number,file_page,combine_from,message_index;};
    auto snap=[&]{return Snapshot{active,commands,examining,combining,files,file_list,message.active(),selected,command,file_number,file_page,combine_from,message.index()};};
    const auto before=snap();const bool was_examining=examining&&!message.active();
    feedback_sound=0xffffffff;
    if(active&&command_window_moving()&&key!=MenuInput::Open)return true;
    // CursorMove/MainCommand ignore directions and decide while the cure sweep owns statusflg 0x20.
    if(active&&pulse.healing()&&!message.active()&&key!=MenuInput::Cancel&&key!=MenuInput::Open)return true;
    const auto actions_before=commands?actions(inventory):std::vector<std::string>{};
    const unsigned item_before=selected<12?Inventory::id(inventory.slots()[selected]):0;
    const bool handled=apply_input(key,inventory);
    if(!before.commands&&commands)command_frames=0;                       // ItemCommand testmode 0: slide in
    if(before.commands&&!commands&&active){erase_frames=4;erase_item=item_before;erase_actions=actions_before;erase_command=before.command;} // ItemCommandErase
    if(!active){command_frames=0;erase_frames=0;}
    if(feedback_sound!=0xffffffff)return handled;
    const auto after=snap();
    auto key_of=[](const Snapshot& s){return std::make_tuple(s.active,s.commands,s.examining,s.combining,s.files,s.file_list,s.message,s.selected,s.command,s.file_number,s.file_page,s.combine_from,s.message_index);};
    const bool changed=key_of(before)!=key_of(after);
    if(key==MenuInput::Open){if(!before.active&&after.active)feedback_sound=3;else if(before.active&&!after.active)feedback_sound=0;}
    else if(key==MenuInput::Cancel){if(changed)feedback_sound=0;}
    else if(key==MenuInput::Confirm){if(changed||use_item)feedback_sound=before.message?0:3;}
    else if(changed&&!was_examining)feedback_sound=2; // examine rotation is silent
    return handled;
}
bool StatusMenu::apply_input(MenuInput key,Inventory& inventory){
    if(active&&message.active()){
        if(key==MenuInput::Open)return true;
        if(key==MenuInput::Cancel){message=MessageDialog{};inspection.answer(inventory,false);pending_combine_first_=pending_combine_second_=0;finish_inspection(inventory);return true;}
        if(message.choices()&&(key==MenuInput::Left||key==MenuInput::Up||key==MenuInput::Right||key==MenuInput::Down)){const int step=key==MenuInput::Left||key==MenuInput::Up?-1:1;message.selection=(message.selection+message.choices()+step)%message.choices();return true;}
        if(key==MenuInput::Confirm){const bool choice=message.choices(),yes=message.selection==0;message.confirm();if(!message.active()){
            inspection.answer(inventory,choice&&yes);opening_item=inspection.running()?inspection.source():0;opening_frame=0;
            if(pending_combine_first_){const auto first=pending_combine_first_,second=pending_combine_second_;pending_combine_first_=pending_combine_second_=0;if(choice&&yes)combine(inventory,first,second);}
            finish_inspection(inventory);
        }}return true;
    }
    if(inspection.running())return true;
    if(files){
        if(!file_list&&(file_scroll||file_wait))return true;
        auto leave_reader=[&]{if(file_number==0&&file_manual_read)file_manual_read(file_folder.automatic());if(file_acquisition){files=file_acquisition=false;begin_close();}else{files=file_list=false;nav=!file_item_source;maincsr=1;file_item_source=false;slide.open(false);}};
        if(file_list){
            if(file_folder.moving())return true;
            const unsigned pad=key==MenuInput::Up?1:key==MenuInput::Down?2:key==MenuInput::Left?4:key==MenuInput::Right?8:key==MenuInput::Confirm?0x800:(key==MenuInput::Cancel||key==MenuInput::Open)?0x1000:0;
            file_folder.step(inventory,pad);file_number=file_folder.file();feedback_sound=file_folder.take_sound();
            if(file_folder.take_navigation()){files=file_list=false;nav=true;maincsr=1;}
            if(file_folder.take_reader()){file_transition=true;slide.file_out();file_exit_selected=false;file_wait=0;file_scroll=0;}
            return true;
        }
        if(key==MenuInput::Cancel||key==MenuInput::Open){leave_reader();return true;}
        // FileScrollSet: right on the last page selects EXIT; left first
        // deselects it. Decide only leaves the reader with EXIT selected.
        if(key==MenuInput::Confirm&&file_exit_selected){leave_reader();feedback_sound=3;return true;}
        if(key==MenuInput::Left&&file_exit_selected){file_exit_selected=false;file_wait=2;return true;}
        if(key==MenuInput::Right&&file_page+1==file_pages()){file_exit_selected=true;return true;}
        if((key==MenuInput::Left&&file_page)||(key==MenuInput::Right&&file_page+1<file_pages())){file_scroll=key==MenuInput::Right?1:2;file_move=10;file_page+=key==MenuInput::Right?1:-1;feedback_sound=4;}
        return true;
    }
    if(key==MenuInput::Open){if(examining)close_examine(inventory);active=!active;commands=examining=combining=false;nav=nav_pressed=false;nav_wait=0;selected=1;command=0;inspection=ItemInspection{};return true;}if(!active)return false;
    if(key==MenuInput::Cancel){if(examining)close_examine(inventory);else if(commands)commands=false;else if(combining){combining=false;selected=combine_from;}else if(!nav){nav=true;maincsr=3;nav_wait=0;feedback_sound=0;}else if(!nav_wait)active=false;return true;} // CursorMove 0x1000 enters MainCommand (maincsr 3), MainCommand 0x1000 is StatusCancel
    if(nav){ // MainCommand testmode 1: the pad is ignored while the pressed button animates (wait)
        if(nav_wait||nav_pressed)return true;
        if(key==MenuInput::Left&&maincsr){--maincsr;feedback_sound=2;}else if(key==MenuInput::Right&&maincsr<3){++maincsr;feedback_sound=2;}
        else if(key==MenuInput::Confirm){if(maincsr==2)feedback_sound=1;else{nav_wait=4;nav_pressed=true;feedback_sound=3;}} // MAP needs the map screen (not ported): error SE
        return true;}
    if(examining){if(key==MenuInput::Left)examine_yaw-=2048;if(key==MenuInput::Right)examine_yaw+=2048;if(key==MenuInput::Up)examine_pitch+=2048;if(key==MenuInput::Down)examine_pitch-=2048;if(key==MenuInput::Confirm)inspect(inventory);return true;}
    if(commands){auto list=actions(inventory);if(list.empty()){commands=false;return true;}if(key==MenuInput::Up)command=(command+list.size()-1)%list.size();if(key==MenuInput::Down)command=(command+1)%list.size();if(key==MenuInput::Confirm){
        const auto item=Inventory::id(inventory.slots()[selected]);
        if(command==0){if(itemdata[item].type&1){inventory.equip(selected);}else if(item==145)open_file(inventory,0,true);
            else if((itemdata[item].type&0x5f)==8){ // ItemCommand -> ItemUse -> Use_00; the status screen stays open
                unsigned control=0;const auto result=inventory.use_item(selected,nullptr,control,stage,room,pulse.condition(),message.active());
                if(result.message)show_message(result.message);if(result.pulse)pulse.begin_cure(result.condition,result.pulse);}
            else{use_item=item;active=false;}}
        else if(command==1){if(item==145)open_file(inventory,0,true);else{examining=true;inspection=ItemInspection{};const auto angle=inspection_default_angles(item);examine_pitch=angle[0];examine_yaw=angle[1];examine_roll=angle[2];}}
        else if(command==2){combining=true;combine_from=selected;}else if(command==3){use_item=item;active=false;}
        else if(command==4&&toggle_shotgun_burst)toggle_shotgun_burst();
        commands=false;
    }return true;}
    // sub1.c CursorMove: listcsr_0 = selected (pip index, 1 = equipment), listcsr_2 = grid cell.
    // ItemSort keeps wide items first; sakai is the pip index of the first narrow/empty entry.
    {unsigned sakai=2;while(sakai<12){const auto id=Inventory::id(inventory.slots()[sakai]);if(!(id&&(itemdata[id].type&0x100)))break;++sakai;}
     const unsigned max=inventory.side_pack?10:8;unsigned& l0=selected;unsigned l2=grid_cell(inventory,selected);
     // CursorMove: up from the equipment cell (listcsr_2 == 1) or left from it (listcsr_0 == 1) enters MainCommand.
     if((key==MenuInput::Up&&l2==1)||(key==MenuInput::Left&&l2<=2&&l0==1)){nav=true;nav_pressed=false;nav_wait=0;maincsr=3;feedback_sound=0;return true;}
     if(key==MenuInput::Up&&l2!=1){if(l2>=3){if(l0<=sakai){l0--;l2-=2;}else{l0-=2;l2-=2;}}else{l2--;l0--;}}
     else if(key==MenuInput::Down&&l2<max){if(l0<sakai){if((l2&1)&&l0==sakai-1)l0+=2;else l0++;l2+=2;}else{l0+=2;l2+=2;}}
     if(key==MenuInput::Right&&l2<max+1){if(l0<sakai){if(l2<max){if(l2&1)l2++;else l2+=2;l0++;}}else{l0++;l2++;}}
     else if(key==MenuInput::Left&&l2>2){if(l0<sakai){if(l0>2){l0--;l2-=2;}}else{l0--;l2--;}}
     selected=std::clamp(l0,1u,11u);
     while(selected>1&&grid_cell(inventory,selected)>max+1)--selected;} // host: a bag removed while open leaves no locked cell
    if(key==MenuInput::Confirm){if(combining){const auto rule=inventory.combination_rule(selected,combine_from);
        if(rule>=0&&(combidata[rule+2]&255)==6){pending_combine_first_=selected;pending_combine_second_=combine_from;show_message(155);}else combine(inventory,selected,combine_from);
    }else if(Inventory::id(inventory.slots()[selected])){commands=true;command=0;}}
    return true;
}
GameUi::GameUi(const std::filesystem::path& data,const std::filesystem::path& mods):data_(data),mods_(mods){
    auto load=[&](const char* identity,unsigned count){std::vector<SceneTexture> out;for(unsigned i=0;i<count;++i){const auto rel=std::filesystem::path("textures")/identity/(asset_number(i)+".tga");out.push_back(read_tga(!mods.empty()&&std::filesystem::is_regular_file(mods/rel)?mods/rel:data/rel));}return out;};
    menu_=decode_texture_bundle(read_asset_file(data/"archives/ITEM1/0145.bin",8*1024*1024));
    for(auto& texture:menu_)for(auto& bank:texture.palette_banks)for(auto& pixel:bank.second)pixel=(pixel&0xffffff)|(std::min(255u,(pixel>>24)*2)<<24);
    auto menu_overrides=load("menu",5);for(unsigned i=0;i<menu_overrides.size();++i)menu_[i].pixels=menu_overrides[i].pixels;
    fonts_=load("fonts",4);
    dialog_fonts_=fonts_;
    // The original atlas includes a dark edge and a muted cream glyph core.
    // Map that ramp to black/white for dialogue, retaining its antialiasing and alpha.
    for(auto& font:dialog_fonts_){unsigned low=255,high=0;for(auto c:font.pixels)if(c>>24){const auto value=std::max({c&255,(c>>8)&255,(c>>16)&255});low=std::min(low,value);high=std::max(high,value);}
        if(high>low)for(auto& c:font.pixels)if(c>>24){const auto value=std::max({c&255,(c>>8)&255,(c>>16)&255});const auto shade=(value-low)*255/(high-low);c=(c&0xff000000)|shade*0x010101;}}
    messages_=read_asset_file(data/"files/SYSMES.ALD",4*1024*1024);BinaryView v(messages_);system_table_=8+v.u32(0);v.require(system_table_,4);
}

void GameUi::label(SoftwareRenderer& image,const std::string& str,int x,int y,float scale,std::uint32_t color)const{
    Message message;for(unsigned char c:str)message.push_back(c=='\n'?0xff00:c==' '?0xff01:c>=32&&c<140?c-32:31);message.push_back(0xffff);text(image,message,x,y,scale,color,true);
}
void GameUi::draw_title(SoftwareRenderer& image){
    if(title_.empty()){
        const auto packet=read_asset_file(data_/"archives/ADV/0002.bin",8*1024*1024);BinaryView view(packet);
        for(unsigned i=0;i<3;++i){const auto resource=view.u32(i*4);const auto offset=view.relative(resource,view.u32(resource+4),256);
            const auto length=128u+view.u32(offset+128);view.require(offset,length);
            // Wrap the existing extended TIM2 in the shared decoder's PVP envelope.
            std::vector<std::uint8_t> bundle(44+32+length+4);auto put=[&](std::size_t p,std::uint32_t value){for(unsigned j=0;j<4;++j)bundle[p+j]=std::uint8_t(value>>(j*8));};
            put(0,0x00494c50);put(4,12);put(32,1);put(44,0x324d4954);put(48,length);std::copy_n(packet.begin()+offset,length,bundle.begin()+76);put(76+length,0xffffffff);
            auto textures=decode_texture_bundle(bundle);if(textures.size()!=1)throw std::runtime_error("original title texture missing");title_.push_back(std::move(textures.front()));
            for(auto& pixel:title_.back().pixels)pixel=(pixel&0xffffff)|(std::min(255u,(pixel>>24)*2)<<24);
        }
        Scene replacement;replacement.textures=title_;apply_texture_files(replacement,data_,mods_,"title");title_=std::move(replacement.textures);
    }
    std::fill(image.canvas().begin(),image.canvas().end(),0xff000000);
    blit_pixels(image,title_[0],0,0,640,480,0,0,int(title_[0].width)*640/1024,int(title_[0].height)*480/512);
    blit_pixels(image,title_[2],20,396,352,52,0,460,352,512);
    blit_pixels(image,title_[2],348,418,240,32,272,432,512,464);
}
void GameUi::draw_title_options(SoftwareRenderer& image,unsigned selected){
    if(title_.empty())draw_title(image);
    blit_pixels(image,title_[2],64,288,512,32,0,96,512,128,selected==0?0xffffffff:0xff7f7f7f);
    blit_pixels(image,title_[2],64,320,512,32,0,128,512,160,selected==1?0xffffffff:0xff7f7f7f);
    label(image,"Beenden",264,352,.9f,selected==2?0xffffffff:0xff7f7f7f);
}
Message GameUi::system_message(unsigned i)const{return read_message(messages_,system_table_,i);}
void GameUi::draw_pause(SoftwareRenderer& image,bool controller_present)const{
    // screen.c bhDrawScreenSaver: mode 8/6, a black 640x480 polygon with
    // sys->ssv_an=64.  With black source that is dest * (255-64)/255.
    for(auto& pixel:image.canvas()){std::uint32_t out=0xff000000;for(unsigned shift:{0u,8u,16u})out|=unsigned(((pixel>>shift)&255u)*(255u-64u)/255u)<<shift;pixel=out;}
    // system.c: pause_mes at (280,188), then EasyDispMessage(244,493/492).
    show_message(image,Message{0x30,0x41,0x55,0x53,0x45,0xffff},280,188);
    const auto message=system_message(controller_present?493:492);
    // EasyDispMessage centers every 0xff00-delimited line independently at
    // x=320. The no-controller branch calls bhFontScaleSet(.71,1,1).
    const float sx=controller_present?1.f:.71f;std::size_t begin=0;int y=244;
    while(begin<message.size()){
        std::size_t end=begin;float width=0;for(;end<message.size()&&message[end]!=0xff00&&message[end]!=0xffff;++end){const auto c=message[end];width+=(c==0xff01?14.f:(c<std::size(FontSz)?FontSz[c]:28))*sx;}
        Message line(message.begin()+begin,message.begin()+end);line.push_back(0xffff);
        text(image,line,int(320.f-width/2.f),y,sx,0xffffffff,true,-1,1.f);
        if(end==message.size()||message[end]==0xffff)break;begin=end+1;y+=30;
    }
}
void GameUi::draw_event_timer(SoftwareRenderer& image,const EventVm& events)const{
    if(!events.event_deadline_visible())return;
    // bhDispTime(&pos, 7, sys->evt_tim, colour, -.9): minutes, seconds and
    // centiseconds.  An omitted leading minute digit still advances 20 px.
    const unsigned time=events.event_deadline_ticks,min=(time/3600)%60,sec=(time/60)%60,msc=((time%60)*10)/6;
    const auto colour=time<events.event_deadline_danger?0xffff2020u:0xff20ff20u;
    float x=222,y=76;
    auto glyph=[&](unsigned code){if(code/324>=fonts_.size())return;blit(image,fonts_[code/324],int(x),int(y),28,28,(code%18)*14,(code/18)*14,(code%18)*14+14,(code/18)*14+14,colour);};
    if(min/10)glyph(16+min/10);x+=20;glyph(16+min%10);x+=20;glyph(7);x+=FontSz[7];
    glyph(16+sec/10);x+=20;glyph(16+sec%10);x+=20;glyph(2);x+=FontSz[2];
    glyph(16+msc/10);x+=20;glyph(16+msc%10);
}
std::string GameUi::item_name(unsigned id)const{return message_text(read_message(messages_,4,id));}
void GameUi::text(SoftwareRenderer& image,const Message& message,int x,int y,float scale,std::uint32_t color,bool dialogue,int selected_choice,float yscale)const{
    const int start=x;int choice=0;
    std::function<void(const Message&,unsigned)> write=[&](const Message& part,unsigned depth){
      for(std::size_t i=0;i<part.size();++i){const auto c=part[i];if(c==0xffff)break;if(c==0xff00){x=start;y+=int(30*yscale);continue;}if(c==0xff01){x+=int(14*scale);continue;}
        // bhDispItemName advances the actual font cursor, including variable
        // glyph widths. Character-count estimates overlap the following '?' .
        if(c==0xff03){if(i+1>=part.size())break;if(depth>=4)throw std::runtime_error("recursive item-name message");write(read_message(messages_,4,part[++i]),depth+1);continue;}
        if(c==0xff04){if(choice++==selected_choice)blit_pixels(image,menu_[0],x,y+int(5*yscale),int(18*scale),int(18*yscale),0,28,18,46);x+=int(28*scale);continue;}if(c>=0xfe00){if(c<0xff00)color=c==0xfe01?0xffffe020:0xffffffff;continue;}if(c/324>=fonts_.size())continue;
        const auto glyph=c%324;blit(image,(dialogue?dialog_fonts_:fonts_)[c/324],x,y,int(28*scale),int(28*yscale),(glyph%18)*14,(glyph/18)*14,(glyph%18)*14+14,(glyph/18)*14+14,color);
        x+=int((c<std::size(FontSz)?FontSz[c]:28)*scale);
      }
    };write(message,0);
}
void GameUi::ascii(SoftwareRenderer& image,const std::string& str,int x,int y,float scale,std::uint32_t color)const{Message m;for(unsigned char c:str)m.push_back(c=='\n'?0xff00:c==' '?0xff01:c>=32&&c<140?c-32:31);m.push_back(0xffff);text(image,m,x,y,scale,color);}
void GameUi::icon(SoftwareRenderer& image,unsigned id,int x,int y,int height,int narrow_width)const{
    unsigned n=id;switch(id){case 30:n=13;break;case 63:case 128:case 129:n=41;break;case 130:n=35;break;case 136:n=47;break;case 138:n=48;break;case 142:n=128;break;case 143:n=49;break;case 144:n=102;break;case 145:n=133;break;}
    const auto item=n;if(item>63)n-=64;if(item<64){if(n&&n<4)n=n*2-1;else if(n<33){if(n)n+=4;}else if(n==33||n==34)n+=n-33+4;else n+=6;}
    const bool wide=itemdata[id].type&0x100;const int width=narrow_width*(wide?2:1),u=int(n%7)*34,v=int(n/7)*24;
    blit(image,menu_[item<64?4:3],x,y,width,height,u,v,u+(wide?68:34),v+(height==19?12:24));
}
StatusSlide::StatusSlide(){for(unsigned c=0;c<12;++c)for(unsigned i=0;i<6;++i)pos_[c][i]=i<2?cen_pos99[c][i+2]:0.f;pos_[3][0]=332;}
void StatusSlide::open(bool box){
    box_=box;closed_=false;cnt_=0;fade_=0;sound_=0xffffffff; // BgColorInit
    // CenterPositionInit: start at cen_pos99[0..1], delta to the settled [2..3].
    for(unsigned c=0;c<12;++c){for(unsigned i=0;i<6;++i)pos_[c][i]=cen_pos99[c][i];pos_[c][4]=cen_pos99[c][2]-cen_pos99[c][0];pos_[c][5]=cen_pos99[c][3]-cen_pos99[c][1];}
    flg_=box?0x102080u:0x10a080u;                  // ItemBoxInit / StatusInit
    open1_=box?0:23;open2_=box?0:46;
    flg_|=0x40;                                    // StatusMain case 1
}
void StatusSlide::cancel(){
    flg_|=0x4;
    if(box_)flg_|=0x2000;else{flg_|=0x20000;flg_&=~0x10000u;}
    // StatusCancel: every center except 0 and 7 slides to its off-screen target.
    pos_[1][3]=-80;pos_[2][2]=-448;pos_[3][2]=640;pos_[4][2]=808;pos_[5][2]=644;pos_[6][3]=484;pos_[8][2]=-380;pos_[9][2]=644;pos_[10][3]=-384;pos_[11][2]=660;
    for(unsigned c=1;c<12;++c){auto& p=pos_[c];if(c!=3){p[4]=p[2]-p[0];p[5]=p[3]-p[1];}else p[4]=p[2]-352.f;}
}
void StatusSlide::file_out(){
    cancel();flg_&=~(4u|0x20000u);flg_|=0x9002040u;cnt_=0;closed_=false;
}
void StatusSlide::change_view(){flg_|=0x20040;flg_&=~0x10000u;}
void StatusSlide::sprite_h(){
    if(!(flg_&0x2000)||(flg_&0x8000000))return;
    for(auto& p:pos_)if(cnt_!=8){p[0]+=p[4]/8.f;p[1]+=p[5]/8.f;}
    if(!(flg_&0x4)&&!(flg_&0x1000000)){
        bool chk;if(cnt_==8){chk=true;sound_=7;}else{++cnt_;chk=false;} // CallSystemSe(1,7)
        if(chk){flg_|=0x10000000;flg_|=0x10000;flg_&=~1u;flg_&=~0x2000u;if(box_)flg_&=~0x40u;cnt_=0;return;}
        flg_|=1|0x2000|0x40;flg_&=~0x10000u;flg_&=~0x10000000u;return;
    }
    const bool chk=cnt_==8;if(!chk)++cnt_;
    if(flg_&0x4){if(chk){cnt_=0;flg_|=0x80000;flg_&=~2u;flg_&=~0x2000u;closed_=true;}else{flg_|=2|0x2000;flg_&=~0x80000u;}}
    else if(flg_&0x1000000){if(chk){cnt_=0;flg_|=0x40000;flg_&=~(2u|0x2000u);closed_=true;}else{flg_&=~0x40000u;flg_|=2|0x2000;}}
}
void StatusSlide::sprite_uv(){
    if(!(flg_&0x18030000))return;
    flg_|=0x40;
    if(flg_&0x10000){if(open1_){open1_=std::max(0,open1_-4);}if(open2_){open2_=std::max(0,open2_-8);}}
    if(flg_&0x20000){if(open1_<23)open1_=std::min(23,open1_+4);if(open2_<46)open2_=std::min(46,open2_+8);}
    if(flg_&0x10000){if(!open1_&&!open2_){flg_|=0x4000;if(!(flg_&0x10000000))flg_&=~0x40u;flg_&=~0x10000u;flg_&=~0x8000u;}}
    else if((flg_&0x20000)&&open1_==23&&open2_==46){sound_=9;flg_|=0x8000;flg_&=~0x20000u;flg_&=~0x4000u;if(flg_&4){flg_|=0x2000;flg_&=~0x10u;}} // CallSystemSe(0,9)
    if((flg_&4)||(flg_&0x8000000)){if(pos_[3][0]<352){pos_[3][0]+=8;if(pos_[3][0]>352){pos_[3][0]=352;pos_[3][4]=288;pos_[3][2]=640;flg_&=~0x8000000u;}}}
    else if(pos_[3][0]>332){pos_[3][0]-=4;if(pos_[3][0]<=332){pos_[3][0]=332;flg_&=~0x10000000u;if(!(flg_&0x30000))flg_&=~0x40u;}}
}
bool StatusSlide::tick(){
    if(!(flg_&0x44)&&!(flg_&0x2000)&&!(flg_&0x18030000))return false;
    // CursorMove testmode 1 (mode 1 of the status screen) converts the init flag first.
    if(!box_&&(flg_&0x8000)){flg_|=0x80|0x10000;flg_&=~0x10u;flg_&=~0x8000u;}
    sprite_h();
    if(!box_)sprite_uv();
    constexpr float step=0.057142857f;
    if(flg_&1)fade_+=step;if(flg_&2)fade_-=step;   // BGFadeIn/BGFadeOut
    fade_=std::clamp(fade_,0.f,1.f);
    return closed_;
}
// Mengenanzeige (D09), eigenständig nativ nach der Semantik von BulletSet
// (sub1.c). Zwei Anzeigeklassen über itemdata[].type & 0x1800:
//   0x800  dreistellige Stückzahl (Hunderter, Zehner, Einer),
//   0x1000 Prozentanzeige: Wert / (max/100), danach dieselbe Zifferntrennung
//          plus das Prozentzeichen (Glyphe 10) als vierte Stelle.
// Das gepackte Flagbit 0x800 (unendlich) zeigt statt Ziffern die Glyphe 11.
// Führende Nullen entfallen, die letzte Stelle wird immer gezeichnet; bei der
// Prozentklasse bleibt ab der vorletzten Stelle alles sichtbar, damit "0 %"
// nicht auf das Zeichen allein zusammenfällt.
// Farbbank: Munition (type & 1) wählt über die Flagbits 0x7000 die Variante
// (Item 6 vier Sorten, Item 7 zwei, sonst Standard); die Verbrauchsklassen
// (type & 0x56) haben feste Banken für 16, 17/36 und 37. Die Bankindizes
// stammen aus den Originalpaletten der Statusgrafik.
void GameUi::quantity(SoftwareRenderer& image,std::uint32_t packed,int x,int y,bool box)const{
    const unsigned id=Inventory::id(packed);
    const unsigned kind=itemdata[id].type&0x1800;
    if(!kind)return;
    const unsigned flags=(packed>>16)&0xff00;
    const bool endless=(flags&0x800)!=0;
    const unsigned amount=packed&0xffff;
    unsigned glyph[4]{};unsigned count=0;
    if(endless){glyph[0]=0;glyph[1]=0;glyph[2]=11;count=3;}
    else if(kind==0x800){
        glyph[0]=amount/100;glyph[1]=(amount%100)/10;glyph[2]=amount%10;count=3;
    }else{
        const unsigned step=itemdata[id].max/100;
        unsigned percent=step?amount/step:0;
        if(amount&&!percent)percent=1; // angefangene Ladung zeigt mindestens 1 %
        glyph[0]=percent/100;glyph[1]=(percent%100)/10;glyph[2]=percent%10;glyph[3]=10;count=4;
    }
    unsigned bank=5;
    if(itemdata[id].type&1){
        if(id==6)switch(flags&0x7000){case 0x1000:bank=9;break;case 0x2000:bank=6;break;case 0x4000:bank=7;break;default:bank=10;break;}
        else if(id==7)bank=(flags&0x7000)==0x2000?6u:10u;
        else bank=10;
    }else if(itemdata[id].type&0x56){
        bank=id==16?9u:(id==17||id==36)?6u:id==37?7u:10u;
    }
    auto texture=menu_[0];const auto palette=texture.palette_banks.find(bank);
    if(palette==texture.palette_banks.end())throw std::runtime_error("missing original UI palette bank");
    texture.pixels=palette->second;
    unsigned column=0;bool drawn=false;
    for(unsigned i=0;i<count;++i){
        const bool last=i+1==count;
        const bool percent_tail=kind==0x1000&&!endless&&i+2>=count;
        if(!glyph[i]&&!drawn&&!last&&!percent_tail)continue;
        const auto& t=parts_20[glyph[i]];
        blit(image,texture,x+int(column*10),y+(box?19:32),t.sx,t.sy,t.u1,t.v1,t.u2,t.v2);
        ++column;drawn=true;
    }
}
void GameUi::draw_box(SoftwareRenderer& image,const Inventory& inv,const StatusMenu& menu,unsigned base,unsigned index,bool selecting,const StatusSlide* slide,unsigned exit_state,unsigned note)const{
    rectangle(image,0,0,640,480,0xff101010);
    // Slide offsets of the original cen_pos centers relative to the settled layout (sub1.c SpriteH).
    auto dx=[&](unsigned c){return slide?int(std::lround(slide->x(c)))-centers[c][0]:0;};
    auto dy=[&](unsigned c){return slide?int(std::lround(slide->y(c)))-centers[c][1]:0;};
    std::vector<PARTS> sprites;
    auto group=[&](const auto& table){for(const auto& p:table){if(p.anim==-1||p.anim==255)break;sprites.push_back(p);}};
    // ItemBoxInit's settled sprite groups. Security boxes retain parts03b at
    // center 9; normal boxes show parts18b. No host rectangle or caption.
    for(unsigned i=0;parts_04b[i].anim!=-1&&parts_04b[i].anim!=255;++i){auto p=parts_04b[i];if(!inv.side_pack&&(i==6||i==8||i==9||i==13||i==14))p.pos[1]-=48;sprites.push_back(p);}
    group(parts_05b);
    for(auto p:parts_07b){if(p.anim==-1||p.anim==255)break;if(slide)p.col.r=p.col.g=p.col.b=slide->fade();sprites.push_back(p);} // BGFadeIn/Out
    group(parts_11b);
    // CursorInit disables the second/third cursor sprites. CursorSet positions
    // the first over the selected pip cell; never draw the uninitialised
    // 364-pixel second cursor through the security list.
    if(!selecting&&!exit_state){auto p=parts_09b[0];const auto cell=grid_cell(inv,menu.selected);const bool wide=itemdata[Inventory::id(inv.slots()[menu.selected])].type&0x100;
        p.pos[0]=cell>=2?20+int((cell&~unsigned(wide))%2)*68:base==64?88:4;p.pos[1]=cell>=2?4+int((cell/2)-1)*48:-76;
        p.atr|=0x20;sprites.push_back(p);}

    for(unsigned i=0;i<std::size(parts_15b)&&parts_15b[i].anim!=-1&&parts_15b[i].anim!=255;++i){if(base==64&&i>=31)continue;auto p=parts_15b[i];if(i==22){p.anim=0;p.parts_num=exit_state==0?10:exit_state==1?12:13;}if(i==18||i==19)p.anim=std::any_of(inv.items.begin()+base,inv.items.begin()+base+(base==64?128:32),[](auto item){return item!=0;})?17:16;if(i==27||i==28)p.pos[1]=68+int((base==64?1.59375f:6.375f)*index);sprites.push_back(p);}
    for(unsigned i=0;i<std::size(parts_16b)&&parts_16b[i].anim!=-1&&parts_16b[i].anim!=255;++i){auto p=parts_16b[i];if(!i){p.anim=0;p.parts_num=exit_state?11:0;}sprites.push_back(p);}group(parts_17b);
    if(base==64)group(parts_18b);else for(auto p:parts_03b){if(p.anim==-1||p.anim==255)break;p.cen_no=9;sprites.push_back(p);}
    std::stable_sort(sprites.begin(),sprites.end(),[](const auto& a,const auto& b){return a.pos[2]>b.pos[2];});
    for(const auto& p:sprites){if(p.parts_num>=23||!tables[p.parts_num]||p.cen_no>=12)continue;auto t=tables[p.parts_num][p.anim];if(p.parts_num==15&&p.anim==0)t.sx=(itemdata[Inventory::id(inv.slots()[menu.selected])].type&0x100)?144:76;if(t.texid<0||unsigned(t.texid)>=menu_.size())continue;
        const int nx=(p.setnum>>4)+1,ny=(p.setnum&15)+1;for(int i=0;i<std::max(nx,ny);++i)blit(image,menu_[t.texid],int(p.pos[0])+centers[p.cen_no][0]+dx(p.cen_no)+int(std::min(i,nx-1)*p.move),int(p.pos[1])+centers[p.cen_no][1]+dy(p.cen_no)+int(std::min(i,ny-1)*p.move),t.sx,t.sy,t.u1,t.v1,t.u2,t.v2,tint(p.col),bool(p.atr&8),p.ang);
    }
    const int count=base==64?128:32;
    // ItemBoxIconSet/SpriteOnOff: 38-pixel rows; only 31 < y < 259
    // displayed, bottom row clipped to 19 pixels. Original item-name states.
    for(int row=1;row<6;++row){const auto packed=inv.items[base+(int(index)+count+row-3)%count];const auto id=Inventory::id(packed);const int y=40+31+38*row+dy(10),x0=dx(10);
        if(selecting&&row==3){rectangle(image,43+x0,y-2,348,2,0xff9f9f80);rectangle(image,43+x0,y+36,348,2,0xff9f9f80);}
        if(id){text(image,read_message(messages_,4,inventory_name_id(packed)),44+x0,y,.95f);icon(image,id,324+x0,y,row==5?19:38,42);quantity(image,packed,324+x0,y,true);}
    }
    unsigned cell=0;for(unsigned slot=1;slot<12;++slot){const auto id=Inventory::id(inv.slots()[slot]);const unsigned center=slot==1?(base==64?4:9):5;const int x=(slot==1?(base==64?528:444):460+int(cell&1)*68)+dx(center),y=(slot==1?48:128+int(cell/2)*48)+dy(center);
        if(!selecting&&!exit_state&&slot==menu.selected){rectangle(image,x-1,y-1,id&&(itemdata[id].type&0x100)?138:70,50,0xff238699);rectangle(image,x+1,y+1,id&&(itemdata[id].type&0x100)?134:66,46,0xff111f22);}
        icon(image,id,x,y);if(id)quantity(image,inv.slots()[slot],x,y);
        if(slot>1)cell+=id&&(itemdata[id].type&0x100)?2:1;if(cell>=inv.capacity())break;
    }
    const auto packed=selecting?inv.items[base+index]:inv.slots()[menu.selected];if(menu.message.active())text(image,menu.message.page(),44+dx(6),342+dy(6),1,0xffffffff,true,int(menu.message.selection));else if(note)text(image,system_message(note),44+dx(6),342+dy(6),1,0xffffffff,true);else if(Inventory::id(packed))text(image,read_message(messages_,4,inventory_name_id(packed)),44+dx(6),342+dy(6),.95f);
}
void GameUi::draw(SoftwareRenderer& image,const Inventory& inv,const StatusMenu& menu,const MessageDialog& dialog,unsigned tick){
    if(menu.active&&menu.files&&!menu.file_list){
        // Original filescreen replaces the status sprites. parts_22b[1] is
        // the wallpaper behind bhDispMessage; [0] is a captured framebuffer,
        // not the source wallpaper stretched across a host dialog.
        rectangle(image,0,0,640,480,0xff000000);
        if(!file_textures_.count(menu.file_number)){const auto packet=read_asset_file(data_/"archives/ITEM1"/(asset_number(original_file_wallpaper(menu.file_number))+".bin"),8*1024*1024);file_textures_[menu.file_number]=decode_texture_bundle(packet);}
        const auto& textures=file_textures_[menu.file_number];
        if(!textures.empty()){
            const auto& texture=textures[0];const int shift=menu.file_x-68;
            blit(image,texture,162+shift,80,320,240,0,0,160,160,0xff808080);
            auto decoration=[&](unsigned anim,int x,std::uint32_t color){const auto& t=parts_22[anim];blit(image,texture,x,224,t.sx,t.sy,t.u1,t.v1,t.u2,t.v2,color);};
            if(!menu.file_scroll){
                const bool last=menu.file_page+1==menu.file_pages();
                if(menu.file_page&&(!last||!menu.file_exit_selected))decoration(5,16,0xff00ff00);
                if(!last)decoration(6,592,0xff00ff00);
                if(last)decoration(7,560,menu.file_exit_selected?0xffffffff:0xff666666);
            }
        }
        text(image,system_message(original_file_message(menu.file_number,menu.file_display_page)),menu.file_x,64,1,0xffffffff,true);
        return;
    }
    if(menu.active){
        rectangle(image,0,0,640,480,0xff101010);
        // Slide offsets of the original cen_pos centers relative to the table layout (sub1.c SpriteH/SpriteUV).
        auto dx=[&](unsigned c){return int(std::lround(menu.slide.x(c)))-centers[c][0];};
        auto dy=[&](unsigned c){return int(std::lround(menu.slide.y(c)))-centers[c][1];};
        struct Sprite {PARTS p;unsigned pulse=~0u;};std::vector<Sprite> sprites;
        auto group=[&](const auto& table){for(const auto& p:table){if(p.anim==-1||p.anim==255)break;sprites.push_back({p});}};
        // Settled StatusInit layout. Conditional command/examine layers are added separately.
        // StatusInit/MainCommand: highlight (parts_00b[n], parts 11) and label state (parts_00b[11+n], parts 10/12/13).
        for(unsigned i=0;parts_00b[i].anim!=-1&&parts_00b[i].anim!=255;++i){auto p=parts_00b[i];
            if(i<4){const bool on=(menu.nav||menu.files)?i==menu.maincsr:i==3;p.anim=0;p.parts_num=on?11:0;}
            else if(i>=11&&i<15){const unsigned n=i-11;const bool on=(menu.nav||menu.files)?n==menu.maincsr:n==3;p.anim=int(n);p.parts_num=!on?10:(menu.nav&&!menu.nav_pressed)?12:13;}
            sprites.push_back({p});}
        group(parts_01b);group(parts_02b);group(parts_03b);
        // Original SidePackSet layout: the fifth row exists only with the bag.
        for(unsigned i=0;parts_04b[i].anim!=255&&parts_04b[i].anim!=-1;++i){auto p=parts_04b[i];if(!inv.side_pack&&(i==6||i==8||i==9||i==13||i==14))p.pos[1]-=48;sprites.push_back({p});}
        group(parts_05b);
        for(auto p:parts_07b){if(p.anim==-1||p.anim==255)break;p.col.r=p.col.g=p.col.b=menu.slide.fade();sprites.push_back({p});} // BGFadeIn/Out
        group(parts_11b);
        // SpriteUV: the view-window shutters (parts_08b) close/open with open1/open2.
        for(unsigned i=0;i<4;++i){auto p=parts_08b[i];if(i==1)p.pos[1]=124.f+(46-menu.slide.open2())*2;if(i==3)p.pos[0]=366.f+(23-menu.slide.open1())*2;sprites.push_back({p});}
        for(unsigned i=0;(!menu.files||menu.file_entering)&&i<11;++i){auto p=parts_10b[i];p.anim=i<5?int(inv.player)+int(i)*4:i==10?int(menu.pulse_open()?menu.pulse.condition():StatusPulse::condition(inv.hp,inv.poisoned)):p.anim;sprites.push_back({p});}
        // SpriteDisp submits -PARTS.pos[2]; njDrawTextureSubH submits -1/z.
        // Keep both in the same order: trace z=.1 -> 10, cure z=.2 -> 5,
        // in front of the panel (21), behind the closing shutters (1).
        for(unsigned i=0;(!menu.files||menu.file_entering)&&i<menu.pulse.quads().size();++i){PARTS p{};const float z=menu.pulse.quads()[i][0].z;p.pos[2]=z?std::min(1.f/z,65534.f):65534.f;sprites.push_back({p,i});}
        std::stable_sort(sprites.begin(),sprites.end(),[](const Sprite& a,const Sprite& b){return a.p.pos[2]>b.p.pos[2];});
        for(auto s:sprites){if(s.pulse!=~0u){auto quad=menu.pulse.quads()[s.pulse];for(auto& v:quad){v.x+=float(dx(2));v.y+=float(dy(2));}pulse_quad(image,menu_[2],quad);continue;}auto& p=s.p;if(p.parts_num>=23||!tables[p.parts_num]||p.cen_no>=12)continue;auto t=tables[p.parts_num][p.anim];if(p.parts_num==8){const int o1=menu.slide.open1(),o2=menu.slide.open2();if(p.anim==0){t.sy=o2*2;t.v1=46-o2;}else if(p.anim==1){t.sy=o2*2;t.v2=o2+78;}else if(p.anim==2){t.sx=o1*2;t.u1=249-o1;}if(t.sx<=0||t.sy<=0)continue;}
            if(t.texid<0||unsigned(t.texid)>=menu_.size())continue;
            auto x=int(p.pos[0])+centers[p.cen_no][0]+dx(p.cen_no),y=int(p.pos[1])+centers[p.cen_no][1]+dy(p.cen_no);
            const int nx=(p.setnum>>4)+1,ny=(p.setnum&15)+1;const int reps=std::max(nx,ny);
            for(int i=0;i<reps;++i)blit(image,menu_[t.texid],x+(i<nx?int(i*p.move):int((nx-1)*p.move)),y+(i<ny?int(i*p.move):int((ny-1)*p.move)),t.sx,t.sy,t.u1,t.v1,t.u2,t.v2,tint(p.col),bool(p.atr&8),p.ang);
        }
        // ItemSet, personal slot + the two-column normal inventory; large weapons span a row.
        unsigned cell=0;for(unsigned slot=1;slot<12;++slot){const auto id=Inventory::id(inv.slots()[slot]);const unsigned center=slot==1?4:5;const int x=(slot==1?528:460+int(cell&1)*68)+dx(center),y=(slot==1?48:128+int(cell/2)*48)+dy(center);
            if(menu.combining&&slot==menu.combine_from){rectangle(image,x-1,y-1,(id&&(itemdata[id].type&0x100))?138:70,50,0xffd9ab47);rectangle(image,x+1,y+1,(id&&(itemdata[id].type&0x100))?134:66,46,0xff111f22);}
            if(slot==menu.selected&&!menu.nav){rectangle(image,x-1,y-1,(id&&(itemdata[id].type&0x100))?138:70,50,0xff238699);rectangle(image,x+1,y+1,(id&&(itemdata[id].type&0x100))?134:66,46,0xff111f22);}
            icon(image,id,x,y);if(id){quantity(image,inv.slots()[slot],x,y);if(inv.slots()[0]==slot)ascii(image,"E",x+2,y+2,.6f,0xff50d090);}
            if(slot>1)cell+=id&&(itemdata[id].type&0x100)?2:1;if(cell>=inv.capacity())break;
        }
        if(inv.slots()[0]){const auto id=Inventory::id(inv.slots()[inv.slots()[0]]);icon(image,id,352+((itemdata[id].type&0x100)?24:58)+dx(3),48+dy(3));} // original ArmsSet
        const auto selected_id=menu.pickup?menu.pickup:Inventory::id(inv.slots()[menu.selected]);if(menu.nav&&!menu.files&&!menu.message.active())text(image,system_message(180-menu.maincsr),44+dx(6),342+dy(6),.95f); // MainCommand: bhSetMessage(1,180-maincsr)
        else if(selected_id&&!menu.files&&!menu.message.active()&&!(menu.pickup&&dialog.active()))text(image,read_message(messages_,4,menu.pickup?menu.pickup:inventory_name_id(inv.slots()[menu.selected])),44+dx(6),342+dy(6),.95f);
        if(menu.commands||menu.erase_frames){
            // cen_pos[11] slides 24 px per frame; 96 px is the fully opened window.
            const bool erasing=!menu.commands;const int slide=(4-int(erasing?menu.erase_frames:menu.command_frames))*24;
            const unsigned command_item=erasing?menu.erase_item:selected_id;const unsigned cursor=erasing?menu.erase_command:menu.command;
            const auto command_parts=item_command_parts(command_item,cursor,menu.shotgun_burst&&menu.shotgun_burst());
            for(const auto& p:command_parts){if(!(p.atr&0x20))continue;const auto& part=tables[p.parts_num][p.anim];const int nx=(p.setnum>>4)+1,ny=(p.setnum&15)+1;
                for(int i=0;i<std::max(nx,ny);++i)blit(image,menu_[part.texid],int(p.pos[0])+centers[p.cen_no][0]-96+(p.cen_no==11?slide:0)+int(std::min(i,nx-1)*p.move),int(p.pos[1])+centers[p.cen_no][1]+int(std::min(i,ny-1)*p.move),part.sx,part.sy,part.u1,part.v1,part.u2,part.v2,tint(p.col),bool(p.atr&8),p.ang);
            }}
        if(menu.combining)ascii(image,"Combine",44,400,.8f,0xff50c0ff);
        if((menu.examining||menu.pickup)&&selected_id&&selected_id<145){
            const auto source=menu.pickup?menu.pickup:menu.inspection.running()?menu.inspection.item():selected_id;
            const auto id=inspection_model_id(source);
            if(!item_packets_.count(id)){auto file=mods_/"items"/(asset_number(id)+".bin");if(!std::filesystem::is_regular_file(file))file=data_/"archives/ITEM1"/(asset_number(id-1)+".bin");item_packets_[id]=read_asset_file(file,8*1024*1024);}
            if(!item_models_.count(source)){auto object=decode_item_asset(item_packets_[id],inspection_geometry(item_packets_[id],source));apply_texture_files(object,data_,mods_,std::filesystem::path("items")/asset_number(id));item_models_[source]=std::move(object);}
            auto object=menu.inspection.running()?decode_item_asset(item_packets_[id],inspection_geometry(item_packets_[id],source,source==menu.inspection.source()?menu.opening_frame:0)):item_models_[source];
            if(menu.inspection.running())object.textures=item_models_[source].textures;
            // npGetWHDSize: grösste Absolutkoordinate je Achse über die
            // gezeichneten Knoten; daraus ModelScaleSet.
            NJS_VECTOR whd{0,0,0};
            for(const auto& triangle:object.triangles)for(const auto& vertex:triangle.vertices){whd.x=std::max(whd.x,std::abs(vertex.position.x));whd.y=std::max(whd.y,std::abs(vertex.position.y));whd.z=std::max(whd.z,std::abs(vertex.position.z));}
            const float scale=inspection_model_scale(source,whd);
            NJS_MATRIX rotation;const auto spin=menu.inspection.running()?menu.inspection.spin():std::array<Angle,3>{};inspection_rotation(rotation,{menu.examine_pitch,menu.examine_yaw,menu.examine_roll},spin);
            // DrawSubItem: Kameramatrix (CameraSet) x rotmat x Zoomdrehung x
            // mw.pos x Modellskalierung. Die Kamera steht auf der originalen
            // Entfernung 20 - pos_0.z + pos_1.z, nicht auf einem Faktor.
            const bool animated=menu.inspection.running();
            const float distance=animated?menu.inspection.camera_distance():inspection_rest_camera_distance();
            const auto offset=animated?menu.inspection.model_offset():inspection_model_offset(source);
            NJS_MATRIX view;njUnitMatrix(&view);njTranslate(&view,0,0,distance);njMultiMatrix(&view,&rotation);njTranslate(&view,offset.x,offset.y,offset.z);njScale(&view,scale,scale,scale);
            object.background=0xff101010;SoftwareRenderer item_view(352,196);item_view.draw_view(object,view,500.f*1.174f,500.f);
            for(int y=0;y<196;++y){const int oy=y+132+dy(2),ox=60+dx(2);if(oy<0||oy>=480)continue;const int from=std::max(0,-ox),to=std::min(352,640-ox);if(to>from)std::copy(item_view.pixels().begin()+y*352+from,item_view.pixels().begin()+y*352+to,image.canvas().begin()+oy*640+ox+from);}
        }
        if(menu.files&&menu.file_list&&!menu.file_entering){
            const unsigned id=139;
            if(!item_packets_.count(id)){auto file=mods_/"items"/(asset_number(id)+".bin");if(!std::filesystem::is_regular_file(file))file=data_/"archives/ITEM1"/(asset_number(id-1)+".bin");item_packets_[id]=read_asset_file(file,8*1024*1024);}
            auto folder=menu.file_folder.scene(item_packets_[id],inv);apply_texture_files(folder,data_,mods_,std::filesystem::path("items")/asset_number(id));
            NJS_MATRIX rotation;inspection_rotation(rotation,inspection_default_angles(139),{});
            NJS_MATRIX view;njUnitMatrix(&view);njTranslate(&view,0,0,8);njMultiMatrix(&view,&rotation);
            const auto hide=itemflg[dsptbl[139].hide][2];njTranslate(&view,hoseipos[hide].x,hoseipos[hide].y,hoseipos[hide].z);njScale(&view,.58f,.58f,.58f);
            folder.background=0xff101010;SoftwareRenderer frame(352,184);frame.draw_view(folder,view,500.f*1.174f,500.f);
            for(int y=menu.slide.open2()*2;y<184-menu.slide.open2()*2;++y){const int oy=y+132+dy(2),ox=60+dx(2);if(oy<0||oy>=480)continue;const int from=std::max(menu.slide.open1()*2,-ox),to=std::min(352-menu.slide.open1()*2,640-ox);if(to>from)std::copy(frame.pixels().begin()+y*352+from,frame.pixels().begin()+y*352+to,image.canvas().begin()+oy*640+ox+from);}
            if(menu.file_folder.arrows()&&!menu.file_transition){for(unsigned i:{13u,14u}){const auto& p=parts_10b[i];const auto& t=tables[p.parts_num][p.anim];const auto level=unsigned(menu.file_folder.arrow_brightness(i-13)*255);const auto color=0xff000000u|(level<<16)|(level<<8)|level;blit(image,menu_[t.texid],int(p.pos[0])+centers[p.cen_no][0]+dx(p.cen_no),int(p.pos[1])+centers[p.cen_no][1]+dy(p.cen_no),t.sx,t.sy,t.u1,t.v1,t.u2,t.v2,color,false,p.ang);}}
            if(menu.file_folder.registers()&&!menu.file_folder.moving())text(image,read_message(messages_,4,159+menu.file_number),42+dx(6),342+dy(6),1);
        }
        (void)tick;
    }
    const auto& visible_dialog=menu.active&&menu.message.active()?menu.message:dialog;
    // bhControlMessage mode 1 uses mes_spos[1], replacing the item name.
    // Room dialogue uses mode 0; neither mode draws a background panel.
    if(visible_dialog.active()&&std::any_of(visible_dialog.page().begin(),visible_dialog.page().end(),[](auto code){return code<0xfe00||code==0xff03;}))text(image,visible_dialog.page(),menu.active?44:68,menu.active?342:374,1,0xffffffff,true,int(visible_dialog.selection));
}
}
