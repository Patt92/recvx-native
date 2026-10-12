#include "player_action.h"
#include "player_stairs.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
namespace {
// A player library where every clip has 40 frames and every curve speed 0.5.
struct Host {
    std::uint32_t flg=0x919,flg2=0,stflg=0,st_flg=0,exp1=0;
    std::vector<float> curve=std::vector<float>(64,0.5f);
    std::vector<std::pair<unsigned,unsigned>> steps;std::set<std::string> gaps;int behind=0;
    RECVX_PLAYER_ACTION_IO io{};
    Host(){
        io.flg=&flg;io.flg2=&flg2;io.stflg=&stflg;io.st_flg=&st_flg;io.exp1=&exp1;io.user=this;io.hp=200;io.ply_id=0;io.wpnr_no=2;
        io.motion_frames=[](void*,std::uint32_t)->std::uint32_t{return 40;};
        io.trans_z=[](void* u,std::uint32_t,std::uint32_t* n)->const float*{auto* h=static_cast<Host*>(u);*n=std::uint32_t(h->curve.size());return h->curve.data();};
        io.footstep=[](void* u,std::uint32_t node,std::uint32_t type){static_cast<Host*>(u)->steps.push_back({node,type});};
        io.search_near_enemy_b=[](void* u,std::int32_t)->int{return static_cast<Host*>(u)->behind;};
        io.gap=[](void* u,const char* n){static_cast<Host*>(u)->gaps.insert(n);};
    }
    // One bhControlPlayer pass: pad (unless flg 0x10000), act, quick turn, bhSetMotion.
    void pass(RECVX_PLAYER_ACTION& a,unsigned pad){
        io.pad_ps=pad&~io.pad_on;io.pad_old=io.pad_on;io.pad_on=pad;
        if(!(flg&0x10000u))recvx_player_action_pad(&a,&io,nullptr);
        check(recvx_player_action_bas(&a,&io)==1,"locomotion mode2 left the module");
        recvx_player_action_turn(&a,&io);recvx_player_action_motion(&a,40);
    }
};
}
int main(){try{
    _Make_SinTable();
    // pwksub.c:144: the 16.16 result is compared with the raw frame count.
    check(recvx_player_action_frame_num(30,40,5*65536)==40,"bhGetFrameNum lost the original raw-count clamp");
    check(recvx_player_action_frame_num(30,40,0)==0,"bhGetFrameNum frame zero");
    Host h;RECVX_PLAYER_ACTION a;recvx_player_action_reset(&a,10,20,0,0,0,42);
    // First pass after bhResetPlayer: act_std's entry, idle clip of row [0][0].
    h.pass(a,0);check(a.mode2==0&&a.mode3==1&&a.mtn_no==42&&a.hokan_count==4,"act_std entry");
    check(h.flg&0x10,"bhControlPlayerPad idle does not set flg 0x10");
    // Walk from standing: frame 5, clip PlMtnAct[0][0][2] = 0, curve 0.
    h.pass(a,0x1);check(a.mode2==3&&a.mtn_no==0&&a.frm_no==6*65536,"act_wlk entry frame 5 not used");
    check(std::abs(a.pz-(20-0.5f))<1e-4f&&std::abs(a.px-10)<1e-4f,"walk speed/direction");
    // Run: PlMtnAct[0][0][3] = 4 and the frame through bhGetFrameNum.
    // act_wlk only requests the run (mode2 6); act_run's entry runs in the next pass.
    h.pass(a,0x401);check(a.mode2==6&&a.mode3==0&&a.mtn_no==0&&(h.flg&0x10000),"act_wlk run request");
    h.pass(a,0x401);check(a.mode2==6&&a.mtn_no==4,"act_run entry");
    // Releasing run while moving returns to walk with flg 0x800000, then frame from bhGetFrameNum.
    h.pass(a,0x1);check(a.mode2==3&&a.mode3==0&&(h.flg&0x800000),"run release does not mark walk continuation");
    h.pass(a,0x1);check(a.mtn_no==0&&!(h.flg&0x800000)&&a.frm_no==65536+40,"walk continuation frame not from bhGetFrameNum");
    // Standing turn right (pad 8): clip [0][0][1] = 39, 7.2 degrees, reverse clock.
    h.pass(a,0);const auto ay=a.ay;h.pass(a,0x8);
    check(a.mode2==1&&a.mtn_no==39&&a.mtn_add==-65536,"act_srt right turn");
    check(a.ay==ay-std::int32_t(182.04445f*7.2f),"standing turn rate not 7.2 degrees");
    // Injured (hp 100 -> dmlvl 1): the next walk uses row [0][1], clip 2, turn rate unchanged.
    h.pass(a,0);h.io.hp=100;h.pass(a,0);check(a.dmlvl==1,"bhCheckPlayerKegaMotion dmlvl 1");
    h.pass(a,0x1);check(a.mtn_no==2,"injured walk clip not PlMtnAct[0][1][2]");
    // Danger (hp 20): rtspd 0.8 on the standing turn.
    h.io.hp=20;h.pass(a,0);h.pass(a,0);const auto ay2=a.ay;h.pass(a,0x4);
    check(a.dmlvl==2&&a.ay==ay2+std::int32_t(182.04445f*(7.2f*0.8f)),"danger turn rate not 0.8");
    // Backward with run pressed: quick turn into act_run, yrct 8, ayp = ay + 32768.
    h.io.hp=200;h.pass(a,0);h.pass(a,0);h.pass(a,0x2);check(a.mode2==9&&a.mtn_no==16,"act_bak clip");
    const auto before=a.ay;h.steps.clear();h.pass(a,0x402);
    check(a.mode2==6&&a.mode3==1&&a.mtn_no==4&&(h.flg&0x4110000)==0x4110000,"quick turn entry");
    check(!h.steps.empty()&&h.steps.front()==std::make_pair(17u,1u),"quick turn footstep");
    for(int i=0;i<8;++i)h.pass(a,0x401);
    check(((a.ay-before)&0xFFFF)==32768&&!(h.flg&0x4100000),"quick turn did not end at ay + 32768");
    // Idle timer: after 300 passes at dmlvl 0 the player enters act_sta.
    RECVX_PLAYER_ACTION idle;recvx_player_action_reset(&idle,0,0,0,0,0,42);Host i;
    for(int t=0;t<302;++t)i.pass(idle,0);check(idle.mode2==13&&idle.mtn_no==9&&(i.flg&0x10000),"act_std idle timer did not enter act_sta");
    i.pass(idle,0x1);check(!(i.flg&0x10000),"pad does not release act_sta");
    // A two-handed weapon row (wpntp 1) is fixed at bhSetPlayer.
    RECVX_PLAYER_ACTION gun;recvx_player_action_reset(&gun,0,0,0,1,0,42);Host g;g.pass(gun,0);g.pass(gun,0x1);
    check(gun.mtn_no==1,"wpntp 1 walk clip not PlMtnAct[1][0][2]");
    check(h.gaps.empty()&&i.gaps.empty()&&g.gaps.empty(),"bound services reported a gap");
    std::cout<<"player action ok\n";return 0;
}catch(const std::exception& e){std::cerr<<"player action test failed: "<<e.what()<<"\n";return 1;}}
