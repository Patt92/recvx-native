#include "en01_damage.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "en01_damage_types.inc"

#define TRUE 1
typedef struct {BH_PWORK w;_Alignas(16) unsigned char exp0[0xB0];O_WORK ow[32];int frames_done;int pose_frame;int dead;} EN01_STORE;
_Static_assert(sizeof(EN01_STORE)<=sizeof(RECVX_EN01),"RECVX_EN01 storage too small");

static RECVX_EN01_HOST host;
static MN_WORK motion_table[1024];
static BH_PWORK player_work;
static int player_exp1;
static float player_pos[3];
static const RECVX_EN01* const* neighbours;static unsigned neighbour_count;
static struct {int gm_mode,enow,ewk_n;unsigned pad_on;} sys_state;
static BH_PWORK ene[256];
static int hp_roll_value;
#define sys (&sys_state)
#define plp (&player_work)
static void unbound(const char* name){if(host.unbound)host.unbound(name);}
#define recvx_en01_unbound(name) unbound(name)
static int recvx_wpn_flg(int weapon){return host.weapon_flg?host.weapon_flg(weapon):0;}

/* Original routines the host has not bound. Each reports itself and takes the branch the
   original takes when its query finds nothing (no leg to cut, no stairs, no grab). */
#define EN01_UNBOUND(name,value) {unbound(name);return value;}
static int bhEne01_CutLeg(BH_PWORK* epw){(void)epw;EN01_UNBOUND("bhEne01_CutLeg",0)}
static int bhEne01_DansaCheck(BH_PWORK* epw,int ang,float ofs_z){(void)epw;(void)ang;(void)ofs_z;EN01_UNBOUND("bhEne01_DansaCheck",0)}
static int bhEne01_EatCheck(BH_PWORK* epw,int rng,float dist,int mode){(void)epw;(void)rng;(void)dist;(void)mode;EN01_UNBOUND("bhEne01_EatCheck",0)}
static ATR_WORK* bhKaidanAtrCheck(BH_PWORK* epw,float ofs,int* idx){(void)epw;(void)ofs;(void)idx;unbound("bhKaidanAtrCheck");return 0;}
static void bhSetUseKaidanFlag(BH_PWORK* epw,ATR_WORK* hp,int idx){(void)epw;(void)hp;(void)idx;unbound("bhSetUseKaidanFlag");}
static void bhClrUseKaidanFlag(BH_PWORK* epw){(void)epw;}
static int bhEne01_ZulzulCheck(BH_PWORK* epw,int ang,float dist,NJS_POINT3* pos,int* out){(void)epw;(void)ang;(void)dist;(void)pos;(void)out;EN01_UNBOUND("bhEne01_ZulzulCheck",0)}
static int bhEne01_WormCheck(BH_PWORK* epw){if(epw->mdlver==21||epw->mdlver==22)unbound("bhEne01_WormCheck");return 0;} /* original: 0 unless the model owns a worm child */
static int bhEne01_GakeotiCheck(BH_PWORK* epw){(void)epw;EN01_UNBOUND("bhEne01_GakeotiCheck",0)}
static int ikou3(BH_PWORK* epw,NJS_POINT3* target,int ang){(void)epw;(void)target;(void)ang;EN01_UNBOUND("ikou3",0)}
static void bhEne01_FlyingCap(BH_PWORK* epw,int mode){(void)epw;(void)mode;unbound("bhEne01_FlyingCap");}
static void bhEne01_ExpHeadEffect2(BH_PWORK* epw,int mode){(void)epw;(void)mode;unbound("bhEne01_ExpHeadEffect2");}
static void bhEne01_LinkFireEffect(BH_PWORK* epw,int no){(void)epw;(void)no;unbound("bhEne01_LinkFireEffect");}
static void bhEne_SetSanEffect(BH_PWORK* epw,int joint,BT_WORK* table){(void)epw;(void)joint;(void)table;unbound("bhEne_SetSanEffect");}
static void bhEne_SetNikuhenEffect2(BH_PWORK* epw,int a,NJS_POINT3* pos,int b,int tex){(void)epw;(void)a;(void)pos;(void)b;(void)tex;unbound("bhEne_SetNikuhenEffect2");}
static int bhEne01_ChgTextID(BH_PWORK* epw,int id){(void)epw;return id;}
static void bhEne01_SePlay(BH_PWORK* epw,NJS_POINT3* pos,int no){if(host.play_enemy_se)host.play_enemy_se((RECVX_EN01*)epw,&pos->x,no);}
static void npSetAllMatColor(void* obj,unsigned count,unsigned color){(void)obj;(void)count;(void)color;}
static int bhSetShadow(char* table,unsigned char* work,int a,float b,float c,float d){(void)table;(void)work;(void)a;(void)b;(void)c;(void)d;return 0;}
static void bhEne_SetBlood(BH_PWORK* epw,unsigned char type,BT_WORK* table){if(host.blood)host.blood((RECVX_EN01*)epw,"SetBlood",epw->djnt_no,0,type,table);}
static void bhEne_SetBlood2(BH_PWORK* epw,int joint,NJS_POINT3* offset,int way){if(host.blood)host.blood((RECVX_EN01*)epw,"SetBlood2",joint,&offset->x,way,0);}
static void bhEne_SetBlood4(BH_PWORK* epw,int joint,NJS_POINT3* offset,int tex,int mode){if(host.blood)host.blood((RECVX_EN01*)epw,"SetBlood4",joint,&offset->x,mode,0);(void)tex;}
static void bhLinkBlood(BH_PWORK* epw,int joint,NJS_POINT3* direction,float size){if(host.blood)host.blood((RECVX_EN01*)epw,"LinkBlood",joint,&direction->x,(int)(size*1000),0);}
static void bhSetBloodPoolLnk(BH_PWORK* epw,NJS_POINT3* pos,int ay,BP_WORK* table,int pal){(void)ay;(void)pal;if(host.blood)host.blood((RECVX_EN01*)epw,"BloodPool",0,&pos->x,ay,table);}
static void bhCheckL2Water(NJS_LINE* line,NJS_POINT3* out){(void)line;out->x=out->y=out->z=0;unbound("bhCheckL2Water");}
static void bhAddSpeed(BH_PWORK* epw,int r){epw->px-=epw->spd*njSin(epw->ay+r);epw->pz-=epw->spd*njCos(epw->ay+r);}
static int bhDGCdirCheck(NJS_VECTOR* v,int ay){(void)v;(void)ay;EN01_UNBOUND("bhDGCdirCheck",0)}
static int bhSetMotion(BH_PWORK* epw,int add,int mode,void* data){(void)epw;(void)add;(void)mode;(void)data;return 0;}
static void bhEne01_CalcEnemy(BH_PWORK* epw){(void)epw;}
static int njArcTan2(float y,float x){(void)y;(void)x;return (int)(10430.381f*atan2f(y,x));}

#include "en01_damage.inc"
/* bhEne01_Init rolls rand() % 16 for the hp table; the host passes the roll it always used
   (entity index % 16) so room replays stay deterministic. */

/* bhEne01_Move: the player distance and EXP0 0x800, then the MoveMode2W dispatch. The original goes through
   MVType00/02 first, whose Brain/ActionModeCheck/EatCheck are the zombie AI and are not ported. */
static void bhEne01_Move(BH_PWORK* epw){
    EXP0_I(0x40)&=~0x800;
    if(epw->type!=10){
        const float dx=player_pos[0]-epw->px,dy=player_pos[1]-epw->py,dz=player_pos[2]-epw->pz;EXP0_F(0x54)=sqrtf(dx*dx+dy*dy+dz*dz);
        if(epw->mode2!=9)EXP0_I(0x40)|=0x800;
        unbound("bhEne01_MVType (Brain/ActionModeCheck AI)");
        {const unsigned index=epw->mode2+((epw->type==2||epw->type==3||epw->type==6||epw->type==7||epw->type==9)?10:0);bhEne01_MoveMode2W[index<17?index:0](epw);}
    }
}
static EN01_STORE* store(RECVX_EN01* work){return (EN01_STORE*)work;}
void recvx_en01_set_host(const RECVX_EN01_HOST* h){host=*h;}
void recvx_en01_set_frame_counts(const unsigned* counts,unsigned count){memset(motion_table,0,sizeof motion_table);for(unsigned i=0;i<count&&i<1024;++i)motion_table[i].frm_num=counts[i];}
void recvx_en01_set_neighbours(const RECVX_EN01* const* list,unsigned count){neighbours=list;neighbour_count=count;}
void recvx_en01_set_player(const RECVX_EN01_PLAYER* p){player_pos[0]=p->px;player_pos[1]=p->py;player_pos[2]=p->pz;player_work.flr_no=(char)p->flr_no;player_work.flg=p->flg;player_work.flg2=p->flg2;player_work.at_flg=p->at_flg;player_work.stflg=p->stflg;player_work.way=(short)p->way;player_work.exp1=(unsigned char*)&player_exp1;}

static void bind(EN01_STORE* s){s->w.exp0=s->exp0;s->w.mlwP=&s->w.mdl[0];s->w.mdl[0].owP=s->ow;s->w.mnwP=motion_table;s->w.mnwPb=motion_table;}
void recvx_en01_init(RECVX_EN01* work,unsigned type,unsigned mdlver,int hp_roll,int hard,unsigned joints,int flr_no,float px,float py,float pz,int ay,const RECVX_EN01* const* previous,unsigned previous_count){
    EN01_STORE* s=store(work);memset(s,0,sizeof *s);bind(s);
    BH_PWORK* epw=&s->w;epw->id=1;epw->type=(unsigned short)type;epw->mdlver=(unsigned char)mdlver;epw->flr_no=(char)flr_no;epw->px=px;epw->py=py;epw->pz=pz;epw->ay=ay;
    epw->flg=1;epw->mdl[0].obj_num=joints;epw->mdl[0].owP=s->ow;sys->gm_mode=hard?2:0;hp_roll_value=hp_roll&15;
    sys->ewk_n=0;for(unsigned i=0;i<previous_count&&i<255;++i){const EN01_STORE* p=(const EN01_STORE*)previous[i];ene[sys->ewk_n]=p->w;ene[sys->ewk_n].exp0=(unsigned char*)p->exp0;++sys->ewk_n;}
    bhEne01_Init(epw);
}
void recvx_en01_set_joints(RECVX_EN01* work,const float* matrices,unsigned count){EN01_STORE* s=store(work);bind(s);if(count>32)count=32;for(unsigned i=0;i<count;++i)memcpy(s->ow[i].mtx,matrices+i*16,sizeof s->ow[i].mtx);}
void recvx_en01_set_hp(RECVX_EN01* work,int hp){store(work)->w.hp=hp;}
void recvx_en01_set_transform(RECVX_EN01* work,float px,float py,float pz,int ay){BH_PWORK* w=&store(work)->w;w->px=px;w->py=py;w->pz=pz;w->ay=ay;}
void recvx_en01_hit(RECVX_EN01* work,int weapon,int joint,int damage,unsigned comb_flg_bits,const float* position,const float* direction){
    BH_PWORK* pp=&store(work)->w;bind(store(work));
    pp->flg|=0x4;pp->dpx=position[0];pp->dpy=position[1];pp->dpz=position[2];pp->dvx=direction[0];pp->dvy=direction[1];pp->dvz=direction[2];
    pp->wpnr_no=weapon;pp->djnt_no=joint;pp->dam[joint]+=damage;pp->comb_flg|=comb_flg_bits;
}
/* bhControlEnemy's `ene` array: the other en01 works; this one is masked so `ep != epw` holds. */
static void ene_refresh(RECVX_EN01* self){sys->ewk_n=0;for(unsigned i=0;i<neighbour_count&&i<255;++i){const EN01_STORE* p=(const EN01_STORE*)neighbours[i];ene[sys->ewk_n]=p->w;ene[sys->ewk_n].exp0=(unsigned char*)p->exp0;if((const void*)p==(const void*)self)ene[sys->ewk_n].flg=0;++sys->ewk_n;}}
void recvx_en01_tick(RECVX_EN01* work){
    EN01_STORE* s=store(work);BH_PWORK* epw=&s->w;bind(s);
    /* bhEne01: DmgCheck, MainLoop's mode0 dispatch, the flag/counter clean-up, then bhSetMotion. */
    ene_refresh(work);
    bhEne01_DmgCheck(epw);
    if(epw->mode0>0&&!(epw->flg&0x80)&&(epw->type==1||epw->type==3||epw->type==7)&&!(EXP0_I(64)&0x10000)){bhEne01_LinkFireEffect(epw,rand()%4);EXP0_I(64)|=0x10000;} /* bhEne01_MainLoop */
    switch(epw->mode0){
    case 1:bhEne01_Move(epw);break;
    case 3:bhEne01_Damage(epw);break;
    case 4:bhEne01_Die(epw);break;
    case 0:break;
    default:unbound(epw->mode0==2?"bhEne01_Nage":"bhEne_Event");break;
    }
    if(epw->flg&0x4)epw->flg&=~0x4;
    if(EXP0_UC(47)>0)EXP0_UC(47)--;
    if(EXP0_I(48)>0)EXP0_I(48)--;
    if(EXP0_I(52)>0)EXP0_I(52)--;
    s->pose_frame=epw->frm_no; /* bhSetMotion poses at the frame before it advances */
    {const int total=(int)motion_table[epw->mtn_no<1024?epw->mtn_no:0].frm_num<<16;
     if(total>0&&!(EXP0_I(0x40)&0x10)){epw->frm_no+=epw->mtn_add;if(epw->frm_no<0)epw->frm_no+=total;else if(epw->frm_no>=total)epw->frm_no-=total;}}
    bhEne_InitDamage(epw); /* bhControlEnemy runs it right after bhJumpEnemy */
}
void recvx_en01_state(const RECVX_EN01* work,RECVX_EN01_STATE* out){
    const EN01_STORE* s=(const EN01_STORE*)work;const BH_PWORK* w=&s->w;
    out->hp=w->hp;out->flg=w->flg;out->flg2=w->flg2;out->stflg=w->stflg;out->type=w->type;out->mdlver=w->mdlver;out->mtn_no=w->mtn_no;
    out->mode0=w->mode0;out->mode1=w->mode1;out->mode2=w->mode2;out->mode3=w->mode3;
    out->frm_no=w->frm_no;out->pose_frame=s->pose_frame;out->mtn_add=w->mtn_add;out->hokan_count=(int)w->hokan_count;out->ay=w->ay;
    out->px=w->px;out->py=w->py;out->pz=w->pz;
    out->exp40=*(const unsigned*)(s->exp0+0x40);out->exp44=*(const unsigned*)(s->exp0+0x44);
    out->dead=(w->flg&2)!=0;out->untargetable=(w->flg&0x8)==0;
}
