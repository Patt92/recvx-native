#include "effect_sprites.h"
#include "effect_sprites_internal.h"
#include "effect_eff013.h"
#include "effect_rap_camera.h"
#include "effect_rap_blood.h"
#include "effect_rap_claw.h"
#include "effect_sub3x.h"
#include "chunk_blend.h"
#include "binary_view.h"
#include "core_random.h"
#include "effect_collision.h"
// bhEff017 liest stflg/ct0 des verknuepften Gegnerworks (effsub1.c:4222).
#include "enemies/enemy_work.h"
#include "enemies/en21.h"
#include "enemies/en16.h"
#include "enemies/en03_lib.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace recvx {
// Host-Typen der Effektverknüpfung; EffectSprite hält nur Zeiger darauf.
struct EffectLinkNode {NJS_MATRIX mtx{};};
struct EffectModelWork {EffectLinkNode* owP=nullptr;};
// E03-Basis: ef_info/WpnEffTab/lgttab sind reine Daten; Flamme und Licht
// sind eigenständige native Mechanismen auf dem vorhandenen Besitzer.
// PORTIERUNGSLÜCKE: Andere Effektprozeduren, Explosion/Blut/Trümmer und
// Metalldetektor bleiben ungebunden. Keine Generator-/Originalkernbrücke.
namespace {
EffectOutput pending;
// effect.c ef_info: {model block present, animation textures}. Terminal
// {-1,0} is represented by the end of the native data array.
constexpr std::pair<unsigned,unsigned> categories[]={{1,9},{1,0},{0,2},{0,4},{0,4},{0,4},{0,2},{1,0},{0,1},{0,2},{0,3},{0,5},{0,0},{1,0},{0,0},{0,0},{0,0},{0,2},{0,1},{0,1}};
// UV_WORK (types.h:2194) und EFF5UV/EFF5UVTBL (effsub5.h:6/22) als reine
// Layouttypen der Datentabellen, kein Originalcode.
struct EffectFireUv {short u,v;};
struct EffectFireTable {const EffectFireUv* table;int count,tex_id,ani_ct,palette_bank;short w,h;};
#include "effect_sprite_tables_data.inc"
// bhGetGroundPosition-Anbieter des Raumbesitzers (kein eigener ATR-Satz).
GroundCollision* effect_ground=nullptr;
// rom->walp + sys->mwalp und bhCheckWater des Raumbesitzers (pro Tick gesetzt).
const std::vector<CollisionWall>* effect_wall_set=nullptr;float effect_wall_height=0;std::function<bool(NJS_POINT3,float&)> effect_water_probe;
// sys->mwalp-Zugriff des Raumbesitzers (effsub1.c:4224 bhEff017).
std::function<void(int)> mobile_wall_clear;
// rom->flrp / sys->mflrp (bhCheckFloorEffect), bhSetExtraEffectWork-Platz und Knotenmatrizen des Raumbesitzers.
const std::vector<CollisionWall>* effect_floor_room=nullptr;const std::vector<CollisionWall>* effect_floor_mobile=nullptr;
std::function<int()> extra_reserve;std::function<void(int)> extra_release;
std::function<bool(unsigned,unsigned,unsigned,NJS_MATRIX&)> link_lookup;
std::function<bool(unsigned,unsigned,int&)> work_angle_lookup;
std::function<EffectSprite*(int)> pool_lookup;
EffectAfterimageServices afterimage_services;
// effsub2.c:2296 bhEff_E11_CheckCollisionPlayer des Raumbesitzers (Treffermodelle von
// plp mdl[3] und Figuren 41..43/91..93): Objektindex oder -1, p2 auf den Treffer gekuerzt.
std::function<int(NJS_POINT3,NJS_POINT3&)> player_hit_lookup;
std::function<int(int,unsigned,unsigned,NJS_POINT3,NJS_POINT3,unsigned,unsigned,int)> particle_mk2_hook;
EffectWorldHooks world_hooks;
}
// Gemeinsame Dienste der von Hand portierten Prozeduren (effect_sprites_internal.h).
namespace effect_detail {
// rand() der Originalroutinen: derselbe prozessweite EE-Generator.
float effect_random(){return -recvx_core_random_next()/-2.1474836e9f;}
void quad_color(EffectSprite& s,std::uint32_t color){for(auto& v:s.tv)v.col=color;}
void quad_uv(EffectSprite& s,const EffectUv& uv){
    s.tv[0].u=uv.u;       s.tv[0].v=uv.v;
    s.tv[1].u=uv.u+uv.xs; s.tv[1].v=uv.v;
    s.tv[2].u=uv.u;       s.tv[2].v=uv.v+uv.ys;
    s.tv[3].u=uv.u+uv.xs; s.tv[3].v=uv.v+uv.ys;
}
const EffectUv& latched_uv(const EffectSprite& s,int index,const char* what){
    if(!s.uv)throw std::runtime_error(std::string(what)+" without its original UV table");
    if(index<0)throw std::runtime_error(std::string(what)+" UV cursor below its original table");
    return static_cast<const EffectUv*>(s.uv)[index];
}
// sys->ef der Original-Effektprozeduren (effect.h EF_WORK): eine prozessweite Vorlage.
EffectSpawn& request_template(){static EffectSpawn request;return request;}
}
EffectWeather& effect_weather(){static EffectWeather weather;return weather;}
// sys->gas_py (effsub1.c:8734): ein Besitzer, keine Kopie.
float& effect_gas_height(){static float gas_py=0;return gas_py;}
NJS_PLANE& effect_mirror_plane(){static NJS_PLANE mr_pl{};return mr_pl;}
unsigned& effect_blood_counter(){static unsigned bl_ct=0;return bl_ct;}
WaterWaveField& water_wave(){static WaterWaveField field;return field;}
namespace effect_detail {
void submit_template(int lkono,const NJS_POINT3* offset){
    EffectSpawn e=request_template();e.lkono=lkono;e.linked_work=nullptr;
    e.has_offset=offset!=nullptr;if(offset)e.offset=*offset;else e.offset={};
    e.has_velocity=false;e.velocity={};e.tex_id_override=-1;e.az=0;e.parent_slot=-1;
    e.mode1=0;e.has_color=false;e.color=0;e.has_ao=false;e.ao={};e.has_exp1=false;e.exp1_slot=-1;
    e.link_kind=-1;e.link_index=0;e.drop_link_flag=false;
    e.stflg_or=0;e.texture_from_slot=-1;e.owner_work=nullptr;
    e.has_shp_ct=false;e.shp_ct=0;e.ct3=0;
    e.has_scale_base=false;e.scale_base={};
    pending.spawns.push_back(e);
}
void water_splash4(NJS_POINT3 pos,int ang,int type,float sx,float sy,float sz,const std::function<void(int lkono)>& submit){
    auto& r=request_template();r.id=11;r.flg=1;r.mdlver=0;r.type=unsigned(type+10);r.flr_no=0;
    r.scale={sx,sy,sz};r.position=pos;
    r.ay=ang+16384;submit(0);
    r.ay=ang-16384;submit(1);
}
void submit_spawn(const EffectSpawn& spawn){pending.spawns.push_back(spawn);}
int hook_particle(int kind,unsigned index,int node,NJS_POINT3 offset,NJS_POINT3 direction,unsigned color,int type,const char* what){
    if(!world_hooks.particle){report_gap(what);return -1;}
    return world_hooks.particle(kind,index,node,offset,direction,color,type);
}
bool effect_slot_available(){
    if(!pool_lookup)return true; // isolated procedure tests have no bound owner
    unsigned free=0;
    for(int slot=0;slot<512;++slot){const auto* w=pool_lookup(slot);if(!w||!(w->flg&3u))++free;}
    return free>pending.spawns.size();
}
EffectSpawn& last_request(){
    if(pending.spawns.empty())throw std::runtime_error("effect procedure patches a request before bhSetEffectTb");
    return pending.spawns.back();
}
void queue_opaque(EffectSprite& s){s.opaque_queued=true;}
void queue_custom(EffectSprite& s){s.custom_queued=true;}
void queue_trs2d(EffectSprite& s){s.trs2d_queued=true;}
void request_cb_flg(unsigned set,unsigned clear){pending.cb_flg_set|=set;pending.cb_flg_clear|=clear;}
void request_st_flg(unsigned set,unsigned clear){pending.st_flg_set|=set;pending.st_flg_clear|=clear;}
float ground_height(NJS_POINT3 probe,const char* what){
    if(!effect_ground)throw std::runtime_error(std::string(what)+" without the room's bhGetGroundPosition provider");
    return effect_ground->query(probe);
}
bool wall_reflect(NJS_POINT3& position,unsigned flags,float radius,float height,int& angle,const char* what){
    if(!effect_wall_set)throw std::runtime_error(std::string(what)+" without the room's bhCheckWallRefAngle provider");
    const auto wall=effect_wall(*effect_wall_set,effect_wall_height,position,flags,radius,height);
    if(!wall)return false;
    reflect_effect_wall(*wall,effect_wall_height,position,radius,height,angle);
    return true;
}
bool wall_type(NJS_POINT3 position,unsigned flags,float radius,float height,const char* what){
    if(!effect_wall_set){report_gap(what);return false;}
    return effect_wall(*effect_wall_set,effect_wall_height,position,flags,radius,height).has_value();
}
void request_bomb(NJS_POINT3 position,float radius,int damage_max,int damage_min){pending.bombs.push_back({position,radius,damage_max,damage_min});}
void request_direct(unsigned id,NJS_POINT3 position,int lkono){
    EffectSpawn e;e.flg=1;e.id=id;e.type=0;e.flr_no=0;e.mdlver=0;e.position=position;e.scale={1,1,1};e.ax=0;e.ay=0;e.lkono=lkono;
    pending.spawns.push_back(e);
}
void request_background_sound(unsigned slot,unsigned sound){pending.background_sounds.push_back({slot,sound});}
bool acquire_extra_work(EffectSprite& s){
    if(!extra_reserve)throw std::runtime_error("bhSetExtraEffectWork without the room's eff[512] pool");
    const int slot=extra_reserve();
    if(slot<0)return false;
    s.extra=std::make_shared<std::array<unsigned,312>>();s.extra->fill(0);
    (*s.extra)[0]=2u;s.extra_slot=slot;return true;
}
int reserve_extra_slot(){
    if(!extra_reserve)throw std::runtime_error("bhSetExtraEffectWork without the room's eff[512] pool");
    return extra_reserve();
}
void release_extra_slot(int slot){if(slot>=0&&extra_release)extra_release(slot);}
void release_extra_work(EffectSprite& s){
    if(s.extra)(*s.extra)[0]=0u;
    if(s.extra_slot>=0&&extra_release)extra_release(s.extra_slot);
    s.extra_slot=-1;
}
std::optional<FloorEffectHit> floor_effect(int flr_no,float px,float pz,const char* what){
    if(!effect_floor_room)throw std::runtime_error(std::string(what)+" without the room's bhCheckFloorEffect floor table");
    for(const auto* table:{effect_floor_room,effect_floor_mobile}){
        if(!table)continue;
        for(const auto& a:*table){
            if((a.flags&1u)&&a.type==3&&a.position.x<=px&&a.position.x+a.extent.x>=px&&a.position.z<=pz&&a.position.z+a.extent.z>=pz&&a.floor==flr_no)return FloorEffectHit{a.type,a.parameters[0]};
        }
    }
    return {};
}
bool find_link_node_matrix(unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out){return link_lookup&&link_lookup(kind,index,node,out);}
NJS_MATRIX& link_node_matrix(unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out,const char* what){
    if(!link_lookup||!link_lookup(kind,index,node,out))throw std::runtime_error(std::string(what)+" without the linked work's node matrix");
    return out;
}
EffectSprite* pool_work(int slot){return pool_lookup?pool_lookup(slot):nullptr;}
void report_gap(const char* name){pending.gaps.insert(name);}
bool hook_player_get(RECVX_ENEMY_PLAYER& p,const char* what){if(world_hooks.player_get&&world_hooks.player_get(p))return true;report_gap(what);return false;}
void hook_player_set(const RECVX_ENEMY_PLAYER& p){if(world_hooks.player_set)world_hooks.player_set(p);else report_gap("plp schreiben ohne Spielerbesitzer");}
void hook_player_exp0(unsigned off,float v){if(world_hooks.player_exp0)world_hooks.player_exp0(off,v);else report_gap("PEXP0_F ohne Spielerbesitzer");}
void hook_player_fire(int jno,NJS_POINT3 ofp,float size,int life){if(world_hooks.player_fire)world_hooks.player_fire(jno,ofp,size,life);else report_gap("bhEne_SetFireEffect(plp) ohne Besitzer");}
void hook_fire_light(NJS_POINT3 pos,NJS_POINT3 color,float far_divisor){if(world_hooks.fire_light)world_hooks.fire_light(pos,color,far_divisor);else report_gap("rom->lgtp[1] ohne Lichtbesitzer");}
void hook_en12_binta(void* w,int action){if(world_hooks.en12_binta)world_hooks.en12_binta(w,action);else report_gap("bhEne12_SetFireBintaEffect ohne Gegnerbesitzer");}
bool hook_effect_se_playing(int se){if(world_hooks.effect_se_playing)return world_hooks.effect_se_playing(se);report_gap("bhEne_CheckPlayEffectSE ohne Stimmenbesitzer");return true;}
void hook_player_dv(NJS_POINT3 dv){if(world_hooks.player_dv)world_hooks.player_dv(dv);else report_gap("plp->dvx/dvy/dvz ohne Spielerbesitzer");}
void hook_enemy_se(void* w,NJS_POINT3 pos,int se){if(world_hooks.enemy_se)world_hooks.enemy_se(w,pos,se);else report_gap("RequestEnemySe ohne Stimmenbesitzer (Effekt)");}
bool hook_player_joint(unsigned joint,float* out16,const char* what){if(world_hooks.player_joint&&world_hooks.player_joint(joint,out16))return true;report_gap(what);return false;}
void hook_player_hit(int dax,int day,int djnt_no,NJS_POINT3 dp){if(world_hooks.player_hit)world_hooks.player_hit(dax,day,djnt_no,dp);else report_gap("plp->dax/day/djnt_no/dpx ohne Spielerbesitzer");}
void hook_effect_se(NJS_POINT3 pos,int se){if(world_hooks.effect_se)world_hooks.effect_se(pos,se);else report_gap("bhEne_CallEffectSE ohne Stimmenbesitzer");}
void hook_weapon_se(NJS_POINT3 pos,int se,int slot,const char* what){if(world_hooks.weapon_se)world_hooks.weapon_se(pos,se,slot);else report_gap(what);}
void hook_camera_offset_add(NJS_POINT3 add){if(world_hooks.camera_offset_add)world_hooks.camera_offset_add(add);else report_gap("cam.ofx/ofy/ofz ohne Kamerabesitzer (bhEff30a)");}
void hook_camera_offset_set(NJS_POINT3 value){if(world_hooks.camera_offset_set)world_hooks.camera_offset_set(value);else report_gap("cam.ofx/ofy/ofz ohne Kamerabesitzer (bhEff30a)");}
void hook_room_light0(NJS_POINT3 pos,NJS_POINT3 rgb,float nr,float fr,bool only_if_off,const char* what){if(world_hooks.room_light0)world_hooks.room_light0(pos,rgb,nr,fr,only_if_off);else report_gap(what);}
bool hook_room_effect_link(unsigned type,int& lkflg,int& lkno,const char* what){
    if(!world_hooks.room_effect_link){report_gap((std::string(what)+": rom->effp ohne Raumbesitzer").c_str());return false;}
    if(!world_hooks.room_effect_link(type,lkflg,lkno)){report_gap((std::string(what)+": rom->effp[op->type] ausserhalb rom->eff_n").c_str());return false;}
    return true;
}
std::shared_ptr<const SceneTexture> hook_enemy_texaddr_get(unsigned enemy,unsigned index,const char* what){
    if(!world_hooks.enemy_texaddr_get){report_gap((std::string(what)+": ene[].mlwP->texP ohne Gegnerbesitzer").c_str());return nullptr;}
    auto texaddr=world_hooks.enemy_texaddr_get(enemy,index);
    if(!texaddr)report_gap((std::string(what)+": ene[lkno].mlwP->texP->textures["+std::to_string(index)+"] fehlt").c_str());
    return texaddr;
}
void hook_enemy_texaddr_set(unsigned enemy,unsigned index,const std::shared_ptr<const SceneTexture>& texaddr,const char* what){
    if(!texaddr){report_gap((std::string(what)+": texaddr NULL (kein gesicherter Eintrag)").c_str());return;}
    if(!world_hooks.enemy_texaddr_set||!world_hooks.enemy_texaddr_set(enemy,index,texaddr))
        report_gap((std::string(what)+": ene[lkno].mlwP->texP->textures["+std::to_string(index)+"] nicht schreibbar").c_str());
}
std::shared_ptr<const SceneTexture> hook_effect_texaddr(unsigned group,const char* what){
    if(!world_hooks.effect_texaddr){report_gap((std::string(what)+": sys->ef_tlist ohne Raumbesitzer").c_str());return nullptr;}
    auto texaddr=world_hooks.effect_texaddr(group);
    if(!texaddr)report_gap((std::string(what)+": sys->ef_tn["+std::to_string(group)+"] ohne Effekttextur").c_str());
    return texaddr;
}
void hook_spark_light(NJS_POINT3 pos,NJS_POINT3 dir){if(world_hooks.spark_light)world_hooks.spark_light(pos,dir);else report_gap("rom->lgtp[2] ohne Lichtbesitzer (bhSetEffSpark)");}
bool hook_player_watr(RECVX_ENEMY_CAPSULE& c,const char* what){if(world_hooks.player_watr&&world_hooks.player_watr(c))return true;report_gap(what);return false;}
bool hook_en01_effect(void* work,int kind,int arg,NJS_POINT3 pos,const char* what){if(work&&world_hooks.en01_effect&&world_hooks.en01_effect(work,kind,arg,pos))return true;report_gap(what);return false;}
int hook_player_hit_check_parts(NJS_POINT3 pos,const char* what){if(world_hooks.player_hit_check_parts){const int j=world_hooks.player_hit_check_parts(pos);if(j>=0)return j;}report_gap(what);return -1;}
bool collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,const char* what){
    if(!effect_wall_set){report_gap(what);return false;}
    return effect_line(*effect_wall_set,effect_wall_height,start,end,normal,flags,floor).has_value();
}
std::optional<CollisionWall> collision_line_wall(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,const char* what){
    if(!effect_wall_set){report_gap(what);return std::nullopt;}
    return effect_line(*effect_wall_set,effect_wall_height,start,end,normal,flags,floor);
}
std::function<bool(NJS_POINT3,float&,float&)> effect_water_area_probe;
bool water_area(NJS_POINT3 pos,float& top,float& h,const char* what){
    if(!effect_water_area_probe){report_gap(what);return false;}
    return effect_water_area_probe(pos,top,h);
}
void request_player_kill(){pending.player_killed=true;}
void request_effect_flags(unsigned bits){pending.effect_flags|=bits;}
const EffectAfterimageServices& afterimage_service(){return afterimage_services;}
void sink_slot_into_pv(const EffectSprite& owner,int index){
    auto& request=last_request();
    request.slot_sink_work=owner.slot;request.slot_sink_index=index;
}
int work_angle(unsigned kind,unsigned index,const char* what){
    int ay=0;
    if(!work_angle_lookup||!work_angle_lookup(kind,index,ay))throw std::runtime_error(std::string(what)+" without the linked work's ay");
    return ay;
}
// effsub0.c:10 setentry: sys->ef mit Lage/Winkel/Skalierung des Erzeugers fuellen, bhSetEffectTb(&sys->ef, NULL, NULL, 0).
void setentry(unsigned id,unsigned type,const EffectSprite& s){
    auto& r=request_template();
    r.flg=0x4100001;r.id=id;r.type=type;r.flr_no=0;r.mdlver=0;
    r.position={s.px,s.py,s.pz};r.scale={s.sx,s.sy,s.sz};r.ax=s.ax;r.ay=s.ay;
    submit_template(0);
}
// effsub0.c:29 effinit. op->bank (sys->ef_pbkb[tex_id]) ist die PVP-Palettenbank
// der Originalhardware; der native Zeichenpfad liest dekodierte RGBA-Texturen,
// deshalb hat die Bank hier keinen Abnehmer.
void effinit(EffectSprite& s){
    s.flg|=0x4100000;
    s.tv[0]={-1,-1,0};s.tv[1]={1,-1,0};s.tv[2]={-1,1,0};s.tv[3]={1,1,0};
    quad_color(s,0xffffffffu);
    s.bl_src=8;s.bl_dst=6;
    s.ani_ct=0;s.ct1=0;s.ct0=0;
    s.sxb=s.sx;s.syb=s.sy;
    s.spd=0;s.zn=0;s.yn=0;s.xn=0;
}
// effsub0.c:63 effset: Groesse aus der UV-Kachel und die vier Eckfarben.
void effset(EffectSprite& s,const EffectUv& uv,unsigned num){
    if(num>=10)throw std::runtime_error("bhEff182 smoke colour outside the original kcol table");
    s.sx=4.0f*(s.sxb*uv.xs);
    s.sy=4.0f*(s.syb*uv.ys);
    for(unsigned i=0;i<4;++i)s.tv[i].col=effset_color[num][i];
    quad_uv(s,uv);
}
}
using namespace effect_detail;
namespace {
// effsub1.c:8158 bhEff122 — weltfeste Leuchtplatte, Texturgruppe 41.
// ani_ct waehlt die Platte (type), mode1 die UV-Kachel (Skriptbefehl 0x91).
void step_eff122(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=41;s.flg|=0x4000000;s.bl_src=8;s.bl_dst=6;
        quad_color(s,0xffffffffu);
        // exp0 zeigt auf uvinfo_p0; die Raumskripte adressieren damit auch die
        // direkt folgenden Originalzeilen (siehe Datenmodul, 0x00326B60).
        s.ct0=0;s.uv=eff122_uv_window;s.mode0=1;
    }
    s.ani_ct=s.type;
    if(s.mode1>=22)throw std::runtime_error("bhEff122 mode1 outside the original uvinfo rodata window");
    quad_uv(s,latched_uv(s,int(s.mode1),"bhEff122"));
    queued=true;
}
// effsub1.c:8208 bhEff123 — blinkende Farblampe, gemeinsame Textur 2.
void step_eff123(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=2;s.flg|=0x4100000;s.bl_src=8;s.bl_dst=10;
        s.ani_ct=0;s.ct0=0;s.uv=eff123_uv;s.ct2=0;s.mode0=1;
    }else if(s.mode0==1){
        ++s.ct1;
        if(s.ct1>s.ct2||s.mode1!=0){
            s.ct1=0;s.ct2=int(20.0f+10.0f*effect_random());
            s.mode1=0;s.mode0=2;
        }
        return;
    }
    switch(s.type){
    case 1:quad_color(s,0xffff0000u);break;
    case 2:quad_color(s,0xff00ff00u);break;
    case 3:quad_color(s,0xff0000ffu);break;
    case 4:quad_color(s,0xff00ffffu);break;
    case 5:quad_color(s,~0xffu);break;
    default:quad_color(s,0xffffffffu);break;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff123");
    s.az=s.ct0*4096;
    quad_uv(s,uv);
    s.ct0=(s.ct0+1)&3;
    if(s.ct0==0)s.mode0=1;
    queued=true;
}
// effsub1.c:8458 bhEff125 — Erzeuger; das Skript gibt ihn ueber mode1 frei.
void step_eff125(EffectSprite& s,const EffectContext& c){
    s.flg|=0x1000000;
    if((c.gm_flg&1u)||s.mode1==0)return;
    EffectSpawn e;e.id=126;e.flg=0x4100001;e.mdlver=s.type%4;e.type=s.mode1-1;
    e.scale={0.25f*s.sx,0.25f*s.sy,s.sz};
    const float jitter_x=effect_random()-0.5f,jitter_z=effect_random()-0.5f;
    e.position={s.px+jitter_x,s.py,s.pz+jitter_z};
    e.ay=s.ay;
    pending.spawns.push_back(e);
    s.mode1=0;
}
// effsub1.c:255 bhEff000 -- Patronenhuelse (bhSetGunFire/bhSetYakkyou, Typ 0..3 und 10).
// sys->ef_pol[ef_poln++] ist hier polygon_queued; der Zeichenpfad des Waffenmodells liest es.
void step_eff000(EffectSprite& s,const EffectContext& c){
    s.polygon_queued=false;
    if(int(c.yk_ct)==s.flr_no||s.py<-100.0f){s.flg=0;return;}
    if(!effect_wall_set)throw std::runtime_error("bhEff000 without the room's bhCheckWallType provider");
    if(!effect_ground)throw std::runtime_error("bhEff000 without the room's bhGetGroundPosition provider");
    s.polygon_queued=true;
    auto fall=[&](){ // water drag shared by modes 1 and 4
        if(s.stflg&0x100000){s.zn-=0.005f;if(s.zn<0)s.zn=0;if(s.yn>-0.25f)s.yn-=0.01f;}
        else if(s.yn>-1.0f)s.yn-=0.04f;
    };
    switch(s.mode0){
    case 0:
        s.tex_id=5;s.ani_ct=0;
        // njCalcPoint(link matrix, lox..) is the owner's link_point, applied before this pass.
        s.flg&=~0x80u;
        if(s.mdlver==0){
            switch(s.type){
            case 0:{s.yn=0.2f*effect_random();s.zn=0.65f+0.1f*effect_random();const float r=effect_random();s.ayp=s.ay-(int(182.04445f*(110.0f+(16.0f*r-8.0f)))&0xFFFF);break;}
            case 1:{s.yn=0;s.zn=0.3f+0.1f*effect_random();const float r=effect_random();s.ayp=s.ay-(int(182.04445f*(110.0f+(16.0f*r-8.0f)))&0xFFFF);break;}
            case 2:{s.yn=0.1f*effect_random();s.zn=0.7f+0.1f*effect_random();const float r=effect_random();s.ayp=s.ay-(int(182.04445f*(100.0f+(16.0f*r-8.0f)))&0xFFFF);break;}
            case 3:{s.yn=0.2f+0.1f*effect_random();s.zn=1.0f+0.1f*effect_random();const float r=effect_random();s.ayp=s.ay-(int(182.04445f*(90.0f+(16.0f*r-8.0f)))&0xFFFF);break;}
            case 10:{s.px+=0.2f*effect_random()-0.1f;s.py-=0.2f*effect_random();s.pz+=0.2f*effect_random()-0.1f;
                s.yn=-(0.1f+0.2f*effect_random());s.zn=0;s.ax=16384;s.ayp=s.ay;break;}
            }
        }else{
            s.yn=0.1f*effect_random();s.zn=0.3f+0.1f*effect_random();const float r=effect_random();s.ayp=s.ay-(int(182.04445f*(105.0f+(12.0f*r-6.0f)))&0xFFFF);
        }
        s.ct1=0;s.ct0=0;s.mode0=1;
        [[fallthrough]];
    case 1:
        s.px-=s.zn*njSin(s.ayp);s.pz-=s.zn*njCos(s.ayp);s.py+=s.yn;
        s.ay-=int(182.04445f*(40.0f*s.zn))&0xFFFF;s.ax-=int(182.04445f*(20.0f*s.zn))&0xFFFF;
        fall();
        break;
    case 2:
        if(s.zn>0){
            s.ay-=int(182.04445f*(10.0f*s.zn))&0xFFFF;
            s.px-=s.zn*njSin(s.ayp);s.pz-=s.zn*njCos(s.ayp);s.zn-=0.01f;
            break;
        }
        s.flg=0;return;
    case 3:{
        const float floor=0.1f+effect_ground->query({s.px,s.py,s.pz});
        if(const auto hit=effect_ground->hit();hit&&hit->record&&hit->record->type==6){
            switch(hit->record->id){case 0:s.ayp=0;break;case 1:s.ayp=16384;break;case 2:s.ayp=49152;break;case 3:s.ayp=32768;break;}
            s.ay+=(s.ayp-s.ay)>>1;
            s.px-=s.zn*njSin(s.ay+16384);s.pz-=s.zn*njCos(s.ay+16384);
            if(s.zn<0.5f)s.zn+=0.01f;
            s.py+=s.yn;
            if(s.yn>-1.0f)s.yn-=0.04f;
            if(floor>s.py)s.py=floor;
            return;
        }
        s.mode0=4;
        [[fallthrough]];}
    case 4:{
        s.px-=s.zn*njSin(s.ay+16384);s.pz-=s.zn*njCos(s.ay+16384);
        if(!effect_wall(*effect_wall_set,effect_wall_height,{s.px,s.py-0.1f,s.pz},0x400,0.1f,0.1f)){
            s.py+=s.yn;fall();
        }else{
            s.zn-=0.01f;
            if(s.zn<0){s.zn=0;s.mode0=5;}
        }
        break;}
    case 5:
        s.flg=0;return;
    }
    if((c.st_flg&0x40)&&!(s.stflg&0x100000)){
        float top=0;
        if(effect_water_probe&&effect_water_probe({s.px,s.py,s.pz},top)){
            s.stflg|=0x100000;s.zn*=0.25f;s.yn*=0.25f;
            EffectSpawn splash;splash.flg=1;splash.id=15;splash.type=2;splash.mdlver=0;splash.scale={0.25f,0.25f,0.25f};
            splash.position={s.px,top,s.pz};splash.ay=0;pending.spawns.push_back(splash);
        }
    }
    if(const auto wall=effect_wall(*effect_wall_set,effect_wall_height,{s.px,s.py,s.pz},s.flg,0.1f,0.1f)){
        const float before=s.py;NJS_POINT3 position{s.px,s.py,s.pz};
        reflect_effect_wall(*wall,effect_wall_height,position,0.1f,0.1f,s.ayp);
        s.px=position.x;s.py=position.y;s.pz=position.z;
        if(s.py!=before&&s.yn<0){
            if(s.stflg&0x100000){s.zn=0;s.mode0=2;return;}
            s.py+=0.1f;
            if(wall->type==6){s.yn=0.5f*s.zn;s.zn=0.1f*s.zn;}
            else{s.yn=0.35f*-s.yn;s.zn=0.5f*s.zn;}
            if(std::fabs(s.yn)<0.1f){
                s.ax=0;s.yn=0;
                if(wall->type==6){s.mode0=3;s.zn=0;}else s.mode0=2;
            }
        }
        if(s.ct0==0&&s.type==10){s.zn=0.1f+0.1f*effect_random();s.ayp=int(182.04445f*(360.0f*effect_random()))&0xFFFF;}
        if(s.ct0<2){++s.ct0;pending.room_sounds.push_back({{s.px,s.py,s.pz},262});}
        else if(s.type==1)s.flg=0;
    }else if(s.mode0==2&&s.zn>0)s.mode0=1;
}
// effsub1.c:2335 bhEff007 — das ausgeworfene Magazin (pwksub.c:3372 bhSetMagazine beim
// Nachladen; mdlver 0 Standard, 1 fuer Waffe 12, 2 fuer Waffe 13). Es faellt vom Gelenk der
// Hand, prallt einmal ab und bleibt eine halbe Einheit tiefer liegen; im Wasser bremst es und
// setzt einen Spritzer. Wie bhEff000 steht es jeden Pass in sys->ef_pol.
void step_eff007(EffectSprite& s,const EffectContext& c){
    s.polygon_queued=false;
    if(!effect_ground)throw std::runtime_error("bhEff007 without the room's bhGetGroundPosition provider");
    switch(s.mode0){
    case 0: // :2339-2359
        s.tex_id=7;s.ani_ct=0;
        // njCalcPoint(lkwkp->mlwP->owP[lkono].mtx, &op->lox, &op->px): der Raumbesitzer hat die
        // Linklage dieses Passes schon gesetzt (effect.c:812), danach loest sich der Satz.
        s.gpy=0.6f+effect_ground->query({s.px,s.py,s.pz});
        s.flg&=~0x80u;
        s.yn=-0.3f;
        s.ct0=0;
        // :2354-2357 schreibt auf das rechte Waffenmodell, unabhaengig vom Effektlink.
        if(s.mdlver!=0){if(world_hooks.hide_weapon_part2)world_hooks.hide_weapon_part2();
            else report_gap("bhEff007: sys->obwp->mlwP->objP[2].evalflags ohne Waffenbesitzer");}
        s.mode0=1;
        [[fallthrough]];
    case 1:{ // :2360-2425
        float top=0;
        if((c.st_flg&0x40u)&&!(s.stflg&0x100000u)&&effect_water_probe&&effect_water_probe({s.px,s.py,s.pz},top)){
            s.stflg|=0x100000u;
            s.yn*=0.25f;
            EffectSpawn splash;splash.flg=1;splash.id=15;splash.mdlver=0;splash.type=1;
            splash.scale={0.8f,1.5f,0.8f};
            splash.position={s.px,1.0f+s.py,s.pz};splash.ay=0;
            pending.spawns.push_back(splash);
        }
        s.py+=s.yn;
        if(s.stflg&0x100000u){if(s.yn>-0.3f)s.yn-=0.01f;}
        else if(s.yn>-1.0f)s.yn-=0.05f;
        if(s.py<s.gpy){
            if(s.stflg&0x100000u)s.yn=0;
            else{s.yn=0.3f*-s.yn;hook_weapon_se({s.px,s.py,s.pz},259,0,"bhEff007: CallPlayerWeaponSeEx ohne Stimmenbesitzer");}
            s.mode0=2;
            s.py=s.gpy;
            s.gpy-=0.5f;
        }
        break;}
    case 2: // :2426-2448
        s.py+=s.yn;
        if(s.yn>-1.0f)s.yn-=0.05f;
        if(s.ax<16384)s.ax+=1024;
        if(s.py<s.gpy){s.yn=0;s.py=s.gpy;s.flg=0;return;}
        break;
    default:break;
    }
    s.polygon_queued=true;   // sys->ef_pol[sys->ef_poln++] = op (:2451)
}
// effsub1.c:8488 bhEff126 — aufsteigende Dampf-/Staubwolke, Texturgruppe 35.
void step_eff126(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=35;s.flg|=0x4100000;
        if(s.type<5){
            s.tv[0].x=-1.0f;s.tv[0].y= 1.0f;s.tv[0].z=0;
            s.tv[1].x= 1.0f;s.tv[1].y= 1.0f;s.tv[1].z=0;
            s.tv[2].x=-1.0f;s.tv[2].y=-1.0f;s.tv[2].z=0;
            s.tv[3].x= 1.0f;s.tv[3].y=-1.0f;s.tv[3].z=0;
            quad_color(s,0xffffffffu);
        }
        s.bl_src=8;s.bl_dst=6;
        if(!effect_ground)throw std::runtime_error("bhEff126 without the room's bhGetGroundPosition provider");
        s.gpy=effect_ground->query({s.px,s.py-3.0f,s.pz});
        s.yn=-0.1f;
        switch(s.type){
        case 0:case 1:
            s.uv=eff126_uv[0];s.spd=0.015f*s.sz;s.ani_ct=0;break;
        case 2:case 3:
            s.uv=eff126_uv[1];s.sx*=0.8f;s.sy*=0.8f;s.spd=0.02f*s.sz;s.ani_ct=1;break;
        case 4:
            quad_color(s,((64u-(s.mdlver*16u))<<24)|0xffffffu);
            s.uv=eff126_uv[2];s.sx*=1.2f;s.sy*=1.2f;s.spd=0.005f*s.sz;s.ani_ct=2;break;
        case 6:
            s.yn=0.25f*s.sz;
            [[fallthrough]];
        case 5:
            quad_color(s,((128u-(s.mdlver*32u))<<24)|0xffffffu);
            s.uv=eff126_uv[3];s.ani_ct=2;break;
        default:
            throw std::runtime_error("bhEff126 type outside the original uvinfo selection");
        }
        s.mode0=1;
    }
    switch(s.type){
    case 0:case 1:case 2:case 3:case 4:
        if(s.yn>-3.0f)s.yn-=s.spd;
        s.py+=s.yn;break;
    case 6:
        s.py+=s.yn;s.yn*=0.8f;break;
    default:break;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff126");
    if(uv.u==-1.0f||s.py<s.gpy){s.flg=0;return;}
    quad_uv(s,uv);
    ++s.ct0;
    queued=true;
}
// effsub0.c:1916 bhEff181 — Raucherzeuger; floor(sz) ist der Abstand der Stoesse.
// effsub1.c:2099 bhEff005: blood splash sprite, one UV row per tick until the
// -1 terminator of the size table (type 0 small, 1 medium, 2 large).
void step_eff005(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=5;s.flg|=0x4100000;quad_color(s,0xffffffffu);s.bl_src=8;s.bl_dst=6;s.ct0=0;s.ani_ct=s.mdlver;s.mode0=1;
    }
    const EffectUv* uvp=nullptr;
    switch(s.type){
    case 0:uvp=s.ct0>=0&&s.ct0<14?&eff005_uv_s[s.ct0]:nullptr;break;
    case 1:uvp=s.ct0>=0&&s.ct0<12?&eff005_uv_m[s.ct0]:nullptr;break;
    case 2:uvp=s.ct0>=0&&s.ct0<13?&eff005_uv_l[s.ct0]:nullptr;break;
    default:break; // effsub1.c:2174 leaves uvp unset for other types
    }
    if(!uvp){s.flg=0;throw std::runtime_error("bhEff005 type/ct0 outside its UV tables");}
    if(uvp->u==-1.0f){s.flg=0;return;}
    s.sx=8.0f*(s.sxb*uvp->xs);s.sy=8.0f*(s.syb*uvp->ys);
    quad_uv(s,*uvp);
    ++s.ct0;queued=true;
}
void step_eff181(EffectSprite& s){
    s.flg|=0x1000000;
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0)return;
    const unsigned typ=s.type-1;
    if(std::floor(s.sz)==0.0f){
        setentry(182,typ,s);
        s.mode1=0;s.type=0;return;
    }
    if(s.ct1<1){
        setentry(182,typ,s);
        s.ct1=int(std::floor(s.sz));
    }
    --s.ct1;
}
// effsub0.c:1952 bhEff182 — Rauchwolke, Texturgruppen 400..407.
// type: Farbsatz type%10, Form type/10. njFraction(sz) ist der Vorschub.
void step_eff182(EffectSprite& s,bool& queued){
    const unsigned kcolor=s.type%10,ktype=s.type/10;
    if(s.mode0==0){
        s.tex_id=eff182_texid[kcolor];
        effinit(s);
        s.uv=eff182_kemu[kcolor];
        s.mode0=1;
    }
    NJS_MATRIX basis;njUnitMatrix(&basis);njRotateXYZ(&basis,s.ax,s.ay,0);
    NJS_POINT3 step{0,0,njFraction(s.sz)},moved;njCalcVector(&basis,&step,&moved);
    s.px+=moved.x;s.py+=moved.y;s.pz+=moved.z;
    const auto& uv=latched_uv(s,s.ct0,"bhEff182");
    if(uv.u==-1.0f){s.flg=0;return;}
    effset(s,uv,ktype);
    ++s.ct0;
    queued=true;
}
// effsub1.c:731 bhEff002 — Rauch/Staub-Sprite (Atlas-Gruppe 2). mdlver waehlt die UV-Tabelle,
// type das Verhalten. Die Nachfolgeanforderungen laufen ueber bhSetEffectTb (setentry-Muster).
float eff002_random(){return float(-recvx_core_random_next())/-2147483648.0f;}
void eff002_request(EffectSprite& s,unsigned type,unsigned mdlver){
    EffectSpawn e;e.flg=0x4100001;e.id=2;e.type=type;e.flr_no=0;e.mdlver=mdlver;
    e.position={s.px,s.py,s.pz};e.scale={s.sxb,s.syb,s.szb};e.ax=0;e.ay=0;e.lkono=s.ct0;
    pending.spawns.push_back(e);
}
void step_eff002(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=2;quad_color(s,0x80ffffffu);
        switch(s.type){
        case 2:quad_color(s,0x60c0c0c0u);[[fallthrough]];
        case 0:case 6:s.bl_src=8;s.bl_dst=6;s.yn=0;break;
        case 3:
            s.xn=float(double(eff002_random())*0.1-0.05f);
            s.yn=float(0.1f+double(eff002_random())*0.1);
            s.zn=float(double(eff002_random())*0.1-0.05f);
            [[fallthrough]];
        case 5:
            quad_color(s,s.mdlver==11?0x40ffffffu:0x40c0c0c0u);
            s.ani_ct=1;s.bl_src=8;s.bl_dst=6;break;
        case 4:s.bl_src=8;s.bl_dst=6;s.yn=0;s.ani_ct=1;break;
        case 7:quad_color(s,0x40ffffffu);s.bl_src=8;s.bl_dst=10;s.spd=64.0f;s.ani_ct=1;break;
        case 8:
            s.xn=s.lox;s.yn=s.loy;s.zn=s.loz;
            quad_color(s,0x40ffffffu);s.ani_ct=1;s.bl_src=8;s.bl_dst=10;break;
        case 1:
            s.flg=0x4100001;s.bl_src=11;s.bl_dst=3;
            s.xn=float(double(eff002_random())*0.2-0.1f);
            s.yn=float(0.4f+double(eff002_random())*0.3);
            s.zn=float(double(eff002_random())*0.2-0.1f);
            s.ani_ct=1;break;
        case 9:
            s.xn=float(double(eff002_random())*0.1-0.05f);
            s.yn=s.sz;
            s.zn=float(double(eff002_random())*0.1-0.05f);
            quad_color(s,0xc0202020u);s.ani_ct=1;s.bl_src=8;s.bl_dst=6;break;
        case 10:s.bl_src=8;s.bl_dst=6;s.ct0=s.lkono;s.ct3=128;break;
        default:break;
        }
        switch(s.mdlver){
        case 1:s.uv=eff002_v1;break;
        case 2:case 11:s.uv=eff002_v2;break;
        case 3:s.uv=eff002_v3;break;
        case 4:s.uv=eff002_v4;break;
        case 8:s.uv=eff002_v8;break;
        case 9:s.uv=eff002_v9;break;
        case 10:s.uv=eff002_vz;break;
        default:s.uv=eff002_v0;break;
        }
        s.mode0=1;
        if(s.type<9)return;
    }else if(s.mode0==1){
        switch(s.type){
        case 0:
            if(s.ct0<4)eff002_request(s,10,10);
            [[fallthrough]];
        case 6:
            s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
            s.lox*=0.9f;s.loy*=0.9f;s.loz*=0.9f;
            s.py+=s.yn;
            if(s.yn<0.1f)s.yn+=0.01f;
            break;
        case 3:case 8:
            if(s.flg&0x80){
                eff002_request(s,3,11);
                --s.flr_no;
                if(s.flr_no<=0)s.flg=0;
                return;
            }
            s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
            s.yn=s.yn*0.9f;
            break;
        case 4:
            if(s.ct0<6)eff002_request(s,10,3);
            s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
            s.lox*=0.9f;s.loy*=0.9f;s.loz*=0.9f;
            break;
        case 1:case 9:
            s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
            s.yn=s.yn*0.9f;
            break;
        case 7:
            quad_color(s,(unsigned(s.spd)<<24)|0xffffffu);
            s.spd*=0.9f;
            break;
        case 10:
            quad_color(s,(unsigned(s.ct3)<<24)|0xffffffu);
            s.ct3-=32;
            if(s.ct3<=0){s.flg=0;return;}
            break;
        default:break;
        }
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff002");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.sx=4.0f*(s.sxb*uv.xs);
    s.sy=4.0f*(s.syb*uv.ys);
    quad_uv(s,uv);
    if(s.type<10)++s.ct0;
    queued=true;
}
// effsub5.c:1609 bhEff218 — Feuer. 17 Varianten auf den Texturgruppen 70..79.
void step_eff218(EffectSprite& s,bool& queued){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0){s.flg|=0x1000000;return;}
    s.flg&=~0x1000000u;
    const auto& fire=eff218_tables[(s.type-1)%17];
    if(s.mode0==0){
        s.flg|=0x4180000;s.bl_src=8;s.bl_dst=3;
        s.tv[0].x=s.tv[2].x=-1.0f;s.tv[1].x=s.tv[3].x=1.0f;
        s.tv[0].y=s.tv[1].y=-2.0f;s.tv[2].y=s.tv[3].y=0;
        for(auto& v:s.tv)v.z=0;
        quad_color(s,0xffffffffu);
        s.ct0=int(float(fire.count)*effect_random());
        s.mode0=1;
    }else if(s.mode0==1){
        ++s.ct0;
        if(fire.count<=s.ct0)s.ct0=0;
    }
    s.tex_id=fire.tex_id;s.ani_ct=unsigned(fire.ani_ct);
    if(s.ct0<0||s.ct0>=fire.count)throw std::runtime_error("bhEff218 frame outside its original fire table");
    const auto& info=fire.table[s.ct0];
    s.tv[0].u=s.tv[2].u=float(info.u)/256.0f;
    s.tv[1].u=s.tv[3].u=float((info.u+fire.w)-1)/256.0f;
    s.tv[0].v=s.tv[1].v=info.v?float(info.v+1)/256.0f:float(info.v)/256.0f;
    s.tv[2].v=s.tv[3].v=float(info.v+fire.h)/256.0f;
    queued=true;
}
// effsub5.c:3333 bhEff230 — animierte Leuchttafel, Texturgruppe 414.
// lkono waehlt die UV-Bank, (type-1)%6 die Tafel.
void step_eff230(EffectSprite& s,bool& queued){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0){s.flg|=0x1000000;return;}
    s.flg&=~0x1000000u;
    if(s.lkono<0)throw std::runtime_error("bhEff230 link node below the original UV bank table");
    const unsigned bank=unsigned(s.lkono)%4;
    if(s.mode0==0){
        s.flg|=0x4080000;s.tex_id=414;s.bl_src=8;s.bl_dst=6;s.ct0=0;
        quad_color(s,0xffffffffu);
        s.mode0=1;
    }else if(s.mode0==1){
        ++s.ct0;
        if(int(eff230_uv_max[bank])<=s.ct0)s.ct0=0;
    }
    s.ani_ct=(s.type-1)%6;
    if(s.ct0<0||s.ct0>=int(eff230_uv_max[bank]))throw std::runtime_error("bhEff230 frame outside its original UV bank");
    const auto& info=eff230_uv[bank][s.ct0];
    s.tv[0].u=s.tv[2].u=float(info.u)/256.0f;
    s.tv[1].u=s.tv[3].u=float(info.u+31)/256.0f;
    s.tv[0].v=s.tv[1].v=float(info.v)/256.0f;
    s.tv[2].v=s.tv[3].v=float(info.v+31)/256.0f;
    queued=true;
}
}
void set_effect_context(const EffectContext& context){(void)context;}
LighterSprite lighter_sprite(){
    EffectSpawn spawn;spawn.id=9;spawn.type=2;spawn.scale={.5f,.5f,.5f};
    LighterSprite out;initialize_effect_sprite(out.sprite,spawn);out.scale=spawn.scale;return out;
}
OriginalLight lighter_light(){
    // lgttab[1]. bhSetLightTab replaces its tabular direction with
    // bhGetLightVector(0,0,0), i.e. (0,0,-1).
    return {1,4,30,1,0,9,4,{0,0,0},{.3f,0,-1.5f},{0,0,-1},0,0,0,{3.16f,2.06f,.7f},8,400};
}
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& data){
    if(data.empty())return std::size_t(-1);
    const BinaryView bytes(data);std::size_t cursor=0;
    // Follow exactly the model-bearing categories, not a magic offset or a
    // byte search for PVP. Each model size is followed by the next size word.
    for(const auto [model,frames]:categories)if(model){
        const auto size=bytes.u32(cursor);cursor+=4;
        if(size&0x80000000u)throw std::runtime_error("common effect model chain ends before ef_info");
        bytes.require(cursor,size);cursor+=size;
    }
    bytes.require(cursor,4);cursor+=4; // word fetched after the final model
    const auto offset=(cursor+31)&~std::size_t(31);bytes.require(offset,4);return offset;
}
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(){
    std::vector<std::pair<unsigned,unsigned>> out;unsigned first=0;
    for(const auto [model,frames]:categories){out.emplace_back(first,frames);first+=frames;}return out;
}

// effsub1.c:4220 bhEff017 — der Effekt haelt den verknuepften Work und
// loescht dessen mobile Flaeche, sobald der Work stirbt. Das Original setzt
// op->flg nie zurueck: der Platz bleibt für die restliche Raumlaufzeit belegt
// und die Wache laeuft jeden Pass erneut.
void step_eff017(EffectSprite& s){
    const auto* w=static_cast<const RECVX_ENEMY_WORK*>(s.lkwkp);
    if(!w)return; /* bhSetEffect(17, .., lkp) ist im Original nie ohne Link */
    if(!(w->stflg&0x1000000u))return;
    if(mobile_wall_clear)mobile_wall_clear(w->ct0);
}
// Die von Hand portierten Kennungen: die Tabelle in native/effect_sprite_procedures.cpp
// und die aelteren Schalterfaelle unten (keine doppelte Implementierung derselben Kennung).
// effect.c:33 bhJumpEffect / bhJumpEffect0/2/3/4: diese 143 Plaetze sind bhEffDmy (effsub1.c:22,
// op->flg = 0). Darunter die Raum-Texturhalter 30..84/86..89: room.c:781 legt sie mit
// bhSetEffectTb an, ihre Gruppe bleibt in ef_tn (nativ effect_textures_), der Satz selbst
// endet im ersten bhControlEffect-Pass und gibt seinen eff[512]-Platz frei.
static bool original_dummy_effect(unsigned id){
    return (id>=30&&id<=84)||(id>=86&&id<=89)||id==106||(id>=139&&id<=142)||(id>=277&&id<=295)
        ||(id>=313&&id<=348)||id==358||id==359||(id>=374&&id<=395);
}
const HandEffectProcedure* hand_procedure(unsigned id){
    static const HandEffectProcedure dummy{0,step_eff_dummy};
    if(original_dummy_effect(id))return &dummy;
    for(std::size_t i=0;i<hand_effect_procedure_count;++i)if(hand_effect_procedures[i].id==id)return &hand_effect_procedures[i];
    return nullptr;
}
// en21.c:1168/1324: transient field view of this same eff[] slot, not
// another effect owner. Only fields the two native procedures write return.
static bool step_en21_effect(EffectSprite& s){
    RECVX_EN21_EFFECT op{};
    op.flg=s.flg;op.stflg=s.stflg;op.type=std::uint16_t(s.type);op.mode0=std::uint8_t(s.mode0);
    op.bl_src=s.bl_src;op.bl_dst=s.bl_dst;op.ct0=s.ct0;op.ct1=s.ct1;op.ct2=s.ct2;
    op.px=s.px;op.py=s.py;op.pz=s.pz;op.ay=s.ay;
    op.sx=s.sx;op.sy=s.sy;op.sxb=s.sxb;op.syb=s.syb;op.yn=s.yn;
    for(unsigned i=0;i<4;++i){op.tv[i].col=s.tv[i].col;op.tv[i].u=s.tv[i].u;op.tv[i].v=s.tv[i].v;}
    op.random_next=recvx_core_random_next;
    const bool queued=(s.id==399?recvx_en21_effect_spark(&op):recvx_en21_effect_mark(&op))!=0;
    s.flg=op.flg;s.stflg=op.stflg;s.mode0=op.mode0;s.bl_src=op.bl_src;s.bl_dst=op.bl_dst;
    s.ct0=op.ct0;s.ct1=op.ct1;s.ct2=op.ct2;s.px=op.px;s.py=op.py;s.pz=op.pz;
    s.sx=op.sx;s.sy=op.sy;s.sxb=op.sxb;s.syb=op.syb;s.yn=op.yn;
    for(unsigned i=0;i<4;++i){s.tv[i].col=op.tv[i].col;s.tv[i].u=op.tv[i].u;s.tv[i].v=op.tv[i].v;}
    return queued;
}

// cam.mtx (camera.c:28-37: Basismatrix, Rz(az) Rx(ax) Ry(ay)); nur der Drehanteil
// wird hier gebraucht (njCalcVector). Vor der ersten Kamera ist cam.mtx die Einheit.
static void camera_rotation(const EffectContext& c,NJS_MATRIX& m){
    njUnitMatrix(&m);if(!c.camera_ready)return;
    njScale(&m,1,-1,-1);njRotateZ(&m,c.camera_angles[2]);njRotateX(&m,c.camera_angles[0]);njRotateY(&m,c.camera_angles[1]);
}
// bhCollisionCheckLine2(p1, p2, 0x4400, -1) auf rom->walp + sys->mwalp des Raumbesitzers.
static std::optional<CollisionWall> beam_wall(NJS_POINT3 p1,NJS_POINT3& p2,const char* what){
    if(!effect_wall_set){effect_detail::report_gap(what);return {};}
    NJS_POINT3 normal{};return effect_line(*effect_wall_set,effect_wall_height,p1,p2,normal,0x4400u,-1);
}
static int beam_player(NJS_POINT3 p1,NJS_POINT3& p2,const char* what){
    if(!player_hit_lookup){effect_detail::report_gap(what);return -1;}
    return player_hit_lookup(p1,p2);
}
// NJS_COLOR (argb) als Eckfarbe 0xAARRGGBB; Kanalzuweisung wie das Original (unsigned char).
static std::uint32_t argb(unsigned a,unsigned r,unsigned g,unsigned b){return ((a&255u)<<24)|((r&255u)<<16)|((g&255u)<<8)|(b&255u);}
// njDrawLine3DEx-Farbe: NJS_COLOR (ARGB) ist bereits die Leinwandordnung (SDL ARGB8888).
static std::uint32_t line_color(std::uint32_t c){return c;}
// njDrawPolygon3D(p3c, 3, 0x60) nach njColorBlendingMode(0, 8)/(1, 10) und njFogDisable.
static void beam_triangle(EffectSprite& s,const NJS_POINT3 (&p)[3],const std::uint32_t (&col)[3]){
    EffectSprite::Quad q;q.center={0,0,0};q.world_space=true;q.textured=false;q.fogged=false;q.pn=3;q.gouraud=true;q.source=8;q.destination=10;
    for(unsigned i=0;i<3;++i){q.vertices[i].x=p[i].x;q.vertices[i].y=p[i].y;q.vertices[i].z=p[i].z;q.vertices[i].col=col[i];}
    q.vertices[3]=q.vertices[2];s.quads.push_back(q);
}
// Der Kreisring beider Strahlen: 16 Punkte bei (360*i)/16 Grad (Ganzzahlteilung).
static Angle beam_angle(int i){return Angle(182.04445f*float((360*i)/16));}
// njCalcVector(mtx), njCalcVector(cam.mtx) der Randnormalen; Rate nach effsub2.c:2120-2154.
static float searchlight_rate(float z){
    if(z>0){z-=0.3f;if(z<0)z=0;if(z>0.5f)z=0.5f;return z;}
    z=-z-0.3f;z*=1.6f;if(z<0)z=0;if(z>1.0f)z=1.0f;return z;
}
// effsub2.c:2014 bhEff_E11_SearchLight und :2092 bhEff_E11_SearchLightDraw. Die
// Zeichenfunktion laeuft nativ im selben Pass wie die Prozedur (ef_fnc-Liste); sie
// schreibt pt[16] in den Hilfssatz und ct2 bei Spielertreffer in mode0 2.
static bool step_e11_searchlight(EffectSprite& s,const EffectContext& c){
    using namespace effect_detail;
    switch(s.mode0){
    case 0:
        s.ct0=0;s.ct1=0;s.ct2=0;s.ax=0;s.mode0=0xFF;
        if(!s.extra&&!acquire_extra_work(s))report_gap("bhEff_E11_SearchLight: bhSetExtraEffectWork ohne freien eff[512]-Platz");
        return false;
    case 1:
        if(++s.ct0>=32)s.mode0++;
        s.ax+=int(std::int16_t(s.axp-s.ax))/16;
        s.ct1+=10;break;
    case 2:
        s.ct0=32;s.ax=s.axp;s.ct1+=s.ct2!=0?14:6;break;
    case 3:
        s.ct0-=3;if(s.ct0<=0){s.ct0=0;s.mode0=0;}
        s.ax=s.ax-(s.ax/8);s.ct1+=2;break;
    case 0xFF:return false;
    default:break; // effsub2.c:2016: kein Fall, weiter zu sx/sy/sz und ef_fnc
    }
    s.sx=effect_random();s.sy=effect_random();s.sz=effect_random();
    // --- bhEff_E11_SearchLightDraw ---
    auto* link=static_cast<RECVX_ENEMY_WORK*>(s.lkwkp);
    NJS_MATRIX mtx;
    if(!link||!link_lookup||!link_lookup(1,link->owner_index,4,mtx)){report_gap("bhEff_E11_SearchLightDraw: lkwkp->mlwP->owP[4]");return false;}
    if(!s.extra){report_gap("bhEff_E11_SearchLightDraw: op->exp0 fehlt");return false;}
    auto* pt=extra_rows<NJS_POINT3>(s,"bhEff_E11_SearchLightDraw");
    const NJS_POINT3 p1{mtx[12],mtx[13],mtx[14]};
    NJS_MATRIX cam;camera_rotation(c,cam);
    float rate[16];
    const float spread=std::tan(0.0000958738f*float(s.ax));
    for(int i=0;i<16;++i){
        NJS_POINT3 pos{100.0f*njCos(beam_angle(i))*spread,100.0f*njSin(beam_angle(i))*spread,-100.0f};
        NJS_POINT3 n{pos.x,pos.y,0};njUnitVector(&n);njCalcVector(&mtx,&n,&n);njCalcVector(&cam,&n,&n);
        rate[i]=searchlight_rate(n.z);
        if(c.sp_flg&0x8u){
            njCalcPoint(&mtx,&pos,&pt[i]);
            beam_wall(p1,pt[i],"bhEff_E11_SearchLightDraw: bhCollisionCheckLine2 ohne Wandsatz");
            if(beam_player(p1,pt[i],"bhEff_E11_CheckCollisionPlayer ohne Treffermodelle")!=-1&&s.mode0==2)s.ct2=1;
        }
    }
    std::uint32_t c0,c1;
    const unsigned apex=unsigned(150.0f+55.0f*s.sx);
    if(s.ct2!=0){c0=argb(apex,unsigned(s.ct0*3),0,0);c1=argb(0,unsigned(s.ct0*2),0,0);}
    else{c0=argb(apex,unsigned(0.8f*float(s.ct0)),unsigned(0.8f*float(s.ct0)),unsigned(s.ct0));c1=argb(0,unsigned(0.4f*float(s.ct0)),unsigned(0.4f*float(s.ct0)),unsigned(s.ct0));}
    const float edge=30.0f+15.0f*s.sx;
    for(int i=0;i<16;++i){
        const int j=i<15?i+1:0;
        const NJS_POINT3 area[3]={p1,pt[i],pt[j]};
        const std::uint32_t col[3]={c0,(c1&0xffffffu)|(unsigned(edge*rate[i])<<24),(c1&0xffffffu)|(unsigned(edge*rate[j])<<24)};
        beam_triangle(s,area,col);
    }
    // Zwei umlaufende Linien vom Ursprung zum interpolierten Randpunkt.
    const int id1=(s.ct1/16)&0xF,id2=(id1+1)&0xF;
    const float r=float(s.ct1&0xF)/16.0f;
    const auto mix=[&](int a,int b){return NJS_POINT3{(1.0f-r)*pt[a].x+r*pt[b].x,(1.0f-r)*pt[a].y+r*pt[b].y,(1.0f-r)*pt[a].z+r*pt[b].z};};
    const unsigned red=unsigned(s.ct2<<6),green=unsigned((1-s.ct2)<<6);
    const std::uint32_t l0=line_color(argb(unsigned(64.0f*s.sy),red,green,0)),l1=line_color(argb(unsigned(128.0f*s.sz),red,green,0));
    const auto reg=ninja_blend_register(8,10);
    s.lines.push_back({{p1,mix(id1,id2)},{l0,l1},reg});
    const int id3=((s.ct1/16)+8)&0xF;
    s.lines.push_back({{p1,mix(id3,(id3+1)&0xF)},{l0,l1},reg});
    s.fnc_exit_src=8;s.fnc_exit_dst=6; // njSetSystemAttr stellt den gesicherten Zustand wieder her
    effect_detail::queue_custom(s);
    return true;
}
// effsub2.c:2353 bhEff_E16_LaserSight und :2390 bhEff_E16_LaserSightDraw.
static bool step_e16_laser(EffectSprite& s,const EffectContext& c){
    using namespace effect_detail;
    auto* opp=static_cast<RECVX_ENEMY_WORK*>(s.lkwkp);
    if(!opp){report_gap("bhEff_E16_LaserSight: op->lkwkp fehlt");return false;}
    if((opp->flg!=0)^1){s.flg=0;return false;}
    switch(s.mode0){
    case 0:s.mode0++;return false;
    case 1:return false;
    default:if(opp->stflg&0x1000000u)return false;break;
    }
    NJS_MATRIX mtx;
    if(!link_lookup||!link_lookup(1,opp->owner_index,unsigned(s.lkono),mtx)){report_gap("bhEff_E16_LaserSightDraw: lkwkp->mlwP->owP[lkono]");return false;}
    const NJS_POINT3 p1{mtx[12],mtx[13],mtx[14]};
    NJS_POINT3 pos{0,0,-1500.0f};njCalcPoint(&mtx,&pos,&pos);
    if(c.sp_flg&0x8u){
        s.line_wall=beam_wall(p1,pos,"bhEff_E16_LaserSightDraw: bhCollisionCheckLine2 ohne Wandsatz");
        s.lox=pos.x;s.loy=pos.y;s.loz=pos.z;
        s.ct0=beam_player(p1,pos,"bhEff_E11_CheckCollisionPlayer ohne Treffermodelle");
        s.gpx=pos.x;s.gpy=pos.y;s.gpz=pos.z;
        const RECVX_ENEMY_VEC v{pos.x,pos.y,pos.z};recvx_en16_add_eff_pos(opp,&v);
    }else{pos={s.gpx,s.gpy,s.gpz};}
    const float dx=pos.x-p1.x,dy=pos.y-p1.y,dz=pos.z-p1.z;
    const float len=-njSqrt(dx*dx+dy*dy+dz*dz);
    NJS_POINT3 pt[16];float rate[16];
    NJS_MATRIX cam;camera_rotation(c,cam);
    for(int i=0;i<16;++i){
        NJS_POINT3 pw{0.07f*njCos(beam_angle(i)),0.07f*njSin(beam_angle(i)),len};
        njCalcPoint(&mtx,&pw,&pt[i]);
        NJS_POINT3 n{pw.x,pw.y,0};njUnitVector(&n);njCalcVector(&mtx,&n,&n);njCalcVector(&cam,&n,&n);
        if(n.z>0)rate[i]=0;
        else{float z=-n.z-0.3f;z*=1.6f;if(z<0)z=0;if(z>1.0f)z=1.0f;rate[i]=z;}
    }
    const auto reg=ninja_blend_register(8,10);
    s.lines.push_back({{p1,pos},{line_color(argb(64,64,0,0)),line_color(argb(128,64,0,0))},reg});
    if(opp->type!=0){
        const int num=recvx_en16_eff_num(opp);const RECVX_ENEMY_VEC* effp=recvx_en16_eff_pos(opp);
        for(int i=0;i<num&&effp;++i){
            const unsigned a=unsigned(40-i*4);
            s.lines.push_back({{p1,{effp[i].x,effp[i].y,effp[i].z}},{line_color(argb(a,64,0,0)),line_color(argb(a,64,0,0))},reg});
        }
    }
    for(int i=0;i<16;++i){
        const int j=i<15?i+1:0;
        const NJS_POINT3 area[3]={p1,pt[i],pt[j]};
        const std::uint32_t col[3]={argb(0,0,0,0),argb(unsigned(255.0f*rate[i]),100,0,0),argb(unsigned(255.0f*rate[j]),100,0,0)};
        beam_triangle(s,area,col);
    }
    s.fnc_exit_src=8;s.fnc_exit_dst=6;
    effect_detail::queue_custom(s);
    return true;
}

// zonzon1.c:43 BloodCol[4][2] (reine Datentabelle, gleiche Werte wie recvx_t3_blood_col).
static const std::uint32_t* blood_col(unsigned color,const char* what){
    if(color>=4){effect_detail::report_gap(what);return nullptr;}
    return recvx_t3_blood_col[color];
}
// effsub2.c:851 bhEff_E00_Blood (Effekt 296): Blutspritzer Texturgruppe 5, Bild mdlver,
// Bewegung * 0.97 und Fall 0.1, UV-Zeilen bis u < 0, BaseSize 0.125, ef_trs.
static bool step_e00_blood(EffectSprite& s){
    static const EffectUv tab0[13]={{0.3125f,0.5f,0.0625f,0.0625f},{0.375f,0.4375f,0.125f,0.125f},{0.5f,0.4375f,0.15625f,0.15625f},{0.65625f,0.4375f,0.15625f,0.15625f},{0.8125f,0.4375f,0.15625f,0.15625f},{0.0f,0.5625f,0.1875f,0.1875f},{0.1875f,0.5625f,0.1875f,0.1875f},{0.375f,0.59375f,0.1875f,0.1875f},{0.5625f,0.59375f,0.1875f,0.1875f},{0.0f,0.78125f,0.21875f,0.21875f},{0.21875f,0.78125f,0.21875f,0.21875f},{0.4375f,0.78125f,0.21875f,0.21875f},{-1,0,0,0}};
    static const EffectUv tab1[12]={{0.0f,0.25f,0.0625f,0.0625f},{0.0f,0.15625f,0.09375f,0.09375f},{0.09375f,0.15625f,0.125f,0.125f},{0.21875f,0.15625f,0.15625f,0.15625f},{0.375f,0.15625f,0.15625f,0.15625f},{0.0f,0.0f,0.15625f,0.15625f},{0.15625f,0.0f,0.15625f,0.15625f},{0.3125f,0.0f,0.15625f,0.15625f},{0.46875f,0.0f,0.15625f,0.15625f},{0.625f,0.0f,0.15625f,0.15625f},{0.78125f,0.125f,0.15625f,0.15625f},{-1,0,0,0}};
    static const EffectUv tab2[14]={{0.0f,0.3125f,0.0625f,0.0625f},{0.0f,0.5f,0.0625f,0.0625f},{0.0f,0.375f,0.0625f,0.0625f},{0.0f,0.4375f,0.0625f,0.0625f},{0.0625f,0.3125f,0.125f,0.125f},{0.1875f,0.3125f,0.125f,0.125f},{0.3125f,0.3125f,0.125f,0.125f},{0.4375f,0.3125f,0.125f,0.125f},{0.5625f,0.3125f,0.125f,0.125f},{0.6875f,0.3125f,0.125f,0.125f},{0.8125f,0.3125f,0.125f,0.125f},{0.0625f,0.4375f,0.125f,0.125f},{0.1875f,0.4375f,0.125f,0.125f},{-1,0,0,0}};
    static const EffectUv* const tables[3]={tab0,tab1,tab2};
    switch(s.mode0){
    case 0:
        s.tex_id=5;s.flg|=0x4180000u;effect_detail::quad_color(s,0xffffffffu);s.bl_src=8;s.bl_dst=6;
        s.ani_ct=s.mdlver;s.sxb=s.sx;s.syb=s.sy;s.mtn_no=int(s.type);s.frm_no=0;s.mode0++;break;
    case 1:if(s.frm_no==0){s.flg=0;return false;}break;
    default:break;
    }
    s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
    s.xn*=0.97f;s.yn*=0.97f;s.zn*=0.97f;s.yn-=0.1f;
    if(s.mtn_no<0||s.mtn_no>2){effect_detail::report_gap("bhEff_E00_Blood: type ausserhalb uvtble[3]");s.flg=0;return false;}
    const EffectUv* table=tables[s.mtn_no];const EffectUv& uv=table[s.frm_no];
    s.frm_no++;if(table[s.frm_no].u<0)s.frm_no=0;
    // effsub2.c:4702 bhEff_SetUVInfo(op, uvp, 0.125f)
    effect_detail::quad_uv(s,uv);s.sx=(s.sxb*uv.xs)/0.125f;s.sy=(s.syb*uv.ys)/0.125f;
    return true;
}
// effsub2.c:694 bhEff_E00_BloodBurst (Effekt 297): Partikelstoss am Gelenk lkwkp->owP[lkono]
// nach einer Typliste {type1, type2, size, spd, wait} (P_WK, 16 Byte, Felder per
// Disassembly 0x0023E98C..0x0023EC04: short @0/@2, float @4/@8, short @0xC).
static bool step_e00_blood_burst(EffectSprite& s,const EffectContext& c){
    struct P{int type1,type2;float size,spd;int wait;};
    static const P t0[2]={{17,1,1.5f,0.3f,4},{-1,2,0.7f,0.5f,-1}};
    static const P t1[4]={{17,1,1.5f,-0.8f,0},{9,1,1.5f,0.3f,4},{17,2,1.5f,0.3f,2},{-1,2,1.0f,0.5f,-1}};
    static const P t2[5]={{17,1,1.5f,0.1f,0},{17,1,1.5f,-0.8f,0},{9,1,1.8f,0.3f,4},{17,2,2.3f,0.3f,2},{9,1,1.7f,0.5f,-1}};
    static const P t3[2]={{9,2,1.8f,0.3f,3},{10,1,1.7f,0.5f,-1}};
    static const P t4[5]={{17,1,1.5f,0.1f,0},{-1,2,1.5f,0.8f,0},{10,2,1.3f,0.6f,6},{17,2,1.5f,0.4f,0},{9,1,1.0f,0.7f,-1}};
    static const P t5[2]={{17,1,1.5f,0.8f,0},{10,2,1.3f,0.6f,-1}};
    static const P t6[1]={{17,2,1.5f,0.8f,-1}};
    static const P* const lists[7]={t0,t1,t2,t3,t4,t5,t6};
    static const unsigned counts[7]={2,4,5,2,5,2,1};
    using namespace effect_detail;
    const auto* pp=static_cast<const RECVX_ENEMY_WORK*>(s.lkwkp);
    if(pp&&(pp->stflg&0x1000000u))return false;
    if(s.link_kind==0&&(c.player_stflg&0x1000000u))return false;
    switch(s.mode0){
    case 0:{NJS_POINT3 v{s.xn,s.yn,s.zn};njUnitVector(&v);s.xn=v.x;s.yn=v.y;s.zn=v.z;}
        s.frm_no=0;s.mode0++;
        [[fallthrough]];
    case 1:
        if(s.type>=7){report_gap("bhEff_E00_BloodBurst: type ausserhalb TypeList[7]");s.flg=0;return false;}
        if(s.link_kind<0||!s.link_matrix){report_gap("bhEff_E00_BloodBurst: lkwkp->mlwP->owP[lkono] ohne Link");s.flg=0;return false;}
        for(;;){
            if(unsigned(s.frm_no)>=counts[s.type]){report_gap("bhEff_E00_BloodBurst: frm_no hinter der Typliste");s.flg=0;return false;}
            const P& p=lists[s.type][s.frm_no];
            const NJS_POINT3 lo{s.lox,s.loy,s.loz};
            if(p.type1>=0){
                const auto* col=blood_col(s.mdlver,"bhEff_E00_BloodBurst: BloodCol[mdlver] ausserhalb");
                const NJS_POINT3 dir=p.spd>0?NJS_POINT3{s.xn,s.yn,s.zn}:NJS_POINT3{-s.xn,-s.yn,-s.zn};
                if(!col){}
                else if(!particle_mk2_hook)report_gap("bhSetEffParticleMk2 ohne Partikelbesitzer");
                else particle_mk2_hook(s.link_kind,s.link_index,unsigned(s.lkono),lo,dir,col[0],col[1],p.type1);
            }
            if(p.type2>=0){
                auto& ef=request_template();
                ef.id=296;ef.flg=1;ef.type=unsigned(p.type2);
                ef.scale={s.sx*p.size,s.sy*p.size,1.0f};ef.ax=0;ef.ay=0;ef.mdlver=s.mdlver;
                NJS_MATRIX m;std::copy(s.link_matrix,s.link_matrix+16,m);NJS_POINT3 l=lo;njCalcPoint(&m,&l,&ef.position);
                submit_template(0,nullptr);
                auto& r=last_request();r.has_velocity=true;r.velocity={s.xn*p.spd,s.yn*p.spd,s.zn*p.spd};
            }
            s.ct0=p.wait;
            if(p.wait==-1){s.flg=0;return false;}
            if(p.wait!=0)break;
            s.frm_no++;
        }
        s.frm_no++;s.mode0++;
        [[fallthrough]];
    case 2:
        if(s.ct0!=0){s.ct0--;return false;}
        s.mode0--;break;
    default:break;
    }
    return false;
}

// effsub3.c:2401 bhSetEffGunSpark: Eff308Prm[4] (reine Datentabelle).
static const EffectSpark308::Prm gun_spark_prm[4]={
    {8,0.5f,0.9800000190734863f,1.0f,5,55.0f,0.800000011920929f,4,-0.03266666829586029f},
    {10,0.6000000238418579f,0.9800000190734863f,2.0f,7,50.0f,0.800000011920929f,4,-0.03266666829586029f},
    {12,0.699999988079071f,0.9800000190734863f,3.0f,9,45.0f,0.800000011920929f,4,-0.03266666829586029f},
    {12,1.0f,0.9800000190734863f,3.0f,12,20.0f,0.800000011920929f,4,-0.03266666829586029f}};
// effsub3.c:2492 bhEff308 / :2695 bhEff308Drw: Funkenlinien mit Farbverlauf, Wind und Bodenprall.
static bool step_eff308(EffectSprite& s){
    if(!s.spark308){effect_detail::report_gap("bhEff308 ohne R08_WORK");s.flg=0;return false;}
    auto& r=*s.spark308;
    const int n=r.prm_a.vtx_num;
    if(n<0||n>16){effect_detail::report_gap("bhEff308: vtx_num ausserhalb VtxDir[16]");s.flg=0;return false;}
    if(r.mode==0){
        njUnitVector(&r.vtx_dir);
        for(int i=0;i<n;++i){
            const int ax=int(182.04445f*(r.prm_a.ang_rand*(effect_detail::effect_random()-0.5f)));
            const int ay=int(182.04445f*(r.prm_a.ang_rand*(effect_detail::effect_random()-0.5f)));
            const float sp=r.prm_a.speed+(r.prm_a.spd_rand*(effect_detail::effect_random()-0.5f));
            NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,ay);njRotateX(&m,ax);
            NJS_POINT3 d;njCalcVector(&m,&r.vtx_dir,&d);r.VtxDir[std::size_t(i)]={d.x*sp,d.y*sp,d.z*sp};
        }
        for(int i=0;i<n;++i){r.VtxBuf[std::size_t(2*i)]=r.vtx_pos;r.VtxCol[std::size_t(2*i)]=r.col_src;r.VtxCol[std::size_t(2*i+1)]=r.col_dst&0xFFFFFFu;}
        for(int i=0;i<n;++i){
            const int tim=r.prm_a.time+int(float(r.prm_a.tim_rand)*effect_detail::effect_random());
            r.TimBuf[std::size_t(i)]=tim;
            int src=int(r.col_src),dst=int(r.col_dst);auto& add=r.ColAdd[std::size_t(i)];auto& sub=r.ColSub[std::size_t(i)];
            int tmp=((dst&0xFF)-(src&0xFF))/tim;
            if(tmp<0)sub=std::uint32_t(~tmp&0xFF);else add=std::uint32_t(tmp&0xFF);
            tmp=((dst&0xFF00)-(src&0xFF00))/tim;
            if(tmp<0)sub|=std::uint32_t(~tmp&0xFF00);else add|=std::uint32_t(tmp&0xFF00);
            tmp=((dst&0xFF0000)-(src&0xFF0000))/tim;
            if(tmp<0)sub|=std::uint32_t(~tmp&0xFF0000);else add|=std::uint32_t(tmp&0xFF0000);
            src>>=16;dst>>=16;
            tmp=((dst&0xFF00)-(src&0xFF00))/tim;
            if(tmp<0)sub|=std::uint32_t(~tmp&0xFF00)<<16;else add|=std::uint32_t(tmp&0xFF00)<<16;
        }
        r.wnd_spd=0;r.wnd_acl=0.01f*effect_weather().winds;
        r.mode++;
    }
    r.drw_num=0;
    const float sp=r.wnd_spd;const int ay=effect_weather().windr;
    const NJS_POINT3 wnd{sp*-njSin(ay),0,sp*-njCos(ay)};
    r.wnd_spd=sp+r.wnd_acl;
    for(int i=0;i<n;++i){
        auto& t=r.TimBuf[std::size_t(i)];if(t<=0)continue;
        t--;r.drw_num++;
        auto& v0=r.VtxBuf[std::size_t(2*i)];auto& dir=r.VtxDir[std::size_t(i)];
        r.VtxBuf[std::size_t(2*i+1)]=v0;
        v0.x+=dir.x;v0.y+=dir.y;v0.z+=dir.z;v0.x+=wnd.x;v0.y+=wnd.y;v0.z+=wnd.z;
        dir.x*=r.prm_a.accel;dir.y*=r.prm_a.accel;dir.z*=r.prm_a.accel;dir.y+=r.prm_a.gravity;
        auto& c=r.VtxCol[std::size_t(2*i)];c+=r.ColAdd[std::size_t(i)];c-=r.ColSub[std::size_t(i)];
        if(v0.y<r.gnd_hgh){dir.y*=-0.5f;v0.y=r.gnd_hgh;}
    }
    if(r.drw_num==0){s.flg=0;return false;}
    // bhEff308Drw: njDrawLine3D je Funke mit TimBuf != 0; kein eigenes Blendpaar.
    for(int i=0;i<n;++i)if(r.TimBuf[std::size_t(i)]!=0){SceneLine l{{r.VtxBuf[std::size_t(2*i)],r.VtxBuf[std::size_t(2*i+1)]},{r.VtxCol[std::size_t(2*i)],r.VtxCol[std::size_t(2*i+1)]}};l.inherit_blend=true;s.lines.push_back(l);}
    s.fnc_exit_src=-1;s.fnc_exit_dst=-1;
    effect_detail::queue_custom(s);
    return true;
}
bool start_gun_spark(EffectSprite& s,NJS_POINT3 pos,NJS_POINT3 dir,std::uint32_t src,std::uint32_t dst,int typ){
    if(typ<0||typ>3){effect_detail::report_gap("bhSetEffGunSpark: typ_no ausserhalb Eff308Prm[4]");return false;}
    s.flg=1;s.id=308;s.spark308=std::make_shared<EffectSpark308>();auto& r=*s.spark308;
    r.prm_a=gun_spark_prm[typ];r.vtx_pos=pos;r.vtx_dir=dir;r.col_src=src;r.col_dst=dst;r.gnd_hgh=pos.y-100.0f;
    return true;
}
// effsub4.c:2149/3142/3177: EN09 throw smoke; delayed start, 255-domain UV.
static bool step_en09_smoke(EffectSprite& s){
    static const EffectUv uv[15]={{0,0,16,16},{16,0,24,24},{40,0,24,24},{64,0,32,32},
        {96,0,48,48},{144,0,48,48},{192,0,64,64},{0,64,64,64},{64,64,64,64},
        {128,64,64,64},{192,64,64,64},{0,128,64,64},{64,128,64,64},{128,128,64,64},{192,128,64,64}};
    switch(s.mode0){
    case 0:for(auto& v:s.tv)v.col=0x40808080;s.bl_src=8;s.bl_dst=10;s.flg|=0x4180000;s.stflg|=0x20;
        s.ani_ct=0;s.ct0=s.ct1=s.ct2=0;s.spd=1;s.mode1=s.type;s.mode2=0;++s.mode0;break;
    case 1:if(--s.ct3>=0)break;s.mode2=1;++s.mode0;
        [[fallthrough]];
    case 2:s.xn*=s.zn;s.px+=s.xn*-njSin(s.ay);s.pz+=s.xn*-njCos(s.ay);s.py+=s.yn;break;
    }
    if(!s.mode2)return false;
    if(s.mode1>1)throw std::runtime_error("bhEff369: anim_tbl index outside 0..1");
    const int pattern=s.ct1+(s.mode1?0:4);
    if(pattern==15){s.flg=0;return false;}
    if(pattern<0||pattern>=15)throw std::runtime_error("bhEff369: animation index outside table");
    if(++s.ct0==1){s.ct0=0;if(s.spd!=0)++s.ct1;}
    const auto& u=uv[pattern];s.sx=s.sxb*(u.xs/100);s.sy=s.syb*(u.ys/100);
    s.tv[0].u=s.tv[2].u=u.u/255;s.tv[0].v=s.tv[1].v=u.v/255;
    s.tv[1].u=s.tv[3].u=(u.u+u.xs-1)/255;s.tv[2].v=s.tv[3].v=(u.v+u.ys-1)/255;
    return s.flg!=0;
}
bool hand_ported_effect_sprite(unsigned id){
    switch(id){
    case 110:case 133: // Besitzerprozeduren in RoomWorld, wie 111/134/137.
    case 0:case 2:case 5:case 7:case 9:case 17:case 120:case 122:case 123:case 125:case 111:case 126:case 134:case 137:case 138:case 181:case 182:case 199:case 218:case 230:case 261:case 270:case 296:case 297:case 307:case 308:case 309:case 310:case 311:case 312:case 349:case 369:case 396:case 399:return true;
    // effsub4.c bhJumpEffect4 (effect.c:37), native/effect_sub3x.cpp: 357/360/361/368/398.
    case 357:case 360:case 361:case 368:case 398:return true;
    default:return hand_procedure(id)!=nullptr;
    }
}
bool supported_effect_sprite(unsigned id){return hand_ported_effect_sprite(id);}
// Raum-Texturgruppen (room.c ef_tn, Kennungen 30..99 und ab 400), die von Hand portierte
// Prozeduren ueber tex_id adressieren: effsub1 (31,33,34,36,38,39,40,43), effsub0 (50..57,
// 64,68,69,70..79,80,94,400..412,414,429..431), effsub5 (58..67,411..431), effsub6 (80).
bool ported_effect_atlas(unsigned group){
    switch(group){
    case 31:case 33:case 34:case 35:case 36:case 38:case 39:case 40:case 41:case 43:
    case 50:case 51:case 52:case 53:case 54:case 55:case 56:case 57:case 58:case 59:
    case 60:case 61:case 62:case 63:case 64:case 65:case 66:case 67:case 68:case 69:
    case 42: // screen.c:592 bhDrawThermometer (sys->ond_tnm = ef_tn[42])
    case 44: // effsub1b.c:123 bhDraw134 (sys->ef_tn[44])
    case 80:
    case 93:case 94:case 95:case 96:case 97:case 98:case 99: // effsub1.c:145 bhEffBG (tex_id = id)
    case 85: // effsub6.c:41 bhEff085 (sys->ef_tn[85], erstes Bild als Gegnertextur)
    case 81: // effsub2.c:3698 bhEff_E13_Fluid
    case 82:case 83: // effsub2.c:1061 bhEff_E02_SandDust (tex_id = mdlver + 82)
    case 408:case 409:case 410:case 411:case 412:case 413:case 414:case 415:case 416:case 418:case 420:
    case 417:case 419:case 421:case 422:case 423:case 424:case 425:case 426:case 427:case 428:
    case 429:case 430:case 431:return true;
    case 440:return true; // effsub1b.c:850 bhDraw137 (sys->ef_tn[440], Texturhalter, effect.c:788)
    case 441:return true; // effsub1b.c:1308-1543 bhDraw138 (sys->ef_tn[441] .. +3, Texturhalter RM_8001)
    case 32:return true; // bhDraw107 (Regeneinschlag)
    case 30:return true; // effsub4.c:2904 bhEff_Sub398 (tex_id 30, ohne stflg 0x20: sys->ef_tn[30]); Halter nur in RM_3130
    case 90:case 91:case 92:return true; // effsub1.c:28 bhEffFil (tex_id = op->id)
    default:return (group>=70&&group<=79)||(group>=400&&group<=407);
    }
}
bool step_effect_sprite(EffectSprite& sprite,bool* queued){
    // Die Tabellenprozeduren (effsub0/1/5/6) lesen keine Passflags; die aelteren
    // Schalterfaelle (Patronenhuelse, Flamme, Erzeuger 125) brauchen den Besitzerkontext.
    if(hand_procedure(sprite.id)||sprite.id==199||sprite.id==369||sprite.id==396||sprite.id==399||sprite.id==357||sprite.id==360||sprite.id==361||sprite.id==368||sprite.id==398){ /* effsub4.c 357/360/361/368/398 lesen keinen Kontext */const EffectContext none;return step_effect_sprite(sprite,none,queued);} /* 199 liest keinen Kontext (effsub0.c:3478) */
    if(supported_effect_sprite(sprite.id))throw std::runtime_error("effect step requires native flag context");
    if(queued)*queued=false;return false;
}
bool step_effect_sprite(EffectSprite& s,const EffectContext& c,bool* queued){
    if(queued)*queued=false;if(!supported_effect_sprite(s.id))return false;
    if(s.id==369){const bool entered=step_en09_smoke(s);if(queued)*queued=entered;return true;}
    if(s.id==308){s.custom_queued=false;s.quads.clear();s.lines.clear();const bool entered=step_eff308(s);if(queued)*queued=entered;return true;}
    // effsub3.c OR_WORK-Saetze (bhJumpEffect3[op->id-300], effect.c:831): 310/349 zeichnen nie,
    // 307/309/311/312 reihen sich selbst in ef_fnc ein (effect.c:742 leert die Listen je Pass).
    if(s.id==310){step_eff30a(s,c);if(queued)*queued=false;return true;}
    if(s.id==349){step_eff349(s);if(queued)*queued=false;return true;}
    if(s.id==309||s.id==311){s.custom_queued=false;s.quads.clear();s.lines.clear();const bool entered=s.id==309?step_eff309(s,c):step_eff30b(s,c);if(queued)*queued=entered;return true;}
    if(s.id==307||s.id==312){s.custom_queued=false;s.quads.clear();s.lines.clear();const bool entered=s.id==307?step_eff307(s,c):step_eff30c(s);if(queued)*queued=entered;return true;}
    if(s.id==296){const bool entered=step_e00_blood(s);if(queued)*queued=entered;return true;}
    if(s.id==297){step_e00_blood_burst(s,c);if(queued)*queued=false;return true;}
    if(s.id==261||s.id==270){s.custom_queued=false;s.quads.clear();s.lines.clear();const bool entered=s.id==261?step_e11_searchlight(s,c):step_e16_laser(s,c);if(queued)*queued=entered;return true;}
    if(s.id==396||s.id==399){const bool entered=step_en21_effect(s);if(queued)*queued=entered;return true;}
    if(const auto* procedure=hand_procedure(s.id)){
        // effect.c:742 bhControlEffect leert die Zeichenlisten zu Beginn jedes Passes.
        s.opaque_queued=false;s.punch_queued=false;s.custom_queued=false;s.trs2d_queued=false;s.zanzo_slot=-1;s.quads.clear();s.lines.clear();
        bool entered=false;procedure->step(s,c,entered);
        if(queued)*queued=entered||s.opaque_queued||s.punch_queued||s.custom_queued||s.trs2d_queued||s.zanzo_slot>=0;return true; /* ef_Zanzo (effect.c:938) zaehlt wie eine Zeichenliste */
    }
    if(s.id!=9){
        bool entered=false;
        switch(s.id){
        case 0:step_eff000(s,c);break;
        case 7:step_eff007(s,c);break;
        case 2:step_eff002(s,entered);break;
        case 5:step_eff005(s,entered);break;
        case 17:step_eff017(s);break;
        case 120:{ // effsub1.c:8011-8077 bhEff120: Steuerwerk, nie in einer Zeichenliste.
            s.flg|=0x1000000u;
            const auto shift=[&](int v){
                if(!world_hooks.offset_work_uv||!world_hooks.offset_work_uv(s.lkwkp,4,0,v)||
                   !world_hooks.offset_work_uv(s.lkwkp,2,0,v))
                    effect_detail::report_gap("bhEff120: lkwkp->mlwP->objP[4/2].model UV ohne Besitzer");};
            if(s.mode0==0){if(!(c.gm_flg&1u)){s.ct0=0;s.ct1=1024;shift(s.ct1);s.ct3=0;s.mode0=1;}}
            else if(s.mode0==1){
                if(c.gm_flg&1u){shift(-s.ct1);s.mode0=0;break;}
                int angle=0;
                if(!world_hooks.work_node_angle_z||!world_hooks.work_node_angle_z(s.lkwkp,11,angle)){
                    effect_detail::report_gap("bhEff120: lkwkp->mlwP->objP[11].ang[2] ohne Besitzer");break;}
                const int delta=int(std::int16_t(std::uint16_t(angle-s.azp)));
                // effsub1.c:8058 `op->ct0 = -(az / 32)` ist im Decomp-C falsch fuer negative az:
                // SLUS_201.84 bhEff120 0x233A48-0x233A60 rechnet subu, (short) per dsll32/dsra32,
                // `sra 5` (arithmetisch, rundet gegen -inf) und `negu`. Also ct0 = -floor(az/32):
                // az -1 -> +1, az -33 -> +2 (C-Division gaebe 0 bzw. +1).
                s.azp=angle;s.ct0=-(delta>=0?delta/32:-((-delta+31)/32));shift(s.ct0);s.ct1+=s.ct0;
                if(s.ct1<=0){s.ct1+=1024;shift(1024);}
            }
            break;}
        case 122:step_eff122(s,entered);break;
        case 123:step_eff123(s,entered);break;
        case 125:step_eff125(s,c);break;
        case 126:step_eff126(s,entered);break;
        case 181:step_eff181(s);break;
        case 182:step_eff182(s,entered);break;
        case 199:step_eff199(s,c,entered);break; // effsub0.c:3478 (native/effect_eff199.cpp)
        case 357:step_eff357(s,c,entered);break; // effsub4.c:1235 (native/effect_sub3x.cpp)
        case 360:step_eff360(s,c,entered);break; // effsub4.c:1241
        case 361:step_eff361(s,c,entered);break; // effsub4.c:1335
        case 368:step_eff368(s,c,entered);break; // effsub4.c:2075
        case 398:step_eff398(s,c,entered);break; // effsub4.c:2722
        case 218:step_eff218(s,entered);break;
        case 230:step_eff230(s,entered);break;
        default:return false;
        }
        if(queued)*queued=entered;
        return true;
    }
    if(!s.mode0){
        s.flg|=0x4100000;s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;
        s.flame_compact_uv=s.type>=1&&s.type<=3;s.tex_id=s.flame_compact_uv?8:9;
        for(unsigned i=0;i<4;++i){auto& v=s.tv[i];v.x=(i&1)?1.f:-1.f;v.y=(i&2)?0.f:-2.f;v.z=0;v.col=0xffe0e0e0;}
        s.mode0=1;
    }
    if(s.type==3&&!(c.st_flg&0x20000000u)){s.flg=0;return true;}
    if(s.type==2){
        if(!(c.player_flg&0x1000000u)){s.flg=0;return true;}
        if(!(c.pt_flg&1u)||(c.player_stflg&0x1000000u)||(c.player_mdflg&1u))return true;
        s.flg&=~0x1000000u;
    }
    if(s.ct0==8)s.ct0=0;
    if(s.ct0<0||s.ct0>7)throw std::runtime_error("flame UV cursor outside native bank");
    const float edge=s.flame_compact_uv?.0625f:.25f;
    const float u=float(s.flame_compact_uv?s.ct0:s.ct0%4)*edge;
    const float v=s.flame_compact_uv?.8125f:float(s.ct0/4)*edge;
    for(unsigned i=0;i<4;++i){s.tv[i].u=u+((i&1)?edge:0);s.tv[i].v=v+((i&2)?edge:0);}
    ++s.ct0;if(queued)*queued=true;return true;
}
void step_light_color(unsigned type,unsigned speed,unsigned& clock,NJS_POINT3 color,NJS_POINT3& working,bool running){
    if(!running)return;
    working=color;if(type<1||type>6)return;
    const unsigned shape=(type-1)%3;const float wave=njSin(clock)*(shape==0?.25f:shape==1?.5f:1.f);
    if(shape==2)working={wave*color.x,wave*color.y,wave*color.z};
    else {const float base=shape==0?.75f:.5f;working={wave*color.x+color.x*base,wave*color.y+color.y*base,wave*color.z+color.z*base};}
    unsigned delta=speed<<8;
    if(type>=4)delta=unsigned(float(delta)*(-recvx_core_random_next()/-2.1474836e9f));
    clock=(clock+delta)&0x7fff;
}
bool step_light_type(unsigned type,int speed,NJS_POINT3 color,unsigned thunder,LightTypeWork& w,NJS_POINT3& working_color,NJS_VECTOR& working_direction){
    // -rand() / -2.1474836e9f: Ganzzahl negiert, cvt.s.w, div.s (SLUS_201.84 0x281988-0x2819ac).
    const auto random=[](){return -recvx_core_random_next()/-2.1474836e9f;};
    switch(type){
    case 7:{ // :486-492 r = g = b = |njSin(sys->thunder << 6)|
        const float flash=std::fabs(njSin(Angle(thunder<<6)));working_color={flash,flash,flash};return true;}
    case 8:case 9:{ // :493-510 nur der Schritt ist & 0xFFFF, way selbst laeuft ungekuerzt (0x28188c-0x281898)
        const unsigned step=unsigned(int(182.04445f*(0.5f*float(speed))))&0xffffu;
        const unsigned yaw=unsigned(w.angles[1]);w.angles[1]=std::int32_t(type==8?yaw-step:yaw+step);
        // bhGetLightVector (:1067-1077): Einheitsmatrix, Z, Y, X, Punkt (0, 0, -1).
        NJS_MATRIX m;njUnitMatrix(&m);njRotateZ(&m,w.angles[2]);njRotateY(&m,w.angles[1]);njRotateX(&m,w.angles[0]);
        NJS_POINT3 forward{0,0,-1};njCalcPoint(&m,&forward,&working_direction);working_color=color;return true;}
    case 10:case 11:{ // :511-625; die Farbe ist spc/dif/amb nach dem Schritt
        auto& c=w.flicker;
        const auto dim=[&](){c.x+=0.5f*((0.333f*color.x)-c.x);c.y+=0.5f*((0.333f*color.y)-c.y);c.z+=0.5f*((0.333f*color.z)-c.z);};
        // (float)aspd * 0.01 ist double (dpmul 0x3F847AE147AE147B, _dpfle), der Zufall float.
        const auto chance=[&](){return double(random())<=double(float(speed))*0.01;};
        const auto countdown=[&](){w.clock=unsigned(int(6.0f*random())+3);};
        if(w.mode==0){c=color;++w.mode;}
        else if(w.mode==1){
            if(type==10)c=color;else dim();
            if(chance()){if(type==11)c=color;countdown();++w.mode;}
        }else if(w.mode==2||(type==10&&w.mode==3)){
            dim();w.clock=unsigned(int(w.clock)-1);
            if(int(w.clock)<=0){c=color;if(type==10&&w.mode==2){countdown();++w.mode;}else w.mode=1;}
        }
        working_color=c;return true;}
    case 12:{ // :626-657 Halblicht im mode-1-Pass selbst, ct0 = (int)(aspd * Zufall) + 1
        const float gain=w.mode==1?0.5f:1.0f;working_color={color.x*gain,color.y*gain,color.z*gain};
        if(w.mode<=1){w.clock=unsigned(int(w.clock)-1);
            if(int(w.clock)<=0){w.clock=unsigned(int(float(speed)*random())+1);w.mode=w.mode==0?1u:0u;}}
        return true;}
    }
    return false;
}
void set_effect_walls(const std::vector<CollisionWall>* walls,float height){effect_wall_set=walls;effect_wall_height=height;}
void set_effect_water(std::function<bool(NJS_POINT3,float&)> probe){effect_water_probe=std::move(probe);}
void set_effect_water_area(std::function<bool(NJS_POINT3,float&,float&)> probe){effect_water_area_probe=std::move(probe);}
void set_effect_ground(GroundCollision* ground,const std::vector<CollisionWall>* room,const std::vector<CollisionWall>* mobile){
    // bhGetGroundPosition der Raumwerke; rom->flrp/sys->mflrp fuer bhCheckFloorEffect.
    effect_ground=ground;effect_floor_room=room;effect_floor_mobile=mobile;
}
void set_effect_extra_allocator(std::function<int()> reserve,std::function<void(int)> release){extra_reserve=std::move(reserve);extra_release=std::move(release);}
void set_effect_link_lookup(std::function<bool(unsigned,unsigned,unsigned,NJS_MATRIX&)> lookup){link_lookup=std::move(lookup);}
void set_effect_player_hit(std::function<int(NJS_POINT3,NJS_POINT3&)> lookup){player_hit_lookup=std::move(lookup);}
void set_effect_world_hooks(EffectWorldHooks hooks){world_hooks=std::move(hooks);}
void set_effect_particle_mk2(std::function<int(int,unsigned,unsigned,NJS_POINT3,NJS_POINT3,unsigned,unsigned,int)> hook){particle_mk2_hook=std::move(hook);}
void set_effect_work_angle(std::function<bool(unsigned,unsigned,int&)> lookup){work_angle_lookup=std::move(lookup);}
void set_effect_pool_lookup(std::function<EffectSprite*(int)> lookup){pool_lookup=std::move(lookup);}
void set_effect_afterimage_services(EffectAfterimageServices services){afterimage_services=std::move(services);}
void set_effect_link_matrix(unsigned node,const float* matrix){
    // Die Knotenmatrix hat ohne Effektprozeduren keinen Abnehmer.
    (void)node;(void)matrix;
}
// effsub1.c:4220 bhEff017 — siehe effect_sprites.h.
void set_effect_mobile_wall_clear(std::function<void(int)> clear){mobile_wall_clear=std::move(clear);}

// weapon.c:1706 bhSetExplosion — der Sprengsatz eines Fasses (eneset.c bhEne00) und
// eines Wandtreffers (weapon.c:560 bhCheckGunAtari): Ton 534, Blitzlicht rom->lgtp,
// Zuender bhEff016 (Kennung 16; ruft nach lkono Passes bhCheckBombAtari = Flaechenschaden)
// sowie Feuerball 23 und zwei Truemmer 24. `view` ist cam.vx/vy/vz des Vorbildes.
void set_explosion(NJS_POINT3 position,NJS_POINT3 view){
    pending.sounds.push_back(534); // CallPlayerActionSe(534,1): Bank 2, Liste 534&255 (sdfunc.c:1537 Type 3)
    OriginalLight light{};
    light.flg=3;light.type=101;light.aspd=8;light.lkflg=0;light.lkno=0;light.lkono=0;light.lsrc=4;
    light.position=position;light.offset={0,0,0};light.direction={0,0,0};light.color={4.5f,3.0f,1.5f};
    light.near_range=30.0f;light.far_range=200.0f;
    pending.light=light;
    effect_detail::request_direct(16,position,16); // bhSetEffect(16,&pnt,NULL,16), pnt-ox/oy/oz = 0
    auto& r=effect_detail::request_template(); // sys->ef der Originalerzeuger
    r.id=23;r.flg=1;r.type=1;r.mdlver=1;r.flr_no=0;r.scale={6.0f,6.0f,6.0f};
    r.position={position.x-4.0f*view.x,position.y-4.0f*view.y,position.z-4.0f*view.z};
    r.ax=0;r.ay=0;
    effect_detail::submit_template(0);
    for(int i=0;i<2;++i){ // zwei Truemmer 24; Startversatz aus (0,0,-2) um (ax,ay,0) gedreht
        r.id=24;r.flg=1;r.type=0;r.mdlver=1;r.flr_no=i&3;
        r.scale.x=1.0f+effect_detail::effect_random();r.scale.y=r.scale.x;r.scale.z=0.0f;
        r.ay=int(182.04445f*(360.0f*effect_detail::effect_random()));
        r.ax=int(182.04445f*(80.0f*effect_detail::effect_random())); // Reihenfolge sx,ay,ax wie im Original
        NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,Angle(r.ax),Angle(r.ay),0);
        NJS_VECTOR vc0{0,0,-2.0f},vc1;njCalcVector(&m,&vc0,&vc1); // vc1 = bhSetEffectTb-Versatz
        const NJS_POINT3 offset{vc1.x,vc1.y,vc1.z};
        effect_detail::submit_template(0,&offset);
    }
}
// weapon.c:1812 bhSetExplosionEffect — dieselbe Feuerbalfolge ohne Ton/Licht/Zuender,
// z. B. gameover.c:539 und event.c:8766.
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view){
    auto& r=effect_detail::request_template();
    r.id=23;r.flg=1;r.type=1;r.mdlver=1;r.flr_no=0;r.scale={6.0f,6.0f,6.0f};
    r.position={position.x-4.0f*view.x,position.y-4.0f*view.y,position.z-4.0f*view.z};
    r.ax=0;r.ay=0;
    effect_detail::submit_template(0);
    for(int i=0;i<2;++i){
        r.id=24;r.flg=1;r.type=0;r.mdlver=1;r.flr_no=i&3;
        r.scale.x=1.0f+effect_detail::effect_random();r.scale.y=r.scale.x;r.scale.z=0.0f;
        r.ay=int(182.04445f*(360.0f*effect_detail::effect_random()));
        r.ax=int(182.04445f*(80.0f*effect_detail::effect_random()));
        NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,Angle(r.ax),Angle(r.ay),0);
        NJS_VECTOR vc0{0,0,-2.0f},vc1;njCalcVector(&m,&vc0,&vc1);
        const NJS_POINT3 offset{vc1.x,vc1.y,vc1.z};
        effect_detail::submit_template(0,&offset);
    }
}
EffectOutput take_effect_output(){EffectOutput out=std::move(pending);pending=EffectOutput{};return out;}
// bhSetEffectTb: copy flg/id/type/mdlver/flr_no, position, scale (also kept in sxb..), angles and the joint offset.
void initialize_effect_sprite(EffectSprite& s,const EffectSpawn& e){
    s=EffectSprite{};s.flg=e.flg;s.id=e.id;s.type=e.type;s.tex_id=-1;s.mdlver=e.mdlver;s.flr_no=e.flr_no;s.px=e.position.x;s.py=e.position.y;s.pz=e.position.z;
    s.sx=s.sxb=e.scale.x;s.sy=s.syb=e.scale.y;s.sz=s.szb=e.scale.z;s.ax=e.ax;s.ay=e.ay;s.az=0;s.lkono=e.lkono;
    s.lkwkp=e.linked_work;
    if(e.has_offset){s.lox=e.offset.x;s.loy=e.offset.y;s.loz=e.offset.z;}
    // Nachtraege der Originalerzeuger am neuen eff[n] (siehe EffectSpawn).
    if(e.has_velocity){s.xn=e.velocity.x;s.yn=e.velocity.y;s.zn=e.velocity.z;}
    if(e.tex_id_override>=0)s.tex_id=e.tex_id_override;
    s.az=e.az;s.parent_slot=e.parent_slot;s.mode1=unsigned(e.mode1);
    if(e.has_color)s.tv[0].col=e.color;
    if(e.has_ao){s.aox=e.ao.x;s.aoy=e.ao.y;s.aoz=e.ao.z;}
    if(e.has_exp1)s.exp1_slot=e.exp1_slot;
    s.stflg|=e.stflg_or;s.owner_work=e.owner_work;
    if(e.has_shp_ct)s.shp_ct=e.shp_ct;
    s.ct3=e.ct3;
    if(e.has_scale_base){s.sxb=e.scale_base.x;s.syb=e.scale_base.y;s.szb=e.scale_base.z;}
}
// effect.c:bhSetEffectTb scans the shared eff[512] pool and accepts the first
// slot whose flg has neither allocation bit set. Missing tail entries are zeroed.
int original_effect_slot(const std::vector<unsigned>& flags){for(unsigned i=0;i<512;++i)if(i>=flags.size()||!(flags[i]&3u))return int(i);return -1;}
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,std::pair<unsigned,unsigned>>& bases,std::pair<unsigned,unsigned>* fnc_blend,bool mirror){
    // Host-Fehlergrenze: alle Texturverweise vor dem ersten Szenenschreibvorgang
    // pruefen. So kann der Besitzer genau diesen Zeicheneintrag auslassen,
    // ohne halbe Geometrie zu behalten oder die Simulation zu veraendern.
    for(const auto& q:sprite.quads){
        if(!q.textured)continue;
        const auto base=bases.find(q.category);
        const std::string detail="effect="+std::to_string(sprite.id)+", category="+std::to_string(q.category)+", frame="+std::to_string(q.frame);
        if(base==bases.end())throw EffectTextureError("original custom-effect texture missing: "+detail);
        if(q.frame>=base->second.second)throw EffectTextureError("original custom-effect texture frame outside its atlas: "+detail+", count="+std::to_string(base->second.second));
    }
    // Vor der ef_fnc-Schleife haben alle Zeichenpaesse 8/6 zurueckgestellt (effect.c:1268/1416).
    const std::pair<unsigned,unsigned> entry=fnc_blend?*fnc_blend:std::pair<unsigned,unsigned>{8,6};
    // DrawEff5SnowRect (effsub5.c:5804): Wind unter cam.mtx, einmal je Aufruf.
    bool wind_ready=false;Angle wind_z=0;
    for(const auto& q0:sprite.quads){
        EffectSprite::Quad q=q0;
        if(q.inherit_blend){q.source=entry.first;q.destination=entry.second;}
        if(q.wind_aligned){
            if(!wind_ready){
                NJS_MATRIX view;camera_view_matrix(camera,view);
                if(mirror){NJS_MATRIX m;std::copy_n(scene.mirror_matrix.data(),16,m);njMultiMatrix(&view,&m);} // cam.mtx im Spiegelpass
                NJS_POINT3 v=q.wind;njCalcVector(&view,&v,&v);
                wind_z=Angle(int(10430.381f*std::atan2(v.y,v.x))+16384);wind_ready=true;
            }
            // njUnitRotPortion, njRotateZ(aAngleZ), [njRotateY]: die Billboardlage des
            // Hosts ist UnitRotPortion; die beiden Drehungen gehen in die Eckpunkte.
            NJS_MATRIX turn;njUnitMatrix(&turn);njRotateZ(&turn,wind_z);if(q.turn_y)njRotateY(&turn,q.turn_y);
            for(auto& v:q.vertices){NJS_POINT3 p{v.x,v.y,v.z};njCalcVector(&turn,&p,&p);v.x=p.x;v.y=p.y;v.z=p.z;}
        }
        if(q.screen_size){
            // njPtclDrawPolygon: GS-Sprite scr.x/y +- h im 640x480-Ninja-Bild. Als Billboard am
            // Mittelpunkt der Kameratiefe z abgebildet: der Renderer projiziert fSx = AspectW*x*dist/z,
            // fSy = y*dist/z (renderer.cpp:115-119), also h Punkte = h*z/(AspectW*dist) bzw. h*z/dist.
            NJS_MATRIX view;camera_view_matrix(camera,view);if(mirror){NJS_MATRIX m;std::copy_n(scene.mirror_matrix.data(),16,m);njMultiMatrix(&view,&m);}NJS_POINT3 c;njCalcPoint(&view,&q.center,&c);
            constexpr float aspect_w=1.174f; // SoftwareRenderer::draw aspect_x
            const float dist=320.0f/std::tan(0.0000958738f*float(camera.perspective/2));
            const float kx=c.z/(aspect_w*dist),ky=c.z/dist;
            for(auto& v:q.vertices){v.x*=kx;v.y*=ky;}
        }
        unsigned texture=0;
        if(q.textured){
            const auto base=bases.find(q.category);if(base==bases.end())throw std::runtime_error("original custom-effect texture missing");
            if(q.frame>=base->second.second)throw std::runtime_error("original custom-effect texture frame outside its atlas: effect="+std::to_string(sprite.id)+", category="+std::to_string(q.category)+", frame="+std::to_string(q.frame)+", count="+std::to_string(base->second.second));
            texture=base->second.first+q.frame;
        }
        EffectSprite quad;quad.id=302;quad.flg=q.world_space?0u:0x100000u;quad.tv=q.vertices;quad.bl_src=int(q.source);quad.bl_dst=int(q.destination);
        quad.fog_disabled=!q.fogged;quad.point_filter=q.point_filter;quad.untextured=!q.textured;quad.pn=q.pn;quad.gouraud=q.gouraud;quad.fog_color_set=q.fog_color_set;quad.fog_color=q.fog_color;
        append_effect_sprite(scene,quad,q.world_space?NJS_POINT3{0,0,0}:q.center,{1,1,1},camera,texture,nullptr,nullptr,mirror,q.mirror_plane_turn);
    }
    if(fnc_blend&&sprite.fnc_exit_src>=0&&sprite.fnc_exit_dst>=0)*fnc_blend={unsigned(sprite.fnc_exit_src),unsigned(sprite.fnc_exit_dst)};
}
// effsub0.c:10 setentry / effect.c:436 bhSetEffectTb: die Anforderung dieses
// Passes; der Besitzer leert die Liste mit take_effect_output().
void push_effect_spawn(const EffectSpawn& spawn){pending.spawns.push_back(spawn);}
// effect.c:1374 (bhDrawTrsEffect3D), :1241 (Ntx), :1515 (Pnc), :1651 (Opq):
// Rueckseitentor flg 0x20000. njCalcPoint der ersten drei Eckpunkte fuellt pa/pb/pc
// (davon bleibt nur z stehen), njProjectScreen ueberschreibt x/y mit dem
// Bildschirmpunkt, njGetPlaneNormal2 (ps2_NaColi.c:314) nimmt die Normale und
// njUnitVector normiert sie; vec.z < 0 heisst Rueckseite. Die Winkelweite kommt
// wie in camera.c:28 aus njSetPerspective(cam.pers) vor dem Zeichenpass.
bool backface_culled(const std::array<SceneVertex,4>& world,const SceneCamera& camera,const Scene* mirror_scene){
    njSetPerspective(camera.perspective);
    NJS_MATRIX view;camera_view_matrix(camera,view);
    if(mirror_scene){if(!mirror_scene->mirror_enabled)throw std::runtime_error("mirror effect backface test without njMirror matrix");
        NJS_MATRIX m;std::copy_n(mirror_scene->mirror_matrix.data(),16,m);njMultiMatrix(&view,&m);}
    NJS_POINT3 p[3];
    for(unsigned i=0;i<3;++i){
        NJS_POINT3 point=world[i].position;
        njCalcPoint(&view,&point,&p[i]);
        NJS_POINT2 screen;njProjectScreen(&view,&point,&screen);
        p[i].x=screen.x;p[i].y=screen.y;
    }
    NJS_VECTOR vec;njGetPlaneNormal2(&p[0],&p[1],&p[2],&vec);njUnitVector(&vec);
    return vec.z<0;
}
bool mirror_billboard_prefix(const Scene& scene,NJS_MATRIX& out){
    if(!scene.mirror_enabled)return false;
    std::copy_n(scene.mirror_matrix.data(),16,out);njUnitTransPortion(&out);
    return njInvertMatrix(&out)!=0;
}
void append_effect_sprite(Scene& scene,const EffectSprite& s,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture,const float* linked_matrix,unsigned* stflg_out,bool mirror,bool mirror_plane_turn){
    if((s.flg|s.stflg)&0x1000000)return;
    // effect.c:1181/1301/1444/1581: im Spiegelpass (gm_flg 0x4000) faellt ein Satz mit mdflg 0x40 aus.
    if(mirror&&(s.mdflg&0x40u))return;
    // bhDrawTrsEffect3D: translation, UnitRotPortion for flag 0x100000,
    // Z rotation, scale. Invert the room view rotation to place the billboard.
    NJS_MATRIX basis;njUnitMatrix(&basis);
    if(s.flg&0x100000){
        // njUnitRotPortion unter cam.mtx; im Spiegelpass ist cam.mtx = cam.mtxb * njMirror (game.c:81-83),
        // deshalb steht dessen invertierter Drehanteil links vor der ungespiegelten Basis.
        if(mirror&&!mirror_billboard_prefix(scene,basis))throw std::runtime_error("mirror effect billboard without njMirror matrix");
        njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);njRotateZ(&basis,s.az);
        // effect.c:1225-1236 (und :1360/1501/1634, effsub2.c:4644): nach njRotateZ(az) unter gm_flg 0x4000
        // njRotateY(32768) bei mr_pl.vx != 0, njRotateX(32768) bei mr_pl.vy != 0 (Float-Test != 0).
        if(mirror&&mirror_plane_turn){const NJS_PLANE& plane=effect_mirror_plane();if(plane.vx!=0.0f)njRotateY(&basis,32768);if(plane.vy!=0.0f)njRotateX(&basis,32768);}
    }
    else {if((s.flg&0x80)&&!(s.flg&0x200000)){if(!linked_matrix)throw std::runtime_error("linked effect draw missing native node matrix");std::copy(linked_matrix,linked_matrix+16,basis);}njRotateXYZ(&basis,s.ax,s.ay,s.az);}
    if(s.pn!=3&&s.pn!=4)throw std::runtime_error("original effect draw with a vertex count outside 3/4");
    std::array<SceneVertex,4> vertices;
    for(unsigned i=0;i<4;++i){NJS_POINT3 point{s.tv[i].x*scale.x,s.tv[i].y*scale.y,s.tv[i].z*scale.z};njCalcVector(&basis,&point,&point);point.x+=position.x;point.y+=position.y;point.z+=position.z;vertices[i]={point,s.tv[i].u,s.tv[i].v,{}};
        // Eigene Eckfarben (njDrawPolygon3DEx): die Flaechenfarbe traegt das Alpha, die
        // Eckfaktoren den RGB-Anteil; verschiedene Alphas tragen die Eckpunkte selbst.
        if(s.gouraud)vertices[i].light={float((s.tv[i].col>>16)&255u)/255.f,float((s.tv[i].col>>8)&255u)/255.f,float(s.tv[i].col&255u)/255.f};}
    // GS-Gouraud interpoliert auch Alpha je Eckpunkt (njDrawPolygon3D mit eigenen
    // Eckfarben, effsub2.c SearchLight/LaserSight): dann traegt jeder Eckpunkt sein Alpha.
    bool vertex_alpha=false;
    if(s.gouraud)for(unsigned i=1;i<s.pn&&i<4;++i)if((s.tv[i].col&0xff000000u)!=(s.tv[0].col&0xff000000u))vertex_alpha=true;
    if(vertex_alpha)for(unsigned i=0;i<4;++i)vertices[i].alpha=float(s.tv[i].col>>24)/255.f;
    // Rueckseitentor: der Originalzeichenpfad setzt stflg 0x10 und zeichnet nicht,
    // sonst loescht er das Bit. Es wird auch ohne Zeichnung gebraucht
    // (screen.c:943 liest es am Rendertextur-Satz).
    if(s.flg&0x20000u){
        const bool culled=backface_culled(vertices,camera,mirror?&scene:nullptr);
        if(stflg_out){if(culled)*stflg_out|=0x10u;else *stflg_out&=~0x10u;}
        if(culled)return;
    }
    // effect.c:bhDrawTrsEffect3D selects this op's pair with
    // njColorBlendingMode(0, op->bl_src)/(1, op->bl_dst) and restores 8/6 after
    // the draw; the primitive draws with the register that pair produced.
    // A sprite whose original procedure the port has not bound yet carries no
    // pair (the additive approximation stays, see color_blending_unavailable and
    // the ledger), so it is not drawn with a substituted register.
    const bool original_pair=s.id==5||s.id==23||s.id==302||supported_effect_sprite(s.id);
    // sys->ef_opq (bhDrawOpqEffect3D, effect.c:1435): festes Paar 8/6 vor der Schleife, Tiefenschreiben.
    if(original_pair){color_blending_mode(0,s.opaque_queued?8:s.bl_src);color_blending_mode(1,s.opaque_queued?6:s.bl_dst);}
    const auto register_=original_pair?color_blending_alpha():0ull;
    const std::array<std::array<unsigned,3>,2> strip{{{0,1,2},{1,3,2}}};
    for(unsigned t=0;t<(s.pn==3?1u:2u);++t){const auto indices=strip[t];SceneTriangle triangle;triangle.mirror=mirror?3:0;for(unsigned i=0;i<3;++i)triangle.vertices[i]=vertices[indices[i]];triangle.color=vertex_alpha?0xffffffffu:s.gouraud?((s.tv[0].col&0xff000000u)|0xffffffu):s.tv[0].col;triangle.gouraud=s.gouraud;triangle.vertex_alpha=vertex_alpha;triangle.texture=texture;triangle.textured=true;triangle.camera_mask=false;triangle.world_lit=true;triangle.depth_write=s.opaque_queued||s.punch_queued;triangle.fogged=!s.fog_disabled;if(s.untextured)triangle.textured=false;
        triangle.fog_color_set=s.fog_color_set;triangle.fog_color=s.fog_color;
        if(original_pair){triangle.blend=SceneBlend::Ninja;triangle.blend_register=register_;}else triangle.blend=SceneBlend::Additive;
        // sys->ef_pnc: ohne ABE gilt das ALPHA-Register nicht (GS gibt Cs aus), sofort statt OT.
        if(s.punch_queued){triangle.blend=SceneBlend::Opaque;triangle.punch_through=true;}
        if(s.id==9)triangle.texture_filter=(s.flg&0x20000000u)?SceneTextureFilter::Point:SceneTextureFilter::Linear;
        if(s.point_filter)triangle.texture_filter=SceneTextureFilter::Point;
        scene.triangles.push_back(triangle);}
    if(original_pair){color_blending_mode(0,8);color_blending_mode(1,6);}
}
}
