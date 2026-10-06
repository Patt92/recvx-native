#include "player_hair.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <string.h>
// Field-name compatible subsets; bhObjClpn and bhCalcHair are copied verbatim (player_hair.inc).
typedef struct {float mtx[16];} O_WORK;
typedef struct {float pos[3];int ang[3];} NJS_CNK_OBJECT;
typedef struct {NJS_CNK_OBJECT* objP;O_WORK* owP;} ML_WORK;
typedef struct {unsigned int flg2;int ax,ay,az;ML_WORK* mlwP;} BH_PWORK;
typedef struct {float x,y,z;unsigned int col;} NJS_POLYGON_VTX;
typedef struct {
    unsigned int flg;unsigned char mode0;int ct0,ct1,ct2,ct3;unsigned char* exp0;void* exp3;unsigned char* lkwkp;
    float xn,yn,zn,gpx,gpy,gpz,spd;ML_WORK* mlwP;NJS_POLYGON_VTX pv[4];
} O_WRK;
static unsigned char player_etc[0x5000]; // sys->pletcp
static struct {unsigned char* pletcp;} sys_work={player_etc};
#define sys (&sys_work)
static NJS_CNK_OBJECT player_objects[6];static O_WORK player_nodes[6];static ML_WORK player_model={player_objects,player_nodes};
static BH_PWORK player;static BH_PWORK* plp=&player;
static NJS_CNK_OBJECT hair_objects[4];static O_WORK hair_nodes[4];static ML_WORK hair_model={hair_objects,hair_nodes};
static O_WRK hair_work;
static void* bhGetFreeMemory(int size,int align){(void)size;(void)align;return player_etc;} // only plp-linked work is used
static void npSetMemoryL(unsigned int* p,int count,unsigned int value){int i;for(i=0;i<count;++i)p[i]=value;}
static void npSetMemory(void* p,int size,int value){memset(p,value,(size_t)size);}
#include "player_hair.inc"
int recvx_hair_tick(const RECVX_HAIR_INPUT* in,int angles[4][2]){
    int i,j,simulated;
    if(!hair_work.flg){hair_work.flg=0x81;hair_work.lkwkp=(unsigned char*)plp;hair_work.mlwP=&hair_model;player.mlwP=&player_model;} // bhSetObject(lkmtab) + lkwkp
    memcpy(player_nodes[5].mtx,in->head,sizeof in->head);
    for(i=0;i<6;++i)for(j=0;j<3;++j)player_objects[i].ang[j]=in->player_angles[i][j];
    player.ax=in->ax;player.ay=in->ay;player.az=in->az;if(in->reset)player.flg2|=0x2;
    hair_nodes[0].mtx[12]=in->root[0];hair_nodes[0].mtx[13]=in->root[1];hair_nodes[0].mtx[14]=in->root[2];
    simulated=hair_work.mode0==1;bhObjClpn(&hair_work);
    // The first call only runs case 0 (buffers), exactly like the original object task.
    for(i=0;i<4;++i){angles[i][0]=hair_objects[i].ang[0];angles[i][1]=hair_objects[i].ang[1];}
    return simulated;
}
