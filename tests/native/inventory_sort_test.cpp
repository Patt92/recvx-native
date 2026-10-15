#include "ui.h"
#include "events.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
// sub1.c ItemSort (sub1.c:2266) und seine drei Original-Aufrufstellen:
//   * sub1.c:8296 StatusMain -> SpriteOnOff  (jeder Frame, subcsr != 3)
//   * sub1.c:8917 bhSearchBullet             (reload())
//   * sub1.c:9044 EraseItem                  (erase())
// Der Dateiablage-Zweig (fileview.c:322) ruft bewusst KEIN ItemSort; der
// Statusbildschirm sortiert aber im naechsten Frame. Diese Datei prueft, dass
// der native Statusbildschirm (StatusMenu::tick) genau das tut und dass
// breite Zwei-Slot-Eintraege dabei korrekt vorne landen.
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static Inventory bag(){Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);return inv;}
// Ein Statusframe: StatusMain-Logik (tick), danach SpriteOnOff mit ItemSort (sub1.c:3580/8296).
static void frame(StatusMenu& menu,Inventory& inv){menu.tick(inv);menu.sprite_on_off(inv);}
// bhControlMessage-Durchlaeufe (StatusMenu::tick): erst die aufgebaute Seite bzw. offene Auswahl nimmt OK an (message.c:228-337).
static void reveal(StatusMenu& menu){for(unsigned k=0;k<400&&menu.message.active()&&!menu.message.closable()&&!menu.message.choosing()&&!menu.message.turnable();++k)menu.message.update();}

static void tick_menu(StatusMenu& menu,Inventory& inv){menu.input(MenuInput::Open,inv);menu.skip_slide();frame(menu,inv);}
static Message fixture_message(unsigned index){
    if(index==155)return {'L'-32,0xff02,0,'?'-32,0xff04,'Y'-32,0xff04,'N'-32,0xffff};
    return {'T'-32,0xffff};
}
// Statusbildschirm offen, Befehlsfenster des Slots `origin`, Befehl 2 (Kombinieren).
static void begin_combine(StatusMenu& menu,Inventory& inv,unsigned origin){
    menu.message_source=fixture_message;menu.input(MenuInput::Open,inv);menu.skip_slide();frame(menu,inv);
    menu.selected=origin;menu.input(MenuInput::Confirm,inv);for(int f=0;f<4;++f)frame(menu,inv);
    menu.input(MenuInput::Down,inv);menu.input(MenuInput::Down,inv);menu.input(MenuInput::Confirm,inv);frame(menu,inv);
    check(menu.combining&&menu.combine_subcsr()==1&&menu.commands&&menu.combine_from==origin,"Kombinieren startet nicht in subcsr 1 mit offenem Befehlsfenster (sub1.c:4843-4851, 5259-5300)");
}
static std::vector<std::string> failures;
static void section(const char* name,const std::function<void()>& body){try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}}
int main(){try{
    _Make_SinTable();
    // Handbuch-Ablage (fileview.c:322): das Handbuch (145) wird ohne ItemSort
    // aus sys->itm entfernt. Ohne den per-Frame-Sort bliebe die Luecke; der
    // Statusbildschirm schliesst sie im naechsten Frame (sub1.c:8296).
    section("Handbuch-Ablage rutscht nach",[&]{
    {auto inv=bag();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);
     inv.slots()[4]=Inventory::pack(145,1);inv.slots()[5]=Inventory::pack(33,1);inv.slots()[6]=Inventory::pack(12,30);inv.slots()[7]=Inventory::pack(9,1);
     inv.erase_read_manual(false);
     check(Inventory::id(inv.slots()[4])==0,"Handbuch-Ablage ohne EraseItem darf die Original-Luecke (fileview.c:322) nicht selbst schliessen");
     StatusMenu menu;tick_menu(menu,inv);
     check(Inventory::id(inv.slots()[2])==33,"fuehrender Zwei-Slot-Eintrag landet nicht vorne");
     check(Inventory::id(inv.slots()[3])==21&&Inventory::id(inv.slots()[4])==22&&Inventory::id(inv.slots()[5])==12&&Inventory::id(inv.slots()[6])==9&&Inventory::id(inv.slots()[7])==0,
           "Statusbildschirm sortiert den Bestand nach der Handbuch-Ablage nicht nach");}
    });
    // EraseItem (sub1.c:9044): sortiert nur, wenn der geloeschte Slot vor dem
    // ausgeruesteten liegt (wp < pip + pip[0]); die Luecke danach ebenfalls der
    // per-Frame-Sort.
    section("EraseItem-Aufrufstelle",[&]{
    {auto inv=bag();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);inv.slots()[4]=Inventory::pack(9,1);
     inv.slots()[0]=5; // ausgeruestet: Slot 5
     inv.erase(3);    // vor dem ausgeruesteten Slot -> pip[0] faellt, ItemSort
     check(inv.slots()[0]==4,"EraseItem vor dem ausgeruesteten Slot senkt pip[0] nicht (sub1.c:9044)");
     check(Inventory::id(inv.slots()[2])==21&&Inventory::id(inv.slots()[3])==9&&Inventory::id(inv.slots()[4])==0,"EraseItem sortiert nach dem Loeschen nicht (ItemSort-Aufruf sub1.c:9044)");}
    {auto inv=bag();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(9,1);
     inv.slots()[0]=0; // kein ausgeruestetes Item -> EraseItem endet ohne ItemSort
     inv.erase(2);
     check(inv.slots()[0]==0&&Inventory::id(inv.slots()[2])==0&&Inventory::id(inv.slots()[3])==9,"EraseItem ohne Ausruestung darf nicht sortieren (Original: wp >= pip + pip[0])");
     StatusMenu menu;tick_menu(menu,inv);
     check(Inventory::id(inv.slots()[2])==9&&Inventory::id(inv.slots()[3])==0,"Statusbildschirm zieht die EraseItem-Luecke ohne Ausruestung nicht nach");}
    });
    // Verbrauch: ItemUse senkt die gepackte Menge auf 0 und loescht den Eintrag
    // (sub1.c ItemUse schreibt (itemid<<16)|0). Der Statusbildschirm zieht die
    // dadurch entstehende Luecke im naechsten Frame nach (sub1.c:8296).
    section("Verbrauch rutscht nach",[&]{
    {auto inv=bag();inv.slots()[2]=Inventory::pack(20,1);inv.slots()[3]=Inventory::pack(12,30);inv.slots()[4]=Inventory::pack(9,1);inv.hp=10;
     unsigned control=0;
     inv.use_item(2,nullptr,control,0,0,0,false);
     check(Inventory::id(inv.slots()[2])==0&&Inventory::id(inv.slots()[3])==12,"ItemUse darf nicht selbst sortieren (kein ItemSort-Aufruf in ItemUse/Use_00)");
     StatusMenu menu;menu.active=true;menu.sprite_on_off(inv);
     check(Inventory::id(inv.slots()[2])==12&&Inventory::id(inv.slots()[3])==9&&Inventory::id(inv.slots()[4])==0,"verbrauchter Erste-Hilfe-Spray (Item 20) zieht den Bestand nicht nach");
     check(inv.occupied()==2,"Verbrauch hinterlaesst eine Luecke im Bestand");}
    });
    // Zwei-Slot-Waffe (type & 0x100): ItemSort legt breite Eintraege vorne ab,
    // sie zaehlen doppelt in die Kapazitaet und verschieben die sakai-Grenze.
    section("Zwei-Slot-Waffe",[&]{
    {auto inv=bag();inv.slots()[2]=Inventory::pack(12,30);inv.slots()[3]=Inventory::pack(34,1);inv.slots()[4]=Inventory::pack(21,1);
     inv.sort();
     check(Inventory::id(inv.slots()[2])==34,"Zwei-Slot-Waffe nicht als erster Eintrag sortiert");
     check(Inventory::id(inv.slots()[3])==12&&Inventory::id(inv.slots()[4])==21,"Reihenfolge der schmalen Eintraege nicht erhalten (stabil)");
     check(inv.occupied()==4,"Zwei-Slot-Waffe zaehlt nicht zwei Kapazitaetseinheiten");
     check(inv.wide_boundary()==3,"sakai-Grenze ignoriert den fuehrenden Zwei-Slot-Eintrag");}
    {auto inv=bag();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(34,1);
     StatusMenu menu;tick_menu(menu,inv);
     check(Inventory::id(inv.slots()[2])==34,"per-Frame-ItemSort sortiert die Zwei-Slot-Waffe nicht nach vorne");}
    });
    // ItemBoxChange (Disassembly 0x002A53E0) ruft kein ItemSort; die Lücke schliesst
    // erst SpriteOnOff desselben Statusframes.
    section("Kiste: Lücke bis SpriteOnOff",[&]{
    {auto inv=bag();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);
     check(inv.box_change(2,0,64,0,1),"Ablage in leeren Kistenplatz abgelehnt");
     check(inv.slots()[2]==0&&Inventory::id(inv.slots()[3])==22&&Inventory::id(inv.items[64])==21,"ItemBoxChange darf nicht selbst sortieren");
     StatusMenu menu;menu.active=true;menu.sprite_on_off(inv);
     check(Inventory::id(inv.slots()[2])==22&&inv.slots()[3]==0,"SpriteOnOff schliesst die Kistenlücke nicht");}
    });
    // ItemCombination (sub1.c:5572-5986): Combi_03 schreibt pip im subcsr-2-Frame,
    // subcsr 3 wartet 4 Frames (mct_00) und schiebt parts_09b[0] in 4 Schritten um
    // mpt/4; solange ruht ItemSort (sub1.c:8294). Erst der Abschlussframe (subcsr 4)
    // sortiert, listcsr_0 = listcsr_1 (movedir 0) und ItemCommandErase beginnt.
    section("Kombination: ItemSort ruht in subcsr 3",[&]{
    {auto inv=bag();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);inv.slots()[4]=Inventory::pack(95,1);
     StatusMenu menu;begin_combine(menu,inv,2);
     menu.input(MenuInput::Right,inv);check(menu.selected==3,"Cursor erreicht Slot 3 nicht");
     const auto before=menu.cursor_parts(inv);
     check(before[0].visible&&before[1].visible&&before[0].anim==2&&before[1].anim==0&&before[0].x==92.f&&before[0].y==8.f&&before[1].x==20.f&&before[1].y==4.f,
           "parts_09b-Lage im Kombinieren weicht ab (CursorSet +4/+4, Quelle 20/4, sub1.c:2151-2158/5268-5273)");
     menu.input(MenuInput::Confirm,inv);frame(menu,inv);check(menu.message.index()==155,"Frage 155 fehlt");
     reveal(menu);while(menu.message.active()&&!menu.message.choices()){menu.input(MenuInput::Confirm,inv);reveal(menu);}
     check(menu.message.active()&&menu.message.selection==0,"Frage 155 bietet keine Ja-Auswahl");
     menu.input(MenuInput::Confirm,inv); // Ja (mes_sel 100)
     check(inv.slots()[3]==Inventory::pack(22,1),"pip darf erst im naechsten Statusframe kombiniert werden");
     menu.tick_sound=~0u;frame(menu,inv);
     check(menu.combine_subcsr()==3&&menu.tick_sound==3&&inv.slots()[2]==Inventory::pack(25,1)&&inv.slots()[3]==0&&Inventory::id(inv.slots()[4])==95,
           "subcsr-2-Frame: Combi_03 / SE 3 / subcsr 3 weicht ab (sub1.c:5899-5924)");
     const float expected[8]={92,92,92,92,75,58,41,24};
     for(int f=0;f<8;++f){frame(menu,inv);
         check(menu.combine_subcsr()==3,"subcsr 3 endet zu frueh");
         check(inv.slots()[3]==0&&Inventory::id(inv.slots()[4])==95,"ItemSort lief waehrend subcsr 3 (sub1.c:8294)");
         const auto p=menu.cursor_parts(inv);check(p[0].x==expected[f]&&p[0].y==8.f&&p[1].x==20.f,"parts_09b[0]-Bewegung mpt/4 nach 4 Wartebildern weicht ab (sub1.c:5941-5958)");}
     frame(menu,inv);
     check(!menu.combining&&menu.combine_subcsr()==0&&menu.selected==2&&!menu.commands&&menu.erase_frames==3,"Abschluss subcsr 3 -> 4 / listcsr_0 = listcsr_1 / ItemCommandErase weicht ab (sub1.c:5960-5986, 3496)");
     check(inv.slots()[2]==Inventory::pack(25,1)&&Inventory::id(inv.slots()[3])==95&&inv.slots()[4]==0,"Abschlussframe sortiert nicht nach (subcsr 4, sub1.c:8294)");
     const auto after=menu.cursor_parts(inv);check(after[0].anim==0&&!after[1].visible&&after[0].x==20.f&&after[0].y==4.f,"Cursor nach dem Kombinieren weicht ab (anim 0, parts_09b[1] aus)");}
    // movedir 1 (Klasse 1, Combi_00 umgekehrt): die Quelle parts_09b[1] wandert
    // (mct 132) zum Cursor; listcsr_0 bleibt; pip[0] sinkt erst am Ende von subcsr 3.
    {auto inv=bag();inv.slots()[2]=Inventory::pack(5,7);inv.slots()[3]=Inventory::pack(95,1);inv.slots()[4]=Inventory::pack(12,10);inv.slots()[5]=Inventory::pack(9,1);inv.slots()[0]=5;
     StatusMenu menu;begin_combine(menu,inv,4);
     menu.input(MenuInput::Up,inv);check(menu.selected==2,"Cursor erreicht die Waffe nicht");
     menu.input(MenuInput::Confirm,inv);check(menu.combine_subcsr()==2&&!menu.message.active(),"Entscheidung ohne Klang nach subcsr 2 fehlt");
     frame(menu,inv);check(menu.combine_subcsr()==3&&inv.slots()[2]==Inventory::pack(5,17)&&inv.slots()[4]==0&&inv.slots()[0]==5,"Combi_00 Klasse 1 / pip[0] vor Ablauf von subcsr 3 weicht ab");
     const float expected[8]={52,52,52,52,40,28,16,4};
     for(int f=0;f<8;++f){frame(menu,inv);const auto p=menu.cursor_parts(inv);check(p[1].y==expected[f]&&p[1].x==20.f&&p[0].x==24.f&&p[0].y==8.f,"parts_09b[1]-Bewegung (movedir 1, mct 132) weicht ab");}
     frame(menu,inv);
     check(!menu.combining&&menu.selected==2&&inv.slots()[0]==4&&Inventory::id(inv.slots()[4])==9&&inv.slots()[5]==0,"movedir 1: listcsr_0 / pip[0]-- (ok 1) / ItemSort am Ende weicht ab (sub1.c:5963-5984)");}
    });
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory sort section(s) failed.\n";return 1;}
    std::cout<<"ItemSort per status frame (also after ItemUse/ItemBoxChange), EraseItem/bhSearchBullet call sites, two-slot ordering and the ItemCombination subcsr 3 pause/mpt motion passed.\n";
    return 0;
}catch(const std::exception& e){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<e.what()<<'\n';return 1;}}
