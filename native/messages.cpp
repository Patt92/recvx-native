#include "messages.h"
#include "binary_view.h"
#include <algorithm>
#include <memory>
namespace recvx {
Message read_message(const std::vector<std::uint8_t>& data,std::size_t table,unsigned index){
    BinaryView v(data);const auto count=v.u32(table);if(count>4096||index>=count)throw std::runtime_error("message index outside table");v.require(table,4+count*4);
    auto p=v.relative(table,v.u32(table+4+index*4),2);Message out;
    for(unsigned i=0;i<16384;++i,p+=2){const auto code=v.u16(p);out.push_back(code);if(code==0xffff)return out;if(code==0xff03||code==0xff02){p+=2;out.push_back(v.u16(p));++i;}}
    throw std::runtime_error("unterminated message");
}
std::string message_text(const Message& text){std::string out;for(std::size_t i=0;i<text.size();++i){const auto c=text[i];if(c==0xffff)break;if(c==0xff00)out+='\n';else if(c==0xff01)out+=' ';else if(c==0xff02){++i;out+='\n';}else if(c==0xff03)++i;else if(c<108)out+=char(c+32);}return out;}
namespace {std::shared_ptr<const std::vector<std::uint8_t>> item_names;}
void bind_item_names(std::vector<std::uint8_t> system_messages){item_names=std::make_shared<const std::vector<std::uint8_t>>(std::move(system_messages));}
Message item_name_message(unsigned id){if(!item_names)throw std::runtime_error("item-name table (sys->mes_ip) is not bound");return read_message(*item_names,4,id);}

void MessageDialog::show(Message text,unsigned index){
    // bhSetMessage: mes_ct, mes_tim, mes_fls and mes_sel restart; st_flg 0x200 on,
    // 0x80000|0x8000|0x4000|0x1000|0x800|0x400 off.
    text_=std::move(text);index_=index;dp_=end_=0;ct_=0;tim_=frame_tim_=0;st_=0;seln_=0;selection=0;
    active_=true;moved_=false;ok_=false;move_=0;visible_.clear();build_page();
}
void MessageDialog::build_page(){
    page_.clear();
    for(std::size_t p=dp_;p<text_.size();){const auto c=text_[p++];
        if(c==0xffff){page_.push_back(c);return;}
        if(c==0xff02){if(p>=text_.size())throw std::runtime_error("truncated message wait");if(!text_[p++]){page_.push_back(0xffff);return;}continue;}
        if(c==0xfe08)return;
        page_.push_back(c);
        if(c==0xff03){if(p>=text_.size())throw std::runtime_error("truncated item-name message");page_.push_back(text_[p++]);}
    }
    throw std::runtime_error("message terminator missing");
}
unsigned MessageDialog::choices()const{return unsigned(std::count(page_.begin(),page_.end(),0xff04));}
void MessageDialog::parse(){
    // message.c:74-226: codes from mes_dp while mct <= mes_ct; layout and colour
    // codes cost nothing, glyphs one, an item name as many as bhDispItemName draws.
    visible_.clear();seln_=0;std::int64_t mct=0;bool end=false;const auto start=dp_;std::size_t p=dp_;
    auto next=[&]{if(p>=text_.size())throw std::runtime_error("message terminator missing");return text_[p++];};
    while(mct<=ct_&&!end){const auto cd=next();
        switch(cd){
        case 0xffff:if(!(st_&0x800u))st_|=0x1000u;st_|=0x4000u;end=true;break;
        case 0xff00:case 0xff01:visible_.push_back(cd);break;
        case 0xff02:{const auto wait=next();
            if(!tim_&&!(st_&0x4000u)&&!(st_&0x800u)){tim_=wait;if(!tim_){st_|=0x1000u;end=true;}else st_|=0x800u;}
            break;}
        case 0xff03:{auto id=next();if(id==0xffff)throw std::runtime_error("item name without sys->sb_id");
            // bhDispItemName (message.c:512): count = mes_ct - mct + 1 glyphs; running
            // out before the name ends (or exactly at its end) returns 0 and ends the pass.
            const auto name=item_name_message(id);std::int64_t count=ct_-mct+1;unsigned drawn=0;bool cut=false;
            for(const auto c:name){if(c==0xffff)break;visible_.push_back(c);++drawn;if(--count<=0){cut=true;break;}}
            if(cut)end=true;else mct+=drawn;
            break;}
        case 0xff04:if(!(st_&0x8000u)){st_|=0x8000u;ct_+=65536;}visible_.push_back(cd);++seln_;break;
        case 0xfe00:if(mct>=ct_){ct_+=65536;st_|=0x400u;}break;
        case 0xfe01:case 0xfe02:case 0xfe03:case 0xfe04:case 0xfe05:visible_.push_back(cd);break;
        case 0xfe06:case 0xfe07:break;
        case 0xfe08:if(!tim_){dp_=p;st_&=~0x800u;}end=true;break;
        case 0xfe09:if(!(st_&0x80000u)){st_|=0x80000u;selection=int(seln_);}break;
        case 0xfe0a:case 0xfe0b:case 0xfe0c:case 0xfe0d:case 0xfe0e:case 0xfe0f:break;
        default:visible_.push_back(cd);++mct;break;
        }
    }
    end_=p;if(dp_!=start)build_page();
}
MessageDialog::Step MessageDialog::pad(bool ok,int direction){
    if(st_&0x8000u){
        // message.c:231-293: OK answers (the owner adds 100 on the status screen and
        // raises the response bits); otherwise 0x4000 falls and pad_ps 0x4/0x8 move.
        if(ok){st_&=~0x8000u;st_|=0x4000u;return Step::Answered;} /* message.c:245 st_flg |= 0x4000 */
        st_&=~0x4000u;
        if(direction){selection+=direction;moved_=true;}
        if(selection>=int(seln_))selection=0;
        if(selection<0)selection=int(seln_)-1;
        return Step::None;
    }
    // message.c:325-337: OK turns an intermediate page (FF02 0) from where the pass stopped.
    if(!(st_&0x4000u)&&(st_&0x1000u)&&ok){dp_=end_;ct_=0;tim_=0;selection=0;st_&=~0x1000u;visible_.clear();build_page();return Step::Turned;}
    return Step::None;
}
MessageDialog::Step MessageDialog::update(){
    moved_=false;Step step=Step::None;
    if(active_){
        parse();frame_tim_=tim_;
        if(tim_)--tim_;
        else if(st_&0x8000u)step=pad(ok_,move_);
        else if((st_&0x4000u)&&!(st_&0x1000u)){active_=false;step=Step::Closed;} // message.c:310
        else if(!(st_&0x4000u)&&(st_&0x1000u))step=pad(ok_,0);
        else if(held_&&!(st_&0x400u))ct_+=4; // pad_on 0x800 without FE00
        else ++ct_;
    }
    ok_=false;move_=0;return step;
}
MessageDialog::Step MessageDialog::press(bool ok,int direction){
    moved_=false;if(!active_||frame_tim_)return Step::None;
    parse();
    if((st_&0x8000u)||(!(st_&0x4000u)&&(st_&0x1000u)))return pad(ok,direction);
    return Step::None;
}
}
