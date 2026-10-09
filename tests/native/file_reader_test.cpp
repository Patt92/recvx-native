#include "ui.h"
#include "asset_io.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static void settle(StatusMenu& menu,Inventory& inv){for(unsigned i=0;i<100;++i)menu.tick(inv);}
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test forderte zuvor die
// Originalroutinen des Dateiordners (SearchTag, SelectFile, SelectTag, FileSyu,
// PlayPageCheck) und die Dateitabellen (fstbl/fsheader/wallpaper) ein. Beide
// Sätze kamen aus file_folder.inc bzw. inventory_engine.inc, erzeugt von
// Generatoren, die in archived/FORBIDDEN_DONTUSE/ liegen; die Lücken sind in
// native/file_folder.cpp und native/inspection.cpp dokumentiert.
//
// Entfallene Zusicherungen und ihr Grund:
//  - Seitenwechsel, EXIT-Auswahl und das Verlassen des Lesers über EXIT: ohne
//    original_file_pages gibt es keine Seitenzahl und damit keine letzte Seite.
//  - Ordnerdrehung, Registercursor und der Übergang in den Leser: FileFolder::
//    step() bewegt nichts mehr (ang00 bleibt 0, kein Moduswechsel).
//  - Die Seitenreduktion des Handbuchs: file_page_reduction meldet 0.
//  - Das Zeichnen des Lesers: original_file_wallpaper/-message melden bewusst
//    einen Fehler, statt einen falschen Originalindex zu liefern.
//
// Geprüft bleibt der eigenständige Host-Anteil: Dateiaufnahme, der Einstieg in
// die Ordnerauswahl über MainCommand FILE, die MakeTag-Verfügbarkeit, das
// Verlassen des Lesers per Abbrechen, die CursorMove-Rücknavigation, die
// Registergeometrie aus den Originaldaten und die Ausrüstungsgeometrie — dazu,
// dass jede Lücke ihren dokumentierten Neutralwert meldet.
int main(int argc,char** argv){try{
    _Make_SinTable();Inventory inv;StatusMenu menu;menu.acquire_file(inv,17);settle(menu,inv);
    check(menu.files&&!menu.file_list&&(inv.items[383]&(1u<<17)),"acquired memo is not a file");
    menu.input(MenuInput::Confirm,inv);check(menu.files&&menu.file_page==0,"decide advanced a page or closed without EXIT");
    // Lücke fstbl: keine Seitenzahl, also keine letzte Seite und kein EXIT.
    check(original_file_pages(17)==0&&menu.file_pages()==0,"file page gap reports a page count");
    menu.input(MenuInput::Right,inv);settle(menu,inv);check(!menu.file_exit_selected&&!menu.file_page&&!menu.file_scroll,"page gap scrolled forward or selected EXIT");
    menu.input(MenuInput::Left,inv);settle(menu,inv);check(!menu.file_exit_selected&&!menu.file_page,"page gap scrolled backwards");
    menu.input(MenuInput::Confirm,inv);check(menu.files&&menu.active,"reader left without the original EXIT button");
    // Abbrechen bleibt der erhaltene Host-Weg aus dem Leser (leave_reader).
    menu.input(MenuInput::Cancel,inv);settle(menu,inv);check(!menu.files&&!menu.active,"acquisition cannot return to gameplay");
    auto enter_folder=[&]{menu=StatusMenu{};menu.input(MenuInput::Open,inv);settle(menu,inv);menu.nav=true;menu.maincsr=1;menu.input(MenuInput::Confirm,inv);settle(menu,inv);check(menu.files&&menu.file_list&&!menu.file_folder.registers(),"FILE does not enter folder selection");};
    inv.items[383]=1u<<17;enter_folder();check(inv.items[383]&1,"MakeTag does not make the manual available");
    // Lücke FileSyu/SelectFile/SelectTag: keine Drehung, kein Register, kein Leser.
    check(menu.file_folder.folder()==0&&!menu.file_folder.moving(),"folder gap starts rotated or moving");
    for(const auto key:{MenuInput::Left,MenuInput::Right,MenuInput::Up,MenuInput::Down}){menu.input(key,inv);settle(menu,inv);
        check(menu.file_folder.folder()==0&&menu.file_number==0,"folder gap rotated a folder or moved the register cursor");}
    menu.input(MenuInput::Confirm,inv);settle(menu,inv);check(!menu.file_folder.registers()&&!menu.file_transition&&menu.file_list,"folder gap opened a folder or entered the reader");
    menu.input(MenuInput::Cancel,inv);settle(menu,inv);check(menu.files&&menu.file_list,"folder gap left the folder selection");
    // CursorMove-Rücknavigation des Statusbildschirms bleibt Host-Code; sie war
    // vorher nur über das Verlassen des Ordners erreichbar.
    {Inventory back;StatusMenu status;status.input(MenuInput::Open,back);settle(status,back);
     status.input(MenuInput::Cancel,back);check(status.nav&&status.maincsr==3,"CursorMove back differs from original ITEM navigation");
     status.input(MenuInput::Cancel,back);settle(status,back);check(!status.active,"MainCommand back does not close status");}
    // Das Handbuchitem öffnet weiter die Ordneransicht; die automatische
    // Weiterschaltung in den Leser fehlt mit FileFolder::step.
    {Inventory manual;std::fill(manual.slots()+2,manual.slots()+12,0);manual.slots()[2]=Inventory::pack(145,1);StatusMenu status;status.input(MenuInput::Open,manual);settle(status,manual);status.selected=2;status.input(MenuInput::Confirm,manual);for(unsigned i=0;i<4;++i)status.tick(manual);status.input(MenuInput::Confirm,manual);settle(status,manual);
     check(status.files&&status.file_number==0,"manual item action does not open the original file view");
     check(status.file_list,"folder gap advanced into the reader without FileFolder::step");}
    // Lücke PlayPageCheck: keine Seitenzahl, unabhängig vom Fortschritt.
    std::array<std::uint32_t,32> flags{};check(file_page_reduction(flags)==0,"manual page gap reports a page count");
    for(const unsigned bit:{0x96u,0xE0u,0x129u,0x2Bu,0x199u}){flags[bit/32]|=0x80000000u>>(bit%32);check(file_page_reduction(flags)==0,"manual page gap reacts to progress flags");}
    if(argc>1){
        GameUi ui(std::filesystem::u8path(argv[1]));MessageDialog dialog;const auto out=argc>2?std::filesystem::u8path(argv[2]):std::filesystem::path{};if(!out.empty())std::filesystem::create_directories(out);
        // Lücke fstbl/wallpaper: der Leser ist nicht darstellbar. Statt eines
        // falschen Originalindex meldet das Zeichnen einen Fehler.
        for(const unsigned file:{0u,2u,17u}){menu=StatusMenu{};menu.acquire_file(inv,file);settle(menu,inv);SoftwareRenderer image(640,480);bool reported=false;
            try{ui.draw(image,inv,menu,dialog,0);}catch(const std::runtime_error&){reported=true;}
            check(reported,"file reader gap draws a guessed page or wallpaper");}
        // Die Ordnergeometrie kommt weiter aus den Originaldaten (scene()).
        inv.items[383]=0xffffff;enter_folder();SoftwareRenderer folder_frame(640,480);ui.draw(folder_frame,inv,menu,dialog,0);if(!out.empty())folder_frame.save_bmp(out/"folder-closed.bmp");
        // Vorher prüfte diese Stelle nur die 16 Dreiecke des einen geöffneten
        // Ordners. Ohne FileSyu/SelectFile bleibt afsmode 0x20 ungesetzt, also
        // zeichnet scene() alle drei Ordner in Ruhelage: 24 eigene Register =
        // 48 Dreiecke. Geprüft bleibt, dass die Register aus den Originaldaten
        // kommen, die MakeTag-Farben stimmen und genau das von file() gewählte
        // Register hervorgehoben ist.
        const auto packet=read_asset_file(std::filesystem::u8path(argv[1])/"archives/ITEM1/0138.bin",8*1024*1024);const auto mesh=menu.file_folder.scene(packet,inv);check(mesh.triangles.size()>=48,"original owned register geometry is absent");unsigned selected_tags=0;for(unsigned i=unsigned(mesh.triangles.size())-48;i<mesh.triangles.size();++i){const auto& t=mesh.triangles[i];check(!t.textured&&(t.color==0xff808080||t.color==0xffc0c0c0),"register colors differ from MakeTag");selected_tags+=t.color==0xffc0c0c0;}check(selected_tags==2,"selected register is not highlighted");
        for(unsigned id:{5u,10u}){std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(id,18);inv.slots()[0]=2;menu=StatusMenu{};menu.input(MenuInput::Open,inv);settle(menu,inv);SoftwareRenderer equipped(640,480),empty(640,480);ui.draw(equipped,inv,menu,dialog,0);inv.slots()[0]=0;ui.draw(empty,inv,menu,dialog,0);int lo=640,hi=-1;for(int y=48;y<96;++y)for(int x=350;x<520;++x)if(equipped.pixels()[y*640+x]!=empty.pixels()[y*640+x]){lo=std::min(lo,x);hi=std::max(hi,x);}const int x=int(menu.slide.x(3))+((itemdata[id].type&0x100)?24:58),w=(itemdata[id].type&0x100)?136:68;check(lo>=x&&hi<x+w&&hi>=lo,"equipped icon is not in original ArmsSet bounds");if(!out.empty())equipped.save_bmp(out/("equip-"+std::to_string(id)+".bmp"));}
    }
    std::cout<<"File acquisition, folder entry, reader cancel, status navigation, register geometry and equipment placement passed; page tables, folder stepping and reader drawing are documented gaps.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
