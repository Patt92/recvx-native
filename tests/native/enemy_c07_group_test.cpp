/* C07 Komponentenpruefung. Dienstfixtures sind diagnostische Aufstellungen,
   KEIN Runtime-/Gameplaynachweis. Behauptete Werte stammen aus en11.c und
   den jeweils bezeichneten Originalstellen, nicht aus Host-Spielverhalten. */
#include "en11.h"
#include "en12.h"
#include "en14.h"
#include "en18.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool b,const char* msg) { if(!b) throw std::runtime_error(msg); }
namespace {
std::vector<std::string> gaps;
std::vector<void*> allocations;
std::vector<int> sounds;
std::vector<std::string> trace;
RECVX_ENEMY_PLAYER ply;
RECVX_EN11_EFFECT lamp;
std::array<std::array<int32_t,3>,4> angles{};
uint32_t flags[4];
int weapon_j1=-1,weapon_j2=-1;float weapon_r=-1;
int32_t random_value=0;
unsigned random_calls=0,line_calls=0,collision_calls=0,models=0,motions=0,pan_calls=0;
void record_gap(const char* n){ gaps.emplace_back(n); if(std::getenv("C07_SHOW_GAPS"))std::cerr<<"GAP: "<<n<<"\n"; }
void* allocate(uint32_t n){auto* p=std::calloc(1,n);allocations.push_back(p);return p;}
void clear_allocations(){for(auto* p:allocations)std::free(p);allocations.clear();}
int get_player(RECVX_ENEMY_PLAYER* p){*p=ply;return 1;}
void set_player(const RECVX_ENEMY_PLAYER* p){ply=*p;}
uint32_t system_flags(uint32_t bank){return flags[bank];}
void flags_or(uint32_t b,uint32_t mask){flags[b]|=mask;}
void flags_clear(uint32_t b,uint32_t mask){flags[b]&=~mask;}
uint32_t now(){return 3;}
int32_t random_next(){++random_calls;return random_value;}
int line(uint32_t variant,const RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC*,uint32_t f,int floor,RECVX_ENEMY_ATR*,uint32_t* id){require(variant==1&&f==0&&floor==0,"EN11 line uses wrong original variant");++line_calls;if(id)*id=0;return 0;}
float ground(const RECVX_ENEMY_VEC*){return 0;}
int collision(RECVX_ENEMY_WORK*){++collision_calls;trace.emplace_back("collision");return 0;}
int motion(RECVX_ENEMY_WORK*,int add,uint32_t){++motions;trace.emplace_back("motion");require(add==0,"EN11 static model must sample with add0");return 0;}
/* Die add0-Erwartung oben gilt nur fuer das statische EN11-Modell; die
   uebrigen Identitaeten der Gruppe schieben ihre Motion normal vor. */
int motion_any(RECVX_ENEMY_WORK*,int,uint32_t){++motions;trace.emplace_back("motion");return 0;}
void model(RECVX_ENEMY_WORK*){++models;trace.emplace_back("model");}
void sound(uint32_t index,const RECVX_ENEMY_VEC*,int se){require(index==3,"original enow lost");sounds.push_back(se);}
int playing(uint32_t,int){return 0;}
void pan(uint32_t,const RECVX_ENEMY_VEC*,int){++pan_calls;}
int create(const RECVX_EN11_EFFECT_TEMPLATE* t,RECVX_ENEMY_WORK* link){require(t->id==261&&t->flg==1&&t->type==0&&t->mdlver==0&&t->pos.x==0&&t->pos.y==0&&t->pos.z==0&&t->scale.x==0&&t->scale.y==0&&t->scale.z==0&&t->ay==0&&link->id==11,"EN11 Eff261 original template changed");lamp={};return 7;}
int read_effect(int slot,RECVX_EN11_EFFECT* out){require(slot==7,"EN11 lost linked effect identity");*out=lamp;return 1;}
void set_effect(int slot,const RECVX_EN11_EFFECT* in){require(slot==7,"EN11 patches wrong effect");lamp=*in;}
int node_get(RECVX_ENEMY_WORK*,uint32_t n,int32_t* out){std::copy(angles.at(n).begin(),angles.at(n).end(),out);return 1;}
void node_set(RECVX_ENEMY_WORK*,uint32_t n,const int32_t* in){std::copy(in,in+3,angles.at(n).begin());}
void node_flags(RECVX_ENEMY_WORK*,uint32_t,uint32_t,uint32_t){}
void reset(){weapon_j1=weapon_j2=-1;weapon_r=-1;clear_allocations();gaps.clear();sounds.clear();trace.clear();ply={};lamp={};angles={};std::fill(flags,flags+4,0);random_value=0;random_calls=line_calls=collision_calls=models=motions=pan_calls=0;}
/* Die Fixtures binden absichtlich nur die Dienste des geprueften
   Originalzweigs. Darum wird nicht "keine Luecke" behauptet, sondern dass
   KEINE Luecke den geprueften Zweig betrifft. */
void require_no_gap_in(const char* part,const char* msg){
 for(const auto& g:gaps) require(g.find(part)==std::string::npos,msg);
}
RECVX_ENEMY_SERVICES services(){RECVX_ENEMY_SERVICES s{};s.gap=record_gap;s.calloc_work=allocate;s.player_get=get_player;s.player_set=set_player;s.line=line;s.ground=ground;s.collision_works=collision;s.set_motion=motion;s.calc_model=model;s.system_flags=system_flags;s.system_flags_or=flags_or;s.enemy_now=now;s.enemy_se=sound;s.enemy_se_playing=playing;s.random_next=random_next;return s;}
RECVX_EN11_SERVICES services11(){RECVX_EN11_SERVICES s{};s.flags_clear=flags_clear;s.effect_create=create;s.effect_get=read_effect;s.effect_set=set_effect;s.node_angles_get=node_get;s.node_angles_set=node_set;s.node_work_flags=node_flags;s.se_pan=pan;return s;}
void en11_contract(){
 reset();auto sv=services();auto ex=services11();recvx_en11_bind(&ex);
 RECVX_ENEMY_WORK w{};w.id=11;w.flg=7;w.sx=w.sy=w.sz=1;w.px=4;w.py=30;w.pz=9;w.sv=&sv;
 flags[1]=0x101;recvx_en11(&w);
 require(gaps.empty(),"EN11 Init service fixture incomplete");
 require(w.mode0==1&&w.mode1==1&&w.mode2==0&&w.mode3==0&&w.hp==0&&w.ar==3.5f&&w.ah==1.5f&&w.car==1&&w.cah==1&&w.spd==0.2f,"EN11 Init values en11.c:142 differ");
 require(w.flg==0x1849&&flags[1]==0x100&&w.clp_jno[0]==1&&w.clp_jno[1]==4&&w.clp_jno[2]==-1,"EN11 flags/clipping differ");
 require(trace==std::vector<std::string>({"motion","collision","model"}),"EN11 Init post-dispatch order differs");
 require(motions==1&&models==1&&line_calls==1,"EN11 Init calls differ");
 require(std::fabs(w.root_mtx[5]+1)<1e-6f&&w.root_mtx[12]==4&&w.root_mtx[13]==30&&w.root_mtx[14]==9,"EN11 root orientation/translation not preserved");
 // MV00 postdecrement: first pass sets30 then decrements to29. Exit on31st.
 for(unsigned i=0;i<30;++i)recvx_en11(&w);
 require(w.mode2==0&&w.ct0==0,"EN11 MV00 ended before original postdecrement");
 recvx_en11(&w);require(w.mode2==1&&w.mode3==0&&w.ct0==-1,"EN11 MV00 transition/ct0 differs");
 // Run end-of-scan with no camera interpolation (count0 avoids zero cross).
 w.mode2=1;w.mode3=1;w.ct0=0;w.ct1=0;w.ct2=0;random_value=0;
 float z=w.pz;recvx_en11(&w);
 require(w.ct0==99&&w.ct2==1&&w.pz==z-0.2f&&random_calls==2,"EN11 scan timer/movement/RNG order differs");
 require(sounds==std::vector<int>({0x11300}),"EN11 scan loop SE differs");
 // Shared alarm: ct2 from Eff261 sets rm_flg unless st_flg0x2000, after
 // Brain the current scan still runs, THEN switches to MV07.
 lamp.ct2=1;flags[2]=0x2000;w.mode2=1;w.mode3=1;w.ct0=99;w.ct1=0;
 recvx_en11(&w);require(!(flags[1]&1)&&w.mode2==7&&w.mode3==0,"EN11 alarm suppression/order differs");
 flags[2]=0;recvx_en11(&w);require((flags[1]&1)&&w.mode3==1&&sounds.back()==0x2302,"EN11 alarm SE or room flag differs");
 auto count=sounds.size();recvx_en11(&w);require(sounds.size()==count,"EN11 MV07 repeats one-shot sound");
 // Reset linked work while preserving effect allocation and unowned fields.
 w.mode0=0;lamp.ct2=91;lamp.ax=12;recvx_en11(&w);
 require(lamp.mode0==0&&lamp.ct2==0&&lamp.ax==12,"EN11 reinit does not preserve effect fields");
 // Original no-op Nage/Damage/Die still execute outer collision/model path.
 for(unsigned mode=2;mode<=4;mode++){w.mode0=uint8_t(mode);unsigned before=models;recvx_en11(&w);require(models==before+1&&w.mode0==mode,"EN11 proven empty state outer path lost");}
 // Missing callbacks must not yield a fabricated pose or advance scan.
 w.mode0=1;w.mode2=1;w.mode3=1;w.ct1=0;auto incomplete=ex;incomplete.node_angles_set=nullptr;recvx_en11_bind(&incomplete);auto before=models;recvx_en11(&w);require(!gaps.empty()&&models==before,"EN11 missing node service silently replaced");
 recvx_en11_bind(nullptr);clear_allocations();
}

/* ---------------------------------------------------------------------------
   EN12 — bhEne12_Init, en12.c:516-585. Alle behaupteten Werte stehen dort;
   es wird kein Hostverhalten gegen sich selbst geprueft. */
int tb_id=-1,tb_flg=-1;uint32_t tb_mask=0,cleared_bank=0,cleared_bits=0;
int shadow_lkono=-1;float shadow_s[3]={0,0,0};const int8_t* shadow_tab=nullptr;
int effect_tb(const RECVX_EN12_EFFECT_TEMPLATE* t,uint32_t mask,RECVX_ENEMY_WORK*,
              const RECVX_ENEMY_VEC*,int){tb_id=int(t->id);tb_flg=int(t->flg);tb_mask=mask;return 3;}
void flags_clear_bank(uint32_t which,uint32_t bits){cleared_bank=which;cleared_bits=bits;}
void weapon_atr(RECVX_ENEMY_WORK*,int j1,int j2,float r){weapon_j1=j1;weapon_j2=j2;weapon_r=r;}
int shadow(const int8_t* jtb,RECVX_ENEMY_WORK*,int lkono,float sx,float sy,float sz)
{shadow_tab=jtb;shadow_lkono=lkono;shadow_s[0]=sx;shadow_s[1]=sy;shadow_s[2]=sz;return 5;}
void en12_contract(){
 reset();tb_id=tb_flg=-1;tb_mask=0;cleared_bank=cleared_bits=0;shadow_lkono=-1;
 auto sv=services();sv.set_shadow=shadow;sv.set_motion=motion_any;sv.weapon_atr=weapon_atr;
 RECVX_EN12_SERVICES ex{};ex.effect_tb=effect_tb;ex.system_flags_clear=flags_clear_bank;
 RECVX_ENEMY_WORK w{};w.id=12;w.sv=&sv;
 require(recvx_en12_bind(&w,&ex),"EN12 bind must allocate exp0 through calloc_work");
 recvx_en12(&w);
 /* en12.c:520-521/523-528: flg |= 0x78, flg &= ~6, Koerper und HP. */
 require((w.flg&0x78u)==0x78u&&!(w.flg&6u),"EN12 Init flg en12.c:520 differs");
 require(w.ar==3.5f&&w.ah==1.0f&&w.car==3.0f&&w.hp==300,"EN12 Init body/HP en12.c:523 differs");
 /* en12.c:530-533 Modi, :535-541 Motion. hokan_rate ist 0, NICHT 65536. */
 require(w.mode0==1&&w.mode1==1&&w.mode2==1&&w.mode3==0,"EN12 Init modes en12.c:530 differ");
 require(w.mtn_no==0&&w.mtn_md==0x20u&&w.hokan_rate==0&&w.hokan_count==0
         &&w.mtn_add==65536&&w.frm_no==0,"EN12 Init motion en12.c:535 differs");
 /* en12.c:543-550 clp_jno. Der Doppelwert 13 bei 4 und 5 ist original. */
 const int8_t clip12[8]={5,1,7,11,13,13,17,21};
 require(std::equal(clip12,clip12+8,w.clp_jno),"EN12 Init clp_jno en12.c:543 differs");
 /* en12.c:554-556: eff-Vorlage 262/flg 1 vor der Poolallokation. */
 require(tb_id==262&&tb_flg==1&&tb_mask==(RECVX_EN12_EF_ID|RECVX_EN12_EF_FLG),
         "EN12 Init eff262 template en12.c:554 differs");
 /* en12.c:570: bhSetShadow(SdwTab, epw, 2, 6,6,4) und danach flg 0x800. */
 require(shadow_lkono==2&&shadow_s[0]==6.0f&&shadow_s[1]==6.0f&&shadow_s[2]==4.0f
         &&(w.flg&0x800u),"EN12 Init bhSetShadow en12.c:570 differs");
 /* en12.c:231 SdwTab = {1,2,5,0xD,9,0x15,0x11,0xFF}; 0xFF ist signed -1. */
 require(shadow_tab&&shadow_tab[0]==1&&shadow_tab[2]==5&&shadow_tab[3]==0xD
         &&shadow_tab[6]==0x11&&shadow_tab[7]==-1,"EN12 SdwTab en12.c:231 differs");
 /* en12.c:574-580: stflg &= ~8, lok_jno 5, flg &= ~0x100000, mdflg &= ~0x20. */
 require(!(w.stflg&8u)&&w.lok_jno==5&&!(w.flg&0x100000u)&&!(w.mdflg&0x20u),
         "EN12 Init trailer en12.c:574 differs");
 /* en12.c:584 sys->rm_flg &= ~1 — Bank 1 Bit 0, nicht cb_flg. */
 require(cleared_bank==1u&&cleared_bits==1u,"EN12 Init rm_flg clear en12.c:584 differs");
 /* en12.c:502 bhEne_SetWeponAtr(epw, 5, 21, 4.0f) im Rahmen von bhEne12. */
 require(weapon_j1==5&&weapon_j2==21&&weapon_r==4.0f,"EN12 bhEne_SetWeponAtr en12.c:502 differs");
 require_no_gap_in("en12 Init","EN12 Init reports an unbound original service");
 require_no_gap_in("bhEne12_Init","EN12 Init reports an unbound original service");
 clear_allocations();
}

/* ---------------------------------------------------------------------------
   EN14 — bhEne14_Init, en14.c:225-322, und bhEne14_TailInit, en14.c:1086. */
const RECVX_EN14_SERVICES* en14_ex=nullptr;
const RECVX_EN14_SERVICES* en14_resolve(const RECVX_ENEMY_WORK*){return en14_ex;}
uint32_t enemy_now_zero(){return 0;}
RECVX_ENEMY_WORK* en14_parent=nullptr;
RECVX_ENEMY_WORK* enemy_at(uint32_t i){return i==0?en14_parent:nullptr;}
int joint_identity(const RECVX_ENEMY_WORK*,uint32_t joint,float* out)
{for(int i=0;i<16;i++)out[i]=0;out[0]=out[5]=out[10]=out[15]=1;
 out[12]=float(joint);out[13]=0;out[14]=0;return 1;}
int beam_none(uint32_t* h){if(h)*h=0;return 1;}
void en14_contract(){
 for(unsigned type=0;type<2;type++){
  reset();shadow_lkono=-1;
  auto sv=services();sv.set_shadow=shadow;sv.enemy_now=enemy_now_zero;sv.set_motion=motion_any;sv.weapon_atr=weapon_atr;
  sv.enemy_at=enemy_at;sv.joint_matrix=joint_identity;
  RECVX_EN14_SERVICES ex{};ex.find_beam=beam_none;en14_ex=&ex;
  recvx_en14_bind_services(en14_resolve);
  RECVX_ENEMY_WORK parent{};parent.id=13;parent.flg=1;parent.px=7;parent.py=8;parent.pz=9;parent.ay=0x1000;
  en14_parent=&parent;
  RECVX_ENEMY_WORK w{};w.id=14;w.type=uint16_t(type);w.sv=&sv;
  recvx_en14(&w);
  /* en14.c:230-233: ar 8, ah 25, dann car ZWEIMAL (4, danach 25). Der
     zweite Schreibzugriff gewinnt; 4 ist kein Endzustand. */
  require(w.ar==8.0f&&w.ah==25.0f&&w.car==25.0f,"EN14 Init body en14.c:230 differs");
  require(w.hp==400,"EN14 Init HP en14.c:234 differs");
  if(type==0){
   /* en14.c:238-249 Typ 0 und :285-299 EN13-Verknuepfung. */
   require(w.mode0==1&&w.mode1==0&&w.mode2==0&&w.mode3==0&&w.mtn_no==0,
           "EN14 Init type0 state en14.c:238 differs");
   require(w.lkwkp==&parent&&w.lkono==0&&w.px==7&&w.py==8&&w.pz==9&&w.ay==0x1000
           &&!(w.flg&0x10u),"EN14 Init type0 EN13 link en14.c:287 differs");
   require(w.stflg&8u,"EN14 Init type0 stflg en14.c:318 differs");
  } else {
   /* en14.c:252-263 Typ 1 und :307-316 Schatten. */
   require(w.mode0==1&&w.mode1==1&&w.mode2==4&&w.mode3==0&&w.mtn_no==5,
           "EN14 Init type1 state en14.c:252 differs");
   require(shadow_lkono==2&&shadow_s[0]==6.0f&&shadow_s[2]==4.0f&&(w.flg&0x800u)
           &&!(w.stflg&8u),"EN14 Init type1 bhSetShadow en14.c:309 differs");
  }
  require(w.hokan_rate==65536&&w.hokan_count==0&&w.mtn_add==65536&&w.frm_no==0
          &&w.mtn_md==0,"EN14 Init motion en14.c:244 differs");
  /* en14.c:267-274 clp_jno, :278 mdflg, :283 flg &= ~0x80. */
  const int8_t clip14[8]={6,1,7,9,11,15,18,21};
  require(std::equal(clip14,clip14+8,w.clp_jno),"EN14 Init clp_jno en14.c:267 differs");
  require(!(w.mdflg&0x20u)&&!(w.flg&0x80u),"EN14 Init mdflg/flg en14.c:278 differs");
  /* en14.c:1086-1104: elf Schwanzglieder, Ruhelage = owP[11+i], Laenge 2.5. */
  auto* st=(RECVX_EN14_STATE*)w.exp0;
  require(st,"EN14 Init EXP0 missing");
  for(unsigned i=0;i<11;i++)
   require(st->length[i]==2.5f&&st->position[i].x==float(11+i)
           &&st->velocity[i].x==0,"EN14 TailInit en14.c:1086 differs");
  /* en14.c:206 bhEne_SetWeponAtr(epw, 6, 1, 5.0f) im Rahmen von bhEne14. */
  require(weapon_j1==6&&weapon_j2==1&&weapon_r==5.0f,"EN14 bhEne_SetWeponAtr en14.c:206 differs");
  require_no_gap_in("bhEne14_Init","EN14 Init reports an unbound original service");
  require_no_gap_in("TailInit","EN14 TailInit reports an unbound original service");
  recvx_en14_bind_services(nullptr);en14_ex=nullptr;en14_parent=nullptr;
  clear_allocations();
 }
}

/* ---------------------------------------------------------------------------
   EN18 — bhEne18_Init en18.c:373-620, bhEne18_HitMark en18.c:973-1073 und
   die Kinderzeugung in en13.c:96-170. Jede Behauptung ist dort belegt. */
RECVX_ENEMY_ATR mobile_wall{};int mobile_wall_calls=0;
int wall_alloc(){return mobile_wall_calls++;}
int wall_get(int index,RECVX_ENEMY_ATR* out){require(index==0,"EN18 wrong mobile wall");*out=mobile_wall;return 1;}
void wall_set(int index,const RECVX_ENEMY_ATR* in){require(index==0,"EN18 wrong mobile wall");mobile_wall=*in;}
static int root_tokens[3]={1,2,3};
void* model_root(const RECVX_ENEMY_WORK*,uint32_t slot){return &root_tokens[slot%3];}
uint32_t obj_num_fixed(const RECVX_ENEMY_WORK*){return 14;}
void en18_contract(){
 RECVX_EN18_SERVICES ex{};ex.model_root=model_root;recvx_en18_bind(&ex);
 /* en13.c:15 EGG_WORK ene18 = {0x8081, 18, ...}: die Kinder tragen id 18,
    und en13.c:100/113/126/139/159 setzen lkwkp = Parent, lkono 0, lo* 0. */
 const unsigned child_types[10]={1,2,3,0,4,5,6,7,8,9};
 for(unsigned k=0;k<10;k++){
  unsigned type=child_types[k];
  reset();mobile_wall={};mobile_wall_calls=0;
  auto sv=services();sv.set_motion=motion_any;sv.mobile_wall_alloc=wall_alloc;sv.mobile_wall_get=wall_get;
  sv.mobile_wall_set=wall_set;sv.obj_num=obj_num_fixed;
  RECVX_ENEMY_WORK parent{};parent.id=13;parent.flg=1;parent.px=2;parent.py=3;parent.pz=4;
  RECVX_ENEMY_WORK w{};w.id=18;w.type=uint16_t(type);w.sv=&sv;w.lkwkp=&parent;w.lkono=0;
  random_value=2147483647;  /* rand/2^31 > 0.5 -> ct1 = +1 */
  recvx_en18(&w);
  require((w.flg&0x8000u)&&!(w.flg&6u)&&(w.mdflg&0x20u),"EN18 Init flg en18.c:389 differs");
  require(w.ar==5.0f&&w.ah==1.0f&&w.car==3.0f,"EN18 Init body en18.c:394 differs");
  /* en18.c:377 InitAction[10] = {1,0,...}: nur type 0 startet in MV01. */
  require(w.mode0==1&&w.mode1==0&&w.mode2==(type==0?1:0)&&w.mode3==0,
          "EN18 Init InitAction en18.c:377 differs");
  require(w.hokan_rate==65536&&w.mtn_add==65536&&w.mtn_no==0&&w.frm_no==0,
          "EN18 Init motion en18.c:402 differs");
  require(w.ct1==1,"EN18 Init rand>0.5 -> ct1 = 1 (en18.c:410) differs");
  /* en18.c:425-434: ein mobiler Wandplatz, type 3, alles andere null. */
  require(mobile_wall_calls==1&&mobile_wall.type==3u&&mobile_wall.flg==0u
          &&mobile_wall.w==0&&mobile_wall.h==0&&mobile_wall.d==0&&mobile_wall.attr==0u,
          "EN18 Init sys->mwalp entry en18.c:431 differs");
  if(type<4){
   require(!(w.flg&0x28u),"EN18 Init type<4 must return before flg 0x28 (en18.c:441)");
  } else {
   require((w.flg&0x28u)==0x28u&&(w.mdflg&1u),"EN18 Init flg 0x28 en18.c:443 differs");
   if(type==4)
    require(w.watr.r==11.0f&&w.watr.c1.y==21.0f&&w.watr.c2.y==8.0f&&w.watr.c1.x==2.0f,
            "EN18 Init type4 capsule en18.c:452 differs");
   else
    require(w.watr.r==(type<7?4.0f:6.0f),"EN18 Init capsule radius en18.c:464-617 differs");
  }
  require(gaps.empty(),"EN18 Init fixture incomplete (bhEne18 has no outer pass)");
  /* en18.c:1073 gibt total_dam zurueck; -1 ist die Fehlmarke des Ports.
     en13.c:629/744 nehmen nur `if (max_dam < dam)` mit max_dam = 0, also
     wird -1 dort nie Schaden. Hier wird nur die Marke geprueft. */
  RECVX_ENEMY_WORK h=w;auto no_obj=sv;no_obj.obj_num=nullptr;h.sv=&no_obj;
  require(recvx_en18_hitmark(&h)==-1,"EN18 HitMark must report -1 without obj_num");
  h.sv=&sv;h.wpnr_no=21;
  require(recvx_en18_hitmark(&h)==-1,"EN18 HitMark must report -1 for weapon >= 21");
  h.wpnr_no=0;h.djnt_no=14;
  require(recvx_en18_hitmark(&h)==-1,"EN18 HitMark must report -1 for joint >= 14");
  clear_allocations();
 }
 /* Ein Parent, der nicht EN13 ist, loescht das Kind (en18.c:383-387). */
 reset();auto sv=services();sv.set_motion=motion_any;
 RECVX_ENEMY_WORK other{};other.id=14;other.flg=1;
 RECVX_ENEMY_WORK w{};w.id=18;w.flg=0x8001;w.sv=&sv;w.lkwkp=&other;
 recvx_en18(&w);
 require(w.flg==0,"EN18 Init must clear flg for a non-EN13 parent (en18.c:385)");
 recvx_en18_bind(nullptr);clear_allocations();
}

/* RDT raw read: bhSetRoom room pointer file+16; ROM_WORK.enep+8,
   ene_n+0x88, ETTY stride36, id+4,type+6, flg Bit0. It verifies actual
   identity evidence only, not spawning or rendered gameplay. */
uint32_t u32(const std::vector<unsigned char>& b,size_t o){require(o+4<=b.size(),"RDT field out of range");return b[o]|uint32_t(b[o+1])<<8|uint32_t(b[o+2])<<16|uint32_t(b[o+3])<<24;}
/* Gruppenbeleg: 11/12/14 kommen in den Raumdaten vor, 18 und 27 nicht
   (18 erzeugt en13.c:100-170, 27 erzeugt en06.c:1182), und 13/14/30 teilen
   dieselben zwei Raeume. Zahlen gemessen, nicht geschaetzt. */
void data_contract(const std::filesystem::path& data){
 const unsigned ids[7]={11,12,13,14,18,27,30};
 const unsigned expect[7]={8,3,2,2,0,0,8};
 unsigned counts[7]={};
 std::array<std::vector<std::string>,7> where{};
 for(const auto& f:std::filesystem::directory_iterator(data/"rooms")){
  if(f.path().extension()!=".RDT")continue;
  std::ifstream in(f.path(),std::ios::binary);
  std::vector<unsigned char>b((std::istreambuf_iterator<char>(in)),{});
  auto h=u32(b,16),p=u32(b,h+8),n=u32(b,h+0x88);
  for(unsigned i=0;i<n;i++){auto q=p+i*36;auto packed=u32(b,q+4);
   for(unsigned k=0;k<7;k++)if((packed&0xffff)==ids[k]&&(u32(b,q)&1)){
    ++counts[k];if(where[k].empty()||where[k].back()!=f.path().filename().string())
     where[k].push_back(f.path().filename().string());}}}
 for(unsigned k=0;k<7;k++)
  require(counts[k]==expect[k],"C07 original ETTY identities/counts differ");
 /* 13, 14 und 30 stehen in genau denselben Raeumen: eine Kampfgruppe. */
 std::vector<std::string> a=where[2],b14=where[3],c=where[6];
 std::sort(a.begin(),a.end());std::sort(b14.begin(),b14.end());std::sort(c.begin(),c.end());
 require(a==b14&&a==c&&a.size()==2,"C07 EN13/EN14/EN30 room overlap differs");
}
}
/* ORIGINAL-FIX (QUALITY.md) in en12.c MV04 (en12.c:973-987): plp->mode2 ausserhalb 3..12 setzt
   angx/angz nicht. Der Sprungabstand ofx behaelt dann seinen Wert (geklemmt 20..45), mode3 laeuft
   weiter. Mit mode2 3 zielt ofx auf plp - 15 * (sin, cos)(ay). */
uint32_t en12_frames(const RECVX_ENEMY_WORK*,uint32_t){return 30u;}
void en12_mv04_fix(){
 reset();
 auto sv=services();sv.set_motion=motion_any;sv.motion_frames=en12_frames;
 RECVX_EN12_SERVICES ex{};ex.effect_tb=effect_tb;ex.system_flags_clear=flags_clear_bank;
 RECVX_ENEMY_WORK w{};w.id=12;w.sv=&sv;
 require(recvx_en12_bind(&w,&ex),"EN12 MV04: bind");
 w.flg=1;w.mode0=1;w.mode1=1;w.mode2=4;w.mode3=0;w.px=0;w.pz=0;w.ofx=33.0f;
 ply.mode2=0;ply.pos={100,0,0};ply.ay=0;
 recvx_en12(&w);
 require(w.ofx==33.0f,"EN12 MV04: ofx bei plp->mode2 0 nicht unveraendert (ORIGINAL-FIX)");
 require(w.mode3>=1,"EN12 MV04: mode3 laeuft bei plp->mode2 0 nicht weiter");
 for(const auto& g:gaps)require(g.find("f20/f2")==std::string::npos,"EN12 MV04: alte Luecke gemeldet");
 w.mode2=4;w.mode3=0;w.ofx=0;ply.mode2=3;ply.pos={0,0,40};ply.ay=0;
 recvx_en12(&w);
 require(std::fabs(w.ofx-25.0f)<1e-4f,"EN12 MV04: ofx bei plp->mode2 3 nicht |plp - 15*cos(ay)| = 25");
 recvx_en12_bind(nullptr,nullptr);clear_allocations();
}
int main(int argc,char**argv){try{_Make_SinTable();en11_contract();en12_contract();en12_mv04_fix();
 en14_contract();en18_contract();if(argc>1)data_contract(argv[1]);
 std::cout<<"C07 component and identity contracts passed (not gameplay).\n";return 0;}
 catch(const std::exception& e){clear_allocations();std::cerr<<e.what()<<'\n';return 1;}}
