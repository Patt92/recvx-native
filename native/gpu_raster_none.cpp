#include "gpu_raster.h"
namespace recvx {
// Plattformen ohne eigenes GPU-Backend (macOS: gpu_raster_metal.mm, Windows: gpu_raster_d3d11.cpp):
// Linux nimmt OpenGL, sofern gebaut; sonst zeichnet die Bandstufe in Software.
#if !defined(__APPLE__)&&!defined(_WIN32)
std::shared_ptr<GpuRaster> create_gpu_raster(std::string& reason){
#ifdef RECVX_HAVE_OPENGL
    return create_opengl_raster(reason);
#else
    reason="no GPU raster backend on this platform";return nullptr;
#endif
}
#endif
#ifndef RECVX_HAVE_OPENGL
std::shared_ptr<GpuRaster> create_opengl_raster(std::string& reason,bool){reason="built without OpenGL (RECVX_ENABLE_OPENGL=OFF)";return nullptr;}
#endif
#ifndef RECVX_HAVE_VULKAN
std::shared_ptr<GpuRaster> create_vulkan_raster(std::string& reason){reason="built without Vulkan (RECVX_ENABLE_VULKAN=OFF)";return nullptr;}
#endif
#ifndef RECVX_HAVE_D3D12
std::shared_ptr<GpuRaster> create_d3d12_raster(std::string& reason){
#ifdef _WIN32
    reason="built without Direct3D 12 (RECVX_ENABLE_D3D12=OFF)";
#else
    reason="Direct3D 12 is available on Windows only";
#endif
    return nullptr;}
#endif
}
