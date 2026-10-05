#include "actor.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>
using Bytes=std::vector<std::uint8_t>;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void near(float value,float expected,const char* message){check(std::abs(value-expected)<0.00002f,message);}
static void u16(Bytes& d,std::size_t p,unsigned v){d.at(p)=v&255;d.at(p+1)=(v>>8)&255;}
static void u32(Bytes& d,std::size_t p,unsigned v){u16(d,p,v);u16(d,p+2,v>>16);}
static void f32(Bytes& d,std::size_t p,float value){unsigned v;std::memcpy(&v,&value,4);u32(d,p,v);}
static void append(Bytes& d,const Bytes& block){d.insert(d.end(),block.begin(),block.end());}
static void word(Bytes& d,unsigned value){const auto p=d.size();d.resize(p+4);u32(d,p,value);}
struct Fixture{Bytes bytes;std::size_t model,weight,motion,texture;};
static Fixture fixture(){
    // Two-bone MDL: a root translation and a child mesh. Its first vertex is
    // half affected by the child's own rotation; the other two use full weight.
    Bytes mdl(340);u32(mdl,0,0x804c444d);u16(mdl,4,16);u16(mdl,6,2);u32(mdl,8,0xffffffff);u32(mdl,12,0);
    for(unsigned i=0;i<2;++i){const auto p=16+i*52;u32(mdl,p,7);u32(mdl,p+4,i?104:0xffffffff);f32(mdl,p+8,i?2:10);u32(mdl,p+44,i?0xffffffff:52);u32(mdl,p+48,0xffffffff);}
    u32(mdl,120,128);u32(mdl,124,292);
    mdl[144]=51;u16(mdl,146,39);u16(mdl,150,3);
    const float points[][3]{{2,0,0},{0,2,0},{0,0,2}};
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)f32(mdl,208+i*32+j*4,points[i][j]);mdl[304]=255;
    for(unsigned i=0;i<3;++i)f32(mdl,224+i*32,1); // Original normals point along X.
    mdl[308]=8;u16(mdl,310,0);mdl[312]=65;u16(mdl,314,11);u16(mdl,316,1);u16(mdl,318,3);
    for(unsigned i=0;i<3;++i){u16(mdl,320+i*6,i);u16(mdl,322+i*6,i==1?255:0);u16(mdl,324+i*6,i==2?255:0);}mdl[338]=255;
    Bytes skin(40);u32(skin,0,0x4e494b53);u32(skin,4,1);u32(skin,8,0);u32(skin,12,3);u32(skin,16,0);u32(skin,20,3);
    u32(skin,24,0x01040000);u32(skin,28,0x01090000);u32(skin,32,0x01090000);u32(skin,36,0xffffffff);
    Bytes models;word(models,unsigned(skin.size()));append(models,skin);const auto mdl_offset=models.size()+4;word(models,unsigned(mdl.size()));append(models,mdl);word(models,0xffffffff);
    // MTN with two integer frames: root X=10 then 20; child Z angle=0 then 90 degrees.
    Bytes motion(80);u32(motion,0,0x804e544d);u16(motion,4,16);u16(motion,6,2);u32(motion,8,0);u32(motion,12,0xffffffff);
    u32(motion,16,12);u32(motion,20,2);u16(motion,24,3);u32(motion,28,28);u32(motion,32,0xffffffff);u32(motion,36,0xffffffff);u32(motion,40,52);
    f32(motion,44,10);f32(motion,56,20);u16(motion,78,16384);
    Bytes motions;word(motions,unsigned(motion.size()));append(motions,motion);word(motions,0);word(motions,0xffffffff);
    Bytes curves;word(curves,12);word(curves,0);const auto curve=curves.size();curves.resize(curve+8);f32(curves,curve,0.4f);f32(curves,curve+4,0.8f);word(curves,0xffffffff);
    // One extended TIM2 texture, with exactly four bytes for the PVP terminator.
    Bytes textures(1320);u32(textures,0,0x324d4954);u32(textures,4,1284);u32(textures,32,0x324d4954);textures[37]=1;u16(textures,38,1);
    u32(textures,160,1284);u32(textures,164,1024);u32(textures,168,4);u16(textures,172,128);u16(textures,174,256);
    textures[176]=3;textures[177]=1;textures[178]=3;textures[179]=5;u16(textures,180,2);u16(textures,182,2);
    for(unsigned i=0;i<256;++i)u32(textures,292+i*4,0x80ffffff);u32(textures,1316,0xffffffff);
    Fixture f;word(f.bytes,unsigned(models.size()));append(f.bytes,models);f.model=4+mdl_offset;f.weight=4+4+24;
    word(f.bytes,unsigned(motions.size()));f.motion=f.bytes.size()+4;append(f.bytes,motions);
    word(f.bytes,unsigned(curves.size()));append(f.bytes,curves);word(f.bytes,unsigned(textures.size()));f.texture=f.bytes.size();append(f.bytes,textures);return f;
}
template<class F>static void rejects(F f,const char* message){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}check(rejected,message);}
int main(){try{
    _Make_SinTable();auto f=fixture();recvx::PlayerAsset player(f.bytes);
    check(player.bones()==2&&player.motions()==2&&player.has_motion(0)&&!player.has_motion(1),"motion holes/skeleton counts");
    check(player.motion_frames(0)==2&&player.textures().size()==1,"motion frames/PVP terminator");
    auto first=player.sample(0,0);auto second=player.sample(0,65536);auto bones=player.pose(0,65536);std::vector<recvx::BoneMatrix> drawn_bones;player.sample(0,65536,{},0,&drawn_bones);check(bones.size()==drawn_bones.size(),"pose-only skeleton size differs");for(unsigned i=0;i<bones.size();++i)for(unsigned j=0;j<16;++j)near(bones[i].value[j],drawn_bones[i].value[j],"pose-only matrix differs from rendered geometry");
    check(first.triangles.size()==1&&first.triangles[0].textured&&!first.triangles[0].camera_mask,"animated geometry flags");
    near(first.triangles[0].vertices[0].position.x,14,"bind origin translation");
    near(second.triangles[0].vertices[0].position.x,23,"half-weight vertex X");near(second.triangles[0].vertices[0].position.y,1,"half-weight vertex Y");
    near(second.triangles[0].vertices[1].position.x,20,"full-weight vertex X");near(second.triangles[0].vertices[1].position.y,0,"full-weight vertex Y");
    near(second.triangles[0].vertices[0].normal.x,std::sqrt(.5f),"half-weight normal X");near(second.triangles[0].vertices[0].normal.y,std::sqrt(.5f),"half-weight normal Y");
    near(second.triangles[0].vertices[1].normal.x,0,"full-weight normal X");near(second.triangles[0].vertices[1].normal.y,1,"full-weight normal Y");
    near(player.sample(0,65535).triangles[0].vertices[0].position.x,14,"integer key sampling");
    near(player.sample(0,131072).triangles[0].vertices[0].position.x,14,"positive frame wrap");
    near(player.sample(0,-1,{5,0,0}).triangles[0].vertices[0].position.x,28,"negative wrap/actor translation");
    near(player.translation_speed(0,0),0.4f,"first speed key");near(player.translation_speed(0,-1),0.8f,"negative speed wrap");
    recvx::Scene stage;stage.textures=player.textures();stage.textures.push_back(player.textures()[0]);recvx::append_actor(stage,first,1);
    check(stage.triangles[0].texture==1&&!stage.triangles[0].camera_mask,"actor texture offset");
    rejects([&]{player.sample(1,0);},"missing motion accepted");rejects([&]{player.translation_speed(1,0);},"missing speed curve accepted");
    rejects([&]{recvx::append_actor(stage,first,2);},"invalid actor texture accepted");
    auto bad=f.bytes;u32(bad,f.weight,0x02040000);rejects([&]{recvx::PlayerAsset asset(bad);},"skin bone outside skeleton accepted");
    bad=f.bytes;u32(bad,f.weight,0x010a0000);rejects([&]{recvx::PlayerAsset asset(bad);},"skin level outside table accepted");
    bad=f.bytes;u32(bad,f.model+16+44,0);rejects([&]{recvx::PlayerAsset asset(bad);},"cyclic player hierarchy accepted");
    bad=f.bytes;u32(bad,f.motion+20,65536);rejects([&]{recvx::PlayerAsset asset(bad);},"truncated motion keys accepted");
    bad=f.bytes;u32(bad,f.motion+44,0x7fc00000);rejects([&]{recvx::PlayerAsset asset(bad);},"NaN motion position accepted");
    for(const auto size:{f.model+20,f.motion+40,f.texture+256,f.bytes.size()-1}){bad=f.bytes;bad.resize(size);rejects([&]{recvx::PlayerAsset asset(bad);},"truncated player packet accepted");}
    // Face initialization inserts children selected by MASK mesh IDs, not by
    // a room-specific wrist correction. Script indices move; skin indices do not.
    Bytes face(16+27*52),mask(64,255);u32(face,0,0x804c444d);u16(face,4,16);u16(face,6,27);u32(face,12,0);u32(mask,0,0x4b53414d);
    for(unsigned n=0;n<27;++n){const auto p=16+n*52;u32(face,p+4,(n==1||(n>=6&&n<=10))?0:0xffffffff);u32(face,p+44,0xffffffff);u32(face,p+48,n==26?0xffffffff:(n+1)*52);}
    mask[0x35]=4;mask[0x36]=2;mask[0x37]=3;mask[0x38]=5;
    const auto layout=recvx::decode_actor_node_layout(face,mask);check(layout.script_to_model.size()==31&&layout.motion_to_model.size()==22,"procedural face nodes not reconstructed");
    check(layout.script_to_model[18]==14&&layout.script_to_model[22]==18&&layout.model_to_script[14]==18,"original wrist indices still refer to legs");
    check(layout.motion_to_model[6]==11&&layout.motion_to_model.back()==26,"procedural face consumes body motion keys");
    check(recvx::decode_actor_node_layout(face,{},true).motion_to_model.size()==22,"unmasked cinematic body consumes procedural channels");
    mask[0x38]=255;check(recvx::decode_actor_node_layout(face,mask).script_to_model.size()==30,"MASK eye selection ignored");
    recvx::MotionClip lower,upper;lower.frames=upper.frames=2;lower.positions.resize(8);lower.angles.resize(8);upper.positions.resize(10);upper.angles.resize(10);upper.angles[1]={{{0,0,0}},{{0,16384,0}}};
    const auto joined=recvx::join_motion_segments(lower,upper);check(joined.positions.size()==18&&joined.angles[9][1][1]==16384,"upper-body segment absent or attached at wrong node");upper.frames=3;rejects([&]{recvx::join_motion_segments(lower,upper);},"different segment clocks silently accepted");
    std::cout<<"Native player motion/skin tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
