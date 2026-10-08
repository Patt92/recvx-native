// Host graphics synthesizer for the original Ninja/GS packet code.
// Register and drawing semantics follow the PS2 GS: PACKED GIF data, vertex
// kick per PRIM type, TFX MODULATE with TCC=1, FGE fog, alpha test, Z test
// (16-bit Z buffer), alpha blending ((A-B)*C>>7)+D with PABE/FBA and
// COLCLAMP, bilinear TEX1, CLAMP/REPEAT and scissoring.
#include "core_gs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
extern "C" unsigned int palbuf[4096];
namespace {
struct Vertex {float x=0,y=0;std::uint32_t z=0;float s=0,t=0,q=1;float u=0,v=0;std::uint8_t r=0,g=0,b=0,a=0,f=255;};
struct Gs {
    std::vector<std::uint32_t> frame=std::vector<std::uint32_t>(RECVX_CORE_GS_WIDTH*RECVX_CORE_GS_HEIGHT,0);
    std::vector<std::uint16_t> depth=std::vector<std::uint16_t>(RECVX_CORE_GS_WIDTH*RECVX_CORE_GS_HEIGHT,0);
    std::uint64_t prim=0,test=0,alpha=0,fogcol=0,tex1=0,clamp=0,zbuf=0;unsigned pabe=0,fba=0;
    std::uint8_t r=0x80,g=0x80,b=0x80,a=0x80;float q=1,s=0,t=0;float u=0,v=0;std::uint8_t f=255;
    std::array<Vertex,3> queue{};unsigned count=0,fan_first=0;bool have_fan=false;
    const std::uint8_t* tim2=nullptr;unsigned bank=0;
    int sx0=0,sy0=0,sx1=RECVX_CORE_GS_WIDTH-1,sy1=RECVX_CORE_GS_HEIGHT-1;
    struct Ot {const std::uint8_t* tim2;unsigned bank;std::uint64_t alpha;std::vector<std::uint64_t> data;};
    std::array<std::vector<Ot>,4096> ot;
} gs;

unsigned field(std::uint64_t value,unsigned shift,unsigned bits){return unsigned((value>>shift)&((1ull<<bits)-1));}

// TIM2 picture in EE memory (TIM2_PICTUREHEADER_EX, image at +0x100). Paletted
// pictures (PictFormat 5..8) read the CLUT loaded into palbuf by ClutCopy /
// ClutCopy256 at bank*16 (Send_1024_Clut_data places bank b as an 8x2 CSM1
// block, so the GS lookup is linear there). Other pictures use their own CLUT
// uploaded in file order, whose CSM1 lookup swaps index bits 3 and 4.
struct Picture {const std::uint8_t* base;unsigned format,type,width,height,image_size,tw,th;};
std::uint16_t u16(const std::uint8_t* p){return std::uint16_t(p[0]|(p[1]<<8));}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
unsigned log2_ceil(unsigned v){return v<=1?0u:32u-unsigned(__builtin_clz(v-1));}
Picture picture(const std::uint8_t* p){return {p,p[0x90],p[0x93],u16(p+0x94),u16(p+0x96),u32(p+0x88),1u<<log2_ceil(u16(p+0x94)),1u<<log2_ceil(u16(p+0x96))};}
std::uint32_t argb(std::uint32_t rgba){return (rgba&0xff00ff00u)|((rgba&255u)<<16)|((rgba>>16)&255u);}
std::uint32_t texel(const Picture& t,int x,int y){
    const unsigned wms=field(gs.clamp,0,2),wmt=field(gs.clamp,2,2);
    auto wrap=[](int c,unsigned mode,unsigned size){if(mode==1)return std::clamp(c,0,int(size)-1);c%=int(size);return c<0?c+int(size):c;};
    x=wrap(x,wms,t.tw);y=wrap(y,wmt,t.th);
    if(unsigned(x)>=t.width||unsigned(y)>=t.height)return 0;
    const std::size_t i=std::size_t(y)*t.width+std::size_t(x);const auto* image=t.base+0x100;
    if(t.type==3)return argb(u32(image+i*4));
    if(t.type!=4&&t.type!=5)return 0;
    unsigned index=t.type==5?image[i]:(image[i/2]>>((i&1)*4))&15u;
    if(t.format>=5&&t.format<=8)return argb(palbuf[((gs.bank&0x3f)*16+index)&4095]);
    index=(index&~24u)|((index&8u)<<1)|((index&16u)>>1);
    return argb(u32(image+t.image_size+std::size_t(index)*4));
}
std::uint32_t sample(float u,float v){
    if(!gs.tim2)return 0x80808080u;
    const auto t=picture(gs.tim2);
    // TEX1 MMAG LINEAR samples around texel centres; NEAREST takes the texel.
    if(!field(gs.tex1,5,1))return texel(t,int(std::floor(u)),int(std::floor(v)));
    const float fu=u-.5f,fv=v-.5f;const int x0=int(std::floor(fu)),y0=int(std::floor(fv));const float ax=fu-float(x0),ay=fv-float(y0);
    const std::uint32_t c00=texel(t,x0,y0),c10=texel(t,x0+1,y0),c01=texel(t,x0,y0+1),c11=texel(t,x0+1,y0+1);
    std::uint32_t out=0;
    for(unsigned shift:{0u,8u,16u,24u}){
        auto ch=[&](std::uint32_t c){return float((c>>shift)&255);};
        const float value=(ch(c00)*(1-ax)+ch(c10)*ax)*(1-ay)+(ch(c01)*(1-ax)+ch(c11)*ax)*ay;
        out|=std::uint32_t(std::clamp(int(value+.5f),0,255))<<shift;
    }
    return out;
}
bool alpha_pass(unsigned value){
    const unsigned atst=field(gs.test,1,3),aref=field(gs.test,4,8);
    switch(atst){case 0:return false;case 1:return true;case 2:return value<aref;case 3:return value<=aref;case 4:return value==aref;case 5:return value>=aref;case 6:return value>aref;default:return value!=aref;}
}
void plot(int x,int y,std::uint32_t z,int r,int g,int b,int a){
    if(x<gs.sx0||x>gs.sx1||y<gs.sy0||y>gs.sy1||x<0||y<0||x>=RECVX_CORE_GS_WIDTH||y>=RECVX_CORE_GS_HEIGHT)return;
    const std::size_t at=std::size_t(y)*RECVX_CORE_GS_WIDTH+std::size_t(x);
    bool write_frame=true,write_z=!field(gs.zbuf,32,1),write_alpha=true;
    if(field(gs.test,0,1)&&!alpha_pass(unsigned(a))){
        switch(field(gs.test,12,2)){case 0:return;case 1:write_z=false;break;case 2:write_frame=false;break;default:write_alpha=false;write_z=false;break;}
    }
    if(field(gs.test,16,1)){
        const auto zz=std::uint16_t(std::min<std::uint32_t>(z,0xffff));
        switch(field(gs.test,17,2)){case 0:return;case 1:break;case 2:if(zz<gs.depth[at])return;break;default:if(zz<=gs.depth[at])return;}
    }
    const auto dst=gs.frame[at];
    if(field(gs.prim,6,1)&&!(gs.pabe&&!(a&0x80))){
        const int cs[3]={r,g,b},cd[3]={int((dst>>16)&255),int((dst>>8)&255),int(dst&255)};const int ad=int(dst>>24);
        const unsigned A=field(gs.alpha,0,2),B=field(gs.alpha,2,2),C=field(gs.alpha,4,2),D=field(gs.alpha,6,2),fix=field(gs.alpha,32,8);
        const int c=C==0?a:C==1?ad:int(fix);int out[3];
        for(unsigned i=0;i<3;++i){auto sel=[&](unsigned s){return s==0?cs[i]:s==1?cd[i]:0;};out[i]=std::clamp(((sel(A)-sel(B))*c>>7)+sel(D),0,255);}
        r=out[0];g=out[1];b=out[2];
    }
    if(gs.fba)a|=0x80;
    if(write_frame){
        std::uint32_t value=(std::uint32_t(std::clamp(r,0,255))<<16)|(std::uint32_t(std::clamp(g,0,255))<<8)|std::uint32_t(std::clamp(b,0,255));
        value|=write_alpha?std::uint32_t(std::clamp(a,0,255))<<24:(dst&0xff000000u);
        gs.frame[at]=value;
    }
    if(write_z&&field(gs.test,16,1))gs.depth[at]=std::uint16_t(std::min<std::uint32_t>(z,0xffff));
}
// Shades one pixel from interpolated attributes.
void shade(int x,int y,float z,float r,float g,float b,float a,float s,float t,float q,float u,float v,float f){
    int cr=int(r),cg=int(g),cb=int(b),ca=int(a);
    if(field(gs.prim,4,1)){
        float tu=u,tv=v;
        if(!field(gs.prim,8,1)){const float w=gs.tim2?float(picture(gs.tim2).tw):1,h=gs.tim2?float(picture(gs.tim2).th):1;tu=s/q*w;tv=t/q*h;}
        const auto c=sample(tu,tv);
        const int tr=int((c>>16)&255),tg=int((c>>8)&255),tb=int(c&255),ta=int(c>>24);
        cr=std::min(255,tr*cr>>7);cg=std::min(255,tg*cg>>7);cb=std::min(255,tb*cb>>7);ca=std::min(255,ta*ca>>7);
    }
    if(field(gs.prim,5,1)){const int fog=int(f),fr=int(field(gs.fogcol,0,8)),fg=int(field(gs.fogcol,8,8)),fb=int(field(gs.fogcol,16,8));
        cr=(fog*cr+(255-fog)*fr)>>8;cg=(fog*cg+(255-fog)*fg)>>8;cb=(fog*cb+(255-fog)*fb)>>8;}
    plot(x,y,std::uint32_t(std::max(0.f,z)),cr,cg,cb,ca);
}
struct Attr {float v[11];};
Attr attributes(const Vertex& p){return {{float(p.z),float(p.r),float(p.g),float(p.b),float(p.a),p.s,p.t,p.q,p.u,p.v,float(p.f)}};}
void triangle(const Vertex& p0,const Vertex& p1,const Vertex& p2){
    const bool gouraud=field(gs.prim,3,1);
    Attr a0=attributes(p0),a1=attributes(p1),a2=attributes(p2);
    if(!gouraud)for(unsigned i=1;i<5;++i)a0.v[i]=a1.v[i]=a2.v[i];
    const float area=(p1.x-p0.x)*(p2.y-p0.y)-(p2.x-p0.x)*(p1.y-p0.y);if(area==0)return;
    const int x0=std::max(gs.sx0,int(std::ceil(std::min({p0.x,p1.x,p2.x})))),x1=std::min(gs.sx1,int(std::ceil(std::max({p0.x,p1.x,p2.x})))-1);
    const int y0=std::max(gs.sy0,int(std::ceil(std::min({p0.y,p1.y,p2.y})))),y1=std::min(gs.sy1,int(std::ceil(std::max({p0.y,p1.y,p2.y})))-1);
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
        const float px=float(x),py=float(y);
        float w0=((p1.x-px)*(p2.y-py)-(p2.x-px)*(p1.y-py))/area,w1=((p2.x-px)*(p0.y-py)-(p0.x-px)*(p2.y-py))/area,w2=1-w0-w1;
        // Top-left fill convention: samples on a right/bottom edge are excluded.
        if(w0<0||w1<0||w2<0)continue;
        if((w0==0&&!(p1.y<p2.y||(p1.y==p2.y&&p1.x>p2.x)))||(w1==0&&!(p2.y<p0.y||(p2.y==p0.y&&p2.x>p0.x)))||(w2==0&&!(p0.y<p1.y||(p0.y==p1.y&&p0.x>p1.x))))continue;
        float v[11];for(unsigned i=0;i<11;++i)v[i]=a0.v[i]*w0+a1.v[i]*w1+a2.v[i]*w2;
        shade(x,y,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10]);
    }
}
void sprite(const Vertex& p0,const Vertex& p1){
    const float lx=std::min(p0.x,p1.x),hx=std::max(p0.x,p1.x),ly=std::min(p0.y,p1.y),hy=std::max(p0.y,p1.y);
    const int x0=std::max(gs.sx0,int(std::ceil(lx))),x1=std::min(gs.sx1,int(std::ceil(hx))-1),y0=std::max(gs.sy0,int(std::ceil(ly))),y1=std::min(gs.sy1,int(std::ceil(hy))-1);
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
        const float tx=hx>lx?(float(x)-p0.x)/(p1.x-p0.x):0,ty=hy>ly?(float(y)-p0.y)/(p1.y-p0.y):0;
        shade(x,y,float(p1.z),p1.r,p1.g,p1.b,p1.a,p0.s+(p1.s-p0.s)*tx,p0.t+(p1.t-p0.t)*ty,p1.q,p0.u+(p1.u-p0.u)*tx,p0.v+(p1.v-p0.v)*ty,p1.f);
    }
}
void line(const Vertex& p0,const Vertex& p1){
    const float dx=p1.x-p0.x,dy=p1.y-p0.y;const int steps=std::max(1,int(std::ceil(std::max(std::fabs(dx),std::fabs(dy)))));
    const bool gouraud=field(gs.prim,3,1);
    for(int i=0;i<steps;++i){const float t=float(i)/float(steps);const auto& c=gouraud?p0:p1;auto mix=[&](float a,float b){return gouraud?a+(b-a)*t:b;};
        shade(int(std::floor(p0.x+dx*t)),int(std::floor(p0.y+dy*t)),float(p0.z)+(float(p1.z)-float(p0.z))*t,mix(c.r,p1.r),mix(c.g,p1.g),mix(c.b,p1.b),mix(c.a,p1.a),p0.s+(p1.s-p0.s)*t,p0.t+(p1.t-p0.t)*t,p1.q,p0.u+(p1.u-p0.u)*t,p0.v+(p1.v-p0.v)*t,p1.f);}
}
void set_prim(std::uint64_t value){gs.prim=value;gs.count=0;gs.have_fan=false;}
// Vertex kick; `draw` is false when ADC is set (the vertex only enters the queue).
void kick(Vertex v,bool draw){
    const unsigned type=field(gs.prim,0,3);
    switch(type){
    case 0:if(draw)shade(int(v.x),int(v.y),float(v.z),v.r,v.g,v.b,v.a,v.s,v.t,v.q,v.u,v.v,v.f);return;
    case 1:case 6:{gs.queue[gs.count++]=v;if(gs.count==2){if(draw){if(type==1)line(gs.queue[0],gs.queue[1]);else sprite(gs.queue[0],gs.queue[1]);}gs.count=0;}return;}
    case 2:{if(gs.count&&draw)line(gs.queue[0],v);gs.queue[0]=v;gs.count=1;return;}
    case 3:{gs.queue[gs.count++]=v;if(gs.count==3){if(draw)triangle(gs.queue[0],gs.queue[1],gs.queue[2]);gs.count=0;}return;}
    case 4:{if(gs.count<2){gs.queue[gs.count++]=v;return;}if(draw)triangle(gs.queue[0],gs.queue[1],v);gs.queue[0]=gs.queue[1];gs.queue[1]=v;return;}
    case 5:{if(gs.count==0){gs.queue[0]=v;gs.count=1;return;}if(gs.count==1){gs.queue[1]=v;gs.count=2;return;}if(draw)triangle(gs.queue[0],gs.queue[1],v);gs.queue[1]=v;return;}
    default:return;
    }
}
void write_register(unsigned address,std::uint64_t data){
    switch(address){
    case 0x00:set_prim(data);break;
    case 0x01:gs.r=std::uint8_t(field(data,0,8));gs.g=std::uint8_t(field(data,8,8));gs.b=std::uint8_t(field(data,16,8));gs.a=std::uint8_t(field(data,24,8));{std::uint32_t qbits=std::uint32_t(data>>32);std::memcpy(&gs.q,&qbits,4);}break;
    case 0x02:{std::uint32_t sb=std::uint32_t(data),tb=std::uint32_t(data>>32);std::memcpy(&gs.s,&sb,4);std::memcpy(&gs.t,&tb,4);break;}
    case 0x03:gs.u=float(field(data,0,14))/16.f;gs.v=float(field(data,16,14))/16.f;break;
    case 0x04:case 0x0c:case 0x05:case 0x0d:{Vertex v;v.x=float(field(data,0,16))/16.f-1728.f;v.y=float(field(data,16,16))/16.f-1808.f;
        const bool fog=address==0x04||address==0x0c;v.z=fog?field(data,32,24):field(data,32,32);v.f=fog?std::uint8_t(field(data,56,8)):gs.f;if(fog)gs.f=v.f;
        v.r=gs.r;v.g=gs.g;v.b=gs.b;v.a=gs.a;v.s=gs.s;v.t=gs.t;v.q=gs.q;v.u=gs.u;v.v=gs.v;kick(v,address<0x08);break;}
    case 0x0a:gs.f=std::uint8_t(field(data,56,8));break;
    case 0x14:gs.tex1=data;break;case 0x08:gs.clamp=data;break;
    case 0x3d:gs.fogcol=data;break;
    case 0x42:gs.alpha=data;break;case 0x47:gs.test=data;break;case 0x49:gs.pabe=unsigned(data&1);break;
    case 0x4a:gs.fba=unsigned(data&1);break;case 0x4e:gs.zbuf=data;break;
    // SCISSOR_1: SCAX0 0-10, SCAX1 16-26, SCAY0 32-42, SCAY1 48-58 (window coordinates, inclusive).
    case 0x40:gs.sx0=int(field(data,0,11));gs.sx1=std::min(int(field(data,16,11)),RECVX_CORE_GS_WIDTH-1);
        gs.sy0=int(field(data,32,11));gs.sy1=std::min(int(field(data,48,11)),RECVX_CORE_GS_HEIGHT-1);break;
    // TEX0_1: the image is bound by handle (recvx_core_gs_bind_texture); palette bank follows from the
    // CLUT address (CBP 0x3FF0 + bank/16 quarter) and CSA as encoded by the original texture path.
    case 0x06:{const unsigned psm=field(data,20,6);
        if(psm==0x13||psm==0x14)gs.bank=((((field(data,37,14)-0x3FF0u)*4u)&0x30u)|field(data,56,5));break;}
    default:break; // TEX0/TEXFLUSH/frame setup: textures are bound by handle.
    }
}
// PACKED-mode register data (GIF REGS descriptors 0..0xe).
void packed(unsigned reg,const std::uint64_t* q){
    const std::uint64_t lo=q[0],hi=q[1];
    switch(reg){
    case 0x0:write_register(0x00,lo);break;
    case 0x1:gs.r=std::uint8_t(field(lo,0,8));gs.g=std::uint8_t(field(lo,32,8));gs.b=std::uint8_t(field(hi,0,8));gs.a=std::uint8_t(field(hi,32,8));break;
    case 0x2:{std::uint32_t sb=std::uint32_t(lo),tb=std::uint32_t(lo>>32),qb=std::uint32_t(hi);std::memcpy(&gs.s,&sb,4);std::memcpy(&gs.t,&tb,4);std::memcpy(&gs.q,&qb,4);break;}
    case 0x3:gs.u=float(field(lo,0,14))/16.f;gs.v=float(field(lo,32,14))/16.f;break;
    case 0x4:{Vertex v;v.x=float(field(lo,0,16))/16.f-1728.f;v.y=float(field(lo,32,16))/16.f-1808.f;v.z=field(hi,4,24);v.f=std::uint8_t(field(hi,36,8));gs.f=v.f;
        v.r=gs.r;v.g=gs.g;v.b=gs.b;v.a=gs.a;v.s=gs.s;v.t=gs.t;v.q=gs.q;v.u=gs.u;v.v=gs.v;kick(v,!field(hi,47,1));break;}
    case 0x5:{Vertex v;v.x=float(field(lo,0,16))/16.f-1728.f;v.y=float(field(lo,32,16))/16.f-1808.f;v.z=std::uint32_t(hi);v.f=gs.f;
        v.r=gs.r;v.g=gs.g;v.b=gs.b;v.a=gs.a;v.s=gs.s;v.t=gs.t;v.q=gs.q;v.u=gs.u;v.v=gs.v;kick(v,!field(hi,47,1));break;}
    case 0xa:gs.f=std::uint8_t(field(hi,36,8));break;
    case 0xe:write_register(unsigned(hi&0xff),lo);break;
    default:break;
    }
}
// GIF tag stream (PACKED mode only, as built by the original Ninja routines).
const std::uint64_t* gif(const std::uint64_t* q,const std::uint64_t* end){
    while(q<end){
        const std::uint64_t tag=q[0],regs=q[1];q+=2;
        const unsigned nloop=field(tag,0,15),eop=field(tag,15,1),pre=field(tag,46,1),flg=field(tag,58,2);unsigned nreg=field(tag,60,4);if(!nreg)nreg=16;
        if(pre)set_prim(field(tag,47,11));
        if(flg==0){for(unsigned loop=0;loop<nloop&&q<end;++loop)for(unsigned r=0;r<nreg&&q<end;++r,q+=2)packed(unsigned((regs>>(4*r))&15),q);}
        else{const std::uint64_t words=std::uint64_t(nloop)*(flg==1?nreg:2);q+=(words+1)/2*2;} // REGLIST/IMAGE data: not used by these routines
        if(eop)break;
    }
    return q;
}
}
extern "C" {
void recvx_core_gs_execute(const std::uint64_t* packet){
    // DMA tag: QWC in bits 0-15 (DMAend | qwc); GIF data follows.
    const unsigned qwc=unsigned(packet[0]&0xffff);gif(packet+2,packet+2+std::size_t(qwc)*2);
}
void recvx_core_gs_bind_texture(const void* tim2,unsigned bank){gs.tim2=static_cast<const std::uint8_t*>(tim2);gs.bank=bank;}
void recvx_core_gs_scissor(int x0,int y0,int x1,int y1){gs.sx0=x0;gs.sy0=y0;gs.sx1=x1;gs.sy1=y1;}
void recvx_core_gs_scissor_reset(void){gs.sx0=0;gs.sy0=0;gs.sx1=RECVX_CORE_GS_WIDTH-1;gs.sy1=RECVX_CORE_GS_HEIGHT-1;}
void recvx_core_gs_clear(std::uint32_t argb){std::fill(gs.frame.begin(),gs.frame.end(),argb);std::fill(gs.depth.begin(),gs.depth.end(),std::uint16_t(0));}
void recvx_core_gs_ot_add(unsigned otz,const void* tim2,unsigned bank,std::uint64_t alpha,const std::uint64_t* data,unsigned qwords){
    gs.ot[std::min(otz,4095u)].push_back({static_cast<const std::uint8_t*>(tim2),bank,alpha,std::vector<std::uint64_t>(data,data+std::size_t(qwords)*2)});
}
void recvx_core_gs_ot_clear(void){for(auto& bucket:gs.ot)bucket.clear();}
void recvx_core_gs_ot_draw(void){
    // Original ordering-table traversal: buckets in ascending OT index, entries in insertion order;
    // each entry restores its texture/bank and ALPHA before its GIF data.
    for(auto& bucket:gs.ot)for(auto& entry:bucket){
        gs.tim2=entry.tim2;gs.bank=entry.bank;gs.alpha=entry.alpha;
        gif(entry.data.data(),entry.data.data()+entry.data.size());
    }
}
const std::uint32_t* recvx_core_gs_framebuffer(void){return gs.frame.data();}
}
