#include "player_push_core.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
typedef RECVX_STAIR_AREA ATR_WORK;
typedef struct {float x,y;} NJS_POINT2;
typedef struct {float px,py,pz,vx,vy,vz;} NJS_LINE;
typedef struct {NJS_POINT3 v[8];} NJS_BOX;
typedef struct {NJS_POINT3 c;float r;} NJS_SPHERE;
typedef struct {NJS_POINT3 c1,c2;float r;} NJS_CAPSULE;
typedef struct {float px,py,pz,ox,oy,oz;} POINT;
typedef RECVX_PUSH_ANGLE NJS_MKEY_A_MOD;
typedef RECVX_PUSH_POSITION NJS_MKEY_F_MOD;
typedef RECVX_PUSH_CHANNEL NJS_MDATA2_MOD;
typedef RECVX_PUSH_MOTION MN_WORK;
typedef struct {unsigned flg;} O_WORK;
typedef struct {O_WORK* owP;RECVX_PUSH_POSE* objP;} ML_WORK;
typedef struct {
 unsigned flg,flg2,stflg,mdflg;unsigned char mode0,mode1,mode2,mode3;
 float px,py,pz,pxb,pyb,pzb;int ay,ayp;char flr_no;float ar,ah,aw,ad,spd;
 int idx_ct,aspd,param,pn,ct0,ct1,ct2,ct3,ani_ct,psh_ct,psh_idx;
 unsigned mtn_no,mtn_md,hokan_count,mtn_attr;int frm_no,frm_mode,mtn_add,hokan_rate;
 ATR_WORK* dan_ap;int flr_snd,footeff;void* exp0;void* exp1;MN_WORK* mnwP;ML_WORK* mlwP;
 void* lkwkp;int id;
} BH_PWORK;
static void PushModeAssign(BH_PWORK* p,unsigned v){p->mode0=v;p->mode1=v>>8;p->mode2=v>>16;p->mode3=v>>24;}
typedef BH_PWORK O_WRK;
static O_WRK* eff;
static _Thread_local unsigned* unsupported_services;
typedef struct {unsigned st_flg,gm_flg,pad_on,cb_flg;float pad_dx,pad_ax;int mwal_n,metc_n,mflr_n,dla_n,flr_idx,onow,psh_snd,ply_id;ATR_WORK *mwalp,*metcp,*mflrp;} PUSH_SYS;
typedef struct {unsigned wal_n,etc_n,flr_n;ATR_WORK *walp,*etcp,*flrp;float h,grand[32];} PUSH_ROOM;
static _Thread_local PUSH_SYS* sys;
static _Thread_local PUSH_ROOM* rom;
static _Thread_local BH_PWORK* plp;
static _Thread_local const RECVX_PUSH_HOST* host;
static _Thread_local const float* translation;
#define ABS(x) ((x)<0?-(x):(x))
#define EXP1_I(o) (*(int*)((char*)plp->exp1+(o)))
static float* bhGetTransZ(int n){assert(n==9);return (float*)translation;}
static void CallPlayerActionSe(int n,int flag){if(flag&&host->sound)host->sound(host->user,n);}
static void StartVibrationEx(int p,int n){if(host->vibration)host->vibration(host->user,p,n);}
static int bhSetEffect(int id,POINT* p,void* link,int n){
 if(!host->set_effect){++*unsupported_services;return -1;}
 int slot=host->set_effect(host->user,id,&p->px,link,n);
 if(slot<0||slot>=512)return -1;
 eff[slot].flg=0x81;eff[slot].id=id;eff[slot].lkwkp=link;
 return slot;
}
#include "player_push.inc"
struct RECVX_PUSH_CORE {PUSH_SYS system;PUSH_ROOM room;BH_PWORK player,*objects,effects[512];unsigned count,sp_flg;int position_bound;EXP_WORK exp;int exp1[32];RECVX_PUSH_HOST host;ML_WORK model;O_WORK nodes[256];const float* translation;unsigned unsupported;};
static void bind(RECVX_PUSH_CORE* c){sys=&c->system;rom=&c->room;plp=&c->player;eff=c->effects;host=&c->host;translation=c->translation;unsupported_services=&c->unsupported;}
static void export_state(RECVX_PUSH_STATE* s){s->unsupported_services=*unsupported_services;s->active=plp->mode2==18;s->phase=plp->mode3;s->contact=plp->psh_ct;s->wall=plp->psh_idx;s->ay=plp->ay&65535;s->motion=plp->mtn_no;s->frame=plp->frm_no;s->mode0=plp->mode0;s->flags=plp->flg;s->status=plp->stflg;s->x=plp->px;s->y=plp->py;s->z=plp->pz;s->speed=plp->spd;}
static void objects_tick(RECVX_PUSH_CORE* c){if(!(c->sp_flg&4))return;for(unsigned i=0;i<c->count;++i){O_WRK* o=c->objects+i;if(!(o->flg&1)||(o->stflg&0x1000000)||(o->flg&0x100000 && o->mode0==0))continue;o->pxb=o->px;o->pyb=o->py;o->pzb=o->pz;sys->onow=o->idx_ct;bhObj001(o);}}
RECVX_PUSH_CORE* recvx_push_create(const ATR_WORK* walls,unsigned n,float height,const float* grand,const RECVX_PUSH_OBJECT* objects,unsigned count,const RECVX_PUSH_HOST* h){
 RECVX_PUSH_CORE* c=calloc(1,sizeof *c);if(!c)return NULL;c->sp_flg=5;c->room.wal_n=n;c->room.h=height;c->room.walp=calloc(n?n:1,sizeof(ATR_WORK));if(n)memcpy(c->room.walp,walls,n*sizeof(ATR_WORK));memcpy(c->room.grand,grand,sizeof c->room.grand);c->count=count;c->objects=calloc(count?count:1,sizeof(BH_PWORK));c->system.mwalp=calloc(count*2+4,sizeof(ATR_WORK));c->system.metcp=calloc(count*2+1,sizeof(ATR_WORK));c->system.mflrp=calloc(count+1,sizeof(ATR_WORK));if(h)c->host=*h;c->system.ply_id=c->host.ply_id;
 c->player.exp0=&c->exp;c->player.exp1=c->exp1;c->model.owP=c->nodes;c->player.mlwP=&c->model;c->exp.rtspd=1;c->player.flg=0x119;c->player.stflg=0x40000000;c->player.ar=3.5f;c->player.ah=PlyInfo[c->system.ply_id][1];c->player.mode0=1;
 for(unsigned i=0;i<count;++i){BH_PWORK* o=c->objects+i;o->flg=objects[i].flags;o->idx_ct=objects[i].index;o->aspd=objects[i].aspd;o->param=objects[i].param;o->flr_no=objects[i].floor;o->px=objects[i].x;o->py=objects[i].y;o->pz=objects[i].z;}
 bind(c);objects_tick(c);return c;
}
void recvx_push_enabled(RECVX_PUSH_CORE* c,unsigned flags){c->sp_flg=flags;}
void recvx_push_destroy(RECVX_PUSH_CORE* c){if(c){free(c->room.walp);free(c->room.etcp);free(c->room.flrp);free(c->objects);free(c->system.mwalp);free(c->system.metcp);free(c->system.mflrp);free(c);}}
void recvx_push_motions(RECVX_PUSH_CORE* c,MN_WORK* motions,unsigned count,const float* tr,unsigned tr_count,RECVX_PUSH_POSE* poses,unsigned nodes){assert(count>50&&tr_count>1&&nodes<=256);c->player.mnwP=motions;c->translation=tr;c->model.objP=poses;}
void recvx_push_contact(RECVX_PUSH_CORE* c,RECVX_PUSH_STATE* s,float ox,float oy,float oz,float x,float y,float z,int ay,int floor,int walking,int dmlvl,int wpntp){bind(c);if(!(c->sp_flg&1)){plp->px=ox;plp->py=oy;plp->pz=oz;objects_tick(c);export_state(s);return;}// bhInitPlayer calls bhCheckFloorP before the first collision. External
 // actor changes (including the separate original climb work) rebind its
 // current floor state before continuing bhControlPlayer.
 if(!c->position_bound||plp->px!=ox||plp->py!=oy||plp->pz!=oz||plp->flr_no!=floor){plp->px=ox;plp->py=oy;plp->pz=oz;plp->ay=ay;plp->flr_no=floor;bhCheckFloorP(plp);c->position_bound=1;}
 plp->px=x;plp->py=y;plp->pz=z;plp->ay=ay;plp->flr_no=floor;c->exp.dmlvl=dmlvl;c->exp.wpntp=wpntp;plp->mode2=walking?3:0;if(!walking)plp->psh_ct=0;c->exp.arp=walking?2:0;
 // bhCPM2_act_wlk tests the previous collision pass before current collision.
 if(walking&&plp->psh_ct>5&&!(plp->stflg&0x80)){sys->st_flg|=4;plp->stflg|=0x10080;plp->mode2=18;plp->mode3=0;}
 plp->pxb=ox;plp->pyb=oy;plp->pzb=oz;c->exp.bpxb=c->exp.bpx;c->exp.bpyb=c->exp.bpy;c->exp.bpzb=c->exp.bpz;PushPlayerCollision();bhCheckFloorP(plp);objects_tick(c);export_state(s);}
void recvx_push_wall_resolve(RECVX_PUSH_CORE* c,RECVX_PUSH_STATE* s,float x,float y,float z,int ay,int floor,float radius,float height){bind(c);plp->px=x;plp->py=y;plp->pz=z;plp->ay=ay;plp->flr_no=floor;plp->ar=radius;plp->ah=height;bhCheckWall(plp);export_state(s);}
void recvx_push_tick(RECVX_PUSH_CORE* c,RECVX_PUSH_STATE* s,int forward,int ply_id,int dmlvl,int wpntp){bind(c);if(!(c->sp_flg&1)){objects_tick(c);export_state(s);return;}sys->ply_id=ply_id;plp->ah=PlyInfo[ply_id][1];c->exp.dmlvl=dmlvl;c->exp.wpntp=wpntp;NJS_POINT3 old={plp->px,plp->py,plp->pz};
 if(!forward&&(plp->stflg&0x80)){plp->flg|=0x200000;if(plp->mode3==5)plp->mode3=6;}
 c->exp.bpxb=c->exp.bpx;c->exp.bpyb=c->exp.bpy;c->exp.bpzb=c->exp.bpz;
 PushRotationSpeed();plp->mtn_md=0;bhCPM2_act_psh();bhSetMotion(plp,plp->mtn_add,plp->mtn_md,NULL);
 if(plp->frm_no/65536>=plp->mnwP[plp->mtn_no].frm_num-1)plp->flg|=0x400000;else plp->flg&=~0x400000;
 plp->pxb=old.x;plp->pyb=old.y;plp->pzb=old.z;PushPlayerCollision();bhCheckFloorP(plp);objects_tick(c);export_state(s);
}
void recvx_push_object(RECVX_PUSH_CORE* c,unsigned i,float out[3]){assert(i<c->count);out[0]=c->objects[i].px;out[1]=c->objects[i].py;out[2]=c->objects[i].pz;}
unsigned recvx_push_walls(RECVX_PUSH_CORE* c,const ATR_WORK** out){*out=c->system.mwalp;return c->system.mwal_n;}
unsigned recvx_push_steps(RECVX_PUSH_CORE* c,const ATR_WORK** out){*out=c->system.metcp;return c->system.metc_n;}
unsigned recvx_push_floors(RECVX_PUSH_CORE* c,const ATR_WORK** out){*out=c->system.mflrp;return c->system.mflr_n;}

void recvx_push_update_object(RECVX_PUSH_CORE* c,unsigned i,float x,float y,float z,int hidden){assert(i<c->count);BH_PWORK* o=&c->objects[i];o->px=x;o->py=y;o->pz=z;if(hidden)o->stflg|=0x1000000;else o->stflg&=~0x1000000;}
void recvx_push_update_walls(RECVX_PUSH_CORE* c,const ATR_WORK* w,unsigned n){assert(n==c->room.wal_n);if(n)memcpy(c->room.walp,w,n*sizeof *w);}

void recvx_push_terrain(RECVX_PUSH_CORE* c,const ATR_WORK* etc,unsigned ne,const ATR_WORK* flr,unsigned nf){
 assert(!c->position_bound);free(c->room.etcp);free(c->room.flrp);c->room.etc_n=ne;c->room.flr_n=nf;
 c->room.etcp=calloc(ne?ne:1,sizeof(ATR_WORK));c->room.flrp=calloc(nf?nf:1,sizeof(ATR_WORK));
 if(ne)memcpy(c->room.etcp,etc,ne*sizeof(ATR_WORK));if(nf)memcpy(c->room.flrp,flr,nf*sizeof(ATR_WORK));
}

// Original bhCheckWallType can query a wall table without a player work.
int recvx_push_probe(const ATR_WORK* walls,unsigned n,float height,float x,float y,float z,float radius,float body_height,unsigned flags,unsigned* attr){
 PUSH_ROOM room={0};PUSH_SYS system={0};PUSH_ROOM* previous_room=rom;PUSH_SYS* previous_system=sys;
 room.walp=(ATR_WORK*)walls;room.wal_n=n;room.h=height;rom=&room;sys=&system;
 NJS_POINT3 position={x,y,z};ATR_WORK* hit=bhCheckWallType(&position,flags,radius,body_height);
 if(hit&&attr)*attr=hit->attr;rom=previous_room;sys=previous_system;return hit!=NULL;
}
