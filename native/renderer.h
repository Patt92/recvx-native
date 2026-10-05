#pragma once
#include "scene.h"
#include <filesystem>
namespace recvx {
class SoftwareRenderer {
public:
    SoftwareRenderer(int width,int height);
    void draw(const Scene& scene,const SceneCamera& camera,bool wireframe=false);
    // Already prepared camera-space matrix, including the original item-view
    // path which bypasses njSetView's world Y/Z conversion.
    void draw_view(const Scene& scene,const NJS_MATRIX& view,float focal_x,float focal_y,bool wireframe=false,const std::array<std::uint32_t,16>& hidden={});
    const std::vector<std::uint32_t>& pixels() const {return pixels_;}
    std::vector<std::uint32_t>& canvas(){return pixels_;}
    int width() const{return width_;}
    int height() const{return height_;}
    std::size_t covered_pixels() const;
    void save_bmp(const std::filesystem::path& path) const;
private:
    int width_,height_;
    std::vector<std::uint32_t> pixels_;
    std::vector<float> depth_;
};
}
