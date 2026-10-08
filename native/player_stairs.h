#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Original ATR_WORK etc record (types.h, 0x24 bytes) of a stair/ladder area.
typedef struct {unsigned char flg,type,id;signed char flr_no;unsigned attr;float px,py,pz,w,h,d;unsigned char prm0,prm1,prm2,prm3;} RECVX_STAIR_AREA;
// Host services for the original routines (bhGetObjMotion, owP world nodes, foot sounds).
typedef struct {
    void* user;
    int (*motion_root)(void* user,unsigned motion,int frame,float pos[3]); // key of object 0, 0 when the motion has none
    unsigned (*motion_frames)(void* user,unsigned motion);
    void (*node_position)(void* user,unsigned node,float out[3]);           // plp->mlwP->owP[node].mtx[12..14]
    // bhCheckFloorSound scans room flrp followed by bhObj001's mflrp; the
    // host receives its exact query operands rather than a synthesized floor.
    int (*floor_sound)(void* user,int floor,float x,float z,int special);
    void (*footstep)(void* user,int sound);                                    // CallPlayerFootStepSe
    // World position of `node` for motion/frame at (px,py,pz,ay): bhCalcFixOffset with the new objP pose.
    void (*pose_node)(void* user,unsigned motion,int frame,float px,float py,float pz,int ay,unsigned node,float out[3]);
    const float* grand;                                                       // rom->grand[32]
    int ply_id;
} RECVX_STAIR_HOST;
typedef struct {int active;float px,py,pz;int ay;unsigned motion;int frame;int floor;unsigned unsupported_services;} RECVX_STAIR_STATE;
// bhCheckExmAtari type 1: starts kdu/kdd (stairs, attr&1 == 0) or hsu/hsd (ladders, attr&1).
int recvx_stairs_begin(RECVX_STAIR_STATE* state,const RECVX_STAIR_AREA* area,const float* grand,float px,float py,float pz,int ay,int floor,int dmlvl,int wpntp);
// One original 30-Hz player frame: action routine, then bhSetMotion's frame advance.
int recvx_stairs_tick(RECVX_STAIR_STATE* state,const RECVX_STAIR_HOST* host);
#ifdef __cplusplus
}
#endif
