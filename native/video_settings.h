#pragma once
#include <filesystem>
#include <string>
namespace recvx {
// Host-only render quality (not original behaviour). The game itself always
// renders 640x480 at the original 30-Hz tick; quality only changes how the
// software renderer samples textures and resolves edges.
//   Original: nearest texels, no supersampling (PS2-like, shimmers)
//   Niedrig:  trilinear/anisotropic filtering, no supersampling
//   Hoch:     filtering + 2x2 supersampling (default)
//   Ultra:    filtering + 3x3 supersampling
enum class RenderQuality {Original,Low,High,Ultra};
// renderer: Host-Rasterer der Bandstufe. gpu = Plattform-Backend (gpu_raster.h, Metal auf macOS),
// sonst Software; dasselbe Bild bis auf Grenzpixel, die CPU zeichnet nicht mehr jedes Pixel.
enum class RenderBackend {Gpu,Software};
struct VideoSettings {
    RenderQuality quality=RenderQuality::High;
    RenderBackend renderer=RenderBackend::Gpu;
    bool filtering()const{return quality!=RenderQuality::Original;}
    int supersampling()const{return quality==RenderQuality::Ultra?3:quality==RenderQuality::High?2:1;}
    std::string describe()const;
    static VideoSettings read(const std::filesystem::path& path); // missing file -> defaults
    void save(const std::filesystem::path& path)const;
};
// Persisted in the platform user-data directory as video.ini; --video-config overrides it.
class VideoConfig {
public:
    VideoConfig()=default;
    explicit VideoConfig(std::filesystem::path path);
    const VideoSettings& settings()const{return settings_;}
    void cycle(int direction,bool persist=true);
    void set(RenderQuality quality,bool persist=true);
    std::string describe()const{return settings_.describe();}
    const std::string& status()const{return status_;}
private:
    std::filesystem::path path_;
    VideoSettings settings_;
    std::string status_;
};
}
