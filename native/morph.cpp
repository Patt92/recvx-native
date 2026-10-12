#include "morph.h"
#include "binary_view.h"
#include <stdexcept>
namespace recvx {
namespace {
// HDR_PS header the original reinterprets at the head of every vlist.
constexpr std::size_t header_size=64;
// npTransform advances sp/dp/fp by two NJS_POINT4 per vertex.
constexpr std::size_t vertex_size=32;
}
MorphStreams morph_streams(const std::vector<std::uint8_t>& model,std::size_t objects,const std::vector<std::uint32_t>* evalflags){
    MorphStreams out;out.model=&model;out.chunk.assign(objects,MorphStreams::none);
    BinaryView v(model);
    const auto base=v.u16(4);
    const auto nodes=v.relative(base,v.u32(12),objects*52);
    for(std::size_t i=0;i<objects;++i){
        // npPushMdlstr2 skips evalflags 0x8; a NULL model has no vlist to blend.
        const auto flags=evalflags&&i<evalflags->size()?(*evalflags)[i]:v.u32(nodes+i*52);
        if(flags&0x8u)continue;
        const auto mdl=v.u32(nodes+i*52+4);
        if(mdl==0xffffffff)continue;
        const auto vlist=v.u32(v.relative(base,mdl,24));
        if(vlist==0xffffffff)continue;
        const auto chunk=v.relative(base,vlist,header_size);
        // npCalcMorphing reads nb from this header before npTransform walks the
        // stream, so the whole payload must be addressable in the asset.
        const auto count=v.u16(chunk+6);
        v.require(chunk+header_size,std::size_t(count)*vertex_size);
        out.chunk[i]=chunk;
    }
    return out;
}
std::size_t MorphStreams::count(std::size_t object)const{
    const auto at=stream(object);
    if(at==none)throw std::runtime_error("morph object has no vertex stream");
    return BinaryView(*model).u16(at+6);
}
void MorphStreams::vertex(std::size_t object,std::size_t index,float* position,float* normal)const{
    const auto at=stream(object);
    if(at==none)throw std::runtime_error("morph object has no vertex stream");
    const auto count=BinaryView(*model).u16(at+6);
    if(index>=count)throw std::runtime_error("morph vertex outside object stream");
    BinaryView v(*model);
    const auto p=at+header_size+index*vertex_size;
    position[0]=v.f32(p);position[1]=v.f32(p+4);position[2]=v.f32(p+8);
    normal[0]=v.f32(p+16);normal[1]=v.f32(p+20);normal[2]=v.f32(p+24);
}
void morph_vertex(const MorphStreams& src,std::size_t src_object,
                  const MorphStreams& dst,std::size_t dst_object,
                  std::size_t index,float factor,float* position,float* normal){
    float sp[3],sn[3],dp[3],dn[3];
    src.vertex(src_object,index,sp,sn);dst.vertex(dst_object,index,dp,dn);
    // npTransform blends XYZ only and keeps the source W; NJS_POINT3/NJS_VECTOR
    // carry no W here, so both branches copy the source components as they are.
    for(unsigned axis=0;axis<3;++axis){
        position[axis]=sp[axis]==dp[axis]?sp[axis]:sp[axis]+(dp[axis]-sp[axis])*factor;
        normal[axis]=sn[axis]==dn[axis]?sn[axis]:sn[axis]+(dn[axis]-sn[axis])*factor;
    }
}
bool morph_blend(const MorphStreams& a,const MorphStreams& b,std::size_t object,
                 std::size_t index,float factor,float* position,float* normal){
    if(a.stream(object)==MorphStreams::none||b.stream(object)==MorphStreams::none)return false;
    // npTransform returns before writing when the two streams disagree.
    if(a.count(object)!=b.count(object))return false;
    morph_vertex(a,object,b,object,index,factor,position,normal);
    return true;
}
}
