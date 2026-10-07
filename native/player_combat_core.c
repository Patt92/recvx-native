#include "player_combat_core.h"
#include "ninja_matrix.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
typedef RECVX_PUSH_ANGLE NJS_MKEY_A_MOD;
typedef RECVX_PUSH_POSITION NJS_MKEY_F_MOD;
typedef RECVX_PUSH_CHANNEL NJS_MDATA2_MOD;
// Original field layouts from the shared enemy adapter; only pointer representation changes.
#include "en01_damage_types.inc"
#include "player_combat_types.inc"
#define ply (*plp)
#define ABS(x) ((x)<0?-(x):(x))
static _Thread_local BH_PWORK *plp,*ene;
static _Thread_local const RECVX_COMBAT_HOST* host;
static _Thread_local unsigned* shot_count;
static _Thread_local struct {unsigned* pip;} swork;
static NJS_MATRIX lcmat[2];
static struct {int pe_ax,pe_pers,ax,axp,ppers;} cam;
typedef struct {unsigned gm_flg,st_flg,pt_flg,ef_flg,ev_flg[32],pad_on,pad_ps;int ply_id,gm_mode,ewk_n,eor_ct,yk_ct,mg_ct;float pad_ax,pad_dx,pad_dy,pad_ay;BH_PWORK* obwp;EF_WORK ef;ML_WORK efm[512];NJS_POINT3 apos;ATR_WORK* ahtp;} COMBAT_SYS;
static _Thread_local COMBAT_SYS* sys;

typedef BH_PWORK O_WRK;
typedef struct {int idx;float ln;} SC_WORK;
static _Thread_local O_WRK* eff;
static _Thread_local struct {LGT_WORK* lgtp;} room;
#define rom (&room)
static void npSetMemoryL(void* p,unsigned n,unsigned value){assert(value==0);memset(p,0,n*4);}
static void StartVibrationEx(int,int);
static int bhSetEffectTb(EF_WORK*,NJS_POINT3*,unsigned char*,int);
static void bhSetEffGunSpark(NJS_POINT3* p,NJS_POINT3* d,unsigned s,unsigned e,int t){host->spark(host->user,&p->x,&d->x,s,e,t);}
static int bhCheckL2Wall(NJS_LINE* l,unsigned f,float* distance){return host->line(host->user,&l->px,&l->vx,f,distance,&sys->apos.x,sys->ahtp);}
static int bhCheckWallAttrB89(ATR_WORK* w){return (w->prm0&3)!=0;}
static float bhGetGroundPosition(NJS_POINT3* p){return host->ground(host->user,&p->x);}
static ATR_WORK* bhCheckWater(NJS_POINT3* p){static _Thread_local ATR_WORK w;return host->water(host->user,&p->x,&w)?&w:NULL;}
static void unbound(const char* name){if(host->unbound)host->unbound(host->user,name);}
static int bhSearchBullet(void){return host->search_bullet(host->user);}
static int bhCkFlg(const unsigned* flags,int index){return (flags[index>>5]&(0x80000000u>>(index&31)))!=0;}
static void bhStFlg(unsigned* flags,int index){flags[index>>5]|=0x80000000u>>(index&31);}
static void bhCheckKnifeAtari(GA_WORK* shot){host->knife_hit(host->user,(const RECVX_GUN_HIT*)shot);}
static int bhCheckGunAtari(GA_WORK* shot){return host->gun_hit(host->user,(const RECVX_GUN_HIT*)shot);}
static int bhCheckC2WallN(NJS_CAPSULE* cap,unsigned flags){return host->wall_hit(host->user,&cap->c1.x,&cap->c2.x,cap->r,flags);}
static void CallPlayerWeaponSeEx(NJS_POINT3* p,unsigned se,int flag){(void)flag;host->sound(host->user,&p->x,se);}
static int bhCheckFloorSound(BH_PWORK* p,int floor,float x,float z){return host->floor(host->user,floor,x,p->py,z);}
static void CallPlayerFootStepSe(int s,int a,int b){host->footstep(host->user,s,a,b);}
static void StartVibrationEx(int p,int n){host->vibration(host->user,p,n);}
static int bhCountBullet(void);
#include "player_combat.inc"
static int bhCountBullet(void){int result=bhCountBulletOriginal();if(result)++*shot_count;return result;}
struct RECVX_COMBAT_CORE {COMBAT_SYS system;LGT_WORK light;ATR_WORK wall;O_WRK effects[512];BH_PWORK player,enemies[128],weapon;EXP_WORK exp;int exp1[32];PP_WORK pair;RECVX_COMBAT_HOST host;ML_WORK model,weapon_model;O_WORK nodes[256];NJS_CNK_OBJECT objects[256],weapon_objects[8];MN_WORK* motions;unsigned motion_count;RECVX_PUSH_POSE* poses;unsigned pose_count,shots;int pose_frame;};
static void bind(RECVX_COMBAT_CORE* c){sys=&c->system;plp=&c->player;ene=c->enemies;host=&c->host;shot_count=&c->shots;eff=c->effects;room.lgtp=&c->light;sys->ahtp=&c->wall;}
RECVX_COMBAT_CORE* recvx_combat_create(const RECVX_COMBAT_HOST* h){RECVX_COMBAT_CORE* c=calloc(1,sizeof *c);if(!c)return NULL;c->host=*h;c->player.exp0=(unsigned char*)&c->exp;c->player.exp1=(unsigned char*)c->exp1;c->player.exp2=(unsigned char*)&c->pair;c->player.mlwP=&c->model;c->model.owP=c->nodes;c->model.objP=c->objects;c->exp.rtspd=1;c->player.mode0=1;c->player.flg=0x20119;c->player.stflg=0x40000000;c->system.obwp=&c->weapon;c->weapon.mlwP=&c->weapon_model;c->weapon_model.objP=c->weapon_objects;c->pair.gun_delay=-1;c->player.src_no=-1;return c;}
void recvx_combat_destroy(RECVX_COMBAT_CORE* c){if(c){free(c->motions);free(c);}}
void recvx_combat_motions(RECVX_COMBAT_CORE* c,const RECVX_PUSH_MOTION* m,unsigned count,RECVX_PUSH_POSE* poses,unsigned nodes){assert(nodes<=256&&count>PlMtnWpn[4]);free(c->motions);c->motions=calloc(count,sizeof(MN_WORK));c->motion_count=count;for(unsigned i=0;i<count;++i){c->motions[i].flg=m[i].flg;c->motions[i].obj_num=m[i].obj_num;c->motions[i].frm_num=m[i].frm_num;c->motions[i].datP=m[i].datP;c->motions[i].md2P=m[i].md2P;c->motions[i].atrP=m[i].atrP;}c->player.mnwP=c->motions;c->poses=poses;c->pose_count=nodes;c->model.obj_num=nodes;for(unsigned i=0;i<nodes;++i){memcpy(c->objects[i].pos,poses[i].pos,sizeof poses[i].pos);memcpy(c->objects[i].ang,poses[i].ang,sizeof poses[i].ang);}}
void recvx_combat_seed(RECVX_COMBAT_CORE* c,unsigned motion,int frame){assert(motion<c->motion_count);c->player.mtn_no=motion;c->player.frm_no=frame;for(unsigned i=0;i<c->pose_count;++i){memcpy(c->objects[i].pos,c->poses[i].pos,sizeof c->poses[i].pos);memcpy(c->objects[i].ang,c->poses[i].ang,sizeof c->poses[i].ang);}}
void recvx_combat_nodes(RECVX_COMBAT_CORE* c,const float* matrices,unsigned n){assert(n<=256);for(unsigned i=0;i<n;++i)memcpy(c->nodes[i].mtx,matrices+i*16,sizeof c->nodes[i].mtx);}
void recvx_combat_enemies(RECVX_COMBAT_CORE* c,const RECVX_COMBAT_ENEMY* list,unsigned count){assert(count<=128);unsigned max=0;for(unsigned i=0;i<count;++i){assert(list[i].index<128);if(list[i].index+1>max)max=list[i].index+1;}for(unsigned i=0;i<128;++i)c->enemies[i].flg=0;c->system.ewk_n=max;for(unsigned i=0;i<count;++i){const RECVX_COMBAT_ENEMY* t=list+i;BH_PWORK* e=c->enemies+t->index;e->flg=0x21;e->px=t->x;e->py=t->y;e->pz=t->z;e->watr.c1=(NJS_POINT3){t->x,t->y+t->radius,t->z};e->watr.c2=(NJS_POINT3){t->x,t->y+t->height-t->radius,t->z};e->watr.r=t->radius;}}
void recvx_combat_tick(RECVX_COMBAT_CORE* c,RECVX_COMBAT_STATE* out,unsigned pad,unsigned pressed,unsigned* items,int weapon,int character,float x,float y,float z,int yaw,unsigned* gm_flags,unsigned* ev_flags){bind(c);swork.pip=items;sys->pad_on=pad;sys->pad_ps=pressed;sys->pad_dx=(pad&8)?128:(pad&4)?-128:0;sys->pad_dy=(pad&0x20)?-128:(pad&0x40)?128:0;sys->ply_id=character;if(gm_flags)sys->gm_flg=*gm_flags;if(ev_flags)memcpy(sys->ev_flg,ev_flags,sizeof sys->ev_flg);plp->wpnr_no=weapon;plp->px=x;plp->py=y;plp->pz=z;plp->ay=yaw;plp->mtn_md=0;
 CombatEntry();if(plp->mode1)bhCPM1_act_atk();CombatYaw();PlyPchMain(plp);c->pose_frame=plp->frm_no;
 if(plp->mode1){assert(plp->mtn_no<c->motion_count&&plp->mnwP[plp->mtn_no].datP);bhSetMotion(plp,plp->mtn_add,plp->mtn_md,plp->mtn_tp);if(plp->frm_no/65536>=(int)plp->mnwP[plp->mtn_no].frm_num-1)plp->flg|=0x400000;else plp->flg&=~0x400000;}
 for(unsigned i=0;i<c->pose_count;++i){memcpy(c->poses[i].pos,c->objects[i].pos,sizeof c->poses[i].pos);memcpy(c->poses[i].ang,c->objects[i].ang,sizeof c->poses[i].ang);}
 if(!(plp->flg&0x10000)){plp->mode1=plp->mode2=plp->mode3=0;}
 *out=(RECVX_COMBAT_STATE){.active=plp->mode1!=0,.mode=plp->mode2,.phase=plp->mode3,.motion=plp->mtn_no,.frame=plp->frm_no,.pose_frame=c->pose_frame,.yaw=plp->ay&65535,.pitch=plp->wax,.flags=plp->flg,.status=plp->stflg,.at_flags=plp->at_flg,.hokan_count=plp->hokan_count,.shots=c->shots,.gm_flags=sys->gm_flg,.target_right=plp->src_no,.target_left=plp->src_no,.arm_right=c->pair.arm_ar,.arm_left=c->pair.arm_al};
 for(unsigned i=0;i<512;++i)if((eff[i].flg&3)&&!eff[i].func){host->effect(host->user,i,eff+i);eff[i].func=(void*)1;}
 host->light(host->user,&c->light);
 if(gm_flags)*gm_flags=sys->gm_flg;
 if(ev_flags)memcpy(ev_flags,sys->ev_flg,sizeof sys->ev_flg);
 if(c->pair.sch_hed.nextP){search* t=c->pair.sch_hed.nextP;out->target_right=out->target_left=t->idx;if(c->pair.arm_dl||c->pair.arm_dr)for(search* n=t->nextP;n;n=n->nextP)if((short)(n->dir-t->dir)>=-16384&&(short)(n->dir-t->dir)<=16384){out->target_right=(c->pair.mode&8)?n->idx:t->idx;out->target_left=(c->pair.mode&8)?t->idx:n->idx;break;}}
}

void recvx_combat_after_model(RECVX_COMBAT_CORE* c){bind(c);CombatKnife();}
