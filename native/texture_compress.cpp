// recvx-texture-compress <source-textures> <target-textures> [jobs]
// Schreibt jede TGA des Quellbaums verlustfrei komprimiert (compress_tga: 8-Bit-Farbtabelle mit RLE,
// sonst RGB/RGBA-RLE) an denselben relativen Pfad. Jede Datei wird vor dem Schreiben gegen ihre
// Quelle geprueft; der Zielbaum darf noch nicht existieren (prepare_native_data.py --compressed).
#include "asset_io.h"
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
    if(argc!=3&&argc!=4)throw std::runtime_error("usage: recvx-texture-compress <source-textures> <target-textures> [jobs]");
    const auto source=fs::u8path(argv[1]),target=fs::u8path(argv[2]);
    if(!fs::is_directory(source))throw std::runtime_error("source texture tree missing: "+source.string());
    if(fs::exists(target))throw std::runtime_error("target exists: "+target.string());
    const unsigned jobs=argc==4?unsigned(std::stoul(argv[3])):std::max(1u,std::thread::hardware_concurrency());
    if(!jobs||jobs>256)throw std::runtime_error("jobs must be 1..256");
    std::vector<fs::path> files;
    for(const auto& entry:fs::recursive_directory_iterator(source))if(entry.is_regular_file()&&entry.path().extension()==".tga")files.push_back(entry.path());
    std::sort(files.begin(),files.end());
    std::atomic<std::size_t> next{0},in{0},out{0},indexed{0};std::mutex error_lock;std::string error;
    auto work=[&]{for(std::size_t i;(i=next++)<files.size();){try{
            const auto bytes=recvx::read_asset_file(files[i],256*1024*1024+65536);
            const auto packed=recvx::compress_tga(bytes,files[i]);
            recvx::write_asset_file(target/fs::relative(files[i],source),packed);
            in+=bytes.size();out+=packed.size();if(packed[2]==9)++indexed;
        }catch(const std::exception& e){std::lock_guard<std::mutex> lock(error_lock);if(error.empty())error=e.what();next=files.size();}}};
    std::vector<std::thread> pool;for(unsigned j=0;j<jobs;++j)pool.emplace_back(work);for(auto& t:pool)t.join();
    if(!error.empty())throw std::runtime_error(error);
    std::cout<<"Compressed textures: "<<files.size()<<" files ("<<indexed<<" colour-mapped), "<<in<<" -> "<<out<<" bytes\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"recvx-texture-compress: "<<e.what()<<'\n';return 1;}}
