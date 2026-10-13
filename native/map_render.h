#pragma once
#include "map_screen.h"
#include "renderer.h"
#include <vector>
namespace recvx {
/* Binds MapDrawList (the primitives bhControlMap emitted in the last tick) to
   the software renderer, i.e. the Ps2AddPrim2D/3D side of map.c:

   - One depth buffer for everything: njDrawLine2D/Polygon2D/Sprite2D hand
     `pri` to Ps2AddPrim as the vertex z (ps2_NaDraw2D.c:71/145,
     ps2_NaSprite.c:299) and Ps2AddPrim maps it with the same zbuff_a/b as the
     3D path (ps2_dummy.c: sz = a + b / z against -b * (1/z) - a), so a 2D
     primitive at pri sits at camera distance -pri. The screen points come
     back through the njSetScreen contract of map.c:356-369 (dist 500,
     aspect 1.174, 640x480 centred at 320/240) and njClipZ(-2,-20000).
   - njDrawSprite2D attr 0x22 = COLOR | ALPHA (ninjadef.h BIT_1/BIT_5): the
     TexAnm cell of the map (spr < 17) or stage texture uses the installed
     constant material, quantized to trunc(128*channel), and texel alpha.
   - njCnkEasyMultiDraw* under njControl3D(0x810) + njSetConstantAttr(-1,0x800)
     draws every strip with constant material and NJD_FST_UA. The selected
     vu1GetVertexColorCM does NOT consume the normal-light intensity;
     untextured RGB is trunc(128*material), texture modulation divides by 128.
     MapLightMain's state remains available, but is not this shader's input.
   - njDrawPolygon3DEx(CmpArw,4,1): translucent strip, colour per vertex. */
struct MapRenderStats {
    unsigned fills=0,lines=0,sprites=0,markers=0,objects=0,compass=0,triangles=0;
    unsigned sprites_without_texture=0,markers_without_model=0,objects_without_model=0,draws_below_near=0;
};
class MapRenderer {
public:
    void draw(SoftwareRenderer& image,const MapScreen& map);
    const MapRenderStats& stats()const{return stats_;}
    const Scene& scene()const{return scene_;}
    /* Resources decoded from the index / floor file (test access). */
    std::size_t marker_count()const{return markers_.size();}
    std::size_t object_geometry_count()const{return object_geometry_.size();}
    std::size_t map_texture_count()const{return map_textures_.size();}
    std::size_t stage_texture_count()const{return stage_textures_.size();}
private:
    void refresh(const MapScreen& map);
    void add_screen_quad(const std::array<float,8>& points,std::uint32_t color,float priority,bool transparent);
    void add_model(const std::vector<SceneTriangle>& geometry,const NJS_MATRIX& matrix,const RECVX_MAP_ARGB& material,const MapScreen& map);
    unsigned index_generation_=~0u,stage_generation_=~0u;
    std::vector<std::vector<SceneTriangle>> markers_;          /* MrkMdl[n] in model space */
    std::vector<std::vector<SceneTriangle>> object_geometry_;  /* floor model, by NJS_CNK_OBJECT file index */
    std::vector<SceneTexture> map_textures_,stage_textures_;
    Scene scene_;
    MapRenderStats stats_;
};
}
