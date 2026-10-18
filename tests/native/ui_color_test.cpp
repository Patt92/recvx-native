#include "renderer.h"
#include "save_slots.h"
#include "typewriter.h"
#include "ui.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
using namespace recvx;
// Farbkanaele im 2D-/UI-Pfad. Leinwand und SceneTexture sind 0xAARRGGBB
// (runtime.cpp SDL_PIXELFORMAT_ARGB8888, asset_io.cpp read_tga, scene.cpp TIM2-Umrechnung).
// Die Originalfarben sind NJS_COLOR.color = 0xAARRGGBB: njDrawPolygon2DM schreibt
// argb.r/.g/.b einzeln in RGBAQ (ps2_NaDraw2D.c:229-240), argb.r ist Bit 16..23.
// Damit ist die Originalfarbe unveraendert die Leinwandfarbe; ein R/B-Tausch ist falsch.
// Pruefung mit asymmetrischen Farben (R != B) gegen eine weisse Bezugsdarstellung,
// die gegen jeden Kanaltausch unveraenderlich ist.
static void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static std::string hex(std::uint32_t v){char b[16];std::snprintf(b,sizeof b,"%08x",v);return b;}
static unsigned ch(std::uint32_t c,unsigned shift){return (c>>shift)&255u;}
class TempDirectory {
public:
    TempDirectory(){const auto base=std::filesystem::temp_directory_path();const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;;++attempt){path_=base/("recvx-ui-color-test-"+std::to_string(nonce)+"-"+std::to_string(attempt));if(std::filesystem::create_directory(path_))return;}}
    ~TempDirectory(){std::error_code ec;std::filesystem::remove_all(path_,ec);}
    const std::filesystem::path& path()const{return path_;}
private:
    std::filesystem::path path_;
};
// Bildpunkt (x,y) mit dem groessten Unterschied zwischen zwei Bildern im Rechteck.
struct Diff {std::size_t changed=0;int x=-1,y=-1;};
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("usage: recvx-ui-color-tests <data>");
    const auto data=std::filesystem::u8path(argv[1]);
    GameUi ui(data);
    // ---- 1. Schreibmaschine, Dateiname im harten Spielmodus.
    // mcDispFileName (ps2_McSaveFile.c:785-829): gm_mode 2 -> col 0xFFFF2020 (:806), gm_mode 0
    // -> col -1 (:798); bhDispMessageEx/bhDispFontEx (message.c:452-494) legen argb unveraendert
    // in alle vier NJS_COLOR. Erwartet: R wie bei Weiss, G/B auf 0x20/0xFF gedaempft.
    auto render=[&](int game_mode){
        const TempDirectory saves;
        Snapshot snap;snap.save.ply_id=0;snap.save.stg_no=1;snap.save.rom_no=8;snap.save.save_ct=1;snap.save.ply_hp[0]=120;
        snap.save.stv_tm=4321;snap.save.gm_mode=game_mode;snap.host.room="RM_1080.RDX";snap.host.body=10;
        write_save_slot(saves.path(),1,snap);
        // Slot 3: vorhandene, unlesbare Datei (Host-Zustand "damaged", save_slots.cpp:30).
        {std::ofstream bad(save_slot_path(saves.path(),3),std::ios::binary);bad<<"not a save";}
        Typewriter screen;screen.open(Typewriter::Mode::Save,saves.path());
        check(screen.phase()==Typewriter::Phase::Files,"Slotliste nicht offen");
        check(screen.slots().at(0).exists&&screen.slots().at(0).game_mode==game_mode,"Slot 1 traegt gm_mode nicht");
        check(screen.slots().at(2).damaged,"Slot 3 ist nicht als beschaedigt erkannt");
        SoftwareRenderer image(640,480);ui.draw_typewriter(image,screen);return image;
    };
    const auto white=render(0),hard=render(2);
    auto at=[](const SoftwareRenderer& image,int x,int y){return image.pixels()[std::size_t(y)*640+x];};
    // Zeile 0: y = 108 (draw_typewriter, McSaveFile.c:627/647 fy = 108 + 40*Zeile), Name ab x 132.
    std::size_t changed=0;int bx=-1,by=-1;unsigned best=0;
    for(int y=108;y<136;++y)for(int x=132;x<600;++x){
        const auto w=at(white,x,y),h=at(hard,x,y);if(w==h)continue;++changed;
        check(ch(h,16)+1>=ch(w,16)&&ch(h,16)<=ch(w,16)+1,"harter Modus: R weicht von Weiss ab bei ("+std::to_string(x)+","+std::to_string(y)+") weiss "+hex(w)+" rot "+hex(h));
        check(ch(h,0)<=ch(w,0)&&ch(h,8)<=ch(w,8),"harter Modus: G/B nicht gedaempft bei ("+std::to_string(x)+","+std::to_string(y)+") weiss "+hex(w)+" rot "+hex(h));
        const unsigned drop=ch(w,0)-ch(h,0);if(drop>best){best=drop;bx=x;by=y;}
    }
    check(changed>200,"harter Modus aendert die Namenszeile kaum: "+std::to_string(changed));
    check(best>32,"harter Modus: B kaum gedaempft");
    std::cout<<"typewriter gm_mode 2 (0xFFFF2020): "<<changed<<" px, sample ("<<bx<<","<<by<<") white "<<hex(at(white,bx,by))<<" -> hard "<<hex(at(hard,bx,by))<<"\n";
    // Beschaedigter Slot (Host-Kennzeichnung, kein Original): Ziffern 0xFFFF2020 ueber
    // bhDispFontEx-Pfad und die als rot benannte Marke. Beide muessen rot erscheinen.
    // Zeile 2: y = 188; Ziffer '3' bei x 86, Marke bei (126, 195), oberste Markenzeile voll.
    check(at(hard,126,195)==0xffff0000u,"Marke des beschaedigten Slots nicht rot: "+hex(at(hard,126,195)));
    std::size_t lit=0;for(int y=188;y<216;++y)for(int x=86;x<114;++x){const auto c=at(hard,x,y);if(std::max({ch(c,0),ch(c,8),ch(c,16)})<48)continue;++lit;
        check(ch(c,16)>ch(c,0),"Ziffer des beschaedigten Slots nicht rot bei ("+std::to_string(x)+","+std::to_string(y)+"): "+hex(c));}
    check(lit>20,"Ziffer des beschaedigten Slots fehlt");
    std::cout<<"typewriter damaged slot: mark "<<hex(at(hard,126,195))<<", digit pixels R>B: "<<lit<<"\n";
    // ---- 2. Farbcodes im Meldungstext.
    // bhControlMessage (message.c:161-175): 0xFE01..0xFE05 -> col 0..4; bhDispFont (:401-431):
    // 1 = 0xFF20E0FF, 2 = 0xFFFF2020, 3 = 0xFF20FF20, 4 = 0xFF606060, sonst (auch 0) 0xFFFFFFFF.
    auto message=[&](std::uint16_t code){SoftwareRenderer image(640,480);std::fill(image.canvas().begin(),image.canvas().end(),0xff000000u);
        Message m;if(code)m.push_back(code);m.push_back(33);m.push_back(0xffff);ui.show_message(image,m,100,100);return image;};
    const auto base=message(0);
    struct Expect {std::uint16_t code;std::uint32_t color;};
    for(const auto e:{Expect{0xfe01,0xffffffffu},Expect{0xfe02,0xff20e0ffu},Expect{0xfe03,0xffff2020u},Expect{0xfe04,0xff20ff20u},Expect{0xfe05,0xff606060u}}){
        const auto img=message(e.code);std::size_t lit=0,dimmed=0;int sx=-1,sy=-1;unsigned peak=0;
        for(int y=100;y<128;++y)for(int x=100;x<128;++x){const auto w=at(base,x,y),c=at(img,x,y);
            for(unsigned shift:{0u,8u,16u}){
                if(ch(e.color,shift)==255)check(ch(c,shift)==ch(w,shift),"Farbcode "+hex(e.code)+": voller Kanal "+std::to_string(shift)+" veraendert, weiss "+hex(w)+" -> "+hex(c));
                else{check(ch(c,shift)<=ch(w,shift),"Farbcode "+hex(e.code)+": Kanal "+std::to_string(shift)+" nicht gedaempft");if(ch(w,shift)>64&&ch(c,shift)<ch(w,shift))++dimmed;}
            }
            if(ch(w,16)>64)++lit;if(ch(w,16)>peak){peak=ch(w,16);sx=x;sy=y;}
        }
        check(lit>10,"Glyphe fehlt");
        if(e.color!=0xffffffffu)check(dimmed>10,"Farbcode "+hex(e.code)+" daempft keinen Kanal (Farbe "+hex(e.color)+")");
        std::cout<<"message code "<<hex(e.code)<<" (bhDispFont "<<hex(e.color)<<"): ("<<sx<<","<<sy<<") white "<<hex(at(base,sx,sy))<<" -> "<<hex(at(img,sx,sy))<<"\n";
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
