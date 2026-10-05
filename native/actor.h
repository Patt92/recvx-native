#pragma once
#include "scene.h"
#include <filesystem>
#include <optional>
namespace recvx {
struct BoneMatrix {NJS_MATRIX value;};
struct MotionClip {
    unsigned frames=0;
    bool interpolate=false;
    std::vector<std::vector<NJS_POINT3>> positions;
    std::vector<std::vector<std::array<std::int32_t,3>>> angles;
};
MotionClip decode_motion_asset(const std::vector<std::uint8_t>& data);
class PlayerAsset {
public:
    explicit PlayerAsset(const std::vector<std::uint8_t>& data);
    static PlayerAsset load(const std::filesystem::path& root,unsigned archive_entry=10,bool inspect_archive=false);
    std::size_t bones() const {return bind_pose_.size();}
    std::size_t motions() const {return motions_.size();}
    bool has_motion(std::size_t i) const {return i<motions_.size() && motions_[i] && motions_[i]->positions.size()==bones();}
    unsigned motion_frames(std::size_t i) const;
    void install_weapon_motions(const std::vector<std::optional<MotionClip>>& clips);
    // bhGetTransZ tables: element zero is metadata; speed keys start at one.
    float translation_speed(std::size_t curve,std::int64_t frame_q16) const;
    // Healthy original idle/walk/run/back clips and signed translation curves.
    void locomotion(SceneSpawn& actor,std::size_t& motion,std::int64_t& frame_q16,int move,bool run,int turn)const;
    const std::vector<SceneTexture>& textures() const {return textures_;}
    // Original keys are sampled on integer frames, selected by a signed 16.16 clock.
    std::vector<BoneMatrix> pose(std::size_t motion,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0) const;
    std::vector<BoneMatrix> pose_clip(const MotionClip& clip,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0,const std::vector<unsigned>& nodes={}) const;
    Scene sample(std::size_t motion,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0,std::vector<BoneMatrix>* matrices_out=nullptr) const;
    Scene sample_clip(const MotionClip& clip,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0,const std::vector<unsigned>& nodes={},std::vector<BoneMatrix>* matrices_out=nullptr) const;
private:
    struct SkinMesh {
        std::size_t object=0;
        std::vector<NJS_POINT3> local_vertices;
        std::vector<NJS_VECTOR> local_normals;
        std::vector<std::uint32_t> weights;
    };
    struct Matrix {NJS_MATRIX value;};
    std::vector<ModelPose> sampled_pose(const MotionClip& clip,std::int64_t frame_q16,const std::vector<unsigned>& nodes)const;
    std::vector<std::uint8_t> model_;
    std::vector<ModelPose> bind_pose_;
    std::vector<std::int32_t> children_,siblings_;
    std::vector<SkinMesh> skin_;
    std::vector<std::optional<MotionClip>> motions_;
    std::vector<SceneTexture> textures_;
    std::vector<std::vector<float>> translations_;
    std::vector<Matrix> matrices(const std::vector<ModelPose>& pose,NJS_POINT3 position,Angle yaw,bool translations_only=false) const;
};
// Appends character geometry without applying stage-object camera masks.
void append_actor(Scene& stage,const Scene& actor,std::size_t texture_base);
}
