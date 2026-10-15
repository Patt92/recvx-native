#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace recvx {
using Message=std::vector<std::uint16_t>;
Message read_message(const std::vector<std::uint8_t>& data,std::size_t table,unsigned index);
std::string message_text(const Message& message);
// sys->mes_ip: the system item-name table bhDispItemName reads (SYSMES.ALD, table 4).
void bind_item_names(std::vector<std::uint8_t> system_messages);
Message item_name_message(unsigned id);
// message.c bhSetMessage/bhControlMessage, one update per game pass.
class MessageDialog {
public:
    enum class Step {None,Closed,Answered,Turned};
    void show(Message text,unsigned index); // bhSetMessage
    bool active()const{return active_;}
    unsigned index()const{return index_;}
    const Message& page()const{return page_;}       // the whole current page
    const Message& visible()const{return visible_;} // what the last pass drew (mes_ct)
    unsigned choices()const;                        // FF04 fields of the current page
    bool choosing()const{return active_&&(st_&0x8000u);}
    bool timer_running()const{return tim_!=0;}
    // st_flg 0x4000|0x1000 after the last pass: the final page may be closed with
    // OK/Cancel (game.c:264, sub1.c:8418). False while a choice is open (message.c:265).
    bool closable()const{return active_&&(st_&0x4000u)&&(st_&0x1000u);}
    // st_flg 0x1000 ohne 0x4000: eine aufgebaute Zwischenseite, die OK umblaettert (message.c:325).
    bool turnable()const{return active_&&!(st_&0x8000u)&&!(st_&0x4000u)&&(st_&0x1000u);}
    void close(){active_=false;}
    // pad_on 0x800 dieses Durchlaufs; vorgemerkte Tasten verbraucht erst update().
    void begin_frame(bool ok_held){held_=ok_held;}
    void queue_ok(){ok_=true;}
    void queue_move(int direction){move_+=direction;}
    bool moved()const{return moved_;}
    // The whole bhControlMessage pass with the queued presses.
    Step update();
    // The pad part of the pass for an owner whose update already ran this frame
    // (the status screen): no step, no timer, no automatic close.
    Step press(bool ok,int direction);
    int selection=0;
private:
    bool active_=false,ok_=false,held_=false,moved_=false;
    int move_=0;
    unsigned index_=0,st_=0,tim_=0,frame_tim_=0,seln_=0;
    std::int64_t ct_=0;
    std::size_t dp_=0,end_=0;
    Message text_,page_,visible_;
    void parse();
    void build_page();
    Step pad(bool ok,int direction);
};
}
