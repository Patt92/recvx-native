#include "computer_core.h"
#include "ninja_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// Field-name compatible subset of the original system work and Ninja services
// used by sp_evt.c; the terminal routines themselves are copied verbatim.
#define TRUE 1
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-non-prototype" // bhControlComEvtScript(&com_md1) mirrors the original K&R-style prototype
#endif
typedef struct {int dummy;} NJS_TEXNAME;
typedef struct {NJS_TEXNAME* textures;int nbTexture;} NJS_TEXLIST;
typedef struct {float px,py,pz,vx,vy,vz;} NJS_LINE;
typedef struct {float x,y;} NJS_POINT2;
typedef struct {unsigned color;} NJS_COLOR;
typedef struct {NJS_POINT2* p;NJS_COLOR* col;void* tex;int num;} NJS_POINT2COL;
typedef struct {int year,month,day,hour,minute,second;} SYS_RTC_DATE;
static struct {
    unsigned com_flg;unsigned short com_md0,com_md1;unsigned com_num;void* com_exp;
    unsigned pad_on,pad_ps,st_flg,cb_flg,sp_flg,gm_flg,ts_flg,bcl_ct;unsigned ev_flg[32];
    unsigned char *memp,*sbs_sp,*subtxp;float fade_pbk;
} sys_work;
static struct {struct {void* texP;} mdl;} rom_work;
#define sys (&sys_work)
#define rom (&rom_work)
static unsigned palbuf[4096];
static unsigned recvx_com_unsupported_character;
static RECVX_COM_FRAME* now;
static int draw_trans,draw_texture;
static unsigned draw_color;
static void bhSetScreenFade(float color,float frames){(void)color;(void)frames;}
static void bhStandPlayerMotion(void){if(now)now->stand_player=1;}
static void CallSystemSe(int type,int number){(void)type;if(now&&now->sound_count<16)now->sounds[now->sound_count++]=(unsigned)number;}
static void bhStFlg(unsigned* flg,unsigned bit){(void)flg;if(now&&now->flag_count<16)now->flags[now->flag_count++]=bit;}
static int bhDispMessage(float px,float py,float pri,int type,int index,int color,int count){
    (void)px;(void)pri;(void)type;(void)color;(void)count;
    if(now){now->message_visible=1;now->message_number=index;now->message_lines=(int)((434.0f-py)/30.0f+0.5f);}return 0;}
static void njMemCopy4(void* dst,const void* src,unsigned words){memmove(dst,src,words*4);}
static void npSetMemory(unsigned char* dst,unsigned size,unsigned char value){memset(dst,value,size);}
static void npSetMemoryL(unsigned* dst,unsigned words,unsigned value){unsigned i;for(i=0;i<words;++i)dst[i]=value;}
static int bhSetMemPvpTexture(NJS_TEXLIST* list,unsigned char* pack,int flag){(void)list;(void)pack;(void)flag;return 3;} // SYSTEM entry 2: background, window/font/keyboard, icons
static void njTextureFilterMode(int mode){(void)mode;}
static void njColorBlendingMode(int target,int mode){(void)target;(void)mode;}
static int syRtcInit(void){return 0;}
static void syRtcFinish(void){}
static void syRtcGetDate(SYS_RTC_DATE* date){time_t t=time(NULL);struct tm* local=localtime(&t);date->hour=local->tm_hour;date->minute=local->tm_min;date->second=local->tm_sec;}
static void njReleaseTexture(NJS_TEXLIST* list){(void)list;}
static void bhGarbageTexture(void* list,int count){(void)list;(void)count;}
static void bhCopyMainmem2Texmem(void* list){(void)list;}
static void njSetTexture(NJS_TEXLIST* list){(void)list;}
static void njUserClipping(int mode,NJS_POINT2* area){(void)mode;(void)area;}
static void njQuadTextureStart(int trans){draw_trans=trans;}
static void njQuadTextureEnd(void){}
static void njSetQuadTexture(int texture,unsigned color){draw_texture=texture;draw_color=color;}
static RECVX_COM_DRAW* next_draw(void){return now&&now->draw_count<RECVX_COM_MAX_DRAW?&now->draw[now->draw_count++]:NULL;}
static void njDrawPolygon2D(NJS_POINT2COL* polygon,int count,float priority,int attribute){
    RECVX_COM_DRAW* d=next_draw();(void)count;(void)attribute;if(!d)return;
    d->kind=1;d->trans=1;d->texture=-1;d->x1=polygon->p[0].x;d->y1=polygon->p[0].y;d->x2=polygon->p[2].x;d->y2=polygon->p[2].y;
    d->u1=d->v1=d->u2=d->v2=0;d->z=priority;d->color=polygon->col[0].color;
}
struct QUAD;
static void njDrawQuadTexture(struct QUAD* q,float z);
static void* bhGetFreeMemory(unsigned size,unsigned align);
#include "computer.inc"
static COM_EVT_WORK com_work;
static void* bhGetFreeMemory(unsigned size,unsigned align){(void)size;(void)align;return &com_work;}
static void njDrawQuadTexture(struct QUAD* q,float z){
    RECVX_COM_DRAW* d=next_draw();if(!d)return;
    d->kind=0;d->trans=draw_trans;d->texture=draw_texture;d->x1=q->x1;d->y1=q->y1;d->x2=q->x2;d->y2=q->y2;d->u1=q->u1;d->v1=q->v1;d->u2=q->u2;d->v2=q->v2;d->z=z;d->color=draw_color;
}
void recvx_com_begin(unsigned number,const RECVX_COM_SYSTEM* system){
    memset(&sys_work,0,sizeof sys_work);sys->st_flg=system->st_flg;sys->cb_flg=system->cb_flg;sys->sp_flg=system->sp_flg;
    sys->com_num=number;recvx_com_unsupported_character=0;
    // bhSysCall: st_flg |= 0x4000000, bhKeepSpEvtComputer, mn_mode0 = 7 (com_md0/com_md1 = 0), sp_flg = 0x30.
    sys->st_flg|=0x4000000;bhKeepSpEvtComputer();sys->com_md0=0;sys->com_md1=0;sys->sp_flg=0;sys->sp_flg|=0x30;
}
int recvx_com_tick(const RECVX_COM_INPUT* input,RECVX_COM_FRAME* frame){
    const unsigned short before=sys->com_md0;
    memset(frame,0,sizeof *frame);now=frame;frame->active=1;
    sys->pad_on=input->pad_on;sys->pad_ps=input->pad_ps;
    bhControlSpEvtComputer();
    // bhSysCall leaves mode 7 once bhExitSpEvtComputer has cleared cb_flg 0x400000.
    if(before==2){frame->active=0;frame->finished=1;}
    frame->unsupported_character=recvx_com_unsupported_character;
    frame->system.st_flg=sys->st_flg;frame->system.cb_flg=sys->cb_flg;frame->system.sp_flg=sys->sp_flg;now=NULL;return frame->active;
}
