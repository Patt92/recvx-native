#pragma once
#include "events.h"
#include "inventory.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace recvx {
// Original SAVEFILE (types.h, 0x838 bytes): SYS_WORK version..save_end plus
// CheckSam. bhPushGameData (room.c) fills the player fields from plp and
// copies this range; bhPopGameData copies it back and reloads the room with
// ss_flg 0x200 / cb_flg 0x800000, which places the player at ply_pos/ply_ang.
struct SaveFile {
    static constexpr std::size_t size=0x838;
    std::uint32_t version=10;
    std::int32_t save_ct=0;
    std::int8_t ply_id=0,stg_no=0,rom_no=0,rcase=0,pos_no=0,cut_no=0,flr_no=0,reserve0=0;
    std::int32_t gm_mode=0;
    std::array<std::uint32_t,32> ev_flg{};
    std::array<std::uint32_t,16> it_flg{},ic_flg{};
    std::array<std::uint32_t,32> ed_flg{};
    std::array<std::uint32_t,16> ky_flg{};
    std::array<std::uint32_t,8> mp_flg{};
    std::array<std::uint32_t,384> itm{};
    std::array<std::uint32_t,4> ply_stflg{};
    NJS_POINT3 ply_pos{};
    std::int32_t ply_ang=0;
    std::array<std::int8_t,4> ply_wno{};
    std::array<std::int16_t,4> ply_hp{};
    std::array<std::uint8_t,4> evt_posno{};
    std::int32_t time=0,stv_tm=0;
    std::int16_t spray_ct=0,retry_ct=0,clear_ct=0,reserve1=0;
    std::uint32_t check_sum=0; // Original algorithm lives in the undecompiled card code; unused here.
    std::vector<std::uint8_t> encode()const; // little-endian, original offsets
    static SaveFile decode(const std::vector<std::uint8_t>& bytes);
};
// Host state outside SAVEFILE. In the original these live in the running
// session (gm_flg/ts_flg/plp->flg2/ssd_flg, max HP, condition), which
// bhPopGameData leaves untouched; a snapshot file must carry them itself.
struct SnapshotHost {
    std::string room;          // RM_xxxx.RDX, redundant check of stg_no/rom_no
    unsigned body=10;          // SYSTEM asset of the player model
    std::array<std::array<std::uint32_t,32>,4> session_flags{}; // matrices 11,12,15,16
    std::array<unsigned,3> session_values{}; // event values 7,18,25
    int max_hp=160;
    bool poisoned=false,infected=false,side_pack=false;
};
struct Snapshot {SaveFile save;SnapshotHost host;};
struct SnapshotPlayer {NJS_POINT3 position{};Angle yaw=0;int floor=0;unsigned spawn=0;std::size_t camera=0;};
// bhPushGameData. Counters (save_ct/retry_ct/clear_ct) are never incremented:
// a snapshot is no typewriter save and must not touch ranking counters.
Snapshot capture_snapshot(const Inventory& inventory,const EventVm& events,unsigned body,const std::string& room,const SnapshotPlayer& player);
// bhPopGameData, split along the native load order: inventory before the room
// is built, flags after inherit_progress and before Event_init scripts.
void restore_inventory(const Snapshot& snapshot,Inventory& inventory);
void restore_events(const Snapshot& snapshot,EventVm& events);
SnapshotPlayer snapshot_player(const Snapshot& snapshot);
void write_snapshot(const std::filesystem::path& path,const Snapshot& snapshot);
Snapshot read_snapshot(const std::filesystem::path& path);
}
