#pragma once
#include "scene.h"
#include "face.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <map>
#include <memory>
namespace recvx {
struct ModelNodeOverride {std::optional<NJS_POINT3> position;std::optional<std::array<Angle,3>> angles;};
using ModelNodeOverrides=std::map<unsigned,ModelNodeOverride>;
struct BoneMatrix {NJS_MATRIX value;};
struct MotionClip {
    unsigned frames=0,flags=0;
    std::vector<std::uint16_t> attributes;
    bool interpolate=false;
    std::vector<std::vector<NJS_POINT3>> positions;
    std::vector<std::vector<std::array<std::int32_t,3>>> angles;
    // Motion.c:131 liest den Key bei frm_no>>16 ohne Grenze gegen frm_num.
    // Startet ein Wechsel hinter dem Ende (bhGetFrameNum klemmt auf die
    // Bildzahl, en01.c:10297 verschiebt ein zweites Mal um 16; chg_mtn_upper
    // gibt den Frame der laengeren Unterkoerpermotion weiter, en01.c:10314),
    // liest das Original den naechsten Block im MTN-Bytebild weiter. `tail_keys`
    // ist die Anzahl der Keys, die dafuer aus demselben Bereich an JEDEN
    // keyfuehrenden Kanal angehaengt wurden (0 = der Nachbarb lock liegt
    // ausserhalb; dann bleibt es bei der Meldung).
    unsigned tail_keys=0;
};
// fmCnkInitContext inserts procedural face children, then _fmSetObjP flattens
// the hierarchy. Keep script indices separate from the immutable skin indices.
struct ActorNodeLayout {std::vector<unsigned> script_to_model,motion_to_model,model_to_script;};
ActorNodeLayout decode_actor_node_layout(const std::vector<std::uint8_t>& model,const std::vector<std::uint8_t>& mask,bool face_actor=false);
MotionClip join_motion_segments(const MotionClip& lower,const MotionClip& upper);
std::vector<ModelPose> sample_motion_pose(const MotionClip& clip,std::int64_t frame,const std::vector<ModelPose>& rest,const std::vector<unsigned>& nodes={});
// Eine Motion mit mehr Kanaelen als das Modell Knoten hat schreibt im Original hinter
// objP (Motion.c:131). Der Host meldet die Luecke hier statt sie zu erfinden.
void bind_pose_gap(std::function<void(const char*)> sink);
MotionClip decode_motion_asset(const std::vector<std::uint8_t>& data);
class PlayerAsset {
public:
    explicit PlayerAsset(const std::vector<std::uint8_t>& data,bool face_actor=false);
    static PlayerAsset load(const std::filesystem::path& root,unsigned archive_entry=10,bool inspect_archive=false);
    std::vector<unsigned> motion_nodes(unsigned channels)const;
    std::vector<BoneMatrix> script_nodes(const std::vector<BoneMatrix>& model_nodes)const;
    unsigned script_index(unsigned model_node)const{return node_layout_.model_to_script.at(model_node);}
    unsigned model_index(unsigned script_node)const;
    std::size_t bones() const {return bind_pose_.size();}
    // Logische Knotenzahl = mlwP->obj_num nach bhInitMask: Modellknoten plus die
    // Tochterobjekte, die fmCnkInitContext einfuegt (face_bh.c:535-561 obj_num++).
    std::size_t script_count() const {return node_layout_.script_to_model.size();}
    std::size_t motions() const {return motions_.size();}
    const MotionClip& motion_clip(std::size_t i)const{return motions_.at(i).value();}
    // mnwP[i].datP != NULL (Motion.c:29). SetMtn* (Motion.c:131) laeuft ueber obj_num der MOTION:
    // weniger Kanaele als Knoten schreiben nur die ersten obj_num objP (Wesker 0013: Motion 9 mit
    // 20 Kanaelen auf 22 Knoten), die uebrigen behalten ihren Stand; mehr Kanaele meldet set_motion.
    bool has_motion(std::size_t i) const {return i<motions_.size() && motions_[i] && motions_[i]->positions.size()<=bones();}
    // mnwP[i] der Bibliothek ohne Knotenzahlpruefung: sys->plmthp traegt auch Motionen fremder
    // Modelle (en27.c:61 Mottenei auf plp->mdl[6]/[7] mit den Spielermotionen 93..96).
    const MotionClip* library_motion(std::size_t i)const{return i<motions_.size()&&motions_[i]?&*motions_[i]:nullptr;}
    unsigned motion_frames(std::size_t i) const;
    void install_weapon_motions(const std::vector<std::optional<MotionClip>>& clips);
    // bhGetTransZ tables: element zero is metadata; speed keys start at one.
    const std::vector<float>& translation_curve(std::size_t i)const{return translations_.at(i);}
    float translation_speed(std::size_t curve,std::int64_t frame_q16) const;
    // Healthy original idle/walk/run/back clips and signed translation curves.
    void locomotion(SceneSpawn& actor,std::size_t& motion,std::int64_t& frame_q16,int move,bool run,int turn)const;
    const FaceAsset* face()const{return face_.get();}
    const std::vector<SceneTexture>& textures() const {return textures_;}
    // Original keys are sampled on integer frames, selected by a signed 16.16 clock.
    std::vector<ModelPose> local_pose(std::size_t motion,std::int64_t frame_q16,const ModelNodeOverrides& overrides={})const;
    std::vector<ModelPose> local_pose_clip(const MotionClip& clip,std::int64_t frame_q16,const std::vector<unsigned>& nodes={},const ModelNodeOverrides& overrides={})const;
    std::vector<BoneMatrix> pose(std::size_t motion,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0,const ModelNodeOverrides& overrides={}) const;
    std::vector<BoneMatrix> pose_clip(const MotionClip& clip,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0,const std::vector<unsigned>& nodes={},const ModelNodeOverrides& overrides={}) const;
    Scene sample(std::size_t motion,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0,std::vector<BoneMatrix>* matrices_out=nullptr,const ModelNodeOverrides& overrides={},const FaceParameter* face_param=nullptr,const MorphSource* morph=nullptr,const std::vector<std::uint8_t>* live_model=nullptr) const;
    // bhReadPlayerData: further plp->mdl[n] (n >= 1) with their SKIN/texture bundle, e.g. Claire's
    // ponytail mdl[4] used by obwp[2] (bhResetPlayer). nullptr when the model has no objects.
    const PlayerAsset* model(unsigned index)const{return index>0&&index<=models_.size()?models_[index-1].get():nullptr;}
    std::size_t model_count()const{return models_.size()+1;}
    // mdl[index] (index >= 1) ist im Satz vorhanden, liess sich aber nicht
    // lesen (Ausnahme des Lesers). Ein Modellsatz mit 0 Objekten
    // (actor.cpp:493, `u16(6)==0`) ist KEIN Fehler und liefert hier false.
    bool model_failed(unsigned index)const{return index>0&&index<=model_failed_.size()&&model_failed_[index-1]!=0;}
    // Bind pose, posed into world space by `root` (object world matrix incl. link offset).
    // `angles` overrides the X/Y rotation of the first nodes (bhObjClpn writes objP[i].ang[0..1]).
    Scene sample_rest(const NJS_MATRIX& root,const std::vector<std::array<std::int32_t,2>>& angles={})const;
    NJS_POINT3 bind_position(std::size_t node)const{return bind_pose_.at(node).position;}
    // bhMlbBinRealize: the initial mlwP->objP pos/ang and evalflags of every physical node.
    const std::vector<ModelPose>& bind_pose()const{return bind_pose_;}
    // NJS_CNK_OBJECT child/sibling of a physical node; -1 = NULL.
    std::int32_t child(std::size_t node)const{return node<children_.size()?children_[node]:-1;}
    std::int32_t sibling(std::size_t node)const{return node<siblings_.size()?siblings_[node]:-1;}
    std::vector<std::uint32_t> node_evalflags()const;
    // MdlPut.c:262 bhCalcTree over a native objP pose below `root`. A node with
    // owP flg 0x1 keeps the matrix the caller passes in `owp_mtx` (njSetMatrix)
    // and its subtree continues from it.
    std::vector<BoneMatrix> tree_matrices(const std::vector<ModelPose>& pose,const NJS_MATRIX& root,const std::vector<std::uint32_t>& owp_flg={},const std::vector<BoneMatrix>& owp_mtx={})const;
    // Dasselbe mit objP/owP += first, obj_num -= first (en01.c:3148-3156):
    // bhCalcTree beginnt bei Knoten `first` unter `root` und schreibt nur die
    // owP der Knoten first.. in `owp_mtx`; die uebrigen bleiben unberuehrt.
    void tree_window(const std::vector<ModelPose>& pose,const NJS_MATRIX& root,unsigned first,const std::vector<std::uint32_t>& owp_flg,std::vector<BoneMatrix>& owp_mtx)const;
    Scene sample_clip(const MotionClip& clip,std::int64_t frame_q16,NJS_POINT3 position={},Angle yaw=0,const std::vector<unsigned>& nodes={},std::vector<BoneMatrix>* matrices_out=nullptr,const ModelNodeOverrides& overrides={},const FaceParameter* face_param=nullptr,const MorphSource* morph=nullptr,const std::vector<std::uint8_t>* live_model=nullptr) const;
    // Wie sample_clip, aber mit vorgegebener Wurzelmatrix statt T(position) * RY(yaw): bhCalcModel
    // (MdlPut.c:242) mit flg 0x80 setzt lkwkp->owP[lkono].mtx * T(lox,loy,loz) * RXYZ(ax,ay,az).
    Scene sample_clip_root(const MotionClip& clip,std::int64_t frame_q16,const NJS_MATRIX& root,const std::vector<unsigned>& nodes={},std::vector<BoneMatrix>* matrices_out=nullptr,const ModelNodeOverrides& overrides={},const MorphSource* morph=nullptr)const;
    Scene sample_root(std::size_t motion,std::int64_t frame_q16,const NJS_MATRIX& root,std::vector<BoneMatrix>* matrices_out=nullptr,const ModelNodeOverrides& overrides={},const MorphSource* morph=nullptr)const;
    Scene sample_work(const std::vector<ModelPose>& pose,const std::vector<BoneMatrix>& matrices,const std::vector<unsigned>& flags,std::vector<BoneMatrix>* matrices_out=nullptr,const FaceParameter* face_param=nullptr,const std::vector<std::uint8_t>* live_model=nullptr,const MorphSource* morph=nullptr)const;
    // sp_evt.c:1478-1500: npRetSkinConvert, dann njCnkEasyDrawObject auf dem lokalen objP.
    // Keine Motionabtastung und kein npCalcSkin/owP; evalflags und Objekt-Skalierung bleiben wirksam.
    Scene sample_tree(const std::vector<ModelPose>& pose,const std::vector<std::uint32_t>& flags,const std::vector<std::uint8_t>* live_model=nullptr)const;
    // bhEff136: npCalcSkin plus a direct model draw against committed physical owP matrices.
    // Does not sample motion, traverse a hidden ancestor, or apply a morph draw of another model.
    Scene sample_object_work(unsigned object,const std::vector<ModelPose>& pose,const std::vector<BoneMatrix>& matrices,const std::vector<std::uint32_t>& flags,const std::vector<std::uint8_t>* live_model=nullptr,bool skin_fm=false)const;
    const std::vector<std::uint8_t>& encoded_model()const{return model_;}
    // The model stream as the original holds it in memory after load: npSkinConvert
    // has rewritten every skinned vertex into its influencing bone's bind origin.
    // A writable copy for routines that edit vlist data in place (bhEne13_ScaleModel).
    std::vector<std::uint8_t> live_stream()const;
    const std::vector<std::uint8_t>& encoded_skin()const{return encoded_skin_;}
    const std::vector<std::vector<std::uint8_t>>& encoded_motions()const{return encoded_motions_;}
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
    std::vector<std::uint8_t> encoded_skin_;
    std::shared_ptr<FaceAsset> face_;
    std::vector<ModelPose> bind_pose_;
    ActorNodeLayout node_layout_;
    std::vector<std::int32_t> children_,siblings_;
    std::vector<SkinMesh> skin_;
    std::vector<std::optional<MotionClip>> motions_;
    std::vector<std::vector<std::uint8_t>> encoded_motions_;
    std::vector<SceneTexture> textures_;
    std::vector<std::vector<float>> translations_;
    std::vector<std::shared_ptr<PlayerAsset>> models_;
    std::vector<std::uint8_t> model_failed_;   // je models_[k], 1 = Lesefehler
    void load_models(const std::vector<std::uint8_t>& data,std::size_t model_end,std::size_t morph_end);
    std::vector<Matrix> matrices(const std::vector<ModelPose>& pose,NJS_POINT3 position,Angle yaw,bool translations_only=false) const;
    Scene render_pose(ModelGeometryOptions geometry,std::vector<Matrix> full,std::vector<BoneMatrix>* matrices_out,const FaceParameter* face_param,const std::vector<std::uint8_t>* live_model=nullptr)const;
};
// Appends character geometry without applying stage-object camera masks.
void append_actor(Scene& stage,const Scene& actor,std::size_t texture_base);
}
