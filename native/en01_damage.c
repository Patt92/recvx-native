#include "en01_damage.h"
#include "ninja_matrix.h"
#include <math.h>
#include "capsule_math.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "en01_damage_types.inc"

#define TRUE 1
typedef struct {BH_PWORK w;_Alignas(16) unsigned char exp0[0xB0];O_WORK ow[32];NJS_CNK_OBJECT obj[32];BH_PWORK child;_Alignas(16) unsigned char cexp0[0xB0];int dog;int has_child;int enemy_no;int neck_angle;int frames_done;int pose_frame;int dead;int flinch;int flinch_ang[2];ATR_WORK* stair;} EN01_STORE;
_Static_assert(sizeof(EN01_STORE)<=sizeof(RECVX_EN01),"RECVX_EN01 storage too small");

static RECVX_EN01_HOST host;
static MN_WORK motion_table[1024],dog_motion_table[1024];
static BH_PWORK player_work;
static ML_WORK player_ml;static O_WORK player_ow[32];static unsigned char player_exp0[0x100];static MN_WORK player_own_table[1];static int player_ready;
static int ai_enabled;
static int player_exp1[16];
static float player_pos[3];
static const RECVX_EN01* const* neighbours;static unsigned neighbour_count;
static ATR_WORK mwal_storage[64];
static MN_WORK* rmthp_null;
static struct {int gm_mode,enow,ewk_n;unsigned pad_on;EF_WORK ef;int mwal_n;ATR_WORK* mwalp;int metc_n,mflr_n;ATR_WORK metcp[64],mflrp[64];unsigned rm_flg,st_flg;int ply_id;MN_WORK* rmthp;unsigned pad_ps;} sys_state={.mwalp=mwal_storage};
static ATR_WORK route_storage[256];static unsigned char route_table[256*256];
static struct {float grand[34];ATR_WORK* rutp;unsigned char* ruttp;int rut_n;int etc_n,flr_n;ATR_WORK* etcp;ATR_WORK* flrp;} rom_state={{0},route_storage,route_table,0};
#define rom (&rom_state)
static ATR_WORK terrain_etc[4096],terrain_flr[4096];
static ATR_WORK** recvx_stair(BH_PWORK* epw){return &((EN01_STORE*)epw)->stair;}
static BH_PWORK ene[256];
#define sys (&sys_state)
#define plp (&player_work)
static void unbound(const char* name){if(host.unbound)host.unbound(name);}
#define recvx_en01_unbound(name) unbound(name)
static int recvx_wpn_flg(int weapon){return host.weapon_flg?host.weapon_flg(weapon):0;}

/* Original routines the host has not bound. Each reports itself and takes the branch the
   original takes when its query finds nothing (no leg to cut, no stairs, no grab). */
#define EN01_UNBOUND(name,value) {unbound(name);return value;}
static int bhEne01_CutLeg(BH_PWORK* epw){(void)epw;EN01_UNBOUND("bhEne01_CutLeg",0)}
static int bhEne01_WormCheck(BH_PWORK* epw){if(epw->mdlver==21||epw->mdlver==22)unbound("bhEne01_WormCheck");return 0;} /* original: 0 unless the model owns a worm child */
static void bhEne01_FlyingCap(BH_PWORK* epw,int mode){(void)epw;(void)mode;unbound("bhEne01_FlyingCap");}
static void bhEne01_LinkFireEffect(BH_PWORK* epw,int no){(void)epw;(void)no;unbound("bhEne01_LinkFireEffect");}
static void bhEne_SetDFireEffect(BH_PWORK* epw,int joint,BT_WORK* table,int type){(void)epw;(void)joint;(void)table;(void)type;unbound("bhEne_SetDFireEffect");}
static void bhEne_SetSanEffect(BH_PWORK* epw,int joint,BT_WORK* table){(void)epw;(void)joint;(void)table;unbound("bhEne_SetSanEffect");}
static int bhEne01_ChgTextID(BH_PWORK* epw,int id){(void)epw;return id;}
static int ChechPlayEnemySe(int enemy,int se){return host.enemy_se_playing?host.enemy_se_playing((unsigned)enemy,se):0;}
static void RequestEnemySe(int enemy,NJS_POINT3* pos,int se){if(host.request_enemy_se)host.request_enemy_se((unsigned)enemy,&pos->x,se);}
static void npSetAllMatColor(void* obj,unsigned count,unsigned color){(void)obj;(void)count;(void)color;}
static int bhSetShadow(char* table,unsigned char* work,int a,float b,float c,float d){(void)table;(void)work;(void)a;(void)b;(void)c;(void)d;return 0;}
static void bhSetBloodPoolLnk(BH_PWORK* epw,NJS_POINT3* pos,int ay,BP_WORK* table,int pal){(void)ay;(void)pal;if(host.blood)host.blood((RECVX_EN01*)epw,"BloodPool",pal,&pos->x,ay,table);}
static void bhCheckL2Water(NJS_LINE* line,NJS_POINT3* out){(void)line;out->x=out->y=out->z=0;unbound("bhCheckL2Water");}
static void bhAddSpeed(BH_PWORK* epw,int r){epw->px-=epw->spd*njSin(epw->ay+r);epw->pz-=epw->spd*njCos(epw->ay+r);}
/* bhLinkBlood (effect.c) is an empty function in the original. */
static void bhLinkBlood(BH_PWORK* epw,int joint,NJS_POINT3* direction,float size){(void)epw;(void)joint;(void)direction;(void)size;}
/* bhSetEffectTb/bhSetEffParticle: the host owns the effect slots. Effects without a bound procedure return -1 (slot full). */
typedef struct {unsigned stflg,flg;void* txp[1];int tex_id;float px,py,pz,xn,yn,zn;int ct0,ct1,mode0,mode1;unsigned char* lkwkp;RECVX_EN01_EFFECT request;} O_WRK;
static O_WRK eff[512],particle_dummy;
static int bhSetEffectTb(EF_WORK* ef,NJS_POINT3* offset,unsigned char* lkp,int lkono){
    if(ef->id!=356&&host.effect_bound&&!host.effect_bound(ef->id)){unbound("bhEff");return -1;}
    int slot=0;while(slot<512&&eff[slot].flg)++slot;if(slot==512)return -1;
    memset(&eff[slot],0,sizeof eff[slot]);O_WRK* o=&eff[slot];o->request=(RECVX_EN01_EFFECT){ef->flg,ef->id,ef->type,ef->flr_no,ef->mdlver,ef->px,ef->py,ef->pz,ef->sx,ef->sy,ef->sz,ef->ay,ef->ax};
    if(ef->id==356){o->flg=1;return slot;}
    if(ef->id==362)return slot;
    if(host.spawn_effect)host.spawn_effect((RECVX_EN01*)lkp,&o->request,offset?&offset->x:0,lkono);
    return slot;
}
static void recvx_finish_effect(BH_PWORK* epw,int slot){if(slot<0)return;O_WRK* o=&eff[slot];o->request.xn=o->xn;o->request.yn=o->yn;o->request.zn=o->zn;o->request.texture=o->tex_id;o->request.stflg=o->stflg;if(host.spawn_effect)host.spawn_effect((RECVX_EN01*)epw,&o->request,0,0);}
static int bhCheckFloorNum(float y){for(int i=31;i>=0;--i)if(rom->grand[i]<=y)return i-2;return -2;}
static O_WRK* bhSetEffParticle(BH_PWORK* epw,int joint,NJS_POINT3* offset,NJS_POINT3* direction,unsigned color,int type){
    if(host.spawn_particle)host.spawn_particle((RECVX_EN01*)epw,joint,&offset->x,&direction->x,color,type);else unbound("bhSetEffParticle");
    return &particle_dummy;
}
/* bhEne00 services: the explosion goes to the host (effects, light, sound, blast damage); the checks the
   barrel never enables (flg 0x40/0x8/0x10) stay empty. */
static void bhCheckPlayer(BH_PWORK* ep){(void)ep;}
static void bhCheckEnemies(BH_PWORK* ep){(void)ep;}
static void bhCheckWall(BH_PWORK* ep){(void)ep;}
static void bhCalcModel(BH_PWORK* ep){(void)ep;}
static void bhSetExplosion(NJS_POINT3* pos){if(host.explode)host.explode(&pos->x);}
static int bhSetEffect(int effno,POINT* pnt,unsigned char* lkp,int lkono){(void)pnt;(void)lkp;(void)lkono;if(host.effect_bound&&!host.effect_bound((unsigned)effno))unbound("bhEff017 (barrel wall release on deactivation)");return -1;}
/* Zombie AI services. The wall queries and foot lock come from the host's collision/pose data. */
static ATR_WORK line_wall;static NJS_POINT3 line_normal;
static ATR_WORK* bhCollisionCheckLine(NJS_POINT3* from,NJS_POINT3* to){return host.line_hit&&host.line_hit(&from->x,&to->x,&line_normal.x,(unsigned char*)&line_wall)?&line_wall:0;}
static void bhGetHitCollisionNormal(NJS_POINT3* normal){*normal=line_normal;}
static ATR_WORK wall_dummy;
static int bhEne01_PoisonCheck(BH_PWORK* epw){(void)epw;return 0;} /* no poison state is bound */
static ATR_WORK* bhCheckWallType(NJS_POINT3* pos,unsigned flg,float ar,float ah){(void)flg;return (host.wall_probe&&host.wall_probe(&pos->x,ar,ah,flg))?&wall_dummy:0;}
static int bhCheckWallEx(BH_PWORK* epw,NJS_POINT3* ps,NJS_POINT3* ops,float ar,float ah){(void)epw;return host.wall_move?host.wall_move(&ps->x,&ops->x,ar,ah,epw->flg,epw->stflg):0;}
static void bhCalcFixOffset(BH_PWORK* epw,const char* tree,NJS_POINT3* offset,NJS_POINT3* out){
    static const NJS_POINT3 zero={0,0,0};if(!offset)offset=(NJS_POINT3*)&zero;out->x=out->y=out->z=0;
    if(host.fix_offset)host.fix_offset((RECVX_EN01*)epw,tree,&offset->x,&out->x);
}
static void bhEne_SetVibration(int no){(void)no;} /* pad vibration is not routed */
static float njDistanceP2P(NJS_POINT3* a,NJS_POINT3* b){const float x=a->x-b->x,y=a->y-b->y,z=a->z-b->z;return sqrtf(x*x+y*y+z*z);}
static void CallPlayerVoice(int no){if(host.player_voice)host.player_voice(no);else unbound("CallPlayerVoice");}
static int npCollisionCheckSC(NJS_SPHERE* sphere,NJS_CAPSULE* capsule){float a[3],b[3];const float radius=sphere->r+capsule->r;return recvx_segment_closest(&sphere->c.x,&sphere->c.x,&capsule->c1.x,&capsule->c2.x,a,b)<=radius*radius;}
static int npCollisionCheckCCEx(NJS_CAPSULE* a,NJS_CAPSULE* b,NJS_POINT3* out){float p[3],q[3];const float radius=a->r+b->r;const float distance=recvx_segment_closest(&a->c1.x,&a->c2.x,&b->c1.x,&b->c2.x,p,q);out->x=p[0];out->y=p[1];out->z=p[2];return distance<=radius*radius;}
static float njDistanceP2L(NJS_POINT3* p,NJS_LINE* line,NJS_POINT3* out){const float length=line->vx*line->vx+line->vy*line->vy+line->vz*line->vz;const float t=length?((p->x-line->px)*line->vx+(p->y-line->py)*line->vy+(p->z-line->pz)*line->vz)/length:0;out->x=line->px+t*line->vx;out->y=line->py+t*line->vy;out->z=line->pz+t*line->vz;return njScalor(&(NJS_VECTOR){p->x-out->x,p->y-out->y,p->z-out->z});}

static void bhCheckDansa(BH_PWORK* epw){(void)epw;} /* step/ledge adjustment of the wall check: not ported */
static int bhDGCdirCheck(NJS_VECTOR* v,int ay){(void)v;(void)ay;EN01_UNBOUND("bhDGCdirCheck",0)}
static int bhSetMotion(BH_PWORK* epw,int add,int mode,void* data){(void)epw;(void)add;(void)mode;(void)data;return 0;}
static void bhEne01_CalcEnemy(BH_PWORK* epw){(void)epw;}
static int njArcTan2(float y,float x){(void)y;(void)x;return (int)(10430.381f*atan2f(y,x));}

#include "en01_damage.inc"
static int en04_contact_flg,en04_atari_flg;
#include "en04_damage.inc"
/* HP selection remains the original rand() % 16 call in en01/en04 Init. */

static EN01_STORE* store(RECVX_EN01* work){return (EN01_STORE*)work;}
void recvx_en01_set_host(const RECVX_EN01_HOST* h){host=*h;}
void recvx_en01_set_frame_counts(const unsigned* counts,unsigned count){memset(motion_table,0,sizeof motion_table);for(unsigned i=0;i<count&&i<1024;++i)motion_table[i].frm_num=counts[i];}
void recvx_en01_set_neighbours(const RECVX_EN01* const* list,unsigned count){neighbours=list;neighbour_count=count;}
void recvx_en01_set_ai(int enabled){ai_enabled=enabled;}
void recvx_en01_set_routes(const unsigned char* records,unsigned count,const unsigned char* table){
    if(count>256)count=256;memset(route_storage,0,sizeof route_storage);if(records)memcpy(route_storage,records,count*sizeof(ATR_WORK));memset(route_table,0xff,sizeof route_table);if(table)memcpy(route_table,table,(size_t)count*count);rom->rut_n=(int)count;
}
void recvx_en01_set_terrain(const unsigned char* etc,unsigned etc_count,const unsigned char* flr,unsigned flr_count){
    if(etc_count>4096)etc_count=4096;if(flr_count>4096)flr_count=4096;
    memset(terrain_etc,0,sizeof terrain_etc);memset(terrain_flr,0,sizeof terrain_flr);
    if(etc)memcpy(terrain_etc,etc,etc_count*sizeof(ATR_WORK));if(flr)memcpy(terrain_flr,flr,flr_count*sizeof(ATR_WORK));
    rom->etcp=terrain_etc;rom->etc_n=etc_count;rom->flrp=terrain_flr;rom->flr_n=flr_count;
}
/* The player work is persistent: the grab routines (NG00..02, PlyDG00/01, PlayerLink) own its mode, motion, position and
   flags while mode0 is 4 or 6; the host only refreshes what the original reads from the live player. */
void recvx_en01_set_player(const RECVX_EN01_PLAYER* p){
    const int grabbed=player_work.mode0==4||player_work.mode0==6;
    if(!player_ready){player_ready=1;player_work.mode0=1;player_work.flg=8;player_work.ar=3.5f;player_work.ah=16.5f;player_work.mnwP=player_own_table;player_work.mnwPb=player_own_table;player_work.mtn_no=42;}
    player_work.flg=(player_work.flg&~2u)|(p->flg&2u);
    player_work.exp0=player_exp0;player_work.exp1=(unsigned char*)player_exp1;player_work.mtx=(float(*)[16])player_work.mtxbuf;
    if(!grabbed){player_work.px=p->px;player_work.pz=p->pz;player_work.ay=p->ay;}
    player_work.py=p->py;player_work.hp=p->hp;player_work.flr_no=(char)p->flr_no;player_work.at_flg=p->at_flg;player_work.way=(short)p->way;
    player_work.stflg=(player_work.stflg&~0x80000000u)|(p->stflg&0x80000000u);sys->pad_ps=p->pad_ps;
    player_work.mlwP=&player_ml;player_ml.owP=player_ow;player_ml.obj_num=32;
    {const unsigned count=p->joint_count>32?32:p->joint_count;for(unsigned i=0;i<count;++i)memcpy(player_ow[i].mtx,p->joints+i*16,sizeof player_ow[i].mtx);
     if(!count){memset(player_ow[0].mtx,0,sizeof player_ow[0].mtx);player_ow[0].mtx[12]=p->px;player_ow[0].mtx[13]=p->py;player_ow[0].mtx[14]=p->pz;}}
    sys->ply_id=p->player_id;player_pos[0]=p->px;player_pos[1]=p->py;player_pos[2]=p->pz;
}
/* Frame step of the grabbed player's clip (bhControlPlayer's bhSetMotion) once per world tick. */
void recvx_en01_player_tick(void){
    if((player_work.mode0==4||player_work.mode0==6)&&player_work.mnwP==motion_table){
        const int total=(int)motion_table[player_work.mtn_no<1024?player_work.mtn_no:0].frm_num<<16;
        if(total>0){player_work.frm_no+=player_work.mtn_add;if(player_work.frm_no<0)player_work.frm_no+=total;else if(player_work.frm_no>=total)player_work.frm_no-=total;}
    }
}
void recvx_en01_player_state(RECVX_EN01_PLAYER_STATE* out){
    out->mode0=player_work.mode0;out->mode1=player_work.mode1;out->mode2=player_work.mode2;out->mode3=player_work.mode3;
    out->flg=player_work.flg;out->stflg=player_work.stflg;out->hp=player_work.hp;out->px=player_work.px;out->py=player_work.py;out->pz=player_work.pz;out->ay=player_work.ay;
    out->mtn_no=player_work.mtn_no;out->frm_no=player_work.frm_no;out->enemy_clip=player_work.mnwP==motion_table;out->mtn_add=player_work.mtn_add;
}
void recvx_en01_player_reset(void){memset(&player_work,0,sizeof player_work);memset(player_exp1,0,sizeof player_exp1);memset(player_exp0,0,sizeof player_exp0);player_ready=0;}
static void bind(EN01_STORE* s){if(s->has_child){s->w.exp1=(unsigned char*)&s->child;s->child.exp0=s->cexp0;s->child.mnwP=motion_table;s->child.mnwPb=motion_table;s->child.mlwP=&s->child.mdl[0];s->child.lkwkp=(unsigned char*)&s->w;}
    s->w.exp0=s->exp0;s->w.mlwP=&s->w.mdl[0];s->w.mdl[0].owP=s->ow;s->w.mdl[0].objP=s->obj;s->w.mbp[0]=s->obj;s->w.mbp[1]=s->obj;s->w.mnwP=s->dog?dog_motion_table:motion_table;s->w.mnwPb=s->w.mnwP;}
void recvx_en01_init(RECVX_EN01* work,unsigned type,unsigned mdlver,int hard,unsigned joints,int flr_no,float px,float py,float pz,int ay,const RECVX_EN01* const* previous,unsigned previous_count){
    EN01_STORE* s=store(work);memset(s,0,sizeof *s);bind(s);
    BH_PWORK* epw=&s->w;epw->id=1;epw->type=(unsigned short)type;epw->mdlver=(unsigned char)mdlver;epw->flr_no=(char)flr_no;epw->px=px;epw->py=py;epw->pz=pz;epw->ay=ay;
    epw->flg=1;epw->mdl[0].obj_num=joints;epw->mdl[0].owP=s->ow;sys->gm_mode=hard?2:0;
    sys->ewk_n=0;for(unsigned i=0;i<previous_count&&i<255;++i){const EN01_STORE* p=(const EN01_STORE*)previous[i];ene[sys->ewk_n]=p->w;ene[sys->ewk_n].exp0=(unsigned char*)p->exp0;++sys->ewk_n;}
    bhEne01_Init(epw);
    /* The original runs the lower body and a linked upper-body work (exp1, motion + 200). The host actor is one
       entity, but the routines address both works, so the upper work is kept as real state. */
    s->has_child=1;bind(s);s->child.flg=0x81;s->child.id=1;s->child.type=epw->type;s->child.mtn_no=epw->mtn_no+200;s->child.mtn_add=epw->mtn_add;
    *(int*)&s->child.mode0=*(int*)&epw->mode0;s->child.hp=epw->hp;
}
void recvx_en01_set_joints(RECVX_EN01* work,const float* matrices,unsigned count){EN01_STORE* s=store(work);bind(s);if(count>32)count=32;for(unsigned i=0;i<count;++i)memcpy(s->ow[i].mtx,matrices+i*16,sizeof s->ow[i].mtx);}
void recvx_en01_set_enemy_no(RECVX_EN01* work,int number){store(work)->enemy_no=number;}
void recvx_en01_set_hp(RECVX_EN01* work,int hp){store(work)->w.hp=hp;}
void recvx_en01_set_transform(RECVX_EN01* work,float px,float py,float pz,int ay){BH_PWORK* w=&store(work)->w;w->px=px;w->py=py;w->pz=pz;w->ay=ay;}
int recvx_en01_capsule_hit(RECVX_EN01* work,const float start[3],const float end[3],float radius,float hit[3]){NJS_CAPSULE cap={{start[0],start[1],start[2]},{end[0],end[1],end[2]},radius};NJS_POINT3 pos;EN01_STORE* s=store(work);bind(s);const int joint=bhCheckCapCol2Capsule(&s->w,&cap,&pos);if(joint>=0){hit[0]=pos.x;hit[1]=pos.y;hit[2]=pos.z;}return joint;}
void recvx_en01_hit(RECVX_EN01* work,int weapon,int joint,int damage,unsigned comb_flg_bits,const float* position,const float* direction){
    BH_PWORK* pp=&store(work)->w;bind(store(work));
    pp->flg|=0x4;pp->dpx=position[0];pp->dpy=position[1];pp->dpz=position[2];pp->dvx=direction[0];pp->dvy=direction[1];pp->dvz=direction[2];
    pp->wpnr_no=weapon;pp->djnt_no=joint;pp->dam[joint]+=damage;pp->comb_flg|=comb_flg_bits;
}
/* bhControlEnemy's `ene` array: the other en01 works; this one is masked so `ep != epw` holds. */
static void ene_refresh(RECVX_EN01* self){sys->ewk_n=0;for(unsigned i=0;i<neighbour_count&&i<255;++i){const EN01_STORE* p=(const EN01_STORE*)neighbours[i];ene[sys->ewk_n]=p->w;ene[sys->ewk_n].exp0=(unsigned char*)p->exp0;if((const void*)p==(const void*)self)ene[sys->ewk_n].flg=0;++sys->ewk_n;}}
/* bhEne01_CollCheckWall (en01.c): line of sight to the player (EXP0 0x70) and the wall correction of the walking
   pose (t = EXP0 & 0xF is 1 for idle/walk, 2 for the run). The limb sub-checks through bhEne_CalcPartsPos and the
   crawler branches are not ported. */
static void bhEne01_CollCheckWall(BH_PWORK* epw){
    NJS_POINT3 ps,ops;ps.x=plp->px;ps.y=0.5f+plp->py;ps.z=plp->pz;
    EXP0_I(0x70)=(host.line_blocked&&host.line_blocked(&epw->px,&ps.x))?1:0;
    {const int t=EXP0_I(0x40)&0xF;const int crawler=(epw->type==2||epw->type==3||epw->type==9);
     if((t==1&&(epw->flg&0x10)&&!crawler)||t==2){
        ps.x=epw->px+epw->aox;ps.y=epw->py+epw->aoy;ps.z=epw->pz+epw->aoz;
        ops.x=epw->pxb+epw->aox;ops.y=epw->pyb+epw->aoy;ops.z=epw->pzb+epw->aoz;
        bhCheckWallEx(epw,&ps,&ops,epw->ar,epw->ah);
        epw->px+=(ps.x-(epw->px+epw->aox));epw->py+=(ps.y-(epw->py+epw->aoy));epw->pz+=(ps.z-(epw->pz+epw->aoz));
     }else if(t!=1&&t!=2)unbound("bhEne01_CollCheckWall (limb/crawler branches)");}
}
/* bhEne01_SearchNeck, child half (the upper-body work turns object 11 towards the player). The parent and child share one
   EXP0 here, so EPP_EXP0/EXP0 are the same block. */
static void bhEne01_NeckStep(EN01_STORE* s){
    BH_PWORK* epw=&s->w;NJS_CNK_OBJECT* obj=&epw->mlwP->objP[11];
    if(epw->mode0>=5)return;
    if(EXP0_I(0x40)&0x1000){s->obj[8].ang[1]=epw->wax;obj->ang[1]=EXP0_I(0x50);}
    else if((EXP0_I(0x40)&0x800)&&(EXP0_UC(0x28)&0x40)&&EXP0_F(0x54)<35.0f){obj->ang[1]=EXP0_I(0x50);bhEne01_RotNeck(epw,11,plp,5);EXP0_I(0x50)=obj->ang[1];}
    else{
        if(EXP0_I(0x50)!=0){
            if(EXP0_I(0x50)<32768){EXP0_I(0x50)-=910;if(EXP0_I(0x50)<0)EXP0_I(0x50)=0;}
            else{EXP0_I(0x50)+=910;if(EXP0_I(0x50)>=65536)EXP0_I(0x50)=0;}
        }
        obj->ang[1]=EXP0_I(0x50);
    }
    s->neck_angle=obj->ang[1];
}
void recvx_en01_tick(RECVX_EN01* work){
    EN01_STORE* s=store(work);BH_PWORK* epw=&s->w;bind(s);sys->enow=s->enemy_no;
    /* bhControlEnemy: pxb = px before the enemy procedure. */
    epw->pxb=epw->px;epw->pyb=epw->py;epw->pzb=epw->pz;epw->axb=epw->ax;epw->ayb=epw->ay;epw->azb=epw->az;
    /* bhEne01: DmgCheck, MainLoop's mode0 dispatch, the flag/counter clean-up, then bhSetMotion. */
    ene_refresh(work);
    bhEne01_DmgCheck(epw);
    if(epw->mode0>0&&!(epw->flg&0x80)&&(epw->type==1||epw->type==3||epw->type==7)&&!(EXP0_I(64)&0x10000)){bhEne01_LinkFireEffect(epw,rand()%4);EXP0_I(64)|=0x10000;} /* bhEne01_MainLoop */
    switch(epw->mode0){
    case 1:
        if(ai_enabled)bhEne01_Move(epw);
        else{ /* AI off: only the knocked-down recovery (MV04) runs; EXP0 0x54 is the player distance Move computes */
            const float dx=player_pos[0]-epw->px,dy=player_pos[1]-epw->py,dz=player_pos[2]-epw->pz;EXP0_F(0x54)=sqrtf(dx*dx+dy*dy+dz*dz);
            if(epw->mode2==4)bhEne01_MV04(epw);}
        break;
    case 2:bhEne01_Nage(epw);break;
    case 3:bhEne01_Damage(epw);break;
    case 4:bhEne01_Die(epw);break;
    case 0:break;
    default:unbound("bhEne_Event");break;
    }
    bhEne01_PlayerControl(plp,epw); /* bhEne01_MainLoop: the grabbed player's reaction state */
    if(epw->flg&0x4)epw->flg&=~0x4;
    if(EXP0_UC(47)>0)EXP0_UC(47)--;
    if(EXP0_I(48)>0)EXP0_I(48)--;
    if(EXP0_I(52)>0)EXP0_I(52)--;
    bhEne01_Flinch(epw); /* bhEne01_SetMtn: damage flinch of object 8 */
    s->flinch=flinch_valid;s->flinch_ang[0]=flinch_obj.ang[0];s->flinch_ang[1]=flinch_obj.ang[1];
    bhEne01_NeckStep(s);
    s->pose_frame=epw->frm_no; /* bhSetMotion poses at the frame before it advances */
    {const int frm=epw->frm_no/65536;
     bhEne01_CheckMtnTbl(epw,frm); /* bhEne01_SetMtn tail: foot lock moves px/pz, per-frame SE tables */
     {const int total=(int)motion_table[epw->mtn_no<1024?epw->mtn_no:0].frm_num<<16;
      if(total>0&&!(EXP0_I(0x40)&0x10)){epw->frm_no+=epw->mtn_add;int wrapped=0;if(epw->frm_no<0){epw->frm_no+=total;wrapped=-1;}else if(epw->frm_no>=total){epw->frm_no-=total;wrapped=1;}
       if(wrapped)epw->flg|=0x2000000;else epw->flg&=~0x2000000;}}}
    bhEne01_CollCheckWall(epw);
    if(EXP0_I(64)&0x80000)bhEne01_PlayerLink(plp,epw); /* bhEne01: the grabbed player follows the zombie */
    for(unsigned i=0;i<512;++i)if(eff[i].flg&&(BH_PWORK*)eff[i].lkwkp==epw)bhEff_Sub356(&eff[i]);
    bhEne_InitDamage(epw); /* bhControlEnemy runs it right after bhJumpEnemy */
}
void recvx_en01_state(const RECVX_EN01* work,RECVX_EN01_STATE* out){
    const EN01_STORE* s=(const EN01_STORE*)work;const BH_PWORK* w=&s->w;
    out->hp=w->hp;out->flg=w->flg;out->flg2=w->flg2;out->stflg=w->stflg;out->type=w->type;out->mdlver=w->mdlver;out->mtn_no=w->mtn_no;
    out->mode0=w->mode0;out->mode1=w->mode1;out->mode2=w->mode2;out->mode3=w->mode3;
    out->frm_no=w->frm_no;out->pose_frame=s->pose_frame;out->mtn_add=w->mtn_add;out->hokan_count=(int)w->hokan_count;out->ay=w->ay;
    out->px=w->px;out->py=w->py;out->pz=w->pz;
    out->exp40=*(const unsigned*)(s->exp0+0x40);out->exp44=*(const unsigned*)(s->exp0+0x44);
    out->neck_angle=s->neck_angle;out->upper_mtn=s->has_child?s->child.mtn_no:0;out->mdflg=w->mdflg;out->flinch=s->flinch;out->flinch_angle[0]=s->flinch_ang[0];out->flinch_angle[1]=s->flinch_ang[1];
    out->dead=(w->flg&2)!=0;out->untargetable=(w->flg&0x8)==0;
}
void recvx_en01_bomb(RECVX_EN01* work,const float* position,float radius,int damage_max,int damage_min,const float* contact,float distance){
    BH_PWORK* pp=&store(work)->w;bind(store(work));(void)distance;
    NJS_POINT3 pos={position[0],position[1],position[2]},ps={contact[0],contact[1],contact[2]};
    bhBombHit(pp,&pos,radius,damage_max,damage_min,ps); /* weapon.c bhCheckBombAtari: assignment of the blast hit */
}

/* ---- bhEne00: explosive barrel ---- */
void recvx_en00_reset(const float* ground,unsigned count){sys->mwal_n=0;memset(mwal_storage,0,sizeof mwal_storage);memset(rom->grand,0,sizeof rom->grand);for(unsigned i=0;i<count&&i<34;++i)rom->grand[i]=ground[i];}
void recvx_en00_init(RECVX_EN01* work,int flr_no,float px,float py,float pz,int ay){
    EN01_STORE* s=store(work);memset(s,0,sizeof *s);bind(s);BH_PWORK* ep=&s->w;
    ep->id=0;ep->flr_no=(char)flr_no;ep->px=px;ep->py=py;ep->pz=pz;ep->ay=ay;ep->flg=1;ep->mdl[0].obj_num=1;ep->mdl[0].owP=s->ow;
    for(int i=0;i<16;++i)s->ow[0].mtx[i]=(i%5==0)?1.f:0.f;s->ow[0].mtx[12]=px;s->ow[0].mtx[13]=py;s->ow[0].mtx[14]=pz;
    bhEne00(ep); /* mode 0: capsule, movable wall (sys->mwalp) and the effect-17 request */
}
void recvx_en00_tick(RECVX_EN01* work){
    EN01_STORE* s=store(work);BH_PWORK* ep=&s->w;bind(s);
    s->ow[0].mtx[12]=ep->px;s->ow[0].mtx[13]=ep->py;s->ow[0].mtx[14]=ep->pz;
    bhEne00(ep);
}
int recvx_en00_wall(const RECVX_EN01* work,RECVX_EN00_WALL* out){
    const EN01_STORE* s=(const EN01_STORE*)work;if(!(s->w.flg2&0x80000000u))return 0;const ATR_WORK* hp=&mwal_storage[s->w.ct0];
    out->flg=hp->flg;out->type=hp->type;out->attr=hp->attr;out->flr_no=hp->flr_no;out->px=hp->px;out->py=hp->py;out->pz=hp->pz;out->w=hp->w;out->h=hp->h;out->d=hp->d;return 1;
}

void recvx_en04_set_frame_counts(const unsigned* counts,unsigned count){memset(dog_motion_table,0,sizeof dog_motion_table);for(unsigned i=0;i<count&&i<1024;++i)dog_motion_table[i].frm_num=counts[i];}
void recvx_en04_init(RECVX_EN01* work,unsigned type,int hard,int floor,float x,float y,float z,int ay){
    EN01_STORE* s=store(work);memset(s,0,sizeof *s);s->dog=1;bind(s);BH_PWORK* epw=&s->w;
    epw->id=4;epw->type=type;epw->flr_no=floor;epw->px=x;epw->py=y;epw->pz=z;epw->ay=ay;
    epw->mdl[0].obj_num=20;epw->flg=1;sys->gm_mode=hard?2:0;
    bhEne04_Init(epw);epw->cpcl_no=17;

}
void recvx_en04_tick(RECVX_EN01* work){
    EN01_STORE* s=store(work);bind(s);BH_PWORK* epw=&s->w;sys->enow=s->enemy_no;
    epw->pxb=epw->px;epw->pyb=epw->py;epw->pzb=epw->pz;
    bhEne04_DmgChk(epw);
    if(epw->mode0==3){if(epw->mode2==1)bhEne04_DG01(epw);else bhEne04_DG00(epw);}
    else if(epw->mode0==4)bhEne04_DD00(epw);
    else if(epw->mode0==1&&epw->mode2==5)bhEne04_MV05(epw);
    else if(epw->mode0==1&&epw->mode2==4)bhEne04_MV04(epw);
    s->pose_frame=epw->frm_no;
    const int total=(int)epw->mnwP[epw->mtn_no].frm_num<<16;
    if(total>0){epw->frm_no+=epw->mtn_add;if(epw->frm_no>=total){epw->frm_no-=total;epw->flg|=0x2000000;}else epw->flg&=~0x2000000;}
    if(EXP0_I(0x14)>0)--EXP0_I(0x14);if(EXP0_I(0x18)>0)--EXP0_I(0x18);
    epw->flg&=~4;bhEne_InitDamage(epw);
}
void recvx_enemy_modes(RECVX_EN01* work,int mode0,int mode1,int mode2,int mode3,unsigned type){
    EN01_STORE* s=store(work);bind(s);BH_PWORK* w=&s->w;
    w->mode0=mode0;if(mode1>=0)w->mode1=mode1;w->mode2=mode2;if(mode3>=0)w->mode3=mode3;
    if(type!=~0u)w->type=(w->type&0xff00)|(type&0xff);
}
void recvx_enemy_release(RECVX_EN01* work,int mode0,int mode1,int mode2,int mode3,unsigned type){
    recvx_enemy_modes(work,mode0,mode1,mode2,mode3,type);EN01_STORE* s=store(work);BH_PWORK* w=&s->w;
    w->ct2&=~0xff;w->stflg&=~0x10000u;w->flg&=~0x200000u;
    if(s->has_child){s->child.mode0=1;s->child.type=w->type;}
}
void recvx_enemy_motion(RECVX_EN01* work,unsigned motion,int frame,int add){BH_PWORK* w=&store(work)->w;w->mtn_no=motion;w->frm_no=frame;w->mtn_add=add;}
