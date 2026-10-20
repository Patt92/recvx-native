#pragma once
#include "scene.h"
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
// Rasterisierungsauftraege des SoftwareRenderers nach Transformation, Clipping, Streifenregeln
// und Sortierung: ein Dreieck in Bildkoordinaten (W x H des Ueberabtastbilds) mit allen Werten,
// die die Pixelschleife liest. Reine Daten; Rasterbeobachter und GPU-Rasterer
// (gpu_raster.h) erhalten dieselbe Liste, die die Bandstufe zeichnet, in derselben Reihenfolge.
namespace recvx {
namespace raster {
struct Projected {float x,y,invz,u_over_z,v_over_z;float tz=0;};
struct Setup {
    std::array<Projected,3> p;float area;int minx,maxx,miny,maxy;std::uint32_t color;const SceneTexture* texture;
    bool clamp_u=false,clamp_v=false;
    bool gouraud=false;std::array<NJS_POINT3,3> light{};
    bool vertex_alpha=false;std::array<float,3> alpha{1,1,1};
    float Ux,Uy,Vx,Vy,Wx,Wy;SceneBlend blend=SceneBlend::Opaque;bool depth_write=true,punch_through=false;
    bool general_water=false,mirror_room=false,mirror_effect=false;
    bool fog_color_set=false;std::uint32_t fog_color=0;
    std::uint64_t blend_register=0x44;
    unsigned texture_color_divisor=255;
    bool filtered=false,linear_only=false,post=false;
    // GS FOG: every CNK strip is drawn with FGE (ps2_NinjaCnk.c:1880); F per vertex
    // from njCalcFogPower(fVSZ) (:1196), interpolated like the vertex colour.
    bool fogged=false;std::array<float,3> fog{};
};
// bhDrawMdfEffect eines Schattenwerks: Maske im 640x480-Bild (shadow_raster.h) und das
// Rechteck/die Nebelstaerke des Komposits (renderer.cpp, shadow_mask_composite).
struct ShadowComposite {std::vector<unsigned char> mask;float rx0,ry0,rx1,ry1;unsigned fog_power;};
// Masken eines Modifierpasses aus der transportierten Empfaengertiefe (W x H, Ueberabtastbild).
using ShadowPass=std::function<std::vector<ShadowComposite>(bool mirror,const float* transported)>;
// Dasselbe Schattenwerk fuer einen Rasterer, der die Maske selbst zeichnet: transportierte
// Dreiecke (shadow_transport.h) in Reihenfolge der beiden Durchgaenge von raster_shadow_mask,
// bereits mit Flaeche, Eckreihenfolge und Pixelgrenzen wie dort. subtract = Durchgang 1.
struct ShadowFace {
    std::array<float,3> x,y;std::array<std::uint32_t,3> depth;
    float area;int minx,miny,maxx,maxy;bool subtract;
};
struct ShadowWork {std::vector<ShadowFace> faces;float rx0,ry0,rx1,ry1;unsigned fog_power;};
using ShadowGeometry=std::function<std::vector<ShadowWork>(bool mirror)>;
// Ein Bild der Bandstufe: Setups [0,opaque_end) sind undurchsichtig/direktes Wasser in Einreichreihenfolge,
// [opaque_end,size) die tiefensortierte OT. Mit Schatten (shadows) laufen zwischen den Phasen
// [0,before_modifiers) | Modifier | [before_modifiers,before_mirror_effects) | Spiegel-Modifier | Rest
// die Modifierpaesse (game.c:318/417/427).
struct Frame {
    int width,height,supersampling;
    const std::vector<Setup>* setups;std::size_t opaque_end;
    std::uint32_t background,fog_pixel;bool wireframe,shadows;
    std::size_t before_modifiers=0,before_mirror_effects=0;
    std::array<bool,2> shadow_pass{}; // Schattenwerke ohne/mit Spiegelpass (shadow.mirror)
};
}
}
