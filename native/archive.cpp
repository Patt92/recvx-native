#include "archive.h"
#include <array>
#include <fstream>
#include <stdexcept>
#include <utility>
namespace recvx {
static std::uint32_t le32(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
static void read_exact(std::ifstream& file, unsigned char* p, std::size_t n) {
    if (n && !file.read(reinterpret_cast<char*>(p), static_cast<std::streamsize>(n)))
        throw std::runtime_error("truncated AFS file");
}
AfsArchive::AfsArchive(std::filesystem::path path) : path_(std::move(path)) {
    std::ifstream file(path_, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open AFS: " + path_.string());
    std::array<unsigned char, 8> header{};
    read_exact(file, header.data(), header.size());
    if (header[0]!='A' || header[1]!='F' || header[2]!='S' || (header[3]!=0 && header[3]!=' '))
        throw std::runtime_error("invalid AFS signature: " + path_.string());
    const auto size = std::filesystem::file_size(path_);
    const auto count = le32(header.data()+4);
    const std::uint64_t table_end = 8 + std::uint64_t(count)*8;
    // Keep allocation bounded even if a corrupt archive claims billions of entries.
    if (table_end > size || count > 1000000)
        throw std::runtime_error("invalid AFS entry count");
    entries_.reserve(count);
    for (std::uint32_t i=0; i<count; ++i) {
        std::array<unsigned char,8> record{};
        read_exact(file, record.data(), record.size());
        const ArchiveEntry entry{le32(record.data()), le32(record.data()+4)};
        if (std::uint64_t(entry.offset)+entry.size > size || (entry.size && entry.offset < table_end))
            throw std::runtime_error("AFS entry exceeds data bounds: " + path_.string());
        entries_.push_back(entry);
    }
}
std::vector<std::uint8_t> AfsArchive::read(std::size_t index, std::size_t limit) const {
    if (index >= entries_.size()) throw std::runtime_error("AFS entry index out of range");
    const auto entry = entries_[index];
    if (entry.size > limit) throw std::runtime_error("AFS member exceeds input limit");
    std::ifstream file(path_, std::ios::binary);
    if (!file) throw std::runtime_error("cannot reopen AFS: " + path_.string());
    file.seekg(entry.offset);
    if (!file) throw std::runtime_error("cannot seek to AFS member");
    std::vector<std::uint8_t> data(entry.size);
    read_exact(file, data.data(), data.size());
    return data;
}
std::filesystem::path disc_file(const std::filesystem::path& root, const std::string& name) {
    // PS2 filenames are ASCII; resolve casing on case-sensitive desktop volumes.
    if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name=="." || name=="..")
        throw std::runtime_error("invalid disc filename");
    auto fold = [](std::string s) {
        for (auto& c : s) if (c>='a' && c<='z') c=char(c-'a'+'A');
        return s;
    };
    for (const auto& file : std::filesystem::directory_iterator(root))
        if (fold(file.path().filename().string())==fold(name) && file.is_regular_file())
            return file.path();
    throw std::runtime_error("missing disc file: " + name);
}
}
