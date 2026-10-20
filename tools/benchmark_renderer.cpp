#include "room.h"
#include "renderer.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#endif
// Repeated identical original-room frames isolate raster cost from input,
// simulation, asset loading and display synchronization. Never a gameplay test.
// Reports wall time and process CPU time (all worker threads) per frame: the
// CPU/wall ratio is the energy question, wall time alone hides busy workers.
namespace {
double cpu_seconds(){
#ifdef _WIN32
    FILETIME c,e,k,u;if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return 0;
    auto s=[](const FILETIME& f){return double((unsigned long long(f.dwHighDateTime)<<32)|f.dwLowDateTime)*1e-7;};
    return s(k)+s(u);
#else
    rusage r{};getrusage(RUSAGE_SELF,&r);
    return double(r.ru_utime.tv_sec)+double(r.ru_utime.tv_usec)*1e-6+double(r.ru_stime.tv_sec)+double(r.ru_stime.tv_usec)*1e-6;
#endif
}
}
int main(int argc,char** argv){try{
    if(argc<5||argc>7)throw std::runtime_error("usage: recvx-render-benchmark data RM_0050.RDX frames ss [threads|auto] [new.bmp]");
    _Make_SinTable();recvx::RoomArchive rooms(argv[1]);auto scene=recvx::decode_scene(rooms.load(argv[2],64*1024*1024));
    const auto frames=std::stoul(argv[3]);if(!frames||frames>10000||scene.cameras.empty())throw std::runtime_error("invalid benchmark");
    recvx::SoftwareRenderer image(640,480);image.set_supersampling(std::stoi(argv[4]));
    const std::string threads=argc>=6?argv[5]:"auto";if(threads!="auto")image.set_render_threads(unsigned(std::stoul(threads)));
    image.draw(scene,scene.cameras[0]); // warm the shared mip cache
    const auto start=std::chrono::steady_clock::now();const double cpu_start=cpu_seconds();
    for(unsigned i=0;i<frames;++i)image.draw(scene,scene.cameras[0]);
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();const double cpu=cpu_seconds()-cpu_start;
    if(argc==7)image.save_bmp(argv[6]);
    std::uint64_t hash=1469598103934665603ull;for(auto p:image.pixels()){hash^=p;hash*=1099511628211ull;}
    std::cout<<argv[2]<<": "<<frames<<" frames, ss "<<argv[4]<<", threads "<<threads<<", "<<seconds*1000/frames<<" ms/frame wall, "<<cpu*1000/frames<<" ms/frame cpu, ratio "<<(seconds>0?cpu/seconds:0)<<", image "<<std::hex<<hash<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
