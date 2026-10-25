// Vulkan-Backend des Host-GPU-Rasterers (gpu_raster.h), auf allen Desktopplattformen ausdruecklich waehlbar
// (RECVX_ENABLE_VULKAN). Plattformanpassung, keine Spiellogik: Clipping, Streifenregeln, Sortierung und
// Schattenphasen bleiben im SoftwareRenderer; hier wird nur dieselbe raster::Setup-/ShadowWork-Liste mit den
// Pixelregeln der Bandstufe gerastert und Bild/Tiefe zurueckgegeben.
//
// Gegenstueck zum programmierbaren Mischen (Metal) und zu den Rasterizer Ordered Views (D3D11) ist
// VK_EXT_fragment_shader_interlock (fragmentShaderPixelInterlock): Farbe, 1/z, transportierte Tiefe und
// Schattenmasken liegen als Storage-Images vor; der Fragmentshader liest den bisherigen Wert in
// Primitivreihenfolge und schreibt ihn wie die Software. Ohne Interlock gibt es kein Backend (Softwarepfad),
// statt eines nur aehnlich wirkenden Mischens. Kein Festfunktionsmischen, kein Tiefentest, kein Swapchain:
// das Bild wird offscreen gerastert und zurueckgelesen; die Praesentation bleibt beim SDL-Pfad der Runtime.
//
// Shader: native/gpu_raster_vulkan.glsl, zur Buildzeit mit glslc nach SPIR-V (precise = NoContraction,
// SignedZeroInfNanPreserve). Division/Rundung bleibt Treibereigenschaft: Bitgleichheit belegt erst der
// Paritaetstest auf genau diesem Treiber.
//
// Lader: die Vulkan-Bibliothek wird zur Laufzeit geoeffnet (Loader oder MoltenVK als ICD); fehlt sie,
// liefert create_vulkan_raster nullptr mit Begruendung.
#include "gpu_raster.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace recvx {
namespace {
// SPIR-V der Stufen (gpu_raster_vulkan.glsl, glslc -mfmt=num).
const std::uint32_t raster_vs_spv[]={
#include "gpu_raster_vulkan_raster_vs.inc"
};
const std::uint32_t raster_fs_spv[]={
#include "gpu_raster_vulkan_raster_fs.inc"
};
const std::uint32_t mask_vs_spv[]={
#include "gpu_raster_vulkan_mask_vs.inc"
};
const std::uint32_t mask_fs_spv[]={
#include "gpu_raster_vulkan_mask_fs.inc"
};
const std::uint32_t full_vs_spv[]={
#include "gpu_raster_vulkan_full_vs.inc"
};
const std::uint32_t composite_fs_spv[]={
#include "gpu_raster_vulkan_composite_fs.inc"
};
const std::uint32_t resolve_cs_spv[]={
#include "gpu_raster_vulkan_resolve_cs.inc"
};

#define RECVX_VK_INSTANCE(X) \
    X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceFeatures2) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkEnumerateDeviceExtensionProperties) X(vkGetPhysicalDeviceFormatProperties) X(vkGetPhysicalDeviceProperties2) X(vkCreateDevice) X(vkGetDeviceProcAddr) X(vkDestroyInstance)
#define RECVX_VK_DEVICE(X) \
    X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkDeviceWaitIdle) X(vkQueueSubmit) X(vkQueueWaitIdle) \
    X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandBuffer) \
    X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) X(vkResetFences) \
    X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) \
    X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) \
    X(vkCreateImageView) X(vkDestroyImageView) X(vkCreateSampler) X(vkDestroySampler) \
    X(vkAllocateMemory) X(vkFreeMemory) X(vkMapMemory) X(vkUnmapMemory) \
    X(vkCreateShaderModule) X(vkDestroyShaderModule) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) \
    X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) X(vkCreateGraphicsPipelines) X(vkCreateComputePipelines) \
    X(vkDestroyPipeline) X(vkCreateRenderPass) X(vkDestroyRenderPass) X(vkCreateFramebuffer) X(vkDestroyFramebuffer) \
    X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) X(vkResetDescriptorPool) X(vkAllocateDescriptorSets) \
    X(vkUpdateDescriptorSets) X(vkCmdBeginRenderPass) X(vkCmdEndRenderPass) X(vkCmdBindPipeline) \
    X(vkCmdBindDescriptorSets) X(vkCmdPushConstants) X(vkCmdDraw) X(vkCmdDispatch) X(vkCmdPipelineBarrier) \
    X(vkCmdClearColorImage) X(vkCmdCopyBuffer) X(vkCmdCopyBufferToImage) X(vkCmdSetViewport) X(vkCmdSetScissor)

void require(VkResult r,const char* what){if(r!=VK_SUCCESS)throw std::runtime_error(std::string("Vulkan ")+what+": "+std::to_string(int(r)));}
// Vulkan-Struktur mit Nullwerten und gesetztem sType (pNext = nullptr).
template<class T> T info(VkStructureType type){T value{};value.sType=type;return value;}

// Loader-Bibliothek, Instanz und logisches Geraet. Eigener Besitzer, damit Texturen, die nach dem Backend
// sterben (TextureFilterCache::gpu), ihre Objekte noch an einem lebenden Geraet freigeben.
struct Device {
    void* library=nullptr;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr=nullptr;
    VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;std::uint32_t family=0;VkPhysicalDeviceMemoryProperties memory{};
    std::string name;float max_anisotropy=1;bool anisotropy=false;std::uint32_t max_layers=256;
#define RECVX_VK_MEMBER(name) PFN_##name name=nullptr;
    RECVX_VK_INSTANCE(RECVX_VK_MEMBER) RECVX_VK_DEVICE(RECVX_VK_MEMBER)
#undef RECVX_VK_MEMBER
    // Freizugebende Objekte von Texturen, deren Cacheeintrag starb; geloescht, sobald kein Kommandopuffer sie
    // mehr benutzen kann (Beginn des naechsten Bildes einer Instanz oder Abbau).
    struct Dead{VkImage image;VkImageView view;VkDeviceMemory memory;};
    std::mutex mutex;std::vector<Dead> dead;
    // Alle Instanzen dieses Geraets zeichnen nacheinander: ein Bild haelt diesen Riegel vom Aufzeichnen bis
    // nach dem Warten auf seinen Zaun. Damit ist zu Beginn jedes Bildes keine Arbeit des Geraets offen (collect
    // ist sicher) und die Warteschlange wird nie von zwei Threads zugleich beschickt.
    std::mutex frame;
    void collect(){
        std::vector<Dead> list;{std::lock_guard<std::mutex> lock(mutex);list.swap(dead);}
        for(const auto& d:list){vkDestroyImageView(device,d.view,nullptr);vkDestroyImage(device,d.image,nullptr);vkFreeMemory(device,d.memory,nullptr);}}
    ~Device(){
        if(device){vkDeviceWaitIdle(device);collect();vkDestroyDevice(device,nullptr);}
        if(instance)vkDestroyInstance(instance,nullptr);
#if defined(_WIN32)
        if(library)FreeLibrary(static_cast<HMODULE>(library));
#else
        if(library)dlclose(library);
#endif
    }
    std::uint32_t memory_type(std::uint32_t bits,VkMemoryPropertyFlags wanted)const{
        for(std::uint32_t i=0;i<memory.memoryTypeCount;++i)
            if((bits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&wanted)==wanted)return i;
        throw std::runtime_error("Vulkan memory type: none suitable");}
};

struct alignas(16) GpuSetup{float p0[4],p1[4],p2[4],uv0[4],uv1[4],grad[4],l0[4],l1[4],l2[4],fog[4];std::uint32_t info[4],blend[4];std::int32_t box[4];};
static_assert(sizeof(GpuSetup)==208,"setup layout");
struct alignas(16) GpuShadowTri{float v01[4],v2a[4];std::uint32_t depth[4];std::int32_t box[4];};
static_assert(sizeof(GpuShadowTri)==64,"shadow triangle layout");
// Push-Konstanten wie Push in gpu_raster_vulkan.glsl (std430: vec2 auf 8, vec4 auf 16 ausgerichtet).
struct Push{float W,H;std::uint32_t fog_pixel,wireframe,background,mask_w,mask_h,layer;float rect[4];std::uint32_t fog_power,scale,out_w,out_h;};
static_assert(sizeof(Push)==64,"push constant layout");
// Darstellungsart einer Textur, Index in TextureFilterCache::gpu wie bei Metal/D3D11/OpenGL.
enum Variant{Point=1,Straight=2,Premultiplied=3,Punch=4};
// GPU-Kopie einer Scene-Textur. Haelt das Geraet am Leben: Caches koennen jedes Backend ueberleben.
struct Texture{VkImage image=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;std::shared_ptr<Device> owner;};
struct Batch{std::size_t first,count;VkImageView texture;bool point;unsigned sampler;};
struct Buffer{VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void* mapped=nullptr;VkDeviceSize size=0;};
struct Image{VkImage image=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;};

class VulkanRaster final:public GpuRaster{
public:
    explicit VulkanRaster(std::shared_ptr<Device> device):dev_(std::move(device)),d_(*dev_){
        {auto ci=info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);ci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;ci.queueFamilyIndex=d_.family;
         require(d_.vkCreateCommandPool(d_.device,&ci,nullptr,&pool_),"command pool");
         // cmd_: ein Bild; once_: einmalige Einreichungen ausserhalb eines Bildes (Flaechen, Platzhalter).
         VkCommandBuffer buffers[2];
         auto ai=info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);ai.commandPool=pool_;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=2;
         require(d_.vkAllocateCommandBuffers(d_.device,&ai,buffers),"command buffer");cmd_=buffers[0];once_=buffers[1];
         auto fi=info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);require(d_.vkCreateFence(d_.device,&fi,nullptr,&fence_),"fence");}
        // Satz 0: Setups, Schattendreiecke, Farbe, 1/z, transportierte Tiefe, Masken, Ausgabe Farbe/Tiefe.
        {const VkDescriptorType types[8]={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
         std::array<VkDescriptorSetLayoutBinding,8> b{};
         for(std::uint32_t i=0;i<8;++i){b[i].binding=i;b[i].descriptorType=types[i];b[i].descriptorCount=1;
             b[i].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT;}
         auto ci=info<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);ci.bindingCount=8;ci.pBindings=b.data();
         require(d_.vkCreateDescriptorSetLayout(d_.device,&ci,nullptr,&frame_layout_),"descriptor layout");}
        // Satz 1: gefilterte Textur mit Sampler, Punkttextur (texelFetch, Sampler ohne Wirkung).
        {std::array<VkDescriptorSetLayoutBinding,2> b{};
         for(std::uint32_t i=0;i<2;++i){b[i].binding=i;b[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;b[i].descriptorCount=1;b[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;}
         auto ci=info<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);ci.bindingCount=2;ci.pBindings=b.data();
         require(d_.vkCreateDescriptorSetLayout(d_.device,&ci,nullptr,&texture_layout_),"texture layout");}
        {const VkDescriptorSetLayout sets[2]={frame_layout_,texture_layout_};
         VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Push)};
         auto ci=info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);ci.setLayoutCount=2;ci.pSetLayouts=sets;ci.pushConstantRangeCount=1;ci.pPushConstantRanges=&range;
         require(d_.vkCreatePipelineLayout(d_.device,&ci,nullptr,&layout_),"pipeline layout");}
        // Ein Durchgang ohne Anhaenge: Rasterung legt nur Fragmente an, alle Ergebnisse gehen ueber Storage-Images.
        {VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
         auto ci=info<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);ci.subpassCount=1;ci.pSubpasses=&sub;
         require(d_.vkCreateRenderPass(d_.device,&ci,nullptr,&pass_),"render pass");}
        raster_=graphics(raster_vs_spv,sizeof raster_vs_spv,raster_fs_spv,sizeof raster_fs_spv);
        mask_=graphics(mask_vs_spv,sizeof mask_vs_spv,mask_fs_spv,sizeof mask_fs_spv);
        composite_=graphics(full_vs_spv,sizeof full_vs_spv,composite_fs_spv,sizeof composite_fs_spv);
        {const VkShaderModule m=module(resolve_cs_spv,sizeof resolve_cs_spv);
         auto ci=info<VkComputePipelineCreateInfo>(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
         ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,m,"main",nullptr};ci.layout=layout_;
         const VkResult r=d_.vkCreateComputePipelines(d_.device,VK_NULL_HANDLE,1,&ci,nullptr,&resolve_);d_.vkDestroyShaderModule(d_.device,m,nullptr);require(r,"resolve pipeline");}
        // Sampler wie die anderen Backends: [linear_only][clamp_u][clamp_v]; trilinear mit 4x anisotrop, sonst bilinear Stufe 0.
        for(int linear=0;linear<2;++linear)for(int cu=0;cu<2;++cu)for(int cv=0;cv<2;++cv){
            auto ci=info<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);ci.magFilter=ci.minFilter=VK_FILTER_LINEAR;
            ci.mipmapMode=linear?VK_SAMPLER_MIPMAP_MODE_NEAREST:VK_SAMPLER_MIPMAP_MODE_LINEAR;
            ci.addressModeU=cu?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:VK_SAMPLER_ADDRESS_MODE_REPEAT;
            ci.addressModeV=cv?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:VK_SAMPLER_ADDRESS_MODE_REPEAT;ci.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
            ci.anisotropyEnable=(!linear&&d_.anisotropy)?VK_TRUE:VK_FALSE;ci.maxAnisotropy=std::min(4.f,d_.max_anisotropy);
            ci.minLod=0;ci.maxLod=linear?0.f:VK_LOD_CLAMP_NONE;
            require(d_.vkCreateSampler(d_.device,&ci,nullptr,&samplers_[linear][cu][cv]),"sampler");}
        {auto ci=info<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);ci.magFilter=ci.minFilter=VK_FILTER_NEAREST;ci.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
         ci.addressModeU=ci.addressModeV=ci.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
         require(d_.vkCreateSampler(d_.device,&ci,nullptr,&nearest_),"point sampler");}
        // 1x1-Platzhalter fuer die jeweils nicht benutzte Texturbindung (der Shader liest sie nicht).
        // Einreichung unter dem Bildriegel: eine andere Instanz desselben Geraets kann gerade zeichnen.
        {std::lock_guard<std::mutex> lock(d_.frame);const std::uint32_t zero=0;blank_point_=upload_image(VK_FORMAT_R32_UINT,1,1,{&zero},4);blank_=upload_image(VK_FORMAT_B8G8R8A8_UNORM,1,1,{&zero},4);}
    }
    ~VulkanRaster()override{
        std::lock_guard<std::mutex> lock(d_.frame);d_.vkDeviceWaitIdle(d_.device);
        for(auto& s:samplers_)for(auto& t:s)for(auto x:t)d_.vkDestroySampler(d_.device,x,nullptr);
        d_.vkDestroySampler(d_.device,nearest_,nullptr);
        destroy(blank_);destroy(blank_point_);
        for(auto* i:{&color_,&depth_,&transported_,&mask_image_})destroy(*i);
        for(auto* b:{&setup_buffer_,&tri_buffer_,&out_color_,&out_depth_})destroy(*b);
        for(auto& b:staging_uploads_)destroy(b);
        if(framebuffer_)d_.vkDestroyFramebuffer(d_.device,framebuffer_,nullptr);
        if(mask_framebuffer_)d_.vkDestroyFramebuffer(d_.device,mask_framebuffer_,nullptr);
        for(auto p:pools_)d_.vkDestroyDescriptorPool(d_.device,p,nullptr);
        for(auto p:{raster_,mask_,composite_,resolve_})d_.vkDestroyPipeline(d_.device,p,nullptr);
        d_.vkDestroyRenderPass(d_.device,pass_,nullptr);d_.vkDestroyPipelineLayout(d_.device,layout_,nullptr);
        d_.vkDestroyDescriptorSetLayout(d_.device,texture_layout_,nullptr);d_.vkDestroyDescriptorSetLayout(d_.device,frame_layout_,nullptr);
        d_.vkDestroyFence(d_.device,fence_,nullptr);d_.vkDestroyCommandPool(d_.device,pool_,nullptr);
    }
    std::string name()const override{return "Vulkan "+d_.name;}
    const std::vector<std::string>& unsupported()const override{return unsupported_;}
    unsigned draw_calls()const override{return draw_calls_;}
    unsigned shadow_composites()const override{return composites_;}
    const Timings& timings()const override{return timings_;}
    bool render(const raster::Frame& frame,const raster::ShadowGeometry& shadows,std::vector<std::uint32_t>& pixels,std::vector<float>& depth)override{
        try{const bool drawn=render_frame(frame,shadows,pixels,depth);commit_uploads();return drawn;}
        catch(...){
            // Eine schon eingereichte Arbeit (Fehler erst beim Warten) darf die verworfenen Kopien noch lesen:
            // vor deren Freigabe auf das Geraet warten (bei verlorenem Geraet kehrt das sofort zurueck).
            {std::lock_guard<std::mutex> lock(d_.frame);d_.vkDeviceWaitIdle(d_.device);}
            drop_uploads();throw;}
    }
private:
    bool render_frame(const raster::Frame& frame,const raster::ShadowGeometry& shadows,std::vector<std::uint32_t>& pixels,std::vector<float>& depth){
        using Clock=std::chrono::steady_clock;
        const auto since=[](Clock::time_point from){return std::chrono::duration<double>(Clock::now()-from).count();};
        const auto& setups=*frame.setups;const int S=frame.supersampling,W=frame.width*S,H=frame.height*S;
        if(W<=0||H<=0)throw std::runtime_error("Vulkan render target is empty");
        std::lock_guard<std::mutex> lock(d_.frame);
        // Unter dem Bildriegel ist keine Arbeit des Geraets offen: tote Texturen und Hochladepuffer sind frei.
        d_.collect();for(auto& b:staging_uploads_)destroy(b);staging_uploads_.clear();uploads_.clear();
        draw_calls_=0;composites_=0;tri_offset_=0;
        for(auto p:pools_)require(d_.vkResetDescriptorPool(d_.device,p,0),"descriptor pool reset");pool_used_=0;
        targets(W,H);
        {const auto t=Clock::now();build(frame);timings_.build+=since(t);}
        const auto encode_begin=Clock::now();
        const std::size_t n=std::size_t(frame.width)*frame.height;
        // Alle Bindungen vor dem Aufzeichnen: Setups, Ausgabe (hostsichtbar, direkt gelesen), Maske, Dreiecke.
        write(setup_buffer_,setups_.data(),setups_.size()*sizeof(GpuSetup),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ensure(out_color_,std::max<std::size_t>(n,1)*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);ensure(out_depth_,std::max<std::size_t>(n,1)*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ensure(tri_buffer_,sizeof(GpuShadowTri),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);mask_target(1);
        begin();
        // Uploads (Texturen dieses Bildes) vor allem anderen; dann Farbe = Hintergrund, beide Tiefen = 0.
        for(const auto& u:uploads_)record_copy(u.image,staging_uploads_[u.buffer].buffer,u.regions);
        barrier(VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        {VkClearColorValue bg{};bg.uint32[0]=frame.background;VkClearColorValue zero{};const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
         d_.vkCmdClearColorImage(cmd_,color_.image,VK_IMAGE_LAYOUT_GENERAL,&bg,1,&range);
         d_.vkCmdClearColorImage(cmd_,depth_.image,VK_IMAGE_LAYOUT_GENERAL,&zero,1,&range);
         d_.vkCmdClearColorImage(cmd_,transported_.image,VK_IMAGE_LAYOUT_GENERAL,&zero,1,&range);}
        barrier(VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
        const Push push{float(W),float(H),frame.fog_pixel,frame.wireframe?1u:0u,frame.background,std::uint32_t(W),std::uint32_t(H),0,{0,0,0,0},0,std::uint32_t(S),std::uint32_t(frame.width),std::uint32_t(frame.height)};
        VkDescriptorSet frame_set=frame_descriptors();
        const std::size_t ends[3]={frame.shadows?frame.before_modifiers:setups.size(),frame.shadows?frame.before_mirror_effects:setups.size(),setups.size()};
        std::size_t batch=0;
        for(unsigned phase=0;phase<3;++phase){
            begin_pass(framebuffer_,W,H);
            d_.vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,raster_);
            d_.vkCmdBindDescriptorSets(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&frame_set,0,nullptr);
            d_.vkCmdPushConstants(cmd_,layout_,all_stages,0,sizeof push,&push);
            VkImageView bound=VK_NULL_HANDLE;unsigned bound_sampler=~0u;bool bound_point=false;
            for(;batch<batches_.size()&&batches_[batch].first<ends[phase];++batch){const auto& b=batches_[batch];
                if(b.texture!=bound||b.sampler!=bound_sampler||b.point!=bound_point){
                    const VkDescriptorSet set=texture_descriptors(b);
                    d_.vkCmdBindDescriptorSets(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,1,1,&set,0,nullptr);
                    bound=b.texture;bound_sampler=b.sampler;bound_point=b.point;}
                // Vertex 6*first..: gl_VertexIndex enthaelt firstVertex (Vulkan-Vorgabe), der Shader teilt durch 6.
                d_.vkCmdDraw(cmd_,std::uint32_t(b.count*6),1,std::uint32_t(b.first*6),0);++draw_calls_;}
            d_.vkCmdEndRenderPass(cmd_);
            barrier(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            if(phase<2&&frame.shadows&&frame.shadow_pass[phase]&&shadows){
                // Schattenwerke dieses Modifierpasses (CPU, haengen nicht vom GPU-Bild ab), dann Masken aus der
                // transportierten Tiefe des bisherigen Bildes und die Komposite in Werkreihenfolge.
                const auto shadow_begin=Clock::now();const auto works=shadows(phase==1);timings_.build+=since(shadow_begin);
                if(works.empty())continue;
                if(works.size()>d_.max_layers)throw std::runtime_error("too many shadow works in one pass");
                std::size_t faces=0;for(const auto& w:works)faces+=w.faces.size();
                if(works.size()>mask_layers_||(tri_offset_+faces*sizeof(GpuShadowTri))>tri_buffer_.size){
                    // Wachsen ersetzt gebundene Objekte: das Aufgezeichnete zuerst ausfuehren, dann neu binden.
                    submit_and_wait(cmd_);tri_offset_=0;
                    mask_target(std::uint32_t(works.size()));ensure(tri_buffer_,faces*sizeof(GpuShadowTri),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                    begin();frame_set=frame_descriptors();}
                masks(works,W,H,push,frame_set);}
        }
        // Aufloesung direkt in die hostsichtbaren Ausgabepuffer.
        d_.vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_COMPUTE,resolve_);
        d_.vkCmdBindDescriptorSets(cmd_,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&frame_set,0,nullptr);
        d_.vkCmdPushConstants(cmd_,layout_,all_stages,0,sizeof push,&push);
        d_.vkCmdDispatch(cmd_,std::uint32_t((frame.width+15)/16),std::uint32_t((frame.height+15)/16),1);
        {auto mb=info<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);mb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;mb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
         d_.vkCmdPipelineBarrier(cmd_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&mb,0,nullptr,0,nullptr);}
        timings_.encode+=since(encode_begin);
        {const auto t=Clock::now();submit_and_wait(cmd_);timings_.wait+=since(t);++timings_.submits;}
        {const auto t=Clock::now();pixels.resize(n);depth.resize(n);
         if(n){std::memcpy(pixels.data(),out_color_.mapped,n*4);std::memcpy(depth.data(),out_depth_.mapped,n*4);}
         timings_.readback+=since(t);}
        return true;
    }
    // Hochzuladende Textur dieses Bildes: Quelle im Hostpuffer, Ziel-Image mit allen Stufen.
    struct Upload{VkImage image;std::size_t buffer;std::vector<VkBufferImageCopy> regions;};
    static constexpr VkShaderStageFlags all_stages=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT;
    VkShaderModule module(const std::uint32_t* code,std::size_t size){
        auto ci=info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);ci.codeSize=size;ci.pCode=code;VkShaderModule m;
        require(d_.vkCreateShaderModule(d_.device,&ci,nullptr,&m),"shader module");return m;}
    // Kein Festfunktionszustand darf das Ergebnis veraendern: kein Mischen, kein Tiefen-/Schablonentest, kein
    // Verwerfen von Rueckseiten, Einfachabtastung; Viewport/Scissor dynamisch je Durchgang.
    VkPipeline graphics(const std::uint32_t* vs,std::size_t vs_size,const std::uint32_t* fs,std::size_t fs_size){
        const VkShaderModule v=module(vs,vs_size),f=module(fs,fs_size);
        VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,v,"main",nullptr},
                                                   {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,f,"main",nullptr}};
        auto vi=info<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
        auto ia=info<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        auto vp=info<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);vp.viewportCount=1;vp.scissorCount=1;
        auto rs=info<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
        rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
        auto ms=info<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        auto ds=info<VkPipelineDepthStencilStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
        auto cb=info<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
        const VkDynamicState dyn[2]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        auto dy=info<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);dy.dynamicStateCount=2;dy.pDynamicStates=dyn;
        auto ci=info<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);ci.stageCount=2;ci.pStages=stages;
        ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;
        ci.pDepthStencilState=&ds;ci.pColorBlendState=&cb;ci.pDynamicState=&dy;ci.layout=layout_;ci.renderPass=pass_;ci.subpass=0;
        VkPipeline p;const VkResult r=d_.vkCreateGraphicsPipelines(d_.device,VK_NULL_HANDLE,1,&ci,nullptr,&p);
        d_.vkDestroyShaderModule(d_.device,v,nullptr);d_.vkDestroyShaderModule(d_.device,f,nullptr);require(r,"graphics pipeline");return p;}
    void destroy(Image& i){if(i.view)d_.vkDestroyImageView(d_.device,i.view,nullptr);if(i.image)d_.vkDestroyImage(d_.device,i.image,nullptr);if(i.memory)d_.vkFreeMemory(d_.device,i.memory,nullptr);i={};}
    void destroy(Buffer& b){if(b.buffer)d_.vkDestroyBuffer(d_.device,b.buffer,nullptr);if(b.memory)d_.vkFreeMemory(d_.device,b.memory,nullptr);b={};}
    // Hostsichtbarer, kohaerenter Puffer, dauerhaft abgebildet; waechst nur.
    void ensure(Buffer& b,std::size_t bytes,VkBufferUsageFlags usage){
        if(b.buffer&&b.size>=bytes)return;
        destroy(b);const VkDeviceSize size=std::max<VkDeviceSize>(256,VkDeviceSize(bytes)*3/2);
        auto ci=info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);ci.size=size;ci.usage=usage|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;require(d_.vkCreateBuffer(d_.device,&ci,nullptr,&b.buffer),"buffer");
        VkMemoryRequirements req;d_.vkGetBufferMemoryRequirements(d_.device,b.buffer,&req);
        auto ai=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);ai.allocationSize=req.size;
        ai.memoryTypeIndex=d_.memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        require(d_.vkAllocateMemory(d_.device,&ai,nullptr,&b.memory),"buffer memory");require(d_.vkBindBufferMemory(d_.device,b.buffer,b.memory,0),"bind buffer");
        require(d_.vkMapMemory(d_.device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped),"map buffer");b.size=size;}
    void write(Buffer& b,const void* data,std::size_t bytes,VkBufferUsageFlags usage){ensure(b,std::max<std::size_t>(bytes,16),usage);if(bytes)std::memcpy(b.mapped,data,bytes);}
    Image image(VkFormat format,std::uint32_t w,std::uint32_t h,std::uint32_t layers,std::uint32_t levels,VkImageUsageFlags usage,VkImageViewType view_type){
        Image i;auto ci=info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={w,h,1};
        ci.mipLevels=levels;ci.arrayLayers=layers;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;
        ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        require(d_.vkCreateImage(d_.device,&ci,nullptr,&i.image),"image");
        VkMemoryRequirements req;d_.vkGetImageMemoryRequirements(d_.device,i.image,&req);
        auto ai=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);ai.allocationSize=req.size;ai.memoryTypeIndex=d_.memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        require(d_.vkAllocateMemory(d_.device,&ai,nullptr,&i.memory),"image memory");require(d_.vkBindImageMemory(d_.device,i.image,i.memory,0),"bind image");
        auto vi=info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);vi.image=i.image;vi.viewType=view_type;vi.format=format;
        vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,layers};require(d_.vkCreateImageView(d_.device,&vi,nullptr,&i.view),"image view");
        return i;}
    void begin(){
        auto bi=info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        require(d_.vkResetCommandBuffer(cmd_,0),"command reset");require(d_.vkBeginCommandBuffer(cmd_,&bi),"command begin");}
    void begin_once(){
        auto bi=info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        require(d_.vkResetCommandBuffer(once_,0),"command reset");require(d_.vkBeginCommandBuffer(once_,&bi),"command begin");}
    // Ausfuehren und warten. Aufrufer halten Device::frame (render) oder sind im Aufbau (Konstruktor):
    // das Geraet bearbeitet dann nichts anderes, also auch keine fremde Einreichung derselben Warteschlange.
    void submit_and_wait(VkCommandBuffer buffer){
        require(d_.vkEndCommandBuffer(buffer),"command end");
        auto si=info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);si.commandBufferCount=1;si.pCommandBuffers=&buffer;
        require(d_.vkResetFences(d_.device,1,&fence_),"fence reset");
        require(d_.vkQueueSubmit(d_.queue,1,&si,fence_),"submit");
        require(d_.vkWaitForFences(d_.device,1,&fence_,VK_TRUE,UINT64_MAX),"wait");}
    // Neue Bildflaechen in VK_IMAGE_LAYOUT_GENERAL ueberfuehren (einmalig, eigene Einreichung).
    void to_general(const std::vector<std::pair<VkImage,std::uint32_t>>& images){
        begin_once();
        std::vector<VkImageMemoryBarrier> barriers;
        for(const auto& [img,layers]:images){auto b=info<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=img;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,layers};
            b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;barriers.push_back(b);}
        d_.vkCmdPipelineBarrier(once_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,std::uint32_t(barriers.size()),barriers.data());
        submit_and_wait(once_);}
    // Aufrufer: render() unter Device::frame, also ohne offene Arbeit des Geraets; Ersetzen ist sicher.
    void targets(int W,int H){
        if(color_.image&&W==width_&&H==height_)return;
        for(auto* i:{&color_,&depth_,&transported_})destroy(*i);
        if(framebuffer_){d_.vkDestroyFramebuffer(d_.device,framebuffer_,nullptr);framebuffer_=VK_NULL_HANDLE;}
        const VkImageUsageFlags usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        color_=image(VK_FORMAT_R32_UINT,std::uint32_t(W),std::uint32_t(H),1,1,usage,VK_IMAGE_VIEW_TYPE_2D);
        depth_=image(VK_FORMAT_R32_SFLOAT,std::uint32_t(W),std::uint32_t(H),1,1,usage,VK_IMAGE_VIEW_TYPE_2D);
        transported_=image(VK_FORMAT_R32_SFLOAT,std::uint32_t(W),std::uint32_t(H),1,1,usage,VK_IMAGE_VIEW_TYPE_2D);
        to_general({{color_.image,1},{depth_.image,1},{transported_.image,1}});
        framebuffer_=empty_framebuffer(std::uint32_t(W),std::uint32_t(H));
        width_=W;height_=H;}
    VkFramebuffer empty_framebuffer(std::uint32_t w,std::uint32_t h){
        auto ci=info<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);ci.renderPass=pass_;ci.width=w;ci.height=h;ci.layers=1;
        VkFramebuffer f;require(d_.vkCreateFramebuffer(d_.device,&ci,nullptr,&f),"framebuffer");return f;}
    void mask_target(std::uint32_t layers){
        if(mask_image_.image&&mask_layers_>=layers)return;
        destroy(mask_image_);
        if(layers>d_.max_layers)throw std::runtime_error("too many shadow works in one pass");
        mask_layers_=std::min(d_.max_layers,std::max(layers,mask_layers_*2));
        // R32UI statt R8UI: Storage-Images mit 8 Bit sind nicht ueberall formatlos les-/schreibbar; Werte bleiben 0..255.
        mask_image_=image(VK_FORMAT_R32_UINT,640,480,mask_layers_,1,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        to_general({{mask_image_.image,mask_layers_}});
        if(!mask_framebuffer_)mask_framebuffer_=empty_framebuffer(640,480);}
    void begin_pass(VkFramebuffer framebuffer,int w,int h){
        auto bi=info<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);bi.renderPass=pass_;bi.framebuffer=framebuffer;bi.renderArea={{0,0},{std::uint32_t(w),std::uint32_t(h)}};
        d_.vkCmdBeginRenderPass(cmd_,&bi,VK_SUBPASS_CONTENTS_INLINE);
        // Kein y-Umklappen: NDC -1 ist Bildzeile 0, wie Setupkoordinaten und gl_FragCoord (Vulkan-Vorgabe).
        const VkViewport vp{0,0,float(w),float(h),0,1};const VkRect2D sc{{0,0},{std::uint32_t(w),std::uint32_t(h)}};
        d_.vkCmdSetViewport(cmd_,0,1,&vp);d_.vkCmdSetScissor(cmd_,0,1,&sc);}
    // Ausfuehrungs- und Speicherabhaengigkeit fuer alle Lese-/Schreibzugriffe der Shader und Transfers.
    void barrier(VkPipelineStageFlags src,VkPipelineStageFlags dst){
        auto mb=info<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
        mb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;
        mb.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;
        d_.vkCmdPipelineBarrier(cmd_,src,dst,0,1,&mb,0,nullptr,0,nullptr);}
    VkDescriptorSet allocate(VkDescriptorSetLayout layout){
        for(;;){
            if(pool_used_<pools_.size()){
                auto ai=info<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);ai.descriptorPool=pools_[pool_used_];ai.descriptorSetCount=1;ai.pSetLayouts=&layout;
                VkDescriptorSet set;const VkResult r=d_.vkAllocateDescriptorSets(d_.device,&ai,&set);
                if(r==VK_SUCCESS)return set;
                if(r!=VK_ERROR_OUT_OF_POOL_MEMORY&&r!=VK_ERROR_FRAGMENTED_POOL)require(r,"descriptor set");
                ++pool_used_;continue;}
            const VkDescriptorPoolSize sizes[3]={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,64},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,64},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1024}};
            auto ci=info<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);ci.maxSets=512;ci.poolSizeCount=3;ci.pPoolSizes=sizes;
            VkDescriptorPool p;require(d_.vkCreateDescriptorPool(d_.device,&ci,nullptr,&p),"descriptor pool");pools_.push_back(p);}}
    VkDescriptorSet frame_descriptors(){
        const VkDescriptorSet set=allocate(frame_layout_);
        const VkDescriptorBufferInfo buffers[4]={{setup_buffer_.buffer,0,VK_WHOLE_SIZE},{tri_buffer_.buffer,0,VK_WHOLE_SIZE},{out_color_.buffer,0,VK_WHOLE_SIZE},{out_depth_.buffer,0,VK_WHOLE_SIZE}};
        const VkDescriptorImageInfo images[4]={{VK_NULL_HANDLE,color_.view,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,depth_.view,VK_IMAGE_LAYOUT_GENERAL},
                                               {VK_NULL_HANDLE,transported_.view,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,mask_image_.view,VK_IMAGE_LAYOUT_GENERAL}};
        std::array<VkWriteDescriptorSet,8> w{};
        for(std::uint32_t i=0;i<8;++i){w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w[i].dstSet=set;w[i].dstBinding=i;w[i].descriptorCount=1;}
        for(std::uint32_t i:{0u,1u,6u,7u}){w[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w[i].pBufferInfo=&buffers[i<2?i:i-4];}
        for(std::uint32_t i=2;i<6;++i){w[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;w[i].pImageInfo=&images[i-2];}
        d_.vkUpdateDescriptorSets(d_.device,8,w.data(),0,nullptr);
        return set;}
    VkDescriptorSet texture_descriptors(const Batch& b){
        const VkDescriptorSet set=allocate(texture_layout_);
        const VkSampler sampler=samplers_[b.sampler>>2][(b.sampler>>1)&1][b.sampler&1];
        const VkDescriptorImageInfo images[2]={{sampler,b.point?blank_.view:b.texture,VK_IMAGE_LAYOUT_GENERAL},{nearest_,b.point?b.texture:blank_point_.view,VK_IMAGE_LAYOUT_GENERAL}};
        std::array<VkWriteDescriptorSet,2> w{};
        for(std::uint32_t i=0;i<2;++i){w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w[i].dstSet=set;w[i].dstBinding=i;w[i].descriptorCount=1;
            w[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w[i].pImageInfo=&images[i];}
        d_.vkUpdateDescriptorSets(d_.device,2,w.data(),0,nullptr);
        return set;}
    // Sofortiger Upload (Platzhalter beim Aufbau): eigene Einreichung.
    Image upload_image(VkFormat format,std::uint32_t w,std::uint32_t h,const std::vector<const void*>& levels,std::size_t texel){
        Image i=image(format,w,h,1,std::uint32_t(levels.size()),VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_VIEW_TYPE_2D);
        Buffer stage;std::size_t total=0;{std::uint32_t lw=w,lh=h;for(std::size_t l=0;l<levels.size();++l){total+=std::size_t(lw)*lh*texel;lw=std::max(1u,lw/2);lh=std::max(1u,lh/2);}}
        ensure(stage,total,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        Upload u{i.image,0,{}};std::size_t offset=0;std::uint32_t lw=w,lh=h;
        for(std::size_t l=0;l<levels.size();++l){const std::size_t bytes=std::size_t(lw)*lh*texel;std::memcpy(static_cast<char*>(stage.mapped)+offset,levels[l],bytes);
            VkBufferImageCopy c{};c.bufferOffset=offset;c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,std::uint32_t(l),0,1};c.imageExtent={lw,lh,1};u.regions.push_back(c);
            offset+=bytes;lw=std::max(1u,lw/2);lh=std::max(1u,lh/2);}
        begin_once();record_copy(once_,i.image,stage.buffer,u.regions);submit_and_wait(once_);destroy(stage);
        return i;}
    void record_copy(VkImage target,VkBuffer source,const std::vector<VkBufferImageCopy>& regions){record_copy(cmd_,target,source,regions);}
    void record_copy(VkCommandBuffer buffer,VkImage target,VkBuffer source,const std::vector<VkBufferImageCopy>& regions){
        const std::uint32_t levels=std::uint32_t(regions.size());
        auto b=info<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=target;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,1};
        b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        d_.vkCmdPipelineBarrier(buffer,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        d_.vkCmdCopyBufferToImage(buffer,source,target,VK_IMAGE_LAYOUT_GENERAL,levels,regions.data());}
    VkImageView texture(const SceneTexture& t,Variant variant){
        auto& cache=*t.filter_cache;const auto fingerprint=t.fingerprint();auto& slot=cache.gpu[variant];
        if(slot&&(cache.gpu_owner[variant]==cache_owner()||cache.gpu_owner[variant]==pending_owner_)&&cache.gpu_fingerprint[variant]==fingerprint)
            return static_cast<Texture*>(slot.get())->view;
        const auto upload_begin=std::chrono::steady_clock::now();++timings_.uploads;
        const std::size_t levels=variant==Point?1:1+(variant==Punch?cache.punch_mips.size():cache.mips.size());
        const VkFormat format=variant==Point?VK_FORMAT_R32_UINT:variant==Premultiplied?VK_FORMAT_R16G16B16A16_UNORM:VK_FORMAT_B8G8R8A8_UNORM;
        const std::size_t texel=variant==Premultiplied?8:4;
        Image i=image(format,t.width,t.height,1,std::uint32_t(levels),VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_VIEW_TYPE_2D);
        // Ab hier besitzt der Cache-Slot das Image; scheitert der Hostpuffer, gibt drop_uploads es frei.
        auto* holder=new Texture{i.image,i.view,i.memory,dev_};
        slot=std::shared_ptr<void>(holder,[](void* p){auto* t=static_cast<Texture*>(p);
            {std::lock_guard<std::mutex> lock(t->owner->mutex);t->owner->dead.push_back({t->image,t->view,t->memory});}delete t;});
        cache.gpu_fingerprint[variant]=fingerprint;cache.gpu_owner[variant]=pending_owner_;pending_.push_back({&cache,variant});
        // Daten in einen eigenen Hostpuffer dieses Bildes; kopiert wird am Anfang des Kommandopuffers.
        std::size_t total=0;{unsigned w=t.width,h=t.height;for(std::size_t l=0;l<levels;++l){total+=std::size_t(w)*h*texel;w=std::max(1u,w/2);h=std::max(1u,h/2);}}
        staging_uploads_.emplace_back();Buffer& stage=staging_uploads_.back();ensure(stage,total,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        Upload u{i.image,staging_uploads_.size()-1,{}};
        unsigned w=t.width,h=t.height;std::size_t offset=0;
        for(std::size_t level=0;level<levels;++level){
            const std::uint32_t* src=level==0?t.pixels.data():(variant==Punch?cache.punch_mips[level-1].data():cache.mips[level-1].data());
            char* dst=static_cast<char*>(stage.mapped)+offset;const std::size_t count=std::size_t(w)*h;
            // Punkttextur unveraendert als 0xAARRGGBB (texelFetch liefert dieselben Bits wie die Bandstufe).
            if(variant==Premultiplied){auto* wide=reinterpret_cast<std::uint16_t*>(dst);
                for(std::size_t k=0;k<count;++k){const auto p=src[k];const double a=double(p>>24)/255.0;
                    wide[k*4]=std::uint16_t(double((p>>16)&255)/255.0*a*65535.0+.5);wide[k*4+1]=std::uint16_t(double((p>>8)&255)/255.0*a*65535.0+.5);
                    wide[k*4+2]=std::uint16_t(double(p&255)/255.0*a*65535.0+.5);wide[k*4+3]=std::uint16_t(a*65535.0+.5);}}
            else std::memcpy(dst,src,count*4);
            VkBufferImageCopy c{};c.bufferOffset=offset;c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,std::uint32_t(level),0,1};c.imageExtent={w,h,1};u.regions.push_back(c);
            offset+=count*texel;w=std::max(1u,w/2);h=std::max(1u,h/2);}
        uploads_.push_back(std::move(u));
        // Erst nach der Einreichung gilt die Kopie als vorhanden (render: commit_uploads/drop_uploads).
        timings_.upload+=std::chrono::duration<double>(std::chrono::steady_clock::now()-upload_begin).count();
        return i.view;}
    // Hochgeladene Texturen dieses Bildes gehoeren nach erfolgreicher Einreichung zu dieser Instanz; nach
    // einem Fehler werden ihre Slots verworfen, damit ein spaeteres Bild sie neu hochlaedt.
    void commit_uploads(){for(const auto& [cache,variant]:pending_)if(cache->gpu_owner[variant]==pending_owner_)cache->gpu_owner[variant]=cache_owner();pending_.clear();}
    void drop_uploads(){
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
            const VkImageView view=s.texture?this->texture(*s.texture,variant):blank_point_.view;
            const bool point_sampled=!s.texture||variant==Point;
            const unsigned sampler=unsigned(s.linear_only?4:0)|unsigned(s.clamp_u?2:0)|unsigned(s.clamp_v?1:0);
            // Ein Batch darf keine Phasengrenze ueberspannen (siehe gpu_raster_metal.mm build()).
            const bool phase_start=frame.shadows&&(i==frame.before_modifiers||i==frame.before_mirror_effects);
            if(!phase_start&&!batches_.empty()&&batches_.back().texture==view&&batches_.back().sampler==sampler&&batches_.back().point==point_sampled
               &&batches_.back().first+batches_.back().count==i){++batches_.back().count;continue;}
            batches_.push_back({i,1,view,point_sampled,sampler});
        }
    }
    // Alle Masken dieser Phase (eine Ebene je Werk) in Dreiecksreihenfolge wie raster_shadow_mask, danach die
    // Komposite in Werkreihenfolge. render() hat Maskenebenen und Dreieckspuffer vorher passend gross gemacht.
    void masks(const std::vector<raster::ShadowWork>& works,int W,int H,const Push& push,VkDescriptorSet set){
        tris_.clear();
        for(std::size_t layer=0;layer<works.size();++layer){
            if(works[layer].fog_power>255u)throw std::runtime_error("shadow: invalid fog channel");
            for(const auto& f:works[layer].faces){
                GpuShadowTri t{};
                t.v01[0]=f.x[0];t.v01[1]=f.y[0];t.v01[2]=f.x[1];t.v01[3]=f.y[1];
                t.v2a[0]=f.x[2];t.v2a[1]=f.y[2];t.v2a[2]=f.area;t.v2a[3]=0;
                t.depth[0]=f.depth[0];t.depth[1]=f.depth[1];t.depth[2]=f.depth[2];
                t.depth[3]=(std::uint32_t(layer)<<1)|(f.subtract?1u:0u);
                t.box[0]=f.minx;t.box[1]=f.miny;t.box[2]=f.maxx;t.box[3]=f.maxy;
                tris_.push_back(t);}}
        // Jeder Modifierpass schreibt hinter den vorigen (der Puffer wird erst bei der Einreichung gelesen).
        if(!tris_.empty())std::memcpy(static_cast<char*>(tri_buffer_.mapped)+tri_offset_,tris_.data(),tris_.size()*sizeof(GpuShadowTri));
        {VkClearColorValue zero{};const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,std::uint32_t(works.size())};
         d_.vkCmdClearColorImage(cmd_,mask_image_.image,VK_IMAGE_LAYOUT_GENERAL,&zero,1,&range);
         barrier(VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);}
        if(!tris_.empty()){
            begin_pass(mask_framebuffer_,640,480);
            d_.vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,mask_);
            d_.vkCmdBindDescriptorSets(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&set,0,nullptr);
            d_.vkCmdPushConstants(cmd_,layout_,all_stages,0,sizeof push,&push);
            d_.vkCmdDraw(cmd_,std::uint32_t(tris_.size()*6),1,std::uint32_t(tri_offset_/sizeof(GpuShadowTri)*6),0);++draw_calls_;
            d_.vkCmdEndRenderPass(cmd_);
            barrier(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);}
        tri_offset_+=tris_.size()*sizeof(GpuShadowTri);
        // Komposite in einem Durchgang: der Interlock ordnet sie in Zeichenreihenfolge (je Werk ein Aufruf).
        begin_pass(framebuffer_,W,H);
        d_.vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,composite_);
        d_.vkCmdBindDescriptorSets(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&set,0,nullptr);
        for(std::size_t i=0;i<works.size();++i){const auto& c=works[i];
            Push p=push;p.rect[0]=c.rx0;p.rect[1]=c.ry0;p.rect[2]=c.rx1;p.rect[3]=c.ry1;p.fog_power=c.fog_power;p.layer=std::uint32_t(i);
            d_.vkCmdPushConstants(cmd_,layout_,all_stages,0,sizeof p,&p);
            d_.vkCmdDraw(cmd_,3,1,0,0);++draw_calls_;++composites_;}
        d_.vkCmdEndRenderPass(cmd_);
        barrier(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
    std::shared_ptr<Device> dev_;Device& d_;
    VkCommandPool pool_=VK_NULL_HANDLE;VkCommandBuffer cmd_=VK_NULL_HANDLE,once_=VK_NULL_HANDLE;VkFence fence_=VK_NULL_HANDLE;
    VkDescriptorSetLayout frame_layout_=VK_NULL_HANDLE,texture_layout_=VK_NULL_HANDLE;VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkRenderPass pass_=VK_NULL_HANDLE;VkPipeline raster_=VK_NULL_HANDLE,mask_=VK_NULL_HANDLE,composite_=VK_NULL_HANDLE,resolve_=VK_NULL_HANDLE;
    std::array<std::array<std::array<VkSampler,2>,2>,2> samplers_{};VkSampler nearest_=VK_NULL_HANDLE;
    Image blank_,blank_point_,color_,depth_,transported_,mask_image_;std::uint32_t mask_layers_=0;
    VkFramebuffer framebuffer_=VK_NULL_HANDLE,mask_framebuffer_=VK_NULL_HANDLE;int width_=0,height_=0;
    Buffer setup_buffer_,tri_buffer_,out_color_,out_depth_;std::vector<Buffer> staging_uploads_;std::vector<Upload> uploads_;
    std::vector<VkDescriptorPool> pools_;std::size_t pool_used_=0;std::size_t tri_offset_=0;
    std::vector<GpuSetup> setups_;std::vector<Batch> batches_;std::vector<GpuShadowTri> tris_;
    std::vector<std::pair<TextureFilterCache*,Variant>> pending_;
    // Schluessel fuer Kopien, deren Upload im laufenden Bild aufgezeichnet, aber noch nicht ausgefuehrt ist.
    const std::shared_ptr<const void> pending_owner_=std::make_shared<const char>(0);
    std::vector<std::string> unsupported_;unsigned draw_calls_=0,composites_=0;Timings timings_;
};

void* open_library(std::string& tried){
#if defined(_WIN32)
    char path[1024]={};const DWORD length=GetEnvironmentVariableA("RECVX_VULKAN_LIBRARY",path,sizeof path);
    const char* name=length>0&&length<sizeof path?path:"vulkan-1.dll";
    tried=name;return reinterpret_cast<void*>(LoadLibraryA(name));
#else
    // RECVX_VULKAN_LIBRARY: ausdruecklicher Pfad (z. B. eine bestimmte MoltenVK-/Loader-Datei); gesetzt gilt
    // nur dieser, ohne Rueckfall auf andere Bibliotheken.
    std::vector<std::string> names;
    if(const char* env=std::getenv("RECVX_VULKAN_LIBRARY");env&&*env)names.push_back(env);
    else{
#if defined(__APPLE__)
    // Gebuendelt (Contents/Frameworks) oder Loader/MoltenVK aus dem Suchpfad bzw. Homebrew.
    for(const char* n:{"@executable_path/../Frameworks/libvulkan.1.dylib","@executable_path/../Frameworks/libMoltenVK.dylib","libvulkan.1.dylib","libMoltenVK.dylib",
                       "/usr/local/lib/libvulkan.1.dylib","/opt/homebrew/lib/libvulkan.1.dylib","/opt/homebrew/lib/libMoltenVK.dylib","/usr/local/lib/libMoltenVK.dylib"})names.push_back(n);
#else
    names.push_back("libvulkan.so.1");names.push_back("libvulkan.so");
#endif
    }
    for(const auto& n:names){if(!tried.empty())tried+=", ";tried+=n;if(void* l=dlopen(n.c_str(),RTLD_NOW|RTLD_LOCAL))return l;}
    return nullptr;
#endif
}
void* symbol(void* library,const char* name){
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library),name));
#else
    return dlsym(library,name);
#endif
}
bool has_extension(const std::vector<VkExtensionProperties>& list,const char* name){
    for(const auto& e:list)if(!std::strcmp(e.extensionName,name))return true;return false;}

std::shared_ptr<Device> create_device(std::string& reason){
    auto d=std::make_shared<Device>();std::string tried;
    d->library=open_library(tried);
    if(!d->library){reason="Vulkan library not found ("+tried+")";return nullptr;}
    d->vkGetInstanceProcAddr=reinterpret_cast<PFN_vkGetInstanceProcAddr>(symbol(d->library,"vkGetInstanceProcAddr"));
    if(!d->vkGetInstanceProcAddr){reason="vkGetInstanceProcAddr missing";return nullptr;}
    const auto create_instance=reinterpret_cast<PFN_vkCreateInstance>(d->vkGetInstanceProcAddr(VK_NULL_HANDLE,"vkCreateInstance"));
    const auto instance_extensions=reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(d->vkGetInstanceProcAddr(VK_NULL_HANDLE,"vkEnumerateInstanceExtensionProperties"));
    if(!create_instance||!instance_extensions){reason="Vulkan instance functions missing";return nullptr;}
    std::vector<VkExtensionProperties> available;{std::uint32_t n=0;instance_extensions(nullptr,&n,nullptr);available.resize(n);instance_extensions(nullptr,&n,available.data());available.resize(n);}
    std::vector<const char*> enabled;VkInstanceCreateFlags flags=0;
    // Portabilitaetstreiber (MoltenVK ueber den Loader) werden nur mit dieser Erweiterung aufgezaehlt.
    if(has_extension(available,"VK_KHR_portability_enumeration")){enabled.push_back("VK_KHR_portability_enumeration");flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;}
    if(has_extension(available,"VK_KHR_get_physical_device_properties2"))enabled.push_back("VK_KHR_get_physical_device_properties2");
    auto app=info<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);app.pApplicationName="RECVX Native";app.apiVersion=VK_API_VERSION_1_1;
    auto ici=info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);ici.flags=flags;ici.pApplicationInfo=&app;
    ici.enabledExtensionCount=std::uint32_t(enabled.size());ici.ppEnabledExtensionNames=enabled.data();
    if(const VkResult r=create_instance(&ici,nullptr,&d->instance);r!=VK_SUCCESS){reason="vkCreateInstance failed ("+std::to_string(int(r))+")";d->instance=VK_NULL_HANDLE;return nullptr;}
#define RECVX_VK_LOAD(name) d->name=reinterpret_cast<PFN_##name>(d->vkGetInstanceProcAddr(d->instance,#name)); \
    if(!d->name){reason="Vulkan function " #name " missing";return nullptr;}
    RECVX_VK_INSTANCE(RECVX_VK_LOAD)
#undef RECVX_VK_LOAD
    std::vector<VkPhysicalDevice> devices;{std::uint32_t n=0;d->vkEnumeratePhysicalDevices(d->instance,&n,nullptr);devices.resize(n);d->vkEnumeratePhysicalDevices(d->instance,&n,devices.data());devices.resize(n);}
    if(devices.empty()){reason="no Vulkan device";return nullptr;}
    // Erstes Geraet mit Pixel-Interlock, Fragment-Speicherzugriffen und Grafik-/Rechenwarteschlange;
    // diskrete vor integrierten Geraeten. Ohne Interlock: kein Backend (siehe Kopfkommentar).
    std::string rejected;int best_rank=-1;
    for(const auto p:devices){
        VkPhysicalDeviceProperties props;d->vkGetPhysicalDeviceProperties(p,&props);
        std::vector<VkExtensionProperties> ext;{std::uint32_t n=0;d->vkEnumerateDeviceExtensionProperties(p,nullptr,&n,nullptr);ext.resize(n);d->vkEnumerateDeviceExtensionProperties(p,nullptr,&n,ext.data());ext.resize(n);}
        auto interlock=info<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT);
        auto features=info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);features.pNext=&interlock;
        const bool has_interlock=has_extension(ext,"VK_EXT_fragment_shader_interlock");
        if(has_interlock)d->vkGetPhysicalDeviceFeatures2(p,&features);
        std::uint32_t family=~0u;{std::uint32_t n=0;d->vkGetPhysicalDeviceQueueFamilyProperties(p,&n,nullptr);std::vector<VkQueueFamilyProperties> q(n);d->vkGetPhysicalDeviceQueueFamilyProperties(p,&n,q.data());
            for(std::uint32_t i=0;i<n;++i)if((q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)&&(q[i].queueFlags&VK_QUEUE_COMPUTE_BIT)){family=i;break;}}
        std::string missing;
        if(!has_interlock||!interlock.fragmentShaderPixelInterlock)missing+=" fragmentShaderPixelInterlock";
        if(has_interlock&&!features.features.fragmentStoresAndAtomics)missing+=" fragmentStoresAndAtomics";
        // SignedZeroInfNanPreserve (gpu_raster_vulkan.glsl) braucht die Erweiterung und die Eigenschaft fuer 32 Bit.
        if(!has_extension(ext,"VK_KHR_shader_float_controls"))missing+=" VK_KHR_shader_float_controls";
        else{auto controls=info<VkPhysicalDeviceFloatControlsPropertiesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR);
            auto properties=info<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);properties.pNext=&controls;
            d->vkGetPhysicalDeviceProperties2(p,&properties);
            if(!controls.shaderSignedZeroInfNanPreserveFloat32)missing+=" shaderSignedZeroInfNanPreserveFloat32";}
        if(family==~0u)missing+=" graphics/compute queue";
        // Flaechen (Storage), Punkttexturen (texelFetch) und gefilterte Ketten (Linearfilter, auch 16 Bit).
        const auto format=[&](VkFormat f,VkFormatFeatureFlags need,const char* label){
            VkFormatProperties fp;d->vkGetPhysicalDeviceFormatProperties(p,f,&fp);
            if((fp.optimalTilingFeatures&need)!=need){missing+=" ";missing+=label;}};
        format(VK_FORMAT_R32_UINT,VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT,"R32_UINT");
        format(VK_FORMAT_R32_SFLOAT,VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT,"R32_SFLOAT");
        format(VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT,"B8G8R8A8_UNORM");
        format(VK_FORMAT_R16G16B16A16_UNORM,VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT,"R16G16B16A16_UNORM");
        if(!missing.empty()){if(!rejected.empty())rejected+="; ";rejected+=std::string(props.deviceName)+" lacks"+missing;continue;}
        const int rank=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?2:props.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?1:0;
        if(rank<=best_rank)continue;
        best_rank=rank;d->physical=p;d->family=family;d->name=props.deviceName;
        d->anisotropy=features.features.samplerAnisotropy==VK_TRUE;d->max_anisotropy=props.limits.maxSamplerAnisotropy;
        d->max_layers=std::min<std::uint32_t>(2048,props.limits.maxImageArrayLayers);}
    if(!d->physical){reason="no Vulkan device with ordered fragment interlock ("+rejected+")";return nullptr;}
    std::vector<VkExtensionProperties> ext;{std::uint32_t n=0;d->vkEnumerateDeviceExtensionProperties(d->physical,nullptr,&n,nullptr);ext.resize(n);d->vkEnumerateDeviceExtensionProperties(d->physical,nullptr,&n,ext.data());ext.resize(n);}
    std::vector<const char*> device_ext={"VK_EXT_fragment_shader_interlock","VK_KHR_shader_float_controls"};
    // Vorgabe der Portabilitaetsspezifikation: vorhandenes VK_KHR_portability_subset muss aktiviert werden.
    if(has_extension(ext,"VK_KHR_portability_subset"))device_ext.push_back("VK_KHR_portability_subset");
    auto interlock=info<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT);interlock.fragmentShaderPixelInterlock=VK_TRUE;
    auto features=info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);features.pNext=&interlock;
    features.features.fragmentStoresAndAtomics=VK_TRUE;features.features.samplerAnisotropy=d->anisotropy?VK_TRUE:VK_FALSE;
    const float priority=1;auto qci=info<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);qci.queueFamilyIndex=d->family;qci.queueCount=1;qci.pQueuePriorities=&priority;
    auto dci=info<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);dci.pNext=&features;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
    dci.enabledExtensionCount=std::uint32_t(device_ext.size());dci.ppEnabledExtensionNames=device_ext.data();
    if(const VkResult r=d->vkCreateDevice(d->physical,&dci,nullptr,&d->device);r!=VK_SUCCESS){reason="vkCreateDevice failed ("+std::to_string(int(r))+")";d->device=VK_NULL_HANDLE;return nullptr;}
#define RECVX_VK_LOAD(name) d->name=reinterpret_cast<PFN_##name>(d->vkGetDeviceProcAddr(d->device,#name)); \
    if(!d->name){reason="Vulkan function " #name " missing";return nullptr;}
    RECVX_VK_DEVICE(RECVX_VK_LOAD)
#undef RECVX_VK_LOAD
    d->vkGetPhysicalDeviceMemoryProperties(d->physical,&d->memory);
    d->vkGetDeviceQueue(d->device,d->family,0,&d->queue);
    return d;
}
}
std::shared_ptr<GpuRaster> create_vulkan_raster(std::string& reason){
    // Ein Geraet je Prozess, von allen Instanzen geteilt (Hauptbild, Quellpass, Auswahlwechsel); jede Instanz
    // hat eigene Flaechen, Kommandopuffer und einen eigenen cache_owner().
    static std::mutex mutex;static std::weak_ptr<Device> shared;static std::string failure;static bool failed=false;
    std::lock_guard<std::mutex> lock(mutex);
    if(failed){reason=failure;return nullptr;}
    auto device=shared.lock();
    if(!device){device=create_device(reason);if(!device){failed=true;failure=reason;return nullptr;}shared=device;}
    try{return std::make_shared<VulkanRaster>(device);}
    catch(const std::exception& e){reason=e.what();return nullptr;}
}
}
