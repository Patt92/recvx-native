#pragma once
#include <vector>
namespace recvx {
// O_WRK fields read and written by objitm.c bhObjItmBox (bhJumpObject[100]).
struct ItemBoxObjectWork {unsigned type=0,mode0=0;int ct0=0,ax=0;};
// One bhControlObject pass of a type-100 object. `cb_flg`/`player_stflg` are
// sys->cb_flg and plp->stflg; CallSystemSe numbers are appended to `sounds`.
void control_item_box_object(ItemBoxObjectWork& work,unsigned& cb_flg,unsigned& player_stflg,std::vector<unsigned>& sounds);
}
