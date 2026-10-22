/* EN71 separat; Erwartungen aus en71.c:23-116. Diagnose-Dienstfixture,
   keine Behauptung zum produktiven mdl[0..3]-Zeichenpfad. Die Modellwahl
   (en71.c:41 mlwP = &mdl[mode3]) geht ueber select_model; mdl_no (Skin-Index)
   schreibt EN71 nie. */
#include "en71.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
std::vector<void*> blocks;std::vector<std::string> trace,gaps;unsigned selected=0,mlw=0,motions=0;bool sampled=false;
void gap(const char* s){gaps.emplace_back(s);}
void* allocate(uint32_t n){require(n==4,"EN71 exp allocation differs");auto p=std::calloc(1,n);blocks.push_back(p);return p;}
int select(RECVX_ENEMY_WORK*,uint32_t k){trace.emplace_back("select");mlw=k;return 1;}
int motion(RECVX_ENEMY_WORK* w,int add,uint32_t){trace.emplace_back("motion");selected=mlw;sampled=true;require(w->mdl_no==0,"EN71 wrote the skin index mdl_no");++motions;require(add==65536,"EN71 motion increment differs");w->frm_no+=add;return 0;}
void model(RECVX_ENEMY_WORK*){trace.emplace_back("model");require(!sampled||mlw==selected,"EN71 changed mdl after sampling");sampled=false;}
void event(RECVX_ENEMY_WORK*){trace.emplace_back("event");}
RECVX_ENEMY_SERVICES services(){RECVX_ENEMY_SERVICES s{};s.gap=gap;s.calloc_work=allocate;s.set_motion=motion;s.calc_model=model;s.event=event;s.select_model=select;return s;}
void reset(){for(auto p:blocks)std::free(p);blocks.clear();trace.clear();gaps.clear();motions=0;}
void test(){auto s=services();
    for(unsigned type=0;type<=6;++type){
        reset();RECVX_ENEMY_WORK w{};w.id=71;w.type=uint16_t(type);w.sv=&s;w.flg=7;w.ax=123;
        recvx_en71(&w);
        require(w.flg==0x19&&w.mode0==1&&w.mode1==0&&w.mode2==1&&w.mode3==0,"EN71 init/first cycle differs");
        require(w.mtn_no==(type==1||type==3||type==5?1u:0u),"EN71 type motion differs");
        require(w.ax==(type==2||type==3?16384:type==4||type==5?8192:123),"EN71 type angles differ");
        require(w.hokan_count==0&&w.hokan_rate==65536&&w.frm_no==65536,"EN71 init clock differs");
        require(trace==std::vector<std::string>({"select","motion","model"}),"EN71 outer order differs");
        for(unsigned tick=2;tick<=24;++tick){recvx_en71(&w);require(mlw==(tick/3)%4,"EN71 four-model cycle differs");require(w.frm_no==int32_t(tick*65536),"EN71 clock stopped during model selection");}
        require(gaps.empty(),"EN71 fixture reports a gap");
        for(unsigned mode=2;mode<=4;++mode){w.mode0=uint8_t(mode);auto before=motions;recvx_en71(&w);require(w.mode0==mode&&motions==before+1,"original empty mode skipped outer pass");}
        trace.clear();w.mode0=5;recvx_en71(&w);require(trace==std::vector<std::string>({"event","select","motion","model"}),"EN71 event/clock order differs");
        // Reinit retains the exp allocation but resets the model cycle.
        auto exp=w.exp0;w.mode0=0;recvx_en71(&w);require(w.exp0==exp&&mlw==0&&w.mdl_no==0&&w.mode2==1,"EN71 reinit replaced owner or retained cycle");
        s.select_model=nullptr;recvx_en71(&w);require(!gaps.empty()&&gaps.back()=="bhEne71: mlwP = &mdl[mode3]","missing model selection not reported");s.select_model=select;gaps.clear();
        s.set_motion=nullptr;recvx_en71(&w);require(!gaps.empty()&&gaps.back()=="bhSetMotion","missing motion provider not reported");s.set_motion=motion;
    }
}
}
int main(){try{test();reset();std::cout<<"EN71 component PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';reset();return 1;}}
