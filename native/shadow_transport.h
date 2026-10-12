#pragma once
#include "shadow_projection.h"
#include "shadow_depth.h"

namespace recvx {
// Native vertex transport, not a VU/GS register machine. Source of every
// operation: Ps2AddPrim3DMod ELF 0x002CC79C..0x002CC7E8.
struct ShadowTransportVertex {
    float x,y; // viewport-relative XY after absolute 12.4 conversion
    std::uint32_t depth;
    bool outside;
};

inline ShadowTransportVertex transport_shadow_vertex(
    const ShadowProjectedVertex& vertex,const ShadowDepthCoefficients& coefficients)
{
    if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)
       ||!std::isfinite(vertex.z))
        throw std::runtime_error("shadow transport: nonfinite vertex");
    // The guard accumulator is replaced AGAIN by the depth VADDA. It must
    // never be used as a depth offset. Keep the multiply separate from ADD.
    const float guard_base=vertex.z-2048.0625f;
    const volatile float guard_product=vertex.z*0.062501907f;
    const float guard_z=guard_base+guard_product;
    const float guard_x=vertex.x-2048.f,guard_y=vertex.y-2048.f;
    const bool outside=guard_x>2047.f||guard_x<-2047.f
        ||guard_y>2047.f||guard_y<-2047.f
        ||guard_z>2047.f||guard_z<-2047.f;
    return {std::trunc(vertex.x*16.f)/16.f-1728.f,
            std::trunc(vertex.y*16.f)/16.f-1808.f,
            shadow_vertex_depth(coefficients,vertex.invz),outside};
}

inline bool shadow_transport_triangle_suppressed(
    const std::array<ShadowTransportVertex,3>& vertices)
{
    // Mod clears CLIP once per three-vertex packet. At the final vertex its
    // low 18 bits hold all three tests; ANY set bit becomes ADC. This is a
    // triangle rejection, not clipping to the near plane or an AND-of-planes.
    return vertices[0].outside||vertices[1].outside||vertices[2].outside;
}
}
