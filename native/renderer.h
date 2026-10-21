#pragma once
#include "scene.h"
#include "gpu_raster.h"
#include <algorithm>
#include <filesystem>
#include <functional>
namespace recvx {
// vclipw.xyz eines Sichtraumpunkts gegen ClipMatrix2 (_Make_ClipMatrix, ps2_NaView.c:233):
// m[0][0] = projection/volume_x, m[1][1] = projection/volume_y, z-Zeilen aus njClipZ near/far
// (positive Abstaende). Bits +x 1, -x 2, +y 4, -y 8, +z (fern) 16, -z (nah) 32.
unsigned strip_clip_code(const NJS_POINT3& view_point,float projection,float volume_x,float volume_y,float near,float far);
class SoftwareRenderer {
public:
    SoftwareRenderer(int width,int height);
    void draw(const Scene& scene,const SceneCamera& camera,bool wireframe=false);
    // screen.c:961-974 njSetScreen(dist, w, h, cx = w/2, cy = h/2) mit unveraendertem njSetAspect
    // (BHD_ASPECT 1.174/1): fSx = 1.174*x*dist/z, fSy = y*dist/z in einem Bild dieser Groesse.
    // Die Ninja-Streifen-Clipregeln (ClipVolume des 640er Bildes) gelten hier nicht.
    void draw_screen(const Scene& scene,const SceneCamera& camera,float dist,float aspect_y=1.0f);
    // screen.c:816: njSetPerspective bei w=640, dann dist *= 0.8, 512x480,
    // mit dem von bhEff110 gesetzten njSetAspect und originaler Streifen-Clipregel.
    void draw_full_screen_source(const Scene& scene,const SceneCamera& camera,float aspect_y,bool wireframe=false);
    // Already prepared camera-space matrix, including the original item-view
    // path which bypasses njSetView's world Y/Z conversion.
    // near/far: njClipZ distances in view space (far 0 = unlimited).
    // volume_x/y: ClipVolume der Streifenregel, nur im Ninja-Bildschirm von draw() wirksam.
    void draw_view(const Scene& scene,const NJS_MATRIX& view,float focal_x,float focal_y,bool wireframe=false,const std::array<std::uint32_t,16>& hidden={},float near=0.05f,float far=0,float volume_x=320.0f,float volume_y=240.0f);
    const std::vector<std::uint32_t>& pixels() const {return pixels_;}
    // 1/z of the nearest surface per pixel; 0 where nothing was drawn. Lets a
    // caller composite an off-screen pass without a colour key.
    const std::vector<float>& depth() const {return depth_;}
    // Passive evidence of the last geometry pass, not another camera owner.
    // Passive evidence of the last geometry pass, not another camera owner.
    const std::array<float,2>& last_clip()const{return last_clip_;}
    // Diagnose der Bildaufbereitung (Summen seit Start), Sekunden: Setup/Clipping, Schattenmasken,
    // GPU-Raster, Bandstufe, Linien.
    struct Timings {double setup=0,shadow=0,gpu=0,bands=0,lines=0;unsigned frames=0;};
    const Timings& timings()const{return timings_;}void reset_timings(){timings_=Timings{};}
    std::vector<std::uint32_t>& canvas(){return pixels_;}
    // Trilinear mipmapping with up to 4x anisotropic taps (removes texture
    // shimmer). Off reproduces the previous nearest-neighbour sampling.
    void set_texture_filtering(bool enabled){filter_=enabled;}
    bool texture_filtering()const{return filter_;}
    // Worker threads for band rasterization (0 = up to four workers). Output is identical for any value.
    void set_render_threads(unsigned count){threads_=count;}
    unsigned render_threads()const{return threads_;}
    // NxN supersampling (1..4, default 2): the scene is rendered at N times the size and
    // box-filtered down, which steadies moving polygon edges. Overlay drawing is unaffected.
    void set_supersampling(int factor){ss_=factor;}
    int supersampling()const{return ss_;}
    // Diagnose (GPU-Machbarkeit): wird je Bild mit der sortierten Setup-Liste aufgerufen, bevor die
    // Bandstufe zeichnet. Die Liste lebt nur waehrend des Aufrufs; das Softwarebild bleibt unveraendert.
    void set_raster_observer(std::function<void(const raster::Frame&)> observer){observer_=std::move(observer);}
    // Host-GPU-Rasterer (gpu_raster.h) fuer die Bandstufe; nullptr = Software. Pixel, Tiefe, Linien und
    // alle Overlays bleiben in pixels()/depth(); Drahtgitter zeichnet immer in Software.
    void set_gpu(std::shared_ptr<GpuRaster> gpu){gpu_=std::move(gpu);last_gpu_frame_=false;gpu_error_.clear();}
    // Headless-Takt ohne Bild: Szene und Zeichenseiteneffekte (bhDrawEffect-Funktionen,
    // pl_sleep_cnt) laufen wie in jedem Spielbild, nur die Rasterung entfaellt.
    // Beim Wechsel auf ein gerastertes Bild startet die Leinwand wie ein frischer Renderer, damit
    // 2D-Schichten der verworfenen Takte (Fade, Menue) nicht aufeinander liegen.
    void set_discard(bool discard){if(discard_&&!discard){std::fill(pixels_.begin(),pixels_.end(),0u);std::fill(depth_.begin(),depth_.end(),0.0f);}discard_=discard;}
    bool discarding()const{return discard_;}
    const std::shared_ptr<GpuRaster>& gpu()const{return gpu_;}
    bool last_gpu_frame()const{return last_gpu_frame_;}
    const std::string& gpu_error()const{return gpu_error_;}
    int width() const{return width_;}
    int height() const{return height_;}
    std::size_t covered_pixels() const;
    void save_bmp(const std::filesystem::path& path) const;
private:
    int width_,height_;
    bool filter_=true;
    bool discard_=false;
    unsigned threads_=0;
    int ss_=2;
    std::function<void(const raster::Frame&)> observer_;
    std::shared_ptr<GpuRaster> gpu_;
    bool last_gpu_frame_=false;std::string gpu_error_;
    Timings timings_;
    std::array<float,2> last_clip_{{0.05f,0.f}};
    // Bildschirmmassstab der Ninja-Projektion fuer die Clipregel der Chunk-Streifen:
    // fSx = AspectW*x*dist/z, fSy = AspectH*y*dist/z in 640x480-Einheiten (ps2_NinjaCnk.c:1195).
    // Nur draw() setzt ihn (Spiel-/Tuerkamera); 0 = Regel aus (Sonderansichten).
    float ninja_sx_=0,ninja_sy_=0;
    std::vector<std::uint32_t> hi_pixels_;
    std::vector<float> hi_depth_;
    std::vector<std::uint32_t> pixels_;
    std::vector<float> depth_;
    void draw_view_projected(const Scene& scene,const NJS_MATRIX& view,float focal_x,float focal_y,bool wireframe,const std::array<std::uint32_t,16>& hidden,float near,float far,float volume_x,float volume_y,float projection);
};
}
