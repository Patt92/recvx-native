#pragma once
#include "player_stairs.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned short key[3];} RECVX_PUSH_ANGLE;
typedef struct {float key[3];} RECVX_PUSH_POSITION;
typedef struct {void* p[2];} RECVX_PUSH_CHANNEL;
typedef struct {unsigned flg;int obj_num,frm_num;void* datP;RECVX_PUSH_CHANNEL* md2P;unsigned short* atrP;} RECVX_PUSH_MOTION;
typedef struct {float pos[3];int ang[3];} RECVX_PUSH_POSE;
typedef struct {int index;unsigned flags;int aspd,param,floor;float x,y,z;} RECVX_PUSH_OBJECT;
typedef struct {void* user;void (*sound)(void*,int);void (*vibration)(void*,int,int);int (*set_effect)(void*,int,const float*,void*,int);int ply_id;} RECVX_PUSH_HOST;
typedef struct RECVX_PUSH_CORE RECVX_PUSH_CORE;
typedef struct {int active,phase,contact,wall,ay,motion,frame,mode0;unsigned flags,status,unsupported_services;float x,y,z,speed;} RECVX_PUSH_STATE;
RECVX_PUSH_CORE* recvx_push_create(const RECVX_STAIR_AREA*,unsigned,float,const float*,const RECVX_PUSH_OBJECT*,unsigned,const RECVX_PUSH_HOST*);
int recvx_push_probe(const RECVX_STAIR_AREA*,unsigned,float,float,float,float,float,float,unsigned,unsigned*);
void recvx_push_terrain(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA*,unsigned,const RECVX_STAIR_AREA*,unsigned);
void recvx_push_enabled(RECVX_PUSH_CORE*,unsigned);
void recvx_push_destroy(RECVX_PUSH_CORE*);
void recvx_push_motions(RECVX_PUSH_CORE*,RECVX_PUSH_MOTION*,unsigned,const float*,unsigned,RECVX_PUSH_POSE*,unsigned);
// Free locomotion collision, bhCPM2_act_wlk threshold and bhObj001 scheduler.
void recvx_push_contact(RECVX_PUSH_CORE*,RECVX_PUSH_STATE*,float,float,float,float,float,float,int,int,int,int,int);
// Run the original bhCheckWall once on the current player work after its
// movement state has selected the candidate position.
void recvx_push_wall_resolve(RECVX_PUSH_CORE*,RECVX_PUSH_STATE*,float,float,float,int,int,float,float);
// bhControlPlayerPad -> bhCPM2_act_psh -> bhSetMotion -> bhControlPlayer collision -> bhObj001.
void recvx_push_tick(RECVX_PUSH_CORE*,RECVX_PUSH_STATE*,int,int,int,int);
void recvx_push_update_object(RECVX_PUSH_CORE*,unsigned,float,float,float,int);
void recvx_push_update_walls(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA*,unsigned);
void recvx_push_object(RECVX_PUSH_CORE*,unsigned,float[3]);
unsigned recvx_push_walls(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA**);
// bhObj001's climbable crate variants (aspd 0/6) publish their paired
// dynamic ETC areas through sys->metcp.  They are consumed by bhCheckDansa
// and bhCheckExmAtari exactly like room ETC entries.
unsigned recvx_push_steps(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA**);
unsigned recvx_push_floors(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA**);
#ifdef __cplusplus
}
#endif
