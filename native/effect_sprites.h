#pragma once
#include "scene.h"
#include <array>
#include <optional>
#include <set>
#include <functional>
#include <memory>
#include <map>
#include "collision.h"
#include "enemies/enemy_work.h"
namespace recvx {
struct EffectModelWork;
// EFF5SNOWRECT (effsub5.h:50): die Schneebox von bhEff219; Aufbau in effect_sprites_internal.h.
struct EffectSnow;
// Nachbild (effsub1b.c:690 bhDraw136): die Dreiecke von objP[lkono] des verlinkten Werks
// in der Lage des Bildes, in dem bhEff136 angelegt wurde (npCalcSkin/npCopyVlist und
// op->mtx = owP[lkono].mtx ergeben zusammen genau diese Weltlage). Texturen bezieht der
// Zeichenpass aus dem Modell des Werks (kind/index wie EffectSprite::link_kind).
struct EffectAfterimage {unsigned kind=0,index=0,mdl=0,node=0;std::vector<SceneTriangle> triangles;
    std::vector<Scene::AmbientWrite> ambient_writes;
    // Selected mdl[mdlver].texP, not necessarily the body's texP. Shared pixel storage.
    std::vector<SceneTexture> textures;};
// R08_WORK (effsub3.c:2492 bhEff308): Funkenstrahl aus bhSetEffGunSpark/bhSetRapEff.
struct EffectSpark308;
// effsub3.c OR_WORK-Saetze (bhSetRapEff): R0A_WORK 310 Kamerawackel, R49_WORK 349 Rueckruf,
// R09_WORK 309 / R0B_WORK 311 Blut, R07_WORK 307 Klauenflaeche, R0_WK 312 gespiegelte Klaue.
// Definitionen in native/effect_rap_camera.h, effect_rap_blood.h, effect_rap_claw.h.
struct EffectCameraShake30a;struct EffectCallback349;struct EffectBlood309;struct EffectBlood30b;struct EffectClawPlane307;struct EffectClaw30c;
// ANM_WORK (effsub3.h:38) von bhEff303: sie liegt im Werk selbst bei (char*)oP + 1028,
// das DSP_WRK dahinter bei + 1060. Von dessen VtxBuf nimmt bhEff303 nur die vier
// Eckpunkte (oP->tvp = dspP->VtxBuf, effsub3.c:1555), und die sind nativ EffectSprite::tv;
// vtx_scl/vtx_pos liest allein ryRapTexDrw der Saetze 309/311.
struct EffectRapAnm;
// UV_WORK (types.h:2194): eine Zeile der Original-UV-Tabellen {u, v, Breite, Hoehe}.
struct EffectUv {float u,v,xs,ys;};
// Fields consumed by the original bhEff200/201/218 procedures and draw path.
// Vierecks-Kategorie: Textur aus der eigenen txp[0]-Liste des Satzes (SpriteWork::texture_list).
constexpr unsigned kEffectOwnTextureList=0xC0000000u;
struct EffectSprite {
    unsigned id=0,type=0,mode0=0,mode1=0,mode2=0,mode3=0,flg=0,stflg=0,ani_ct=0;bool polygon_queued=false;
    // sys->ef_ntx2d[ef_ntx2dn++] = op (effsub1.c:9908/9930 bhEff130 type 3): die untexturierte
    // Schirmliste dieses Passes; bhDrawNtxEffect2D (effect.c) zeichnet pv[0..pn) mit
    // njDrawPolygon(trans 1) ueber das fertige Bild. Wie polygon_queued je Pass neu gesetzt.
    bool ntx2d_queued=false;
    // sys->ef_trs2d[ef_trs2dn++] = op (effsub1.c:249 bhEff2D): die texturierte Schirmliste dieses
    // Passes; bhDrawTrsEffect2D (effect.c:1744) zeichnet njDrawTexture(tvp, pn, gidx, 1) mit dem
    // Blendpaar des Satzes. Wie polygon_queued je Pass neu gesetzt; kein Mitglied von sys->ef_trs.
    bool trs2d_queued=false;
    // Mitgliedschaft dieses Bildes in sys->ef_trs (effect.c:910 bhControlEffect
    // leert die Liste jeden Pass; bhDrawTrsEffect3D zeichnet nur, was drinsteht).
    bool frame_queued=false;
    // O_WRK.mdflg (offset 0x84). The draw pass reads bit 0x40 together with
    // gm_flg 0x4000 (effect.c:1301); AllocOwork zeroes it and no original
    // effect procedure writes it, so only a future owner can set it.
    unsigned mdflg=0;
    int ayp=0;float yn=0;void* func=nullptr;
    // O_WRK.pxb/pyb/pzb: die Lage des Vorbildes, die effect.c:794 bhControlEffect vor jedem
    // Prozeduraufruf festhaelt (effsub2.c bhEff_E03_Acid prueft die Strecke pxb -> px).
    float pxb=0,pyb=0,pzb=0;
    int ax=0,ay=0,az=0,lkono=0,ct0=0,ct1=0,ct2=0,ct3=0,tex_id=-1,bl_src=8,bl_dst=6;
    // O_WRK.exp0 = &sys->ahtp->flg (effsub1.c:3478 bhEff013): der getroffene Wandsatz als Index
    // in den Wandsatz des Raumbesitzers; -1 = kein Satz.
    int wall_ref=-1;
    // O_WRK.exp1 = &htP->flg (effsub3.c:501 bhEff301): der ATR-Typ (exp1[1]) des getroffenen Satzes; -1 = NULL,
    // 256 = Wasserflaeche (:539, nur als Nicht-NULL gelesen).
    int exp1_wall_type=-1;
    // O_WRK.exp0 as the room procedures use it: the UV table latched at init.
    const void* uv=nullptr;
    // bhSetEffectTb/bhSetEffect fields used by the generated procedures (px..pz must stay contiguous).
    unsigned mdlver=0;int flr_no=0;float px=0,py=0,pz=0,gpx=0,gpy=0,gpz=0,lox=0,loy=0,loz=0,xn=0,zn=0,sx=1,sy=1,sz=1,sxb=1,syb=1,szb=1,spd=0;void* lkwkp=nullptr;EffectModelWork* mlwP=nullptr;
    // O_WRK.aox/aoy/aoz (0x70..0x78) und ofx/ofy/ofz (0x3A4/0x3A8/0x3AC): weitere
    // Arbeitsfelder der Originalprozeduren (bhEff352 kopiert dort seine Startlage ab).
    float aox=0,aoy=0,aoz=0,ofx=0,ofy=0,ofz=0;int axp=0,azp=0;
    float ar=0; // O_WRK.ar (bhEff_Sub363 schreibt 1.0, keine Prozedur liest es)
    // O_WRK.shp_ct (0x34C): Bodenhoehe unter dem Werk (bhEff352-Abschlusspruefung) und
    // O_WRK.mtx[13] (Y der Zeichenmatrix des letzten Bildes, ebendieselbe Pruefung).
    float shp_ct=0,mtx_world_y=0;
    // O_WRK.aspd (bhEff134: Leuchtzaehler der Kran-Anzeige).
    int aspd=0;
    // O_WRK.mtn_attr (en15.c:2664/2714 bhEff_E15_Poison: Folgezaehler der Giftwolken).
    int mtn_attr=0;
    // O_WRK.exp1 als eff-Platz (en12.c:1815 eff[eno].exp1 = EXP0_I(32)); -1 = NULL.
    int exp1_slot=-1;
    // Platz dieses Satzes in eff[512] (der Raumbesitzer traegt ihn vor jedem Pass ein) und
    // O_WRK.exp0 als Elternsatz (effsub5.c bhEff202/226/247 schreiben eff[n].exp0 = op):
    // parent_slot ist der Platz, parent der vom Besitzer vor dem Pass aufgeloeste Satz.
    int slot=-1,parent_slot=-1;const EffectSprite* parent=nullptr;
    // O_WRK.exp0 als erzeugendes Gegnerwerk (en15.c:2353/2379 eff[eno].exp0 = epw): der
    // RECVX_ENEMY_WORK, dessen exp0 bhEff_E15_Poison liest und schreibt; nullptr = NULL.
    void* owner_work=nullptr;
    // Mitgliedschaft in sys->ef_opq (effect.c:897 bhDrawOpqEffect3D: festes Blendpaar 8/6, Tiefenschreiben).
    bool opaque_queued=false;
    // Mitgliedschaft in sys->ef_pnc (effect.c:886-892): bhDrawTrsEffect3D unter njControl3D(0x4000).
    // Ps2_use_pt_flag (ps2_NinjaCnk.c:209) loescht dort ABE (ps2_dummy.c:510-516): die Primitive geht
    // nicht in die OT, sondern sofort ohne Mischen mit TEST ATST GREATER 0 und Tiefenschreiben.
    bool punch_queued=false;
    // Mitgliedschaft in sys->ef_fnc (effect.c:929): der Satz zeichnet sich mit op->func
    // selbst (quads/lines), nicht ueber bhDrawTrsEffect3D und tex_id/ani_ct.
    bool custom_queued=false;
    // O_WRK.jno[15]: the joint chain bhSetShadow copies up to its -1 terminator.
    std::array<std::int8_t,15> jno{};
    // O_WRK.hide[4] (0x4D0) und clp_jno[8] (0x350), beide char (EE: vorzeichenbehaftet). bhEff138
    // (effsub1b.c:1618) fuehrt darin die Sperr-/Blinkstufen und die Ziel-/Restmengen der drei Tanks.
    std::array<std::int8_t,4> hide{};std::array<std::int8_t,8> clp_jno{};
    struct Vertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t col=0;};
    // effect.c:506-532 bhSetEffectTb legt Eckpunkte und UVs gemeinsam an:
    // (-1,-1)/(1,-1)/(-1,1)/(1,1) mit (0,0)/(1,0)/(0,1)/(1,1).
    std::array<Vertex,4> tv{{{-1,-1,0,0,0},{1,-1,0,1,0},{-1,1,0,0,1},{1,1,0,1,1}}};
    // Ein Zeichenaufruf einer eigenen Zeichenfunktion (sys->ef_fnc, op->func): als
    // Billboard um center (njUnitRotPortion) oder mit absoluten Welt-Eckpunkten
    // (world_space); category/frame = Texturgruppe und Bild, textured false =
    // njPtclDrawPolygon (unbelegte Farbflaeche), point_filter = njTextureFilterMode(0).
    struct Quad {NJS_POINT3 center;std::array<Vertex,4> vertices;unsigned category,source,destination;unsigned frame=0,pn=4;bool textured=true,world_space=false,point_filter=false,fogged=true,gouraud=false;
        // Ps2SetFogColorSys fuer genau diesen Aufruf (ps2_NaDraw.c:469 njDrawTexture3DHEx).
        bool fog_color_set=false;std::uint32_t fog_color=0;
        // njPtclDrawPolygon (ps2_NinjaPtcl.c:22-80): die Eckpunkte sind +-h Punkte des 640x480-
        // Ninja-Bildschirms um den projizierten Mittelpunkt (scr.x/y +- h, ohne invz*dist), keine
        // Welteinheiten wie bei njPtclDrawSprite (:100, invz*w*dist).
        bool screen_size=false;
        // Die Zeichenfunktion setzt kein Blendpaar und erbt den Zustand der vorherigen
        // ef_fnc-Funktion (effsub5.c DrawEff5SnowRect); source/destination gelten dann nicht.
        bool inherit_blend=false;
        // DrawEff5SnowRect: njCalcVector(NULL, &wind) unter cam.mtx, Drehung
        // atan2(v.y, v.x) + 16384 um Z, danach turn_y um Y; wind ist die Weltrichtung.
        bool wind_aligned=false;NJS_POINT3 wind{};int turn_y=0;
        // bhEff_Draw3DSprite (effsub2.c:4644): Billboard dreht im Spiegelpass um mr_pl. Andere
        // ef_fnc-Billboards (njUnitRotPortion ohne diesen Zweig) tun das nicht.
        bool mirror_plane_turn=false;};
    std::shared_ptr<std::array<unsigned,312>> extra;std::vector<Quad> quads;
    // njDrawLine3D-Aufrufe einer eigenen Zeichenfunktion (effsub1.c bhEff106): Weltlinien dieses Passes.
    std::vector<SceneLine> lines;
    // bhSetExtraEffectWork: der eff[512]-Platz (flg 2) des Hilfsarbeitssatzes, vom Raumbesitzer vergeben.
    int extra_slot=-1;
    // Knotenmatrix des verlinkten Satzes (flg 0x80: lkwkp->mlwP->owP[lkono].mtx), vom Raumbesitzer vor dem Pass gesetzt.
    const float* link_matrix=nullptr;
    // O_WRK.mtn_no/frm_no/ah/aw (effsub6.c bhEff149) und njFogDisable des eigenen Zeichenaufrufs.
    int mtn_no=0,frm_no=0;float ah=0,aw=0;bool fog_disabled=false,point_filter=false,untextured=false;
    // O_WRK.frm_mode (types.h:151, unsigned int @0x370): bhEff303 zaehlt damit die
    // Ausblendpaesse (effsub3.c:1592 `(int)--oP->frm_mode >= 0`).
    unsigned frm_mode=0;
    // O_WRK.car (bhEff108: Ringradius in Kartenzellen).
    float car=0;
    // lkwkp als Raumfigur: 0 Spieler, 1 Raumgegner, 2 Objekt, 3 Item (Index link_index); -1 ohne Link.
    // Vom Raumbesitzer bei bhSetEffectTb bzw. beim Raumladen (room.c:787 bhSetEffectLink) gesetzt.
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
    // bhEff136: Nachbild, ef_Zanzo-Platz dieses Passes (-1 keiner; effect.c:767 leert die
    // Liste je Steuerpass) und das Materialalpha (ct0 + 1) * 40, das bhDraw136 setzt.
    std::shared_ptr<const EffectAfterimage> afterimage;int zanzo_slot=-1;unsigned afterimage_alpha=0;
    std::shared_ptr<EffectSpark308> spark308;
    // free4 der OR_WORK-Saetze aus bhSetRapEff (effsub3.c:2137), je Kennung ein Satz.
    std::shared_ptr<EffectCameraShake30a> shake30a;std::shared_ptr<EffectCallback349> callback349;
    std::shared_ptr<EffectBlood309> blood309;std::shared_ptr<EffectBlood30b> blood30b;
    std::shared_ptr<EffectClawPlane307> plane307;std::shared_ptr<EffectClaw30c> claw30c;
    // stflg 0x20 txp[0] = mlwP->texP eines Gegnerwerks fuer ef_fnc-Vierecke (bhEff_Sub354,
    // en22.c:2638): Kategorie 0x80000000|Raumindex wie plane307; kEffectOwnTextureList = die
    // Texturliste dieses Satzes (SpriteWork::texture_list, z. B. epw->mdl[1].texP); 0 = nicht gesetzt.
    unsigned txp_category=0;
    // O_WRK.mlwP = &ene[model_enemy].mdl[model_mdl] (en22.c:2683) fuer bhEff_ModelDraw (bhEff_Sub364).
    int model_enemy=-1;unsigned model_mdl=0;
    // ANM_WORK von bhEff303 (im Werk bei oP + 1028, siehe die Vorwaertsdeklaration oben).
    std::shared_ptr<EffectRapAnm> rap_anm;
    bool flame_compact_uv=false; // bhEff009 latches its UV bank at initialization.
    // O_WRK.exp0 von bhEff_E16_LaserSightDraw: die von bhCollisionCheckLine2 getroffene Flaeche (ATR_WORK*).
    std::optional<CollisionWall> line_wall;
    // O_WRK.exp0 von bhEff085 (effsub6.c:49): der gesicherte texaddr von ene[lkno].mlwP->texP->
    // textures[3], als Griff auf die Szenentextur; nullptr = NULL (AllocOwork nullt den Satz).
    std::shared_ptr<const SceneTexture> exp0_texaddr;
};
// Read-only per-pass values from native owners; never persisted as a sys/plp work.
// gm_flg: bhSetRoom (room.c:471) clears bit 0 on every room load and no other
// original game source writes it, so the bhEff125 gate stays open after a load.
struct EffectContext {unsigned st_flg=0,pt_flg=1,player_flg=0,player_stflg=0,player_mdflg=0,cb_flg=0,sp_flg=8,yk_ct=0,gm_flg=0;
    // Drehanteil von cam.mtx aus bhControlCamera des Vorbildes (game.c:50 vor :58, RoomWorld::control_camera);
    // camera_ready false = im Raum noch kein bhControlCamera, cam.mtx Einheitsmatrix (room.c:33, camera.c:18).
    std::array<std::int32_t,3> camera_angles{};bool camera_ready=false;
    // Der Raumbesitzer fuehrt die schreibenden ef_fnc-Zeichenteile (bhEff022/024/025/027/100/107)
    // selbst je bhDrawEffect aus (draw_effect_sprite_work); die Steuerung reiht dann nur ein.
    bool defer_custom_draw=false;
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
    // sys->rcase (Skriptbefehl 0x37 bhRoomCaseNo, sonst der Fall der geladenen Raumdatei).
    unsigned room_case=0;
    // cam.wpx/wpy/wpz (Auge) und cam.vx/vy/vz (Blickrichtung) des Vorbildes (camera.c:45-56).
    NJS_POINT3 camera_position{},camera_view{};
    // cam.flg 0x8 und sys->fil_no/fil_rt (cut.c:412-421) fuer bhEffFil (effsub1.c:28).
    bool camera_filter=false;int filter_no=0,filter_rate=0;
    // cam.mtx des Vorbildes (camera.c:28-37) als scene.cpp camera_view_matrix (= diag(1,-1,-1) * cam.mtx),
    // fuer bhCheckCamWall2D (effsub3.c:1611); camera_matrix_ready false = vor dem ersten bhControlCamera.
    std::array<float,16> camera_matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};bool camera_matrix_ready=false;
    // cam.flg 0x10 = ci->flg 0x200 des Schnitts (cut.c:424/517): keine Blutflecken auf der Linse (bhEff301 :463).
    bool camera_cut_wall=false;};
// sys->winds/windr (mit den Vorbildwerten windsb/windrb) und sys->thunder: Wetterzustand,
// den nur die Effektprozeduren schreiben (effsub1.c bhEff102/105) und bhEff106 bzw.
// bhDrawThunder (effect.c:1775, nur Raum 1-09) lesen. Ein Besitzer, keine Kopie.
struct EffectWeather {float winds=0,windsb=0;int windr=0,windrb=0;unsigned thunder=0;};
EffectWeather& effect_weather();
// sys->gas_py: Hoehe der Gaswand. Nur bhEff127 schreibt und liest sie
// (effsub1.c:8734 Null beim Start, :8818 Anstieg, :8778/:8784/:8792/:8823 Leser).
float& effect_gas_height();
// sys->mr_pl (types.h:791): die Spiegelebene. Einziger Schreiber ist bhEff112
// (effsub1.c:7183-7190); Leser sind der Spiegelpass (game.c:83 njMirror, MdlPut.c:61/93)
// und die Billboardzweige unter gm_flg 0x4000 (effect.c:1227/1362/1503/1636,
// effsub2.c:4646). bhInitRoomChangeSystem (system.c:178) setzt sie nicht zurueck; das
// zugehoerige st_flg 0x100 loescht es (system.c:187 st_flg = 0).
NJS_PLANE& effect_mirror_plane();
// sys->bl_ct: der Hoehenversatz, mit dem Blutflecken gegeneinander gestaffelt werden
// (effect.c:54 und system.c:262 nullen ihn). Alle Schreiber zaehlen (x + 1) & 0x1FF und
// addieren 0.001 * bl_ct auf die Hoehe; die Reihenfolge ist je Fundstelle verschieden:
// bhClrEff_RY (effsub3.c:176 nutzen, :178 zaehlen) und bhEff301 (:529/:531) gegen
// bhEff303 (effsub3.c:1532 zaehlen, :1534 nutzen). Ein Besitzer fuer alle, auch fuer den
// Partikelpfad in native/effect_particles.cpp; keine zweite Zaehlung.
unsigned& effect_blood_counter();
// sys->wt_nbpt/wt_px/wt_pz/wt_xp/wt_zp/wt_minx/wt_minz/wt_maxx/wt_maxz/wt_wvp (types.h:781-790):
// die Wasserhoehenkarte. bhObj005 (objitm.c:1270-1289) legt sie aus dem Wasserobjekt an
// und laesst sie abklingen, bhEff108 (effsub1.c:6435) hebt Kreisringe an. Ein Besitzer,
// keine Kopie; `allocated` steht fuer den von bhGetFreeMemory bezogenen wt_wvp (das Original
// loescht ihn beim Raumwechsel nicht, sys->st_flg 0x40 bleibt stehen).
struct WaterWaveField {float px=0,pz=0;int xp=0,zp=0,minx=0,minz=0,maxx=0,maxz=0,nbpt=0;std::vector<float> wvp;bool allocated=false;};
WaterWaveField& water_wave();
void set_effect_context(const EffectContext& context);
void set_effect_walls(const std::vector<CollisionWall>* walls,float room_height);
// bhCheckWater(&px): first containing water area; top receives py + h.
void set_effect_water(std::function<bool(NJS_POINT3,float&)> probe);
// bhCheckWater(&px) mit dem Satz: top = py + h und h (effsub3.c:541 bhEff301 streut 0.5 * h * rand).
void set_effect_water_area(std::function<bool(NJS_POINT3,float&,float&)> probe);
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
// effsub2.c:4446 bhEff_E14_Mucus: opp->mlwP->owP[node].mtx eines Gegnerwerks (RECVX_ENEMY_WORK*), das
// die Prozedur selbst in ene[] gefunden hat (kein flg-0x80-Link). false: Werk/Gelenk ohne nativen Baum.
// Definiert in native/effect_sprite_procedures.cpp.
void set_effect_work_node_matrix(std::function<bool(const void* work,unsigned node,NJS_MATRIX& out)> lookup);
// effsub2.c:2296 bhEff_E11_CheckCollisionPlayer: Objektindex des getroffenen Treffermodells oder -1; kuerzt p2.
// Zugriffe der Effektprozeduren auf Spieler-, Licht-, Gegner- und Stimmenbesitzer des
// Raums (effsub2.c E12: plp-Schaden/-Schub, rom->lgtp[1], bhEne12_SetFireBintaEffect,
// bhEne_CallEffectSE). Ein leerer Dienst wird als Luecke gemeldet.
struct EffectWorldHooks {
    // effsub1.c:8011 bhEff120: objP[node].ang[2] und npSetOffsetUV auf
    // lkwkp->mlwP, kein zweiter Modell-/Posenbesitzer.
    std::function<bool(void* work,unsigned node,int& angle)> work_node_angle_z;
    std::function<bool(void* work,unsigned node,int u,int v)> offset_work_uv;
    std::function<bool(RECVX_ENEMY_PLAYER&)> player_get;
    std::function<void(const RECVX_ENEMY_PLAYER&)> player_set;
    std::function<void(unsigned offset,float value)> player_exp0;
    std::function<void(int jno,NJS_POINT3 ofp,float size,int life)> player_fire;
    // rom->lgtp[1]: E12_Fire (:2681) summiert px/5+py/pz, fr = 20 + lkono/10, Farbe 3/1.3/0.3;
    // E14_Fire (:4012) summiert px/py/pz, fr fest 20, Farbe 3/1.8/0.7. far_divisor 0 = fest.
    std::function<void(NJS_POINT3 pos,NJS_POINT3 color,float far_divisor)> fire_light;
    std::function<void(void* enemy_work,int action)> en12_binta;
    std::function<bool(int se)> effect_se_playing;
    std::function<void(NJS_POINT3 pos,int se)> effect_se;
    // sdfunc.c:1546 CallPlayerWeaponSeEx(&op->px, SeNo, SlotNo) aus einer Effektprozedur
    // (effsub1.c:2415 bhEff007: das Magazin schlaegt auf dem Boden auf). Bank und Syukan
    // bestimmt der Besitzer wie bei den Kampfstimmen (WpnTab[plp->wpnr_no].snd_wpno).
    std::function<void(NJS_POINT3 pos,int se,int slot)> weapon_se;
    std::function<void()> hide_weapon_part2; // effsub1.c:2356 obwp[0].objP[2].evalflags |= 8
    // plp->dvx/dvy/dvz (effsub2.c:2791/2981): derselbe Feldsatz, den bhEne_AttackHitCheck schreibt.
    std::function<void(NJS_POINT3 dv)> player_dv;
    // plp->watr (player.c:1758: c1 = owP[4], c2 = owP[0], r 2.5) fuer npCollisionCheckCC
    // der Effektprozeduren (effsub2.c:1536 bhEff_E03_Acid). false = kein Spielerbaum.
    std::function<bool(RECVX_ENEMY_CAPSULE&)> player_watr;
    // zonzon.c:1376 bhEne_HitCheckParts(plp, pos): naechstes Spielergelenk zu pos wird plp->djnt_no (mit dv/dp);
    // Rueckgabe djnt_no, -1 = kein Spielerbaum (effsub4.c:496 bhEff_Sub350).
    std::function<int(NJS_POINT3)> player_hit_check_parts;
    // Rueckrufe in den EN01-Gegnercode aus einer Effektprozedur (effsub4.c:1189 bhEff_Sub356):
    // kind 0 = bhEne01_NikuhenEffect(work, pos, arg), kind 1 = bhEne01_NeckBloodEffect2(work, arg).
    // false = Gegnerwerk/Dienste nicht gebunden.
    std::function<bool(void* enemy_work,int kind,int arg,NJS_POINT3 pos)> en01_effect;
    // RequestEnemySe(GetLocalEneNo(op->exp0), pos, se) (en15.c:2659): die Stimme des erzeugenden
    // Gegnerwerks (RECVX_ENEMY_WORK*).
    std::function<void(void* enemy_work,NJS_POINT3 pos,int se)> enemy_se;
    // plp->mlwP->owP[joint].mtx (en15.c:2176 bhEne15_AttackPlayerSS: Gelenk 5). false = kein Spielerbaum.
    std::function<bool(unsigned joint,float* out16)> player_joint;
    // plp->dax/day, djnt_no und dpx/dpy/dpz eines Treffers (en15.c:2183-2187).
    std::function<void(int dax,int day,int djnt_no,NJS_POINT3 dp)> player_hit;
    // cam.ofx/ofy/ofz (effsub3.c:3177 Nullsetzen, :3220 njAddVector) des Kamerabesitzers.
    std::function<void(NJS_POINT3 add)> camera_offset_add;
    std::function<void(NJS_POINT3 value)> camera_offset_set;
    // rom->lgtp[2] von bhSetEffSpark (effsub3.c:2455-2481): flg 3, type 13, aspd 16, lsrc 4,
    // nr 15, fr 30, rgb 3/1.8/0.7, Richtung dir, Lage pos, lkono++ (ct0/lkono/v genullt, wenn flg 1 aus war).
    std::function<void(NJS_POINT3 pos,NJS_POINT3 dir)> spark_light;
    // rom->lgtp[0] der Feuer-/Explosionslichter: flg 3, type 101, aspd 8, lkflg/lkno/lkono 0,
    // mode 0, lsrc 4, Richtung 0, ct0 0, light NULL; rgb/nr/fr und Lage je Aufrufer.
    // only_if_off: nur wenn lgtp[0].flg & 0x2 nicht steht (effsub1.c bhEff019 type 0);
    // bhEff130 type 6 (effsub1.c:10076) schreibt unbedingt.
    std::function<void(NJS_POINT3 pos,NJS_POINT3 rgb,float nr,float fr,bool only_if_off)> room_light0;
    // effsub6.c:43/56 bhEff085: rom->effp[type].lkflg/lkno, wie bhSetEffectLink (room.c:787) sie beim
    // Raumladen hinterlaesst (lkno ausserhalb der Raumanzahl -> 0). false: type ausserhalb rom->eff_n.
    std::function<bool(unsigned type,int& lkflg,int& lkno)> room_effect_link;
    // effsub6.c:47-51/60 bhEff085: ene[enemy].mlwP->texP->textures[index].texaddr als Griff auf die
    // Szenentextur. get: nullptr, wenn der Werksatz diesen Eintrag nicht hat. set schreibt den Eintrag
    // der geteilten texP (sys->et_lp, room.c:592-604): jeder Raumgegner mit derselben Liste sieht ihn.
    std::function<std::shared_ptr<const SceneTexture>(unsigned enemy,unsigned index)> enemy_texaddr_get;
    std::function<bool(unsigned enemy,unsigned index,const std::shared_ptr<const SceneTexture>& texaddr)> enemy_texaddr_set;
    // effsub6.c:41 &sys->ef_tlist.textures[sys->ef_tn[group]]: das erste Bild der Effektgruppe.
    std::function<std::shared_ptr<const SceneTexture>(unsigned group)> effect_texaddr;
    // effsub4.c:2298/2302 bhEff370 type 9 -> bhSetEffParticle, nicht Mk2 (andere Farbfolge).
    std::function<int(int kind,unsigned index,int node,NJS_POINT3 offset,NJS_POINT3 direction,unsigned color,int type)> particle;
};
void set_effect_world_hooks(EffectWorldHooks hooks);
// effsub3.c:1008 bhSetEffParticleMk2(lkwkp, lnk_no, off, dir, src, dst, typ) aus einer Effektprozedur (Partikelbesitzer des Raums).
void set_effect_particle_mk2(std::function<int(int link_kind,unsigned link_index,unsigned node,NJS_POINT3 offset,NJS_POINT3 direction,unsigned src,unsigned dst,int type)> hook);
void set_effect_player_hit(std::function<int(NJS_POINT3 p1,NJS_POINT3& p2)> lookup);
// pp->ay eines Spieler-/Gegnerwerksatzes (BH_PWORK.ay), nicht aus einer Matrix gerechnet.
void set_effect_work_angle(std::function<bool(unsigned kind,unsigned index,int& ay)> lookup);
// eff[n] des geteilten Satzes: Prozeduren, die ihre Kinder ueber deren Platz fuehren
// (effsub1.c bhEff118), greifen damit auf den Nachbarsatz zu.
void set_effect_pool_lookup(std::function<EffectSprite*(int slot)> lookup);
// Dienste des Raumbesitzers fuer bhEff135/136 (effsub1b.c:621..760):
// zan_open: sys->ef_flg 0x4 und der 2-MB-Puffer zan_memp beim ersten Lauf im Raum;
// zan_reserve(bytes): sys->zanp vorruecken, false wenn &zanp[n] >= zan_memp + 0x200000;
// vertex_count: objP[node].model->vlist[1] >> 16 von mdl[mdl] des Werks, -1 = model NULL,
// -2 = der Host kennt das Modell nicht; work_position: pp->px/py/pz; capture: das Nachbild.
struct EffectAfterimageServices {
    std::function<void()> zan_open;
    std::function<bool(std::size_t bytes)> zan_reserve;
    std::function<int(unsigned kind,unsigned index,unsigned mdl,unsigned node)> vertex_count;
    std::function<std::optional<NJS_POINT3>(unsigned kind,unsigned index)> work_position;
    std::function<std::shared_ptr<const EffectAfterimage>(unsigned kind,unsigned index,unsigned mdl,unsigned node)> capture;
};
void set_effect_afterimage_services(EffectAfterimageServices services);
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view);
// Texturgruppe -> {erste Szenentextur, Bildzahl}.
// fnc_blend: das Blendpaar, das vor diesem ef_fnc-Eintrag gilt; der Aufruf schreibt das
// Paar zurueck, das danach gilt (effect.c:929, Liste in Platzreihenfolge).
// mirror: zweiter bhDrawEffect unter cam.mtx * njMirror (game.c:81-87, Scene::mirror_matrix).
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,std::pair<unsigned,unsigned>>& texture_bases,std::pair<unsigned,unsigned>* fnc_blend=nullptr,bool mirror=false);
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
    // bhSetEffectTb(&sys->ef, NULL, lkwkp, lkono) mit Linkzeiger: das Werk (kind wie
    // EffectSprite::link_kind). drop_link_flag: der Erzeuger loescht danach eff[n].flg 0x80
    // (effsub1b.c:689 bhEff135), der Satz haelt lkwkp, folgt dem Gelenk aber nicht.
    int link_kind=-1;unsigned link_index=0;bool drop_link_flag=false;
    // effsub2.c:3035-3040: eff[eno].aox/aoy/aoz und exp1 (eff-Platz, -1 = NULL) am neuen Satz.
    bool has_ao=false;NJS_POINT3 ao{};bool has_exp1=false;int exp1_slot=-1;
    // bhSetEffectTb gibt den belegten Platz zurueck; eine Prozedur, die ihre Kinder ueber
    // deren Platz fuehrt (effsub1.c bhEff118), legt ihn in ihrem eigenen pv ab. Der
    // Raumbesitzer traegt ihn nach der Zuweisung dort ein (-1 = kein Rueckschreiben).
    int slot_sink_work=-1,slot_sink_index=-1;
    // en15.c:2470-2474 _bhEne_SetPoison2: eff[eno].stflg |= 0x20, txp[0] = op->txp[0] (die Textur
    // des Elternsatzes `texture_from_slot`) und exp0 = op->exp0 (das erzeugende Gegnerwerk).
    unsigned stflg_or=0;int texture_from_slot=-1;void* owner_work=nullptr;
    // effsub4.c:305..314 / :2360..2369: Nachtraege am Kind, keine sys->ef-Felder.
    bool has_shp_ct=false;float shp_ct=0;int ct3=0;
    // effsub2.c:1371-1379 bhEff_E02_SandParticle2: eff[eno].sx/sy/sz = 3 (scale) und danach
    // eff[eno].sxb/syb/szb = 0.2/0.3/0.2 am neuen Satz; bhSetEffectTb setzt sonst sxb = sx.
    bool has_scale_base=false;NJS_POINT3 scale_base{};
};
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
    bool player_killed=false;
    // sys->ef_flg-Bits, die eine Prozedur setzt (effsub1b.c:631 bhEff135: 0x4).
    unsigned effect_flags=0;
    // sys->cb_flg-Bits, die eine Prozedur setzt bzw. loescht (effsub1.c:4831/4839 bhEff021: 0x40);
    // der Eventbesitzer (flags[10][0]) traegt sie ein.
    unsigned cb_flg_set=0,cb_flg_clear=0;
    // sys->st_flg-Bits, die eine Prozedur setzt bzw. loescht (effsub1.c:7192/7196 bhEff112: 0x100);
    // der Eventbesitzer (flags[5][0]) traegt sie ein.
    unsigned st_flg_set=0,st_flg_clear=0;};
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
// effect.c:929: Zeichenteil der schreibenden ef_fnc-Routinen eines Satzes (false = keine solche
// Routine oder kein Zeichenpass). stateful_effect_draw: die Kennungen dieser Routinen.
bool draw_effect_sprite_work(EffectSprite& sprite,const EffectContext& context);
bool stateful_effect_draw(unsigned id);
// light.c types 1..6: working RGB is sampled before advancing the clock.
void step_light_color(unsigned type,unsigned speed,unsigned& clock,NJS_POINT3 color,NJS_POINT3& working,bool running);
// light.c:486-657 Typen 7..12 im sp_flg-0x80-Zweig von bhControlLight. LGT_WORK-Felder:
// mode (0x74), ct0 (0x78), spc/dif/amb (0x40..0x48, Flackerfarbe), wax/way/waz (0xc8..0xd0).
// Rueckgabe false: kein Typ 7..12, Aufrufer behaelt seinen Pfad. thunder ist sys->thunder.
struct LightTypeWork {unsigned mode=0,clock=0;NJS_POINT3 flicker{};std::array<std::int32_t,3> angles{};};
bool step_light_type(unsigned type,int speed,NJS_POINT3 color,unsigned thunder,LightTypeWork& work,NJS_POINT3& working_color,NJS_VECTOR& working_direction);
bool supported_effect_sprite(unsigned id);
// effsub3.c:2401 bhSetEffGunSpark -> bhSetRapEff(308): fuellt einen frisch belegten eff-Satz.
bool start_gun_spark(EffectSprite& sprite,NJS_POINT3 pos,NJS_POINT3 dir,std::uint32_t src_col,std::uint32_t dst_col,int typ_no);
// Room texture groups (room.c:305-329 ef_tn) a ported procedure addresses
// through tex_id. Their atlases keep the decoded Ninja alpha, because the
// ported effects draw with their own njColorBlendingMode pair.
bool ported_effect_atlas(unsigned group);
// effect.c:bhSetEffectTb scans the shared eff[512] pool and accepts the first
// slot whose flg has neither allocation bit set. Missing tail entries are zeroed.
int original_effect_slot(const std::vector<unsigned>& flags);

// stflg_out nimmt das Rueckseitenbit 0x10 auf, das der Originalzeichenpfad in
// den Arbeitssatz zurueckschreibt (effect.c:1391/1532/1668); nullptr verwirft es.
// mirror: Aufruf aus dem Spiegelpass (game.c:427): Billboardbasis aus cam.mtx * njMirror, Tor
// gm_flg 0x4000 + mdflg 0x40, Dreiecke mit SceneTriangle::mirror 3. mirror_plane_turn: der
// mr_pl-Zweig effect.c:1225/1360/1501/1634 bzw. effsub2.c:4644 (nicht njPtclDrawSprite).
void append_effect_sprite(Scene& scene,const EffectSprite& sprite,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture,const float* linked_matrix=nullptr,unsigned* stflg_out=nullptr,bool mirror=false,bool mirror_plane_turn=true);
// Drehanteil von cam.mtx * njMirror im Spiegelpass, invertiert und ohne Verschiebung: die Basis,
// die njUnitRotPortion unter dem gespiegelten cam.mtx in Weltkoordinaten bedeutet (links vor
// der ungespiegelten Billboardbasis). false ohne gueltige Spiegelmatrix.
bool mirror_billboard_prefix(const Scene& scene,NJS_MATRIX& out);
// effect.c:1378-1390 Rueckseitentest flg 0x20000 (njGetPlaneNormal2 der projizierten Ecken 0..2).
// mirror_scene != nullptr: unter cam.mtx * mirror_scene->mirror_matrix (Spiegelpass).
bool backface_culled(const std::array<SceneVertex,4>& world,const SceneCamera& camera,const Scene* mirror_scene=nullptr);
}
