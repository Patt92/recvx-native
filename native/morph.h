#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace recvx {
// njplus.c npPushMdlstr2/npCalcMorphing/npTransform walk a realized model string
// as an array of NJS_CNK_OBJECT. Object i owns its vlist through u32(object_i + 4);
// the 64-byte chunk header carries usIndexMax at +6 (npCalcMorphing's nb) and the
// stream holds two NJS_POINT4 — position, then normal — per vertex.
//
// npPushMdlstr2 and npCalcMorphing only touch objects that have a model and no
// evalflags 0x8, so exactly those objects carry a stream below.
struct MorphStreams {
    static constexpr std::size_t none=std::size_t(-1);
    const std::vector<std::uint8_t>* model=nullptr;
    std::vector<std::size_t> chunk; // vlist chunk offset per object, none when absent
    std::size_t objects()const{return chunk.size();}
    std::size_t stream(std::size_t object)const{return object<chunk.size()?chunk[object]:none;}
    // npCalcMorphing's nb: usIndexMax of the object's vlist.
    std::size_t count(std::size_t object)const;
    // Raw vertex `index`: position at +0, normal at +16.
    void vertex(std::size_t object,std::size_t index,float* position,float* normal)const;
};
MorphStreams morph_streams(const std::vector<std::uint8_t>& model,std::size_t objects);
// npTransform's per-vertex step for two objects npCalcMorphing paired up.
// npTransform scales its own argument by 0.001; `factor` is that scaled value.
// Components the source and target share are copied verbatim, matching the
// original's equal-source/target branch.
void morph_vertex(const MorphStreams& src,std::size_t src_object,
                  const MorphStreams& dst,std::size_t dst_object,
                  std::size_t index,float factor,float* position,float* normal);
// npCalcMorphing for one object. False when npTransform returns without writing:
// either object has no stream, or their usIndexMax differ.
bool morph_blend(const MorphStreams& a,const MorphStreams& b,std::size_t object,
                 std::size_t index,float factor,float* position,float* normal);
}
