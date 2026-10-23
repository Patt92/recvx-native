#pragma once
#include "player_push.h"
#include "actor.h"
#include "audio.h"
#include "events.h"
#include "renderer.h"
#include "collision.h"
#include "effect_collision.h"
#include "geometry_motion.h"
#include "effect_sprites.h"
#include "hit_model.h"
#include "enemy_spawn.h"
#include "subplayer.h"
#include "en01_damage.h"
#include "enemy_work.h"
#include "enemy_dispatch.h"
#include "en03_collision.h"
#include "effect_particles.h"
#include "weapon_flash.h"
#include "player_combat_core.h"
#include "weapon_fly.h"
#include "object_itmbox.h"
#include "player_head.h"
#include "player_action.h"
#include "view_clip.h"
#include <deque>
#include <set>
#include <string>
#include <map>
#include <tuple>
#include <memory>
#include <optional>
#include <functional>
namespace recvx {
struct PlayerGroundWork {NJS_POINT3 ground{};NJS_POINT3 water_c1{},water_c2{};float water_radius=0;std::optional<GroundHit> hit;};
// O_WRK von bhObj005 (objitm.c:1213, bhJumpObject[5] Wasserflaeche): mode0, ct0..ct3, xn/zn,
// exp1 (der Original-vlist: base/base_normals, in dessen x das Original je Eckpunkt den
// Zeiger auf seine Kartenzelle ablegt: cell) und exp2 (die gezeichnete Kopie: vertices/
// normals). uv_* ist die Summe der npSetOffsetUV2-Aufrufe auf objP[0].model->plist.
struct WaterWaveWork {unsigned mode0=0;int ct0=0,ct1=0,ct2=0,ct3=0;float xn=0,zn=0;unsigned chunk_type=0;
    std::vector<NJS_POINT3> base,vertices;std::vector<NJS_VECTOR> base_normals,normals;std::vector<int> cell;
    bool orphan=false; // objitm.c:1230: zweites Wasserobjekt ohne eigene exp1/exp2
    bool uv_ok=false;std::size_t uv_chunk=0;int uv_u=0,uv_v=0;
    bool neighbour_gap=false,range_gap=false;};
struct WorldEntity {
    std::vector<FaceParameter> face_keys;unsigned face_frame=0,face_flags=0;
    // face.c:321 bhSetLip only stores the selected stream in exp0[268];
    // face.c:141 bhControlMask copies it into LIP_WORK.top/ptr while
    // lip->flag == 0. A running stream therefore keeps its own source.
    std::optional<FaceParameter> facial;std::vector<std::uint8_t> lip_stream,lip_current_stream;unsigned lip_bank=0,lip_pos=0,lip_count=0,lip_time=0,lip_cur=0,lip_next=0;bool lip_active=false;
bool subplayer_bound=false,subplayer_no_motion=false;SceneSpawn transform{};std::array<int,3> angles{};unsigned identity=0,type=0,death_ticks=0,object_flags=0,object_parameter=0;int object_floor=0;
    // O_WRK.hide[0..3] (objitm.c:200-203 bhSetObject, :262-265 bhSetItem): die
    // Schnittnummern, in denen ein Werk mit flg 0x40000000 gezeichnet wird.
    // 255 steht in den Originaldaten fuer "kein Schnitt".
    std::array<std::uint8_t,4> object_hide{{255,255,255,255}};
    // O_WRK fields of a room object (kind 2) that enemy routines read and write
    // (en04.c:1100 door branch, :3955 wire shake): flg2, mode1, mode3, ct0, ayp.
    // O_WRK yn und gpx/gpy/gpz (objitm.c:1860 Ausgangslage) fuer bhEne56/bhEff134.
    float object_yn=0;NJS_POINT3 object_origin{};
    // objitm.c:1846 bhObj008: O_WRK mode0, ct0 (Platz in sys->mwalp, hier object_mobile_walls_)
    // und owP[0].mtx aus dem bhCalcModel des vorigen bhControlObjItm-Durchlaufs (objitm.c:410).
    unsigned object008_mode0=0;int object008_wall=-1;bool object008_owp_valid=false;BoneMatrix object008_owp{};
    // objitm.c:1648 bhObj007 (bhJumpObject[7]): der Greif-und-Dreh-Gegenstand. mode0 0/1
    // Annaeherung, 2/3 Drehen, 4 fertig; ct0 ist der Annaeherungszaehler, spd der gehaltene
    // Abstand. Nur RM_0150 Objekt 5 (die Eisenstatue) ist in allen Raeumen vom Typ 7.
    unsigned object007_mode0=0;int object007_ct0=0;float object007_spd=0;
    unsigned object_flg2=0,object_mode1=0,object_mode3=0;int object_ct0=0,object_ayp=0;int hp=0;bool dead=false,combat_enabled=true;bool hidden=false,inactive=false,frozen=false,scripted=false,has_motion=false,room_motion=true;unsigned motion=0;std::int64_t frame=0,step=65536;bool linked=false,link_released=false;unsigned parent_kind=0,parent_index=0,parent_node=0;NJS_POINT3 link_offset{};std::map<unsigned,bool> hidden_nodes;ModelNodeOverrides node_overrides;std::shared_ptr<RECVX_EN01> en01;unsigned upper_motion=0;bool flinch=false;std::array<int,2> flinch_angles{};int neck_angle=0;
    // bhEneLookFlgSet (event.c): original BH_PWORK flg bit 0x8000. Enemy AI
    // consumers (en01.c) are still pending; the state itself is exact.
    bool look=false;
    // Common_controll 41 (event.c:10346): original BH_PWORK flg bit 0x40000.
    // Its readers are family-specific (en01.c:10498, en05.c:4877, en09.c:7574,
    // en12.c:1601, en17.c:681, en26.c:1270) and the player foot-fix gate
    // (player.c:1539, ported in player_stairs.c:918); there is no generic
    // model-link reader in MdlPut.c/objitm.c. The bit is stored exactly here;
    // the per-family consumer is the open contract (see work report).
    bool model_link=false;
    // bhSetMotion samples frm_no before advancing it. Event conditions read
    // the advanced clock; rendering and joint controls use the sampled pose.
    std::optional<std::int64_t> sampled_frame;
    unsigned motion_mode3=0; // O_WRK: only bhSetObj/ItmMotion advances mode 1.
    bool motion_slow=false; // Common 24/25/34 mtn_md bit 8, separate from MTN flags.
    // Skriptpfad: der vor dem Vorschub abgetastete Frame. Nativer Gegnerwork:
    // bhSetMotion (Motion.c:21) tastet objP bei frm_no ab und schiebt danach vor;
    // frame ist die vorgeschobene Uhr, sampled_frame der gezeichnete Frame.
    std::int64_t pose_frame()const{return (scripted||enemy_native)&&sampled_frame?*sampled_frame:frame;}
    // objP of the original work. motion/room_motion/frame/has_motion above are
    // mtn_no/frm_no, the state the control pass will sample next;
    // bhSetMotion/bhCalcModel write the object tree, and they only run while
    // that pass is enabled. Everything that draws or reads a pose therefore
    // uses the committed values, so a work written while the pass is paused
    // (sp_flg bit 0x2 clear, e.g. the special-event computer) leaves the
    // drawn tree alone until the pass returns.
    bool pose_set=false,pose_room=true,pose_motion_valid=false;
    unsigned pose_mtn=0;std::int64_t pose_frm=0;
    void commit_pose(){pose_set=true;pose_mtn=motion;pose_room=room_motion;pose_motion_valid=has_motion;pose_frm=pose_frame();}
    unsigned posed_motion()const{return pose_set?pose_mtn:motion;}
    bool posed_room_motion()const{return pose_set?pose_room:room_motion;}
    bool posed_has_motion()const{return pose_set?pose_motion_valid:has_motion;}
    std::int64_t posed_frame()const{return pose_set?pose_frm:pose_frame();}
    // O_WRK.aspd (int, from ETTY_WORK short): objects select ob_spc with it, items
    // glint with light 1 of intensity 0.1*aspd (objitm.c:417/571).
    int aspd=0;
    // Item glint state of bhControlObjItm (objitm.c:417-450): ct0/ct1/ct2, ayp and mdflg 0x80.
    int item_ct0=0,item_ct1=0,item_ct2=0,item_ayp=0;bool item_glint=false;
    // bhControlObjItm list of a room object (objitm.c:364-383): 0 none, 1 ob_nlg, 2 ob_hlg, 3 ob_spc,
    // with its position in ob_spc (bhDrawObjItm skips the first entry in five rooms).
    unsigned object_list=0,object_spc_index=0;int draw_tp=0; // O_WRK.draw_tp = ETTY prm1 (bhSetObject/bhSetItem)
    // bhSetAlphaFadeObject (objitm.c:721): flg 0x200000 with pv[0] jntno/jnt_n and pv[1] alpha/count;
    // npSetAllMatAlphaColor rewrites the node materials for good (node -> diffuse alpha byte).
    bool alpha_fade=false;int alpha_jntno=0,alpha_jnt_n=0;float alpha_target=0,alpha_count=0;std::map<unsigned,unsigned> node_alpha;
    // bhObj005: nur Objekte vom Typ 5 (objitm.c:361 bhJumpObject[op->type]).
    std::shared_ptr<WaterWaveWork> water;
    // bhCyoutenHenkei family: mdflg bit 2 plus shp_ct (0..1000) and, for
    // bhCyoutenHenkeiSetEX, the original mbp model indices obj_a/obj_b.
    // Vertex interpolation/rendering is pending; the shape clock is exact.
    bool morph=false;float morph_shape=0;unsigned morph_obj_a=0,morph_obj_b=0;
    // objP[node].evalflags of the realized models beyond the Work's own
    // (mbp[0], whose live array is enemy_evalflags). The blend reads the live
    // array of both sources, not the immutable asset's.
    std::vector<std::vector<std::uint32_t>> morph_model_flags;
    const std::vector<std::uint32_t>* morph_live_flags(unsigned model)const{
        if(own_row&&model==own_model)return model?&enemy_evalflags:nullptr; // mdl[own_model] ist mlwP
        if(own_row&&!model){const auto at=own_models.find(0);return at!=own_models.end()&&!at->second.evalflags.empty()?&at->second.evalflags:nullptr;}
        return model&&model<=morph_model_flags.size()&&!morph_model_flags[model-1].empty()?&morph_model_flags[model-1]:nullptr;
    }
    // C02: the original BH_PWORK of a room enemy (eneset.c:170 bhSetEnemy).
    // It lives here; RoomWorld's enemy table only points at it. enemy_native:
    // bhJumpEnemy[id] has a native routine, so this work (not the fields
    // above) is the authority during the enemy pass (enemy_work.h, B).
    RECVX_ENEMY_WORK enemy{};bool enemy_bound=false,enemy_native=false;const PlayerAsset* enemy_asset=nullptr; // mlwP source (models_ is fixed after load)
    // mlwP of that work: objP pos/ang (bhSetMotion), objP evalflags, owP
    // flg/mtx (bhCalcTree). enemy_calculated: bhCalcModel has run once.
    std::vector<ModelPose> enemy_obj;std::vector<std::uint32_t> enemy_evalflags,enemy_owp_flg;std::vector<BoneMatrix> enemy_owp_mtx;bool enemy_calculated=false;
    // `epw->mlwP = &epw->mdl[k]` der eigenen Reihe (select_model): own_row ist mdl[0], own_model
    // das aktive k; die Felder oben sind dann der Zustand von mdl[k]. Nicht aktive mdl[k]
    // behalten objP-Pose und owP-flg/mtx in own_models; ihre objP-evalflags liegen fuer k > 0 in
    // morph_model_flags[k-1], fuer k == 0 in own_models[0].
    struct OwnModel {std::vector<ModelPose> obj;std::vector<std::uint32_t> evalflags,owp_flg;std::vector<BoneMatrix> owp_mtx;bool calculated=false;};
    const PlayerAsset* own_row=nullptr;unsigned own_model=0;std::map<unsigned,OwnModel> own_models;
    const PlayerAsset* model_row()const{return own_row?own_row:enemy_asset;}
    // bhPutModel (eneset.c:843): unter mdflg 0x2 zeichnet es obj_a, sonst mlwP->objP; das ist nach
    // select_model mdl[own_model], ohne Wechsel die Wurzel aus morph_obj_a (select_root, en18).
    unsigned drawn_root()const{const bool blending=morph||(enemy_bound&&(enemy.mdflg&2u));return !blending&&own_row&&own_model?own_model:morph_obj_a;}
    // Verknuepfte Kindworks (bhEne01_SetLinkEnemy, en01.c:2815). model_owner:
    // `epp->mlwP = epw->mdl` — das Kind liest und schreibt den Objektbaum des
    // Elternworks, es hat keinen eigenen. model_window: objP/owP += 8 und
    // obj_num -= 8 am geteilten ML_WORK (en01.c:2799/10347/3148); der Wert gehoert
    // deshalb dem Baumbesitzer. motion_library: `epp->mnwP = epw->mnwP`
    // (en01.c:2829), die Identitaet, deren emtp das Kind abspielt; -1 = eigene.
    // room.c:559 bhSetEneMdl ohne MDL-Block mit Objekten (mdl_n 0) bzw. ein mdl[slot] hinter
    // mdl_n: das ML_WORK bleibt genullt (obj_num 0); bhCalcTree/Draw laufen leer, kein Fehler.
    bool model_absent=false;
    WorldEntity* model_owner=nullptr;unsigned model_window=0;int motion_library=-1;
    // epw->mnwP = (MN_WORK*)sys->plmthp (en27.c:61): die Motionbibliothek des Spielers.
    bool player_motion_library=false;
    // `epp->mlwP = &epw->mdl[base]` (en13.c:111-184): mdl[k] dieses Works ist
    // mdl[base+k] des Elternworks; model_bank ist dessen Modellreihe, nullptr = eigene.
    const PlayerAsset* model_bank=nullptr;unsigned model_base=0;
    // Beschreibbarer Vertexstrom des gezeichneten mlwP->objP (bhEne13_ScaleModel
    // schreibt p[0]/p[1] an Ort und Stelle). Leer = unveraenderte Modelldaten.
    mutable std::vector<std::uint8_t> live_model;
    mutable std::map<unsigned,std::vector<std::uint8_t>> live_models;
    const std::vector<std::uint8_t>* live_stream(unsigned model)const{
        if(!model)return live_model.empty()?nullptr:&live_model;
        const auto at=live_models.find(model);return at==live_models.end()?nullptr:&at->second;
    }
    // npSetAllMatColor(mlwP->objP, obj_num, color) (njplus.c:1375) patcht in
    // JEDEM Objekt des Baums die Materialchunks 17..23: B/G/R werden immer
    // geschrieben, A nur, wenn der Modellwert nicht 0 ist. Der Wert gehoert
    // wie der Baum dem model_owner.
    bool enemy_material_set=false;std::uint32_t enemy_material_color=0;
    // Entity values as the last publish left them; a difference before the
    // next pass is a script write that recvx_enemy_adopt takes over.
    // pp->dan_ap (hitchk.c:5050 bhSetDansaLimitAtari): last step ETC this
    // enemy work stood on, as its place in rom->etcp + sys->metcp.
    std::optional<std::size_t> enemy_dansa_step;
    struct EnemyMirror {NJS_POINT3 position{};std::array<int,3> angles{};int yaw=0,floor=0;unsigned motion=0;bool room_motion=false;std::int64_t frame=0,step=0;int hp=0;bool hidden=false,inactive=false,look=false;} enemy_mirror{};};
// RoomWorld::append: normaler Zeichenpass oder das Monitorbild von screen.c:920 (siehe dort).
struct WorldDrawPass{bool small_screen=false;unsigned ncut=0;};
class RoomWorld {
    friend struct LightEffectLinkTestAccess; // gezielte Work-/Publikationsfixture, keine Runtime-Alternative
    friend struct EnemyJointMappingTestAccess; // nur effect_player_hit am echten Raum, keine Runtime-Alternative
public:
    RoomWorld(const std::vector<std::uint8_t>& room,const Scene& stage,SceneSpawn& player,std::size_t& motion,std::int64_t& frame,AudioMixer& audio,const std::filesystem::path& data={},const std::filesystem::path& mods={},const std::string& room_name="RM_0000",unsigned game_mode=0);
    struct CombatTarget {unsigned index,identity;NJS_POINT3 position;float radius,height;int hp;};
    std::vector<CombatTarget> combat_targets()const;
    void combat_difficulty(unsigned mode);
    void sound_monitor();
    // weapon.c bhCheckEnemies/bhSetGunFire: assigns one hit (joint damage, hit position/direction) to the en01 work.
    bool damage(unsigned index,int weapon,int amount,unsigned comb_flg,NJS_POINT3 position,NJS_POINT3 direction,int hit_joint=-1);
    // Original en01 routines the damage/death port called while they are still unbound (reported, never hidden).
    const std::set<std::string>& unbound_enemy_routines()const{return unbound_enemy_;}
    // Host-Warnungen fuer isoliert ausgelassene Zeicheneintraege, keine Simulationsluecken.
    const std::set<std::string>& render_warnings()const{return render_warnings_;}
    // effect.c:268/effsub3.c AllocOworkOne: naechster freier eff[512]-Platz ab `start`, genullt und mit
    // flg 2 vorgemerkt (-1 = voll). Die Erzeuger (bhSetRapEff, bhSetEffectTb) schreiben flg 1 selbst.
    int reserve_effect_slot(unsigned start=0);
    // effect.c:869 bhControlEffect fuehrt mit bhEff349 (effsub3.c:3234) Gegnercode aus:
    // en19.c:250 traegt bhEne19_TyBloodSet als fnc_prcP ein, und die Routine ruft
    // bhGetGroundPosition, mlwP->objP[18] und bhSetEffectTb auf. Im Original ist das
    // derselbe sys/rom/eff-Satz wie im Gegnerpass; nativ sind der Weltzeiger und die
    // Gegnerdienste nur waehrend bhControlEnemy gebunden. Dieser Rahmen stellt beide
    // fuer den Effektpass her und setzt sie danach genau auf ihren vorigen Stand zurueck.
    // Implementierung in native/world_enemies.cpp, wo diese Bindungen liegen.
    class EnemyCallbackFrame {
    public:
        explicit EnemyCallbackFrame(RoomWorld* world);
        ~EnemyCallbackFrame();
        EnemyCallbackFrame(const EnemyCallbackFrame&)=delete;
        EnemyCallbackFrame& operator=(const EnemyCallbackFrame&)=delete;
    private:
        RoomWorld* previous_world_;
        // Der vorige Stand von recvx_en19_bound(); als void*, damit world.h die
        // Dienststruktur des Gegnermoduls nicht kennen muss.
        const void* previous_en19_;
        // bhEff_Sub356 (effsub4.c:1189) ruft EN01-Code: T3-Ergaenzung und EN01-Kontext wie im Gegnerpass.
        const void* previous_ext_;const void* previous_en01_;std::shared_ptr<void> en01_ctx_;
    };
    std::size_t blood_particles()const{return particles_.active();}
    // event.c:4302 bhEffBloodSet: epw->djnt_no und bhEne_SetBlood2 am Gegnerwork.
    bool script_blood_set(unsigned index,unsigned joint,int type,unsigned flags,int turn,NJS_POINT3 offset);
    // event.c:4110 bhEffectSandSet (0x98): bhEne02_SetSandEffect(ene[enep[index].wrk_no], &pos, type);
    // `relative` ist v2 != 0, dann kommen x und z zur Gegnerlage (y bleibt der reine Versatz).
    bool script_sand_effect(unsigned index,NJS_POINT3 offset,bool relative,int type);
    // event.c:8577 bhEffectSandSetMatsumoto (0xC3): bhEne02_SetSandEffectEV(ene[work], type1, type2).
    bool script_sand_effect_ev(unsigned work,int type1,int type2);
    // Die EN02-Dienste gelten nur waehrend des Gegnerpasses (control_enemy_works bindet und
    // loest sie wieder). Der Skriptbefehl merkt seinen Auftrag deshalb vor und der Gegnerpass
    // desselben Bildes fuehrt ihn aus, bevor die Gegner laufen — dieselbe Reihenfolge wie im
    // Original (game.c: Eventtask vor bhControlEnemy vor bhControlEffect). Lage und Typ sind
    // schon im Skriptpass bestimmt, also aus demselben Stand wie dort.
    struct SandRequest {RECVX_ENEMY_WORK* work=nullptr;NJS_POINT3 pos{};int type=0,type2=0;bool ev=false;};
    std::vector<SandRequest> pending_sand_;
    void run_pending_sand();
    bool enemy_dead(unsigned index)const;
    // bhObjDposCk (event.c:7605): obwp[index].ay und objP[physical].ang[0] eines Raumgegners.
    bool object_ay(unsigned index,int& ay)const;
    void set_object_ay(unsigned index,int ay);
    bool enemy_node_ax(unsigned enemy,unsigned physical,int& ax)const;
    void set_enemy_node_ax(unsigned enemy,unsigned physical,int ax);
    struct JointHit {int joint;NJS_POINT3 point;};
    std::optional<JointHit> enemy_capsule_hit(unsigned index,NJS_POINT3 start,NJS_POINT3 end,float radius);
    void set_enemy_health(unsigned index,int hp);
    // bhEneSetCk: original BH_PWORK stflg 0x1000000 (not drawn, no hit/sight checks).
    void deactivate_enemy(unsigned index);
    void deactivate_item(unsigned index); // bhItmCk/bhItmSetCk
    bool step_clear(NJS_POINT3 probe,float radius=3.5f,float height=16.5f)const;
    // Native bhCheckWallType / bhCollisionCheckLine/2/3 over the existing
    // room-then-mobile geometry. These are not bhCheckWall movement or
    // bhCheckL2Wall (which has a different capsule/inset contract).
    std::optional<CollisionWall> collision_contact(NJS_POINT3 probe,unsigned flags,float radius,float height)const;
    std::optional<CollisionWall> collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal)const;
    std::optional<CollisionWall> collision_line(NJS_POINT3 start,NJS_POINT3& end,NJS_POINT3& normal,unsigned flags,int floor,bool omit_attribute4=false)const;
    float shot_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range)const{return wall_ray_distance(collision_geometry(),room_height_,origin,direction,range,floor_at(origin));}
    float combat_wall_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,unsigned flags=0x400)const{return wall_ray_distance(collision_geometry(),room_height_,origin,direction,range,-32768,flags);}
    // pwksub.c:718 bhCheckL2Wall(lp, flg, len) over rom->walp + sys->mwalp (collision_geometry
    // order): the line runs from `origin` along `vector` (not normalised). Returns the hit
    // count != 0, *len = nr, *apos = sys->apos and, on a hit, *atr = *sys->ahtp and *index
    // its position in collision_geometry() (-1 = NULL).
    int l2_wall(NJS_POINT3 origin,NJS_POINT3 vector,unsigned flg,float* len,NJS_POINT3* apos,CollisionWall* atr=nullptr,int* index=nullptr)const;
    // pwksub.c:2172 bhCheckC2WallN(cap, flg) over rom->walp + sys->mwalp (collision_geometry
    // order): the capsule with its own radius (0.1 at every weapon/aim caller) against all
    // records types 0..7, attr 0x2000000 skipped with flg 0x40000000, odd types with 0x400.
    // Not combat_wall_distance: that ray ignores cap->r and the type 6/7 records.
    int c2_wall_n(const RECVX_CB_CAPSULE& cap,unsigned flg)const;
    // bhGetGroundPosition for the original attack path; same collision set as movement.
    float combat_ground(NJS_POINT3 probe);
    std::optional<GroundHit> ground_hit()const{return ground_.hit();}
    const PlayerGroundWork* player_ground_work()const{return player_ground_ready_?&player_ground_:nullptr;}
    void player_stair_ground_state(unsigned status,int mode0){stair_ground_state_=std::make_pair(status,mode0);}
    void clear_player_stair_ground_state(){stair_ground_state_.reset();}
    // sys->ef_tlist ab sys->ef_tn[group]: die Bilder einer geladenen Effektgruppe (nullptr = nicht geladen).
    const std::vector<SceneTexture>* effect_group_textures(unsigned group)const{const auto at=effect_textures_.find(group);return at==effect_textures_.end()?nullptr:&at->second;}
    const std::vector<SceneTexture>* weapon_flash_textures()const{const auto at=effect_textures_.find(3);return at==effect_textures_.end()?nullptr:&at->second;}
    bool bind(const EventCommand& command);
    void set_simulation_flags(unsigned flags){simulation_flags_=flags;}
    const WorldEntity* state(unsigned kind,unsigned index)const;
    // bhGameOverSet: the room script requested the death sequence (original
    // gov_md0 driver of gameover.c is pending).
    bool game_over_requested()const{return game_over_;}
    // bhCyoutenHenkei family on the player work (bhCyoutenHenkeiSetEX type 0).
    bool player_enemy_motion()const{return player_enemy_motion_;}
    // en07.c:2190/2221: plp->mnwP is the controlled work's library while a grab
    // owns the player, and plp->mtn_no then indexes that library. The host frame
    // count must follow the same source as the sampled clip.
    unsigned player_motion_frames()const;
    bool player_morph()const{return player_morph_.morph;}float player_morph_shape()const{return player_morph_.morph_shape;}
    const EffectSprite* effect_state(unsigned index)const{return index<sprites_.size()?&sprites_[index].sprite:nullptr;}
    // mlwP->texP-Liste eines Raumwerks, wie der Zeichenpass sie verwendet (bhEff085 tauscht textures[3]).
    const std::vector<SceneTexture>* model_textures(unsigned kind,unsigned index)const{for(const auto& m:models_)if(m.kind==kind&&m.index==index)return &m.object.textures;return nullptr;}
    bool effect_visible(unsigned index)const;
    EffectContext effect_context()const;
    void set_presentation_flags(unsigned flags){presentation_flags_=flags;} // sys->pt_flg, native draw owner
    unsigned presentation_flags()const{return presentation_flags_;}
    // game.c:196-224 / :115-139 (bhCheckExmAtari, cb_flg 0x8): exm_attr 0x2 also drops
    // rom->lgtp[1].flg 0x2; neither restore path sets it again.
    void clear_exm_light(){if(lights_.size()>1)lights_[1].flags&=~2u;}
    // hitchk.c:4819: cam.flg 0x20 (cut.c bhSetCut/bhSetFixedCut take it from the cut's
    // flg 0x400; bhSetEventCamera/bhSetEventFixedCut clear it) arms ef_flg 0x10.
    void mark_exm_cut_effect(){if(!event_camera&&(cut_fog_flags_&0x400u))effect_flags_|=0x10u;}
    void set_fixed_cut(unsigned cno);                       // cut.c:448 bhSetFixedCut
    void set_event_camera(unsigned cno,unsigned kno,bool lock); // cut.c:1644 / :1726
    // cut.c:12 bhCheckCut(1): bhSetCut(cam.ncut, 1) for the player's current cut.
    void return_to_cut(); // cut.c:12-64 bhCheckCut(1): Augenzweig (gm_flg 0x40) oder erzwungener bhSetCut
    // cut.c:2472-2763 Augenkamera: cam.flg 0x4 (bhControlPlEyeCamera nach dem Abschalten) und der
    // Augenpfad unter gm_flg 0x40 (bhSetPlEyeCamera). Ein Kamerabesitzer neben Raum-/Eventkamera.
    struct EyeCamera{bool active=false,returning=false;unsigned mode0=0,ct0=0;NJS_POINT3 position{},target{};
        std::int32_t ax=0,ay=0,az=0,axp=0,ayp=0,azp=0,pers=11832,ppers=11832;float ln=0,pln=0;};
    int eye_pe_ax_=0,eye_pe_pers_=11832; // cam.pe_ax/pe_pers (bhCPM0_action, act_suw/rld, bhSyukanModeSet)
    void set_eye_request(int pe_ax,int pe_pers){eye_pe_ax_=pe_ax;eye_pe_pers_=pe_pers;}
    int eye_pe_ax()const{return eye_pe_ax_;}
    int eye_pe_pers()const{return eye_pe_pers_;}
    bool eye_camera_active()const{return eye_.active;} // Diagnose: cut.c Augenkamera (gm_flg 0x40) laeuft
    void set_eye_ppers(int ppers){eye_.ppers=ppers;}           // cam.ppers (playpch2.c act_scp)
    void clear_eye_ax(){eye_.ax=eye_.axp=0;}                   // cam.ax = cam.axp = 0 (playpch2.c:57)
    void set_player_wax(int wax){player_wax_=std::int16_t(wax);}
    void set_player_aim_mode(unsigned mode1,unsigned at_flg){player_aim_mode1_=mode1;player_aim_at_flg_=at_flg;}
    // sys->ef_flg. system.c:195 initializes it to 0; its only original writer
    // is the mn_md1 room-load sequence (system.c:1830/1884), which no native
    // owner drives yet, so bit 0 stays clear until one binds it.
    void set_effect_flags(unsigned flags){effect_flags_=flags;}
    unsigned effect_flags()const{return effect_flags_;}
    struct LightState {unsigned flags,type,clock,link,index,node;NJS_POINT3 position,color;float near_range,far_range;
        NJS_POINT3 direction;unsigned mode;std::array<std::int32_t,3> working_angles;NJS_POINT3 flicker_color;unsigned speed;};
    std::optional<LightState> light_state(unsigned index,bool event=false)const;
    ModelPose model_pose(unsigned kind,unsigned index,unsigned script_node=0)const;
    // sp_evt.c bhDrawSpEvtComCharacter: the special-event computer draws
    // rom->enep[chr_no] into its character window. The original writes the
    // script's mtn_no/frm_no and a zero position/angle into that enemy work
    // and draws the object tree with njCnkEasyDraw (white light from
    // (4,6,10), diffuse 1.0, ambient 0.2). Returns nothing when the room has
    // no such enemy or its motion library lacks the clip; the caller reports
    // that as a gap instead of drawing a substitute.
    std::optional<Scene> computer_character(unsigned chr_no,unsigned motion,unsigned frame_no);
    void set_player_asset(const PlayerAsset* player){for(auto it=afterimage_models_.begin();it!=afterimage_models_.end();)if(std::get<0>(it->first)==0)it=afterimage_models_.erase(it);else ++it;player_asset_=player;if(player){player_obj_=player->bind_pose();player_owp_flags_.assign(player_obj_.size(),0);}}
    void set_player_character(unsigned character){player_character_=int(character&3);}
    // C02: transient enemy view of the existing player. Runtime binds its
    // Inventory HP and EXP1 head flags by address; neither is copied here.
    void bind_enemy_player(int& hp,std::uint32_t& head_flags){player_hp_owner_=&hp;player_head_flags_=&head_flags;}
    // plp->exp1 head work (player_head.h): Player_controll 129/130/142/150 write its flags and nod script.
    void bind_player_head(RECVX_HEAD_STATE& head){player_head_=&head;}
    bool enemy_player_get(RECVX_ENEMY_PLAYER& out)const;
    bool enemy_player_set(const RECVX_ENEMY_PLAYER& in);
    unsigned enemy_player_motion_frames(unsigned motion,bool from_enemy)const;
    // sys->ene / sys->ewk_n: die eine Gegnertabelle des Raumlaufs. Der
    // Kampfpass liest sie ueber den Dienst `enemies` (pwksub.c:244
    // bhSearchNearEnemy, playpch.c:318 bhSearchEnemy, weapon.c:614
    // bhCheckGunAtari); sie haelt nur Zeiger auf die vorhandenen Entities und
    // wird vom Gegnerpass desselben Besitzers gefuellt.
    RECVX_ENEMY_TABLE& enemy_table(){return enemy_table_;}
    void commit_player_joints(const std::vector<BoneMatrix>& nodes){player_joint_matrices_=nodes;}
    // game.c:357-360: der mit stflg 0x1000000 verborgene Spieler wird gezeichnet, solange pl_sleep_cnt laeuft; je Zeichnung -1.
    bool player_sleep_draw(){if(!pl_sleep_cnt_)return false;--pl_sleep_cnt_;return true;}
    // owP des zuletzt berechneten Spielerbaums (das bhCalcModel(plp) der vorigen Runde).
    // Genau dieses owP liest das Original in bhSetGunFire/bhControlEffect; eine Abfrage
    // darf keinen neuen Motionzustand daraus samplen.
    const std::vector<BoneMatrix>& player_committed_joints()const{return player_joint_matrices_;}
    // Publish objP and physical owP from the same control pass. Afterimage capture must
    // not infer this pose from the next motion frame. Additional mdl[n] need their own work.
    void commit_afterimage_model(unsigned kind,unsigned index,unsigned mdl,const PlayerAsset& asset,const std::vector<ModelPose>& pose,const std::vector<BoneMatrix>& nodes,const std::vector<std::uint32_t>& flags);
    // bhCalcModel(plp) (player.c:1517) runs on mlwP->objP after the mode routine
    // and bhControlPlayerHead, so the owP it commits carries the script node
    // writes and the crate pose as well.  The owner of the committed matrices
    // uses this to sample exactly that object tree.
    ModelNodeOverrides player_pose_overrides()const{auto edits=overrides(0,0);if(push_pose_current())for(const auto& v:push_->pose())edits[v.first]=v.second;return player_pose_edits(edits);}
    bool enemy_player_joint(unsigned joint,BoneMatrix& out)const;
    std::vector<ModelPose> player_local_pose()const;
    // Motion.c:21 bhSetMotion (player.c:1502), Knotenteil: SetMtn*(Hokan) auf plp->mlwP->objP
    // mit dem Stand VOR Vorschub und hokan_count-Abzug; der Takt bleibt beim Pfad.
    void player_set_motion_pose(std::size_t mtn_no,std::int32_t frm_no,unsigned mode,unsigned hokan_count,int hokan_rate);
    // plp->mlwP->objP[Skriptknoten] nach dem Knotenteil DIESES Passes (bhCalcFixOffset, Motion.c:868).
    bool player_obj_node(unsigned script_node,float pos[3],int ang[3])const{
        if(!player_asset_||!player_obj_written_)return false;
        try{const auto i=player_asset_->model_index(script_node);if(i>=player_obj_.size())return false;
            const auto& o=player_obj_[i];pos[0]=o.position.x;pos[1]=o.position.y;pos[2]=o.position.z;
            ang[0]=int(o.angles[0]);ang[1]=int(o.angles[1]);ang[2]=int(o.angles[2]);return true;}
        catch(const std::exception&){return false;}}
    // Beginn eines Spielerpasses: lief im vorigen kein Knotenteil, folgt objP der dargestellten
    // Clip-Pose, damit eine spaetere Mischung vom gezeigten Stand ausgeht.
    void begin_player_motion_pass();
    // Bearbeitungen der Spielerpose (Kampf/Schieben/Skript) ueber objP gelegt, solange es die
    // Darstellung traegt; sonst unveraendert.
    ModelNodeOverrides player_pose_edits(const ModelNodeOverrides& edits={})const;
    // plp->mlwP->objP als Knotenvorgabe fuer einen bhCalcModel(plp)-Verbraucher ausserhalb des
    // Spielerpasses (gameover.c:453): der ganze Baum, unabhaengig von motion_ (der Spielerpass
    // prueft die Motion ueber player_pose_edits). Leer, solange kein Knotenteil objP geschrieben hat.
    ModelNodeOverrides player_obj_overrides()const{ModelNodeOverrides out;if(!player_obj_drives_||!player_asset_||player_obj_.size()!=player_asset_->bones())return out;for(unsigned i=0;i<player_obj_.size();++i)out[i]={player_obj_[i].position,player_obj_[i].angles};return out;}
    void set_pad_clear_callback(std::function<void(uint32_t)> cb){pad_clear_cb_=std::move(cb);}
    void clear_pad_on(uint32_t bits){if(pad_clear_cb_)pad_clear_cb_(bits);}
    void set_effect_counter(unsigned yk_ct){effect_yk_ct_=yk_ct;}
    unsigned effect_counter()const{return effect_yk_ct_;} // sys->yk_ct
    int spawn_player_effect(const EffectSpawn& effect){return spawn_effect(effect,1,0);}
    // bhSetEffectTb(eft, &ops, NULL, jno) aus dem Spielerkampf: derselbe Pool, ohne Link.
    int spawn_unlinked_effect(const EffectSpawn& effect){return spawn_effect(effect,0,0);}
    // bhSetEffectTb for bhSetGunFire requests (bhEff003); link_kind 1 = player, 2 = enemy.
    int spawn_weapon_flash(const RECVX_CB_EFFECT& effect,unsigned link_kind,unsigned link_index);
    void set_gun_light(const RECVX_CB_LIGHT& light); // rom->lgtp[0] from bhSetGunFire
    // hitchk.c bhCheckExmAtari, type 4 with attr 0x2: obwp[object] with flg 1
    // becomes type 100 (mode0..mode3 cleared). Returns false when flg 1 is clear.
    bool start_item_box(unsigned object);
    // bhControlObject for every type-100 room object: bhJumpObject[100] = bhObjItmBox.
    void control_item_boxes(unsigned& cb_flg,unsigned& player_stflg);
    // objitm.c:1213 bhObj005 fuer jedes Typ-5-Objekt (Wasserflaeche) im bhControlObjItm-Durchlauf:
    // legt sys->wt_* an (water_wave()), setzt sys->st_flg 0x40, verschiebt je Pass die
    // Eckpunkte/Normalen der gezeichneten Kopie und laesst die Karte abklingen.
    void control_water_objects();
    // objitm.c:1846 bhObj008 (bhJumpObject[8]): ATR-Quader in sys->mwalp, folgt owP[0] des Objekts.
    void control_mobile_wall_objects();
    void control_uv_objects(); // objitm.c:2050 bhObj010, dispatch type 10
    // objitm.c:1648 bhObj007 (bhJumpObject[7]): der Greif-und-Dreh-Gegenstand im
    // bhControlObjItm-Durchlauf (nur RM_0150 Objekt 5, Typ 7).
    void control_turn_objects();
    void control_turn_object(class WorldEntity& e);
    bool offset_work_uv(void* work,unsigned node,int u,int v);
    bool work_node_angle_z(void* work,unsigned node,int& angle)const;
    const WaterWaveWork* water_work(unsigned object)const;
    // bhSetExplosion: the effect works, flash light and sound; `view` is the camera direction (cam.vx/vy/vz).
    void explode(NJS_POINT3 position,NJS_POINT3 view);
    // weapon.c:1812 bhSetExplosionEffect(pos) an diesem Besitzer: Feuerball 23 und zwei Truemmer 24
    // aus sys->ef, Lage pos - 4 * cam.v mit dem cam.v der gerade gueltigen Kamera (camera.c:62-71
    // bhMakeCameraVector; unter cam.flg 0x40 die direkt geschriebene Lage). Aufrufer: die
    // Todesszene gameover.c:539 (gov_etb) und event.c:8766. false, wenn ein Satz nicht angelegt
    // werden konnte (Prozedur unportiert oder eff[512] voll); die Luecke steht dann in
    // unbound_enemy_routines().
    bool explode_effect(NJS_POINT3 position);
    // gameover.c:313-327: cam.flg = 0x40 und cam.pers/ln/ax..az/px..pz direkt geschrieben; dieselbe
    // Kameraform wie bhEff137 (cut.c:138-147, bhControlMonitorCamera ist leer). cut = cam.ncut.
    void set_monitor_camera(unsigned cut,NJS_POINT3 position,int ax,int ay,int az,float ln,int pers);
    // effect.c:153 bhClearEffect: eff[512] genullt (gameover.c:278 beim Eintritt der Todesszene; room.c:37
    // deckt der Raumwechsel ab). Nur die O_WRK-Saetze; Partikelnutzlasten ausserhalb von eff[] bleiben wie
    // im Original stehen und werden ohne ihren Wurzelsatz nicht mehr fortgeschrieben.
    void clear_effects();
    // event.c:8801 bhEffClearEvt (Skriptbefehl 0xca, 2 Bytes) -> effect.c:159 bhClearEventEffect: drei
    // Durchlaeufe ueber eff[512], je nur Saetze mit flg & 1 nach id. bhClrEff_YT (effect.c:166): 5/11/113/
    // 114/136/159/225/398 flg = 0; 22/24/25/27 flg = 0 und *(int*)exp0 = 0 (Hilfssatz aus
    // bhSetExtraEffectWork, falls gesetzt). bhClrEff_RY (effsub3.c:117): 301 flg = 0; 302 zusaetzlich die
    // Hilfsplaetze exp0/exp1. bhClrEff_YG (effsub2.c:4793): 250/253/256/257/272/296/297/298 flg = 0;
    // 254/260 flg = 0 und *(int*)exp0 = 0. Alles andere (Raumeffekte, 23, Blutlachen 300) bleibt.
    void clear_event_effects();
    // Blast damage dealt to the player by bhCheckBombAtari since the last call (hp points).
    int take_blast_damage(){const auto value=blast_damage_;blast_damage_=0;return value;}
    // Zombie AI (debug menu). Off keeps idle zombies standing; damage reactions still run.
    void set_enemy_ai(bool enabled){enemy_ai_=enabled;}
    bool enemy_ai()const{return enemy_ai_;}
    // The hp the host shows is clamped at 0; a negative plp->hp (the original death condition) stays until the host revives the player.
    void set_player_poisoned(bool poisoned){player_poisoned_=poisoned;}
    void set_player_hp(int hp){if(player_killed_&&hp<=0)return;if(hp>0){player_killed_=false;player_dead_=false;}player_hp_=hp;}
    int player_health()const{return player_hp_owner_?*player_hp_owner_:player_hp_;}
    // Zombie grab/bite (needs the AI). Default off; the debug menu switches it.
    void set_enemy_grab(bool enabled){enemy_grab_=enabled;}
    bool enemy_grab()const{return enemy_grab_;}
    // Host-Komfortschalter "Gegnerangriffe" (debug.ini enemy_attacks, --enemy-attacks, Debugmenue), keine Originalmechanik.
    // Aus: Gegner und Gegnereffekte senken kein HP, setzen kein Gift (stflg 0x80000/0x200000), toeten nicht und uebernehmen
    // den Spieler nicht (mode0 4/5/6). Die Gegner-KI laeuft weiter. Standard an = Originalverhalten.
    void set_enemy_attacks(bool enabled){enemy_attacks_=enabled;}
    bool enemy_attacks()const{return enemy_attacks_;}
    // sys->pad_ps while grabbed: newly pressed direction bits (0xf) and buttons (0x2d00); bhEne_LeverCheck shakes the zombie off.
    void set_player_lever(unsigned pad_ps){lever_bits_=pad_ps;}
    unsigned player_lever()const{return lever_bits_;}
    bool player_grabbed()const{return grab_driving_;}
    bool player_dead()const{return player_dead_;}
    bool player_killed()const{return player_killed_;}
    void set_player_attack(bool attacking){player_attacking_=attacking;player_at_flags_=(player_at_flags_&~2u)|(attacking?2u:0u);} // plp->at_flg & 2
    void set_player_weapon(unsigned wpnr_no){player_weapon_=wpnr_no;} // plp->wpnr_no for bhControlPlayer effects
    void set_weapon_part2_hook(std::function<void()> hook){hide_weapon_part2_=std::move(hook);}
    // sys->obwp[0]/[1].mdflg 0x1 (objitm.c:364: nicht in sys->ob_hlg) beim Waffenmodellbesitzer.
    void set_weapon_models_hook(std::function<void(bool)> hook){weapon_models_hidden_=std::move(hook);}
    // plp->lkwkp (obwp-Index), lkono, lox/loy/loz und flg 0x80 aus event.c:5263 bhObjLinkSetObjPly.
    // bhSetPlayer (player.c:712/719) nullt lox..loz und lkwkp; der Raumbesitzer beginnt je Raum leer.
    struct PlayerObjectLink{bool linked=false;unsigned object=0,node=0;NJS_POINT3 offset{};};
    const PlayerObjectLink& player_object_link()const{return player_object_link_;}
    // obwp[index].mlwP->owP[node].mtx des Raumbesitzers (wie enemy_joint_position fuer Gegner).
    std::optional<BoneMatrix> object_node_matrix(unsigned index,unsigned node)const{return link_matrix(2,index,node);}
    void set_player_way(std::int16_t way){player_way_=way;} // plp->way, written by the combat pass (player.c:1707/4992)
    void set_player_speed(float spd){player_spd_=spd;} // plp->spd of the native action pass
    std::optional<float> take_player_speed(){if(!player_spd_written_)return std::nullopt;player_spd_written_=false;return player_spd_;}
    // PEXP0_I(0x14) dmlvl des nativen Aktionsbesitzers (en05.c:5627 bhEne05_CheckJump liest es).
    void set_player_dmlvl(std::int32_t dmlvl){player_dmlvl_=dmlvl;player_dmlvl_bound_=true;}
    void set_player_wpntp(std::int32_t wpntp){player_wpntp_=wpntp;} // EXP_WORK.wpntp (pl_smove08 mode1 42)
    // plp->stflg |= 0x80000 (en05.c:1827/1973/2852): der Giftzustand gehoert dem Inventar.
    bool take_player_poison(){const bool v=player_poison_request_;player_poison_request_=false;return v;}
    bool take_player_infect(){const bool v=player_infect_request_;player_infect_request_=false;return v;}
    // StartVibrationEx(AtrbId, VibNo) requests of enemies, zonzon helpers and
    // event opcode 0x9f; SetEventVibrationMode of opcode 0x9e. The runtime owns
    // the vibration unit and drains these once per tick.
    std::vector<std::pair<int,int>> take_vibration_requests(){auto out=std::move(vibration_requests_);vibration_requests_.clear();return out;}
    std::optional<int> take_vibration_mode(){auto out=vibration_mode_request_;vibration_mode_request_.reset();return out;}
    // PEXP0_F(offset) writes by enemy grab routines (en04.c:3448/3449); the
    // player's EXP_WORK floats live with the runtime's action work.
    std::vector<std::pair<unsigned,float>> take_player_exp0_writes(){auto out=std::move(player_exp0_writes_);player_exp0_writes_.clear();return out;}
    void start_task(unsigned level);
    // game.c:48-59 in zwei Teilen um den Spielerpass des Hosts: tick_enemy_pass() = bhControlEnemy und der
    // Kopf von bhControlPlayer (player.c:1366-1387) samt Skript-/Griffmodus; tick_after_player() = Ende von
    // bhControlPlayer (player.c:1743-1771), bhControlEffect, bhControlObjItm, bhControlLight. tick() = beide.
    void tick();
    void tick_enemy_pass();
    void tick_after_player();
    void control_lights(); // light.c:332 bhControlLight (game.c:59; auch bhInitCamSet event.c:4745)
    unsigned take_enemy_room_flags(){const auto flags=enemy_spawn_.room_flags;enemy_spawn_.room_flags=0;return flags;}
    Scene sample_player(const PlayerAsset& player,std::vector<BoneMatrix>* nodes=nullptr,const ModelNodeOverrides& combat={})const;
    // screen.c:920 bhDrawSmallScreenRenderTexture -> game.c:381 bhAllEasyDrawModel: Raum,
    // bhDrawObjItm und bhDrawEnemy unter gm_flg | 0x80 und st_flg | 0x20 mit cam.ncut = Rendercut
    // (cut.c:544 bhSetRenderCut); kein Spieler, keine Effekte. small_screen false = normaler Pass.
    using DrawPass=WorldDrawPass;
    void append(Scene& scene,const std::vector<BoneMatrix>& player_nodes={},const SceneCamera& active={},const DrawPass& pass={})const;
    // game.c:63 und screen.c:928-957: der Rendercut-Auftrag dieses Bildes (eff[ren_idx[0]], Kamera
    // aus rom->cutp[op->type].cam[0] mit cam.ofx, dist = 0.8 * dist bei 640 Breite) oder nichts.
    struct SmallScreenPass{unsigned slot=0,cut=0;SceneCamera camera;float dist=0,aspect_y=1.0f;};
    std::optional<SmallScreenPass> small_screen_pass(const SceneCamera& view);
    // screen.c:1007-1012: das fertige Bild als ef_tn[op->id]; Ps2_rendertex_initflag faellt.
    void commit_small_screen(std::vector<std::uint32_t> pixels,unsigned width,unsigned height);
    // sub1.c:3175 ItemTaskCheck: nach dem Statusschirm in Raum 1-05 Ps2_rendertex_initflag = 1.
    void item_screen_closed(){if(stage_no_==1&&room_no_==5)rendertex_init_=true;}
    // player.c:1666-1676 (nach bhCheckFloorP): unter sys->st_flg 0x100 loescht der Spielerpass
    // plp->mdflg 0x40 und setzt es, wenn bhCheckFloorEffect(plp->flr_no, plp->gpx, plp->gpz) eine
    // Flaeche mit prm0 2 liefert. Der Aufrufer reicht plp->flr_no desselben bhCheckFloorP.
    void check_player_mirror_floor(int flr_no);
    bool player_mirror_off()const{return player_mirror_off_;} // plp->mdflg 0x40
    bool player_model_hidden()const{return player_model_hidden_;} // plp->mdflg 0x1 (bhSetDispObj, event.c:2283)
    // cut.c:22 bhCheckCut: gm_flg &= ~0x20 je Hauptschleife, nach der exm-Festkamera (game.c:274-291)
    // und vor bhControlEvent; danach setzen Schnitt-/Ereigniskameras das Bit fuer dieses Bild.
    void begin_cut_check(){if(collision_events_)collision_events_->flags[11][0]&=~0x20u;}
    std::size_t export_textures(const std::filesystem::path& output,const std::string& room_name)const;
    SceneCamera camera(const Scene& stage,const SceneCamera& active)const;
    unsigned frame(unsigned kind,unsigned index)const;
    std::optional<NJS_POINT3> position(unsigned kind,unsigned index)const;
    // ene[n].mlwP->owP[node].mtx[12..14] (event.c:7826 bhEtcAtariEnePosSet).
    std::optional<NJS_POINT3> enemy_joint_position(unsigned index,unsigned node)const{const auto m=link_matrix(1,index,node);if(!m)return {};return NJS_POINT3{m->value[12],m->value[13],m->value[14]};}
    // ene[slot].mlwP->owP[node].mtx (pwksub.c:455-457 bhSearchNearEnemy2, player.c:7132-7134
    // bhSetHeadRotation): `slot` ist der Platz in sys->ene (enemy_table), nicht der Raumindex.
    // Gelesen wird der Baum, den das letzte bhCalcModel dieses Works hinterliess - bei
    // `epp->mlwP = epw->mdl` (en01.c:2827) der des Elternworks -, keine neue Motionpose.
    // Leer ohne Work, ohne Modell oder vor dem ersten bhCalcModel.
    std::optional<BoneMatrix> enemy_slot_joint_matrix(unsigned slot,unsigned node)const;
    // sys->obwp[index] als angehaengtes Objekt des Spielers: lkmtab (player.c:22-26, flg 0x81,
    // id 1211/1212) bindet bhSetPlayer/bhResetPlayer an plp, lkono 5 (player.c:759-783/965-989).
    // Ereignisse verlinken es um: bhObjLinkSet (0x32, event.c:2177) auf ene[enep[v0].wrk_no],
    // bhObjLinkSetPly (0x34, event.c:2501) zurueck auf plp, beide mit lkono/lox/loy/loz.
    // Ohne Linkbefehl gilt die lkmtab-Bindung mit `bound_offset` am Spieler.
    // control: objitm.c:289 (sp_flg 0x4) und :310-321 (stflg 0x1000000 vom lkwkp, pl_sleep_cnt).
    // listed: :363-371 (mdflg 0x1, id >= 1210 nur mit pt_flg 0x1). draw: bhDrawObjItm :503
    // (pt_flg 0x4) und bhDrawObject :635-656 (gelinkt pt_flg 0x1, stflg ohne pl_sleep_cnt). lkmtab traegt
    // weder flg 0x20000000 noch 0x40000000, das mdflg-0x40- und das hide[]-Tor (:654/:659) greifen nie.
    struct AttachedObject{bool linked=true;unsigned kind=0,index=0,node=5;NJS_POINT3 offset{};bool sleeping=false,control=false,listed=false,draw=false;};
    AttachedObject attached_object(unsigned index,NJS_POINT3 bound_offset)const;
    // pp = lkwkp eines Gegnerworks, wie bhCalcHair/bhObjClpn es lesen (player.c:7219-7241,
    // objitm.c:2180-2342): ax/ay/az, mlwP->objP[0..5].ang, mlwP->owP[5].mtx und flg2.
    // Leer, solange das Work keinen nativen, berechneten Objektbaum hat.
    struct LinkedWork{BoneMatrix head;std::array<std::array<int,3>,6> angles{};int ax=0,ay=0,az=0;unsigned flg2=0;};
    std::optional<LinkedWork> enemy_link_work(unsigned index)const;
    // objitm.c:2184-2186: bhObjClpn loescht pp->flg2 0x2 am gelinkten Work.
    void clear_enemy_flg2(unsigned index,unsigned bits);
    // Pruefnaht: das Werk-flg (ETTY flg bei bhSetEneMdl eneset.c:183 / bhSetObject / bhSetItem; e.object_flags,
    // bei gebundenem Gegnerwerk zusaetzlich enemy.flg). Im Original hat Bit 0x1 nach dem Setzen keinen Schreiber.
    void set_work_flags(unsigned kind,unsigned index,unsigned flg);
    // Testnaht: fn im Rahmen der Gegnerrueckrufe (EnemyCallbackFrame) ausfuehren, wie aus dem Gegnerpass.
    void run_enemy_callback(const std::function<void()>& fn){EnemyCallbackFrame frame(this);fn();}
    // sys->itwp[n].px/py/pz (event.c:7896 bhEtcAtariEvtPosSet).
    void set_item_position(unsigned index,NJS_POINT3 pos){entity(3,index).transform.position=pos;}
    bool player_hidden=false,player_scripted=false,player_action=false;
    bool game_over_=false; // bhGameOverSet request; death driver pending
    WorldEntity player_morph_{}; // bhCyoutenHenkei on the player work
    bool event_camera=false;
    unsigned event_camera_index=0;
    std::optional<unsigned> fixed_camera;
    unsigned event_camera_key()const{return event_camera_key_;}
    unsigned event_camera_clock()const{return event_camera_clock_;} // cam.ct0
    bool event_camera_paused()const{return event_camera_paused_;}   // cam.mode0 == 3
    bool event_camera_finished()const{return event_camera_finished_;} // cam.mode0 == 2
    // bhSetCut (cut.c:281): clip, hide tables and the cam.fog_* hand-over. The
    // bhSetFixedCut clip call passes cam.ncut, its fog comes from cno separately.
    void initialize_camera_clip(unsigned cut,bool hand_over_fog=true);
    // player.c:842 bhInitPlayer -> bhCheckCut(1) -> bhSetCut(ncut, 1) (cut.c:91) -> bhInitActiveCamera (cut.c:442):
    // Raumeintritt setzt den Schnitt und den Startwert cam.plx/plz (cut.c:681/683) in einem Schritt.
    void init_room_camera(const Scene& stage,unsigned cut){initialize_camera_clip(cut);init_camera_target(stage,cut);}
    ViewClip camera_clip()const{return installed_clip_;}
    // ClipVolume des letzten _Make_ClipVolume-Aufrufs (bhChangeClipVolumeRM beim Raumschnitt,
    // bhChangeClipVolume bei der Ereigniskamera); nichts sonst setzt es zurueck.
    ClipVolume clip_volume()const{return installed_volume_;}
    void tick_camera(const Scene& stage,const SceneCamera& target,std::size_t cut,bool tracking=true);
    // game.c:53-58: bhSetPlEyeCamera und bhControlCamera (camera.c:23-59) rechnen cam.mtx/vx/wpx aus der
    // cam-Lage dieses Zeitpunkts (nach bhCheckCut des Vorbildes und Task 8). Zeichnen, Effekte (cam.mtx,
    // cam.vx, cam.wpx) und der Tonmonitor lesen diesen Stand bis zum naechsten Aufruf. Vor dem ersten
    // Aufruf im Raum ist cam genullt (room.c:33 bhInitCamera, camera.c:8/18): kein Wert.
    void control_camera(const Scene& stage,const SceneCamera& target);
    const std::optional<SceneCamera>& controlled_camera()const{return controlled_camera_;}
    // Diagnose: cam.ct0 und Init-Pass (cam.mode0 0) der Ereigniskamera zum Zeitpunkt von bhControlCamera.
    unsigned controlled_event_clock()const{return controlled_event_clock_;}
    bool controlled_event_init()const{return controlled_event_init_;}
    // effsub3.c:2401 bhSetEffGunSpark: AllocOworkOne (erster freier eff-Platz), Effekt 308.
    int gun_spark(NJS_POINT3 pos,NJS_POINT3 dir,std::uint32_t src_col,std::uint32_t dst_col,int typ_no);
    // sys->pad_on/pad_ps dieses Passes (bhSysCallMain), fuer Effektprozeduren mit Padsteuerung (bhEff134).
    void set_pad(unsigned on,unsigned ps){pad_on_=on;pad_ps_=ps;}
    // screen.c bhSetScreenFade und sys->fade_pbk beim Besitzer der Runtime.
    void set_screen_fade_hooks(std::function<void(std::uint32_t,float)> set,std::function<std::uint32_t()> previous){screen_fade_set_=std::move(set);screen_fade_previous_=std::move(previous);}
    // player.c:1114 bhStandPlayerMotion (native/player_action.c) auf dem einen Spielerwerk der
    // Runtime (RECVX_PLAYER_ACTION); Gift/Infektion 0x280000 liest es beim Inventar. Flags, Modus,
    // spx/spz, Motion/Bild und Bibliothek schreibt die Welt selbst; bhCalcModel(plp) ist der naechste
    // Posenpass. Ohne gebundenes Werk: benannte Luecke, false.
    void bind_player_action(RECVX_PLAYER_ACTION* work,const Inventory* inventory){player_action_work_=work;player_inventory_=inventory;}
    bool stand_player_motion();
    // Anzahl ausgefuehrter bhStandPlayerMotion seit dem Raumaufbau: *(int*)&plp->mode0 = 1 beendet die
    // laufende Aktion; der Aktionsbesitzer der Runtime liest den Zaehler fuer seinen Wiedereintritt.
    unsigned player_stand_count()const{return player_stand_count_;}
    void set_camera_offset_owner(NJS_POINT3* offset){camera_offset_=offset;}
    // Kindworks aus bhSetEnemy (Diagnose: --state).
    const std::deque<WorldEntity>& enemy_children()const{return enemy_children_;}
    // effsub1b.c:101 bhDraw134: die Kran-Anzeige (2D, Texturgruppe 44) ueber dem Bild.
    // effect.c:929 ef_fnc-2D-Anzeigen: bhDraw134 (Kran) und bhDraw137 (Ueberwachungskamera).
    // mirror_pass: danach unter st_flg 0x100 den zweiten bhDrawEffect (game.c:427) mit seinen
    // 2D-Listen (ntx2d, trs2d, ef_fnc) in Originalreihenfolge; false im Rendertexturbild.
    void draw_fnc_huds(SoftwareRenderer& image,bool mirror_pass=true)const;
    // game.c:68 / screen.c:816: der gesamte bhAllDrawModel-Pass in 512x480, dann
    // die 15 Streifen aus sys->rpb ueber die 512x512-Rendertextur. true = gezeichnet.
    bool draw_full_screen(SoftwareRenderer& image,const Scene& scene,const SceneCamera& view,bool wireframe=false)const;
    // game.c:106, nach bhAllDrawModel/Rendertextur, vor bhDrawScreenFade, unabhaengig von pt_flg.
    void draw_thermometer(SoftwareRenderer& image)const;
    // cut.c:138-147 cam.flg 0x40: die Monitorkamera, deren Lage allein bhEff137
    // (effsub1b.c:1015) schreibt; bhControlMonitorCamera (cut.c:2802) ist leer.
    bool monitor_camera_active()const{return monitor_.active;}
    // effect.c:905 bhDrawNtxEffect2D(sys->ef_ntx2d): untexturierte Schirmpolygone der
    // Effektsaetze dieses Passes (bhEff130 type 3), njDrawPolygon(pvp, pn, 1) ueber das
    // fertige Bild, Tore flg/stflg 0x1000000 und gm_flg 0x4000 + mdflg 0x40.
    // pass 1 = zweiter bhDrawEffect im Spiegelpass (gm_flg 0x4000), aus draw_fnc_huds.
    void draw_ntx2d_effects(SoftwareRenderer& image,unsigned pass=0)const;
    // effect.c:912 bhDrawTrsEffect2D(sys->ef_trs2d): texturierte Schirmvierecke der Effektsaetze dieses
    // Passes (effsub1.c bhEff2D), njDrawTexture(tvp, pn, gidx, 1) mit bl_src/bl_dst ueber das fertige Bild.
    void draw_trs2d_effects(SoftwareRenderer& image,unsigned pass=0)const;
    // face.c:79 bhControlMask fuer einen Besitzer mit Maskendaten (Gegnerpass id > 90 oder Weltpass).
    void control_mask(WorldEntity& e,const PlayerAsset& actor);
    struct FogView {bool enabled;std::uint32_t color;float near_range,far_range;int ct;};
    FogView fog_view()const{return {fog_.enabled,fog_color_,cam_fog_near_,cam_fog_far_,fog_ct_};}
    // sync.c:15 njSetEORFunction(bhControlEOR): der Nebelteil (:143-255) laeuft am Ende jedes Bildes, auch
    // ohne Kamerapass. Unter gm_flg 0x200000 (gameover.c:323, Todesszene md1 10) nur njFogDisable (:143).
    void end_of_render(){control_fog();}
    // ps2_event.c / sync.c:120-139. Separate from camera tick: an event command may tick that more than once per frame.
    void control_background();
    void suspend_background(){background_enabled_=false;} // system.c:581/598/613/630/649
    void restore_background(unsigned delay); // sys->bcl_ct + gm_flg 0x8000; does not enable until EOR expiry
    // ROM_WORK.bak_col, kept raw (0xAARRGGBB); effect 138 saves/restores it separately from EOR arming.
    std::uint32_t room_background()const{return room_background_;}
    void set_room_background(std::uint32_t color){room_background_=color;}
    std::uint32_t background_pixel()const; // the process-wide installed njSetBackColor (back_color.h), host canvas form
    bool background_enabled()const{return background_enabled_;}
    unsigned background_delay()const{return background_delay_;}
    bool camera_transitioning()const{return active_camera_.transitioning();}
    unsigned camera_clock()const{return active_camera_.clock();}
    int floor_at(NJS_POINT3 position)const;
    const RECVX_PUSH_STATE* push_state()const{return push_?&push_->state():nullptr;}
    bool pushing()const{return push_&&push_->active();}
    bool tick_push(bool forward);
    // objitm.c:1648 bhObj007 / player.c:4336 bhCPM2_act_rpsh: Greif-und-Dreh-Gegenstand.
    // bhCPM2_act_rpsh is dispatched from mode2 22 (player.c:1957). A script
    // reset (bhLoadWork/Player_controll 128) clears that work field; the action
    // must end with it, not with a separate host latch.
    bool turning()const{return turn_active_&&((collision_mode_>>16)&0xffu)==0x16u;}
    bool tick_turn(bool forward);
    // bhObj001 stores climb / descend ETC records in sys->metcp.  Unlike the
    // room ETC table, these records move with their O_WRK.
    std::vector<RECVX_STAIR_AREA> push_steps();
    std::vector<RECVX_STAIR_AREA> push_floors();
    void initialize_player_spawn(); // bhSetRoom: POS height selects rom->grand
    const std::array<float,32>& floor_heights()const{return floors_;} // rom->grand
    unsigned floor_sound(NJS_POINT3 position,bool special=false)const;
    unsigned floor_sound(int floor,float x,float z,bool special=false)const;
    // hitchk.c bhCheckWater: the first original type-3 water area with prm0 == 1
    // that contains the probe, room flrp before bhObj001's mflrp. `top` then
    // receives fp->py + fp->h, the surface the original splash is placed on.
    bool water_top(NJS_POINT3 position,float& top)const;
    bool water_area_at(NJS_POINT3 position,float& top,float& h)const; // bhCheckWater mit h
    // hitchk.c:5561 bhCheckL2Water: line (origin, vec) against the top plane of
    // the same water areas in the same order; the first hit within |vec| wins.
    bool l2_water(NJS_POINT3 origin,NJS_POINT3 vec,NJS_POINT3& hit)const;
    // weapon.c:607 bhCheckGunAtari / pwksub.c:218 bhSearchNearEnemy fuer Effektprozeduren
    // (effsub1.c:3352 bhEff013): derselbe Kampfkern auf sys->ene, Effekte in die Passliste.
    int effect_gun_atari(RECVX_GA_WORK& gat);
    RECVX_COMBAT_SERVICES effect_combat_services();
    bool effect_search_near_enemy(NJS_POINT3 pos,int ay,int& r,float& h,int& id);
    // pwksub.c:2643 bhSetWaterSplash4: two id-11 effects (type+10) from the
    // shared sys->ef template, ay = ang +/- 16384, lkono 0 and 1.
    void water_splash4(NJS_POINT3 pos,int ang,int type,float sx,float sy,float sz);
    // pwksub.c:2572 bhSetWaterSplash2(pp, pos, ...): Boden unter (pos.x, pp->py, pos.z), bhCheckWater,
    // dann drei id-11-Effekte aus sys->ef (ay = pp->ay + 16384 / - 16384 / + 32768, lkono 0/1/2).
    void water_splash2(float pp_py,int pp_ay,NJS_POINT3 pos,int type,float sx,float sy,float sz);
    // effsub4.c:19/35 bhEff_SetPtcl/bhEff_SetPtcl1 (Effekt 365), :117 bhEff_SetPtcl2 und
    // :199 bhEff_SetPtcl2V (Effekt 366). Jeder Satz fuehrt seine Punkte in einem
    // bhEff_AllocOwork-Hilfsarbeitssatz (EffectPtcl365Work/EffectPtcl366Work), den der
    // Erzeuger anlegt und fuellt; die Prozeduren rechnen darauf weiter.
    void set_ptcl(RECVX_ENEMY_WORK* w,NJS_POINT3 pos,int tex_id);
    void set_ptcl1(RECVX_ENEMY_WORK* w,NJS_POINT3 pos,int tex_id,int wcnt);
    void set_ptcl2(NJS_POINT3 pos,const float* mtx16);
    void set_ptcl2v(NJS_POINT3 pos,NJS_POINT3* dv,int wcnt);
    // en17.c:1689 bhEne17_AfterimageAxEffect: ein 364-Satz (bhEff_ModelDraw) mit
    // mlwP = &epw->mdl[5], flr_no 1 (npSetAllMatColor) und tv[0].col = argb.
    void en17_afterimage(RECVX_ENEMY_WORK* w,const float* mtx16,NJS_POINT3 ofs,std::uint32_t argb);
    void player_footstep(const PlayerAsset& player);
    void player_foot_sound(const PlayerAsset& player,unsigned node,unsigned type);
    // pl_smove01: CallPlayerFootStepSe(plp->flr_snd, type, 1) ohne bhCheckFloorSound.
    void player_floor_step(unsigned flr_snd,unsigned type){audio_.player_footstep_se(flr_snd,type);}
    unsigned player_floor_sound()const{return collision_floor_sound_;} // plp->flr_snd (bhCheckFloorP Typ 1)
    bool search_near_enemy_b(NJS_POINT3 pos,int ay,int ar,float len)const;
    NJS_POINT3 player_safe_position()const{return collision_safe_;}
    // Prueflesen: cam.plx/ply/plz (y ohne ci->h) und EXP_WORK plx/plz (nur x/z gefuehrt).
    std::optional<NJS_POINT3> camera_follow_target()const{if(!camera_target_valid_)return std::nullopt;return camera_target_;}
    NJS_POINT3 player_camera_point()const{return player_camera_point_;}
    int player_floor_number()const{return collision_floor_;} // plp->flr_no
    void set_player_safe_position(NJS_POINT3 p){collision_safe_=p;}
    // Host-Diagnose-Teleport (Replay "position", Debug): der Spieler steht danach so, als
    // waere er schon dort gewesen. Wie die Original-Teleports der Aktionsenden
    // (player.c:3095-3099: px = spx, bpx = bpxb = px) folgen spx/spz und die Vorsonde bpxb der
    // neuen Lage; sonst waehlt bhCheckWallEx die Austrittsseite nach der alten Sonde.
    // Ende einer Stufen-/Treppenaktion: bpx = bpxb = px, bpy = bpyb = 12.5 + py, bpz = bpzb = pz
    // (player.c:3099/3294/3474/3692). Ohne das vergleicht der erste Koerperpass nach der
    // Landung gegen die Vorsonde der Ausgangsebene.
    // Dieselben Enden setzen plp->pxb/pyb/pzb = px/py/pz (player.c:3103/3298/3478/3696/4097/4292),
    // die alte Lage des Koerper-bhCheckWallEx (:1647) im selben Pass.
    void set_player_previous_probe(NJS_POINT3 p){collision_previous_probe_={p.x,p.y+12.5f,p.z};player_prev_pos_=p;}
    void place_player(NJS_POINT3 p){collision_safe_=p;collision_previous_probe_={p.x,p.y+12.5f,p.z};collision_front_offset_=collision_front_target_=0.f;collision_front_point_=p;collision_probe_ready_=true;}
    // EXP_WORK fpx/fpy/fpz und arn, wie sie der Koerperteil von bhControlPlayer zuletzt
    // schrieb (player.c:1578-1587): fpx = px - sin(ay)*arn mit der Lage NACH dem
    // Aktionsschritt und VOR bhCheckWallEx (:1622/:1647 aendern px, fpx nicht).
    // bhCheckExmAtari liest daraus die Sonden pxn[0]/pxn[3] (hitchk.c:4605-4618).
    NJS_POINT3 player_front_point()const{return collision_front_point_;}
    float player_front_offset()const{return collision_front_offset_;}
    // bhControlPlayer player.c:1578-1591 und :1649-1654: der Spielerpass fuehrt
    // EXP_WORK arn an arp nach und schreibt fpx/bpx/spx aus der aktuellen Lage.
    // `arp` ist der Wert, den bhCPM2_act_* dieses Bildes hinterlassen hat.
    void settle_player_body(NJS_POINT3& position,float arp);
    void finish_player_pass(NJS_POINT3& position,Angle yaw);
    // plp->pxb/pyb/pzb: Kopf von bhControlPlayer (player.c:1366-1369), Stufenenden und
    // Ereignisbefehle (event.c:5868/9665/12044); Leser :1647 und en04.c:911, en15.c:2901/3122.
    NJS_POINT3 player_pass_origin()const{return player_prev_pos_;}
    bool player_safe_position_ready()const{return collision_probe_ready_;}
    void report_enemy_gap(const char* name){unbound_enemy_.insert(name);}
    NJS_POINT3 move(NJS_POINT3 from,NJS_POINT3 to,float radius=3.5f,float height=16.5f,std::optional<bool> walking={});
    void bind_player_collision(EventVm&);
    NJS_POINT3 move_player(NJS_POINT3 from,NJS_POINT3 to,int movement,bool run,int turn=0);
    void check_player_floor(NJS_POINT3 position,Angle yaw,int floor);
    // *(int*)&plp->mode0 of the collision-owned player work (mode0..mode3,
    // little endian). bhCheckWallEx writes 2/0x102 for a fire wall.
    unsigned player_mode()const{return collision_mode_;}
    void set_player_mode(unsigned mode){collision_mode_=mode;}
    // plp->mnwP = plp->mnwPb (player.c:1410 vor bhCPM0_die): die eigene Bibliothek, ohne Standmotion.
    void set_player_library_own(){player_room_motion_=false;player_enemy_motion_=false;}
    // sys->rmthp ist ein 512er Feld genullter MN_WORK; der Host fuehrt nur die
    // vorhandenen Eintraege. Ein mtn_no dahinter hat datP NULL: bhSetMotion
    // (Motion.c:29) tastet nicht ab und schreibt objP nicht.
    const MotionClip* player_room_clip()const{return player_room_motion_&&motion_<room_motions_.size()&&room_motions_[motion_].frames?&room_motions_[motion_]:nullptr;}
    // Free player motion with an explicit EXP_WORK arp (bhCPM0_damage).
    NJS_POINT3 move_player_arp(NJS_POINT3 from,NJS_POINT3 to,float arp){return resolve_player_motion(from,to,3.5f,player_character_?17.5f:16.5f,arp);}
private:
    // texture_list: Lage des PVP-Buendels von mdl[0] in der Raumdatei, 0 = mdl[0] ohne texP. Gleiche
    // Werte stehen fuer dieselbe sys->et_lp[id][mdlno][0]-Liste (room.c:592-604, texture_cache beim Laden).
    struct Model {unsigned kind,index;std::unique_ptr<PlayerAsset> actor;Scene object;std::size_t source_model=0;std::vector<ModelPose> rest_pose;std::size_t texture_list=0;};
    // rom->effp[i].lkflg/lkno nach bhSetEffectLink (room.c:787): die Raumtabelle, die bhEff085 ueber
    // op->type liest (effsub6.c:43/56); unabhaengig davon, welcher Satz spaeter auf Platz i liegt.
    std::vector<std::pair<int,int>> room_effect_links_;
    // bhSetEneMdl realizes one model string per MDL block of the entity's asset
    // list; that order is the mbp[] the morph opcodes index. mbp[0] is the Work's
    // own model, the blocks load_models kept follow as mbp[1..].
    // nullptr when the entity has no such variant, which the original would read
    // as a NULL mlwP->objP.
    static const PlayerAsset* model_variant(const Model& model,unsigned index){
        if(!model.actor)return nullptr;
        return index==0?model.actor.get():model.actor->model(index);
    }
    /* BH_SCEWORK mode1/mode2 (types.h:1953) */
    struct Context {unsigned kind=0,index=0,node=0,mode1=0,mode2=0;unsigned mtn_md=0,mdflg=0;int hokan_rate=0;bool work=false;int ev_eff_no=0;std::array<int,3> pos_sign{},ang_sign{};NJS_POINT3 increment{};std::array<int,3> angle_increment{};std::array<std::array<float,3>,4> positions{},angles{};std::array<std::array<float,3>,2> ground{};std::array<std::array<float,5>,2> e_lgt{};};
    // mode: LGT_WORK.mode (types.h:412), 0 bei jedem Schreiber (bhInitLight light.c:187, bhSetLightTab
    // light.c:247, bhSetGunFire pwksub.c:2708, bhSetExplosion weapon.c:1726). lit_after_clear: der letzte
    // bhControlLight-Pass hat das Licht noch gesetzt (lsrc 4, light.c:788), obwohl er im selben
    // Durchlauf flg 0x1/0x2 geloescht hat (Typ 13/100/101, light.c:658-699).
    struct Light {unsigned flags,type,speed,link,index,node,source,clock=0;NJS_POINT3 position,offset,direction,color;float near_range,far_range;NJS_POINT3 working_color{};bool color_sampled=false;unsigned mode=0;bool lit_after_clear=false;
        // light.c:494-505, types.h:399-435: spc/dif/amb tragen fuer 10/11 den
        // Flackerzustand; wax/way/waz sind fortgeschriebene, nicht auf 16 Bit
        // begrenzte Winkel. w* wird beim Pausieren gelesen (:703-717).
        NJS_POINT3 flicker_color{},working_position{},working_direction{};
        std::array<std::int32_t,3> working_angles{};
        float working_near_range=0,working_far_range=0;
        bool effect_link_resolved=false; // Ausgabe des letzten bhControlLight, kein neuer Draw-Resolver
        int ct1=0; // LGT_WORK.ct1 (en22.c:2933 bhEne22_CtrLight)
        };
    // CAM_KEYF_WORK: flg (0x8 = this key carries fog), fog_col/fog_nr/fog_fr.
    struct EventCamera {SceneCamera camera;unsigned frame=0,link,index,node;NJS_POINT3 offset;std::array<std::uint32_t,4> hidden_lights;unsigned flags=0;std::uint32_t fog_color=0;float fog_near=0,fog_far=0;};
    struct EventCameraTrack {std::vector<EventCamera> keys;unsigned next=0;};
    bool room_uv_modified_=false;
    std::vector<std::uint8_t> room_;std::size_t header_;unsigned simulation_flags_=~0u;
    std::unique_ptr<PlayerPush> push_;
    GroundCollision ground_; // room-owned original sys->htp, shared by ground consumers
    // Collision-owned fields of the existing native player, not another
    // player position/work. In production, flags/status/control/system refer
    // directly to EventVm's existing banks. Fallbacks serve World-only users.
    EventVm* collision_events_=nullptr;
    unsigned collision_flags_=0x919,collision_status_=0x40000000,collision_flags2_=0;
    unsigned collision_control_=0,collision_system_=0,collision_mode_=1;
    unsigned collision_floor_sound_=0,collision_foot_effect_=0;
    int collision_floor_=0,collision_push_count_=0,collision_push_index_=0,collision_floor_index_=0;
    float collision_front_offset_=0,collision_front_target_=0; // EXP_WORK arn/arp
    NJS_POINT3 collision_front_point_{}; // EXP_WORK fpx/fpy/fpz (player.c:1585-1587)
    NJS_POINT3 collision_previous_probe_{},collision_safe_{};
    bool collision_probe_ready_=false;
    // Der Koerperteil von bhControlPlayer laeuft im Original genau einmal je
    // Bild (player.c:1578-1655). Im Host besitzt ihn der Bewegungspass; ohne
    // Bewegungspass uebernimmt ihn die bhCheckFloorP-Stelle.
    bool collision_body_pass_=false;
    void player_body_pass(NJS_POINT3& position,Angle yaw);
    std::optional<std::size_t> collision_step_;
    MotionActor motion_actor();
    std::vector<CollisionWall> motion_steps()const;
    std::vector<CollisionWall> motion_floors()const;
    NJS_POINT3 resolve_player_motion(NJS_POINT3,NJS_POINT3,float,float,std::optional<float>,std::optional<Angle> yaw={});
    PlayerGroundWork player_ground_{};bool player_ground_ready_=false;
    std::optional<std::pair<unsigned,int>> stair_ground_state_;
    std::vector<unsigned> push_objects_;
    bool push_pose_=false,player_poisoned_=false;
    int push_health()const{return player_health()>=120?(player_poisoned_?1:0):player_health()>=30?1:2;}
    // Die Slow-Abtastung der Schiebeaktion; solange objP die Darstellung traegt, hat es sie bereits.
    bool push_pose_current()const{return push_&&push_pose_&&!player_obj_drives_&&!player_scripted&&!player_enemy_motion_&&motion_==std::size_t(push_->state().motion)&&frame_==push_->state().frame;}
    void ensure_push();
    void sync_push_objects();
    void update_push();
    void update_ground_geometry();
    void update_player_ground_work();
    void refresh_motion_overrides(bool enemy_phase);
    void advance_entity_motions(bool enemy_phase);
    std::vector<CollisionWall> collision_geometry()const;
    const PlayerAsset* player_asset_=nullptr;
    bool player_room_motion_=false,player_frozen_=false;
    std::int64_t player_step_=65536;
    int* player_hp_owner_=nullptr;std::uint32_t* player_head_flags_=nullptr;RECVX_HEAD_STATE* player_head_=nullptr;
    int player_ct0_=0,player_ct1_=0,player_ayp_=0,player_hokan_rate_=0;
    int player_azp_=0; // plp->azp: das Zielwinkel, das bhObj007 an bhCPM2_act_rpsh uebergibt
    // bhCPM2_act_rpsh (player.c:4336): der Drehzustand der Greif-und-Dreh-Aktion (mode2 22).
    bool turn_active_=false;unsigned turn_phase_=0;int turn_ayp_=0;unsigned turn_motion_=0;std::int64_t turn_frame_=0;int turn_hokan_rate_=0;unsigned turn_hokan_count_=0;int turn_mtn_add_=0;
    // plp im Skriptmotionsmodus: Common_controll 24/25/34 Selektor 0 (event.c:11469/10960/11802) setzt
    // mode0 7 / mode2 8 (bhCPM0_event -> pl_smove08, pl_evt.c:649) mit mode1/ct0/ct1/ct2; bhControlPlayer
    // fuehrt danach bhSetMotion(plp, mtn_add, mtn_md) (player.c:1502) auf dem einen objP.
    struct PlayerScriptMotion{bool active=false;unsigned mode3=0,mode1=0;int ct0=0,ct1=0,ct2=0;bool room_library=false;} player_smove_;
    std::int32_t player_wpntp_=0;
    void control_player_script_motion();
    unsigned player_mtn_md_=0,player_hokan_count_=0,player_at_flags_=0,player_enemy_owner_=0;
    const RECVX_ENEMY_WORK* player_enemy_work_=nullptr; // plp->mnwP: the grabbing work
    // plp->mnwP hat zwei Schreiber: das Feld der Spielersicht (RECVX_ENEMY_PLAYER.mtn_enemy*) und
    // die Dienste player_library (en03/en13/en15/en19/en23/en29). Ein Gegner liest die Sicht, ruft den
    // Dienst und schreibt die Sicht danach zurueck (en03.c:6825 liegt zwischen beiden). Die Sicht
    // uebernimmt deshalb nur, was sie gegenueber dem zuletzt ausgegebenen Stand selbst geaendert hat.
    mutable bool player_lib_seen_motion_=false;mutable unsigned player_lib_seen_owner_=0;mutable const RECVX_ENEMY_WORK* player_lib_seen_work_=nullptr;
    unsigned player_frm_mode_=0,player_mtn_attr_=0;
    std::vector<ModelPose> player_obj_;std::vector<unsigned> player_owp_flags_;
    // plp->mlwP->objP traegt die Darstellung, solange der Takt des laufenden Pfads den
    // Knotenteil von bhSetMotion ueber player_set_motion_pose schreibt (player_obj_drives_).
    bool player_obj_drives_=false,player_obj_written_=false;std::size_t player_obj_motion_=~std::size_t(0);
    bool enemy_player_driving_=false;
    // owP belongs to the player model and is published by its control pass,
    // not by rendering. Enemy joint queries read the previous committed tree.
    std::vector<BoneMatrix> player_joint_matrices_;
    struct AfterimageModelWork {const PlayerAsset* asset=nullptr;std::vector<ModelPose> pose;std::vector<BoneMatrix> nodes;std::vector<std::uint32_t> flags;};
    // Views of published mdl[n], not another work dispatcher. Room lifetime and explicit commit.
    std::map<std::tuple<unsigned,unsigned,unsigned>,AfterimageModelWork> afterimage_models_;
    // sys->ufo_pos/ufo_md/ufo_flg/ufo_oidx: geteilt von bhEff134 (effsub1b.c:220) und bhEne56.
    struct UfoState{NJS_POINT3 pos{};int md=0,flg=0;unsigned oidx=0;} ufo_;
    // cam.px/py/pz, cam.ax/ay/az, cam.pers, cam.ln unter cam.flg 0x40 (bhEff137). Raumlokal wie
    // cam selbst: bhInitCamera (camera.c:9 cam.flg = 0) beim Raumaufbau, bhSetCut loescht 0x46.
    struct MonitorCamera{bool active=false;unsigned cut=0;NJS_POINT3 position{};std::int32_t ax=0,ay=0,az=0,pers=8192;float ln=0;} monitor_;
    // plp->ct1/ayp (en12.c:1710-1742 Drehung im Griff) und npSetAllMatColor(plp) (en12.c:1192).
    std::optional<std::uint32_t> player_material_; // plp ct1/ayp liegen in player_ct1_/player_ayp_ (oben)
    unsigned pad_on_=0,pad_ps_=0;
    RECVX_PLAYER_ACTION* player_action_work_=nullptr;const Inventory* player_inventory_=nullptr;unsigned player_stand_count_=0;
    // cam.ofx/ofy/ofz (camera.c:36/49/81): Besitzer ist die Runtime; sie nullt ihn bei jedem
    // Raumladen wie bhInitCamera (room.c:33, camera.c:8). nullptr = nicht gebunden.
    NJS_POINT3* camera_offset_=nullptr;
    std::function<void(std::uint32_t,float)> screen_fade_set_;std::function<std::uint32_t()> screen_fade_previous_;
    void step_ufo_effect(unsigned slot);
    void step_monitor_effect(unsigned slot); // effsub1b.c:1015 bhEff137
    void step_oil_effect(unsigned slot);     // effsub1b.c:1618 bhEff138
    void step_render_monitor_effect(unsigned slot); // effsub1.c:6927 bhEff111
    void step_full_screen_effect(unsigned slot); // effsub1.c:6660 bhEff110
    void step_thermometer_effect(unsigned slot); // effsub1b.c:17 bhEff133
    // bhEne_SetFireEffect(plp, ...) und bhEne12_SetFireBintaEffect aus Effektprozeduren.
    void effect_player_fire(int jno,NJS_POINT3 ofp,float size,int life);
    void effect_en12_binta(RECVX_ENEMY_WORK* w,int action);
    // hitchkl.c:735 Datensicht der Treffermodelle (plp mdl[3], Figuren mdl[1]) je Modell.
    std::map<const PlayerAsset*,std::vector<std::optional<HitModelObject>>> hit_models_;
    // effsub2.c:2296 bhEff_E11_CheckCollisionPlayer.
    int effect_player_hit(NJS_POINT3 p1,NJS_POINT3& p2);
    BoneMatrix player_link_matrix_{};
    bool player_link_matrix_valid_=false;
    std::function<void(uint32_t)> pad_clear_cb_;
    SceneSpawn& player_;std::size_t& motion_;std::int64_t& frame_;AudioMixer& audio_;
    std::array<Context,16> contexts_{};
    std::map<std::pair<unsigned,unsigned>,WorldEntity> entities_;
    mutable std::set<std::string> unbound_enemy_; // const owP/pose services report their gaps too
    mutable std::set<std::string> render_warnings_; // Host-Diagnose fuer isolierte Darstellungsfehler
    // Plaetze, deren letzter Zeichenpass das Rueckseitentor flg 0x20000 geschlossen
    // hat (effect.c:1391 op->stflg |= 0x10). bhControlEffect traegt sie im naechsten
    // Bild in stflg ein; der Zeichenpass selbst ist const.
    mutable std::set<unsigned> effect_backface_;
    unsigned enemy_game_mode_=0;
    // bhSetEffectTb from game code: `link_kind` 0 = world position, 2 = enemy joint (WorldEntity index/joint).
    int spawn_effect(const EffectSpawn& spawn,unsigned link_kind=0,unsigned link_index=0);
    void gun_fire(unsigned kind,unsigned index,const BoneMatrix& joint,unsigned wno,unsigned jno,unsigned hand,int ang);
    void set_yakkyou(unsigned kind,unsigned index,const BoneMatrix& joint,const BoneMatrix& root,unsigned wno,unsigned jno,unsigned hand,int ang);
    unsigned process_effect_output(); // Rueckgabe: Anforderungen, die keinen Platz bekamen (bhEff<id> gemeldet)
    void tick_shadow(unsigned slot);
    int en11_light_control(RECVX_ENEMY_WORK* w,int mode);
    static void bind_en11_services();
    // effect.c:644 bhSetShadow on a native work; returns the eff[512] slot or -1.
    int set_enemy_shadow(const std::int8_t* jtb,RECVX_ENEMY_WORK* link,int lkono,float sx,float sy,float sz);
    // player.c:791/905 bhSetShadow(NULL, plp, 1, 4.5, 4.0, 3.5): its lkwkp names plp through this tag.
    unsigned char player_shadow_tag_=0;
    // effsub2.c:1717 bhEff_E03_Shadow: der Schatten von bhSetShadow mit id 258 (en23.c:613).
    void tick_e03_shadow(unsigned slot);
    // effsub3.c:1640 rySetShadow / :1683 bhEff304 (Modifier-Schatten ueber sys->efm[1], drei owP-Gelenke).
    int ry_set_shadow(RECVX_ENEMY_WORK* w,int obj0,int obj1,int obj2,float off_a,float off_b);
    void tick_ry_shadow(unsigned slot);
    void write_particle_slot(unsigned slot,unsigned flags,unsigned id);
    void bomb_damage(const BombRequest& bomb);
    void bomb_enemy_damage(const BombRequest& bomb);
    EffectParticles particles_;
    void tick_en01();
    // C02 enemy pass on the native works (eneset.c:344 bhControlEnemy) and
    // the room-load half (room.c:114 bhSetEneMdl -> bhSetEnemy).
    RECVX_ENEMY_TABLE enemy_table_{};RECVX_ENEMY_DISPATCH enemy_dispatch_{};RECVX_ENEMY_SERVICES enemy_services_{};
    std::vector<std::unique_ptr<unsigned char[]>> enemy_exp_blocks_;std::size_t enemy_exp_used_=0;
    // sys->mwalp entries an enemy allocated (bhEne00 eneset.c:427, en25.c:58 ...).
    // game.c:48-51 runs bhControlEnemy before bhControlObjItm, so the first
    // pass puts them ahead of the bhObj001 entries PlayerPush owns.
    std::vector<CollisionWall> enemy_mobile_walls_;
    // sys->mwalp-Eintraege der Objektprozeduren (bhObj008), im ersten Objektdurchlauf zugeteilt.
    // Kein Raum fuehrt Typ 1 (PlayerPush) und Typ 8 zugleich; sie liegen hinter den Push-Eintraegen.
    std::vector<CollisionWall> object_mobile_walls_;
    // T3 service supplement (en03_lib.h RECVX_ENEMY_EXT): the original
    // rom->walp + sys->mwalp order for the shared en03.c:5220 wall pass.
    RECVX_ENEMY_EXT enemy_ext_{};std::vector<CollisionWall> enemy_wall_cache_;
    // Kindworks aus bhSetEnemy(&zob_lkmtab, ...) (en01.c:2817): Raumlebensdauer
    // wie ene[], stabile Adressen, weil die Tabelle nur Zeiger haelt.
    std::deque<WorldEntity> enemy_children_;
    WorldEntity* create_enemy_child(RECVX_ENEMY_WORK* parent);
    // player.c:1042 bhCheckMothEgg (aus bhSetPlayer/bhInitRoomChangePlayer/bhResetPlayer, vor
    // bhInitEvent): je gesetzter plp->stflg-Marke 0x8000000/0x10000000/0x20000000 ein EGG_WORK
    // {flg 1, id 27, type 0/1/2} ueber bhSetEnemy. Ohne Elternwork und ohne Modell (mlwP NULL).
    void check_moth_egg();
    // bhSetEnemy(tmpl) fuer ein Kind mit `mlwP = &parent->mdl[base]`; share: denselben
    // ML_WORK wie dieses Kind benutzen (en13.c:170).
    WorldEntity* create_enemy_child(RECVX_ENEMY_WORK* parent,const RECVX_ENEMY_TEMPLATE& tmpl,unsigned base,RECVX_ENEMY_WORK* share);
    // eneset.c:797 bhDrawEnemy fuer die Kindworks aus bhSetEnemy (enemy_children_).
    // mirror: sys->st_flg 0x100 mit gueltigem njMirror, MdlPut.c:57/89 zeichnet jedes Kind ein zweites Mal.
    void append_enemy_children(Scene& scene,bool mirror=false,bool small_screen=false)const;
    // MdlPut.c:40: der Spiegelaufruf liegt im Zweig mlwP->flg & 0x80; mlwP->flg ist das Statusbyte
    // [3] des MDL-Kopfes (binfunc.c:13/34 bhMlbBinRealize).
    bool put_model_cnk(const Model& model)const;
    // op->mdflg eines bhPutModel-Works fuer den Spiegeltest; nullopt ohne nativen Besitzer.
    std::optional<unsigned> put_model_mdflg(const Model& model,const WorldEntity& e)const;
    bool player_mirror_off_=false; // plp->mdflg 0x40 (player.c:1668/1674); bhSetPlayer setzt mdflg = 0x20 (player.c:698/864)
    bool player_model_hidden_=false; // plp->mdflg 0x1 (bhSetDispObj, event.c:2283): nur das Modell, sperrt den Pass nicht
    // effsub3.c:3567 bhEff30cDrw (Effekt 312, ef_fnc): gespiegeltes Klauenmodell unter R0_WK.mtxP; reflected ist
    // der zweite bhDrawEffect aus bhEtcMirrorDrawModel (game.c:427), Lichtkategorie 5 wie bhEff_ModelDraw.
    void append_claw_model(Scene& scene,const EffectSprite& sprite,std::pair<unsigned,unsigned>* fnc_blend,bool reflected)const;
    // effsub4.c:3306 bhEff_ModelDraw (bhEff_Sub364): objP[mdlver].model von mlwP = ene[model_enemy].mdl[model_mdl]
    // unter T(p) * RotZ/Y/X(a) * S(s), txp[0] = mlwP->texP, SimpleMulti-Licht (Kategorie 5).
    // gm_flg traegt das Effekt-mdflg-Tor; reflected ist der bereits geplante zweite Zeichenaufruf.
    void append_effect_model(Scene& scene,const EffectSprite& sprite,const std::vector<SceneTexture>* texture_list,std::pair<unsigned,unsigned>* fnc_blend,unsigned gm_flg,bool reflected)const;
    // game.c:77-91: st_flg 0x100, pt_flg 0x10 und ein gueltiges njMirror(sys->mr_pl) -> bhEtcMirrorDrawModel
    // ruft bhDrawEffect ein zweites Mal (game.c:427).
    bool mirror_effect_pass_active()const;
    // effect.c:929 die schreibenden ef_fnc-Zeichenteile fuer bhDrawEffect-Aufruf `pass` (0 normal, 1
    // Spiegel) dieses Steuerbildes, genau einmal: ein erneutes Zusammensetzen desselben Bildes nimmt
    // die gemerkte Ausgabe statt den Zustand ein weiteres Mal fortzuschreiben.
    void draw_stateful_effects(unsigned pass,const EffectContext& context)const;
    // game.c:61-92 zeichnet jedes Spielbild; hat der Host ein Steuerbild nicht zusammengesetzt,
    // holt der naechste Steuerpass dessen Zeichenteile nach (Zustand wie im Original).
    void draw_pending_stateful_effects();
    std::uint64_t effect_control_serial_=0; // Zaehler der bhControlEffect-Aufrufe
    unsigned effect_control_sp_flg_=0; // sys->sp_flg dieses Steuerbildes (fuer den nachgeholten Zeichenteil)
    void bind_enemy_services();
    void init_enemy_works();
    void control_enemy_works();
    void control_enemy_player_motion();
    int player_set_motion_now(int add,const MotionClip*& clip);
    const MotionClip* player_enemy_clip()const;
    // mlwP eines Works: der Objektbaum, den er liest und schreibt. Ein Kind
    // mit `epp->mlwP = epw->mdl` (en01.c:2827) hat keinen eigenen.
    WorldEntity& tree_owner(WorldEntity& e)const{return e.model_owner?*e.model_owner:e;}
    const WorldEntity& tree_owner(const WorldEntity& e)const{return e.model_owner?*e.model_owner:e;}
    void adopt_enemy_script(WorldEntity& e);
    void publish_enemy(WorldEntity& e);
    const MotionClip* enemy_mn_work(const WorldEntity& e,std::uint32_t mtn_no)const;
    // Ist fuer diesen Work ueberhaupt ein Clip vorhanden? Szenenfiguren wie
    // Identitaet 91 in RM_9301 haben in ihrer RDT keinen emtp-Eintrag (room.c
    // legt dann eine genullte mnwP an): es laeuft dort kein bhSetMotion, also
    // ueberschreibt auch nichts einen Skript-Schreibzugriff auf objP.
    bool enemy_motion_available(const WorldEntity& e,unsigned mtn,bool room)const;
    void calc_enemy_model(WorldEntity& e);
    // eneset.c:797 bhDrawEnemy: Works mit mdflg 0x4 rechnen ihren Baum erst beim
    // Zeichnen (bhCalcModel vor bhPutModel). Einmal je Bild am Ende des Passes.
    void calc_enemy_draw_models();
    void control_item_glint(); // objitm.c:397-450, item part of bhControlObjItm
    void build_object_lists(); // objitm.c:289-383, object part of bhControlObjItm
    void control_alpha_fade(unsigned kind,unsigned index); // objitm.c:740 bhControlAlphaFadeObject
    // pwksub.c:39 bhCalcLinkModel(pp): bhCalcModel fuer jedes aktive Work mit
    // flg 0x80, dessen lkwkp pp ist.
    void calc_enemy_link_model(WorldEntity& e);
    // event.c:11070 (Common 25), :11600 (Common 24), :11941 (Common 34): der
    // ct1-Schalter waehlt nach dem Modusaufbau die sofortige Auswertung
    // bhSetMotion(work,0,0,0) / bhCalcModel / bhCalcLinkModel.
    void enemy_script_evaluate(WorldEntity& e,unsigned mode);
    void sync_player_grab();
    // player.c:1600-1664, the tail of bhControlPlayer that bhCtrPly_mode0 does
    // not gate: the bps probe, the body bhCheckWallEx and bhCheckFloorP also run
    // in the two enemy-driven player modes (bhCPM0_nage/bhCPM0_enedie).
    void control_player_grab_collision();
    std::vector<BoneMatrix> player_joints_now()const;
    const MotionClip& enemy_library_clip(unsigned identity,unsigned index)const;
    void apply_en01_flinch();
    void init_en01(bool hard);
    void init_barrels();
    std::vector<Model> models_;
    // txp[0] is a captured list on the room/model asset, not a copied atlas.
    // models_ is fixed after load; SpriteWork (including snapshots) keeps only
    // the reference and dies with this RoomWorld.
    struct SpriteWork {const std::vector<SceneTexture>* texture_list=nullptr;
        // mutable: die ef_fnc-Zeichenteile schreiben den Satz im Zeichenpass (effect.c:929).
        mutable EffectSprite sprite;NJS_POINT3 scale;
        // O_WRK.mtxbuf: nach bhSetEffect(Tb) genullt, nur originale Matrixschreiber
        // publizieren. Kein T(px)/naechster Motionframe als Ersatz fuer den letzten Zeichenpass.
        mutable BoneMatrix effect_root{};
        // Ausgabe der schreibenden Zeichenteile je bhDrawEffect-Aufruf dieses Steuerbildes.
        struct DrawnPass {std::uint64_t serial=~0ull;std::vector<EffectSprite::Quad> quads;std::vector<SceneLine> lines;};
        mutable std::array<DrawnPass,2> drawn{};bool hidden=false;unsigned link=0,index=0,node=0;NJS_POINT3 offset{};bool player_effect=false,queued=true,dynamic=false;
        // Mitgliedschaft in sys->ef_thl dieses Passes (bhEff111; effect.c:902 bhDrawThlEffect3D).
        bool thl=false;
        // lkwkp = &hp->flg of a room/enemy ATR work (bhEff029): its place in rom->walp + sys->mwalp.
        std::optional<std::size_t> atr_link;std::optional<WeaponFlash> weapon;};
    // bhSetEffect(id, &ps, NULL, 0) + bhEff_AllocOwork von bhEff_SetPtcl1/2/2V (effsub4.c:49/3028).
    SpriteWork* begin_ptcl(unsigned id,NJS_POINT3 pos,const char* what);
    void draw_ufo_hud(SoftwareRenderer& image,const SpriteWork& hud)const;     // effsub1b.c:101 bhDraw134
    void draw_monitor_hud(SoftwareRenderer& image,const SpriteWork& hud)const; // effsub1b.c:828 bhDraw137
    void draw_oil_hud(SoftwareRenderer& image,const SpriteWork& hud)const;     // effsub1b.c:1258 bhDraw138
    void draw_eff021_bars(SoftwareRenderer& image)const;                        // effsub1.c:4756 bhDraw021

    bool player_attacking_=false;int blast_damage_=0;std::int32_t player_dmlvl_=0;bool player_dmlvl_bound_=false,player_poison_request_=false,player_infect_request_=false;bool barrels_ready_=false,enemy_ai_=true,enemy_grab_=true,enemy_attacks_=true,player_enemy_motion_=false,grab_driving_=false,player_dead_=false,player_killed_=false;unsigned lever_bits_=0;int player_hp_=200,player_character_=0;std::int16_t player_way_=0;std::map<unsigned,std::size_t> barrel_walls_;
    float player_spd_=0; // plp->spd
    // plp->spd, das ein Gegnerpass waehrend des Griffs schreibt (en15.c:2915/3118): der Besitzer
    // ist die native Aktion (runtime action.spd); take_player_speed reicht den Wert dorthin zurueck.
    bool player_spd_written_=false;
    // plp->dax/day (en15.c:2098/2153/1379, Rueckstossrichtung des Griffs).
    std::int32_t player_dax_=0,player_day_=0;
    std::int32_t player_axp_=0; // plp->axp (en17.c:1388/1412, bhAddSpeed)
    // player.c:1366-1387 bhControlPlayer: pxb/pyb/pzb of the previous pass and
    // EXP_WORK nlxb/nlyb/nlzb, the last settled root position (owP[0]).
    NJS_POINT3 player_prev_pos_{},player_nl_{};
    bool player_pass_head_=false; // player.c:1365: der Kopf dieses Passes lief (tick_enemy_pass)
    // plp->djnt_no / dvx..dvz / dpx..dpz written by enemy hit routines.
    int player_hit_joint_=-1;NJS_POINT3 player_hit_dv_{},player_hit_dp_{};
    // plp->mlwP->objP[node] writes of the enemy pass; the next player motion
    // write replaces them, so they are removed at the next world tick.
    std::vector<unsigned> player_transient_nodes_;
    std::vector<std::pair<int,int>> vibration_requests_;std::optional<int> vibration_mode_request_;
    std::vector<std::pair<unsigned,float>> player_exp0_writes_;
    std::vector<CollisionWall> enemy_floor_cache_,enemy_step_cache_;
    int effect_template_ay_=0; // sys->ef.ay carried between EN01 writers (RECVX_EN01_EF_KEEP_AY)
    bool enemy_work_index(const RECVX_ENEMY_WORK* w,unsigned& index)const;
    int effect_tb(unsigned flg,int id,int type,unsigned mdlver,NJS_POINT3 position,NJS_POINT3 scale,int ax,int ay,const RECVX_ENEMY_VEC* ofs,RECVX_ENEMY_WORK* link_w,bool link_player,int lkono);
    // bhSetEffect (effect.c:331) / bhSetEffectTb ohne die Prozedurpruefung.
    int allocate_effect(const EffectSpawn& spawn,unsigned link_kind,unsigned link_index);
    std::function<void()> hide_weapon_part2_;std::function<void(bool)> weapon_models_hidden_;
    PlayerObjectLink player_object_link_{};
    // MdlPut.c:242 bhCalcModel(plp) mit flg 0x80: obwp[lkwkp].owP[lkono].mtx * T(lox,loy,loz) * RXYZ(ax,ay,az).
    bool player_link_root(NJS_MATRIX& root)const;
    unsigned player_weapon_=0,effect_yk_ct_=0,presentation_flags_=~0u; // system.c initializes pt_flg to ~0u
    unsigned stage_no_=0,room_no_=0; // sys->stg_no/rom_no aus dem Raumnamen (RoomIdentity)
    unsigned enemy_count_=0; // rom->ene_n
    unsigned effect_flags_=0; // system.c:195 sys->ef_flg = 0
    // sys->zanp - sys->zan_memp: belegte Bytes des 2-MB-Nachbildpuffers (effsub1b.c:627).
    std::size_t zan_used_=0;
    unsigned room_case_=0; // Fall der geladenen Raumdatei (RM_SRRC)
    unsigned pl_sleep_cnt_=0; // system.c:43 pl_sleep_cnt (event.c:2832, game.c:357)
    EyeCamera eye_{};std::int16_t player_wax_=0;unsigned player_aim_mode1_=0,player_aim_at_flg_=0;
    std::uint32_t room_fog_color_=0;float room_fog_near_=0,room_fog_far_=0;
    bool eye_owp5(NJS_MATRIX& m)const;void init_eye_camera(const Scene& stage,std::size_t cut);void init_eye_camera();void set_eye_camera();void control_eye_camera(const Scene& stage,std::size_t cut,bool hold);
    // event.c:9391 Player_controll 131: plp mode0 7 mit mode2 (bhCPM0_event_tbl) und mode1; die Runtime
    // fuehrt pl_smove (player_event.c) ueber ihren Spielerbesitzer aus und meldet hier das Ende.
    struct PlayerEvent{bool requested=false;unsigned mode1=0,mode2=0;bool reset=false;};PlayerEvent player_event_{};
public:
    const PlayerEvent& player_event()const{return player_event_;}
    void clear_player_event_reset(){player_event_.reset=false;}
private:
    // npSetAllMatAlphaColor (njplus.c:1495) aus bhDraw136 schreibt das Alpha dauerhaft in
    // die Diffusmaterialien von objP[node] des Werks. Schluessel (kind,index,mdl,node).
    // before: Stand, den Objekte/Items sehen (bhDrawObjItm vor bhDrawEffect, game.c:341/346),
    // after: nach den Nachbildern dieses Bildes (bhDrawEnemy/bhDrawPlayer danach).
    std::map<std::tuple<unsigned,unsigned,unsigned,unsigned>,unsigned> afterimage_alpha_before_,afterimage_alpha_after_;
    // Laufender Stand waehrend des Zeichenpasses (beginnt bei before).
    mutable std::map<std::tuple<unsigned,unsigned,unsigned,unsigned>,unsigned> afterimage_alpha_running_;
    void control_player_effects();
    std::vector<CollisionWall> effect_walls_; // rom->walp + sys->mwalp as bhCheckWallType sees them
    const std::vector<SceneTexture>* effect_texture_list(const SpriteWork& work)const;
    std::vector<SpriteWork> sprites_;std::map<unsigned,std::vector<SceneTexture>> effect_textures_;
    // sys->efid: Raumzeile -> Platz im bestehenden eff[512]-Besitzer. Bleibt beim
    // Freigeben/Recycling des Zielplatzes unveraendert (room.c:781).
    std::vector<unsigned> room_effect_slots_;
    std::optional<unsigned> light_effect_slot(unsigned index)const;
    std::optional<BoneMatrix> light_effect_matrix(unsigned slot,unsigned node)const;
    // sys->efm[0]/[1]/[7]/[13]: die vier Modellbloecke aus bhInitEffect (effect.c:66-80).
    // bhDrawPolEffect zeichnet objP[op->mdlver].model von efm[op->id], deshalb je Block roh.
    std::vector<std::uint8_t> effect_model0_block_,effect_model1_block_,effect_model7_block_,effect_model13_;
    // sys->ef_tlist.textures: die flache gemeinsame Effekttexturliste (bhDrawPolEffect
    // adressiert sie mit tex_id + ani_ct ohne sys->ef_tn, siehe world.cpp).
    std::vector<SceneTexture> effect_common_textures_;
    // event.c:220 bhPushEffectWork/:234 bhPopEffectWork sichern den ganzen
    // eff[512]-Satz (hier sprites_ plus die Partikelnutzlast des Besitzers) und
    // stellen ihn auf Anforderung wieder her. Das Original kopiert in einen
    // Speicherblock und ersetzt ihn vollstaendig, deshalb ist auch hier genau
    // ein Schnappschuss aktiv.
    struct EffectPushState {std::vector<SpriteWork> works;std::shared_ptr<const EffectParticles::State> particles;};
    std::optional<EffectPushState> effect_push_;
    std::vector<std::shared_ptr<const ShadowGeometry>> shadow_models_;
    std::vector<MotionClip> room_motions_;
    std::vector<std::vector<FaceParameter>> face_sequences_;std::vector<unsigned> face_sequence_ids_;std::vector<std::vector<std::uint8_t>> lip_sequences_;
    mutable std::map<unsigned,MotionClip> room_motion_segments_;
    mutable std::map<const MotionClip*,MotionClip> slow_motion_clips_;
    struct MotionLibrary {std::vector<std::pair<std::size_t,unsigned>> entries;mutable std::map<unsigned,MotionClip> decoded,segmented;};
    std::map<unsigned,MotionLibrary> enemy_motions_;
    // Work clip (mtn_no): what the control pass samples next.
    const MotionClip& entity_motion(const WorldEntity& entity)const;
    // Object-tree clip (objP): what the last control pass actually wrote.
    const MotionClip& posed_motion_clip(const WorldEntity& entity)const;
    const MotionClip& motion_clip(const WorldEntity& entity,unsigned motion,bool room_motion)const;
    const MotionClip& entity_motion_data(const WorldEntity& entity,unsigned motion,bool room_motion)const;
    // Der verknuepfte Oberkoerperwork (bhEne01_SetLinkEnemy, en01.c:2815) teilt
    // sich Objektbaum und owP mit dem Hauptwork; sein bhSetMotion laeuft mit
    // objP/owP += 8 und obj_num -= 8 auf den Knoten 8.. (en01.c:2799-2805,
    // :10347-10353, :3145-3160). Unterer und oberer Teil liegen als getrennte
    // Clips vor: mnwP[mtn] mit 8 Kanaelen und mnwP[mtn + 0xc8] mit 10
    // (en01_walk_mtn, en01.c:1118); im Raumvorrat ist das Paar (mtn, mtn+1).
    // Ein Entity hat nur EINEN Objektbaum, deshalb fuehrt der Host beide
    // Fenster in einem Clip zusammen. Die Kanalordnung bleibt die des
    // Originals: mtn_tp = en01_flipTree (en01.c:2400) bildet Kanal k auf Knoten
    // flipTree[k] ab, und flipTree[8..17] == 8 + flipTree2[0..9] (en01.c:2407).
    // nullptr, wenn kein Paar vorliegt; der Aufrufer behaelt dann den unteren
    // Clip (kein Ersatzclipp, keine erfundene Oberkoerperpose).
    const MotionClip* segmented_clip(const MotionClip& lower,const MotionClip& upper,std::map<unsigned,MotionClip>& cache,unsigned key)const;
    std::vector<CollisionWall> walls_;
    // rom->flrp: die eine Bodenflaechentabelle des Raums (Bodenklang = prm0, Wasser type 3,
    // Gegnerflaechen type 2). Skriptbefehle 0x0c/0x7c schreiben hier; alle Leser sehen dieselben
    // Saetze. effect_water_mobile_ ist sys->mflrp (bhObj001), je Pass aus push_ neu gefuellt.
    std::vector<CollisionWall> room_floors_,effect_water_mobile_;
    EnemySpawnContext enemy_spawn_;
    std::vector<EnemyArea> enemy_areas()const; // ATR_WORK-Sicht fuer activate_enemy_spawn
    std::vector<CollisionWall> route_areas_;std::vector<std::uint8_t> route_table_; // rom->rutp / rom->ruttp (rutchk.c)
    unsigned footstep_slot_=4;
    std::size_t footstep_motion_=~std::size_t(0);
    std::int64_t footstep_frame_=-1;
    float room_height_=0;
    std::array<float,32> floors_{};
    std::vector<EventCameraTrack> event_cameras_;
    unsigned event_camera_key_=0,event_camera_clock_=0;
    // cut.c:1803 bhSetEventHideObjLgt(cno, kno) schreibt die Objekt- und Lichtmasken EINMAL
    // in rom->mdl.objP[].evalflags und rom->evlp[].flg; sie bleiben dort bis zum naechsten
    // Aufruf stehen. Aufgerufen wird sie bei bhSetEventCamera (:1692), bhSetEventFixedCut
    // (:1766), im Initpass (:2020) und nach jedem Keywechsel (:2136) — NICHT aber bei dem
    // Keywechsel, der in das Endhalten fuehrt: cut.c:2124 bricht mit `break` vor dem Aufruf
    // ab. In der Haltephase steht die Kamera also auf dem letzten Key, die Masken aber noch
    // auf dem vorletzten. Die Objektmaske wird aus diesem Key gelesen; die Lichtmaske schreibt
    // set_event_camera_masks einmal in Light::flags, damit bhControlLight sie loeschen kann.
    unsigned event_camera_mask_key_=0;
    void set_event_camera_masks(); // cut.c:1803 bhSetEventHideObjLgt
    bool event_camera_finished_=false;
    // cam.mode0 of bhControlEventCamera (cut.c:1986): 0 = init pass (event_camera_init_),
    // 1 = travelling, 2 = final hold (event_camera_finished_), 3 = held by
    // bhCamPauseSet (event_camera_paused_, event.c:1032; the switch has no case 3).
    bool event_camera_init_=false,event_camera_paused_=false;
    std::optional<SceneCamera> controlled_camera_;unsigned controlled_event_clock_=0;bool controlled_event_init_=false; // control_camera()
    // cam.ips/ian/ipf/icr: the 20 Overhauser control slots of bhInitEventCamera (cut.c:1845).
    std::array<float,60> event_ips_{},event_ian_{},event_ipf_{},event_icr_{};
    struct EventCameraLock {unsigned flg=0,no=0,ono=0;NJS_POINT3 offset{};};
    std::array<const EventCamera*,20> event_camera_control_keys()const;
    EventCameraLock event_camera_lock_fields(unsigned track,unsigned slot)const; // raw CAM_KEYF_WORK slot; kfp[1] past keyf_n reads the following slot
    NJS_POINT3 event_camera_lock_position(const EventCamera& key); // cut.c:2391 bhGetEvtCamLockPosition
    void event_camera_lock_angles(const EventCamera& key,std::int16_t& ax,std::int16_t& ay); // cut.c:2248 / :1682
    void init_event_camera();       // cut.c:1845 bhInitEventCamera
    void check_event_camera_lock(); // cut.c:2221 bhCheckEvtCamLockPosition
    void control_event_camera();    // cut.c:1986 bhControlEventCamera
    SceneCamera event_camera_pose_{};
    // Native camera owner's installed njClipZ and last GameNear/GameFar.
    // Flag/key mutations alone do not invoke either original clip routine.
    ViewClip installed_clip_{},room_clip_{};
    ClipVolume installed_volume_{};
    std::optional<unsigned> clip_cut_;
    bool restore_camera_clip_=false;
    ActiveCameraMotion active_camera_;std::vector<SceneCamera> room_cameras_;
    bool camera_force_=false;
    // cut.c:1197-1217 bhControlActiveCamera: cam.plx/ply/plz, das Verfolgungsziel der Raumkamera.
    // y ohne ci->h: camera_for_actor addiert target_height (ci->h) selbst (:1202/:1208/:1215, :682).
    NJS_POINT3 camera_target_{};bool camera_target_valid_=false;
    // EXP_WORK plx/plz (exp0[24]/[26]): bhInitActiveCamera schreibt gpx/gpz (cut.c:681/683), der
    // Spielerpass unter plp->flg 0x200000 spx/spz (player.c:1657-1661). ply (exp0[25]) hat im Original
    // keinen Leser (nur cut.c:682, player.c:1660, room.c:488/505 schreiben) und wird nicht gefuehrt.
    NJS_POINT3 player_camera_point_{};
    // bhCheckCut(1) aus dem Skript (bhCamSet 1 / bhInitCamSet): bhSetCut(cam.ncut, 1) ruft
    // bhInitActiveCamera (cut.c:442) auch ohne Schnittwechsel. Das Ende der Augenkamera (cut.c:2611)
    // setzt nur cam.flg 0x4 zurueck und initialisiert nicht erneut.
    bool camera_target_init_=false;
    // cut.c:681-1033 bhInitActiveCamera: cam.plx/plz und EXP_WORK plx/plz aus plp->gpx/gpz; bei
    // fehlendem owP[0]-Knoten (stflg 0x30 / mode0 7) bleibt das Ziel unveraendert und wird als Luecke gemeldet.
    void init_camera_target(const Scene& stage,std::size_t cut);
    // plp->gpx/gpz (player.c:1744-1752): owP[0] unter stflg 0x30 oder mode0 7, sonst px/pz. y = plp->py
    // (gpy = bhGetGroundPosition, :1757, ohne nativen Besitzer). Nicht gebunden: kein Knoten.
    std::optional<NJS_POINT3> player_ground_point()const;
    // cam.flg 0x8 und sys->fil_no/fil_rt (cut.c:412-421/505-514); bhInitCamera (camera.c:9) nullt cam.flg.
    bool camera_filter_=false;int filter_no_=0,filter_rate_=0;
    bool camera_cut_wall_=false; // cam.flg 0x10 (cut.c:424/517: ci->flg 0x200)
    // Fog owner (sync.c:143-255 bhControlEOR): cam.fog_* as handed over by
    // bhSetCut/bhSetFixedCut/bhSetEventCamera, sys->fog_ct/fog_cngct, the
    // installed njGenerateFogTable3 state and the njSetFogColor colour.
    std::uint32_t cam_fog_color_=0;float cam_fog_near_=0,cam_fog_far_=0;unsigned cut_fog_flags_=0; // rom->cutp[cam.ncut].cam[camver].flg
    int fog_ct_=0,fog_cngct_=2; // system.c:156: 3 with ss_flg bit 1 (bhChangeHWSetting), else 2; the host has no ss_flg bit 1 owner
    FogState fog_;std::uint32_t fog_color_=0;
    std::uint32_t script_fog_color_=0; // sys->fog_col (bhFogColorSet 0x4c), read while st_flg 0x100000
    void cut_fog_handover(const SceneCamera& ci,bool set_cut); // cut.c:349-377 bhSetCut / :463-484 bhSetFixedCut
    void control_fog(); // sync.c:143-255
    void install_fog(); // njSetFogColor + njGenerateFogTable3 + njSetFogTable + njFogEnable
    std::vector<Light> lights_,event_lights_;
    std::vector<NJS_POINT3> positions_;
    mutable std::map<std::pair<unsigned,unsigned>,std::vector<BoneMatrix>> actor_nodes_;
    std::array<NJS_POINT3,4> ambient_{};
    mutable NJS_POINT3 ambient_material_{1,1,1}; // ps2_NinjaCnk.c:158, persists between model draws
    std::array<unsigned,4> ambient_slots_{};
    std::uint32_t room_background_=0; // rom->bak_col; the installed clear itself is the global Db.clear owner (back_color.h)
    unsigned background_delay_=0;bool background_enabled_=false; // BackColorFlag
    void change_background(bool event);
    std::map<unsigned,ItemBoxObjectWork> item_boxes_; // O_WRK mode0/ct0 of type-100 objects
    void control_water_object(unsigned index,WorldEntity& e);
    unsigned object_pass_count_=0; // sys->gfrm_ct (system.c:556), nur die Paritaet in bhObj005
    WorldEntity& entity(unsigned kind,unsigned index);
    const Model& model(unsigned kind,unsigned index)const;
    unsigned model_index(unsigned kind,unsigned index,unsigned script_node)const;
    const ModelNodeOverrides& overrides(unsigned kind,unsigned index)const;
    bool entity_hidden(unsigned kind,unsigned index)const;
    // objitm.c:562 (bhDrawObjItm, Items), :659 (bhDrawObject) und :688
    // (bhDrawSpObject) teilen dieselbe Bedingung: ein Werk mit flg 0x40000000
    // wird nur gezeichnet, wenn sys->gm_flg & 0x40, cam.flg & 0x46 oder
    // cam.ncut einem seiner vier hide[]-Eintraege entspricht.
    bool object_cut_hidden(const WorldEntity& state,std::optional<unsigned> ncut={})const;
    // pwksub.c:2503 bhCheckClipPoint unter der Kamera des laufenden Bildes; true = ausserhalb.
    bool clip_point(const SceneCamera& view,NJS_POINT3 pos)const;
    std::vector<unsigned> render_texture_slots_; // sys->ren_idx[0..ren_n) (system.c:254 nullt ren_n je Raum)
    bool rendertex_init_=false;                  // Ps2_rendertex_initflag (screen.c:18)
    std::vector<ModelPose> object_pose(const Model& model,const WorldEntity& state)const;
    std::vector<BoneMatrix> object_matrices(const Model& model,const WorldEntity& entity,const NJS_MATRIX& transform)const;
    ModelPose model_pose_physical(unsigned kind,unsigned index,unsigned physical)const; // objP[physical] of the original model
    std::optional<BoneMatrix> link_matrix(unsigned kind,unsigned index,unsigned node,unsigned depth=0)const;
    bool entity_geometry(const Model& model,const WorldEntity& e,Scene& object)const;
    const PlayerAsset* afterimage_asset(unsigned kind,unsigned index,unsigned mdl)const;
    bool afterimage_node_hidden(unsigned kind,unsigned index,unsigned mdl,unsigned node)const;
    // effsub1b.c:713 bhEff136: objP[node] von mdl[mdl] des Werks in seiner jetzigen Lage.
    std::shared_ptr<const EffectAfterimage> capture_afterimage(unsigned kind,unsigned index,unsigned mdl,unsigned node)const;
    // effsub1b.c:690 bhDraw136: ein Nachbild in die Szene, mit dem Materialalpha des Satzes.
    void append_afterimage(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,std::pair<unsigned,unsigned>* fnc_blend,bool mirror=false)const;
    NJS_POINT3 link_point(unsigned kind,unsigned index,unsigned node,NJS_POINT3 offset)const;
    struct RenderScreenVertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t bcol=0,ocol=0;};
    std::array<RenderScreenVertex,256> render_screen_vertices_{}; // sys->rpb, ein Besitzer
    float render_aspect_y_=1.0f; // BHD_ASPECT_Y, system.c:273
    struct Thermometer {unsigned cursor=0;std::array<int,3> digits{{1,0,0}};int direction=0;unsigned texture=0;} thermometer_; // system.c:266-270

};
}
