#include "door_core.h"
#include <math.h>
typedef RECVX_DOOR_STATE _door_wrk;
typedef RECVX_DOOR_NODE NJS_CNK_OBJECT;
typedef struct {void (*procP)(_door_wrk*);} _proc_wrk;
#include "door_structs.inc"
static int CompareSint32(int,int,int);
static int CompareFloat(float,int,float);
static void FadeProc1(_door_wrk*);
static void ViewProc1(_door_wrk*);static void ViewProc2(_door_wrk*);static void ViewProc3(_door_wrk*);static void ViewProc4(_door_wrk*);static void ViewProc5(_door_wrk*);static void ViewProc6(_door_wrk*);static void ViewProc7(_door_wrk*);
static const _proc_wrk ViewProcTbl[8]={{0},{ViewProc1},{ViewProc2},{ViewProc3},{ViewProc4},{ViewProc5},{ViewProc6},{ViewProc7}};
static void VectorMove(NJS_POINT3* pos,int yaw,int pitch,float speed){NJS_MATRIX matrix;njUnitMatrix(&matrix);njTranslate(&matrix,pos->x,pos->y,pos->z);njRotateY(&matrix,yaw);njRotateX(&matrix,pitch);njTranslate(&matrix,0,0,-speed);njGetTranslation(&matrix,pos);}
#include "door_procedures.inc"
size_t recvx_door_parameter_size(unsigned family,unsigned type){
    static const size_t fade[]={0,sizeof(FADEPROC_WORK)};
    static const size_t view[]={0,sizeof(VIEWPROC1_WORK),sizeof(VIEWPROC2_WORK),sizeof(VIEWPROC3_WORK),sizeof(VIEWPROC4_WORK),sizeof(VIEWPROC5_WORK),sizeof(VIEWPROC6_WORK),sizeof(VIEWPROC7_WORK)};
    static const size_t door[]={0,sizeof(DOORPROC1_WORK),sizeof(DOORPROC2_WORK),sizeof(DOORPROC3_WORK),sizeof(DOORPROC4_WORK),sizeof(DOORPROC5_WORK),0,sizeof(DOORPROC7_WORK)};
    return family==0?(type<2?fade[type]:0):family==1?(type<8?view[type]:0):(type<8?door[type]:0);
}
int recvx_door_configure(RECVX_DOOR_STATE* state,unsigned fade,unsigned view,unsigned door){
    static void (*const doors[8])(_door_wrk*)={0,DoorProc1,DoorProc2,DoorProc3,DoorProc4,DoorProc5,DoorProc6,DoorProc7};
    if(fade>1||view>7||door>7)return 0;
    state->fde_prcP=fade?FadeProc1:0;state->vew_prcP=ViewProcTbl[view].procP;state->dor_prcP=doors[door];return 1;
}
void recvx_door_step(RECVX_DOOR_STATE* state){if(state->status&0x40)return;if((state->status&1)&&state->fde_prcP)state->fde_prcP(state);if((state->status&2)&&state->vew_prcP)state->vew_prcP(state);if((state->status&4)&&state->dor_prcP)state->dor_prcP(state);}

int recvx_door_validate_view(unsigned type,const unsigned char* p,size_t bytes,unsigned depth){
 if(type>7||depth>8||bytes<recvx_door_parameter_size(1,type))return 0;
 int next=0;size_t size=0;
 switch(type){case 4:next=((const VIEWPROC4_WORK*)p)->chg_vew;size=sizeof(VIEWPROC4_WORK);break;case 5:next=((const VIEWPROC5_WORK*)p)->chg_vew;size=sizeof(VIEWPROC5_WORK);break;case 6:next=((const VIEWPROC6_WORK*)p)->chg_vew;size=sizeof(VIEWPROC6_WORK);break;default:return 1;}
 return next>=0&&next<=7&&recvx_door_validate_view((unsigned)next,p+size,bytes-size,depth+1);
}
