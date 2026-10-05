#pragma once
#include "ninja_matrix.h"
#include <array>
#include <cstdint>
#include <vector>
#include <optional>
namespace recvx {
struct SceneVertex { NJS_POINT3 position; float u=0,v=0; NJS_VECTOR normal{}; };
struct SceneTexture {
    unsigned width=0,height=0;
    std::vector<std::uint32_t> pixels;
};
struct SceneTriangle {
    std::array<SceneVertex,3> vertices;
    std::uint32_t color=0xffb2b2b2;
    std::uint16_t texture=0;
    std::uint16_t object=0;
    bool textured=false;
    bool camera_mask=true;
    bool world_lit=false;
    unsigned light_category=1;
};
struct SceneCamera {
    NJS_POINT3 position{};
    std::array<std::int32_t,3> angles{};
    float distance=0;
    std::int32_t perspective=8192;
    std::array<std::uint32_t,16> hidden_objects{};
    std::uint16_t flags=0;
    NJS_POINT3 cut_origin{},cut_extent{};
    float follow_x=0,follow_z=0,target_height=0;
    std::array<float,4> y_offsets{};
    std::array<std::int32_t,2> angle_limits{};
    std::array<float,4> roll_offsets{},perspectives{};
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
std::optional<std::size_t> camera_for_position(const Scene& scene,NJS_POINT3 position,std::int8_t floor=0);
// bhInitActiveCamera target evaluation (no transition/event-camera state).
SceneCamera camera_for_actor(const Scene& scene,std::size_t camera,NJS_POINT3 position,std::int8_t floor=0);
struct ModelPose {
    NJS_POINT3 position{};
    std::array<std::int32_t,3> angles{};
};
struct ModelGeometryOptions {
    NJS_MATRIX transform{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::vector<ModelPose> pose;
    std::vector<std::uint32_t> evalflags;
    std::vector<std::vector<NJS_POINT3>> world_vertices;
    std::vector<std::vector<NJS_VECTOR>> world_normals;
};
// Standalone MDL/PVP path shared by stage and animated character loading.
Scene decode_model_asset(const std::vector<std::uint8_t>& data,std::size_t model_offset,
                         std::size_t texture_offset,const ModelGeometryOptions& options={});
}
