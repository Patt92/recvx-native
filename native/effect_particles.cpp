#include "effect_particles.h"
#include "chunk_blend.h"
#include "core_random.h"
#include "effect_sprites.h"
#include "enemies/enemy_work.h"
#include "ninja_matrix.h"
#include <cstring>
#include <stdexcept>

namespace recvx {
// Native Neubestimmung der Partikelarbeiten aus effsub3.c. Kein Originalkoerper,
// kein Generator, kein Originalkern.
//
// Strukturabweichung: das Original haelt Effektsprites, Partikel und Blutlachen
// in EINEM Pool eff[512] (effect.c, AllocOwork in effsub3.c). Nativ besitzt
// effect_sprites.cpp die Sprites, diese Klasse die Partikelarbeiten. Die
// Aufteilung auf zwei Besitzer ist gegenueber dem einen Originalpool eine
// bewusste Abweichung; innerhalb dieser Klasse bleiben Vergabereihenfolge
// (AllocOwork ab owk_scn_noG, AllocOworkOne immer ab Index 0), Belegung mit
// drei Plaetzen je Partikelsatz und Kapazitaet 512 unveraendert.
//
// Rendererabbildungen. Das PRIM-Feld des GIF-Tags beginnt bei Bit 47 (PRE ist
// Bit 46): Ps2AddPrim3D ORt das Primwort in SCE_GIF_SET_TAG(0,1,1,0,0,3), die
// ELF-Konstante dafuer ist 0x3000400000008000 (Ps2AddPrim3D 0x002CBE1C..0x002CBE2C:
// NREG 3, PRE Bit 46, EOP). Belege fuer die Lage: ps2_dummy.c:510/514 loescht ABE mit
// ~SCE_GIF_SET_TAG(0,0,0,SCE_GS_SET_PRIM(0,0,0,0,1,..),0,0) und prueft dafuer
// Bit 53 (0x20000000000000), ps2_NaGraphics3D.c:532 setzt 0x16800000000000
// (Bit 47 gesetzt, PRIM 0x2D = TRIFAN|IIP|FGE), njDrawQuadTexture
// (ps2_NaDraw.c:563/567) gibt zwei Eckpunkte mit 0x2B../0x0B.. aus (PRIM 0x56/
// 0x16 = SPRITE|TME[|ABE]). Das Primwort ist also (Konstante >> 47):
// 1. njDrawTexture3DEx(oP->tvp, 16, 1) zeichnet ueber njDrawTextureSub3D
//    Ps2AddPrim3D(0x3E..)/(0x1E..) (ps2_NaDraw.c:351/355) = PRIM 0x7C/0x3C =
//    TRISTRIP|IIP|TME|FGE[|ABE]: ein Dreiecksstreifen ueber die 16 Eckpunkte in
//    tv_buf-Reihenfolge. bhEff300 legt sie im Zickzack um den Kreis an
//    (effsub3.c:264-284, Winkel eff_dir+16384+4096*{0,1,-1,2,-2,..,7,-7,8};
//    VtxTbl effsub3.c:236 ist die Kreisfolge dazu), der Streifen fuellt also
//    das Sechzehneck mit 14 Dreiecken.
// 2. bhEff_PtclSpriteDrawB -> njPtclDrawSprite (ps2_NinjaPtcl.c:100-160)
//    zeichnet ueber Ps2AddPrim3D(0x3B..) = PRIM 0x76 = SPRITE|TME|FGE|ABE.
//    Je Punkt stehen zwei Eckpunkte im Puffer (scr -+ iz*w*dist, ST 0 bzw. 1,
//    gleiches z/Farbe/Nebel), uebergeben wird aber num = n (ELF
//    njPtclDrawSprite 0x002D8528/0x002D86A4: a1 -> s7 -> a2 unveraendert),
//    und Ps2AddPrim3D gibt genau num Eckpunkte zu je 48 Byte aus (Schleife
//    0x002CBEBC, DMA-Laenge 3*num+3). Aus n Punkten werden damit n/2
//    GS-Sprites aus den Punkten 0..n/2-1; bei vier Punkten je Aufruf
//    (effsub3.c:1420-1423) sind das die Punkte 0,1,4,5,8,9,12,13 der Stufe.
//    Ein GS-Sprite ist ein bildschirmachsenparalleles Rechteck konstanter
//    Tiefe mit linear 0..1 laufender Textur. Nativ ist das genau ein Quad in
//    der Bildebene (Billboardbasis von append_effect_sprite, Tiefe = Punkt-
//    tiefe). Die halbe Breite ist iz*w*dist Ninja-Bildpunkte ohne AspectW,
//    waehrend scr.x selbst mit AspectW projiziert wird (njViewScreenMatrix
//    ps2_NaView.c:221, BHD_ASPECT_X 1.174 main.c:53); in Sichtkoordinaten ist
//    die halbe Kante also w/AspectW waagerecht und h/AspectH (1.0) senkrecht.
namespace {

// effsub3.c:19 — der Vergabezeiger des gemeinsamen Pools.
constexpr unsigned eff_slots = 512;

// effsub3.c:911-1005 — PtclTbl/PtclDat. mov_no zeigt in Eff302Prm, col_lv
// skaliert die Quellfarbe, drw_typ waehlt FuncTbl (1 Sprite, 2 Linie).
struct PD {unsigned mov_no, col_lv, drw_typ;};
struct PT {unsigned set_num; const PD* pd;};

constexpr PD PtclDat00[2] = {{0, 255, 1}, {0, 224, 1}};
constexpr PD PtclDat01[2] = {{1, 255, 1}, {1, 224, 1}};
constexpr PD PtclDat02[2] = {{2, 255, 1}, {3, 224, 1}};
constexpr PD PtclDat03[1] = {{4, 255, 1}};
constexpr PD PtclDat04[2] = {{5, 255, 2}, {11, 128, 1}};
constexpr PD PtclDat05[2] = {{6, 255, 2}, {11, 128, 1}};
constexpr PD PtclDat06[2] = {{7, 255, 2}, {11, 128, 1}};
constexpr PD PtclDat07[2] = {{8, 255, 2}, {11, 128, 1}};
constexpr PD PtclDat08[1] = {{9, 255, 1}};
constexpr PD PtclDat09[2] = {{10, 255, 1}, {10, 224, 1}};
constexpr PD PtclDat10[1] = {{12, 255, 1}};
constexpr PD PtclDat11[1] = {{13, 255, 1}};
constexpr PD PtclDat12[2] = {{14, 255, 1}, {14, 160, 1}};
constexpr PD PtclDat13[2] = {{15, 255, 1}, {15, 160, 1}};
constexpr PD PtclDat14[1] = {{16, 255, 1}};
constexpr PD PtclDat15[2] = {{17, 255, 1}, {17, 160, 1}};
constexpr PD PtclDat16[2] = {{18, 255, 1}, {18, 160, 1}};
constexpr PD PtclDat17[1] = {{0, 255, 1}};
constexpr PD PtclDat18[1] = {{19, 255, 1}};

constexpr PT PtclTbl[19] = {
    {2, PtclDat00}, {2, PtclDat01}, {2, PtclDat02}, {1, PtclDat03},
    {2, PtclDat04}, {2, PtclDat05}, {2, PtclDat06}, {2, PtclDat07},
    {1, PtclDat08}, {2, PtclDat09}, {1, PtclDat10}, {1, PtclDat11},
    {2, PtclDat12}, {2, PtclDat13}, {1, PtclDat14}, {2, PtclDat15},
    {2, PtclDat16}, {1, PtclDat17}, {1, PtclDat18}
};

// effsub3.c:29-51 — Eff302Prm, unveraendert. stg_timer ist im Original ein
// char[6]; -1 deaktiviert eine Stufe (die Einrichtung springt darueber).
struct Prm {
    float vtx_speed, vtx_accel, ang_rand, pos_rand, spd_rand, vtx_scale;
    int stg_erase;
    signed char stg_timer[6];
    float vtx_gravity[3];
};

constexpr Prm Eff302Prm[20] = {
    {1.0f, -0.08f,  20.0f, 0.5f, 0.5f,  0.025f, 16, { 0,  2,  4,  6,  8, 10}, {0.0f, -0.03266667, 0.0f}},
    {0.4f,   0.0f,  45.0f, 1.0f, 1.0f,  0.025f, 20, { 0,  1,  8,  9, 16, 17}, {0.0f, -0.06533334, 0.0f}},
    {1.2f, -0.08f,  10.0f, 0.5f, 0.5f,  0.025f, 16, { 0,  0,  0,  0,  0,  0}, {0.0f, -0.03266667, 0.0f}},
    {1.0f, -0.05f,  60.0f, 0.5f, 0.5f,  0.025f, 24, { 0,  0,  0,  0,  0,  0}, {0.0f, -0.03266667, 0.0f}},
    {0.4f,   0.0f,  45.0f, 1.0f, 1.0f,  0.025f, 20, {-1, -1,  0,  0,  1,  1}, {0.0f, -0.06533334, 0.0f}},
    {0.5f, -0.05f,  55.0f, 0.1f, 0.8f,   0.01f,  5, {-1, -1, -1, -1, -1,  0}, {0.0f, -0.03266667, 0.0f}},
    {0.6f, -0.05f,  50.0f, 0.1f, 0.8f,   0.02f,  7, {-1, -1, -1, -1, -1,  0}, {0.0f, -0.03266667, 0.0f}},
    {0.7f, -0.05f,  45.0f, 0.1f, 0.8f,   0.03f,  9, {-1, -1, -1, -1, -1,  0}, {0.0f, -0.03266667, 0.0f}},
    {1.0f, -0.01f,  20.0f, 0.1f, 0.8f,   0.03f, 12, {-1, -1, -1, -1,  0,  1}, {0.0f, -0.03266667, 0.0f}},
    {0.8f, -0.05f,  20.0f, 0.5f, 0.5f,  0.025f, 12, {-1, -1, -1, -1,  0,  0}, {0.0f, -0.03266667, 0.0f}},
    {0.3f,   0.0f, 360.0f, 1.0f, 1.0f,  0.025f, 16, { 0,  0,  0,  0,  0,  0}, {0.0f, -0.03266667, 0.0f}},
    {0.3f,   0.0f,  45.0f, 0.0f, 1.0f,  0.025f,  7, {-1, -1, -1, -1, -1,  0}, {0.0f, -0.03266667, 0.0f}},
    {0.2f,   0.0f, 360.0f, 1.0f, 1.0f,  0.025f, 12, { 0,  0,  0,  0,  0,  0}, {0.0f, -0.03266667, 0.0f}},
    {0.3f,   0.0f, 180.0f, 0.5f, 0.5f,  0.025f, 16, { 0,  2,  4,  6,  8, 10}, {0.0f, -0.03266667, 0.0f}},
    {1.0f, -0.08f, 360.0f, 1.0f, 1.0f,  0.035f,  9, { 0,  0,  2,  2,  5,  5}, {0.0f, -0.03266667, 0.0f}},
    {0.4f,   0.0f, 150.0f, 1.0f, 1.0f,   0.04f, 16, { 0,  0,  2,  2,  5,  5}, {0.0f, -0.03266667, 0.0f}},
    {0.4f, -0.02f,  60.0f, 1.0f, 1.0f,   0.04f, 10, {-1,  0,  0,  0,  1,  1}, {0.0f, -0.016333334, 0.0f}},
    {0.5f, -0.04f,  45.0f, 1.0f, 1.0f,  0.045f, 10, { 0,  0,  1,  2,  3,  5}, {0.0f, -0.06533334, 0.0f}},
    {0.8f, -0.05f,  60.0f, 0.5f, 1.0f,   0.05f, 12, { 0,  0,  1,  1,  2,  3}, {0.0f, -0.098000005, 0.0f}},
    {0.4f,   0.0f,  60.0f, 0.5f, 0.5f, 0.0055f, 40, {-1, -1,  0,  0,  1,  1}, {0.0f, -0.02f, 0.0f}}
};

// effsub3.c:1008 — Mk2 teilt die Emitterfelder, nur mtn_attr/mtn_no kommen aus
// Quell- und Ziel-Farbe statt aus der col_lv-Verschneidung.
struct Work {
    bool live = false;
    unsigned mode1 = 0;         // oP->mode1 (zonzon.c:627); no reader in bhEff302
    unsigned type = 0;          // oP->type = pdP->mov_no
    unsigned draw_typ = 0;      // pdP->drw_typ, 1 = Sprite, 2 = Linie
    bool linked = false;        // oP->flg 0x80
    unsigned link_kind = 0, link_index = 0, link_node = 0;
    NJS_POINT3 offset{};        // oP->lox/loy/loz (verknuepft)
    NJS_POINT3 position{};      // oP->px/py/pz (auch das Ziel der Verknuepfung)
    NJS_POINT3 direction{};     // oP->xn, normiert
    unsigned attributes = 0, destination = 0; // mtn_attr, mtn_no
    // E02_WORK
    int mode0 = 0;
    int stg_stt = 0, stg_num = 0, vtx_num = 0;
    float pos_rand = 0, spd_rand = 0, vtx_speed = 0, vtx_accel = 0;
    int stg_erase = 0;
    float scale = 0, scl_add_1st = 0, scl_add_2nd = 0;
    unsigned src_col = 0, dst_col = 0;
    int col_add = 0, col_sub = 0;
    NJS_POINT3 vtx_gravity{};   // E02_WORK
    NJS_POINT3 pos_bak{};
    int stg_col[6]{};
    int stg_tim[6]{};
    NJS_POINT3 stg_grv[6]{};
    NJS_POINT3 vtx_dir[16]{};
    float vtx_spd[96]{};
    float stg_scl[6]{};
    NJS_POINT3 vtx_pos[96]{};   // E02_WRK
    int part_slot = -1, vert_slot = -1; // Poolplaetze der beiden Zusatzarbeiten
    bool draw_queued = false;   // sys->ef_fnc-Eintrag dieses Bildes
};

// effsub3.c:145-231 — E17_WORK der Blutlache. lkwkp verweist auf den vorhandenen
// Gegnerwork; die Lache folgt seiner Lage nicht (flg 0x200001 ohne Linkbit 0x80).
struct Pool {
    bool live = false, draw_queued = false;
    const RECVX_ENEMY_WORK* linked_work = nullptr; // lkwkp, nur Sichtbarkeit (effsub3.c:389-394)
    int work_slot = -1;         // E17_WORK reserves a second shared eff slot.
    NJS_POINT3 position{};      // oP->px/py/pz
    int eff_dir = 0, srd_dir = 0;
    float srd_pos = 0, bld_spd = 0;
    float srt_spd[5]{};
    int srt_vtx[5]{};
    struct Vtx {NJS_POINT3 offset{}; float u = 0, v = 0; unsigned col = 0;};
    Vtx buffer[16]{};           // tv_buf: Position und UV in einem Satz
    NJS_POINT3 spd[16]{};       // tv_spd
    unsigned palette = 0;      // ani_ct
    int ct0 = 0, mode0 = 0;
};

// effsub3.c:67-114 — AllocOwork/AllocOworkOne ueber dem gemeinsamen Pool.
int allocate(unsigned char* flags, unsigned& cursor, bool from_start)
{
    unsigned i = from_start ? 0u : cursor;
    for (; i < eff_slots; ++i) {
        if (!(flags[i] & 3u)) {
            flags[i] = 0x2;
            if (!from_start) cursor = i + 1;
            return int(i);
        }
    }
    return -1;
}

// (-rand() / -2.1474836E9f) aus effsub3.c:1242.
float random_unit()
{
    return -float(recvx_core_random_next()) / -2.1474836e9f;
}

void add_triangle(Scene& scene, const SceneVertex* quad, unsigned a, unsigned b, unsigned c,
                  unsigned color, unsigned texture)
{
    SceneTriangle triangle;
    triangle.vertices[0] = quad[a];
    triangle.vertices[1] = quad[b];
    triangle.vertices[2] = quad[c];
    triangle.color = color;
    triangle.texture = std::uint16_t(texture);
    triangle.textured = true;
    triangle.camera_mask = false;
    triangle.world_lit = true;
    triangle.depth_write = false;
    // bhEff_PtclSpriteDrawB setzt njColorBlendingMode(0,8)/(1,6) vor seinen
    // njPtclDrawSprite-Aufrufen; bhSetBloodPoolLnk schreibt dasselbe Paar.
    color_blending_mode(0, 8);
    color_blending_mode(1, 6);
    triangle.blend = SceneBlend::Ninja;
    triangle.blend_register = color_blending_alpha();
    scene.triangles.push_back(triangle);
}

}

struct EffectParticles::Impl {
    std::vector<Work> works;
    std::vector<Pool> pools;
    unsigned char used[eff_slots]{}; // Standalone fixture only; production reads RoomWorld.
    std::function<unsigned(unsigned)> owner_flags;
    std::function<void(unsigned,unsigned,unsigned)> owner_write;
    unsigned scan = 0;          // owk_scn_noG
    // sys->bl_ct liegt nicht hier, sondern bei effect_blood_counter() (effect_sprites.h):
    // ein Besitzer fuer diesen Pfad und bhEff301/bhEff303.
    std::set<std::string> unsupported;
    Impl(): works(eff_slots), pools(eff_slots) {}

    unsigned flags(unsigned slot) const {
        return owner_flags ? owner_flags(slot) : used[slot];
    }
    void write(unsigned slot,unsigned flg,unsigned id) {
        if(owner_write)owner_write(slot,flg,id);
        else used[slot]=static_cast<unsigned char>(flg);
    }
    // AllocOwork (ab owk_scn_noG fortlaufend) bzw. AllocOworkOne (immer ab 0).
    int reserve(bool from_start) {
        if(!owner_flags)return allocate(used,scan,from_start);
        for(unsigned i=from_start?0u:scan;i<eff_slots;++i){
            if(flags(i)&3u)continue;
            works[i]=Work{};
            pools[i]=Pool{};
            write(i,2u,0u);
            if(!from_start)scan=i+1;
            return int(i);
        }
        return -1;
    }
    void free_slot(int slot) {
        const auto index=unsigned(slot);
        write(index,0u,0u);
        works[index]=Work{};
        pools[index]=Pool{};
    }
    Work& root(int slot) {
        auto& w=works[std::size_t(slot)];
        w.live=true;
        write(unsigned(slot),1u,302u);
        return w;
    }

    // effsub3.c:117-142 — bhClrEff_RY gibt alle drei Plaetze eines Partikels frei.
    void release(Work& w)
    {
        if (w.part_slot >= 0) free_slot(w.part_slot);
        if (w.vert_slot >= 0) free_slot(w.vert_slot);
        w = Work{};
    }

    Pool* alloc_pool()
    {
        const int root_slot=reserve(true);
        if(root_slot<0)return nullptr;
        const int work_slot=reserve(true);
        if(work_slot<0){
            free_slot(root_slot);
            return nullptr;
        }
        auto& p=pools[unsigned(root_slot)];
        p=Pool{};
        p.live=true;
        p.work_slot=work_slot;
        write(unsigned(root_slot),0x200001u,300u);
        return &p;
    }

    std::size_t active_count() const
    {
        std::size_t n = 0;
        for (const auto& w : works) if (w.live) ++n;
        for (const auto& p : pools) if (p.live) ++n;
        return n;
    }
};

EffectParticles::EffectParticles():impl_(std::make_unique<Impl>()){}
EffectParticles::~EffectParticles()=default;
EffectParticles::EffectParticles(EffectParticles&&) noexcept=default;
EffectParticles& EffectParticles::operator=(EffectParticles&&) noexcept=default;

// effsub3.c:911 — bhSetEffParticle. Ein Aufruf legt je Tabelleneintrag drei
// Arbeiten an (Wurzel, E02_WORK, E02_WRK) und beginnt die Vergabe wieder bei
// owk_scn_noG = 0. Bei erschoepftem Pool wird der ganze Satz freigegeben.
int EffectParticles::spawn(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned color)
{
    if(type>=19)throw std::runtime_error("particle type outside PtclTbl");
    const PT& table=PtclTbl[type];
    impl_->scan=0;
    int first=-1;
    for(unsigned set_no=0;set_no<table.set_num;++set_no){
        const PD& entry=table.pd[set_no];
        // Ein Satz sind drei Arbeiten aus dem einen Pool: die Wurzel (O_WRK),
        // ihr E02_WORK und ihr E02_WRK. Alle drei belegen einen Platz; die
        // Daten der beiden Zusatzarbeiten liegen nativ beim Wurzeleintrag.
        const int root_slot=impl_->reserve(false);
        const int part_slot=impl_->reserve(false);
        const int vert_slot=impl_->reserve(false);
        if(root_slot<0||part_slot<0||vert_slot<0){
            if(root_slot>=0)impl_->free_slot(root_slot);
            if(part_slot>=0)impl_->free_slot(part_slot);
            if(vert_slot>=0)impl_->free_slot(vert_slot);
            return first;
        }
        Work& w=impl_->root(root_slot);
        if(first<0)first=root_slot;
        w.part_slot=part_slot;
        w.vert_slot=vert_slot;
        w.type=entry.mov_no;
        w.draw_typ=entry.drw_typ;
        if(entry.drw_typ!=1&&entry.drw_typ!=2)throw std::runtime_error("particle draw kind outside FuncTbl");
        // (color & 0xFF00FF) * (col_lv + 1) >> 8 je Kanalgruppe, Alpha fest.
        w.attributes=(((color&0xFF00FFu)*(entry.col_lv+1u))>>8)&0xFF00FFu;
        w.attributes|=((((color&0xFF00u)*(entry.col_lv+1u))>>8)&0xFF00u)|0xFF000000u;
        w.destination=(w.attributes&0xFFFFFFu)|0x20000000u;
        w.linked=link.linked;
        impl_->write(unsigned(root_slot),link.linked?0x81u:1u,302u);
        w.link_kind=link.kind;
        w.link_index=link.index;
        w.link_node=link.node;
        if(link.linked)w.offset=offset;
        else w.position=offset;
        w.direction=direction;
        njUnitVector(reinterpret_cast<NJS_VECTOR*>(&w.direction));
    }
    return first;
}

void EffectParticles::set_mode1(unsigned slot,unsigned value)
{
    if(slot>=eff_slots)return;
    auto& w=impl_->works[slot];
    if(w.live)w.mode1=value;
}

// effsub3.c:117 bhClrEff_RY, case 302 (:132-136): *(int*)oP->exp1 = 0 und *(int*)oP->exp0 = 0
// geben die beiden Hilfsplaetze (E02_WRK/E02_WORK, :951-952) frei, dann oP->flg = 0 fuer die
// Wurzel. Aufrufer ist bhClearEventEffect (effect.c:159) fuer eine Wurzel mit flg & 1 und id 302.
void EffectParticles::clear_event(unsigned slot)
{
    if(slot>=eff_slots)throw std::runtime_error("bhClrEff_RY outside eff[512]");
    auto& w=impl_->works[slot];
    if(w.live)impl_->release(w);
    impl_->free_slot(int(slot));
}

// effsub3.c:1008 — bhSetEffParticleMk2: gleiche Felder, mtn_attr aus src_col und
// mtn_no direkt aus dst_col, ohne die 0x20000000-Verknuepfung.
int EffectParticles::spawn_mk2(unsigned type,const ParticleLink& link,NJS_POINT3 offset,NJS_POINT3 direction,unsigned source_color,unsigned destination_color)
{
    if(type>=19)throw std::runtime_error("particle type outside PtclTbl");
    const PT& table=PtclTbl[type];
    impl_->scan=0;
    int first=-1;
    for(unsigned set_no=0;set_no<table.set_num;++set_no){
        const PD& entry=table.pd[set_no];
        const int root_slot=impl_->reserve(false);
        const int part_slot=impl_->reserve(false);
        const int vert_slot=impl_->reserve(false);
        if(root_slot<0||part_slot<0||vert_slot<0){
            if(root_slot>=0)impl_->free_slot(root_slot);
            if(part_slot>=0)impl_->free_slot(part_slot);
            if(vert_slot>=0)impl_->free_slot(vert_slot);
            return first;
        }
        Work& w=impl_->root(root_slot);
        if(first<0)first=root_slot;
        w.part_slot=part_slot;
        w.vert_slot=vert_slot;
        w.type=entry.mov_no;
        w.draw_typ=entry.drw_typ;
        if(entry.drw_typ!=1&&entry.drw_typ!=2)throw std::runtime_error("particle draw kind outside FuncTbl");
        w.attributes=source_color;
        w.destination=destination_color;
        w.linked=link.linked;
        impl_->write(unsigned(root_slot),link.linked?0x81u:1u,302u);
        w.link_kind=link.kind;
        w.link_index=link.index;
        w.link_node=link.node;
        if(link.linked)w.offset=offset;
        else w.position=offset;
        w.direction=direction;
        njUnitVector(reinterpret_cast<NJS_VECTOR*>(&w.direction));
    }
    return first;
}

// effsub3.c:1107-1381 — bhEff302, zweistufig: case 0 richtet die sechs Stufen
// ein, case 1 traegt sie weiter und integriert die Punkte.
static void step_work(Work& w)
{
    if(w.mode0==0){
        const Prm& prm=Eff302Prm[w.type];
        w.stg_num=0;
        w.vtx_num=0;
        w.stg_stt=0;
        w.src_col=w.attributes;
        w.dst_col=w.destination;
        w.vtx_speed=prm.vtx_speed;
        w.vtx_accel=prm.vtx_accel;
        w.vtx_gravity={prm.vtx_gravity[0],prm.vtx_gravity[1],prm.vtx_gravity[2]};
        w.stg_erase=prm.stg_erase;
        w.pos_rand=prm.pos_rand;
        w.spd_rand=prm.spd_rand;
        w.pos_bak=w.position;
        w.scale=prm.vtx_scale;
        w.scl_add_1st=prm.vtx_scale;
        w.scl_add_2nd=-0.5f*prm.vtx_scale;
        for(unsigned i=0;i<6;++i){
            w.stg_tim[i]=-int(prm.stg_timer[i]);
            if(w.stg_tim[i]>0)w.stg_stt=int(i)+1;
        }
        NJS_MATRIX lcmat;
        for(unsigned i=0;i<16;++i){
            const int ax=int(182.04445f*(prm.ang_rand*(random_unit()-0.5f)));
            const int ay=int(182.04445f*(prm.ang_rand*(random_unit()-0.5f)));
            const int az=int(182.04445f*(prm.ang_rand*(random_unit()-0.5f)));
            w.vtx_dir[i]=w.direction;
            njUnitMatrix(&lcmat);
            njRotateXYZ(&lcmat,ax,ay,az);
            njCalcVector(&lcmat,reinterpret_cast<NJS_VECTOR*>(&w.vtx_dir[i]),reinterpret_cast<NJS_VECTOR*>(&w.vtx_dir[i]));
        }
        const int cnt=w.stg_erase;
        const unsigned src=w.src_col,dst=w.dst_col;
        int tmp=int((dst&0xFFu)-unsigned(src&0xFFu))/cnt;
        if(tmp<0)w.col_sub=(~tmp+1)&0xFF;else w.col_add=tmp&0xFF;
        tmp=int(((dst&0xFF00u)-(src&0xFF00u))>>0)/cnt;
        if(tmp<0)w.col_sub|=(~tmp+256)&0xFF00;else w.col_add|=tmp&0xFF00;
        tmp=int(((dst&0xFF0000u)-(src&0xFF0000u))>>0)/cnt;
        if(tmp<0)w.col_sub|=(~tmp+65536)&0xFF0000;else w.col_add|=tmp&0xFF0000;
        const unsigned sh_src=src>>16,sh_dst=dst>>16;
        tmp=int((sh_dst&0xFF00u)-unsigned(sh_src&0xFF00u))/cnt;
        if(tmp<0)w.col_sub|=((~tmp+256)&0xFF00)<<16;else w.col_add|=(tmp&0xFF00)<<16;
        ++w.mode0;
        return;
    }
    int stg_no=w.stg_stt;
    for(;stg_no<6;++stg_no){
        int& tim=w.stg_tim[stg_no];
        NJS_POINT3& grv=w.stg_grv[stg_no];
        if(tim>=0){
            if(tim==0){
                NJS_POINT3* pos=&w.vtx_pos[std::size_t(stg_no)*16];
                float* spd=&w.vtx_spd[std::size_t(stg_no)*16];
                const float b_spd=w.vtx_speed,p_rnd=w.pos_rand,s_rnd=w.spd_rand;
                for(unsigned i=0;i<16;++i){
                    pos[i]=w.position;
                    pos[i].x+=p_rnd*(random_unit()-0.5f);
                    pos[i].y+=p_rnd*(random_unit()-0.5f);
                    pos[i].z+=p_rnd*(random_unit()-0.5f);
                    spd[i]=b_spd-(s_rnd*(b_spd*(random_unit()-0.5f)));
                }
                ++w.stg_num;
                w.vtx_num+=16;
                w.stg_col[stg_no]=int(w.src_col);
                w.stg_scl[stg_no]=w.scale;
            }else if(tim<w.stg_erase){
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&grv),reinterpret_cast<NJS_VECTOR*>(&w.vtx_gravity));
                w.stg_col[stg_no]+=w.col_add;
                w.stg_col[stg_no]-=w.col_sub;
                if(tim<(w.stg_erase/2))w.stg_scl[stg_no]+=w.scl_add_1st;
                else w.stg_scl[stg_no]+=w.scl_add_2nd;
            }else if(tim==w.stg_erase){
                --w.stg_num;
                w.vtx_num-=16;
                w.stg_stt=stg_no+1;
            }
        }
        ++tim;
    }
    if(w.stg_num>0){
        if(w.stg_stt<0||w.stg_stt>5)throw std::runtime_error("particle stage cursor outside stg_buf");
        const int stg_stt=w.stg_stt;
        NJS_POINT3* pos=&w.vtx_pos[std::size_t(stg_stt)*16];
        float* spd=&w.vtx_spd[std::size_t(stg_stt)*16];
        NJS_POINT3* grv=&w.stg_grv[stg_stt];
        if(w.vtx_num!=0){
            w.draw_queued=true;
            for(int n=w.vtx_num-1;n>=0;grv++){
                for(int k=16;k>0;--k){
                    const float s=*spd;
                    const NJS_POINT3& d=w.vtx_dir[unsigned(n)&0xFu];
                    pos->x+=d.x*s;
                    pos->y+=d.y*s;
                    pos->z+=d.z*s;
                    njAddVector(reinterpret_cast<NJS_VECTOR*>(pos),reinterpret_cast<NJS_VECTOR*>(grv));
                    --n;
                    ++pos;
                    *spd=s+w.vtx_accel;
                    ++spd;
                }
            }
        }
    }else{
        w.live=false;
        w.draw_queued=false;
    }
}

// effsub3.c:145-231 — bhSetBloodPoolLnk. Zwei Arbeiten aus AllocOworkOne; die
// Hoehe staffelt sich ueber sys->bl_ct (nativ effect_blood_counter(), derselbe Besitzer,
// den auch bhEff301/bhEff303 in native/effect_rap_blood.cpp zaehlen).
int EffectParticles::spawn_pool(const void* table,NJS_POINT3 position,int direction,int palette,const RECVX_ENEMY_WORK* linked_work)
{
    (void)palette;
    if(!table)throw std::runtime_error("blood pool without BP_WORK table");
    struct BP {NJS_POINT3 off_pos;int srd_dir;float srd_pos;float bld_spd;float srt_spd[5];int srt_dir[5];};
    const BP& tab=*static_cast<const BP*>(table);
    Pool* p=impl_->alloc_pool();
    if(!p)return 0;                                   // effsub3.c:228-231
    p->linked_work=linked_work;
    p->position=position;
    p->position.y+=0.01f;
    njAddVector(reinterpret_cast<NJS_VECTOR*>(&p->position),reinterpret_cast<NJS_VECTOR*>(const_cast<NJS_POINT3*>(&tab.off_pos)));
    // effsub3.c:176/:178 — erst verwenden, dann zaehlen (bhEff303 macht es umgekehrt).
    p->position.y+=0.001f*float(effect_blood_counter());
    effect_blood_counter()=(effect_blood_counter()+1)&0x1FF;
    p->eff_dir=direction;
    p->srd_dir=tab.srd_dir;
    p->srd_pos=tab.srd_pos;
    p->bld_spd=tab.bld_spd;
    for(unsigned i=0;i<5;++i){
        p->srt_spd[i]=tab.srt_spd[i];
        p->srt_vtx[i]=tab.srt_dir[i];
    }
    p->palette=unsigned(palette);
    p->mode0=0;
    p->ct0=0;
    return 1;                                         // effsub3.c:226
}

// effsub3.c:234-402 — bhEff300 mit VtxTbl[16].
static void step_pool(recvx::Pool& p,const std::function<bool(NJS_POINT3)>& wall,unsigned presentation_flags)
{
    p.draw_queued=false;
    static const int VtxTbl[16]={0,1,3,5,7,9,11,13,15,14,12,10,8,6,4,2};
    if(p.mode0==0){
        const float spd=p.srd_pos*p.bld_spd;
        const int base=p.srd_dir;
        NJS_POINT3 center{spd*-njCos(base),0,spd*njSin(base)};
        int ang=p.eff_dir+16384;
        for(unsigned i=0;i<16;++i){
            int tmp=4096*int(i);
            const float s=p.bld_spd;
            if(!(i&1u))tmp*=-1;
            ang+=tmp;
            p.buffer[i].offset={0,0,0};
            p.buffer[i].u=0.5f+(0.5f*njCos(ang));
            p.buffer[i].v=0.5f+(0.5f*njSin(ang));
            p.buffer[i].col=0xF0808080u;
            p.spd[i].x=s*njCos(ang);
            p.spd[i].y=0;
            p.spd[i].z=s*-njSin(ang);
            njSubVector(reinterpret_cast<NJS_VECTOR*>(&p.spd[i]),reinterpret_cast<NJS_VECTOR*>(&center));
        }
        p.ct0=0;
        ++p.mode0;
        return;
    }
    if(p.mode0==1){
        for(unsigned i=0;i<16;++i){
            njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[i].offset),reinterpret_cast<NJS_VECTOR*>(&p.spd[i]));
            p.spd[i].x*=0.98f;
            p.spd[i].z*=0.98f;
        }
        for(unsigned srt_no=0;srt_no<5;++srt_no){
            const float spd=p.srt_spd[srt_no];
            if(spd==0)continue;
            const int pnt=p.srt_vtx[srt_no];
            // Die Originalzeile tvP[VtxTbl[pnt]] indiziert ungeprueft. Alle vier
            // Originaltabellen (en01_BldTbl, en01_BldTbl2, en04_BldTbl, en07
            // BloodParam) halten srt_dir in 0..15; ein Verstoss wird benannt
            // statt still maskiert.
            if(pnt<0||pnt>15)throw std::runtime_error("blood pool srt_dir outside VtxTbl");
            NJS_POINT3 add=p.spd[std::size_t(VtxTbl[pnt])];
            NJS_POINT3 pos=p.position;
            njAddVector(reinterpret_cast<NJS_VECTOR*>(&pos),reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[pnt&0xF])].offset));
            if(wall(pos)){
                p.srt_spd[srt_no]=0;
            }else{
                add.x*=spd;
                add.z*=spd;
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[pnt])].offset),reinterpret_cast<NJS_VECTOR*>(&add));
                add.x*=0.9f;
                add.z*=0.9f;
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[(pnt+1)&0xF])].offset),reinterpret_cast<NJS_VECTOR*>(&add));
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[(pnt-1)&0xF])].offset),reinterpret_cast<NJS_VECTOR*>(&add));
                add.x*=0.4f;
                add.z*=0.4f;
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[(pnt+2)&0xF])].offset),reinterpret_cast<NJS_VECTOR*>(&add));
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[(pnt-2)&0xF])].offset),reinterpret_cast<NJS_VECTOR*>(&add));
                add.x*=0.2f;
                add.z*=0.2f;
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[(pnt+3)&0xF])].offset),reinterpret_cast<NJS_VECTOR*>(&add));
                njAddVector(reinterpret_cast<NJS_VECTOR*>(&p.buffer[std::size_t(VtxTbl[(pnt-3)&0xF])].offset),reinterpret_cast<NJS_VECTOR*>(&add));
            }
        }
        if(p.ct0++>256){++p.mode0;p.ct0=0;}
    }
    // effsub3.c:385-397: case 1 faellt nach der Ausbreitung in case 2 durch.
    // Das Einreihen geschieht im Kontrollpass, nicht bei jedem Zeichenaufruf.
    p.draw_queued=!p.linked_work||(!(p.linked_work->stflg&0x1000000u)&&(presentation_flags&2u));
}

// effect.c:776-845 — die gemeinsame Steuerschleife loest flg 0x80 ueber
// njCalcPoint(&lkwkp->mlwP->owP[lkono].mtx, &lox, &px) auf. Genau das leistet
// der resolve-Rueckruf; die Wurzelarbeit wird dabei verschoben, wie im Original.
void EffectParticles::bind_pool(const std::function<unsigned(unsigned)>& flags,
                                const std::function<void(unsigned,unsigned,unsigned)>& write)
{
    if(!flags||!write)throw std::runtime_error("particle pool requires both owner callbacks");
    if(impl_->active_count())throw std::runtime_error("cannot rebind live particle payloads");
    impl_->owner_flags=flags;
    impl_->owner_write=write;
}

void EffectParticles::tick_slot(unsigned slot,const std::function<NJS_POINT3(const ParticleLink&,NJS_POINT3)>& resolve,const std::function<bool(NJS_POINT3)>& wall,unsigned presentation_flags)
{
    if(slot>=eff_slots)throw std::runtime_error("particle dispatch outside eff[512]");
    auto& w=impl_->works[slot];
    if(w.live){
        w.draw_queued=false;
        if(w.linked){
            ParticleLink link;
            link.linked=true;
            link.kind=w.link_kind;
            link.index=w.link_index;
            link.node=w.link_node;
            w.position=resolve(link,w.offset);
        }
        step_work(w);
        if(!w.live){
            impl_->release(w);
            impl_->free_slot(int(slot));
        }
    }
    auto& p=impl_->pools[slot];
    if(p.live)step_pool(p,wall,presentation_flags);
}

void EffectParticles::tick(const std::function<NJS_POINT3(const ParticleLink&,NJS_POINT3)>& resolve,const std::function<bool(NJS_POINT3)>& wall,unsigned presentation_flags)
{
    for(unsigned slot=0;slot<eff_slots;++slot)
        if(impl_->flags(slot)&1u)tick_slot(slot,resolve,wall,presentation_flags);
}

std::size_t EffectParticles::active()const{return impl_->active_count();}

// effect.c:220/234: der Originalkopie entspricht der vollstaendige Satz der
// Nutzlasten (works/pools je eff-Platz, bl_ct als Hoehenstaffelung und der
// AllocOwork-Umlaufzeiger). sprites_ gehoert dem Raumbesitzer.
struct EffectParticles::State {
    std::vector<Work> works;
    std::vector<Pool> pools;
    unsigned scan=0,blood_counter=0;
};
std::shared_ptr<const EffectParticles::State> EffectParticles::save()const{
    auto state=std::make_shared<State>();
    state->works=impl_->works;state->pools=impl_->pools;
    state->scan=impl_->scan;state->blood_counter=effect_blood_counter();
    return state;
}
void EffectParticles::restore(const std::shared_ptr<const State>& state){
    if(!state)return;
    impl_->works=state->works;impl_->pools=state->pools;
    impl_->scan=state->scan;effect_blood_counter()=state->blood_counter;
}

const std::set<std::string>& EffectParticles::unsupported()const{return impl_->unsupported;}

// effsub3.c:1432-1503 — bhEff_PtclLineDraw. Je Stufe sechzehn Strecken von der
// Punktposition nach pos + (dir[15-i] * spd + grv) * (spd * scale * 100). Der
// Anfangspunkt traegt nur RGB, der Endpunkt zusaetzlich den Alphaanteil
// (lne_col[0] = col & 0xFFFFFF, lne_col[1] = col), den njDrawLine3D ueber die
// Strecke mischt.
static void append_work_lines(Scene& scene,const recvx::Work& w)
{
    for(int n=0;n<w.stg_num;++n){
        const int stg_no=w.stg_stt+n;
        if(stg_no<0||stg_no>5)throw std::runtime_error("particle stage cursor outside stg_buf");
        const unsigned col=unsigned(w.stg_col[stg_no]);
        const float* spd=&w.vtx_spd[std::size_t(stg_no)*16];
        const NJS_POINT3* p3=&w.vtx_pos[std::size_t(stg_no)*16];
        const NJS_POINT3* dir=&w.vtx_dir[15];
        NJS_POINT3 grv=w.stg_grv[stg_no];
        for(unsigned i=0;i<16;++i){
            const float v=*spd;
            const float scl=v*w.scale*100.0f;
            NJS_POINT3 vct{dir->x*v,dir->y*v,dir->z*v};
            njAddVector(&vct,&grv);
            const NJS_POINT3& p=p3[i];
            SceneLine line;
            line.points[0]=p;
            line.points[1]={p.x+vct.x*scl,p.y+vct.y*scl,p.z+vct.z*scl};
            line.colors[0]=col&0xFFFFFFu;
            line.colors[1]=col;
            scene.lines.push_back(line);
            ++spd;
            --dir;
        }
    }
}

// effsub3.c:1386-1429 — bhEff_PtclSpriteDrawB: je Stufe vier njPtclDrawSprite
// ueber je vier Punkte, Texturrahmen tex_id + (stg_tim & 3). Jeder Aufruf gibt
// nur seine Punkte 0 und 1 als GS-Sprite aus (Kopfkommentar, Punkt 2).
void EffectParticles::append(Scene& scene,const SceneCamera& camera,unsigned texture_base,bool mirror)const
{
    // BHD_ASPECT_X (main.c:53, system.c:272), mit dem auch der Renderer x projiziert.
    constexpr float aspect_w=1.174f;
    constexpr unsigned call_points=4;                 // njPtclDrawSprite(&bufP[4k], 4, scl, scl)
    constexpr unsigned call_sprites=call_points/2;    // Ps2AddPrim3D(.., num = 4): vier Eckpunkte
    for(const auto& w:impl_->works){
        if(!w.live||!w.draw_queued)continue;
        // effsub3.c:21-27 — FuncTbl[4] = {NULL, bhEff_PtclSpriteDrawB,
        // bhEff_PtclLineDraw, NULL}; 0 und 3 haben keine Zeichenroutine.
        if(w.draw_typ==2){append_work_lines(scene,w);continue;}
        if(w.draw_typ!=1)continue;
        for(int n=0;n<w.stg_num;++n){
            const int stg_no=w.stg_stt+n;
            if(stg_no<0||stg_no>5)throw std::runtime_error("particle stage cursor outside stg_buf");
            const float scl=w.stg_scl[stg_no];
            const NJS_POINT3* points=&w.vtx_pos[std::size_t(stg_no)*16];
            const unsigned color=unsigned(w.stg_col[stg_no]);
            const unsigned texture=texture_base+unsigned(w.stg_tim[stg_no]&3);
            for(unsigned call=0;call<16/call_points;++call){
                for(unsigned sprite=0;sprite<call_sprites;++sprite){
                    EffectSprite s;
                    s.id=302;
                    s.flg=0x100000;
                    s.bl_src=8;
                    s.bl_dst=6;
                    // Eckpunkt A (scr - iz*w*dist, ST 0/0) oben links, B (+, ST 1/1)
                    // unten rechts; die Billboardbasis bildet lokal x/y auf Sicht x/y
                    // (y nach unten) ab.
                    for(unsigned i=0;i<4;++i){
                        s.tv[i].x=(i&1u)?1.f:-1.f;
                        s.tv[i].y=(i&2u)?1.f:-1.f;
                        s.tv[i].z=0;
                        s.tv[i].u=float(i&1u);
                        s.tv[i].v=(i&2u)?1.f:0.f;
                        s.tv[i].col=color;
                    }
                    append_effect_sprite(scene,s,points[call*call_points+sprite],{scl/aspect_w,scl,scl},camera,texture,nullptr,nullptr,mirror,false);
                }
            }
        }
    }
}

// effect.c:1287-1424 — bhDrawTrsEffect3D zeichnet eine eingereihte Arbeit mit
// njDrawTexture3DEx(op->tvp, op->pn, 1); bhSetBloodPoolLnk setzt pn = 16
// (effsub3.c:194) und flg 0x200001, die Matrix ist also px ohne Drehung
// (effect.c:1336-1342, ax..az = 0, sx..sz = 1). njDrawTextureSub3D gibt die
// 16 Eckpunkte als Dreiecksstreifen aus (Kopfkommentar, Punkt 1): Dreieck t
// besteht aus den Eckpunkten t, t+1, t+2. Die Texturfarbe liegt in allen
// Punkten gleich (effsub3.c:286). Fuer den ganzen Aufruf gilt die Clipregel
// von njDrawTextureSub3D/Ps2AddPrim3D (SceneTriangle::ninja_primitive).
void EffectParticles::append_pools(Scene& scene,unsigned texture_base)const
{
    for(const auto& p:impl_->pools){
        if(!p.live||!p.draw_queued)continue;
        const unsigned texture=texture_base+p.palette;
        const unsigned color=p.buffer[0].col;
        SceneVertex strip[16];
        for(unsigned i=0;i<16;++i){
            const auto& b=p.buffer[i];
            strip[i].position={p.position.x+b.offset.x,p.position.y+b.offset.y,p.position.z+b.offset.z};
            strip[i].u=b.u;
            strip[i].v=b.v;
        }
        const std::size_t first=scene.triangles.size();
        // Der GS kennt keine Rueckseiten; die Umlaufrichtung folgt wie in
        // append_effect_sprite ({0,1,2},{1,3,2}) dem Streifen.
        for(unsigned t=0;t+2<16;++t){
            if(t&1u)add_triangle(scene,strip,t,t+2,t+1,color,texture);
            else add_triangle(scene,strip,t,t+1,t+2,color,texture);
        }
        for(auto k=first;k<scene.triangles.size();++k){
            scene.triangles[k].ninja_primitive=std::uint32_t(first+1);
            scene.triangles[k].ninja_screen_reject=true;
        }
    }
}

}
