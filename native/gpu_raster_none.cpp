#include "gpu_raster.h"
namespace recvx {
// Plattformen ohne GPU-Backend zeichnen in Software. Windows bekommt ein eigenes Direct3D-11-Backend
// hinter derselben Schnittstelle (Plan Phase D2).
std::shared_ptr<GpuRaster> create_gpu_raster(std::string& reason){reason="no GPU raster backend on this platform";return nullptr;}
}
