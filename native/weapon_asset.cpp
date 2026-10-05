#include "weapon_asset.h"
#include "asset_io.h"
#include "binary_view.h"
#include <algorithm>
namespace recvx {
WeaponAsset WeaponAsset::decode(const std::vector<std::uint8_t>& bytes){
    BinaryView v(bytes);WeaponAsset out;std::size_t p=0;
    auto section=[&](unsigned alignment){auto n=v.u32(p);p+=4;if(n&0x80000000){p=(p+alignment-1)&~std::size_t(alignment-1);n&=0x7fffffff;}v.require(p,n);const auto start=p;p+=n;return std::make_pair(start,n);};
    // bhReadWeaponData: right model/PVP, left model/PVP, sized MTN list.
    for(auto* target:{&out.right,&out.left}){auto model=section(4),texture=section(64);if(model.second)*target=decode_model_asset(bytes,model.first,texture.second?texture.first:0);}
    auto list=section(4);p=list.first;const auto end=p+list.second;bool terminated=!list.second;
    while(p<end&&out.motions.size()<64){const auto n=v.u32(p);p+=4;if(n==0xffffffff){terminated=true;break;}if(n>end-p)throw std::runtime_error("weapon motion exceeds section");out.motions.push_back(n?std::optional<MotionClip>(decode_motion_asset({bytes.begin()+p,bytes.begin()+p+n})):std::nullopt);p+=n;}
    if(!terminated)throw std::runtime_error("unterminated weapon motion list");return out;
}
WeaponAsset WeaponAsset::load(const std::filesystem::path& data,unsigned player,unsigned weapon,const std::filesystem::path& mods){
    if(player>4||weapon>20)throw std::runtime_error("weapon archive identity outside table");const auto name=asset_number(20+player*30+weapon)+".bin";auto path=mods/"weapons"/name;if(mods.empty()||!std::filesystem::is_regular_file(path))path=data/"archives/SYSTEM"/name;
    auto out=decode(read_asset_file(path,8*1024*1024));apply_texture_files(out.right,data,mods,std::filesystem::path("weapons")/asset_number(20+player*30+weapon)/"right");apply_texture_files(out.left,data,mods,std::filesystem::path("weapons")/asset_number(20+player*30+weapon)/"left");return out;
}
void WeaponAsset::append(Scene& scene,const std::vector<BoneMatrix>& bones)const{
    unsigned i=0;for(const auto* source:{&right,&left}){const unsigned bone=i++?13:9;if(source->triangles.empty()||bone>=bones.size())continue;auto object=*source;NJS_MATRIX matrix;std::copy(std::begin(bones[bone].value),std::end(bones[bone].value),std::begin(matrix));
        for(auto& t:object.triangles)for(auto& vertex:t.vertices){njCalcPoint(&matrix,&vertex.position,&vertex.position);njCalcVector(&matrix,&vertex.normal,&vertex.normal);njUnitVector(&vertex.normal);}
        const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),object.textures.begin(),object.textures.end());append_actor(scene,object,base);
    }
}
}
