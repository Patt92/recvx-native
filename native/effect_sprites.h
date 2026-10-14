#pragma once
#include "scene.h"
#include <optional>
#include <set>
#include <functional>
#include <memory>
#include <map>
#include "collision.h"
namespace recvx {
struct EffectModelWork;
// EFF5SNOWRECT (effsub5.h:50): die Schneebox von bhEff219; Aufbau in effect_sprites_internal.h.
struct EffectSnow;
// UV_WORK (types.h:2194): eine Zeile der Original-UV-Tabellen {u, v, Breite, Hoehe}.
struct EffectUv {float u,v,xs,ys;};
// Fields consumed by the original bhEff200/201/218 procedures and draw path.
struct EffectSprite {
    unsigned id=0,type=0,mode0=0,mode1=0,mode2=0,mode3=0,flg=0,stflg=0,ani_ct=0;bool polygon_queued=false;
    // Mitgliedschaft dieses Bildes in sys->ef_trs (effect.c:910 bhControlEffect
    // leert die Liste jeden Pass; bhDrawTrsEffect3D zeichnet nur, was drinsteht).
    bool frame_queued=false;
    // O_WRK.mdflg (offset 0x84). The draw pass reads bit 0x40 together with
    // gm_flg 0x4000 (effect.c:1301); AllocOwork zeroes it and no original
    // effect procedure writes it, so only a future owner can set it.
    unsigned mdflg=0;
    int ayp=0;float yn=0;void* func=nullptr;
    int ax=0,ay=0,az=0,lkono=0,ct0=0,ct1=0,ct2=0,ct3=0,tex_id=-1,bl_src=8,bl_dst=6;
    // O_WRK.exp0 as the room procedures use it: the UV table latched at init.
    const void* uv=nullptr;
    // bhSetEffectTb/bhSetEffect fields used by the generated procedures (px..pz must stay contiguous).
    unsigned mdlver=0;int flr_no=0;float px=0,py=0,pz=0,gpx=0,gpy=0,gpz=0,lox=0,loy=0,loz=0,xn=0,zn=0,sx=1,sy=1,sz=1,sxb=1,syb=1,szb=1,spd=0;void* lkwkp=nullptr;EffectModelWork* mlwP=nullptr;
    // O_WRK.aox/aoy/aoz (0x70..0x78) und ofy (0x3A8): weitere Arbeitsfelder der Originalprozeduren.
    float aox=0,aoy=0,aoz=0,ofy=0;int axp=0,azp=0;
    // Platz dieses Satzes in eff[512] (der Raumbesitzer traegt ihn vor jedem Pass ein) und
    // O_WRK.exp0 als Elternsatz (effsub5.c bhEff202/226/247 schreiben eff[n].exp0 = op):
    // parent_slot ist der Platz, parent der vom Besitzer vor dem Pass aufgeloeste Satz.
    int slot=-1,parent_slot=-1;const EffectSprite* parent=nullptr;
    // Mitgliedschaft in sys->ef_opq (effect.c:897 bhDrawOpqEffect3D: festes Blendpaar 8/6, Tiefenschreiben).
    bool opaque_queued=false;
    // Mitgliedschaft in sys->ef_fnc (effect.c:929): der Satz zeichnet sich mit op->func
    // selbst (quads/lines), nicht ueber bhDrawTrsEffect3D und tex_id/ani_ct.
    bool custom_queued=false;
    // O_WRK.jno[15]: the joint chain bhSetShadow copies up to its -1 terminator.
    std::array<std::int8_t,15> jno{};
    struct Vertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t col=0;};
    std::array<Vertex,4> tv{{{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0}}};
    // Ein Zeichenaufruf einer eigenen Zeichenfunktion (sys->ef_fnc, op->func): als
    // Billboard um center (njUnitRotPortion) oder mit absoluten Welt-Eckpunkten
    // (world_space); category/frame = Texturgruppe und Bild, textured false =
    // njPtclDrawPolygon (unbelegte Farbflaeche), point_filter = njTextureFilterMode(0).
    struct Quad {NJS_POINT3 center;std::array<Vertex,4> vertices;unsigned category,source,destination;unsigned frame=0,pn=4;bool textured=true,world_space=false,point_filter=false,fogged=true,gouraud=false;
        // Ps2SetFogColorSys fuer genau diesen Aufruf (ps2_NaDraw.c:469 njDrawTexture3DHEx).
        bool fog_color_set=false;std::uint32_t fog_color=0;
        // Die Zeichenfunktion setzt kein Blendpaar und erbt den Zustand der vorherigen
        // ef_fnc-Funktion (effsub5.c DrawEff5SnowRect); source/destination gelten dann nicht.
        bool inherit_blend=false;
        // DrawEff5SnowRect: njCalcVector(NULL, &wind) unter cam.mtx, Drehung
        // atan2(v.y, v.x) + 16384 um Z, danach turn_y um Y; wind ist die Weltrichtung.
        bool wind_aligned=false;NJS_POINT3 wind{};int turn_y=0;};
    std::shared_ptr<std::array<unsigned,312>> extra;std::vector<Quad> quads;
    // njDrawLine3D-Aufrufe einer eigenen Zeichenfunktion (effsub1.c bhEff106): Weltlinien dieses Passes.
    std::vector<SceneLine> lines;
    // bhSetExtraEffectWork: der eff[512]-Platz (flg 2) des Hilfsarbeitssatzes, vom Raumbesitzer vergeben.
    int extra_slot=-1;
    // Knotenmatrix des verlinkten Satzes (flg 0x80: lkwkp->mlwP->owP[lkono].mtx), vom Raumbesitzer vor dem Pass gesetzt.
    const float* link_matrix=nullptr;
    // O_WRK.mtn_no/frm_no/ah/aw (effsub6.c bhEff149) und njFogDisable des eigenen Zeichenaufrufs.
    int mtn_no=0,frm_no=0;float ah=0,aw=0;bool fog_disabled=false,point_filter=false,untextured=false;
    // lkwkp als Raumfigur: 0 Spieler, 1 Raumgegner (Index link_index); -1 ohne Link. Vom Raumbesitzer bei bhSetEffectTb gesetzt.
    int link_kind=-1;unsigned link_index=0;
    // Ps2SetFogColorSys fuer genau diesen Zeichenaufruf (ps2_NaDraw.c:469).
    bool fog_color_set=false;std::uint32_t fog_color=0;
    // O_WRK.pn: Eckpunktzahl des Zeichenaufrufs (njDrawTexture3DEx/njDrawPolygon3DExSetData);
    // 4 ist ein Viereck aus zwei Dreiecken, 3 ein einzelnes Dreieck.
    unsigned pn=4;
    // tv[i].col sind eigene Eckfarben (NJS_POLYGON_VTX des ef_ntx-Pfads) statt einer Flaechenfarbe.
    bool gouraud=false;
    // O_WRK.pv[4] (NJS_POLYGON_VTX): freier Arbeitsspeicher, den einzelne Prozeduren
    // anders deuten (effsub1.c bhEff118 legt dort acht eff[]-Plaetze als short ab).
    float pv[16]{};
    unsigned char* exp0=nullptr;
    // Blendpaar, das die eigene Zeichenfunktion (op->func) beim Verlassen stehen
    // laesst; -1 heisst, sie beruehrt den Zustand nicht. Fast alle stellen 8/6 zurueck,
    // bhDraw107 (effsub1.c:6326) laesst 8/10 stehen.
    int fnc_exit_src=8,fnc_exit_dst=6;
    // bhEff219: O_WRK.exp0 zeigt auf die Schneebox. Geteilt wie der Originalzeiger,
    // auch ueber den Effektstapel (Skriptbefehl 0x70/0x71 kopiert eff[]).
    std::shared_ptr<EffectSnow> snow;
    bool flame_compact_uv=false; // bhEff009 latches its UV bank at initialization.
    // O_WRK.exp0 von bhEff_E16_LaserSightDraw: die von bhCollisionCheckLine2 getroffene Flaeche (ATR_WORK*).
    std::optional<CollisionWall> line_wall;
};
// Read-only per-pass values from native owners; never persisted as a sys/plp work.
// gm_flg: bhSetRoom (room.c:471) clears bit 0 on every room load and no other
// original game source writes it, so the bhEff125 gate stays open after a load.
struct EffectContext {unsigned st_flg=0,pt_flg=1,player_flg=0,player_stflg=0,player_mdflg=0,cb_flg=0,sp_flg=8,yk_ct=0,gm_flg=0;
    // cam.ax/ay/az des Bildes, in dem bhControlEffect laeuft (game.c:50 vor bhControlCamera:58,
    // also die Lage des Vorbildes); camera_ready false = cam.mtx noch Einheitsmatrix (camera.c:18).
    std::array<std::int32_t,3> camera_angles{};bool camera_ready=false;
    // sys->stg_no/rom_no (bhEff113: Raum 1-09 hat eine andere Partikelzahl).
    unsigned stage_no=0,room_no=0;
    // rom->ene_n: Zahl der Raumgegner (bhEff236 rechnet ax % ene_n).
    unsigned enemy_count=0;
    // sys->ef_slow: Zeitlupenteiler, den event.c:836 (bhSysSet 19) setzt. 0 heisst ungebunden;
    // das Tor st_flg 0x2000000 hat in den Originalquellen keinen Schreiber.
    int ef_slow=0;
    // bhCkFlg(sys->ev_flg, 2): das Gas in bhEff127 bleibt aus, solange dieses
    // Ereignisbit steht (objitm.c:1791/1830 setzt es). Flagmatrix 1 des
    // Eventbesitzers, Bit 2 von oben gezaehlt.
    bool ev_flg_2=false;
    // cam.wpx/wpy/wpz (Auge) und cam.vx/vy/vz (Blickrichtung) des Vorbildes (camera.c:45-56).
    NJS_POINT3 camera_position{},camera_view{};};
// sys->winds/windr (mit den Vorbildwerten windsb/windrb) und sys->thunder: Wetterzustand,
// den nur die Effektprozeduren schreiben (effsub1.c bhEff102/105) und bhEff106 bzw.
// bhDrawThunder (effect.c:1775, nur Raum 1-09) lesen. Ein Besitzer, keine Kopie.
struct EffectWeather {float winds=0,windsb=0;int windr=0,windrb=0;float thunder=0;};
EffectWeather& effect_weather();
// sys->gas_py: Hoehe der Gaswand. Nur bhEff127 schreibt und liest sie
// (effsub1.c:8734 Null beim Start, :8818 Anstieg, :8778/:8784/:8792/:8823 Leser).
float& effect_gas_height();
void set_effect_context(const EffectContext& context);
void set_effect_walls(const std::vector<CollisionWall>* walls,float room_height);
// bhCheckWater(&px): first containing water area; top receives py + h.
void set_effect_water(std::function<bool(NJS_POINT3,float&)> probe);
void set_effect_ground(class GroundCollision* ground,const std::vector<CollisionWall>* water_room,const std::vector<CollisionWall>* water_mobile);
void set_effect_link_matrix(unsigned node,const float* matrix);
// effsub1.c:4218 bhEff017: the effect watches the EN00 work it was spawned
// with (bhSetEffect(17, &null_point, ep, 0), eneset.c:461) and clears
// sys->mwalp[work->ct0].flg bit 0 as soon as that work dies
// (stflg 0x1000000, event.c bhEneSetCk). The room owner resolves the entry in
// its own mobile wall list, because sys->mwalp is not an effect-side table.
void set_effect_mobile_wall_clear(std::function<void(int index)> clear);
// effect.c:268 bhSetExtraEffectWork: der Raumbesitzer vergibt den naechsten eff[512]-Platz
// mit flg 2 (oder -1) und gibt ihn wieder frei (*(int*)op->exp0 = 0).
void set_effect_extra_allocator(std::function<int()> reserve,std::function<void(int)> release);
// pp->mlwP->owP[node].mtx eines Spieler-/Gegnersatzes (kind 0 Spieler, 1 Gegner nach Raumindex).
void set_effect_link_lookup(std::function<bool(unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out)> lookup);
// effsub2.c:2296 bhEff_E11_CheckCollisionPlayer: Objektindex des getroffenen Treffermodells oder -1; kuerzt p2.
// effsub3.c:1008 bhSetEffParticleMk2(lkwkp, lnk_no, off, dir, src, dst, typ) aus einer Effektprozedur (Partikelbesitzer des Raums).
void set_effect_particle_mk2(std::function<int(int link_kind,unsigned link_index,unsigned node,NJS_POINT3 offset,NJS_POINT3 direction,unsigned src,unsigned dst,int type)> hook);
void set_effect_player_hit(std::function<int(NJS_POINT3 p1,NJS_POINT3& p2)> lookup);
// pp->ay eines Spieler-/Gegnerwerksatzes (BH_PWORK.ay), nicht aus einer Matrix gerechnet.
void set_effect_work_angle(std::function<bool(unsigned kind,unsigned index,int& ay)> lookup);
// eff[n] des geteilten Satzes: Prozeduren, die ihre Kinder ueber deren Platz fuehren
// (effsub1.c bhEff118), greifen damit auf den Nachbarsatz zu.
void set_effect_pool_lookup(std::function<EffectSprite*(int slot)> lookup);
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view);
// Texturgruppe -> {erste Szenentextur, Bildzahl}.
// fnc_blend: das Blendpaar, das vor diesem ef_fnc-Eintrag gilt; der Aufruf schreibt das
// Paar zurueck, das danach gilt (effect.c:929, Liste in Platzreihenfolge).
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,std::pair<unsigned,unsigned>>& texture_bases,std::pair<unsigned,unsigned>* fnc_blend=nullptr);
// bhControlPlayer lighter (wpnr_no 1): WpnEffTab[1][0] and lgttab[1].
struct LighterSprite {EffectSprite sprite;NJS_POINT3 scale;};
LighterSprite lighter_sprite();
struct OriginalLight {unsigned flg,type;int aspd,lkflg,lkno,lkono,lsrc;NJS_POINT3 position,offset,direction;float spc,dif,amb;NJS_POINT3 color;float near_range,far_range;};
OriginalLight lighter_light();
// bhSetEffectTb / bhSetEffect: a request to start one effect work (the host owns the slots).
struct EffectSpawn {unsigned flg=1,id=0,type=0;int flr_no=0;unsigned mdlver=0;NJS_POINT3 position{},scale{1,1,1};int ax=0,ay=0,lkono=0;bool has_offset=false;NJS_POINT3 offset{};void* linked_work=nullptr;
    // Was die Originalerzeuger nach bhSetEffectTb am neuen Satz eff[n] nachtragen
    // (effsub5.c bhEff238: xn/yn/zn; 220/240: tex_id; 228: az; 202/226/247: exp0 = op).
    bool has_velocity=false;NJS_POINT3 velocity{};int tex_id_override=-1,az=0,parent_slot=-1;
    // effsub6.c:154 bhEff146_Init schreibt am neuen Satz zusaetzlich mode1 und
    // tv[0].col (nur der erste Eckpunkt, wie op->tv->col im Original).
    int mode1=0;bool has_color=false;std::uint32_t color=0;
    // bhSetEffectTb gibt den belegten Platz zurueck; eine Prozedur, die ihre Kinder ueber
    // deren Platz fuehrt (effsub1.c bhEff118), legt ihn in ihrem eigenen pv ab. Der
    // Raumbesitzer traegt ihn nach der Zuweisung dort ein (-1 = kein Rueckschreiben).
    int slot_sink_work=-1,slot_sink_index=-1;};
struct BombRequest {NJS_POINT3 position;float radius;int damage_max,damage_min;}; // bhCheckBombAtari
// CallYakkyouSe(&op->px, SeNo): the owner resolves the arms bank and plays it on SE slot 10.
struct EffectRoomSound {NJS_POINT3 position;int sound;};
// CallBackGroundSe(slot, SeNo) der Effektprozeduren (effsub1.c bhEff105): Hintergrundstimme des Raumbesitzers.
struct EffectBackgroundSound {unsigned slot,sound;};
struct EffectOutput {std::vector<EffectSpawn> spawns;std::vector<unsigned> sounds;std::vector<EffectRoomSound> room_sounds;std::vector<BombRequest> bombs;std::vector<EffectBackgroundSound> background_sounds;std::optional<OriginalLight> light;std::set<unsigned> unsupported;
    // Benannte Dienste, die eine sonst portierte Prozedur nicht erreichen konnte; der
    // Raumbesitzer meldet sie, statt sie zu verdecken.
    std::set<std::string> gaps;
    // effsub1.c:8825 bhEff127: plp->hp = -1, wenn die Gaswand ueber den
    // Spielerkopf steigt. Der Raumbesitzer traegt es am Spielerwork ein.
    bool player_killed=false;};
// Fills the effect work exactly as bhSetEffectTb copies an EF_WORK.
void initialize_effect_sprite(EffectSprite& sprite,const EffectSpawn& spawn);
// bhSetExplosion (weapon.c): sound 534, flash light, effects 16/23/24. `view` is cam.vx/vy/vz.
void set_explosion(NJS_POINT3 position,NJS_POINT3 view);
EffectOutput take_effect_output();
// effsub0.c:10 setentry / effect.c:436 bhSetEffectTb: eine Anforderung in die
// Liste dieses Passes. Die portierten Originalprozeduren rufen sie.
void push_effect_spawn(const EffectSpawn& spawn);
// bhInitEffect: SYSTEM entry 1 holds the ef_info models, then the common effect PVP list.
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& system_entry);
// sys->ef_tn: first texture of each ef_info category (bhDrawTrsEffect3D uses ef_tn[tex_id]+ani_ct).
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(); // {first texture, frames}
// Runs the independent native procedure; `queued` reports whether it entered the translucent list this frame
// (bhDrawTrsEffect3D only draws queued work).
bool step_effect_sprite(EffectSprite& sprite,bool* queued=nullptr);
// Per-pass view of authoritative native flags, not a stored sys/player copy.
bool step_effect_sprite(EffectSprite& sprite,const EffectContext& context,bool* queued=nullptr);
// light.c types 1..6: working RGB is sampled before advancing the clock.
void step_light_color(unsigned type,unsigned speed,unsigned& clock,NJS_POINT3 color,NJS_POINT3& working,bool running);
bool supported_effect_sprite(unsigned id);
// Room texture groups (room.c:305-329 ef_tn) a ported procedure addresses
// through tex_id. Their atlases keep the decoded Ninja alpha, because the
// ported effects draw with their own njColorBlendingMode pair.
bool ported_effect_atlas(unsigned group);
// effect.c:bhSetEffectTb scans the shared eff[512] pool and accepts the first
// slot whose flg has neither allocation bit set. Missing tail entries are zeroed.
int original_effect_slot(const std::vector<unsigned>& flags);

// stflg_out nimmt das Rueckseitenbit 0x10 auf, das der Originalzeichenpfad in
// den Arbeitssatz zurueckschreibt (effect.c:1391/1532/1668); nullptr verwirft es.
void append_effect_sprite(Scene& scene,const EffectSprite& sprite,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture,const float* linked_matrix=nullptr,unsigned* stflg_out=nullptr);
}
