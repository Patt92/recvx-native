// message.c:365 bhDispFont: original atlas, white/default or coloured vertex tint; no luminance remap.
#include "ui.h"
#include "asset_io.h"
#include "movie.h"
#include <iostream>
#include <stdexcept>
using namespace recvx;
int main(int argc,char** argv){try{
    if(argc<2||argc>3)throw std::runtime_error("data path required");
    const auto data=std::filesystem::u8path(argv[1]);GameUi ui(data);Scene source;source.textures.resize(4);
    apply_texture_files(source,data,{},"fonts");
    for(unsigned code:{33u,29u,350u,680u,1000u})for(auto tint:{0xffffffffu,0xffff2020u,0xff20e0ffu}){
        SoftwareRenderer image(640,480);std::fill(image.canvas().begin(),image.canvas().end(),0xff213547u);
        ui.show_message(image,Message{std::uint16_t(code),0xffff},10,20,tint);
        const auto& atlas=source.textures.at(code/324);const unsigned glyph=code%324;
        const unsigned u0=(glyph%18)*14*atlas.width/256,v0=(glyph/18)*14*atlas.height/256;
        const unsigned u1=((glyph%18)*14+14)*atlas.width/256,v1=((glyph/18)*14+14)*atlas.height/256;
        for(unsigned y=0;y<28;++y)for(unsigned x=0;x<28;++x){
            const auto c=atlas.pixels[(v0+y*(v1-v0)/28)*atlas.width+u0+x*(u1-u0)/28];
            const unsigned alpha=(c>>24)*(tint>>24)/255;unsigned expected=0xff000000u;
            for(unsigned shift:{0u,8u,16u}){unsigned value=((c>>shift)&255)*((tint>>shift)&255)/255;expected|=((value*alpha+((0xff213547u>>shift)&255)*(255-alpha))/255)<<shift;}
            if(image.pixels()[(20+y)*640+10+x]!=expected)throw std::runtime_error("dialog glyph changed original atlas colour: "+std::to_string(code));
        }
    }
    // ELF 0x1334B4-0x1334D8: ts_flg 0x200 selects mode 0, otherwise mode 1.
    Inventory inventory;StatusMenu menu;MessageDialog dialog;dialog.show({33,0xff02,0,34,0xffff},0);
    for(unsigned i=0;i<20&&!dialog.turnable();++i)dialog.update();
    if(!dialog.turnable()||!dialog.arrow_drawn())throw std::runtime_error("page/arrow fixture not ready");
    for(unsigned flags:{0u,0x200u,0x1200u})for(bool movie:{false,true}){
        SoftwareRenderer actual(640,480),expected(640,480);
        std::fill(actual.canvas().begin(),actual.canvas().end(),0xff213547u);
        std::fill(expected.canvas().begin(),expected.canvas().end(),0xff213547u);
        ui.draw(actual,inventory,menu,dialog,0,flags,movie);
        const bool status=!(flags&0x200u);
        ui.show_message(expected,dialog.visible(),status?44:68,status?342:(movie&&!(flags&0x1000u)?371:374));
        ui.show_message(expected,{29,0xffff},status?212:306,status?432:434);
        if(actual.pixels()!=expected.pixels())throw std::runtime_error("dialog layout ignores ts_flg 0x200");
    }
    // message.c:303: the choice marker uses colour 0 even after a text colour command.
    for(unsigned colour:{0xfe02u,0xfe03u,0xfe04u,0xfe05u}){
        MessageDialog choice;choice.show({std::uint16_t(colour),0xff04,33,0xff04,34,0xffff},0);
        for(unsigned i=0;i<30&&!choice.choosing();++i)choice.update();
        if(!choice.choosing()||choice.cursor_drawn()!=0)throw std::runtime_error("choice fixture not ready");
        SoftwareRenderer actual(640,480),expected(640,480);
        std::fill(actual.canvas().begin(),actual.canvas().end(),0xff213547u);
        std::fill(expected.canvas().begin(),expected.canvas().end(),0xff213547u);
        ui.draw(actual,inventory,menu,choice,0,0x200u);
        ui.show_message(expected,choice.visible(),68,374);
        ui.show_message(expected,{28,0xffff},68,374); // independent white marker, coloured text unchanged
        if(actual.pixels()!=expected.pixels())throw std::runtime_error("choice marker inherited text colour");
    }
    // Real cooked FMV plus the production MoviePlayer -> GameUi composition. This is not a room-script replay.
    AudioMixer audio;MoviePlayer movie(data,{},audio);movie.start(1,0);
    for(unsigned i=0;i<90;++i)movie.tick();
    if(!movie.active()||movie.frame()==0)throw std::runtime_error("real FMV did not advance through 90 ticks");
    SoftwareRenderer actual(640,480),expected(640,480);movie.draw(actual);expected.canvas()=actual.pixels();
    ui.draw(actual,inventory,menu,dialog,0,0x200u,movie.active());
    ui.show_message(expected,dialog.visible(),68,371);ui.show_message(expected,{29,0xffff},306,434);
    if(actual.pixels()!=expected.pixels())throw std::runtime_error("real movie/dialog composition differs");
    if(argc>2)actual.save_bmp(std::filesystem::u8path(argv[2]));
    movie.stop();
    std::cout<<"Dialog atlas colours and tints preserved\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
