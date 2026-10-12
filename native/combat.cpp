#include "combat.h"
#include "player_combat_core.h"
#include <functional>
#include "weapon_tables.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace recvx {
#include "combat_tables_data.inc"
bool weapon_shot_sound(unsigned weapon,unsigned& bank,unsigned& program){if(weapon>=23||!WpnTab[weapon].seno0)return false;bank=unsigned(WpnTab[weapon].snd_wpno);program=unsigned(WpnTab[weapon].seno0)&255;return true;}
NJS_POINT3 weapon_fire_offset(unsigned weapon){if(weapon>=23)throw std::runtime_error("weapon outside WpnTab");return WpnTab[weapon].wp_fps1;}
float CombatController::target_distance(NJS_POINT3 origin,NJS_POINT3 direction,float range,NJS_POINT3 position,float radius,float height){
    const float cap=std::min(radius,height*.5f),bottom=position.y+cap,top=position.y+height-cap;
    const float dx=origin.x-position.x,dz=origin.z-position.z,a=direction.x*direction.x+direction.z*direction.z,b=dx*direction.x+dz*direction.z,c=dx*dx+dz*dz-radius*radius;
    float result=range;if(c<=0&&origin.y>=bottom&&origin.y<=top)return 0;
    if(a>1e-8f){const float d=b*b-a*c;if(d>=0){for(const float t:{(-b-std::sqrt(d))/a,(-b+std::sqrt(d))/a}){const float y=origin.y+direction.y*t;if(t>=0&&t<result&&y>=bottom&&y<=top)result=t;}}}
    const float length=direction.x*direction.x+direction.y*direction.y+direction.z*direction.z;
    if(length>1e-8f)for(const float y:{bottom,top}){const float dy=origin.y-y,q=dx*direction.x+dy*direction.y+dz*direction.z,r=dx*dx+dy*dy+dz*dz-radius*radius;if(r<=0)return 0;const float d=q*q-length*r;if(d>=0){const float t=(-q-std::sqrt(d))/length;if(t>=0&&t<result)result=t;}}
    return result;
}
// player_combat_core.h hält WPN_TAB (player.h:7, 0x68 Byte) und EF_WORK
// (types.h:212, 0x28 Byte) layoutgleich zu den hier vorhandenen Tabellen; die
// Tabellenansicht zeigt deshalb auf genau diese eine Tabelle, nicht auf eine
// Kopie (pwksub.c:2748/3266 schreiben WpnEffTab).
static_assert(sizeof(WPN_TAB)==sizeof(RECVX_CB_WPN_TAB)&&sizeof(WPN_TAB)==0x68,"WPN_TAB layout");
static_assert(sizeof(EF_WORK)==sizeof(RECVX_CB_EF_WORK)&&sizeof(EF_WORK)==0x28,"EF_WORK layout");
struct CombatOriginal {
    RECVX_COMBAT_CORE* core=nullptr;
    RECVX_CB_TABLES tables{};RECVX_COMBAT_SYS sys{};RECVX_COMBAT_RESULT result{};unsigned pad=0;
    // Knotenabbildung: die originalen objP/owP-Indizes sind die logischen
    // (Script-)Knoten des vorhandenen Besitzers; motion_angle liest den
    // Winkelkanal über den zugehörigen Modellknoten.
    std::vector<unsigned> script_to_model;
    std::vector<RECVX_CB_MOTION> motions;
    const PlayerAsset* asset=nullptr;
    std::set<std::string> unavailable;
    // Dienste mit vorhandenem Besitzer. Ein ungebundener Dienst wird über
    // `gap` mit dem Originalnamen gemeldet und tut nichts.
    std::function<int()> reload;
    std::function<int()> check_bullet,count_bullet;
    std::function<int(unsigned)> event_flag;
    std::function<void(unsigned)> set_event_flag,pt_flg_or;
    std::function<int(const RECVX_CB_LINE*,unsigned,float*,RECVX_CB_WALL_HIT*)> l2_wall;
    std::function<int(const RECVX_CB_CAPSULE*,unsigned)> c2_wall_n;
    std::function<int(const RECVX_CB_EFFECT*)> effect;
    std::function<void(const RECVX_CB_VEC*,int,int)> weapon_se;
    std::function<void(int,float,float)> footstep;
    std::function<void(const RECVX_CB_LIGHT*)> light0;
    explicit CombatOriginal(const PlayerAsset& player):asset(&player){
        tables.wpn=reinterpret_cast<const RECVX_CB_WPN_TAB*>(WpnTab);
        tables.eff=reinterpret_cast<RECVX_CB_EF_WORK(*)[4]>(WpnEffTab);
        tables.mtn_wpn=PlMtnWpn;tables.knf_atr=KnfAtrTab;
        tables.dam_near=EneDamNear;tables.dam_mid=EneDamMid;tables.dam_far=EneDamFar;
        // WpnAngTab[21][3] (weapon.c:118) fehlt in combat_tables_data.inc und
        // liegt deshalb als reine Datentabelle in player_combat_core.c; der
        // Integrator-Patch trägt sie dort nach.
        tables.ang=RECVX_CB_WPN_ANG_TAB;
        RECVX_COMBAT_SERVICES s{};s.user=this;
        s.gap=[](void* p,const char* name){static_cast<CombatOriginal*>(p)->unavailable.insert(name);};
        s.search_bullet=[](void* p){auto* c=static_cast<CombatOriginal*>(p);return int32_t(c->reload?c->reload():0);};
        s.check_bullet=[](void* p){auto* c=static_cast<CombatOriginal*>(p);return int16_t(c->check_bullet?c->check_bullet():0);};
        s.count_bullet=[](void* p){auto* c=static_cast<CombatOriginal*>(p);return int32_t(c->count_bullet?c->count_bullet():0);};
        s.event_flag=[](void* p,uint32_t no){auto* c=static_cast<CombatOriginal*>(p);return int32_t(c->event_flag?c->event_flag(no):0);};
        s.set_event_flag=[](void* p,uint32_t no){auto* c=static_cast<CombatOriginal*>(p);if(c->set_event_flag)c->set_event_flag(no);};
        s.pt_flg_or=[](void* p,uint32_t bits){auto* c=static_cast<CombatOriginal*>(p);if(c->pt_flg_or)c->pt_flg_or(bits);};
        s.l2_wall=[](void* p,const RECVX_CB_LINE* l,uint32_t f,float* len,RECVX_CB_WALL_HIT* hit){auto* c=static_cast<CombatOriginal*>(p);return int32_t(c->l2_wall?c->l2_wall(l,f,len,hit):0);};
        s.c2_wall_n=[](void* p,const RECVX_CB_CAPSULE* cap,uint32_t f){auto* c=static_cast<CombatOriginal*>(p);return int32_t(c->c2_wall_n?c->c2_wall_n(cap,f):0);};
        s.effect=[](void* p,const RECVX_CB_EFFECT* e){auto* c=static_cast<CombatOriginal*>(p);return int32_t(c->effect?c->effect(e):-1);};
        s.weapon_se=[](void* p,const RECVX_CB_VEC* pos,int32_t se,int32_t slot){auto* c=static_cast<CombatOriginal*>(p);if(c->weapon_se)c->weapon_se(pos,se,slot);};
        s.footstep=[](void* p,int32_t floor,float x,float z){auto* c=static_cast<CombatOriginal*>(p);if(c->footstep)c->footstep(floor,x,z);};
        s.light0=[](void* p,const RECVX_CB_LIGHT* l){auto* c=static_cast<CombatOriginal*>(p);if(c->light0)c->light0(l);};
        // GetOneObjectMotion (playpch.c:185) über die vorhandene Motionquelle.
        s.motion_angle=[](void* p,uint32_t mtn,uint32_t node,uint32_t frame,int32_t* ang)->int32_t{
            auto* c=static_cast<CombatOriginal*>(p);
            if(!c->asset||node>=c->script_to_model.size())return 0;
            if(!c->asset->has_motion(mtn))return 0;
            const auto& clip=c->asset->motion_clip(mtn);
            const unsigned model=c->script_to_model[node];
            if(model>=clip.angles.size()||clip.angles[model].empty())return 0;
            const auto& keys=clip.angles[model];
            const auto& key=keys[std::min<std::size_t>(frame,keys.size()-1)];
            ang[0]=key[0];ang[1]=key[1];ang[2]=key[2];return 1;};
        // Ungebundene Besitzer: Gegnertabelle (C02 ist in world noch nicht
        // exponiert), Wasserabfragen (P03/E01), Wandzerstörung, Funken/
        // Spritzer/Licht (E-Pakete) und Vibration ohne Treiber. Sie bleiben
        // NULL; der Kern meldet sie namentlich über `gap`.
        core=recvx_combat_create(&tables,&s);if(!core)throw std::bad_alloc();
        // Logische Knotenzahl des Körpers (ActorNodeLayout script_to_model).
        {std::vector<BoneMatrix> ident(player.bones());for(auto& m:ident)njUnitMatrix(&m.value);
         const auto script=player.script_nodes(ident);script_to_model.resize(script.size());
         for(unsigned n=0;n<script.size();++n)script_to_model[n]=player.model_index(n);}
        refresh_motions(player);
        recvx_combat_room_reset(core);
    }
    ~CombatOriginal(){recvx_combat_destroy(core);}
    // mnwP: MN_WORK-Sicht auf die vorhandenen Spielermotions (Basis plus
    // bhReadWeaponData). frm_num/flg/has_data stammen aus dem MTN-Kopf.
    void refresh_motions(const PlayerAsset& player){
        motions.assign(player.motions(),RECVX_CB_MOTION{});
        for(std::size_t i=0;i<motions.size();++i){
            if(!player.has_motion(i))continue;
            const auto& clip=player.motion_clip(i);
            motions[i].flg=clip.flags;motions[i].frm_num=int32_t(clip.frames);
            motions[i].obj_num=int32_t(clip.angles.size());motions[i].has_data=1;
        }
        auto& view=player_view();
        view.mnw=motions.data();view.mnw_n=unsigned(motions.size());
    }
    RECVX_COMBAT_PLAYER& player_view(){return *recvx_combat_player(core);}
    RECVX_COMBAT_PLAYER& player(){return player_view();}
};
// bhCPM1_act_atk (player.c:4481) verteilt auf plp->mode2; bhCPM2_act_suw
// (player.c:4545) und die übrigen mode2-Zweige schalten auf plp->mode3.
unsigned CombatController::original_mode()const{return original_?unsigned(original_->player().mode2):0;}
unsigned CombatController::original_phase()const{return original_?unsigned(original_->player().mode3):0;}
// player.c:1809/1815: der Pass gehört bhCPM1_act_atk genau bei mode1 != 0.
bool CombatController::aiming()const{return original_&&original_->result.active;}
// mode2 69/169 = bhCPM2_act_atk/_atk_pch (Schuss), 72 = bhCPM2_act_knf
// (Messer) — die drei zuschlagenden Zweige der Tabelle in player.c:4527-4563.
bool CombatController::attacking()const{const unsigned m=original_mode();return original_&&(m==69||m==169||m==72);}
std::int16_t CombatController::aim_yaw()const{return original_?original_->player().way:std::int16_t(0);}
// Nur die Knoten, die der Kampfpass selbst geschrieben hat (PlyPchMain
// playpch.c:112-130). Alles andere bleibt die Pose der Motionquelle.
ModelNodeOverrides CombatController::pose_overrides(const PlayerAsset&,std::size_t,std::int64_t)const{
    if(!original_)return {};ModelNodeOverrides out;const auto& p=original_->player();
    for(unsigned n=0;n<p.nodes&&n<original_->script_to_model.size();++n){
        if(!(p.obj_dirty[n>>5]&(1u<<(n&31))))continue;const auto& o=p.obj[n];
        out[original_->script_to_model[n]]={NJS_POINT3{o.pos[0],o.pos[1],o.pos[2]},std::array<Angle,3>{Angle(o.ang[0]),Angle(o.ang[1]),Angle(o.ang[2])}};}
    return out;
}
bool CombatController::tick(CombatControls input,bool blocked,Inventory& inventory,PlayerAsset& player,SceneSpawn& actor,std::size_t& motion,std::int64_t& frame,RoomWorld* world,AudioMixer& audio,EventVm* events){
    body_=&player;
    const auto weapon=inventory.weapon();if(weapon!=weapon_||inventory.player!=player_){original_.reset();weapon_=weapon;player_=inventory.player;asset_.reset();
        if(weapon<=20&&!data_.empty()){asset_=WeaponAsset::load(data_,inventory.player,weapon,mods_);if(weapon)player.install_weapon_motions(asset_->motions);bare_hands_=weapon?WeaponAsset::load(data_,inventory.player,0,mods_):asset_;}}
    const bool supported=weapon_==2||weapon_==3||weapon_==4||weapon_==5||weapon_==6||weapon_==7||weapon_==8||weapon_==9||weapon_==12||weapon_==13;
    if(blocked||!supported){original_.reset();aim_={};if(blocked&&(input.aim||input.attack))inhibited_=true;return false;}
    if(inhibited_){if(input.aim||input.attack)return false;inhibited_=false;}
    if(!original_){if(!input.aim)return false;original_=std::make_shared<CombatOriginal>(player);recvx_combat_set_weapon(original_->core,int(weapon_));}
    auto& engine=*original_;
    // bhSearchBullet (sub1.c:8862) liegt beim vorhandenen Inventarbesitzer.
    engine.reload=[&]{return inventory.reload()?1:0;};
    // weapon.c:421 bhCheckBullet: swork.pip[pip[0]], -1 bei Endlosmunition
    // (Flagbit 0x800) oder plp->at_flg 0x40000000.
    engine.check_bullet=[&]()->int{
        const auto slot=inventory.slots()[0];if(!slot||slot>=12)return 0;
        const auto entry=inventory.slots()[slot];
        if(((entry>>16)&0x800)||inventory.unlimited_attack)return -1;
        return int(std::int16_t(entry&0xffff));};
    // weapon.c:439 bhCountBullet beim vorhandenen Inventarbesitzer.
    engine.count_bullet=[&]{return inventory.count_bullet()?1:0;};
    // bhCkFlg/bhStFlg(sys->ev_flg, n): Flagmatrix 1 des vorhandenen
    // Eventbesitzers (snapshot.cpp:73 bindet genau sie an ev_flg).
    engine.event_flag=[&](unsigned no){return events?(events->flag(1,no)?1:0):0;};
    engine.set_event_flag=[&](unsigned no){if(events)events->set_flag(1,no,0);};
    // sys->pt_flg gehoert RoomWorld::set_presentation_flags; ein Leseweg
    // fehlt, deshalb wird der Zugriff namentlich gemeldet (Anschlussvertrag).
    engine.pt_flg_or=[&](unsigned bits){(void)bits;engine.unavailable.insert("sys->pt_flg (RoomWorld ohne Leseweg)");};
    // CallPlayerWeaponSeEx(pPos, SeNo, SlotNo): WpnTab[w].snd_wpno liefert das Paket mit SE-Bank 1;
    // Slotwahl 8/9/19 und Get3DSoundParameter im gemeinsamen Mixerpfad, gm_flg 0x01000000 = Syukan.
    engine.weapon_se=[&](const RECVX_CB_VEC* pos,int se,int slot){
        audio.call_player_weapon_se(unsigned(WpnTab[weapon_].snd_wpno),se,{pos->x,pos->y,pos->z},slot,events&&(events->flags[11][0]&0x01000000u));};
    // CallPlayerFootStepSe(bhCheckFloorSound(...),1,1).
    // CallPlayerFootStepSe(bhCheckFloorSound(...),1,1) -> CallPlayerFootStepSeEx(..,&PlayerPos); x/z bestimmen nur den Bodenklang.
    engine.footstep=[&](int,float x,float z){if(!world)return;const unsigned surface=world->floor_sound({x,0,z});if(surface<=4)audio.player_footstep_se(surface,1,true,events&&(events->flags[11][0]&0x01000000u));};
    // bhCheckL2Wall (pwksub.c:718) gegen den vorhandenen Raumkollisionssatz.
    // Der ATR-Satz (sys->ahtp) ist noch nicht exponiert; der Kern meldet ihn.
    engine.l2_wall=[&](const RECVX_CB_LINE* l,unsigned flags,float* len,RECVX_CB_WALL_HIT* hit)->int{
        if(!world||!l)return 0;NJS_POINT3 dir{l->vx,l->vy,l->vz};const float length=njUnitVector(&dir);if(length<=0)return 0;
        const float reach=world->combat_wall_distance({l->px,l->py,l->pz},dir,length,flags);
        if(reach>=length)return 0;if(len)*len=reach;
        if(hit){*hit={};hit->apos={l->px+dir.x*reach,l->py+dir.y*reach,l->pz+dir.z*reach};}
        return 1;};
    // bhCheckC2WallN (pwksub.c:2172): Sichtkapsel gegen denselben Satz.
    engine.c2_wall_n=[&](const RECVX_CB_CAPSULE* cap,unsigned flags)->int{
        if(!world||!cap)return 0;
        NJS_POINT3 dir{cap->c2.x-cap->c1.x,cap->c2.y-cap->c1.y,cap->c2.z-cap->c1.z};
        const float length=njUnitVector(&dir);if(length<=0)return 0;
        return world->combat_wall_distance({cap->c1.x,cap->c1.y,cap->c1.z},dir,length,flags)<length?1:0;};
    // bhSetGunFire (pwksub.c:2670) fordert ueber bhSetEffectTb die Arbeit aus WpnEffTab[wno][0]
    // an, bhSetYakkyou (pwksub.c:3242) die aus [wno][3] und [wno][2]. flg/id/type/mdlver stammen
    // also aus der Originaltabelle; id 3 ist das Muendungsfeuer (eigener Zeichenweg), alle
    // uebrigen portierten ids laufen ueber den gemeinsamen bhSetEffectTb-Pfad (z. B. bhEff000).
    engine.effect=[&](const RECVX_CB_EFFECT* e)->int{
        if(!world)return -1;
        world->set_effect_counter(engine.sys.yk_ct);
        if(e->ef.id==3)return world->spawn_weapon_flash(*e,1,0);
        EffectSpawn spawn;spawn.flg=e->ef.flg;spawn.id=e->ef.id;spawn.type=e->ef.type;spawn.flr_no=e->ef.flr_no;spawn.mdlver=e->ef.mdlver;
        spawn.position={e->ef.px,e->ef.py,e->ef.pz};spawn.scale={e->ef.sx,e->ef.sy,e->ef.sz};spawn.ax=e->ef.ax;spawn.ay=e->ef.ay;spawn.lkono=e->jno;
        spawn.has_offset=e->has_offset!=0;spawn.offset={e->offset.x,e->offset.y,e->offset.z};
        return world->spawn_player_effect(spawn);};
    engine.light0=[&](const RECVX_CB_LIGHT* l){if(world)world->set_gun_light(*l);};
    // Kampfsicht auf den einen Spielerwork: Lage/Winkel kommen je Pass vom
    // Hostakteur und gehen danach dorthin zurück (Header Z. 25-29).
    auto& view=engine.player();
    if(engine.motions.size()!=player.motions())engine.refresh_motions(player);
    view.ply_id=int(player_);view.wpnr_no=int(weapon_);
    view.px=actor.position.x;view.py=actor.position.y;view.pz=actor.position.z;view.ay=actor.yaw;
    view.mtn_no=unsigned(motion);view.frm_no=int(frame);
    publish_pose(player,motion,frame,actor);
    // bhSetPad (pad.c:162) und die Flagbänke bhCk 11/5/6 des vorhandenen Besitzers.
    const unsigned pad=(input.aim?0x10u:0)|(input.attack?0x100u:0)|(input.vertical>0?0x20u:input.vertical<0?0x40u:0)|(input.turn>0?8u:input.turn<0?4u:0)|(input.next_target?0x80u:0);
    engine.sys.pad_on=pad;engine.sys.pad_ps=pad&~engine.pad;
    engine.sys.gm_flg=events?&events->flags[11][0]:nullptr;
    engine.sys.st_flg=events?&events->flags[5][0]:nullptr;
    engine.sys.sp_flg=events?&events->flags[6][0]:nullptr;
    // sys->gm_flg 0x40000 (Magazin leer) hat im Original genau einen Platz;
    // der Inventarbesitzer setzt es in bhCountBullet, der Kampf löscht es.
    if(events&&inventory.chamber_empty)events->flags[11][0]|=0x40000u;
    const unsigned old_shots=engine.result.shots,old_hits=engine.result.hits,old_empty=engine.result.empty;const bool was_attacking=attacking();
    recvx_combat_tick(engine.core,&engine.sys,&engine.result);engine.pad=pad;
    if(events)inventory.chamber_empty=(events->flags[11][0]&0x40000u)!=0;
    if(engine.sys.cam_written)engine.unavailable.insert("cam.pe_ax/cam.pe_pers (Kamerabesitzer)");
    if(world)world->set_effect_counter(engine.sys.yk_ct);
    shots+=engine.result.shots-old_shots;hits+=engine.result.hits-old_hits;empty+=engine.result.empty-old_empty;
    if(!was_attacking&&attacking()){++attacks;struck_.clear();}
    if(engine.result.active){actor.yaw=view.ay;actor.position.x=view.px;actor.position.z=view.pz;motion=view.mtn_no;frame=view.frm_no;}
    vertical_=input.vertical;
    // Nach bhCalcModel: Messerabfrage (player.c:1689-1724) und bhObjWpn.
    publish_pose(player,motion,frame,actor);
    recvx_combat_after_model(engine.core,&engine.sys,&engine.result);
    // PlyPchMain führt die Aufschaltung in pp.lck_tbl als RECVX_ENEMY_WORK*;
    // owner_index ist der Raumindex des vorhandenen Besitzers. Ohne die noch
    // fehlende C02-Bindung der Gegnertabelle in world bleibt sie leer.
    aim_={};aim_.yaw=view.ay;aim_.right_angle=view.pp.obj_r_ang[1];aim_.left_angle=view.pp.obj_l_ang[1];
    if(view.pp.lck_n>0&&view.pp.lck_tbl[0])aim_.right=view.pp.lck_tbl[0]->owner_index;
    if(view.pp.lck_n>1&&view.pp.lck_tbl[1])aim_.left=view.pp.lck_tbl[1]->owner_index;
    if(!engine.result.active){motion=42;frame=0;original_.reset();aim_={};return false;}return true;
}
// objP des Originals: die Pose, die das Motionsampling geschrieben hat, plus
// die Gelenkmatrizen des letzten bhCalcModel. Beides je Pass neu.
void CombatController::publish_pose(PlayerAsset& player,std::size_t motion,std::int64_t frame,const SceneSpawn& actor){
    auto& engine=*original_;auto& view=engine.player();
    const auto overrides=pose_overrides(player,motion,frame);
    if(!player.has_motion(motion)){view.nodes=0;return;}
    const auto local=player.local_pose(motion,frame,overrides);
    const auto bones=player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw,overrides));
    view.nodes=unsigned(std::min<std::size_t>(RECVX_CB_NODES,std::min(bones.size(),engine.script_to_model.size())));
    for(unsigned n=0;n<view.nodes;++n){
        std::copy_n(bones[n].value,16,view.owp_mtx[n]);
        const unsigned m=engine.script_to_model[n];if(m>=local.size())continue;
        view.obj[n].pos[0]=local[m].position.x;view.obj[n].pos[1]=local[m].position.y;view.obj[n].pos[2]=local[m].position.z;
        view.obj[n].ang[0]=local[m].angles[0];view.obj[n].ang[1]=local[m].angles[1];view.obj[n].ang[2]=local[m].angles[2];
    }
}
const std::set<std::string>& CombatController::unbound_routines()const{
    static const std::set<std::string> none;return original_?original_->unavailable:none;
}
void CombatController::append(Scene& scene,const std::vector<BoneMatrix>& bones,bool scripted,const RoomWorld*)const{
    if(bones.empty())return;
    const auto hands=body_?body_->script_nodes(bones):bones;
    if(scripted){if(bare_hands_)bare_hands_->append(scene,hands);}
    else if(asset_)asset_->append(scene,hands);
}
}
