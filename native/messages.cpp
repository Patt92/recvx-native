#include "messages.h"
#include "binary_view.h"
namespace recvx {
Message read_message(const std::vector<std::uint8_t>& data,std::size_t table,unsigned index){
    BinaryView v(data);const auto count=v.u32(table);if(count>4096||index>=count)throw std::runtime_error("message index outside table");v.require(table,4+count*4);
    auto p=v.relative(table,v.u32(table+4+index*4),2);Message out;
    for(unsigned i=0;i<16384;++i,p+=2){const auto code=v.u16(p);out.push_back(code);if(code==0xffff)return out;}
    throw std::runtime_error("unterminated message");
}
std::string message_text(const Message& text){std::string out;for(std::size_t i=0;i<text.size();++i){const auto c=text[i];if(c==0xffff)break;if(c==0xff00)out+='\n';else if(c==0xff01)out+=' ';else if(c==0xff02){++i;out+='\n';}else if(c==0xff03||c==0xff04)++i;else if(c<108)out+=char(c+32);}return out;}
void MessageDialog::show(Message text,unsigned index){text_=std::move(text);index_=index;position_=0;auto_ticks_=0;selection=0;active_=true;next_page();}
void MessageDialog::next_page(){page_.clear();while(position_<text_.size()){const auto c=text_[position_++];if(c==0xffff){page_.push_back(c);if(page_.size()==1&&!auto_ticks_)auto_ticks_=1;return;}if(c==0xff02){if(position_==text_.size())throw std::runtime_error("truncated message wait");const auto delay=text_[position_++];if(delay){auto_ticks_=delay;continue;}page_.push_back(0xffff);return;}page_.push_back(c);}throw std::runtime_error("message terminator missing");}
bool MessageDialog::tick(){if(active_&&auto_ticks_&&!--auto_ticks_){active_=false;return true;}return false;}
void MessageDialog::confirm(){if(!active_)return;if(position_>=text_.size()||text_[position_-1]==0xffff)active_=false;else next_page();}
}
