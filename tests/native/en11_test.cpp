/* EN11 einzeln: alle Move-Dispatcher, Kameraknoten, Lichtbefehle und Reinit.
   Semantikreferenz en11.c:52-1249; kein Eff261-/Runtime-Nachweis. */
#include "en11.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
RECVX_ENEMY_PLAYER pl{};
RECVX_EN11_EFFECT lamp{};
std::vector<void*> blocks;
std::vector<std::string> gaps,trace;
std::vector<int> sounds;
std::array<std::array<int32_t,3>,4> angles{};
uint32_t flags[4]{};
int32_t rng=0;unsigned rng_calls=0,models=0,motions=0,effect_creates=0,effect_writes=0,lines=0;
void gap(const char* s){gaps.emplace_back(s);}
void* alloc(uint32_t size){require(size>=0x70,"EN11 EXP block smaller than original");auto p=std::calloc(1,size);blocks.push_back(p);return p;}
uint32_t sys(uint32_t i){return flags[i];}
void flags_or(uint32_t i,uint32_t b){flags[i]|=b;}
void flags_clear(uint32_t i,uint32_t b){flags[i]&=~b;}
int player(RECVX_ENEMY_PLAYER* p){*p=pl;return 1;}
int line(uint32_t v,const RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC*,uint32_t f,int floor,RECVX_ENEMY_ATR*,uint32_t* id){require(v==1&&f==0&&floor==0,"wrong line variant");++lines;if(id)*id=0;return 0;}
int collision(RECVX_ENEMY_WORK*){trace.emplace_back("collision");return 0;}
float ground(const RECVX_ENEMY_VEC*){return 0;}
int32_t random_next(){++rng_calls;return rng;}
int motion(RECVX_ENEMY_WORK*,int add,uint32_t){require(add==0,"EN11 idle motion must not advance");++motions;trace.emplace_back("motion");return 0;}
void model(RECVX_ENEMY_WORK*){++models;trace.emplace_back("model");}
uint32_t now(){return 3;}
void sound(uint32_t i,const RECVX_ENEMY_VEC*,int n){require(i==3,"ENOW differs");sounds.push_back(n);}
int playing(uint32_t,int){return 0;}
void pan(uint32_t,const RECVX_ENEMY_VEC*,int){}
int getnode(RECVX_ENEMY_WORK*,uint32_t i,int32_t* a){std::copy(angles.at(i).begin(),angles.at(i).end(),a);return 1;}
void setnode(RECVX_ENEMY_WORK*,uint32_t i,const int32_t* a){std::copy(a,a+3,angles.at(i).begin());}
void node_flags(RECVX_ENEMY_WORK*,uint32_t i,uint32_t set,uint32_t clear){require((i==2||i==3)&&((set==2&&clear==0)||(clear==2&&set==0)),"MV09 wrong node flag operation");trace.emplace_back(set?"flags_set":"flags_clear");}
int matrix(const RECVX_ENEMY_WORK*,uint32_t i,float* out){require(i<=1,"MV09 wrong matrix index");std::fill(out,out+16,0.f);out[0]=out[5]=out[10]=out[15]=1;out[12]=4;out[13]=30;out[14]=9;return 1;}
int create(const RECVX_EN11_EFFECT_TEMPLATE* t,RECVX_ENEMY_WORK* w){require(t->id==261&&t->flg==1&&t->type==0&&t->mdlver==0&&t->pos.x==0&&t->scale.x==0&&t->scale.y==0&&t->scale.z==0&&t->ay==0&&w->id==11,"wrong linked Eff261 template");++effect_creates;lamp={};return 7;}
int effect_get(int slot,RECVX_EN11_EFFECT* out){require(slot==7,"lost effect owner");*out=lamp;return 1;}
void effect_set(int slot,const RECVX_EN11_EFFECT* in){require(slot==7,"wrong effect slot write");++effect_writes;lamp=*in;}
RECVX_ENEMY_SERVICES services(){RECVX_ENEMY_SERVICES s{};s.gap=gap;s.calloc_work=alloc;s.system_flags=sys;s.system_flags_or=flags_or;s.player_get=player;s.line=line;s.ground=ground;s.collision_works=collision;s.random_next=random_next;s.set_motion=motion;s.calc_model=model;s.enemy_now=now;s.enemy_se=sound;s.enemy_se_playing=playing;s.joint_matrix=matrix;return s;}
RECVX_EN11_SERVICES extras(){RECVX_EN11_SERVICES s{};s.flags_clear=flags_clear;s.effect_create=create;s.effect_get=effect_get;s.effect_set=effect_set;s.node_angles_get=getnode;s.node_angles_set=setnode;s.node_work_flags=node_flags;s.se_pan=pan;return s;}
void reset(){for(auto p:blocks)std::free(p);blocks.clear();gaps.clear();trace.clear();sounds.clear();angles={};pl={};pl.pos={0,0,0};lamp={};std::fill(flags,flags+4,0u);rng=0;rng_calls=models=motions=effect_creates=effect_writes=lines=0;}
RECVX_ENEMY_WORK init(RECVX_ENEMY_SERVICES& sv,unsigned type){RECVX_ENEMY_WORK w{};w.id=11;w.flg=1;w.py=30;w.type=uint16_t(type);w.sv=&sv;recvx_en11(&w);return w;}
void layout_and_light(){reset();auto sv=services();auto ex=extras();recvx_en11_bind(&ex);auto w=init(sv,0);auto* s=static_cast<RECVX_EN11_STATE*>(w.exp0);require(reinterpret_cast<char*>(&s->alarm)-reinterpret_cast<char*>(s)==0x56,"alarm offset differs");require(reinterpret_cast<char*>(&s->dir)-reinterpret_cast<char*>(s)==0x5c,"dir offset differs");
    for(int mode=0;mode<4;++mode){lamp={9,42,123,456};recvx_en11_light_control(&w,mode);require(lamp.ct2==42,"LightControl clobbered detector ct2");if(mode==0)require(lamp.mode0==1&&lamp.axp==0x38e&&lamp.ax==123,"light open differs");if(mode==1)require(lamp.mode0==3&&lamp.axp==456,"light close differs");if(mode==2)require(lamp.mode0==0&&lamp.ax==123,"light disable differs");if(mode==3)require(lamp.mode0==2&&lamp.ax==0x38e&&lamp.axp==0x38e,"light steady differs");}
    auto writes=effect_writes;recvx_en11_light_control(&w,4);require(effect_writes==writes,"unknown LightControl command wrote the work");
    lamp.ax=77;lamp.ct2=31;w.mode0=0;recvx_en11(&w);require(effect_creates==1&&lamp.ct2==0&&lamp.ax==77,"reinit reallocated effect or reset unowned fields");
}
void all_move_modes(){for(unsigned type=0;type<5;++type){
    reset();auto sv=services();auto ex=extras();recvx_en11_bind(&ex);auto w=init(sv,type);require(gaps.empty()&&models==1,"Init/initial aim failed");
    for(unsigned mode=0;mode<10;++mode){w.mode0=1;w.mode1=0;w.mode2=uint8_t(mode);w.mode3=0;lamp.ct2=0;static_cast<RECVX_EN11_STATE*>(w.exp0)->alarm=0;static_cast<RECVX_EN11_STATE*>(w.exp0)->dir=1;
        // MV06 is wall-mounted only; type0 is not an original valid input.
        if(mode==6&&type==0)continue;
        trace.clear();auto before=models;recvx_en11(&w);require(models==before+1,"Move case failed before model publication");require(gaps.empty(),"valid Move case emitted a gap");
        if(mode==0)require(w.ct0==29&&w.mode3==1,"MV00 wait differs");
        if(mode==1||mode==4||mode==5||mode==6)require(w.ct0==98&&w.mode3==1,"scan fallthrough differs");
        if(mode==2)require(w.ct0==98&&w.mode3==1,"MV02 forward differs");
        if(mode==3)require(w.way==-0x222&&w.ct0==28&&w.mode3==1,"MV03 turn differs");
        if(mode==7)require(w.mode3==1&&sounds.back()==0x2302,"alarm sound differs");
        if(mode==8)require(w.mode1==0&&w.mode3==1&&lamp.mode0==3,"MV08 light shutdown differs");
        if(mode==9){require(w.mode2==(type?6:4)&&w.mode3==1&&w.ct0==150&&w.ct1==32,"MV09 restore differs");require(trace.size()>5&&trace[0]=="flags_set"&&trace[1]=="flags_set"&&trace[2]=="motion"&&trace[3]=="flags_clear"&&trace[4]=="flags_clear","MV09 owP/motion order differs");}
    }
    // Brain propagates detection before Move; suppressed rm flag does not
    // prevent the current scan changing to alarm mode afterwards.
    w.mode0=1;w.mode1=1;w.mode2=type?6:1;w.mode3=1;w.ct0=99;w.ct1=0;lamp.ct2=1;flags[2]=0x2000;
    recvx_en11(&w);require(!(flags[1]&1)&&w.mode2==7&&w.mode3==0,"alarm propagation ordering differs");
    // Room motion runs instead of wall collision and retains caller flags.
    trace.clear();w.mode0=2;w.mtn_room=1;w.flg|=0x1000;recvx_en11(&w);require(trace==std::vector<std::string>({"motion","model"})&&(w.flg&0x1000)&&(w.flg&0x80000),"room-motion branch differs");
    ex.node_angles_set=nullptr;recvx_en11_bind(&ex);w.mtn_room=0;w.flg|=0x80000;auto before=models;recvx_en11(&w);require(models==before&&!gaps.empty(),"missing node service manufactured a pose");
}}
}
int main(){try{_Make_SinTable();layout_and_light();all_move_modes();reset();recvx_en11_bind(nullptr);std::cout<<"EN11 component PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';reset();recvx_en11_bind(nullptr);return 1;}}
