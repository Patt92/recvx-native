// Native data view of the chunk models bhCollisionCheckL2MDL (hitchkl.c:735)
// tests a line against: the player's hit model mdl[3] (effsub2.c:2310) and
// mdl[1] of figures 41..43/91..93 (effsub2.c:2334). Layout per
// bhMlbBinRealize/bhCnkBinRealize (binfunc.c); no original routine is used.
#include "hit_model.h"
#include "binary_view.h"
#include "effect_collision.h"
#include "geometry_line.h"
#include <cmath>
#include <stdexcept>
namespace recvx {
std::vector<std::optional<HitModelObject>> decode_hit_model(const std::vector<std::uint8_t>& mlb){
    const BinaryView v(mlb);
    const unsigned status=v.u8(3),objects=v.u16(6);
    const std::size_t base=v.u16(4);
    const auto obj_off=std::int32_t(v.u32(12));
    std::vector<std::optional<HitModelObject>> out(objects);
    if(obj_off==-1)return out;
    // NJS_CNK_MODEL only with status bit 0x80 (bhCnkBinRealize); a basic model
    // has no vlist/plist for this test.
    if(!(status&0x80u))throw std::runtime_error("bhCollisionCheckL2MDL: MLB without chunk models");
    for(unsigned i=0;i<objects;++i){
        const std::size_t object=base+std::size_t(obj_off)+std::size_t(i)*52; // NJS_CNK_OBJECT
        const auto model=std::int32_t(v.u32(object+4));
        if(model==-1)continue;
        const std::size_t m=base+std::size_t(model);
        const auto vlist=std::int32_t(v.u32(m)),plist=std::int32_t(v.u32(m+4));
        if(vlist==-1||plist==-1)throw std::runtime_error("bhCollisionCheckL2MDL: chunk model without vlist/plist");
        HitModelObject h;
        h.center={v.f32(m+8),v.f32(m+12),v.f32(m+16)};
        h.radius=std::fabs(v.f32(m+20)); // bhCnkBinRealize: r = -r for the stored negative radius
        const std::size_t vp=base+std::size_t(vlist),pp=base+std::size_t(plist);
        const auto count=std::int16_t(v.u16(vp+6));
        if(count<0)throw std::runtime_error("bhCollisionCheckL2MDL: negative vertex count");
        for(int k=0;k<count;++k){const std::size_t q=vp+8+std::size_t(k)*12;h.vertices.push_back({v.f32(q),v.f32(q+4),v.f32(q+8)});}
        const unsigned polygons=v.u8(pp+4);
        for(unsigned k=0;k<polygons*3;++k)h.indices.push_back(std::int16_t(v.u16(pp+6+std::size_t(k)*2)));
        out[i]=std::move(h);
    }
    return out;
}
HitModelResult line_hit_model(NJS_POINT3 start,NJS_POINT3& end,const HitModelObject& object,const NJS_MATRIX& matrix){
    NJS_MATRIX m;for(unsigned i=0;i<16;++i)m[i]=matrix[i];
    NJS_POINT3 local=object.center,center;njCalcPoint(&m,&local,&center);
    NJS_POINT3 closest;
    if(line_geometry::point_line(center,start,line_geometry::subtract(end,start),closest)>object.radius)return HitModelResult::Miss;
    std::vector<NJS_POINT3> points(object.vertices.size());
    for(std::size_t i=0;i<points.size();++i){NJS_POINT3 p=object.vertices[i];njCalcPoint(&m,&p,&points[i]);}
    bool hit=false;
    for(std::size_t i=0;i+2<object.indices.size();i+=3){
        NJS_POINT3 area[3];
        for(unsigned j=0;j<3;++j){const auto index=object.indices[i+j];if(index<0||std::size_t(index)>=points.size())return HitModelResult::IndexOutside;area[j]=points[std::size_t(index)];}
        if(effect_line_polygon(start,end,area,3))hit=true;
    }
    return hit?HitModelResult::Hit:HitModelResult::Miss;
}
}
