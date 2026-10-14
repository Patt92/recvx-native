#include "save_slots.h"
#include <stdexcept>
#include <iterator>
namespace recvx {
namespace {
// RoomNameSwitch, bup_00.c:112–180. Pure original room-name mapping.
unsigned save_room(unsigned room){
    constexpr unsigned rooms[]={0,1,108,311,500,604,709,722,904,914,4,921,600,724,404,804,304,702,207};
    for(unsigned i=0;i<std::size(rooms);++i)if(rooms[i]==room)return i;
    return 0;
}
}
std::filesystem::path save_slot_path(const std::filesystem::path& saves_dir,unsigned slot){
    if(slot<1||slot>kSaveSlots)throw std::runtime_error("save slot outside 1..15");
    return saves_dir/("save-"+std::to_string(slot)+".rcvxsav");
}
std::vector<SaveSlotInfo> list_save_slots(const std::filesystem::path& saves_dir){
    std::vector<SaveSlotInfo> out;out.reserve(kSaveSlots);
    for(unsigned slot=1;slot<=kSaveSlots;++slot){
        SaveSlotInfo info;info.slot=slot;
        const auto path=save_slot_path(saves_dir,slot);
        std::error_code ec;
        if(std::filesystem::is_regular_file(path,ec)){
            try{
                const auto snap=read_snapshot(path);
                info.exists=true;info.room=snap.host.room;info.player=unsigned(std::uint8_t(snap.save.ply_id));
                info.hp=snap.save.ply_hp[std::uint8_t(snap.save.ply_id)&3];info.play_time=unsigned(snap.save.stv_tm);info.save_ct=unsigned(snap.save.save_ct);
                info.game_mode=snap.save.gm_mode;
                info.save_room=save_room(unsigned(std::uint8_t(snap.save.stg_no))*100+unsigned(std::uint8_t(snap.save.rom_no)));
            }catch(const std::exception&){info.exists=false;info.damaged=true;}
        }
        out.push_back(std::move(info));
    }
    return out;
}
void write_save_slot(const std::filesystem::path& saves_dir,unsigned slot,const Snapshot& snapshot){
    write_snapshot(save_slot_path(saves_dir,slot),snapshot);
}
Snapshot read_save_slot(const std::filesystem::path& saves_dir,unsigned slot){
    return read_snapshot(save_slot_path(saves_dir,slot));
}
}
