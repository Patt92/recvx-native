#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Opaque storage for the original en01 BH_PWORK subset plus its EXP0 block.
#ifdef __cplusplus
typedef struct {alignas(16) unsigned char data[8192];} RECVX_EN01;
#else
typedef struct {_Alignas(16) unsigned char data[8192];} RECVX_EN01;
#endif

// Fields of the work the host mirrors into its entity each tick.
typedef struct {
    int hp;unsigned flg,flg2,stflg,type,mdlver,mtn_no;
    int mode0,mode1,mode2,mode3;
    int frm_no,pose_frame,mtn_add,hokan_count,ay;
    float px,py,pz;
    unsigned exp40,exp44;
    int dead,untargetable;
} RECVX_EN01_STATE;

// Host services the original routines call (bhEne01_SePlay, blood, bhSetBloodPoolLnk ...).
typedef struct {
    int (*weapon_flg)(int weapon);
    // Called with the original arguments for effects the host has not bound yet. `name` is the
    // original routine; the host records it so unported behaviour is reported, not hidden.
    void (*unbound)(const char* name);
    void (*play_enemy_se)(RECVX_EN01* work,const float* position,int id);
    // bhEne_SetBlood*/bhLinkBlood/bhSetBloodPoolLnk effect requests; table is the original data.
    void (*blood)(RECVX_EN01* work,const char* kind,int joint,const float* offset,int param,const void* table);
} RECVX_EN01_HOST;

typedef struct {int flr_no;unsigned flg,flg2,at_flg,stflg;int way;float px,py,pz;} RECVX_EN01_PLAYER;

void recvx_en01_set_host(const RECVX_EN01_HOST* host);
// Original MN_WORK frame counts of the enemy motion library (mnwP[mtn].frm_num).
void recvx_en01_set_frame_counts(const unsigned* counts,unsigned count);
// bhControlEnemy's `ene` list (all en01 works in room order); call before recvx_en01_tick.
void recvx_en01_set_neighbours(const RECVX_EN01* const* list,unsigned count);
void recvx_en01_set_player(const RECVX_EN01_PLAYER* player);
// bhEne01_Init without the linked child actors; `joints` is mlwP->obj_num.
void recvx_en01_init(RECVX_EN01* work,unsigned type,unsigned mdlver,int hp_roll,int hard,unsigned joints,int flr_no,float px,float py,float pz,int ay,const RECVX_EN01* const* previous,unsigned previous_count);
// Joint matrices (mlwP->owP[i].mtx, 16 floats each) used for the nearest-joint rule of weapon.c.
void recvx_en01_set_joints(RECVX_EN01* work,const float* matrices,unsigned count);
void recvx_en01_set_hp(RECVX_EN01* work,int hp);
void recvx_en01_set_transform(RECVX_EN01* work,float px,float py,float pz,int ay);
// Attack assignment of weapon.c bhCheckEnemies/bhSetGunFire for one enemy.
void recvx_en01_hit(RECVX_EN01* work,int weapon,int joint,int damage,unsigned comb_flg_bits,const float* hit_position,const float* direction);
// bhEne01: DmgCheck, mode0 dispatch (Damage/Die), counters, then bhSetMotion's frame advance.
void recvx_en01_tick(RECVX_EN01* work);
void recvx_en01_state(const RECVX_EN01* work,RECVX_EN01_STATE* state);
#ifdef __cplusplus
}
#endif
