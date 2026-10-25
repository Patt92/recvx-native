// recvx-mod-bake <data-tree> <mods> [--compress]
// Bakes a mods folder into a PRIVATE copy of a cooked data tree (the package staging tree), so an app can
// ship without a mods folder and without per-texture MD5 work at runtime. Every replacement is the file the
// runtime would pick for the same asset (asset_io.cpp apply_texture_files, the *.bin/RDT/RVV/MLT lookups):
//   mods/textures/<identity>/<n>.tga  -> textures/<identity>/<n>.tga           (per-asset texture)
//   mods/textures/hd/<md5>.tga        -> every textures/**.tga whose cooked MD5 is <md5> (if no per-asset file)
//   mods/rooms/RM_xxxx.RDT            -> rooms/RM_xxxx.RDT
//   mods/players|weapons/NNNN.bin     -> archives/SYSTEM/NNNN.bin
//   mods/doors/NNNN.bin               -> archives/MRY/(71+NNNN).bin
//   mods/items/NNNN.bin               -> archives/ITEM1/(NNNN-1).bin
//   mods/movies/NNNN.rvv              -> movies/NNNN.rvv
//   mods/audio/system/COMMON.MLT      -> files/COMMON.MLT
// Textures are written in the cooked layout, with --compress as colour-mapped/RLE TGA (compress_tga).
// Files are replaced by rename, never written through, so a hard-linked staging copy leaves the source
// tree untouched. Anything else in mods (hd-source/, DDS, manifests) is not part of the runtime lookups
// and is ignored.
#include "asset_io.h"
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
namespace fs=std::filesystem;
static void replace_file(const fs::path& target,const std::vector<std::uint8_t>& bytes){
    fs::create_directories(target.parent_path());auto temporary=target;temporary+=".bake-tmp";
    fs::remove(temporary);recvx::write_asset_file(temporary,bytes);fs::rename(temporary,target);
}
static void replace_copy(const fs::path& source,const fs::path& target){
    fs::create_directories(target.parent_path());auto temporary=target;temporary+=".bake-tmp";
    fs::remove(temporary);fs::copy_file(source,temporary);fs::rename(temporary,target);
}
static bool numbered(const fs::path& p,const char* ext,unsigned& n){
    const auto stem=p.stem().string();if(p.extension()!=ext||stem.size()!=4||stem.find_first_not_of("0123456789")!=std::string::npos)return false;
    n=unsigned(std::stoul(stem));return true;
}
int main(int argc,char** argv){try{
    if(argc!=3&&!(argc==4&&std::string(argv[3])=="--compress"))throw std::runtime_error("usage: recvx-mod-bake <data-tree> <mods> [--compress]");
    const auto data=fs::u8path(argv[1]),mods=fs::u8path(argv[2]);const bool compress=argc==4;
    if(!fs::is_regular_file(data/"native-data.version"))throw std::runtime_error("not a cooked data tree: "+data.string());
    if(!fs::is_directory(mods)){std::cout<<"Baked mods: none ("<<mods.string()<<" missing)\n";return 0;}
    auto encode=[&](const fs::path& source){const auto bytes=recvx::encode_tga(recvx::read_tga(source));return compress?recvx::compress_tga(bytes,source):bytes;};
    // Texturen: Mod-Datei je Asset vor HD vor data/ (dieselbe Rangfolge wie apply_texture_files).
    std::map<fs::path,fs::path> texture_jobs; // target -> source
    const auto mod_textures=mods/"textures";std::size_t per_asset=0,hd_hits=0;
    if(fs::is_directory(mod_textures))for(auto it=fs::recursive_directory_iterator(mod_textures);it!=fs::recursive_directory_iterator();++it){
        if(it->is_directory()&&it->path()==mod_textures/"hd"){it.disable_recursion_pending();continue;}
        if(!it->is_regular_file()||it->path().extension()!=".tga")continue;
        texture_jobs[data/"textures"/fs::relative(it->path(),mod_textures)]=it->path();++per_asset;}
    std::map<std::string,fs::path> hd;
    if(fs::is_directory(mod_textures/"hd"))for(const auto& e:fs::directory_iterator(mod_textures/"hd"))if(e.is_regular_file()&&e.path().extension()==".tga")hd[e.path().stem().string()]=e.path();
    if(!hd.empty()&&fs::is_directory(data/"textures")){
        std::vector<fs::path> cooked;for(const auto& e:fs::recursive_directory_iterator(data/"textures"))if(e.is_regular_file()&&e.path().extension()==".tga"&&!texture_jobs.count(e.path()))cooked.push_back(e.path());
        std::vector<std::string> keys(cooked.size());std::atomic<std::size_t> next{0};
        auto hash=[&]{for(std::size_t i;(i=next++)<cooked.size();)keys[i]=recvx::cooked_tga_md5(cooked[i]);};
        std::vector<std::thread> pool;for(unsigned j=0;j<std::max(1u,std::thread::hardware_concurrency());++j)pool.emplace_back(hash);for(auto& t:pool)t.join();
        for(std::size_t i=0;i<cooked.size();++i)if(const auto at=hd.find(keys[i]);at!=hd.end()){texture_jobs[cooked[i]]=at->second;++hd_hits;}
    }
    {std::vector<std::pair<fs::path,fs::path>> list(texture_jobs.begin(),texture_jobs.end());std::atomic<std::size_t> next{0};std::mutex lock;std::string error;
     auto work=[&]{for(std::size_t i;(i=next++)<list.size();){try{replace_file(list[i].first,encode(list[i].second));}
         catch(const std::exception& e){std::lock_guard<std::mutex> l(lock);if(error.empty())error=e.what();next=list.size();}}};
     std::vector<std::thread> pool;for(unsigned j=0;j<std::max(1u,std::thread::hardware_concurrency());++j)pool.emplace_back(work);for(auto& t:pool)t.join();
     if(!error.empty())throw std::runtime_error(error);}
    // Uebrige Ersetzungen: dieselben Pfade, die die Runtime unter --mods nachschlaegt.
    std::size_t files=0;
    auto each=[&](const fs::path& folder,auto&& target){if(!fs::is_directory(mods/folder))return;
        for(const auto& e:fs::directory_iterator(mods/folder))if(e.is_regular_file()){const auto to=target(e.path());if(!to.empty()){replace_copy(e.path(),to);++files;}}};
    unsigned n=0;
    each("rooms",[&](const fs::path& p){return p.extension()==".RDT"?data/"rooms"/p.filename():fs::path{};});
    each("players",[&](const fs::path& p){return numbered(p,".bin",n)?data/"archives/SYSTEM"/p.filename():fs::path{};});
    each("weapons",[&](const fs::path& p){return numbered(p,".bin",n)?data/"archives/SYSTEM"/p.filename():fs::path{};});
    each("doors",[&](const fs::path& p){return numbered(p,".bin",n)?data/"archives/MRY"/(recvx::asset_number(71+n)+".bin"):fs::path{};});
    each("items",[&](const fs::path& p){return numbered(p,".bin",n)&&n>0?data/"archives/ITEM1"/(recvx::asset_number(n-1)+".bin"):fs::path{};});
    each("movies",[&](const fs::path& p){return numbered(p,".rvv",n)?data/"movies"/p.filename():fs::path{};});
    if(fs::is_regular_file(mods/"audio/system/COMMON.MLT")){replace_copy(mods/"audio/system/COMMON.MLT",data/"files/COMMON.MLT");++files;}
    std::cout<<"Baked mods from "<<mods.string()<<": "<<per_asset<<" per-asset textures, "<<hd_hits<<" textures from "<<hd.size()<<" HD files, "<<files<<" other files"<<(compress?" (textures compressed)":"")<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"recvx-mod-bake: "<<e.what()<<'\n';return 1;}}
