#pragma once
#include <algorithm>
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
// Host-Rasterer: gpu = Plattformstandard (Metal / D3D11 / OpenGL), opengl = ausdruecklich GL 4.1,
// vulkan = ausdruecklich Vulkan mit geordnetem Fragment-Interlock. Software bleibt Referenz und
// Fehlerpfad; Qualitaet und Backendwahl sind unabhaengig. Der Wert ist zugleich der Debugmenue-Index.
enum class RenderBackend {Gpu,Software,OpenGL,Vulkan};
// Host-Praesentationsstufe (metalfx_present.h, macOS): das fertige 640x480-Bild wird nach dem
// Zeichnen hochskaliert, zwischen zwei Takten interpoliert und nachbearbeitet. Keine Spiellogik;
// das 640x480-Bild, Takt und Ereignisse bleiben unveraendert. Werte sind zugleich Debugmenue-Indizes.
//   Upscaler: off = Punktvergroesserung wie bisher; MetalFX spatial; MetalFX temporal (mit
//   Tiefe und Bewegungsvektoren aus der Kamera-Reprojektion, ohne Subpixel-Jitter).
//   FrameGeneration: MetalFX-Frame-Interpolation (macOS 26+) erzeugt je Spieltakt ein Zwischenbild.
//   Sharpness: Grad des kontrastadaptiven Schaerfens (aus, leicht, mittel, stark), unabhaengig vom Modus.
//   PostProcess: Off; CRT light (Zeilenraster der 480 Spielzeilen + einfache Maske, sRGB); CRT full (gamma-
//   korrektes Zeilenraster mit Helligkeitsausgleich, Schlitzmaske je Ausgabepixel, Bloom/Halation).
enum class Upscaler {Off,MetalFxSpatial,MetalFxTemporal};
// Host-Bildrate der Praesentation (alle Backends): der Spieltakt bleibt 30 Hz (timing.h), zwischen zwei
// Takten wird das zuletzt gezeichnete Bild erneut praesentiert (mit Frame-Interpolation das Zwischenbild
// in der ersten Takthaelfte). 30 = ein Bild je Takt; 60/120 = 2/4 Praesentationen je Takt mit VSync;
// Unlimited = VSync aus, fortlaufend praesentieren (hoehere CPU-/GPU-Last).
enum class FrameRate {Hz30,Hz60,Hz120,Unlimited};
enum class FrameGeneration {Off,MetalFx};
enum class Sharpness {Off,Light,Medium,Strong};
enum class PostProcess {Off,CrtLight,CrtFull};
struct VideoSettings {
    RenderQuality quality=RenderQuality::High;
    RenderBackend renderer=RenderBackend::Gpu;
    FrameRate frame_rate=FrameRate::Hz30;
    Upscaler upscaler=Upscaler::Off;
    FrameGeneration frame_generation=FrameGeneration::Off;
    Sharpness sharpness=Sharpness::Off;
    PostProcess post_process=PostProcess::Off;
    bool filtering()const{return quality!=RenderQuality::Original;}
    int supersampling()const{return quality==RenderQuality::Ultra?3:quality==RenderQuality::High?2:1;}
    // true: the presentation stage has work to do (otherwise the plain SDL copy presents).
    bool presentation()const{return upscaler!=Upscaler::Off||frame_generation!=FrameGeneration::Off||post_process!=PostProcess::Off||sharpness!=Sharpness::Off;}
    float sharpness_amount()const{return sharpness==Sharpness::Strong?1.0f:sharpness==Sharpness::Medium?0.6f:sharpness==Sharpness::Light?0.3f:0.0f;}
    std::string describe_sharpness()const;
    // Praesentationen je Spieltakt (0 = unbegrenzt); Frame-Interpolation braucht mindestens zwei.
    int presents_per_tick()const{const int n=frame_rate==FrameRate::Hz30?1:frame_rate==FrameRate::Hz60?2:frame_rate==FrameRate::Hz120?4:0;return n&&frame_generation!=FrameGeneration::Off?std::max(n,2):n;}
    bool vsync()const{return frame_rate!=FrameRate::Unlimited;}
    std::string describe_frame_rate()const;
    std::string describe()const;
    std::string describe_upscaler()const;
    std::string describe_frame_generation()const;
    std::string describe_post_process()const;
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
    void set_renderer(RenderBackend renderer,bool persist=true);
    void cycle_frame_rate(int direction,bool persist=true);
    void set_frame_rate(FrameRate value,bool persist=true);
    void cycle_upscaler(int direction,bool persist=true);
    void cycle_frame_generation(int direction,bool persist=true);
    void cycle_sharpness(int direction,bool persist=true);
    void cycle_post_process(int direction,bool persist=true);
    void set_upscaler(Upscaler value,bool persist=true);
    void set_frame_generation(FrameGeneration value,bool persist=true);
    void set_sharpness(Sharpness value,bool persist=true);
    void set_post_process(PostProcess value,bool persist=true);
    std::string describe()const{return settings_.describe();}
    // Row values with the runtime's availability note ("not available" when the stage is missing).
    std::string describe_upscaler()const;
    std::string describe_frame_generation()const;
    std::string describe_sharpness()const;
    std::string describe_post_process()const;
    // Runtime report: presentation stage (post-processing), MetalFX upscalers and frame generation usable?
    void set_availability(bool presentation,bool upscaler,bool frame_generation){presentation_available_=presentation;upscaler_available_=upscaler;frame_generation_available_=frame_generation;}
    bool presentation_available()const{return presentation_available_;}
    bool upscaler_available()const{return upscaler_available_;}
    bool frame_generation_available()const{return frame_generation_available_;}
    const std::string& status()const{return status_;}
private:
    void persist(bool persist);
    std::filesystem::path path_;
    VideoSettings settings_;
    std::string status_;
    bool presentation_available_=false,upscaler_available_=false,frame_generation_available_=false;
};
}
