/* EN20 Ameise B einzeln; Originalsemantik en20.c:33-423.
   Dienstfixtures sind diagnostisch, keine Runtime-Abnahme. */
#include "en20.h"
#include "core_random.h"
#include "ninja_math.h"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
std::vector<void*> blocks;
std::vector<std::string> trace,gaps;
std::vector<int32_t> randoms;
unsigned draws=0,motions=0,models=0;
void gap(const char* s){gaps.emplace_back(s);}
void* alloc(uint32_t n){require(n==0x30,"EXP allocation size differs");auto p=std::calloc(1,n);blocks.push_back(p);return p;}
int32_t random_next(){require(draws<randoms.size(),"unexpected RNG draw");return randoms[draws++];}
uint32_t frames(const RECVX_ENEMY_WORK*,uint32_t n){switch(n){case 1:return 7;case 2:return 5;case 3:return 3;case 4:return 4;case 5:return 8;case 6:return 9;default:throw std::runtime_error("unexpected frame table index");}}
int motion(RECVX_ENEMY_WORK*,int add,uint32_t){require(add==65536,"motion advance differs");++motions;trace.emplace_back("motion");return 0;}
void model(RECVX_ENEMY_WORK*){++models;trace.emplace_back("model");}
void event(RECVX_ENEMY_WORK* w){trace.emplace_back("event");w->px=24;}
RECVX_ENEMY_SERVICES services(){RECVX_ENEMY_SERVICES s{};s.gap=gap;s.calloc_work=alloc;s.random_next=random_next;s.motion_frames=frames;s.set_motion=motion;s.calc_model=model;s.event=event;return s;}
void reset(){for(auto p:blocks)std::free(p);blocks.clear();trace.clear();gaps.clear();randoms.clear();draws=motions=models=0;}
RECVX_ENEMY_WORK init(RECVX_ENEMY_SERVICES& s,unsigned type){RECVX_ENEMY_WORK w{};w.id=20;w.type=uint16_t(type);w.flg=7;w.sv=&s;w.px=71;w.pz=70;w.ax=123;w.ay=456;recvx_en20(&w);return w;}
void initialization(){reset();auto s=services();auto w=init(s,0);require(w.flg==0x8001&&w.hp==100&&w.ax==16384&&w.ay==int32_t(10430.381f*atan2f(1.f,0.f)),"type0 initialization differs");require(w.ar==0&&w.ah==0&&w.mtn_no==0&&w.hokan_count==0&&w.hokan_rate==65536&&w.mtn_add==65536&&w.mode0==1&&w.mode2==0,"initial modes/body differ");require(draws==0&&trace==std::vector<std::string>({"motion","model"}),"Init consumed RNG or changed order");auto q=init(s,1);require(q.ax==123&&q.ay==456,"nonzero type acquired type0 angles");}
void random_branches(){reset();auto s=services();auto w=init(s,1);
    // MV00: timer is consumed immediately by the original fallthrough.
    randoms={9};recvx_en20(&w);require(w.ct0==18&&w.mode3==1&&draws==1,"MV00 postdecrement differs");
    for(unsigned r=0;r<6;++r){w.mode2=0;w.mode3=1;w.ct0=0;randoms.push_back(int(r));recvx_en20(&w);require(w.mode2==(r==0?6:r==1?7:0)&&w.mode3==0&&w.ct0==-1,"MV00 random dispatch differs");}
    for(unsigned r=0;r<10;++r){w.mode2=1;w.mode3=1;w.ct0=0;w.spd=0;randoms.push_back(int(r));recvx_en20(&w);require(w.mode2==(r<2?0:r==2?3:r==3?2:1)&&w.mode3==0,"MV01 random dispatch differs");}
    for(unsigned mode=1;mode<=3;++mode){w.mode2=uint8_t(mode);w.mode3=0;w.mtn_no=0;randoms.push_back(2);auto x=w.px,z=w.pz;recvx_en20(&w);require(w.mtn_no==7&&w.ct0==11&&w.hokan_count==3&&w.spd==0&&w.px==x&&w.pz==z,"MV01..03 initialization differs");w.ct0=0;if(mode==1)randoms.push_back(4);recvx_en20(&w);require(w.mode2==1&&w.mode3==0,"MV02/03 return differs");}
}
void frame_sequences(){reset();auto s=services();auto w=init(s,1);
    for(unsigned mode=4;mode<=5;++mode){w.mode2=uint8_t(mode);w.mode3=0;w.mtn_no=0;recvx_en20(&w);require(w.mtn_no==mode+1&&w.ct0==int(mode+3)&&w.spd==0&&w.mode3==1,"MV04/05 duration differs");w.ct0=0;recvx_en20(&w);require(w.mode2==1&&w.mode3==0,"MV04/05 return differs");}
    w.mode2=6;w.mode3=0;w.mtn_no=0;recvx_en20(&w);require(w.mtn_no==3&&w.ct0==2&&w.mode3==1,"MV06 phase1 differs");w.ct0=0;recvx_en20(&w);require(w.mtn_no==4&&w.ct0==8&&w.mode3==2,"MV06 must use twice motion4 length without same-pass decrement");for(unsigned i=0;i<8;++i)recvx_en20(&w);require(w.ct0==0&&w.mode3==2,"MV06 phase2 ended early");recvx_en20(&w);require(w.mode2==0&&w.mode3==0,"MV06 return differs");
    w.mode2=7;w.mode3=0;w.mtn_no=0;recvx_en20(&w);require(w.mtn_no==2&&w.ct0==4,"MV07 phase1 differs");w.ct0=0;recvx_en20(&w);require(w.mtn_no==1&&w.ct0==7&&w.mode3==2,"MV07 phase2 differs");w.ct0=0;recvx_en20(&w);require(w.mode2==0&&w.mode3==0,"MV07 return differs");
    for(unsigned mode=2;mode<=4;++mode){w.mode0=uint8_t(mode);auto n=models;recvx_en20(&w);require(w.mode0==mode&&models==n+1,"empty original state dropped outer model pass");}
    trace.clear();w.mode0=5;recvx_en20(&w);require(trace==std::vector<std::string>({"event","motion","model"})&&w.px==24,"event dispatch order differs");
    w.mode0=1;w.mode2=8;recvx_en20(&w);require(!gaps.empty(),"bad dispatch index not reported");
}
void shared_random(){reset();auto s=services();s.random_next=nullptr;recvx_core_random_seed(1);auto w=init(s,1);recvx_en20(&w);require(w.ct0==9,"shared EE random modulo differs");require(recvx_core_random_next()==377401575,"shared EE draw count differs");require(gaps.empty(),"shared EE provider falsely missing");}
}
int main(){try{_Make_SinTable();initialization();random_branches();frame_sequences();shared_random();reset();std::cout<<"EN20 component PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';reset();return 1;}}
