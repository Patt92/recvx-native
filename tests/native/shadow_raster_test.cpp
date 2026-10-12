#include "shadow_raster.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* reason){
    if(!ok)throw std::runtime_error(reason);
}
int main(){
    try{
        recvx::ShadowProjection p;
        p.drawable=true;
        recvx::ShadowProjectedFace front;
        front.vertices={recvx::ShadowProjectedVertex{1730,1810,5,.2f},
                        recvx::ShadowProjectedVertex{1750,1810,5,.2f},
                        recvx::ShadowProjectedVertex{1730,1830,5,.2f}};
        front.original_normal=-400;
        // Reverse winding to make the additive original pass.
        std::swap(front.vertices[1],front.vertices[2]);
        front.original_normal=400;
        p.faces.push_back(front);
        const auto coefficients=recvx::shadow_depth_coefficients(-2.f,-20000.f);
        const auto accept=[](int,int,float){return true;};
        auto mask=recvx::raster_shadow_mask(p,coefficients,accept);
        check(mask[5*640+5]==32,"additive pass missing");
        check(mask[100*640+100]==0,"outside triangle modified");
        auto back=front;
        std::swap(back.vertices[1],back.vertices[2]);
        back.original_normal=-400;
        p.faces.push_back(back);
        mask=recvx::raster_shadow_mask(p,coefficients,accept);
        for(auto pixel:mask)check(pixel==0,"opposite passes did not cancel");
        const auto reject=[](int,int,float){return false;};
        mask=recvx::raster_shadow_mask(p,coefficients,reject);
        for(auto pixel:mask)check(pixel==0,"rejected depth produced mask");
        // All three coordinates fall into one 1/16-pixel cell.
        // They enclose a sample before quantization, but have zero area after it.
        recvx::ShadowProjection tiny;
        tiny.drawable=true;
        recvx::ShadowProjectedFace collapsed;
        collapsed.vertices={
            recvx::ShadowProjectedVertex{1733.5001f,1813.5001f,5,.2f},
            recvx::ShadowProjectedVertex{1733.55f,1813.5001f,5,.2f},
            recvx::ShadowProjectedVertex{1733.5001f,1813.55f,5,.2f}
        };
        collapsed.original_normal=1;
        tiny.faces.push_back(collapsed);
        const auto collapsed_mask=recvx::raster_shadow_mask(tiny,coefficients,accept);
        for(auto pixel:collapsed_mask)
            check(pixel==0,"subpixel-collapsed triangle produced coverage");

        // Distinguish vertex quantization BEFORE interpolation from mapping
        // interpolated reciprocal Z afterward. Diagnostic D=1000*q.
        recvx::ShadowProjection sloped;
        sloped.drawable=true;
        auto varying=front;
        varying.vertices[0].invz=.0019375f;
        varying.vertices[1].invz=.0029375f;
        varying.vertices[2].invz=.0049375f;
        sloped.faces.push_back(varying);
        bool sampled=false;
        recvx::raster_shadow_mask(sloped,{0,1000},[&](int x,int y,float depth){
            if(x==5&&y==5){
                // barycentric weights .65,.175,.175 on quantized XY.
                check(std::fabs(depth-1.7f)<.000001f,
                      "raster converted interpolated reciprocal instead of vertices");
                sampled=true;
            }
            return true;
        });
        check(sampled,"sloped diagnostic sample not covered");
        auto guarded=p;
        guarded.faces.resize(1);
        guarded.faces[0].vertices[0].z=4000;
        unsigned callbacks=0;
        const auto guard_mask=recvx::raster_shadow_mask(guarded,coefficients,
            [&](int,int,float){++callbacks;return true;});
        check(callbacks==0,"rejected corner did not suppress the triangle");
        for(auto pixel:guard_mask)check(pixel==0,"guard-rejected triangle wrote mask");

        // Explicit boundary test: truncation, not nearest rounding.
        check(std::trunc(1733.54f*16.f)/16.f==1733.5f,
              "screen quantization must truncate");
        std::cout<<"PASS: additive mask, opposite-face cancellation, depth callback\n";
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
