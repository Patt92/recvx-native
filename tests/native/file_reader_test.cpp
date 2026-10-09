#include "ui.h"
#include "asset_io.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static void settle(StatusMenu& menu,Inventory& inv){for(unsigned i=0;i<100;++i)menu.tick(inv);}
// D03: Dateiordner, Registerauswahl, Leserübergänge und die Seitentabellen
// liegen wieder eigenständig nativ vor (native/file_folder.cpp,
// native/inspection.cpp). Diese Abdeckung ist die Fassung von vor dem Rückbau.
int main(int argc,char** argv){try{
    _Make_SinTable();Inventory inv;StatusMenu menu;menu.acquire_file(inv,17);settle(menu,inv);
    check(menu.files&&!menu.file_list&&(inv.items[383]&(1u<<17)),"acquired memo is not a file");
    menu.input(MenuInput::Confirm,inv);check(menu.files&&menu.file_page==0,"decide advanced a page or closed without EXIT");
    while(menu.file_page+1<original_file_pages(17)){menu.input(MenuInput::Right,inv);settle(menu,inv);}
    menu.input(MenuInput::Confirm,inv);check(menu.files&&!menu.file_exit_selected,"last page exits without selecting EXIT");
    menu.input(MenuInput::Right,inv);check(menu.file_exit_selected,"right on last page does not select EXIT");
    menu.input(MenuInput::Left,inv);check(!menu.file_exit_selected&&menu.file_page+1==original_file_pages(17),"left from EXIT skipped a page");
    settle(menu,inv);menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);settle(menu,inv);check(!menu.active,"acquisition cannot return to gameplay");
    auto enter_folder=[&]{menu=StatusMenu{};menu.input(MenuInput::Open,inv);settle(menu,inv);menu.nav=true;menu.maincsr=1;menu.input(MenuInput::Confirm,inv);settle(menu,inv);check(menu.files&&menu.file_list&&!menu.file_folder.registers(),"FILE does not enter folder selection");};
    inv.items[383]=1u<<17;enter_folder();check(inv.items[383]&1,"MakeTag does not make the manual available");
    menu.input(MenuInput::Left,inv);settle(menu,inv);check(menu.file_folder.folder()==1,"left does not rotate to the next original folder");
    menu.input(MenuInput::Confirm,inv);settle(menu,inv);check(!menu.file_folder.registers(),"empty folder opened");
    menu.input(MenuInput::Left,inv);settle(menu,inv);check(menu.file_folder.folder()==2,"second folder rotation failed");
    menu.input(MenuInput::Confirm,inv);settle(menu,inv);check(menu.file_list&&menu.file_folder.registers()&&menu.file_number==17,"confirm does not open the folder before the file");
    menu.input(MenuInput::Down,inv);check(menu.file_number==17,"register cursor selects an unowned file");
    menu.input(MenuInput::Cancel,inv);settle(menu,inv);check(!menu.file_folder.registers()&&menu.files,"register cancel does not close the folder");
    menu.input(MenuInput::Confirm,inv);settle(menu,inv);menu.input(MenuInput::Confirm,inv);check(menu.file_transition,"file does not slide out the inventory");settle(menu,inv);check(menu.files&&!menu.file_list&&!menu.file_scroll,"folder transition cannot finish");
    menu.input(MenuInput::Cancel,inv);settle(menu,inv);check(!menu.files&&menu.active&&menu.nav&&menu.maincsr==1,"FileExit01 does not return to MainCommand FILE");
    menu.nav=false;menu.input(MenuInput::Cancel,inv);check(menu.nav&&menu.maincsr==3,"CursorMove back differs from original ITEM navigation");
    menu.input(MenuInput::Cancel,inv);settle(menu,inv);check(!menu.active,"MainCommand back does not close status");
    inv.slots()[2]=Inventory::pack(145,1);menu=StatusMenu{};menu.input(MenuInput::Open,inv);settle(menu,inv);menu.selected=2;menu.input(MenuInput::Confirm,inv);for(unsigned i=0;i<4;++i)menu.tick(inv);menu.input(MenuInput::Confirm,inv);settle(menu,inv);check(menu.files&&!menu.file_list&&menu.file_number==0,"manual item action does not run the original automatic folder opening");
    std::array<std::uint32_t,32> flags{};check(file_page_reduction(flags)==7,"initial manual page count differs");
    for(auto pair:{std::pair<unsigned,unsigned>{0x96,6},{0xE0,4},{0x129,2},{0x2B,1},{0x199,0}}){flags[pair.first/32]|=0x80000000u>>(pair.first%32);check(file_page_reduction(flags)==pair.second,"original manual progression priority failed");}
    if(argc>1){
        GameUi ui(std::filesystem::u8path(argv[1]));MessageDialog dialog;const auto out=argc>2?std::filesystem::u8path(argv[2]):std::filesystem::path{};if(!out.empty())std::filesystem::create_directories(out);
        for(unsigned file:{0u,2u,17u}){menu=StatusMenu{};menu.acquire_file(inv,file);settle(menu,inv);SoftwareRenderer image(640,480);ui.draw(image,inv,menu,dialog,0);if(!out.empty())image.save_bmp(out/("file-"+std::to_string(file)+".bmp"));check(image.pixels()[0]==0xff000000,"reader still draws the status dialog");if(original_file_pages(file)>1){menu.input(MenuInput::Right,inv);settle(menu,inv);ui.draw(image,inv,menu,dialog,0);if(!out.empty())image.save_bmp(out/("file-"+std::to_string(file)+"-body.bmp"));}}
        inv.items[383]=0xffffff;enter_folder();SoftwareRenderer folder_frame(640,480);ui.draw(folder_frame,inv,menu,dialog,0);if(!out.empty())folder_frame.save_bmp(out/"folder-closed.bmp");
        menu.input(MenuInput::Confirm,inv);for(unsigned i=0;i<14;++i){menu.tick(inv);ui.draw(folder_frame,inv,menu,dialog,i);if(!out.empty()&&(i==2||i==8||i==13))folder_frame.save_bmp(out/("folder-open-"+std::to_string(i)+".bmp"));}settle(menu,inv);check(menu.file_folder.registers(),"folder remains locked after opening");
        const auto packet=read_asset_file(std::filesystem::u8path(argv[1])/"archives/ITEM1/0138.bin",8*1024*1024);const auto mesh=menu.file_folder.scene(packet,inv);check(mesh.triangles.size()>=16,"original owned register geometry is absent");unsigned selected_tags=0;for(unsigned i=unsigned(mesh.triangles.size())-16;i<mesh.triangles.size();++i){const auto& t=mesh.triangles[i];check(!t.textured&&(t.color==0xff808080||t.color==0xffc0c0c0),"register colors differ from MakeTag");selected_tags+=t.color==0xffc0c0c0;}check(selected_tags==2,"selected register is not highlighted");
        menu.input(MenuInput::Down,inv);check(menu.file_number==1,"owned register navigation failed");ui.draw(folder_frame,inv,menu,dialog,0);if(!out.empty())folder_frame.save_bmp(out/"folder-register-1.bmp");
        menu.input(MenuInput::Confirm,inv);for(unsigned i=0;i<30;++i){menu.tick(inv);ui.draw(folder_frame,inv,menu,dialog,i);if(!out.empty()&&(i==3||i==9||i==15||i==23))folder_frame.save_bmp(out/("folder-to-reader-"+std::to_string(i)+".bmp"));}settle(menu,inv);check(!menu.file_list,"reader transition remains blocked");
        for(unsigned id:{5u,10u}){std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(id,18);inv.slots()[0]=2;menu=StatusMenu{};menu.input(MenuInput::Open,inv);settle(menu,inv);SoftwareRenderer equipped(640,480),empty(640,480);ui.draw(equipped,inv,menu,dialog,0);inv.slots()[0]=0;ui.draw(empty,inv,menu,dialog,0);int lo=640,hi=-1;for(int y=48;y<96;++y)for(int x=350;x<520;++x)if(equipped.pixels()[y*640+x]!=empty.pixels()[y*640+x]){lo=std::min(lo,x);hi=std::max(hi,x);}const int x=int(menu.slide.x(3))+((itemdata[id].type&0x100)?24:58),w=(itemdata[id].type&0x100)?136:68;check(lo>=x&&hi<x+w&&hi>=lo,"equipped icon is not in original ArmsSet bounds");if(!out.empty())equipped.save_bmp(out/("equip-"+std::to_string(id)+".bmp"));}
    }
    std::cout<<"original folders, registers, manual progression, reader controls and equipment placement passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
