#include "asset_io.h"
#include "computer.h"
#include "screen_fade.h"
#include "ui.h"
#include "room.h"
#include "md5.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace recvx;
namespace fs=std::filesystem;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static SceneTexture solid(unsigned w,unsigned h,std::uint32_t pixel){SceneTexture t;t.width=w;t.height=h;t.pixels.assign(std::size_t(w)*h,pixel);return t;}
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("needs original data path");
    _Make_SinTable();const fs::path data=argv[1];
    const auto root=fs::temp_directory_path()/("recvx-bundle-mod-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path path;~Cleanup(){set_texture_mods_enabled(true);std::error_code error;fs::remove_all(path,error);}} cleanup{root};
    const auto mods=root/"mods";fs::create_directories(mods/"textures/hd");
    GameUi ui(data,mods);
    Inventory event_inventory;MessageDialog event_dialog;
    const auto room=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);EventVm events(room,event_inventory,event_dialog);
    events.com_number=3;events.flags[10][0]|=0x400000;
    ComputerTerminal computer(data,mods);check(computer.begin(events),"terminal cannot begin");
    for(unsigned i=0;i<8&&!computer.running();++i){control_screen_fade(events.flags[10][0]);computer.monitor(events);} // mn_mode0 7 (system.c:2118-2147)
    for(unsigned i=0;i<12;++i)computer.tick(0,0,events,nullptr);
    auto terminal_image=[&]{SoftwareRenderer image(640,480);computer.draw(image,ui,events);auto pixels=image.pixels();
        // syRtcGetDate is intentionally live. Ignore ONLY the displayed clock
        // rectangle when comparing texture reloads across a wall-clock second.
        for(unsigned y=295;y<330;++y)for(unsigned x=360;x<510;++x)pixels[y*640+x]=0;
        return pixels;};
    const auto original=terminal_image();const auto original_stats=texture_file_stats();
    // Reload after a toggle without resetting the terminal automaton.
    write_tga(mods/"textures/computer/0000.tga",solid(1024,1024,0xff00ffff));
    set_texture_mods_enabled(false);check(terminal_image()==original,"mods off changes decoded terminal");
    set_texture_mods_enabled(true);const auto changed=terminal_image();
    check(changed!=original,"terminal does not draw the replacement background");
    check(texture_file_stats().mod>original_stats.mod,"terminal bypassed shared mod loader");
    check(computer.active(),"texture reload changed terminal state");
    set_texture_mods_enabled(false);check(terminal_image()==original,"terminal toggle does not restore original");
    set_texture_mods_enabled(true);
    // Read the same acquired file through its actual UI owner. Override the
    // original archive identity, not the inventory file number.
    Inventory inventory;StatusMenu menu;menu.acquire_file(inventory,17);
    for(unsigned i=0;i<100;++i)menu.tick(inventory);
    MessageDialog dialog;
    auto file_image=[&]{SoftwareRenderer image(640,480);ui.draw(image,inventory,menu,dialog,0);return image.pixels();};
    const auto file_original=file_image();
    const auto wallpaper=asset_number(original_file_wallpaper(17));
    write_tga(mods/"textures/files"/wallpaper/"0000.tga",solid(512,512,0xffff00ff));ui.reload_textures();
    check(file_image()!=file_original,"file reader bypasses texture overrides");
    set_texture_mods_enabled(false);ui.reload_textures();check(file_image()==file_original,"file reload does not restore original");
    // Per-bank mods affect quantities only through the selected original bank.
    set_texture_mods_enabled(true);ui.reload_textures();
    auto quantity_image=[&](unsigned flags){Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(6|flags,15);
        StatusMenu status;status.message_source=[](unsigned){return Message{'T'-32,0xffff};};status.input(MenuInput::Open,inv);status.skip_slide();status.tick(inv);
        SoftwareRenderer image(640,480);ui.draw(image,inv,status,dialog,0);return image.pixels();};
    const auto bank9=quantity_image(0x1000),bank6=quantity_image(0x2000);
    write_tga(mods/"textures/palettes/menu/0000/0009/0000.tga",solid(512,512,0xff00ffff));ui.reload_textures();
    check(quantity_image(0x1000)!=bank9,"quantity ignores explicit bank override");
    check(quantity_image(0x2000)==bank6,"bank override leaked into another ammunition type");
    // A canonical decoded key reaches the same terminal, not only a diagnostic
    // explicit-path texture. Removing a test-created file leaves originals intact.
    fs::remove(mods/"textures/computer/0000.tga");
    const auto original_bundle=decode_texture_bundle(read_asset_file(data/"archives/SYSTEM/0002.bin",8*1024*1024));
    const auto key=md5_hex(encode_tga(original_bundle.at(0)));
    write_tga(mods/"textures/hd"/(key+".tga"),solid(1024,1024,0xffff00ff));
    set_texture_mods_enabled(false);terminal_image();set_texture_mods_enabled(true);
    check(terminal_image()!=original,"decoded canonical hash does not reach terminal");
    // Reset/re-entry must retain normal script lifecycle and the same resolver.
    computer.reset();events.flags[10][0]|=0x400000;check(computer.begin(events),"terminal cannot re-enter after reset");
    for(unsigned i=0;i<8&&!computer.running();++i){control_screen_fade(events.flags[10][0]);computer.monitor(events);}
    for(unsigned i=0;i<12;++i)computer.tick(0,0,events,nullptr);
    const auto reentry=terminal_image();set_texture_mods_enabled(false);
    check(terminal_image()!=reentry,"terminal re-entry lost mod/original distinction");
    if(argc>2){const fs::path output=argv[2];fs::create_directories(output);SoftwareRenderer image(640,480);
        set_texture_mods_enabled(false);computer.draw(image,ui,events);image.save_bmp(output/"computer-original.bmp");
        set_texture_mods_enabled(true);computer.draw(image,ui,events);image.save_bmp(output/"computer-test-mod.bmp");
        if(argc>3){ComputerTerminal hd_terminal(data,fs::path(argv[3]));
            hd_terminal.character_scene=[](unsigned,unsigned,unsigned)->std::optional<Scene>{return {};};
            events.flags[10][0]|=0x400000;check(hd_terminal.begin(events),"HD terminal begin");
            for(unsigned i=0;i<12;++i)hd_terminal.tick(0,0,events,nullptr);
            hd_terminal.draw(image,ui,events);image.save_bmp(output/"computer-gcd-mod.bmp");}
    }
    std::cout<<"Terminal/file overrides, live mod toggle, original bank selection and isolated quantity bank replacement passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
