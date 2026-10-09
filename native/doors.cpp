#include "doors.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
namespace recvx {
std::string DoorRequest::room_name()const{
    return RoomIdentity{stage,room,room_case}.name();
}
void DoorSequence::begin(const std::filesystem::path& data,const std::filesystem::path& mods,const DoorRequest& request){
    if(active_)throw std::runtime_error("door transition already active");
    // bhSysCallDoordemo mode 0: attr 0x80000000 or type 255 skips the door demo (mode 10, no MRY read).
    direct_=(request.attribute&0x80000000u)||request.type==255;
    if(direct_){request_=request;active_=false;finished_=true;skipped_=false;ticks_=0;sound_bank_=0;sounds_.clear();sound_commands_.clear();return;}
    const auto override=mods/"doors"/(asset_number(request.type)+".bin");
    auto bytes=read_asset_file(std::filesystem::is_regular_file(override)?override:data/"archives/MRY"/(asset_number(71+request.type)+".bin"),8*1024*1024);
    BinaryView v(bytes);if(v.u32(0)!=0x4341504d)throw std::runtime_error("door packet requires MPAC");
    std::size_t p=v.relative(4,v.u32(4),4);std::array<std::size_t,4> offsets{},sizes{};
    for(unsigned i=0;i<4;++i){sizes[i]=v.u32(p);offsets[i]=p+4;v.require(offsets[i],sizes[i]);p=offsets[i]+sizes[i];}
    sound_bank_=v.u32(offsets[2]);sounds_.clear();sound_commands_.clear();
    sound_timing_={0,0,0,0,0,0x1000000,0x2000000,0x4000000,0x10,0x20,-1};
    model_=std::move(bytes);model_offset_=offsets[0];texture_offset_=offsets[1];
    parameters_.assign(model_.begin()+offsets[3],model_.begin()+offsets[3]+sizes[3]);BinaryView param(parameters_);
    if(param.u8(0)!='P'||param.u8(1)!='M'||param.u8(2)!='B'||!param.u8(3))throw std::runtime_error("invalid door parameters");
    const unsigned requested_case=(request.attribute&0x180)>>7,selected=requested_case<param.u8(3)?requested_case:0;
    // PT_WORK: five procedure slots (fade, camera, door, light, shake), each a
    // number and an offset relative to the parameter block.
    const auto table=4+selected*20;param.require(table,20);std::array<unsigned,5> types{};std::array<std::size_t,5> po{};
    for(unsigned i=0;i<5;++i){types[i]=param.u16(table+i*4);po[i]=param.u16(table+i*4+2);}
    for(unsigned i=0;i<3;++i){
        if(types[i]>(i==0?1u:7u))throw std::runtime_error("unsupported door procedure");
        if(po[i]%4)throw std::runtime_error("unaligned door parameters");
        param.require(po[i],recvx_door_parameter_size(i,types[i]));
    }
    if(!types[0]||!recvx_door_validate_view(types[1],parameters_.data()+po[1],parameters_.size()-po[1],0))throw std::runtime_error("invalid door camera/fade parameters");
    BinaryView mdl(model_);count_=mdl.u16(model_offset_+6);if(!count_||count_>128)throw std::runtime_error("invalid door node count");
    const auto base=mdl.relative(model_offset_,mdl.u16(model_offset_+4),0);objects_=mdl.relative(base,mdl.u32(model_offset_+12),count_*52);
    // Some original procedures form pointers to an unused second door leaf.
    nodes_.assign(std::max<std::size_t>(count_,64),{});
    for(unsigned i=0;i<count_;++i){const auto o=objects_+i*52;auto& n=nodes_[i];n.evalflags=mdl.u32(o);n.model=mdl.u32(o+4);for(unsigned k=0;k<3;++k){n.pos[k]=mdl.f32(o+8+k*4);n.ang[k]=mdl.i32(o+20+k*4);}}
    if((types[2]==2&&count_<51)||(types[2]==4&&count_<4)||(types[2]==6&&count_<4)||(types[2]==7&&count_<4))throw std::runtime_error("door procedure does not match model nodes");
    core_={};core_.dmo_atr=int(request.attribute);core_.objP=nodes_.data();core_.nodes=unsigned(count_);
    core_.fpP=parameters_.data()+po[0];core_.vpP=parameters_.data()+po[1];core_.dpP=parameters_.data()+po[2];
    // light.c LightProc1/2 and the PuruProc shake are not driven here; their
    // numbers are kept so the gap is visible instead of silent.
    core_.lgt_type=int(types[3]);core_.pru_type=int(types[4]);
    if(!recvx_door_configure(&core_,types[0],types[1],types[2]))throw std::runtime_error("door packet selects an undefined procedure");
    if(request.attribute&1)core_.status|=0x4000;if(request.attribute&4)core_.status|=0x4800;if(request.attribute&16)core_.status|=0xa000;if(request.attribute&8)core_.status|=0x1000;
    if(count_<5)core_.status&=~0x3000;
    if(request.attribute&2){core_.status^=0xc00;if(count_>=5)core_.status^=0x2000;else core_.status&=~0x2000;}
    if(request.attribute&32)core_.status|=0x40000000;if(request.attribute&64)core_.status|=0x20000000;
    core_.status|=0x1000001f;
    auto scene=decode_model_asset(model_,model_offset_,texture_offset_);apply_texture_files(scene,data,mods,std::filesystem::path("doors")/asset_number(request.type));textures_=std::move(scene.textures);
    request_=request;active_=true;finished_=skipped_=false;ticks_=0;
}
void DoorSequence::tick(){if(!active_)return;recvx_door_step(&core_);
    // door.c bhControlDoorDemo: two sound request groups with the same priority.
    for(unsigned shift:{0u,4u}){
        // bhControlDoorDemo retains the second group until FadeProc1 sets
        // 0x20. These are deferred closing sounds, not immediate door sounds.
        if(shift&&!(core_.status&0x20))continue;
        const unsigned bits=unsigned(core_.status)>>(16+shift);if(bits&2)sounds_.push_back(1);else if(bits&8)sounds_.push_back(3);else if(bits&4)sounds_.push_back(2);else if(bits&1)sounds_.push_back(0);core_.status&=~int(0xf0000u<<shift);
    }
    for(unsigned command=0;command<10;++command)if(core_.status&sound_timing_[command]){sound_commands_.push_back(command);sound_timing_[command]=0;}
    ++ticks_;if(core_.status&0x40){active_=false;finished_=true;}if(ticks_>3600)throw std::runtime_error("original door sequence exceeded tick budget");}
void DoorSequence::skip(){if(active_){skipped_=true;active_=false;finished_=true;core_.status|=0x40;}}
void DoorSequence::draw(SoftwareRenderer& renderer){
    Scene scene;scene.background=0xff000000;SceneCamera camera;camera.position=core_.vew_pos;for(unsigned i=0;i<3;++i)camera.angles[i]=-core_.vew_ang[i];camera.perspective=12743;
    if(core_.status&0x200){ModelGeometryOptions options;options.pose.resize(count_);if(core_.status&0x400)njRotateY(&options.transform,32768);
        for(unsigned i=0;i<count_;++i){const auto& n=nodes_[i];options.pose[i]={{n.pos[0],n.pos[1],n.pos[2]},{n.ang[0],n.ang[1],n.ang[2]}};std::memcpy(model_.data()+objects_+i*52,&n.evalflags,4);std::memcpy(model_.data()+objects_+i*52+4,&n.model,4);}
        scene=decode_model_asset(model_,model_offset_,0,options);scene.textures=textures_;scene.background=0xff000000;
    }renderer.draw(scene,camera);
    if(core_.status&0x100){const float opacity=std::clamp(core_.fde_rate,0.0f,1.0f);for(auto& pixel:renderer.canvas()){std::uint32_t mixed=0xff000000;for(unsigned shift:{0u,8u,16u})mixed|=unsigned(((pixel>>shift)&255)*(1-opacity)+((unsigned(core_.fde_col)>>shift)&255)*opacity)<<shift;pixel=mixed;}}
}
}
