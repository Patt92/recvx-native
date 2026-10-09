#pragma once
#include "ninja_matrix.h"
#include <array>
#include <cstdint>
#include <vector>
#include <optional>
#include <memory>
#include <map>
namespace recvx {
// light: per-vertex lighting factor (r,g,b), used when the triangle is gouraud-lit.
struct SceneVertex { NJS_POINT3 position; float u=0,v=0; NJS_VECTOR normal{}; NJS_POINT3 light{1,1,1}; };
// Texture samples are immutable during scene composition. Copies share storage;
// resource edits detach so mods/atlas edits cannot alter another decoded asset.
class TexturePixels {
    std::shared_ptr<std::vector<std::uint32_t>> storage_=std::make_shared<std::vector<std::uint32_t>>();
    std::uint64_t revision_=0;
    const std::vector<std::uint32_t>& read()const{static const std::vector<std::uint32_t> empty;return storage_?*storage_:empty;}
    std::vector<std::uint32_t>& write(){++revision_;if(!storage_)storage_=std::make_shared<std::vector<std::uint32_t>>();else if(storage_.use_count()!=1)storage_=std::make_shared<std::vector<std::uint32_t>>(*storage_);return *storage_;}
public:
    TexturePixels()=default;
    TexturePixels(std::vector<std::uint32_t> data):storage_(std::make_shared<std::vector<std::uint32_t>>(std::move(data))){}
    TexturePixels(std::initializer_list<std::uint32_t> data):TexturePixels(std::vector<std::uint32_t>(data)){}
    std::size_t size()const{return read().size();}bool empty()const{return read().empty();}
    std::uint64_t revision()const{return revision_;}
    const std::uint32_t* data()const{return read().data();}std::uint32_t* data(){return write().data();}
    const std::uint32_t& operator[](std::size_t i)const{return read()[i];}std::uint32_t& operator[](std::size_t i){return write()[i];}
    auto begin()const{return read().begin();}auto end()const{return read().end();}
    auto begin(){return write().begin();}auto end(){return write().end();}
    void resize(std::size_t n){write().resize(n);}void reserve(std::size_t n){write().reserve(n);}
    void clear(){write().clear();}void push_back(std::uint32_t n){write().push_back(n);}
    void assign(std::size_t n,std::uint32_t value){write().assign(n,value);}
    friend bool operator==(const TexturePixels& a,const TexturePixels& b){return a.read()==b.read();}
    friend bool operator==(const TexturePixels& a,const std::vector<std::uint32_t>& b){return a.read()==b;}
    friend bool operator==(const std::vector<std::uint32_t>& a,const TexturePixels& b){return a==b.read();}
};
struct TextureFilterCache {
    bool ready=false,cutout=false,uniform_alpha=false;
    std::uint64_t fingerprint=0;std::uint32_t max_alpha=0;
    std::vector<std::vector<std::uint32_t>> mips;
};
struct SceneTexture {
    unsigned width=0,height=0;
    TexturePixels pixels;
    std::map<unsigned,TexturePixels> palette_banks; // njSetPaletteBankNum, PLI TEXflag palette-only records
    mutable std::shared_ptr<TextureFilterCache> filter_cache=std::make_shared<TextureFilterCache>();
    void invalidate_mips()const{filter_cache=std::make_shared<TextureFilterCache>();}
    std::uint64_t fingerprint()const{
        std::uint64_t h=1469598103934665603ull^(std::uint64_t(width)<<32|height);h*=1099511628211ull;
        h^=reinterpret_cast<std::uintptr_t>(pixels.data());h*=1099511628211ull;h^=pixels.size();h*=1099511628211ull;
        h^=pixels.revision();h*=1099511628211ull;
        return h;
    }
};
// Ninja: NJD_FST_UA strip, original GS ALPHA register from chunk/material bits.
// Alpha/Additive are explicit host sprite operations; all draw after opaque geometry.
enum class SceneBlend {Opaque,Additive,Alpha,Ninja};
struct SceneTriangle {
    std::array<SceneVertex,3> vertices;
    std::uint32_t color=0xffb2b2b2;
    std::uint16_t texture=0;
    std::uint16_t object=0;
    bool textured=false;
    bool camera_mask=true;
    bool world_lit=false;
    bool gouraud=false; // color is the unlit base; vertices carry light factors (Ninja per-vertex lighting)
    SceneBlend blend=SceneBlend::Opaque;bool depth_write=true;
    unsigned light_category=1;
    std::uint64_t blend_register=0x44; // original GS source-alpha/inverse-source-alpha
};
struct SceneLine {std::array<NJS_POINT3,2> points;std::array<std::uint32_t,2> colors;};
struct SceneCamera {
    NJS_POINT3 position{};
    std::array<std::int32_t,3> angles{};
    float distance=0;
    std::int32_t perspective=8192;
    float clip_near=0.05f,clip_far=0; // njClipZ (bhChangeViewClip*), set by the runtime; far 0 = unlimited
    std::array<std::uint32_t,16> hidden_objects{};
    std::uint16_t flags=0;
    // Original CAM_WRK spd, am_spd and aa_spd; control runs at 30 Hz.
    int transition_speed=0,angle_speed=0;float move_speed=0;
    NJS_POINT3 cut_origin{},cut_extent{};
    float follow_x=0,follow_z=0,target_height=0;
    std::array<float,4> y_offsets{};
    std::array<std::int32_t,2> angle_limits{};
    std::array<float,4> roll_offsets{},perspectives{};
};
// bhSetCut / bhControlActiveCamera state, independent of render calls.
class ActiveCameraMotion {
public:
    void reset(const SceneCamera& view,std::size_t cut);
    void select(const SceneCamera& target,std::size_t cut,bool force=false);
    void tick(const SceneCamera& target);
    const SceneCamera& view()const{return view_;}
    bool ready()const{return cut_.has_value();}
    bool transitioning()const{return blending_;}
    unsigned clock()const{return clock_;}
private:
    SceneCamera view_{};std::optional<std::size_t> cut_;
    bool blending_=false;unsigned clock_=0;
};
struct SceneSpawn {NJS_POINT3 position{};Angle yaw=0;};
struct SceneCutArea {
    std::size_t camera=0;
    std::int8_t floor=0;
    unsigned type=0;
    float minx=0,minz=0,maxx=0,maxz=0;
    std::uint32_t attributes=0;
};
struct Scene {
    std::vector<SceneTriangle> triangles;
    std::vector<SceneLine> lines;
    std::vector<SceneCamera> cameras;
    std::vector<SceneTexture> textures;
    std::vector<SceneSpawn> spawns;
    std::vector<SceneCutArea> cuts;
    std::vector<bool> cut_enabled,cut_tables; // per CUT_WORK: flg bit 0, decoded areas
    std::size_t object_count=0;
    std::uint32_t background=0xff101010;
};
Scene decode_scene(const std::vector<std::uint8_t>& room);
std::vector<SceneTexture> decode_texture_bundle(const std::vector<std::uint8_t>& data,std::size_t offset=0);
// One NJS_CNK_MODEL (vlist + plist) from raw memory with absolute addresses,
// transformed by `world` (njCnk*DrawModel through the host renderer).
Scene decode_memory_model(const std::uint8_t* memory,std::size_t size,std::uint32_t model,const NJS_MATRIX& world);
std::optional<std::size_t> camera_for_position(const Scene& scene,NJS_POINT3 position,std::int8_t floor=0);
// bhInitActiveCamera target evaluation (no transition/event-camera state).
SceneCamera camera_for_actor(const Scene& scene,std::size_t camera,NJS_POINT3 position,std::int8_t floor=0,const NJS_POINT3* eye=nullptr);
struct ModelPose {
    NJS_POINT3 position{};
    std::array<std::int32_t,3> angles{};
};
struct ModelGeometryOptions {
    NJS_MATRIX transform{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::vector<ModelPose> pose;
    std::vector<std::uint32_t> evalflags;
    // Authoritative native O_WORK matrices: rendering must not resample a
    // motion or rebuild a second pose after original procedural joint edits.
    std::vector<std::array<float,16>> world_matrices;
    std::vector<std::vector<NJS_POINT3>> world_vertices;
    std::vector<std::vector<NJS_VECTOR>> world_normals;
};
// Standalone MDL/PVP path shared by stage and animated character loading.
Scene decode_model_asset(const std::vector<std::uint8_t>& data,std::size_t model_offset,
                         std::size_t texture_offset,const ModelGeometryOptions& options={});
}
