#include "object_itmbox.h"
#include "ninja_math.h"
namespace recvx {
namespace itmbox_original {
struct O_WRK {unsigned short type;unsigned char mode0;int ax;int ct0;};
struct System {unsigned cb_flg;};
struct Player {unsigned stflg;};
static thread_local System system;static thread_local System* sys=&system;
static thread_local Player player;static thread_local Player* plp=&player;
static thread_local std::vector<unsigned>* sounds=nullptr;
static void CallSystemSe(int,unsigned number){sounds->push_back(number);}
#include "object_itmbox.inc"
}
void control_item_box_object(ItemBoxObjectWork& work,unsigned& cb_flg,unsigned& player_stflg,std::vector<unsigned>& out){
    using namespace itmbox_original;
    O_WRK op{static_cast<unsigned short>(work.type),static_cast<unsigned char>(work.mode0),work.ax,work.ct0};
    system={cb_flg};player={player_stflg};sounds=&out;
    bhObjItmBox(&op);
    sounds=nullptr;cb_flg=system.cb_flg;player_stflg=player.stflg;
    work={op.type,op.mode0,op.ct0,op.ax};
}
}
