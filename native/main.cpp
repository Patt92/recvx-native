#include "expand.h"
#include "archive.h"
#include "room.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace fs = std::filesystem;
static int inspect(const fs::path& root) {
    if (!fs::is_directory(root)) throw std::runtime_error("data directory does not exist");
    std::ifstream config(root / "SYSTEM.CNF");
    const std::string contents((std::istreambuf_iterator<char>(config)), {});
    if (contents.find("SLUS_201.84") == std::string::npos)
        throw std::runtime_error("SYSTEM.CNF does not identify supported US release SLUS_201.84");
    if (!fs::is_regular_file(root / "SLUS_201.84"))
        throw std::runtime_error("SLUS_201.84 is missing");
    std::ifstream manifest(root / "CF_ROM.TXT");
    if (!manifest) throw std::runtime_error("CF_ROM.TXT is missing");
    std::size_t files=0, entries=0;
    std::string line;
    while (std::getline(manifest, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (line.empty()) continue;
        std::replace(line.begin(), line.end(), '\\', '/');
        while (!line.empty() && line.front()=='/') line.erase(0,1);
        fs::path relative(line);
        if (relative.is_absolute() || relative.has_root_name())
            throw std::runtime_error("invalid manifest path");
        for (const auto& component : relative)
            if (component == "..") throw std::runtime_error("invalid manifest traversal");
        auto path = root / relative;
        // Disc manifests use lowercase names; extracted ISO files often uppercase.
        if (!fs::exists(path)) {
            auto upper = line;
            for (auto& c : upper) if (c>='a' && c<='z') c = char(c-'a'+'A');
            path = root / upper;
        }
        if (!fs::is_regular_file(path)) throw std::runtime_error("missing asset: " + line);
        const auto extension = relative.extension().string();
        if (extension == ".afs" || extension == ".AFS") {
            const auto n = recvx::AfsArchive(path).entries().size();
            entries += n;
            std::cout << line << ": " << n << " AFS entries\n";
        }
        ++files;
    }
    if (!files) throw std::runtime_error("empty asset manifest");
    std::cout << "Validated " << files << " manifest files and " << entries << " AFS entries.\n"
              << "Native data tools only; gameplay and rendering are not yet implemented.\n";
    return 0;
}
static void write_bytes(const fs::path& output_path, const std::vector<std::uint8_t>& data) {
    if (fs::exists(output_path)) throw std::runtime_error("output already exists: " + output_path.string());
    std::ofstream output(output_path, std::ios::binary);
    if (!data.empty()) output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    output.close();
    if (!output) throw std::runtime_error("cannot write output: " + output_path.string());
}
static int rooms(const fs::path& root, const fs::path& destination = {}) {
    recvx::RoomArchive archive(root);
    std::ofstream manifest;
    if (!destination.empty()) {
        if (!fs::create_directory(destination)) throw std::runtime_error("export directory already exists");
        manifest.open(destination/"rooms.csv.partial");
        if (!manifest) throw std::runtime_error("cannot create room manifest");
        manifest << "name,bytes,version,cameras,lights,enemies,objects,items,effects,walls,other_collision,floors,positions,routes,event_controls,messages,event_lights\n";
    }
    std::uint64_t total = 0;
    for (std::size_t i=0; i<archive.size(); ++i) {
        const auto data = archive.load(i,64*1024*1024);
        const auto info = recvx::inspect_room(data);
        total += data.size();
        std::cout << archive.name(i) << ": " << data.size() << " decoded bytes, "
                  << info.counts[0] << " cameras, " << info.counts[2] << " enemies\n";
        if (manifest.is_open()) {
            auto output_name = archive.name(i);
            output_name.replace(output_name.size()-3,3,"RDT");
            write_bytes(destination/output_name,data);
            manifest << output_name << ',' << data.size() << ',' << info.version;
            for (auto count : info.counts) manifest << ',' << count;
            manifest << '\n';
            if (!manifest) throw std::runtime_error("cannot write room manifest");
        }
    }
    if (manifest.is_open()) {
        manifest.close();
        if (!manifest) throw std::runtime_error("cannot finish room manifest");
        fs::rename(destination/"rooms.csv.partial",destination/"rooms.csv");
    }
    std::cout << "Decoded and inspected " << archive.size() << " rooms (" << total << " bytes).\n";
    return 0;
}
int main(int argc, char** argv) {
    try {
        if (argc==3 && std::string(argv[1])=="--inspect") return inspect(fs::u8path(argv[2]));
        if (argc==3 && std::string(argv[1])=="--verify-rooms") return rooms(fs::u8path(argv[2]));
        if (argc==4 && std::string(argv[1])=="--export-rooms") return rooms(fs::u8path(argv[2]),fs::u8path(argv[3]));
        if (argc==5 && std::string(argv[1])=="--extract-room") {
            const recvx::RoomArchive archive(fs::u8path(argv[2]));
            const auto data = archive.load(argv[3],64*1024*1024);
            write_bytes(fs::u8path(argv[4]),data);
            std::cout << "Decoded " << argv[3] << " (" << data.size() << " bytes)\n";
            return 0;
        }
        if (argc==5 && std::string(argv[1])=="--expand") {
            const std::string limit_text(argv[4]);
            if (limit_text.empty() || limit_text.find_first_not_of("0123456789") != std::string::npos)
                throw std::runtime_error("output limit must be a positive byte count");
            const auto limit = std::stoull(limit_text);
            if (!limit || limit > std::numeric_limits<std::size_t>::max())
                throw std::runtime_error("invalid output limit");
            const auto input_path = fs::u8path(argv[2]);
            const auto output_path = fs::u8path(argv[3]);
            if (fs::exists(output_path)) throw std::runtime_error("output already exists");
            if (fs::file_size(input_path)>256ULL*1024*1024)
                throw std::runtime_error("compressed input exceeds 256 MiB tool limit");
            std::ifstream input(input_path, std::ios::binary);
            if (!input) throw std::runtime_error("cannot open input");
            const std::vector<std::uint8_t> source((std::istreambuf_iterator<char>(input)), {});
            const auto result = recvx::expand(source, static_cast<std::size_t>(limit));
            write_bytes(output_path,result);
            std::cout << "Expanded " << result.size() << " bytes\n";
            return 0;
        }
        std::cout << "recvx-native: native port data tools (not a playable game)\n"
                  << "  --inspect <extracted-disc-directory>\n"
                  << "  --expand <compressed-file> <new-output-file> <max-output-bytes>\n"
                  << "  --verify-rooms <extracted-disc-directory>\n"
                  << "  --export-rooms <extracted-disc-directory> <new-output-directory>\n"
                  << "  --extract-room <extracted-disc-directory> <RM_0000.RDX> <new-output-file>\n";
        return argc==1 ? 0 : 2;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
