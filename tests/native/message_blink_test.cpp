// bhControlMessage (message.c:229-347): Auswahlcursor (Schriftzeichen 28 an mes_sps[mes_sel]) und
// Seitenpfeil (Schriftzeichen 29 an mes_spos[mode][2..3]) blinken ueber mes_fls: gezeichnet bei
// mes_fls < 24, danach mes_fls = (mes_fls + 1) & 0x1F; bhSetMessage, jede Auswahlbewegung und das
// Umblaettern setzen mes_fls 0. OK in der Auswahl kehrt vor der Zeichnung zurueck.
#include "messages.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool ok,const std::string& reason){if(!ok)throw std::runtime_error(reason);}
int main(){try{
    // Seite 1 "AB" + FF02 0 (Seitenende, st_flg 0x1000), Seite 2 "CD" FF04 "E" FF04 "F" FFFF.
    const Message text{33,34,0xff02,0,35,36,0xff04,37,0xff04,38,0xffff};
    MessageDialog d;d.show(text,0);
    unsigned guard=0;while(!d.turnable()){d.update();check(++guard<40,"Seite 1 wird nicht fertig");}
    // Seitenpfeil: der erste Durchlauf am Seitenende zeichnet (mes_fls 0), 24 an, 8 aus.
    std::string pattern;for(unsigned i=0;i<64;++i){d.update();pattern+=d.arrow_drawn()?'1':'0';check(d.cursor_drawn()<0,"Cursor ohne Auswahl");}
    // Der Durchlauf, der turnable() setzte, zeichnete schon (fls 0 -> 1): danach 23 an, 8 aus, 24 an, ...
    check(pattern.substr(0,23)==std::string(23,'1')&&pattern.substr(23,8)==std::string(8,'0')&&pattern.substr(31,24)==std::string(24,'1'),"Pfeilblinken "+pattern);
    d.queue_ok();check(d.update()==MessageDialog::Step::Turned&&d.arrow_drawn(),"Umblaettern: Pfeil im selben Durchlauf (mes_fls 0)");
    guard=0;while(!d.choosing()){d.update();check(++guard<40,"Auswahl wird nicht erreicht");}
    // Der Durchlauf, der FF04 erreicht, zeichnet schon (message.c:131-140 setzt 0x8000, :231 folgt im selben
    // Aufruf). mes_fls stand seit dem Umblaettern auf 1 (Textaufbau zaehlt es nicht): 23 an, 8 aus.
    std::string choice;choice+=d.cursor_drawn()==0?'1':'0';
    for(unsigned i=0;i<40;++i){d.update();choice+=d.cursor_drawn()==0?'1':(d.cursor_drawn()<0?'0':'x');}
    check(choice.substr(0,23)==std::string(23,'1')&&choice.substr(23,8)==std::string(8,'0')&&choice[31]=='1',"Cursorblinken "+choice);
    d.queue_move(1);d.update();check(d.moved()&&d.cursor_drawn()==1,"Bewegung setzt mes_fls 0 und zeigt den Cursor sofort");
    d.queue_ok();check(d.update()==MessageDialog::Step::Answered&&d.cursor_drawn()<0,"OK kehrt vor der Zeichnung zurueck");
    std::cout<<"message blink PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"message blink: "<<e.what()<<'\n';return 1;}}
