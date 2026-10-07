#pragma once
#include "scene.h"
namespace recvx {
struct WPN_TAB {
    unsigned flg,atack_ct,fend_ct,at_cct;
    short ef_yct,act_ct0,act_ct1,act_ct2;
    float r,l,rn,rmax;
    NJS_POINT3 wp_fps1,wp_fps2,wp_cps;
    unsigned char ltp,lr,lg,lb;unsigned short lnr,lfr;
    float hrate,ef_scale;short snd_wpno,hiteff;unsigned short seno0,seno1;
    char vib_tp,vib_ct,reserve1,reserve2;
};
struct EF_WORK {unsigned flg;unsigned short id,type;short flr_no;unsigned short mdlver;float px,py,pz,sx,sy,sz;short ay,ax;};
extern EF_WORK WpnEffTab[23][4];
extern const WPN_TAB WpnTab[23];
extern const unsigned short PlMtnWpn[5];
extern const int KnfAtrTab[4],en01_hp_tbl[2][16],en04_hp_tbl[2][16];
extern const short EneDamNear[31][22],EneDamMid[31][22],EneDamFar[31][22];
}
