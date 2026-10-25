#pragma once
// HLSL-Quelle der Direct3D-Rasterer (gpu_raster_d3d11.cpp, gpu_raster_d3d12.cpp): dieselben Rechnungen wie
// native/gpu_raster_metal.mm und renderer.cpp rasterize(). Ordnung ueber Rasterizer Ordered Views; die
// Registerbelegung (t0..t3, s0, u0..u5, b0..b3) ist der Bindungsvertrag beider Backends.
namespace recvx {
inline const char* const raster_hlsl_source=R"HLSL(
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
// Erster Setup-Index des Zeichenaufrufs. Jeder Aufruf startet bei Vertex 0: ob SV_VertexID den
// StartVertexLocation-Versatz enthaelt, legen Treiber verschieden aus (WARP und Parallels nicht).
cbuffer DrawCB : register(b3) { uint dFirst,dPad0,dPad1,dPad2; };
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
    uint index=dFirst+vid/6u,corner=vid%6u; int4 b=Setups[index].box;
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
}
