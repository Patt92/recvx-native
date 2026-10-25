// Metal-Backend des Host-GPU-Rasterers (gpu_raster.h), nur macOS. Plattformanpassung, keine
// Spiellogik: es rastert die raster::Setup-Liste des SoftwareRenderers mit dem Koerper der
// Pixelschleife der Bandstufe (renderer.cpp, rasterize) und liefert Bild und Tiefe zurueck.
//
// - Programmierbares Mischen (Framebuffer-Fetch, Apple-GPU): jedes Pixel liest Farbe (0xAARRGGBB),
//   1/z und transportierte Tiefe seines Vorgaengers in Einreichreihenfolge und schreibt sie wie die
//   Software. Abdeckung (Kantenfunktionen, halboffene Regel), 1/z, Attribute, Nebel, GS ALPHA
//   (blend_gs, alle Register), Abdeckungsmischung und Tiefenschwellen sind dieselben Rechnungen;
//   IEEE-Arithmetik (kein Fast-Math, keine FMA-Kontraktion; renderer.cpp ebenso ohne Kontraktion).
// - Jedes Setup wird als Rechteck seiner Pixelgrenzen gezeichnet; der Shader entscheidet die Pixel.
// - Gefilterte Host-Qualitaetsstufen tasten mit dem Hardwarefilter ab (trilinear, 4x anisotrop) und
//   den analytischen Ableitungen der Bandstufe; deren Vierpixel-Ableitungsfolge ist reihenfolge-
//   abhaengig und wird nicht nachgebildet. Punktabtastung liest den Texel wie die Software.
// - Schattenmodifier: transportierte Tiefe nach der Phase zurueckgelesen; Masken rechnet der
//   Renderer (shadow_raster.h), das Komposit (shadow_mask_composite) der Shader.
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "gpu_raster.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <stdexcept>
namespace recvx {
namespace {
const char* shader_source=R"MSL(
#include <metal_stdlib>
#pragma METAL fp contract(off)
using namespace metal;
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
struct FrameU { float W,H; uint fog_pixel,wireframe; uint background,pad0,pad1,pad2; };
struct VOut { float4 pos [[position]]; uint setup [[flat]]; };
struct FOut { uint pixel [[color(0)]]; float depth [[color(1)]]; float transported [[color(2)]]; };
// Flags: 1 gouraud, 2 vertex_alpha, 4 textured, 8 filtered, 16 punch, 32 fogged, 64 fog_color_set,
// 128 clamp_u, 256 clamp_v, 512 cutout, 1024 uniform_alpha, 2048 linear_only, 4096 nonopaque,
// 16384 general_water, 32768 premultiplied chain, 65536 depth_write.
vertex VOut vs(uint vid [[vertex_id]], const device Setup* setups [[buffer(0)]], constant FrameU& f [[buffer(1)]]) {
    const uint index=vid/6u,corner=vid%6u; const int4 b=setups[index].box;
    const bool right=corner==1u||corner==2u||corner==4u, bottom=corner==2u||corner==4u||corner==5u;
    const float x=float(right?b.z+1:b.x),y=float(bottom?b.w+1:b.y);
    VOut o; o.pos=float4(x/f.W*2.0-1.0,1.0-y/f.H*2.0,0.0,1.0); o.setup=index; return o;
}
inline uint ch(uint c,uint s){return (c>>s)&255u;}
inline float edge(float2 a,float2 b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
inline bool inclusive(float2 from,float2 to){return to.y<from.y||(to.y==from.y&&to.x>from.x);}
inline uint lit(uint base,float r,float g,float b){
    return (base&0xff000000u)|(uint(clamp(float(ch(base,16))*r+0.5f,0.0f,255.0f))<<16)|(uint(clamp(float(ch(base,8))*g+0.5f,0.0f,255.0f))<<8)|uint(clamp(float(ch(base,0))*b+0.5f,0.0f,255.0f));}
inline uint modulate(uint t,uint l,uint d){
    return 0xff000000u|(min(255u,(ch(t,16)*ch(l,16))/d)<<16)|(min(255u,(ch(t,8)*ch(l,8))/d)<<8)|min(255u,(ch(t,0)*ch(l,0))/d);}
inline uint pack(float4 t){return (uint(rint(t.a*255.0f))<<24)|(uint(rint(t.r*255.0f))<<16)|(uint(rint(t.g*255.0f))<<8)|uint(rint(t.b*255.0f));}
// chunk_blend.cpp blend_gs: (A - B) * C + D je Kanal.
inline uint blend_gs(uint lo,uint hi,uint source,uint destination,float source_alpha){
    const uint a=lo&3u,b=(lo>>2)&3u,c=(lo>>4)&3u,d=(lo>>6)&3u;
    const float alpha=clamp(source_alpha,0.0f,1.0f);
    const float factor=c==0u?alpha:c==1u?float(destination>>24)/255.0f:float(hi&255u)/128.0f;
    uint result=uint(alpha*255.0f+0.5f)<<24;
    for(uint shift=0u;shift<24u;shift+=8u){
        const float ca=a==0u?float(ch(source,shift)):a==1u?float(ch(destination,shift)):0.0f;
        const float cb=b==0u?float(ch(source,shift)):b==1u?float(ch(destination,shift)):0.0f;
        const float cd=d==0u?float(ch(source,shift)):d==1u?float(ch(destination,shift)):0.0f;
        result|=uint(clamp((ca-cb)*factor+cd+0.5f,0.0f,255.0f))<<shift;}
    return result;
}
fragment FOut fs(VOut in [[stage_in]], uint below [[color(0)]], float below_depth [[color(1)]], float below_transported [[color(2)]],
                 const device Setup* setups [[buffer(0)]], constant FrameU& f [[buffer(1)]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {
    const Setup s=setups[in.setup];
    FOut o; o.pixel=below; o.depth=below_depth; o.transported=below_transported;
    const float X=in.pos.x,Y=in.pos.y; // Pixelmitte x + 0.5, y + 0.5
    const float2 p0=s.p0.xy,p1=s.p1.xy,p2=s.p2.xy; const float area=s.uv1.z;
    const float a=edge(p1,p2,X,Y)/area,b=edge(p2,p0,X,Y)/area,c=edge(p0,p1,X,Y)/area;
    if(a<0.0f||b<0.0f||c<0.0f)discard_fragment();
    if((a==0.0f&&!inclusive(p1,p2))||(b==0.0f&&!inclusive(p2,p0))||(c==0.0f&&!inclusive(p0,p1)))discard_fragment();
    const float invz=s.p0.z+b*(s.p1.z-s.p0.z)+c*(s.p2.z-s.p0.z);
    if(invz<below_depth)discard_fragment(); // GEQUAL
    const uint color=s.info.x,flags=s.info.y,divisor=s.info.w&0xffffu; const float max_alpha=float(s.info.w>>16);
    const float material_alpha=float(color>>24)/255.0f*((flags&2u)?clamp(a*s.l0.w+b*s.l1.w+c*s.l2.w,0.0f,1.0f):1.0f);
    if(material_alpha<=0.0f)discard_fragment(); // TEST GREATER 0 / KEEP
    const uint kind=s.blend.z;
    uint pixel=color; float coverage=1.0f;
    if(flags&1u)pixel=lit(color,a*s.l0.x+b*s.l1.x+c*s.l2.x,a*s.l0.y+b*s.l1.y+c*s.l2.y,a*s.l0.z+b*s.l1.z+c*s.l2.z);
    if(flags&4u){
        const float rz=1.0f/invz;
        float u=(a*s.uv0.x+b*s.uv0.z+c*s.uv1.x)*rz;
        float v=(a*s.uv0.y+b*s.uv0.w+c*s.uv1.y)*rz;
        if(!(flags&8u)){
            if(flags&128u)u=clamp(u,0.0f,1.0f);else u-=floor(u);
            if(flags&256u)v=clamp(v,0.0f,1.0f);else v-=floor(v);
            const uint w=tex.get_width(),h=tex.get_height();
            const uint texel=pack(tex.read(uint2(min(w-1u,uint(u*float(w))),min(h-1u,uint(v*float(h))))));
            if((texel>>24)==0u)discard_fragment();
            pixel=modulate(texel,pixel,divisor);
            if(kind!=0u)coverage=float(texel>>24)/255.0f;
        } else {
            // Ableitungen von (u,v) je Bildpixel wie die Bandstufe: d(U/W) = (dU - u*dW)/W.
            const float Wx=s.uv1.w,Wy=s.fog.w;
            const float2 ddx=float2((s.grad.x-u*Wx)*rz,(s.grad.z-v*Wx)*rz),ddy=float2((s.grad.y-u*Wy)*rz,(s.grad.w-v*Wy)*rz);
            if(!(flags&128u))u-=floor(u);
            if(!(flags&256u))v-=floor(v);
            const float4 t=(flags&2048u)?tex.sample(smp,float2(u,v),level(0.0f)):tex.sample(smp,float2(u,v),gradient2d(ddx,ddy));
            if(t.a<=0.0f)discard_fragment();
            if(kind!=0u)coverage=min(1.0f,(flags&1024u)?max_alpha/255.0f:t.a);
            else if((flags&512u)&&!(flags&16u)){coverage=min(1.0f,t.a*255.0f/max_alpha);if(coverage<1.0f/32.0f)discard_fragment();}
            const float3 rgb=(flags&32768u)?t.rgb/t.a:t.rgb;
            const uint texel=0xff000000u|(uint(clamp(rgb.r*255.0f+0.5f,0.0f,255.0f))<<16)|(uint(clamp(rgb.g*255.0f+0.5f,0.0f,255.0f))<<8)|uint(clamp(rgb.b*255.0f+0.5f,0.0f,255.0f));
            pixel=modulate(texel,pixel,divisor);
        }
    }
    if(flags&32u){ // GS: Cv = (Cs*F + Cf*(255-F)) >> 8
        const uint F=uint(clamp(a*s.fog.x+b*s.fog.y+c*s.fog.z,0.0f,255.0f));const uint fog=(flags&64u)?s.info.z:f.fog_pixel;
        pixel=(pixel&0xff000000u)|(((ch(pixel,16)*F+ch(fog,16)*(255u-F))>>8)<<16)|(((ch(pixel,8)*F+ch(fog,8)*(255u-F))>>8)<<8)|((ch(pixel,0)*F+ch(fog,0)*(255u-F))>>8);
    }
    const bool depth_write=(flags&65536u)!=0u;
    if(kind==1u){ // additiv, ohne Tiefe
        const float alpha=coverage*material_alpha;uint mixed=0xff000000u;
        for(uint shift=0u;shift<24u;shift+=8u)mixed|=min(255u,ch(below,shift)+uint(float(ch(pixel,shift))*alpha+0.5f))<<shift;
        o.pixel=mixed;return o;}
    if(kind==2u){ // GS ALPHA; Tiefe ohne transportierte Tiefe (Ps2ZbuffOff alpha >= 128 / direktes Wasser)
        const float alpha=coverage*material_alpha;
        o.pixel=blend_gs(s.blend.x,s.blend.y,pixel,below,alpha);
        if(depth_write&&((flags&16384u)?alpha>0.0f:alpha>=0.5f))o.depth=invz;
        return o;}
    if(coverage<0.999f){
        uint mixed=0xff000000u;
        for(uint shift=0u;shift<24u;shift+=8u)mixed|=uint(float(ch(below,shift))*(1.0f-coverage)+float(ch(pixel,shift))*coverage+0.5f)<<shift;
        pixel=mixed;if(coverage<0.5f){o.pixel=pixel;return o;}
    }
    if(depth_write){o.depth=invz;o.transported=a*s.p0.w+b*s.p1.w+c*s.p2.w;}
    o.pixel=(f.wireframe!=0u&&min(min(a,b),c)>0.025f)?f.background:pixel;
    return o;
}
// Maskenrasterung von bhDrawMdfEffect (shadow_raster.h) im 640x480-Bild: ein Dreieck je
// Rechteck seiner Pixelgrenzen, saettigende Akkumulation je Fragment ueber Framebuffer-Fetch,
// Tiefenvergleich GEQUAL gegen die transportierte Empfaengertiefe (shadow_depth_pass).
struct ShadowTri { float4 v01; float4 v2a; uint4 depth; int4 box; };
struct MaskU { uint W,H,pad0,pad1; };
struct MaskVOut { float4 pos [[position]]; uint layer [[render_target_array_index]]; uint tri [[flat]]; };
vertex MaskVOut vs_mask(uint vid [[vertex_id]], const device ShadowTri* tris [[buffer(0)]]) {
    const uint index=vid/6u,corner=vid%6u; const int4 b=tris[index].box;
    const bool right=corner==1u||corner==2u||corner==4u, bottom=corner==2u||corner==4u||corner==5u;
    const float x=float(right?b.z+1:b.x),y=float(bottom?b.w+1:b.y);
    MaskVOut o; o.pos=float4(x/640.0f*2.0f-1.0f,1.0f-y/480.0f*2.0f,0.0f,1.0f);
    o.layer=tris[index].depth.w>>1u; o.tri=index; return o;
}
fragment uint fs_mask(MaskVOut in [[stage_in]], uint below [[color(0)]],
                      const device ShadowTri* tris [[buffer(0)]], constant MaskU& m [[buffer(1)]],
                      texture2d<float> transported [[texture(0)]]) {
    const ShadowTri t=tris[in.tri];
    const float2 p0=t.v01.xy,p1=t.v01.zw,p2=t.v2a.xy; const float X=in.pos.x,Y=in.pos.y;
    const float a=edge(p1,p2,X,Y),b=edge(p2,p0,X,Y),c=edge(p0,p1,X,Y);
    if(a<0.0f||b<0.0f||c<0.0f)discard_fragment();
    if((a==0.0f&&!inclusive(p1,p2))||(b==0.0f&&!inclusive(p2,p0))||(c==0.0f&&!inclusive(p0,p1)))discard_fragment();
    const float depth=(a*float(t.depth.x)+b*float(t.depth.y)+c*float(t.depth.z))/t.v2a.z;
    const uint sx=min(m.W-1u,uint(X*float(m.W)/640.0f)),sy=min(m.H-1u,uint(Y*float(m.H)/480.0f));
    if(uint(depth)<uint(transported.read(uint2(sx,sy)).r))discard_fragment(); // GEQUAL
    const int value=int(below)+((t.depth.w&1u)?-32:32); // shadow_mask_fragment
    return uint(clamp(value,0,255))&0xf8u;
}
struct CompositeU { float W,H,rx0,ry0,rx1,ry1; uint fog_power,layer; };
vertex float4 vs_full(uint vid [[vertex_id]]) { const float2 p=float2(float((vid<<1)&2u),float(vid&2u)); return float4(p*2.0f-1.0f,0.0f,1.0f); }
fragment FOut fs_shadow(float4 pos [[position]], uint below [[color(0)]], float below_depth [[color(1)]], float below_transported [[color(2)]],
                        constant CompositeU& c [[buffer(0)]], texture2d_array<uint> mask [[texture(0)]]) {
    FOut o; o.pixel=below; o.depth=below_depth; o.transported=below_transported;
    const float y=pos.y-0.5f,x=pos.x-0.5f;
    const int my=min(479,int((y+0.5f)*480.0f/c.H)); if(float(my)+0.5f<c.ry0||float(my)+0.5f>=c.ry1)return o;
    const int mx=min(639,int((x+0.5f)*640.0f/c.W)); if(float(mx)+0.5f<c.rx0||float(mx)+0.5f>=c.rx1)return o;
    const uint value=mask.read(uint2(mx,my),c.layer).r; if(value==0u)return o;
    const uint reduction=(value*((c.fog_power*3u)/16u))/128u; // shadow_mask_composite
    uint result=below&0xff000000u;
    for(uint shift=0u;shift<24u;shift+=8u){const uint v=ch(below,shift);result|=(v>reduction?v-reduction:0u)<<shift;}
    o.pixel=result;return o;
}
kernel void resolve(texture2d<uint,access::read> color [[texture(0)]], texture2d<float,access::read> depth [[texture(1)]],
                    device uint* out_color [[buffer(0)]], device float* out_depth [[buffer(1)]], constant uint4& size [[buffer(2)]], uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=size.x||gid.y>=size.y)return;
    const uint S=size.z,at=gid.y*size.x+gid.x;
    if(S==1u){out_color[at]=color.read(gid).r;out_depth[at]=depth.read(gid).r;return;}
    uint r=0u,g=0u,b=0u; float nearest=0.0f; const uint n=S*S;
    for(uint sy=0u;sy<S;++sy)for(uint sx=0u;sx<S;++sx){const uint2 q=gid*S+uint2(sx,sy);const uint c=color.read(q).r;
        r+=c&255u;g+=(c>>8)&255u;b+=(c>>16)&255u;nearest=max(nearest,depth.read(q).r);}
    out_color[at]=0xff000000u|(((b+n/2u)/n)<<16)|(((g+n/2u)/n)<<8)|((r+n/2u)/n);out_depth[at]=nearest;
}
)MSL";
struct alignas(16) GpuSetup{float p0[4],p1[4],p2[4],uv0[4],uv1[4],grad[4],l0[4],l1[4],l2[4],fog[4];std::uint32_t info[4],blend[4];std::int32_t box[4];};
static_assert(sizeof(GpuSetup)==208,"setup layout");
struct FrameU{float W,H;std::uint32_t fog_pixel,wireframe,background,pad[3];};
struct CompositeU{float W,H,rx0,ry0,rx1,ry1;std::uint32_t fog_power,layer;};
struct alignas(16) GpuShadowTri{float v01[4],v2a[4];std::uint32_t depth[4];std::int32_t box[4];};
static_assert(sizeof(GpuShadowTri)==64,"shadow triangle layout");
struct MaskU{std::uint32_t W,H,pad[2];};
// Darstellungsart einer Textur: Stufe 0 (Punktabtastung), gerade Kette, praemultiplizierte Kette
// (alpha-gewichtete Mittelung), Punch-Kette (RGB ungewichtet).
enum Variant{Point=1,Straight=2,Premultiplied=3,Punch=4};
struct Batch{std::size_t first,count;id<MTLTexture> texture;id<MTLSamplerState> sampler;};
class MetalRaster final:public GpuRaster{
public:
    explicit MetalRaster(id<MTLDevice> device):device_(device){
        queue_=[device_ newCommandQueue];
        MTLCompileOptions* options=[MTLCompileOptions new];
        if(@available(macOS 15.0,iOS 18.0,tvOS 18.0,*))options.mathMode=MTLMathModeSafe;
        else{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            options.fastMathEnabled=NO;
#pragma clang diagnostic pop
        }
        NSError* error=nil;id<MTLLibrary> library=[device_ newLibraryWithSource:[NSString stringWithUTF8String:shader_source] options:options error:&error];
        if(!library)throw std::runtime_error(std::string("Metal shader: ")+error.localizedDescription.UTF8String);
        const auto pipeline=[&](NSString* vs,NSString* fs){MTLRenderPipelineDescriptor* d=[MTLRenderPipelineDescriptor new];
            d.vertexFunction=[library newFunctionWithName:vs];d.fragmentFunction=[library newFunctionWithName:fs];
            d.colorAttachments[0].pixelFormat=MTLPixelFormatR32Uint;d.colorAttachments[1].pixelFormat=MTLPixelFormatR32Float;d.colorAttachments[2].pixelFormat=MTLPixelFormatR32Float;
            NSError* e=nil;id<MTLRenderPipelineState> p=[device_ newRenderPipelineStateWithDescriptor:d error:&e];
            if(!p)throw std::runtime_error(std::string("Metal pipeline: ")+e.localizedDescription.UTF8String);return p;};
        raster_=pipeline(@"vs",@"fs");composite_=pipeline(@"vs_full",@"fs_shadow");
        {MTLRenderPipelineDescriptor* d=[MTLRenderPipelineDescriptor new];
         d.vertexFunction=[library newFunctionWithName:@"vs_mask"];d.fragmentFunction=[library newFunctionWithName:@"fs_mask"];
         d.colorAttachments[0].pixelFormat=MTLPixelFormatR8Uint;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
         NSError* e=nil;mask_pipeline_=[device_ newRenderPipelineStateWithDescriptor:d error:&e];
         if(!mask_pipeline_)throw std::runtime_error(std::string("Metal mask pipeline: ")+e.localizedDescription.UTF8String);}
        resolve_=[device_ newComputePipelineStateWithFunction:[library newFunctionWithName:@"resolve"] error:&error];
        if(!resolve_)throw std::runtime_error("Metal resolve pipeline");
        for(int linear=0;linear<2;++linear)for(int cu=0;cu<2;++cu)for(int cv=0;cv<2;++cv){MTLSamplerDescriptor* d=[MTLSamplerDescriptor new];
            d.minFilter=MTLSamplerMinMagFilterLinear;d.magFilter=MTLSamplerMinMagFilterLinear;
            d.mipFilter=linear?MTLSamplerMipFilterNotMipmapped:MTLSamplerMipFilterLinear;d.maxAnisotropy=linear?1:4;
            d.sAddressMode=cu?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeRepeat;d.tAddressMode=cv?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeRepeat;
            samplers_[linear][cu][cv]=[device_ newSamplerStateWithDescriptor:d];}
        MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:1 height:1 mipmapped:NO];d.usage=MTLTextureUsageShaderRead;blank_=[device_ newTextureWithDescriptor:d];
    }
    std::string name()const override{return std::string("Metal ")+device_.name.UTF8String;}
    const std::vector<std::string>& unsupported()const override{return unsupported_;}
    unsigned draw_calls()const override{return draw_calls_;}
    unsigned shadow_composites()const override{return composites_;}
    const Timings& timings()const override{return timings_;}
    bool render(const raster::Frame& frame,const raster::ShadowGeometry& shadows,std::vector<std::uint32_t>& pixels,std::vector<float>& depth)override{@autoreleasepool{
        using Clock=std::chrono::steady_clock;
        const auto since=[](Clock::time_point from){return std::chrono::duration<double>(Clock::now()-from).count();};
        const auto& setups=*frame.setups;const int S=frame.supersampling,W=frame.width*S,H=frame.height*S;
        targets(W,H);{const auto t=Clock::now();build(frame);timings_.build+=since(t);}
        const std::size_t bytes=std::max<std::size_t>(setups_.size(),1)*sizeof(GpuSetup);
        if(!setup_buffer_||setup_buffer_.length<bytes)setup_buffer_=[device_ newBufferWithLength:bytes*2 options:MTLResourceStorageModeShared];
        if(!setups_.empty())std::memcpy(setup_buffer_.contents,setups_.data(),setups_.size()*sizeof(GpuSetup));
        const FrameU fu{float(W),float(H),frame.fog_pixel,frame.wireframe?1u:0u,frame.background,{0,0,0}};
        draw_calls_=0;composites_=0;
        const std::size_t ends[3]={frame.shadows?frame.before_modifiers:setups.size(),frame.shadows?frame.before_mirror_effects:setups.size(),setups.size()};
        auto encode_begin=Clock::now();
        id<MTLCommandBuffer> cb=[queue_ commandBuffer];std::size_t batch=0;std::vector<raster::ShadowWork> pending;
        for(unsigned phase=0;phase<3;++phase){
            id<MTLRenderCommandEncoder> e=encoder(cb,phase==0,frame.background);
            for(std::size_t i=0;i<pending.size();++i)composite(e,pending[i],i,W,H);pending.clear();
            [e setRenderPipelineState:raster_];
            [e setVertexBuffer:setup_buffer_ offset:0 atIndex:0];[e setFragmentBuffer:setup_buffer_ offset:0 atIndex:0];
            [e setVertexBytes:&fu length:sizeof fu atIndex:1];[e setFragmentBytes:&fu length:sizeof fu atIndex:1];
            for(;batch<batches_.size()&&batches_[batch].first<ends[phase];++batch){const auto& b=batches_[batch];
                [e setFragmentTexture:b.texture atIndex:0];[e setFragmentSamplerState:b.sampler atIndex:0];
                [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:b.first*6 vertexCount:b.count*6];++draw_calls_;}
            [e endEncoding];
            if(phase<2&&frame.shadows&&frame.shadow_pass[phase]&&shadows){
                // Die Masken zeichnet die GPU aus derselben transportierten Tiefe: eigener Renderpass
                // im selben Befehlspuffer, deshalb kein Warten und kein Ruecklesen der Tiefe.
                pending=shadows(phase==1);masks(cb,pending,W,H);
            }
        }
        id<MTLComputeCommandEncoder> c=[cb computeCommandEncoder];[c setComputePipelineState:resolve_];
        [c setTexture:color_ atIndex:0];[c setTexture:depth_ atIndex:1];[c setBuffer:out_color_ offset:0 atIndex:0];[c setBuffer:out_depth_ offset:0 atIndex:1];
        const std::uint32_t size[4]={std::uint32_t(frame.width),std::uint32_t(frame.height),std::uint32_t(S),0};[c setBytes:size length:sizeof size atIndex:2];
        [c dispatchThreads:MTLSizeMake(frame.width,frame.height,1) threadsPerThreadgroup:MTLSizeMake(16,16,1)];[c endEncoding];
        timings_.encode+=since(encode_begin);
        {const auto t=Clock::now();wait_for(cb);check(cb);timings_.wait+=since(t);++timings_.submits;
         timings_.gpu+=cb.GPUEndTime-cb.GPUStartTime;}
        const auto readback_begin=Clock::now();
        const std::size_t n=std::size_t(frame.width)*frame.height;pixels.resize(n);depth.resize(n);
        std::memcpy(pixels.data(),out_color_.contents,n*4);std::memcpy(depth.data(),out_depth_.contents,n*4);
        timings_.readback+=since(readback_begin);
        return true;
    }}
private:
    // Schlafend auf die GPU warten statt waitUntilCompleted: dessen Wartezeit zaehlt als CPU-Zeit
    // (gemessen 5,1 ms je Bild bei 0,3 ms GPU-Arbeit), der Abschlusshandler weckt den Thread.
    static void wait_for(id<MTLCommandBuffer> cb){
        struct Fence{std::mutex mutex;std::condition_variable signal;bool done=false;};
        const auto fence=std::make_shared<Fence>();
        [cb addCompletedHandler:^(id<MTLCommandBuffer>){{std::lock_guard<std::mutex> lock(fence->mutex);fence->done=true;}fence->signal.notify_one();}];
        [cb commit];
        std::unique_lock<std::mutex> lock(fence->mutex);fence->signal.wait(lock,[&]{return fence->done;});
    }
    static void check(id<MTLCommandBuffer> cb){if(cb.error)throw std::runtime_error(std::string("Metal command buffer: ")+cb.error.localizedDescription.UTF8String);}
    void targets(int W,int H){
        if(color_&&int(color_.width)==W&&int(color_.height)==H)return;
        const auto make=[&](MTLPixelFormat format,MTLTextureUsage usage,MTLStorageMode storage){MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:W height:H mipmapped:NO];d.usage=usage;d.storageMode=storage;return [device_ newTextureWithDescriptor:d];};
        color_=make(MTLPixelFormatR32Uint,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,MTLStorageModePrivate);
        depth_=make(MTLPixelFormatR32Float,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,MTLStorageModePrivate);
        transported_=make(MTLPixelFormatR32Float,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,MTLStorageModePrivate);
        const std::size_t n=std::size_t(W)*H;
        out_color_=[device_ newBufferWithLength:n*4 options:MTLResourceStorageModeShared];out_depth_=[device_ newBufferWithLength:n*4 options:MTLResourceStorageModeShared];
    }
    id<MTLRenderCommandEncoder> encoder(id<MTLCommandBuffer> cb,bool clear,std::uint32_t background){
        MTLRenderPassDescriptor* rp=[MTLRenderPassDescriptor renderPassDescriptor];
        id<MTLTexture> attachments[3]={color_,depth_,transported_};
        for(int i=0;i<3;++i){rp.colorAttachments[i].texture=attachments[i];rp.colorAttachments[i].loadAction=clear?MTLLoadActionClear:MTLLoadActionLoad;rp.colorAttachments[i].storeAction=MTLStoreActionStore;
            rp.colorAttachments[i].clearColor=MTLClearColorMake(i==0?double(background):0.0,0,0,0);}
        id<MTLRenderCommandEncoder> e=[cb renderCommandEncoderWithDescriptor:rp];[e setCullMode:MTLCullModeNone];return e;
    }
    id<MTLTexture> texture(const SceneTexture& t,Variant variant){
        auto& cache=*t.filter_cache;const auto fingerprint=t.fingerprint();auto& slot=cache.gpu[variant];
        if(slot&&cache.gpu_owner[variant]==cache_owner()&&cache.gpu_fingerprint[variant]==fingerprint)return (__bridge id<MTLTexture>)slot.get();
        const auto upload_begin=std::chrono::steady_clock::now();++timings_.uploads;
        const std::size_t levels=variant==Point?1:1+(variant==Punch?cache.punch_mips.size():cache.mips.size());
        MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:variant==Premultiplied?MTLPixelFormatRGBA16Unorm:MTLPixelFormatBGRA8Unorm width:t.width height:t.height mipmapped:levels>1];
        d.mipmapLevelCount=levels;d.usage=MTLTextureUsageShaderRead;d.storageMode=MTLStorageModeShared;
        id<MTLTexture> tex=[device_ newTextureWithDescriptor:d];
        unsigned w=t.width,h=t.height;std::vector<std::uint16_t> wide;
        for(std::size_t level=0;level<levels;++level){
            const std::uint32_t* src=level==0?t.pixels.data():(variant==Punch?cache.punch_mips[level-1].data():cache.mips[level-1].data());
            if(variant==Premultiplied){wide.resize(std::size_t(w)*h*4);
                for(std::size_t i=0;i<std::size_t(w)*h;++i){const auto p=src[i];const double a=double(p>>24)/255.0;
                    wide[i*4]=std::uint16_t(double((p>>16)&255)/255.0*a*65535.0+.5);wide[i*4+1]=std::uint16_t(double((p>>8)&255)/255.0*a*65535.0+.5);
                    wide[i*4+2]=std::uint16_t(double(p&255)/255.0*a*65535.0+.5);wide[i*4+3]=std::uint16_t(a*65535.0+.5);}
                [tex replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:level withBytes:wide.data() bytesPerRow:w*8];}
            else [tex replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:level withBytes:src bytesPerRow:w*4];
            w=std::max(1u,w/2);h=std::max(1u,h/2);}
        slot=std::shared_ptr<void>((__bridge_retained void*)tex,[](void* p){CFRelease(p);});cache.gpu_fingerprint[variant]=fingerprint;cache.gpu_owner[variant]=cache_owner();
        timings_.upload+=std::chrono::duration<double>(std::chrono::steady_clock::now()-upload_begin).count();
        return tex;
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
            const id<MTLTexture> texture=s.texture?this->texture(*s.texture,variant):blank_;
            const id<MTLSamplerState> sampler=samplers_[s.linear_only?1:0][s.clamp_u?1:0][s.clamp_v?1:0];
            // Ein Batch darf keine Phasengrenze ueberspannen: render() zeichnet ihn sonst ganz vor den
            // Schattenmasken bzw. dem Spiegelpass (game.c:318/417, renderer.cpp Phasenfolge).
            const bool phase_start=frame.shadows&&(i==frame.before_modifiers||i==frame.before_mirror_effects);
            if(!phase_start&&!batches_.empty()&&batches_.back().texture==texture&&batches_.back().sampler==sampler&&batches_.back().first+batches_.back().count==i){++batches_.back().count;continue;}
            batches_.push_back({i,1,texture,sampler});
        }
    }
    // Ein Renderpass fuer alle Masken dieser Phase (eine Ebene je Werk): die Dreiecksreihenfolge je
    // Ebene ist die von raster_shadow_mask, die saettigende Akkumulation macht der Shader.
    void masks(id<MTLCommandBuffer> cb,const std::vector<raster::ShadowWork>& works,int W,int H){
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
        mask_targets(unsigned(works.size()));
        MTLRenderPassDescriptor* rp=[MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture=mask_;rp.colorAttachments[0].loadAction=MTLLoadActionClear;
        rp.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);rp.colorAttachments[0].storeAction=MTLStoreActionStore;
        rp.renderTargetArrayLength=works.size();
        id<MTLRenderCommandEncoder> e=[cb renderCommandEncoderWithDescriptor:rp];
        [e setCullMode:MTLCullModeNone];
        if(!triangles_.empty()){
            const std::size_t bytes=triangles_.size()*sizeof(GpuShadowTri);
            if(!shadow_buffer_||shadow_buffer_.length<bytes)shadow_buffer_=[device_ newBufferWithLength:bytes*2 options:MTLResourceStorageModeShared];
            std::memcpy(shadow_buffer_.contents,triangles_.data(),bytes);
            const MaskU mu{std::uint32_t(W),std::uint32_t(H),{0,0}};
            [e setRenderPipelineState:mask_pipeline_];
            [e setVertexBuffer:shadow_buffer_ offset:0 atIndex:0];[e setFragmentBuffer:shadow_buffer_ offset:0 atIndex:0];
            [e setFragmentBytes:&mu length:sizeof mu atIndex:1];[e setFragmentTexture:transported_ atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:triangles_.size()*6];++draw_calls_;}
        [e endEncoding];
    }
    void mask_targets(unsigned layers){
        if(mask_&&mask_.arrayLength>=layers)return;
        MTLTextureDescriptor* d=[MTLTextureDescriptor new];d.textureType=MTLTextureType2DArray;d.pixelFormat=MTLPixelFormatR8Uint;
        d.width=640;d.height=480;d.arrayLength=layers;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;d.storageMode=MTLStorageModePrivate;
        mask_=[device_ newTextureWithDescriptor:d];
    }
    void composite(id<MTLRenderCommandEncoder> e,const raster::ShadowWork& c,std::size_t index,int W,int H){
        if(c.fog_power>255u)throw std::runtime_error("shadow: invalid fog channel");
        const CompositeU u{float(W),float(H),c.rx0,c.ry0,c.rx1,c.ry1,c.fog_power,std::uint32_t(index)};
        [e setRenderPipelineState:composite_];[e setFragmentBytes:&u length:sizeof u atIndex:0];[e setFragmentTexture:mask_ atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];++draw_calls_;++composites_;
    }
    id<MTLDevice> device_;id<MTLCommandQueue> queue_;id<MTLRenderPipelineState> raster_,composite_,mask_pipeline_;id<MTLComputePipelineState> resolve_;
    id<MTLSamplerState> samplers_[2][2][2];id<MTLTexture> blank_,color_,depth_,transported_;id<MTLBuffer> out_color_,out_depth_,setup_buffer_,shadow_buffer_;id<MTLTexture> mask_;
    std::vector<GpuSetup> setups_;std::vector<Batch> batches_;std::vector<GpuShadowTri> triangles_;std::vector<std::string> unsupported_;unsigned draw_calls_=0,composites_=0;Timings timings_;
};
}
std::shared_ptr<GpuRaster> create_gpu_raster(std::string& reason){@autoreleasepool{
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    if(!device){reason="no Metal device";return nullptr;}
    // Programmierbares Mischen (Framebuffer-Fetch) haben alle Apple-GPUs; ohne es kein exakter Pfad.
    if(![device supportsFamily:MTLGPUFamilyApple1]){reason="Metal device without programmable blending";return nullptr;}
    try{return std::make_shared<MetalRaster>(device);}catch(const std::exception& e){reason=e.what();return nullptr;}
}}
}
