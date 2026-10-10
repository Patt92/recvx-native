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
    // near/far: njClipZ distances in view space (far 0 = unlimited).
    void draw_view(const Scene& scene,const NJS_MATRIX& view,float focal_x,float focal_y,bool wireframe=false,const std::array<std::uint32_t,16>& hidden={},float near=0.05f,float far=0);
    const std::vector<std::uint32_t>& pixels() const {return pixels_;}
    // 1/z of the nearest surface per pixel; 0 where nothing was drawn. Lets a
    // caller composite an off-screen pass without a colour key.
    const std::vector<float>& depth() const {return depth_;}
    // Passive evidence of the last geometry pass, not another camera owner.
    const std::array<float,2>& last_clip()const{return last_clip_;}
    std::vector<std::uint32_t>& canvas(){return pixels_;}
    // Trilinear mipmapping with up to 4x anisotropic taps (removes texture
    // shimmer). Off reproduces the previous nearest-neighbour sampling.
    void set_texture_filtering(bool enabled){filter_=enabled;}
    bool texture_filtering()const{return filter_;}
    // Worker threads for band rasterization (0 = up to four workers). Output is identical for any value.
    void set_render_threads(unsigned count){threads_=count;}
    // NxN supersampling (1..4, default 2): the scene is rendered at N times the size and
    // box-filtered down, which steadies moving polygon edges. Overlay drawing is unaffected.
    void set_supersampling(int factor){ss_=factor;}
    int supersampling()const{return ss_;}
    int width() const{return width_;}
    int height() const{return height_;}
    std::size_t covered_pixels() const;
    void save_bmp(const std::filesystem::path& path) const;
private:
    int width_,height_;
    bool filter_=true;
    unsigned threads_=0;
    int ss_=2;
    std::array<float,2> last_clip_{{0.05f,0.f}};
    std::vector<std::uint32_t> hi_pixels_;
    std::vector<float> hi_depth_;
    std::vector<std::uint32_t> pixels_;
    std::vector<float> depth_;
};
}
