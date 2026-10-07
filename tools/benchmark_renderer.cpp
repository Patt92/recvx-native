#include "room.h"
#include "renderer.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
// Repeated identical original-room frames isolate raster cost from input,
// simulation, asset loading and display synchronization. Never a gameplay test.
int main(int argc,char** argv){try{
    if(argc!=5&&argc!=6)throw std::runtime_error("usage: recvx-render-benchmark data RM_0050.RDX frames ss [new.bmp]");
    _Make_SinTable();recvx::RoomArchive rooms(argv[1]);auto scene=recvx::decode_scene(rooms.load(argv[2],64*1024*1024));
    const auto frames=std::stoul(argv[3]);if(!frames||frames>10000||scene.cameras.empty())throw std::runtime_error("invalid benchmark");
    recvx::SoftwareRenderer image(640,480);image.set_supersampling(std::stoi(argv[4]));
    image.draw(scene,scene.cameras[0]); // warm the shared mip cache
    const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<frames;++i)image.draw(scene,scene.cameras[0]);
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    if(argc==6)image.save_bmp(argv[5]);
    std::uint64_t hash=1469598103934665603ull;for(auto p:image.pixels()){hash^=p;hash*=1099511628211ull;}
    std::cout<<argv[2]<<": "<<frames<<" frames, "<<seconds*1000/frames<<" ms/frame, image "<<std::hex<<hash<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
