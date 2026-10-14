#pragma once
#include "snapshot.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace recvx {
// Schreibmaschinen-Spielstaende als Host-Datei je Slot (Plattformanpassung der
// Memory Card, AGENTS.md "Spielstaende"). Das Datenformat ist die originale
// SAVEFILE-Struktur (snapshot.h SaveFile, 0x838 Byte) plus die Hostfelder des
// Snapshots. Liegt in <Benutzerdaten>/saves (user_paths.h).
constexpr unsigned kSaveSlots=15; // bup_00.c SelectFileInfo/stg/saveversion[15]
struct SaveSlotInfo {
    unsigned slot=0;         // 1..kSaveSlots
    bool exists=false;
    std::string room;        // RM_xxxx.RDX (redundante Pruefung)
    unsigned player=0;       // ply_id
    int hp=0;
    unsigned play_time=0;    // stv_tm, 60-Hz VSync
    std::uint32_t save_ct=0;
    int game_mode=0;         // SAVEFILE.gm_mode; originale Dateinamenfarbe
    unsigned save_room=0;    // RoomNameSwitch(stg_no * 100 + rom_no), nicht Host-Raumtext
    bool damaged=false;     // vorhandene, nicht lesbare Datei ist kein freier Slot
};
std::filesystem::path save_slot_path(const std::filesystem::path& saves_dir,unsigned slot);
std::vector<SaveSlotInfo> list_save_slots(const std::filesystem::path& saves_dir);
void write_save_slot(const std::filesystem::path& saves_dir,unsigned slot,const Snapshot& snapshot);
// Wirft, wenn der Slot fehlt oder beschaedigt ist.
Snapshot read_save_slot(const std::filesystem::path& saves_dir,unsigned slot);
}
