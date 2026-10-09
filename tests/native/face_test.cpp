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
    _Make_SinTable();FaceParameter a,b;a.frame=10;b.frame=30;b.muscle[31]=8;b.jawang=40;a.eye.z=b.eye.z=-6;
    std::vector<std::uint8_t> packed(164);const std::int32_t negative_angle=-36;std::memcpy(packed.data()+128,&negative_angle,4);
    const auto bank=decode_face_parameters(packed,0,1,false,true);const auto converted=lip_face_parameter(bank[0]);
    check(std::abs(converted.jawang-negative_angle*0.005493164f)<0.000001f,"negative original MASK lip angle was rejected or clamped");
    bool rejected=false;try{decode_face_parameters(packed,0,1);}catch(const std::runtime_error&){rejected=true;}check(rejected,"ordinary facial coordinates accept NaN angle data");
    std::vector<FaceParameter> keys{a,b};check(sample_face_parameters(keys,0).jawang==0&&sample_face_parameters(keys,40).jawang==40,"face endpoints");
    check(std::abs(sample_face_parameters(keys,20).jawang-20)<.001f&&sample_face_parameters(keys,20).muscle[31]==4,"signed-mask high muscle/interpolation");
    for(unsigned flag=0;flag<5;++flag){keys[0].flag=short(flag);const auto p=sample_face_parameters(keys,15);const float expected[]{10,2.5f,20,40*(.5f-.5f*njCos(8192)),0};check(std::abs(p.jawang-expected[flag])<.001f,"original facial easing modes");}
    if(argc>1){const auto data=std::filesystem::u8path(argv[1]);unsigned checked=0;
        {
            const auto room=read_asset_file(data/"rooms/RM_3230.RDT",64*1024*1024);const auto scene=decode_scene(room);auto spawn=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(room,scene,spawn,motion,frame,audio,data);auto player=PlayerAsset::load(data);world.set_player_asset(&player);world.tick();
            unsigned index=0;while(index<128&&(!world.state(1,index)||!world.state(1,index)->facial))++index;check(index<128,"real lip actor unavailable");
            auto command=[&](unsigned opcode,unsigned sequence){EventCommand c;c.opcode=opcode;c.bytes={std::uint8_t(opcode),0,std::uint8_t(index),std::uint8_t(sequence)};check(world.bind(c),"real lip command not bound");};
            command(0x3d,0);command(0x3f,0);world.tick();const auto active=world.state(1,index)->lip_current_stream;check(world.state(1,index)->lip_active&&!active.empty(),"original lip stream did not activate");
            command(0x3d,3);world.tick();const auto* state=world.state(1,index);check(state->lip_stream!=active&&state->lip_current_stream==active,"bhSetLip retargeted an already active LIP_WORK cursor");
        }
        for(const auto room:{"RM_0030","RM_0040"}){const auto bytes=read_asset_file(data/"rooms"/(std::string(room)+".RDT"),64*1024*1024);BinaryView v(bytes);const auto header=inspect_room(bytes).room_offset;const auto table=v.u32(20),enep=v.u32(header+8);unsigned slot=1;
            for(unsigned i=0;i<v.u32(header+0x88);++i){if(!(v.u32(enep+i*36)&1))continue;const auto model=v.u32(table+slot++*4);if(v.u16(enep+i*36+4)<90)continue;PlayerAsset actor(actor_packet(bytes,model,0),true);check(actor.face()!=nullptr,"real cinematic MASK absent");MotionClip clip;clip.frames=1;clip.positions.resize(actor.bones());clip.angles.resize(actor.bones());const auto neutral=actor.sample_clip(clip,0);bool moved=false;
                for(const auto& param:actor.face()->keys()){const auto posed=actor.sample_clip(clip,0,{},0,{},nullptr,{},&param);check(posed.triangles.size()==neutral.triangles.size(),"facial geometry disappeared");for(unsigned t=0;t<posed.triangles.size();++t)for(unsigned j=0;j<3;++j){const auto p=posed.triangles[t].vertices[j].position,q=neutral.triangles[t].vertices[j].position;check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"nonfinite MASK deformation");moved=moved||std::abs(p.x-q.x)+std::abs(p.y-q.y)+std::abs(p.z-q.z)>.01f;}}
                check(moved,"real MASK keys do not deform geometry");const auto again=actor.sample_clip(clip,0);check(again.triangles[0].vertices[0].position.x==neutral.triangles[0].vertices[0].position.x,"face sampling mutates source");++checked;
            }}check(checked==4,"Steve/Claire in both scenes not checked");
        const auto ui=decode_texture_bundle(read_asset_file(data/"archives/ITEM1/0145.bin",8*1024*1024));check(ui.size()==5&&ui[0].palette_banks.size()==7,"palette-only records change texture slots or disappear");for(unsigned bank:{5u,6u,7u,8u,9u,10u})check(ui[0].palette_banks.count(bank)==1,"original menu bank absent");check(!(ui[0].palette_banks.at(10)==ui[0].palette_banks.at(5)),"green quantities reuse default palette");
    }
    std::cout<<"Original face easing, four cinematic MASK assets, immutable geometry and menu palette banks verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
