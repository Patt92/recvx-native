// Host-Praesentationsstufe mit MetalFX (metalfx_present.h), nur macOS. Plattformanpassung, keine
// Spiellogik: das 640x480-Bild der Runtime wird unveraendert hochgeladen; die Stufe skaliert,
// interpoliert und nachbearbeitet nur die Anzeige.
//
// Kette je Bild (ein oder zwei Befehlspuffer derselben Warteschlange):
//   Upload (Farbe, Szenenbild, 1/z)  ->  motion_k (Bewegungsvektoren, umgekehrte Tiefe, 2D-Maske)
//   -> depth_fs (Depth32Float fuer MetalFX)  ->  [MTLFXTemporalScaler]  ->  [MTLFXSpatialScaler]
//   -> [MTLFXFrameInterpolator: Zwischenbild aus vorigem und aktuellem Bild]
//   -> present_k (Post-Processing + 4:3-Einpassung) in das Drawable  ->  presentDrawable.
// Die MetalFX-Klassen sind schwach gelinkt (-weak_framework MetalFX) und hinter @available; auf
// aelteren Systemen bleibt die Stufe mit Post-Processing/Einpassung nutzbar.
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>
#include <simd/simd.h>
#include "metalfx_present.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <dispatch/dispatch.h>
#include <sstream>
#include <AvailabilityVersions.h>
// Frame-Interpolation braucht das macOS-26-SDK, die Kameramatrizen des Interpolators das macOS-27-SDK.
#if defined(__MAC_26_0)
#define RECVX_SDK_FRAME_INTERPOLATION 1
#endif
#if defined(__MAC_27_0)
#define RECVX_SDK_INTERPOLATOR_MATRICES 1
#endif
namespace recvx {
namespace {
const char* present_shader=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct MotionU {
    float4x4 cur_inv;    // Sicht -> Welt des aktuellen Bildes
    float4x4 prev_view;  // Welt -> Sicht des vorigen Bildes
    float2 focal_cur, focal_prev, center;
    float depth_a, depth_b; // umgekehrte endliche Tiefe d = a*(1/z) - b
    uint has_scene, has_prev, W, H;
};
// Bewegungsvektor je Pixel: wohin dieses Pixel im vorigen Bild lag (prev - cur, in Pixeln des
// 640x480-Bildes; MetalFX-Konvention mit motionVectorScale 1). 2D-Pixel: unbewegt, naechste Tiefe.
kernel void motion_k(texture2d<float,access::read> depth [[texture(0)]], texture2d<float,access::read> pixels [[texture(1)]],
                     texture2d<float,access::read> scene [[texture(2)]], texture2d<float,access::write> motion [[texture(3)]],
                     texture2d<float,access::write> depth_out [[texture(4)]], constant MotionU& u [[buffer(0)]], uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=u.W||gid.y>=u.H)return;
    const float invz=depth.read(gid).r;
    bool ui=u.has_scene==0u;
    if(!ui){const float4 a=pixels.read(gid),b=scene.read(gid);ui=any(a!=b);}
    float2 mv=float2(0.0);float d=0.0;
    if(ui)d=1.0;
    else if(invz>0.0){
        d=clamp(u.depth_a*invz-u.depth_b,0.0,1.0);
        if(u.has_prev!=0u){
            const float z=1.0/invz;const float2 pc=float2(gid)+0.5;
            const float4 v=float4((pc.x-u.center.x)*z/u.focal_cur.x,(pc.y-u.center.y)*z/u.focal_cur.y,z,1.0);
            const float4 p=u.prev_view*(u.cur_inv*v);
            if(p.z>1e-6){const float2 pp=u.center+p.xy/p.z*u.focal_prev;mv=pp-pc;}
        }
    }
    motion.write(float4(mv,0.0,0.0),gid);depth_out.write(float4(d,0.0,0.0,0.0),gid);
}
struct DV { float4 pos [[position]]; };
vertex DV depth_vs(uint vid [[vertex_id]]) {DV o;const float2 p=float2(vid==1u?3.0:-1.0,vid==2u?3.0:-1.0);o.pos=float4(p,0.0,1.0);return o;}
struct DF { float depth [[depth(any)]]; };
fragment DF depth_fs(DV in [[stage_in]], texture2d<float,access::read> src [[texture(0)]]) {DF o;o.depth=src.read(uint2(in.pos.xy)).r;return o;}
// Kopie zwischen Formaten (BGRA8 <-> RGBA16Float) in gleicher Groesse.
kernel void blit_k(texture2d<float,access::read> src [[texture(0)]], texture2d<float,access::write> dst [[texture(1)]], uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=dst.get_width()||gid.y>=dst.get_height())return;dst.write(src.read(gid),gid);
}
struct PresentU {
    float2 src_size, fit_origin, fit_size;
    uint2 dst_size;
    uint mode, nearest, game_lines, pad;
    float sharpness;
};
// Post-Processing und 4:3-Einpassung: mode 0 Kopie, 1 kontrastadaptives Schaerfen (5-Tap-CAS),
// 2 CRT (Schaerfen + Zeilenraster der game_lines Spielzeilen + Lochmaske). Rand schwarz.
kernel void present_k(texture2d<float,access::sample> src [[texture(0)]], texture2d<float,access::write> dst [[texture(1)]], constant PresentU& u [[buffer(0)]], uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=u.dst_size.x||gid.y>=u.dst_size.y)return;
    const float2 p=float2(gid)+0.5-u.fit_origin;
    if(p.x<0.0||p.y<0.0||p.x>=u.fit_size.x||p.y>=u.fit_size.y){dst.write(float4(0.0,0.0,0.0,1.0),gid);return;}
    constexpr sampler lin(coord::normalized,filter::linear,address::clamp_to_edge);
    constexpr sampler pt(coord::normalized,filter::nearest,address::clamp_to_edge);
    const float2 uv=p/u.fit_size;
    float3 c=(u.nearest!=0u?src.sample(pt,uv):src.sample(lin,uv)).rgb;
    if(u.mode>=1u){
        const float2 t=1.0/u.src_size;
        const float3 a=src.sample(lin,uv+float2(0.0,-t.y)).rgb,b=src.sample(lin,uv+float2(-t.x,0.0)).rgb,d=src.sample(lin,uv+float2(t.x,0.0)).rgb,e=src.sample(lin,uv+float2(0.0,t.y)).rgb;
        const float3 mn=min(min(min(a,b),min(d,e)),c),mx=max(max(max(a,b),max(d,e)),c);
        const float3 amp=sqrt(clamp(min(mn,1.0-mx)/max(mx,float3(1e-4)),0.0,1.0));
        const float3 w=amp*(-1.0/(8.0-3.0*u.sharpness));
        c=clamp(((a+b+d+e)*w+c)/(4.0*w+1.0),0.0,1.0);
    }
    if(u.mode==2u){
        const float f=fract(p.y/u.fit_size.y*float(u.game_lines));
        c*=0.70+0.30*sin(f*3.14159265);
        const uint k=gid.x%3u;c*=float3(k==0u?1.0:0.9,k==1u?1.0:0.9,k==2u?1.0:0.9);
    }
    dst.write(float4(c,1.0),gid);
}
)MSL";
struct MotionU {float cur_inv[16],prev_view[16];float focal_cur[2],focal_prev[2],center[2];float depth_a,depth_b;std::uint32_t has_scene,has_prev,W,H;};
static_assert(sizeof(MotionU)==128+24+8+16,"motion uniform layout");
struct PresentU {float src_size[2],fit_origin[2],fit_size[2];std::uint32_t dst_size[2];std::uint32_t mode,nearest,game_lines,pad;float sharpness;float pad2[3];};
simd_float4x4 to_simd(const float* m){simd_float4x4 out;for(int c=0;c<4;++c)out.columns[c]=simd_make_float4(m[c*4],m[c*4+1],m[c*4+2],m[c*4+3]);return out;}
void from_simd(const simd_float4x4& m,float* out){for(int c=0;c<4;++c){out[c*4]=m.columns[c][0];out[c*4+1]=m.columns[c][1];out[c*4+2]=m.columns[c][2];out[c*4+3]=m.columns[c][3];}}
class Presenter final:public MetalFxPresenter {
public:
    explicit Presenter(CAMetalLayer* layer):layer_(layer),device_(layer.device){
        queue_=[device_ newCommandQueue];queue_.label=@"RECVX presentation";
        NSError* error=nil;MTLCompileOptions* options=[MTLCompileOptions new];
        id<MTLLibrary> library=[device_ newLibraryWithSource:[NSString stringWithUTF8String:present_shader] options:options error:&error];
        if(!library)throw std::runtime_error(std::string("presentation shader: ")+error.localizedDescription.UTF8String);
        const auto compute=[&](NSString* name){id<MTLComputePipelineState> p=[device_ newComputePipelineStateWithFunction:[library newFunctionWithName:name] error:&error];
            if(!p)throw std::runtime_error(std::string("presentation pipeline ")+name.UTF8String+": "+error.localizedDescription.UTF8String);return p;};
        motion_=compute(@"motion_k");blit_=compute(@"blit_k");present_=compute(@"present_k");
        {MTLRenderPipelineDescriptor* d=[MTLRenderPipelineDescriptor new];d.vertexFunction=[library newFunctionWithName:@"depth_vs"];d.fragmentFunction=[library newFunctionWithName:@"depth_fs"];
         d.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;depth_pipeline_=[device_ newRenderPipelineStateWithDescriptor:d error:&error];
         if(!depth_pipeline_)throw std::runtime_error(std::string("presentation depth pipeline: ")+error.localizedDescription.UTF8String);
         MTLDepthStencilDescriptor* ds=[MTLDepthStencilDescriptor new];ds.depthWriteEnabled=YES;ds.depthCompareFunction=MTLCompareFunctionAlways;depth_state_=[device_ newDepthStencilStateWithDescriptor:ds];}
        if(@available(macOS 13.0,*)){upscaler_supported_=[MTLFXSpatialScalerDescriptor supportsDevice:device_]&&[MTLFXTemporalScalerDescriptor supportsDevice:device_];}
#if RECVX_SDK_FRAME_INTERPOLATION
        if(@available(macOS 26.0,*)){frame_generation_supported_=[MTLFXFrameInterpolatorDescriptor supportsDevice:device_];}
#endif
        inflight_=dispatch_semaphore_create(1);
    }
    void configure(const VideoSettings& settings)override{settings_=settings;dirty_=true;}
    bool upscaler_supported()const override{return upscaler_supported_;}
    bool frame_generation_supported()const override{return frame_generation_supported_;}
    const PresentStats& stats()const override{return stats_;}
    std::string describe()const override{
        std::ostringstream out;
        if(chain_.temporal)out<<"MetalFX temporal "<<chain_.temporal_scale<<"x";
        if(chain_.spatial)out<<(chain_.temporal?" + ":"")<<"MetalFX spatial "<<chain_.spatial_scale<<"x";
        if(!chain_.temporal&&!chain_.spatial)out<<"nearest";
        if(chain_.interpolator)out<<", frame interpolation";
        if(settings_.post_process==PostProcess::Sharpen)out<<", sharpen";else if(settings_.post_process==PostProcess::Crt)out<<", CRT";
        if(!notes_.empty())out<<" ("<<notes_<<")";
        return out.str();
    }
    bool present(const PresentFrame& frame,bool show_interpolated)override{@autoreleasepool{
        if(broken_||frame.width!=640||frame.height!=480||!frame.pixels||!frame.depth)return false;
        const CGSize ds=layer_.drawableSize;const int dw=int(ds.width),dh=int(ds.height);
        if(dw<=0||dh<=0){++stats_.skipped;return true;}
        @try{try{
        if(dirty_||dw!=drawable_w_||dh!=drawable_h_){rebuild(dw,dh);}
        // Vorigen Befehlspuffer abwarten, bevor die Upload-Texturen ueberschrieben werden.
        dispatch_semaphore_wait(inflight_,DISPATCH_TIME_FOREVER);
        const bool reset=!history_valid_||frame.scene_key!=scene_key_||chain_.reset_pending;
        chain_.reset_pending=false;scene_key_=frame.scene_key;if(reset)++stats_.resets;
        upload(frame);
        id<MTLCommandBuffer> cb=[queue_ commandBuffer];cb.label=@"RECVX present";
        encode_motion(cb,frame,reset);
        id<MTLTexture> color=color_in_;
        if(chain_.work_format!=MTLPixelFormatBGRA8Unorm){blit(cb,color_in_,work_in_);color=work_in_;}
        const unsigned cur=current_,prev=current_^1u;
        if(@available(macOS 13.0,*)){
            if(chain_.temporal){id<MTLFXTemporalScaler> t=(id<MTLFXTemporalScaler>)chain_.temporal;t.colorTexture=color;t.depthTexture=depth_fx_;t.motionTexture=motion_tex_;
                t.outputTexture=chain_.spatial?temporal_out_:final_[cur];t.jitterOffsetX=0;t.jitterOffsetY=0;t.motionVectorScaleX=1;t.motionVectorScaleY=1;
                t.reset=reset;t.depthReversed=YES;t.inputContentWidth=640;t.inputContentHeight=480;[t encodeToCommandBuffer:cb];color=t.outputTexture;}
            if(chain_.spatial){id<MTLFXSpatialScaler> s=(id<MTLFXSpatialScaler>)chain_.spatial;s.colorTexture=color;s.outputTexture=final_[cur];s.inputContentWidth=color.width;s.inputContentHeight=color.height;[s encodeToCommandBuffer:cb];color=final_[cur];}
        }
        if(!chain_.temporal&&!chain_.spatial&&color!=final_[cur]){blit(cb,color,final_[cur]);color=final_[cur];}
        bool interpolated=false;
#if RECVX_SDK_FRAME_INTERPOLATION
        if(@available(macOS 26.0,*)){
            if(chain_.interpolator&&!reset&&frame.delta_seconds>0){
                id<MTLFXFrameInterpolator> fi=(id<MTLFXFrameInterpolator>)chain_.interpolator;
                fi.colorTexture=final_[cur];fi.prevColorTexture=final_[prev];fi.depthTexture=depth_fx_;fi.motionTexture=motion_tex_;fi.outputTexture=interp_;
                fi.motionVectorScaleX=1;fi.motionVectorScaleY=1;fi.deltaTime=float(frame.delta_seconds);fi.depthReversed=YES;fi.jitterOffsetX=0;fi.jitterOffsetY=0;
                fi.shouldResetHistory=NO;fi.uiTexture=nil;fi.uiTextureComposited=NO;
                const float fy=frame.projection.valid&&frame.projection.focal_y>0?frame.projection.focal_y:last_focal_[1];
                const float fov=fy>0?2.0f*std::atan(240.0f/fy)*180.0f/3.14159265f:60.0f;
                fi.fieldOfView=fov;fi.aspectRatio=4.0f/3.0f;fi.nearPlane=depth_near_;fi.farPlane=depth_far_;
#if RECVX_SDK_INTERPOLATOR_MATRICES
                if(@available(macOS 27.0,*)){
                    fi.worldToViewMatrix=to_simd(last_view_);
                    const float a=depth_near_*depth_far_/(depth_far_-depth_near_),b=depth_near_/(depth_far_-depth_near_);
                    const float fx=last_focal_[0]>0?last_focal_[0]:fy;
                    simd_float4x4 clip={{{2.0f*fx/640.0f,0,0,0},{0,-2.0f*fy/480.0f,0,0},{0,0,-b,1},{0,0,a,0}}};
                    fi.viewToClipMatrix=clip;}
#endif
                [fi encodeToCommandBuffer:cb];interpolated=true;
            }
        }
#endif
        latest_real_=final_[cur];latest_interp_=interpolated?interp_:nil;if(interpolated)++stats_.interpolated;
        show(cb,show_interpolated&&latest_interp_?latest_interp_:latest_real_);
        history_valid_=true;current_=prev;
        return true;
        }catch(const std::exception& e){fail(e.what());return false;}
        }@catch(NSException* e){fail(std::string(e.name.UTF8String)+": "+e.reason.UTF8String);return false;}
    }}
    bool represent(bool interpolated)override{@autoreleasepool{
        if(broken_||!latest_real_)return false;
        const CGSize ds=layer_.drawableSize;if(int(ds.width)!=drawable_w_||int(ds.height)!=drawable_h_)return false; /* Groesse geaendert: naechstes present() baut neu */
        dispatch_semaphore_wait(inflight_,DISPATCH_TIME_FOREVER);
        @try{try{
        id<MTLCommandBuffer> cb=[queue_ commandBuffer];cb.label=@"RECVX represent";
        show(cb,interpolated&&latest_interp_?latest_interp_:latest_real_);++stats_.repeated;
        return true;
        }catch(const std::exception& e){fail(e.what());return false;}
        }@catch(NSException* e){fail(std::string(e.name.UTF8String)+": "+e.reason.UTF8String);return false;}
    }}
    ~Presenter()override{dispatch_semaphore_wait(inflight_,DISPATCH_TIME_FOREVER);dispatch_semaphore_signal(inflight_);}
private:
    // Fehler der Stufe: einmal melden, Stufe abschalten; die Runtime praesentiert dann ueber SDL.
    void fail(const std::string& what){broken_=true;dispatch_semaphore_signal(inflight_);notes_="disabled: "+what;std::fprintf(stderr,"MetalFX presentation disabled: %s\n",what.c_str());}
    // MetalFX-Objekte als generische id: die Protokolle sind je OS-Version verfuegbar, die Nutzung steht
    // hinter @available und Praeprozessor-Guards fuer das SDK.
    struct Chain {
        id temporal=nil,spatial=nil,interpolator=nil;
        float temporal_scale=0,spatial_scale=0;MTLPixelFormat work_format=MTLPixelFormatBGRA8Unorm;int out_w=640,out_h=480,temporal_w=0,temporal_h=0;bool reset_pending=false;
    };
    // Bild in das naechste Drawable einpassen und sofort praesentieren (VSync ueber displaySyncEnabled der Layer,
    // von SDL_RenderSetVSync gesetzt); nextDrawable wartet hoechstens auf ein freies Drawable.
    void show(id<MTLCommandBuffer> cb,id<MTLTexture> source){
        id<CAMetalDrawable> drawable=[layer_ nextDrawable];
        if(drawable){encode_present(cb,source,drawable.texture,!chain_.temporal&&!chain_.spatial);[cb presentDrawable:drawable];++stats_.presents;}else ++stats_.skipped;
        commit_last(cb);
    }
    void commit_last(id<MTLCommandBuffer> cb){
        dispatch_semaphore_t s=inflight_;PresentStats* stats=&stats_;
        [cb addCompletedHandler:^(id<MTLCommandBuffer> done){stats->gpu_seconds+=done.GPUEndTime-done.GPUStartTime;dispatch_semaphore_signal(s);}];
        [cb commit];
    }
    id<MTLTexture> make(MTLPixelFormat format,int w,int h,MTLTextureUsage usage,MTLStorageMode storage){
        MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:NSUInteger(w) height:NSUInteger(h) mipmapped:NO];d.usage=usage;d.storageMode=storage;
        id<MTLTexture> t=[device_ newTextureWithDescriptor:d];if(!t)throw std::runtime_error("presentation texture");return t;}
    void upload(const PresentFrame& frame){
        const MTLRegion r=MTLRegionMake2D(0,0,640,480);
        [color_in_ replaceRegion:r mipmapLevel:0 withBytes:frame.pixels bytesPerRow:640*4];
        [scene_in_ replaceRegion:r mipmapLevel:0 withBytes:frame.scene?frame.scene:frame.pixels bytesPerRow:640*4];
        [depth_in_ replaceRegion:r mipmapLevel:0 withBytes:frame.depth bytesPerRow:640*4];
    }
    void encode_motion(id<MTLCommandBuffer> cb,const PresentFrame& frame,bool reset){
        MotionU u{};u.W=640;u.H=480;u.center[0]=320.0f;u.center[1]=240.0f;
        const auto& p=frame.projection;u.has_scene=p.valid&&frame.scene?1u:0u;
        if(p.valid){
            // Umgekehrte endliche Tiefe mit den njClipZ-Abstaenden; far 0 (unbegrenzt) als 100000.
            const float near=std::max(p.near,1e-3f),far=p.far>near?p.far:100000.0f;
            depth_near_=near;depth_far_=far;u.depth_a=near*far/(far-near);u.depth_b=near/(far-near);
            u.focal_cur[0]=p.focal_x;u.focal_cur[1]=p.focal_y;
            const simd_float4x4 view=to_simd(p.view);from_simd(simd_inverse(view),u.cur_inv);
            if(!reset&&last_valid_){u.has_prev=1u;std::copy_n(last_view_,16,u.prev_view);u.focal_prev[0]=last_focal_[0];u.focal_prev[1]=last_focal_[1];}
            std::copy_n(p.view,16,last_view_);last_focal_[0]=p.focal_x;last_focal_[1]=p.focal_y;last_valid_=true;
        }else{u.depth_a=0;u.depth_b=0;u.focal_cur[0]=u.focal_cur[1]=1;from_simd(matrix_identity_float4x4,u.cur_inv);last_valid_=false;if(depth_near_<=0){depth_near_=0.05f;depth_far_=100000.0f;}}
        id<MTLComputeCommandEncoder> c=[cb computeCommandEncoder];c.label=@"motion";[c setComputePipelineState:motion_];
        [c setTexture:depth_in_ atIndex:0];[c setTexture:color_in_ atIndex:1];[c setTexture:scene_in_ atIndex:2];[c setTexture:motion_tex_ atIndex:3];[c setTexture:depth_value_ atIndex:4];
        [c setBytes:&u length:sizeof u atIndex:0];[c dispatchThreads:MTLSizeMake(640,480,1) threadsPerThreadgroup:MTLSizeMake(16,16,1)];[c endEncoding];
        MTLRenderPassDescriptor* rp=[MTLRenderPassDescriptor renderPassDescriptor];rp.depthAttachment.texture=depth_fx_;rp.depthAttachment.loadAction=MTLLoadActionDontCare;rp.depthAttachment.storeAction=MTLStoreActionStore;
        id<MTLRenderCommandEncoder> e=[cb renderCommandEncoderWithDescriptor:rp];e.label=@"depth";[e setRenderPipelineState:depth_pipeline_];[e setDepthStencilState:depth_state_];
        [e setFragmentTexture:depth_value_ atIndex:0];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[e endEncoding];
    }
    void blit(id<MTLCommandBuffer> cb,id<MTLTexture> src,id<MTLTexture> dst){
        id<MTLComputeCommandEncoder> c=[cb computeCommandEncoder];c.label=@"blit";[c setComputePipelineState:blit_];[c setTexture:src atIndex:0];[c setTexture:dst atIndex:1];
        [c dispatchThreads:MTLSizeMake(dst.width,dst.height,1) threadsPerThreadgroup:MTLSizeMake(16,16,1)];[c endEncoding];
    }
    void encode_present(id<MTLCommandBuffer> cb,id<MTLTexture> src,id<MTLTexture> dst,bool nearest){
        PresentU u{};u.src_size[0]=float(src.width);u.src_size[1]=float(src.height);u.fit_origin[0]=float(fit_x_);u.fit_origin[1]=float(fit_y_);u.fit_size[0]=float(fit_w_);u.fit_size[1]=float(fit_h_);
        u.dst_size[0]=std::uint32_t(dst.width);u.dst_size[1]=std::uint32_t(dst.height);u.mode=settings_.post_process==PostProcess::Crt?2u:settings_.post_process==PostProcess::Sharpen?1u:0u;
        u.nearest=nearest?1u:0u;u.game_lines=480;u.sharpness=0.5f;
        id<MTLComputeCommandEncoder> c=[cb computeCommandEncoder];c.label=@"present";[c setComputePipelineState:present_];[c setTexture:src atIndex:0];[c setTexture:dst atIndex:1];
        [c setBytes:&u length:sizeof u atIndex:0];[c dispatchThreads:MTLSizeMake(dst.width,dst.height,1) threadsPerThreadgroup:MTLSizeMake(16,16,1)];[c endEncoding];
    }
    // Kette fuer Fenstergroesse und Einstellungen neu aufbauen: Einpassung, Skalierer, Interpolator, Texturen.
    void rebuild(int dw,int dh){
        dirty_=false;drawable_w_=dw;drawable_h_=dh;notes_.clear();
        const double scale=std::min(dw/640.0,dh/480.0);fit_w_=std::max(1,int(std::floor(640.0*scale)));fit_h_=std::max(1,int(std::floor(480.0*scale)));fit_x_=(dw-fit_w_)/2;fit_y_=(dh-fit_h_)/2;
        chain_=Chain{};chain_.reset_pending=true;history_valid_=false;latest_real_=nil;latest_interp_=nil;
        const MTLTextureUsage all=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|MTLTextureUsageRenderTarget;
        if(!color_in_){
            color_in_=make(MTLPixelFormatBGRA8Unorm,640,480,all,MTLStorageModeShared);scene_in_=make(MTLPixelFormatBGRA8Unorm,640,480,MTLTextureUsageShaderRead,MTLStorageModeShared);
            depth_in_=make(MTLPixelFormatR32Float,640,480,MTLTextureUsageShaderRead,MTLStorageModeShared);
            motion_tex_=make(MTLPixelFormatRG16Float,640,480,all,MTLStorageModePrivate);depth_value_=make(MTLPixelFormatR32Float,640,480,all,MTLStorageModePrivate);
            depth_fx_=make(MTLPixelFormatDepth32Float,640,480,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,MTLStorageModePrivate);
        }
        // Zuerst BGRA8 als Arbeitsformat, sonst RGBA16Float (Konvertierung per blit_k).
        bool scalers_ok=false;
        for(MTLPixelFormat format:{MTLPixelFormatBGRA8Unorm,MTLPixelFormatRGBA16Float}){
            chain_.work_format=format;
            if((scalers_ok=build_scalers(format)))break;
        }
        if(!scalers_ok){chain_.work_format=MTLPixelFormatBGRA8Unorm;append_note("MetalFX scaler creation failed for BGRA8 and RGBA16Float");}
        const int ow=chain_.out_w,oh=chain_.out_h;
        for(auto& t:final_)t=make(chain_.work_format,ow,oh,all,MTLStorageModePrivate);
        temporal_out_=chain_.spatial&&chain_.temporal?make(chain_.work_format,chain_.temporal_w,chain_.temporal_h,all,MTLStorageModePrivate):nil;
        work_in_=chain_.work_format!=MTLPixelFormatBGRA8Unorm?make(chain_.work_format,640,480,all,MTLStorageModePrivate):nil;
        interp_=nil;
#if RECVX_SDK_FRAME_INTERPOLATION
        if(@available(macOS 26.0,*)){
            if(settings_.frame_generation==FrameGeneration::MetalFx){
                if(!frame_generation_supported_)append_note("frame interpolation unsupported on this device/OS");
                else{
                    MTLFXFrameInterpolatorDescriptor* d=[MTLFXFrameInterpolatorDescriptor new];
                    d.colorTextureFormat=chain_.work_format;d.outputTextureFormat=chain_.work_format;d.depthTextureFormat=MTLPixelFormatDepth32Float;d.motionTextureFormat=MTLPixelFormatRG16Float;d.uiTextureFormat=chain_.work_format;
                    d.inputWidth=640;d.inputHeight=480;d.outputWidth=NSUInteger(ow);d.outputHeight=NSUInteger(oh);
                    if(chain_.temporal&&!chain_.spatial)d.scaler=(id<MTLFXTemporalScaler>)chain_.temporal; // Interpolator liest die Historie des Skalierers mit
                    chain_.interpolator=[d newFrameInterpolatorWithDevice:device_];
                    if(!chain_.interpolator&&d.scaler){d.scaler=nil;chain_.interpolator=[d newFrameInterpolatorWithDevice:device_];}
                    if(!chain_.interpolator)append_note("frame interpolator creation failed");
                    else interp_=make(chain_.work_format,ow,oh,all,MTLStorageModePrivate);
                }
            }
        }else if(settings_.frame_generation==FrameGeneration::MetalFx)append_note("frame interpolation needs macOS 26");
#else
        if(settings_.frame_generation==FrameGeneration::MetalFx)append_note("frame interpolation needs a macOS 26 SDK build");
#endif
    }
    void append_note(const std::string& note){if(!notes_.empty())notes_+="; ";notes_+=note;}
    // Skalierer fuer die Einpassungsgroesse; false, wenn MetalFX dieses Farbformat ablehnt.
    bool build_scalers(MTLPixelFormat format){
        chain_.temporal=nil;chain_.spatial=nil;chain_.temporal_scale=chain_.spatial_scale=0;chain_.out_w=640;chain_.out_h=480;
        if(settings_.upscaler==Upscaler::Off)return true;
        if(!upscaler_supported_){append_note("MetalFX upscaling unsupported on this device/OS");return true;}
        const double fit=std::min(fit_w_/640.0,fit_h_/480.0);
        if(fit<=1.0){append_note("window not larger than 640x480");return true;}
        bool ok=true;
        if(@available(macOS 13.0,*)){
            int w=640,h=480;
            if(settings_.upscaler==Upscaler::MetalFxTemporal){
                float lo=1.0f,hi=2.0f; // macOS 13: fester Bereich 1..2; ab 14 fragt das Geraet
                if(@available(macOS 14.0,*)){lo=[MTLFXTemporalScalerDescriptor supportedInputContentMinScaleForDevice:device_];hi=[MTLFXTemporalScalerDescriptor supportedInputContentMaxScaleForDevice:device_];}
                const double s=std::clamp(fit,double(std::max(lo,1.0f)),double(std::max(hi,std::max(lo,1.0f))));
                MTLFXTemporalScalerDescriptor* d=[MTLFXTemporalScalerDescriptor new];
                d.colorTextureFormat=format;d.outputTextureFormat=format;d.depthTextureFormat=MTLPixelFormatDepth32Float;d.motionTextureFormat=MTLPixelFormatRG16Float;
                d.inputWidth=640;d.inputHeight=480;d.outputWidth=NSUInteger(std::lround(640*s));d.outputHeight=NSUInteger(std::lround(480*s));
                d.autoExposureEnabled=NO;d.requiresSynchronousInitialization=YES;d.inputContentPropertiesEnabled=NO;
                chain_.temporal=[d newTemporalScalerWithDevice:device_];
                if(!chain_.temporal){ok=false;}else{chain_.temporal_scale=float(s);w=int(d.outputWidth);h=int(d.outputHeight);chain_.temporal_w=w;chain_.temporal_h=h;}
            }
            if(ok&&(settings_.upscaler==Upscaler::MetalFxSpatial||(chain_.temporal&&(w<fit_w_-1||h<fit_h_-1)))){
                // Volle Einpassung, sonst ganzzahlige Faktoren absteigend.
                std::vector<std::pair<int,int>> sizes{{fit_w_,fit_h_}};
                for(int k=4;k>=2;--k)if(w*k<=fit_w_&&h*k<=fit_h_)sizes.push_back({w*k,h*k});
                for(const auto& [ow,oh]:sizes){
                    MTLFXSpatialScalerDescriptor* d=[MTLFXSpatialScalerDescriptor new];
                    d.colorTextureFormat=format;d.outputTextureFormat=format;d.inputWidth=NSUInteger(w);d.inputHeight=NSUInteger(h);d.outputWidth=NSUInteger(ow);d.outputHeight=NSUInteger(oh);
                    d.colorProcessingMode=MTLFXSpatialScalerColorProcessingModePerceptual;
                    chain_.spatial=[d newSpatialScalerWithDevice:device_];
                    if(chain_.spatial){chain_.spatial_scale=float(ow)/float(w);w=ow;h=oh;break;}
                }
                if(!chain_.spatial&&settings_.upscaler==Upscaler::MetalFxSpatial)ok=false;
            }
            if(ok){chain_.out_w=w;chain_.out_h=h;}
        }
        if(!ok){chain_.temporal=nil;chain_.spatial=nil;chain_.temporal_scale=chain_.spatial_scale=0;chain_.out_w=640;chain_.out_h=480;}
        return ok;
    }
    CAMetalLayer* layer_;id<MTLDevice> device_;id<MTLCommandQueue> queue_;
    id<MTLComputePipelineState> motion_,blit_,present_;id<MTLRenderPipelineState> depth_pipeline_;id<MTLDepthStencilState> depth_state_;
    id<MTLTexture> color_in_,scene_in_,depth_in_,motion_tex_,depth_value_,depth_fx_,work_in_,temporal_out_,interp_,latest_real_,latest_interp_;id<MTLTexture> final_[2];
    dispatch_semaphore_t inflight_;
    VideoSettings settings_;Chain chain_;PresentStats stats_;std::string notes_;
    bool dirty_=true,upscaler_supported_=false,frame_generation_supported_=false,history_valid_=false,last_valid_=false,broken_=false;
    int drawable_w_=0,drawable_h_=0,fit_x_=0,fit_y_=0,fit_w_=640,fit_h_=480;unsigned current_=0,scene_key_=0;
    float last_view_[16]{},last_focal_[2]{},depth_near_=0,depth_far_=0;
};
}
std::unique_ptr<MetalFxPresenter> create_metalfx_presenter(void* layer_pointer,std::string& reason){@autoreleasepool{
    if(!layer_pointer){reason="SDL renderer has no Metal layer";return nullptr;}
    CAMetalLayer* layer=(__bridge CAMetalLayer*)layer_pointer;
    if(![layer isKindOfClass:[CAMetalLayer class]]){reason="SDL layer is not a CAMetalLayer";return nullptr;}
    if(!layer.device){reason="Metal layer has no device";return nullptr;}
    try{return std::make_unique<Presenter>(layer);}catch(const std::exception& e){reason=e.what();return nullptr;}
}}
}
