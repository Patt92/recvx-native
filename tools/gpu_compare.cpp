#include "room.h"
#include "renderer.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#endif
// Host-GPU-Rasterer gegen Bandstufe auf demselben Raumbild (gpu_raster.h): Pixelvergleich und
// Zeit je Bild. Diagnose der Plattformanpassung, kein Gameplaytest. Rueckgabe 77 ohne GPU-Backend.
namespace {
double cpu_seconds(){
#ifdef _WIN32
    FILETIME c,e,k,u;if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return 0;
    auto s=[](const FILETIME& f){return double((static_cast<unsigned long long>(f.dwHighDateTime)<<32)|f.dwLowDateTime)*1e-7;};
    return s(k)+s(u);
#else
    rusage r{};getrusage(RUSAGE_SELF,&r);
    return double(r.ru_utime.tv_sec)+double(r.ru_utime.tv_usec)*1e-6+double(r.ru_stime.tv_sec)+double(r.ru_stime.tv_usec)*1e-6;
#endif
}
}
int main(int argc,char** argv){try{
    std::string backend="gpu";bool force_copy=false;
    if(argc>=2&&std::string(argv[argc-1])=="--force-copy"){force_copy=true;--argc;}
    if(argc>=3&&std::string(argv[argc-2])=="--backend"){backend=argv[argc-1];argc-=2;}
    if(backend!="gpu"&&backend!="opengl")throw std::runtime_error("backend must be gpu or opengl");
    if(argc<6||argc>8)throw std::runtime_error("usage: recvx-gpu-compare data RM_0020.RDX frames ss nearest|filtered [camera] [out-dir] [--backend gpu|opengl] [--force-copy]");
    _Make_SinTable();recvx::RoomArchive rooms(argv[1]);auto scene=recvx::decode_scene(rooms.load(argv[2],64*1024*1024));
    const auto frames=std::stoul(argv[3]);const int ss=std::stoi(argv[4]);const std::string mode=argv[5];
    const std::size_t camera=argc>=7?std::stoul(argv[6]):0;const std::string out=argc==8?argv[7]:"";
    if(!frames||frames>10000||ss<1||ss>4||camera>=scene.cameras.size()||(mode!="nearest"&&mode!="filtered"))throw std::runtime_error("invalid arguments");
    std::string reason;auto gpu=backend=="opengl"?recvx::create_opengl_raster(reason,force_copy):recvx::create_gpu_raster(reason);if(!gpu){std::cout<<"SKIP: "<<reason<<'\n';return 77;}
    recvx::SoftwareRenderer software(640,480),hardware(640,480);
    for(auto* r:{&software,&hardware}){r->set_supersampling(ss);r->set_texture_filtering(mode=="filtered");}
    hardware.set_gpu(gpu);
    software.draw(scene,scene.cameras[camera]);hardware.draw(scene,scene.cameras[camera]);
    if(!hardware.last_gpu_frame()||hardware.gpu()!=gpu)throw std::runtime_error("GPU frame fell back: "+hardware.gpu_error());
    const auto& a=software.pixels();const auto& b=hardware.pixels();const auto& da=software.depth();const auto& db=hardware.depth();
    // Farbe (RGB) und Pixelalpha getrennt; Tiefe als relative Abweichung von 1/z und als Bedeckung (> 0).
    std::size_t exact=0,d1=0,d4=0,big=0,alpha_diff=0,cover_diff=0;unsigned max_diff=0,max_alpha=0;double max_rel=0;std::vector<std::size_t> worst;
    for(std::size_t i=0;i<a.size();++i){unsigned d=0;for(unsigned shift:{0u,8u,16u})d=std::max(d,unsigned(std::abs(int((a[i]>>shift)&255)-int((b[i]>>shift)&255))));
        const unsigned ad=unsigned(std::abs(int(a[i]>>24)-int(b[i]>>24)));if(ad){++alpha_diff;max_alpha=std::max(max_alpha,ad);}
        max_diff=std::max(max_diff,d);if(!d)++exact;else if(d<=1)++d1;else if(d<=4)++d4;else{++big;if(worst.size()<8)worst.push_back(i);}
        if((da[i]>0)!=(db[i]>0))++cover_diff;else if(da[i]>0)max_rel=std::max(max_rel,std::abs(double(da[i])-double(db[i]))/double(da[i]));}
    std::cout<<gpu->name()<<" | "<<argv[2]<<" camera "<<camera+1<<" ss "<<ss<<' '<<mode<<": draws "<<gpu->draw_calls()<<'\n'
             <<"rgb "<<a.size()<<": exact "<<exact<<" ("<<100.0*double(exact)/double(a.size())<<" %), diff<=1 "<<d1<<", diff 2..4 "<<d4<<", diff>4 "<<big<<", max "<<max_diff
             <<"; alpha differs "<<alpha_diff<<" (max "<<max_alpha<<"); depth: coverage differs "<<cover_diff<<", max relative "<<max_rel<<'\n';
    for(const auto i:worst)std::cout<<"  diff>4 at "<<i%640<<','<<i/640<<": software "<<std::hex<<a[i]<<" gpu "<<b[i]<<std::dec<<" depth "<<da[i]<<'/'<<db[i]<<'\n';
    for(const auto& u:gpu->unsupported())std::cout<<"software frame: "<<u<<'\n';
    if(!out.empty()){software.save_bmp(out+"/software.bmp");hardware.save_bmp(out+"/gpu.bmp");}
    for(auto* r:{&software,&hardware}){
        const auto start=std::chrono::steady_clock::now();const double cpu0=cpu_seconds();
        for(unsigned i=0;i<frames;++i){r->draw(scene,scene.cameras[camera]);if(r==&hardware&&(!hardware.last_gpu_frame()||hardware.gpu()!=gpu))throw std::runtime_error("GPU benchmark fell back: "+hardware.gpu_error());}
        const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),cpu=cpu_seconds()-cpu0;
        std::cout<<(r==&software?"software":"gpu     ")<<": "<<wall*1000/double(frames)<<" ms/frame wall, "<<cpu*1000/double(frames)<<" ms/frame cpu\n";}
    if(mode=="nearest"&&(exact!=a.size()||alpha_diff||cover_diff||max_rel!=0))return 1;
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
