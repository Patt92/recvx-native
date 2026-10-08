#include "player_stairs.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <string.h>
// Field-name compatible subsets of the original works; the routines are copied verbatim.
typedef RECVX_STAIR_AREA ATR_WORK;
typedef struct {float mtx[16];} O_WORK;
typedef struct {O_WORK* owP;} ML_WORK;
typedef struct {
    unsigned int flg;char flr_no,flr_nob;unsigned char mode0,mode1,mode2,mode3;
    float px,py,pz;int ax,ay,az;float pxb,pyb,pzb;float spd;unsigned int flg2,mdflg,stflg;
    unsigned int mtn_no;int frm_no;int hokan_rate;unsigned int hokan_count;int mtn_add;unsigned int mtn_md;int ayp;
    int ct0,ct1,ct2,ct3,flr_snd;float yn,gpx,gpz;ML_WORK* mlwP;void* exp0;void* exp1;
} BH_PWORK;
#define ABS(x) ((x)<0?-(x):(x))
#define EXP1_I(o) (*(int*)((char*)plp->exp1+(o)))
typedef NJS_POINT3 POINT;
static unsigned unsupported_services;
static struct {ATR_WORK* pl_htp;unsigned int st_flg;char ply_id;struct {int id,flg,mdlver,type,flr_no,ay;float sx,sy,sz,px,py,pz;} ef;} sys_work;
static struct {float grand[32];} rom_work;
#define sys (&sys_work)
#define rom (&rom_work)
static O_WORK nodes[32];
static ML_WORK model={nodes};
static int exp1_words[32];
static BH_PWORK player;
static BH_PWORK* plp=&player;
static ATR_WORK area_work;
static const RECVX_STAIR_HOST* host_now;
static void bhAddSpeed(BH_PWORK* pp,int r){int angle=(pp->ay+r)&0xFFFF;pp->px-=pp->spd*njSin(angle);pp->pz-=pp->spd*njCos(angle);}
static int bhCheckFloorNum(float py){int i,fno=2;for(i=0;i<31;i++)if((rom->grand[i]&&py>=rom->grand[i])||(i==2&&py>=rom->grand[i]))fno=i;return fno-2;}
static void bhSetFloorNum(BH_PWORK* pp){int fnow=pp->flr_no;pp->flr_no=(char)bhCheckFloorNum(pp->py);if(pp->flr_no!=fnow)pp->flr_nob=(char)fnow;}
static void bhGetObjMotion(BH_PWORK* ewP,int obj_no,float* pos,int* ang){(void)obj_no;ang[0]=ang[1]=ang[2]=0;if(!host_now->motion_root(host_now->user,ewP->mtn_no,ewP->frm_no/65536,pos))pos[0]=pos[1]=pos[2]=0;}
static int bhCheckFloorSound(BH_PWORK* pp,int floor,float x,float z){return host_now->floor_sound(host_now->user,floor,x,z,(pp->stflg&0x10)!=0);}
static void CallPlayerFootStepSe(int floor_sound,int a,int b){(void)a;(void)b;if(host_now->footstep)host_now->footstep(host_now->user,floor_sound);}
static void bhClrUseKaidanFlag(BH_PWORK* pp){(void)pp;area_work.attr&=~0x400000u;} // area copy only; the room table is untouched
// Water particles and rumble are separate host services. No water flags are
// synthesized by this bridge. Keep unsupported calls visible if added later.
static void bhSetEffect(int id,POINT* pos,void* link,int count){(void)id;(void)pos;(void)link;(void)count;++unsupported_services;}
static void bhSetEffectTb(void* effect,void* a,void* b,int n){(void)effect;(void)a;(void)b;(void)n;++unsupported_services;}
static ATR_WORK* bhCheckWater(NJS_POINT3* pos){(void)pos;++unsupported_services;return NULL;}
static void CallPlayerActionSe(int id,int n){(void)id;(void)n;++unsupported_services;}
static void StartVibrationEx(int port,int id){(void)port;(void)id;}
#include "player_stairs.inc"
static EXP_WORK exp0_work;
int recvx_stairs_begin(RECVX_STAIR_STATE* state,const RECVX_STAIR_AREA* area,const float* grand,float px,float py,float pz,int ay,int floor,int dmlvl,int wpntp){
    unsigned i;unsupported_services=0;state->unsupported_services=0;for(i=0;i<32;++i)rom_work.grand[i]=grand[i];
    memset(&player,0,sizeof player);memset(&exp0_work,0,sizeof exp0_work);memset(exp1_words,0,sizeof exp1_words);
    area_work=*area;area_work.attr|=0x400000u; /* bhSetUseKaidanFlag */
    exp0_work.wpntp=wpntp;exp0_work.dmlvl=dmlvl;exp0_work.rtspd=1.0f;
    player.exp0=&exp0_work;player.exp1=exp1_words;player.mlwP=&model;
    player.px=px;player.py=py;player.pz=pz;player.ay=ay;player.flr_no=(char)floor;
    // bhCheckExmAtari case 1: stflg |= 0x80010010, flg |= 0x10400, flg &= ~0x110, st_flg |= 4.
    player.stflg|=area->type==2?0x80010020u:0x80010010u;player.flg|=0x10400u;player.flg&=~0x110u;sys_work.st_flg|=4;sys_work.pl_htp=&area_work;
    player.mode0=1;player.mode1=0;player.mode3=0;
    if(area->type==2)player.mode2=area->prm0?17:16;
    else if(area->prm0==0)player.mode2=(area->attr&1)?20:14;
    else{player.mode2=(area->attr&1)?21:15;player.flr_no=(char)bhCheckFloorNum(player.py-2.0f*area->prm2);}
    state->active=1;state->px=px;state->py=py;state->pz=pz;state->ay=ay;state->motion=0;state->frame=0;state->floor=floor;
    return 1;
}
int recvx_stairs_tick(RECVX_STAIR_STATE* state,const RECVX_STAIR_HOST* host){
    unsigned i,frames;if(!state->active)return 0;
    host_now=host;sys_work.ply_id=(char)host->ply_id;for(i=0;i<32;++i)rom_work.grand[i]=host->grand[i];
    for(i=0;i<32;++i){float p[3];host->node_position(host->user,i,p);nodes[i].mtx[12]=p[0];nodes[i].mtx[13]=p[1];nodes[i].mtx[14]=p[2];}
    switch(player.mode2){case 16:bhCPM2_act_dnu();break;case 17:bhCPM2_act_dnd();break;case 14:bhCPM2_act_kdu();break;case 15:bhCPM2_act_kdd();break;case 20:bhCPM2_act_hsu();break;case 21:bhCPM2_act_hsd();break;default:state->active=0;break;}
    if(state->active&&player.mode2!=14&&player.mode2!=15&&player.mode2!=16&&player.mode2!=17&&player.mode2!=20&&player.mode2!=21)state->active=0; /* *(int*)&mode0 = 1 */
    // bhSetMotion: frm_no += mtn_add, wrapped by frm_num; bhControlPlayer sets flg 0x400000 on the last frame.
    frames=host->motion_frames(host->user,player.mtn_no);
    {const int sampled=player.frm_no; /* SetMtn* writes objP from the frame before the advance */
    if(frames){const int total=(int)frames<<16;player.frm_no+=player.mtn_add;if(player.frm_no<0)player.frm_no+=total;if(player.frm_no>=total)player.frm_no-=total;
        if(player.frm_no/65536>=(int)frames-1)player.flg|=0x400000u;else player.flg&=~0x400000u;}
    // bhControlPlayer: flg 0x40000 keeps the PlyLegRoute foot planted (bhFixPosition / XYZ with 0x8000000).
    if(player.flg&0x40000u){static const unsigned feet[2]={17,21};const unsigned foot=feet[(player.flg&0x80000u)?1:0];float now[3];
        host->pose_node(host->user,player.mtn_no,sampled,player.px,player.py,player.pz,player.ay,foot,now);
        player.px-=now[0]-nodes[foot].mtx[12];player.pz-=now[2]-nodes[foot].mtx[14];if(player.flg&0x8000000u)player.py-=now[1]-nodes[foot].mtx[13];}}
    state->unsupported_services=unsupported_services;state->px=player.px;state->py=player.py;state->pz=player.pz;state->ay=player.ay&0xFFFF;state->motion=player.mtn_no;state->frame=player.frm_no;state->floor=player.flr_no;
    return state->active;
}
