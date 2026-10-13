#include "screen_fade.h"
#include <algorithm>
namespace recvx {
void ScreenFade::set(std::uint32_t argb,float ct){
    /* screen.c:21-47 */
    pbk_=ap_<<24|rp_<<16|gp_<<8|bp_;
    ct_=0;
    hkn_=ct<=1.0f?1.0f:1.0f+ct;
    ao_=an_;ro_=rn_;go_=gn_;bo_=bn_;
    ap_=argb>>24;rp_=(argb>>16)&0xFF;gp_=(argb>>8)&0xFF;bp_=argb&0xFF;
}
bool ScreenFade::control(){
    /* screen.c:50-95 */
    float hkn=hkn_;
    if(hkn>1.0f&&ct_==0){ct_+=1.0f;return false;}
    if(hkn==1.0f){an_=float(ap_);rn_=float(rp_);gn_=float(gp_);bn_=float(bp_);return true;}
    hkn-=1.0f;
    an_+=(float(ap_)-ao_)/hkn;
    rn_+=(float(rp_)-ro_)/hkn;
    gn_+=(float(gp_)-go_)/hkn;
    bn_+=(float(bp_)-bo_)/hkn;
    ct_+=1.0f;
    if(ct_>=hkn){an_=float(ap_);rn_=float(rp_);gn_=float(gp_);bn_=float(bp_);return true;}
    return false;
}
void ScreenFade::draw(std::vector<std::uint32_t>& canvas,int width,int height)const{
    /* screen.c:97-139: argb from the truncated float channels (unsigned int
       casts), njDrawPolygon2D converts the alpha with (a+1)>>1 into the GS
       0..128 ratio. */
    const unsigned a=unsigned(an_)&0xFFu,r=unsigned(rn_)&0xFFu,g=unsigned(gn_)&0xFFu,b=unsigned(bn_)&0xFFu;
    const unsigned ratio=(a+1)>>1;
    if(!ratio)return;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        auto& d=canvas[std::size_t(y)*std::size_t(width)+std::size_t(x)];
        std::uint32_t out=0xff000000u;
        const unsigned src[3]={b,g,r};
        for(unsigned i=0;i<3;++i){const unsigned shift=i*8u;const unsigned below=(d>>shift)&255u;
            out|=std::min(255u,(src[i]*ratio+below*(128u-ratio)+64u)/128u)<<shift;}
        d=out;
    }
}
}
