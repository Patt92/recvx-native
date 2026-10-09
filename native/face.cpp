#include "face.h"
#include "binary_view.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <functional>
#include <stdexcept>
namespace recvx {
std::vector<FaceParameter> decode_face_parameters(const std::vector<std::uint8_t>& b,std::size_t p,unsigned n,bool lip,bool mask_lip_bank){
    BinaryView v(b);if(n>65536)throw std::runtime_error("too many facial keys");v.require(p,std::size_t(n)*164);std::vector<FaceParameter> out(n);
    for(auto& k:out){for(unsigned j=0;j<32;++j)k.muscle[j]=v.f32(p+j*4);
        // MASK lip angles retain signed angle bits in PARAM_WORK's float
        // fields (face_bh.c fmCnkSetParamLip). Negative angles can have IEEE
        // NaN bits. Preserve them until the lip consumer converts them.
        auto angle=[&](std::size_t at){if(lip)return v.i32(at)*0.005493164f;if(mask_lip_bank){const auto bits=v.u32(at);float raw;std::memcpy(&raw,&bits,4);return raw;}return v.f32(at);};
        k.jawang=angle(p+128);k.jawtrans=v.f32(p+132);k.eye={v.f32(p+136),v.f32(p+140),v.f32(p+144)};
        k.tangx=angle(p+148);k.tangy=angle(p+152);k.tangz=v.f32(p+156);k.frame=short(v.u16(p+160));k.flag=short(v.u16(p+162));p+=164;}
    return out;
}
FaceParameter sample_face_parameters(const std::vector<FaceParameter>& keys,unsigned frame){
    if(keys.empty())throw std::runtime_error("empty facial key list");
    unsigned i=0;while(i<keys.size()&&unsigned(keys[i].frame)<=frame)++i;
    if(!i)return keys.front();if(i==keys.size())return keys.back();const auto& a=keys[i-1];const auto& b=keys[i];
    return interpolate_face_parameters(a,b,(float(frame)-a.frame)/(float(b.frame)-a.frame));
}
FaceParameter interpolate_face_parameters(const FaceParameter& a,const FaceParameter& b,float t,float level,unsigned mask){
    float rate=t;switch(a.flag){case 1:rate=t*t;break;case 2:rate=njSqrt(t);break;case 3:rate=.5f-.5f*njCos(int(32768*t));break;case 4:rate=0;break;}
    FaceParameter out=a;for(unsigned j=0;j<32;++j)if(mask&(1u<<j))out.muscle[j]=level*(a.muscle[j]+rate*(b.muscle[j]-a.muscle[j]));
    auto mix=[&](float x,float y){return x+rate*(y-x);};out.jawang=level*mix(a.jawang,b.jawang);out.jawtrans=level*mix(a.jawtrans,b.jawtrans);out.eye={mix(a.eye.x,b.eye.x),mix(a.eye.y,b.eye.y),mix(a.eye.z,b.eye.z)};out.tangx=level*mix(a.tangx,b.tangx);out.tangy=level*mix(a.tangy,b.tangy);out.tangz=level*mix(a.tangz,b.tangz);return out;
}
FaceParameter lip_face_parameter(FaceParameter p){auto angle=[](float x){std::int32_t value;std::memcpy(&value,&x,4);return value*0.005493164f;};p.jawang=angle(p.jawang);p.tangx=angle(p.tangx);p.tangy=angle(p.tangy);return p;
}

namespace {
NJS_POINT3 point(const std::vector<std::uint8_t>& b,std::size_t p){BinaryView v(b);return {v.f32(p),v.f32(p+4),v.f32(p+8)};}
void store(std::vector<std::uint8_t>& b,std::size_t p,NJS_POINT3 value){std::memcpy(b.data()+p,&value,12);}
}
FaceAsset::FaceAsset(const std::vector<std::uint8_t>& model,const std::vector<std::uint8_t>& mask){
    BinaryView v(model),m(mask);m.require(0,64);const auto base=v.u16(4),n=v.u16(6);const auto nodes=v.relative(base,v.u32(12),n*52);std::vector<unsigned> meshes;
    std::function<void(unsigned)> walk=[&](unsigned index){const auto o=nodes+index*52;if(v.u32(o+4)!=0xffffffff)meshes.push_back(index);for(unsigned field:{44u,48u})if(v.u32(o+field)!=0xffffffff){const auto at=v.relative(base,v.u32(o+field),52);walk(unsigned((at-nodes)/52));}};walk(0);
    auto mesh=[&](unsigned id)->std::optional<Mesh>{if(id==255)return {};if(id>=meshes.size())throw std::runtime_error("MASK mesh index outside hierarchy");Mesh out;out.object=meshes[id];const auto o=nodes+out.object*52,mdl=v.relative(base,v.u32(o+4),24),chunk=v.relative(base,v.u32(mdl),8);const auto type=v.u8(chunk);if(type<32||type>51)throw std::runtime_error("unsupported MASK vertex chunk");
        out.count=v.u16(chunk+6);out.payload=chunk+(type==51?64:8);out.stride=type==51||type==33?32:type==41?24:(type>=42&&type<=47)?28:type==34?12:(type==49||type==50)?20:16;
        out.normal=type==51||type==33?16:(type>=41&&type<=47)?12:0;
        v.require(out.payload,std::size_t(out.count)*out.stride);out.angles={v.i32(o+20),v.i32(o+24),v.i32(o+28)};out.position=point(model,o+8);return out;};
    const auto face=mesh(m.u8(52));if(!face)throw std::runtime_error("MASK has no face mesh");face_=*face;const auto tongue=mesh(m.u8(53));if(tongue)tongue_=*tongue;tooth_=mesh(m.u8(54));for(unsigned i=0;i<9;++i)eyes_[i]=mesh(m.u8(55+i));
    // _fmCnkSetEyeballObject saves the bind matrix after resetting the eye
    // parent's angles. _fmCnkCalcEye uses its forward-vector sign for pitch.
    std::function<void(unsigned,NJS_MATRIX&)> eye_bind=[&](unsigned index,NJS_MATRIX& parent){NJS_MATRIX world;std::copy_n(parent,16,world);const auto o=nodes+index*52;const auto pos=point(model,o+8);njTranslate(&world,pos.x,pos.y,pos.z);bool eye=false;for(const auto& e:eyes_)eye=eye||(e&&e->object==index);if(!eye)njRotateXYZ(&world,v.i32(o+20),v.i32(o+24),v.i32(o+28));for(auto& e:eyes_)if(e&&e->object==index)e->gaze_sign=world[10]>0?1:-1;
        if(v.u32(o+44)!=0xffffffff)eye_bind(unsigned((v.relative(base,v.u32(o+44),52)-nodes)/52),world);if(v.u32(o+48)!=0xffffffff)eye_bind(unsigned((v.relative(base,v.u32(o+48),52)-nodes)/52),parent);};NJS_MATRIX root;njUnitMatrix(&root);eye_bind(0,root);
    const auto nl=m.u32(16),nc=m.u32(20),nj=m.u32(24),nt=m.u32(28);m.require(64,std::size_t(nl)*4);std::size_t normal_count=0;for(unsigned i=0;i<nl;++i)normal_count+=m.u8(64+i*4+3);
    auto p=64+std::size_t(nl)*4+normal_count*4;m.require(p,std::size_t(nc)*20+std::size_t(nj+nt)*8);unsigned connection=0;
    for(unsigned i=0;i<nl;++i){Group g;g.vertex=m.u16(64+i*4);if(g.vertex>=face_.count)throw std::runtime_error("MASK vertex outside face");for(unsigned j=0;j<m.u8(64+i*4+2);++j){if(connection++>=nc)throw std::runtime_error("MASK connection count mismatch");const auto id=m.u32(p);if(id>=32)throw std::runtime_error("MASK muscle outside parameters");g.muscles.push_back({id,point(mask,p+4),m.f32(p+16)});p+=20;}groups_.push_back(std::move(g));}
    if(connection!=nc)throw std::runtime_error("MASK unused connections");
    for(unsigned i=0;i<nj;++i){const auto id=m.u32(p);if(id>=face_.count)throw std::runtime_error("MASK jaw vertex outside face");jaw_.push_back({id,m.f32(p+4)});p+=8;}
    for(unsigned i=0;i<nt;++i){const auto id=m.u32(p);if(!tongue||id>=tongue_.count)throw std::runtime_error("MASK tongue vertex outside mesh");tang_.push_back({id,m.f32(p+4)});p+=8;}
    keys_=decode_face_parameters(mask,p,m.u32(32),false,true);if(keys_.empty())throw std::runtime_error("MASK has no parameters");
    const auto a=m.u16(8),b=m.u16(10);if(a>=face_.count||b>=face_.count)throw std::runtime_error("MASK jaw axis outside face");const auto origin=point(model,face_.payload+a*face_.stride),end=point(model,face_.payload+b*face_.stride);const float x=end.x-origin.x,y=end.y-origin.y,z=end.z-origin.z;
    const int ry=int(10430.378f*std::atan(y/x)),rz=int(10430.378f*std::atan(z/std::sqrt(x*x+y*y)));
    njUnitMatrix(&jaw_to_);njRotateY(&jaw_to_,-rz);njRotateZ(&jaw_to_,-ry);njTranslate(&jaw_to_,-origin.x,-origin.y,-origin.z);
    njUnitMatrix(&jaw_from_);njTranslate(&jaw_from_,origin.x,origin.y,origin.z);njRotateZ(&jaw_from_,ry);njRotateY(&jaw_from_,rz);
}
void FaceAsset::deform(std::vector<std::uint8_t>& model,const FaceParameter& param)const{
    // _fmCnkCalcMuscle: source copy plus normalized signed connection vectors.
    for(const auto& g:groups_){auto p=point(model,face_.payload+g.vertex*face_.stride);float sum=0;NJS_POINT3 delta{};for(const auto& c:g.muscles){const auto value=param.muscle[c.muscle],rate=c.scale*std::abs(value);sum+=rate;delta.x+=c.vector.x*value*rate;delta.y+=c.vector.y*value*rate;delta.z+=c.vector.z*value*rate;}if(sum!=0){p.x+=delta.x/sum;p.y+=delta.y/sum;p.z+=delta.z/sum;}store(model,face_.payload+g.vertex*face_.stride,p);}
    // _fmCnkCalcJaw uses separate angle rates, but full jaw translation.
    for(const auto& w:jaw_){NJS_MATRIX matrix;std::copy_n(jaw_from_,16,matrix);njRotateX(&matrix,int(-param.jawang*w.rate*182.04445f)&65535);njTranslate(&matrix,param.jawtrans,0,0);NJS_MATRIX to;std::copy_n(jaw_to_,16,to);njMultiMatrix(&matrix,&to);auto p=point(model,face_.payload+w.vertex*face_.stride);njCalcPoint(&matrix,&p,&p);store(model,face_.payload+w.vertex*face_.stride,p);}
    for(const auto& w:tang_){NJS_MATRIX matrix;njUnitMatrix(&matrix);njRotateX(&matrix,int(param.tangx*w.rate*182.04445f)&65535);njRotateY(&matrix,int(param.tangy*w.rate*182.04445f)&65535);auto p=point(model,tongue_.payload+w.vertex*tongue_.stride);p.z*=param.tangz;njCalcPoint(&matrix,&p,&p);store(model,tongue_.payload+w.vertex*tongue_.stride,p);}
    // Inserted children retain the source object's rotation. Their parent carries
    // the procedural jaw/eye rotation; apply that product to the mesh vertices.
    auto transform=[&](const Mesh& mesh,NJS_MATRIX& parent){NJS_MATRIX matrix;std::copy_n(parent,16,matrix);njRotateXYZ(&matrix,mesh.angles[0],mesh.angles[1],mesh.angles[2]);NJS_MATRIX inverse;njUnitMatrix(&inverse);njRotateZ(&inverse,-mesh.angles[2]);njRotateY(&inverse,-mesh.angles[1]);njRotateX(&inverse,-mesh.angles[0]);njMultiMatrix(&inverse,&matrix);for(unsigned i=0;i<mesh.count;++i){auto p=point(model,mesh.payload+i*mesh.stride);njCalcPoint(&inverse,&p,&p);store(model,mesh.payload+i*mesh.stride,p);if(mesh.normal){auto n=point(model,mesh.payload+i*mesh.stride+mesh.normal);njCalcVector(&inverse,&n,&n);store(model,mesh.payload+i*mesh.stride+mesh.normal,n);}}};
    NJS_MATRIX jaw;std::copy_n(jaw_from_,16,jaw);jaw[12]=jaw[13]=jaw[14]=0;njRotateX(&jaw,int(-param.jawang*182.04445f)&65535);if(tooth_)transform(*tooth_,jaw);if(!tang_.empty())transform(tongue_,jaw);
    for(const auto& eye:eyes_)if(eye){NJS_MATRIX matrix;njUnitMatrix(&matrix);const auto p=param.eye;const float length=std::sqrt(p.x*p.x+p.z*p.z);const int x=int(10430.381f*std::atan(p.y/length));const int y=int(10430.381f*std::atan(p.x/p.z))+(p.z<0?0:32768);njRotateXYZ(&matrix,eye->gaze_sign*x,y,0);transform(*eye,matrix);}
}
}
