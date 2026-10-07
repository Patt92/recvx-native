#include "asset_io.h"
#include "md5.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace recvx;
namespace fs=std::filesystem;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static SceneTexture solid(unsigned w,unsigned h,std::uint32_t color){SceneTexture t;t.width=w;t.height=h;t.pixels.assign(std::size_t(w)*h,color);return t;}
int main(){try{
    // RFC 1321 test vectors (the key must match Python's hashlib.md5 in tools/build_hd_textures.py).
    check(md5_hex(nullptr,0)=="d41d8cd98f00b204e9800998ecf8427e","md5 empty");
    const std::string abc="abc";check(md5_hex(reinterpret_cast<const std::uint8_t*>(abc.data()),abc.size())=="900150983cd24fb0d6963f7d28e17f72","md5 abc");
    const std::string longer="12345678901234567890123456789012345678901234567890123456789012345678901234567890";
    check(md5_hex(reinterpret_cast<const std::uint8_t*>(longer.data()),longer.size())=="57edf4a22be3c955ac49da2e2107b67a","md5 multi-block");
    const auto root=fs::temp_directory_path()/("recvx-hd-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}}cleanup{root};
    const auto data=root/"data",mods=root/"mods";const fs::path identity="rooms/RM_TEST";
    write_tga(data/"textures"/identity/"0000.tga",solid(4,4,0x80ff0000));write_tga(data/"textures"/identity/"0001.tga",solid(4,4,0x8000ff00));
    Scene scene;scene.textures={solid(2,2,0x11111111),solid(2,2,0x22222222),solid(2,2,0x33333333)};
    apply_texture_files(scene,data,{},identity);
    check(scene.textures[0].width==4&&scene.textures[1].width==4&&scene.textures[2].width==2,"cooked data textures not applied");
    // Content-keyed HD replacement applies by pixel-file identity, wherever the texture is used.
    const auto key=md5_hex(read_asset_file(data/"textures"/identity/"0000.tga",1<<20));
    write_tga(mods/"textures/hd"/(key+".tga"),solid(16,16,0x80ffffff));
    scene.textures={solid(2,2,0),solid(2,2,0),solid(2,2,0)};apply_texture_files(scene,data,mods,identity);
    check(scene.textures[0].width==16&&scene.textures[1].width==4,"HD replacement did not apply by content key");
    // Another room using the same cooked content gets the same replacement; different content does not.
    write_tga(data/"textures/rooms/RM_OTHER/0007.tga",solid(4,4,0x80ff0000));
    Scene other;other.textures.assign(8,solid(1,1,0));apply_texture_files(other,data,mods,"rooms/RM_OTHER");check(other.textures[7].width==16,"shared texture not replaced in second room");
    // Explicit per-asset file wins over the content-keyed HD file.
    write_tga(mods/"textures"/identity/"0000.tga",solid(8,8,0x80000000));
    scene.textures={solid(2,2,0),solid(2,2,0),solid(2,2,0)};apply_texture_files(scene,data,mods,identity);check(scene.textures[0].width==8,"explicit mod file lost precedence");
    std::cout<<"HD texture replacement: MD5 vectors, content-keyed lookup, shared use and precedence verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<"HD texture test failed: "<<e.what()<<'\n';return 1;}}
