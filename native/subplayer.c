#include "subplayer.h"
#include "ninja_math.h"
#include <math.h>
#include <stdlib.h>
#include <stddef.h>
typedef RECVX_SUBPLAYER BH_PWORK;
typedef void (*Mv00Subpl_proc)(BH_PWORK*);
typedef void (*Mv01Subpl_proc)(BH_PWORK*);
static void bhSetMotion(BH_PWORK* work,int add,int mode,void* data){(void)add;(void)mode;(void)data;work->advanced=1;}
static void bhAddSpeed(BH_PWORK* work,int r){work->px-=work->spd*njSin(work->ay+r);work->pz-=work->spd*njCos(work->ay+r);}
#include "subplayer.inc"
int recvx_is_subplayer(unsigned identity){return identity<100&&subplayer_ids[identity];}
void recvx_subplayer_init(RECVX_SUBPLAYER* work){work->mode0=1;em60_init(work);}
int recvx_subplayer_tick(RECVX_SUBPLAYER* work){
    work->advanced=0;work->flg|=0x8100;work->flg&=~2u;
    if(work->ct2<0||work->ct2>=15)return 0;
    if(work->mode0==0){recvx_subplayer_init(work);return 1;}
    if(work->mode0==1&&work->mode2==0){subpl_mv00_tbl[work->ct2](work);return 1;}
    if(work->mode0==5||work->mode0==6){subpl_mv01_tbl[work->ct2](work);return 1;}
    return 0;
}
