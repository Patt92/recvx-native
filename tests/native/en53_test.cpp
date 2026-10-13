/* EN53 separat: Originalreferenz en15.c:3350-3745. Diagnosefixtures fuer
   Material/Modell/eff-Dienste, keine produktive Raum-Abnahme. */
#include "en53.h"
#include "en03_lib.h"
#include "ninja_math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::vector<std::string> gaps;
std::vector<std::pair<unsigned,unsigned>> pairs;
std::vector<std::pair<unsigned,unsigned>> colors;
std::vector<RECVX_T3_EFF> effects;
std::vector<RECVX_T3_EFFPATCH> patches;
unsigned source_a=0,source_b=1,root=99,rng_calls=0,particles=0,se_slot=999;
int se_number=0;RECVX_ENEMY_WORK* owner=nullptr;
void gap(const char* s){gaps.emplace_back(s);}
int ambient(RECVX_ENEMY_WORK*,uint32_t mdl,uint8_t n){colors.emplace_back(mdl,n);return 1;}
int setroot(RECVX_ENEMY_WORK*,uint32_t n){root=n;return 1;}
int pair(RECVX_ENEMY_WORK*,uint32_t a,uint32_t b){source_a=a;source_b=b;pairs.emplace_back(a,b);return 1;}
int indices(const RECVX_ENEMY_WORK*,uint32_t* a,uint32_t* b){*a=source_a;*b=source_b;return 1;}
const void* texture(const RECVX_ENEMY_WORK*){return &source_a;}
int32_t random_next(){++rng_calls;return 0;}
int matrix(const RECVX_ENEMY_WORK*,uint32_t n,float* m){require(n==1,"EN53 blood wrong joint");std::fill(m,m+16,0.f);m[0]=m[5]=m[10]=m[15]=1;m[12]=2;m[13]=3;m[14]=4;return 1;}
uint32_t count(){return 3;}
RECVX_ENEMY_WORK* at(uint32_t n){return n==2?owner:nullptr;}
void se(uint32_t n,const RECVX_ENEMY_VEC*,int code){se_slot=n;se_number=code;}
int effect(const RECVX_T3_EFF* e,const RECVX_ENEMY_VEC*,RECVX_T3_LINK,int){effects.push_back(*e);return int(effects.size()-1);}
void patch(int n,const RECVX_T3_EFFPATCH* p){require(n>=0&&size_t(n)<effects.size(),"wrong debris slot");patches.push_back(*p);}
int particle(RECVX_T3_LINK lk,int n,const RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC*,uint32_t,uint32_t,int type){require(lk.w==owner&&n==1&&(type==0||type==2),"wrong blood linkage");++particles;return 0;}
RECVX_EN53_SERVICES extras(){RECVX_EN53_SERVICES s{};s.child_ambient=ambient;s.model_root=setroot;s.morph_pair=pair;s.morph_indices=indices;s.work_texture=texture;return s;}
RECVX_ENEMY_SERVICES services(){RECVX_ENEMY_SERVICES s{};s.gap=gap;s.random_next=random_next;s.joint_matrix=matrix;s.enemy_count=count;s.enemy_at=at;s.enemy_se=se;return s;}
void reset(){gaps.clear();pairs.clear();colors.clear();effects.clear();patches.clear();source_a=0;source_b=1;root=99;rng_calls=particles=0;se_slot=999;se_number=0;}
void stream(){
    // tags 18/19/22/23: only the single designated material byte changes.
    for(unsigned tag:{18u,19u,22u,23u}){
        std::array<uint8_t,18> b{};b.fill(0x61);b[0]=uint8_t(tag);b[1]=0;b[2]=6;b[3]=0;b[16]=0xff;b[17]=0;
        auto before=b;require(recvx_en53_change_ambient(b.data(),b.size(),178),"valid material stream rejected");
        unsigned off=(tag==18||tag==22)?5:9;before[off]=178;require(b==before,"ChangeAmbient altered another byte");
    }
    uint8_t short_stream[]={0,0,8,0,7,8,0xff,0};require(recvx_en53_change_ambient(short_stream,sizeof short_stream,178),"short-word transport differs");
    uint8_t bad[]={18,0,6,0};require(!recvx_en53_change_ambient(bad,sizeof bad,178),"truncated chunk accepted");
}
void cycle(){reset();auto sv=services();auto ex=extras();recvx_en53_bind(&ex);RECVX_ENEMY_WORK w{};owner=&w;w.id=53;w.flg=0x17f;w.sv=&sv;recvx_en53(&w);
    require(w.mode0==1&&w.mode1==0&&w.shp_ct==400&&root==0,"CoreInit did not fall through to CoreMove");
    require(w.flg==1&&w.mdflg==6&&w.mtn_add==0&&colors==std::vector<std::pair<unsigned,unsigned>>({{0,178},{1,178},{2,178}}),"CoreInit fields/materials differ");
    require(pairs==std::vector<std::pair<unsigned,unsigned>>({{0,1}}),"Init morph pair differs");
    recvx_en53(&w);recvx_en53(&w);require(w.shp_ct==1200&&w.mode1==0,"shape threshold consumed early");recvx_en53(&w);require(w.mode1==1&&w.shp_ct==1200&&source_a==1&&source_b==2,"phase0->1 incorrectly reset shape");
    recvx_en53(&w);require(w.mode1==2&&w.ct0==0&&w.shp_ct==1200,"phase1->2 differs");for(int i=0;i<3;++i)recvx_en53(&w);require(w.mode1==2&&w.ct0==3,"wait postincrement differs");recvx_en53(&w);require(w.mode1==3&&w.shp_ct==0&&source_a==2&&source_b==1,"phase2->3 differs");
    for(int i=0;i<6;++i)recvx_en53(&w);require(w.mode1==3&&w.shp_ct>1000,"phase3 increments differ");recvx_en53(&w);require(w.mode1==4&&w.shp_ct==0&&source_a==1&&source_b==0,"phase3->4 differs");for(int i=0;i<7;++i)recvx_en53(&w);require(w.shp_ct==1050&&w.mode1==4,"phase4 threshold differs");recvx_en53(&w);require(w.mode1==0&&w.shp_ct==0&&source_a==0&&source_b==1,"pulse cycle did not return");
    w.mdflg|=1;recvx_en53(&w);require(w.shp_ct==0,"hidden core still pulses");
    recvx_en53_bind(nullptr);w={};w.id=53;w.sv=&sv;recvx_en53(&w);require(w.mode0==0&&!gaps.empty(),"missing model provider fabricated a running core");
}
void death(){reset();auto sv=services();auto ex=extras();recvx_en53_bind(&ex);RECVX_ENEMY_EXT ext{};ext.gap=gap;ext.effect_tb=effect;ext.effect_patch=patch;ext.effect_particle_mk2=particle;recvx_enemy_ext_bind(&ext);
    RECVX_ENEMY_WORK w{};owner=&w;w.id=53;w.sv=&sv;w.mdflg=2;w.mode0=3;w.mode2=1;
    recvx_en53(&w);require(w.ct1==0&&w.mode3==1&&w.shp_ct==200,"dying pulse divisor/initial pass differs");
    unsigned guard=0;while(w.mode1!=5&&guard++<2000)recvx_en53(&w);require(w.mode1==5&&w.ct1==5,"dying pulse did not stop after fifth cycle");auto shape=w.shp_ct;recvx_en53(&w);require(w.shp_ct==shape,"dying state5 should be original no-op");
    w.mode2=2;w.mode3=0;w.mode1=0;w.shp_ct=0;colors.clear();recvx_en53(&w);require(w.ct2==174&&colors.size()==2&&colors[0].second==178&&w.shp_ct==400,"fade patch/decrement order differs");
    guard=0;while(w.mode1!=5&&guard++<100)recvx_en53(&w);require(w.ct2==0&&w.mode1==5&&effects.empty(),"fade did not reach delayed debris state");recvx_en53(&w);
    require(w.mode1==6&&w.id==53&&(w.mdflg&1)&&w.djnt_no==1,"death final fields differ");
    require(effects.size()==38&&patches.size()==32&&particles==6&&rng_calls==48,"death particle/debris/RNG count differs");
    require(effects[0].id==250&&effects[0].sx==0.1f&&effects[0].sy==0.1f&&effects[0].sz==0&&effects[0].mdlver==0,"debris template differs");
    require(patches[0].tex_id==1&&patches[0].texp==&source_a&&patches[0].stflg_or==0x20,"debris model texture provenance differs");
    require(se_slot==2&&uint32_t(se_number)==0x8000020au,"GetLocalEneNo/SE differs");require(gaps.empty(),"complete death fixture contains a gap");
    auto n=effects.size();recvx_en53(&w);require(effects.size()==n,"hidden dead core repeated debris");
    recvx_enemy_ext_bind(nullptr);
}
}
int main(){try{_Make_SinTable();stream();cycle();death();recvx_en53_bind(nullptr);std::cout<<"EN53 component PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';recvx_enemy_ext_bind(nullptr);recvx_en53_bind(nullptr);return 1;}}
