#include "asset_io.h"
#include "renderer.h"
#include "scene.h"
#include "typewriter.h"
#include "ui.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
using namespace recvx;
// Schreibmaschinen-Slotliste (ps2_SaveScreen.c:203-207 ExecuteSaveScreen -> DispBackGroundTexture,
// McSaveFile.c:544-554 mcDisplayFileSelectWindow -> mcDispWindowFoundtion).
// Jedes Bild beginnt mit der Loeschfarbe Schwarz (bup_00.c:299-304 njSetBackColor(0,0,0) bei
// gestopptem Task 7, sceGsSetDefDBuffDc(...,1) ps2_dummy.c:89). Die Hintergrundtexturen ITEM1/0144
// sind schwarz gekeyt (Ps2AlphaIs000, ps2_dummy.c:1700) und am Alphatest (ps2_dummy.c:363) Loecher.
// Das Bild darf deshalb nicht vom vorherigen Inhalt der Leinwand abhaengen, und das Fenster
// 0x18808080 mischt in jedem Bild genau einmal: Loch innerhalb (16..616, 96..310) wird
// ((0x80-0)*(0x18808080>>25))>>7 = 12 (ALPHA 0x44 aus bhDispFont 8/6, ps2_NaSystem.c alpha_tbl),
// Loch ausserhalb bleibt 0.
static void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
class TempDirectory {
public:
    TempDirectory(){const auto base=std::filesystem::temp_directory_path();const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;;++attempt){path_=base/("recvx-typewriter-draw-test-"+std::to_string(nonce)+"-"+std::to_string(attempt));if(std::filesystem::create_directory(path_))return;}}
    ~TempDirectory(){std::error_code ec;std::filesystem::remove_all(path_,ec);}
    const std::filesystem::path& path()const{return path_;}
private:
    std::filesystem::path path_;
};
static std::string hex(std::uint32_t v){char b[16];std::snprintf(b,sizeof b,"%08x",v);return b;}
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("usage: recvx-typewriter-draw-tests <data>");
    const auto data=std::filesystem::u8path(argv[1]);
    // Die gewaehlten Pruefpunkte muessen in den Originaltexturen tatsaechlich Loecher sein
    // (5x5-Umgebung Alpha 0, damit die Abtastrundung keine Rolle spielt).
    const auto tex=decode_texture_bundle(read_asset_file(data/"archives/ITEM1/0144.bin",8*1024*1024));
    check(tex.size()==3,"ITEM1/0144 hat nicht drei Texturen");
    auto hole=[&](unsigned t,unsigned u,unsigned v){for(unsigned y=v-2;y<=v+2;++y)for(unsigned x=u-2;x<=u+2;++x)if(tex[t].pixels[y*tex[t].width+x]>>24)return false;return true;};
    // Bildpunkt (600,250): Textur 1, u=(600-512+.5)/128*.24, v=(250.5/480)*.93 -> Texel (84,248).
    // Bildpunkt (2,150): Textur 0, Texel (2,149).
    check(hole(1,84,248),"Pruefpunkt (600,250) ist kein Texturloch");
    check(hole(0,2,149),"Pruefpunkt (2,150) ist kein Texturloch");
    const TempDirectory saves;
    GameUi ui(data);Typewriter screen;screen.open(Typewriter::Mode::Save,saves.path());
    check(screen.phase()==Typewriter::Phase::Files,"Slotliste nicht offen");
    SoftwareRenderer reference(640,480);
    std::fill(reference.canvas().begin(),reference.canvas().end(),0xff000000u);
    ui.draw_typewriter(reference,screen);
    auto at=[](SoftwareRenderer& image,int x,int y){return image.canvas()[std::size_t(y)*640+x];};
    check(at(reference,600,250)==0xff0c0c0cu,"Fensterloch nicht 12/255: "+hex(at(reference,600,250)));
    check(at(reference,2,150)==0xff000000u,"Loch ausserhalb des Fensters nicht schwarz: "+hex(at(reference,2,150)));
    // Vorheriges Bild (hier weiss als Stellvertreter fuer das letzte Raumbild) und mehrere
    // Durchlaeufe hintereinander, wie der interaktive Takt draw() je Bild aufruft.
    SoftwareRenderer image(640,480);
    std::fill(image.canvas().begin(),image.canvas().end(),0xffffffffu);
    for(unsigned pass=0;pass<30;++pass)ui.draw_typewriter(image,screen);
    check(at(image,600,250)==0xff0c0c0cu,"Fensterloch sammelt ueber Bilder an: "+hex(at(image,600,250)));
    check(at(image,2,150)==0xff000000u,"Loch zeigt das vorige Bild: "+hex(at(image,2,150)));
    std::size_t differing=0;for(std::size_t i=0;i<image.canvas().size();++i)if(image.canvas()[i]!=reference.canvas()[i])++differing;
    check(differing==0,"Bild haengt vom vorigen Leinwandinhalt ab: "+std::to_string(differing)+" Bildpunkte");
    std::cout<<"typewriter draw: frame independent, window hole 12/255, outside hole 0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
