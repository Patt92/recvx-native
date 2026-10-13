/* EN53 — Nosferatus eigener Kern. Eigenstaendige native Zustandsmaschine,
   Originalreferenz en15.c:3439-3745. Kein generierter Koerper/zweiter Work.
   Kein Anschluss an jump[53], solange die Modell-/Materialdienste fehlen. */
#include "en53.h"
#include "en03_lib.h"
#include "../core_random.h"
#include "../ninja_math.h"
#include <string.h>

static const RECVX_EN53_SERVICES* en53_services;
void recvx_en53_bind(const RECVX_EN53_SERVICES* sv){en53_services=sv;}
static void gap(RECVX_ENEMY_WORK* epw,const char* name){if(epw->sv&&epw->sv->gap)epw->sv->gap(name);else recvx_t3_gap(name);}

/* en15.c:3350: Chunk 18/22 patch Byte 5, 19/23 Byte 9; der Stream ist
   little-endian short-Worte. Fuer 0..5 ein Wort, 8/9 zwei Worte,
   sonst zwei Kopfwoerter plus die angegebene Nutzlaenge. */
int recvx_en53_change_ambient(uint8_t* stream,size_t n,uint8_t value){
    size_t p=0;
    if(!stream)return 0;
    while(p+2<=n){
        const unsigned word=stream[p]|((unsigned)stream[p+1]<<8);
        const unsigned tag=stream[p];
        size_t step;
        if(word==0xffu)return 1; /* Original testet das ganze short-Wort. */
        if(tag<=5u){p+=2;continue;}
        if(tag==8u||tag==9u){if(p+4>n)return 0;p+=4;continue;}
        if(p+4>n)return 0;
        const int16_t words=(int16_t)(stream[p+2]|((unsigned)stream[p+3]<<8));
        if(words<0)return 0;
        step=4u+2u*(size_t)words;
        if(step>n-p)return 0;
        if(tag==18u||tag==22u){if(step<=5)return 0;stream[p+5]=value;}
        else if(tag==19u||tag==23u){if(step<=9)return 0;stream[p+9]=value;}
        p+=step;
    }
    return 0;
}
static int pair(RECVX_ENEMY_WORK* epw,unsigned a,unsigned b){
    if(en53_services&&en53_services->morph_pair&&en53_services->morph_pair(epw,a,b))return 1;
    gap(epw,"EN53 obj_a/obj_b (mbp[])");return 0;
}
static int ambient(RECVX_ENEMY_WORK* epw,unsigned i,int value){
    if(en53_services&&en53_services->child_ambient&&en53_services->child_ambient(epw,i,(uint8_t)value))return 1;
    gap(epw,"EN53 ChangeAmbient(mbp[].child.model.plist)");return 0;
}
static int random_value(RECVX_ENEMY_WORK* epw){return epw->sv&&epw->sv->random_next?epw->sv->random_next():recvx_core_random_next();}
static float random01(RECVX_ENEMY_WORK* epw){return (float)random_value(epw)/2147483648.0f;}

/* en15.c:3409: unlike zonzon SetMince, scale is chosen ONCE for all pieces,
   sz=0 and the velocity and texture come from the supplied work itself. */
static int mince(RECVX_ENEMY_WORK* epw,int type,int count){
    const RECVX_ENEMY_EXT* ext=recvx_enemy_ext();
    if(!ext||!ext->effect_tb||!ext->effect_patch||!en53_services||!en53_services->work_texture){gap(epw,"EN53 SetMince: effect_tb/effect_patch/epw->txp[0]");return 0;}
    RECVX_T3_EFF ef={0};
    ef.id=250;ef.flg=1;ef.type=type;ef.px=epw->dpx;ef.py=epw->dpy;ef.pz=epw->dpz;
    ef.sx=ef.sy=0.1f+0.3f*random01(epw);ef.sz=0;ef.mdlver=0;
    for(int i=0;i<count;++i){
        ef.ay=(int32_t)((recvx_t3_arc_tan2(epw->dvx,epw->dvz)+21845.0f*random01(epw))-10922.0f);
        RECVX_T3_LINK none={NULL,0};
        const int slot=ext->effect_tb(&ef,NULL,none,0);
        if(slot!=-1){
            RECVX_T3_EFFPATCH patch={0};
            patch.fields=RECVX_T3_EFF_STFLG_OR|RECVX_T3_EFF_TEXP|RECVX_T3_EFF_TEX_ID|RECVX_T3_EFF_VECTOR;
            patch.stflg_or=0x20;patch.texp=en53_services->work_texture(epw);patch.tex_id=1;
            patch.xn=epw->dvx;patch.yn=epw->dvy;patch.zn=epw->dvz;
            ext->effect_patch(slot,&patch);
        }
    }
    return 1;
}
/* en15.c:3439: no hp/exp allocation here; model and morph state only. */
static int init(RECVX_ENEMY_WORK* epw){
    if(!en53_services||!en53_services->model_root||!en53_services->morph_pair||!en53_services->child_ambient){gap(epw,"EN53 CoreInit: model/material services");return 0;}
    epw->flg&=~0x178u;epw->flg&=~6u;epw->mdflg|=4u;
    epw->aox=epw->aoy=epw->aoz=0;epw->lox=epw->loy=epw->loz=0;
    epw->mdflg|=2u;epw->shp_ct=0;epw->mtn_md=epw->mtn_no=0;epw->mtn_tp=NULL;
    epw->mtn_add=epw->frm_no=epw->hokan_rate=0;epw->hokan_count=0;
    for(unsigned i=0;i<3;++i)if(!ambient(epw,i,178))return 0;
    if(!en53_services->model_root(epw,0)){gap(epw,"EN53 mlwP->objP = mbp[0]");return 0;}
    if(!pair(epw,0,1))return 0;
    epw->shp_ct=0;++epw->mode0;epw->mode1=0;return 1;
}
/* en15.c:3472. Do not reset shape at 0->1 or 1->2: those omissions are
   real original state transitions, not a reason to normalize the curve. */
static int move(RECVX_ENEMY_WORK* epw){
    if((epw->mdflg&1u)||!(epw->mdflg&2u))return 1;
    switch(epw->mode1){
    case 0:
        if(epw->shp_ct>1000.f){if(!pair(epw,1,2))return 0;epw->mode1=1;epw->ct0=0;}
        else epw->shp_ct+=400.f;
        break;
    case 1:
        if(epw->shp_ct>1000.f){epw->mode1=2;epw->ct0=0;}
        else epw->shp_ct+=166.67f;
        break;
    case 2:
        if(epw->ct0++==3){if(!pair(epw,2,1))return 0;epw->mode1=3;epw->shp_ct=0;}
        break;
    case 3:
        if(epw->shp_ct>1000.f){if(!pair(epw,1,0))return 0;epw->mode1=4;epw->shp_ct=0;}
        else epw->shp_ct+=166.67f;
        break;
    case 4:
        if(epw->shp_ct>1000.f){if(!pair(epw,0,1))return 0;epw->mode1=0;epw->shp_ct=0;}
        else epw->shp_ct+=150.f;
        break;
    default: break;
    }
    return 1;
}
/* CoreDie mode2=1 (en15.c:3545-3617): diminishing pulse, fifth cycle ends
   at mode1=5. The original has no case 5 here, so it remains stopped. */
static int dying_pulse(RECVX_ENEMY_WORK* epw){
    if(epw->mode3==0){epw->ct1=0;++epw->mode3;}
    const float divisor=(float)(epw->ct1+2);
    switch(epw->mode1){
    case 0:
        if(epw->shp_ct>1000.f){if(!pair(epw,1,2))return 0;epw->mode1=1;}
        else epw->shp_ct+=400.f/divisor;
        break;
    case 1:
        if(epw->shp_ct>1000.f){epw->ct0=0;++epw->ct1;epw->mode1=epw->ct1<5?2:5;}
        else epw->shp_ct+=166.67f/divisor;
        break;
    case 2:
        if(epw->ct0++==3){if(!pair(epw,2,1))return 0;epw->mode1=3;epw->shp_ct=0;}
        break;
    case 3:
        if(epw->shp_ct>1000.f){if(!pair(epw,1,0))return 0;epw->mode1=4;epw->shp_ct=0;}
        else epw->shp_ct+=166.67f/divisor;
        break;
    case 4:
        if(epw->shp_ct>1000.f){if(!pair(epw,0,1))return 0;epw->mode1=0;epw->shp_ct=0;}
        else epw->shp_ct+=150.f/divisor;
        break;
    default: break;
    }
    return 1;
}
/* en15.c:3619-3722: material fade followed by original debris/blood/audio.
   No synthetic disappearance or forced completion while services are absent. */
static int death_fade(RECVX_ENEMY_WORK* epw){
    if(epw->mdflg&1u)return 1;
    if(!en53_services||!en53_services->morph_indices){gap(epw,"EN53 obj_a/obj_b indices");return 0;}
    if(epw->mode3==0){epw->ct2=178;if(epw->mode1<5)epw->mode1=0;++epw->mode3;}
    unsigned a,b;
    if(!en53_services->morph_indices(epw,&a,&b)){gap(epw,"EN53 morph_indices");return 0;}
    if(!ambient(epw,a,epw->ct2)||!ambient(epw,b,epw->ct2))return 0;
    epw->ct2=epw->ct2>4?epw->ct2-4:0;
    switch(epw->mode1){
    case 0:
        if(epw->shp_ct>1000.f){if(!pair(epw,1,2))return 0;epw->mode1=1;epw->ct0=0;}
        else epw->shp_ct+=400.f;
        break;
    case 1:
        if(epw->shp_ct>1000.f){epw->mode1=2;epw->ct0=0;}
        else epw->shp_ct+=166.67f;
        break;
    case 2: if(epw->ct2==0)epw->mode1=5;break;
    case 3:
        if(epw->shp_ct>1000.f){if(!pair(epw,1,0))return 0;epw->mode1=4;epw->shp_ct=0;}
        else epw->shp_ct+=166.67f;
        break;
    case 4:
        if(epw->shp_ct>1000.f){if(!pair(epw,0,1))return 0;epw->mode1=0;epw->shp_ct=0;}
        else epw->shp_ct+=150.f;
        break;
    case 5:{
        const RECVX_ENEMY_EXT* ext=recvx_enemy_ext();
        if(!epw->sv||!epw->sv->joint_matrix||!epw->sv->enemy_se||!epw->sv->enemy_count||!epw->sv->enemy_at||!ext||!ext->effect_tb||!ext->effect_patch||!ext->effect_particle_mk2||!en53_services->work_texture){gap(epw,"EN53 CoreDie debris/blood/texture/SE services");return 0;}
        float joint[16];if(!epw->sv->joint_matrix(epw,1,joint)){gap(epw,"EN53 owP[1].mtx");return 0;}
        epw->mdflg|=1u;epw->id=15;epw->djnt_no=1;
        epw->dpx=joint[12];epw->dpy=joint[13];epw->dpz=joint[14];
        epw->dvx=2.f*njSin(epw->ay);epw->dvz=2.f*njCos(epw->ay);epw->dvy=0;
        (void)mince(epw,2,32);
        RECVX_T3_LINK lk={epw,0};recvx_t3_set_blood_effect5(lk,2,2);
        for(int i=0;i<5;++i){
            /* njRandom()-0.5 is a double expression in this function. */
            epw->dpx=(float)(epw->dpx+(random01(epw)-0.5)*5.f);
            epw->dpy=(float)(epw->dpy+(random01(epw)-0.5)*5.f);
            epw->dpz=(float)(epw->dpz+(random01(epw)-0.5)*5.f);
            recvx_t3_set_blood_effect5(lk,0,2);
        }
        epw->id=53;
        /* en15.c:295 GetLocalEneNo searches work pointers, not model ID. */
        unsigned index=0;const unsigned n=epw->sv->enemy_count();
        for(;index<n;++index)if(epw->sv->enemy_at(index)==epw)break;
        if(index==n){gap(epw,"GetLocalEneNo: EN53 absent from ene[]");return 0;}
        RECVX_ENEMY_VEC pos={epw->px,epw->py,epw->pz};
        epw->sv->enemy_se(index,&pos,(int32_t)0x8000020au);epw->mode1=6;
        break;
    }
    default:break;
    }
    return 1;
}
void recvx_en53(RECVX_ENEMY_WORK* epw){
    if(!epw)return;
    switch(epw->mode0){
    case 0: if(!init(epw))return; /* en15.c:3731: fall through */
    case 1: (void)move(epw);break;
    case 3:
        if(epw->mode2==1)(void)dying_pulse(epw);
        else if(epw->mode2==2)(void)death_fade(epw);
        break;
    case 5:
        if(epw->sv&&epw->sv->event)epw->sv->event(epw);else gap(epw,"bhEne_Event (EN53)");
        break;
    default:break;
    }
}
