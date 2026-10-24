#include "world.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
static std::vector<std::uint8_t> actor_packet(const std::vector<std::uint8_t>& b,std::size_t p,std::size_t){
    recvx::BinaryView v(b);std::vector<std::uint8_t> records;unsigned bones=0;
    auto put=[](auto& out,unsigned value){for(unsigned i=0;i<4;++i)out.push_back(std::uint8_t(value>>(i*8)));};
    for(unsigned i=0;i<64;++i){const auto size=v.u32(p);p+=4;put(records,size);if(size==0xffffffff)break;if((v.u32(p)&0xffffff)==0x4c444d)bones=v.u16(p+6);records.insert(records.end(),b.begin()+p,b.begin()+p+size);p+=size;}
    std::vector<std::uint8_t> out;put(out,unsigned(records.size()));out.insert(out.end(),records.begin(),records.end());put(out,8);put(out,0);put(out,0xffffffff);put(out,4);put(out,0xffffffff);put(out,0);if(!bones)throw std::runtime_error("cinematic model absent");return out;
}
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    _Make_SinTable();check(argc==2,"loose data path required");const auto data=std::filesystem::u8path(argv[1]);unsigned checked=0;
    for(const auto room:{"RM_0030","RM_0040"}){
        const auto bytes=read_asset_file(data/"rooms"/(std::string(room)+".RDT"),64*1024*1024);BinaryView v(bytes);const auto h=inspect_room(bytes).room_offset;const auto table=v.u32(20),enep=v.u32(h+8);unsigned slot=1;
        for(unsigned i=0;i<v.u32(h+0x88);++i){if(!(v.u32(enep+i*36)&1))continue;const auto offset=v.u32(table+slot++*4);if(v.u16(enep+i*36+4)<90)continue;
            PlayerAsset actor(actor_packet(bytes,offset,0),true);check(actor.face()!=nullptr,"real MASK absent");MotionClip clip;clip.frames=1;clip.positions.resize(actor.bones());clip.angles.resize(actor.bones());const auto pose=actor.local_pose_clip(clip,0);std::vector<BoneMatrix> work;actor.sample_clip(clip,0,{},0,{},&work);const auto saved=work;std::vector<unsigned> flags(actor.bones(),0);const auto neutral=actor.sample_work(pose,work,flags);bool moved=false;
            for(const auto& param:actor.face()->keys()){
                std::vector<BoneMatrix> output;const auto scene=actor.sample_work(pose,work,flags,&output,&param);check(output.size()==saved.size(),"work count changed");
                for(unsigned n=0;n<saved.size();++n)for(unsigned k=0;k<16;++k)check(output[n].value[k]==saved[n].value[k]&&work[n].value[k]==saved[n].value[k],"face recalculated body matrices");
                check(scene.triangles.size()==neutral.triangles.size(),"face geometry disappeared");
                for(unsigned t=0;t<scene.triangles.size();++t)for(unsigned j=0;j<3;++j){const auto p=scene.triangles[t].vertices[j].position,q=neutral.triangles[t].vertices[j].position;check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"nonfinite face");moved=moved||std::abs(p.x-q.x)+std::abs(p.y-q.y)+std::abs(p.z-q.z)>.01f;}
            }
            check(moved,"real MASK did not deform work geometry");const auto reset=actor.sample_work(pose,work,flags);for(unsigned t=0;t<reset.triangles.size();++t)for(unsigned j=0;j<3;++j){const auto p=reset.triangles[t].vertices[j].position,q=neutral.triangles[t].vertices[j].position;check(p.x==q.x&&p.y==q.y&&p.z==q.z,"face deformation persisted after reset");}++checked;
        }
    }
    check(checked==4,"four real MASK actors not checked");std::cout<<"Real MASK work geometry verified\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
