#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Opaque storage for one original BH_PWORK plus its EXP0 block and node works.
#ifdef __cplusplus
typedef struct {alignas(16) unsigned char data[16384];} RECVX_ENEMY;
#else
typedef struct {_Alignas(16) unsigned char data[16384];} RECVX_ENEMY;
#endif

// The BH_PWORK fields the host mirrors into its entity after every tick.
typedef struct {
    int hp;unsigned flg,flg2,stflg,mdflg,type,mdlver;
    unsigned mtn_no;int mode0,mode1,mode2,mode3;
    int frm_no,pose_frame,mtn_add,hokan_count,hokan_rate;unsigned mtn_md;
    int ax,ay,az;float px,py,pz,spd;
    unsigned model_slot;       // mlwP - mdl: the mdl[] entry the procedure selected
    int dead,drawn,targetable; // flg 0x2 / mdflg 0x1 / flg 0x8
} RECVX_ENEMY_STATE;

// The movable collision wall an enemy owns in sys->mwalp (en25's crusher box).
typedef struct {unsigned flg,type,attr;int flr_no;float px,py,pz,w,h,d;} RECVX_ENEMY_WALL;

// One bhSetEffectTb request the original procedures issue.
typedef struct {unsigned flg,id,type;int flr_no;unsigned mdlver;float px,py,pz,sx,sy,sz;int ay,ax;} RECVX_ENEMY_EFFECT;

// The live player state the original routines read through plp.
typedef struct {
    int flr_no;unsigned flg,flg2,stflg;float px,py,pz;int ay,hp;
    int mode0,mode1,mode2,mode3;float car,cah;
    const float* joints;unsigned joint_count; // mlwP->owP[i].mtx
} RECVX_ENEMY_PLAYER;
// What the original routines wrote back into plp (damage, grab, crusher death).
typedef struct {int hp,mode0,mode1,mode2,mode3;unsigned flg,flg2,stflg;float px,py,pz;} RECVX_ENEMY_PLAYER_STATE;

typedef struct {
    // Reports an original routine the host has not bound yet, by its original
    // name. Never used to hide behaviour: the caller lists them as port gaps.
    void (*unbound)(const char* name);
    // sdfunc.c RequestEnemySe through bhEne_CallSE (sys->enow = enemy number).
    void (*request_enemy_se)(unsigned enemy,const float* position,int se);
    // sdfunc.c CallSystemSe (the parameter is ignored by the original).
    void (*system_se)(int se);
    // sdfunc.c StartVibrationEx(AtrbId, VibNo).
    void (*vibration)(int attribute,int number);
    // bhSetEffectTb; `id` selects the original effect procedure.
    void (*spawn_effect)(const RECVX_ENEMY_EFFECT* effect);
    int (*effect_bound)(unsigned id);
    // bhSetEffParticleMk2 through bhEne_SetBloodEffect4 (world position, no link).
    void (*spawn_particle)(const float* position,const float* direction,unsigned source_color,unsigned destination_color,int type);
    // bhSetBloodPoolLnk through bhEne_BloodPool; `table` is the original BP_WORK.
    void (*blood_pool)(const float* position,int angle,int palette,const void* table);
    // hitchk.c bhCheckWall needs the room walls; the host owns that geometry.
    // ps starts at the work position, ops is the position before the procedure.
    int (*wall_move)(float* position,const float* previous,float radius,float height,unsigned flg,unsigned stflg);
} RECVX_ENEMY_HOST;

void recvx_enemy_ai_set_host(const RECVX_ENEMY_HOST* host);
// Room state the original routines read: rom->grand (floor heights) and the
// movable-wall array sys->mwalp. Call once per room load, before any init.
void recvx_enemy_ai_reset(const float* ground,unsigned ground_count);
// Original MN_WORK frame counts of this enemy identity's motion library.
void recvx_enemy_ai_set_frame_counts(unsigned identity,const unsigned* counts,unsigned count);
void recvx_enemy_ai_set_player(const RECVX_ENEMY_PLAYER* player);
void recvx_enemy_ai_player_state(RECVX_ENEMY_PLAYER_STATE* state);

// True when bhJumpEnemy dispatches `identity` to one of the ported procedures.
int recvx_enemy_ai_supported(unsigned identity);
// bhSetEnemy + the enemy's own Init through bhJumpEnemy's first mode0 pass.
// `joints` is mlwP->obj_num, `models` is mdl_n (the original flipbook count).
void recvx_enemy_ai_init(RECVX_ENEMY* work,unsigned identity,unsigned type,unsigned mdlver,
                         unsigned joints,unsigned models,int flr_no,
                         float px,float py,float pz,int ay);
void recvx_enemy_ai_set_enemy_no(RECVX_ENEMY* work,int number); // sys->enow
void recvx_enemy_ai_set_transform(RECVX_ENEMY* work,float px,float py,float pz,int ay);
// Joint matrices of the pose the work had at the end of the previous tick.
void recvx_enemy_ai_set_joints(RECVX_ENEMY* work,const float* matrices,unsigned count);
// bhControlEnemy: one bhJumpEnemy[id] pass plus bhSetMotion's frame advance.
void recvx_enemy_ai_tick(RECVX_ENEMY* work);
void recvx_enemy_ai_state(const RECVX_ENEMY* work,RECVX_ENEMY_STATE* state);
int recvx_enemy_ai_wall(const RECVX_ENEMY* work,RECVX_ENEMY_WALL* out);
#ifdef __cplusplus
}
#endif
