#include "world.h"
#include "asset_io.h"
#include "archive.h"
#include "weapon_asset.h"
#include "ui.h"
#include "binary_view.h"
#include <iostream>

// Offline catalogue only: reuse the native asset owners/decoders and their
// runtime identities. No simulation, replacement renderer or second work world.
static std::size_t export_catalog(const std::filesystem::path& data,const std::filesystem::path& output){
    std::size_t count=0;
    const auto save=[&](const auto& textures,const std::filesystem::path& identity){
        for(unsigned i=0;i<textures.size();++i){recvx::write_tga(output/"textures"/identity/(recvx::asset_number(i)+".tga"),textures[i]);++count;}
    };
    for(unsigned player=0;player<2;++player)for(unsigned weapon=0;weapon<=20;++weapon){
        const auto asset=recvx::WeaponAsset::load(data,player,weapon);
        const auto identity=std::filesystem::path("weapons")/recvx::asset_number(20+player*30+weapon);
        save(asset.right.textures,identity/"right");save(asset.left.textures,identity/"left");
    }
    for(const auto& record:std::filesystem::directory_iterator(data/"archives/MRY")){
        const auto stem=record.path().stem().string();
        if(record.path().extension()!=".bin"||stem.size()!=4||stem.find_first_not_of("0123456789")!=std::string::npos)continue;
        const unsigned index=unsigned(std::stoul(stem));if(index<71||index>=71+255)continue;
        // DoorSequence::begin: MPAC's first two length-prefixed blocks are
        // model and texture list, starting at the offset relative to word 4.
        const auto bytes=recvx::read_asset_file(record.path(),8*1024*1024);
        recvx::BinaryView view(bytes);if(view.u32(0)!=0x4341504d)continue;
        auto p=view.relative(4,view.u32(4),4);const auto model=p+4;
        view.require(model,view.u32(p));p=model+view.u32(p);const auto texture=p+4;view.require(texture,view.u32(p));
        const auto scene=recvx::decode_model_asset(bytes,model,texture);save(scene.textures,std::filesystem::path("doors")/recvx::asset_number(index-71));
    }
    recvx::GameUi ui(data);
    for(unsigned i=0;i<3;++i){recvx::write_tga(output/"textures/title"/(recvx::asset_number(i)+".tga"),ui.title_texture(i));++count;}
    save(recvx::decode_texture_bundle(recvx::read_asset_file(data/"archives/ITEM1/0144.bin",8*1024*1024)),"typewriter");
    return count;
}
static std::size_t export_effect_catalog(const std::filesystem::path& data,const std::filesystem::path& output){
    std::size_t count=0;
    for(const auto& file:std::filesystem::directory_iterator(data/"rooms")){
        if(file.path().extension()!=".RDT")continue;
        const auto name=file.path().stem().string();auto bytes=recvx::read_asset_file(file.path(),64*1024*1024);
        const auto scene=recvx::decode_scene(bytes);recvx::SceneSpawn actor{};std::size_t motion=42;std::int64_t frame=0;recvx::AudioMixer audio;
        // Asset initialization only; no frame/event/enemy simulation. No mods.
        recvx::RoomWorld world(bytes,scene,actor,motion,frame,audio);
        for(unsigned group=0;group<512;++group)if(const auto* textures=world.effect_group_textures(group))
            for(unsigned i=0;i<textures->size();++i){recvx::write_tga(output/"textures/effects"/name/recvx::asset_number(group)/(recvx::asset_number(i)+".tga"),textures->at(i));++count;}
    }
    return count;
}
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--help"){
        std::cout<<"recvx-world-assets <cooked-data> <RM_xxxx>\n"
                    "recvx-world-assets --catalog <cooked-data> <new-output-directory>\n"
                    "recvx-world-assets --effect-catalog <cooked-data> <new-output-directory>\n";return 0;
    }
    if(argc==4&&(std::string(argv[1])=="--catalog"||std::string(argv[1])=="--effect-catalog")){
        const auto output=std::filesystem::u8path(argv[3]);
        if(std::filesystem::exists(output)&&(!std::filesystem::is_directory(output)||!std::filesystem::is_empty(output)))
            throw std::runtime_error("catalogue output must be a new or empty directory");
    }
    if(argc==4&&std::string(argv[1])=="--effect-catalog"){
        _Make_SinTable();const auto count=export_effect_catalog(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]));
        std::cout<<"Exported "<<count<<" native room effect catalogue textures.\n";return 0;
    }
    if(argc==4&&std::string(argv[1])=="--catalog"){
        _Make_SinTable();const auto count=export_catalog(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]));
        std::cout<<"Exported "<<count<<" native catalogue textures.\n";return 0;
    }
    if(argc!=3)throw std::runtime_error("usage: recvx-world-assets <cooked-data> <room-name-without-extension>");
    _Make_SinTable();const auto data=std::filesystem::u8path(argv[1]);const std::string name=argv[2];
    if(name.size()!=7||name.substr(0,3)!="RM_"||name.find_first_not_of("0123456789ABCDEF",3)!=std::string::npos)throw std::runtime_error("invalid room name");
    auto bytes=recvx::read_asset_file(data/"rooms"/(name+".RDT"),64*1024*1024);auto scene=recvx::decode_scene(bytes);recvx::SceneSpawn actor{};std::size_t motion=42;std::int64_t frame=0;recvx::AudioMixer audio;
    recvx::RoomWorld world(bytes,scene,actor,motion,frame,audio);std::cout<<"Exported "<<world.export_textures(data,name)<<" room actor/object/item textures.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
