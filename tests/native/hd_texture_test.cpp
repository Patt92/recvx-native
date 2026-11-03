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
    // Runtime-only identities (weapons/doors/effects/title) have no cooked
    // file to hash. Explicit overrides must work with their decoded originals.
    const fs::path weapon_identity="weapons/0021/right";
    write_tga(mods/"textures"/weapon_identity/"0000.tga",solid(8,8,0xff123456));
    Scene weapon;weapon.textures={solid(2,2,0xff654321)};weapon.textures[0].clamp_u=true;
    // Content-keyed HD replacement applies by pixel-file identity, wherever the texture is used.
    const auto key=md5_hex(read_asset_file(data/"textures"/identity/"0000.tga",1<<20));
    write_tga(mods/"textures/hd"/(key+".tga"),solid(16,16,0x80ffffff));
    apply_texture_files(weapon,data,mods,weapon_identity);
    check(weapon.textures[0].width==8&&weapon.textures[0].pixels[0]==0xff123456,"explicit uncooked identity override missing");
    check(weapon.textures[0].clamp_u,"override changed original wrap mode");
    scene.textures={solid(2,2,0),solid(2,2,0),solid(2,2,0)};apply_texture_files(scene,data,mods,identity);
    check(scene.textures[0].width==16&&scene.textures[1].width==4,"HD replacement did not apply by content key");
    // Another room using the same cooked content gets the same replacement; different content does not.
    write_tga(data/"textures/rooms/RM_OTHER/0007.tga",solid(4,4,0x80ff0000));
    Scene other;other.textures.assign(8,solid(1,1,0));apply_texture_files(other,data,mods,"rooms/RM_OTHER");check(other.textures[7].width==16,"shared texture not replaced in second room");
    // Explicit per-asset file wins over the content-keyed HD file.
    write_tga(mods/"textures"/identity/"0000.tga",solid(8,8,0x80000000));
    scene.textures={solid(2,2,0),solid(2,2,0),solid(2,2,0)};apply_texture_files(scene,data,mods,identity);check(scene.textures[0].width==8,"explicit mod file lost precedence");
    // Decoded-only resources use the ORIGINAL canonical image as identity.
    const auto decoded=solid(4,4,0xff334455);const auto decoded_key=md5_hex(encode_tga(decoded));
    write_tga(mods/"textures/hd"/(decoded_key+".tga"),solid(12,12,0xff778899));
    Scene terminal;terminal.textures={decoded};terminal.textures[0].clamp_v=true;
    apply_texture_files(terminal,data,mods,"computer");
    check(terminal.textures[0].width==12&&terminal.textures[0].clamp_v,"decoded-only content identity not replaced");
    // Original palette bank identity stays independent of base override and
    // of every other bank, including when one bank changes resolution.
    auto banked=solid(4,4,0xff000000);banked.palette_banks[5]=decoded.pixels;
    banked.palette_banks[9]=solid(4,4,0xffff0000).pixels;
    auto banks=load_texture_palette_files(banked,data,mods,"menu",0);
    check(banks.at(5).width==12&&banks.at(9).width==4,"bank overrides leaked to another bank");
    write_tga(mods/"textures/palettes/menu/0000/0009/0000.tga",solid(16,16,0xff00ff00));
    banks=load_texture_palette_files(banked,data,mods,"menu",0);
    check(banks.at(9).width==16&&banks.at(9).pixels[0]==0xff00ff00,"explicit palette override missing");
    set_texture_mods_enabled(false);
    terminal.textures={decoded};apply_texture_files(terminal,data,mods,"computer");
    check(terminal.textures[0].width==4&&terminal.textures[0].pixels[0]==0xff334455,"decoded mod switch failed");
    banks=load_texture_palette_files(banked,data,mods,"menu",0);
    check(banks.at(5).width==4&&banks.at(9).pixels[0]==0xffff0000,"palette mod switch failed");
    set_texture_mods_enabled(true);
    std::cout<<"HD texture replacement: MD5 vectors, content-keyed lookup, shared use and precedence verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<"HD texture test failed: "<<e.what()<<'\n';return 1;}}
