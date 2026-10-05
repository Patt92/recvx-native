#include "ui.h"
#include "asset_io.h"
#include "binary_view.h"
#include "item_asset.h"
#include <algorithm>
#include <cmath>
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
unsigned last_slot(const Inventory& inv){unsigned cells=0,last=1;for(unsigned slot=2;slot<12&&cells<inv.capacity();++slot){const auto id=Inventory::id(inv.slots()[slot]);cells+=id&&(itemdata[id].type&0x100)?2:1;last=slot;}return last;}
}
struct GameUi::PulseState {pulse::PULSE_WORK work{};unsigned tick=0,condition=99;};
std::vector<std::string> StatusMenu::actions(const Inventory& inventory)const{
    auto item=Inventory::id(inventory.slots()[selected]);if(!item)return {};if(item==145)return {"File"};if(itemdata[item].type&1)return {"Equip","Examine","Combine","Use"};return {"Use","Examine","Combine"};
}
void StatusMenu::show_message(unsigned index){if(!message_source)throw std::runtime_error("inventory message source is not bound");message.show(message_source(index),index);}
void StatusMenu::close_examine(Inventory& inventory){
    if(Inventory::id(inventory.slots()[selected])==30)inventory.transform(selected,30,13);
    examining=false;pending_item_=pending_group_=pending_slot_=0;
}
void StatusMenu::inspect(Inventory& inventory){
    const auto item=Inventory::id(inventory.slots()[selected]);if(!item||item>=156)return;
    const auto group=unsigned(dsptbl[item].hide);if(group>=41)throw std::runtime_error("invalid item inspection group");
    const auto directions=itemflg[group][4];const unsigned first=directions&15,count=directions>>4;
    unsigned match=count;const Angle angles[]={examine_pitch,examine_yaw,examine_roll};
    for(unsigned i=0;i<count;++i){if(first+i>=14)throw std::runtime_error("invalid item inspection direction");bool valid=true;for(unsigned a=0;a<3;++a){const auto tolerance=c_a[first+i][a*2],target=c_a[first+i][a*2+1];if(tolerance&&unsigned((angles[a]+tolerance-target)&65535)>unsigned(tolerance*2))valid=false;}if(valid){match=i;break;}}
    if(match==count||item==19||item==33||item==34||item==136||item==142){show_message(item==82&&!Inventory::quantity(inventory.slots()[selected])?140:item);return;}
    const unsigned code=mestbl[itemflg[group][5]+match],index=code&0xfff;
    if(index==181&&!inventory.contains(50)){show_message(188);return;}
    if((code&0xf000)==0x1000&&idsettbl[itemflg[group][6]]){pending_item_=item;pending_group_=itemflg[group][6];pending_slot_=selected;}
    // Other examination text is still useful without claiming unported file or
    // multi-item extraction animations as completed inventory operations.
    show_message(index);
    if(index==72)inventory.slots()[selected]|=0x80000000;
}
void StatusMenu::tick(Inventory& inventory){
    message.tick();if(!opening_item)return;
    if(++opening_frame<23)return;
    const auto result=idsettbl[pending_group_];const auto description=getmestbl[pending_group_];
    if(!inventory.transform(pending_slot_,pending_item_,result)){feedback_sound=1;opening_item=0;pending_item_=pending_group_=pending_slot_=0;return;}
    selected=pending_slot_;opening_item=0;pending_item_=pending_group_=pending_slot_=0;examine_yaw=examine_pitch=examine_roll=0;show_message(description);
}
bool StatusMenu::input(MenuInput key,Inventory& inventory){
    feedback_sound=0xffffffff;
    if(active&&message.active()){
        if(key==MenuInput::Open)return true;
        if(key==MenuInput::Cancel){message=MessageDialog{};pending_item_=pending_group_=pending_slot_=0;return true;}
        if(message.choices()&&(key==MenuInput::Left||key==MenuInput::Up||key==MenuInput::Right||key==MenuInput::Down)){const int step=key==MenuInput::Left||key==MenuInput::Up?-1:1;message.selection=(message.selection+message.choices()+step)%message.choices();return true;}
        if(key==MenuInput::Confirm){const bool choice=message.choices(),yes=message.selection==0;message.confirm();if(!message.active()&&pending_item_){if(choice&&yes){opening_item=pending_item_;opening_frame=0;}else pending_item_=pending_group_=pending_slot_=0;}}return true;
    }
    if(opening_item)return true;
    if(key==MenuInput::Open){if(examining)close_examine(inventory);active=!active;commands=examining=combining=false;selected=1;command=0;return true;}if(!active)return false;
    if(key==MenuInput::Cancel){if(examining)close_examine(inventory);else if(commands)commands=false;else if(combining){combining=false;selected=combine_from;}else active=false;return true;}
    if(examining){if(key==MenuInput::Left)examine_yaw-=2048;if(key==MenuInput::Right)examine_yaw+=2048;if(key==MenuInput::Up)examine_pitch+=2048;if(key==MenuInput::Down)examine_pitch-=2048;if(key==MenuInput::Confirm)inspect(inventory);return true;}
    if(commands){auto list=actions(inventory);if(list.empty()){commands=false;return true;}if(key==MenuInput::Up)command=(command+list.size()-1)%list.size();if(key==MenuInput::Down)command=(command+1)%list.size();if(key==MenuInput::Confirm){
        if(command==0){const auto item=Inventory::id(inventory.slots()[selected]);if(itemdata[item].type&1){inventory.equip(selected);active=false;}else if(item==145)examining=true;
            else if(item>=20&&item<=29){if(!inventory.heal(selected)){show_message(161);feedback_sound=1;}}
            else{use_item=item;active=false;}}
        else if(command==1){examining=true;examine_yaw=examine_pitch=examine_roll=0;}else if(command==2){combining=true;combine_from=selected;}else if(command==3){use_item=Inventory::id(inventory.slots()[selected]);active=false;}
        commands=false;
    }return true;}
    const auto last=last_slot(inventory);selected=std::min(selected,last);
    if(key==MenuInput::Up){if(selected<=3)selected=1;else selected-=2;}if(key==MenuInput::Down){selected=selected==1?2:std::min(last,selected+2);}
    if(key==MenuInput::Left&&selected>1)--selected;if(key==MenuInput::Right)selected=std::min(last,selected+1);
    if(key==MenuInput::Confirm){if(combining){unsigned index=0,destination=0;if(inventory.combine(selected,combine_from,&index,&destination)){combining=false;selected=destination;}else{feedback_sound=1;if(index)show_message(index);}}else if(Inventory::id(inventory.slots()[selected])){commands=true;command=0;}}
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
    const int start=x;int choice=0;for(std::size_t i=0;i<message.size();++i){const auto c=message[i];if(c==0xffff)break;if(c==0xff00){x=start;y+=int(30*scale);continue;}if(c==0xff01){x+=int(14*scale);continue;}
        if(c==0xff03){if(i+1>=message.size())break;const auto id=message[++i];auto name=read_message(messages_,4,id);text(image,name,x,y,scale,color,dialogue);x+=int(message_text(name).size()*14*scale);continue;}
        if(c==0xff04){if(choice++==selected_choice)blit_pixels(image,menu_[0],x,y+int(5*scale),int(18*scale),int(18*scale),0,28,18,46);x+=int(28*scale);continue;}if(c>=0xfe00){if(c<0xff00)color=c==0xfe01?0xffffe020:0xffffffff;continue;}if(c/324>=fonts_.size())continue;
        const auto glyph=c%324;blit(image,(dialogue?dialog_fonts_:fonts_)[c/324],x,y,int(28*scale),int(28*scale),(glyph%18)*14,(glyph/18)*14,(glyph%18)*14+14,(glyph/18)*14+14,color);
        x+=int((c<108?FontSz[c]:28)*scale);
    }
}
void GameUi::ascii(SoftwareRenderer& image,const std::string& str,int x,int y,float scale,std::uint32_t color)const{Message m;for(unsigned char c:str)m.push_back(c=='\n'?0xff00:c==' '?0xff01:c>=32&&c<140?c-32:31);m.push_back(0xffff);text(image,m,x,y,scale,color);}
void GameUi::icon(SoftwareRenderer& image,unsigned id,int x,int y)const{
    unsigned n=id;switch(id){case 30:n=13;break;case 63:case 128:case 129:n=41;break;case 130:n=35;break;case 136:n=47;break;case 138:n=48;break;case 142:n=128;break;case 143:n=49;break;case 144:n=102;break;case 145:n=133;break;}
    const auto item=n;if(item>63)n-=64;if(item<64){if(n&&n<4)n=n*2-1;else if(n<33){if(n)n+=4;}else if(n==33||n==34)n+=n-33+4;else n+=6;}
    const int width=(itemdata[id].type&0x100)?136:68,u=int(n%7)*34,v=int(n/7)*24;
    blit(image,menu_[item<64?4:3],x,y,width,48,u,v,u+width/2,v+24);
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
        for(unsigned i=0;i<11;++i){auto p=parts_10b[i];p.anim=i<5?int(inv.player)+int(i)*4:i==10?int(inv.poisoned?4:inv.hp>=120?0:inv.hp>=60?1:inv.hp>=30?2:3):p.anim;sprites.push_back({p});}
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
        // Pulse atlas and Fine/Caution/Danger/Poison labels from the original texture.
        unsigned condition=inv.poisoned?4:inv.hp>=120?0:inv.hp>=60?1:inv.hp>=30?2:3;

        const int u=condition>=3?216:0,v=condition==0?296:(condition==2||condition==4?440:368);
        if(!pulse_)pulse_=std::make_shared<PulseState>();pulse::swork.hp=condition;
        if(pulse_->condition!=condition||tick<pulse_->tick){pulse::PulseInit();pulse_->work=pulse::pulsework;pulse_->condition=condition;pulse_->tick=tick-std::min(120u,tick);}
        pulse::pulsework=pulse_->work;
        for(unsigned i=0;i<std::min(120u,tick-pulse_->tick);++i)pulse::PulseAnim();
        pulse_->tick=tick;pulse_->work=pulse::pulsework;
        // Pulse00's five strips: interpolate the original vertex alpha and additive colour.
        for(unsigned strip=0;strip<5;++strip){const auto& pw=pulse_->work;const int begin=int(pw.pos_00[strip].x),end=int(pw.pos_01[strip].x);
            for(int x=begin;x<end;++x){const float fraction=float(x-begin)/std::max(1,end-begin);std::uint32_t color=0;
                for(unsigned shift:{0u,8u,16u,24u}){const auto left=(pw.col00[strip].color>>shift)&255,right=(pw.col01[strip].color>>shift)&255;unsigned component=unsigned(left+(int(right)-int(left))*fraction);
                    if(shift!=24){const auto add_left=(pw.ocol00[strip]>>shift)&255,add_right=(pw.ocol01[strip]>>shift)&255;component=std::min(255u,component+unsigned(add_left+(int(add_right)-int(add_left))*fraction));}color|=component<<shift;}
                blit_pixels(image,menu_[2],176+x,234,1,72,u+x,v,u+x+1,v+72,color);
            }
        }
        const auto selected_id=Inventory::id(inv.slots()[menu.selected]);if(selected_id)text(image,read_message(messages_,4,selected_id==145?159:selected_id),44,342,.95f);
        if(menu.commands){for(unsigned n=0;n<13;++n){auto p=parts_06b[n];const bool weapon=itemdata[selected_id].type&1;const int shrink=weapon?32:64;
            if(n==2||n==3||n==5||n==7)p.pos[1]-=shrink;if(n==8)p.setnum=weapon?2:1;if(n==9||n==10)p.setnum=weapon?3:2;const auto& part=tables[p.parts_num][p.anim];const int nx=(p.setnum>>4)+1,ny=(p.setnum&15)+1;
            for(int i=0;i<std::max(nx,ny);++i)blit(image,menu_[part.texid],int(p.pos[0])+centers[p.cen_no][0]-96+int(std::min(i,nx-1)*p.move),int(p.pos[1])+centers[p.cen_no][1]+int(std::min(i,ny-1)*p.move),part.sx,part.sy,part.u1,part.v1,part.u2,part.v2,tint(p.col),bool(p.atr&8),p.ang);
        }auto list=menu.actions(inv);for(unsigned i=0;i<list.size();++i){if(list[i]=="File")ascii(image,"File",364,146,.8f);else{const auto& label=parts_10[i==0?(list[0]=="Equip"?7:8):i==1?9:i==2?10:8];blit(image,menu_[label.texid],366,146+int(i)*32,label.sx,label.sy,label.u1,label.v1,label.u2,label.v2,i==menu.command?0xff2080ff:0xffffffff);}}}
        if(menu.combining)ascii(image,"Combine",44,400,.8f,0xff50c0ff);
        if(menu.examining&&selected_id&&selected_id<145){
            const auto id=menu.opening_item?menu.opening_item:selected_id==131?5:selected_id==136?33:selected_id;
            if(!item_models_.count(id)){auto file=mods_/"items"/(asset_number(id)+".bin");if(!std::filesystem::is_regular_file(file))file=data_/"archives/ITEM1"/(asset_number(id-1)+".bin");item_packets_[id]=read_asset_file(file,8*1024*1024);auto object=decode_item_asset(item_packets_[id],0,id);apply_texture_files(object,data_,mods_,std::filesystem::path("items")/asset_number(id));item_models_[id]=std::move(object);}
            auto object=menu.opening_item?decode_item_asset(item_packets_[id],menu.opening_frame,id):item_models_[id];if(menu.opening_item)object.textures=item_models_[id].textures;NJS_POINT3 lo{1e20f,1e20f,1e20f},hi{-1e20f,-1e20f,-1e20f};
            for(const auto& triangle:object.triangles)for(const auto& vertex:triangle.vertices){lo.x=std::min(lo.x,vertex.position.x);lo.y=std::min(lo.y,vertex.position.y);lo.z=std::min(lo.z,vertex.position.z);hi.x=std::max(hi.x,vertex.position.x);hi.y=std::max(hi.y,vertex.position.y);hi.z=std::max(hi.z,vertex.position.z);}
            const NJS_POINT3 center{(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};const float radius=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,0.01f});
            NJS_MATRIX rotation;njUnitMatrix(&rotation);njRotateXYZ(&rotation,menu.examine_pitch,menu.examine_yaw,menu.examine_roll);for(auto& triangle:object.triangles)for(auto& vertex:triangle.vertices){NJS_POINT3 local{vertex.position.x-center.x,vertex.position.y-center.y,vertex.position.z-center.z};njCalcPoint(&rotation,&local,&local);vertex.position={center.x+local.x,center.y+local.y,center.z+local.z};njCalcVector(&rotation,&vertex.normal,&vertex.normal);}
            object.background=0xff101010;SceneCamera camera;camera.position={center.x,center.y,center.z+radius*2.8f};SoftwareRenderer item_view(352,196);item_view.draw(object,camera);
            for(int y=0;y<196;++y)std::copy_n(item_view.pixels().begin()+y*352,352,image.canvas().begin()+(y+132)*640+60);
        }
        (void)tick;
    }
    const auto& visible_dialog=menu.active&&menu.message.active()?menu.message:dialog;
    if(visible_dialog.active()&&std::any_of(visible_dialog.page().begin(),visible_dialog.page().end(),[](auto code){return code<0xfe00||code==0xff03;}))text(image,visible_dialog.page(),68,374,1,0xffffffff,true,int(visible_dialog.selection));
}
}
