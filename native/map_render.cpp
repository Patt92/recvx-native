#include "map_render.h"
#include "back_color.h"
#include "binary_view.h"
#include "map_tables.h"
#include "widescreen.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace recvx {
namespace {
constexpr float kDist=500.0f;        /* njSetScreen dist (map.c:361) */
float kAspect(){return recvx_aspect_x();} /* njSetAspect 1.174 (map.c:358); Host-Mod Breitbild */
constexpr float kNear=2.0f,kFar=20000.0f; /* njClipZ(-2,-20000) (map.c:356) */
constexpr float kCenterX=320.0f,kCenterY=240.0f;

/* Screen point at depth -pri back into the camera space draw_view projects
   with focal (kDist*kAspect(), kDist). 2D primitives are not 3D-clipped in the
   original (Ps2AddPrim clip_3d_on 0); a depth inside the near plane keeps the
   point by sitting on the plane. */
NJS_POINT3 screen_point(float sx,float sy,float pri,MapRenderStats& stats){
    float z=-pri;
    if(z<kNear){z=kNear;++stats.draws_below_near;}
    return {(sx-kCenterX)*z/(kDist*kAspect()),(sy-kCenterY)*z/kDist,z};
}
std::uint32_t material_color(const RECVX_MAP_ARGB& m){
    const auto channel=[](float v){return std::uint32_t(std::clamp(std::lround(255.0f*v),0l,255l));};
    return channel(m.a)<<24|channel(m.r)<<16|channel(m.g)<<8|channel(m.b);
}
SceneTriangle flat_triangle(const NJS_POINT3& a,const NJS_POINT3& b,const NJS_POINT3& c,std::uint32_t color,SceneBlend blend){
    SceneTriangle t;
    t.vertices[0].position=a;t.vertices[1].position=b;t.vertices[2].position=c;
    t.color=color;t.blend=blend;t.camera_mask=false;t.world_lit=true;t.gouraud=false;
    return t;
}
} // namespace

void MapRenderer::refresh(const MapScreen& map){
    if(index_generation_!=map.index_generation()){
        index_generation_=map.index_generation();
        markers_.clear();map_textures_.clear();
        const auto& index=map.index_buffer();
        for(const auto& chunk:map.marker_models()){
            /* bhMlbBinRealize(pacP, mlwP) (map.c:388): the marker object tree in
               model space; njCnkEasyMultiDrawObject applies the node transforms. */
            auto scene=decode_model_asset(index,chunk.offset,0);
            markers_.push_back(std::move(scene.triangles));
        }
        if(map.map_texture().size)map_textures_=decode_texture_bundle(index,map.map_texture().offset);
    }
    if(stage_generation_!=map.stage_generation()){
        stage_generation_=map.stage_generation();
        object_geometry_.clear();stage_textures_.clear();
        const auto& floor=map.buffer();
        if(map.stage_model().size){
            /* MapCodeProcess case 4 draws cur_objP->model alone under the
               purged absolute transform: decode every node with an identity
               work matrix so each object's strips stay in its own space. */
            const BinaryView view(floor);
            view.require(map.stage_model().offset,16);
            const unsigned count=view.u16(map.stage_model().offset+6);
            ModelGeometryOptions options;
            NJS_MATRIX unit;njUnitMatrix(&unit);
            std::array<float,16> identity{};std::copy_n(unit,16,identity.begin());
            options.world_matrices.assign(count,identity);
            options.evalflags.assign(count,0u);
            auto scene=decode_model_asset(floor,map.stage_model().offset,0,options);
            object_geometry_.assign(count,{});
            for(auto& t:scene.triangles){
                if(t.object>=count)throw std::runtime_error("map floor strip outside the object array");
                object_geometry_[t.object].push_back(t);
            }
        }
        if(map.stage_texture().size)stage_textures_=decode_texture_bundle(floor,map.stage_texture().offset);
    }
}

void MapRenderer::add_screen_quad(const std::array<float,8>& p,std::uint32_t color,float priority,bool transparent){
    /* njDrawPolygon2D attr 0x60: NJD_FILL = triangle fan, NJD_TRANSPARENT =
       ABE (ps2_NaDraw2D.c:148-156); the colour alpha becomes (a+1)>>1 of 128. */
    const NJS_POINT3 v[4]={screen_point(p[0],p[1],priority,stats_),screen_point(p[2],p[3],priority,stats_),
                           screen_point(p[4],p[5],priority,stats_),screen_point(p[6],p[7],priority,stats_)};
    const auto blend=transparent?SceneBlend::Alpha:SceneBlend::Opaque;
    scene_.triangles.push_back(flat_triangle(v[0],v[1],v[2],color,blend));
    scene_.triangles.push_back(flat_triangle(v[0],v[2],v[3],color,blend));
    stats_.triangles+=2;
}

void MapRenderer::add_model(const std::vector<SceneTriangle>& geometry,const NJS_MATRIX& matrix,const RECVX_MAP_ARGB& material,const MapScreen& map){
    /* map.c:734 njControl3D(0x810) selects the CM colour function:
       njCnkCs:1748 / vu1GetVertexColorCM:1590 reads diffuse and alpha ONLY,
       not the intensities calculated by MapLightMain/njCnkCvVn.
       vu1SetDiffuseMaterial emits 128*RGB; untextured fragments retain these
       truncated channels. Textured fragments use them as MODULATE factors
       over 128, retained explicitly in the native renderer. */
    NJS_MATRIX m;std::copy_n(matrix,16,m);
    (void)map;
    for(const auto& source:geometry){
        SceneTriangle t=source;
        for(auto& vertex:t.vertices){
            NJS_POINT3 local=vertex.position;njCalcPoint(&m,&local,&vertex.position);
            NJS_VECTOR n=vertex.normal;njCalcVector(&m,&n,&vertex.normal);
        }
        const auto channel=[](float value){return unsigned(std::clamp(128.f*value,0.f,255.f));};
        t.texture_color_divisor=128;
        t.color=(material_color(material)&0xff000000u)|channel(material.r)<<16|channel(material.g)<<8|channel(material.b);
        t.camera_mask=false;t.world_lit=true;t.gouraud=false;
        /* NJD_FST_UA is forced even for full-alpha materials: the chunk
           blend pair remains authoritative (not always SA/ISA). */
        t.blend=SceneBlend::Ninja;
        scene_.triangles.push_back(t);
        ++stats_.triangles;
    }
}

void MapRenderer::draw(SoftwareRenderer& image,const MapScreen& map){
    refresh(map);
    scene_=Scene{};
    scene_.background=installed_back_pixel(); /* Db.clear; MapScreen sets it with njSetBackColor(0,0,0), map.c:71/359 */
    stats_=MapRenderStats{};
    scene_.textures=map_textures_;
    const std::size_t stage_base=scene_.textures.size();
    scene_.textures.insert(scene_.textures.end(),stage_textures_.begin(),stage_textures_.end());
    const auto& objects=map.map_objects();
    for(const auto& d:map.draw_list()){
        switch(d.kind){
        case MapDraw::Kind::Fill:
            add_screen_quad(d.points,d.color,d.priority,(d.attribute&0x40u)!=0);
            ++stats_.fills;
            break;
        case MapDraw::Kind::Line:{
            /* njDrawLine2D: two points, pri as z, colour per point. */
            SceneLine line;
            line.points={screen_point(d.points[0],d.points[1],d.priority,stats_),screen_point(d.points[2],d.points[3],d.priority,stats_)};
            line.colors={d.color,d.color};
            scene_.lines.push_back(line);
            ++stats_.lines;
            break;
        }
        case MapDraw::Kind::Sprite:{
            if(d.sprite<0||d.sprite>=RECVX_MP_SPR_NUM){++stats_.sprites_without_texture;break;}
            const auto& anim=recvx_map_tex_anm[std::size_t(d.sprite)];
            const auto& list=d.stage_texture?stage_textures_:map_textures_;
            if(anim.texid<0||std::size_t(anim.texid)>=list.size()){++stats_.sprites_without_texture;break;}
            const std::uint16_t texture=std::uint16_t((d.stage_texture?stage_base:0)+std::size_t(anim.texid));
            /* njDrawSprite2D with sx = sy = 1 (ps2_NaSprite.c:239-245): the
               quad spans p - c .. p + (size - c); u/v are 1/256 texture
               fractions (shift by bitcount(width)-4 into 12.4 texels). */
            const float left=d.points[0]-float(anim.cx),top=d.points[1]-float(anim.cy);
            const float right=d.points[0]+float(anim.sx-anim.cx),bottom=d.points[1]+float(anim.sy-anim.cy);
            /* ps2_NaSprite.c:40-101: BOTH axes use bitcount(width)-4,
               plus eight 12.4 units (half a texel). MapDrawSprite ang=0
               selects quadrant 3, which adds no further UV correction. */
            const auto& tex=list[std::size_t(anim.texid)];
            const float u1=float(anim.u1)/256.f+.5f/float(tex.width),u2=float(anim.u2)/256.f+.5f/float(tex.width);
            const float v1=(float(anim.v1)*float(tex.width)/256.f+.5f)/float(tex.height);
            const float v2=(float(anim.v2)*float(tex.width)/256.f+.5f)/float(tex.height);
            const float pri=std::min(d.priority,-1.0f);
            const NJS_POINT3 p[4]={screen_point(left,top,pri,stats_),screen_point(right,top,pri,stats_),
                                   screen_point(right,bottom,pri,stats_),screen_point(left,bottom,pri,stats_)};
            const float uv[4][2]={{u1,v1},{u2,v1},{u2,v2},{u1,v2}};
            const int order[2][3]={{0,1,2},{0,2,3}};
            for(const auto& tri:order){
                SceneTriangle t;
                for(int i=0;i<3;++i){t.vertices[i].position=p[tri[i]];t.vertices[i].u=uv[tri[i]][0];t.vertices[i].v=uv[tri[i]][1];}
                /* MapDrawSprite installs its constant material and requests
                   NJD_SPRITE_COLOR (0x2), not an unconditional white sprite. */
                const auto channel=[](float v){return unsigned(std::clamp(128.f*v,0.f,255.f));};
                t.color=(d.color&0xff000000u)|channel(d.material.r)<<16|channel(d.material.g)<<8|channel(d.material.b);
                t.texture_color_divisor=128;
                t.textured=true;t.texture=texture;t.camera_mask=false;t.world_lit=true;
                t.blend=SceneBlend::Ninja;t.blend_register=0x44;
                scene_.triangles.push_back(t);
            }
            stats_.triangles+=2;
            ++stats_.sprites;
            break;
        }
        case MapDraw::Kind::Marker:
            if(d.model<0||std::size_t(d.model)>=markers_.size()){++stats_.markers_without_model;break;}
            add_model(markers_[std::size_t(d.model)],d.matrix,d.material,map);
            ++stats_.markers;
            break;
        case MapDraw::Kind::MapObject:{
            if(d.model<0||std::size_t(d.model)>=objects.size()){++stats_.objects_without_model;break;}
            const auto node=objects[std::size_t(d.model)].node;
            if(node>=object_geometry_.size()){++stats_.objects_without_model;break;}
            const auto first=scene_.triangles.size();
            add_model(object_geometry_[node],d.matrix,d.material,map);
            for(std::size_t i=first;i<scene_.triangles.size();++i)if(scene_.triangles[i].textured)
                scene_.triangles[i].texture=std::uint16_t(stage_base+scene_.triangles[i].texture);
            ++stats_.objects;
            break;
        }
        case MapDraw::Kind::Compass:{
            /* FsubCompass: njDrawPolygon3DEx(CmpArw,4,1) - a translucent
               strip (Ps2AddPrim3D 0x26: TRISTRIP, IIP, ABE) under the matrix
               the draw carries; col is 0xAARRGGBB with a >> 1 as GS ratio. */
            NJS_MATRIX m;std::copy_n(d.matrix,16,m);
            NJS_POINT3 v[4];
            for(int i=0;i<4;++i){NJS_POINT3 local{recvx_map_cmp_arw[i].x,recvx_map_cmp_arw[i].y,recvx_map_cmp_arw[i].z};njCalcPoint(&m,&local,&v[i]);}
            for(int i=0;i+2<4;++i){
                SceneTriangle t=flat_triangle(v[i],v[i+1],v[i+2],recvx_map_cmp_arw[i].col,SceneBlend::Alpha);
                scene_.triangles.push_back(t);
            }
            stats_.triangles+=2;
            ++stats_.compass;
            break;
        }
        }
    }
    NJS_MATRIX unit;njUnitMatrix(&unit);
    image.draw_view(scene_,unit,kDist*kAspect(),kDist,false,{},kNear,kFar);
}
}
