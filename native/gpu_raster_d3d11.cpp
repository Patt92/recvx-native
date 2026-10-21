// Direct3D-11-Backend des Host-GPU-Rasterers (gpu_raster.h), nur Windows. Plattformanpassung,
// keine Spiellogik: es rastert die raster::Setup-Liste des SoftwareRenderers mit dem Koerper der
// Pixelschleife der Bandstufe (renderer.cpp, rasterize) und liefert Bild und Tiefe zurueck.
//
// Gegenstueck zum programmierbaren Mischen des Metal-Backends sind Rasterizer Ordered Views
// (D3D11.3, D3D11_FEATURE_D3D11_OPTIONS2.ROVsSupported): Farbe, Tiefe, transportierte Tiefe und
// Schattenmaske liegen als RasterizerOrderedTexture2D vor, der Pixelshader liest den bisherigen
// Wert in garantierter Zeichenreihenfolge und schreibt ihn wie die Software. Ohne ROV gibt es
// kein Backend (Softwarepfad), statt eines nur aehnlich wirkenden Mischens.
#include "gpu_raster.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_3.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
namespace recvx {
namespace {
// Shaderquelle: dieselben Rechnungen wie native/gpu_raster_metal.mm und renderer.cpp rasterize().
const char* shader_source=R"HLSL(
struct Setup {
    float4 p0,p1,p2;   // x, y, 1/z, transportierte Tiefe
    float4 uv0;        // u/z0, v/z0, u/z1, v/z1
    float4 uv1;        // u/z2, v/z2, area, Wx
    float4 grad;       // Ux, Uy, Vx, Vy
    float4 l0,l1,l2;   // Licht xyz, Eckalpha
    float4 fog;        // F0, F1, F2, Wy
    uint4 info;        // Farbe, Flags, Nebelfarbe, Divisor | max_alpha << 16
    uint4 blend;       // GS ALPHA lo, hi, Art (0 undurchsichtig, 1 additiv, 2 GS), 0
    int4 box;          // minx, miny, maxx, maxy
};
struct ShadowTri { float4 v01; float4 v2a; uint4 depth; int4 box; };
StructuredBuffer<Setup> Setups : register(t0);
StructuredBuffer<ShadowTri> Tris : register(t1);
Texture2D<float4> Tex : register(t2);
Texture2D<uint> TexPoint : register(t3);
SamplerState Samp : register(s0);
RasterizerOrderedTexture2D<uint> Color : register(u0);
RasterizerOrderedTexture2D<float> DepthBuf : register(u1);
RasterizerOrderedTexture2D<float> Transported : register(u2);
RasterizerOrderedTexture2DArray<uint> Mask : register(u3);
cbuffer FrameCB : register(b0) { float gW,gH; uint gFogPixel,gWireframe; uint gBackground,gMaskW,gMaskH,gPad; };
cbuffer CompositeCB : register(b1) { float cW,cH,crx0,cry0,crx1,cry1; uint cFog,cLayer; };
// Flags: 1 gouraud, 2 vertex_alpha, 4 textured, 8 filtered, 16 punch, 32 fogged, 64 fog_color_set,
// 128 clamp_u, 256 clamp_v, 512 cutout, 1024 uniform_alpha, 2048 linear_only, 4096 nonopaque,
// 16384 general_water, 32768 premultiplied chain, 65536 depth_write.
uint ch(uint c,uint s){return (c>>s)&255u;}
bool inclusive(float2 from,float2 to){return to.y<from.y||(to.y==from.y&&to.x>from.x);}
uint lit(uint base,float r,float g,float b){
    return (base&0xff000000u)|(((uint)clamp((float)ch(base,16)*r+0.5f,0.0f,255.0f))<<16)
          |(((uint)clamp((float)ch(base,8)*g+0.5f,0.0f,255.0f))<<8)|((uint)clamp((float)ch(base,0)*b+0.5f,0.0f,255.0f));}
uint modulate(uint t,uint l,uint d){
    return 0xff000000u|(min(255u,(ch(t,16)*ch(l,16))/d)<<16)|(min(255u,(ch(t,8)*ch(l,8))/d)<<8)|min(255u,(ch(t,0)*ch(l,0))/d);}
// chunk_blend.cpp blend_gs: (A - B) * C + D je Kanal.
uint blend_gs(uint lo,uint hi,uint source,uint destination,float source_alpha){
    uint a=lo&3u,b=(lo>>2)&3u,c=(lo>>4)&3u,d=(lo>>6)&3u;
    float alpha=clamp(source_alpha,0.0f,1.0f);
    float factor=(c==0u)?alpha:((c==1u)?(float)(destination>>24)/255.0f:(float)(hi&255u)/128.0f);
    uint result=((uint)(alpha*255.0f+0.5f))<<24;
    [unroll] for(uint shift=0u;shift<24u;shift+=8u){
        float ca=(a==0u)?(float)ch(source,shift):((a==1u)?(float)ch(destination,shift):0.0f);
        float cb=(b==0u)?(float)ch(source,shift):((b==1u)?(float)ch(destination,shift):0.0f);
        float cd=(d==0u)?(float)ch(source,shift):((d==1u)?(float)ch(destination,shift):0.0f);
        result|=((uint)clamp((ca-cb)*factor+cd+0.5f,0.0f,255.0f))<<shift;}
    return result;
}
struct VOut { float4 pos : SV_Position; nointerpolation uint index : INDEX; };
VOut vs_raster(uint vid : SV_VertexID) {
    uint index=vid/6u,corner=vid%6u; int4 b=Setups[index].box;
    bool right=(corner==1u||corner==2u||corner==4u),bottom=(corner==2u||corner==4u||corner==5u);
    float x=(float)(right?b.z+1:b.x),y=(float)(bottom?b.w+1:b.y);
    VOut o; o.pos=float4(x/gW*2.0f-1.0f,1.0f-y/gH*2.0f,0.0f,1.0f); o.index=index; return o;
}
void ps_raster(VOut input) {
    Setup s=Setups[input.index];
    uint2 at=uint2(input.pos.xy);
    float2 p0=s.p0.xy,p1=s.p1.xy,p2=s.p2.xy; float X=input.pos.x,Y=input.pos.y;
    precise float a=(p2.x-p1.x)*(Y-p1.y)-(p2.y-p1.y)*(X-p1.x);
    precise float b=(p0.x-p2.x)*(Y-p2.y)-(p0.y-p2.y)*(X-p2.x);
    precise float c=(p1.x-p0.x)*(Y-p0.y)-(p1.y-p0.y)*(X-p0.x);
    a/=s.uv1.z; b/=s.uv1.z; c/=s.uv1.z;
    if(a<0.0f||b<0.0f||c<0.0f)discard;
    if((a==0.0f&&!inclusive(p1,p2))||(b==0.0f&&!inclusive(p2,p0))||(c==0.0f&&!inclusive(p0,p1)))discard;
    precise float invz=s.p0.z+b*(s.p1.z-s.p0.z)+c*(s.p2.z-s.p0.z);
    float below_depth=DepthBuf[at];
    if(invz<below_depth)discard; // GEQUAL
    uint color=s.info.x,flags=s.info.y,divisor=s.info.w&0xffffu; float max_alpha=(float)(s.info.w>>16);
    float material_alpha=(float)(color>>24)/255.0f*((flags&2u)?clamp(a*s.l0.w+b*s.l1.w+c*s.l2.w,0.0f,1.0f):1.0f);
    if(material_alpha<=0.0f)discard; // TEST GREATER 0 / KEEP
    uint kind=s.blend.z;
    uint below=Color[at];
    uint pixel=color; float coverage=1.0f;
    if(flags&1u)pixel=lit(color,a*s.l0.x+b*s.l1.x+c*s.l2.x,a*s.l0.y+b*s.l1.y+c*s.l2.y,a*s.l0.z+b*s.l1.z+c*s.l2.z);
    if(flags&4u){
        float rz=1.0f/invz;
        precise float u=(a*s.uv0.x+b*s.uv0.z+c*s.uv1.x)*rz;
        precise float v=(a*s.uv0.y+b*s.uv0.w+c*s.uv1.y)*rz;
        if(!(flags&8u)){
            u=(flags&128u)?clamp(u,0.0f,1.0f):u-floor(u);
            v=(flags&256u)?clamp(v,0.0f,1.0f):v-floor(v);
            uint tw,th; TexPoint.GetDimensions(tw,th);
            uint texel=TexPoint.Load(int3(min(tw-1u,(uint)(u*(float)tw)),min(th-1u,(uint)(v*(float)th)),0));
            if((texel>>24)==0u)discard;
            pixel=modulate(texel,pixel,divisor);
            if(kind!=0u)coverage=(float)(texel>>24)/255.0f;
        } else {
            // Ableitungen von (u,v) je Bildpixel wie die Bandstufe: d(U/W) = (dU - u*dW)/W.
            float Wx=s.uv1.w,Wy=s.fog.w;
            float2 gx=float2((s.grad.x-u*Wx)*rz,(s.grad.z-v*Wx)*rz),gy=float2((s.grad.y-u*Wy)*rz,(s.grad.w-v*Wy)*rz);
            if(!(flags&128u))u-=floor(u);
            if(!(flags&256u))v-=floor(v);
            float4 t=(flags&2048u)?Tex.SampleLevel(Samp,float2(u,v),0.0f):Tex.SampleGrad(Samp,float2(u,v),gx,gy);
            if(t.a<=0.0f)discard;
            if(kind!=0u)coverage=min(1.0f,(flags&1024u)?max_alpha/255.0f:t.a);
            else if((flags&512u)&&!(flags&16u)){coverage=min(1.0f,t.a*255.0f/max_alpha);if(coverage<1.0f/32.0f)discard;}
            float3 rgb=(flags&32768u)?t.rgb/t.a:t.rgb;
            uint texel=0xff000000u|(((uint)clamp(rgb.r*255.0f+0.5f,0.0f,255.0f))<<16)
                      |(((uint)clamp(rgb.g*255.0f+0.5f,0.0f,255.0f))<<8)|((uint)clamp(rgb.b*255.0f+0.5f,0.0f,255.0f));
            pixel=modulate(texel,pixel,divisor);
        }
    }
    if(flags&32u){ // GS: Cv = (Cs*F + Cf*(255-F)) >> 8
        uint F=(uint)clamp(a*s.fog.x+b*s.fog.y+c*s.fog.z,0.0f,255.0f); uint fog=(flags&64u)?s.info.z:gFogPixel;
        pixel=(pixel&0xff000000u)|(((ch(pixel,16)*F+ch(fog,16)*(255u-F))>>8)<<16)
             |(((ch(pixel,8)*F+ch(fog,8)*(255u-F))>>8)<<8)|((ch(pixel,0)*F+ch(fog,0)*(255u-F))>>8);
    }
    bool depth_write=(flags&65536u)!=0u;
    if(kind==1u){ // additiv, ohne Tiefe
        float alpha=coverage*material_alpha; uint mixed=0xff000000u;
        [unroll] for(uint shift=0u;shift<24u;shift+=8u)mixed|=min(255u,ch(below,shift)+(uint)((float)ch(pixel,shift)*alpha+0.5f))<<shift;
        Color[at]=mixed; return;}
    if(kind==2u){ // GS ALPHA; Tiefe ohne transportierte Tiefe (Ps2ZbuffOff alpha >= 128 / direktes Wasser)
        float alpha=coverage*material_alpha;
        Color[at]=blend_gs(s.blend.x,s.blend.y,pixel,below,alpha);
        if(depth_write&&((flags&16384u)?alpha>0.0f:alpha>=0.5f))DepthBuf[at]=invz;
        return;}
    if(coverage<0.999f){
        uint mixed=0xff000000u;
        [unroll] for(uint shift=0u;shift<24u;shift+=8u)mixed|=((uint)((float)ch(below,shift)*(1.0f-coverage)+(float)ch(pixel,shift)*coverage+0.5f))<<shift;
        pixel=mixed; if(coverage<0.5f){Color[at]=pixel;return;}
    }
    if(depth_write){DepthBuf[at]=invz;Transported[at]=a*s.p0.w+b*s.p1.w+c*s.p2.w;}
    Color[at]=(gWireframe!=0u&&min(min(a,b),c)>0.025f)?gBackground:pixel;
}
// Maskenrasterung von bhDrawMdfEffect (shadow_raster.h) im 640x480-Bild.
struct MaskVOut { float4 pos : SV_Position; nointerpolation uint index : INDEX; };
MaskVOut vs_mask(uint vid : SV_VertexID) {
    uint index=vid/6u,corner=vid%6u; int4 b=Tris[index].box;
    bool right=(corner==1u||corner==2u||corner==4u),bottom=(corner==2u||corner==4u||corner==5u);
    float x=(float)(right?b.z+1:b.x),y=(float)(bottom?b.w+1:b.y);
    MaskVOut o; o.pos=float4(x/640.0f*2.0f-1.0f,1.0f-y/480.0f*2.0f,0.0f,1.0f); o.index=index; return o;
}
void ps_mask(MaskVOut input) {
    ShadowTri t=Tris[input.index];
    float2 p0=t.v01.xy,p1=t.v01.zw,p2=t.v2a.xy; float X=input.pos.x,Y=input.pos.y;
    precise float a=(p2.x-p1.x)*(Y-p1.y)-(p2.y-p1.y)*(X-p1.x);
    precise float b=(p0.x-p2.x)*(Y-p2.y)-(p0.y-p2.y)*(X-p2.x);
    precise float c=(p1.x-p0.x)*(Y-p0.y)-(p1.y-p0.y)*(X-p0.x);
    if(a<0.0f||b<0.0f||c<0.0f)discard;
    if((a==0.0f&&!inclusive(p1,p2))||(b==0.0f&&!inclusive(p2,p0))||(c==0.0f&&!inclusive(p0,p1)))discard;
    precise float depth=(a*(float)t.depth.x+b*(float)t.depth.y+c*(float)t.depth.z)/t.v2a.z;
    uint sx=min(gMaskW-1u,(uint)(X*(float)gMaskW/640.0f)),sy=min(gMaskH-1u,(uint)(Y*(float)gMaskH/480.0f));
    if((uint)depth<(uint)Transported[uint2(sx,sy)])discard; // GEQUAL
    uint layer=t.depth.w>>1u;
    uint3 cell=uint3(uint(input.pos.x),uint(input.pos.y),layer);
    int value=(int)Mask[cell]+((t.depth.w&1u)?-32:32); // shadow_mask_fragment
    Mask[cell]=(uint)clamp(value,0,255)&0xf8u;
}
float4 vs_full(uint vid : SV_VertexID) : SV_Position {
    float2 p=float2((float)((vid<<1u)&2u),(float)(vid&2u)); return float4(p*2.0f-1.0f,0.0f,1.0f);
}
void ps_shadow(float4 pos : SV_Position) {
    uint2 at=uint2(pos.xy); float y=pos.y-0.5f,x=pos.x-0.5f;
    int my=min(479,(int)((y+0.5f)*480.0f/cH)); if((float)my+0.5f<cry0||(float)my+0.5f>=cry1)return;
    int mx=min(639,(int)((x+0.5f)*640.0f/cW)); if((float)mx+0.5f<crx0||(float)mx+0.5f>=crx1)return;
    uint value=Mask[uint3((uint)mx,(uint)my,cLayer)]; if(value==0u)return;
    uint reduction=(value*((cFog*3u)/16u))/128u; // shadow_mask_composite
    uint below=Color[at]; uint result=below&0xff000000u;
    [unroll] for(uint shift=0u;shift<24u;shift+=8u){uint v=ch(below,shift);result|=(v>reduction?v-reduction:0u)<<shift;}
    Color[at]=result;
}
RWTexture2D<uint> ColorRW : register(u0);
RWTexture2D<float> DepthRW : register(u1);
RWBuffer<uint> OutColor : register(u4);
RWBuffer<float> OutDepth : register(u5);
cbuffer ResolveCB : register(b2) { uint4 rSize; };
[numthreads(16,16,1)]
void cs_resolve(uint3 gid : SV_DispatchThreadID) {
    if(gid.x>=rSize.x||gid.y>=rSize.y)return;
    uint S=rSize.z,at=gid.y*rSize.x+gid.x;
    if(S==1u){OutColor[at]=ColorRW[gid.xy];OutDepth[at]=DepthRW[gid.xy];return;}
    uint r=0u,g=0u,b=0u; float nearest=0.0f; uint n=S*S;
    for(uint sy=0u;sy<S;++sy)for(uint sx=0u;sx<S;++sx){
        uint2 q=gid.xy*S+uint2(sx,sy); uint cv=ColorRW[q];
        r+=cv&255u;g+=(cv>>8)&255u;b+=(cv>>16)&255u;nearest=max(nearest,DepthRW[q]);}
    OutColor[at]=0xff000000u|(((b+n/2u)/n)<<16)|(((g+n/2u)/n)<<8)|((r+n/2u)/n);OutDepth[at]=nearest;
}
)HLSL";
template<class T> struct Com {
    T* p=nullptr;
    Com()=default; ~Com(){reset();}
    Com(const Com&)=delete; Com& operator=(const Com&)=delete;
    Com(Com&& o)noexcept:p(o.p){o.p=nullptr;}
    Com& operator=(Com&& o)noexcept{if(this!=&o){reset();p=o.p;o.p=nullptr;}return *this;}
    void reset(){if(p){p->Release();p=nullptr;}}
    T** put(){reset();return &p;}
    T* get()const{return p;}
    explicit operator bool()const{return p!=nullptr;}
    T* operator->()const{return p;}
};
void require(HRESULT hr,const char* what){if(FAILED(hr))throw std::runtime_error(std::string("D3D11: ")+what);}
struct alignas(16) GpuSetup{float p0[4],p1[4],p2[4],uv0[4],uv1[4],grad[4],l0[4],l1[4],l2[4],fog[4];std::uint32_t info[4],blend[4];std::int32_t box[4];};
static_assert(sizeof(GpuSetup)==208,"setup layout");
struct alignas(16) GpuShadowTri{float v01[4],v2a[4];std::uint32_t depth[4];std::int32_t box[4];};
static_assert(sizeof(GpuShadowTri)==64,"shadow triangle layout");
struct FrameCB{float W,H;std::uint32_t fog_pixel,wireframe,background,mask_w,mask_h,pad;};
struct CompositeCB{float W,H,rx0,ry0,rx1,ry1;std::uint32_t fog,layer;};
struct ResolveCB{std::uint32_t size[4];};
// Darstellungsart einer Textur: Stufe 0 (Punktabtastung), gerade Kette, praemultiplizierte Kette
// (alpha-gewichtete Mittelung), Punch-Kette (RGB ungewichtet).
enum Variant{Point=1,Straight=2,Premultiplied=3,Punch=4};
struct Batch{std::size_t first,count;ID3D11ShaderResourceView* texture;unsigned sampler;bool point;};
using PFN_D3D11_CREATE_DEVICE=HRESULT(WINAPI*)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
using PFN_D3DCOMPILE=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
class D3D11Raster final:public GpuRaster{
public:
    D3D11Raster(ID3D11Device* device,ID3D11DeviceContext* context,PFN_D3DCOMPILE compile,std::string adapter)
        :compile_(compile),adapter_(std::move(adapter)){
        device_.p=device;context_.p=context;
        vs_raster_=vertex_shader("vs_raster");ps_raster_=pixel_shader("ps_raster");
        vs_mask_=vertex_shader("vs_mask");ps_mask_=pixel_shader("ps_mask");
        vs_full_=vertex_shader("vs_full");ps_shadow_=pixel_shader("ps_shadow");
        {const auto blob=compile_shader("cs_resolve","cs_5_0");
         require(device_->CreateComputeShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,cs_resolve_.put()),"compute shader");}
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=FALSE;
        require(device_->CreateRasterizerState(&rd,raster_state_.put()),"rasterizer state");
        for(int linear=0;linear<2;++linear)for(int cu=0;cu<2;++cu)for(int cv=0;cv<2;++cv){
            D3D11_SAMPLER_DESC sd{};
            sd.Filter=linear?D3D11_FILTER_MIN_MAG_MIP_LINEAR:D3D11_FILTER_ANISOTROPIC;sd.MaxAnisotropy=linear?1u:4u;
            sd.AddressU=cu?D3D11_TEXTURE_ADDRESS_CLAMP:D3D11_TEXTURE_ADDRESS_WRAP;
            sd.AddressV=cv?D3D11_TEXTURE_ADDRESS_CLAMP:D3D11_TEXTURE_ADDRESS_WRAP;
            sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sd.MaxLOD=D3D11_FLOAT32_MAX;
            require(device_->CreateSamplerState(&sd,samplers_[linear][cu][cv].put()),"sampler");}
        frame_cb_=constant_buffer(sizeof(FrameCB));composite_cb_=constant_buffer(sizeof(CompositeCB));resolve_cb_=constant_buffer(sizeof(ResolveCB));
        {// 1x1-Platzhalter fuer untexturierte Zeichnungen (der Shader liest ihn nicht).
         const std::uint32_t pixel=0;D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=1;td.MipLevels=td.ArraySize=1;
         td.Format=DXGI_FORMAT_R32_UINT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
         D3D11_SUBRESOURCE_DATA sr{};sr.pSysMem=&pixel;sr.SysMemPitch=4;Com<ID3D11Texture2D> t;
         require(device_->CreateTexture2D(&td,&sr,t.put()),"blank texture");
         require(device_->CreateShaderResourceView(t.get(),nullptr,blank_.put()),"blank view");}
    }
    std::string name()const override{return "Direct3D 11 "+adapter_;}
    const std::vector<std::string>& unsupported()const override{return unsupported_;}
    unsigned draw_calls()const override{return draw_calls_;}
    unsigned shadow_composites()const override{return composites_;}
    const Timings& timings()const override{return timings_;}
    bool render(const raster::Frame& frame,const raster::ShadowGeometry& shadows,std::vector<std::uint32_t>& pixels,std::vector<float>& depth)override{
        using Clock=std::chrono::steady_clock;
        const auto since=[](Clock::time_point from){return std::chrono::duration<double>(Clock::now()-from).count();};
        const auto& setups=*frame.setups;const int S=frame.supersampling,W=frame.width*S,H=frame.height*S;
        targets(W,H,frame.width,frame.height);
        {const auto t=Clock::now();build(frame);timings_.build+=since(t);}
        const auto encode_begin=Clock::now();
        draw_calls_=0;composites_=0;
        upload(setup_buffer_,setup_capacity_,setups_.data(),setups_.size(),sizeof(GpuSetup),setup_view_);
        // Farbe mit der Hintergrundfarbe, Tiefe und transportierte Tiefe mit 0 vorbelegen.
        {const UINT background[4]={frame.background,0,0,0};context_->ClearUnorderedAccessViewUint(color_uav_.get(),background);}
        {const FLOAT zero[4]={0,0,0,0};context_->ClearUnorderedAccessViewFloat(depth_uav_.get(),zero);context_->ClearUnorderedAccessViewFloat(transported_uav_.get(),zero);}
        const FrameCB fu{float(W),float(H),frame.fog_pixel,frame.wireframe?1u:0u,frame.background,std::uint32_t(W),std::uint32_t(H),0};
        write(frame_cb_,&fu,sizeof fu);
        context_->RSSetState(raster_state_.get());
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->IASetInputLayout(nullptr);
        ID3D11Buffer* frame_cbs[]={frame_cb_.get()};
        context_->VSSetConstantBuffers(0,1,frame_cbs);context_->PSSetConstantBuffers(0,1,frame_cbs);
        const std::size_t ends[3]={frame.shadows?frame.before_modifiers:setups.size(),frame.shadows?frame.before_mirror_effects:setups.size(),setups.size()};
        std::size_t batch=0;std::vector<raster::ShadowWork> pending;
        for(unsigned phase=0;phase<3;++phase){
            for(std::size_t i=0;i<pending.size();++i)composite(pending[i],i,W,H);pending.clear();
            bind_targets(W,H);
            context_->VSSetShader(vs_raster_.get(),nullptr,0);context_->PSSetShader(ps_raster_.get(),nullptr,0);
            ID3D11ShaderResourceView* setup_views[]={setup_view_.get()};
            context_->VSSetShaderResources(0,1,setup_views);context_->PSSetShaderResources(0,1,setup_views);
            for(;batch<batches_.size()&&batches_[batch].first<ends[phase];++batch){
                const auto& b=batches_[batch];
                ID3D11ShaderResourceView* texture[]={b.point?nullptr:b.texture,b.point?b.texture:nullptr};
                context_->PSSetShaderResources(2,2,texture);
                ID3D11SamplerState* sampler[]={samplers_[b.sampler>>2][(b.sampler>>1)&1][b.sampler&1].get()};
                context_->PSSetSamplers(0,1,sampler);
                context_->Draw(UINT(b.count*6),UINT(b.first*6));++draw_calls_;}
            if(phase<2&&frame.shadows&&frame.shadow_pass[phase]&&shadows){
                // Die Masken zeichnet die GPU aus derselben transportierten Tiefe; D3D11 synchronisiert
                // UAV-Zugriffe zwischen Zeichenaufrufen selbst, deshalb kein Warten und kein Ruecklesen.
                const auto shadow_begin=Clock::now();pending=shadows(phase==1);timings_.encode-=0;
                masks(pending,W,H);timings_.build+=since(shadow_begin);}
        }
        for(std::size_t i=0;i<pending.size();++i)composite(pending[i],i,W,H);
        resolve(frame.width,frame.height,S);
        timings_.encode+=since(encode_begin);
        {const auto t=Clock::now();
         context_->CopyResource(stage_color_.get(),out_color_.get());context_->CopyResource(stage_depth_.get(),out_depth_.get());
         const std::size_t n=std::size_t(frame.width)*frame.height;pixels.resize(n);depth.resize(n);
         D3D11_MAPPED_SUBRESOURCE mapped{};
         require(context_->Map(stage_color_.get(),0,D3D11_MAP_READ,0,&mapped),"map colour");
         std::memcpy(pixels.data(),mapped.pData,n*4);context_->Unmap(stage_color_.get(),0);
         require(context_->Map(stage_depth_.get(),0,D3D11_MAP_READ,0,&mapped),"map depth");
         std::memcpy(depth.data(),mapped.pData,n*4);context_->Unmap(stage_depth_.get(),0);
         timings_.wait+=since(t);++timings_.submits;}
        return true;
    }
private:
    Com<ID3DBlob> compile_shader(const char* entry,const char* target){
        Com<ID3DBlob> code,errors;
        // IEEE-Arithmetik wie die Bandstufe (-ffp-contract=off): keine zusammengefassten Operationen.
        const UINT flags=D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS;
        const HRESULT hr=compile_(shader_source,std::strlen(shader_source),"recvx",nullptr,nullptr,entry,target,flags,0,code.put(),errors.put());
        if(FAILED(hr))throw std::runtime_error(std::string("D3D11 shader ")+entry+": "+(errors?static_cast<const char*>(errors->GetBufferPointer()):"compile failed"));
        return code;
    }
    Com<ID3D11VertexShader> vertex_shader(const char* entry){
        const auto blob=compile_shader(entry,"vs_5_0");Com<ID3D11VertexShader> shader;
        require(device_->CreateVertexShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,shader.put()),"vertex shader");return shader;}
    Com<ID3D11PixelShader> pixel_shader(const char* entry){
        const auto blob=compile_shader(entry,"ps_5_0");Com<ID3D11PixelShader> shader;
        require(device_->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,shader.put()),"pixel shader");return shader;}
    Com<ID3D11Buffer> constant_buffer(std::size_t bytes){
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT((bytes+15)&~std::size_t(15));bd.Usage=D3D11_USAGE_DYNAMIC;
        bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        Com<ID3D11Buffer> buffer;require(device_->CreateBuffer(&bd,nullptr,buffer.put()),"constant buffer");return buffer;}
    void write(const Com<ID3D11Buffer>& buffer,const void* data,std::size_t bytes){
        D3D11_MAPPED_SUBRESOURCE mapped{};
        require(context_->Map(buffer.get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped),"map constant buffer");
        std::memcpy(mapped.pData,data,bytes);context_->Unmap(buffer.get(),0);}
    // Strukturpuffer mit Wachstum; der Inhalt wird je Bild neu geschrieben.
    void upload(Com<ID3D11Buffer>& buffer,std::size_t& capacity,const void* data,std::size_t count,std::size_t stride,Com<ID3D11ShaderResourceView>& view){
        if(!count)return;
        if(!buffer||capacity<count){
            capacity=std::max<std::size_t>(count*2,256);
            D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(capacity*stride);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;bd.StructureByteStride=UINT(stride);
            require(device_->CreateBuffer(&bd,nullptr,buffer.put()),"structured buffer");
            D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_UNKNOWN;sd.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;
            sd.Buffer.FirstElement=0;sd.Buffer.NumElements=UINT(capacity);
            require(device_->CreateShaderResourceView(buffer.get(),&sd,view.put()),"structured view");}
        D3D11_MAPPED_SUBRESOURCE mapped{};
        require(context_->Map(buffer.get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped),"map structured buffer");
        std::memcpy(mapped.pData,data,count*stride);context_->Unmap(buffer.get(),0);}
    void targets(int W,int H,int base_w,int base_h){
        if(color_&&target_w_==W&&target_h_==H)return;
        target_w_=W;target_h_=H;
        const auto surface=[&](DXGI_FORMAT format,int w,int h,Com<ID3D11Texture2D>& texture,Com<ID3D11UnorderedAccessView>& uav){
            D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=td.ArraySize=1;td.Format=format;
            td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE;
            require(device_->CreateTexture2D(&td,nullptr,texture.put()),"render target");
            require(device_->CreateUnorderedAccessView(texture.get(),nullptr,uav.put()),"render target view");};
        surface(DXGI_FORMAT_R32_UINT,W,H,color_,color_uav_);
        surface(DXGI_FORMAT_R32_FLOAT,W,H,depth_,depth_uav_);
        surface(DXGI_FORMAT_R32_FLOAT,W,H,transported_,transported_uav_);
        const std::size_t n=std::size_t(base_w)*base_h;
        const auto out_buffer=[&](DXGI_FORMAT format,Com<ID3D11Buffer>& buffer,Com<ID3D11UnorderedAccessView>& uav,Com<ID3D11Buffer>& stage){
            D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(n*4);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
            require(device_->CreateBuffer(&bd,nullptr,buffer.put()),"output buffer");
            D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};ud.Format=format;ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;ud.Buffer.NumElements=UINT(n);
            require(device_->CreateUnorderedAccessView(buffer.get(),&ud,uav.put()),"output view");
            D3D11_BUFFER_DESC sd{};sd.ByteWidth=UINT(n*4);sd.Usage=D3D11_USAGE_STAGING;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            require(device_->CreateBuffer(&sd,nullptr,stage.put()),"staging buffer");};
        out_buffer(DXGI_FORMAT_R32_UINT,out_color_,out_color_uav_,stage_color_);
        out_buffer(DXGI_FORMAT_R32_FLOAT,out_depth_,out_depth_uav_,stage_depth_);
    }
    void bind_targets(int W,int H){
        ID3D11UnorderedAccessView* uavs[]={color_uav_.get(),depth_uav_.get(),transported_uav_.get(),mask_uav_.get()};
        context_->OMSetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,4,uavs,nullptr);
        D3D11_VIEWPORT vp{};vp.Width=float(W);vp.Height=float(H);vp.MaxDepth=1.f;context_->RSSetViewports(1,&vp);}
    ID3D11ShaderResourceView* texture(const SceneTexture& t,Variant variant){
        auto& cache=*t.filter_cache;const auto fingerprint=t.fingerprint();auto& slot=cache.gpu[variant];
        if(slot&&cache.gpu_owner[variant]==cache_owner()&&cache.gpu_fingerprint[variant]==fingerprint)return static_cast<ID3D11ShaderResourceView*>(slot.get());
        const auto upload_begin=std::chrono::steady_clock::now();++timings_.uploads;
        const std::size_t levels=variant==Point?1:1+(variant==Punch?cache.punch_mips.size():cache.mips.size());
        D3D11_TEXTURE2D_DESC td{};td.Width=t.width;td.Height=t.height;td.MipLevels=UINT(levels);td.ArraySize=1;
        td.Format=variant==Point?DXGI_FORMAT_R32_UINT:variant==Premultiplied?DXGI_FORMAT_R16G16B16A16_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        std::vector<D3D11_SUBRESOURCE_DATA> data(levels);std::vector<std::vector<std::uint16_t>> wide(levels);
        unsigned w=t.width,h=t.height;
        for(std::size_t level=0;level<levels;++level){
            const std::uint32_t* src=level==0?t.pixels.data():(variant==Punch?cache.punch_mips[level-1].data():cache.mips[level-1].data());
            if(variant==Premultiplied){wide[level].resize(std::size_t(w)*h*4);
                for(std::size_t i=0;i<std::size_t(w)*h;++i){const auto p=src[i];const double a=double(p>>24)/255.0;
                    wide[level][i*4]=std::uint16_t(double((p>>16)&255)/255.0*a*65535.0+.5);wide[level][i*4+1]=std::uint16_t(double((p>>8)&255)/255.0*a*65535.0+.5);
                    wide[level][i*4+2]=std::uint16_t(double(p&255)/255.0*a*65535.0+.5);wide[level][i*4+3]=std::uint16_t(a*65535.0+.5);}
                data[level].pSysMem=wide[level].data();data[level].SysMemPitch=UINT(w*8);}
            else{data[level].pSysMem=src;data[level].SysMemPitch=UINT(w*4);}
            w=std::max(1u,w/2);h=std::max(1u,h/2);}
        Com<ID3D11Texture2D> texture2d;require(device_->CreateTexture2D(&td,data.data(),texture2d.put()),"texture");
        ID3D11ShaderResourceView* view=nullptr;
        require(device_->CreateShaderResourceView(texture2d.get(),nullptr,&view),"texture view");
        slot=std::shared_ptr<void>(view,[](void* p){static_cast<ID3D11ShaderResourceView*>(p)->Release();});
        cache.gpu_fingerprint[variant]=fingerprint;cache.gpu_owner[variant]=cache_owner();
        timings_.upload+=std::chrono::duration<double>(std::chrono::steady_clock::now()-upload_begin).count();
        return view;
    }
    void build(const raster::Frame& frame){
        const auto& setups=*frame.setups;setups_.resize(setups.size());batches_.clear();
        for(std::size_t i=0;i<setups.size();++i){const auto& s=setups[i];auto& g=setups_[i];
            std::uint32_t flags=0;unsigned max_alpha=255;Variant variant=Point;
            if(s.gouraud)flags|=1;if(s.vertex_alpha)flags|=2;if(s.fogged)flags|=32;if(s.fog_color_set)flags|=64;if(s.clamp_u)flags|=128;if(s.clamp_v)flags|=256;
            if(s.punch_through)flags|=16;if(s.linear_only)flags|=2048;if(s.blend!=SceneBlend::Opaque)flags|=4096;if(s.general_water)flags|=16384;if(s.depth_write)flags|=65536;
            if(s.texture){flags|=4;
                if(s.filtered){flags|=8;const auto& c=*s.texture->filter_cache;max_alpha=c.max_alpha;if(c.cutout)flags|=512;if(c.uniform_alpha)flags|=1024;
                    variant=s.punch_through?Punch:c.uniform_alpha?Straight:Premultiplied;if(variant==Premultiplied)flags|=32768;}}
            const auto point=[](float* out,const raster::Projected& p){out[0]=p.x;out[1]=p.y;out[2]=p.invz;out[3]=p.tz;};
            point(g.p0,s.p[0]);point(g.p1,s.p[1]);point(g.p2,s.p[2]);
            g.uv0[0]=s.p[0].u_over_z;g.uv0[1]=s.p[0].v_over_z;g.uv0[2]=s.p[1].u_over_z;g.uv0[3]=s.p[1].v_over_z;
            g.uv1[0]=s.p[2].u_over_z;g.uv1[1]=s.p[2].v_over_z;g.uv1[2]=s.area;g.uv1[3]=s.Wx;
            g.grad[0]=s.Ux;g.grad[1]=s.Uy;g.grad[2]=s.Vx;g.grad[3]=s.Vy;
            float* lights[3]={g.l0,g.l1,g.l2};for(int k=0;k<3;++k){lights[k][0]=s.light[k].x;lights[k][1]=s.light[k].y;lights[k][2]=s.light[k].z;lights[k][3]=s.alpha[k];}
            g.fog[0]=s.fog[0];g.fog[1]=s.fog[1];g.fog[2]=s.fog[2];g.fog[3]=s.Wy;
            g.info[0]=s.color;g.info[1]=flags;g.info[2]=s.fog_color;g.info[3]=(s.texture_color_divisor&0xffffu)|(max_alpha<<16);
            const std::uint64_t reg=s.blend==SceneBlend::Ninja?s.blend_register:0x44;
            g.blend[0]=std::uint32_t(reg);g.blend[1]=std::uint32_t(reg>>32);g.blend[2]=s.blend==SceneBlend::Opaque?0u:s.blend==SceneBlend::Additive?1u:2u;g.blend[3]=0;
            g.box[0]=s.minx;g.box[1]=s.miny;g.box[2]=s.maxx;g.box[3]=s.maxy;
            ID3D11ShaderResourceView* view=s.texture?this->texture(*s.texture,variant):blank_.get();
            const unsigned sampler=unsigned(s.linear_only?4:0)|unsigned(s.clamp_u?2:0)|unsigned(s.clamp_v?1:0);
            const bool point_sampled=!s.texture||variant==Point;
            // Ein Batch darf keine Phasengrenze ueberspannen (siehe gpu_raster_metal.mm build()).
            const bool phase_start=frame.shadows&&(i==frame.before_modifiers||i==frame.before_mirror_effects);
            if(!phase_start&&!batches_.empty()&&batches_.back().texture==view&&batches_.back().sampler==sampler&&batches_.back().point==point_sampled
               &&batches_.back().first+batches_.back().count==i){++batches_.back().count;continue;}
            batches_.push_back({i,1,view,sampler,point_sampled});
        }
    }
    // Alle Masken dieser Phase (eine Ebene je Werk): Dreiecksreihenfolge je Ebene wie raster_shadow_mask.
    void masks(const std::vector<raster::ShadowWork>& works,int W,int H){
        if(works.empty())return;
        if(works.size()>2048)throw std::runtime_error("too many shadow works in one pass");
        triangles_.clear();
        for(std::size_t layer=0;layer<works.size();++layer)for(const auto& f:works[layer].faces){
            GpuShadowTri t{};
            t.v01[0]=f.x[0];t.v01[1]=f.y[0];t.v01[2]=f.x[1];t.v01[3]=f.y[1];
            t.v2a[0]=f.x[2];t.v2a[1]=f.y[2];t.v2a[2]=f.area;t.v2a[3]=0;
            t.depth[0]=f.depth[0];t.depth[1]=f.depth[1];t.depth[2]=f.depth[2];
            t.depth[3]=(std::uint32_t(layer)<<1)|(f.subtract?1u:0u);
            t.box[0]=f.minx;t.box[1]=f.miny;t.box[2]=f.maxx;t.box[3]=f.maxy;
            triangles_.push_back(t);}
        mask_target(unsigned(works.size()));
        {const UINT zero[4]={0,0,0,0};context_->ClearUnorderedAccessViewUint(mask_uav_.get(),zero);}
        if(triangles_.empty())return;
        upload(shadow_buffer_,shadow_capacity_,triangles_.data(),triangles_.size(),sizeof(GpuShadowTri),shadow_view_);
        bind_targets(640,480);
        context_->VSSetShader(vs_mask_.get(),nullptr,0);context_->PSSetShader(ps_mask_.get(),nullptr,0);
        ID3D11ShaderResourceView* views[]={shadow_view_.get()};
        context_->VSSetShaderResources(1,1,views);context_->PSSetShaderResources(1,1,views);
        context_->Draw(UINT(triangles_.size()*6),0);++draw_calls_;
        bind_targets(W,H);
    }
    void mask_target(unsigned layers){
        if(mask_&&mask_layers_>=layers)return;
        mask_layers_=layers;
        D3D11_TEXTURE2D_DESC td{};td.Width=640;td.Height=480;td.MipLevels=1;td.ArraySize=layers;td.Format=DXGI_FORMAT_R8_UINT;
        td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE;
        require(device_->CreateTexture2D(&td,nullptr,mask_.put()),"mask texture");
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};ud.Format=DXGI_FORMAT_R8_UINT;ud.ViewDimension=D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
        ud.Texture2DArray.ArraySize=layers;require(device_->CreateUnorderedAccessView(mask_.get(),&ud,mask_uav_.put()),"mask view");}
    void composite(const raster::ShadowWork& c,std::size_t index,int W,int H){
        if(c.fog_power>255u)throw std::runtime_error("shadow: invalid fog channel");
        const CompositeCB u{float(W),float(H),c.rx0,c.ry0,c.rx1,c.ry1,c.fog_power,std::uint32_t(index)};
        write(composite_cb_,&u,sizeof u);
        ID3D11Buffer* buffers[]={composite_cb_.get()};context_->PSSetConstantBuffers(1,1,buffers);
        bind_targets(W,H);
        context_->VSSetShader(vs_full_.get(),nullptr,0);context_->PSSetShader(ps_shadow_.get(),nullptr,0);
        context_->Draw(3,0);++draw_calls_;++composites_;}
    void resolve(int width,int height,int S){
        ID3D11UnorderedAccessView* none[]={nullptr,nullptr,nullptr,nullptr};
        context_->OMSetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,4,none,nullptr);
        const ResolveCB rc{{std::uint32_t(width),std::uint32_t(height),std::uint32_t(S),0}};
        write(resolve_cb_,&rc,sizeof rc);
        ID3D11Buffer* buffers[]={resolve_cb_.get()};context_->CSSetConstantBuffers(2,1,buffers);
        ID3D11UnorderedAccessView* uavs[]={color_uav_.get(),depth_uav_.get(),nullptr,nullptr,out_color_uav_.get(),out_depth_uav_.get()};
        context_->CSSetUnorderedAccessViews(0,6,uavs,nullptr);
        context_->CSSetShader(cs_resolve_.get(),nullptr,0);
        context_->Dispatch(UINT((width+15)/16),UINT((height+15)/16),1);
        ID3D11UnorderedAccessView* clear[]={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
        context_->CSSetUnorderedAccessViews(0,6,clear,nullptr);}
    Com<ID3D11Device> device_;Com<ID3D11DeviceContext> context_;PFN_D3DCOMPILE compile_;std::string adapter_;
    Com<ID3D11VertexShader> vs_raster_,vs_mask_,vs_full_;Com<ID3D11PixelShader> ps_raster_,ps_mask_,ps_shadow_;
    Com<ID3D11ComputeShader> cs_resolve_;Com<ID3D11RasterizerState> raster_state_;Com<ID3D11SamplerState> samplers_[2][2][2];
    Com<ID3D11Texture2D> color_,depth_,transported_,mask_;
    Com<ID3D11UnorderedAccessView> color_uav_,depth_uav_,transported_uav_,mask_uav_,out_color_uav_,out_depth_uav_;
    Com<ID3D11Buffer> out_color_,out_depth_,stage_color_,stage_depth_,setup_buffer_,shadow_buffer_,frame_cb_,composite_cb_,resolve_cb_;
    Com<ID3D11ShaderResourceView> setup_view_,shadow_view_,blank_;
    std::vector<GpuSetup> setups_;std::vector<GpuShadowTri> triangles_;std::vector<Batch> batches_;
    std::vector<std::string> unsupported_;Timings timings_;
    std::size_t setup_capacity_=0,shadow_capacity_=0;unsigned draw_calls_=0,composites_=0,mask_layers_=0;int target_w_=0,target_h_=0;
};
}
std::shared_ptr<GpuRaster> create_gpu_raster(std::string& reason){
    const HMODULE d3d11=LoadLibraryA("d3d11.dll");
    if(!d3d11){reason="d3d11.dll not available";return nullptr;}
    const HMODULE compiler=LoadLibraryA("d3dcompiler_47.dll");
    if(!compiler){reason="d3dcompiler_47.dll not available";return nullptr;}
    const auto create=reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(reinterpret_cast<void*>(GetProcAddress(d3d11,"D3D11CreateDevice")));
    const auto compile=reinterpret_cast<PFN_D3DCOMPILE>(reinterpret_cast<void*>(GetProcAddress(compiler,"D3DCompile")));
    if(!create||!compile){reason="Direct3D 11 entry points missing";return nullptr;}
    const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
    ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;D3D_FEATURE_LEVEL level{};
    // RECVX_D3D11_DRIVER=warp: Microsofts Software-Referenz (in Windows enthalten, mit ROV) statt des
    // Grafiktreibers. Trennt Fehler des Backends von Fehlern eines Treibers (z.B. VM-Uebersetzungsschichten).
    char driver_name[16]{};
    const bool warp=GetEnvironmentVariableA("RECVX_D3D11_DRIVER",driver_name,sizeof driver_name)>0&&std::string(driver_name)=="warp";
    if(FAILED(create(nullptr,warp?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,2,D3D11_SDK_VERSION,&device,&level,&context))){
        reason=warp?"no Direct3D 11 WARP device":"no Direct3D 11 hardware device";return nullptr;}
    std::string adapter=warp?"WARP":"device";
    {ID3D11Device3* device3=nullptr;
     // Ohne Rasterizer Ordered Views gibt es keine garantierte Zeichenreihenfolge im Pixelshader und
     // damit keine exakte Abbildung der Bandstufe: dann Softwarepfad statt aehnlichem Mischen.
     if(FAILED(device->QueryInterface(__uuidof(ID3D11Device3),reinterpret_cast<void**>(&device3)))||!device3){
        if(context)context->Release();device->Release();reason="Direct3D 11.3 not available (no rasterizer ordered views)";return nullptr;}
     D3D11_FEATURE_DATA_D3D11_OPTIONS2 options{};
     const HRESULT hr=device3->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2,&options,sizeof options);
     device3->Release();
     if(FAILED(hr)||!options.ROVsSupported){
        if(context)context->Release();device->Release();reason="GPU without rasterizer ordered views";return nullptr;}}
    {IDXGIDevice* dxgi=nullptr;
     if(SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(&dxgi)))&&dxgi){
        IDXGIAdapter* a=nullptr;
        if(SUCCEEDED(dxgi->GetAdapter(&a))&&a){DXGI_ADAPTER_DESC desc{};
            if(SUCCEEDED(a->GetDesc(&desc))){char text[128]{};
                WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,text,sizeof text-1,nullptr,nullptr);adapter=text;}
            a->Release();}
        dxgi->Release();}}
    try{return std::make_shared<D3D11Raster>(device,context,compile,adapter);}
    catch(const std::exception& e){reason=e.what();return nullptr;}
}
}
