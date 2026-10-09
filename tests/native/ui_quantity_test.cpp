#include "ui.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
// D09: eigenständig nativ implementierte Mengenanzeige (native/ui.cpp
// GameUi::quantity) nach der Semantik von BulletSet (sub1.c).
//
// Geprüft wird die sichtbare Wirkung über die öffentliche Zeichenschnittstelle:
// Das Statusmenü mit einem Stapelitem muss mehr Pixel zeichnen als dasselbe
// Menü mit Menge 0, und verschiedene Mengen müssen verschiedene Bilder
// ergeben. Ohne Datenpfad gibt es keine Originalatlanten; dann bleibt nur die
// Typklassifizierung prüfbar.
int main(int argc,char** argv){try{
    unsigned counted=0,percent=0;
    for(unsigned id=1;id<156;++id){const unsigned kind=itemdata[id].type&0x1800;
        if(kind==0x800)++counted;else if(kind==0x1000)++percent;}
    check(counted&&percent,"original quantity classes missing from itemdata");
    if(argc>1){
        const auto data=std::filesystem::u8path(argv[1]);
        GameUi ui(data);
        // Waffe mit zweistelliger Kapazitaet: nur dann sind ein- und
        // zweistellige Mengen beide gueltige Originalwerte (max liegt bei den
        // Stueckzahl-Items zwischen 1 und 30).
        unsigned ammo=0;
        for(unsigned id=1;id<156&&!ammo;++id)
            if((itemdata[id].type&0x1800)==0x800&&(itemdata[id].type&1)&&itemdata[id].max>=10)ammo=id;
        check(ammo,"no stacking ammunition item with a two digit capacity");
        // Dasselbe Menü, nur die Menge unterscheidet sich.
        auto render=[&](unsigned amount,unsigned flags){
            Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);
            inv.slots()[2]=Inventory::pack(ammo|flags,amount);
            StatusMenu menu;menu.message_source=[](unsigned){return Message{'T'-32,0xffff};};
            menu.input(MenuInput::Open,inv);menu.skip_slide();menu.tick(inv);
            MessageDialog dialog;SoftwareRenderer image(640,480);
            ui.draw(image,inv,menu,dialog,0);
            return image.pixels();};
        // Der Statushintergrund ist deckend, also zaehlt die Gesamtpixelzahl
        // nichts. Verglichen wird gegen dasselbe Menue ohne Item: die Zellen,
        // die sich unterscheiden, sind Icon plus Mengenanzeige.
        Inventory bare;std::fill(bare.slots()+2,bare.slots()+12,0);
        StatusMenu bm;bm.message_source=[](unsigned){return Message{'T'-32,0xffff};};
        bm.input(MenuInput::Open,bare);bm.skip_slide();bm.tick(bare);
        MessageDialog bd;SoftwareRenderer blank(640,480);ui.draw(blank,bare,bm,bd,0);
        auto changed=[&](const std::vector<std::uint32_t>& pixels){
            unsigned n=0;for(std::size_t i=0;i<pixels.size();++i)n+=pixels[i]!=blank.pixels()[i];return n;};
        const unsigned full=itemdata[ammo].max; // zweistellig, siehe oben
        const auto one=render(7,0),two=render(full,0),endless=render(full,0x800);
        check(changed(one),"item cell draws nothing at all");
        // Mehr Stellen heisst mehr veraenderte Pixel gegenueber dem leeren Menue.
        check(changed(two)>changed(one),"two digit amount draws no more than a single digit");
        // Unendlich zeigt eine Sonderglyphe statt der Ziffern.
        check(endless!=two,"endless ammunition renders like a plain amount");
        // Die Farbbank unterscheidet die Munitionssorten von Item 6.
        if((itemdata[6].type&0x1801)==0x801){
            Inventory a,b;
            for(auto* inv:{&a,&b})std::fill(inv->slots()+2,inv->slots()+12,0);
            a.slots()[2]=Inventory::pack(6|0x1000,15);b.slots()[2]=Inventory::pack(6|0x2000,15);
            auto shot=[&](Inventory& inv){StatusMenu m;m.message_source=[](unsigned){return Message{'T'-32,0xffff};};
                m.input(MenuInput::Open,inv);m.skip_slide();m.tick(inv);MessageDialog d;SoftwareRenderer r(640,480);
                ui.draw(r,inv,m,d,0);return r.pixels();};
            // Gleiche Menge, nur die Sortenflags unterscheiden sich.
            check(shot(a)!=shot(b),"ammunition variants must use different palette banks");}
        // D09 Teil 2: Befehlsfenster nach ItemCommand testmode 0 + TrigerSet +
        // Cursorschleife. Verglichen wird dasselbe Menü mit offenem und
        // geschlossenem Fenster; die Differenzfläche ist das Fenster.
        struct Box{int x0=640,y0=480,x1=-1,y1=-1;unsigned n=0;int height()const{return y1-y0+1;}};
        auto window=[&](unsigned item,unsigned cursor,bool burst,std::vector<std::uint32_t>* shown=nullptr){
            Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(item,1);
            auto frame=[&](bool open){StatusMenu m;m.message_source=[](unsigned){return Message{'T'-32,0xffff};};m.shotgun_burst=[burst]{return burst;};
                m.input(MenuInput::Open,inv);m.skip_slide();m.tick(inv);m.selected=2;
                if(open){m.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)m.tick(inv);for(unsigned i=0;i<cursor;++i)m.input(MenuInput::Down,inv);check(m.commands&&m.command==cursor,"command window/cursor not reached");}
                else for(int f=0;f<4;++f)m.tick(inv); // gleiche Tickzahl: EKG und Cursorblinken laufen identisch
                MessageDialog d;SoftwareRenderer r(640,480);ui.draw(r,inv,m,d,0);return r.pixels();};
            const auto closed=frame(false),open=frame(true);Box b;
            for(int y=0;y<480;++y)for(int x=0;x<640;++x)if(closed[y*640+x]!=open[y*640+x]){b.x0=std::min(b.x0,x);b.x1=std::max(b.x1,x);b.y0=std::min(b.y0,y);b.y1=std::max(b.y1,y);++b.n;}
            if(shown)*shown=open;return b;};
        const auto herb=window(21,0,false),gun=window(5,0,false),shotgun=window(10,0,false);
        check(herb.n&&gun.n&&shotgun.n,"command window draws nothing");
        // TrigerSet: Rahmenunterkante 120/152/184 (Teil 5) -> +32 bzw. +64 px.
        check(gun.height()-herb.height()==32,"weapon command window is not 32 px taller than the item window (TrigerSet)");
        check(shotgun.height()-herb.height()==64,"shotgun command window is not 64 px taller than the item window (TrigerSet)");
        // Cursorschleife: gewählte Zeile atr|4, Farbe (0.125,0.5,1). Die blaue
        // Zeile wandert mit dem Cursor nach unten.
        auto blue_row=[&](unsigned item,unsigned cursor){std::vector<std::uint32_t> img;const auto b=window(item,cursor,false,&img);double sum=0;unsigned n=0;
            for(int y=b.y0;y<=b.y1;++y)for(int x=b.x0;x<=b.x1;++x){const auto p=img[y*640+x];const int r=(p>>16)&255,g=(p>>8)&255,bl=p&255;if(bl>r+60&&g>r)sum+=y,++n;}
            check(n>0,"selected command row is not tinted (0.125,0.5,1)");return sum/n;};
        check(blue_row(5,1)>blue_row(5,0)+8&&blue_row(5,2)>blue_row(5,1)+8,"highlighted command row does not follow the cursor");
        // Item 10: Teil 17 zeigt anim 12 (Salve, ev_flg 74) bzw. 11.
        {std::vector<std::uint32_t> single,burst;window(10,0,false,&single);window(10,0,true,&burst);check(single!=burst,"shotgun burst marker (TrigerSet part 17 anim 11/12) not drawn");}
        std::cout<<"Native quantity display (digits, endless glyph, palette banks) and the ItemCommand window (TrigerSet heights, cursor colour, burst marker) change the drawn status screen.\n";
        return 0;
    }
    std::cout<<"Quantity classes present; pass the data path to verify drawn digits.\n";return 0;
}catch(const std::exception& e){std::cerr<<"UI quantity test failed: "<<e.what()<<'\n';return 1;}}
