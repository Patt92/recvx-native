// Direct3D-12-Backend des Host-GPU-Rasterers (gpu_raster.h), nur Windows, nie Plattformstandard
// (das bleibt Direct3D 11). Plattformanpassung, keine Spiellogik: es rastert die raster::Setup-Liste
// des SoftwareRenderers mit demselben HLSL wie das D3D11-Backend (gpu_raster_hlsl.h) und liefert Bild
// und Tiefe zurueck. Zweck: ein D3D12-Geraet und eine D3D12-Befehlsliste als Grundlage fuer spaetere
// D3D12-Praesentationsstufen (FidelityFX/FSR brauchen DirectX 12; offen, siehe docs/NATIVE_PORT.md).
//
// Ordnung wie bei D3D11 ueber Rasterizer Ordered Views (D3D12_FEATURE_D3D12_OPTIONS.ROVsSupported):
// Farbe, Tiefe, transportierte Tiefe und Schattenmaske sind UAVs, der Pixelshader liest den bisherigen
// Wert in Primitivreihenfolge und schreibt ihn wie die Software. Ohne ROV gibt es kein Backend
// (Softwarepfad), statt eines nur aehnlich wirkenden Mischens. Anders als D3D11 synchronisiert D3D12
// UAV-Zugriffe zwischen Zeichenaufrufen nicht selbst: zwischen den Aufrufen steht je eine UAV-Barriere,
// wie die Fragment-Barrieren des Vulkan-Backends; die ROV-Ordnung gilt damit nachweisbar je Aufruf.
//
// Ein Bild = eine Befehlsliste: Uploads, Loeschen, Phasen, Masken, Komposite, Aufloesung, Kopie in den
// Ruecklesepuffer, Zaun. Der Aufrufer wartet auf den Zaun (wie D3D11 Map). Texturkopien werden erst nach
// erfolgreicher Ausfuehrung dem Cache zugesprochen (pending_owner_, wie gpu_raster_vulkan.cpp): ein Bild,
// das nach dem Aufzeichnen scheitert, hinterlaesst keine Kopie ohne ausgefuehrten Upload.
#include "gpu_raster.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#include "gpu_raster_hlsl.h"
namespace recvx {
namespace {
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
    if(FAILED(hr)){char text[160];std::snprintf(text,sizeof text,"D3D12: %s (0x%08lx)",what,static_cast<unsigned long>(hr));throw std::runtime_error(text);}}
struct alignas(16) GpuSetup{float p0[4],p1[4],p2[4],uv0[4],uv1[4],grad[4],l0[4],l1[4],l2[4],fog[4];std::uint32_t info[4],blend[4];std::int32_t box[4];};
static_assert(sizeof(GpuSetup)==208,"setup layout");
struct alignas(16) GpuShadowTri{float v01[4],v2a[4];std::uint32_t depth[4];std::int32_t box[4];};
static_assert(sizeof(GpuShadowTri)==64,"shadow triangle layout");
// Root-Konstanten je cbuffer des Shaders (b0..b3): 8+8+4+4 DWORDs.
struct FrameCB{float W,H;std::uint32_t fog_pixel,wireframe,background,mask_w,mask_h,pad;};
struct CompositeCB{float W,H,rx0,ry0,rx1,ry1;std::uint32_t fog,layer;};
struct ResolveCB{std::uint32_t size[4];};
struct DrawCB{std::uint32_t first,pad[3];};
static_assert(sizeof(FrameCB)==32&&sizeof(CompositeCB)==32&&sizeof(ResolveCB)==16&&sizeof(DrawCB)==16,"root constant layout");
enum RootParam{RootFrame=0,RootComposite=1,RootResolve=2,RootDraw=3,RootSrv=4,RootUav=5,RootSampler=6};
enum Variant{Point=1,Straight=2,Premultiplied=3,Punch=4};
struct Batch{std::size_t first,count;ID3D12Resource* texture;unsigned sampler;bool point;};
using PFN_CreateDevice=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);
using PFN_SerializeRootSignature=HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*,D3D_ROOT_SIGNATURE_VERSION,ID3DBlob**,ID3DBlob**);
using PFN_CreateFactory1=HRESULT(WINAPI*)(REFIID,void**);
using PFN_Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
using PFN_GetDebugInterface=HRESULT(WINAPI*)(REFIID,void**);
// ID3D12Debug (d3d12sdklayers.h) ohne SDK-Header: nur EnableDebugLayer (RECVX_D3D12_DEBUG=1).
struct DebugLayer:IUnknown{virtual void STDMETHODCALLTYPE EnableDebugLayer()=0;};
constexpr GUID iid_debug_layer={0x344488b7,0x6846,0x474b,{0xb9,0x89,0xf0,0x27,0x44,0x82,0x45,0xe0}};
constexpr UINT uav_count=6; // u0 Farbe, u1 Tiefe, u2 transportiert, u3 Maske, u4/u5 Ausgabe
constexpr UINT srv_count=4; // t0 Setups, t1 Dreiecke, t2 Tex, t3 TexPoint
class D3D12Raster final:public GpuRaster{
public:
    D3D12Raster(Com<ID3D12Device> device,PFN_Compile compile,PFN_SerializeRootSignature serialize,std::string adapter)
        :device_(std::move(device)),compile_(compile),adapter_(std::move(adapter)){
        {D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
         require(device_->CreateCommandQueue(&qd,__uuidof(ID3D12CommandQueue),queue_.put_void()),"command queue");}
        require(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,__uuidof(ID3D12CommandAllocator),allocator_.put_void()),"command allocator");
        require(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator_.get(),nullptr,__uuidof(ID3D12GraphicsCommandList),cmd_.put_void()),"command list");
        require(cmd_->Close(),"close initial command list");
        require(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,__uuidof(ID3D12Fence),fence_.put_void()),"fence");
        event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event_)throw std::runtime_error("D3D12: fence event");
        descriptor_size_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        {D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=uav_count;
         require(device_->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),cpu_heap_.put_void()),"cpu descriptor heap");}
        {// Abtaster s0: Index linear*4 + clamp_u*2 + clamp_v (Batch::sampler).
         D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;hd.NumDescriptors=8;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
         require(device_->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),sampler_heap_.put_void()),"sampler heap");
         const UINT step=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);sampler_size_=step;
         for(unsigned i=0;i<8;++i){const bool linear=i&4,cu=i&2,cv=i&1;
            D3D12_SAMPLER_DESC sd{};sd.Filter=linear?D3D12_FILTER_MIN_MAG_MIP_LINEAR:D3D12_FILTER_ANISOTROPIC;sd.MaxAnisotropy=linear?1u:4u;
            sd.AddressU=cu?D3D12_TEXTURE_ADDRESS_MODE_CLAMP:D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sd.AddressV=cv?D3D12_TEXTURE_ADDRESS_MODE_CLAMP:D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sd.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;sd.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;sd.MaxLOD=D3D12_FLOAT32_MAX;
            D3D12_CPU_DESCRIPTOR_HANDLE h=sampler_heap_->GetCPUDescriptorHandleForHeapStart();h.ptr+=SIZE_T(i)*step;
            device_->CreateSampler(&sd,h);}}
        {// Root-Signatur: vier Konstantenbloecke, SRV-Tabelle t0..t3, UAV-Tabelle u0..u5, Abtastertabelle s0.
         D3D12_DESCRIPTOR_RANGE ranges[3]{};
         ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;ranges[0].NumDescriptors=srv_count;
         ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;ranges[1].NumDescriptors=uav_count;
         ranges[2].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;ranges[2].NumDescriptors=1;
         D3D12_ROOT_PARAMETER params[7]{};
         const UINT constants[4]={sizeof(FrameCB)/4,sizeof(CompositeCB)/4,sizeof(ResolveCB)/4,sizeof(DrawCB)/4};
         const UINT registers[4]={0,1,2,3};
         for(int i=0;i<4;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[i].Constants.ShaderRegister=registers[i];params[i].Constants.Num32BitValues=constants[i];params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;}
         for(int i=0;i<3;++i){params[4+i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[4+i].DescriptorTable.NumDescriptorRanges=1;params[4+i].DescriptorTable.pDescriptorRanges=&ranges[i];params[4+i].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;}
         D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=7;rd.pParameters=params;rd.Flags=D3D12_ROOT_SIGNATURE_FLAG_NONE;
         Com<ID3DBlob> blob,errors;
         if(FAILED(serialize(&rd,D3D_ROOT_SIGNATURE_VERSION_1,blob.put(),errors.put())))
             throw std::runtime_error(std::string("D3D12 root signature: ")+(errors?static_cast<const char*>(errors->GetBufferPointer()):"serialize failed"));
         require(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),__uuidof(ID3D12RootSignature),root_.put_void()),"root signature");}
        raster_pso_=graphics_pipeline("vs_raster","ps_raster");
        mask_pso_=graphics_pipeline("vs_mask","ps_mask");
        shadow_pso_=graphics_pipeline("vs_full","ps_shadow");
        {const auto cs=compile_shader("cs_resolve","cs_5_1");
         D3D12_COMPUTE_PIPELINE_STATE_DESC cd{};cd.pRootSignature=root_.get();cd.CS={cs->GetBufferPointer(),cs->GetBufferSize()};
         require(device_->CreateComputePipelineState(&cd,__uuidof(ID3D12PipelineState),resolve_pso_.put_void()),"compute pipeline");}
        // 1x1-Platzhalter: t2 (float4) und t3 (uint) muessen gueltige Deskriptoren tragen, der Shader liest nur den gebundenen.
        begin();
        blank_float_=upload_texture(1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{{nullptr,4}},"blank float");
        blank_uint_=upload_texture(1,1,DXGI_FORMAT_R32_UINT,{{nullptr,4}},"blank uint");
        submit_and_wait();
    }
    ~D3D12Raster()override{if(event_)CloseHandle(event_);}
    std::string name()const override{return "Direct3D 12 "+adapter_;}
    const std::vector<std::string>& unsupported()const override{return unsupported_;}
    unsigned draw_calls()const override{return draw_calls_;}
    unsigned shadow_composites()const override{return composites_;}
    const Timings& timings()const override{return timings_;}
    bool render(const raster::Frame& frame,const raster::ShadowGeometry& shadows,std::vector<std::uint32_t>& pixels,std::vector<float>& depth)override{
        using Clock=std::chrono::steady_clock;
        const auto since=[](Clock::time_point from){return std::chrono::duration<double>(Clock::now()-from).count();};
        const auto& setups=*frame.setups;const int S=frame.supersampling,W=frame.width*S,H=frame.height*S;
        targets(W,H,frame.width,frame.height);
        begin();
        {const auto t=Clock::now();build(frame);timings_.build+=since(t);}
        const auto encode_begin=Clock::now();
        draw_calls_=0;composites_=0;
        write_structured(setup_buffer_,setup_capacity_,setups_.data(),setups_.size(),sizeof(GpuSetup));
        // Sichtbarer Heap des Bildes: bis zu drei UAV-Bloecke (Maskenwachstum je Phase), vier SRVs je Batch
        // und je Maskenpass. Vor dem ersten Eintrag angelegt, danach nur noch befuellt.
        visible_heap(uav_count*3+srv_count*(batches_.size()+3));
        {ID3D12DescriptorHeap* heaps[]={visible_heap_.get(),sampler_heap_.get()};cmd_->SetDescriptorHeaps(2,heaps);}
        cmd_->SetGraphicsRootSignature(root_.get());cmd_->SetComputeRootSignature(root_.get());
        uav_block();
        // Farbe mit der Hintergrundfarbe, Tiefe und transportierte Tiefe mit 0 vorbelegen.
        {const UINT background[4]={frame.background,0,0,0};cmd_->ClearUnorderedAccessViewUint(uav_gpu(0),uav_cpu(0),color_.get(),background,0,nullptr);}
        {const FLOAT zero[4]={0,0,0,0};
         cmd_->ClearUnorderedAccessViewFloat(uav_gpu(1),uav_cpu(1),depth_.get(),zero,0,nullptr);
         cmd_->ClearUnorderedAccessViewFloat(uav_gpu(2),uav_cpu(2),transported_.get(),zero,0,nullptr);}
        uav_barrier();
        const FrameCB fu{float(W),float(H),frame.fog_pixel,frame.wireframe?1u:0u,frame.background,std::uint32_t(W),std::uint32_t(H),0};
        cmd_->SetGraphicsRoot32BitConstants(RootFrame,sizeof fu/4,&fu,0);
        cmd_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const std::size_t ends[3]={frame.shadows?frame.before_modifiers:setups.size(),frame.shadows?frame.before_mirror_effects:setups.size(),setups.size()};
        std::size_t batch=0;std::vector<raster::ShadowWork> pending;
        for(unsigned phase=0;phase<3;++phase){
            for(std::size_t i=0;i<pending.size();++i)composite(pending[i],i,W,H);pending.clear();
            bind_targets(W,H);
            cmd_->SetPipelineState(raster_pso_.get());
            for(;batch<batches_.size()&&batches_[batch].first<ends[phase];++batch){
                const auto& b=batches_[batch];
                srv_block(setup_buffer_.get(),setups_.size(),sizeof(GpuSetup),b.point?blank_float_.get():b.texture,b.point?b.texture:blank_uint_.get());
                {D3D12_GPU_DESCRIPTOR_HANDLE s=sampler_heap_->GetGPUDescriptorHandleForHeapStart();s.ptr+=UINT64(b.sampler)*sampler_size_;cmd_->SetGraphicsRootDescriptorTable(RootSampler,s);}
                const DrawCB dc{std::uint32_t(b.first),{0,0,0}};cmd_->SetGraphicsRoot32BitConstants(RootDraw,sizeof dc/4,&dc,0);
                cmd_->DrawInstanced(UINT(b.count*6),1,0,0);++draw_calls_;uav_barrier();}
            if(phase<2&&frame.shadows&&frame.shadow_pass[phase]&&shadows){
                // Die Masken zeichnet die GPU aus derselben transportierten Tiefe, kein Ruecklesen.
                const auto shadow_begin=Clock::now();pending=shadows(phase==1);
                masks(pending,W,H);timings_.build+=since(shadow_begin);}
        }
        for(std::size_t i=0;i<pending.size();++i)composite(pending[i],i,W,H);
        resolve(frame.width,frame.height,S);
        // Aufgeloestes Bild in die Ruecklesepuffer; die Ausgabepuffer gehen danach wieder in den UAV-Zustand.
        transition(out_color_.get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(out_depth_.get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmd_->CopyResource(read_color_.get(),out_color_.get());cmd_->CopyResource(read_depth_.get(),out_depth_.get());
        transition(out_color_.get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(out_depth_.get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        timings_.encode+=since(encode_begin);
        {const auto t=Clock::now();submit_and_wait();timings_.wait+=since(t);++timings_.submits;}
        {const auto t=Clock::now();
         const std::size_t n=std::size_t(frame.width)*frame.height;pixels.resize(n);depth.resize(n);
         read_back(read_color_,pixels.data(),n*4);read_back(read_depth_,depth.data(),n*4);
         timings_.readback+=since(t);}
        return true;
    }
private:
    Com<ID3DBlob> compile_shader(const char* entry,const char* target){
        Com<ID3DBlob> code,errors;
        // IEEE-Arithmetik wie die Bandstufe (-ffp-contract=off): keine zusammengefassten Operationen.
        const UINT flags=D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS;
        const HRESULT hr=compile_(raster_hlsl_source,std::strlen(raster_hlsl_source),"recvx",nullptr,nullptr,entry,target,flags,0,code.put(),errors.put());
        if(FAILED(hr))throw std::runtime_error(std::string("D3D12 shader ")+entry+": "+(errors?static_cast<const char*>(errors->GetBufferPointer()):"compile failed"));
        return code;
    }
    // Zeichenpipeline ohne Render-Targets: nur UAVs (ROV), ein Sample, kein Tiefenpuffer, kein Culling.
    Com<ID3D12PipelineState> graphics_pipeline(const char* vs_entry,const char* ps_entry){
        const auto vs=compile_shader(vs_entry,"vs_5_1"),ps=compile_shader(ps_entry,"ps_5_1");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root_.get();
        pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
        pd.BlendState.RenderTarget[0].RenderTargetWriteMask=0;pd.SampleMask=~0u;
        pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=FALSE;
        pd.RasterizerState.ConservativeRaster=D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=0;pd.DSVFormat=DXGI_FORMAT_UNKNOWN;pd.SampleDesc.Count=1;
        Com<ID3D12PipelineState> pso;require(device_->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),pso.put_void()),ps_entry);return pso;}
    Com<ID3D12Resource> committed(D3D12_HEAP_TYPE heap,const D3D12_RESOURCE_DESC& desc,D3D12_RESOURCE_STATES state,const char* what){
        D3D12_HEAP_PROPERTIES hp{};hp.Type=heap;Com<ID3D12Resource> r;
        require(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,__uuidof(ID3D12Resource),r.put_void()),what);return r;}
    static D3D12_RESOURCE_DESC buffer_desc(UINT64 bytes,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;
        d.Format=DXGI_FORMAT_UNKNOWN;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;return d;}
    static D3D12_RESOURCE_DESC texture_desc(UINT w,UINT h,UINT16 layers,UINT16 mips,DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags){
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=layers;d.MipLevels=mips;
        d.Format=format;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;d.Flags=flags;return d;}
    void transition(ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){
        D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=r;b.Transition.StateBefore=from;b.Transition.StateAfter=to;
        b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;cmd_->ResourceBarrier(1,&b);}
    void uav_barrier(){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;b.UAV.pResource=nullptr;cmd_->ResourceBarrier(1,&b);}
    // Befehlsliste des Bildes oeffnen; ein nach dem Aufzeichnen gescheitertes Bild wird hier verworfen.
    void begin(){
        if(recording_){cmd_->Close();recording_=false;discard_uploads();}
        require(allocator_->Reset(),"reset allocator");
        require(cmd_->Reset(allocator_.get(),nullptr),"reset command list");
        recording_=true;visible_used_=0;uploads_.clear();retired_.clear();retired_heaps_.clear();
    }
    void submit_and_wait(){
        require(cmd_->Close(),"close command list");recording_=false;
        ID3D12CommandList* lists[]={cmd_.get()};queue_->ExecuteCommandLists(1,lists);
        const UINT64 value=++fence_value_;
        require(queue_->Signal(fence_.get(),value),"signal fence");
        if(fence_->GetCompletedValue()<value){require(fence_->SetEventOnCompletion(value,event_),"fence event");WaitForSingleObject(event_,INFINITE);}
        commit_uploads();uploads_.clear();retired_.clear();retired_heaps_.clear();
    }
    void read_back(const Com<ID3D12Resource>& buffer,void* out,std::size_t bytes){
        void* mapped=nullptr;const D3D12_RANGE range{0,bytes};
        require(buffer->Map(0,&range,&mapped),"map readback");std::memcpy(out,mapped,bytes);
        const D3D12_RANGE none{0,0};buffer->Unmap(0,&none);}
    // Strukturpuffer im Upload-Heap (CPU-beschreibbar, von der GPU gelesen), Inhalt je Bild neu.
    void write_structured(Com<ID3D12Resource>& buffer,std::size_t& capacity,const void* data,std::size_t count,std::size_t stride){
        if(!count)return;
        if(!buffer||capacity<count){
            capacity=std::max<std::size_t>(count*2,256);
            if(buffer)retired_.push_back(std::move(buffer));
            buffer=committed(D3D12_HEAP_TYPE_UPLOAD,buffer_desc(capacity*stride),D3D12_RESOURCE_STATE_GENERIC_READ,"structured buffer");}
        void* mapped=nullptr;const D3D12_RANGE none{0,0};
        require(buffer->Map(0,&none,&mapped),"map structured buffer");std::memcpy(mapped,data,count*stride);buffer->Unmap(0,nullptr);}
    void visible_heap(std::size_t descriptors){
        if(visible_heap_&&visible_capacity_>=descriptors)return;
        if(visible_heap_)retired_heaps_.push_back(std::move(visible_heap_));
        visible_capacity_=std::max<std::size_t>(descriptors*2,256);
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=UINT(visible_capacity_);hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        require(device_->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),visible_heap_.put_void()),"shader visible heap");}
    std::size_t allocate(std::size_t count){
        if(visible_used_+count>visible_capacity_)throw std::runtime_error("D3D12: descriptor heap exhausted");
        const auto at=visible_used_;visible_used_+=count;return at;}
    D3D12_CPU_DESCRIPTOR_HANDLE visible_cpu(std::size_t index)const{auto h=visible_heap_->GetCPUDescriptorHandleForHeapStart();h.ptr+=SIZE_T(index)*descriptor_size_;return h;}
    D3D12_GPU_DESCRIPTOR_HANDLE visible_gpu(std::size_t index)const{auto h=visible_heap_->GetGPUDescriptorHandleForHeapStart();h.ptr+=UINT64(index)*descriptor_size_;return h;}
    D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu(unsigned slot)const{auto h=cpu_heap_->GetCPUDescriptorHandleForHeapStart();h.ptr+=SIZE_T(slot)*descriptor_size_;return h;}
    D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu(unsigned slot)const{return visible_gpu(uav_block_+slot);}
    // Neuer UAV-Block im sichtbaren Heap (Kopie der sechs CPU-Deskriptoren); fruehere Bloecke bleiben fuer bereits
    // aufgezeichnete Befehle unberuehrt (Maskenwachstum mitten im Bild).
    void uav_block(){uav_block_=allocate(uav_count);device_->CopyDescriptorsSimple(uav_count,visible_cpu(uav_block_),uav_cpu(0),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);}
    void srv_block(ID3D12Resource* setups,std::size_t setup_count,std::size_t setup_stride,ID3D12Resource* tex,ID3D12Resource* tex_point){
        const auto at=allocate(srv_count);
        structured_view(visible_cpu(at),setups,setup_count,setup_stride);
        structured_view(visible_cpu(at+1),shadow_buffer_?shadow_buffer_.get():setups,shadow_buffer_?shadow_capacity_:setup_count,sizeof(GpuShadowTri));
        device_->CreateShaderResourceView(tex,nullptr,visible_cpu(at+2));
        device_->CreateShaderResourceView(tex_point,nullptr,visible_cpu(at+3));
        cmd_->SetGraphicsRootDescriptorTable(RootSrv,visible_gpu(at));}
    void structured_view(D3D12_CPU_DESCRIPTOR_HANDLE at,ID3D12Resource* buffer,std::size_t count,std::size_t stride){
        D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_UNKNOWN;sd.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
        sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Buffer.NumElements=UINT(std::max<std::size_t>(count,1));sd.Buffer.StructureByteStride=UINT(stride);
        device_->CreateShaderResourceView(buffer,&sd,at);}
    void targets(int W,int H,int base_w,int base_h){
        if(color_&&target_w_==W&&target_h_==H)return;
        target_w_=W;target_h_=H;
        // Kein Bild in Arbeit (jedes Bild wartet auf seinen Zaun): alte Ziele duerfen sofort frei.
        const auto surface=[&](DXGI_FORMAT format,unsigned slot,Com<ID3D12Resource>& texture){
            texture=committed(D3D12_HEAP_TYPE_DEFAULT,texture_desc(UINT(W),UINT(H),1,1,format,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"render target");
            device_->CreateUnorderedAccessView(texture.get(),nullptr,nullptr,uav_cpu(slot));};
        surface(DXGI_FORMAT_R32_UINT,0,color_);surface(DXGI_FORMAT_R32_FLOAT,1,depth_);surface(DXGI_FORMAT_R32_FLOAT,2,transported_);
        if(!mask_)mask_target(1);
        const std::size_t n=std::size_t(base_w)*base_h;
        const auto out_buffer=[&](DXGI_FORMAT format,unsigned slot,Com<ID3D12Resource>& buffer,Com<ID3D12Resource>& read){
            buffer=committed(D3D12_HEAP_TYPE_DEFAULT,buffer_desc(n*4,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"output buffer");
            D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};ud.Format=format;ud.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;ud.Buffer.NumElements=UINT(n);
            device_->CreateUnorderedAccessView(buffer.get(),nullptr,&ud,uav_cpu(slot));
            read=committed(D3D12_HEAP_TYPE_READBACK,buffer_desc(n*4),D3D12_RESOURCE_STATE_COPY_DEST,"readback buffer");};
        out_buffer(DXGI_FORMAT_R32_UINT,4,out_color_,read_color_);out_buffer(DXGI_FORMAT_R32_FLOAT,5,out_depth_,read_depth_);
    }
    void bind_targets(int W,int H){
        cmd_->SetGraphicsRootDescriptorTable(RootUav,visible_gpu(uav_block_));
        D3D12_VIEWPORT vp{0.f,0.f,float(W),float(H),0.f,1.f};cmd_->RSSetViewports(1,&vp);
        D3D12_RECT sc{0,0,LONG(W),LONG(H)};cmd_->RSSetScissorRects(1,&sc);}
    struct Level{const void* data;UINT pitch;};
    // Textur ueber einen Upload-Puffer anlegen; die Kopie ist Teil der laufenden Befehlsliste.
    Com<ID3D12Resource> upload_texture(unsigned w,unsigned h,DXGI_FORMAT format,const std::vector<Level>& levels,const char* what){
        auto texture=committed(D3D12_HEAP_TYPE_DEFAULT,texture_desc(w,h,1,UINT16(levels.size()),format,D3D12_RESOURCE_FLAG_NONE),D3D12_RESOURCE_STATE_COPY_DEST,what);
        const auto desc=texture_desc(w,h,1,UINT16(levels.size()),format,D3D12_RESOURCE_FLAG_NONE);
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(levels.size());std::vector<UINT> rows(levels.size());std::vector<UINT64> row_bytes(levels.size());UINT64 total=0;
        device_->GetCopyableFootprints(&desc,0,UINT(levels.size()),0,footprints.data(),rows.data(),row_bytes.data(),&total);
        auto staging=committed(D3D12_HEAP_TYPE_UPLOAD,buffer_desc(total),D3D12_RESOURCE_STATE_GENERIC_READ,"texture upload");
        {void* mapped=nullptr;const D3D12_RANGE none{0,0};require(staging->Map(0,&none,&mapped),"map texture upload");
         for(std::size_t level=0;level<levels.size();++level){
             char* dst=static_cast<char*>(mapped)+footprints[level].Offset;
             for(UINT row=0;row<rows[level];++row){
                 if(levels[level].data)std::memcpy(dst+std::size_t(row)*footprints[level].Footprint.RowPitch,static_cast<const char*>(levels[level].data)+std::size_t(row)*levels[level].pitch,std::size_t(row_bytes[level]));
                 else std::memset(dst+std::size_t(row)*footprints[level].Footprint.RowPitch,0,std::size_t(row_bytes[level]));}}
         staging->Unmap(0,nullptr);}
        for(std::size_t level=0;level<levels.size();++level){
            D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=texture.get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=UINT(level);
            D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=staging.get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprints[level];
            cmd_->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}
        transition(texture.get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        uploads_.push_back(std::move(staging));
        return texture;
    }
    ID3D12Resource* texture(const SceneTexture& t,Variant variant){
        auto& cache=*t.filter_cache;const auto fingerprint=t.fingerprint();auto& slot=cache.gpu[variant];
        if(slot&&(cache.gpu_owner[variant]==cache_owner()||cache.gpu_owner[variant]==pending_owner_)&&cache.gpu_fingerprint[variant]==fingerprint)return static_cast<ID3D12Resource*>(slot.get());
        const auto upload_begin=std::chrono::steady_clock::now();++timings_.uploads;
        const std::size_t levels=variant==Point?1:1+(variant==Punch?cache.punch_mips.size():cache.mips.size());
        const DXGI_FORMAT format=variant==Point?DXGI_FORMAT_R32_UINT:variant==Premultiplied?DXGI_FORMAT_R16G16B16A16_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
        std::vector<Level> data(levels);std::vector<std::vector<std::uint16_t>> wide(levels);
        unsigned w=t.width,h=t.height;
        for(std::size_t level=0;level<levels;++level){
            const std::uint32_t* src=level==0?t.pixels.data():(variant==Punch?cache.punch_mips[level-1].data():cache.mips[level-1].data());
            if(variant==Premultiplied){wide[level].resize(std::size_t(w)*h*4);
                for(std::size_t i=0;i<std::size_t(w)*h;++i){const auto p=src[i];const double a=double(p>>24)/255.0;
                    wide[level][i*4]=std::uint16_t(double((p>>16)&255)/255.0*a*65535.0+.5);wide[level][i*4+1]=std::uint16_t(double((p>>8)&255)/255.0*a*65535.0+.5);
                    wide[level][i*4+2]=std::uint16_t(double(p&255)/255.0*a*65535.0+.5);wide[level][i*4+3]=std::uint16_t(a*65535.0+.5);}
                data[level]={wide[level].data(),UINT(w*8)};}
            else data[level]={src,UINT(w*4)};
            w=std::max(1u,w/2);h=std::max(1u,h/2);}
        auto resource=upload_texture(t.width,t.height,format,data,"texture");
        ID3D12Resource* raw=resource.get();resource.p=nullptr;
        // Bis zur Ausfuehrung gehoert die Kopie dem Bild (pending_owner_); commit_uploads/discard_uploads entscheiden.
        slot=std::shared_ptr<void>(raw,[](void* p){static_cast<ID3D12Resource*>(p)->Release();});
        cache.gpu_fingerprint[variant]=fingerprint;cache.gpu_owner[variant]=pending_owner_;pending_.push_back({t.filter_cache,variant});
        timings_.upload+=std::chrono::duration<double>(std::chrono::steady_clock::now()-upload_begin).count();
        return raw;
    }
    void commit_uploads(){for(const auto& [cache,variant]:pending_)if(cache->gpu_owner[variant]==pending_owner_)cache->gpu_owner[variant]=cache_owner();pending_.clear();}
    void discard_uploads(){
        for(const auto& [cache,variant]:pending_)if(cache->gpu_owner[variant]==pending_owner_){cache->gpu[variant].reset();cache->gpu_owner[variant].reset();cache->gpu_fingerprint[variant]=0;}
        pending_.clear();}
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
            ID3D12Resource* view=s.texture?this->texture(*s.texture,variant):blank_uint_.get();
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
        if(mask_target(unsigned(works.size())))uav_block();
        {const UINT zero[4]={0,0,0,0};cmd_->ClearUnorderedAccessViewUint(uav_gpu(3),uav_cpu(3),mask_.get(),zero,0,nullptr);}
        uav_barrier();
        if(triangles_.empty())return;
        // Der Dreieckspuffer wird je Phase neu geschrieben; der vorige Pass ist bereits aufgezeichnet, aber noch nicht
        // ausgefuehrt: deshalb je Phase ein eigener Puffer (retired_ haelt den alten bis zum Zaun).
        if(shadow_buffer_)retired_.push_back(std::move(shadow_buffer_));shadow_capacity_=0;
        write_structured(shadow_buffer_,shadow_capacity_,triangles_.data(),triangles_.size(),sizeof(GpuShadowTri));
        bind_targets(640,480);
        cmd_->SetPipelineState(mask_pso_.get());
        srv_block(setup_buffer_.get(),setups_.size(),sizeof(GpuSetup),blank_float_.get(),blank_uint_.get());
        cmd_->DrawInstanced(UINT(triangles_.size()*6),1,0,0);++draw_calls_;uav_barrier();
        bind_targets(W,H);
    }
    // true: neue Maske (groesser), Deskriptor u3 neu; der Aufrufer legt einen neuen UAV-Block an.
    bool mask_target(unsigned layers){
        if(mask_&&mask_layers_>=layers)return false;
        mask_layers_=layers;
        if(mask_)retired_.push_back(std::move(mask_));
        // R32_UINT statt R8_UINT: typisierte UAV-Lesezugriffe sind fuer R32_UINT auf jedem D3D12-Geraet garantiert;
        // Werte 0..255 unveraendert (wie das Vulkan-Backend).
        mask_=committed(D3D12_HEAP_TYPE_DEFAULT,texture_desc(640,480,UINT16(layers),1,DXGI_FORMAT_R32_UINT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,"mask texture");
        D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};ud.Format=DXGI_FORMAT_R32_UINT;ud.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;ud.Texture2DArray.ArraySize=layers;
        device_->CreateUnorderedAccessView(mask_.get(),nullptr,&ud,uav_cpu(3));
        return true;}
    void composite(const raster::ShadowWork& c,std::size_t index,int W,int H){
        if(c.fog_power>255u)throw std::runtime_error("shadow: invalid fog channel");
        const CompositeCB u{float(W),float(H),c.rx0,c.ry0,c.rx1,c.ry1,c.fog_power,std::uint32_t(index)};
        cmd_->SetGraphicsRoot32BitConstants(RootComposite,sizeof u/4,&u,0);
        bind_targets(W,H);
        cmd_->SetPipelineState(shadow_pso_.get());
        cmd_->DrawInstanced(3,1,0,0);++draw_calls_;++composites_;uav_barrier();}
    void resolve(int width,int height,int S){
        const ResolveCB rc{{std::uint32_t(width),std::uint32_t(height),std::uint32_t(S),0}};
        cmd_->SetComputeRoot32BitConstants(RootResolve,sizeof rc/4,&rc,0);
        cmd_->SetComputeRootDescriptorTable(RootUav,visible_gpu(uav_block_));
        cmd_->SetPipelineState(resolve_pso_.get());
        cmd_->Dispatch(UINT((width+15)/16),UINT((height+15)/16),1);uav_barrier();}
    Com<ID3D12Device> device_;PFN_Compile compile_;std::string adapter_;
    Com<ID3D12CommandQueue> queue_;Com<ID3D12CommandAllocator> allocator_;Com<ID3D12GraphicsCommandList> cmd_;Com<ID3D12Fence> fence_;HANDLE event_=nullptr;UINT64 fence_value_=0;bool recording_=false;
    Com<ID3D12RootSignature> root_;Com<ID3D12PipelineState> raster_pso_,mask_pso_,shadow_pso_,resolve_pso_;
    Com<ID3D12DescriptorHeap> cpu_heap_,visible_heap_,sampler_heap_;std::vector<Com<ID3D12DescriptorHeap>> retired_heaps_;
    UINT descriptor_size_=0,sampler_size_=0;std::size_t visible_capacity_=0,visible_used_=0,uav_block_=0;
    Com<ID3D12Resource> color_,depth_,transported_,mask_,out_color_,out_depth_,read_color_,read_depth_,setup_buffer_,shadow_buffer_,blank_float_,blank_uint_;
    std::vector<Com<ID3D12Resource>> uploads_,retired_;
    std::vector<GpuSetup> setups_;std::vector<GpuShadowTri> triangles_;std::vector<Batch> batches_;
    std::vector<std::pair<std::shared_ptr<TextureFilterCache>,Variant>> pending_;
    // Schluessel fuer Kopien, deren Upload im laufenden Bild aufgezeichnet, aber noch nicht ausgefuehrt ist.
    const std::shared_ptr<const void> pending_owner_=std::make_shared<const char>(0);
    std::vector<std::string> unsupported_;Timings timings_;
    std::size_t setup_capacity_=0,shadow_capacity_=0;unsigned draw_calls_=0,composites_=0,mask_layers_=0;int target_w_=0,target_h_=0;
};
}
std::shared_ptr<GpuRaster> create_d3d12_raster(std::string& reason){
    const HMODULE d3d12=LoadLibraryA("d3d12.dll");
    if(!d3d12){reason="d3d12.dll not available";return nullptr;}
    const HMODULE dxgi=LoadLibraryA("dxgi.dll");
    if(!dxgi){reason="dxgi.dll not available";return nullptr;}
    const HMODULE compiler=LoadLibraryA("d3dcompiler_47.dll");
    if(!compiler){reason="d3dcompiler_47.dll not available";return nullptr;}
    const auto create=reinterpret_cast<PFN_CreateDevice>(reinterpret_cast<void*>(GetProcAddress(d3d12,"D3D12CreateDevice")));
    const auto serialize=reinterpret_cast<PFN_SerializeRootSignature>(reinterpret_cast<void*>(GetProcAddress(d3d12,"D3D12SerializeRootSignature")));
    const auto factory1=reinterpret_cast<PFN_CreateFactory1>(reinterpret_cast<void*>(GetProcAddress(dxgi,"CreateDXGIFactory1")));
    const auto compile=reinterpret_cast<PFN_Compile>(reinterpret_cast<void*>(GetProcAddress(compiler,"D3DCompile")));
    if(!create||!serialize||!factory1||!compile){reason="Direct3D 12 entry points missing";return nullptr;}
    char env[16]{};
    // RECVX_D3D12_DEBUG=1: Validierungsschicht (Meldungen im Debugger-Ausgabefenster), vor dem Geraet.
    if(GetEnvironmentVariableA("RECVX_D3D12_DEBUG",env,sizeof env)>0&&std::string(env)=="1"){
        if(const auto get=reinterpret_cast<PFN_GetDebugInterface>(reinterpret_cast<void*>(GetProcAddress(d3d12,"D3D12GetDebugInterface")))){
            DebugLayer* debug=nullptr;if(SUCCEEDED(get(iid_debug_layer,reinterpret_cast<void**>(&debug)))&&debug){debug->EnableDebugLayer();debug->Release();}}}
    // RECVX_D3D12_DRIVER=warp: Microsofts Software-Referenz (in Windows enthalten, mit ROV) statt des
    // Grafiktreibers. Trennt Fehler des Backends von Fehlern eines Treibers.
    const bool warp=GetEnvironmentVariableA("RECVX_D3D12_DRIVER",env,sizeof env)>0&&std::string(env)=="warp";
    Com<IDXGIFactory1> factory;
    if(FAILED(factory1(__uuidof(IDXGIFactory1),factory.put_void()))){reason="no DXGI factory";return nullptr;}
    Com<IDXGIAdapter1> adapter;std::string adapter_name=warp?"WARP":"device";
    if(warp){
        Com<IDXGIFactory4> factory4;
        if(FAILED(factory->QueryInterface(__uuidof(IDXGIFactory4),factory4.put_void()))||FAILED(factory4->EnumWarpAdapter(__uuidof(IDXGIAdapter1),adapter.put_void()))){
            reason="no Direct3D 12 WARP adapter";return nullptr;}
    }else{
        // Erster Hardware-Adapter mit D3D12-Geraet (Software-Adapter uebersprungen).
        for(UINT i=0;factory->EnumAdapters1(i,adapter.put())==S_OK;++i){
            DXGI_ADAPTER_DESC1 desc{};if(FAILED(adapter->GetDesc1(&desc))||(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))continue;
            if(SUCCEEDED(create(adapter.get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr))){
                char text[128]{};WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,text,sizeof text-1,nullptr,nullptr);adapter_name=text;break;}
            adapter.reset();}
        if(!adapter){reason="no Direct3D 12 hardware device";return nullptr;}
    }
    Com<ID3D12Device> device;
    if(FAILED(create(adapter.get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),device.put_void()))){
        reason=warp?"no Direct3D 12 WARP device":"no Direct3D 12 hardware device";return nullptr;}
    {// Ohne Rasterizer Ordered Views gibt es keine garantierte Zeichenreihenfolge im Pixelshader und
     // damit keine exakte Abbildung der Bandstufe: dann Softwarepfad statt aehnlichem Mischen.
     D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
     if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof options))||!options.ROVsSupported){
        reason="GPU without rasterizer ordered views";return nullptr;}}
    try{return std::make_shared<D3D12Raster>(std::move(device),compile,serialize,adapter_name);}
    catch(const std::exception& e){reason=e.what();return nullptr;}
}
}
