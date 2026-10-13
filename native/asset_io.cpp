#include "asset_io.h"
#include <cstring>
#include "binary_view.h"
#include "md5.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
namespace recvx {
std::vector<std::uint8_t> read_asset_file(const std::filesystem::path& path,std::size_t limit){
    const auto size=std::filesystem::file_size(path);if(size>limit)throw std::runtime_error("asset exceeds size limit: "+path.string());
    std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("cannot read asset: "+path.string());
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));if(size)stream.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(size));
    if(!stream)throw std::runtime_error("truncated asset: "+path.string());return result;
}
void write_asset_file(const std::filesystem::path& path,const std::vector<std::uint8_t>& data){
    if(std::filesystem::exists(path))throw std::runtime_error("asset output already exists: "+path.string());
    std::filesystem::create_directories(path.parent_path());std::ofstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("cannot create asset: "+path.string());
    stream.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));stream.close();
    if(!stream)throw std::runtime_error("cannot write asset: "+path.string());
}
std::string asset_number(unsigned i){std::ostringstream s;s<<std::setfill('0')<<std::setw(4)<<i;return s.str();}
std::size_t system_texture_bundle_offset(const std::vector<std::uint8_t>& entry){
    std::size_t p=0;
    for(unsigned block=0;block<256;++block){
        if(p+4>entry.size())return std::size_t(-1);
        std::uint32_t size=0;std::memcpy(&size,entry.data()+p,4);p+=4;
        if(size&0x80000000u){p=(p+31)&~std::size_t(31);return p<entry.size()?p:std::size_t(-1);}
        if(size>entry.size()||p+size>entry.size())return std::size_t(-1);
        p+=size;
    }
    return std::size_t(-1);
}
// Texture files and decoded original textures share one alpha domain: the
// host 8-bit alpha whose blend factor is alpha/255.  Ps2AlphaIsHalf's GS
// 0..128 ratio is the same value over 128, i.e. the same factor, so a palette
// keeps its file alpha and the TGA carries it unchanged.
SceneTexture read_tga(const std::filesystem::path& path){
    const auto bytes=read_asset_file(path,256*1024*1024+65536);BinaryView v(bytes);v.require(0,18);
    if(v.u8(1)!=0||v.u8(2)!=2||(v.u8(16)!=24&&v.u8(16)!=32))throw std::runtime_error("texture must be uncompressed RGB/RGBA TGA: "+path.string());
    SceneTexture t;t.width=v.u16(12);t.height=v.u16(14);const auto stride=v.u8(16)/8;
    if(!t.width||!t.height||t.width>8192||t.height>8192)throw std::runtime_error("TGA dimensions outside 1..8192");
    const auto start=18+v.u8(0);const auto descriptor=v.u8(17);if(descriptor&0xc0)throw std::runtime_error("interleaved TGA is unsupported");
    const auto count=std::size_t(t.width)*t.height;v.require(start,count*stride);t.pixels.resize(count);
    for(unsigned y=0;y<t.height;++y)for(unsigned x=0;x<t.width;++x){
        const auto at=start+(std::size_t(y)*t.width+x)*stride;const auto row=(descriptor&32)?y:t.height-1-y,col=(descriptor&16)?t.width-1-x:x;
        t.pixels[std::size_t(row)*t.width+col]=(stride==4?std::uint32_t(v.u8(at+3))<<24:0xff000000)|(std::uint32_t(v.u8(at+2))<<16)|(std::uint32_t(v.u8(at+1))<<8)|v.u8(at);
    }
    return t;
}
void write_tga(const std::filesystem::path& path,const SceneTexture& t){
    if(!t.width||!t.height||t.width>8192||t.height>8192||t.pixels.size()!=std::size_t(t.width)*t.height)throw std::runtime_error("invalid exported texture");
    std::vector<std::uint8_t> bytes(18+t.pixels.size()*4);bytes[2]=2;bytes[12]=t.width&255;bytes[13]=t.width>>8;bytes[14]=t.height&255;bytes[15]=t.height>>8;bytes[16]=32;bytes[17]=40;
    for(std::size_t i=0;i<t.pixels.size();++i){const auto p=t.pixels[i];for(unsigned c=0;c<3;++c)bytes[18+i*4+c]=(p>>(c*8))&255;bytes[18+i*4+3]=(p>>24)&255;}
    write_asset_file(path,bytes);
}
void apply_texture_files(Scene& scene,const std::filesystem::path& data,const std::filesystem::path& mods,const std::filesystem::path& identity){
    // Precedence: explicit per-asset mod file > content-keyed HD replacement
    // (mods/textures/hd/<md5 of the cooked data TGA>.tga) > cooked data file > decoded original.
    for(std::size_t i=0;i<scene.textures.size();++i){const auto relative=std::filesystem::path("textures")/identity/(asset_number(unsigned(i))+".tga");
        if(!mods.empty()&&std::filesystem::is_regular_file(mods/relative)){scene.textures[i]=read_tga(mods/relative);continue;}
        if(!std::filesystem::is_regular_file(data/relative))continue;
        if(!mods.empty()){const auto hd=mods/"textures"/"hd"/(md5_hex(read_asset_file(data/relative,256*1024*1024+65536))+".tga");
            if(std::filesystem::is_regular_file(hd)){scene.textures[i]=read_tga(hd);continue;}}
        scene.textures[i]=read_tga(data/relative);
    }
}
}
