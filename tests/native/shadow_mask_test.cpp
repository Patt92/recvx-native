#include "shadow_mask.h"
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* message){
    if(!ok)throw std::runtime_error(message);
}
int main(){
    try{
        using namespace recvx;
        check(shadow_mask_fragment(0,false)==32,"front mask fragment");
        check(shadow_mask_fragment(32,true)==0,"opposite face cancellation");
        check(shadow_mask_fragment(0,true)==0,"negative mask must saturate");
        unsigned mask=0;
        for(unsigned i=0;i<8;++i)mask=shadow_mask_fragment(mask,false);
        check(mask==248,"5-bit saturation must occur per fragment");
        mask=shadow_mask_fragment(mask,true);
        check(mask==216,"saturated mask is not a winding counter");
        check(shadow_mask_composite(0xff806040u,32,255)==0xff755535u,
              "full-fog integer modulation/subtraction");
        check(shadow_mask_composite(0xff806040u,32,128)==0xff7a5a3au,
              "half-fog modulation");
        check(shadow_mask_composite(0xff806040u,32,0)==0xff806040u,
              "zero fog must not darken");
        check(shadow_mask_composite(0xff806040u,0,255)==0xff806040u,
              "zero mask must preserve destination");
        check(shadow_mask_composite(0xff03090fu,32,255)==0xff000004u,
              "destination subtraction must saturate");
        for(unsigned fog=0;fog<256;++fog){
            const unsigned reduction=(32*((fog*3)/16))/128;
            const auto pixel=shadow_mask_composite(0xffffffffu,32,fog);
            check((pixel&255u)==255u-reduction,"fog sweep");
        }
        bool rejected=false;
        try{(void)shadow_mask_composite(0,255,255);}
        catch(const std::runtime_error&){rejected=true;}
        check(rejected,"invalid expanded mask accepted");
        std::cout<<"PASS: two-pass mask arithmetic, RGB5 saturation, fog modulation, subtraction\n";
        std::cout<<"Arithmetic evidence only; rasterization and scene integration remain open\n";
    }catch(const std::exception& error){
        std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;
    }
}
