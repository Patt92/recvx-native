#pragma once
#include "ninja_matrix.h"
#include "fog.h"
#include <array>
#include <cstdint>
#include <vector>
#include <optional>
#include <memory>
#include <map>
#include <functional>
namespace recvx {
// light: per-vertex lighting factor (r,g,b), used when the triangle is gouraud-lit.
// alpha: Faktor auf das Dreiecksalpha, nur mit SceneTriangle::vertex_alpha (GS-Gouraud
// interpoliert RGBA je Eckpunkt, njDrawPolygon3D mit eigenen Eckfarben).
struct SceneVertex { NJS_POINT3 position; float u=0,v=0; NJS_VECTOR normal{}; NJS_POINT3 light{1,1,1}; float alpha=1; };
// Texture samples are immutable during scene composition. Copies share storage;
// resource edits detach so mods/atlas edits cannot alter another decoded asset.
class TexturePixels {
    std::shared_ptr<std::vector<std::uint32_t>> storage_=std::make_shared<std::vector<std::uint32_t>>();
    std::uint64_t revision_=0;
    const std::vector<std::uint32_t>& read()const{static const std::vector<std::uint32_t> empty;return storage_?*storage_:empty;}
    std::vector<std::uint32_t>& write(){++revision_;if(!storage_)storage_=std::make_shared<std::vector<std::uint32_t>>();else if(storage_.use_count()!=1)storage_=std::make_shared<std::vector<std::uint32_t>>(*storage_);return *storage_;}
public:
    TexturePixels()=default;
    TexturePixels(std::vector<std::uint32_t> data):storage_(std::make_shared<std::vector<std::uint32_t>>(std::move(data))){}
    TexturePixels(std::initializer_list<std::uint32_t> data):TexturePixels(std::vector<std::uint32_t>(data)){}
    std::size_t size()const{return read().size();}bool empty()const{return read().empty();}
    std::uint64_t revision()const{return revision_;}
    const std::uint32_t* data()const{return read().data();}std::uint32_t* data(){return write().data();}
    const std::uint32_t& operator[](std::size_t i)const{return read()[i];}std::uint32_t& operator[](std::size_t i){return write()[i];}
    auto begin()const{return read().begin();}auto end()const{return read().end();}
    auto begin(){return write().begin();}auto end(){return write().end();}
    void resize(std::size_t n){write().resize(n);}void reserve(std::size_t n){write().reserve(n);}
    void clear(){write().clear();}void push_back(std::uint32_t n){write().push_back(n);}
    void assign(std::size_t n,std::uint32_t value){write().assign(n,value);}
    friend bool operator==(const TexturePixels& a,const TexturePixels& b){return a.read()==b.read();}
    friend bool operator==(const TexturePixels& a,const std::vector<std::uint32_t>& b){return a.read()==b;}
    friend bool operator==(const std::vector<std::uint32_t>& a,const TexturePixels& b){return a==b.read();}
};
struct TextureFilterCache {
    bool ready=false,cutout=false,uniform_alpha=false;
    std::uint64_t fingerprint=0;std::uint32_t max_alpha=0;
    std::vector<std::vector<std::uint32_t>> mips;
    std::vector<std::vector<std::uint32_t>> punch_mips; // RGB ohne Alpha-Gewichtung (ohne ABE)
    // Host-GPU-Kopien dieses Texturstands je Darstellungsart (gpu_raster.h), geprueft mit demselben
    // Fingerabdruck wie die Mips. Lebensdauer = dieser Cache: ein neuer Texturstand gibt sie frei.
    std::array<std::shared_ptr<void>,5> gpu{};std::array<std::uint64_t,5> gpu_fingerprint{};
};
struct SceneTexture {
    unsigned width=0,height=0;
    TexturePixels pixels;
    std::map<unsigned,TexturePixels> palette_banks; // njSetPaletteBankNum, PLI TEXflag palette-only records
    mutable std::shared_ptr<TextureFilterCache> filter_cache=std::make_shared<TextureFilterCache>();
    // GS CLAMP WMS/WMT (Ps2TexLoad, ps2_dummy.c:1650: SET_CLAMP_1(ClampFlag, REPEAT, ...)).
    // Ps2CheckTextureAlpha (:1836-1844) setzt ClampFlag 0xFF000FF5 (beide Achsen CLAMP), nur
    // Gindex 20500/15205..15209 behalten 0xFF000FF0 (REPEAT); Rendertexturen 0 (ps2_loadtim2.c:124).
    bool clamp_u=false,clamp_v=false;
    void invalidate_mips()const{filter_cache=std::make_shared<TextureFilterCache>();}
    std::uint64_t fingerprint()const{
        std::uint64_t h=1469598103934665603ull^(std::uint64_t(width)<<32|height);h*=1099511628211ull;
        h^=reinterpret_cast<std::uintptr_t>(pixels.data());h*=1099511628211ull;h^=pixels.size();h*=1099511628211ull;
        h^=pixels.revision();h*=1099511628211ull;
        return h;
    }
};
// Ninja: NJD_FST_UA strip, original GS ALPHA register from chunk/material bits.
// Alpha/Additive are explicit host sprite operations; all draw after opaque geometry.
enum class SceneBlend {Opaque,Additive,Alpha,Ninja};
enum class SceneTextureFilter {Default,Point,Linear};
struct SceneTriangle {
    std::array<SceneVertex,3> vertices;
    std::uint32_t color=0xffb2b2b2;
    std::uint16_t texture=0;
    std::uint16_t object=0;
    bool textured=false;
    bool camera_mask=true;
    bool world_lit=false;
    bool after_modifiers=false; // enemy/player draw phase: game.c:318 draws these after bhDrawEffect modifiers
    bool gouraud=false; // color is the unlit base; vertices carry light factors (Ninja per-vertex lighting)
    bool vertex_alpha=false; // vertices carry their own alpha factor (Gouraud RGBA)
    SceneBlend blend=SceneBlend::Opaque;bool depth_write=true;
    // GS ohne ABE mit TEST ATST GREATER 0 (Punch-Through, ps2_dummy.c:510-516/543): Texel mit Alpha 0
    // fallen weg, alle anderen schreiben Farbe und Tiefe ganz - keine Teilabdeckung wie bei Ausschnittstexturen.
    bool punch_through=false;
    bool fogged=true; // njFogDisable um einen Effekt-Zeichenaufruf (effsub2.c bhEff_Draw3DSprite)
    // Ps2SetFogColorSys um genau einen Zeichenaufruf (ps2_NaDraw.c:469 njDrawTexture3DHEx
    // nimmt dafuer die Offsetfarbe des ersten Eckpunkts) und danach Ps2SetFogColor zurueck.
    bool fog_color_set=false;std::uint32_t fog_color=0; // 0x00RRGGBB wie njSetFogColor
    // Fester GS-Nebelkoeffizient F (0..255) dieses Aufrufs statt des Ninja-Entfernungswerts scr.fog:
    // njDrawTextureSub3DH (ps2_NaDraw.c:443) schreibt XYZF2.F = 128 in jeden Eckpunkt, waehrend
    // njDrawTextureSub3D/njDrawPolygonSub3D (:236/:336) scr.fog eintragen. FGE steht im Prim-Wort beider
    // Texturpfade (0x1E/0x3E, Ps2AddPrim3D ps2_dummy.c:546), der GS mischt also unabhaengig vom
    // Raumnebel Cv = (Cs*F + FOGCOL*(255-F)) >> 8 mit FOGCOL aus Ps2SetFogColorSys (:1681).
    bool fog_fixed=false;float fog_fixed_value=255.f;
    unsigned light_category=1;
    std::uint64_t blend_register=0x44; // original GS source-alpha/inverse-source-alpha
    SceneTextureFilter texture_filter=SceneTextureFilter::Default;
    unsigned texture_color_divisor=255; // raw Ninja constant-material RGB uses 128, native normalized RGB uses 255
    // color stammt aus einem Diffusmaterial-Chunk (NJD_CM_D/DA/DS/DAS, Typ 17/19/21/23);
    // nur dort schreibt npSetAllMatAlphaColor (njplus.c:1495) das Alpha.
    bool diffuse_material=false;
    NJS_POINT3 ambient_material{1,1,1};
    bool chunk_ambient=false; // strip colour functions containing ambient (ps2_NinjaCnk.c:141)
    unsigned material_phase=0; // game.c:318/objitm.c:503: room, en_obj, object lists, items, effects, enemies, player
    // Ninja-Indizes des letzten Material-/NJD_CB_BA-Chunks vor dem Streifen; bhDraw136
    // hinterlaesst dieses Paar fuer die naechste ef_fnc-Funktion (-1 = unbekannt).
    std::int8_t chunk_mode_src=-1,chunk_mode_dst=-1;
    // Chunk-Streifen (njCnkCs/njCnkCsUvh/njCnkCsUvn, ps2_NinjaCnk.c:1720/1860/2292) folgen der
    // Clipregel der vu1DrawTriangleStrip*-Funktionen (ps2_Vu1Strip.c:846/967/1081/1234), nicht
    // einer exakten Z-Klippung. strip_segment != 0 ist das <=60-Eckpunkt-Paket, das njCnkCsUvh/
    // njCnkCsUvn als Ganzes verwerfen, wenn alle Eckpunkte ausserhalb derselben Ebene liegen
    // (ps2_NinjaCnk.c:2017-2104/2240-2272); njCnkCs (ohne UV) hat diese Paketpruefung nicht.
    bool strip_clip=false;std::uint32_t strip_segment=0;
    // Effektaufrufe der eff[]-Zeichenpaesse (effect.c bhDrawNtxEffect3D/bhDrawTrsEffect3D/
    // bhDrawOpqEffect3D) laufen nicht durch njClipZ: njDrawTextureSub3D (ps2_NaDraw.c:302)
    // verwirft den ganzen Aufruf, wenn ein Eckpunkt scr.z < 0 hat oder alle Eckpunkte jenseits
    // derselben Kante des 640x480-Bildes liegen (_Clip_Screen, ps2_Vu1Strip.c:1202);
    // njDrawPolygonSub3D (ps2_NaDraw.c:222, ef_ntx) prueft beides nicht (ninja_screen_reject false).
    // Danach setzt Ps2AddPrim3D (ps2_dummy.c:486) jedem Eckpunkt mit |x-2048| > 2047, |y-2048| > 2047 oder |1.062501907*z-2048.0625| > 2047
    // (vclipw gegen clip_vec/zclip_ab_vec) das Clipbit; ein Streifendreieck mit einem solchen
    // Eckpunkt bekommt ADC und wird nicht gezeichnet. Eine Z-Klippung gibt es dort nicht.
    // ninja_primitive != 0 nummeriert den Aufruf; seine Dreiecke folgen aufeinander.
    std::uint32_t ninja_primitive=0;bool ninja_screen_reject=false;
    // Nur Metadaten, KEIN Easy-Default des allgemeinen Dekoders. Die belegte
    // bhPutModel-Route entscheidet aus draw_tp und UA/DB, ob der OT-Cache CLAMP
    // statt des Texturheaders setzt (ps2_dummy.c:1420). Andere Besitzer bleiben offen.
    std::int16_t cnk_strip_head=-1;bool texture_cache_clamp=false;
    // objitm.c:471-492 / game.c:371-376: ob_spc[0] of the five original water rooms.
    // ulDrawGeneralPurposeWater bypasses the translucent OT (ps2_NinjaCnk.c:1927/2359):
    // direct draw after the player, before bhEtcMirrorDrawModel and the final OT, no OT CLAMP;
    // TEST GREATER 0 / KEEP with depth writes (ps2_dummy.c:2021-2025), not OT's alpha >=128.
    bool general_purpose_water=false;
    // Spiegelpass (sys->st_flg 0x100, Ebene sys->mr_pl): 1 = zweiter bhPutModel-Aufruf unter
    // njMirror(cam.mtx) (MdlPut.c:57-79/89-111), 2 = bhEtcMirrorDrawModel (game.c:77-91). Die Lage
    // bleibt die ungespiegelte Weltlage, der Renderer zeichnet mit cam.mtx * Scene::mirror_matrix.
    // 3 = bhDrawEffect im Spiegelpass (game.c:427): neu erzeugte Effektausgabe dieses Aufrufs.
    std::uint8_t mirror=0;
    // bhSetHideObject(offset) (cut.c:612): Wortversatz in hidobj; 4 im Spiegelpass (game.c:409).
    std::uint8_t hidden_offset=0;
    // bhPutModel(plp) (game.c:364/367): Dreieck des Spielerkoerpers; der Spiegeltest liest plp->mdflg 0x40.
    bool player_body=false;
};
// Expliziter bhPutModel-Zeichenpfad: draw_tp 0..3 (MdlPut.c:8-12/33-37),
// UA/DB -> pCnkCsVu1FuncTbl (ps2_NinjaCnk.c:106/1921/2353), nur TransDouble(I)
// -> Ps2AddPrim3DEx -> OT-Cache. Andere Zeichenbesitzer rufen dies nicht implizit auf.
void route_model_texture_cache(std::vector<SceneTriangle>& triangles,unsigned draw_tp);
// colors as NJS_COLOR 0xAARRGGBB = canvas order (SDL_PIXELFORMAT_ARGB8888); blend_register = GS ALPHA of the draw (0x44 = njDrawLine3D default 8/6).
// inherit_blend: die Zeichenfunktion setzt kein Blendpaar (effsub3.c bhEff308Drw); der Raumbesitzer
// traegt das Register des ef_fnc-Vorgaengers ein.
// mirror != 0: njDrawLine3D im Spiegelpass (game.c:427) unter cam.mtx * Scene::mirror_matrix.
struct SceneLine {std::array<NJS_POINT3,2> points;std::array<std::uint32_t,2> colors;std::uint64_t blend_register=0x44;bool inherit_blend=false;std::uint8_t mirror=0;};
struct SceneCamera {
    NJS_POINT3 position{};
    std::array<std::int32_t,3> angles{};
    float distance=0;
    std::int32_t perspective=8192;
    float clip_near=0.05f,clip_far=0; // njClipZ (bhChangeViewClip*), set by the runtime; far 0 = unlimited
    // ClipVolume.x/y (_Make_ClipVolume, ps2_NaView.c:298) der Streifen-Clipmatrix ClipMatrix2;
    // bhChangeClipVolume/bhChangeClipVolumeRM, vom Runtime gesetzt. Grundwert ps2_NaView.c:24.
    float clip_volume_x=320.0f,clip_volume_y=240.0f;
    std::array<std::uint32_t,16> hidden_objects{};
    std::uint16_t flags=0;
    std::uint8_t key_frame=0; // CAM_WRK Byte 2: light.c:376/800 liest den Schnitt als CAM_KEYF_WORK, clp = 2 * (unsigned char)ci->frame
    std::uint32_t fog_color=0;float fog_near=0,fog_far=0; // RDT CAM_WRK b8/bc/c0, signed distances
    std::int8_t filter_no=0,filter_rate=0; // CAM_WRK fil_no/fil_rt (char, 0x51/0x52), gelesen unter flg 0x8000
    // Original CAM_WRK spd, am_spd and aa_spd; control runs at 30 Hz.
    int transition_speed=0,angle_speed=0;float move_speed=0;
    NJS_POINT3 cut_origin{},cut_extent{};
    float follow_x=0,follow_z=0,target_height=0;
    std::array<float,4> y_offsets{};
    std::array<std::int32_t,2> angle_limits{};
    std::array<float,4> roll_offsets{},perspectives{};
};
// bhSetCut / bhControlActiveCamera state, independent of render calls.
class ActiveCameraMotion {
public:
    void reset(const SceneCamera& view,std::size_t cut);
    void select(const SceneCamera& target,std::size_t cut,bool force=false);
    void tick(const SceneCamera& target);
    const SceneCamera& view()const{return view_;}
    bool ready()const{return cut_.has_value();}
    bool transitioning()const{return blending_;}
    unsigned clock()const{return clock_;}
private:
    SceneCamera view_{};std::optional<std::size_t> cut_;
    bool blending_=false;unsigned clock_=0;
};
// BH_PWORK px/py/pz and ax/ay/az (types.h:94-96) for the one player work.
// pitch/roll are the work's ax/az: bhSetObject zeroes them and no original
// routine ever writes plp->ax or plp->az - map.c:1074, player.c:1375/1377,
// bhControlPlayerHead (player.c:6760) and bhCalcHair (player.c:7219) only read
// them.  They are carried here so every consumer reads the same work instead
// of assuming a value locally.
struct SceneSpawn {NJS_POINT3 position{};Angle yaw=0;Angle pitch=0,roll=0;};
struct SceneCutArea {
    std::size_t camera=0;
    std::int8_t floor=0;
    unsigned type=0;
    float minx=0,minz=0,maxx=0,maxz=0;
    std::uint32_t attributes=0;
};
// F05: original modifier models remain separate from material triangles.
struct ShadowGeometry;
struct SceneShadow {
    std::shared_ptr<const ShadowGeometry> geometry;
    std::array<float,16> transform{};
    // game.c:427 Spiegel-bhDrawEffect: bhDrawMdfEffect (effect.c:866) unter cam.mtx*njMirror, nach dem
    // Spiegelraum (:417) und vor den uebrigen Spiegeleffekten; braucht Scene::mirror_matrix.
    bool mirror=false;
};
struct Scene {
    std::vector<SceneShadow> shadows;
    FogState fog; // installed EOR state, copied from the existing World owner
    std::uint32_t fog_color=0; // njSetFogColor (sync.c:163/177/200): 0x00RRGGBB of cam.fog_col or sys->fog_col
    struct AmbientWrite {
        std::size_t triangle=0;NJS_POINT3 color{};unsigned phase=0,object=0;bool camera_mask=false;
        std::uint8_t hidden_offset=0; // SceneTriangle::hidden_offset der Raumspiegelung
    };
    std::vector<AmbientWrite> ambient_writes; // ordered material writes, also for empty polygon lists
    NJS_POINT3 resolve_ambient_material(NJS_POINT3 initial={1,1,1},const SceneCamera* camera=nullptr);
    void erase_triangles(const std::function<bool(const SceneTriangle&)>& predicate);
    std::vector<SceneTriangle> triangles;
    std::vector<SceneLine> lines;
    std::vector<SceneCamera> cameras;
    std::vector<SceneTexture> textures;
    std::vector<SceneSpawn> spawns;
    std::vector<SceneCutArea> cuts;
    std::vector<bool> cut_enabled,cut_tables; // per CUT_WORK: flg bit 0, decoded areas
    std::size_t object_count=0;
    std::uint32_t background=0xff101010;
    // njMirror(I, &sys->mr_pl) dieses Bildes (ps2_NaMatrix.c:1016); gilt fuer Dreiecke mit mirror != 0.
    bool mirror_enabled=false;std::array<float,16> mirror_matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
};
Scene decode_scene(const std::vector<std::uint8_t>& room);
std::vector<SceneTexture> decode_texture_bundle(const std::vector<std::uint8_t>& data,std::size_t offset=0);
// One NJS_CNK_MODEL (vlist + plist) from raw memory with absolute addresses,
// transformed by `world` (njCnk*DrawModel through the host renderer).
Scene decode_memory_model(const std::uint8_t* memory,std::size_t size,std::uint32_t model,const NJS_MATRIX& world);
std::optional<std::size_t> camera_for_position(const Scene& scene,NJS_POINT3 position,std::int8_t floor=0);
// bhInitActiveCamera target evaluation (no transition/event-camera state).
SceneCamera camera_for_actor(const Scene& scene,std::size_t camera,NJS_POINT3 position,std::int8_t floor=0,const NJS_POINT3* eye=nullptr);
// camera.c:47-57 cam.wpx/wpy/wpz: pivot + RotY(ay)RotX(ax)RotZ(az)(0,0,-ln).
// This is the sound system's CameraPos, not the eye of the view matrix.
NJS_POINT3 camera_world_position(const SceneCamera& camera);
// camera.c:28-36 bhControlCamera: cmat (njSetView, diag(1,-1,-1)), Dolly
// -cam.ln, njRotateZ/X/Y und die Kameraverschiebung. Danach ist z vor der
// Kamera positiv und y zeigt nach unten.
void camera_view_matrix(const SceneCamera& camera,NJS_MATRIX& out);
struct ModelPose {
    NJS_POINT3 position{};
    std::array<std::int32_t,3> angles{};
};
// njplus.c npCalcMorphing: obj_b's realized model string plus shp_ct. The
// renderer blends the selected model's own object streams toward it for this
// draw, the span npPushMdlstr2/npCalcMorphing/npPopMdlstr2 brackets bhPutModel.
struct MorphSource {
    const std::vector<std::uint8_t>* model=nullptr;
    // Live objP[node].evalflags of obj_b. obj_a's come from ModelGeometryOptions
    // evalflags, or from its own asset when the caller has no live copy.
    const std::vector<std::uint32_t>* flags=nullptr;
    float shape=0;
};
struct ModelGeometryOptions {
    NJS_MATRIX transform{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::vector<ModelPose> pose;
    std::vector<std::uint32_t> evalflags;
    MorphSource morph{};
    // Direct model call (bhDraw136): decode this physical object only, ignoring its hide bit.
    int draw_object=-1;
    bool skin_fm=false; // npCalcSkinFM skips logical nodes 11..14 and remaps influences >10 by +4
    // Authoritative native O_WORK matrices: rendering must not resample a
    // motion or rebuild a second pose after original procedural joint edits.
    std::vector<std::array<float,16>> world_matrices;
    std::vector<std::vector<NJS_POINT3>> world_vertices;
    std::vector<std::vector<NJS_VECTOR>> world_normals;
    // objitm.c:1294 bhObj005: objP[n].model->vlist zeigt auf eine jedes Bild neu beschriebene
    // Kopie des Eckpunkt-Chunks. Je Objekt die lokalen Lagen/Normalen dieser Kopie; sie
    // ersetzen den Chunk-Inhalt vor der Knotenmatrix (leer = Chunk unveraendert).
    std::vector<std::vector<NJS_POINT3>> local_vertices;
    std::vector<std::vector<NJS_VECTOR>> local_normals;
    // njplus.c:1653 npSetOffsetUV2: der aufsummierte u/v-Versatz (1/1024, short-Arithmetik)
    // des Streifenchunks 66, der uv_chunk Bytes hinter dem plist-Anfang von objP[uv_object]
    // liegt. uv_object -1 = kein Versatz.
    int uv_object=-1;std::size_t uv_chunk=0;int uv_u=0,uv_v=0;
};
// objP[object].model->vlist, erster Eckpunkt-Chunk (bhObj005 liest ps->x/ps->z und y und
// beschreibt die Kopie): Chunk-Typ, lokale Lagen und (Typ 41/51) Normalen in Chunkreihenfolge.
// nullopt, wenn das Objekt kein Modell oder keinen vlist hat.
struct ModelObjectVertices {unsigned chunk_type=0;std::vector<NJS_POINT3> positions;std::vector<NJS_VECTOR> normals;};
std::optional<ModelObjectVertices> model_object_vertices(const std::vector<std::uint8_t>& data,std::size_t model,unsigned object);
// njplus.c:1662-1670: der Lauf von npSetOffsetUV2 ueber objP[object].model->plist (je Kopf ein
// short, je Nicht-66-Chunk ein weiteres short, Ende bei 255). found: der Lauf landet auf einem
// Kopf 66; chunk: dessen Byteabstand zum plist-Anfang; usr_num: die zwei Nutzerbits des
// Streifenkopfes; proper: dieser Kopf ist zugleich der erste 66-Chunk der regulaeren
// Chunk-Kette (sonst liest das Original einen Fremdkopf).
struct ModelObjectUvWalk {bool found=false,proper=false;unsigned usr_num=0;std::size_t chunk=0;};
// njplus.c:1564 npSetOffsetUV: addiert short-UVs in ALLEN Streifen 65/66 des
// objP[object].model->plist. Kein Matrix-/Samplerzustand; der vorhandene Modellstrom
// bleibt Quelle der Zeichenpfade. false = kein Modell/plist.
bool offset_model_object_uv(std::vector<std::uint8_t>& data,std::size_t model,unsigned object,int offu,int offv);
std::optional<ModelObjectUvWalk> model_object_uv_walk(const std::vector<std::uint8_t>& data,std::size_t model,unsigned object);
// Standalone MDL/PVP path shared by stage and animated character loading.
Scene decode_model_asset(const std::vector<std::uint8_t>& data,std::size_t model_offset,
                         std::size_t texture_offset,const ModelGeometryOptions& options={});
// mlwP->objP[object].model->vlist[1] >> 16 (effsub1b.c:646 bhEff135): die Eckpunktzahl
// des ersten Eckpunkt-Chunks. nullopt, wenn das Objekt kein Modell hat (model NULL).
std::optional<unsigned> model_object_vertex_count(const std::vector<std::uint8_t>& data,std::size_t model,unsigned object);
}
