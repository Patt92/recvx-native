#pragma once
#include "scene.h"
#include "shadow_geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace recvx {
struct ShadowProjectedVertex {
    // x/y carry the viewport offset (fNaViwOffsetX/Y + fSx/fSy), as the
    // original draw path stores them in pP->fX/fY (ps2_NinjaCnk.c:1584).
    // sx/sy keep the raw pre-offset fSx/fSy (ps2_NinjaCnk.c:1202/1539);
    // the face normal is computed from these, not from the offset values.
    float x=0,y=0,z=0,invz=0,sx=0,sy=0;
};
struct ShadowProjectedFace {
    std::array<ShadowProjectedVertex,3> vertices;
    // njCnkCoP3: pass 0 accepts normal > 0, pass 1 normal < 0.
    float original_normal=0;
};
struct ShadowProjection {
    std::vector<ShadowProjectedFace> faces;
    std::array<float,4> bounds{};
    // Fog is sampled later from this view-space Z using the actual fog owner.
    float last_vertex_z=0;
    bool drawable=false;
};

// Keep individual arithmetic operations: CvVMod uses the composed affine
// matrix and translation-first coordinates, not two point transformations.
inline float shadow_product(float a,float b){const volatile float p=a*b;return p;}
inline void shadow_view_model(NJS_MATRIX& out,const NJS_MATRIX& view,
                              const std::array<float,16>& model)
{
    std::copy(model.begin(),model.end(),out);
    for(unsigned column=0;column<4;++column)for(unsigned row=0;row<3;++row){
        float value=shadow_product(view[row],model[column*4]);
        value+=shadow_product(view[4+row],model[column*4+1]);
        value+=shadow_product(view[8+row],model[column*4+2]);
        if(column==3)value+=view[12+row];
        out[column*4+row]=value;
    }
}

// Projection in the original 640x480 domain, preserving dist/aspect order.
// No camera reconstruction, silhouette replacement or inferred fog state.
inline ShadowProjection project_shadow(
    const SceneShadow& draw,const NJS_MATRIX& view,float distance,
    float aspect_x,float aspect_y)
{
    if(!draw.geometry)
        throw std::runtime_error("shadow: missing modifier geometry");
    const auto& geometry=*draw.geometry;
    ShadowProjection result;
    result.bounds={65536.f,65536.f,-65536.f,-65536.f};
    NJS_MATRIX combined;
    shadow_view_model(combined,view,draw.transform);
    std::vector<ShadowProjectedVertex> vertices;
    vertices.reserve(geometry.vertices.size());
    bool near_rejected=false;
    for(const auto& p:geometry.vertices){
        const auto coordinate=[&](unsigned row){
            float value=combined[12+row]+shadow_product(combined[row],p[0]);
            value+=shadow_product(combined[4+row],p[1]);
            value+=shadow_product(combined[8+row],p[2]);return value;
        };
        NJS_POINT3 camera{coordinate(0),coordinate(1),coordinate(2)};
        if(!std::isfinite(camera.x)||!std::isfinite(camera.y)
           ||!std::isfinite(camera.z))
            throw std::runtime_error("shadow: nonfinite transformed vertex");
        // njCnkCvVMod flags ANY loaded vertex with Z < 1.
        // Ps2ShadowDraw then suppresses the entire composite rectangle.
        if(camera.z<1.f)near_rejected=true;
        ShadowProjectedVertex vertex;
        vertex.z=camera.z;
        if(camera.z!=0.f){
            vertex.invz=1.f/camera.z;
            // fSx/fSy before the viewport offset (ps2_NinjaCnk.c:1202/1539).
            const float scale=shadow_product(distance,vertex.invz);
            vertex.sx=shadow_product(aspect_x,shadow_product(camera.x,scale));
            vertex.sy=shadow_product(aspect_y,shadow_product(camera.y,scale));
            // pP->fX/fY after the offset (ps2_NinjaCnk.c:1584).
            vertex.x=2048.f+vertex.sx;
            vertex.y=2048.f+vertex.sy;
        }
        vertices.push_back(vertex);
    }
    if(near_rejected)return result;
    for(const auto& triangle:geometry.triangles){
        ShadowProjectedFace face;
        for(unsigned i=0;i<3;++i){
            if(triangle[i]>=vertices.size())
                throw std::runtime_error("shadow: unloaded modifier vertex");
            const auto& p=vertices[triangle[i]];
            face.vertices[i]=p;
            result.bounds[0]=std::min(result.bounds[0],p.x);
            result.bounds[1]=std::min(result.bounds[1],p.y);
            result.bounds[2]=std::max(result.bounds[2],p.x);
            result.bounds[3]=std::max(result.bounds[3],p.y);
        }
        const auto& a=face.vertices[0];
        const auto& b=face.vertices[1];
        const auto& c=face.vertices[2];
        // njCnkCoP3:1684 computes the face normal from the pre-offset
        // fSx/fSy, with this exact operand order. Adding the viewport
        // offset first cancels algebraically but loses float precision for
        // small or near-degenerate triangles, whose sign selects the pass.
        face.original_normal=(b.sx-c.sx)*(a.sy-c.sy)
                            -(a.sx-c.sx)*(b.sy-c.sy);
        result.last_vertex_z=c.z;
        result.faces.push_back(face);
    }
    if(result.faces.empty())return result;
    // Ps2ShadowDraw rejects the untrimmed bounds before viewport clipping.
    for(const float value:result.bounds)
        if(!std::isfinite(value)||value<1536.f||value>=2560.f)
            return result;
    result.bounds[0]=std::clamp(result.bounds[0],1728.f,2368.f);
    result.bounds[2]=std::clamp(result.bounds[2],1728.f,2368.f);
    result.bounds[1]=std::clamp(result.bounds[1],1808.f,2288.f);
    result.bounds[3]=std::clamp(result.bounds[3],1808.f,2288.f);
    result.drawable=true;
    return result;
}

// Explicit focal-only diagnostic contract; production callers preserve the
// separately owned Ninja screen distance/aspects with the overload above.
inline ShadowProjection project_shadow(
    const SceneShadow& draw,const NJS_MATRIX& view,float focal_x,float focal_y)
{
    return project_shadow(draw,view,1.f,focal_x,focal_y);
}
}
