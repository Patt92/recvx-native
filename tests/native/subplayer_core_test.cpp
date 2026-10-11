/* C05 — direkte Work-, Tabellen-, Motion- und Freigabevertraege.
   Testdienste sind Diagnosefixtures, keine produktiven Ersatzdienste.
   Positive Motionfaelle linken den vorhandenen nativen motion_set.cpp.
   Semantik: subpl.c:12..410; eneset.c:170..388; ELF-Tabellenadressen im Header. */
#include "subplayer.h"
#include "motion_set.h"
#include "ninja_math.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <climits>

static void check(bool yes,const char* reason){if(!yes)throw std::runtime_error(reason);}
struct Host {
    RECVX_ENEMY_WORK work{};
    RECVX_ENEMY_SERVICES services{};
    std::vector<std::string> gaps,order;
    unsigned samples=0,models=0;
    int last_add=0;
    uint32_t last_mode=0;
    const void* last_dat=nullptr;
    std::vector<int32_t> sampled_frames;
    bool real_motion=false,sideeffects=false;
    recvx::MotionClip clip;
    std::vector<recvx::ModelPose> pose;
    std::vector<uint32_t> owp;
    static Host* active;
    Host(){
        active=this;work.id=91;work.sv=&services;
        services.gap=[](const char* n){active->gaps.emplace_back(n);};
        services.set_motion=[](RECVX_ENEMY_WORK* w,int add,uint32_t mode)->int {
            auto& h=*active;check(w==&h.work,"Motion must receive sole authoritative work");
            ++h.samples;h.order.emplace_back("motion");h.sampled_frames.push_back(w->frm_no);h.last_add=add;h.last_mode=mode;h.last_dat=w->mtn_tp;
            if(h.real_motion){
                recvx::MotionWork m{w->frm_no,w->frm_mode,w->hokan_count,w->mtn_attr,w->hokan_rate};std::string gap;
                int r=recvx::set_motion(m,add,mode,&h.clip,static_cast<const int8_t*>(w->mtn_tp),h.pose,h.owp,&gap);
                w->frm_no=m.frm_no;w->frm_mode=m.frm_mode;w->hokan_count=m.hokan_count;w->mtn_attr=m.mtn_attr;
                if(!gap.empty())h.gaps.push_back(gap);
                return r;
            }
            if(h.sideeffects){
                w->hokan_count=3;w->hokan_rate=47;w->mtn_md=0x77;w->mtn_attr=0x87654321;
                w->frm_no=49152;w->frm_mode=2;w->px=12;w->pz=18;w->ay=0;w->spd=4;
                w->flg|=0x20000000;w->ct1=713;
            }
            return 0;
        };
        services.calc_model=[](RECVX_ENEMY_WORK* w){
            auto& h=*active;check(w==&h.work,"Model must receive authoritative work");++h.models;h.order.emplace_back("model");
            if(h.sideeffects){
                check(w->hokan_count==3&&w->hokan_rate==47&&w->mtn_md==0x77,"Motion effects precede model");
                check(w->frm_no==49152&&w->mtn_attr==0x87654321&&w->ct1==713,"Frame/attr/counter effects retained");
                check(w->px==12&&std::fabs(w->pz-14)<0.001f,"Motion then movement then model, same work");
                check((w->flg&0x8100)==0x8100&&(w->flg&0x20000000),"Model sees pass and motion flags");
                w->stflg|=0x200000;w->mode2=19;w->py=57;
            }
        };
    }
    void moving(unsigned mode,unsigned branch){
        work.mode0=uint8_t(mode);work.mode1=7;work.mode3=0;work.ct2=int(branch);work.ct3=250;
        work.pz=10;work.mtn_add=32768;work.mtn_md=0x120;work.frm_mode=99;
        work.hokan_count=99;work.hokan_rate=77;work.spd=-1;work.mtn_tp=this;
    }
};
Host* Host::active=nullptr;

enum Branch {Idle,Walk,Hold,Glide,Full,Blend,IdleCut,IdleEx,HoldCut,GlideCut,HoldEx};
static const Branch move_table[15]={Idle,Walk,Hold,Glide,Full,Blend,IdleCut,Idle,Idle,Walk,IdleEx,Idle,Idle,Idle,Idle};
static const Branch script_table[15]={Hold,Glide,Hold,Glide,Full,Blend,HoldCut,GlideCut,Idle,Walk,HoldEx,Idle,Idle,Idle,Idle};

void branches(){
    for(unsigned mode:{1u,5u,6u})for(unsigned at=0;at<15;++at){
        Host h;h.moving(mode,at);auto& w=h.work;const auto b=(mode==1?move_table:script_table)[at];
        check(recvx_subplayer_tick(&w)==1,"Every valid table row runs");
        check(w.mode3==1&&w.mtn_no==7&&w.frm_mode==0,"Branch entry fields");
        const bool sample=b==Idle||b==Walk||b==Full||b==Blend||b==IdleCut||b==IdleEx;
        const bool moved=b==Walk||b==Glide||b==GlideCut;
        check(h.samples==(sample?1u:0u)&&h.models==1,"Correct sample and model calls");
        check((w.px!=0||w.pz!=10)==moved,"Correct movement branches");
        const unsigned count=b==IdleEx||b==HoldEx?99:b==Full||b==IdleCut||b==HoldCut||b==GlideCut?0:b==Blend?16:8;
        check(w.hokan_count==count,"Hokan count differs by branch");
        check(w.hokan_rate==((b==Idle||b==IdleEx||b==Walk)?0:77),"Selective Hokan rate reset");
        check(w.mtn_md==((b==Idle||b==IdleEx)?0x20:0x120),"Selective mode-bit clearing");
        check(w.mtn_add==((b==Full||b==Blend)?65536:32768),"Full versus preserved step");
        check((w.mtn_tp==nullptr)==(b==Walk||b==Full||b==Blend),"Only indicated entries clear datP");
        check(w.spd==(moved?2.5f:b==Hold||b==HoldCut||b==HoldEx?0.f:-1.f),"Selective speed writes");
        check(h.gaps.empty(),"Bound original table rows have no gaps");
        h.samples=h.models=0;w.mode3=3;float x=w.px,z=w.pz;int frame=w.frm_no;
        recvx_subplayer_tick(&w);check(h.samples==0&&h.models==1&&w.px==x&&w.pz==z&&w.frm_no==frame,"Unmatched mode3 keeps pose yet calculates model");
    }
}

void initialization(){
    for(unsigned id=40;id<160;++id){
        Host h;auto& w=h.work;w.id=uint16_t(id);w.mode0=0;w.mode1=51;w.mode2=0;
        w.mode3=4;w.ct0=1;w.ct1=2;w.ct2=3;w.ct3=4;w.mtn_tp=&h;w.flg=0x80000003;
        check(recvx_subplayer_tick(&w)==1,"All 120 init entries run");
        check(w.mode0==1&&w.mode1==51&&w.mode3==3,"Init does not reset unrelated modes");
        check(w.ct0==1&&w.ct1==2&&w.ct2==3&&w.ct3==4,"Init preserves counters");
        check(w.hokan_rate==0&&w.hokan_count==0&&w.mtn_no==0&&w.mtn_md==0x20&&w.mtn_add==32768,"Init motion tuple");
        check(w.frm_no==0&&w.frm_mode==0&&w.mtn_tp==nullptr,"Init frame/datP tuple");
        check(w.flg==0x80008101&&h.samples==1&&h.models==1,"Flags before init, model after init");
    }
    for(unsigned id:{33u,37u,38u}){
        Host h;h.moving(0,0);h.work.id=uint16_t(id);
        check(recvx_subplayer_tick(&h.work)==1,"ELF confirmed neighbor alias runs");
        check(h.work.mode0==1&&h.work.mtn_no==7,"Alias changes mode0 before branch");
        check(h.samples==(id==33?1u:0u)&&h.models==1,"Correct alias table");
    }
    for(unsigned id:{31u,32u,34u,35u,36u,39u,160u,199u}){
        Host h;h.work.id=uint16_t(id);check(recvx_subplayer_tick(&h.work)==0,"Null/recursive/out-of-range init not invented");
        check(!h.gaps.empty()&&h.samples==0&&h.models==0,"Invalid original dispatch is named");
    }
    Host h;h.work.id=199;h.real_motion=true;h.pose.resize(1);h.owp.resize(1);
    h.clip.frames=4;h.clip.flags=0x10;h.clip.positions={{{0,0,0},{1,0,0},{2,0,0},{3,0,0}}};h.clip.angles={{{0,0,0},{0,0,0},{0,0,0},{0,0,0}}};
    RECVX_SUBPLAYER_MODEL_SERVICES model{};
    model.sample_window=[](RECVX_ENEMY_WORK* w,uint32_t skip,int add,uint32_t mode){
        auto& h=*Host::active;check(skip==8&&w->frm_no==0&&add==32768&&mode==0x20,"ID199 exact window request");
        h.order.emplace_back("window+8");(void)h.services.set_motion(w,add,mode);h.order.emplace_back("restore");
    };
    recvx_subplayer_initialize_motion(&h.work,&model);
    check(h.samples==2&&h.models==0&&h.work.frm_no==65536,"ID199 both real sample advances retained");
    check(h.order==std::vector<std::string>{"window+8","motion","restore","motion"},"Window restoration precedes full sample");
    check(h.sampled_frames==std::vector<int32_t>{0,32768},"Second ID199 sample uses advanced half frame");
    h.order.clear();h.gaps.clear();h.samples=0;
    recvx_subplayer_initialize_motion(&h.work,nullptr);
    check(h.samples==1&&h.gaps.size()==1&&h.gaps[0]=="em60_init:mlwP-window-8","Missing window is explicitly a gap");
}

void authoritative_sideeffects(){
    Host h;h.moving(1,1);h.sideeffects=true;
    recvx_subplayer_enemy(&h.work);
    check(h.order==std::vector<std::string>{"motion","model"},"Motion precedes model");
    check(h.work.stflg==0x200000&&h.work.mode2==19&&h.work.py==57,"Model sideeffects not overwritten by a shadow copy");
    Host e;e.moving(5,8);e.work.flg=3;e.work.mtn_tp=&e;e.work.mtn_md=2;
    recvx_subplayer_enemy_event(&e.work);
    check(e.samples==1&&e.models==0&&e.work.flg==3&&e.last_dat==&e,"Event preserves caller flags, forwards datP and does not calculate model");
    Host f;f.moving(1,0);recvx_subplayer_enemy_event2(&f.work);
    check(f.samples==1&&f.models==0,"Event2 owns only movement table");
    Host empty;const auto before=empty.work;recvx_subplayer_enemy28(&empty.work);
    check(std::memcmp(&before,&empty.work,sizeof before)==0&&empty.order.empty(),"Original ID28 really empty");
}

void real_motion(){
    Host h;h.real_motion=true;h.pose.resize(1);h.owp.resize(1);
    h.clip.frames=3;h.clip.flags=0x10;h.clip.positions={{{10,0,0},{20,0,0},{30,0,0}}};h.clip.angles={{{0,0,0},{0,0,0},{0,0,0}}};h.clip.attributes={11,22,33};
    h.work.mode0=0;recvx_subplayer_enemy(&h.work);
    check(h.work.frm_no==32768&&h.pose[0].position.x==10&&h.work.mtn_attr==11,"Init samples zero before half-step");
    // Common/Sub control supply mode0/ct2/mode3; no host-owned frame clock.
    h.work.mode0=1;h.work.mode1=0;h.work.ct2=10;h.work.mode3=0;h.work.hokan_count=0;h.work.mtn_md=0x28;
    recvx_subplayer_enemy(&h.work);
    check(h.work.frm_no==65536&&h.pose[0].position.x==15,"Subplayer motion samples half before next advance");
    h.work.ct2=0;h.work.mode3=0;h.pose[0].position.x=100;
    recvx_subplayer_enemy(&h.work);
    check(h.work.hokan_count==7&&std::fabs(h.pose[0].position.x-90)<0.0001f,"Real Hokan decrement and interpolation retained");
    h.work.hokan_count=0;h.work.frm_no=2*65536;h.work.mtn_add=65536;h.work.mode3=1;
    recvx_subplayer_enemy(&h.work);
    check(h.pose[0].position.x==30&&h.work.frm_no==0&&h.work.mtn_attr==33,"Wrap sample and preadvance attribute");
    h.work.mtn_add=-32768;recvx_subplayer_enemy(&h.work);
    check(h.pose[0].position.x==10&&h.work.frm_no==163840,"Backward wrap retained");
    h.work.mode0=5;h.work.ct2=0;h.work.mode3=0;auto frame=h.work.frm_no;auto samples=h.samples;
    recvx_subplayer_enemy(&h.work);check(h.work.frm_no==frame&&h.samples==samples,"Hold branch never advances behind the native owner");
    check(h.gaps.empty(),"Real native motion has no missing service");
}

void errors_and_angles(){
    for(unsigned mode:{2u,3u,4u,7u,255u}){Host h;h.moving(mode,0);check(recvx_subplayer_tick(&h.work)==0&&h.models==0&&!h.gaps.empty(),"Null/bad mode diagnosed");}
    for(int ct:{-1,15,INT_MAX})for(unsigned mode:{1u,5u,6u}){Host h;h.moving(mode,0);h.work.ct2=ct;check(!recvx_subplayer_tick(&h.work)&&h.models==0&&!h.gaps.empty(),"Invalid ct2 does not call unrelated function");}
    Host h;h.moving(1,0);h.services.set_motion=nullptr;h.services.calc_model=nullptr;
    recvx_subplayer_enemy(&h.work);check(h.gaps==std::vector<std::string>{"bhSetMotion:subplayer","bhCalcModel:subplayer"}&&h.work.frm_no==0,"Every missing service named, no fake frame");
    check(recvx_subplayer_dir_target(0,0,0,10,0,182)==-182,"Negative turn limit");
    check(recvx_subplayer_dir_target(0,0,0,-10,0,182)==182,"Positive turn limit");
    check(recvx_subplayer_dir_target(0,0,0,10,0,32767)==-16384,"Quarter turn");
    Host a;a.moving(1,1);a.work.ct0=0xffff;a.work.ct3=0;a.work.pz=0;recvx_subplayer_enemy(&a.work);int ang=a.work.ay;
    Host b;b.moving(5,1);b.work.ct0=0xffff;b.work.ct3=0;b.work.pz=0;recvx_subplayer_enemy(&b.work);
    check((ang==182||ang==-182)&&b.work.ay==0,"Unsigned/signed packed Z differs as original");
    Host c;c.moving(5,1);c.work.mode3=1;c.work.ay=INT_MAX;c.work.ct3=100;recvx_subplayer_enemy(&c.work);check(c.gaps.empty(),"32bit angle wrap does not invoke signed overflow");
}

void lifetime(){
    Host h;RECVX_ENEMY_TABLE table{};recvx_enemy_table_init(&table);
    RECVX_ENEMY_TEMPLATE t{};t.flg=1;t.id=91;t.px=3;t.py=8;t.pz=10;t.ay=1000;
    h.services.grand=[](uint32_t at)->float{return at==2?8.f:0.f;};
    check(recvx_enemy_set(&table,&h.work,&t,&h,7,&h.services)==&h.work,"Subplayer allocated on supplied owner work");
    check(table.ewk_n==1&&table.slot[0]==&h.work&&h.work.owner==&h,"No private subplayer allocation");
    check(recvx_enemy_pass_enabled(&h.work),"Active work enabled");
    recvx_subplayer_enemy(&h.work);check(h.work.mode0==1&&h.work.mode3==3,"Spawn and first original control init");
    h.work.stflg=0x1000000;check(!recvx_enemy_pass_enabled(&h.work),"Event deactivate respected by outer owner");
    h.work.stflg=0;h.work.flg&=~1u;recvx_enemy_check_work_num(&table);
    check(table.ewk_n==0&&!recvx_enemy_pass_enabled(&h.work),"Release uses original active flag and highwater");
    check(recvx_enemy_set(&table,&h.work,&t,&h,9,&h.services)==&h.work,"Reentry same owner slot");
    recvx_subplayer_enemy(&h.work);check(h.work.owner_index==9&&h.work.mode0==1&&h.work.mode3==3,"Reentry does not retain old state");
}

int main(){try{_Make_SinTable();branches();initialization();authoritative_sideeffects();real_motion();errors_and_angles();lifetime();std::cout<<"C05: all direct-work/45 branch rows/120 init rows/motion/lifetime checks PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"C05 FAIL: "<<e.what()<<'\n';return 1;}}
