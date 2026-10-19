#include "doors.h"
#include "back_color.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <iomanip>
#include <sstream>
namespace recvx {
std::string DoorRequest::room_name()const{
    return RoomIdentity{stage,room,room_case}.name();
}
void DoorSequence::begin(const std::filesystem::path& data,const std::filesystem::path& mods,const DoorRequest& request,bool install_back_color){
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
    // Highest procedure number of each family: fade 1, camera 7, door 7,
    // light 2, shake 1.
    static const unsigned highest[5]={1,7,7,2,1};
    for(unsigned i=0;i<5;++i){
        if(types[i]>highest[i])throw std::runtime_error("unsupported door procedure");
        if(po[i]%4)throw std::runtime_error("unaligned door parameters");
        // A light block is only as long as the lights it declares, so its
        // extent has to be measured instead of assumed.
        if(i!=3)param.require(po[i],recvx_door_parameter_size(i,types[i]));
    }
    // Both blocks of LightProc2 are reachable: the push side reads the first,
    // the pull side the second.
    if(types[3])param.require(po[3],1);
    for(unsigned block=1;block<=types[3];++block)
        if(!recvx_door_light_extent(parameters_.data()+po[3],parameters_.size()-po[3],block))
            throw std::runtime_error("door light block does not fit its parameter section");
    if(!types[0]||!recvx_door_validate_view(types[1],parameters_.data()+po[1],parameters_.size()-po[1],0))throw std::runtime_error("invalid door camera/fade parameters");
    BinaryView mdl(model_);count_=mdl.u16(model_offset_+6);if(!count_||count_>128)throw std::runtime_error("invalid door node count");
    const auto base=mdl.relative(model_offset_,mdl.u16(model_offset_+4),0);objects_=mdl.relative(base,mdl.u32(model_offset_+12),count_*52);
    // Some original procedures form pointers to an unused second door leaf.
    nodes_.assign(std::max<std::size_t>(count_,64),{});
    for(unsigned i=0;i<count_;++i){const auto o=objects_+i*52;auto& n=nodes_[i];n.evalflags=mdl.u32(o);n.model=mdl.u32(o+4);for(unsigned k=0;k<3;++k){n.pos[k]=mdl.f32(o+8+k*4);n.ang[k]=mdl.i32(o+20+k*4);}}
    if((types[2]==2&&count_<51)||(types[2]==4&&count_<4)||(types[2]==6&&count_<4)||(types[2]==7&&count_<4))throw std::runtime_error("door procedure does not match model nodes");
    // bhInitDoor clears the work and then puts the point-light rate at one and
    // the ambient offset at zero; DokiProcLgt is what moves them afterwards.
    core_={};core_.lgt_pnt_rte={1,1,1};core_.lgt_amb_off={0,0,0};
    core_.dmo_atr=int(request.attribute);core_.objP=nodes_.data();core_.nodes=unsigned(count_);
    core_.fpP=parameters_.data()+po[0];core_.vpP=parameters_.data()+po[1];core_.dpP=parameters_.data()+po[2];
    core_.lpP=parameters_.data()+po[3];core_.ppP=parameters_.data()+po[4];
    core_.lgt_type=int(types[3]);core_.pru_type=int(types[4]);
    if(!recvx_door_configure(&core_,types[0],types[1],types[2],types[3],types[4]))throw std::runtime_error("door packet selects an undefined procedure");
    // bhSetDoor njClipZ: the dmo_clip table narrows the demo of door type 1
    // reading from the MRY partition when its case runs ViewProc4.
    clip_case_=recvx_door_clip(int(request.type),RECVX_DOOR_MRY_PARTITION,int(types[1]),&clip_near_,&clip_far_)!=0;
    // bhSetDoor (door.c:243) after njClipZ/njSetAspect, like bhSysCallDoordemo mode 2 before it (system.c:982).
    if(install_back_color)set_back_color(0);
    if(request.attribute&1)core_.status|=0x4000;if(request.attribute&4)core_.status|=0x4800;if(request.attribute&16)core_.status|=0xa000;if(request.attribute&8)core_.status|=0x1000;
    if(count_<5)core_.status&=~0x3000;
    if(request.attribute&2){core_.status^=0xc00;if(count_>=5)core_.status^=0x2000;else core_.status&=~0x2000;}
    if(request.attribute&32)core_.status|=0x40000000;if(request.attribute&64)core_.status|=0x20000000;
    core_.status|=0x1000001f;
    // dmo_atr 0x40 replaces the packet's shake by the built-in heartbeat one.
    if(core_.status&0x20000000)recvx_door_select_doki_shake(&core_);
    shake_sounds_.clear();shake_patterns_.clear();
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
    // PuruProc1 raises CallSystemSe(0, PruSndTbl[snd_no]) and
    // StartVibrationEx(2, pru_no); the pass that rearms raises the pattern twice.
    for(int i=0;i<core_.pru_se;++i)shake_sounds_.push_back(core_.pru_se_no);
    for(int i=0;i<core_.pru_vib;++i)shake_patterns_.push_back({core_.pru_vib_atrb,core_.pru_vib_no});
    core_.pru_se=core_.pru_vib=0;
    ++ticks_;if(core_.status&0x40){active_=false;finished_=true;}if(ticks_>3600)throw std::runtime_error("original door sequence exceeded tick budget");}
// BEWUSSTE ABWEICHUNG VOM ORIGINAL - NICHT ERNEUT WEGPORTIEREN.
// Host-Komfortfunktion, im Original nicht vorhanden: bhSysCallDoordemo
// (system.c:939) liest kein Pad, bhStartDoorDemo (room.c:978) loescht
// pad_ps/pad_on, und bhControlDoor (door.c:386-528,638-646) endet erst mit
// dem Statusbit 0x40 aus FadeProc1. Vom Nutzer ausdruecklich gewuenscht
// (2026-10-13); die Tuerstimmen werden dabei gestoppt (runtime.cpp), damit
// kein halb gespielter Tuerton in den Zielraum laeuft.
// Gesetzt wird dasselbe Abschlussbit, das FadeProc1 setzen wuerde, damit der
// uebrige Ablauf (Raumwechsel, Freigabe) genau den regulaeren Weg nimmt.
void DoorSequence::skip(){if(active_){skipped_=true;active_=false;finished_=true;core_.status|=0x40;}}
// njCnkSetEasyMultiAmbient plus the point lights of slots 2..4, evaluated the
// way Ninja chunk lighting does it: per vertex over its own normal and
// position, with the linear near/far falloff of njCnkSetEasyMultiLightRange.
// Without a light procedure the demo keeps the drawing it had.
void DoorSequence::light_scene(Scene& scene)const{
    if(!core_.lgt_on)return;
    const NJS_POINT3 ambient{core_.lgt_amb[0],core_.lgt_amb[1],core_.lgt_amb[2]};
    for(auto& t:scene.triangles){
        NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z},
                   b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},face;
        njOuterProduct(&a,&b,&face);njUnitVector(&face);
        for(auto& vertex:t.vertices){
            auto normal=vertex.normal;
            if(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z<.0001f)normal=face;else njUnitVector(&normal);
            auto colour=ambient;
            for(const auto& light:core_.lgt_pnt){
                if(!light.on)continue;
                NJS_VECTOR direction{light.pos.x-vertex.position.x,light.pos.y-vertex.position.y,light.pos.z-vertex.position.z};
                const float distance=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
                const float attenuation=light.f_rang>light.n_rang?std::clamp((light.f_rang-distance)/(light.f_rang-light.n_rang),0.f,1.f):0.f;
                njUnitVector(&direction);
                const float diffuse=std::max(0.f,direction.x*normal.x+direction.y*normal.y+direction.z*normal.z)*attenuation;
                colour.x+=light.col[0]*diffuse;colour.y+=light.col[1]*diffuse;colour.z+=light.col[2]*diffuse;
            }
            vertex.light={std::clamp(colour.x,0.f,1.f),std::clamp(colour.y,0.f,1.f),std::clamp(colour.z,0.f,1.f)};
        }
        t.gouraud=true;t.world_lit=true;
    }
}
void DoorSequence::draw(SoftwareRenderer& renderer){
    // Every frame clears with the installed njSetBackColor: 0 from bhSetDoor, the fade colour in mode 10 (system.c:1033).
    Scene scene;scene.background=installed_back_pixel();SceneCamera camera;camera.position=core_.vew_pos;for(unsigned i=0;i<3;++i)camera.angles[i]=-core_.vew_ang[i];camera.perspective=12743;
    // bhSetDoor njClipZ. njSetAspect(1.174, 1) equals BHD_ASPECT_X/Y, which the
    // renderer already uses, so only the clip range differs from gameplay.
    camera.clip_near=clip_near_;camera.clip_far=clip_far_;
    // bhControlDoor (door.c:395) zeichnet das Tuermodell und ryExcuteFade nur,
    // solange das Abschlussbit 0x40 fehlt; nach bhExitDoor (system.c:1003/1006,
    // Modus 6) und im Direktfall (Modus 10) wird kein DoorWrk-Modell gezeichnet.
    // Ein beendeter, uebersprungener oder nie gestarteter Lauf darf daher weder
    // im Uebergabebild noch im ersten Bild der naechsten Anforderung erscheinen.
    const bool controlling=!(core_.status&0x40);
    if(controlling&&(core_.status&0x200)){ModelGeometryOptions options;options.pose.resize(count_);if(core_.status&0x400)njRotateY(&options.transform,32768);
        for(unsigned i=0;i<count_;++i){const auto& n=nodes_[i];options.pose[i]={{n.pos[0],n.pos[1],n.pos[2]},{n.ang[0],n.ang[1],n.ang[2]}};std::memcpy(model_.data()+objects_+i*52,&n.evalflags,4);std::memcpy(model_.data()+objects_+i*52+4,&n.model,4);}
        scene=decode_model_asset(model_,model_offset_,0,options);scene.textures=textures_;scene.background=installed_back_pixel();
        light_scene(scene);
    }renderer.draw(scene,camera);
    if(controlling&&(core_.status&0x100)){const float opacity=std::clamp(core_.fde_rate,0.0f,1.0f);for(auto& pixel:renderer.canvas()){std::uint32_t mixed=0xff000000;for(unsigned shift:{0u,8u,16u})mixed|=unsigned(((pixel>>shift)&255)*(1-opacity)+((unsigned(core_.fde_col)>>shift)&255)*opacity)<<shift;pixel=mixed;}}
}
}
