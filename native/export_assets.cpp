#include "asset_io.h"
#include "archive.h"
#include "room.h"
#include "actor.h"
#include "binary_view.h"
#include "item_asset.h"
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::size_t export_ui(const fs::path& output){
    auto effect=recvx::read_asset_file(output/"archives/SYSTEM/0001.bin",8*1024*1024);
    recvx::BinaryView view(effect);std::size_t p=0;
    for(unsigned i=0;i<256;++i){const auto size=view.u32(p);p+=4;if(size&0x80000000){p=(p+31)&~std::size_t(31);break;}view.require(p,size);p+=size;}
    auto fonts=recvx::decode_texture_bundle(effect,p);
    auto menu=recvx::decode_texture_bundle(recvx::read_asset_file(output/"archives/ITEM1/0145.bin",8*1024*1024));
    std::size_t count=0;
    for(const auto& bundle:{std::make_pair("fonts",&fonts),std::make_pair("menu",&menu)})
        for(std::size_t t=0;t<bundle.second->size();++t){recvx::write_tga(output/"textures"/bundle.first/(recvx::asset_number(unsigned(t))+".tga"),(*bundle.second)[t]);++count;}
    return count;
}
static std::size_t export_items(const fs::path& output){
    std::size_t count=0,models=0;
    for(unsigned id=1;id<=144;++id){const auto bytes=recvx::read_asset_file(output/"archives/ITEM1"/(recvx::asset_number(id-1)+".bin"),8*1024*1024);recvx::BinaryView v(bytes);
        if((v.u32(4)&0xffffff)!=0x4c444d)continue;
        const auto scene=recvx::decode_item_asset(bytes);++models;
        for(std::size_t t=0;t<scene.textures.size();++t){recvx::write_tga(output/"textures/items"/recvx::asset_number(id)/(recvx::asset_number(unsigned(t))+".tga"),scene.textures[t]);++count;}
    }
    std::cout<<"Decoded "<<models<<" original item models.\n";return count;
}
int main(int argc,char** argv){try{
    if(argc!=3){std::cout<<"recvx-assets <extracted-disc-root> <new-native-data-directory>\n";return argc==1?0:2;}
    if(std::string(argv[1])=="--items"){std::cout<<"Exported "<<export_items(fs::u8path(argv[2]))<<" item textures.\n";return 0;}
    if(std::string(argv[1])=="--ui"){std::cout<<"Exported "<<export_ui(fs::u8path(argv[2]))<<" UI/effect textures.\n";return 0;}
    const fs::path source=fs::u8path(argv[1]),output=fs::u8path(argv[2]);if(fs::exists(output))throw std::runtime_error("export destination already exists");
    if(!fs::is_directory(source))throw std::runtime_error("extracted disc root missing");
    recvx::RoomArchive rooms(source);_Make_SinTable();fs::create_directories(output);
    std::size_t files=0,entries=0,textures=0;
    for(const auto& entry:fs::recursive_directory_iterator(source))if(entry.is_regular_file()){
        auto extension=entry.path().extension().string();for(auto& c:extension)if(c>='a'&&c<='z')c=char(c-'a'+'A');
        if(extension==".AFS"){
            const recvx::AfsArchive archive(entry.path());auto name=entry.path().stem().string();for(auto& c:name)if(c>='a'&&c<='z')c=char(c-'a'+'A');
            for(std::size_t i=0;i<archive.entries().size();++i){recvx::write_asset_file(output/"archives"/name/(recvx::asset_number(unsigned(i))+".bin"),archive.read(i,256*1024*1024));++entries;}
        }else{const auto destination=output/"files"/fs::relative(entry.path(),source);fs::create_directories(destination.parent_path());fs::copy_file(entry.path(),destination);}
        ++files;
    }
    for(std::size_t i=0;i<rooms.size();++i){
        const auto bytes=rooms.load(i,64*1024*1024);auto name=rooms.name(i);name.replace(name.size()-3,3,"RDT");recvx::write_asset_file(output/"rooms"/name,bytes);
        const auto scene=recvx::decode_scene(bytes);
        for(std::size_t t=0;t<scene.textures.size();++t){recvx::write_tga(output/"textures/rooms"/name.substr(0,name.size()-4)/(recvx::asset_number(unsigned(t))+".tga"),scene.textures[t]);++textures;}
    }
    // The source is the extracted disc: player bodies live in SYSTEM.AFS, not as loose cooked entries.
    for(unsigned i=10;i<=14;++i){const auto player=recvx::PlayerAsset::load(source,i,true);
        for(std::size_t t=0;t<player.textures().size();++t){recvx::write_tga(output/"textures/players"/recvx::asset_number(i)/(recvx::asset_number(unsigned(t))+".tga"),player.textures()[t]);++textures;}
    }
    textures+=export_ui(output);textures+=export_items(output);
    std::ofstream complete(output/"native-data.version");complete<<"recvx-assets-1\n";complete.close();if(!complete)throw std::runtime_error("cannot finish data marker");
    std::cout<<"Exported "<<files<<" disc files, "<<entries<<" archive entries, "<<rooms.size()<<" uncompressed rooms and "<<textures<<" editable textures.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Asset export error: "<<e.what()<<'\n';return 1;}}
