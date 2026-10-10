#include "world.h"
namespace recvx {
bool RoomWorld::start_item_box(unsigned object){
    auto& e=entity(2,object);if(!(e.object_flags&1))return false;
    // op->type = 100; *(int*)&op->mode0 = 0;
    e.type=100;auto& work=item_boxes_[object];work.type=100;work.mode0=0;work.ax=e.angles[0];return true;
}
void RoomWorld::control_item_boxes(unsigned& cb_flg,unsigned& player_stflg){
    if(!(simulation_flags_&4u))return; // bhControlObjItm's sp_flg gate.
    for(auto it=item_boxes_.begin();it!=item_boxes_.end();){
        auto& e=entity(2,it->first);auto& work=it->second;
        if(e.type!=100){it=item_boxes_.erase(it);continue;}
        if(!(e.object_flags&1u)){++it;continue;}
        std::vector<unsigned> sounds;work.ax=e.angles[0];
        control_item_box_object(work,cb_flg,player_stflg,sounds);
        e.angles[0]=work.ax;e.type=work.type;actor_nodes_.clear();
        for(const auto sound:sounds)audio_.play_system_midi(sound);
        if(work.type!=100)it=item_boxes_.erase(it);else ++it;
    }
}
}
