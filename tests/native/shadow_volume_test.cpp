#include "shadow_raster.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* reason){
    if(!ok)throw std::runtime_error(reason);
}
struct ReceiverDepth {
    std::uint32_t transported;
    bool operator()(int,int,float value)const{
        // Constant diagnostic receiver; varying receiver interpolation and
        // final fragment conversion require a separate renderer contract.
        return value>=transported;
    }
};
int main(int argc,char** argv){
    try{
        check(argc==2,"original SYSTEM asset required");
        std::ifstream file(argv[1],std::ios::binary);
        check(bool(file),"cannot open SYSTEM asset");
        const std::vector<std::uint8_t> bytes(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        const auto models=recvx::decode_shadow_geometry(bytes);
        check(models.size()==2,"original shadow models missing");
        recvx::SceneShadow draw;
        draw.geometry=std::make_shared<const recvx::ShadowGeometry>(models[0]);
        draw.transform={4,0,0,0,0,4,0,0,0,0,4,0,0,0,10,1};
        NJS_MATRIX view;
        njUnitMatrix(&view);
        const auto projected=recvx::project_shadow(draw,view,320,240);
        check(projected.drawable,"original diagnostic volume rejected");

        const auto coefficients=recvx::shadow_depth_coefficients(-2.f,-20000.f);
        for(const float receiver_z:{5.f,10.f,15.f}){
            const auto mask=recvx::raster_shadow_mask(
                projected,coefficients,ReceiverDepth{
                    recvx::shadow_vertex_depth(coefficients,1.f/receiver_z)});
            unsigned nonzero=0;
            for(const auto pixel:mask)if(pixel)++nonzero;
            const unsigned center=mask[240*640+320];
            std::cout<<"receiver_z="<<receiver_z
                     <<" center_mask="<<center
                     <<" nonzero_pixels="<<nonzero<<"\n";
            if(receiver_z==10.f){
                check(center==32,"interior receiver missing single-volume mask");
                check(nonzero>0,"interior produced no covered pixels");
                check(recvx::shadow_mask_composite(
                    0xff808080u,center,255)==0xff757575u,
                    "interior mask modulation differs");
            }else{
                check(nonzero==0,
                      "receiver outside volume retained a shadow mask");
            }
        }

        auto rejected=projected;
        rejected.drawable=false;
        const auto empty=recvx::raster_shadow_mask(
            rejected,coefficients,ReceiverDepth{
                recvx::shadow_vertex_depth(coefficients,.1f)});
        for(const auto pixel:empty)
            check(pixel==0,"rejected projection produced coverage");

        std::cout<<"PASS: original volume projection, two-pass mask and composition\n";
        std::cout<<"Transported vertex depth / constant receiver GEQUAL; no runtime pixel acceptance\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
