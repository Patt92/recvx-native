#include "chunk_blend.h"
#include <utility>
#include <algorithm>
#include <stdexcept>
namespace recvx {
#include "chunk_blend_data.inc"
namespace {
// Ps2_nj_save_current.mode_bk[0..1]/set_last and Ps2_gs_save.mode_bk[0..1] with
// its ALPHA register. ps2_dummy.c:188-189 Ps2Init leaves (8,6) selected, the
// first njColorBlendingModeSys(8,6) gives alpha_tbl[8][6] = 0x44.
int nj_mode_bk[2]={8,6};
int nj_set_last=1;
int gs_mode_bk[2]={8,6};
std::uint64_t gs_alpha=0x44;
std::vector<std::string> unavailable;
void diagnose(int source,int destination){
    const auto entry=std::to_string(source)+"/"+std::to_string(destination);
    for(const auto& known:unavailable)if(known==entry)return;
    unavailable.push_back(entry);
}
}
void color_blending_mode(int target,int mode){
    if(target!=0&&target!=1)throw std::runtime_error("njColorBlendingMode target outside Ps2_nj_save_current.mode_bk");
    nj_mode_bk[target]=mode;
    nj_set_last=target;
    color_blending_mode_sys(nj_mode_bk[0],nj_mode_bk[1]);
}
void color_blending_mode_sys(int source,int destination){
    gs_mode_bk[0]=source;
    gs_mode_bk[1]=destination;
    // The select_Last field stays where the last njColorBlendingMode put it, so
    // the branch tests nj_save (ps2_NaSystem.c:361-379).
    if(gs_mode_bk[nj_set_last]<2){
        gs_alpha=gs_mode_bk[nj_set_last]==0?1u:68u;
        return;
    }
    if(gs_mode_bk[0]<2||gs_mode_bk[1]<2)return; // the register keeps its value
    if(source>12||destination>11||alpha_tbl[source][destination]==0xffffffffull){diagnose(source,destination);return;}
    gs_alpha=alpha_tbl[source][destination];
}
std::uint64_t color_blending_alpha(){return gs_alpha;}
std::uint64_t ninja_blend_register(int source,int destination){
    if(source<2||destination<2||source>12||destination>11||alpha_tbl[source][destination]==0xffffffffull)throw std::runtime_error("njColorBlendingMode pair without an ALPHA register");
    return alpha_tbl[source][destination];
}
const std::vector<std::string>& color_blending_unavailable(){return unavailable;}
std::pair<int,int> chunk_blend_modes(unsigned source,unsigned destination){
    if(source>7||destination>7)throw std::runtime_error("chunk blend index outside Ninja table");
    return {int(lNaCnkSrcAlphaMode[source]),int(lNaCnkDstAlphaMode[destination])};
}
std::uint64_t chunk_blend_register(unsigned source,unsigned destination){
    if(source>7||destination>7)throw std::runtime_error("chunk blend index outside Ninja table");
    const auto mode=alpha_tbl[lNaCnkSrcAlphaMode[source]][lNaCnkDstAlphaMode[destination]];
    if(mode==0xffffffffull)throw std::runtime_error("unsupported original Ninja chunk blend pair");
    return mode;
}
std::uint32_t blend_gs(std::uint64_t mode,std::uint32_t source,std::uint32_t destination,float source_alpha){
    const unsigned a=mode&3,b=(mode>>2)&3,c=(mode>>4)&3,d=(mode>>6)&3;
    const float alpha=std::clamp(source_alpha,0.f,1.f);
    const float factor=c==0?alpha:c==1?float(destination>>24)/255.f:float((mode>>32)&255)/128.f;
    std::uint32_t result=std::uint32_t(alpha*255.f+.5f)<<24;
    for(unsigned shift:{0u,8u,16u}){
        auto color=[&](unsigned selector){return selector==0?float((source>>shift)&255):selector==1?float((destination>>shift)&255):0.f;};
        result|=std::uint32_t(std::clamp((color(a)-color(b))*factor+color(d)+.5f,0.f,255.f))<<shift;
    }
    return result;
}
}
