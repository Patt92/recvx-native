#pragma once
#include "player_push_core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct RECVX_COMBAT_CORE RECVX_COMBAT_CORE;
typedef struct {float px,py,pz,vx,vy,vz,gx,gy,gz;unsigned at_flg;int wpn_no;float r,l,rn,rmax;int ax,ay;} RECVX_GUN_HIT;
// Original O_WRK fields an effect consumer needs; bhSetEffect/bhSetEffectTb fill them.
typedef struct {unsigned flg;unsigned id,type,mdlver;int flr_no;float px,py,pz,sx,sy,sz;int ax,ay,az;unsigned lkono;float lox,loy,loz;const float* matrix;} RECVX_COMBAT_EFFECT;
typedef struct {
 void* user;
 int (*search_bullet)(void*);
 int (*knife_hit)(void*,const RECVX_GUN_HIT*);
 int (*gun_hit)(void*,const RECVX_GUN_HIT*);
 int (*wall_hit)(void*,const float*,const float*,float,unsigned);
 void (*sound)(void*,const float*,unsigned);
 void (*muzzle)(void*,int,int,int,int,const float*);
 void (*unbound)(void*,const char*);
 int (*floor)(void*,int,float,float,float);
 void (*footstep)(void*,int,int,int);
 void (*vibration)(void*,int,int);
 void (*spark)(void*,const float*,const float*,unsigned,unsigned,int);
 int (*line)(void*,const float*,const float*,unsigned,float*,float*,void*);
 float (*ground)(void*,const float*);
 int (*water)(void*,const float*,void*);
 void (*effect)(void*,unsigned,const RECVX_COMBAT_EFFECT*);
 void (*light)(void*,const void*);
} RECVX_COMBAT_HOST;
typedef struct {unsigned index;float x,y,z,radius,height;} RECVX_COMBAT_ENEMY;
typedef struct {int active,mode,phase,motion,frame,pose_frame,yaw,pitch;unsigned flags,status,at_flags,hokan_count,shots,gm_flags,yk_ct;int target_right,target_left,arm_right,arm_left;} RECVX_COMBAT_STATE;
RECVX_COMBAT_CORE* recvx_combat_create(const RECVX_COMBAT_HOST*);
void recvx_combat_destroy(RECVX_COMBAT_CORE*);
void recvx_combat_motions(RECVX_COMBAT_CORE*,const RECVX_PUSH_MOTION*,unsigned,RECVX_PUSH_POSE*,unsigned);
void recvx_combat_seed(RECVX_COMBAT_CORE*,unsigned,int);
void recvx_combat_enemies(RECVX_COMBAT_CORE*,const RECVX_COMBAT_ENEMY*,unsigned);
void recvx_combat_nodes(RECVX_COMBAT_CORE*,const float*,unsigned);
void recvx_combat_after_model(RECVX_COMBAT_CORE*);
void recvx_combat_tick(RECVX_COMBAT_CORE*,RECVX_COMBAT_STATE*,unsigned,unsigned,unsigned*,int,int,float,float,float,int,unsigned*,unsigned*);
#ifdef __cplusplus
}
#endif
