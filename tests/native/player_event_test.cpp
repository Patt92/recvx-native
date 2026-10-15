#include "player_event.h"
#include "player_stairs.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
namespace {
// Eine Bibliothek, in der jede Motion 40 Bilder hat (pl_evt.c liest mnwP[mtn].frm_num).
struct Host {
    std::uint32_t flg=0x919,flg2=0,stflg=0,st_flg=0,exp1=0;
    std::vector<std::pair<unsigned,unsigned>> steps,floor_steps;std::set<std::string> gaps;
    RECVX_PLAYER_ACTION_IO io{};RECVX_PLAYER_EVENT_IO ev{};
    Host(){
        io.flg=&flg;io.flg2=&flg2;io.stflg=&stflg;io.st_flg=&st_flg;io.exp1=&exp1;io.user=this;io.hp=200;io.ply_id=0;io.wpnr_no=0;
        io.motion_frames=[](void*,std::uint32_t)->std::uint32_t{return 40;};
        io.footstep=[](void* u,std::uint32_t node,std::uint32_t type){static_cast<Host*>(u)->steps.push_back({node,type});};
        io.gap=[](void* u,const char* n){static_cast<Host*>(u)->gaps.insert(n);};
        ev.base=&io;ev.flr_snd=3;
        ev.floor_step=[](void* u,std::uint32_t snd,std::uint32_t type){static_cast<Host*>(u)->floor_steps.push_back({snd,type});};
    }
};
}
int main(){try{
    _Make_SinTable();
    { // pl_evt.c:68 pl_smove00: Ereignispose PlMtnAct[..][6], danach [6]+1.
        Host h;RECVX_PLAYER_ACTION a{};RECVX_PLAYER_EVENT e{};a.rtspd=1;
        check(recvx_player_event(&a,&e,&h.ev)==1,"pl_smove00 not dispatched");
        check(a.mtn_no==recvx_player_mtn_act(0,0,6)&&a.mode3==1&&a.hokan_count==8&&a.hokan_rate==49152,"pl_smove00 case 0");
        check((h.flg&0xD0000u)==0xD0000u&&(h.exp1&4u),"pl_smove00 flg 0xD0000 / EXP1_I(0) 4");
        a.frm_no=38*65536;recvx_player_event(&a,&e,&h.ev);check(a.mode3==1,"pl_smove00 left the pose early");
        a.frm_no=39*65536;recvx_player_event(&a,&e,&h.ev);check(a.mtn_no==recvx_player_mtn_act(0,0,6)+1&&a.mode3==2&&a.hokan_count==4&&a.frm_no==0,"pl_smove00 case 1");
    }
    { // pl_evt.c:111 pl_smove01 mode1 0: zurueck zur sicheren Lage spx/spz waehrend der Ueberblendung.
        Host h;RECVX_PLAYER_ACTION a{};RECVX_PLAYER_EVENT e{};a.mode2=1;a.spx=10;a.spz=-5;h.flg|=0x2C0000u;
        recvx_player_event(&a,&e,&h.ev);
        check(a.mtn_no==recvx_player_mtn_act(0,0,0)&&std::fabs(a.px-3.f)<1e-6f&&std::fabs(a.pz+1.5f)<1e-6f,"pl_smove01 case 0/1 step toward spx/spz");
        check(!(h.flg&0x200000u)&&(h.flg&0xC0000u),"pl_smove01 flg 0x200000 / 0xC0000");
        a.hokan_count=0;recvx_player_event(&a,&e,&h.ev);check(!(h.flg&0xC0000u)&&std::fabs(a.px-3.f)<1e-6f,"pl_smove01 end of blend");
    }
    { // pl_smove01 mode1 1: links drehen mit Rueckwaertsmotion und Fussschritt aus flr_snd.
        Host h;RECVX_PLAYER_ACTION a{};RECVX_PLAYER_EVENT e{};a.mode2=1;e.mode1=1;a.rtspd=1;a.ay=0;
        recvx_player_event(&a,&e,&h.ev);
        check(a.mtn_no==recvx_player_mtn_act(0,0,1)&&a.ay==-int(182.04445f*7.2f)&&a.mtn_add==-65536,"pl_smove01 mode1 1");
        a.frm_no=65536;recvx_player_event(&a,&e,&h.ev);check(h.floor_steps.size()==1&&h.floor_steps[0].first==3,"pl_smove01 footstep");
    }
    { // pl_evt.c:222 pl_smove02: gehen; ohne flg 0x800000 ab Bild 5, Geschwindigkeit aus njCos.
        Host h;RECVX_PLAYER_ACTION a{};RECVX_PLAYER_EVENT e{};a.mode2=2;a.ay=0;
        recvx_player_event(&a,&e,&h.ev);
        const float spd=0.18f+std::fabs(0.15f*(float)njCos(5*int(65536/40)));
        check(a.mtn_no==recvx_player_mtn_act(0,0,2)&&a.frm_no==5*65536&&a.arp==2.0f&&std::fabs(a.spd-spd)<1e-6f,"pl_smove02 case 0/1");
        check(std::fabs(a.pz+spd*njCos(0))<1e-6f,"pl_smove02 bhAddSpeed");
        const int frame=recvx_player_foot_snd(0,0,0);a.frm_no=frame*65536;recvx_player_event(&a,&e,&h.ev);
        check(!h.steps.empty()&&h.steps.back().first==17&&(h.stflg&0x200u),"pl_smove02 footstep at PlFootSnd");
    }
    { // pl_evt.c:546 pl_smove06: zum Ziel aus ct0 (X oben, Z unten) gehen.
        Host h;RECVX_PLAYER_ACTION a{};RECVX_PLAYER_EVENT e{};a.mode2=6;a.px=0;a.pz=0;a.ay=0;a.ct0=int(std::uint32_t(1000)<<16);
        recvx_player_event(&a,&e,&h.ev);
        check(a.ay==-910&&(h.flg&0x200000u),"pl_smove06 turn toward the target");
    }
    { // pl_evt.c:649 pl_smove08: mtn_no = mode1 (42 = PlMtnAct[..][0]), ct0 1 haelt Bild ct2-2.
        Host h;RECVX_PLAYER_ACTION a{};RECVX_PLAYER_EVENT e{};a.mode2=8;e.mode1=42;a.ct0=1;e.ct2=10;e.mtn_md=0x2;
        recvx_player_event(&a,&e,&h.ev);check(a.mtn_no==recvx_player_mtn_act(0,0,0)&&a.mode3==3&&!(e.mtn_md&2u),"pl_smove08 case 0");
        a.frm_no=9*65536;recvx_player_event(&a,&e,&h.ev);check(a.frm_no==8*65536&&a.hokan_count==4,"pl_smove08 case 3");
        a.mode3=4;a.frm_no=12*65536;recvx_player_event(&a,&e,&h.ev);check(e.ct2==12&&a.frm_no==11*65536,"pl_smove08 case 4");
    }
    { Host h;RECVX_PLAYER_ACTION a{};RECVX_PLAYER_EVENT e{};a.mode2=10;check(recvx_player_event(&a,&e,&h.ev)==0&&!h.gaps.empty(),"mode2 outside bhCPM0_event_tbl not reported");}
    std::cout<<"A02 player event motions (pl_evt.c pl_smove00..08) match the original state machines.\n";return 0;
}catch(const std::exception& e){std::cerr<<"player event test failed: "<<e.what()<<'\n';return 1;}}
