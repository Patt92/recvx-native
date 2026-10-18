#include "inventory.h"
#include "renderer.h"
#include "ui.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
// Ausruestungsflaeche oben rechts im Kistenbildschirm (RM_1080 u. a.).
// ArmsSet (sub1.c:4975-5105) laeuft je Bild aus SpriteOnOff (:8311) und setzt im
// Kistenbildschirm (subscreenmode 0x4) center 9 statt 3 (:4988-4996). Normale Kiste:
// parts_11b anim 4/6/5 (Hintergrund aus parts_16) und das ausgeruestete Item parts_14b[0]
// (:5001-5052), Ziffern ueber BulletSet mode 0 (:5088-5090). Das Standarditem pip[1]
// zeigt ItemSet in der normalen Kiste nicht (atr &= ~0x20, :2477), nur in der
// Sicherheitskiste bei center 9 (:2457-2474).
// Bezug: der Statusbildschirm zeichnet dieselbe ArmsSet-Gruppe an center 3; seine
// eingeschwungene Lage ist x 332 (SpriteUV, StatusSlide pos_[3][0]). Beide Flaechen
// muessen bildpunktgleich sein, nur um 436 - 332 = 104 verschoben.
static void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static std::string hex(std::uint32_t v){char b[16];std::snprintf(b,sizeof b,"%08x",v);return b;}
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("usage: recvx-item-box-equipment-tests <data>");
    GameUi ui(std::filesystem::u8path(argv[1]));
    // Testaufstellung: Claire mit Feuerzeug im Standardplatz (standard[], sub1.c:1290).
    auto inventory=[](unsigned standard,unsigned equipped){Inventory inv;std::fill(inv.slots(),inv.slots()+16,0u);
        if(standard)inv.slots()[1]=Inventory::pack(standard,1);
        inv.slots()[2]=Inventory::pack(equipped?equipped:9,15);inv.slots()[3]=Inventory::pack(16,1);inv.slots()[0]=equipped?2:0;
        for(unsigned i=0;i<7;++i)inv.items[64+i]=Inventory::pack(5+i,1);return inv;};
    StatusMenu menu;menu.active=true;menu.selected=2;
    auto box=[&](const Inventory& inv,unsigned base){SoftwareRenderer image(640,480);ui.draw_box(image,inv,menu,base,base==64?3:0,false);return image;};
    auto status=[&](const Inventory& inv){SoftwareRenderer image(640,480);MessageDialog dialog;ui.draw(image,inv,menu,dialog,0);return image;};
    auto at=[](const SoftwareRenderer& image,int x,int y){return image.pixels()[std::size_t(y)*640+x];};
    // Flaeche parts_11b: x 24..160 relativ zum Zentrum, y 8..56 (parts_16 anim 4/6/5, je 48 hoch).
    auto same_as_status=[&](const SoftwareRenderer& b,const SoftwareRenderer& s,const std::string& what){
        for(int y=48;y<96;++y)for(int x=0;x<136;++x){const auto bp=at(b,460+x,y),sp=at(s,356+x,y);
            check(bp==sp,what+": Kiste ("+std::to_string(460+x)+","+std::to_string(y)+") "+hex(bp)+" != Status ("+std::to_string(356+x)+","+std::to_string(y)+") "+hex(sp));}};
    // 1. Nichts ausgeruestet: die Flaeche ist vollstaendig belegt (vorher schwarzer Spalt
    //    x 512..527: parts_11b lag an center 3, pip[1] an center 4), und sie gleicht dem Statusbildschirm.
    {const auto inv=inventory(55,0);const auto b=box(inv,64);
     for(int y=48;y<96;++y)for(int x=460;x<596;++x){const auto c=at(b,x,y);check(std::max({(c>>16)&255u,(c>>8)&255u,c&255u})>=16,"Ausruestungsflaeche hat schwarzes Loch bei ("+std::to_string(x)+","+std::to_string(y)+"): "+hex(c));}
     same_as_status(b,status(inv),"leere Ausruestung");}
    // 2. Standarditem pip[1] in der normalen Kiste unsichtbar, in der Sicherheitskiste sichtbar.
    {const auto with=inventory(55,9),without=inventory(0,9);
     check(box(with,64).pixels()==box(without,64).pixels(),"normale Kiste zeichnet das Standarditem pip[1]");
     const auto a=box(with,224),b=box(without,224);std::size_t changed=0;
     for(int y=48;y<96;++y)for(int x=444;x<512;++x)changed+=at(a,x,y)!=at(b,x,y);
     check(changed>200,"Sicherheitskiste zeigt das Standarditem nicht: "+std::to_string(changed));}
    // 3. Ausgeruestetes Item (schmal: pos 58, breit: pos 24) samt Ziffern wie im Status.
    for(const unsigned item:{9u,1u}){const auto inv=inventory(55,item);const auto b=box(inv,64);
     check(b.pixels()!=box(inventory(55,0),64).pixels(),"ausgeruestetes Item "+std::to_string(item)+" fehlt in der Kiste");
     same_as_status(b,status(inv),"ausgeruestetes Item "+std::to_string(item));}
    std::cout<<"item box equipment slot: ArmsSet center 9 matches status center 3; pip[1] hidden in normal box\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
