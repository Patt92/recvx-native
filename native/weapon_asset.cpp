#include "weapon_asset.h"
#include "asset_io.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <algorithm>
#include <functional>
namespace recvx {
WeaponAsset WeaponAsset::decode(const std::vector<std::uint8_t>& bytes){
    BinaryView v(bytes);WeaponAsset out;std::size_t p=0;
    auto section=[&](unsigned alignment){auto n=v.u32(p);p+=4;if(n&0x80000000){p=(p+alignment-1)&~std::size_t(alignment-1);n&=0x7fffffff;}v.require(p,n);const auto start=p;p+=n;return std::make_pair(start,n);};
    // bhReadWeaponData: right model/PVP, left model/PVP, sized MTN list.
    bool right_model=true;
    for(auto* target:{&out.right,&out.left}){auto model=section(4),texture=section(64);if(model.second){*target=decode_model_asset(bytes,model.first,texture.second?texture.first:0);
            if(right_model){ // objP-Baum (NJS_CNK_OBJECT: evalflags, model, pos[3], ang[3], scl[3], child, sibling; 52 Bytes)
                const auto at=model.first;v.require(at,16);const auto base=v.relative(at,v.u16(at+4),0);const std::size_t count=v.u16(at+6);
                const auto nodes=v.relative(base,v.u32(at+12),count*52);
                auto link=[&](std::uint32_t off)->std::int32_t{if(off==0xffffffffu)return -1;const auto n=v.relative(base,off,52);
                    if(n<nodes||(n-nodes)%52||(n-nodes)/52>=count)throw std::runtime_error("weapon object link outside node table");return std::int32_t((n-nodes)/52);};
                for(std::size_t i=0;i<count;++i){const auto o=nodes+i*52;
                    out.right_nodes.push_back({{{v.f32(o+8),v.f32(o+12),v.f32(o+16)},{v.i32(o+20),v.i32(o+24),v.i32(o+28)}},link(v.u32(o+44)),link(v.u32(o+48))});}}}
        right_model=false;}
    auto list=section(4);p=list.first;const auto end=p+list.second;bool terminated=!list.second;
    while(p<end&&out.motions.size()<64){const auto n=v.u32(p);p+=4;if(n==0xffffffff){terminated=true;break;}if(n>end-p)throw std::runtime_error("weapon motion exceeds section");out.motions.push_back(n?std::optional<MotionClip>(decode_motion_asset({bytes.begin()+p,bytes.begin()+p+n})):std::nullopt);p+=n;}
    if(!terminated)throw std::runtime_error("unterminated weapon motion list");return out;
}
WeaponAsset WeaponAsset::load(const std::filesystem::path& data,unsigned player,unsigned weapon,const std::filesystem::path& mods){
    if(player>4||weapon>20)throw std::runtime_error("weapon archive identity outside table");const auto name=asset_number(20+player*30+weapon)+".bin";auto path=mods/"weapons"/name;if(mods.empty()||!std::filesystem::is_regular_file(path))path=data/"archives/SYSTEM"/name;
    auto out=decode(read_asset_file(path,8*1024*1024));apply_texture_files(out.right,data,mods,std::filesystem::path("weapons")/asset_number(20+player*30+weapon)/"right");apply_texture_files(out.left,data,mods,std::filesystem::path("weapons")/asset_number(20+player*30+weapon)/"left");return out;
}
std::optional<std::array<float,16>> WeaponAsset::right_node_matrix(const NJS_MATRIX& hand,int ax,int ay,int az,unsigned node,const std::map<unsigned,ModelPose>& overrides)const{
    if(node>=right_nodes.size())return std::nullopt;
    NJS_MATRIX root;std::copy_n(hand,16,root);njRotateXYZ(&root,ax,ay,az); // lox/loy/loz 0 (bhSetWeapon nullt das Werk)
    std::optional<std::array<float,16>> found;
    // MdlPut.c:262 bhCalcTree: Geschwister erben die Matrix des Elternteils, Kinder die des Knotens.
    std::function<void(std::int32_t,const NJS_MATRIX&,unsigned)> visit=[&](std::int32_t i,const NJS_MATRIX& parent,unsigned depth){
        if(i<0||depth>64||found)return;const auto& n=right_nodes[std::size_t(i)];
        const auto at=overrides.find(unsigned(i));const auto& p=at!=overrides.end()?at->second:n.pose;
        NJS_MATRIX m;std::copy_n(parent,16,m);njTranslate(&m,p.position.x,p.position.y,p.position.z);njRotateXYZ(&m,p.angles[0],p.angles[1],p.angles[2]);
        if(unsigned(i)==node){found.emplace();std::copy_n(m,16,found->data());return;}
        visit(n.child,m,depth+1);visit(n.sibling,parent,depth+1);};
    visit(0,root,0);
    return found;
}
void WeaponAsset::append(Scene& scene,const std::vector<BoneMatrix>& bones,bool right_part2_hidden,bool models_hidden)const{
    if(models_hidden)return; // objitm.c:364: mdflg 0x1 haelt obwp[0]/[1] aus sys->ob_hlg
    unsigned i=0;for(const auto* source:{&right,&left}){const unsigned bone=i++?13:9;if(source->triangles.empty()||bone>=bones.size())continue;auto object=*source;
        // effsub1.c:2356: objP[2].evalflags 0x8 betrifft nur dessen Modell, nicht Kinder/Schwester.
        if(bone==9&&right_part2_hidden){
            object.ambient_writes.erase(std::remove_if(object.ambient_writes.begin(),object.ambient_writes.end(),[](const auto& w){return w.object==2;}),object.ambient_writes.end());
            object.erase_triangles([](const auto& t){return t.object==2;});}
        NJS_MATRIX matrix;std::copy(std::begin(bones[bone].value),std::end(bones[bone].value),std::begin(matrix));
        for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}
        const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,base);
    }
}
}
