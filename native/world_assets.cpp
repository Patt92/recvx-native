#include "world.h"
#include "asset_io.h"
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("usage: recvx-world-assets <cooked-data> <room-name-without-extension>");
    _Make_SinTable();const auto data=std::filesystem::u8path(argv[1]);const std::string name=argv[2];
    if(name.size()!=7||name.substr(0,3)!="RM_"||name.find_first_not_of("0123456789ABCDEF",3)!=std::string::npos)throw std::runtime_error("invalid room name");
    auto bytes=recvx::read_asset_file(data/"rooms"/(name+".RDT"),64*1024*1024);auto scene=recvx::decode_scene(bytes);recvx::SceneSpawn actor{};std::size_t motion=42;std::int64_t frame=0;recvx::AudioMixer audio;
    recvx::RoomWorld world(bytes,scene,actor,motion,frame,audio);std::cout<<"Exported "<<world.export_textures(data,name)<<" room actor/object/item textures.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
