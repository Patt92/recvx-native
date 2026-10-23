/* C04 Komponentenpruefung fuer Identitaet 4 (Cerberus).
   Jede behauptete Zahl steht mit Datei:Zeile im Original
   src/ps2/veronica/prog/en04.c bzw. zonzon.c; keine Erwartung stammt aus
   dem Host oder aus dem neuen Code selbst. Die Dienstfixture ist eine
   diagnostische Aufstellung und KEIN Runtime-/Gameplaynachweis. */
#include "en04.h"
#include "en04_tables.h"
#include "ninja_matrix.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace recvx { extern const int en04_hp_tbl[2][16]; }

static void require(bool b,const char* msg){ if(!b) throw std::runtime_error(msg); }

namespace {

/* en04.c:390 en04_hp_tbl[2][16] woertlich. */
const int hp_tbl[2][16] = {
    { 60, 60, 60, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 120, 120, 120 },
    { 30, 30, 30, 45, 45, 45, 45, 45, 60, 60, 60, 60, 60,  75,  75,  75 }
};

std::vector<std::string> gaps;
std::vector<void*> allocations;
std::vector<int> sounds;
std::vector<int> voices;
std::vector<int> vibrations;
std::vector<std::string> trace;
RECVX_ENEMY_PLAYER ply;
RECVX_ENEMY_ATR floors[8];
unsigned floor_count=0;
RECVX_EN04_OBJECT objects[8];
unsigned object_total=0;
RECVX_ENEMY_ATR stair_rec;
int stair_present=0,stair_index=0;
int32_t random_value=0;
unsigned random_calls=0,models=0,motions=0,shadow_calls=0;
uint32_t material_color=0;
int wall_hit_result=0,side_left=0,side_right=0,line_hit_result=0,sc_result=0;
uint8_t line_hit_type=0;
uint32_t frames_per_motion=20;
int lever=0;
float ground_height=0.0f;
int32_t pad_cleared=0;
int32_t hit_joint=-1;
std::vector<std::pair<uint32_t,float>> node_pos_writes;
int32_t node_angles[8][3];
int blood_pool_calls=0;
RECVX_ENEMY_BP_WORK blood_pool_seen{};
int player_blood_calls=0;

void record_gap(const char* n){ gaps.emplace_back(n); }
void* allocate(uint32_t n){ auto* p=std::calloc(1,n); allocations.push_back(p); return p; }
void clear_allocations(){ for(auto* p:allocations) std::free(p); allocations.clear(); }
int get_player(RECVX_ENEMY_PLAYER* p){ *p=ply; return 1; }
void set_player(const RECVX_ENEMY_PLAYER* p){ ply=*p; }
int32_t random_next(){ ++random_calls; return random_value; }
uint32_t motion_frames(const RECVX_ENEMY_WORK*,uint32_t){ return frames_per_motion; }
uint32_t obj_num(const RECVX_ENEMY_WORK*){ return 19; }
int set_motion(RECVX_ENEMY_WORK*,int,uint32_t){ ++motions; trace.emplace_back("motion"); return 0; }
void calc_model(RECVX_ENEMY_WORK*){ ++models; trace.emplace_back("model"); }
int joint_matrix(const RECVX_ENEMY_WORK* w,uint32_t j,float* out){
    std::memset(out,0,16*sizeof(float)); out[0]=out[5]=out[10]=out[15]=1.0f;
    out[12]=w->px+float(j); out[13]=w->py+1.0f; out[14]=w->pz; return 1;
}
int joint_point(const RECVX_ENEMY_WORK* w,uint32_t j,const RECVX_ENEMY_VEC* l,RECVX_ENEMY_VEC* o){
    o->x=w->px+float(j)+l->x; o->y=w->py+1.0f+l->y; o->z=w->pz+l->z; return 1;
}
int player_joint_matrix(uint32_t j,float* out){
    std::memset(out,0,16*sizeof(float)); out[0]=out[5]=out[10]=out[15]=1.0f;
    out[12]=ply.pos.x; out[13]=ply.pos.y; out[14]=ply.pos.z; (void)j; return 1;
}
float grand(uint32_t){ return ground_height; }
int floor_num(float){ return 1; }
int lever_check(){ return lever; }
int wall_type(const RECVX_ENEMY_VEC*,uint32_t,float,float){ return wall_hit_result; }
int wall_move(RECVX_ENEMY_WORK*,RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC*,float,float,uint32_t){ return 0; }
int check_player(RECVX_ENEMY_WORK*){ trace.emplace_back("checkplayer"); return 0; }
int check_enemies(RECVX_ENEMY_WORK*){ trace.emplace_back("checkenemies"); return 0; }
uint32_t enemy_now(){ return 3; }
void enemy_se(uint32_t,const RECVX_ENEMY_VEC*,int se){ sounds.push_back(se); }
void player_voice(int se){ voices.push_back(se); }
void vibration(int motor,int power){ require(motor==1,"zonzon.c:1346 uses motor 1"); vibrations.push_back(power); }
int set_shadow(const int8_t* t,RECVX_ENEMY_WORK*,int mode,float a,float b,float c){
    ++shadow_calls;
    /* en04.c:259 En04SdwTab und en04.c:998 bhSetShadow(...,1,3,3,6). */
    require(t[0]==5&&t[1]==15&&t[2]==18&&t[3]==8&&t[4]==11&&t[5]==-1,
            "En04SdwTab en04.c:259 differs");
    require(mode==1&&a==3.0f&&b==3.0f&&c==6.0f,"bhSetShadow en04.c:998 arguments differ");
    return 1;
}
int sphere_capsule(const RECVX_ENEMY_VEC*,float r,const RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC*,float){
    require(r==1.2f,"en04.c:3579 sphere radius differs"); return sc_result;
}
void stair_use(int,int){}
uint32_t enemy_count(){ return 0; }
RECVX_ENEMY_WORK* enemy_at(uint32_t){ return nullptr; }
int line(uint32_t,const RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC* n,uint32_t,int,
         RECVX_ENEMY_ATR* rec,uint32_t*){
    if(n){ n->x=0; n->y=0; n->z=-1; }
    if(rec){ *rec={}; rec->type=line_hit_type; }
    return line_hit_result;
}

uint32_t flr_count(){ return floor_count; }
int flr_at(uint32_t i,RECVX_ENEMY_ATR* out){ if(i>=floor_count) return 0; *out=floors[i]; return 1; }
int stair_area_atr(RECVX_ENEMY_WORK*,float,RECVX_ENEMY_ATR* rec,int* idx){
    if(!stair_present) return 0; *rec=stair_rec; *idx=stair_index; return 1;
}
/* route_zone 0: bhCheckRoute ohne Wegezone (rutchk.c:14-24, 0xFF, pos unberuehrt). */
int route_zone=1; RECVX_ENEMY_VEC route_point{};
int check_route(const RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC* to,RECVX_ENEMY_VEC* out){
    if(!route_zone) return 0;
    *out = route_point.x||route_point.z ? route_point : *to; return 1; }
int collision_check_wall2(RECVX_ENEMY_WORK*,const RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC*,
                          RECVX_ENEMY_VEC* pd,float,float){ *pd={}; return 0; }
void calc_damage(RECVX_ENEMY_WORK*,const RECVX_EN04_COMBWEP*,const RECVX_EN04_COMBJOINT*){}
int weapon_flg(uint32_t,uint32_t* out){ *out=0; return 1; }
void set_material_color(RECVX_ENEMY_WORK*,uint32_t c){ material_color=c; }
int node_transform(const RECVX_ENEMY_WORK*,uint32_t,RECVX_ENEMY_VEC* p,int32_t* a){
    *p={}; a[0]=a[1]=a[2]=0; return 1;
}
void node_pos_set(RECVX_ENEMY_WORK*,uint32_t,uint32_t axis,float v){ node_pos_writes.emplace_back(axis,v); }
void node_ang_set(RECVX_ENEMY_WORK*,uint32_t n,uint32_t axis,int32_t v){ node_angles[n][axis]=v; }
void set_blood(RECVX_ENEMY_WORK*,uint8_t,const RECVX_EN04_BT*){}
void set_blood2(RECVX_ENEMY_WORK*,uint8_t,RECVX_ENEMY_VEC*,int32_t){}
void player_set_blood(uint8_t t,const RECVX_EN04_BT*){ require(t==1,"en04.c:2976 player blood type"); ++player_blood_calls; }
void player_set_blood2(uint8_t,RECVX_ENEMY_VEC*,int32_t){}
int set_dfire(RECVX_ENEMY_WORK*,int,const RECVX_EN04_BT*,int){ return 0; }
int set_san(RECVX_ENEMY_WORK*,int,const RECVX_EN04_BT*){ return 0; }
void nikuhen(RECVX_ENEMY_WORK*,int,const RECVX_ENEMY_VEC*,int,int){}
int blood_pool_lnk(RECVX_ENEMY_WORK*,const RECVX_ENEMY_VEC*,int32_t,const RECVX_ENEMY_BP_WORK* t,int bnk){
    ++blood_pool_calls; blood_pool_seen=*t;
    require(bnk==0,"en04.c:3379 pal_bnk differs"); return 1;
}
void player_hit_check_parts(const RECVX_ENEMY_VEC*){}
void player_hit_set(int32_t j,const RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC*){ hit_joint=j; }
int player_hit_get(int32_t* j,RECVX_ENEMY_VEC* dv,RECVX_ENEMY_VEC*){ if(j)*j=0; if(dv)*dv={}; return 1; }
int player_capsule(RECVX_ENEMY_CAPSULE* out){ *out={}; out->r=1.0f; return 1; }
void player_node_pos_set(uint32_t,uint32_t,float){}
void player_exp0_float_set(uint32_t,float){}
void pad_on_clear(uint32_t bits){ pad_cleared|=int32_t(bits); }
uint32_t object_count(){ return object_total; }
int object_get(uint32_t i,RECVX_EN04_OBJECT* o){ if(i>=object_total) return 0; *o=objects[i]; return 1; }
void object_set(uint32_t i,const RECVX_EN04_OBJECT* o){ if(i<object_total) objects[i]=*o; }
int player_speed(float* out){ *out=0.0f; return 1; }
int player_collision_parts(uint32_t,uint32_t,RECVX_ENEMY_VEC* d){ *d={}; return 1; }

RECVX_ENEMY_SERVICES services(){
    RECVX_ENEMY_SERVICES s{};
    s.gap=record_gap; s.calloc_work=allocate; s.player_get=get_player; s.player_set=set_player;
    s.random_next=random_next; s.motion_frames=motion_frames; s.obj_num=obj_num;
    s.set_motion=set_motion; s.calc_model=calc_model; s.joint_matrix=joint_matrix;
    s.joint_point=joint_point; s.player_joint_matrix=player_joint_matrix;
    s.grand=grand; s.floor_num=floor_num; s.lever_check=lever_check;
    s.wall_type=wall_type; s.wall_move=wall_move; s.check_player=check_player;
    s.check_enemies=check_enemies; s.enemy_now=enemy_now; s.enemy_se=enemy_se;
    s.player_voice=player_voice; s.vibration=vibration; s.set_shadow=set_shadow;
    s.sphere_capsule=sphere_capsule; s.stair_use=stair_use; s.enemy_count=enemy_count;
    s.enemy_at=enemy_at; s.line=line;
    return s;
}
RECVX_EN04_SERVICES services04(){
    RECVX_EN04_SERVICES s{};
    s.flr_count=flr_count; s.flr_at=flr_at; s.stair_area_atr=stair_area_atr;
    s.check_route=check_route; s.collision_check_wall2=collision_check_wall2;
    s.calc_damage=calc_damage; s.weapon_flg=weapon_flg; s.set_material_color=set_material_color;
    s.node_transform=node_transform; s.node_pos_set=node_pos_set; s.node_ang_set=node_ang_set;
    s.set_blood=set_blood; s.set_blood2=set_blood2;
    s.player_set_blood=player_set_blood; s.player_set_blood2=player_set_blood2;
    s.set_dfire_effect=set_dfire; s.set_san_effect=set_san; s.nikuhen_effect2=nikuhen;
    s.blood_pool_lnk=blood_pool_lnk; s.player_hit_check_parts=player_hit_check_parts;
    s.player_hit_set=player_hit_set; s.player_hit_get=player_hit_get;
    s.player_capsule=player_capsule; s.player_node_pos_set=player_node_pos_set;
    s.player_exp0_float_set=player_exp0_float_set; s.pad_on_clear=pad_on_clear;
    s.object_count=object_count; s.object_get=object_get; s.object_set=object_set;
    s.player_speed=player_speed; s.player_collision_parts=player_collision_parts;
    return s;
}

RECVX_EN04_SERVICES ext;
RECVX_ENEMY_SERVICES base;

void reset(){
    clear_allocations(); gaps.clear(); sounds.clear(); voices.clear(); vibrations.clear();
    trace.clear(); ply={}; floor_count=0; object_total=0; stair_present=0; stair_rec={};
    random_value=0; random_calls=models=motions=shadow_calls=0; material_color=0;
    wall_hit_result=side_left=side_right=line_hit_result=sc_result=0; line_hit_type=0;
    frames_per_motion=20; lever=0; ground_height=0.0f; pad_cleared=0;
    node_pos_writes.clear(); std::memset(node_angles,0,sizeof node_angles);
    blood_pool_calls=0; blood_pool_seen={}; player_blood_calls=0; hit_joint=-1;
    base=services(); ext=services04();
    RECVX_EN04_CONTEXT ctx{}; ctx.en04=&ext; ctx.game_mode=0;
    recvx_en04_set_context(&ctx);
    RECVX_EN04_DATA data{}; data.hp_tbl=hp_tbl; recvx_en04_set_data(&data);
    recvx_en04_reset_shared();
}

RECVX_ENEMY_WORK make(){ RECVX_ENEMY_WORK w{}; w.id=4; w.flg=1; w.sv=&base; return w; }

/* ---------------------------------------------------------------------------
   1) Reine Originaldaten
   --------------------------------------------------------------------------- */
void tables(){
    /* en04.c:26 En04_PlyMtn_OffsetTbl = { 0x64, 0x69, 0x64, 0x69 }. */
    require(recvx_en04_player_mtn[0]==100&&recvx_en04_player_mtn[1]==105&&
            recvx_en04_player_mtn[2]==100&&recvx_en04_player_mtn[3]==105,
            "En04_PlyMtn_OffsetTbl en04.c:26 differs");
    /* en04.c:254 en04_tree[1][8] und :259 En04SdwTab[6]. */
    require(recvx_en04_tree[0]==0&&recvx_en04_tree[5]==5&&recvx_en04_tree[6]==255,
            "en04_tree en04.c:254 differs");
    require(recvx_en04_shadow[0]==5&&recvx_en04_shadow[2]==18&&recvx_en04_shadow[5]==-1,
            "En04SdwTab en04.c:259 differs");
    /* en04.c:264 En04_WpnDamageTbl: Zeile 15 { 0x18, 0, 1, 3, 3 } in der
       Feldfolge types.h:1931 {flg, nm_act, nm_blood, cb_act, cb_blood}. */
    require(recvx_en04_wpn_damage[15].flg==0x18&&recvx_en04_wpn_damage[15].nm_act==0&&
            recvx_en04_wpn_damage[15].nm_blood==1&&recvx_en04_wpn_damage[15].cb_act==3&&
            recvx_en04_wpn_damage[15].cb_blood==3,"En04_WpnDamageTbl[15] en04.c:285 differs");
    /* en04.c:274 Zeile 4 (Handfeuerwaffe) { 0x00, 0, 7, 3, 3 }: nm_blood 7, cb_act 3. */
    require(recvx_en04_wpn_damage[4].nm_blood==7&&recvx_en04_wpn_damage[4].cb_act==3&&
            recvx_en04_wpn_damage[4].cb_blood==3,"En04_WpnDamageTbl[4] en04.c:274 differs");
    require(recvx_en04_wpn_damage[11].flg==1&&recvx_en04_wpn_damage[14].flg==2&&
            recvx_en04_wpn_damage[16].flg==0x0a&&recvx_en04_wpn_damage[18].flg==0x0c&&
            recvx_en04_wpn_damage[2].cb_act==3&&recvx_en04_wpn_damage[6].cb_blood==5,
            "En04_WpnDamageTbl Zusatzwirkungen en04.c:268-285 differ");
    /* en04.c:289/312: En04 fuehrt beide Kombotabellen vollstaendig mit Nullen. */
    for(int i=0;i<21;i++) require(recvx_en04_combo_wep[i].crit==0&&recvx_en04_combo_wep[i].bonus==0,
                                  "CombWepTbl en04.c:289 ist im Original null");
    for(int i=0;i<19;i++) require(recvx_en04_combo_joint[i].flg==0&&recvx_en04_combo_joint[i].correct==0,
                                  "CombJointTbl en04.c:312 ist im Original null");
    /* en04.c:336 Ene04CapColTab: Zeile 6 { 12, 12, 15 }, Zeile 3 { 0, 4, -6 }. */
    require(recvx_en04_capsules[6].x==12&&recvx_en04_capsules[6].y==12&&recvx_en04_capsules[6].z==15,
            "Ene04CapColTab[6] en04.c:342 differs");
    require(recvx_en04_capsules[3].x==0&&recvx_en04_capsules[3].y==4&&recvx_en04_capsules[3].z==-6,
            "Ene04CapColTab[3] en04.c:339 differs");
    require(recvx_en04_capsules[16].x==0&&recvx_en04_capsules[16].y==0&&recvx_en04_capsules[16].z==0,
            "Ene04CapColTab[16] en04.c:352 differs");
    /* en04.c:356 en04_BldTbl. */
    require(recvx_en04_blood_pool.off_pos.y==0.1f&&recvx_en04_blood_pool.bld_spd==0.08f&&
            recvx_en04_blood_pool.srd_dir==0&&recvx_en04_blood_pool.srt_spd[0]==0.8f&&
            recvx_en04_blood_pool.srt_spd[4]==0.9f&&recvx_en04_blood_pool.srt_dir[4]==14,
            "en04_BldTbl en04.c:356 differs");
    /* en04.c:366 en04prt_blood_tbl: Zeile 12 fuehrt xlen/ylen 0 und size 2. */
    require(recvx_en04_blood[12].lnk_obj==12&&recvx_en04_blood[12].xlen==0.0f&&
            recvx_en04_blood[12].ylen==0.0f&&recvx_en04_blood[12].size==2.0f&&
            recvx_en04_blood[12].len==1.0f,"en04prt_blood_tbl[12] en04.c:378 differs");
    require(recvx_en04_blood[1].xlen==1.0f&&recvx_en04_blood[1].size==3.0f&&
            recvx_en04_blood[6].xlen==0.2f&&recvx_en04_blood[6].ylen==0.5f&&
            recvx_en04_blood[8].ylen==0.2f,"en04prt_blood_tbl en04.c:367-375 differs");
    /* en04.c:390 en04_hp_tbl, auch im gemeinsamen Datenmodul. */
    require(hp_tbl[0][0]==60&&hp_tbl[0][3]==90&&hp_tbl[0][13]==120&&
            hp_tbl[1][0]==30&&hp_tbl[1][8]==60&&hp_tbl[1][13]==75,
            "en04_hp_tbl Erwartung en04.c:390 differs");
    for(int r=0;r<2;r++) for(int c=0;c<16;c++)
        require(recvx::en04_hp_tbl[r][c]==hp_tbl[r][c],
                "native/combat_tables_data.inc:620 weicht von en04.c:390 ab");
    /* en04.c:398 en04_flipTree: 6..11 spiegeln die Beinketten. */
    require(recvx_en04_flip[6]==9&&recvx_en04_flip[7]==10&&recvx_en04_flip[8]==11&&
            recvx_en04_flip[9]==6&&recvx_en04_flip[13]==16&&recvx_en04_flip[16]==13&&
            recvx_en04_flip[19]==255,"en04_flipTree en04.c:398 differs");
    /* en04.c:3391 ply_ofs_pos. */
    require(std::fabs(recvx_en04_player_offset[0].x+0.937615f)<1e-6f&&
            std::fabs(recvx_en04_player_offset[0].z+9.776372f)<1e-6f&&
            std::fabs(recvx_en04_player_offset[1].x+0.112207f)<1e-6f&&
            std::fabs(recvx_en04_player_offset[1].z+10.525611f)<1e-6f,
            "ply_ofs_pos en04.c:3391 differs");
    /* en01.c:1345 kaidan_ang. */
    require(recvx_en04_stair_angle[0]==0&&recvx_en04_stair_angle[1]==49152&&
            recvx_en04_stair_angle[2]==32768&&recvx_en04_stair_angle[3]==16384,
            "kaidan_ang en01.c:1345 differs");
    /* en04.c:3957 bhEne04_ShakeWire pos[10]. */
    require(recvx_en04_wire_offset[0].y==0.1f&&recvx_en04_wire_offset[0].z==0.05f&&
            recvx_en04_wire_offset[2].z==0.35f&&recvx_en04_wire_offset[5].z==-0.2f&&
            recvx_en04_wire_offset[9].y==-0.1f&&recvx_en04_wire_offset[9].z==0.0f,
            "ShakeWire pos[] en04.c:3957 differs");
    /* en04.c:33 en04_mtn_tbl: Zeile MTN 16 fuehrt vier Ausloeser. */
    const RECVX_EN04_MTBL* m16=nullptr;
    for(unsigned i=0;i<30&&recvx_en04_sfx[i].no!=UINT32_MAX;i++)
        if(recvx_en04_sfx[i].no==16) m16=&recvx_en04_sfx[i];
    require(m16&&m16->atb[0].frm==18&&m16->atb[0].act==0x12301u&&
            m16->atb[1].frm==31&&m16->atb[1].act==0x1011304u&&
            m16->atb[2].frm==55&&m16->atb[2].act==0x1011306u&&
            m16->atb[3].frm==90&&m16->atb[3].act==0x1012303u,
            "en04_mtn_tbl MTN16 en04.c:116 differs");
    /* en04.c:215 en04_mtn_tbl2: MTN 19 vibriert auf Frame 10 und 36. */
    const RECVX_EN04_MTBL* v19=nullptr;
    for(unsigned i=0;i<6&&recvx_en04_vib[i].no!=UINT32_MAX;i++)
        if(recvx_en04_vib[i].no==19) v19=&recvx_en04_vib[i];
    require(v19&&v19->atb[0].frm==10&&v19->atb[0].act==0&&
            v19->atb[1].frm==36&&v19->atb[1].act==1,
            "en04_mtn_tbl2 MTN19 en04.c:230 differs");
}

/* ---------------------------------------------------------------------------
   2) bhEne04_Init (en04.c:939)
   --------------------------------------------------------------------------- */
void init(){
    reset();
    auto w=make(); w.mode0=0; random_value=3;
    recvx_en04_init_work(&w);
    require(gaps.empty(),"EN04 Init Dienstfixture unvollstaendig");
    require(w.ar==3.0f&&w.ah==4.0f&&w.aw==0.0f&&w.ad==0.0f&&w.car==3.5f&&w.cah==4.0f&&w.stflg==0,
            "bhEne04_Init Koerpermasse en04.c:943-955 differ");
    /* rand()%16 == 3 -> en04_hp_tbl[0][3] == 90 (en04.c:957/959). */
    require(w.hp==90,"bhEne04_Init hp en04.c:959 differs");
    require(w.mtn_no==0&&w.mtn_add==0&&w.frm_no==0&&w.mtn_md==0&&
            w.hokan_rate==0&&w.hokan_count==0&&w.mtn_tp==recvx_en04_flip,
            "bhEne04_Init Motionstart en04.c:972-978 differs");
    require(w.exp0!=nullptr,"bhEne_CallocWork en04.c:966 nicht ausgefuehrt");
    /* en04.c:991 EXP0_I(0x10) |= 0x21; :992 flg |= 0x178, &= ~2. */
    const auto* e=static_cast<const RECVX_EN04_EXP0*>(w.exp0);
    require(e->flg==0x21u,"EXP0_I(0x10) en04.c:991 differs");
    require(w.flg==(1u|0x178u|0x800u)&&!(w.flg&2u),"flg en04.c:992/997 differs");
    require(w.cpcl==recvx_en04_capsules,"cpcl en04.c:994 differs");
    require(shadow_calls==1&&(w.flg&0x800u),"bhSetShadow en04.c:996 differs");
    require(!(w.mdflg&0x400u),"mdflg en04.c:1001 differs");
    require(material_color==0xffb2b2b2u,"npSetAllMatColor en04.c:1003 differs");
    require(w.clp_jno[0]==4&&w.clp_jno[1]==5&&w.clp_jno[2]==6&&w.clp_jno[3]==9&&
            w.clp_jno[4]==15&&w.clp_jno[5]==15&&w.clp_jno[6]==-1&&w.clp_jno[7]==0,
            "clp_jno en04.c:1005-1012 differs");
    require(w.mode0==1&&w.mode1==0&&w.mode2==0&&w.mode3==0&&w.lok_jno==4,
            "bhEne04_Init Endmodus en04.c:1014-1018 differs");
    require(recvx_en04_contact_flags()==0&&recvx_en04_area_flags()==0,
            "en04_contact_flg/en04_atari_flg en04.c:943 nicht genullt");
    /* gm_mode 2 nimmt Zeile 1 (en04.c:963). */
    RECVX_EN04_CONTEXT hard{}; hard.en04=&ext; hard.game_mode=2;
    recvx_en04_set_context(&hard);
    auto h=make(); random_value=13; recvx_en04_init_work(&h);
    require(h.hp==75,"bhEne04_Init hp gm_mode 2 en04.c:963 differs");
    /* Fehlende hp_tbl-Bindung wird gemeldet, kein Ersatzwert. */
    recvx_en04_set_data(nullptr); gaps.clear();
    auto u=make(); recvx_en04_init_work(&u);
    require(gaps.size()==1&&gaps[0]=="en04_hp_tbl"&&u.hp==0,
            "fehlende en04_hp_tbl muss als Luecke gemeldet werden");
}

/* ---------------------------------------------------------------------------
   3) bhEne04_Move / MVType00 / MV00 (en04.c:1020/1048/1656)
   --------------------------------------------------------------------------- */
/* ORIGINAL-FIX (QUALITY.md) in recvx_en04_brain: en04.c:1437/1444 ohne Wegezone. Der Zielblock
   laeuft, sobald EneSearch Bit 0x80 im Bild mit Zaehler 3 loescht (en04.c:1405). Mit Zone wird
   das Ziel der Zonenmittelpunkt, ohne Zone bleibt das bisherige Ziel (kein Stapelrest). */
void brain_route(){
    reset();
    auto w=make(); w.mode0=1; w.mode1=1; w.mode2=0; w.mode3=0;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* e=static_cast<RECVX_EN04_EXP0*>(w.exp0);
    ply.pos={40,0,60}; ply.flr_no=0; e->dist=80.0f;
    e->search=3;                                   /* Zaehler 3 beim Eintritt: :1405 &= 0x5F loescht Bit 0x80 */
    route_zone=1; route_point={7,0,9}; e->target={1,0,2};
    recvx_en04_brain(&w);
    require(!(e->search&0x80u),"EneSearch :1405 loescht Bit 0x80 im Zaehlerstand 3");
    require(e->target.x==7.0f&&e->target.z==9.0f,"Brain :1438 Ziel = Wegezonenpunkt");
    e->search=3; route_zone=0; e->target={1,0,2};
    recvx_en04_brain(&w);
    require(e->target.x==1.0f&&e->target.z==2.0f,"ORIGINAL-FIX: ohne Wegezone bleibt das bisherige Ziel");
    require(gaps.empty(),"ohne Wegezone keine Luecke mehr");
    route_zone=1; route_point={};
}
/* bhEne04_Escape Fall 1 ohne EXP0_I(0x3C) (en04.c:2851-2858): d_tmp ist $s0, von allen drei
   MVTypeXX vor dem jalr positiv gesetzt -> Rueckgabe 2, esc_mode bleibt 1. Mit Wandtreffer
   d_tmp = wax und esc_mode 2. Vorher brach der Port hier mit -1 ab. */
void escape_mode1(){
    reset();
    auto w=make(); w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* e=static_cast<RECVX_EN04_EXP0*>(w.exp0);
    ply.pos={0,0,100}; e->u48.esc_mode=1; e->wall_hit=0; e->esc_ct=0; w.wax=-300;
    const int r=recvx_en04_escape(&w,90,1152);
    require(r==2,"Escape Fall 1 ohne 0x3C: bestimmter Rueckgabewert 2 ($s0 > 0)");
    require(e->u48.esc_mode==1,"Escape Fall 1 ohne 0x3C: esc_mode bleibt 1");
    require(gaps.empty(),"Escape Fall 1 ohne 0x3C meldet eine Luecke");
}
void move_mv00(){
    reset();
    auto w=make(); w.mode0=1; w.mode1=1; w.mode2=0; w.mode3=0; w.type=0;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* e=static_cast<RECVX_EN04_EXP0*>(w.exp0);
    e->flg=0x80; random_value=5; ply.pos={0,0,100}; ply.flr_no=0;
    /* mode1 == 1 ruft Brain; dort bleibt mode2 0, weil dist gross ist. */
    recvx_en04_move(&w);
    /* en04.c:1659 type 0 -> ChgMtn(47, 0, 7). */
    require(w.mtn_no==47&&w.frm_no==0&&w.hokan_count==7&&w.hokan_rate==32768&&
            w.mtn_add==65536&&w.mtn_md==0,"bhEne04_MV00 ChgMtn en04.c:1659 differs");
    /* en04.c:1668 ct0 = (rand()%128)+100, :1670 mode1 = 1. */
    require(w.ct0==(5%128)+100-1,"MV00 ct0 en04.c:1668 (nach dem Durchfall-Dekrement) differs");
    require(w.mode1==1&&w.mode3==1,"MV00 mode en04.c:1669 differs");
    /* en04.c:1674/1675: Fall 1 laeuft im selben Pass und setzt 0x80/4096. */
    require((e->flg&0x80u)&&e->neck==4096,"MV00 Durchfall en04.c:1674 differs");

    /* type != 0 nimmt MTN 49 (en04.c:1663). */
    reset();
    auto t=make(); t.mode0=1; t.mode1=0; t.mode2=0; t.type=1;
    t.exp0=allocate(sizeof(RECVX_EN04_EXP0)); random_value=0;
    /* MVType01 braucht keinen Treffer: ohne Bereiche bleibt type 1. */
    recvx_en04_move(&t);
    require(t.mtn_no==49&&t.hokan_count==7,"MV00 ChgMtn en04.c:1663 differs");

    /* en04.c:1677 Fall 1 -> 2: ChgMtn(48, 0, 10) und 0x80/neck geloescht. */
    reset();
    auto g=make(); g.mode0=1; g.mode1=1; g.mode2=0; g.mode3=1; g.type=0; g.ct0=1;
    g.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    ply.pos={0,0,500};
    recvx_en04_move(&g);
    auto* ge=static_cast<RECVX_EN04_EXP0*>(g.exp0);
    require(g.mtn_no==48&&g.hokan_count==10&&g.mode3==2,"MV00 en04.c:1680 differs");
    require(!(ge->flg&0x80u)&&ge->neck==0,"MV00 en04.c:1683 differs");

    /* en04.c:1689 Fall 2 am letzten Frame: 0x400000 -> mode2 2, sonst 1. */
    reset();
    auto c=make(); c.mode0=1; c.mode1=1; c.mode2=0; c.mode3=2; c.type=0;
    c.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    c.frm_no=(int32_t)((frames_per_motion-1)*65536); ply.pos={0,0,500};
    recvx_en04_move(&c);
    require(c.mode1==1&&c.mode2==1&&c.mode3==0,"MV00 en04.c:1703 differs");
    reset();
    auto d=make(); d.mode0=1; d.mode1=1; d.mode2=0; d.mode3=2; d.type=0;
    d.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(d.exp0)->flg=0x400000;
    d.frm_no=(int32_t)((frames_per_motion-1)*65536); ply.pos={0,0,500};
    recvx_en04_move(&d);
    require(d.mode1==1&&d.mode2==2&&d.mode3==0,"MV00 en04.c:1697 differs");

    /* en04.c:1022: Move loescht die Nackenfreigabe VOR dem Typzweig. */
    reset();
    auto n=make(); n.mode0=1; n.mode1=0; n.mode2=5; n.type=0;
    n.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(n.exp0)->flg=0x80;
    n.mode3=1; n.frm_no=0; ply.pos={0,0,20};
    recvx_en04_move(&n);
    require(!(static_cast<RECVX_EN04_EXP0*>(n.exp0)->flg&0x80u),
            "bhEne04_Move muss EXP0 0x80 loeschen (en04.c:1022)");
    /* Unbekannter MoveType wird gemeldet, nicht geraten (en04.c:424). */
    reset();
    auto bad=make(); bad.mode0=1; bad.type=3; bad.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    recvx_en04_move(&bad);
    require(gaps.size()==1&&gaps[0]=="bhEne04_MoveType index","MoveType-Grenze en04.c:424 fehlt");
}

/* ---------------------------------------------------------------------------
   4) bhEne04_MV01 und MV02 (en04.c:1713/1792)
   --------------------------------------------------------------------------- */
void mv01_mv02(){
    reset();
    auto w=make(); w.mode0=1; w.mode1=0; w.mode2=1; w.mode3=0; w.type=0;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0)); random_value=7;
    /* MV01 wird aus MV00 heraus betreten, dort laeuft MTN 47/48. */
    w.mtn_no=47; ply.pos={11,0,13};
    recvx_en04_move(&w);
    auto* e=static_cast<RECVX_EN04_EXP0*>(w.exp0);
    /* en04.c:1726 ChgMtn(0,0,7); :1728 way = 256; :1730 ct0 = rand%32+75. */
    require(w.mtn_no==0&&w.hokan_count==7&&w.hokan_rate==32768&&w.mtn_add==65536,
            "MV01 ChgMtn en04.c:1726 differs");
    /* en04.c:3705: dieselbe Motion wird nicht neu gestartet, nur mtn_add. */
    {RECVX_ENEMY_WORK same=w;same.hokan_count=3;
     require(recvx_en04_chg_mtn(&same,same.mtn_no,0,9)==-1&&same.hokan_count==3&&
             same.mtn_add==65536,"bhEne04_ChgMtn Wiederholung en04.c:3705 differs");}
    require(w.way==256,"MV01 way en04.c:1728 differs");
    require(w.ct0==(7%32)+75-1,"MV01 ct0 en04.c:1730 differs");
    /* en04.c:1733/1734: Ziel ist die Spielerlage. */
    require(e->target.x==11.0f&&e->target.z==13.0f,"MV01 Ziel en04.c:1733 differs");
    /* en04.c:1740 spd = 0.15f, :1784 ct2++ je Pass. */
    require(w.spd==0.15f&&w.ct2==1,"MV01 spd/ct2 en04.c:1740/1784 differs");
    require(sounds.empty(),"MV01 SE darf erst auf ct2%90==0 kommen (en04.c:1786)");

    /* en04.c:1748 Wandtreffer -> CheckSideWall(10.0f, 0), ct0 16384, ct1 256. */
    reset();
    auto s=make(); s.mode0=1; s.mode1=0; s.mode2=1; s.mode3=1; s.type=0;
    s.exp0=allocate(sizeof(RECVX_EN04_EXP0)); s.way=256;
    wall_hit_result=1; static_cast<RECVX_EN04_EXP0*>(s.exp0)->wall_hit=1;
    ply.pos={0,0,10}; random_value=1;
    recvx_en04_move(&s);
    require(s.ct0==16384&&s.ct1==256&&s.mode3==2,"MV01 en04.c:1763 differs");

    /* en04.c:1771 Fall 2: spd 0.08, ay += way, ct0 -= 256. */
    reset();
    auto r=make(); r.mode0=1; r.mode1=0; r.mode2=1; r.mode3=2; r.type=0;
    r.exp0=allocate(sizeof(RECVX_EN04_EXP0)); r.way=256; r.ct0=16384; r.ay=1000;
    ply.pos={0,0,10};
    recvx_en04_move(&r);
    require(r.spd==0.08f&&r.ay==1256&&r.ct0==16128,"MV01 Fall 2 en04.c:1772-1778 differs");

    /* en04.c:1798 MV02 Fall 0: way 1152, spd 0.08, 0x3000/0x4000 geloescht. */
    reset();
    auto m=make(); m.mode0=1; m.mode1=0; m.mode2=2; m.mode3=0; m.type=0;
    m.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(m.exp0)->flg=0x7000;
    ply.pos={0,0,40}; random_value=2;
    recvx_en04_move(&m);
    auto* me=static_cast<RECVX_EN04_EXP0*>(m.exp0);
    require(m.way==1152&&m.spd!=0.0f,"MV02 way en04.c:1806 differs");
    require(!(me->flg&0x3000u)&&!(me->flg&0x4000u),"MV02 en04.c:1817 differs");
    /* en04.c:1810: nur bei gesetztem 0x800 wird ct1 aus rand gezogen. */
    require(m.ct1!=0,"MV02 ct1 en04.c:1812 differs");

    /* en04.c:1824-1826: dieser Zweig laesst `rot` unzugewiesen in die
       Schlusszeile laufen. Im Original liegt `rot` in $s0, das
       bhEne04_MVType00 als $sp + 0x58 (SLUS_201.84 0x1A6138) unveraendert
       weitergibt: ein positiver Wert, bhEne04_RunMotion (en04.c:3657) nimmt
       damit idx 0 = mtn_tbl[0] = 3. Keine Luecke mehr. */
    reset();
    auto u=make(); u.mode0=1; u.mode1=0; u.mode2=2; u.mode3=1; u.type=0;
    u.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(u.exp0)->flg=0x1000;
    u.mtn_no=1; ply.pos={0,0,-40};
    // e->target liegt hinter dem Hund (niten_dir 180 Grad gegen ay), e->dist ueber 15: sonst setzt
    // die Sonde en04.c:1962 rot auf 0 und RunMotion waehlt mit idx 1 wieder mtn 1.
    auto* ue=static_cast<RECVX_EN04_EXP0*>(u.exp0); ue->target={0,0,-40}; ue->dist=40.0f; u.ay=32768;
    recvx_en04_move(&u);
    require(u.mode3==2,"MV02 en04.c:1825 Zustandswechsel fehlt");
    for(const auto& g:gaps) require(g.find("en04.c:1825")==std::string::npos,"MV02 rot ist per Disassembly belegt, keine Luecke");
    require(u.mtn_no==3,"MV02 0x1000: rot > 0, RunMotion muss mtn_tbl[0] = 3 waehlen");
}

/* ---------------------------------------------------------------------------
   5) bhEne04_MV11 Treppe (en04.c:2709)
   --------------------------------------------------------------------------- */
void mv11_stairs(){
    reset();
    auto w=make(); w.mode0=1; w.mode1=0; w.mode2=11; w.mode3=0; w.type=0;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* e=static_cast<RECVX_EN04_EXP0*>(w.exp0);
    e->stair_rec.prm0=0; e->stair_rec.prm1=2; e->stair_rec.prm2=4;
    w.py=10.0f; w.flr_no=1; w.ay=0; ply.pos={0,0,30};
    recvx_en04_move(&w);
    /* en04.c:2714-2718: prm0 == 0 ist aufwaerts, yn = py + prm2*2. */
    require((e->flg&0x10000u)&&e->stair_h==0&&w.yn==18.0f,
            "MV11 Aufstieg en04.c:2714 differs");
    require(e->stair_flr==1&&(e->flg&0x10u),"MV11 en04.c:2727/2728 differs");
    /* en04.c:2730: axp = kaidan_ang[prm1] = 32768; ayp = ((32768-0)&0xffff)
       -> == 32768, nicht > 32768, also ayp = 32768/10 = 3276. */
    require(w.axp==32768&&w.ayp==3276,"MV11 Drehschritt en04.c:2730-2737 differs");
    /* en04.c:2738 ct0 = 10; der Durchfall in Fall 1 dreht und dekrementiert. */
    require(w.ct0==9&&w.ay==3276&&w.mode3==1,"MV11 Durchfall en04.c:2740 differs");

    /* en04.c:2720: prm0 != 0 ist abwaerts. */
    reset();
    auto d=make(); d.mode0=1; d.mode1=0; d.mode2=11; d.mode3=0; d.type=0;
    d.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* de=static_cast<RECVX_EN04_EXP0*>(d.exp0);
    de->stair_rec.prm0=1; de->stair_rec.prm1=1; de->stair_rec.prm2=4;
    d.py=10.0f; ply.pos={0,0,30};
    recvx_en04_move(&d);
    require(!(de->flg&0x10000u)&&de->stair_h==4&&d.yn==2.0f,
            "MV11 Abstieg en04.c:2720 differs");
    require(d.axp==49152,"MV11 kaidan_ang[1] en04.c:2730 differs");

    /* en04.c:2743 ct0 laeuft ab: ChgMtn 37 aufwaerts, Flags und Startlage. */
    reset();
    auto g=make(); g.mode0=1; g.mode1=0; g.mode2=11; g.mode3=1; g.type=0;
    g.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* ge=static_cast<RECVX_EN04_EXP0*>(g.exp0);
    ge->flg=0x10020; g.ct0=0; g.axp=16384; g.px=3; g.py=4; g.pz=5;
    ply.pos={0,0,30};
    recvx_en04_move(&g);
    require(g.mtn_no==37&&g.hokan_count==5,"MV11 ChgMtn en04.c:2745 differs");
    require((ge->flg&0x40000000u)&&(ge->flg&0x20000u)&&!(ge->flg&0x20u)&&(g.flg2&1u),
            "MV11 Flags en04.c:2747-2751 differs");
    require(ge->link.x==3.0f&&ge->link.y==4.0f&&ge->link.z==5.0f&&g.ay==16384&&g.mode3==2,
            "MV11 Startlage en04.c:2753-2757 differs");
    reset();
    auto n=make(); n.mode0=1; n.mode1=0; n.mode2=11; n.mode3=1; n.type=0;
    n.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(n.exp0)->flg=0; n.ct0=0; ply.pos={0,0,30};
    recvx_en04_move(&n);
    require(n.mtn_no==38,"MV11 Abstiegsmotion en04.c:2745 differs");
}

/* ---------------------------------------------------------------------------
   6) Schaden: DmgChk / ChgDmgMode / DamageAdd (en04.c:626/688/733)
   --------------------------------------------------------------------------- */
void damage(){
    reset();
    auto w=make(); w.mode0=1; w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    w.flg|=4; w.hp=90; w.total_dam=20; w.comb_wep=2; w.wpnr_no=2; w.dam[1]=20;
    recvx_en04_dmg_chk(&w);
    /* en04.c:743 hp -= total_dam; :752 SE 16847623 solange hp >= 0. */
    require(w.hp==70,"bhEne04_DamageAdd en04.c:743 differs");
    require(sounds.size()==1&&sounds[0]==16847623,"DamageAdd SE en04.c:752 differs");
    /* en04.c:700/702 nm_act von Waffe 2 ist 0 (< 4) -> Modus 3/0/0/0. */
    require(w.mode0==3&&w.mode1==0&&w.mode2==0&&w.mode3==0,
            "bhEne04_ChgDmgMode en04.c:719-723 differs");
    require((w.flg&0x40u),"bhEne04_DmgChk flg 0x40 en04.c:646 differs");
    require(w.type==0,"bhEne04_DmgChk type en04.c:638 differs");

    /* en04.c:748 hp < 0 nimmt SE 16786185. */
    reset();
    auto k=make(); k.mode0=1; k.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    k.flg|=4; k.hp=30; k.total_dam=60; k.comb_wep=2; k.wpnr_no=2;
    recvx_en04_dmg_chk(&k);
    require(k.hp==-30&&sounds.size()==1&&sounds[0]==16786185,
            "DamageAdd Todes-SE en04.c:748 differs");
    /* en04.c:694: hp < 0 erzwingt den Kombozweig. */
    require((k.comb_flg&1u)&&k.comb_timeout==0&&k.comb_pnt==0,
            "ChgDmgMode en04.c:695-697 differs");

    /* en04.c:696/716/719: toetender Treffer mit Waffe 4 (Zeile { 0, 0, 7, 3, 3 })
       nimmt act = cb_act = 3 < 4 -> Schadensmodus 3; erst DG00 :3226 fuehrt dann
       bei hp < 0 nach mode0 4. Mit vertauschter Feldfolge war act 7 und der
       Hund blieb in mode0 1 (RM_1000: hp -7, nie tot). */
    reset();
    auto l=make(); l.mode0=1; l.mode2=7; l.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    l.flg|=4; l.hp=13; l.total_dam=20; l.comb_wep=4; l.wpnr_no=4; l.dam[1]=20;
    recvx_en04_dmg_chk(&l);
    require(l.hp==-7&&l.mode0==3&&l.mode1==0&&l.mode2==0&&l.mode3==0,
            "ChgDmgMode toetender Treffer cb_act en04.c:716-723 differs");

    /* en04.c:724: EXP0 0x20000 (Treppe) waehlt DamageMode2 1. */
    reset();
    auto s=make(); s.mode0=1; s.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(s.exp0)->flg=0x20000;
    s.flg|=4; s.hp=90; s.total_dam=10; s.comb_wep=2; s.wpnr_no=2;
    recvx_en04_dmg_chk(&s);
    require(s.mode0==3&&s.mode2==1,"ChgDmgMode en04.c:726 differs");

    /* en04.c:706: EXP0 0x100000 -> mode3 2 und ChgMtn(14, 0, 10). */
    reset();
    auto h=make(); h.mode0=1; h.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(h.exp0)->flg=0x100000;
    h.flg|=4; h.hp=90; h.total_dam=10; h.comb_wep=2; h.wpnr_no=2;
    recvx_en04_dmg_chk(&h);
    require(h.mode0==3&&h.mode1==0&&h.mode2==0&&h.mode3==2,
            "ChgDmgMode harter Wechsel en04.c:709-713 differs");
    require(h.mtn_no==14&&h.hokan_count==10,"ChgDmgMode ChgMtn en04.c:714 differs");
    require(!(static_cast<RECVX_EN04_EXP0*>(h.exp0)->flg&0x100000u),
            "ChgDmgMode en04.c:715 loescht 0x100000 nicht");
}

/* ---------------------------------------------------------------------------
   7) bhEne04_DG00 / DG01 / DamageMode2[2] (en04.c:3106/3246/491)
   --------------------------------------------------------------------------- */
void damage_states(){
    /* en04.c:3156: ohne 0x100 way 4096 und MTN 11, 0x40000000 geloescht. */
    reset();
    auto w=make(); w.mode0=3; w.mode1=0; w.mode2=0; w.mode3=0;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(w.exp0)->flg=0x40000000;
    recvx_en04_damage(&w);
    require(w.way==4096&&w.mtn_no==11&&w.spd==0.0f&&w.hokan_count==5,
            "DG00 en04.c:3156-3161 differs");
    require(!(static_cast<RECVX_EN04_EXP0*>(w.exp0)->flg&0x40000000u),
            "DG00 en04.c:3162 differs");
    require(w.mode3==1,"DG00 Durchfall en04.c:3164 differs");

    /* en04.c:3145: mit 0x100 (Kombotreffer) way -28672 und MTN 10. */
    reset();
    auto b=make(); b.mode0=3; b.mode2=0; b.mode3=0;
    b.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(b.exp0)->flg=0x100;
    recvx_en04_damage(&b);
    require(b.way==-28672&&b.mtn_no==10,"DG00 en04.c:3148-3150 differs");

    /* en04.c:3127: 0x8000 (Zaun) nimmt MTN 44/45 und springt auf mode3 2. */
    reset();
    auto f=make(); f.mode0=3; f.mode2=0; f.mode3=0;
    f.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(f.exp0)->flg=0x8000;
    recvx_en04_damage(&f);
    require(f.mtn_no==44&&f.way==4096&&f.mode3==2&&
            (static_cast<RECVX_EN04_EXP0*>(f.exp0)->flg&0x40000000u),
            "DG00 Zaunzweig en04.c:3129-3143 differs");
    reset();
    auto f2=make(); f2.mode0=3; f2.mode2=0; f2.mode3=0;
    f2.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(f2.exp0)->flg=0x8100;
    recvx_en04_damage(&f2);
    require(f2.mtn_no==45&&f2.way==-28672,"DG00 en04.c:3133 differs");

    /* en04.c:3118: 0x400 (Sprung) hebt um 3.8 und nimmt MTN 12. */
    reset();
    auto j=make(); j.mode0=3; j.mode2=0; j.mode3=0; j.py=5.0f; j.flg|=0x100;
    j.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(j.exp0)->flg=0x8400;
    recvx_en04_damage(&j);
    auto* je=static_cast<RECVX_EN04_EXP0*>(j.exp0);
    require(j.mtn_no==12&&je->u48.jump==1.0f&&!(je->flg&0x8400u),
            "DG00 Sprungzweig en04.c:3120-3125 differs");
    require(std::fabs(j.py-(5.0f+3.8f+((std::fabs(1.0f)*njSin(8192))-0.0f)))<1e-4f,
            "DG00 py en04.c:3122/3168 differs");

    /* en04.c:3216 Fall 2 am letzten Frame: hp < 0 -> Mode0 4 (Tod). */
    reset();
    auto d=make(); d.mode0=3; d.mode2=0; d.mode3=2; d.hp=-1;
    d.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    d.frm_no=(int32_t)((frames_per_motion-1)*65536);
    recvx_en04_damage(&d);
    require(d.mtn_add==0&&d.mode0==4&&d.mode1==0&&d.mode2==0&&d.mode3==0,
            "DG00 Tod en04.c:3222-3227 differs");
    reset();
    auto a=make(); a.mode0=3; a.mode2=0; a.mode3=2; a.hp=10;
    a.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    a.frm_no=(int32_t)((frames_per_motion-1)*65536);
    recvx_en04_damage(&a);
    require(a.mode0==1&&a.mode1==0&&a.mode2==5&&a.mode3==0&&
            (static_cast<RECVX_EN04_EXP0*>(a.exp0)->flg&0x2100000u)==0x2100000u,
            "DG00 Aufstehen en04.c:3230-3236 differs");

    /* en04.c:491 bhEne04_DamageMode2[2] zeigt wieder auf DG00. */
    reset();
    auto t=make(); t.mode0=3; t.mode2=2; t.mode3=0;
    t.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    recvx_en04_damage(&t);
    require(gaps.empty(),"DamageMode2[2] en04.c:493 fehlt (darf nicht melden)");
    require(t.mtn_no==11&&t.mode3==1,"DamageMode2[2] fuehrt nicht DG00 aus");

    /* en04.c:3250 DG01 Fall 0: Treppenflags und Sprunghoehe aus dem Abstand. */
    reset();
    auto s=make(); s.mode0=3; s.mode2=1; s.mode3=0; s.px=0; s.pz=0; s.py=0;
    s.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* se=static_cast<RECVX_EN04_EXP0*>(s.exp0);
    se->flg=0x40020010|0x10000; se->stair_rec.prm1=0; se->link={30.0f,0.0f,40.0f};
    recvx_en04_damage(&s);
    require(!(se->flg&0x40000000u)&&!(se->flg&0x20000u)&&!(se->flg&0x20u)&&(s.flg2&1u),
            "DG01 Flags en04.c:3258-3262 differs");
    require(s.mtn_no==12,"DG01 ChgMtn en04.c:3269 differs");
    /* dist = |(0,0)-(30,40)| = 50; jump = 50*0.033333335 + 1 = 2.6666667. */
    require(std::fabs(se->u48.jump-2.6666667f)<1e-5f,"DG01 jump en04.c:3292 differs");
    require(s.mode3==1,"DG01 Durchfall en04.c:3296 differs");
}

/* ---------------------------------------------------------------------------
   8) Tod: bhEne04_DD00 (en04.c:3370)
   --------------------------------------------------------------------------- */
void death(){
    reset();
    auto w=make(); w.mode0=4; w.mode1=0; w.mode2=0; w.mode3=0;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    w.flg|=0x28; w.px=1; w.py=2; w.pz=3; w.ay=4096;
    static_cast<RECVX_EN04_EXP0*>(w.exp0)->flg=0x80;
    recvx_en04_die(&w);
    /* en04.c:3375 flg |= 2; :3377 flg &= ~0x28. */
    require((w.flg&2u)&&!(w.flg&0x28u),"DD00 flg en04.c:3375/3377 differs");
    require(blood_pool_calls==1&&blood_pool_seen.bld_spd==0.08f&&
            blood_pool_seen.off_pos.y==0.1f,"DD00 bhSetBloodPoolLnk en04.c:3379 differs");
    require(w.mode3==1,"DD00 mode3 en04.c:3381 differs");
    /* en04.c:1042: Die loescht zuerst die Nackenfreigabe. */
    require(!(static_cast<RECVX_EN04_EXP0*>(w.exp0)->flg&0x80u),
            "bhEne04_Die muss EXP0 0x80 loeschen (en04.c:1042)");
    /* Weitere Pass: DD00 Fall 1 tut nichts mehr. */
    blood_pool_calls=0; recvx_en04_die(&w);
    require(blood_pool_calls==0&&w.mode3==1,"DD00 Fall 1 en04.c:3383 differs");
    /* en04.c:498 DieMode2 hat genau einen Eintrag. */
    reset();
    auto b=make(); b.mode0=4; b.mode2=1; b.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    recvx_en04_die(&b);
    require(gaps.size()==1&&gaps[0]=="bhEne04_DieMode2 index","DieMode2-Grenze en04.c:498 fehlt");
}

/* ---------------------------------------------------------------------------
   9) Griff: NGType00 / NG00 (en04.c:2922/2939)
   --------------------------------------------------------------------------- */
void grab(){
    reset();
    auto w=make(); w.mode0=2; w.mode1=0; w.mode2=0; w.mode3=0;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    w.px=0; w.pz=0; w.ay=0; w.flg|=0x40;
    ply.pos={0,0,-10}; ply.ay=0;
    recvx_en04_nage(&w);
    auto* e=static_cast<RECVX_EN04_EXP0*>(w.exp0);
    /* en04.c:2927: bhCdirCheck(plp->ay, epw->ay) == 0 setzt 0x200. Bei
       gleichem Winkel liefert zonzon.c:94 1, also bleibt 0x200 geloescht. */
    require(!(e->flg&0x200u),"NGType00 en04.c:2931 differs");
    /* en04.c:2946 ChgMtn(17, 0, 5); :2950 flg &= ~0x40; :2952/2953 Griffbits. */
    require(w.mtn_no==17&&w.hokan_count==5,"NG00 ChgMtn en04.c:2946 differs");
    require(!(w.flg&0x40u),"NG00 en04.c:2950 differs");
    require((e->flg&0x40000u)&&(e->flg&0x80000u),"NG00 en04.c:2952 differs");
    /* en04.c:2955: der Spieler geht in Mode0 4. */
    require(ply.mode0==4&&ply.mode1==0&&ply.mode2==0&&ply.mode3==0,
            "NG00 Spielermodus en04.c:2955 differs");
    require(w.ct0==60,"NG00 ct0 en04.c:2960 differs");
    /* en04.c:2948: ay zeigt zum Spieler (NitenDir_ck). */
    require(w.ay==((int)(std::atan2(0.0f-0.0f,0.0f-(-10.0f))*10430.381f)),
            "NG00 ay en04.c:2948 differs");

    /* en04.c:2968 Frame 12: Trefferpunkt, djnt_no 12, Spielerblut, SE. */
    reset();
    auto f=make(); f.mode0=2; f.mode2=0; f.mode3=1;
    f.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    f.frm_no=12*65536; ply.pos={0,0,-10};
    recvx_en04_nage(&f);
    require(hit_joint==12,"NG00 djnt_no en04.c:2974 differs");
    require(player_blood_calls==1,"NG00 bhEne_SetBlood(plp,1) en04.c:2976 differs");
    require(sounds.size()==1&&sounds[0]==16847622,"NG00 SE en04.c:2978 differs");

    /* en04.c:2987 Fall 2 Frame 1: Spieler verliert 8 HP, min 0. */
    reset();
    auto d=make(); d.mode0=2; d.mode2=0; d.mode3=2; d.ct0=60;
    d.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    d.frm_no=1*65536; ply.hp=5; ply.pos={0,0,-10}; lever=4;
    recvx_en04_nage(&d);
    require(ply.hp==0,"NG00 Spielerschaden en04.c:2989-2993 differs");
    /* en04.c:2996: ct0 -= bhEne_LeverCheck() + 1. */
    require(d.ct0==60-(4+1),"NG00 Hebelrueckzug en04.c:2996 differs");

    /* en04.c:3038 Fall 4 am letzten Frame: zurueck in Mode0 1 / Mode2 5. */
    reset();
    auto o=make(); o.mode0=2; o.mode2=0; o.mode3=4;
    o.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    o.frm_no=(int32_t)((frames_per_motion-1)*65536); ply.pos={0,0,-10};
    recvx_en04_nage(&o);
    require((o.flg&0x40u)&&o.mtn_add==0&&o.mode0==1&&o.mode1==0&&o.mode2==5&&o.mode3==0,
            "NG00 Abschluss en04.c:3040-3048 differs");
    require((static_cast<RECVX_EN04_EXP0*>(o.exp0)->flg&0x2100000u)==0x2100000u,
            "NG00 en04.c:3050 differs");

    /* en04.c:3056 NG01 Fall 0: Mode0 6 am Spieler, Kollisionsmodus 2. */
    reset();
    auto n=make(); n.mode0=2; n.mode2=1; n.mode3=0; ground_height=7.0f;
    n.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(n.exp0)->flg=0xF;
    ply.pos={0,0,-10};
    recvx_en04_nage(&n);
    auto* ne=static_cast<RECVX_EN04_EXP0*>(n.exp0);
    require(n.py==7.0f,"NG01 py en04.c:3059 differs");
    require(n.mtn_no==16&&n.hokan_count==5,"NG01 ChgMtn en04.c:3061 differs");
    require((ne->flg&0x40000u)&&(ne->flg&0x80000u)&&(ne->flg&0xFu)==2u,
            "NG01 en04.c:3067-3071 differs");
    require(ply.mode0==6&&ply.mode1==1,"NG01 Spielermodus en04.c:3073 differs");

    /* en04.c:485 NageMode2 hat genau zwei Eintraege. */
    reset();
    auto b=make(); b.mode0=2; b.mode2=2; b.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    recvx_en04_nage(&b);
    require(gaps.size()==1&&gaps[0]=="bhEne04_NageMode2 index","NageMode2-Grenze en04.c:485 fehlt");
}

/* ---------------------------------------------------------------------------
   10) bhEne04_PlyDamageCheck (en04.c:3562)
   --------------------------------------------------------------------------- */
void player_damage(){
    reset();
    auto w=make(); w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    w.ay=0; ply.ay=0; ply.hp=100; sc_result=1;
    /* en04.c:3581: bhCdirCheck(plp->ay, epw->ay) == 1 -> hit bleibt 0. */
    int r=recvx_en04_ply_damage_check(&w,0);
    require(r==1,"PlyDamageCheck en04.c:3625 differs");
    require((ply.flg&4u)&&ply.mode0==2&&ply.mode2==1&&ply.mode3==0&&ply.mode1==1,
            "PlyDamageCheck Spielermodus en04.c:3613-3623 differs");
    require(vibrations.size()==1&&vibrations[0]==10,
            "bhEne_SetVibration(1) zonzon.c:1352 differs");

    /* en04.c:3587: Typ 1 mit plp->hp < 12 liefert 2 (toedlicher Griff). */
    reset();
    auto k=make(); k.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    ply.hp=11; sc_result=1;
    require(recvx_en04_ply_damage_check(&k,1)==2,"PlyDamageCheck en04.c:3589 differs");
    require(!(ply.flg&4u),"PlyDamageCheck darf vor dem 2er-Ausgang nichts setzen");
    reset();
    auto k2=make(); k2.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    ply.hp=12; sc_result=1;
    require(recvx_en04_ply_damage_check(&k2,1)==1,"PlyDamageCheck Grenze hp 12 en04.c:3587 differs");

    /* en04.c:3570: bereits getroffener, toter oder gesperrter Spieler -> 0. */
    reset();
    auto s=make(); s.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    ply.hp=100; ply.flg=4; sc_result=1;
    require(recvx_en04_ply_damage_check(&s,0)==0,"PlyDamageCheck en04.c:3572 differs");
    reset();
    auto s2=make(); s2.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    ply.hp=100; ply.stflg=0x80000000u; sc_result=1;
    require(recvx_en04_ply_damage_check(&s2,0)==0,"PlyDamageCheck stflg en04.c:3572 differs");
    /* en04.c:3577: ohne Kapseltreffer kein Schaden. */
    reset();
    auto m=make(); m.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    ply.hp=100; sc_result=0;
    require(recvx_en04_ply_damage_check(&m,0)==0,"PlyDamageCheck en04.c:3579 differs");
}

/* ---------------------------------------------------------------------------
   11) bhEne04 Rahmen und ShakeWire (en04.c:516/3955)
   --------------------------------------------------------------------------- */
void frame(){
    reset();
    auto w=make(); w.mode0=1; w.mode1=0; w.mode2=0; w.mode3=1; w.type=0; w.ct0=100;
    w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    auto* e=static_cast<RECVX_EN04_EXP0*>(w.exp0);
    e->ct_14=2; e->ct_18=3; w.flg|=4; w.dam[3]=9; ply.pos={0,0,80};
    object_total=7;
    for(unsigned i=0;i<object_total;i++){ objects[i]={}; }
    objects[4].flg=1; objects[4].id=52; objects[4].flg2=1; objects[4].mode3=0;
    objects[4].py=10.0f; objects[4].ay=0; objects[4].ayp=0;
    recvx_en04(&w);
    /* en04.c:523: der dam-Satz wird nach DmgChk/MainLoop geleert. */
    require(w.dam[3]==0&&!(w.flg&4u),"bhEne04 dam-Reset en04.c:523-532 differs");
    /* en04.c:549/551: beide Zaehler laufen am Passende ab. */
    require(e->ct_14==1&&e->ct_18==2,"bhEne04 Zaehler en04.c:546-554 differs");
    /* en04.c:539: nur Objekte ab Index 4 bis rom->obj_n-2 mit id 52. */
    require(objects[4].mode1==1&&objects[4].mode3==1&&objects[4].ct0==1,
            "bhEne04_ShakeWire en04.c:3975-3985 differs");
    require(std::fabs(objects[4].py-(10.0f+0.1f))<1e-6f,
            "ShakeWire py en04.c:3980 differs");
    require(objects[6].mode1==0,"bhEne04 darf obj_n-1 nicht ruetteln (en04.c:539)");
    /* en04.c:3988: nach zehn Schritten endet das Ruetteln. */
    reset(); object_total=7;
    for(unsigned i=0;i<object_total;i++) objects[i]={};
    objects[4].flg=1; objects[4].id=52; objects[4].flg2=1; objects[4].mode3=1; objects[4].ct0=9;
    auto s=make(); s.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    recvx_en04_shake_wire(&s,4);
    require(objects[4].mode3==2&&!(objects[4].flg2&1u),"ShakeWire Ende en04.c:3987 differs");
    recvx_en04_shake_wire(&s,4);
    require(objects[4].mode1==0,"ShakeWire en04.c:3994 differs");
}

/* ---------------------------------------------------------------------------
   12) bhEne04_RotNeck / SearchNeck (en04.c:3997/4008)
   --------------------------------------------------------------------------- */
void neck(){
    reset();
    auto w=make(); w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    recvx_en04_rot_neck(&w,0,4096,0);
    /* en04.c:4001-4003: objP[2] = ry, objP[3] = ry + ry/4, objP[4] = ry + ry/2. */
    require(node_angles[2][1]==4096&&node_angles[3][1]==4096+1024&&node_angles[4][1]==4096+2048,
            "bhEne04_RotNeck en04.c:4001 differs");
    /* en04.c:4040: +-4096 ist die Grenze. */
    reset();
    auto l=make(); l.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(l.exp0)->neck=5000;
    require(recvx_en04_search_neck(&l)==1&&
            static_cast<RECVX_EN04_EXP0*>(l.exp0)->neck==4096,
            "bhEne04_SearchNeck Grenze en04.c:4040 differs");
}

} // namespace

/* en04.c:3413/3490/3500 indizieren En04_PlyMtn_OffsetTbl[4] direkt mit sys->ply_id;
   ein Index ausserhalb ist keine Zeile 0 (Claire), sondern eine benannte Luecke.
   en04.c:3548 vergleicht frm mit pl->mnwP[mtn].frm_num - 1; fehlt die Framezahl,
   wird das gemeldet und der Abschluss nicht aus einer erfundenen Grenze gezogen. */
void player_branch_gaps(){
    for(int branch=0;branch<2;++branch){
        reset();
        auto w=make(); w.exp0=allocate(sizeof(RECVX_EN04_EXP0));
        static_cast<RECVX_EN04_EXP0*>(w.exp0)->flg|=0x40000u;
        ply.mode0=4; ply.mode1=uint8_t(branch); ply.mode3=0; ply.ply_id=4; ply.mtn_no=77;
        recvx_en04_player_control(&w);
        require(ply.mtn_no==77,"ungueltige ply_id darf keine Claire-Motion setzen");
        require(!gaps.empty()&&gaps.back()=="En04_PlyMtn_OffsetTbl[sys->ply_id]","ungueltige ply_id wird nicht gemeldet");
    }
    reset();
    auto d=make(); d.exp0=allocate(sizeof(RECVX_EN04_EXP0));
    static_cast<RECVX_EN04_EXP0*>(d.exp0)->flg|=0x40000u;
    ply.mode0=4; ply.mode1=1; ply.mode3=1; ply.ply_id=0; ply.frm_no=-65536; ply.mtn_add=65536;
    frames_per_motion=0;
    recvx_en04_player_control(&d);
    require(ply.mtn_add==65536&&!(ply.flg&2u),"PlyDG01 ohne frm_num darf nicht abschliessen");
    require(!gaps.empty()&&gaps.back()=="pl->mnwP[mtn] (EN04 PlyDG01)","fehlende frm_num wird nicht gemeldet");
    frames_per_motion=20;
}
int main(){
    try {
        tables(); init(); brain_route(); escape_mode1(); move_mv00(); mv01_mv02(); mv11_stairs();
        damage(); damage_states(); death(); grab(); player_damage(); frame(); neck(); player_branch_gaps();
        clear_allocations();
        recvx_en04_set_context(nullptr); recvx_en04_set_data(nullptr);
        std::cout<<"en04 component contract ok\n";
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"en04 test failed: "<<ex.what()<<"\n";
        return 1;
    }
}
