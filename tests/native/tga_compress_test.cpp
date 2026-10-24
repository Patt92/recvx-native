// compress_tga / read_tga: verlustfreie Datenbaumform der gekochten TGAs (prepare_native_data.py --compressed).
// Farbtabelle (Typ 9) bis 256 Farben, sonst RGB/RGBA-RLE (Typ 10); read_tga ergibt dieselben Bildpunkte, der
// HD-Inhaltsschluessel (apply_texture_files) dieselben Bytes wie die unkomprimierte Quelle.
#include "asset_io.h"
#include "md5.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace recvx;
namespace fs=std::filesystem;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::vector<std::uint8_t> raw_tga(unsigned w,unsigned h,unsigned bpp,unsigned descriptor,const std::vector<std::uint32_t>& argb){
    std::vector<std::uint8_t> b(18);b[2]=2;b[12]=std::uint8_t(w&255);b[13]=std::uint8_t(w>>8);b[14]=std::uint8_t(h&255);b[15]=std::uint8_t(h>>8);b[16]=std::uint8_t(bpp);b[17]=std::uint8_t(descriptor);
    for(const auto p:argb){b.push_back(std::uint8_t(p));b.push_back(std::uint8_t(p>>8));b.push_back(std::uint8_t(p>>16));if(bpp==32)b.push_back(std::uint8_t(p>>24));}
    return b;
}
static void write(const fs::path& p,const std::vector<std::uint8_t>& b){fs::create_directories(p.parent_path());write_asset_file(p,b);}
int main(){try{
    const auto root=fs::temp_directory_path()/("recvx-tga-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}}cleanup{root};
    // 1) 32 Bit, oben links (Deskriptor 0x28 wie die gekochten Dateien), Laeufe, Einzelpunkte, Alphastufen.
    std::vector<std::uint32_t> a(40*9);for(std::size_t i=0;i<a.size();++i)a[i]=(i%7<4?0x80102030u:0x00ff0000u|std::uint32_t((i*37)%200)<<24)|std::uint32_t(i%5);
    const auto a_raw=raw_tga(40,9,32,0x28,a);
    const auto a_packed=compress_tga(a_raw,"a.tga");
    check(a_packed[1]==1&&a_packed[2]==9&&a_packed[16]==8&&a_packed[7]==32,"<=256 Farben: Typ 9 mit 32-Bit-Tabelle erwartet");
    check(a_packed.size()<a_raw.size(),"Farbtabelle ist nicht kleiner");
    write(root/"raw/a.tga",a_raw);write(root/"packed/a.tga",a_packed);
    check(read_tga(root/"raw/a.tga").pixels==read_tga(root/"packed/a.tga").pixels,"Typ 9 liest andere Bildpunkte");
    // 2) 24 Bit, unten links (Deskriptor 0): Ausrichtung bleibt die der Quelle.
    std::vector<std::uint32_t> b(13*5);for(std::size_t i=0;i<b.size();++i)b[i]=0xff000000u|std::uint32_t(i*1013);
    const auto b_raw=raw_tga(13,5,24,0,b);const auto b_packed=compress_tga(b_raw,"b.tga");
    check(b_packed[2]==9&&b_packed[7]==24,"24 Bit: Tabelleneintraege 24 Bit erwartet");
    write(root/"raw/b.tga",b_raw);write(root/"packed/b.tga",b_packed);
    check(read_tga(root/"raw/b.tga").pixels==read_tga(root/"packed/b.tga").pixels,"24-Bit-Typ-9 liest andere Bildpunkte");
    // 3) mehr als 256 Farben: RGBA-RLE (Typ 10), weiterhin bildpunktgleich.
    std::vector<std::uint32_t> c(32*32);for(std::size_t i=0;i<c.size();++i)c[i]=0xff000000u|std::uint32_t(i*7919);
    const auto c_raw=raw_tga(32,32,32,0x28,c);const auto c_packed=compress_tga(c_raw,"c.tga");
    check(c_packed[1]==0&&c_packed[2]==10,">256 Farben: Typ 10 erwartet");
    write(root/"raw/c.tga",c_raw);write(root/"packed/c.tga",c_packed);
    check(read_tga(root/"raw/c.tga").pixels==read_tga(root/"packed/c.tga").pixels,"Typ 10 liest andere Bildpunkte");
    // 4) HD-Ersetzung: der Schluessel ist das MD5 der unkomprimierten Datei, auch im komprimierten Baum.
    const fs::path identity="rooms/RM_TEST";
    write(root/"data/textures"/identity/"0000.tga",a_packed);
    SceneTexture hd;hd.width=2;hd.height=2;hd.pixels.assign(4,0xff00ff00u);
    write_tga(root/"mods/textures/hd"/(md5_hex(a_raw.data(),a_raw.size())+".tga"),hd);
    Scene scene;scene.textures.resize(1);apply_texture_files(scene,root/"data",root/"mods",identity);
    check(scene.textures[0].width==2&&scene.textures[0].pixels==hd.pixels,"HD-Schluessel des komprimierten Baums weicht ab");
    // 5) Index ausserhalb der Tabelle wird abgewiesen, nicht ersetzt.
    auto broken=a_packed;broken[5]=1;broken[6]=0; // Tabellenlaenge 1, Indizes reichen weiter
    write(root/"broken/a.tga",broken);
    bool threw=false;try{read_tga(root/"broken/a.tga");}catch(const std::runtime_error&){threw=true;}
    check(threw,"Index ausserhalb der Farbtabelle nicht abgewiesen");
    // 6) Quellen mit Anhang (TGA-2.0-Fusszeile) oder bereits komprimierte Quellen werden abgewiesen.
    auto footer=a_raw;for(const char c:std::string("\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0",26))footer.push_back(std::uint8_t(c));
    threw=false;try{compress_tga(footer,"footer.tga");}catch(const std::runtime_error&){threw=true;}
    check(threw,"Quelle mit Fusszeile nicht abgewiesen");
    threw=false;try{compress_tga(a_packed,"packed.tga");}catch(const std::runtime_error&){threw=true;}
    check(threw,"bereits komprimierte Quelle nicht abgewiesen");
    std::cout<<"TGA compression: colour-mapped/RLE round trip, orientation, HD key and index bounds passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"tga compress test failed: "<<e.what()<<'\n';return 1;}}
