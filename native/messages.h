#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace recvx {
using Message=std::vector<std::uint16_t>;
Message read_message(const std::vector<std::uint8_t>& data,std::size_t table,unsigned index);
std::string message_text(const Message& message);
class MessageDialog {
public:
    void show(Message text,unsigned index);
    bool active()const{return active_;}
    unsigned index()const{return index_;}
    const Message& page()const{return page_;}
    void confirm();
    bool tick();
    unsigned selection=0;
private:
    bool active_=false;
    unsigned index_=0,auto_ticks_=0;
    std::size_t position_=0;
    Message text_,page_;
    void next_page();
};
}
