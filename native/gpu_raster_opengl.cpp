// OpenGL-Backend des Host-GPU-Rasterers (gpu_raster.h): Core-Profil 4.1, damit derselbe Pfad auch unter
// macOS laeuft; Standardbackend unter Linux, sonst ausdruecklich waehlbar. Plattformanpassung, keine
// Spiellogik: Clipping, Streifenregeln, Sortierung und Schattenphasen bleiben im SoftwareRenderer, hier
// wird nur dieselbe raster::Setup-/ShadowWork-Liste mit den Pixelregeln der Bandstufe gerastert.
//
// OpenGL ordnet kein Lesen-Aendern-Schreiben desselben Pixels zwischen ueberlappenden Fragmenten (das
// leisten Metal ueber Framebuffer-Fetch und D3D11 ueber Rasterizer Ordered Views). Deshalb gibt es zwei
// Saetze derselben Flaechen: A (Farbe 0xAARRGGBB, 1/z, transportierte Schattentiefe) wird beschrieben,
// R nur gelesen. Vor jeder Gruppe kopiert ein Blit das betroffene Rechteck von A nach R; eine Gruppe
// besteht aus aufeinanderfolgenden Setups mit paarweise disjunkten Pixelrechtecken, sodass jedes Pixel
// je Zeichenaufruf hoechstens einmal gelesen und geschrieben wird und die Listenreihenfolge erhalten
// bleibt. Dasselbe gilt fuer die saettigenden Schattenmasken und die Komposite. Ab GL 4.5 bzw.
// ARB_texture_barrier kann der Farbrasterpass stattdessen aus A lesen: TextureBarrier zwischen den
// Gruppen, genau ein Same-Pixel-Read/Write je Texel einer Gruppe (ARB_texture_barrier, Abschnitt 9.3.1).
// Masken und Komposite behalten den Kopierpfad; force_copy erzwingt ihn auch fuer den Farbrasterpass.
//
// Bildzeile y liegt in Speicherzeile y (keine Spiegelung): Setupkoordinaten, gl_FragCoord, texelFetch,
// Blits und glReadPixels benutzen dieselbe Zaehlung.
//
// Kritische Rechnungen sind `precise`, damit der Compiler keine Multiplikation-Addition zusammenzieht.
// Division/Rundung bleibt Treibereigenschaft: Bitgleichheit zur Bandstufe belegt erst der Paritaetstest
// auf genau diesem Treiber; keine Epsilons oder angenaeherten Korrekturformeln.
#include "gpu_raster.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
namespace recvx {
namespace {
#if defined(_WIN32)
#define RECVX_GLAPI __stdcall
#else
#define RECVX_GLAPI
#endif
using GLenum=unsigned int;using GLuint=unsigned int;using GLint=int;using GLsizei=int;using GLboolean=unsigned char;
using GLbitfield=unsigned int;using GLfloat=float;using GLchar=char;using GLubyte=unsigned char;
constexpr GLenum GL_NO_ERROR=0,GL_NONE=0,GL_TRIANGLES=0x0004,GL_CULL_FACE=0x0B44,GL_DEPTH_TEST=0x0B71,GL_STENCIL_TEST=0x0B90,
    GL_DITHER=0x0BD0,GL_BLEND=0x0BE2,GL_SCISSOR_TEST=0x0C11,GL_UNPACK_ALIGNMENT=0x0CF5,GL_PACK_ALIGNMENT=0x0D05,GL_MAX_TEXTURE_SIZE=0x0D33,
    GL_TEXTURE_2D=0x0DE1,GL_UNSIGNED_BYTE=0x1401,GL_UNSIGNED_SHORT=0x1403,GL_UNSIGNED_INT=0x1405,GL_FLOAT=0x1406,GL_COLOR=0x1800,
    GL_RED=0x1903,GL_RGBA=0x1908,GL_RENDERER=0x1F01,GL_EXTENSIONS=0x1F03,GL_NEAREST=0x2600,GL_LINEAR=0x2601,
    GL_LINEAR_MIPMAP_LINEAR=0x2703,GL_TEXTURE_MAG_FILTER=0x2800,GL_TEXTURE_MIN_FILTER=0x2801,GL_TEXTURE_WRAP_S=0x2802,
    GL_TEXTURE_WRAP_T=0x2803,GL_REPEAT=0x2901,GL_COLOR_BUFFER_BIT=0x4000,GL_RGBA8=0x8058,GL_RGBA16=0x805B,GL_MULTISAMPLE=0x809D,
    GL_BGRA=0x80E1,GL_CLAMP_TO_EDGE=0x812F,GL_TEXTURE_BASE_LEVEL=0x813C,GL_TEXTURE_MAX_LEVEL=0x813D,GL_MAJOR_VERSION=0x821B,
    GL_MINOR_VERSION=0x821C,GL_NUM_EXTENSIONS=0x821D,GL_R32F=0x822E,GL_R8UI=0x8232,GL_R32UI=0x8236,GL_TEXTURE0=0x84C0,
    GL_TEXTURE_MAX_ANISOTROPY=0x84FE,GL_MAX_ARRAY_TEXTURE_LAYERS=0x88FF,GL_FRAGMENT_SHADER=0x8B30,GL_VERTEX_SHADER=0x8B31,
    GL_COMPILE_STATUS=0x8B81,GL_LINK_STATUS=0x8B82,GL_INFO_LOG_LENGTH=0x8B84,GL_TEXTURE_2D_ARRAY=0x8C1A,GL_READ_FRAMEBUFFER=0x8CA8,
    GL_DRAW_FRAMEBUFFER=0x8CA9,GL_FRAMEBUFFER_COMPLETE=0x8CD5,GL_COLOR_ATTACHMENT0=0x8CE0,GL_FRAMEBUFFER=0x8D40,GL_RGBA32UI=0x8D70,
    GL_RED_INTEGER=0x8D94,GL_RGBA_INTEGER=0x8D99,GL_FRAMEBUFFER_SRGB=0x8DB9;
// Alle Einsprungpunkte ueber SDL_GL_GetProcAddress des eigenen Kontexts; keine Loaderbibliothek.
#define RECVX_GL_FUNCTIONS(X) \
    X(GLenum,GetError,(void)) X(void,GetIntegerv,(GLenum,GLint*)) X(const GLubyte*,GetString,(GLenum)) \
    X(const GLubyte*,GetStringi,(GLenum,GLuint)) X(void,Enable,(GLenum)) X(void,Disable,(GLenum)) \
    X(void,Viewport,(GLint,GLint,GLsizei,GLsizei)) X(void,GenTextures,(GLsizei,GLuint*)) X(void,DeleteTextures,(GLsizei,const GLuint*)) \
    X(void,BindTexture,(GLenum,GLuint)) X(void,ActiveTexture,(GLenum)) \
    X(void,TexImage2D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) \
    X(void,TexSubImage2D,(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*)) \
    X(void,TexImage3D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) \
    X(void,TexParameteri,(GLenum,GLenum,GLint)) X(void,PixelStorei,(GLenum,GLint)) \
    X(void,GenFramebuffers,(GLsizei,GLuint*)) X(void,DeleteFramebuffers,(GLsizei,const GLuint*)) X(void,BindFramebuffer,(GLenum,GLuint)) \
    X(void,FramebufferTexture2D,(GLenum,GLenum,GLenum,GLuint,GLint)) X(void,FramebufferTextureLayer,(GLenum,GLenum,GLuint,GLint,GLint)) \
    X(GLenum,CheckFramebufferStatus,(GLenum)) X(void,DrawBuffers,(GLsizei,const GLenum*)) X(void,ReadBuffer,(GLenum)) \
    X(void,BlitFramebuffer,(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum)) \
    X(void,ClearBufferuiv,(GLenum,GLint,const GLuint*)) X(void,ClearBufferfv,(GLenum,GLint,const GLfloat*)) \
    X(void,ReadPixels,(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*)) X(GLuint,CreateShader,(GLenum)) \
    X(void,ShaderSource,(GLuint,GLsizei,const GLchar* const*,const GLint*)) X(void,CompileShader,(GLuint)) \
    X(void,GetShaderiv,(GLuint,GLenum,GLint*)) X(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) X(GLuint,CreateProgram,(void)) \
    X(void,AttachShader,(GLuint,GLuint)) X(void,LinkProgram,(GLuint)) X(void,GetProgramiv,(GLuint,GLenum,GLint*)) \
    X(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) X(void,DeleteShader,(GLuint)) X(void,UseProgram,(GLuint)) \
    X(GLint,GetUniformLocation,(GLuint,const GLchar*)) X(void,Uniform1i,(GLint,GLint)) X(void,Uniform1ui,(GLint,GLuint)) X(void,Uniform2ui,(GLint,GLuint,GLuint)) \
    X(void,Uniform2f,(GLint,GLfloat,GLfloat)) X(void,Uniform4f,(GLint,GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void,GenVertexArrays,(GLsizei,GLuint*)) X(void,BindVertexArray,(GLuint)) X(void,DrawArrays,(GLenum,GLint,GLsizei)) \
    X(void,GenSamplers,(GLsizei,GLuint*)) X(void,SamplerParameteri,(GLuint,GLenum,GLint)) X(void,SamplerParameterf,(GLuint,GLenum,GLfloat)) \
    X(void,BindSampler,(GLuint,GLuint))
struct Gl {
#define RECVX_GL_MEMBER(ret,name,args) ret(RECVX_GLAPI* name)args=nullptr;
    RECVX_GL_FUNCTIONS(RECVX_GL_MEMBER)
#undef RECVX_GL_MEMBER
    void load(){
#define RECVX_GL_LOAD(ret,name,args) name=reinterpret_cast<ret(RECVX_GLAPI*)args>(SDL_GL_GetProcAddress("gl" #name)); \
        if(!name)throw std::runtime_error("OpenGL function gl" #name " missing");
        RECVX_GL_FUNCTIONS(RECVX_GL_LOAD)
#undef RECVX_GL_LOAD
    }
};

const char* common_glsl=R"GLSL(#version 410 core
// Gleiche Operanden wie die Bandstufe, ohne Kontraktion oder algebraische Ersatzformel.
float div_rn(float x,float y){precise float result=x/y;return result;}
uint ch(uint c,uint s){return (c>>s)&255u;}
float edge(vec2 a,vec2 b,float x,float y){precise float r=(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);return r;}
bool inclusive(vec2 from,vec2 to){return to.y<from.y||(to.y==from.y&&to.x>from.x);}
)GLSL";

// Setupliste als RGBA32UI-Textur: 13 Texel je Setup, 256 Setups je Zeile (Layout wie GpuSetup).
const char* raster_vs=R"GLSL(
uniform usampler2D u_setups;uniform vec2 u_size;
flat out int v_setup;
void main(){
    int index=gl_VertexID/6,corner=gl_VertexID%6;
    ivec4 b=ivec4(texelFetch(u_setups,ivec2((index%256)*13+12,index/256),0));
    bool right=corner==1||corner==2||corner==4,bottom=corner==2||corner==4||corner==5;
    float x=float(right?b.z+1:b.x),y=float(bottom?b.w+1:b.y);
    gl_Position=vec4(x/u_size.x*2.0-1.0,y/u_size.y*2.0-1.0,0.0,1.0);v_setup=index;}
)GLSL";
const char* raster_fs=R"GLSL(
uniform usampler2D u_setups;uniform usampler2D u_point;uniform sampler2D u_tex;
uniform usampler2D u_below_color;uniform sampler2D u_below_depth;uniform sampler2D u_below_transported;
uniform uint u_fog_pixel,u_wireframe,u_background;
flat in int v_setup;
layout(location=0) out uint o_pixel;layout(location=1) out float o_depth;layout(location=2) out float o_transported;
uvec4 S(int k){return texelFetch(u_setups,ivec2((v_setup%256)*13+k,v_setup/256),0);}
vec4 F(int k){return uintBitsToFloat(S(k));}
uint lit1(uint base,uint s,float f){precise float v=float(ch(base,s))*f+0.5;return uint(clamp(v,0.0,255.0));}
uint lit(uint base,float r,float g,float b){return (base&0xff000000u)|(lit1(base,16u,r)<<16)|(lit1(base,8u,g)<<8)|lit1(base,0u,b);}
uint modulate(uint t,uint l,uint d){
    return 0xff000000u|(min(255u,(ch(t,16u)*ch(l,16u))/d)<<16)|(min(255u,(ch(t,8u)*ch(l,8u))/d)<<8)|min(255u,(ch(t,0u)*ch(l,0u))/d);}
// chunk_blend.cpp blend_gs: (A - B) * C + D je Kanal.
uint blend_gs(uint lo,uint hi,uint source,uint destination,float source_alpha){
    uint a=lo&3u,b=(lo>>2)&3u,c=(lo>>4)&3u,d=(lo>>6)&3u;
    float alpha=clamp(source_alpha,0.0,1.0);
    precise float factor=c==0u?alpha:c==1u?div_rn(float(destination>>24),255.0):div_rn(float(hi&255u),128.0);
    precise float av=alpha*255.0+0.5;
    uint result=uint(av)<<24;
    for(uint shift=0u;shift<24u;shift+=8u){
        float ca=a==0u?float(ch(source,shift)):a==1u?float(ch(destination,shift)):0.0;
        float cb=b==0u?float(ch(source,shift)):b==1u?float(ch(destination,shift)):0.0;
        float cd=d==0u?float(ch(source,shift)):d==1u?float(ch(destination,shift)):0.0;
        precise float v=(ca-cb)*factor+cd+0.5;
        result|=uint(clamp(v,0.0,255.0))<<shift;}
    return result;}
// Flags wie gpu_raster_metal.mm: 1 gouraud, 2 vertex_alpha, 4 textured, 8 filtered, 16 punch, 32 fogged,
// 64 fog_color_set, 128 clamp_u, 256 clamp_v, 512 cutout, 1024 uniform_alpha, 2048 linear_only,
// 4096 nonopaque, 16384 general_water, 32768 premultiplied chain, 65536 depth_write.
void main(){
    ivec2 at=ivec2(gl_FragCoord.xy);
    uint below=texelFetch(u_below_color,at,0).r;float below_depth=texelFetch(u_below_depth,at,0).r;
    o_pixel=below;o_depth=below_depth;o_transported=texelFetch(u_below_transported,at,0).r;
    vec4 p0=F(0),p1=F(1),p2=F(2),uv0=F(3),uv1=F(4),grad=F(5),l0=F(6),l1=F(7),l2=F(8),fogv=F(9);uvec4 info=S(10),blend=S(11);
    float X=gl_FragCoord.x,Y=gl_FragCoord.y; // Pixelmitte x + 0.5, y + 0.5
    float area=uv1.z;
    precise float a=div_rn(edge(p1.xy,p2.xy,X,Y),area);
    precise float b=div_rn(edge(p2.xy,p0.xy,X,Y),area);
    precise float c=div_rn(edge(p0.xy,p1.xy,X,Y),area);
    if(a<0.0||b<0.0||c<0.0)discard;
    if((a==0.0&&!inclusive(p1.xy,p2.xy))||(b==0.0&&!inclusive(p2.xy,p0.xy))||(c==0.0&&!inclusive(p0.xy,p1.xy)))discard;
    precise float invz=p0.z+b*(p1.z-p0.z)+c*(p2.z-p0.z);
    if(invz<below_depth)discard; // GEQUAL
    uint color=info.x,flags=info.y,divisor=info.w&0xffffu;float max_alpha=float(info.w>>16);
    precise float va=a*l0.w+b*l1.w+c*l2.w;
    precise float material_alpha=div_rn(float(color>>24),255.0)*((flags&2u)!=0u?clamp(va,0.0,1.0):1.0);
    if(material_alpha<=0.0)discard; // TEST GREATER 0 / KEEP
    uint kind=blend.z;
    uint pixel=color;precise float coverage=1.0;
    if((flags&1u)!=0u){
        precise float lr=a*l0.x+b*l1.x+c*l2.x;precise float lg=a*l0.y+b*l1.y+c*l2.y;precise float lb=a*l0.z+b*l1.z+c*l2.z;
        pixel=lit(color,lr,lg,lb);}
    if((flags&4u)!=0u){
        precise float rz=div_rn(1.0,invz);
        precise float u=(a*uv0.x+b*uv0.z+c*uv1.x)*rz;
        precise float v=(a*uv0.y+b*uv0.w+c*uv1.y)*rz;
        if((flags&8u)==0u){
            if((flags&128u)!=0u)u=clamp(u,0.0,1.0);else u-=floor(u);
            if((flags&256u)!=0u)v=clamp(v,0.0,1.0);else v-=floor(v);
            ivec2 size=textureSize(u_point,0);uint w=uint(size.x),h=uint(size.y);
            precise float fu=u*float(w);precise float fv=v*float(h);
            uint texel=texelFetch(u_point,ivec2(int(min(w-1u,uint(fu))),int(min(h-1u,uint(fv)))),0).r;
            if((texel>>24)==0u)discard;
            pixel=modulate(texel,pixel,divisor);
            if(kind!=0u)coverage=div_rn(float(texel>>24),255.0);
        }else{
            // Ableitungen von (u,v) je Bildpixel wie die Bandstufe: d(U/W) = (dU - u*dW)/W.
            float Wx=uv1.w,Wy=fogv.w;
            precise vec2 gx=vec2((grad.x-u*Wx)*rz,(grad.z-v*Wx)*rz);precise vec2 gy=vec2((grad.y-u*Wy)*rz,(grad.w-v*Wy)*rz);
            if((flags&128u)==0u)u-=floor(u);
            if((flags&256u)==0u)v-=floor(v);
            vec4 t=(flags&2048u)!=0u?textureLod(u_tex,vec2(u,v),0.0):textureGrad(u_tex,vec2(u,v),gx,gy);
            if(t.a<=0.0)discard;
            if(kind!=0u)coverage=min(1.0,(flags&1024u)!=0u?div_rn(max_alpha,255.0):t.a);
            else if((flags&512u)!=0u&&(flags&16u)==0u){coverage=min(1.0,div_rn(t.a*255.0,max_alpha));if(coverage<div_rn(1.0,32.0))discard;}
            vec3 rgb=(flags&32768u)!=0u?vec3(div_rn(t.r,t.a),div_rn(t.g,t.a),div_rn(t.b,t.a)):t.rgb;
            precise vec3 q=rgb*255.0+0.5;
            uint texel=0xff000000u|(uint(clamp(q.r,0.0,255.0))<<16)|(uint(clamp(q.g,0.0,255.0))<<8)|uint(clamp(q.b,0.0,255.0));
            pixel=modulate(texel,pixel,divisor);
        }
    }
    if((flags&32u)!=0u){ // GS: Cv = (Cs*F + Cf*(255-F)) >> 8
        precise float fv=a*fogv.x+b*fogv.y+c*fogv.z;
        uint Fc=uint(clamp(fv,0.0,255.0));uint fog=(flags&64u)!=0u?info.z:u_fog_pixel;
        pixel=(pixel&0xff000000u)|(((ch(pixel,16u)*Fc+ch(fog,16u)*(255u-Fc))>>8)<<16)|(((ch(pixel,8u)*Fc+ch(fog,8u)*(255u-Fc))>>8)<<8)|((ch(pixel,0u)*Fc+ch(fog,0u)*(255u-Fc))>>8);
    }
    bool depth_write=(flags&65536u)!=0u;
    if(kind==1u){ // additiv, ohne Tiefe
        precise float alpha=coverage*material_alpha;uint mixed=0xff000000u;
        for(uint shift=0u;shift<24u;shift+=8u){precise float m=float(ch(pixel,shift))*alpha+0.5;mixed|=min(255u,ch(below,shift)+uint(m))<<shift;}
        o_pixel=mixed;return;}
    if(kind==2u){ // GS ALPHA; Tiefe ohne transportierte Tiefe (Ps2ZbuffOff alpha >= 128 / direktes Wasser)
        precise float alpha=coverage*material_alpha;
        o_pixel=blend_gs(blend.x,blend.y,pixel,below,alpha);
        if(depth_write&&((flags&16384u)!=0u?alpha>0.0:alpha>=0.5))o_depth=invz;
        return;}
    if(coverage<0.999){
        uint mixed=0xff000000u;
        for(uint shift=0u;shift<24u;shift+=8u){precise float m=float(ch(below,shift))*(1.0-coverage)+float(ch(pixel,shift))*coverage+0.5;mixed|=uint(m)<<shift;}
        pixel=mixed;if(coverage<0.5){o_pixel=pixel;return;}
    }
    if(depth_write){o_depth=invz;precise float tz=a*p0.w+b*p1.w+c*p2.w;o_transported=tz;}
    o_pixel=(u_wireframe!=0u&&min(min(a,b),c)>0.025)?u_background:pixel;
}
)GLSL";
// Maskenrasterung von bhDrawMdfEffect (shadow_raster.h) im 640x480-Bild: 4 Texel je Dreieck, 1024 je Zeile.
const char* mask_vs=R"GLSL(
uniform usampler2D u_tris;
flat out int v_tri;
void main(){
    int index=gl_VertexID/6,corner=gl_VertexID%6;
    ivec4 b=ivec4(texelFetch(u_tris,ivec2((index%1024)*4+3,index/1024),0));
    bool right=corner==1||corner==2||corner==4,bottom=corner==2||corner==4||corner==5;
    float x=float(right?b.z+1:b.x),y=float(bottom?b.w+1:b.y);
    gl_Position=vec4(x/640.0*2.0-1.0,y/480.0*2.0-1.0,0.0,1.0);v_tri=index;}
)GLSL";
const char* mask_fs=R"GLSL(
uniform usampler2D u_tris;uniform usampler2D u_below;uniform sampler2D u_transported;uniform uvec2 u_size;
flat in int v_tri;
layout(location=0) out uint o_mask;
uvec4 T(int k){return texelFetch(u_tris,ivec2((v_tri%1024)*4+k,v_tri/1024),0);}
void main(){
    ivec2 at=ivec2(gl_FragCoord.xy);uint below=texelFetch(u_below,at,0).r;o_mask=below;
    vec4 v01=uintBitsToFloat(T(0)),v2a=uintBitsToFloat(T(1));uvec4 depth=T(2);
    vec2 p0=v01.xy,p1=v01.zw,p2=v2a.xy;float X=gl_FragCoord.x,Y=gl_FragCoord.y;
    precise float a=edge(p1,p2,X,Y);precise float b=edge(p2,p0,X,Y);precise float c=edge(p0,p1,X,Y);
    if(a<0.0||b<0.0||c<0.0)discard;
    if((a==0.0&&!inclusive(p1,p2))||(b==0.0&&!inclusive(p2,p0))||(c==0.0&&!inclusive(p0,p1)))discard;
    precise float sum=a*float(depth.x)+b*float(depth.y)+c*float(depth.z);
    precise float z=div_rn(sum,v2a.z);
    precise float fx=div_rn(X*float(u_size.x),640.0);precise float fy=div_rn(Y*float(u_size.y),480.0);
    uint sx=min(u_size.x-1u,uint(fx)),sy=min(u_size.y-1u,uint(fy));
    if(uint(z)<uint(texelFetch(u_transported,ivec2(sx,sy),0).r))discard; // GEQUAL
    int value=int(below)+((depth.w&1u)!=0u?-32:32); // shadow_mask_fragment
    o_mask=uint(clamp(value,0,255))&0xf8u;
}
)GLSL";
const char* full_vs=R"GLSL(
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)GLSL";
const char* composite_fs=R"GLSL(
uniform usampler2D u_below_color;uniform sampler2D u_below_depth;uniform sampler2D u_below_transported;
uniform usampler2DArray u_mask;uniform int u_layer;uniform vec2 u_size;uniform vec4 u_rect;uniform uint u_fog;
layout(location=0) out uint o_pixel;layout(location=1) out float o_depth;layout(location=2) out float o_transported;
void main(){
    ivec2 at=ivec2(gl_FragCoord.xy);uint below=texelFetch(u_below_color,at,0).r;
    o_pixel=below;o_depth=texelFetch(u_below_depth,at,0).r;o_transported=texelFetch(u_below_transported,at,0).r;
    float y=gl_FragCoord.y-0.5,x=gl_FragCoord.x-0.5;
    precise float fy=div_rn((y+0.5)*480.0,u_size.y);int my=min(479,int(fy));if(float(my)+0.5<u_rect.y||float(my)+0.5>=u_rect.w)return;
    precise float fx=div_rn((x+0.5)*640.0,u_size.x);int mx=min(639,int(fx));if(float(mx)+0.5<u_rect.x||float(mx)+0.5>=u_rect.z)return;
    uint value=texelFetch(u_mask,ivec3(mx,my,u_layer),0).r;if(value==0u)return;
    uint reduction=(value*((u_fog*3u)/16u))/128u; // shadow_mask_composite
    uint result=below&0xff000000u;
    for(uint shift=0u;shift<24u;shift+=8u){uint v=ch(below,shift);result|=(v>reduction?v-reduction:0u)<<shift;}
    o_pixel=result;
}
)GLSL";
const char* resolve_fs=R"GLSL(
uniform usampler2D u_color;uniform sampler2D u_depth;uniform int u_scale;
layout(location=0) out uint o_pixel;layout(location=1) out float o_depth;
void main(){
    ivec2 gid=ivec2(gl_FragCoord.xy);int S=u_scale;
    if(S==1){o_pixel=texelFetch(u_color,gid,0).r;o_depth=texelFetch(u_depth,gid,0).r;return;}
    uint r=0u,g=0u,b=0u;float nearest=0.0;uint n=uint(S*S);
    for(int sy=0;sy<S;++sy)for(int sx=0;sx<S;++sx){ivec2 q=gid*S+ivec2(sx,sy);uint c=texelFetch(u_color,q,0).r;
        r+=c&255u;g+=(c>>8)&255u;b+=(c>>16)&255u;nearest=max(nearest,texelFetch(u_depth,q,0).r);}
    o_pixel=0xff000000u|(((b+n/2u)/n)<<16)|(((g+n/2u)/n)<<8)|((r+n/2u)/n);o_depth=nearest;
}
)GLSL";

struct alignas(16) GpuSetup{float p0[4],p1[4],p2[4],uv0[4],uv1[4],grad[4],l0[4],l1[4],l2[4],fog[4];std::uint32_t info[4],blend[4];std::int32_t box[4];};
static_assert(sizeof(GpuSetup)==208,"setup layout");
struct alignas(16) GpuShadowTri{float v01[4],v2a[4];std::uint32_t depth[4];std::int32_t box[4];};
static_assert(sizeof(GpuShadowTri)==64,"shadow triangle layout");
constexpr int setups_per_row=256,tris_per_row=1024;
// Darstellungsart einer Textur, Index in TextureFilterCache::gpu wie bei Metal/D3D11.
enum Variant{Point=1,Straight=2,Premultiplied=3,Punch=4};
struct Rect{int x0,y0,x1,y1;}; // [x0,x1) x [y0,y1)
bool overlaps(const Rect& a,const Rect& b){return a.x0<b.x1&&b.x0<a.x1&&a.y0<b.y1&&b.y0<a.y1;}

// Versteckter SDL-OpenGL-Kontext dieses Backends; unabhaengig vom Kontext eines SDL-Praesentationsrenderers.
// Texturen, deren Cacheeintrag ausserhalb eines Renderaufrufs stirbt, werden hier gesammelt und beim
// naechsten Bild im aktuellen Kontext geloescht.
struct Context {
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;bool video=false;Gl gl;
    std::mutex mutex;std::vector<GLuint> dead;
    ~Context(){
        // Hat der Prozess SDL bereits beendet (SDL_Quit der Runtime), sind Fenster und Kontext mit ihm
        // gegangen; dann nichts mehr anfassen.
        if(!video||!SDL_WasInit(SDL_INIT_VIDEO))return;
        SDL_Window* const previous_window=SDL_GL_GetCurrentWindow();const SDL_GLContext previous_context=SDL_GL_GetCurrentContext();
        if(previous_context==context)SDL_GL_MakeCurrent(nullptr,nullptr);
        if(context)SDL_GL_DeleteContext(context);
        if(window)SDL_DestroyWindow(window);
        if(previous_context&&previous_context!=context)SDL_GL_MakeCurrent(previous_window,previous_context);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
};
// Macht den eigenen Kontext fuer einen Bereich aktuell und stellt das vorherige Paar wieder her.
class Current {
public:
    explicit Current(Context& c):window_(SDL_GL_GetCurrentWindow()),context_(SDL_GL_GetCurrentContext()){
        if(SDL_GL_MakeCurrent(c.window,c.context)!=0)throw std::runtime_error(std::string("OpenGL make current: ")+SDL_GetError());}
    ~Current(){SDL_GL_MakeCurrent(context_?window_:nullptr,context_);}
    Current(const Current&)=delete;Current& operator=(const Current&)=delete;
private:
    SDL_Window* window_;SDL_GLContext context_;
};
struct GlTexture{GLuint id=0;std::weak_ptr<Context> owner;};

class OpenGlRaster final:public GpuRaster{
public:
    explicit OpenGlRaster(std::shared_ptr<Context> context,bool force_copy):ctx_(std::move(context)),gl_(ctx_->gl){
        Current current(*ctx_);
        GLint major=0,minor=0;gl_.GetIntegerv(GL_MAJOR_VERSION,&major);gl_.GetIntegerv(GL_MINOR_VERSION,&minor);
        if(major<4||(major==4&&minor<1))throw std::runtime_error("OpenGL "+std::to_string(major)+"."+std::to_string(minor)+" below 4.1 core");
        const auto* renderer=gl_.GetString(GL_RENDERER);renderer_=renderer?reinterpret_cast<const char*>(renderer):"unknown";
        version_=std::to_string(major)+"."+std::to_string(minor);
        bool texture_barrier=major>4||(major==4&&minor>=5);
        GLint extensions=0;gl_.GetIntegerv(GL_NUM_EXTENSIONS,&extensions);
        for(GLint i=0;i<extensions;++i){const auto* e=gl_.GetStringi(GL_EXTENSIONS,GLuint(i));
            if(e&&!std::strcmp(reinterpret_cast<const char*>(e),"GL_ARB_texture_barrier"))texture_barrier=true;
            if(e&&(!std::strcmp(reinterpret_cast<const char*>(e),"GL_EXT_texture_filter_anisotropic")||!std::strcmp(reinterpret_cast<const char*>(e),"GL_ARB_texture_filter_anisotropic")))anisotropy_=true;}
        if(major>4||(major==4&&minor>=6))anisotropy_=true;
        if(texture_barrier&&!force_copy)barrier_=reinterpret_cast<TextureBarrier>(SDL_GL_GetProcAddress("glTextureBarrier"));
        gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE,&max_texture_);gl_.GetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,&max_layers_);
        raster_=program(raster_vs,raster_fs);mask_=program(mask_vs,mask_fs);composite_=program(full_vs,composite_fs);resolve_=program(full_vs,resolve_fs);
        // Textureinheiten: 0 Punkttextur, 1 gefilterte Textur, 2 Setups/Dreiecke, 3..5 Lesesatz, 6 Masken.
        const auto unit=[&](GLuint p,const char* name,int u){gl_.UseProgram(p);const GLint at=gl_.GetUniformLocation(p,name);if(at>=0)gl_.Uniform1i(at,u);};
        unit(raster_,"u_point",0);unit(raster_,"u_tex",1);unit(raster_,"u_setups",2);unit(raster_,"u_below_color",3);unit(raster_,"u_below_depth",4);unit(raster_,"u_below_transported",5);
        unit(mask_,"u_tris",2);unit(mask_,"u_below",3);unit(mask_,"u_transported",4);
        unit(composite_,"u_below_color",3);unit(composite_,"u_below_depth",4);unit(composite_,"u_below_transported",5);unit(composite_,"u_mask",6);
        unit(resolve_,"u_color",3);unit(resolve_,"u_depth",4);
        gl_.GenVertexArrays(1,&vao_);gl_.BindVertexArray(vao_);
        for(int linear=0;linear<2;++linear)for(int cu=0;cu<2;++cu)for(int cv=0;cv<2;++cv){GLuint s=0;gl_.GenSamplers(1,&s);
            gl_.SamplerParameteri(s,GL_TEXTURE_MIN_FILTER,linear?GL_LINEAR:GL_LINEAR_MIPMAP_LINEAR);gl_.SamplerParameteri(s,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            gl_.SamplerParameteri(s,GL_TEXTURE_WRAP_S,cu?GL_CLAMP_TO_EDGE:GL_REPEAT);gl_.SamplerParameteri(s,GL_TEXTURE_WRAP_T,cv?GL_CLAMP_TO_EDGE:GL_REPEAT);
            if(anisotropy_)gl_.SamplerParameterf(s,GL_TEXTURE_MAX_ANISOTROPY,linear?1.f:4.f);
            samplers_[linear][cu][cv]=s;}
        {const std::uint32_t zero=0;blank_point_=make_texture();gl_.TexImage2D(GL_TEXTURE_2D,0,GLint(GL_R32UI),1,1,0,GL_RED_INTEGER,GL_UNSIGNED_INT,&zero);
         blank_=make_texture();gl_.TexImage2D(GL_TEXTURE_2D,0,GLint(GL_RGBA8),1,1,0,GL_BGRA,GL_UNSIGNED_BYTE,&zero);}
        check("initialisation");
    }
    ~OpenGlRaster()override=default; // der Kontext gibt alle Objekte mit sich frei (Context::~Context)
    std::string name()const override{return "OpenGL "+version_+" "+renderer_+(barrier_?" [texture barrier]":" [copy]");}
    const std::vector<std::string>& unsupported()const override{return unsupported_;}
    unsigned draw_calls()const override{return draw_calls_;}
    unsigned shadow_composites()const override{return composites_;}
    const Timings& timings()const override{return timings_;}
    bool render(const raster::Frame& frame,const raster::ShadowGeometry& shadows,std::vector<std::uint32_t>& pixels,std::vector<float>& depth)override{
        using Clock=std::chrono::steady_clock;
        const auto since=[](Clock::time_point from){return std::chrono::duration<double>(Clock::now()-from).count();};
        Current current(*ctx_);
        {std::vector<GLuint> dead;{std::lock_guard<std::mutex> lock(ctx_->mutex);dead.swap(ctx_->dead);}
         if(!dead.empty())gl_.DeleteTextures(GLsizei(dead.size()),dead.data());}
        const auto& setups=*frame.setups;const int S=frame.supersampling,W=frame.width*S,H=frame.height*S;
        draw_calls_=0;composites_=0;
        const auto encode_begin=Clock::now();
        state();
        if(W<=0||H<=0||W>max_texture_||H>max_texture_)throw std::runtime_error("OpenGL render target exceeds texture limits");
        targets(W,H);
        {const auto t=Clock::now();build(frame,W,H);timings_.build+=since(t);}
        upload_rows(setup_texture_,setup_rows_,setups_.data(),setups_.size(),setups_per_row,13);
        // A loeschen: Hintergrund, beide Tiefen 0.
        gl_.BindFramebuffer(GL_FRAMEBUFFER,fbo_a_);gl_.Viewport(0,0,W,H);
        {const GLuint bg[4]={frame.background,0,0,0};const GLfloat zero[4]={0,0,0,0};
         gl_.ClearBufferuiv(GL_COLOR,0,bg);gl_.ClearBufferfv(GL_COLOR,1,zero);gl_.ClearBufferfv(GL_COLOR,2,zero);}
        const std::size_t ends[3]={frame.shadows?frame.before_modifiers:setups.size(),frame.shadows?frame.before_mirror_effects:setups.size(),setups.size()};
        std::size_t group=0;
        for(unsigned phase=0;phase<3;++phase){
            gl_.UseProgram(raster_);
            gl_.Uniform2f(gl_.GetUniformLocation(raster_,"u_size"),float(W),float(H));
            gl_.Uniform1ui(gl_.GetUniformLocation(raster_,"u_fog_pixel"),frame.fog_pixel);
            gl_.Uniform1ui(gl_.GetUniformLocation(raster_,"u_wireframe"),frame.wireframe?1u:0u);
            gl_.Uniform1ui(gl_.GetUniformLocation(raster_,"u_background"),frame.background);
            bind(2,GL_TEXTURE_2D,setup_texture_);const auto* source=barrier_?a_:read_;
            bind(3,GL_TEXTURE_2D,source[0]);bind(4,GL_TEXTURE_2D,source[1]);bind(5,GL_TEXTURE_2D,source[2]);
            for(;group<groups_.size()&&groups_[group].first<ends[phase];++group){const auto& g=groups_[group];
                if(barrier_)barrier_();else copy(fbo_a_,fbo_r_,g.bounds,3);
                gl_.BindFramebuffer(GL_FRAMEBUFFER,fbo_a_);
                bind(0,GL_TEXTURE_2D,g.point?g.texture:blank_point_);bind(1,GL_TEXTURE_2D,g.point?blank_:g.texture);
                gl_.BindSampler(1,g.point?0:g.sampler);
                gl_.DrawArrays(GL_TRIANGLES,GLint(g.first*6),GLsizei(g.count*6));++draw_calls_;}
            if(phase<2&&frame.shadows&&frame.shadow_pass[phase]&&shadows){
                const auto works=shadows(phase==1);
                if(!works.empty()){masks(works,W,H);for(std::size_t i=0;i<works.size();++i)composite(works[i],i,W,H);}
            }
        }
        // Aufloesung in das Grundbild: S = 1 behaelt das Pixelalpha, sonst gerundeter RGB-Mittelwert, Alpha FF,
        // groesstes 1/z (renderer.cpp run).
        out_targets(frame.width,frame.height);
        gl_.BindFramebuffer(GL_FRAMEBUFFER,fbo_out_);gl_.Viewport(0,0,frame.width,frame.height);
        gl_.UseProgram(resolve_);gl_.Uniform1i(gl_.GetUniformLocation(resolve_,"u_scale"),S);
        bind(3,GL_TEXTURE_2D,a_[0]);bind(4,GL_TEXTURE_2D,a_[1]);
        gl_.DrawArrays(GL_TRIANGLES,0,3);++draw_calls_;
        timings_.encode+=since(encode_begin);
        const auto readback_begin=Clock::now();
        const std::size_t n=std::size_t(frame.width)*frame.height;pixels.resize(n);depth.resize(n);
        gl_.BindFramebuffer(GL_READ_FRAMEBUFFER,fbo_out_);gl_.PixelStorei(GL_PACK_ALIGNMENT,4);
        gl_.ReadBuffer(GL_COLOR_ATTACHMENT0);gl_.ReadPixels(0,0,frame.width,frame.height,GL_RED_INTEGER,GL_UNSIGNED_INT,pixels.data());
        gl_.ReadBuffer(GL_COLOR_ATTACHMENT0+1);gl_.ReadPixels(0,0,frame.width,frame.height,GL_RED,GL_FLOAT,depth.data());
        timings_.readback+=since(readback_begin);++timings_.submits;
        check("frame");
        return true;
    }
private:
    struct Group{std::size_t first,count;GLuint texture;bool point;GLuint sampler;Rect bounds;std::vector<Rect> boxes;};
    void check(const char* what){const GLenum e=gl_.GetError();if(e!=GL_NO_ERROR){char text[96];std::snprintf(text,sizeof text,"OpenGL error 0x%04x during %s",e,what);throw std::runtime_error(text);}}
    GLuint shader(GLenum kind,const char* body){
        const GLuint s=gl_.CreateShader(kind);const char* parts[2]={common_glsl,body};gl_.ShaderSource(s,2,parts,nullptr);gl_.CompileShader(s);
        GLint ok=0;gl_.GetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){GLint length=0;gl_.GetShaderiv(s,GL_INFO_LOG_LENGTH,&length);std::string log(std::size_t(std::max(length,1)),'\0');gl_.GetShaderInfoLog(s,length,nullptr,log.data());
            throw std::runtime_error("OpenGL shader: "+log);}
        return s;}
    GLuint program(const char* vs,const char* fs){
        const GLuint p=gl_.CreateProgram(),v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
        gl_.AttachShader(p,v);gl_.AttachShader(p,f);gl_.LinkProgram(p);gl_.DeleteShader(v);gl_.DeleteShader(f);
        GLint ok=0;gl_.GetProgramiv(p,GL_LINK_STATUS,&ok);
        if(!ok){GLint length=0;gl_.GetProgramiv(p,GL_INFO_LOG_LENGTH,&length);std::string log(std::size_t(std::max(length,1)),'\0');gl_.GetProgramInfoLog(p,length,nullptr,log.data());
            throw std::runtime_error("OpenGL program: "+log);}
        return p;}
    GLuint make_texture(GLenum target=GL_TEXTURE_2D){
        GLuint t=0;gl_.GenTextures(1,&t);gl_.ActiveTexture(GL_TEXTURE0+7);gl_.BindTexture(target,t);
        gl_.TexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_NEAREST);gl_.TexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        gl_.TexParameteri(target,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);gl_.TexParameteri(target,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        gl_.TexParameteri(target,GL_TEXTURE_BASE_LEVEL,0);gl_.TexParameteri(target,GL_TEXTURE_MAX_LEVEL,0);
        return t;}
    void bind(int unit,GLenum target,GLuint texture){gl_.ActiveTexture(GL_TEXTURE0+GLenum(unit));gl_.BindTexture(target,texture);}
    // Kein Festfunktionszustand darf das Ergebnis veraendern: Mischen, Tiefen-/Schablonentest, Dithering,
    // Mehrfachabtastung und sRGB-Wandlung bleiben aus; alles Mischen rechnet der Shader.
    void state(){
        for(GLenum e:{GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_DITHER,GL_SCISSOR_TEST,GL_MULTISAMPLE,GL_FRAMEBUFFER_SRGB})gl_.Disable(e);
        gl_.BindVertexArray(vao_);gl_.PixelStorei(GL_UNPACK_ALIGNMENT,4);}
    void surface(GLuint& t,GLenum format,GLenum external,GLenum type,int w,int h){
        if(!t)t=make_texture();bind(7,GL_TEXTURE_2D,t);gl_.TexImage2D(GL_TEXTURE_2D,0,GLint(format),w,h,0,external,type,nullptr);}
    GLuint framebuffer(const GLuint* textures,int count){
        GLuint f=0;gl_.GenFramebuffers(1,&f);gl_.BindFramebuffer(GL_FRAMEBUFFER,f);GLenum buffers[3]={};
        for(int i=0;i<count;++i){gl_.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+GLenum(i),GL_TEXTURE_2D,textures[i],0);buffers[i]=GL_COLOR_ATTACHMENT0+GLenum(i);}
        gl_.DrawBuffers(count,buffers);
        if(gl_.CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("OpenGL framebuffer incomplete");
        return f;}
    void targets(int W,int H){
        if(fbo_a_&&W==width_&&H==height_)return;
        const GLenum formats[3]={GL_R32UI,GL_R32F,GL_R32F},externals[3]={GL_RED_INTEGER,GL_RED,GL_RED},types[3]={GL_UNSIGNED_INT,GL_FLOAT,GL_FLOAT};
        for(int i=0;i<3;++i){surface(a_[i],formats[i],externals[i],types[i],W,H);surface(read_[i],formats[i],externals[i],types[i],W,H);}
        if(!fbo_a_){fbo_a_=framebuffer(a_,3);fbo_r_=framebuffer(read_,3);}
        width_=W;height_=H;}
    void out_targets(int w,int h){
        if(fbo_out_&&w==out_width_&&h==out_height_)return;
        surface(out_[0],GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT,w,h);surface(out_[1],GL_R32F,GL_RED,GL_FLOAT,w,h);
        if(!fbo_out_)fbo_out_=framebuffer(out_,2);
        out_width_=w;out_height_=h;}
    // Rechteck [x0,x1) x [y0,y1) jedes Anhangs von from nach to (gleiche Formate, keine Wandlung).
    void copy(GLuint from,GLuint to,const Rect& r,int attachments){
        if(r.x1<=r.x0||r.y1<=r.y0)return;
        gl_.BindFramebuffer(GL_READ_FRAMEBUFFER,from);gl_.BindFramebuffer(GL_DRAW_FRAMEBUFFER,to);
        for(int i=0;i<attachments;++i){GLenum buffers[3]={GL_NONE,GL_NONE,GL_NONE};buffers[i]=GL_COLOR_ATTACHMENT0+GLenum(i);
            gl_.ReadBuffer(buffers[i]);gl_.DrawBuffers(i+1,buffers);
            gl_.BlitFramebuffer(r.x0,r.y0,r.x1,r.y1,r.x0,r.y0,r.x1,r.y1,GL_COLOR_BUFFER_BIT,GL_NEAREST);}
        const GLenum all[3]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT0+1,GL_COLOR_ATTACHMENT0+2};gl_.DrawBuffers(attachments,all);}
    template<class T> void upload_rows(GLuint& texture,int& rows,const T* data,std::size_t count,int per_row,int texels){
        const int need=std::max(1,int((count+std::size_t(per_row)-1)/std::size_t(per_row)));
        if(need>max_texture_||per_row*texels>max_texture_)throw std::runtime_error("OpenGL: frame exceeds the maximum texture size");
        if(!texture||need>rows){rows=std::max(need,rows*2);if(rows>max_texture_)rows=need;
            surface(texture,GL_RGBA32UI,GL_RGBA_INTEGER,GL_UNSIGNED_INT,per_row*texels,rows);}
        staging_.assign(std::size_t(need)*std::size_t(per_row)*sizeof(T)/4,0u);
        if(count)std::memcpy(staging_.data(),data,count*sizeof(T));
        bind(7,GL_TEXTURE_2D,texture);gl_.TexSubImage2D(GL_TEXTURE_2D,0,0,0,per_row*texels,need,GL_RGBA_INTEGER,GL_UNSIGNED_INT,staging_.data());}
    GLuint texture(const SceneTexture& t,Variant variant){
        auto& cache=*t.filter_cache;const auto fingerprint=t.fingerprint();auto& slot=cache.gpu[variant];
        if(slot&&cache.gpu_owner[variant]==cache_owner()&&cache.gpu_fingerprint[variant]==fingerprint)return static_cast<GlTexture*>(slot.get())->id;
        const auto upload_begin=std::chrono::steady_clock::now();++timings_.uploads;
        const std::size_t levels=variant==Point?1:1+(variant==Punch?cache.punch_mips.size():cache.mips.size());
        GLuint id=make_texture();
        if(variant!=Point){gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(levels-1));gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
            gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
        unsigned w=t.width,h=t.height;std::vector<std::uint16_t> wide;
        for(std::size_t level=0;level<levels;++level){
            const std::uint32_t* src=level==0?t.pixels.data():(variant==Punch?cache.punch_mips[level-1].data():cache.mips[level-1].data());
            // Punkttextur unveraendert als 0xAARRGGBB (texelFetch liefert dieselben Bits wie die Bandstufe).
            if(variant==Point)gl_.TexImage2D(GL_TEXTURE_2D,0,GLint(GL_R32UI),GLsizei(w),GLsizei(h),0,GL_RED_INTEGER,GL_UNSIGNED_INT,src);
            else if(variant==Premultiplied){wide.resize(std::size_t(w)*h*4);
                for(std::size_t i=0;i<std::size_t(w)*h;++i){const auto p=src[i];const double a=double(p>>24)/255.0;
                    wide[i*4]=std::uint16_t(double((p>>16)&255)/255.0*a*65535.0+.5);wide[i*4+1]=std::uint16_t(double((p>>8)&255)/255.0*a*65535.0+.5);
                    wide[i*4+2]=std::uint16_t(double(p&255)/255.0*a*65535.0+.5);wide[i*4+3]=std::uint16_t(a*65535.0+.5);}
                gl_.TexImage2D(GL_TEXTURE_2D,GLint(level),GLint(GL_RGBA16),GLsizei(w),GLsizei(h),0,GL_RGBA,GL_UNSIGNED_SHORT,wide.data());}
            else gl_.TexImage2D(GL_TEXTURE_2D,GLint(level),GLint(GL_RGBA8),GLsizei(w),GLsizei(h),0,GL_BGRA,GL_UNSIGNED_BYTE,src);
            w=std::max(1u,w/2);h=std::max(1u,h/2);}
        auto* holder=new GlTexture{id,ctx_};
        slot=std::shared_ptr<void>(holder,[](void* p){auto* t=static_cast<GlTexture*>(p);
            if(auto owner=t->owner.lock()){std::lock_guard<std::mutex> lock(owner->mutex);owner->dead.push_back(t->id);}delete t;});
        cache.gpu_fingerprint[variant]=fingerprint;cache.gpu_owner[variant]=cache_owner();
        timings_.upload+=std::chrono::duration<double>(std::chrono::steady_clock::now()-upload_begin).count();
        return id;}
    void build(const raster::Frame& frame,int W,int H){
        const auto& setups=*frame.setups;setups_.resize(setups.size());groups_.clear();
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
            const bool point_texture=s.texture&&!s.filtered;
            const GLuint tex=s.texture?texture(*s.texture,variant):0;
            const GLuint sampler=point_texture?0:samplers_[s.linear_only?1:0][s.clamp_u?1:0][s.clamp_v?1:0];
            const Rect box{std::clamp(s.minx,0,W),std::clamp(s.miny,0,H),std::clamp(s.maxx+1,0,W),std::clamp(s.maxy+1,0,H)};
            // Eine Gruppe: gleiche Textur/Sampler, keine Phasengrenze (game.c:318/417) und paarweise disjunkte
            // Rechtecke - nur dann liest jedes Setup den Stand nach allen frueheren Setups.
            const bool phase_start=frame.shadows&&(i==frame.before_modifiers||i==frame.before_mirror_effects);
            bool join=!phase_start&&!groups_.empty();
            if(join){auto& last=groups_.back();
                join=last.texture==tex&&last.point==point_texture&&last.sampler==sampler&&last.boxes.size()<64;
                if(join)for(const auto& other:last.boxes)if(overlaps(other,box)){join=false;break;}}
            if(join){auto& last=groups_.back();++last.count;last.boxes.push_back(box);
                last.bounds={std::min(last.bounds.x0,box.x0),std::min(last.bounds.y0,box.y0),std::max(last.bounds.x1,box.x1),std::max(last.bounds.y1,box.y1)};continue;}
            groups_.push_back({i,1,tex,point_texture,sampler,box,{box}});
        }
    }
    // bhDrawMdfEffect: je Werk eine 640x480-Maske, Flaechen in der Reihenfolge von raster_shadow_mask
    // (erst positive, dann negative), jede mit Saettigung und & 0xf8 auf den Stand aller frueheren.
    void masks(const std::vector<raster::ShadowWork>& works,int W,int H){
        if(int(works.size())>max_layers_)throw std::runtime_error("too many shadow works in one pass");
        if(!mask_a_||int(works.size())>mask_layers_){
            mask_layers_=std::max<int>(int(works.size()),mask_layers_*2);mask_layers_=std::min(mask_layers_,max_layers_);
            if(!mask_a_)mask_a_=make_texture(GL_TEXTURE_2D_ARRAY);
            bind(7,GL_TEXTURE_2D_ARRAY,mask_a_);
            gl_.TexImage3D(GL_TEXTURE_2D_ARRAY,0,GLint(GL_R8UI),640,480,mask_layers_,0,GL_RED_INTEGER,GL_UNSIGNED_BYTE,nullptr);}
        if(!mask_r_){surface(mask_r_,GL_R8UI,GL_RED_INTEGER,GL_UNSIGNED_BYTE,640,480);fbo_mask_r_=framebuffer(&mask_r_,1);gl_.GenFramebuffers(1,&fbo_mask_a_);}
        tris_.clear();std::vector<std::pair<std::size_t,std::size_t>> ranges;
        for(const auto& work:works){const std::size_t first=tris_.size();
            for(const auto& f:work.faces){GpuShadowTri t{};
                t.v01[0]=f.x[0];t.v01[1]=f.y[0];t.v01[2]=f.x[1];t.v01[3]=f.y[1];
                t.v2a[0]=f.x[2];t.v2a[1]=f.y[2];t.v2a[2]=f.area;t.v2a[3]=0;
                t.depth[0]=f.depth[0];t.depth[1]=f.depth[1];t.depth[2]=f.depth[2];t.depth[3]=f.subtract?1u:0u;
                t.box[0]=f.minx;t.box[1]=f.miny;t.box[2]=f.maxx;t.box[3]=f.maxy;tris_.push_back(t);}
            ranges.emplace_back(first,tris_.size());}
        upload_rows(tris_texture_,tris_rows_,tris_.data(),tris_.size(),tris_per_row,4);
        gl_.Viewport(0,0,640,480);gl_.UseProgram(mask_);
        gl_.Uniform2ui(gl_.GetUniformLocation(mask_,"u_size"),unsigned(W),unsigned(H));
        bind(2,GL_TEXTURE_2D,tris_texture_);bind(3,GL_TEXTURE_2D,mask_r_);bind(4,GL_TEXTURE_2D,a_[2]);
        for(std::size_t layer=0;layer<works.size();++layer){
            gl_.BindFramebuffer(GL_FRAMEBUFFER,fbo_mask_a_);
            gl_.FramebufferTextureLayer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,mask_a_,0,GLint(layer));
            {const GLenum buffer=GL_COLOR_ATTACHMENT0;gl_.DrawBuffers(1,&buffer);}
            if(gl_.CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("OpenGL mask framebuffer incomplete");
            {const GLuint zero[4]={0,0,0,0};gl_.ClearBufferuiv(GL_COLOR,0,zero);}
            const auto [first,last]=ranges[layer];
            std::size_t i=first;
            while(i<last){
                // Gruppe paarweise disjunkter Flaechenrechtecke in Listenreihenfolge.
                std::vector<Rect> boxes;Rect bounds{640,480,0,0};std::size_t end=i;
                for(;end<last&&boxes.size()<64;++end){const auto& t=tris_[end];
                    const Rect box{std::clamp(t.box[0],0,640),std::clamp(t.box[1],0,480),std::clamp(t.box[2]+1,0,640),std::clamp(t.box[3]+1,0,480)};
                    bool clash=false;for(const auto& other:boxes)if(overlaps(other,box)){clash=true;break;}
                    if(clash)break;
                    boxes.push_back(box);bounds={std::min(bounds.x0,box.x0),std::min(bounds.y0,box.y0),std::max(bounds.x1,box.x1),std::max(bounds.y1,box.y1)};}
                copy(fbo_mask_a_,fbo_mask_r_,bounds,1);
                gl_.BindFramebuffer(GL_FRAMEBUFFER,fbo_mask_a_);
                gl_.DrawArrays(GL_TRIANGLES,GLint(i*6),GLsizei((end-i)*6));++draw_calls_;
                i=end;}
        }
        gl_.Viewport(0,0,W,H);
    }
    void composite(const raster::ShadowWork& c,std::size_t layer,int W,int H){
        if(c.fog_power>255u)throw std::runtime_error("shadow: invalid fog channel");
        copy(fbo_a_,fbo_r_,{0,0,W,H},3);
        gl_.BindFramebuffer(GL_FRAMEBUFFER,fbo_a_);gl_.Viewport(0,0,W,H);gl_.UseProgram(composite_);
        gl_.Uniform2f(gl_.GetUniformLocation(composite_,"u_size"),float(W),float(H));
        gl_.Uniform4f(gl_.GetUniformLocation(composite_,"u_rect"),c.rx0,c.ry0,c.rx1,c.ry1);
        gl_.Uniform1ui(gl_.GetUniformLocation(composite_,"u_fog"),c.fog_power);
        gl_.Uniform1i(gl_.GetUniformLocation(composite_,"u_layer"),GLint(layer));
        bind(3,GL_TEXTURE_2D,read_[0]);bind(4,GL_TEXTURE_2D,read_[1]);bind(5,GL_TEXTURE_2D,read_[2]);bind(6,GL_TEXTURE_2D_ARRAY,mask_a_);
        gl_.DrawArrays(GL_TRIANGLES,0,3);++draw_calls_;++composites_;}
    using TextureBarrier=void(RECVX_GLAPI*)(void);
    TextureBarrier barrier_=nullptr;
    std::shared_ptr<Context> ctx_;Gl& gl_;std::string renderer_,version_;bool anisotropy_=false;GLint max_texture_=0,max_layers_=0;
    GLuint raster_=0,mask_=0,composite_=0,resolve_=0,vao_=0,samplers_[2][2][2]{},blank_=0,blank_point_=0;
    GLuint a_[3]{},read_[3]{},out_[2]{},fbo_a_=0,fbo_r_=0,fbo_out_=0,setup_texture_=0,tris_texture_=0;
    GLuint mask_a_=0,mask_r_=0,fbo_mask_a_=0,fbo_mask_r_=0;int mask_layers_=0;
    int width_=0,height_=0,out_width_=0,out_height_=0,setup_rows_=0,tris_rows_=0;
    std::vector<GpuSetup> setups_;std::vector<Group> groups_;std::vector<GpuShadowTri> tris_;std::vector<std::uint32_t> staging_;
    std::vector<std::string> unsupported_;unsigned draw_calls_=0,composites_=0;Timings timings_;
};
}
std::shared_ptr<GpuRaster> create_opengl_raster(std::string& reason,bool force_copy){
    auto context=std::make_shared<Context>();
    SDL_SetMainReady();
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)!=0){reason=std::string("OpenGL: no video (")+SDL_GetError()+")";return nullptr;}
    context->video=true;
    // Erzeugungsattribute nur fuer den eigenen Kontext; die vorherigen Werte bleiben fuer andere SDL-Nutzer.
    const SDL_GLattr attributes[]={SDL_GL_CONTEXT_MAJOR_VERSION,SDL_GL_CONTEXT_MINOR_VERSION,SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_FLAGS,SDL_GL_SHARE_WITH_CURRENT_CONTEXT};
    int saved[5]{};for(int i=0;i<5;++i)SDL_GL_GetAttribute(attributes[i],&saved[i]);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT,0);
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,0);
#endif
    SDL_Window* const previous_window=SDL_GL_GetCurrentWindow();const SDL_GLContext previous_context=SDL_GL_GetCurrentContext();
#if !defined(__APPLE__)
    // Moderner Kontext fuer den TextureBarrier-Pfad; GL4.1 bleibt der gemeinsame Mindestumfang.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,5);
#endif
    context->window=SDL_CreateWindow("RECVX OpenGL raster",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(context->window)context->context=SDL_GL_CreateContext(context->window);
#if !defined(__APPLE__)
    if(!context->context){
        if(context->window)SDL_DestroyWindow(context->window);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
        context->window=SDL_CreateWindow("RECVX OpenGL raster",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(context->window)context->context=SDL_GL_CreateContext(context->window);
    }
#endif
    const std::string error=SDL_GetError();
    for(int i=0;i<5;++i)SDL_GL_SetAttribute(attributes[i],saved[i]);
    if(!context->window||!context->context){SDL_GL_MakeCurrent(previous_context?previous_window:nullptr,previous_context);reason="OpenGL 4.1 core context unavailable: "+error;return nullptr;}
    try{
        {Current current(*context);context->gl.load();}
        SDL_GL_MakeCurrent(previous_context?previous_window:nullptr,previous_context);
        return std::make_shared<OpenGlRaster>(context,force_copy);
    }catch(const std::exception& e){SDL_GL_MakeCurrent(previous_context?previous_window:nullptr,previous_context);reason=e.what();return nullptr;}
}
}
