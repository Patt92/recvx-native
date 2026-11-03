#pragma once
#include "shadow_projection.h"
#include "shadow_mask.h"
#include "shadow_transport.h"
#include <stdexcept>
#include <vector>

namespace recvx {
// Native mask raster in original 640x480 screen coordinates.
// scale > 1 (host Ultra quality, not original): the same triangles, edge rules and
// saturation, sampled at (x+.5)/scale in a (640*scale)x(480*scale) mask; scale 1 is
// the original raster bit for bit.
// depth_pass receives screen-linear transported vertex depth, NOT reciprocal
// Z. The receiver must use the same transport, and the final fragment integer
// conversion remains the consumer's contract. Never infer it from depth().
// This component never writes depth or assumes a fog state.
template<class DepthPass>
std::vector<unsigned char> raster_shadow_mask(
    const ShadowProjection& projection,const ShadowDepthCoefficients& coefficients,
    DepthPass depth_pass,unsigned scale=1)
{
    if(scale<1||scale>4)throw std::runtime_error("shadow: mask scale outside 1..4");
    const int MW=640*int(scale),MH=480*int(scale);const float M=float(scale);
    std::vector<unsigned char> mask(std::size_t(MW)*MH,0);
    if(!projection.drawable)return mask;
    const auto edge=[](const ShadowTransportVertex& a,
                       const ShadowTransportVertex& b,float x,float y){
        return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
    };
    const auto inclusive=[](const ShadowTransportVertex& a,
                            const ShadowTransportVertex& b){
        return b.y<a.y||(b.y==a.y&&b.x>a.x);
    };
    // Preserve face order separately within each original pass.
    for(unsigned pass=0;pass<2;++pass){
        for(const auto& face:projection.faces){
            if(pass==0 ? face.original_normal<=0 : face.original_normal>=0)
                continue;
            std::array<ShadowTransportVertex,3> transported;
            for(unsigned i=0;i<3;++i)
                transported[i]=transport_shadow_vertex(face.vertices[i],coefficients);
            if(shadow_transport_triangle_suppressed(transported))continue;
            // XY conversion and depth conversion both precede interpolation.
            auto p=transported;
            float area=edge(p[0],p[1],p[2].x,p[2].y);
            if(!std::isfinite(area)||area==0)continue;
            if(area<0){std::swap(p[1],p[2]);area=-area;}
            const int left=int(std::max(0.f,std::floor(
                std::min({p[0].x,p[1].x,p[2].x})*M)));
            const int right=int(std::min(float(MW-1),std::ceil(
                std::max({p[0].x,p[1].x,p[2].x})*M)));
            const int top=int(std::max(0.f,std::floor(
                std::min({p[0].y,p[1].y,p[2].y})*M)));
            const int bottom=int(std::min(float(MH-1),std::ceil(
                std::max({p[0].y,p[1].y,p[2].y})*M)));
            for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){
                const float X=(float(x)+.5f)/M,Y=(float(y)+.5f)/M;
                const float a=edge(p[1],p[2],X,Y);
                const float b=edge(p[2],p[0],X,Y);
                const float c=edge(p[0],p[1],X,Y);
                if(a<0||b<0||c<0)continue;
                if((a==0&&!inclusive(p[1],p[2]))||
                   (b==0&&!inclusive(p[2],p[0]))||
                   (c==0&&!inclusive(p[0],p[1])))continue;
                const float depth=(a*p[0].depth+b*p[1].depth+c*p[2].depth)/area;
                if(!depth_pass(x,y,depth))continue;
                auto& pixel=mask[std::size_t(y)*MW+x];
                pixel=static_cast<unsigned char>(
                    shadow_mask_fragment(pixel,pass==1));
            }
        }
    }
    return mask;
}
}
