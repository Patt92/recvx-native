#pragma once
#include "scene.h"
namespace recvx {
struct FaceParameter {
    float muscle[32]{};float jawang=0,jawtrans=0;NJS_POINT3 eye{};
    float tangx=0,tangy=0,tangz=0;short frame=0,flag=0;
};
std::vector<FaceParameter> decode_face_parameters(const std::vector<std::uint8_t>& bytes,std::size_t offset,unsigned count,bool lip=false,bool mask_lip_bank=false);
FaceParameter interpolate_face_parameters(const FaceParameter&,const FaceParameter&,float t,float level=1,unsigned mask=0xffffffffu);
FaceParameter lip_face_parameter(FaceParameter);
FaceParameter sample_face_parameters(const std::vector<FaceParameter>& keys,unsigned frame);
// Scalar translation of face_bh.c's MASK geometry routines. Data remain immutable;
// deformation precedes npCalcSkinFM and is applied to the same vertex indices.
class FaceAsset {
public:
    FaceAsset(const std::vector<std::uint8_t>& model,const std::vector<std::uint8_t>& mask);
    const std::vector<FaceParameter>& keys()const{return keys_;}
    void deform(std::vector<std::uint8_t>& model,const FaceParameter& param)const;
private:
    struct Mesh {unsigned object=0;std::size_t payload=0;unsigned count=0,stride=0,normal=0;int gaze_sign=1;std::array<int,3> angles{};NJS_POINT3 position{};};
    struct Connection {unsigned muscle;NJS_POINT3 vector;float scale;};
    struct Group {unsigned vertex;std::vector<Connection> muscles;};
    struct Weight {unsigned vertex;float rate;};
    Mesh face_,tongue_;std::optional<Mesh> tooth_;std::array<std::optional<Mesh>,9> eyes_{};
    std::vector<Group> groups_;std::vector<Weight> jaw_,tang_;
    std::vector<FaceParameter> keys_;NJS_MATRIX jaw_to_,jaw_from_;
};
}
