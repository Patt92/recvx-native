/* EN10 einzeln: diagnostische Dienstfixtures, kein Gameplaynachweis.
   Erwartungen aus en10.c:30/94/159/188/259/297/354, nicht aus Host-Replays. */
#include "en10.h"
#include "en03_lib.h"
#include "core_random.h"
#include "ninja_math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
RECVX_ENEMY_PLAYER pl{};
std::vector<void*> blocks;
std::vector<std::string> gaps,trace;
std::vector<int> sounds;
std::vector<int32_t> randoms;
unsigned random_index=0,model_calls=0,wall_calls=0,player_calls=0,effects=0,mlw=0;
RECVX_T3_EFF last_effect{};
int motor=-1,power=-1;
void gap(const char* s){gaps.emplace_back(s);}
void* allocate(uint32_t n){require(n==0x24,"EN10 exp0 allocation differs");void* p=std::calloc(1,n);blocks.push_back(p);return p;}
int32_t random_next(){require(random_index<randoms.size(),"unexpected RNG draw");return randoms[random_index++];}
int player_get(RECVX_ENEMY_PLAYER* out){*out=pl;return 1;}
void player_set(const RECVX_ENEMY_PLAYER* in){pl=*in;trace.emplace_back("player");}
uint32_t now(){return 6;}
void se(uint32_t i,const RECVX_ENEMY_VEC*,int n){require(i==6,"enemy SE lost enow");sounds.push_back(n);trace.emplace_back("sound");}
int wall(RECVX_ENEMY_WORK*){++wall_calls;trace.emplace_back("wall");return 0;}
int contact(RECVX_ENEMY_WORK*){++player_calls;trace.emplace_back("contact");return 0;}
void model(RECVX_ENEMY_WORK*){++model_calls;trace.emplace_back("model");}
// en10.c:51 mlwP = &mdl[EXP0_UC(0x1)] ueber select_model; mdl_no ist der Skin-Index.
int select(RECVX_ENEMY_WORK*,uint32_t k){mlw=k;return 1;}
int matrix(const RECVX_ENEMY_WORK*,uint32_t node,float* m){require(node==1,"EN10 capsule uses wrong node");std::fill(m,m+16,0.f);m[0]=m[5]=m[10]=m[15]=1;m[12]=2;m[13]=3;m[14]=4;return 1;}
void vibration(int m,int p){motor=m;power=p;trace.emplace_back("vibration");}
int effect(const RECVX_T3_EFF* ef,const RECVX_ENEMY_VEC* ofs,RECVX_T3_LINK link,int node){
    require(!ofs&&!link.w&&!link.player&&node==0,"EN10 blood must not link to an actor");
    last_effect=*ef;++effects;trace.emplace_back("effect");return 19;
}
void blood(const RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC* vec,int col,int a,int b){require(vec->y==1&&col==1&&a==1&&b==0,"death blood arguments differ");trace.emplace_back("blood");}
RECVX_ENEMY_SERVICES services(){RECVX_ENEMY_SERVICES s{};s.gap=gap;s.random_next=random_next;s.calloc_work=allocate;s.player_get=player_get;s.player_set=player_set;s.enemy_now=now;s.enemy_se=se;s.check_wall=wall;s.select_model=select;s.check_player=contact;s.calc_model=model;s.joint_matrix=matrix;s.vibration=vibration;s.blood_effect4=blood;return s;}
void reset(){for(auto p:blocks)std::free(p);blocks.clear();pl={};pl.pos={100,0,100};gaps.clear();trace.clear();sounds.clear();randoms={4,1};random_index=0;model_calls=wall_calls=player_calls=effects=0;motor=power=-1;last_effect={};}
RECVX_ENEMY_WORK init(RECVX_ENEMY_SERVICES& s){RECVX_ENEMY_WORK w{};w.id=10;w.flg=1;w.sv=&s;recvx_en10(&w);return w;}
template<class T> T at(const RECVX_ENEMY_WORK& w,unsigned off){T out;std::memcpy(&out,static_cast<const char*>(w.exp0)+off,sizeof out);return out;}
template<class T> void set(RECVX_ENEMY_WORK& w,unsigned off,T v){std::memcpy(static_cast<char*>(w.exp0)+off,&v,sizeof v);}
void init_and_models(){
    reset();auto s=services();auto w=init(s);
    require(w.mode0==1&&w.mode1==1&&w.mode2==1&&w.mode3==0,"Init modes differ");
    require(w.hp==1&&w.ar==1&&w.car==1&&w.cah==1&&w.spd==0.14f,"Init body differs");
    require(w.flg==0x139&&random_index==2&&at<int32_t>(w,4)==1092,"Init flags/RNG/turn differ");
    require(mlw==0&&w.mdl_no==0&&at<uint8_t>(w,0)==1,"first pass model clock differs");
    require(w.watr.c1.x==2&&w.watr.c1.y==float(double(3)+0.2)&&w.watr.c2.y==3&&w.watr.r==0.8f,"capsule differs");
    // Damage/Die are genuinely empty, but outer model switching still runs.
    w.mode0=4;
    const unsigned expected[11]={0,1,1,1,2,2,2,3,3,3,0};
    for(unsigned i=0;i<11;++i){recvx_en10(&w);require(mlw==expected[i]&&w.mdl_no==0,"model cycle is not three passes per mdl");}
    require(gaps.empty(),"complete EN10 fixture produced gap");
}
void bite(){
    reset();auto s=services();auto w=init(s);RECVX_ENEMY_EXT ext{};ext.effect_tb=effect;recvx_enemy_ext_bind(&ext);
    w.px=10;w.py=2;w.pz=20;w.ay=8192;w.mode0=2;w.mode3=0;pl.hp=7;
    trace.clear();recvx_en10(&w);
    require(pl.hp==6&&(pl.flg&4)&&pl.mode0==2&&pl.mode1==0&&pl.mode2==0&&pl.mode3==0,"bite does not update the actual player view");
    require(w.ct0==9&&w.mode3==1&&motor==1&&power==10,"bite fallthrough/vibration differs");
    require(effects==1&&last_effect.flg==1&&last_effect.id==5&&last_effect.type==0&&last_effect.mdlver==0,"bite effect identity differs");
    require(last_effect.px==9.5f&&last_effect.py==2.3f&&last_effect.pz==19.5f&&last_effect.sx==2&&last_effect.sy==2&&last_effect.sz==2&&last_effect.ay==8192,"bite effect transform differs");
    require(trace.size()>=4&&trace[0]=="player"&&trace[1]=="sound"&&trace[2]=="vibration"&&trace[3]=="effect","bite side-effect order differs");
    for(unsigned i=0;i<9;++i)recvx_en10(&w);
    require(w.ct0==0&&w.mode0==2&&pl.hp==6&&effects==1,"bite ended early or repeated its effect");
    randoms.push_back(4);randoms.push_back(0);recvx_en10(&w);
    require(w.ay==8192+32768&&w.mode0==1&&w.mode2==1&&w.mode3==0,"bite release differs");
    require(at<uint8_t>(w,0x20)==29&&at<int32_t>(w,4)==-728,"cooldown postpass / float turn differs");
    // Original reads/decrements cooldown as unsigned: 0xFF -> 0xFE.
    w.mode0=4;set<uint8_t>(w,0x20,255);recvx_en10(&w);require(at<uint8_t>(w,0x20)==254,"cooldown sign changed");
    // No effect callback must be diagnosed, not turned into a scale-1 spawn.
    recvx_enemy_ext_bind(nullptr);w.mode0=2;w.mode3=0;recvx_en10(&w);
    require(effects==1&&std::find(gaps.begin(),gaps.end(),"bhSetEffectTb (EN10 Biss)")!=gaps.end(),"missing effect provider not reported");
}
void move_and_damage(){
    reset();auto s=services();auto w=init(s);
    w.mode1=0;w.mode2=1;randoms.push_back(9);
    recvx_en10(&w);require(w.ct0==18&&w.mode3==1&&sounds.back()==0x12300,"MV01 initial fallthrough/SE differs");
    require(std::fabs(w.px+0.14f*njSin(1092))<1e-6f,"MV01 movement differs");
    // MV02 uses integer abs on position: 0.9 rounds to 0, 1.0 does not.
    w.mode2=2;w.mode3=1;w.px=0.9f;w.pz=0.9f;w.spd=0;recvx_en10(&w);
    require(w.mode3==2,"home return did not preserve integer truncation");
    w.mode3=1;w.px=1.0f;recvx_en10(&w);require(w.mode3==1,"home return accepted distance 1");
    w.mode0=1;w.mode2=0;w.flg|=4;w.dam[12]=10;recvx_en10(&w);
    require(w.mode0==4&&w.hp==-1&&(w.mdflg&1)&&!(w.flg&4)&&w.dam[12]==0,"damage death/reset differs");
    require(sounds.back()==0x2303,"death SE differs");
}
void shared_rng(){
    reset();auto s=services();s.random_next=nullptr;recvx_core_random_seed(1);
    auto w=init(s); // no synthetic RNG default when a service is absent
    require(at<int32_t>(w,4)==0,"EE first turn value differs");
    require(recvx_core_random_next()==662824084,"EN10 consumed the wrong number of shared RNG values");
    require(gaps.empty(),"existing shared RNG was treated as missing");
}
}
int main(){try{_Make_SinTable();init_and_models();bite();move_and_damage();shared_rng();reset();recvx_enemy_ext_bind(nullptr);std::cout<<"EN10 component PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';reset();return 1;}}
