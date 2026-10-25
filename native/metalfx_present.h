#pragma once
#include "video_settings.h"
#include <cstdint>
#include <memory>
#include <string>
// Host-Praesentationsstufe: nimmt das fertige 640x480-Bild der Runtime (nach allen 2D-Zeichnern), die
// 1/z-Tiefe und die Projektion des letzten Geometriepasses und zeigt es im SDL-Fenster an. Zwei
// Implementierungen derselben Schnittstelle, je Plattform eine:
//   macOS: MetalFX ueber die CAMetalLayer des SDL-Fensters (metalfx_present.mm). Stufen, jede einzeln in
//   video.ini waehlbar (VideoSettings):
//   1. MetalFX-Upscaler: spatial (MTLFXSpatialScaler) oder temporal (MTLFXTemporalScaler mit Tiefe und
//      Bewegungsvektoren). Bewegungsvektoren entstehen aus der Kamera-Reprojektion der Tiefe mit der
//      Projektion des vorigen Bildes; 2D-Pixel (Bild nach dem Geometriepass != Endbild) gelten als
//      unbewegter Vordergrund. Kein Subpixel-Jitter: die 2D-Zeichner schreiben an ganzzahligen
//      640x480-Positionen, ein Jitter des 3D-Passes liesse die 2D-Ebene gegenlaeufig zittern.
//   2. Frame-Interpolation (MTLFXFrameInterpolator, macOS 26+): je Spieltakt ein Zwischenbild aus dem
//      vorigen und dem aktuellen Bild. Die Kadenz (video.ini frame_rate, PresentFrame::slots) laeuft auf dem
//      Praesentations-Thread der Stufe: Zwischenbild in der ersten Takthaelfte, gezeichnetes Bild danach,
//      jede Praesentation sofort (VSync ueber die Layer). Der Hauptthread uebergibt nur das Bild und blockiert
//      nie im VSync (vier Praesentationen je Takt auf dem Hauptthread liessen Takte ausfallen).
//   3. Post-Processing (eigener Metal-Kernel): kontrastadaptives Schaerfen oder CRT-Zeilenraster der
//      480 Spielzeilen, zugleich die 4:3-Einpassung in das Fenster wie SDL_RenderSetLogicalSize.
//   Windows: AMD FSR 4 ueber ein eigenes Direct3D-12-Geraet und eine DXGI-Swapchain auf dem HWND des
//   SDL-Fensters (fsr_present_d3d12.cpp), gleiche Bewegungsvektoren und Post-Processing-Kette.
// Plattformanpassung, keine Spiellogik: das 640x480-Bild, Takt, Eingaben und Ereignisse bleiben
// unveraendert; Screenshots, Headless-Laeufe und der SDL-Kopierpfad lesen weiter dasselbe Bild.
namespace recvx {
struct PresentProjection {
    bool valid=false;     // false: kein Geometriepass in diesem Bild (Titel, Menue): alles ist 2D
    float view[16]{};     // NJS_MATRIX des Passes (Welt -> Sicht, Spaltenreihenfolge wie njCalcPoint)
    float focal_x=0,focal_y=0; // Brennweiten in Pixeln des 640x480-Bildes (x = W/2 + x*focal_x/z)
    float near=0,far=0;   // njClipZ-Abstaende, far 0 = unbegrenzt
};
struct PresentFrame {
    const std::uint32_t* pixels=nullptr; // width*height 0xAARRGGBB, Endbild
    const std::uint32_t* scene=nullptr;  // Bild nach dem Geometriepass (2D-Maske); nullptr: ganz 2D
    const float* depth=nullptr;          // width*height 1/z, 0 = nichts gezeichnet
    int width=640,height=480;
    PresentProjection projection;
    unsigned scene_key=0;    // Raum/Schnitt/Bildschirmbesitzer; Aenderung = Schnitt, Historie verwerfen
    double delta_seconds=0;  // Spielzeit zwischen diesem und dem vorigen praesentierten Bild
    int slots=1;             // Praesentationen bis zum naechsten Bild (Kadenz), 0 = fortlaufend ohne VSync
};
struct PresentStats {unsigned presents=0,repeated=0,interpolated=0,resets=0,skipped=0;double gpu_seconds=0;};
class MetalFxPresenter {
public:
    virtual ~MetalFxPresenter()=default;
    virtual void configure(const VideoSettings& settings)=0;
    // Neues Bild uebergeben (Kopie, kehrt sofort zurueck); der Praesentations-Thread verarbeitet es und zeigt
    // es in frame.slots Praesentationen bis zum naechsten Bild. true: uebernommen; false: Stufe nicht nutzbar
    // (Fehler gemeldet), der Aufrufer praesentiert ueber SDL.
    virtual bool present(const PresentFrame& frame)=0;
    virtual std::string describe()const=0;     // aktive Kette, z. B. "MetalFX temporal 2x + spatial 2x, frame interpolation, sharpen"
    virtual bool upscaler_supported()const=0;  // Skalierer dieser Stufe auf diesem Geraet (MetalFX bzw. FSR 4)
    virtual bool frame_generation_supported()const=0;
    virtual const PresentStats& stats()const=0;
};
// layer: CAMetalLayer* (SDL_RenderGetMetalLayer). nullptr mit Begruendung, wenn kein Metal-Layer/Geraet.
std::unique_ptr<MetalFxPresenter> create_metalfx_presenter(void* layer,std::string& reason);
#if !defined(RECVX_HAVE_METALFX)
inline std::unique_ptr<MetalFxPresenter> create_metalfx_presenter(void*,std::string& reason){reason="MetalFX presentation is built for macOS only";return nullptr;}
#endif
// Windows: FSR-4-Stufe (fsr_present_d3d12.cpp) auf dem HWND des SDL-Fensters; sdk_dir ist der Ordner der
// AMD-ffx-api-DLLs (amd_fidelityfx_loader_dx12.dll bzw. amd_fidelityfx_dx12.dll, amd_fidelityfx_upscaler_dx12.dll,
// Unterordner "FSR4 INT8" fuer das INT8-Modell). nullptr mit Begruendung ohne D3D12-Geraet oder DLLs.
std::unique_ptr<MetalFxPresenter> create_fsr_presenter(void* hwnd,const std::string& sdk_dir,const VideoSettings& initial,std::string& reason);
#if !defined(RECVX_HAVE_FSR)
inline std::unique_ptr<MetalFxPresenter> create_fsr_presenter(void*,const std::string&,const VideoSettings&,std::string& reason){reason="FSR presentation is built for Windows only";return nullptr;}
#endif
}
