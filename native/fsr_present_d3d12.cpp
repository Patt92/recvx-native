// Host-Praesentationsstufe mit AMD FSR 4 (metalfx_present.h, Windows). Plattformanpassung, keine
// Spiellogik: das 640x480-Bild der Runtime wird unveraendert hochgeladen; die Stufe skaliert und
// nachbearbeitet nur die Anzeige. Gegenstueck zu metalfx_present.mm mit derselben Schnittstelle.
//
// Kette je Bild (eine Befehlsliste eines eigenen D3D12-Geraets, DXGI-Flip-Swapchain auf dem HWND des
// SDL-Fensters):
//   Upload (Farbe, Szenenbild, 1/z)  ->  motion_cs (Bewegungsvektoren, umgekehrte Tiefe, 2D-Maske)
//   -> [ffxDispatch Upscale: FSR 4 FP8/INT8, auf GPUs ohne FSR 4 der FSR-3.1-Provider derselben DLL]
//   -> present_cs (Post-Processing + 4:3-Einpassung) in den Backbuffer  ->  Present.
// FSR kommt ausschliesslich aus den signierten AMD-DLLs (ffx-api, native/third_party/FidelityFX-SDK):
// amd_fidelityfx_loader_dx12.dll (SDK 2.x) oder amd_fidelityfx_dx12.dll (SDK-1.x-Name), die den
// Upscaler-Provider amd_fidelityfx_upscaler_dx12.dll aus demselben Ordner laedt; das INT8-Modell liegt
// als gleichnamige DLL im Unterordner "FSR4 INT8". Ohne DLLs oder ohne D3D12-Geraet gibt es keine
// Stufe (SDL-Kopierpfad), keinen Ersatzfilter.
//
// Bewegungsvektoren wie die Metal-Stufe: Kamera-Reprojektion der Tiefe mit der Projektion des vorigen
// Bildes (FSR-Konvention: aktuell -> vorig in Pixeln, motionVectorScale 1). Eigenbewegung von Spieler
// und Gegnern liefert die Engine nicht (offen, docs/NATIVE_PORT.md); 2D-Pixel (Endbild != Szenenbild)
// gelten als unbewegter Vordergrund mit Tiefe 1 und werden ueber die Reaktivmaske von der Historie
// ausgenommen. Kein Subpixel-Jitter (jitterOffset 0): die 2D-Zeichner schreiben an ganzzahligen
// 640x480-Positionen. Farbe ist sRGB (FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE), Tiefe umgekehrt und
// endlich mit den njClipZ-Abstaenden; ein Schnitt (scene_key) setzt reset.
#include "metalfx_present.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#undef near
#undef far
#include "ffx_api.h"
#include "ffx_api_types.h"
#include "ffx_upscale.h"
// ffx_api_loader.h (nur _WINDOWS/MSBuild) wird nicht gebraucht: Funktionszeiger stehen in ffx_api.h, die
// Eintritte holt GetProcAddress aus der per Pfad geladenen DLL.
struct FfxFunctions{PfnFfxCreateContext CreateContext=nullptr;PfnFfxDestroyContext DestroyContext=nullptr;PfnFfxConfigure Configure=nullptr;PfnFfxQuery Query=nullptr;PfnFfxDispatch Dispatch=nullptr;};
#include "dx12/ffx_api_dx12.h"
namespace recvx {
namespace {
// Shaderquelle: motion_cs/ui wie motion_k/ui_k, present_cs/bloom wie present_k/bloom_*_k der Metal-Stufe.
const char* present_hlsl=R"HLSL(
cbuffer MotionCB : register(b0) {
    float4x4 cur_inv;    // Sicht -> Welt des aktuellen Bildes
    float4x4 prev_view;  // Welt -> Sicht des vorigen Bildes
    float2 focal_cur, focal_prev, center;
    float depth_a, depth_b; // umgekehrte endliche Tiefe d = a*(1/z) - b
    uint has_scene, has_prev, W, H;
};
Texture2D<float> DepthIn : register(t0);
Texture2D<float4> PixelsIn : register(t1);
Texture2D<float4> SceneIn : register(t2);
RWTexture2D<float2> MotionOut : register(u0);
RWTexture2D<float> DepthOut : register(u1);
RWTexture2D<float> ReactiveOut : register(u2);
// Bewegungsvektor je Pixel: wohin dieses Pixel im vorigen Bild lag (prev - cur, in Pixeln des 640x480-Bildes).
// 2D-Pixel: unbewegt, naechste Tiefe, reaktiv 1 (keine Historie).
[numthreads(8,8,1)]
void motion_cs(uint3 gid : SV_DispatchThreadID) {
    if(gid.x>=W||gid.y>=H)return;
    const float invz=DepthIn[gid.xy];
    bool ui=has_scene==0u;
    if(!ui){const float4 a=PixelsIn[gid.xy],b=SceneIn[gid.xy];ui=any(a!=b);}
    float2 mv=float2(0.0,0.0);float d=0.0;
    if(ui)d=1.0;
    else if(invz>0.0){
        d=clamp(depth_a*invz-depth_b,0.0,1.0);
        if(has_prev!=0u){
            const float z=1.0/invz;const float2 pc=float2(gid.xy)+0.5;
            const float4 v=float4((pc.x-center.x)*z/focal_cur.x,(pc.y-center.y)*z/focal_cur.y,z,1.0);
            const float4 p=mul(prev_view,mul(cur_inv,v));
            if(p.z>1e-6){const float2 pp=center+p.xy/p.z*focal_prev;mv=pp-pc;}
        }
    }
    MotionOut[gid.xy]=mv;DepthOut[gid.xy]=d;ReactiveOut[gid.xy]=ui?1.0:0.0;
}
cbuffer PresentCB : register(b1) {
    float2 src_size, fit_origin, fit_size;
    uint2 dst_size;
    uint mode, nearest, game_lines, phosphor;
    float sharpness, bloom, mask_strength, sigma_dark;
    float sigma_bright, pad0, pad1, pad2;
};
Texture2D<float4> Src : register(t3);
Texture2D<float4> BloomTex : register(t4);
RWTexture2D<float4> Dst : register(u3);
SamplerState Lin : register(s0);
SamplerState Pt : register(s1);
float3 to_linear(float3 c){return pow(max(c,0.0),2.2);}
float3 to_srgb(float3 c){return pow(max(c,0.0),1.0/2.2);}
// Weiches Knie gegen Beschneiden nach dem Helligkeitsausgleich: linear bis 0.75, darueber asymptotisch gegen 1.
float3 knee(float3 c){return c>0.75?0.75+0.25*(1.0-exp(-(c-0.75)/0.25)):c;}
// Bloom-Quelle: Hellpass in linearem Licht, Viertelaufloesung (4x4-Box ueber bilineare 2x2-Taps).
RWTexture2D<float4> BloomDst : register(u4);
[numthreads(8,8,1)]
void bloom_down_cs(uint3 gid : SV_DispatchThreadID) {
    uint w,h;BloomDst.GetDimensions(w,h);if(gid.x>=w||gid.y>=h)return;
    const float2 t=1.0/float2(w,h);const float2 uv=(float2(gid.xy)+0.5)*t;const float2 hh=t*0.25;
    float3 c=to_linear(Src.SampleLevel(Lin,uv+float2(-hh.x,-hh.y),0).rgb)+to_linear(Src.SampleLevel(Lin,uv+float2(hh.x,-hh.y),0).rgb)+to_linear(Src.SampleLevel(Lin,uv+float2(-hh.x,hh.y),0).rgb)+to_linear(Src.SampleLevel(Lin,uv+float2(hh.x,hh.y),0).rgb);
    c*=0.25;const float l=dot(c,float3(0.2126,0.7152,0.0722));
    BloomDst[gid.xy]=float4(c*smoothstep(0.5,0.9,l),1.0);
}
// Trennbare Gauss-Unschaerfe (9 Taps), dir = (1,0) oder (0,1) in Texeln.
cbuffer BlurCB : register(b2) { float2 dir; float2 blur_pad; };
static const float blur_w[5]={0.2270270270,0.1945945946,0.1216216216,0.0540540541,0.0162162162};
[numthreads(8,8,1)]
void bloom_blur_cs(uint3 gid : SV_DispatchThreadID) {
    uint w,h;BloomDst.GetDimensions(w,h);if(gid.x>=w||gid.y>=h)return;
    const float2 t=1.0/float2(w,h);const float2 uv=(float2(gid.xy)+0.5)*t;
    float3 c=BloomTex.SampleLevel(Lin,uv,0).rgb*blur_w[0];
    for(int i=1;i<5;++i){const float2 o=dir*t*float(i);c+=BloomTex.SampleLevel(Lin,uv+o,0).rgb*blur_w[i];c+=BloomTex.SampleLevel(Lin,uv-o,0).rgb*blur_w[i];}
    BloomDst[gid.xy]=float4(c,1.0);
}
[numthreads(8,8,1)]
void present_cs(uint3 gid : SV_DispatchThreadID) {
    if(gid.x>=dst_size.x||gid.y>=dst_size.y)return;
    const float2 p=float2(gid.xy)+0.5-fit_origin;
    if(p.x<0.0||p.y<0.0||p.x>=fit_size.x||p.y>=fit_size.y){Dst[gid.xy]=float4(0.0,0.0,0.0,1.0);return;}
    const float2 uv=p/fit_size;
    float3 c=(nearest!=0u?Src.SampleLevel(Pt,uv,0):Src.SampleLevel(Lin,uv,0)).rgb;
    if(sharpness>0.0){
        const float2 t=1.0/src_size;
        const float3 a=Src.SampleLevel(Lin,uv+float2(0.0,-t.y),0).rgb,b=Src.SampleLevel(Lin,uv+float2(-t.x,0.0),0).rgb,d=Src.SampleLevel(Lin,uv+float2(t.x,0.0),0).rgb,e=Src.SampleLevel(Lin,uv+float2(0.0,t.y),0).rgb;
        const float3 mn=min(min(min(a,b),min(d,e)),c),mx=max(max(max(a,b),max(d,e)),c);
        const float3 amp=sqrt(clamp(min(mn,1.0-mx)/max(mx,1e-4),0.0,1.0));
        const float3 w=amp*(-1.0/(8.0-3.0*sharpness));
        c=clamp(((a+b+d+e)*w+c)/(4.0*w+1.0),0.0,1.0);
    }
    if(mode!=0u){
        float3 l=to_linear(c);
        // Strahlprofil: Abstand zur Mitte der Spielzeile in Zeilenbreiten; hellere Zeilen sind breiter, so dass die
        // Kompensation des Profils bei Weiss gegen 1 geht und Lichter nicht beschnitten werden.
        const float line=p.y/fit_size.y*float(game_lines);const float f=frac(line)-0.5;
        const float lum=dot(l,float3(0.2126,0.7152,0.0722));const float sigma=lerp(sigma_dark,sigma_bright,clamp(lum,0.0,1.0));
        const float beam=exp(-f*f/(2.0*sigma*sigma));const float beam_gain=lerp(1.0,1.0/min(1.0,sigma*2.5066283),0.5);
        // Maske: light = je Ausgabepixel abwechselnd R/G/B gedaempft; full = Schlitzmaske je Leuchtpunkt (phosphor px).
        float3 mask=float3(1.0,1.0,1.0);
        if(mode==1u){const uint k=gid.x%3u;mask=float3(k==0u?1.0:1.0-mask_strength,k==1u?1.0:1.0-mask_strength,k==2u?1.0:1.0-mask_strength);}
        else{const uint k=(gid.x/phosphor)%3u;const float stagger=((gid.x/(phosphor*3u))&1u)!=0u?0.5:0.0;const float sl=frac(line+stagger);const float slot=sl<0.08?1.0-mask_strength*0.5:1.0;
             mask=float3(k==0u?1.0:1.0-mask_strength,k==1u?1.0:1.0-mask_strength,k==2u?1.0:1.0-mask_strength)*slot;}
        l=l*beam*beam_gain*mask;
        if(bloom>0.0)l+=BloomTex.SampleLevel(Lin,uv,0).rgb*bloom;
        c=to_srgb(mode==2u?knee(l):l);
    }
    Dst[gid.xy]=float4(c,1.0);
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
    void** put_void(){reset();return reinterpret_cast<void**>(&p);}
    T* get()const{return p;}
    explicit operator bool()const{return p!=nullptr;}
    T* operator->()const{return p;}
};
void require(HRESULT hr,const char* what){
    if(FAILED(hr)){char text[160];std::snprintf(text,sizeof text,"FSR presentation: %s (0x%08lx)",what,static_cast<unsigned long>(hr));throw std::runtime_error(text);}}
void require_ffx(ffxReturnCode_t code,const char* what){
    if(code!=FFX_API_RETURN_OK){char text[160];std::snprintf(text,sizeof text,"FSR presentation: %s (ffx %u)",what,unsigned(code));throw std::runtime_error(text);}}
struct alignas(16) MotionCB{float cur_inv[16],prev_view[16];float focal_cur[2],focal_prev[2],center[2];float depth_a,depth_b;std::uint32_t has_scene,has_prev,W,H;};
static_assert(sizeof(MotionCB)==128+24+8+16,"motion constant layout");
struct alignas(16) PresentCB{float src_size[2],fit_origin[2],fit_size[2];std::uint32_t dst_size[2];std::uint32_t mode,nearest,game_lines,phosphor;float sharpness,bloom,mask_strength,sigma_dark;float sigma_bright,pad[3];};
static_assert(sizeof(PresentCB)==80,"present constant layout");
struct alignas(16) BlurCB{float dir[2],pad[2];};
using PFN_CreateDevice=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);
using PFN_SerializeRootSignature=HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*,D3D_ROOT_SIGNATURE_VERSION,ID3DBlob**,ID3DBlob**);
using PFN_CreateFactory2=HRESULT(WINAPI*)(UINT,REFIID,void**);
using PFN_Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
// Spaltenmatrix (NJS_MATRIX-Reihenfolge wie die Metal-Stufe) invertieren; HLSL-float4x4 wird mit mul(M,v)
// als Zeilenmatrix gelesen, deshalb beim Schreiben in die Konstanten transponieren.
void invert4(const float* m,float* out){
    float inv[16];
    inv[0]=m[5]*m[10]*m[15]-m[5]*m[11]*m[14]-m[9]*m[6]*m[15]+m[9]*m[7]*m[14]+m[13]*m[6]*m[11]-m[13]*m[7]*m[10];
    inv[4]=-m[4]*m[10]*m[15]+m[4]*m[11]*m[14]+m[8]*m[6]*m[15]-m[8]*m[7]*m[14]-m[12]*m[6]*m[11]+m[12]*m[7]*m[10];
    inv[8]=m[4]*m[9]*m[15]-m[4]*m[11]*m[13]-m[8]*m[5]*m[15]+m[8]*m[7]*m[13]+m[12]*m[5]*m[11]-m[12]*m[7]*m[9];
    inv[12]=-m[4]*m[9]*m[14]+m[4]*m[10]*m[13]+m[8]*m[5]*m[14]-m[8]*m[6]*m[13]-m[12]*m[5]*m[10]+m[12]*m[6]*m[9];
    inv[1]=-m[1]*m[10]*m[15]+m[1]*m[11]*m[14]+m[9]*m[2]*m[15]-m[9]*m[3]*m[14]-m[13]*m[2]*m[11]+m[13]*m[3]*m[10];
    inv[5]=m[0]*m[10]*m[15]-m[0]*m[11]*m[14]-m[8]*m[2]*m[15]+m[8]*m[3]*m[14]+m[12]*m[2]*m[11]-m[12]*m[3]*m[10];
    inv[9]=-m[0]*m[9]*m[15]+m[0]*m[11]*m[13]+m[8]*m[1]*m[15]-m[8]*m[3]*m[13]-m[12]*m[1]*m[11]+m[12]*m[3]*m[9];
    inv[13]=m[0]*m[9]*m[14]-m[0]*m[10]*m[13]-m[8]*m[1]*m[14]+m[8]*m[2]*m[13]+m[12]*m[1]*m[10]-m[12]*m[2]*m[9];
    inv[2]=m[1]*m[6]*m[15]-m[1]*m[7]*m[14]-m[5]*m[2]*m[15]+m[5]*m[3]*m[14]+m[13]*m[2]*m[7]-m[13]*m[3]*m[6];
    inv[6]=-m[0]*m[6]*m[15]+m[0]*m[7]*m[14]+m[4]*m[2]*m[15]-m[4]*m[3]*m[14]-m[12]*m[2]*m[7]+m[12]*m[3]*m[6];
    inv[10]=m[0]*m[5]*m[15]-m[0]*m[7]*m[13]-m[4]*m[1]*m[15]+m[4]*m[3]*m[13]+m[12]*m[1]*m[7]-m[12]*m[3]*m[5];
    inv[14]=-m[0]*m[5]*m[14]+m[0]*m[6]*m[13]+m[4]*m[1]*m[14]-m[4]*m[2]*m[13]-m[12]*m[1]*m[6]+m[12]*m[2]*m[5];
    inv[3]=-m[1]*m[6]*m[11]+m[1]*m[7]*m[10]+m[5]*m[2]*m[11]-m[5]*m[3]*m[10]-m[9]*m[2]*m[7]+m[9]*m[3]*m[6];
    inv[7]=m[0]*m[6]*m[11]-m[0]*m[7]*m[10]-m[4]*m[2]*m[11]+m[4]*m[3]*m[10]+m[8]*m[2]*m[7]-m[8]*m[3]*m[6];
    inv[11]=-m[0]*m[5]*m[11]+m[0]*m[7]*m[9]+m[4]*m[1]*m[11]-m[4]*m[3]*m[9]-m[8]*m[1]*m[7]+m[8]*m[3]*m[5];
    inv[15]=m[0]*m[5]*m[10]-m[0]*m[6]*m[9]-m[4]*m[1]*m[10]+m[4]*m[2]*m[9]+m[8]*m[1]*m[6]-m[8]*m[2]*m[5];
    const float det=m[0]*inv[0]+m[1]*inv[4]+m[2]*inv[8]+m[3]*inv[12];
    const float s=det!=0.0f?1.0f/det:0.0f;
    for(int i=0;i<16;++i)out[i]=inv[i]*s;
}
void transpose4(const float* m,float* out){for(int c=0;c<4;++c)for(int r=0;r<4;++r)out[r*4+c]=m[c*4+r];}
struct Pending{std::vector<std::uint32_t> pixels,scene;std::vector<float> depth;bool has_scene=false;PresentProjection projection;unsigned scene_key=0;double delta=0;int slots=1;};
// Deskriptorplaetze im sichtbaren Heap (CBV_SRV_UAV): feste Belegung je Bild.
enum Slot{SlotDepthIn=0,SlotPixelsIn,SlotSceneIn,SlotMotionOut,SlotDepthOut,SlotReactiveOut,SlotSrc,SlotBloomTex,SlotDst,SlotBloomDst,SlotCount};
enum Root{RootMotionCB=0,RootPresentCB,RootBlurCB,RootSrvTable,RootUavTable,RootSamplers};
class FsrPresenter final:public MetalFxPresenter{
public:
    FsrPresenter(HWND hwnd,const std::string& sdk_dir,const VideoSettings& initial,Com<ID3D12Device> device,PFN_Compile compile,PFN_SerializeRootSignature serialize,PFN_CreateFactory2 factory2,std::string adapter)
        :hwnd_(hwnd),sdk_dir_(sdk_dir),device_(std::move(device)),compile_(compile),adapter_(std::move(adapter)),settings_(initial),requested_(initial){
        {D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;require(device_->CreateCommandQueue(&qd,__uuidof(ID3D12CommandQueue),queue_.put_void()),"command queue");}
        for(auto& a:allocators_)require(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,__uuidof(ID3D12CommandAllocator),a.put_void()),"command allocator");
        require(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators_[0].get(),nullptr,__uuidof(ID3D12GraphicsCommandList),cmd_.put_void()),"command list");
        require(cmd_->Close(),"close initial command list");
        require(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,__uuidof(ID3D12Fence),fence_.put_void()),"fence");
        event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event_)throw std::runtime_error("FSR presentation: fence event");
        descriptor_size_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        {D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=SlotCount*frames_in_flight;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
         require(device_->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),heap_.put_void()),"descriptor heap");}
        {D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
         require(device_->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),sampler_heap_.put_void()),"sampler heap");
         const UINT step=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
         for(unsigned i=0;i<2;++i){D3D12_SAMPLER_DESC sd{};sd.Filter=i==0?D3D12_FILTER_MIN_MAG_MIP_LINEAR:D3D12_FILTER_MIN_MAG_MIP_POINT;
            sd.AddressU=sd.AddressV=sd.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sd.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;sd.MaxLOD=D3D12_FLOAT32_MAX;
            auto h=sampler_heap_->GetCPUDescriptorHandleForHeapStart();h.ptr+=SIZE_T(i)*step;device_->CreateSampler(&sd,h);}}
        {D3D12_DESCRIPTOR_RANGE ranges[3]{};
         ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;ranges[0].NumDescriptors=5;ranges[0].BaseShaderRegister=0;           // t0..t4
         ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;ranges[1].NumDescriptors=5;ranges[1].BaseShaderRegister=0;           // u0..u4
         ranges[2].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;ranges[2].NumDescriptors=2;ranges[2].BaseShaderRegister=0;       // s0, s1
         D3D12_ROOT_PARAMETER params[6]{};
         const UINT sizes[3]={sizeof(MotionCB)/4,sizeof(PresentCB)/4,sizeof(BlurCB)/4};
         for(int i=0;i<3;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[i].Constants.ShaderRegister=UINT(i);params[i].Constants.Num32BitValues=sizes[i];params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;}
         for(int i=0;i<3;++i){params[3+i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[3+i].DescriptorTable.NumDescriptorRanges=1;params[3+i].DescriptorTable.pDescriptorRanges=&ranges[i];params[3+i].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;}
         D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=6;rd.pParameters=params;
         Com<ID3DBlob> blob,errors;
         if(FAILED(serialize(&rd,D3D_ROOT_SIGNATURE_VERSION_1,blob.put(),errors.put())))throw std::runtime_error(std::string("FSR presentation root signature: ")+(errors?static_cast<const char*>(errors->GetBufferPointer()):"serialize failed"));
         require(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),__uuidof(ID3D12RootSignature),root_.put_void()),"root signature");}
        motion_pso_=compute("motion_cs");present_pso_=compute("present_cs");bloom_down_pso_=compute("bloom_down_cs");bloom_blur_pso_=compute("bloom_blur_cs");
        // Feste 640x480-Eingaben und Zwischenbilder der Stufe.
        color_in_=texture(640,480,DXGI_FORMAT_B8G8R8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,"colour input");
        scene_in_=texture(640,480,DXGI_FORMAT_B8G8R8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,"scene input");
        depth_in_=texture(640,480,DXGI_FORMAT_R32_FLOAT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,"depth input");
        motion_=texture(640,480,DXGI_FORMAT_R16G16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"motion vectors");
        depth_fx_=texture(640,480,DXGI_FORMAT_R32_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"depth for FSR");
        reactive_=texture(640,480,DXGI_FORMAT_R8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"reactive mask");
        for(auto& u:uploads_)u=upload_buffer(upload_pitch*480*3,"upload buffer");
        // Swapchain auf dem SDL-Fenster: Flip-Modell, drei Puffer, Tearing fuer "unlimited" wenn das System es kann.
        {Com<IDXGIFactory4> factory;require(factory2(0,__uuidof(IDXGIFactory4),factory.put_void()),"DXGI factory");
         Com<IDXGIFactory5> factory5;BOOL tearing=FALSE;
         if(SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory5),factory5.put_void()))&&factory5)
             if(FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&tearing,sizeof tearing)))tearing=FALSE;
         tearing_=tearing!=FALSE;
         RECT rc{};GetClientRect(hwnd,&rc);backbuffer_w_=std::max<int>(1,rc.right-rc.left);backbuffer_h_=std::max<int>(1,rc.bottom-rc.top);
         DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=UINT(backbuffer_w_);sd.Height=UINT(backbuffer_h_);sd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;sd.SampleDesc.Count=1;
         sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_UNORDERED_ACCESS;sd.BufferCount=backbuffers;sd.Scaling=DXGI_SCALING_NONE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
         sd.AlphaMode=DXGI_ALPHA_MODE_IGNORE;sd.Flags=tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0u;
         Com<IDXGISwapChain1> chain1;require(factory->CreateSwapChainForHwnd(queue_.get(),hwnd,&sd,nullptr,nullptr,chain1.put()),"swap chain");
         require(chain1->QueryInterface(__uuidof(IDXGISwapChain3),swapchain_.put_void()),"swap chain 3");
         factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);
         acquire_backbuffers();}
        load_ffx();
        worker_=std::thread([this]{run();});
    }
    ~FsrPresenter()override{
        {std::lock_guard<std::mutex> lock(mutex_);stop_=true;}cv_.notify_all();if(worker_.joinable())worker_.join();
        wait_idle();destroy_context();
        if(event_)CloseHandle(event_);
        if(upscaler_module_)FreeLibrary(upscaler_module_);if(loader_module_)FreeLibrary(loader_module_);
    }
    void configure(const VideoSettings& settings)override{std::lock_guard<std::mutex> lock(mutex_);requested_=settings;requested_dirty_=true;}
    bool upscaler_supported()const override{return upscaler_supported_;}
    bool frame_generation_supported()const override{return false;}
    const PresentStats& stats()const override{std::lock_guard<std::mutex> lock(state_mutex_);stats_copy_=stats_;return stats_copy_;}
    std::string describe()const override{
        std::lock_guard<std::mutex> lock(state_mutex_);
        std::ostringstream out;
        if(context_)out<<provider_name_<<" "<<out_w_<<"x"<<out_h_;else out<<"nearest";
        if(settings_.sharpness!=Sharpness::Off)out<<", sharpen "<<settings_.sharpness_amount();
        if(settings_.post_process==PostProcess::CrtLight)out<<", CRT light";else if(settings_.post_process==PostProcess::CrtFull)out<<", CRT full (phosphor "<<phosphor_<<"px, bloom)";
        if(!notes_.empty())out<<" ("<<notes_<<")";
        return out.str();
    }
    // Hauptthread: Bild kopieren und den Praesentations-Thread wecken. Ein noch nicht abgeholtes Bild wird ersetzt.
    bool present(const PresentFrame& frame)override{
        if(broken_.load()||frame.width!=640||frame.height!=480||!frame.pixels||!frame.depth)return false;
        {std::lock_guard<std::mutex> lock(mutex_);
         const std::size_t n=640*480;staging_.pixels.assign(frame.pixels,frame.pixels+n);staging_.depth.assign(frame.depth,frame.depth+n);
         staging_.has_scene=frame.projection.valid&&frame.scene;if(staging_.has_scene)staging_.scene.assign(frame.scene,frame.scene+n);
         staging_.projection=frame.projection;staging_.scene_key=frame.scene_key;staging_.delta=frame.delta_seconds;staging_.slots=frame.slots;staging_new_=true;}
        cv_.notify_all();return true;
    }
private:
    static constexpr unsigned frames_in_flight=2,backbuffers=3;static constexpr std::size_t upload_pitch=(640*4+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    Com<ID3D12PipelineState> compute(const char* entry){
        Com<ID3DBlob> code,errors;
        const HRESULT hr=compile_(present_hlsl,std::strlen(present_hlsl),"fsr_present",nullptr,nullptr,entry,"cs_5_1",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code.put(),errors.put());
        if(FAILED(hr))throw std::runtime_error(std::string("FSR presentation shader ")+entry+": "+(errors?static_cast<const char*>(errors->GetBufferPointer()):"compile failed"));
        D3D12_COMPUTE_PIPELINE_STATE_DESC cd{};cd.pRootSignature=root_.get();cd.CS={code->GetBufferPointer(),code->GetBufferSize()};
        Com<ID3D12PipelineState> pso;require(device_->CreateComputePipelineState(&cd,__uuidof(ID3D12PipelineState),pso.put_void()),entry);return pso;}
    Com<ID3D12Resource> texture(UINT w,UINT h,DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES state,const char* what){
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Flags=flags;
        Com<ID3D12Resource> r;require(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,__uuidof(ID3D12Resource),r.put_void()),what);return r;}
    Com<ID3D12Resource> upload_buffer(UINT64 bytes,const char* what){
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        Com<ID3D12Resource> r;require(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,__uuidof(ID3D12Resource),r.put_void()),what);return r;}
    void transition(ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){
        if(from==to)return;D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=r;b.Transition.StateBefore=from;b.Transition.StateAfter=to;
        b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;cmd_->ResourceBarrier(1,&b);}
    void uav_barrier(){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;cmd_->ResourceBarrier(1,&b);}
    D3D12_CPU_DESCRIPTOR_HANDLE cpu(unsigned slot)const{auto h=heap_->GetCPUDescriptorHandleForHeapStart();h.ptr+=SIZE_T(frame_index_*SlotCount+slot)*descriptor_size_;return h;}
    D3D12_GPU_DESCRIPTOR_HANDLE gpu(unsigned slot)const{auto h=heap_->GetGPUDescriptorHandleForHeapStart();h.ptr+=UINT64(frame_index_*SlotCount+slot)*descriptor_size_;return h;}
    void srv(unsigned slot,ID3D12Resource* r,DXGI_FORMAT format){
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=format;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;
        device_->CreateShaderResourceView(r,&d,cpu(slot));}
    void uav(unsigned slot,ID3D12Resource* r,DXGI_FORMAT format){
        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=format;d.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device_->CreateUnorderedAccessView(r,nullptr,&d,cpu(slot));}
    void acquire_backbuffers(){
        for(unsigned i=0;i<backbuffers;++i)require(swapchain_->GetBuffer(i,__uuidof(ID3D12Resource),backbuffers_[i].put_void()),"back buffer");}
    void wait_idle(){
        const UINT64 value=++fence_value_;if(FAILED(queue_->Signal(fence_.get(),value)))return;
        if(fence_->GetCompletedValue()<value&&SUCCEEDED(fence_->SetEventOnCompletion(value,event_)))WaitForSingleObject(event_,INFINITE);}
    void wait_for(UINT64 value){if(value&&fence_->GetCompletedValue()<value){require(fence_->SetEventOnCompletion(value,event_),"fence event");WaitForSingleObject(event_,INFINITE);}}
    // ffx-api: Loader-DLL (laedt den Upscaler-Provider aus demselben Ordner) oder direkt der Provider.
    // Der Loader laedt amd_fidelityfx_upscaler_dx12.dll per Namen (LoadLibraryA). Das INT8-Modell ist eine
    // gleichnamige DLL im Unterordner "FSR4 INT8": die gewuenschte Variante wird vorab per Pfad geladen, der
    // Loader findet dann das bereits geladene Modul. Ein Wechsel FP8 <-> INT8 braucht einen Neustart der Stufe.
    void load_ffx(){
        const auto try_load=[&](const std::string& dir,const char* name)->HMODULE{
            const std::string path=dir+"\\"+name;return LoadLibraryExA(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);};
        int8_loaded_=requested_.upscaler==Upscaler::Fsr4Int8;
        const std::string upscaler_dir=int8_loaded_?sdk_dir_+"\\FSR4 INT8":sdk_dir_;
        upscaler_module_=try_load(upscaler_dir,"amd_fidelityfx_upscaler_dx12.dll");
        if(!upscaler_module_&&int8_loaded_){append_note("INT8 upscaler DLL not found in "+upscaler_dir+", using FP8");int8_loaded_=false;upscaler_module_=try_load(sdk_dir_,"amd_fidelityfx_upscaler_dx12.dll");}
        loader_module_=try_load(sdk_dir_,"amd_fidelityfx_loader_dx12.dll");
        if(!loader_module_)loader_module_=try_load(sdk_dir_,"amd_fidelityfx_dx12.dll");
        if(!loader_module_){upscaler_supported_=false;append_note("FidelityFX loader DLL not found in "+sdk_dir_);return;}
        ffx_.CreateContext=reinterpret_cast<PfnFfxCreateContext>(reinterpret_cast<void*>(GetProcAddress(loader_module_,"ffxCreateContext")));
        ffx_.DestroyContext=reinterpret_cast<PfnFfxDestroyContext>(reinterpret_cast<void*>(GetProcAddress(loader_module_,"ffxDestroyContext")));
        ffx_.Configure=reinterpret_cast<PfnFfxConfigure>(reinterpret_cast<void*>(GetProcAddress(loader_module_,"ffxConfigure")));
        ffx_.Query=reinterpret_cast<PfnFfxQuery>(reinterpret_cast<void*>(GetProcAddress(loader_module_,"ffxQuery")));
        ffx_.Dispatch=reinterpret_cast<PfnFfxDispatch>(reinterpret_cast<void*>(GetProcAddress(loader_module_,"ffxDispatch")));
        if(!ffx_.CreateContext||!ffx_.DestroyContext||!ffx_.Configure||!ffx_.Query||!ffx_.Dispatch){upscaler_supported_=false;append_note("FidelityFX loader DLL without ffx-api entry points");return;}
        // Verfuegbare Upscaler-Versionen des Providers (FSR 4, FSR 3.1.x, FSR 2.x); ohne Eintrag kein Skalierer.
        std::uint64_t count=0;
        ffxQueryDescGetVersions q{};q.header.type=FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;q.createDescType=FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;q.device=device_.get();q.outputCount=&count;
        if(ffx_.Query(nullptr,&q.header)!=FFX_API_RETURN_OK||count==0){upscaler_supported_=false;append_note("no FidelityFX upscaler provider (amd_fidelityfx_upscaler_dx12.dll missing?)");return;}
        version_ids_.assign(count,0);std::vector<const char*> names(count,nullptr);q.versionIds=version_ids_.data();q.versionNames=names.data();
        if(ffx_.Query(nullptr,&q.header)!=FFX_API_RETURN_OK){upscaler_supported_=false;append_note("FidelityFX version query failed");return;}
        version_names_.clear();for(std::uint64_t i=0;i<count;++i)version_names_.push_back(names[i]?names[i]:"");
        upscaler_supported_=true;
    }
    static void WINAPI ffx_message(std::uint32_t type,const wchar_t* message){
        char text[512]{};WideCharToMultiByte(CP_UTF8,0,message,-1,text,sizeof text-1,nullptr,nullptr);
        std::fprintf(stderr,"FSR %s: %s\n",type==FFX_API_MESSAGE_TYPE_ERROR?"error":"warning",text);}
    void destroy_context(){if(context_){ffx_.DestroyContext(&context_,nullptr);context_=nullptr;}}
    // Kontext fuer die Einpassungsgroesse. Versionswahl: der erste Eintrag ist der neueste Provider (FSR 4);
    // FSR 4 verlangt eine passende GPU/Treiber, sonst schlaegt ffxCreateContext fehl und der naechste
    // Eintrag (FSR 3.1.x) wird genommen. Beide sind in der DLL ("Fallback des Upscaler-Providers").
    bool build_context(){
        destroy_context();provider_name_.clear();
        if(settings_.upscaler!=Upscaler::Fsr4&&settings_.upscaler!=Upscaler::Fsr4Int8)return true;
        if(!upscaler_supported_){append_note("FSR upscaling unavailable (no FidelityFX DLLs)");return true;}
        const double fit=std::min(fit_w_/640.0,fit_h_/480.0);
        if(fit<=1.0){append_note("window not larger than 640x480");return true;}
        out_w_=fit_w_;out_h_=fit_h_;
        for(std::size_t i=0;i<version_ids_.size();++i){
            ffxCreateBackendDX12Desc backend{};backend.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;backend.device=device_.get();
            ffxCreateContextDescUpscaleVersion version{};version.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;version.version=FFX_UPSCALER_VERSION;
            ffxOverrideVersion override{};override.header.type=FFX_API_DESC_TYPE_OVERRIDE_VERSION;override.versionId=version_ids_[i];
            ffxCreateContextDescUpscale desc{};desc.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
            desc.flags=FFX_UPSCALE_ENABLE_DEPTH_INVERTED|FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE;
            desc.maxRenderSize={640,480};desc.maxUpscaleSize={std::uint32_t(out_w_),std::uint32_t(out_h_)};desc.fpMessage=&FsrPresenter::ffx_message;
            desc.header.pNext=&backend.header;backend.header.pNext=&version.header;version.header.pNext=&override.header;
            const auto code=ffx_.CreateContext(&context_,&desc.header,nullptr);
            if(code==FFX_API_RETURN_OK&&context_){
                ffxQueryGetProviderVersion pv{};pv.header.type=FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
                provider_name_=ffx_.Query(&context_,&pv.header)==FFX_API_RETURN_OK&&pv.versionName?pv.versionName:version_names_[i];
                provider_name_+=int8_loaded_?" INT8":" FP8";
                if((settings_.upscaler==Upscaler::Fsr4Int8)!=int8_loaded_)append_note("FP8/INT8 model switch applies after restart");
                if(i>0)append_note("FSR 4 unavailable on this GPU/driver, using "+provider_name_);
                history_valid_=false;return true;}
            context_=nullptr;
        }
        append_note("no FidelityFX upscaler version could be created on this device");return false;
    }
    void append_note(const std::string& note){if(!notes_.empty())notes_+="; ";notes_+=note;}
    void fail(const std::string& what){broken_.store(true);notes_="disabled: "+what;std::fprintf(stderr,"FSR presentation disabled: %s\n",what.c_str());}
    // Einpassung und Zwischenbilder fuer die Fenstergroesse; Swapchain bei Groessenaenderung neu.
    void rebuild(int dw,int dh){
        notes_.clear();
        if(dw!=backbuffer_w_||dh!=backbuffer_h_){
            wait_idle();for(auto& b:backbuffers_)b.reset();
            require(swapchain_->ResizeBuffers(backbuffers,UINT(dw),UINT(dh),DXGI_FORMAT_B8G8R8A8_UNORM,tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0u),"resize swap chain");
            acquire_backbuffers();backbuffer_w_=dw;backbuffer_h_=dh;}
        const double aw=settings_.widescreen?640.0*4.0/3.0:640.0; /* Host-Mod Breitbild: 16:9-Einpassung (widescreen.h) */
        const double scale=std::min(dw/aw,dh/480.0);fit_w_=std::max(1,int(std::lround(aw*scale)));fit_h_=std::max(1,int(std::lround(480*scale)));fit_x_=(dw-fit_w_)/2;fit_y_=(dh-fit_h_)/2;
        out_w_=640;out_h_=480;
        if(!build_context())destroy_context();
        if(context_){upscaled_=texture(UINT(out_w_),UINT(out_h_),DXGI_FORMAT_B8G8R8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"upscaled");}
        else upscaled_.reset();
        phosphor_=std::max(1,int(std::lround(fit_w_/640.0/2.0)));
        if(settings_.post_process==PostProcess::CrtFull){
            bloom_a_=texture(UINT(std::max(1,fit_w_/4)),UINT(std::max(1,fit_h_/4)),DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"bloom a");
            bloom_b_=texture(UINT(std::max(1,fit_w_/4)),UINT(std::max(1,fit_h_/4)),DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"bloom b");}
        else{bloom_a_.reset();bloom_b_.reset();}
        if(!blank_)blank_=texture(1,1,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,"blank");
        dirty_=false;
    }
    // Praesentations-Thread: wie die Metal-Stufe ein Bild in slots Zeitfenstern (ohne Frame-Generation nur
    // Wiederholungen) bis zum naechsten Bild; unlimited (slots 0) praesentiert fortlaufend.
    void run(){
        using Clock=std::chrono::steady_clock;Pending job;bool have=false;Clock::time_point frame_at{};int slot=0,slots=1;double delta=1.0/30;
        for(;;){
            bool take=false;
            {std::unique_lock<std::mutex> lock(mutex_);
             const bool pending=have&&(slots==0||slot+1<slots);
             if(!pending)cv_.wait(lock,[&]{return stop_||staging_new_;});
             else{const auto deadline=frame_at+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(slots==0?0.0:double(slot+1)*delta/slots));cv_.wait_until(lock,deadline,[&]{return stop_||staging_new_;});}
             if(stop_)return;
             if(staging_new_){std::swap(job,staging_);staging_new_=false;take=true;}
             if(requested_dirty_){std::lock_guard<std::mutex> state(state_mutex_);settings_=requested_;dirty_=true;requested_dirty_=false;}}
            if(broken_.load()){have=false;continue;}
            {std::lock_guard<std::mutex> state(state_mutex_);
             try{
                 if(take){slots=job.slots;delta=job.delta>0?job.delta:1.0/30;slot=0;frame_at=Clock::now();have=process(job);}
                 else{if(slots>0)++slot;represent();}
             }catch(const std::exception& e){fail(e.what());have=false;}}
        }
    }
    void begin(){
        frame_index_=(frame_index_+1)%frames_in_flight;wait_for(frame_fence_[frame_index_]);
        require(allocators_[frame_index_]->Reset(),"reset allocator");require(cmd_->Reset(allocators_[frame_index_].get(),nullptr),"reset command list");
        ID3D12DescriptorHeap* heaps[]={heap_.get(),sampler_heap_.get()};cmd_->SetDescriptorHeaps(2,heaps);
        cmd_->SetComputeRootSignature(root_.get());cmd_->SetComputeRootDescriptorTable(RootSrvTable,gpu(0));cmd_->SetComputeRootDescriptorTable(RootUavTable,gpu(SlotMotionOut));
        cmd_->SetComputeRootDescriptorTable(RootSamplers,sampler_heap_->GetGPUDescriptorHandleForHeapStart());
    }
    void submit(){
        require(cmd_->Close(),"close command list");ID3D12CommandList* lists[]={cmd_.get()};queue_->ExecuteCommandLists(1,lists);
        frame_fence_[frame_index_]=++fence_value_;require(queue_->Signal(fence_.get(),frame_fence_[frame_index_]),"signal fence");
    }
    void upload(const Pending& job){
        void* mapped=nullptr;const D3D12_RANGE none{0,0};require(uploads_[frame_index_]->Map(0,&none,&mapped),"map upload");
        char* base=static_cast<char*>(mapped);
        const auto rows=[&](std::size_t offset,const void* src){for(int y=0;y<480;++y)std::memcpy(base+offset+std::size_t(y)*upload_pitch,static_cast<const char*>(src)+std::size_t(y)*640*4,640*4);};
        rows(0,job.pixels.data());rows(upload_pitch*480,job.has_scene?job.scene.data():job.pixels.data());rows(upload_pitch*480*2,job.depth.data());
        uploads_[frame_index_]->Unmap(0,nullptr);
        const auto copy=[&](ID3D12Resource* dst,DXGI_FORMAT format,std::size_t offset,D3D12_RESOURCE_STATES before){
            transition(dst,before,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION d{};d.pResource=dst;d.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;d.SubresourceIndex=0;
            D3D12_TEXTURE_COPY_LOCATION s{};s.pResource=uploads_[frame_index_].get();s.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            s.PlacedFootprint.Offset=offset;s.PlacedFootprint.Footprint.Format=format;s.PlacedFootprint.Footprint.Width=640;s.PlacedFootprint.Footprint.Height=480;s.PlacedFootprint.Footprint.Depth=1;s.PlacedFootprint.Footprint.RowPitch=UINT(upload_pitch);
            cmd_->CopyTextureRegion(&d,0,0,0,&s,nullptr);
            transition(dst,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);};
        const auto before=inputs_uploaded_?D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COPY_DEST;
        copy(color_in_.get(),DXGI_FORMAT_B8G8R8A8_UNORM,0,before);copy(scene_in_.get(),DXGI_FORMAT_B8G8R8A8_UNORM,upload_pitch*480,before);copy(depth_in_.get(),DXGI_FORMAT_R32_FLOAT,upload_pitch*480*2,before);
        inputs_uploaded_=true;
    }
    void encode_motion(const Pending& job,bool reset){
        MotionCB u{};u.W=640;u.H=480;u.center[0]=320.0f;u.center[1]=240.0f;u.has_scene=job.has_scene?1u:0u;
        const auto& p=job.projection;
        if(p.valid){
            // Umgekehrte endliche Tiefe mit den njClipZ-Abstaenden; far 0 (unbegrenzt) als 100000.
            const float near_z=std::max(p.near,1e-3f),far_z=p.far>near_z?p.far:100000.0f;
            depth_near_=near_z;depth_far_=far_z;u.depth_a=near_z*far_z/(far_z-near_z);u.depth_b=near_z/(far_z-near_z);
            u.focal_cur[0]=p.focal_x;u.focal_cur[1]=p.focal_y;
            float inv[16];invert4(p.view,inv);transpose4(inv,u.cur_inv);
            if(!reset&&last_valid_){u.has_prev=1u;transpose4(last_view_,u.prev_view);u.focal_prev[0]=last_focal_[0];u.focal_prev[1]=last_focal_[1];}
            std::copy_n(p.view,16,last_view_);last_focal_[0]=p.focal_x;last_focal_[1]=p.focal_y;last_valid_=true;
        }else last_valid_=false;
        srv(SlotDepthIn,depth_in_.get(),DXGI_FORMAT_R32_FLOAT);srv(SlotPixelsIn,color_in_.get(),DXGI_FORMAT_B8G8R8A8_UNORM);srv(SlotSceneIn,scene_in_.get(),DXGI_FORMAT_B8G8R8A8_UNORM);
        uav(SlotMotionOut,motion_.get(),DXGI_FORMAT_R16G16_FLOAT);uav(SlotDepthOut,depth_fx_.get(),DXGI_FORMAT_R32_FLOAT);uav(SlotReactiveOut,reactive_.get(),DXGI_FORMAT_R8_UNORM);
        cmd_->SetComputeRoot32BitConstants(RootMotionCB,sizeof u/4,&u,0);
        cmd_->SetPipelineState(motion_pso_.get());cmd_->Dispatch(640/8,480/8,1);uav_barrier();
    }
    // Neues Bild: Upload, Bewegungsvektoren, FSR, Post-Processing in den Backbuffer, Present.
    bool process(const Pending& job){
        RECT rc{};GetClientRect(hwnd_,&rc);const int dw=std::max<int>(1,rc.right-rc.left),dh=std::max<int>(1,rc.bottom-rc.top);
        if(dirty_||dw!=backbuffer_w_||dh!=backbuffer_h_)rebuild(dw,dh);
        const bool reset=job.scene_key!=scene_key_||!history_valid_;scene_key_=job.scene_key;if(reset)++stats_.resets;
        begin();
        upload(job);
        encode_motion(job,reset);
        ID3D12Resource* src=color_in_.get();int src_w=640,src_h=480;
        if(context_&&upscaled_){
            // Eingaben in NON_PIXEL_SHADER_RESOURCE (motion/depth/reactive aus UAV), Ausgabe als UAV.
            transition(motion_.get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            transition(depth_fx_.get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            transition(reactive_.get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            ffxDispatchDescUpscale d{};d.header.type=FFX_API_DISPATCH_DESC_TYPE_UPSCALE;d.commandList=cmd_.get();
            d.color=ffxApiGetResourceDX12(color_in_.get(),FFX_API_RESOURCE_STATE_COMPUTE_READ);
            d.depth=ffxApiGetResourceDX12(depth_fx_.get(),FFX_API_RESOURCE_STATE_COMPUTE_READ);
            d.motionVectors=ffxApiGetResourceDX12(motion_.get(),FFX_API_RESOURCE_STATE_COMPUTE_READ);
            d.reactive=ffxApiGetResourceDX12(reactive_.get(),FFX_API_RESOURCE_STATE_COMPUTE_READ);
            d.output=ffxApiGetResourceDX12(upscaled_.get(),FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
            d.jitterOffset={0.0f,0.0f};d.motionVectorScale={1.0f,1.0f};d.renderSize={640,480};d.upscaleSize={std::uint32_t(out_w_),std::uint32_t(out_h_)};
            d.enableSharpening=false;d.sharpness=0.0f;d.frameTimeDelta=float((job.delta>0?job.delta:1.0/30)*1000.0);d.preExposure=1.0f;d.reset=reset;
            const float fy=job.projection.valid&&job.projection.focal_y>0?job.projection.focal_y:(last_focal_[1]>0?last_focal_[1]:240.0f);
            d.cameraNear=depth_near_>0?depth_near_:1.0f;d.cameraFar=depth_far_>0?depth_far_:100000.0f;d.cameraFovAngleVertical=2.0f*std::atan(240.0f/fy);d.viewSpaceToMetersFactor=0.1f;d.flags=0;
            require_ffx(ffx_.Dispatch(&context_,&d.header),"upscale dispatch");
            transition(motion_.get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            transition(depth_fx_.get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            transition(reactive_.get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            transition(upscaled_.get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            // ffxDispatch hinterlaesst Wurzelsignatur und Heaps der Bibliothek: eigene wieder binden.
            ID3D12DescriptorHeap* heaps[]={heap_.get(),sampler_heap_.get()};cmd_->SetDescriptorHeaps(2,heaps);cmd_->SetComputeRootSignature(root_.get());
            cmd_->SetComputeRootDescriptorTable(RootSrvTable,gpu(0));cmd_->SetComputeRootDescriptorTable(RootUavTable,gpu(SlotMotionOut));cmd_->SetComputeRootDescriptorTable(RootSamplers,sampler_heap_->GetGPUDescriptorHandleForHeapStart());
            src=upscaled_.get();src_w=out_w_;src_h=out_h_;
        }
        latest_src_=src;latest_w_=src_w;latest_h_=src_h;
        show(src,src_w,src_h);
        submit();present();
        history_valid_=true;++stats_.presents;
        return true;
    }
    // Wiederholung im naechsten Zeitfenster des Takts (gleiches Bild, erneut praesentiert).
    void represent(){
        if(!latest_src_)return;
        RECT rc{};GetClientRect(hwnd_,&rc);if(std::max<int>(1,rc.right-rc.left)!=backbuffer_w_||std::max<int>(1,rc.bottom-rc.top)!=backbuffer_h_)return;
        begin();show(latest_src_,latest_w_,latest_h_);submit();present();++stats_.repeated;++stats_.presents;
    }
    void present(){
        const UINT sync=settings_.vsync()?1u:0u;const UINT flags=!settings_.vsync()&&tearing_?DXGI_PRESENT_ALLOW_TEARING:0u;
        const HRESULT hr=swapchain_->Present(sync,flags);
        if(hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET)throw std::runtime_error("FSR presentation: device removed");
        require(hr,"present");
    }
    // Post-Processing und 4:3-Einpassung von src in den aktuellen Backbuffer.
    void show(ID3D12Resource* src,int src_w,int src_h){
        const UINT index=swapchain_->GetCurrentBackBufferIndex();ID3D12Resource* back=backbuffers_[index].get();
        srv(SlotSrc,src,DXGI_FORMAT_B8G8R8A8_UNORM);
        uav(SlotDst,back,DXGI_FORMAT_B8G8R8A8_UNORM);
        PresentCB u{};u.src_size[0]=float(src_w);u.src_size[1]=float(src_h);u.fit_origin[0]=float(fit_x_);u.fit_origin[1]=float(fit_y_);u.fit_size[0]=float(fit_w_);u.fit_size[1]=float(fit_h_);
        u.dst_size[0]=std::uint32_t(backbuffer_w_);u.dst_size[1]=std::uint32_t(backbuffer_h_);
        u.mode=settings_.post_process==PostProcess::CrtLight?1u:settings_.post_process==PostProcess::CrtFull?2u:0u;
        u.nearest=(src_w==640&&settings_.upscaler==Upscaler::Off&&settings_.post_process==PostProcess::Off)?1u:0u;
        u.game_lines=480;u.phosphor=std::uint32_t(phosphor_);u.sharpness=settings_.sharpness_amount();
        u.bloom=settings_.post_process==PostProcess::CrtFull?0.35f:0.0f;u.mask_strength=settings_.post_process==PostProcess::CrtFull?0.35f:0.18f;u.sigma_dark=0.22f;u.sigma_bright=0.30f;
        if(settings_.post_process==PostProcess::CrtFull&&bloom_a_&&bloom_b_){
            // Hellpass in Viertelaufloesung, dann zwei Richtungen Gauss; Ergebnis in bloom_a_.
            uav(SlotBloomDst,bloom_a_.get(),DXGI_FORMAT_R16G16B16A16_FLOAT);
            cmd_->SetComputeRoot32BitConstants(RootPresentCB,sizeof u/4,&u,0);
            cmd_->SetPipelineState(bloom_down_pso_.get());cmd_->Dispatch(UINT((std::max(1,fit_w_/4)+7)/8),UINT((std::max(1,fit_h_/4)+7)/8),1);uav_barrier();
            const auto blur=[&](ID3D12Resource* from,ID3D12Resource* to,float dx,float dy){
                transition(from,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                srv(SlotBloomTex,from,DXGI_FORMAT_R16G16B16A16_FLOAT);uav(SlotBloomDst,to,DXGI_FORMAT_R16G16B16A16_FLOAT);
                const BlurCB b{{dx,dy},{0,0}};cmd_->SetComputeRoot32BitConstants(RootBlurCB,sizeof b/4,&b,0);
                cmd_->SetPipelineState(bloom_blur_pso_.get());cmd_->Dispatch(UINT((std::max(1,fit_w_/4)+7)/8),UINT((std::max(1,fit_h_/4)+7)/8),1);uav_barrier();
                transition(from,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);};
            blur(bloom_a_.get(),bloom_b_.get(),1.0f,0.0f);blur(bloom_b_.get(),bloom_a_.get(),0.0f,1.0f);
            transition(bloom_a_.get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            srv(SlotBloomTex,bloom_a_.get(),DXGI_FORMAT_R16G16B16A16_FLOAT);
        }else srv(SlotBloomTex,blank_.get(),DXGI_FORMAT_R16G16B16A16_FLOAT);
        transition(back,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd_->SetComputeRoot32BitConstants(RootPresentCB,sizeof u/4,&u,0);
        cmd_->SetPipelineState(present_pso_.get());cmd_->Dispatch(UINT((backbuffer_w_+7)/8),UINT((backbuffer_h_+7)/8),1);
        transition(back,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PRESENT);
        if(settings_.post_process==PostProcess::CrtFull&&bloom_a_)transition(bloom_a_.get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if(src==upscaled_.get())transition(upscaled_.get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    HWND hwnd_;std::string sdk_dir_;Com<ID3D12Device> device_;PFN_Compile compile_;std::string adapter_;
    Com<ID3D12CommandQueue> queue_;Com<ID3D12CommandAllocator> allocators_[frames_in_flight];Com<ID3D12GraphicsCommandList> cmd_;Com<ID3D12Fence> fence_;HANDLE event_=nullptr;
    UINT64 fence_value_=0,frame_fence_[frames_in_flight]{};unsigned frame_index_=0;
    Com<ID3D12RootSignature> root_;Com<ID3D12PipelineState> motion_pso_,present_pso_,bloom_down_pso_,bloom_blur_pso_;
    Com<ID3D12DescriptorHeap> heap_,sampler_heap_;UINT descriptor_size_=0;
    Com<IDXGISwapChain3> swapchain_;Com<ID3D12Resource> backbuffers_[backbuffers];bool tearing_=false;int backbuffer_w_=0,backbuffer_h_=0;
    Com<ID3D12Resource> color_in_,scene_in_,depth_in_,motion_,depth_fx_,reactive_,upscaled_,bloom_a_,bloom_b_,blank_;Com<ID3D12Resource> uploads_[frames_in_flight];bool inputs_uploaded_=false;
    ID3D12Resource* latest_src_=nullptr;int latest_w_=640,latest_h_=480;
    HMODULE loader_module_=nullptr,upscaler_module_=nullptr;FfxFunctions ffx_{};ffxContext context_=nullptr;std::vector<std::uint64_t> version_ids_;std::vector<std::string> version_names_;std::string provider_name_;
    VideoSettings settings_;PresentStats stats_;std::string notes_;
    bool dirty_=true,upscaler_supported_=false,history_valid_=false,last_valid_=false,int8_loaded_=false;std::atomic<bool> broken_{false};
    std::mutex mutex_;mutable std::mutex state_mutex_;std::condition_variable cv_;Pending staging_;bool staging_new_=false,stop_=false;VideoSettings requested_;bool requested_dirty_=false;
    std::thread worker_;mutable PresentStats stats_copy_;
    int fit_x_=0,fit_y_=0,fit_w_=640,fit_h_=480,out_w_=640,out_h_=480,phosphor_=1;unsigned scene_key_=0;
    float last_view_[16]{},last_focal_[2]{},depth_near_=0,depth_far_=0;
};
}
std::unique_ptr<MetalFxPresenter> create_fsr_presenter(void* hwnd_pointer,const std::string& sdk_dir,const VideoSettings& initial,std::string& reason){
    if(!hwnd_pointer){reason="SDL window has no HWND";return nullptr;}
    const HMODULE d3d12=LoadLibraryA("d3d12.dll");if(!d3d12){reason="d3d12.dll not available";return nullptr;}
    const HMODULE dxgi=LoadLibraryA("dxgi.dll");if(!dxgi){reason="dxgi.dll not available";return nullptr;}
    const HMODULE compiler=LoadLibraryA("d3dcompiler_47.dll");if(!compiler){reason="d3dcompiler_47.dll not available";return nullptr;}
    const auto create=reinterpret_cast<PFN_CreateDevice>(reinterpret_cast<void*>(GetProcAddress(d3d12,"D3D12CreateDevice")));
    const auto serialize=reinterpret_cast<PFN_SerializeRootSignature>(reinterpret_cast<void*>(GetProcAddress(d3d12,"D3D12SerializeRootSignature")));
    const auto factory2=reinterpret_cast<PFN_CreateFactory2>(reinterpret_cast<void*>(GetProcAddress(dxgi,"CreateDXGIFactory2")));
    const auto compile=reinterpret_cast<PFN_Compile>(reinterpret_cast<void*>(GetProcAddress(compiler,"D3DCompile")));
    if(!create||!serialize||!factory2||!compile){reason="Direct3D 12 entry points missing";return nullptr;}
    Com<IDXGIFactory4> factory;if(FAILED(factory2(0,__uuidof(IDXGIFactory4),factory.put_void()))){reason="no DXGI factory";return nullptr;}
    Com<IDXGIAdapter1> adapter;std::string name="device";
    for(UINT i=0;factory->EnumAdapters1(i,adapter.put())==S_OK;++i){
        DXGI_ADAPTER_DESC1 desc{};if(FAILED(adapter->GetDesc1(&desc))||(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))continue;
        if(SUCCEEDED(create(adapter.get(),D3D_FEATURE_LEVEL_12_0,__uuidof(ID3D12Device),nullptr))){char text[128]{};WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,text,sizeof text-1,nullptr,nullptr);name=text;break;}
        adapter.reset();}
    if(!adapter){reason="no Direct3D 12 hardware device (feature level 12.0)";return nullptr;}
    Com<ID3D12Device> device;if(FAILED(create(adapter.get(),D3D_FEATURE_LEVEL_12_0,__uuidof(ID3D12Device),device.put_void()))){reason="Direct3D 12 device creation failed";return nullptr;}
    try{return std::unique_ptr<MetalFxPresenter>(new FsrPresenter(static_cast<HWND>(hwnd_pointer),sdk_dir,initial,std::move(device),compile,serialize,factory2,name));}
    catch(const std::exception& e){reason=e.what();return nullptr;}
}
}
