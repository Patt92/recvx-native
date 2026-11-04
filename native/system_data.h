#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>
namespace recvx {
// Systemspeicher des Originals: CONFIGFILE (types.h:1428, 0x34 Byte) = SYS_WORK 0x00-0x30
// (ssd_ver..best_tm, types.h:548-555) plus Check_Sam. Die Felder ueberleben bhInitSystem
// (system.c:123 loescht erst ab tk_flg) und damit jedes Neue Spiel; geschrieben wird der
// Satz nur am Ende von ControlRanking (ps2_SystemSaveScreen.c), gelesen beim Start
// (adv.c:1260 CreateSysLoadScreen). Host: eine Datei im Spielstandordner statt Memory Card
// (wie die Schreibmaschinen-Slots, save_slots.h). keytype/adjust/vibration/ssd_reserve
// bleiben Host-Einstellungen (controls.ini/video.ini); die Datei traegt sie unveraendert mit.
struct SystemData {
    static constexpr std::size_t size=0x34;
    std::uint32_t ssd_ver=0,ssd_flg=0,ssd_reserve=0;
    std::int8_t keytype=0,adjust_x=0,adjust_y=0,vibration=0;
    std::array<std::int32_t,8> best_tm{};
    // mcCreateConfigInit (ELF 0x2718A0): alles 0, vibration 1.
    static SystemData create_init();
    // mcWriteStartConfigFile (0x271DB0): 0x30 Byte aus sys, Check_Sam = MemoryCardGetSum.
    std::vector<std::uint8_t> encode()const;
    // mcCheckReadStartConfigFile (0x271D20): Summenfehler -> -2 (Datei beschaedigt).
    static SystemData decode(const std::vector<std::uint8_t>& bytes);
};
// ps2_MemoryCard..c:1007 MemoryCardGetSum: Bytesumme.
std::uint32_t memory_card_sum(const std::uint8_t* data,std::size_t size);
// ps2_SystemSaveScreen.c:1261 SysSaveHikaku: gelesene Karte in den laufenden Stand mischen.
void sys_save_hikaku(SystemData& sys,SystemData& file);
std::filesystem::path system_data_path(const std::filesystem::path& saves_dir);
enum class SystemDataRead {Missing,Ok,Damaged};
SystemDataRead read_system_data(const std::filesystem::path& saves_dir,SystemData& out);
void write_system_data(const std::filesystem::path& saves_dir,const SystemData& data);
}
