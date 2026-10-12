#pragma once
#include "binary_view.h"
#include <array>

namespace recvx {
// effect.c:bhDrawMdfEffect selects objP[mdlver].model directly.
// Node transforms/evalflags are not applied by that model-only draw.
struct ShadowGeometry {
    std::vector<std::array<float,3>> vertices;
    std::vector<std::array<unsigned,3>> triangles;
    std::array<float,3> center{};
    float radius=0;
};

// SYSTEM: each length excludes its preceding four-byte length word.
// The second model block belongs to effect 1 (effect.c:ef_info).
inline std::vector<ShadowGeometry> decode_shadow_geometry(
    const std::vector<std::uint8_t>& system)
{
    const BinaryView source(system);
    std::size_t header=0;
    for(unsigned block=0;block<1;++block){
        const auto length=source.u32(header);
        source.require(header+4,length);
        header+=4+length;
    }
    const auto length=source.u32(header);
    source.require(header+4,length);
    // Restrict all relative pointers to this model block, not the whole archive.
    const std::vector<std::uint8_t> bytes(
        system.begin()+header+4,system.begin()+header+4+length);
    const BinaryView v(bytes);
    v.require(0,16);
    if(v.u8(0)!='M'||v.u8(1)!='D'||v.u8(2)!='L'||!(v.u8(3)&0x80))
        throw std::runtime_error("shadow: expected chunk MDL");
    const std::size_t base=v.u16(4);
    const unsigned nodes=v.u16(6);
    const auto objects=v.relative(base,v.u32(12),std::size_t(nodes)*52);
    std::vector<ShadowGeometry> result(nodes);
    for(unsigned node=0;node<nodes;++node){
        auto& out=result[node];
        const auto model_offset=v.u32(objects+node*52+4);
        if(model_offset==0xffffffffu)continue;
        const auto model=v.relative(base,model_offset,24);
        out.center={v.f32(model+8),v.f32(model+12),v.f32(model+16)};
        out.radius=v.f32(model+20); // Negative radius disables original sphere culling.
        const auto vertex_offset=v.u32(model);
        const auto polygon_offset=v.u32(model+4);
        unsigned first=0,count=0;
        if(vertex_offset!=0xffffffffu){
            const auto vertex=v.relative(base,vertex_offset,8);
            if(v.u8(vertex)!=34)
                throw std::runtime_error("shadow: unsupported modifier vertex chunk");
            first=v.u16(vertex+4);
            count=v.u16(vertex+6);
            if(count>65536u-first)
                throw std::runtime_error("shadow: vertex index range");
            const auto chunk_bytes=(std::size_t(v.u16(vertex+2))+1)*4;
            v.require(vertex,chunk_bytes);
            if(chunk_bytes<8||std::size_t(count)*12>chunk_bytes-8)
                throw std::runtime_error("shadow: truncated modifier vertices");
            out.vertices.reserve(count);
            for(unsigned i=0;i<count;++i){
                const auto p=vertex+8+std::size_t(i)*12;
                out.vertices.push_back({v.f32(p),v.f32(p+4),v.f32(p+8)});
            }
        }
        if(polygon_offset!=0xffffffffu){
            const auto polygon=v.relative(base,polygon_offset,6);
            if(v.u8(polygon)!=56)
                throw std::runtime_error("shadow: unsupported modifier polygon chunk");
            const auto chunk_bytes=(std::size_t(v.u16(polygon+2))+2)*2;
            v.require(polygon,chunk_bytes);
            const unsigned flags=v.u16(polygon+4);
            const unsigned count_tri=flags&0x3fff,extra=flags>>14;
            const std::size_t stride=(3+extra)*2;
            if(chunk_bytes<6||std::size_t(count_tri)*stride>chunk_bytes-6)
                throw std::runtime_error("shadow: truncated modifier triangles");
            out.triangles.reserve(count_tri);
            for(unsigned i=0;i<count_tri;++i){
                const auto p=polygon+6+i*stride;
                std::array<unsigned,3> triangle{};
                for(unsigned corner=0;corner<3;++corner){
                    const unsigned index=v.u16(p+corner*2);
                    if(index<first||index-first>=count)
                        throw std::runtime_error("shadow: unloaded modifier vertex");
                    triangle[corner]=index-first;
                }
                // Preserve source winding and polygon order for both mask passes.
                out.triangles.push_back(triangle);
            }
        }
    }
    return result;
}
}
