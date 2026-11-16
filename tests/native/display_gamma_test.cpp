#include "display_gamma.h"
#include "video_settings.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
 DisplayGamma g;std::vector<std::uint32_t> src{0x00000000,0x7f404040,0xffffffff},out;
 check(g.apply(src,out)==src.data(),"neutral must bypass without copying");
 for(float value:{1.7f,2.2f}){g.set(value);auto p=g.apply(src,out);check(p[0]==src[0]&&p[2]==src[2],"black/white endpoints");check((p[1]>>24)==0x7f&&(p[1]&255)>64,"midtones brighter; alpha unchanged");}
 g.set(1);check(g.apply(src,out)==src.data(),"return to exact bypass");
 auto dir=std::filesystem::temp_directory_path()/("recvx-gamma-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(std::filesystem::create_directory(dir),"unique test directory");auto file=dir/"video.ini";
 VideoSettings s;s.gamma=Gamma::Lift17;s.save(file);check(VideoSettings::read(file).gamma==Gamma::Lift17,"gamma roundtrip");
 VideoConfig c(file);c.cycle_gamma(1);check(c.settings().gamma==Gamma::Lift22,"cycle gamma");c.cycle_gamma(1);check(c.settings().gamma==Gamma::Neutral,"wrap gamma");std::filesystem::remove(file);std::filesystem::remove(dir);
 std::cout<<"display gamma PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
