#include "ui.h"
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
#include "ui_tables.inc"
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
    if(item==145)return {"File","Examine","Combine"};
    if(itemdata[item].type&1){std::vector<std::string> list={"Equip","Examine","Combine","Use"};if(item==10)list.push_back(shotgun_burst&&shotgun_burst()?"Single":"Burst");return list;}
    return {"Use","Examine","Combine"};
}
void StatusMenu::reset(Inventory& inventory){
    // Discard transient prompts/actions before a debug mutation can reorder slots.
    if(examining&&!inspection.running()&&selected<12&&Inventory::id(inventory.slots()[selected]))inspection.cancel(inventory,selected);
    auto source=std::move(message_source);auto burst=std::move(shotgun_burst);auto toggle=std::move(toggle_shotgun_burst);const auto old_stage=stage,old_room=room;
    *this=StatusMenu{};message_source=std::move(source);shotgun_burst=std::move(burst);toggle_shotgun_burst=std::move(toggle);stage=old_stage;room=old_room;
}
void StatusMenu::show_message(unsigned index){if(!message_source)throw std::runtime_error("inventory message source is not bound");message.show(message_source(index),index);}
void StatusMenu::close_examine(Inventory& inventory){inspection.cancel(inventory,selected);examining=false;opening_item=opening_frame=0;}
void StatusMenu::inspect(Inventory& inventory){
    inspection.check(inventory,selected,{examine_pitch,examine_yaw,examine_roll});
    if(inspection.message_pending())show_message(inspection.take_message());
    opening_item=inspection.running()?inspection.source():0;opening_frame=0;
}
void StatusMenu::open_file(Inventory& inventory,unsigned file,bool list){
    inventory.items[383]|=1u<<file;files=true;file_list=list;file_number=file;file_page=0;commands=false;
}
void StatusMenu::finish_inspection(Inventory& inventory){
    if(inspection.completed_file()<24){open_file(inventory,inspection.completed_file());examining=false;inspection=ItemInspection{};}
}
void StatusMenu::combine(Inventory& inventory,unsigned first,unsigned second){
    unsigned index=0,destination=0;if(inventory.combine(first,second,&index,&destination)){combining=false;selected=destination;}
    else{feedback_sound=1;if(index)show_message(index);}
}
void StatusMenu::tick(Inventory& inventory){
    if(commands&&command_frames<4)++command_frames;else if(erase_frames)--erase_frames;
    // StatusInit runs PulseInit on every opening; StatusMain then runs PulseMain once per frame.
    const auto condition=StatusPulse::condition(inventory.hp,inventory.poisoned);
    if(!active)pulse_open_=false;else{if(!pulse_open_||(!pulse.healing()&&pulse.condition()!=condition)){pulse.open(condition);pulse_open_=true;}pulse.tick();}
    const bool had_message=message.active();message.tick();if(had_message&&!message.active())finish_inspection(inventory);
    if(!inspection.running())return;
    inspection.tick(inventory);opening_frame=inspection.action_frame();opening_item=inspection.running()?inspection.source():0;
    if(const auto angles=inspection.take_display_reset()){examine_pitch=(*angles)[0];examine_yaw=(*angles)[1];examine_roll=(*angles)[2];}
    if(inspection.message_pending())show_message(inspection.take_message());
}
bool StatusMenu::input(MenuInput key,Inventory& inventory){
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
        if(key==MenuInput::Cancel||key==MenuInput::Open){if(!file_list){file_list=true;file_page=0;}else{files=false;examining=false;}return true;}
        if(file_list){if(key==MenuInput::Up||key==MenuInput::Down){const unsigned step=key==MenuInput::Up?23:1;for(unsigned i=0;i<24;++i){file_number=(file_number+step)%24;if(inventory.items[383]&(1u<<file_number))break;}}
            if(key==MenuInput::Confirm){file_list=false;file_page=0;}return true;}
        if(key==MenuInput::Left&&file_page)--file_page;
        if((key==MenuInput::Right||key==MenuInput::Confirm)&&file_page+1<original_file_pages(file_number))++file_page;
        return true;
    }
    if(key==MenuInput::Open){if(examining)close_examine(inventory);active=!active;commands=examining=combining=false;selected=1;command=0;inspection=ItemInspection{};return true;}if(!active)return false;
    if(key==MenuInput::Cancel){if(examining)close_examine(inventory);else if(commands)commands=false;else if(combining){combining=false;selected=combine_from;}else active=false;return true;}
    if(examining){if(key==MenuInput::Left)examine_yaw-=2048;if(key==MenuInput::Right)examine_yaw+=2048;if(key==MenuInput::Up)examine_pitch+=2048;if(key==MenuInput::Down)examine_pitch-=2048;if(key==MenuInput::Confirm)inspect(inventory);return true;}
    if(commands){auto list=actions(inventory);if(list.empty()){commands=false;return true;}if(key==MenuInput::Up)command=(command+list.size()-1)%list.size();if(key==MenuInput::Down)command=(command+1)%list.size();if(key==MenuInput::Confirm){
        const auto item=Inventory::id(inventory.slots()[selected]);
        if(command==0){if(itemdata[item].type&1){inventory.equip(selected);}else if(item==145)open_file(inventory,0,true);
            else if(item>=20&&item<=29){const auto cure=unsigned(curedata[item-20]);if(!inventory.heal(selected))show_message(inventory.infected&&item==23?198:161);else pulse.cured(StatusPulse::condition(inventory.hp,inventory.poisoned),cure,inventory.infected);}
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
    menu_=load("menu",5);fonts_=load("fonts",4);
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
std::string GameUi::item_name(unsigned id)const{return message_text(read_message(messages_,4,id));}
void GameUi::text(SoftwareRenderer& image,const Message& message,int x,int y,float scale,std::uint32_t color,bool dialogue,int selected_choice)const{
    const int start=x;int choice=0;
    std::function<void(const Message&,unsigned)> write=[&](const Message& part,unsigned depth){
      for(std::size_t i=0;i<part.size();++i){const auto c=part[i];if(c==0xffff)break;if(c==0xff00){x=start;y+=int(30*scale);continue;}if(c==0xff01){x+=int(14*scale);continue;}
        // bhDispItemName advances the actual font cursor, including variable
        // glyph widths. Character-count estimates overlap the following '?' .
        if(c==0xff03){if(i+1>=part.size())break;if(depth>=4)throw std::runtime_error("recursive item-name message");write(read_message(messages_,4,part[++i]),depth+1);continue;}
        if(c==0xff04){if(choice++==selected_choice)blit_pixels(image,menu_[0],x,y+int(5*scale),int(18*scale),int(18*scale),0,28,18,46);x+=int(28*scale);continue;}if(c>=0xfe00){if(c<0xff00)color=c==0xfe01?0xffffe020:0xffffffff;continue;}if(c/324>=fonts_.size())continue;
        const auto glyph=c%324;blit(image,(dialogue?dialog_fonts_:fonts_)[c/324],x,y,int(28*scale),int(28*scale),(glyph%18)*14,(glyph/18)*14,(glyph%18)*14+14,(glyph/18)*14+14,color);
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
BoxSlide::BoxSlide(){for(unsigned c=0;c<12;++c)for(unsigned i=0;i<6;++i)pos_[c][i]=i<2?cen_pos99[c][i+2]:0.f;}
void BoxSlide::open(){
    // CenterPositionInit: start at cen_pos99[0..1], delta to the settled [2..3]; BgColorInit zeroes the fade.
    for(unsigned c=0;c<12;++c){for(unsigned i=0;i<6;++i)pos_[c][i]=cen_pos99[c][i];pos_[c][4]=cen_pos99[c][2]-cen_pos99[c][0];pos_[c][5]=cen_pos99[c][3]-cen_pos99[c][1];}
    phase_=Phase::Opening;count_=0;fade_=0;
}
void BoxSlide::cancel(){
    // StatusCancel: every center except 0 and 7 slides to its off-screen target.
    pos_[1][3]=-80;pos_[2][2]=-448;pos_[3][2]=640;pos_[4][2]=808;pos_[5][2]=644;pos_[6][3]=484;pos_[8][2]=-380;pos_[9][2]=644;pos_[10][3]=-384;pos_[11][2]=660;
    for(unsigned c=1;c<12;++c){auto& p=pos_[c];if(c!=3){p[4]=p[2]-p[0];p[5]=p[3]-p[1];}else p[4]=p[2]-352.f;}
    phase_=Phase::Closing;count_=0;
}
BoxSlide::Event BoxSlide::tick(){
    if(phase_==Phase::Settled)return Event::None;
    constexpr float step=0.057142857f;
    // SpriteH: movement happens on every call except the finishing one (cnt==8).
    if(count_!=8)for(auto& p:pos_){p[0]+=p[4]/8.f;p[1]+=p[5]/8.f;}
    if(count_==8){const bool opening=phase_==Phase::Opening;phase_=Phase::Settled;count_=0;return opening?Event::Opened:Event::Closed;}
    ++count_;
    fade_=std::clamp(fade_+(phase_==Phase::Opening?step:-step),0.f,1.f); // BGFadeIn/BGFadeOut
    return Event::None;
}
float BoxSlide::x(unsigned center)const{return pos_[center][0];}
float BoxSlide::y(unsigned center)const{return pos_[center][1];}
void GameUi::draw_box(SoftwareRenderer& image,const Inventory& inv,const StatusMenu& menu,unsigned base,unsigned index,bool selecting,const BoxSlide* slide)const{
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
    if(!selecting){auto p=parts_09b[0];const auto cell=grid_cell(inv,menu.selected);const bool wide=itemdata[Inventory::id(inv.slots()[menu.selected])].type&0x100;
        p.pos[0]=cell>=2?20+int((cell&~unsigned(wide))%2)*68:base==64?88:4;p.pos[1]=cell>=2?4+int((cell/2)-1)*48:-76;
        p.atr|=0x20;sprites.push_back(p);}

    for(unsigned i=0;i<std::size(parts_15b)&&parts_15b[i].anim!=-1&&parts_15b[i].anim!=255;++i){if(base==64&&i>=31)continue;auto p=parts_15b[i];if(i==22){p.anim=0;p.parts_num=10;}if(i==18||i==19)p.anim=std::any_of(inv.items.begin()+base,inv.items.begin()+base+(base==64?128:32),[](auto item){return item!=0;})?17:16;if(i==27||i==28)p.pos[1]=68+int((base==64?1.59375f:6.375f)*index);sprites.push_back(p);}
    for(unsigned i=0;i<std::size(parts_16b)&&parts_16b[i].anim!=-1&&parts_16b[i].anim!=255;++i){auto p=parts_16b[i];if(!i){p.anim=0;p.parts_num=0;}sprites.push_back(p);}group(parts_17b);
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
        if(id){text(image,read_message(messages_,4,inventory_name_id(packed)),44+x0,y,.95f);icon(image,id,324+x0,y,row==5?19:38,42);if((itemdata[id].type&0x3800)&&Inventory::quantity(packed)>1)ascii(image,std::to_string(Inventory::quantity(packed)),375+x0,y+20,.6f,0xffffe020);}
    }
    unsigned cell=0;for(unsigned slot=1;slot<12;++slot){const auto id=Inventory::id(inv.slots()[slot]);const unsigned center=slot==1?(base==64?4:9):5;const int x=(slot==1?(base==64?528:444):460+int(cell&1)*68)+dx(center),y=(slot==1?48:128+int(cell/2)*48)+dy(center);
        if(!selecting&&slot==menu.selected){rectangle(image,x-1,y-1,id&&(itemdata[id].type&0x100)?138:70,50,0xff238699);rectangle(image,x+1,y+1,id&&(itemdata[id].type&0x100)?134:66,46,0xff111f22);}
        icon(image,id,x,y);if(id&&(itemdata[id].type&0x3800)&&Inventory::quantity(inv.slots()[slot])>1)ascii(image,std::to_string(Inventory::quantity(inv.slots()[slot])),x+42,y+27,.6f,0xffffe020);
        if(slot>1)cell+=id&&(itemdata[id].type&0x100)?2:1;if(cell>=inv.capacity())break;
    }
    const auto packed=selecting?inv.items[base+index]:inv.slots()[menu.selected];if(menu.message.active())text(image,menu.message.page(),44+dx(6),342+dy(6),1,0xffffffff,true,int(menu.message.selection));else if(Inventory::id(packed))text(image,read_message(messages_,4,inventory_name_id(packed)),44+dx(6),342+dy(6),.95f);
}
void GameUi::draw(SoftwareRenderer& image,const Inventory& inv,const StatusMenu& menu,const MessageDialog& dialog,unsigned tick){
    if(menu.active){
        rectangle(image,0,0,640,480,0xff101010);
        struct Sprite {PARTS p;};std::vector<Sprite> sprites;
        auto group=[&](const auto& table){for(const auto& p:table){if(p.anim==-1||p.anim==255)break;sprites.push_back({p});}};
        // Settled StatusInit layout. Conditional command/examine layers are added separately.
        group(parts_00b);group(parts_01b);group(parts_02b);group(parts_03b);
        // Original SidePackSet layout: the fifth row exists only with the bag.
        for(unsigned i=0;parts_04b[i].anim!=255&&parts_04b[i].anim!=-1;++i){auto p=parts_04b[i];if(!inv.side_pack&&(i==6||i==8||i==9||i==13||i==14))p.pos[1]-=48;sprites.push_back({p});}
        group(parts_05b);group(parts_07b);group(parts_11b);
        for(unsigned i=0;i<11;++i){auto p=parts_10b[i];p.anim=i<5?int(inv.player)+int(i)*4:i==10?int(menu.pulse_open()?menu.pulse.condition():StatusPulse::condition(inv.hp,inv.poisoned)):p.anim;sprites.push_back({p});}
        std::stable_sort(sprites.begin(),sprites.end(),[](const Sprite& a,const Sprite& b){return a.p.pos[2]>b.p.pos[2];});
        for(auto s:sprites){auto& p=s.p;if(p.parts_num>=23||!tables[p.parts_num]||p.cen_no>=12)continue;const auto& t=tables[p.parts_num][p.anim];if(t.texid<0||unsigned(t.texid)>=menu_.size())continue;
            auto x=int(p.pos[0])+centers[p.cen_no][0],y=int(p.pos[1])+centers[p.cen_no][1];
            const int nx=(p.setnum>>4)+1,ny=(p.setnum&15)+1;const int reps=std::max(nx,ny);
            for(int i=0;i<reps;++i)blit(image,menu_[t.texid],x+(i<nx?int(i*p.move):int((nx-1)*p.move)),y+(i<ny?int(i*p.move):int((ny-1)*p.move)),t.sx,t.sy,t.u1,t.v1,t.u2,t.v2,tint(p.col),bool(p.atr&8),p.ang);
        }
        // ItemSet, personal slot + the two-column normal inventory; large weapons span a row.
        unsigned cell=0;for(unsigned slot=1;slot<12;++slot){const auto id=Inventory::id(inv.slots()[slot]);const int x=slot==1?528:460+int(cell&1)*68,y=slot==1?48:128+int(cell/2)*48;
            if(menu.combining&&slot==menu.combine_from){rectangle(image,x-1,y-1,(id&&(itemdata[id].type&0x100))?138:70,50,0xffd9ab47);rectangle(image,x+1,y+1,(id&&(itemdata[id].type&0x100))?134:66,46,0xff111f22);}
            if(slot==menu.selected){rectangle(image,x-1,y-1,(id&&(itemdata[id].type&0x100))?138:70,50,0xff238699);rectangle(image,x+1,y+1,(id&&(itemdata[id].type&0x100))?134:66,46,0xff111f22);}
            icon(image,id,x,y);if(id){if((itemdata[id].type&0x3800)&&Inventory::quantity(inv.slots()[slot])>1)ascii(image,std::to_string(Inventory::quantity(inv.slots()[slot])),x+42,y+27,.6f,0xffffe020);if(inv.slots()[0]==slot)ascii(image,"E",x+2,y+2,.6f,0xff50d090);}
            if(slot>1)cell+=id&&(itemdata[id].type&0x100)?2:1;if(cell>=inv.capacity())break;
        }
        if(inv.slots()[0])icon(image,Inventory::id(inv.slots()[inv.slots()[0]]),376,48);
        // Quads captured from the original Pulse00/PulseHealAnim/PulsePoisonHealAnim.
        for(const auto& quad:menu.pulse.quads())pulse_quad(image,menu_[2],quad);
        const auto selected_id=menu.pickup?menu.pickup:Inventory::id(inv.slots()[menu.selected]);if(selected_id&&!menu.message.active()&&!(menu.pickup&&dialog.active()))text(image,read_message(messages_,4,menu.pickup?menu.pickup:inventory_name_id(inv.slots()[menu.selected])),44,342,.95f);
        if(menu.commands||menu.erase_frames){
            // cen_pos[11] slides 24 px per frame; 96 px is the fully opened window.
            const bool erasing=!menu.commands;const int slide=(4-int(erasing?menu.erase_frames:menu.command_frames))*24;
            const unsigned command_item=erasing?menu.erase_item:selected_id;const auto list=erasing?menu.erase_actions:menu.actions(inv);const unsigned cursor=erasing?menu.erase_command:menu.command;
            for(unsigned n=0;n<13;++n){auto p=parts_06b[n];const bool weapon=itemdata[command_item].type&1;const int shrink=command_item==10?0:weapon?32:64;
            if(n==2||n==3||n==5||n==7)p.pos[1]-=shrink;if(n==8)p.setnum=command_item==10?3:weapon?2:1;if(n==9||n==10)p.setnum=command_item==10?4:weapon?3:2;const auto& part=tables[p.parts_num][p.anim];const int nx=(p.setnum>>4)+1,ny=(p.setnum&15)+1;
            for(int i=0;i<std::max(nx,ny);++i)blit(image,menu_[part.texid],int(p.pos[0])+centers[p.cen_no][0]-96+(p.cen_no==11?slide:0)+int(std::min(i,nx-1)*p.move),int(p.pos[1])+centers[p.cen_no][1]+int(std::min(i,ny-1)*p.move),part.sx,part.sy,part.u1,part.v1,part.u2,part.v2,tint(p.col),bool(p.atr&8),p.ang);
        }for(unsigned i=0;i<list.size();++i){if(list[i]=="File")ascii(image,"File",364+slide,146,.8f);else{const auto& label=parts_10[i==0?(list[0]=="Equip"?7:8):i==1?9:i==2?10:i==4?(list[i]=="Single"?11:12):8];blit(image,menu_[label.texid],366+slide,146+int(i)*32,label.sx,label.sy,label.u1,label.v1,label.u2,label.v2,i==cursor?0xff2080ff:0xffffffff);}}}
        if(menu.combining)ascii(image,"Combine",44,400,.8f,0xff50c0ff);
        if((menu.examining||menu.pickup)&&selected_id&&selected_id<145){
            const auto source=menu.pickup?menu.pickup:menu.inspection.running()?menu.inspection.item():selected_id;
            const auto id=inspection_model_id(source);
            if(!item_packets_.count(id)){auto file=mods_/"items"/(asset_number(id)+".bin");if(!std::filesystem::is_regular_file(file))file=data_/"archives/ITEM1"/(asset_number(id-1)+".bin");item_packets_[id]=read_asset_file(file,8*1024*1024);}
            if(!item_models_.count(source)){auto object=decode_item_asset(item_packets_[id],inspection_geometry(item_packets_[id],source));apply_texture_files(object,data_,mods_,std::filesystem::path("items")/asset_number(id));item_models_[source]=std::move(object);}
            auto object=menu.inspection.running()?decode_item_asset(item_packets_[id],inspection_geometry(item_packets_[id],source,source==menu.inspection.source()?menu.opening_frame:0)):item_models_[source];
            if(menu.inspection.running())object.textures=item_models_[source].textures;
            NJS_POINT3 lo{1e20f,1e20f,1e20f},hi{-1e20f,-1e20f,-1e20f};
            for(const auto& triangle:object.triangles)for(const auto& vertex:triangle.vertices){lo.x=std::min(lo.x,vertex.position.x);lo.y=std::min(lo.y,vertex.position.y);lo.z=std::min(lo.z,vertex.position.z);hi.x=std::max(hi.x,vertex.position.x);hi.y=std::max(hi.y,vertex.position.y);hi.z=std::max(hi.z,vertex.position.z);}
            const NJS_POINT3 center{(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};const float radius=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,0.01f});
            NJS_MATRIX rotation;const auto spin=menu.inspection.running()?menu.inspection.spin():std::array<Angle,3>{};inspection_rotation(rotation,{menu.examine_pitch,menu.examine_yaw,menu.examine_roll},spin);
            // DrawSubItem uses an identity camera-space basis. Applying the
            // room camera's (1,-1,-1) basis flips the display and views its back.
            NJS_MATRIX view;njUnitMatrix(&view);njTranslate(&view,0,0,radius*(2.8f+20.f*(1.f-menu.inspection.zoom())));njMultiMatrix(&view,&rotation);njTranslate(&view,-center.x,-center.y,-center.z);
            object.background=0xff101010;SoftwareRenderer item_view(352,196);item_view.draw_view(object,view,500.f*1.174f,500.f);
            for(int y=0;y<196;++y)std::copy_n(item_view.pixels().begin()+y*352,352,image.canvas().begin()+(y+132)*640+60);
        }
        if(menu.files){
            rectangle(image,28,120,572,272,0xff101010);
            if(menu.file_list){unsigned row=0;for(unsigned file=0;file<24;++file)if(inv.items[383]&(1u<<file)){text(image,read_message(messages_,4,159+file),44+int(row/8)*186,128+int(row%8)*30,.65f,file==menu.file_number?0xff50c0ff:0xffffffff);++row;}}
            else{
                if(!file_textures_.count(menu.file_number)){const auto packet=read_asset_file(data_/"archives/ITEM1"/(asset_number(original_file_wallpaper(menu.file_number))+".bin"),8*1024*1024);file_textures_[menu.file_number]=decode_texture_bundle(packet);}
                const auto& textures=file_textures_[menu.file_number];if(!textures.empty())blit_pixels(image,textures[0],40,124,552,264,0,0,textures[0].width,textures[0].height);
                text(image,system_message(original_file_message(menu.file_number,menu.file_page)),52,136,.72f,0xffffffff,true);
                ascii(image,std::to_string(menu.file_page+1)+" / "+std::to_string(original_file_pages(menu.file_number)),526,366,.6f);
            }
        }
        (void)tick;
    }
    const auto& visible_dialog=menu.active&&menu.message.active()?menu.message:dialog;
    // bhControlMessage mode 1 uses mes_spos[1], replacing the item name.
    // Room dialogue uses mode 0; neither mode draws a background panel.
    if(visible_dialog.active()&&std::any_of(visible_dialog.page().begin(),visible_dialog.page().end(),[](auto code){return code<0xfe00||code==0xff03;}))text(image,visible_dialog.page(),menu.active?44:68,menu.active?342:374,1,0xffffffff,true,int(visible_dialog.selection));
}
}
