#include "gpu_raster.h"
namespace recvx {
// Plattformen ohne GPU-Backend zeichnen in Software (macOS: gpu_raster_metal.mm,
// Windows: gpu_raster_d3d11.cpp).
std::shared_ptr<GpuRaster> create_gpu_raster(std::string& reason){reason="no GPU raster backend on this platform";return nullptr;}
}
