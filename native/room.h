#pragma once
#include "archive.h"
#include <array>
#include <string>
#include <memory>
namespace recvx {
// system.c formats stg_no/rom_no/rcase with %1d%02d%1d, never hexadecimal.
struct RoomIdentity {
    unsigned stage=0,room=0,variant=0;
    static RoomIdentity parse(const std::string& name);
    std::string name()const;
};
struct RoomInfo {
    float version;
    std::uint32_t room_offset;
    std::uint32_t model_table_offset, motion_table_offset, event_offset, texture_offset;
    std::array<std::uint32_t, 14> counts;
    std::uint32_t flags, background_color, fog_color;
    float fog_near, fog_far;
};
// Decode serialized values individually. Disk offsets never become host pointers.
RoomInfo inspect_room(const std::vector<std::uint8_t>& data);
class RoomArchive {
public:
    explicit RoomArchive(const std::filesystem::path& root);
    std::size_t size() const;
    std::string name(std::size_t index) const;
    std::vector<std::uint8_t> load(std::size_t index, std::size_t output_limit) const;
    std::vector<std::uint8_t> load(const std::string& name, std::size_t output_limit) const;
private:
    std::unique_ptr<AfsArchive> archive_;
    std::filesystem::path root_;
};
}
