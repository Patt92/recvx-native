#include "renderer.h"
#include "chunk_blend.h"
#include "shadow_raster.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace recvx {
namespace {
using raster::Projected;using raster::Setup;
float edge(const Projected& a,const Projected& b,float x,float y){return(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
std::uint32_t shade(const SceneTriangle& t){
    if(t.world_lit)return t.color;
    NJS_VECTOR a{t.vertices[1].position.x-t.vertices[0].position.x,t.vertices[1].position.y-t.vertices[0].position.y,t.vertices[1].position.z-t.vertices[0].position.z};
    NJS_VECTOR b{t.vertices[2].position.x-t.vertices[0].position.x,t.vertices[2].position.y-t.vertices[0].position.y,t.vertices[2].position.z-t.vertices[0].position.z},n;
    njOuterProduct(&a,&b,&n);njUnitVector(&n);
    const float light=0.30f+0.70f*std::abs(n.x*0.267f+n.y*0.802f+n.z*0.535f);
    auto channel=[&](unsigned shift){return static_cast<unsigned>(((t.color>>shift)&255)*light);};
    return (t.color&0xff000000)|(channel(16)<<16)|(channel(8)<<8)|channel(0);
}
// ---- Texture filtering -------------------------------------------------
// Mip chain: 2x2 box filter weighted by alpha so transparent texels never darken
// the colour of cut-outs. Alpha of the chain is the plain average (coverage).
void build_mips(const SceneTexture& t){
    if(t.filter_cache->ready)t.invalidate_mips();auto& cache=*t.filter_cache;
    cache.mips.clear();cache.punch_mips.clear();cache.max_alpha=0;std::uint32_t min_alpha=255;
    for(const auto p:t.pixels){const auto a=p>>24;cache.max_alpha=std::max(cache.max_alpha,a);min_alpha=std::min(min_alpha,a);}
    cache.cutout=min_alpha==0&&cache.max_alpha>0;cache.uniform_alpha=min_alpha==cache.max_alpha&&min_alpha>0;
    const std::uint32_t* src=t.pixels.data();unsigned w=t.width,h=t.height;
    while(w>1||h>1){
        const unsigned nw=std::max(1u,w/2),nh=std::max(1u,h/2);std::vector<std::uint32_t> next(std::size_t(nw)*nh);
        for(unsigned y=0;y<nh;++y)for(unsigned x=0;x<nw;++x){
            unsigned sum_a=0;float r=0,g=0,b=0;
            for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){
                const auto p=src[std::size_t(std::min(h-1,y*2+dy))*w+std::min(w-1,x*2+dx)];const unsigned a=p>>24;
                sum_a+=a;r+=float(p&255)*a;g+=float((p>>8)&255)*a;b+=float((p>>16)&255)*a;}
            std::uint32_t out=0;
            if(sum_a){const float inv=1.f/float(sum_a);out=(std::uint32_t(sum_a/4)<<24)|(std::uint32_t(b*inv+.5f)<<16)|(std::uint32_t(g*inv+.5f)<<8)|std::uint32_t(r*inv+.5f);}
            next[std::size_t(y)*nw+x]=out;
        }
        cache.mips.push_back(std::move(next));src=cache.mips.back().data();w=nw;h=nh;
    }
    cache.fingerprint=t.fingerprint();cache.ready=true;
}
// Only filtered Punch draws that can sample mip levels need this second RGB chain.
// Called after fingerprint validation in the serial setup pass, never by a band worker.
void build_punch_mips(const SceneTexture& t){
    auto& cache=*t.filter_cache;
    const std::uint32_t* src=t.pixels.data();unsigned w=t.width,h=t.height;
    while(w>1||h>1){
        const unsigned nw=std::max(1u,w/2),nh=std::max(1u,h/2);
        std::vector<std::uint32_t> next(std::size_t(nw)*nh);
        for(unsigned y=0;y<nh;++y)for(unsigned x=0;x<nw;++x){
            unsigned a=0,r=0,g=0,b=0;
            for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){
                const auto p=src[std::size_t(std::min(h-1,y*2+dy))*w+std::min(w-1,x*2+dx)];
                a+=p>>24;r+=p&255;g+=(p>>8)&255;b+=(p>>16)&255;}
            next[std::size_t(y)*nw+x]=(a/4)<<24|((b+2)/4)<<16|((g+2)/4)<<8|(r+2)/4;
        }
        cache.punch_mips.push_back(std::move(next));src=cache.punch_mips.back().data();w=nw;h=nh;
    }
}
// Fast log2 for mip selection: exponent + quadratic fit of the mantissa (|error| < 0.01).
inline float fast_log2(float x){
    std::uint32_t bits;std::memcpy(&bits,&x,4);const float e=float(int(bits>>23)-127);
    bits=(bits&0x7fffffu)|0x3f800000u;float m;std::memcpy(&m,&bits,4);
    return e+(-0.34484843f*m+2.02466578f)*m-0.67487759f;
}
struct Level {const std::uint32_t* pixels;unsigned w,h;bool clamp_u,clamp_v;};
// clamp_u/clamp_v: GS CLAMP_1 dieses Zeichenaufrufs (Texturheader oder OT-Cache, ps2_dummy.c:1420/1650).
Level level_of(const SceneTexture& t,unsigned level,bool clamp_u,bool clamp_v,bool punch=false){
    if(level==0)return {t.pixels.data(),t.width,t.height,clamp_u,clamp_v};
    return {(punch?t.filter_cache->punch_mips:t.filter_cache->mips)[level-1].data(),std::max(1u,t.width>>level),std::max(1u,t.height>>level),clamp_u,clamp_v};
}
// Premultiplied accumulation: acc = {r*a, g*a, b*a, a} scaled by weight.
// Inlined into the pixel loop: the call sat on the hot path (sample profile, 45 % of a
// frame) and the arithmetic order is unchanged, so every pixel stays bit-identical.
#if defined(_MSC_VER)
#define RECVX_RENDER_INLINE __forceinline
#else
#define RECVX_RENDER_INLINE inline __attribute__((always_inline))
#endif
template<bool Weighted,bool Straight=false>
RECVX_RENDER_INLINE void bilinear(const Level& l,float u,float v,float weight,float* acc){
    const float fx=u*float(l.w)-.5f,fy=v*float(l.h)-.5f;
    const int ix=int(fx)-(fx<0),iy=int(fy)-(fy<0);const float tx=fx-float(ix),ty=fy-float(iy);
    const int w=int(l.w),h=int(l.h);
    // GS REPEAT: power-of-two sizes wrap with a mask, other sizes fall back to modulo.
    // GS CLAMP haelt die Texelkoordinate am Rand (beide Bilinearnachbarn bleiben innen).
    auto wrap=[](int i,int n,bool clamp){return clamp?std::clamp(i,0,n-1):(n&(n-1))==0?(i&(n-1)):(((i%n)+n)%n);};
    const int x0=wrap(ix,w,l.clamp_u),x1=wrap(ix+1,w,l.clamp_u),y0=wrap(iy,h,l.clamp_v),y1=wrap(iy+1,h,l.clamp_v);
    const std::uint32_t* r0=l.pixels+std::size_t(y0)*l.w;const std::uint32_t* r1=l.pixels+std::size_t(y1)*l.w;
    const std::uint32_t texel[4]={r0[x0],r0[x1],r1[x0],r1[x1]};
    const float k[4]={weight*(1-tx)*(1-ty),weight*tx*(1-ty),weight*(1-tx)*ty,weight*tx*ty};
    for(int i=0;i<4;++i){
        const auto p=texel[i];float c=k[i];
        if constexpr(Straight){acc[3]+=c*float(p>>24);c*=255.f;acc[4]+=c;}
        else if constexpr(Weighted){const float a=float(p>>24);acc[3]+=c*a;c*=a;}else{c*=255.f;acc[3]+=c;} // constant alpha cancels from RGB; coverage is applied separately
        acc[0]+=c*float(p&255);acc[1]+=c*float((p>>8)&255);acc[2]+=c*float((p>>16)&255);
    }
}
SceneVertex lerp_vertex(const SceneVertex& a,const SceneVertex& b,float t){
    auto mix=[t](float x,float y){return x+(y-x)*t;};
    return {{mix(a.position.x,b.position.x),mix(a.position.y,b.position.y),mix(a.position.z,b.position.z)},mix(a.u,b.u),mix(a.v,b.v),{},{mix(a.light.x,b.light.x),mix(a.light.y,b.light.y),mix(a.light.z,b.light.z)},mix(a.alpha,b.alpha)};
}
// Base colour scaled by an interpolated per-vertex light factor (GS Gouraud: screen-linear).
// njSetFogColor haelt 0x00RRGGBB, die Leinwand und SceneTexture sind 0xAARRGGBB.
std::uint32_t fog_canvas_color(std::uint32_t rgb){return 0xff000000u|(rgb&0xffffffu);}
std::uint32_t lit(std::uint32_t base,float r,float g,float b){
    auto ch=[&](unsigned shift,float k){return std::uint32_t(std::clamp(float((base>>shift)&255)*k+.5f,0.f,255.f));};
    return (base&0xff000000)|(ch(16,r)<<16)|(ch(8,g)<<8)|ch(0,b);
}
std::uint32_t modulate(std::uint32_t texture,std::uint32_t light,unsigned divisor) {
    unsigned result=0xff000000;
    for(unsigned shift:{0u,8u,16u})result|=std::min(255u,(((texture>>shift)&255)*((light>>shift)&255))/divisor)<<shift;
    return result;
}
}
SoftwareRenderer::SoftwareRenderer(int width,int height):width_(width),height_(height){
    if(width<=0||height<=0||width>4096||height>4096)throw std::runtime_error("invalid render dimensions");
    pixels_.resize(std::size_t(width)*height);depth_.resize(pixels_.size());
}
void SoftwareRenderer::draw(const Scene& scene,const SceneCamera& camera,bool wireframe){
    // bhControlCamera order: initial Ninja view, dolly, Z/X/Y, camera translation.
    NJS_MATRIX view;camera_view_matrix(camera,view);
    const float focal=(width_*0.5f)/std::tan(0.0000958738f*(camera.perspective/2));
    const float aspect_x=1.174f;
    const float focal_y=focal*(height_/480.0f)/(width_/640.0f);
    // Ninja-Bildschirm 640x480: fSx = AspectW*x*dist/z, fSy = AspectH*y*dist/z (ps2_NinjaCnk.c:1195).
    ninja_sx_=focal*aspect_x*640.0f/float(width_);ninja_sy_=focal_y*480.0f/float(height_);
    draw_view(scene,view,focal*aspect_x,focal_y,wireframe,camera.hidden_objects,camera.clip_near,camera.clip_far,camera.clip_volume_x,camera.clip_volume_y);
}
void SoftwareRenderer::draw_screen(const Scene& scene,const SceneCamera& camera,float dist,float aspect_y){
    NJS_MATRIX view;camera_view_matrix(camera,view);ninja_sx_=ninja_sy_=0;
    draw_view(scene,view,1.174f*dist,aspect_y*dist,false,camera.hidden_objects,camera.clip_near,camera.clip_far,camera.clip_volume_x,camera.clip_volume_y);
}
void SoftwareRenderer::draw_full_screen_source(const Scene& scene,const SceneCamera& camera,float aspect_y,bool wireframe){
    NJS_MATRIX view;camera_view_matrix(camera,view);
    const float dist=0.8f*(320.0f/std::tan(0.0000958738f*(camera.perspective/2)));
    ninja_sx_=1.174f*dist;ninja_sy_=aspect_y*dist;
    draw_view_projected(scene,view,ninja_sx_,ninja_sy_,wireframe,camera.hidden_objects,camera.clip_near,camera.clip_far,camera.clip_volume_x,camera.clip_volume_y,dist);
}
unsigned strip_clip_code(const NJS_POINT3& v,float projection,float volume_x,float volume_y,float near,float far){
    const float w=std::fabs(v.z),cx=v.x*projection/volume_x,cy=v.y*projection/volume_y;
    const float cz=v.z*(far+near)/(far-near)-2.0f*far*near/(far-near);
    return unsigned(cx>w)|unsigned(cx<-w)<<1|unsigned(cy>w)<<2|unsigned(cy<-w)<<3|unsigned(cz>w)<<4|unsigned(cz<-w)<<5;}
void SoftwareRenderer::draw_view(const Scene& scene,const NJS_MATRIX& matrix,float focal_x,float focal_y,bool wireframe,const std::array<std::uint32_t,16>& hidden,float near,float far,float volume_x,float volume_y){
    draw_view_projected(scene,matrix,focal_x,focal_y,wireframe,hidden,near,far,volume_x,volume_y,ninja_sy_);
}
void SoftwareRenderer::draw_view_projected(const Scene& scene,const NJS_MATRIX& matrix,float focal_x,float focal_y,bool wireframe,const std::array<std::uint32_t,16>& hidden,float near,float far,float volume_x,float volume_y,float ninja_projection){
    last_gpu_frame_=false;
    if(discard_)return;
    using Clock=std::chrono::steady_clock;
    const auto since=[](Clock::time_point from){return std::chrono::duration<double>(Clock::now()-from).count();};
    const auto frame_begin=Clock::now();++timings_.frames;
    // Supersampling renders into a larger buffer and box-filters it down (steadier edges).
    const int S=std::clamp(ss_,1,4),W=width_*S,H=height_*S;
    // Zielpuffer der Bandstufe; geloescht erst, wenn feststeht, dass Software zeichnet (GPU-Pfad: gpu_raster.h).
    std::uint32_t* color=pixels_.data();float* depth=depth_.data();
    const auto clear_targets=[&]{
        if(S>1){hi_pixels_.assign(std::size_t(W)*H,scene.background);hi_depth_.assign(std::size_t(W)*H,0.0f);color=hi_pixels_.data();depth=hi_depth_.data();}
        else{std::fill(pixels_.begin(),pixels_.end(),scene.background);std::fill(depth_.begin(),depth_.end(),0.0f);}};
    NJS_MATRIX view;std::copy_n(matrix,16,view);
    // game.c:81-83 / MdlPut.c:59-61: njSetMatrix(cam.mtxb, cam.mtx); njMirror(cam.mtx, &sys->mr_pl).
    // Dreiecke des Spiegelpasses zeichnen mit cam.mtx * mat, alle anderen mit cam.mtx.
    NJS_MATRIX mirror_view;std::copy_n(view,16,mirror_view);
    if(scene.mirror_enabled){NJS_MATRIX mirror;std::copy_n(scene.mirror_matrix.data(),16,mirror);njMultiMatrix(&mirror_view,&mirror);}
    const auto view_of=[&](const SceneTriangle& t)->NJS_MATRIX*{
        if(!t.mirror)return &view;
        if(!scene.mirror_enabled)throw std::runtime_error("mirror pass triangle without njMirror matrix");
        return &mirror_view;};
    near=std::max(near,0.001f);
    last_clip_={{near,far}};
    std::vector<Setup> setups;setups.reserve(scene.triangles.size());
    // F05: receivers carry the same transported vertex depth as the modifier
    // (Ps2AddPrim3D*/Mod: clamp(-a-b*q), integer vertex Z, screen-linear).
    std::optional<ShadowDepthCoefficients> zc;
    if(!scene.shadows.empty()){
        if(!(far>near))throw std::runtime_error("shadow: finite njClipZ range required for transported depth");
        zc=shadow_depth_coefficients(-near,-far);
    }
    // Chunk-Streifen (SceneTriangle::strip_clip) folgen der Clipregel der Ninja-Streifen, nur
    // im Ninja-Bildschirm von draw(). Bezug: ClipMatrix2 (_Make_ClipMatrix, ps2_NaView.c:233)
    // mit dem ClipVolume der Kamera (_Make_ClipVolume, ps2_NaView.c:298; Grundwert 320x240,
    // ps2_NaView.c:24) und njClipZ-Grenzen near >= 1 / far <= 65535
    // (ps2_NaView.c:142; Grundwert 65535, ps2_NaView.c:21).
    const float ninja_sx=ninja_sx_,ninja_sy=ninja_sy_;ninja_sx_=ninja_sy_=0;
    const bool strip_rule=ninja_sx>0&&ninja_sy>0;
    const float strip_near=std::max(near,1.0f),strip_far=far>near?std::min(far,65535.0f):65535.0f;
    // vclipw.xyz gegen ClipMatrix2: Bits +x 1, -x 2, +y 4, -y 8, +z (fern) 16, -z (nah) 32.
    // ClipMatrix2 liest dist, nicht den von bhEff110 auf 1.25 gesetzten Y-Aspekt.
    const auto clip_code=[&](const NJS_POINT3& v){return strip_clip_code(v,ninja_projection,volume_x,volume_y,strip_near,strip_far);};
    // _Check_DisplayAreaPoint (ps2_Vu1Strip.c:766) mit dem Bildschirmversatz 2048
    // (fNaViwOffsetX/Y = cx+1728/cy+1808, ps2_NaView.c:39) und der Sicht-z (pP->fZ = fVz).
    const auto outside_display=[&](const NJS_POINT3& v){
        if(!(v.z>=1.0f&&v.z<65535.0f))return true;
        const float x=2048.0f+v.x*ninja_sx/v.z,y=2048.0f+v.y*ninja_sy/v.z;
        return !(x>=1.0f&&x<4095.0f&&y>=1.0f&&y<4095.0f);};
    // njCnkCsUvh/njCnkCsUvn verwerfen ein Paket von hoechstens 60 Eckpunkten, wenn alle seine
    // Eckpunkte ausserhalb derselben Ebene liegen (UND der vclipw-Bits, ps2_NinjaCnk.c:2045-2104
    // auf dem VU1-Weg, _CVV ps2_NinjaCnk.c:2240-2266 auf dem EE-Weg).
    std::vector<std::uint8_t> strip_reject;
    if(strip_rule){strip_reject.assign(scene.triangles.size(),0);
        for(std::size_t i=0;i<scene.triangles.size();){
            const auto id=scene.triangles[i].strip_segment;
            if(!id||!scene.triangles[i].strip_clip){++i;continue;}
            std::size_t j=i;unsigned code=63;
            for(;j<scene.triangles.size()&&scene.triangles[j].strip_segment==id;++j)
                for(const auto& vertex:scene.triangles[j].vertices){NJS_POINT3 source=vertex.position,p;njCalcPoint(view_of(scene.triangles[j]),&source,&p);code&=clip_code(p);}
            if(code)std::fill(strip_reject.begin()+std::ptrdiff_t(i),strip_reject.begin()+std::ptrdiff_t(j),std::uint8_t(1));
            i=j;}}
    // Effektaufrufe (SceneTriangle::ninja_primitive). Ps2AddPrim3D (ps2_dummy.c:486): vf7.xy =
    // Bildpunkt - 2048, vf7.z = z + 0.062501907*z - 2048.0625, vclipw.xyz gegen 2047; ein
    // gesetztes Bit in den letzten drei Eckpunkten (Maske 0x3FFFF) gibt dem Streifendreieck ADC.
    const auto ninja_vertex_out=[&](const NJS_POINT3& v){
        if(std::fabs(v.z+0.062501907f*v.z-2048.0625f)>2047.0f)return true;
        return std::fabs(v.x*ninja_sx/v.z)>2047.0f||std::fabs(v.y*ninja_sy/v.z)>2047.0f;};
    // njDrawTextureSub3D (ps2_NaDraw.c:302): scr.z < 0 eines Eckpunkts beendet den Aufruf; cflag
    // beginnt mit 15 und behaelt die x/y-Bits von _Clip_Screen (ps2_Vu1Strip.c:1202:
    // (x-2048)*0.75 und y-2048 gegen 240), die allen Eckpunkten gemeinsam sind.
    std::vector<std::uint8_t> ninja_reject;
    if(strip_rule){ninja_reject.assign(scene.triangles.size(),0);
        for(std::size_t i=0;i<scene.triangles.size();){
            const auto id=scene.triangles[i].ninja_primitive;
            if(!id){++i;continue;}
            std::size_t j=i;bool behind=false;unsigned code=15;
            for(;j<scene.triangles.size()&&scene.triangles[j].ninja_primitive==id;++j)
                for(const auto& vertex:scene.triangles[j].vertices){NJS_POINT3 source=vertex.position,p;njCalcPoint(view_of(scene.triangles[j]),&source,&p);
                    if(p.z<0){behind=true;continue;}
                    const float x=(p.x*ninja_sx/p.z)*0.75f,y=p.y*ninja_sy/p.z;
                    code&=unsigned(x>240.0f)|unsigned(x<-240.0f)<<1|unsigned(y>240.0f)<<2|unsigned(y<-240.0f)<<3;}
            if(scene.triangles[i].ninja_screen_reject&&(behind||code))std::fill(ninja_reject.begin()+std::ptrdiff_t(i),ninja_reject.begin()+std::ptrdiff_t(j),std::uint8_t(1));
            i=j;}}
    for(std::size_t triangle_index=0;triangle_index<scene.triangles.size();++triangle_index){
        const auto& triangle=scene.triangles[triangle_index];
        // bhCkFlg numbers bits from the most significant bit.
        // bhSetHideObject(offset): hidobj ab Wort offset (cut.c:620/641, Spiegelpass 4).
        if(triangle.camera_mask && triangle.object/32+triangle.hidden_offset<16 && (hidden[triangle.object/32+triangle.hidden_offset] & (0x80000000u>>(triangle.object%32))))continue;
        std::vector<SceneVertex> polygon;polygon.reserve(4);
        NJS_MATRIX* const triangle_view=view_of(triangle);
        if(triangle.single_sided){bool back=true;for(const auto& vertex:triangle.vertices){NJS_VECTOR n=vertex.normal,c;njCalcVector(triangle_view,&n,&c);back=back&&0<c.z;}if(back)continue;}
        for(auto vertex:triangle.vertices){njCalcPoint(triangle_view,&vertex.position,&vertex.position);polygon.push_back(vertex);}
        float clip_near=near,clip_far=far;bool clip_z=true;
        if(strip_rule&&triangle.strip_clip){
            if(strip_reject[triangle_index])continue;
            // vu1DrawTriangleStrip* (ps2_Vu1Strip.c:943/1057/1178/1420, VU1 DoubleMain/SingleMain):
            // ein Dreieck geht unverändert in den Streifen, solange keiner seiner Eckpunkte den
            // Anzeigebereich verlässt oder keiner ausserhalb des Clipvolumens liegt; sonst wird es
            // gegen die sechs Ebenen geschnitten (_Check_ScissorPlane, ELF 0x002D3EB0: nah, fern,
            // -x, +x, -y, +y; die Seiten begrenzt hier das Raster). Auch jenseits von njClipZ-fern
            // liegende Dreiecke im Anzeigebereich werden daher ganz gezeichnet.
            bool flagged=false,outside=false;
            for(const auto& v:polygon){flagged=flagged||outside_display(v.position);outside=outside||clip_code(v.position)!=0;}
            if(!flagged||!outside)clip_z=false;else{clip_near=strip_near;clip_far=strip_far;}
        }
        // Effektaufruf: keine njClipZ-Klippung; ein Dreieck mit einem Clipbit-Eckpunkt fehlt ganz.
        if(strip_rule&&triangle.ninja_primitive){
            if(ninja_reject[triangle_index])continue;
            bool flagged=false;for(const auto& v:polygon)flagged=flagged||ninja_vertex_out(v.position);
            if(flagged)continue;
            clip_z=false;
        }
        // Clip in camera space before division; surfaces crossing the near plane remain visible.
        std::vector<SceneVertex> clipped;clipped.reserve(4);
        if(!clip_z)clipped=polygon;
        else for(std::size_t i=0;i<polygon.size();++i){
            const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
            const bool inside=a.position.z>=clip_near,next=b.position.z>=clip_near;
            if(inside)clipped.push_back(a);
            if(inside!=next){const float t=(clip_near-a.position.z)/(b.position.z-a.position.z);clipped.push_back(lerp_vertex(a,b,t));clipped.back().position.z=clip_near;}
        }
        // Far plane (njClipZ far) clipped the same way.
        if(clip_z&&clip_far>clip_near&&clipped.size()>=3){std::vector<SceneVertex> inner;inner.reserve(clipped.size()+1);
            for(std::size_t i=0;i<clipped.size();++i){const auto a=clipped[i],b=clipped[(i+1)%clipped.size()];const bool inside=a.position.z<=clip_far,next=b.position.z<=clip_far;
                if(inside)inner.push_back(a);
                if(inside!=next){const float t=(clip_far-a.position.z)/(b.position.z-a.position.z);inner.push_back(lerp_vertex(a,b,t));inner.back().position.z=clip_far;}}
            clipped=std::move(inner);}
        if(clipped.size()<3)continue;
        const auto color=shade(triangle);
        const auto* texture=triangle.textured && triangle.texture<scene.textures.size()?&scene.textures[triangle.texture]:nullptr;
        for(std::size_t fan=1;fan+1<clipped.size();++fan){
            std::array<Projected,3> p;
            const std::array<SceneVertex,3> v{clipped[0],clipped[fan],clipped[fan+1]};
            bool finite=true;
            for(int i=0;i<3;++i){const auto point=v[i].position;p[i]={W*0.5f+point.x*(focal_x*float(S))/point.z,H*0.5f+point.y*(focal_y*float(S))/point.z,1/point.z,v[i].u/point.z,v[i].v/point.z,zc?float(shadow_vertex_depth(*zc,1/point.z)):0.f};finite=finite&&std::isfinite(p[i].x)&&std::isfinite(p[i].y);}
            if(!finite)continue;
            float area=edge(p[0],p[1],p[2].x,p[2].y);
            if(std::abs(area)<1e-6f||!std::isfinite(area))continue;
            const bool area_swapped=area<0;if(area<0){std::swap(p[1],p[2]);area=-area;}
            const float minxf=std::max(0.0f,std::min({p[0].x,p[1].x,p[2].x}));
            const float maxxf=std::min(float(W-1),std::max({p[0].x,p[1].x,p[2].x}));
            const float minyf=std::max(0.0f,std::min({p[0].y,p[1].y,p[2].y}));
            const float maxyf=std::min(float(H-1),std::max({p[0].y,p[1].y,p[2].y}));
            if(minxf>maxxf||minyf>maxyf)continue;
            const int minx=int(minxf),maxx=int(maxxf),miny=int(minyf),maxy=int(maxyf);
            // Screen-space gradients of the barycentric weights (constant per triangle),
            // used to derive perspective-correct texture derivatives per pixel.
            const float dax=-(p[2].y-p[1].y)/area,day=(p[2].x-p[1].x)/area,dbx=-(p[0].y-p[2].y)/area,dby=(p[0].x-p[2].x)/area,dcx=-(dax+dbx),dcy=-(day+dby);
            Setup setup;setup.p=p;setup.area=area;setup.minx=minx;setup.maxx=maxx;setup.miny=miny;setup.maxy=maxy;setup.color=color;setup.blend=triangle.blend;setup.depth_write=triangle.depth_write;setup.punch_through=triangle.punch_through;setup.texture_color_divisor=triangle.texture_color_divisor;setup.texture=texture&&!texture->pixels.empty()?texture:nullptr;
            if(setup.texture){setup.clamp_u=texture->clamp_u||triangle.texture_cache_clamp;
                setup.clamp_v=texture->clamp_v||triangle.texture_cache_clamp;}
            setup.blend_register=triangle.blend_register;setup.post=triangle.after_modifiers;
            setup.general_water=triangle.general_purpose_water;setup.mirror_room=triangle.mirror==2;setup.mirror_effect=triangle.mirror==3;
            setup.filtered=triangle.texture_filter==SceneTextureFilter::Linear||(triangle.texture_filter==SceneTextureFilter::Default&&filter_);
            setup.linear_only=triangle.texture_filter==SceneTextureFilter::Linear;
            if(triangle.gouraud&&triangle.world_lit){setup.gouraud=true;setup.light={v[0].light,v[1].light,v[2].light};if(area_swapped)std::swap(setup.light[1],setup.light[2]);}
            if(triangle.vertex_alpha){setup.vertex_alpha=true;setup.alpha={v[0].alpha,v[1].alpha,v[2].alpha};if(area_swapped)std::swap(setup.alpha[1],setup.alpha[2]);}
            // njDraw* primitives (effects) choose FGE per call (ps2_NaDraw.c:351/355); only the CNK strips are fogged here.
            if(triangle.fog_color_set){setup.fog_color_set=true;setup.fog_color=fog_canvas_color(triangle.fog_color);}
            if(triangle.fog_fixed){setup.fogged=true;setup.fog={triangle.fog_fixed_value,triangle.fog_fixed_value,triangle.fog_fixed_value};} // XYZF2.F je Eckpunkt fest (ps2_NaDraw.c:443 njDrawTextureSub3DH), ohne Ninja-Nebelzustand
            else if(scene.fog.enabled&&triangle.world_lit&&triangle.fogged){setup.fogged=true;for(int i=0;i<3;++i)setup.fog[i]=fog_sample(scene.fog,v[i].position.z);if(area_swapped)std::swap(setup.fog[1],setup.fog[2]);}
            setup.Ux=dax*p[0].u_over_z+dbx*p[1].u_over_z+dcx*p[2].u_over_z;setup.Uy=day*p[0].u_over_z+dby*p[1].u_over_z+dcy*p[2].u_over_z;
            setup.Vx=dax*p[0].v_over_z+dbx*p[1].v_over_z+dcx*p[2].v_over_z;setup.Vy=day*p[0].v_over_z+dby*p[1].v_over_z+dcy*p[2].v_over_z;
            setup.Wx=dax*p[0].invz+dbx*p[1].invz+dcx*p[2].invz;setup.Wy=day*p[0].invz+dby*p[1].invz+dcy*p[2].invz;
            // Mip pyramids are built here, serially, so the band workers only read them.
            if(setup.filtered&&setup.texture&&(!setup.texture->filter_cache->ready||setup.texture->filter_cache->fingerprint!=setup.texture->fingerprint()))build_mips(*setup.texture);
            // Linear samples only level 0. Default filtered Punch also reads mip levels;
            // identical texture copies can share the completed chain until pixels detach.
            if(setup.filtered&&!setup.linear_only&&setup.punch_through&&setup.texture&&setup.texture->filter_cache->punch_mips.size()!=setup.texture->filter_cache->mips.size())build_punch_mips(*setup.texture);
            setups.push_back(setup);
        }
    }
    std::size_t setups_before_modifiers=0,setups_before_mirror_effects=0;
    // The late water call draws directly even with ABE (ulDrawGeneralPurposeWater,
    // ps2_NinjaCnk.c:1927/2359): after player/opaque, before game.c:87's mirror room
    // and the final translucent OT. Never depth-sort it into that OT.
    {auto first=std::stable_partition(setups.begin(),setups.end(),[](const Setup& s){return s.blend==SceneBlend::Opaque||s.general_water;});
     if(std::any_of(setups.begin(),first,[](const Setup& s){return s.general_water;}))
         std::stable_sort(setups.begin(),first,[](const Setup& a,const Setup& b){
             /* game.c:427 Spiegel-bhDrawEffect nach dem Spiegelraum (:417) */ const auto phase=[](const Setup& s){return s.general_water?1:s.mirror_room?2:s.mirror_effect?3:0;};return phase(a)<phase(b);});
     std::stable_sort(first,setups.end(),[](const Setup& a,const Setup& b){return a.p[0].invz+a.p[1].invz+a.p[2].invz<b.p[0].invz+b.p[1].invz+b.p[2].invz;});
     // game.c:318 draws room, objects, then effects (modifiers) and only then enemy/player.
     if(zc)setups_before_modifiers=std::size_t(std::stable_partition(setups.begin(),first,[](const Setup& s){return !s.post;})-setups.begin());
     // game.c:417/427: der Spiegel-bhDrawMdfEffect liegt hinter dem Spiegelraum (Phase 2) und vor den Spiegeleffekten (Phase 3).
     if(zc)setups_before_mirror_effects=std::size_t(std::find_if(setups.begin(),first,[](const Setup& s){return s.mirror_effect;})-setups.begin());}
    // Stage 2: rasterize horizontal bands, possibly on several threads. Every band visits the
    // setups in submission order, so the result does not depend on the thread count.
    std::vector<float> transported;
    // njSetFogColor und der ARGB-Framebuffer haben dieselbe RGB-Kanalreihenfolge.
    const std::uint32_t fog_pixel=fog_canvas_color(scene.fog_color);
    // Diagnose: ein Beobachter sieht dieselbe Liste wie die Bandstufe (GPU-Vergleich), aendert aber nichts.
    raster::Frame frame{width_,height_,S,&setups,std::size_t(std::find_if(setups.begin(),setups.end(),[](const Setup& s){return s.blend!=SceneBlend::Opaque&&!s.general_water;})-setups.begin()),scene.background,fog_pixel,wireframe,bool(zc)};
    frame.before_modifiers=zc?setups_before_modifiers:setups.size();frame.before_mirror_effects=zc?setups_before_mirror_effects:setups.size();
    for(const auto& shadow:scene.shadows)frame.shadow_pass[shadow.mirror?1:0]=true;
    if(observer_)observer_(frame);
    const auto rasterize=[&](const Setup& s,int y0,int y1){
        const auto& p=s.p;const auto* texture=s.texture;
            int setup_y=-1,setup_x=0,setup_taps=1,setup_l0=0;float setup_du=0,setup_dv=0,setup_frac=0;
            for(int y=std::max(s.miny,y0);y<=std::min(s.maxy,y1-1);++y){
                // Conservative scanline bounds keep the existing pixel/edge tests
                // exact while avoiding the empty corners of triangle bounding boxes.
                const float scan=float(y)+.5f;
                float left=std::numeric_limits<float>::infinity(),right=-left;
                for(unsigned e=0;e<3;++e){const auto& a=p[e];const auto& b=p[(e+1)%3];
                    if(scan<std::min(a.y,b.y)||scan>std::max(a.y,b.y))continue;
                    if(a.y==b.y){left=std::min(left,std::min(a.x,b.x));right=std::max(right,std::max(a.x,b.x));}
                    else{const float hit=a.x+(scan-a.y)*(b.x-a.x)/(b.y-a.y);left=std::min(left,hit);right=std::max(right,hit);}}
                if(!std::isfinite(left)||!std::isfinite(right))continue;
                const int begin=int(std::max(float(s.minx),std::floor(left-.5f)-1));
                const int end=int(std::min(float(s.maxx),std::ceil(right-.5f)+1));
                for(int x=begin;x<=end;++x){
                const float a=edge(p[1],p[2],x+0.5f,y+0.5f)/s.area;
                const float b=edge(p[2],p[0],x+0.5f,y+0.5f)/s.area;
                const float c=edge(p[0],p[1],x+0.5f,y+0.5f)/s.area;if(a<0||b<0||c<0)continue;
                // Half-open coverage for every triangle, including opaque/punch: a shared
                // edge has one owner, but independently submitted coincident faces still draw.
                const auto inclusive=[](const Projected& from,const Projected& to){return to.y<from.y||(to.y==from.y&&to.x>from.x);};
                if((a==0&&!inclusive(p[1],p[2]))||(b==0&&!inclusive(p[2],p[0]))||(c==0&&!inclusive(p[0],p[1])))continue;
                // Affine form preserves constant vertex depth exactly, rather than letting
                // roundoff in a+b+c turn a coplanar face into a spurious farther fragment.
                const float invz=p[0].invz+b*(p[1].invz-p[0].invz)+c*(p[2].invz-p[0].invz);
                const auto index=std::size_t(y)*W+x;
                // Ps2AddPrim3D/Ex/Ex1P/ExI and the chunk TEST state use GEQUAL.
                // This remains native reciprocal depth, not a GS-Z quantization model.
                if(invz<depth[index])continue;
                const float material_alpha=float(s.color>>24)/255.f*(s.vertex_alpha?std::clamp(a*s.alpha[0]+b*s.alpha[1]+c*s.alpha[2],0.f,1.f):1.f);
                if(material_alpha<=0.f)continue; // TEST GREATER 0 / KEEP, also without ABE
                auto pixel=s.color;float coverage=1;
                if(s.gouraud)pixel=lit(s.color,a*s.light[0].x+b*s.light[1].x+c*s.light[2].x,a*s.light[0].y+b*s.light[1].y+c*s.light[2].y,a*s.light[0].z+b*s.light[1].z+c*s.light[2].z);
                if(texture) {
                    const float rz=1.f/invz;
                    float u=(a*p[0].u_over_z+b*p[1].u_over_z+c*p[2].u_over_z)*rz;
                    float v=(a*p[0].v_over_z+b*p[1].v_over_z+c*p[2].v_over_z)*rz;
                    if(!s.filtered) {
                        if(s.clamp_u)u=std::clamp(u,0.f,1.f);else u-=std::floor(u);
                        if(s.clamp_v)v=std::clamp(v,0.f,1.f);else v-=std::floor(v);
                        const auto tx=std::min(texture->width-1,unsigned(u*texture->width));
                        const auto ty=std::min(texture->height-1,unsigned(v*texture->height));
                        const auto texel=texture->pixels[std::size_t(ty)*texture->width+tx];
                        if((texel>>24)==0)continue;
                        pixel=modulate(texel,pixel,s.texture_color_divisor);
                        if(s.blend!=SceneBlend::Opaque){
                            // Every engine texture carries the host 0..255
                            // alpha whose blend factor is alpha/255 (see the
                            // filtered path below).
                            coverage=float(texel>>24)/255.f;
                        }
                    } else {
                                                const float tw=float(texture->width),th=float(texture->height);
                        // Mip level, anisotropy and footprint axis change slowly across a span:
                        // derive them every fourth pixel of a row and reuse in between.
                        if(y!=setup_y||x-setup_x>=4||x<setup_x){
                            setup_y=y;setup_x=x;
                            // Derivatives of (u,v) per screen pixel: d(U/W)=(dU-u*dW)/W.
                            const float dudx=(s.Ux-u*s.Wx)*rz,dudy=(s.Uy-u*s.Wy)*rz,dvdx=(s.Vx-v*s.Wx)*rz,dvdy=(s.Vy-v*s.Wy)*rz;
                            const float lx=std::sqrt(dudx*dudx*tw*tw+dvdx*dvdx*th*th),ly=std::sqrt(dudy*dudy*tw*tw+dvdy*dvdy*th*th);
                            const bool major_x=lx>=ly;const float major=major_x?lx:ly,minor=major_x?ly:lx;
                            int taps=1;if(major>1e-6f&&minor>1e-6f)taps=std::clamp(int(major/minor+.99f),1,4);else if(major>1e-6f)taps=4;
                            setup_taps=taps;setup_du=major_x?dudx:dudy;setup_dv=major_x?dvdx:dvdy;
                            const float lod=std::clamp(fast_log2(std::max(major/float(taps),1e-6f)),0.f,float(texture->filter_cache->mips.size()));
                            setup_l0=int(lod);setup_frac=lod-float(setup_l0);
                        }
                        const int taps=setup_taps,top=int(texture->filter_cache->mips.size());
                        const float du=s.linear_only?0:setup_du,dv=s.linear_only?0:setup_dv;const int l0=s.linear_only?0:setup_l0,l1=s.linear_only?0:std::min(l0+1,top);const float frac=s.linear_only?0:setup_frac;
                        if(!s.clamp_u)u-=std::floor(u);
                        if(!s.clamp_v)v-=std::floor(v);
                        const Level a0=level_of(*texture,unsigned(l0),s.clamp_u,s.clamp_v,s.punch_through),a1=level_of(*texture,unsigned(l1),s.clamp_u,s.clamp_v,s.punch_through);
                        float acc[5]={0,0,0,0,0};
                        const int draw_taps=s.linear_only?1:taps;
                        const bool second=frac>1e-3f&&l1!=l0;const float inv_taps=1.f/float(draw_taps);
                        for(int tap=0;tap<draw_taps;++tap){
                            const float t=(float(tap)+.5f)*inv_taps-.5f,su=u+t*du,sv=v+t*dv;
                            // Ps2TexLoad TFX MODULATE (ps2_dummy.c:1615-1625), ohne ABE: RGB und Alpha
                            // werden unabhaengig gefiltert. Alphatest verwirft danach, er entmischt kein RGB.
                            if(s.punch_through){bilinear<false,true>(a0,su,sv,(1-frac)*inv_taps,acc);if(second)bilinear<false,true>(a1,su,sv,frac*inv_taps,acc);}
                            else if(texture->filter_cache->uniform_alpha){bilinear<false>(a0,su,sv,(1-frac)*inv_taps,acc);if(second)bilinear<false>(a1,su,sv,frac*inv_taps,acc);}
                            else{bilinear<true>(a0,su,sv,(1-frac)*inv_taps,acc);if(second)bilinear<true>(a1,su,sv,frac*inv_taps,acc);}
                        }
                        // Cut-out textures keep their coverage: partly covered pixels blend with
                        // what is already drawn instead of flipping on/off at a threshold.
                        if(acc[3]<=0.f)continue;
                        // Constant alpha cancels out of RGB interpolation. Keep
                        // its actual coverage separately (full opacity is 0xFF).
                        if(s.blend!=SceneBlend::Opaque){
                            // One alpha domain for every path: the texel alpha
                            // is the GS ratio over 255.  Ps2CheckTextureAlpha's
                            // Ps2AlphaIsHalf halves the TIM2 palette and the GS
                            // blends As/128; the same factor is (2*As)/256, so
                            // the engine keeps the 8-bit alpha and divides by
                            // 255.  CNK strips combine it with vu1SetAlphaRatio
                            // (the material alpha over 255) as the VU1 path does
                            // before GS ALPHA.
                            coverage=texture->filter_cache->uniform_alpha?float(texture->filter_cache->max_alpha)/255.f:acc[3]/255.f;
                            coverage=std::min(1.f,coverage);
                        }
                        else if(texture->filter_cache->cutout&&!s.punch_through){coverage=std::min(1.f,acc[3]/float(texture->filter_cache->max_alpha));if(coverage<1.f/32)continue;}
                        const float inv=1.f/(s.punch_through?acc[4]:acc[3]);
                        auto channel=[&](float value){return std::uint32_t(std::clamp(value*inv+.5f,0.f,255.f));};
                        pixel=modulate(0xff000000|(channel(acc[2])<<16)|(channel(acc[1])<<8)|channel(acc[0]),pixel,s.texture_color_divisor);
                    }
                }
                if(s.fogged){ // GS: Cv = (Cs*F + Cf*(255-F)) >> 8 per channel, F from the vertex fog coefficients
                    const unsigned F=unsigned(std::clamp(a*s.fog[0]+b*s.fog[1]+c*s.fog[2],0.f,255.f));
                    const std::uint32_t fog_rgb=s.fog_color_set?s.fog_color:fog_pixel;
                    std::uint32_t out=pixel&0xff000000u;for(unsigned shift:{0u,8u,16u})out|=((((pixel>>shift)&255u)*F+((fog_rgb>>shift)&255u)*(255u-F))>>8)<<shift;pixel=out;}
                if(s.blend==SceneBlend::Additive){
                    const auto below=color[index];std::uint32_t mixed=0xff000000;
                    const float alpha=coverage*material_alpha;
                    for(unsigned shift:{0u,8u,16u})mixed|=std::min(255u,unsigned((below>>shift)&255)+unsigned(float((pixel>>shift)&255)*alpha+.5f))<<shift;
                    color[index]=mixed;continue;
                }
                if(s.blend==SceneBlend::Alpha||s.blend==SceneBlend::Ninja){
                    const float alpha=coverage*material_alpha;
                    color[index]=blend_gs(s.blend==SceneBlend::Ninja?s.blend_register:0x44,pixel,color[index],alpha);
                    // Direct water retains TEST GREATER 0 / KEEP and Z writes, unlike
                    // OT's Ps2ZbuffOff alpha >=128 / FB_ONLY (ps2_dummy.c:1980/2024).
                    if(s.depth_write&&(s.general_water?alpha>0.f:alpha>=.5f))depth[index]=invz;continue;
                }
                if(coverage<.999f){
                    const auto below=color[index];std::uint32_t mixed=0xff000000;
                    for(unsigned shift:{0u,8u,16u})mixed|=std::uint32_t(float((below>>shift)&255)*(1-coverage)+float((pixel>>shift)&255)*coverage+.5f)<<shift;
                    pixel=mixed;if(coverage<.5f){color[index]=pixel;continue;} // too thin to occlude later geometry
                }
                if(s.depth_write){depth[index]=invz;if(!transported.empty())transported[index]=a*p[0].tz+b*p[1].tz+c*p[2].tz;}
                color[index]=wireframe && std::min({a,b,c})>0.025f?scene.background:pixel;
            }
            }
    };
    constexpr int band_rows=8; // base-resolution rows per work item
    const int bands=(height_+band_rows-1)/band_rows;
    // Band rasterization is the bulk of a frame and scales with the hardware
    // threads; the result is identical for any worker count (same bands, same
    // submission order), so this is presentation speed only and never changes a
    // pixel. Two threads stay free for the SDL audio callback and presentation
    // (measured: more workers than hardware threads is slower again), and the
    // count never drops below the four workers used before.
    const unsigned available=std::max(1u,std::thread::hardware_concurrency());
    const unsigned default_workers=std::max(4u,available>2u?available-2u:1u);
    const unsigned workers=std::min<unsigned>(threads_?threads_:default_workers,std::max(1,bands));
    // Stage [first,last) over every band, then optionally downsample. Joining
    // between stages keeps the shadow pass serial and thread-count independent.
    const auto run=[&](std::size_t first,std::size_t last,bool resolve){
        std::atomic<int> next_band{0};
        const auto worker=[&]{for(int band=next_band++;band<bands;band=next_band++){
            const int by0=band*band_rows,by1=std::min(height_,by0+band_rows),y0=by0*S,y1=by1*S;
            for(std::size_t i=first;i<last;++i){const auto& s=setups[i];if(s.maxy>=y0&&s.miny<y1)rasterize(s,y0,y1);}
            if(resolve&&S>1)for(int y=by0;y<by1;++y)for(int x=0;x<width_;++x){
                unsigned r=0,g=0,b=0;float nearest=0;
                for(int sy=0;sy<S;++sy)for(int sx=0;sx<S;++sx){const auto at=std::size_t(y*S+sy)*W+x*S+sx;const auto c=color[at];r+=c&255;g+=(c>>8)&255;b+=(c>>16)&255;nearest=std::max(nearest,depth[at]);}
                const unsigned n=unsigned(S*S);pixels_[std::size_t(y)*width_+x]=0xff000000|(((b+n/2)/n)<<16)|(((g+n/2)/n)<<8)|((r+n/2)/n);depth_[std::size_t(y)*width_+x]=nearest;}
        }};
        if(workers<=1||last-first<64)worker();
        else{std::vector<std::thread> pool;for(unsigned i=1;i<workers;++i)pool.emplace_back(worker);worker();for(auto& t:pool)t.join();}
    };
    // bhDrawMdfEffect: per work two mask passes, then one rectangle composite.
    // Receiver Z is never written. GS compares transported integer Z with GEQUAL.
    const float aspect_x=focal_x*640.f/float(width_),aspect_y=focal_y*480.f/float(height_);
    const raster::ShadowPass shadow_pass=[&](bool mirror,const float* transported_depth){
        const auto shadow_begin=Clock::now();
        std::vector<raster::ShadowComposite> out;
        for(const auto& shadow:scene.shadows){
            if(shadow.mirror!=mirror)continue;
            if(mirror&&!scene.mirror_enabled)throw std::runtime_error("mirror pass modifier without njMirror matrix");
            const auto projection=project_shadow(shadow,*(mirror?&mirror_view:&view),1.f,aspect_x,aspect_y);
            if(!projection.drawable)continue;
            raster::ShadowComposite c;
            c.mask=raster_shadow_mask(projection,*zc,[&](int x,int y,float modifier){
                const int sx=std::min(W-1,int((float(x)+.5f)*float(W)/640.f)),sy=std::min(H-1,int((float(y)+.5f)*float(H)/480.f));
                return shadow_depth_pass(std::uint32_t(modifier),std::uint32_t(transported_depth[std::size_t(sy)*W+sx]));});
            const auto fog=fog_sample(scene.fog,projection.last_vertex_z);
            c.fog_power=unsigned(std::clamp(fog,0.f,255.f));
            const auto quantize=[](float v,float origin){return std::trunc(v*16.f)/16.f-origin;};
            c.rx0=quantize(projection.bounds[0],1728.f);c.ry0=quantize(projection.bounds[1],1808.f);
            c.rx1=quantize(projection.bounds[2],1728.f);c.ry1=quantize(projection.bounds[3],1808.f);
            out.push_back(std::move(c));
        }
        timings_.shadow+=since(shadow_begin);
        return out;};
    timings_.setup+=since(frame_begin);
    // Dieselben Schattenwerke fuer einen Rasterer, der die Maske selbst zeichnet: Projektion,
    // Transport, Unterdrueckung, Flaeche, Eckreihenfolge und Pixelgrenzen wie raster_shadow_mask,
    // nur ohne die Pixelschleife. Reihenfolge: je Werk Durchgang 0, dann Durchgang 1.
    const raster::ShadowGeometry shadow_geometry=[&](bool mirror){
        const auto shadow_begin=Clock::now();
        std::vector<raster::ShadowWork> out;
        for(const auto& shadow:scene.shadows){
            if(shadow.mirror!=mirror)continue;
            if(mirror&&!scene.mirror_enabled)throw std::runtime_error("mirror pass modifier without njMirror matrix");
            const auto projection=project_shadow(shadow,*(mirror?&mirror_view:&view),1.f,aspect_x,aspect_y);
            if(!projection.drawable)continue;
            raster::ShadowWork work;
            for(unsigned pass=0;pass<2;++pass)for(const auto& face:projection.faces){
                if(pass==0?face.original_normal<=0:face.original_normal>=0)continue;
                std::array<ShadowTransportVertex,3> transported;
                for(unsigned i=0;i<3;++i)transported[i]=transport_shadow_vertex(face.vertices[i],*zc);
                if(shadow_transport_triangle_suppressed(transported))continue;
                auto p=transported;
                float area=(p[1].x-p[0].x)*(p[2].y-p[0].y)-(p[1].y-p[0].y)*(p[2].x-p[0].x);
                if(!std::isfinite(area)||area==0)continue;
                if(area<0){std::swap(p[1],p[2]);area=-area;}
                raster::ShadowFace f{};
                for(unsigned i=0;i<3;++i){f.x[i]=p[i].x;f.y[i]=p[i].y;f.depth[i]=p[i].depth;}
                f.area=area;f.subtract=pass==1;
                f.minx=int(std::max(0.f,std::floor(std::min({p[0].x,p[1].x,p[2].x}))));
                f.maxx=int(std::min(639.f,std::ceil(std::max({p[0].x,p[1].x,p[2].x}))));
                f.miny=int(std::max(0.f,std::floor(std::min({p[0].y,p[1].y,p[2].y}))));
                f.maxy=int(std::min(479.f,std::ceil(std::max({p[0].y,p[1].y,p[2].y}))));
                work.faces.push_back(f);
            }
            const auto fog=fog_sample(scene.fog,projection.last_vertex_z);
            work.fog_power=unsigned(std::clamp(fog,0.f,255.f));
            const auto quantize=[](float v,float origin){return std::trunc(v*16.f)/16.f-origin;};
            work.rx0=quantize(projection.bounds[0],1728.f);work.ry0=quantize(projection.bounds[1],1808.f);
            work.rx1=quantize(projection.bounds[2],1728.f);work.ry1=quantize(projection.bounds[3],1808.f);
            out.push_back(std::move(work));
        }
        timings_.shadow+=since(shadow_begin);
        return out;};
    // Host-GPU-Rasterer: dieselbe Liste, dieselben Phasen; ohne exakte Abbildung zeichnet die Bandstufe.
    const auto raster_begin=Clock::now();
    bool gpu_drawn=false;
    if(gpu_&&!wireframe){
        // Ein Geraetefehler (z. B. verlorene GPU) beendet nicht das Spiel: Meldung, danach Software.
        try{gpu_drawn=gpu_->render(frame,zc?shadow_geometry:raster::ShadowGeometry{},pixels_,depth_);}
        catch(const std::exception& e){gpu_error_=e.what();std::fprintf(stderr,"GPU raster disabled, software from now on: %s\n",e.what());gpu_.reset();}}
    last_gpu_frame_=gpu_drawn;
    if(gpu_drawn){}
    else if(!zc){clear_targets();run(0,setups.size(),true);}
    else{
        clear_targets();transported.assign(std::size_t(W)*H,0.f);
        run(0,setups_before_modifiers,false);
        const auto modifiers=[&](bool mirror){
        // Die Masken lesen nur die transportierte Tiefe, nie die Farbe: erst alle Masken, dann die Komposite.
        for(const auto& c:shadow_pass(mirror,transported.data())){
            for(int y=0;y<H;++y){const int my=std::min(479,int((float(y)+.5f)*480.f/float(H)));
                if(float(my)+.5f<c.ry0||float(my)+.5f>=c.ry1)continue;
                for(int x=0;x<W;++x){const int mx=std::min(639,int((float(x)+.5f)*640.f/float(W)));
                    if(float(mx)+.5f<c.rx0||float(mx)+.5f>=c.rx1)continue;
                    const unsigned value=c.mask[std::size_t(my)*640+mx];if(!value)continue;
                    auto& pixel=color[std::size_t(y)*W+x];pixel=shadow_mask_composite(pixel,value,c.fog_power);}}
        }};
        modifiers(false);
        // game.c:417 Spiegelraum, :427 Spiegel-bhDrawEffect mit bhDrawMdfEffect unter cam.mtx*njMirror,
        // danach die uebrigen Spiegeleffekte und die OT.
        run(setups_before_modifiers,setups_before_mirror_effects,false);
        modifiers(true);
        run(setups_before_mirror_effects,setups.size(),true);
    }
    (gpu_drawn?timings_.gpu:timings_.bands)+=since(raster_begin);
    const auto line_begin=Clock::now();
    // njDrawLine3D: finite 3D segments, endpoint colours, depth test, no depth write.
    for(const auto& line:scene.lines){if(line.mirror&&!scene.mirror_enabled)throw std::runtime_error("mirror pass line without njMirror matrix");
        NJS_MATRIX* const line_view=line.mirror?&mirror_view:&view;
        NJS_POINT3 a=line.points[0],b=line.points[1];njCalcPoint(line_view,&a,&a);njCalcPoint(line_view,&b,&b);float lo=0,hi=1;
        const float dz=b.z-a.z;if(std::abs(dz)<1e-8f){if(a.z<near||(far>near&&a.z>far))continue;}
        else{float t=(near-a.z)/dz;if(dz>0)lo=std::max(lo,t);else hi=std::min(hi,t);if(far>near){t=(far-a.z)/dz;if(dz>0)hi=std::min(hi,t);else lo=std::max(lo,t);}}
        if(lo>hi)continue;auto lerp=[](float x,float y,float t){return x+(y-x)*t;};
        const NJS_POINT3 c{lerp(a.x,b.x,lo),lerp(a.y,b.y,lo),lerp(a.z,b.z,lo)},d{lerp(a.x,b.x,hi),lerp(a.y,b.y,hi),lerp(a.z,b.z,hi)};
        float x0=width_*.5f+c.x*focal_x/c.z,y0=height_*.5f+c.y*focal_y/c.z,x1=width_*.5f+d.x*focal_x/d.z,y1=height_*.5f+d.y*focal_y/d.z;
        const float dx=x1-x0,dy=y1-y0;float first=0,last=1;
        auto clip=[&](float p,float q){if(std::abs(p)<1e-8f)return q>=0;const float r=q/p;if(p<0)first=std::max(first,r);else last=std::min(last,r);return first<=last;};
        if(!clip(-dx,x0)||!clip(dx,width_-1-x0)||!clip(-dy,y0)||!clip(dy,height_-1-y0))continue;
        const int steps=std::max(1,int(std::ceil(std::max(std::abs(dx),std::abs(dy))*(last-first))));
        // GS raster rule: pixel i covers [i, i+1), so a coordinate lands in floor(c)
        // (map.c:2337 MapDrawLine aims at 0.5 + floor(x), the pixel centre).
        for(int n=0;n<=steps;++n){const float t=lerp(first,last,float(n)/steps),original=lerp(lo,hi,t);const int x=int(std::floor(x0+dx*t)),y=int(std::floor(y0+dy*t));if(x<0||y<0||x>=width_||y>=height_)continue;
            const auto at=std::size_t(y)*width_+x;const float invz=lerp(1/c.z,1/d.z,t);if(invz<depth_[at])continue;
            unsigned pixel=0;for(unsigned shift:{0u,8u,16u,24u})pixel|=unsigned(std::clamp(lerp(float((line.colors[0]>>shift)&255),float((line.colors[1]>>shift)&255),original),0.f,255.f))<<shift;
            pixels_[at]=blend_gs(line.blend_register,pixel,pixels_[at],std::min(1.f,float(pixel>>24)/255.f));
        }
    }
    timings_.lines+=since(line_begin);
}
std::size_t SoftwareRenderer::covered_pixels() const{return std::count_if(depth_.begin(),depth_.end(),[](float d){return d>0;});}
void SoftwareRenderer::save_bmp(const std::filesystem::path& path)const{
    if(std::filesystem::exists(path))throw std::runtime_error("screenshot already exists");
    std::ofstream out(path,std::ios::binary);
    auto u16=[&](unsigned v){out.put(char(v));out.put(char(v>>8));};
    auto u32=[&](unsigned v){u16(v);u16(v>>16);};
    const unsigned row=(unsigned(width_)*3+3)&~3u,image=row*unsigned(height_);
    out.put('B');out.put('M');u32(54+image);u32(0);u32(54);
    u32(40);u32(width_);u32(height_);u16(1);u16(24);u32(0);u32(image);u32(2835);u32(2835);u32(0);u32(0);
    for(int y=height_-1;y>=0;--y){
        for(int x=0;x<width_;++x){const auto p=pixels_[std::size_t(y)*width_+x];out.put(char(p));out.put(char(p>>8));out.put(char(p>>16));}
        for(unsigned n=unsigned(width_)*3;n<row;++n)out.put(0);
    }
    out.close();if(!out)throw std::runtime_error("cannot write screenshot");
}
}
