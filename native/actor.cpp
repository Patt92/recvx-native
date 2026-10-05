#include "actor.h"
#include "archive.h"
#include "binary_view.h"
#include "asset_io.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
namespace recvx {
namespace {
std::vector<std::uint8_t> slice(const std::vector<std::uint8_t>& data,std::size_t begin,std::size_t size) {
    BinaryView(data).require(begin,size);
    return {data.begin()+begin,data.begin()+begin+size};
}
}
ActorNodeLayout decode_actor_node_layout(const std::vector<std::uint8_t>& model,const std::vector<std::uint8_t>& mask,bool face_actor){
    BinaryView v(model);const auto count=v.u16(6),base=v.u16(4);const auto nodes=v.relative(base,v.u32(12),count*52);
    if(!count||count>256)throw std::runtime_error("invalid face skeleton size");
    auto link=[&](unsigned offset)->int{if(offset==0xffffffff)return -1;const auto p=v.relative(base,offset,52);if(p<nodes||(p-nodes)%52||(p-nodes)/52>=count)throw std::runtime_error("face hierarchy link outside table");return int((p-nodes)/52);};
    std::vector<unsigned> order,meshes;std::vector<bool> visited(count);
    std::function<void(int)> walk=[&](int n){while(n>=0){if(visited[n])throw std::runtime_error("cyclic face hierarchy");visited[n]=true;order.push_back(n);if(v.u32(nodes+n*52+4)!=0xffffffff)meshes.push_back(n);walk(link(v.u32(nodes+n*52+44)));n=link(v.u32(nodes+n*52+48));}};walk(0);
    if(order.size()!=count)throw std::runtime_error("unreachable face node");
    std::vector<unsigned> inserted(count);if(!mask.empty()){BinaryView m(mask);m.require(0,64);if(m.u32(0)!=0x4b53414d)throw std::runtime_error("invalid face mask header");
        // _fmCnkSearchObject counts objects with a mesh and skips the newly
        // inserted children. Tooth, tongue and each valid eye add one child.
        for(unsigned field=0x35;field<0x40;++field){const auto id=m.u8(field);if(id!=0xff&&id<meshes.size())++inserted[meshes[id]];}}
    ActorNodeLayout out;out.model_to_script.resize(count);
    for(const auto n:order){out.model_to_script[n]=out.script_to_model.size();out.script_to_model.push_back(n);for(unsigned i=0;i<inserted[n];++i)out.script_to_model.push_back(n);}
    // bhInitMask marks owP[6..14] as procedural. SetMtn* skips those
    // entries without consuming a motion channel. Skin indices remain raw.
    for(unsigned logical=0;logical<out.script_to_model.size();++logical)if((mask.empty()&&!face_actor)||logical<6||logical>(mask.empty()?10u:14u))out.motion_to_model.push_back(out.script_to_model[logical]);
    return out;
}
std::vector<unsigned> PlayerAsset::motion_nodes(unsigned channels)const{
    if(channels==node_layout_.motion_to_model.size()&&channels!=bones())return node_layout_.motion_to_model;
    return {};
}
std::vector<BoneMatrix> PlayerAsset::script_nodes(const std::vector<BoneMatrix>& model_nodes)const{
    std::vector<BoneMatrix> out;out.reserve(node_layout_.script_to_model.size());for(const auto index:node_layout_.script_to_model)out.push_back(model_nodes.at(index));return out;
}
MotionClip join_motion_segments(const MotionClip& lower,const MotionClip& upper){
    if(!lower.frames||lower.frames!=upper.frames||lower.interpolate!=upper.interpolate||lower.positions.size()!=lower.angles.size()||upper.positions.size()!=upper.angles.size())throw std::runtime_error("incompatible segmented motion clocks");
    auto out=lower;out.positions.insert(out.positions.end(),upper.positions.begin(),upper.positions.end());out.angles.insert(out.angles.end(),upper.angles.begin(),upper.angles.end());return out;
}
MotionClip decode_motion_asset(const std::vector<std::uint8_t>& data) {
    BinaryView v(data);
    if(v.u8(0)!='M'||v.u8(1)!='T'||v.u8(2)!='N'||!(v.u8(3)&0xf0))
        throw std::runtime_error("unsupported player motion format");
    const auto base=v.relative(0,v.u16(4),0),count=std::size_t(v.u16(6));
    if(!count||count>256)throw std::runtime_error("invalid motion skeleton size");
    const auto header=v.relative(base,v.u32(8),12);
    MotionClip clip;clip.frames=v.u32(header+4);clip.interpolate=bool(v.u8(3)&0x20);
    if(!clip.frames||clip.frames>65536||v.u16(header+8)!=3)throw std::runtime_error("unsupported player motion keys");
    const auto channels=v.relative(base,v.u32(header),count*8);
    const auto attribute=v.u32(12);
    if(attribute!=0xffffffff)v.relative(base,attribute,clip.frames*2);
    clip.positions.resize(count);clip.angles.resize(count);
    for(std::size_t i=0;i<count;++i) {
        auto offset=v.u32(channels+i*8);
        if(offset!=0xffffffff) {
            const auto p=v.relative(base,offset,std::size_t(clip.frames)*12);
            auto& keys=clip.positions[i];keys.reserve(clip.frames);
            for(unsigned f=0;f<clip.frames;++f)keys.push_back({v.f32(p+f*12),v.f32(p+f*12+4),v.f32(p+f*12+8)});
        }
        offset=v.u32(channels+i*8+4);
        if(offset!=0xffffffff) {
            const auto p=v.relative(base,offset,std::size_t(clip.frames)*6);
            auto& keys=clip.angles[i];keys.reserve(clip.frames);
            for(unsigned f=0;f<clip.frames;++f)keys.push_back({v.u16(p+f*6),v.u16(p+f*6+2),v.u16(p+f*6+4)});
        }
    }
    return clip;
}
PlayerAsset PlayerAsset::load(const std::filesystem::path& root,unsigned archive_entry,bool inspect_archive) {
    if(archive_entry<10||archive_entry>14)throw std::runtime_error("player archive entry must be 10 through 14");
    const auto loose=root/"archives/SYSTEM"/(asset_number(archive_entry)+".bin");
    if(std::filesystem::is_regular_file(loose))return PlayerAsset(read_asset_file(loose,8*1024*1024));
    if(!inspect_archive)throw std::runtime_error("missing cooked player asset: "+loose.string());
    return PlayerAsset(AfsArchive(disc_file(root,"SYSTEM.AFS")).read(archive_entry,8*1024*1024));
}
PlayerAsset::PlayerAsset(const std::vector<std::uint8_t>& data,bool face_actor) {
    // bhReadPlayerData: sized model/skin section, sized motion list, morph block,
    // then per-model PVP bundles with optional 32-byte alignment.
    BinaryView file(data);
    const auto model_end=file.relative(4,file.u32(0),4);
    std::size_t p=4;std::vector<std::uint8_t> skin_data,mask_data;
    bool terminated=false;
    for(unsigned record=0;record<256 && p<model_end;++record) {
        if(model_end-p<4)throw std::runtime_error("truncated player model record header");
        const auto size=file.u32(p);p+=4;
        if(size==0xffffffff){terminated=true;break;}
        if(size>model_end-p)throw std::runtime_error("player model block outside section");
        if(size) {
            if(size<4)throw std::runtime_error("truncated player model magic");
            const auto magic=file.u32(p);
            if(magic==0x4b53414d)mask_data=slice(data,p,size);
            else if(magic==0x4e494b53 && model_.empty())skin_data=slice(data,p+4,size-4);
            else if((magic&0xffffff)==0x4c444d && model_.empty())model_=slice(data,p,size);
            else if((magic&0xffffff)!=0x4c444d && magic!=0x4e494b53)throw std::runtime_error("unknown player model record");
        }
        p+=size;
    }
    if(!terminated||model_.empty())throw std::runtime_error("missing player model/terminator");
    BinaryView mdl(model_);
    if(!(mdl.u8(3)&0x80))throw std::runtime_error("player model is not Ninja Chunk");
    const auto base=mdl.relative(0,mdl.u16(4),0),count=std::size_t(mdl.u16(6));
    if(!count||count>256)throw std::runtime_error("invalid player bone count");
    const auto nodes=mdl.relative(base,mdl.u32(12),count*52);
    auto link=[&](std::uint32_t off)->std::int32_t {
        if(off==0xffffffff)return -1;
        const auto at=mdl.relative(base,off,52);
        if(at<nodes||(at-nodes)%52||(at-nodes)/52>=count)throw std::runtime_error("bone link outside node table");
        return static_cast<std::int32_t>((at-nodes)/52);
    };
    for(std::size_t i=0;i<count;++i) {
        const auto o=nodes+i*52;
        bind_pose_.push_back({{mdl.f32(o+8),mdl.f32(o+12),mdl.f32(o+16)}, {mdl.i32(o+20),mdl.i32(o+24),mdl.i32(o+28)}});
        children_.push_back(link(mdl.u32(o+44)));siblings_.push_back(link(mdl.u32(o+48)));
    }
    node_layout_=decode_actor_node_layout(model_,mask_data,face_actor);
    const auto bind=matrices(bind_pose_,{},0,true);
    if(!skin_data.empty()) {
        BinaryView sk(skin_data);std::size_t at=0;
        // npSkinConvMain: one record per object in hierarchy order (child before
        // sibling); objects with evalflags 0x20000000 consume no record.
        std::vector<std::size_t> order;std::vector<bool> seen(count);
        std::function<void(std::int32_t)> walk=[&](std::int32_t node){while(node>=0){if(seen[std::size_t(node)])throw std::runtime_error("cyclic bone hierarchy");seen[std::size_t(node)]=true;
            if(!(mdl.u32(nodes+std::size_t(node)*52)&0x20000000))order.push_back(std::size_t(node));walk(children_[std::size_t(node)]);node=siblings_[std::size_t(node)];}};
        walk(0);
        for(const auto bone:order) {
            const auto flags=sk.u32(at);at+=4;
            if(flags&~3u)throw std::runtime_error("unsupported skin flags");
            if(!flags)continue;
            const auto group=sk.u32(at);at+=4;
            if(group>=256)throw std::runtime_error("skin group outside bounds");
            if(!(flags&2))continue;
            const auto vertices=sk.u32(at);at+=4;
            if(!vertices||vertices>65535)throw std::runtime_error("invalid skin vertex count");
            sk.require(at,std::size_t(vertices)*4);
            const auto model=mdl.relative(base,mdl.u32(nodes+bone*52+4),24);
            const auto chunk=mdl.relative(base,mdl.u32(model),64);
            if(mdl.u8(chunk)!=51||mdl.u16(chunk+6)!=vertices)throw std::runtime_error("skin does not match PS2 vertex chunk");
            mdl.require(chunk+64,std::size_t(vertices)*32);
            SkinMesh mesh;mesh.object=bone;
            for(unsigned i=0;i<vertices;++i) {
                const auto weight=sk.u32(at);at+=4;
                const auto influence=weight>>24,level=(weight>>16)&255;
                if(influence>=count||level>9)throw std::runtime_error("invalid skin bone/weight");
                const auto v=chunk+64+i*32;
                // npSkinConvert translates from the mesh's bind origin into its
                // influencing bone's bind origin before runtime deformation.
                mesh.local_vertices.push_back({mdl.f32(v)+bind[bone].value[12]-bind[influence].value[12],
                    mdl.f32(v+4)+bind[bone].value[13]-bind[influence].value[13],
                    mdl.f32(v+8)+bind[bone].value[14]-bind[influence].value[14]});
                mesh.local_normals.push_back({mdl.f32(v+16),mdl.f32(v+20),mdl.f32(v+24)});
                mesh.weights.push_back(weight);
            }
            skin_.push_back(std::move(mesh));
        }
        // npSkinConvSub stops after the last object; room object tables may carry
        // words after the 0xffffffff terminator, which the original never reads.
        if(at!=skin_data.size()&&!(at+4<=skin_data.size()&&sk.u32(at)==0xffffffff))throw std::runtime_error("skin records do not match skeleton");
    }
    p=model_end+4;const auto motion_end=file.relative(p,file.u32(model_end),4);terminated=false;
    for(unsigned index=0;index<1024 && p<motion_end;++index) {
        if(motion_end-p<4)throw std::runtime_error("truncated player motion record header");
        const auto size=file.u32(p);p+=4;
        if(size==0xffffffff){terminated=true;break;}
        if(size>motion_end-p)throw std::runtime_error("player motion block outside section");
        motions_.push_back(size?std::optional<MotionClip>(decode_motion_asset(slice(data,p,size))):std::nullopt);
        p+=size;
    }
    if(!terminated||motions_.empty())throw std::runtime_error("missing player motion list");
    const auto morph_end=file.relative(motion_end+4,file.u32(motion_end),4);
    p=motion_end+4;terminated=false;
    for(unsigned index=0;index<1024 && p<morph_end;++index) {
        if(morph_end-p<4)throw std::runtime_error("truncated translation curve header");
        const auto size=file.u32(p);p+=4;
        if(size==0xffffffff){terminated=true;break;}
        if(size<8||size%4||size>morph_end-p)throw std::runtime_error("invalid player translation curve");
        std::vector<float> curve;curve.reserve(size/4);
        for(std::size_t i=0;i<size/4;++i)curve.push_back(file.f32(p+i*4));
        translations_.push_back(std::move(curve));p+=size;
    }
    if(!terminated)throw std::runtime_error("missing translation curve terminator");
    p=morph_end;auto texture_size=file.u32(p);p+=4;
    if(texture_size&0x80000000){p=(p+31)&~std::size_t(31);texture_size&=0x7fffffff;}
    file.require(p,texture_size);
    auto textures=slice(data,p,texture_size);
    if(!texture_size)return; // room actor without textures (texP NULL); players always carry a bundle
    // Decode texture bundle together with the same model parser; geometry is
    // sampled separately below so textures do not get decoded every frame.
    const auto texture_offset=model_.size();
    auto combined=model_;combined.insert(combined.end(),textures.begin(),textures.end());
    ModelGeometryOptions options;options.pose=bind_pose_;
    textures_=decode_model_asset(combined,0,texture_offset,options).textures;
    if(textures_.empty())throw std::runtime_error("player texture bundle is empty");
}
std::vector<PlayerAsset::Matrix> PlayerAsset::matrices(const std::vector<ModelPose>& pose,NJS_POINT3 position,Angle yaw,bool translations_only) const {
    std::vector<Matrix> result(pose.size());std::vector<bool> visited(pose.size());
    Matrix root;njUnitMatrix(&root.value);njTranslate(&root.value,position.x,position.y,position.z);njRotateY(&root.value,yaw);
    std::function<void(std::int32_t,Matrix&,unsigned)> visit=[&](std::int32_t node,Matrix& parent,unsigned depth) {
        if(node<0)return;
        if(depth>256||std::size_t(node)>=pose.size()||visited[node])throw std::runtime_error("invalid/cyclic skeleton hierarchy");
        visited[node]=true;auto& world=result[node];njSetMatrix(&world.value,&parent.value);
        const auto& transform=pose[node];njTranslate(&world.value,transform.position.x,transform.position.y,transform.position.z);
        if(!translations_only)njRotateXYZ(&world.value,transform.angles[0],transform.angles[1],transform.angles[2]);
        visit(children_[node],world,depth+1);visit(siblings_[node],parent,depth);
    };
    visit(0,root,0);
    if(std::find(visited.begin(),visited.end(),false)!=visited.end())throw std::runtime_error("unreachable player bone");
    return result;
}
void PlayerAsset::install_weapon_motions(const std::vector<std::optional<MotionClip>>& clips){
    if(clips.size()>64)throw std::runtime_error("too many weapon motions");
    for(const auto& clip:clips)if(clip&&clip->positions.size()!=bones())throw std::runtime_error("weapon motion skeleton differs from player");
    motions_.resize(100+clips.size());std::copy(clips.begin(),clips.end(),motions_.begin()+100);
}
unsigned PlayerAsset::motion_frames(std::size_t i) const {
    if(!has_motion(i))throw std::runtime_error("player motion is unavailable");
    return motions_[i]->frames;
}
float PlayerAsset::translation_speed(std::size_t curve,std::int64_t frame_q16) const {
    if(curve>=translations_.size())throw std::runtime_error("player translation curve unavailable");
    const auto& keys=translations_[curve];const auto length=std::int64_t(keys.size()-1)*65536;
    auto frame=frame_q16%length;if(frame<0)frame+=length;
    return keys[std::size_t(frame/65536)+1];
}
void PlayerAsset::locomotion(SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,int move,bool run,int turn)const {
    actor.yaw=(actor.yaw-turn*int(182.04445f*4.5f))&65535;
    const std::size_t next=move>0?(run?4:0):(move<0?16:42);
    if(!has_motion(next))throw std::runtime_error("player locomotion clip unavailable");
    if(next!=motion){motion=next;frame=next==0?5*65536:next==16?25*65536:0;}
    // The dumped backward curve already stores negative deltas. Preserve its
    // sign instead of reversing the animation or negating it a second time.
    if(move){const float step=translation_speed(move>0?(run?3:0):6,frame);
        actor.position.x-=step*njSin(actor.yaw);actor.position.z-=step*njCos(actor.yaw);}
    frame=(frame+65536)%(std::int64_t(motion_frames(motion))*65536);
}
Scene PlayerAsset::sample(std::size_t motion,std::int64_t frame_q16,NJS_POINT3 position,Angle yaw,std::vector<BoneMatrix>* matrices_out,const ModelNodeOverrides& overrides) const {
    if(!has_motion(motion))throw std::runtime_error("actor motion unavailable");
    return sample_clip(*motions_.at(motion),frame_q16,position,yaw,{},matrices_out,overrides);
}
std::vector<ModelPose> PlayerAsset::sampled_pose(const MotionClip& clip,std::int64_t frame_q16,const std::vector<unsigned>& nodes)const{
    if(!clip.frames||clip.positions.size()!=clip.angles.size()||clip.positions.size()>bones())throw std::runtime_error("room motion does not fit actor skeleton");
    const auto length=std::int64_t(clip.frames)*65536;
    auto clock=frame_q16%length;if(clock<0)clock+=length;const auto frame=std::size_t(clock/65536);
    auto pose=bind_pose_;
    const auto next=(frame+1)%clip.frames;const float blend=clip.interpolate?float(clock&65535)/65536:0;
    if(!nodes.empty()&&nodes.size()!=clip.positions.size())throw std::runtime_error("motion node map size differs");
    for(std::size_t i=0;i<clip.positions.size();++i) {
        const auto node=nodes.empty()?i:nodes[i];if(node>=bones())throw std::runtime_error("motion node map outside actor");
        if(!clip.positions[i].empty()){const auto a=clip.positions[i].at(frame),b=clip.positions[i].at(next);pose[node].position={a.x+(b.x-a.x)*blend,a.y+(b.y-a.y)*blend,a.z+(b.z-a.z)*blend};}
        if(clip.angles[i].empty())pose[node].angles={};else for(unsigned axis=0;axis<3;++axis){const auto a=clip.angles[i].at(frame)[axis],b=clip.angles[i].at(next)[axis];int delta=(b-a)&65535;if(delta>=32768)delta-=65536;pose[node].angles[axis]=(a+int(delta*blend))&65535;}
    }
    return pose;
}
unsigned PlayerAsset::model_index(unsigned script_node)const{if(script_node>=node_layout_.script_to_model.size()||node_layout_.script_to_model[script_node]>=bones())throw std::runtime_error("model control addresses a procedural or missing node");return node_layout_.script_to_model[script_node];}
std::vector<ModelPose> PlayerAsset::local_pose(std::size_t motion,std::int64_t frame,const ModelNodeOverrides& overrides)const{if(!has_motion(motion))throw std::runtime_error("actor motion unavailable");return local_pose_clip(*motions_.at(motion),frame,{},overrides);}
std::vector<ModelPose> PlayerAsset::local_pose_clip(const MotionClip& clip,std::int64_t frame,const std::vector<unsigned>& nodes,const ModelNodeOverrides& overrides)const{auto pose=sampled_pose(clip,frame,nodes);for(const auto& pair:overrides){if(pair.first>=pose.size())throw std::runtime_error("model control node outside skeleton");const auto& o=pair.second;if(o.position){const auto p=*o.position;if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("nonfinite model control position");pose[pair.first].position=p;}if(o.angles)pose[pair.first].angles=*o.angles;}return pose;}
std::vector<BoneMatrix> PlayerAsset::pose(std::size_t motion,std::int64_t frame_q16,NJS_POINT3 position,Angle yaw,const ModelNodeOverrides& overrides)const{
    if(!has_motion(motion))throw std::runtime_error("actor motion unavailable");return pose_clip(*motions_.at(motion),frame_q16,position,yaw,{},overrides);
}
std::vector<BoneMatrix> PlayerAsset::pose_clip(const MotionClip& clip,std::int64_t frame_q16,NJS_POINT3 position,Angle yaw,const std::vector<unsigned>& nodes,const ModelNodeOverrides& overrides)const{
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::runtime_error("nonfinite actor position");
    const auto matrices_=matrices(local_pose_clip(clip,frame_q16,nodes,overrides),position,yaw);std::vector<BoneMatrix> result;for(const auto& matrix:matrices_){BoneMatrix bone;std::copy_n(matrix.value,16,bone.value);result.push_back(bone);}return result;
}
Scene PlayerAsset::sample_clip(const MotionClip& clip,std::int64_t frame_q16,NJS_POINT3 position,Angle yaw,const std::vector<unsigned>& nodes,std::vector<BoneMatrix>* matrices_out,const ModelNodeOverrides& overrides) const {
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::runtime_error("nonfinite actor position");
    ModelGeometryOptions geometry;geometry.pose=local_pose_clip(clip,frame_q16,nodes,overrides);
    auto full=matrices(geometry.pose,position,yaw),without_rotation=full;
    if(matrices_out){matrices_out->clear();for(const auto& matrix:full){BoneMatrix copy;std::copy_n(matrix.value,16,copy.value);matrices_out->push_back(copy);}}
    for(std::size_t i=0;i<bones();++i) {
        const auto& a=geometry.pose[i].angles;
        // npInitCalcSkin cancels each bone's current local rotation for the
        // first matrix; npCalcSkin blends it with the full bone matrix.
        njRotXYZ(&without_rotation[i].value,-a[0],-a[1],-a[2]);
    }
    geometry.world_vertices.resize(bones());
    geometry.world_normals.resize(bones());
    for(const auto& mesh:skin_) {
        auto& vertices=geometry.world_vertices[mesh.object];vertices.reserve(mesh.weights.size());
        auto& normals=geometry.world_normals[mesh.object];normals.reserve(mesh.weights.size());
        for(std::size_t i=0;i<mesh.weights.size();++i) {
            const auto word=mesh.weights[i],bone=word>>24,level=(word>>16)&255;
            auto point=mesh.local_vertices[i];NJS_POINT3 a,b;
            njCalcPoint(&full[bone].value,&point,&b);
            if(level==9)vertices.push_back(b);
            else {
                njCalcPoint(&without_rotation[bone].value,&point,&a);
                static constexpr float levels[]{0.1f,0.2f,0.3f,0.4f,0.5f,0.6f,0.7f,0.8f,0.9f,1.0f};
                const float blend=levels[level];
                vertices.push_back({a.x+(b.x-a.x)*blend,a.y+(b.y-a.y)*blend,a.z+(b.z-a.z)*blend});
            }
            // npCalcSkin transforms and blends the normal using the same two
            // influencing-bone matrices, with no translation component.
            NJS_VECTOR na,nb;auto normal=mesh.local_normals[i];njCalcVector(&full[bone].value,&normal,&nb);
            if(level==9)normals.push_back(nb);else{njCalcVector(&without_rotation[bone].value,&normal,&na);const float weight=(level+1)*.1f;normals.push_back({na.x+(nb.x-na.x)*weight,na.y+(nb.y-na.y)*weight,na.z+(nb.z-na.z)*weight});}
        }
    }
    njTranslate(&geometry.transform,position.x,position.y,position.z);njRotateY(&geometry.transform,yaw);
    auto scene=decode_model_asset(model_,0,0,geometry);
    // bhSetEneMdl leaves texP NULL for models without a texture list.
    if(textures_.empty())for(auto& triangle:scene.triangles)triangle.textured=false;
    return scene;
}
void append_actor(Scene& stage,const Scene& actor,std::size_t texture_base) {
    if(texture_base>std::numeric_limits<std::uint16_t>::max())throw std::runtime_error("texture offset exceeds renderer range");
    for(auto triangle:actor.triangles) {
        if(triangle.textured&&texture_base+triangle.texture>=stage.textures.size())throw std::runtime_error("actor texture outside scene table: index "+std::to_string(triangle.texture)+", base "+std::to_string(texture_base)+", count "+std::to_string(stage.textures.size()));
        triangle.texture=static_cast<std::uint16_t>(texture_base+triangle.texture);triangle.camera_mask=false;
        stage.triangles.push_back(triangle);
    }
}
}
