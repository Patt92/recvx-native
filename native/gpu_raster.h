#pragma once
#include "raster_setup.h"
#include <memory>
#include <string>
#include <vector>
// Host-GPU-Rasterer (Plan work/performance_optimization_plan_20261020.md, Phase D). Plattformanpassung,
// keine Spiellogik: der SoftwareRenderer entscheidet Transformation, Clipping, Streifenregeln,
// Sortierung und Phasen; das Backend rastert dieselbe raster::Setup-Liste mit den Pixelregeln der
// Bandstufe und liefert das aufgeloeste Bild und die Tiefe (1/z, Maximum je Ueberabtastblock) zurueck.
// Je Betriebssystem ein Backend: Metal (macOS), sonst noch keines (Softwarepfad).
namespace recvx {
class GpuRaster {
public:
    virtual ~GpuRaster()=default;
    // false: das Bild enthaelt etwas, das dieses Backend nicht exakt abbildet (unsupported());
    // der Aufrufer zeichnet dann in Software. pixels/depth: width*height des Grundbilds.
    virtual bool render(const raster::Frame& frame,const raster::ShadowGeometry& shadows,std::vector<std::uint32_t>& pixels,std::vector<float>& depth)=0;
    virtual std::string name()const=0;
    // Gruende fuer Softwarebilder (z. B. GS-ALPHA-Register ohne Blendgleichung), je einmal.
    virtual const std::vector<std::string>& unsupported()const=0;
    // Zaehler des letzten Bildes (Diagnose): Zeichenaufrufe und Schattenkomposite.
    virtual unsigned draw_calls()const=0;
    virtual unsigned shadow_composites()const=0;
    // Summen seit Start (Sekunden): Auftragsaufbau, Texturuploads, Kodieren, Warten auf die GPU,
    // Ruecklesen. Diagnose der Plattformanpassung.
    struct Timings {double build=0,upload=0,encode=0,wait=0,readback=0,gpu=0;unsigned submits=0,uploads=0;};
    virtual const Timings& timings()const=0;
};
// Backend der Plattform oder nullptr (kein Geraet/kein Backend); reason erklaert nullptr.
std::shared_ptr<GpuRaster> create_gpu_raster(std::string& reason);
}
