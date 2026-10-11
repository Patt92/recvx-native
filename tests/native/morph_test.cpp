#include "actor.h"
#include "morph.h"
#include "ninja_matrix.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using Bytes=std::vector<std::uint8_t>;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void near(float value,float expected,const char* message){check(std::abs(value-expected)<0.0002f,message);}
static void u16(Bytes& d,std::size_t p,unsigned v){d.at(p)=v&255;d.at(p+1)=(v>>8)&255;}
static void u32(Bytes& d,std::size_t p,unsigned v){u16(d,p,v);u16(d,p+2,v>>16);}
static void f32(Bytes& d,std::size_t p,float value){unsigned v;std::memcpy(&v,&value,4);u32(d,p,v);}
static void append(Bytes& d,const Bytes& block){d.insert(d.end(),block.begin(),block.end());}
static void word(Bytes& d,unsigned value){const auto p=d.size();d.resize(p+4);u32(d,p,value);}

// One two-bone actor asset whose single skinned object holds the three given
// points. Two of these are the obj_a/obj_b pair npCalcMorphing blends: the same
// skeleton, the same skin table, different vertex streams.
static Bytes actor_asset(const float points[3][3]){
    Bytes mdl(340);u32(mdl,0,0x804c444d);u16(mdl,4,16);u16(mdl,6,2);u32(mdl,8,0xffffffff);u32(mdl,12,0);
    for(unsigned i=0;i<2;++i){const auto p=16+i*52;u32(mdl,p,7);u32(mdl,p+4,i?104:0xffffffff);f32(mdl,p+8,i?2:10);u32(mdl,p+44,i?0xffffffff:52);u32(mdl,p+48,0xffffffff);}
    u32(mdl,120,128);u32(mdl,124,292);
    mdl[144]=51;u16(mdl,146,39);u16(mdl,150,3);
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)f32(mdl,208+i*32+j*4,points[i][j]);mdl[304]=255;
    for(unsigned i=0;i<3;++i)f32(mdl,224+i*32,1);
    mdl[308]=8;u16(mdl,310,0);mdl[312]=65;u16(mdl,314,11);u16(mdl,316,1);u16(mdl,318,3);
    for(unsigned i=0;i<3;++i){u16(mdl,320+i*6,i);u16(mdl,322+i*6,i==1?255:0);u16(mdl,324+i*6,i==2?255:0);}mdl[338]=255;
    Bytes skin(40);u32(skin,0,0x4e494b53);u32(skin,4,1);u32(skin,8,0);u32(skin,12,3);u32(skin,16,0);u32(skin,20,3);
    u32(skin,24,0x01040000);u32(skin,28,0x01090000);u32(skin,32,0x01090000);u32(skin,36,0xffffffff);
    Bytes models;word(models,unsigned(skin.size()));append(models,skin);word(models,unsigned(mdl.size()));append(models,mdl);word(models,0xffffffff);
    Bytes motion(80);u32(motion,0,0x804e544d);u16(motion,4,16);u16(motion,6,2);u32(motion,8,0);u32(motion,12,0xffffffff);
    u32(motion,16,12);u32(motion,20,2);u16(motion,24,3);u32(motion,28,28);u32(motion,32,0xffffffff);u32(motion,36,0xffffffff);u32(motion,40,52);
    f32(motion,44,10);f32(motion,56,20);u16(motion,78,16384);
    Bytes motions;word(motions,unsigned(motion.size()));append(motions,motion);word(motions,0);word(motions,0xffffffff);
    Bytes curves;word(curves,12);word(curves,0);const auto curve=curves.size();curves.resize(curve+8);f32(curves,curve,0.4f);f32(curves,curve+4,0.8f);word(curves,0xffffffff);
    Bytes out;word(out,unsigned(models.size()));append(out,models);
    word(out,unsigned(motions.size()));append(out,motions);
    word(out,unsigned(curves.size()));append(out,curves);
    word(out,0); // room actor without a texture bundle (texP NULL)
    return out;
}
// A bare model string with three objects: one without a model, one two-point
// stream whose usIndexMax is `count`, and one stream that evalflags 0x8 removes
// from every morph walk.
static Bytes stream_model(unsigned count,const float points[2][3]){
    Bytes mdl(476);
    u16(mdl,4,16);u16(mdl,6,3);u32(mdl,12,0);
    u32(mdl,16,7);u32(mdl,20,0xffffffff);
    u32(mdl,68,7);u32(mdl,72,156);
    u32(mdl,120,0x8);u32(mdl,124,308);
    u32(mdl,172,180);u32(mdl,324,332);
    mdl[196]=51;u16(mdl,202,count);
    mdl[348]=51;u16(mdl,354,2);
    for(unsigned i=0;i<2;++i)for(unsigned j=0;j<3;++j)f32(mdl,260+i*32+j*4,points[i][j]);
    return mdl;
}
int main(){try{
    _Make_SinTable();
    // npPushMdlstr2/npCalcMorphing skip the evalflags 0x8 object and every object
    // without a model, so only object 1 carries a stream.
    const float points[2][3]{{0,2,0},{2,0,0}};
    const auto model=stream_model(2,points);
    const auto streams=recvx::morph_streams(model,3);
    check(streams.objects()==3,"object count");
    check(streams.stream(0)==recvx::MorphStreams::none,"object without a model carries a stream");
    check(streams.stream(1)!=recvx::MorphStreams::none,"realized object lost its stream");
    check(streams.stream(2)==recvx::MorphStreams::none,"evalflags 0x8 object carries a stream");
    check(streams.count(1)==2,"usIndexMax of the realized object");
    float position[3]{},normal[3]{};
    // npCalcMorphing reads nb from obj_a alone, but npTransform then returns
    // without writing when obj_b disagrees.
    check(!recvx::morph_blend(streams,recvx::morph_streams(stream_model(3,points),3),1,0,500*0.001f,position,normal),"differing usIndexMax was blended");
    recvx::morph_blend(streams,streams,1,1,500*0.001f,position,normal);
    near(position[0],2,"same-stream position X");near(position[1],0,"same-stream position Y");near(position[2],0,"same-stream position Z");
    Bytes other=model;
    for(unsigned i=0;i<2;++i)f32(other,260+i*32,points[i][0]+4);
    recvx::morph_blend(streams,recvx::morph_streams(other,3),1,0,500*0.001f,position,normal);
    near(position[0],2,"half blend position X");near(position[1],2,"equal component copied verbatim");near(position[2],0,"source Z preserved");
    recvx::morph_blend(streams,recvx::morph_streams(other,3),1,0,1000*0.001f,position,normal);
    near(position[0],4,"full-strength blend X");near(position[1],2,"full-strength blend Y");
    // npCalcMorphing feeds the skin pass, not the finished geometry: the drawn
    // point at shp_ct 1000 is obj_b's own bind point, at 500 the midpoint.
    const float base[3][3]{{2,0,0},{0,2,0},{0,0,2}},target[3][3]{{6,0,0},{0,2,0},{0,0,2}};
    recvx::PlayerAsset a(actor_asset(base)),b(actor_asset(target));
    check(a.sample(0,0).triangles.size()==1,"fixture has no skinned triangle");
    check(a.bones()==b.bones(),"morph pair skeletons differ");
    const auto first=a.sample(0,0).triangles[0].vertices[0].position;
    const auto second=b.sample(0,0).triangles[0].vertices[0].position;
    near(first.x,14,"obj_a bind vertex");near(second.x,18,"obj_b bind vertex");
    recvx::MorphSource morph{&b.encoded_model(),0};
    const auto rest=a.sample(0,0,{},0,nullptr,{},nullptr,&morph).triangles[0].vertices[0].position;
    near(rest.x,first.x,"shp_ct 0 changed the drawn vertex");
    morph.shape=1000;
    const auto full=a.sample(0,0,{},0,nullptr,{},nullptr,&morph).triangles[0].vertices[0].position;
    near(full.x,second.x,"shp_ct 1000 did not reach obj_b");
    morph.shape=500;
    const auto half=a.sample(0,0,{},0,nullptr,{},nullptr,&morph).triangles[0].vertices[0].position;
    near(half.x,0.5f*(first.x+second.x),"shp_ct 500 is not the midpoint");
    // npPushMdlstr2/npPopMdlstr2 bracket the draw: the immutable stream survives.
    near(a.sample(0,0).triangles[0].vertices[0].position.x,first.x,"blend leaked into the immutable stream");
    std::cout<<"morph ok\n";return 0;
}catch(const std::exception& error){std::cerr<<"morph test failed: "<<error.what()<<"\n";return 1;}}
