#include "asset_io.h"
#include <cstring>
#include "binary_view.h"
#include "md5.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <cctype>
#include <atomic>
#include <optional>
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
// RLE-Pakete: Kopfbyte Bit7 = Wiederholung eines Bildpunkts, sonst Rohfolge; je (Kopf&0x7f)+1 Bildpunkte.
static std::vector<std::uint8_t> rle_tga_pixels(const BinaryView& v,std::size_t in,std::size_t size,unsigned stride,const std::filesystem::path& path){
    std::vector<std::uint8_t> raw(size);std::size_t out=0;
    while(out<raw.size()){v.require(in,1);const auto head=v.u8(in++);const std::size_t n=std::size_t(head&0x7f)+1;
        if(out+n*stride>raw.size())throw std::runtime_error("RLE TGA packet exceeds the image: "+path.string());
        if(head&0x80){v.require(in,stride);for(std::size_t k=0;k<n;++k)for(unsigned c=0;c<stride;++c)raw[out+k*stride+c]=v.u8(in+c);in+=stride;}
        else{v.require(in,n*stride);for(std::size_t k=0;k<n*stride;++k)raw[out+k]=v.u8(in+k);in+=n*stride;}
        out+=n*stride;}
    return raw;
}
// Eine TGA als unkomprimierte Typ-2-Datei: Kopf (Typ 2, ohne Farbtabelle) und Bildpunkte in
// Dateireihenfolge. Typ 10 entpackt RLE; Typ 1/9 (Farbtabelle, 8-Bit-Index, prepare_native_data.py
// --compressed) setzt je Index den Tabelleneintrag ein. Fuer jede gekochte Datei ist das genau die
// Datei, aus der der komprimierte Baum entstand.
struct TgaFile {std::vector<std::uint8_t> raw;unsigned width=0,height=0,stride=0,descriptor=0;std::size_t start=0;};
static TgaFile decode_tga_file(const std::vector<std::uint8_t>& bytes,const std::filesystem::path& path){
    BinaryView v(bytes);v.require(0,18);TgaFile f;const unsigned type=v.u8(2),map_type=v.u8(1);
    f.width=v.u16(12);f.height=v.u16(14);f.descriptor=v.u8(17);f.start=18+v.u8(0);
    if(!f.width||!f.height||f.width>8192||f.height>8192)throw std::runtime_error("TGA dimensions outside 1..8192");
    if(f.descriptor&0xc0)throw std::runtime_error("interleaved TGA is unsupported");
    const std::size_t count=std::size_t(f.width)*f.height;
    std::vector<std::uint8_t> pixels;
    if(type==2||type==10){
        if(map_type!=0||(v.u8(16)!=24&&v.u8(16)!=32))throw std::runtime_error("texture must be RGB/RGBA or 8-bit colour-mapped TGA: "+path.string());
        f.stride=v.u8(16)/8;
        if(type==10)pixels=rle_tga_pixels(v,f.start,count*f.stride,f.stride,path);
        else{v.require(f.start,count*f.stride);pixels.assign(bytes.begin()+std::ptrdiff_t(f.start),bytes.begin()+std::ptrdiff_t(f.start+count*f.stride));}
    }else if(type==1||type==9){
        const unsigned first=v.u16(3),length=v.u16(5),entry=v.u8(7);
        if(map_type!=1||v.u8(16)!=8||(entry!=24&&entry!=32)||!length||first+length>256)throw std::runtime_error("texture must be RGB/RGBA or 8-bit colour-mapped TGA: "+path.string());
        f.stride=entry/8;const std::size_t map=f.start,indices=map+std::size_t(length)*f.stride;v.require(map,std::size_t(length)*f.stride);
        const auto index=type==9?rle_tga_pixels(v,indices,count,1,path):std::vector<std::uint8_t>{};
        if(type==1)v.require(indices,count);
        pixels.resize(count*f.stride);
        for(std::size_t i=0;i<count;++i){const unsigned k=type==9?index[i]:v.u8(indices+i);
            if(k<first||k>=first+length)throw std::runtime_error("colour-mapped TGA index outside its palette: "+path.string());
            for(unsigned c=0;c<f.stride;++c)pixels[i*f.stride+c]=v.u8(map+std::size_t(k-first)*f.stride+c);}
    }else throw std::runtime_error("texture must be RGB/RGBA or 8-bit colour-mapped TGA: "+path.string());
    v.require(0,f.start);f.raw.assign(bytes.begin(),bytes.begin()+std::ptrdiff_t(f.start));
    f.raw[1]=0;f.raw[2]=2;for(unsigned i=3;i<8;++i)f.raw[i]=0;f.raw[16]=std::uint8_t(f.stride*8);
    f.raw.insert(f.raw.end(),pixels.begin(),pixels.end());
    return f;
}
// Inhaltsschluessel der HD-Ersetzung: die Bytes der unkomprimierten gekochten TGA, auch wenn der
// Datenbaum sie komprimiert (RLE oder Farbtabelle) traegt.
static std::vector<std::uint8_t> cooked_tga_identity(const std::filesystem::path& path){
    auto bytes=read_asset_file(path,256*1024*1024+65536);BinaryView v(bytes);v.require(0,18);
    if(v.u8(2)==2)return bytes;
    return decode_tga_file(bytes,path).raw;
}
// TGA-RLE: Wiederholungspakete ab zwei gleichen Bildpunkten, sonst Rohfolgen; je Paket 1..128.
static void rle_tga_append(std::vector<std::uint8_t>& out,const std::uint8_t* src,std::size_t count,unsigned stride){
    auto same=[&](std::size_t a,std::size_t b){return std::memcmp(src+a*stride,src+b*stride,stride)==0;};
    std::size_t i=0;
    while(i<count){
        std::size_t run=1;while(i+run<count&&run<128&&same(i,i+run))++run;
        if(run>=2){out.push_back(std::uint8_t(0x80|(run-1)));out.insert(out.end(),src+i*stride,src+(i+1)*stride);i+=run;continue;}
        std::size_t raw=1;while(i+raw<count&&raw<128&&!(i+raw+1<count&&same(i+raw,i+raw+1)))++raw;
        out.push_back(std::uint8_t(raw-1));out.insert(out.end(),src+i*stride,src+(i+raw)*stride);i+=raw;
    }
}
std::vector<std::uint8_t> compress_tga(const std::vector<std::uint8_t>& bytes,const std::filesystem::path& path){
    const auto f=decode_tga_file(bytes,path);const std::size_t count=std::size_t(f.width)*f.height;
    // Nur unkomprimierte Quellen ohne Anhang (TGA-2.0-Fusszeile, Erweiterungen): deren Offsets waeren
    // nach dem Packen falsch, und der HD-Schluessel ist die ganze Quelldatei.
    if(bytes.size()<2||bytes[2]!=2||bytes.size()!=f.raw.size())throw std::runtime_error("compress_tga needs an uncompressed TGA without trailing data: "+path.string());
    const std::uint8_t* px=f.raw.data()+f.start;
    // Farbtabelle in der Reihenfolge des ersten Auftretens; mehr als 256 Farben bleiben RGB/RGBA-RLE.
    std::map<std::uint32_t,std::uint8_t> palette;std::vector<std::uint8_t> index(count),entries;bool fits=true;
    for(std::size_t i=0;i<count&&fits;++i){std::uint32_t key=0;std::memcpy(&key,px+i*f.stride,f.stride);
        const auto at=palette.find(key);
        if(at!=palette.end()){index[i]=at->second;continue;}
        if(palette.size()==256){fits=false;break;}
        const auto k=std::uint8_t(palette.size());palette.emplace(key,k);index[i]=k;entries.insert(entries.end(),px+i*f.stride,px+(i+1)*f.stride);}
    std::vector<std::uint8_t> out(f.raw.begin(),f.raw.begin()+std::ptrdiff_t(f.start));
    if(fits){
        const auto n=unsigned(palette.size());
        out[1]=1;out[2]=9;out[3]=0;out[4]=0;out[5]=std::uint8_t(n&255);out[6]=std::uint8_t(n>>8);out[7]=std::uint8_t(f.stride*8);out[16]=8;
        out.insert(out.end(),entries.begin(),entries.end());rle_tga_append(out,index.data(),count,1);
    }else{out[2]=10;rle_tga_append(out,px,count,f.stride);}
    if(decode_tga_file(out,path).raw!=f.raw)throw std::runtime_error("compressed TGA does not reproduce its source: "+path.string());
    return out;
}
// Texture files and decoded original textures share one alpha domain: the
// host 8-bit alpha whose blend factor is alpha/255.  Ps2AlphaIsHalf's GS
// 0..128 ratio is the same value over 128, i.e. the same factor, so a palette
// keeps its file alpha and the TGA carries it unchanged.
SceneTexture read_tga(const std::filesystem::path& path){
    const auto bytes=read_asset_file(path,256*1024*1024+65536);
    // Typ 2 = unkomprimiert, Typ 10 = RLE, Typ 1/9 = 8-Bit-Farbtabelle (prepare_native_data.py --compressed):
    // alle drei ergeben dieselbe Bildpunktfolge wie die unkomprimierte Datei.
    const auto file=decode_tga_file(bytes,path);
    SceneTexture t;t.width=file.width;t.height=file.height;const auto stride=file.stride;const auto descriptor=file.descriptor;
    const auto count=std::size_t(t.width)*t.height;t.pixels.resize(count);
    const std::uint8_t* src=file.raw.data()+file.start;
    for(unsigned y=0;y<t.height;++y)for(unsigned x=0;x<t.width;++x){
        const auto at=(std::size_t(y)*t.width+x)*stride;const auto row=(descriptor&32)?y:t.height-1-y,col=(descriptor&16)?t.width-1-x:x;
        t.pixels[std::size_t(row)*t.width+col]=(stride==4?std::uint32_t(src[at+3])<<24:0xff000000)|(std::uint32_t(src[at+2])<<16)|(std::uint32_t(src[at+1])<<8)|src[at];
    }
    return t;
}
void write_tga(const std::filesystem::path& path,const SceneTexture& t){
    if(!t.width||!t.height||t.width>8192||t.height>8192||t.pixels.size()!=std::size_t(t.width)*t.height)throw std::runtime_error("invalid exported texture");
    std::vector<std::uint8_t> bytes(18+t.pixels.size()*4);bytes[2]=2;bytes[12]=t.width&255;bytes[13]=t.width>>8;bytes[14]=t.height&255;bytes[15]=t.height>>8;bytes[16]=32;bytes[17]=40;
    for(std::size_t i=0;i<t.pixels.size();++i){const auto p=t.pixels[i];for(unsigned c=0;c<3;++c)bytes[18+i*4+c]=(p>>(c*8))&255;bytes[18+i*4+3]=(p>>24)&255;}
    write_asset_file(path,bytes);
}
namespace {std::atomic<bool> texture_mods_on{true};std::atomic<std::size_t> stat_mod{0},stat_hd{0},stat_data{0},stat_original{0};}
void set_texture_mods_enabled(bool enabled){texture_mods_on=enabled;}
bool texture_mods_enabled(){return texture_mods_on;}
TextureFileStats texture_file_stats(){return {stat_mod,stat_hd,stat_data,stat_original};}
std::string texture_tree_format(const std::filesystem::path& data){
    // Erste TGA des Baums (sortiert): gekochte Baeume schreiben alle Dateien im selben Format.
    const auto root=data/"textures";if(!std::filesystem::is_directory(root))return "no textures/";
    std::optional<std::filesystem::path> first;
    for(const auto& e:std::filesystem::recursive_directory_iterator(root))if(e.is_regular_file()&&e.path().extension()==".tga"&&(!first||e.path()<*first))first=e.path();
    if(!first)return "no TGA files";
    std::ifstream f(*first,std::ios::binary);unsigned char head[3]{};f.read(reinterpret_cast<char*>(head),3);
    if(!f)return "unreadable TGA";
    switch(head[2]){case 2:return "uncompressed TGA";case 10:return "RLE TGA";case 1:return "colour-mapped TGA";case 9:return "colour-mapped RLE TGA";default:return "TGA type "+std::to_string(head[2]);}
}
std::size_t pcsx2_pack_count(const std::filesystem::path& mods){
    // PCSX2-Ersatztexturen <TEX0-Hash>-[<CLUT-Hash>-]<bits>.dds direkt in textures/: die Schluessel sind
    // xxHash-Werte ueber das GS-Speicherbild, das der Port nicht hat (tools/import_pcsx2_textures.py ordnet zu).
    const auto root=mods/"textures";if(mods.empty()||!std::filesystem::is_directory(root))return 0;
    std::size_t n=0;
    for(const auto& e:std::filesystem::directory_iterator(root)){if(!e.is_regular_file())continue;
        auto ext=e.path().extension().string();for(auto& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));
        const auto stem=e.path().stem().string();if(ext==".dds"&&stem.find('-')!=std::string::npos)++n;}
    return n;
}
std::size_t hd_texture_count(const std::filesystem::path& mods){
    const auto hd=mods/"textures"/"hd";if(mods.empty()||!std::filesystem::is_directory(hd))return 0;
    std::size_t n=0;for(const auto& e:std::filesystem::directory_iterator(hd))if(e.is_regular_file()&&e.path().extension()==".tga")++n;return n;
}
void apply_texture_files(Scene& scene,const std::filesystem::path& data,const std::filesystem::path& mods,const std::filesystem::path& identity){
    // Precedence: explicit per-asset mod file > content-keyed HD replacement
    // (mods/textures/hd/<md5 of the cooked data TGA>.tga) > cooked data file > decoded original.
    // Die Datei ersetzt nur die Bildpunkte; der GS-Wickelmodus gehoert zum Originalkopf.
    // Hostschalter (Debugmenue "Textur-Mods"): aus = beide Mod-Stufen uebergehen, PS2-Bild aus data/.
    const bool use_mods=!mods.empty()&&texture_mods_on;
    for(std::size_t i=0;i<scene.textures.size();++i){const auto relative=std::filesystem::path("textures")/identity/(asset_number(unsigned(i))+".tga");
        auto replace=[&](const std::filesystem::path& file){const bool u=scene.textures[i].clamp_u,v=scene.textures[i].clamp_v;scene.textures[i]=read_tga(file);scene.textures[i].clamp_u=u;scene.textures[i].clamp_v=v;};
        if(use_mods&&std::filesystem::is_regular_file(mods/relative)){replace(mods/relative);++stat_mod;continue;}
        if(!std::filesystem::is_regular_file(data/relative)){++stat_original;continue;}
        if(use_mods){const auto hd=mods/"textures"/"hd"/(md5_hex(cooked_tga_identity(data/relative))+".tga");
            if(std::filesystem::is_regular_file(hd)){replace(hd);++stat_hd;continue;}}
        replace(data/relative);++stat_data;
    }
}
}
