#pragma once
#include "video_settings.h"
#include <cstdint>
#include <memory>
#include <string>
// Host-Praesentationsstufe (macOS): nimmt das fertige 640x480-Bild der Runtime (nach allen 2D-Zeichnern),
// die 1/z-Tiefe und die Projektion des letzten Geometriepasses und zeigt es ueber die CAMetalLayer des
// SDL-Fensters an. Stufen, jede einzeln in video.ini waehlbar (VideoSettings):
//   1. MetalFX-Upscaler: spatial (MTLFXSpatialScaler) oder temporal (MTLFXTemporalScaler mit Tiefe und
//      Bewegungsvektoren). Bewegungsvektoren entstehen aus der Kamera-Reprojektion der Tiefe mit der
//      Projektion des vorigen Bildes; 2D-Pixel (Bild nach dem Geometriepass != Endbild) gelten als
//      unbewegter Vordergrund. Kein Subpixel-Jitter: die 2D-Zeichner schreiben an ganzzahligen
//      640x480-Positionen, ein Jitter des 3D-Passes liesse die 2D-Ebene gegenlaeufig zittern.
//   2. Frame-Interpolation (MTLFXFrameInterpolator, macOS 26+): je Spieltakt ein Zwischenbild aus dem
//      vorigen und dem aktuellen Bild. Die Kadenz bestimmt die Runtime (video.ini frame_rate): sie zeigt
//      das Zwischenbild in der ersten Takthaelfte (present) und das gezeichnete Bild danach (represent);
//      keine Metal-Praesentationszeiten (afterMinimumDuration liess die Anzeige nachlaufen).
//   3. Post-Processing (eigener Metal-Kernel): kontrastadaptives Schaerfen oder CRT-Zeilenraster der
//      480 Spielzeilen, zugleich die 4:3-Einpassung in das Fenster wie SDL_RenderSetLogicalSize.
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
};
struct PresentStats {unsigned presents=0,repeated=0,interpolated=0,resets=0,skipped=0;double gpu_seconds=0;};
class MetalFxPresenter {
public:
    virtual ~MetalFxPresenter()=default;
    virtual void configure(const VideoSettings& settings)=0;
    // Neues Bild verarbeiten und anzeigen: show_interpolated zeigt das Zwischenbild (sofern eines
    // entstand), sonst das gezeichnete Bild. true: angezeigt (oder bewusst uebersprungen, z. B.
    // minimiertes Fenster); false: Stufe nicht nutzbar, der Aufrufer praesentiert ueber SDL.
    virtual bool present(const PresentFrame& frame,bool show_interpolated)=0;
    // Zuletzt verarbeitetes Bild erneut anzeigen (Kadenz der Runtime): interpolated waehlt das Zwischenbild
    // des letzten present(), sonst das gezeichnete Bild. false: nichts vorhanden/Stufe nicht nutzbar.
    virtual bool represent(bool interpolated)=0;
    virtual std::string describe()const=0;     // aktive Kette, z. B. "MetalFX temporal 2x + spatial 2x, frame interpolation, sharpen"
    virtual bool upscaler_supported()const=0;  // MetalFX-Skalierer auf diesem Geraet
    virtual bool frame_generation_supported()const=0;
    virtual const PresentStats& stats()const=0;
};
// layer: CAMetalLayer* (SDL_RenderGetMetalLayer). nullptr mit Begruendung, wenn kein Metal-Layer/Geraet.
std::unique_ptr<MetalFxPresenter> create_metalfx_presenter(void* layer,std::string& reason);
#if !defined(RECVX_HAVE_METALFX)
inline std::unique_ptr<MetalFxPresenter> create_metalfx_presenter(void*,std::string& reason){reason="MetalFX presentation is built for macOS only";return nullptr;}
#endif
}
