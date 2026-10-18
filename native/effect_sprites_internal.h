#pragma once
// Gemeinsame Dienste der von Hand portierten Effektprozeduren. Sie liegen auf
// den vorhandenen Besitzern (EffectSprite, Anforderungs-/Tonliste des
// Raumbesitzers in native/effect_sprites.cpp); es gibt keinen zweiten
// Effektsatz und keine Übersetzerausgabe. Nur fuer native/effect_sprites.cpp
// und native/effect_sprite_procedures.cpp bestimmt.
#include "effect_sprites.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
namespace recvx {
namespace effect_detail {
// rand() der Originalroutinen als Anteil 0..1 (derselbe prozessweite EE-Generator,
// native/core_random.*): -rand() / -2147483648.0f.
float effect_random();
void quad_color(EffectSprite& s,std::uint32_t color);
void quad_uv(EffectSprite& s,const EffectUv& uv);
// Die beim Start festgehaltene UV-Tabelle (O_WRK.exp0) an Zeile index.
const EffectUv& latched_uv(const EffectSprite& s,int index,const char* what);
// sys->ef: die eine Anforderungsvorlage der Original-Effektprozeduren. Sie ist
// prozessweit und behaelt zwischen den Aufrufen ihre Felder; wer nur einen Teil
// schreibt (effsub1.c bhEff101/103/104/015 u.a.), uebergibt den Rest aus dem
// letzten Schreiber. Der Host haelt sie an genau einer Stelle.
EffectSpawn& request_template();
// effect.c:436 bhSetEffectTb(&sys->ef, offset, NULL, lkono): die Vorlage in die
// Liste dieses Passes (offset -> lox/loy/loz des neuen Satzes).
void submit_template(int lkono,const NJS_POINT3* offset=nullptr);
// bhSetEffectTb aus einem Dienst, den eine Effektprozedur ruft (bhEff013 -> bhCheckGunAtari):
// der fertige Satz in die Liste dieses Passes, Reihenfolge wie die Vorlagenaufrufe.
void submit_spawn(const EffectSpawn& spawn);
// pwksub.c:2643 bhSetWaterSplash4: schreibt id 11, flg 1, mdlver 0, type+10, flr_no 0,
// Skalierung und Lage in die Vorlage, dann ay = ang+16384 mit lkono 0 und ay = ang-16384
// mit lkono 1. `submit` legt den Satz an: im Effektpass submit_template, sonst direkt.
void water_splash4(NJS_POINT3 pos,int ang,int type,float sx,float sy,float sz,const std::function<void(int lkono)>& submit);
// effsub0.c:10 setentry: Vorlage mit Lage/Winkel/Skalierung des Erzeugers fuellen und uebergeben.
void setentry(unsigned id,unsigned type,const EffectSprite& s);
// eff[lNumber] nach bhSetEffectTb: die zuletzt uebergebene Anforderung, an der die
// Originalerzeuger Felder nachtragen (Startgeschwindigkeit, tex_id, az, Elternsatz).
EffectSpawn& last_request();
// sys->ef_opq[sys->ef_opqn++] = op (effsub0.c bhEff198): die Opak-Liste dieses Passes.
void queue_opaque(EffectSprite& s);
// sys->ef_fnc[sys->ef_fncn++] = op: der Satz zeichnet sich mit op->func selbst.
void queue_custom(EffectSprite& s);
// sys->ef_trs2d[sys->ef_trs2dn++] = op (effsub1.c:249 bhEff2D): die texturierte Schirmliste.
void queue_trs2d(EffectSprite& s);
// sys->cb_flg |= set; sys->cb_flg &= ~clear (effsub1.c:4831/4839 bhEff021) am Eventbesitzer.
void request_cb_flg(unsigned set,unsigned clear);
void request_st_flg(unsigned set,unsigned clear);
// Original-Datentypen der UV-Tabellen in effsub0.h/effsub5.h (EFF0UV/EFF5UV: {u,v} in
// Texeln einer 256er-Textur; EFF0UVWHUC/EFF5UVWH/EFF5UVWHUC: {u,v,Breite,Hoehe}).
// bhGetGroundPosition(&probe) ueber den Bodenanbieter des Raumbesitzers (set_effect_ground);
// ohne Anbieter eine Luecke, kein Ersatzwert.
float ground_height(NJS_POINT3 probe,const char* what);
// hitchk.c:2178 bhCheckWallRefAngle(&pos, flg, ar, ah, &ay) auf dem Wandsatz des Raumbesitzers
// (set_effect_walls): Lage und Winkel werden an der getroffenen Wand gespiegelt.
bool wall_reflect(NJS_POINT3& position,unsigned flags,float radius,float height,int& angle,const char* what);
// hitchk.c:1939 bhCheckWallType(&pos, flg, ar, ah) != NULL auf demselben Wandsatz; ohne Wandsatz
// benannte Luecke und kein Treffer.
bool wall_type(NJS_POINT3 position,unsigned flags,float radius,float height,const char* what);
// weapon.c bhCheckBombAtari(&pos, ar, dmax, dmin): Sprengschaden des Raumbesitzers.
void request_bomb(NJS_POINT3 position,float radius,int damage_max,int damage_min);
// effect.c:331 bhSetEffect(id, &pos, lkp, lkono): ein Satz mit flg 1, type 0, Skalierung 1 ohne sys->ef.
void request_direct(unsigned id,NJS_POINT3 position,int lkono);
// sdfunc.c CallBackGroundSe(slot, SeNo) aus einer Effektprozedur.
void request_background_sound(unsigned slot,unsigned sound);
// effect.c:268 bhSetExtraEffectWork: Hilfsarbeitssatz (eff[n] mit flg 2, genullt) an s.extra
// mit seinem Platz s.extra_slot; false, wenn der Pool voll ist. release gibt ihn zurueck.
bool acquire_extra_work(EffectSprite& s);
void release_extra_work(EffectSprite& s);
// Sicht auf den Hilfsarbeitssatz ab Byte 4 (&op->exp0[4]) als Feld von T.
template<class T> T* extra_rows(EffectSprite& s,const char* what){
    if(!s.extra)throw std::runtime_error(std::string(what)+" without its extra effect work");
    static_assert(sizeof(T)<=1244,"extra row larger than O_WRK");
    return reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(s.extra->data())+4);
}
// hitchk.c:5503 bhCheckFloorEffect(flr_no, px, pz): erste Bodenflaeche (flg 1, type 3) des
// Raumes mit diesem Boden, die px/pz enthaelt; liefert type/prm0 oder nichts.
struct FloorEffectHit {unsigned type,prm0;};
std::optional<FloorEffectHit> floor_effect(int flr_no,float px,float pz,const char* what);
// pp->mlwP->owP[node].mtx (kind 0 Spieler, 1 Gegner) ueber den Raumbesitzer; ohne Anbieter Luecke.
NJS_MATRIX& link_node_matrix(unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out,const char* what);
// Dasselbe ohne Abbruch: false, wenn der Raumbesitzer den Knoten nicht hat.
bool find_link_node_matrix(unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out);
// pp->ay des Werksatzes (kind 0 Spieler, 1 Gegner nach Raumindex).
int work_angle(unsigned kind,unsigned index,const char* what);
// eff[slot] des geteilten Satzes; nullptr, wenn der Platz nicht existiert.
EffectSprite* pool_work(int slot);
// Den von bhSetEffectTb belegten Platz der zuletzt gestellten Anforderung in das pv des
// Erzeugers zurueckschreiben lassen (short an Stelle index, wie es das Original ablegt).
void sink_slot_into_pv(const EffectSprite& owner,int index);
// Ein Dienst, den diese Prozedur nicht erreichen konnte: benannt melden statt verdecken.
void report_gap(const char* name);
// Besitzerzugriffe (EffectWorldHooks); ohne Besitzer benannte Luecke.
bool hook_player_get(RECVX_ENEMY_PLAYER& p,const char* what);
void hook_player_set(const RECVX_ENEMY_PLAYER& p);
void hook_player_exp0(unsigned offset,float value);
void hook_player_fire(int jno,NJS_POINT3 ofp,float size,int life);
// rom->lgtp[1] der Feuer (effsub2.c:2681 E12_Fire, :4012 E14_Fire): Summenlage, Farbe und der
// Teiler der Reichweite 20 + lkono/far_divisor (0 = feste Reichweite 20).
void hook_fire_light(NJS_POINT3 pos,NJS_POINT3 color,float far_divisor);
void hook_en12_binta(void* enemy_work,int action);
bool hook_effect_se_playing(int se);
void hook_effect_se(NJS_POINT3 pos,int se);
void hook_player_dv(NJS_POINT3 dv);
void hook_enemy_se(void* enemy_work,NJS_POINT3 pos,int se);
bool hook_player_joint(unsigned joint,float* out16,const char* what);
void hook_player_hit(int dax,int day,int djnt_no,NJS_POINT3 dp);
// plp->watr des Spielerbesitzers (EffectWorldHooks::player_watr); ohne Besitzer benannte Luecke.
bool hook_player_watr(RECVX_ENEMY_CAPSULE& c,const char* what);
// cam.ofx/ofy/ofz des Kamerabesitzers (EffectWorldHooks::camera_offset_add/_set); ohne Besitzer benannte Luecke.
void hook_camera_offset_add(NJS_POINT3 add);
void hook_camera_offset_set(NJS_POINT3 value);
// rom->lgtp[2] von bhSetEffSpark (EffectWorldHooks::spark_light); ohne Besitzer benannte Luecke.
void hook_spark_light(NJS_POINT3 pos,NJS_POINT3 dir);
// rom->lgtp[0] (EffectWorldHooks::room_light0); ohne Besitzer benannte Luecke.
void hook_room_light0(NJS_POINT3 pos,NJS_POINT3 rgb,float nr,float fr,bool only_if_off,const char* what);
// bhEff085 (effsub6.c:18): rom->effp[type], ene[].mlwP->texP->textures[].texaddr und
// sys->ef_tlist.textures[sys->ef_tn[group]] des Raumbesitzers; ohne Besitzer benannte Luecke.
bool hook_room_effect_link(unsigned type,int& lkflg,int& lkno,const char* what);
std::shared_ptr<const SceneTexture> hook_enemy_texaddr_get(unsigned enemy,unsigned index,const char* what);
void hook_enemy_texaddr_set(unsigned enemy,unsigned index,const std::shared_ptr<const SceneTexture>& texaddr,const char* what);
std::shared_ptr<const SceneTexture> hook_effect_texaddr(unsigned group,const char* what);
// hitchkl.c:49 bhCollisionCheckLine2(p1, p2, flg, flr_no) auf rom->walp + sys->mwalp des
// Raumbesitzers (set_effect_walls) samt :911 bhGetHitCollisionNormal: end wird auf den Treffer
// gekuerzt, normal ist die Flaechennormale des letzten Treffers. Ohne Wandsatz benannte Luecke.
bool collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,const char* what);
// effsub1.c:8825 bhEff127: plp->hp = -1 am Spielerwork des Raumbesitzers.
void request_player_kill();
// sys->ef_flg |= bits (der Raumbesitzer haelt ef_flg).
void request_effect_flags(unsigned bits);
// Die Nachbilddienste des Raumbesitzers (set_effect_afterimage_services).
const EffectAfterimageServices& afterimage_service();
// bhSetExtraEffectWork ohne Satzinhalt: nur den eff[512]-Platz (flg 2) belegen bzw.
// freigeben (*(int*)work = 0). -1, wenn der Pool voll ist.
int reserve_extra_slot();
void release_extra_slot(int slot);
struct EffectUvCell {short u,v;};
struct EffectUvRect {short u,v,w,h;};
// effsub0.c:29 effinit und effsub0.c:63 effset.
void effinit(EffectSprite& s);
void effset(EffectSprite& s,const EffectUv& uv,unsigned num);
}
// EFF5SNOWRECT (effsub5.h:50) mit seinen Hilfssaetzen. Das Original legt Kopf,
// Punktgruppen (je 100 NJS_POINT3 ab Byte 4) und Vektorfeld in je einen
// bhSetExtraEffectWork-Satz; nativ liegen die Daten hier, die Plaetze bleiben im
// eff[512]-Pool belegt wie im Original (header_slot, Group::slot, vector_slot).
// R08_WORK (effsub3.c): prm_a = Eff308PRM_WORK (Offsets per Disassembly 0x0024AD30..0x0024B184:
// vtx_num @0, speed @4, accel @8, @0xC unbenutzt, time @0x10, ang_rand @0x14, spd_rand @0x18,
// tim_rand @0x1C, gravity @0x20), prm_b = PMB_WORK (vtx_pos, vtx_dir, col_src, col_dst, gnd_hgh).
struct EffectSpark308 {
    struct Prm {int vtx_num;float speed,accel,unused_c;int time;float ang_rand,spd_rand;int tim_rand;float gravity;} prm_a{};
    NJS_POINT3 vtx_pos{},vtx_dir{};std::uint32_t col_src=0,col_dst=0;float gnd_hgh=0;
    int mode=0,drw_num=0;float wnd_spd=0,wnd_acl=0;
    std::array<NJS_POINT3,32> VtxBuf{};std::array<NJS_POINT3,16> VtxDir{};std::array<std::uint32_t,32> VtxCol{};
    std::array<int,16> TimBuf{};std::array<std::uint32_t,16> ColAdd{},ColSub{};
};
struct EffectSnow {
    unsigned flg=2;int header_slot=-1;
    unsigned ulFrame=0;
    NJS_POINT3 AreaCenter{};
    float fAreaSizeX=1,fAreaSizeY=1,fAreaSizeZ=1;
    float fAreaMinX=-0.5f,fAreaMaxX=0.5f,fAreaMinY=-0.5f,fAreaMaxY=0.5f,fAreaMinZ=-0.5f,fAreaMaxZ=0.5f;
    int lSnowTotalMax=0;
    struct Group {int slot=-1;int lPointMax=0;std::array<NJS_POINT3,100> points{};};
    std::array<Group,20> SnowGrp{};int lSnowGrpMax=0;
    int vector_slot=-1;std::array<NJS_POINT3,100> vectors{};
    int lSnowExistPrev=0,lSnowExistCrnt=0,lSnowExistNext=0,lSnowTimerCnt=0,lSnowTimerMax=0;
    // pSnowTexList/ulSnowTexNum: Texturgruppe und Bild (sys->ef_tlist, ef_tn[group] + frame).
    unsigned texture_group=0,texture_frame=0;
    std::array<EffectSprite::Vertex,4> SnowModel{};
};
// effsub5.c:5579.. die Schneebibliothek, eigenstaendig portiert.
namespace effect_detail {
std::shared_ptr<EffectSnow> create_snow_rect(int lSnowMax);
void delete_snow_rect(EffectSnow& snow);
void exec_snow_rect(EffectSnow& snow);
void set_snow_area_center(EffectSnow& snow,float x,float y,float z);
void set_snow_area_center_and_size(EffectSnow& snow,float x,float y,float z,float sx,float sy,float sz);
void set_snow_particle_max(EffectSnow& snow,int lSnowMax);
void mov_snow_particle_max(EffectSnow& snow,int lSnowMax,int lTimeMax);
void arrange_snow_particles(EffectSnow& snow);
void set_snow_particle_texture(EffectSnow& snow,unsigned group,unsigned frame,float u0,float v0,float u1,float v1,float u2,float v2,float u3,float v3);
void set_snow_particle_color(EffectSnow& snow,std::uint32_t c0,std::uint32_t c1,std::uint32_t c2,std::uint32_t c3);
void set_snow_particle_size(EffectSnow& snow,float width,float height);
NJS_VECTOR snow_wind_vector();
}
// Eine von Hand portierte Prozedur, wie effect.c:869 bhControlEffect sie je
// Kennung aufruft; `queued` ist die Mitgliedschaft in sys->ef_trs dieses Passes.
struct HandEffectProcedure {unsigned id;void (*step)(EffectSprite& s,const EffectContext& c,bool& queued);};
extern const HandEffectProcedure hand_effect_procedures[];
extern const std::size_t hand_effect_procedure_count;
}
