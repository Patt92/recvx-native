#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
#include <string>
namespace recvx {
struct ArchiveEntry { std::uint32_t offset, size; };
class AfsArchive {
public:
    explicit AfsArchive(std::filesystem::path path);
    const std::vector<ArchiveEntry>& entries() const { return entries_; }
    std::vector<std::uint8_t> read(std::size_t index, std::size_t limit) const;
private:
    std::filesystem::path path_;
    std::vector<ArchiveEntry> entries_;
};
std::filesystem::path disc_file(const std::filesystem::path& root, const std::string& name);
}
