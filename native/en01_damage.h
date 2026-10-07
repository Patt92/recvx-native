#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Opaque storage for the original en01 BH_PWORK subset plus its EXP0 block.
#ifdef __cplusplus
typedef struct {alignas(16) unsigned char data[16384];} RECVX_EN01;
#else
typedef struct {_Alignas(16) unsigned char data[16384];} RECVX_EN01;
#endif

// Fields of the work the host mirrors into its entity each tick.
typedef struct {
    int hp;unsigned flg,flg2,stflg,type,mdlver,mtn_no;
    int mode0,mode1,mode2,mode3;
    int frm_no,pose_frame,mtn_add,hokan_count,ay;
    float px,py,pz;
    unsigned exp40,exp44;
    unsigned mdflg,upper_mtn; // upper_mtn: motion of the linked upper-body work (exp1)
    int dead,untargetable;
    int neck_angle; // object 11 Y angle (bhEne01_SearchNeck)
    int flinch,flinch_angle[2]; // bhEne01_SetMtn: object 8 X/Y angles while the damage flinch runs
} RECVX_EN01_STATE;

typedef struct {unsigned flg,id,type;int flr_no;unsigned mdlver;float px,py,pz,sx,sy,sz;int ay,ax;float xn,yn,zn;int texture;unsigned stflg;} RECVX_EN01_EFFECT;
// Host services the original routines call (bhEne01_SePlay, blood, bhSetBloodPoolLnk ...).
typedef struct {
    int (*weapon_flg)(int weapon);
    // Called with the original arguments for effects the host has not bound yet. `name` is the
    // original routine; the host records it so unported behaviour is reported, not hidden.
    void (*unbound)(const char* name);
    // sdfunc.c RequestEnemySe / ChechPlayEnemySe (sys->enow = enemy number).
    void (*request_enemy_se)(unsigned enemy,const float* position,int se);
    int (*enemy_se_playing)(unsigned enemy,int se);
    // bhEne_SetBlood*/bhLinkBlood/bhSetBloodPoolLnk effect requests; table is the original data.
    void (*blood)(RECVX_EN01* work,const char* kind,int joint,const float* offset,int param,const void* table);
    // bhSetEffectTb with lkp = enemy work: `offset` (3 floats or NULL) is relative to joint `node`.
    int (*effect_bound)(unsigned id);
    // bhSetExplosion (weapon.c) for the barrel.
    void (*explode)(const float* position);
    void (*spawn_effect)(RECVX_EN01* work,const RECVX_EN01_EFFECT* effect,const float* offset,int node);
    // bhSetEffParticle(epw, joint, offset, direction, color, PtclTbl type).
    // Zombie AI services: bhCheckWallType probe, bhCheckWallEx (ps moves from ops), bhCollisionCheckLine, bhCalcFixOffset.
    int (*wall_probe)(const float* position,float radius,float height,unsigned flg);
    int (*wall_move)(float* position,const float* previous,float radius,float height,unsigned flg,unsigned stflg);
    int (*line_blocked)(const float* from,const float* to);
    void (*fix_offset)(RECVX_EN01* work,const char* tree,const float* offset,float* out);
    void (*player_voice)(int no); // CallPlayerVoice
    void (*spawn_particle)(RECVX_EN01* work,int joint,const float* offset,const float* direction,unsigned color,int type);
    int (*line_hit)(const float* from,float* to,float* normal,unsigned char* record);
} RECVX_EN01_HOST;

typedef struct {int flr_no;unsigned flg,flg2,at_flg,stflg;int way;float px,py,pz;int ay,hp,player_id;unsigned pad_ps;const float* joints;unsigned joint_count;} RECVX_EN01_PLAYER;
typedef struct {int mode0,mode1,mode2,mode3;unsigned flg,stflg;int hp;float px,py,pz;int ay,mtn_no,frm_no,mtn_add,enemy_clip;} RECVX_EN01_PLAYER_STATE;

void recvx_en01_set_host(const RECVX_EN01_HOST* host);
// Original MN_WORK frame counts of the enemy motion library (mnwP[mtn].frm_num).
void recvx_en01_set_frame_counts(const unsigned* counts,unsigned count);
// bhControlEnemy's `ene` list (all en01 works in room order); call before recvx_en01_tick.
void recvx_en01_set_neighbours(const RECVX_EN01* const* list,unsigned count);
void recvx_en01_set_player(const RECVX_EN01_PLAYER* player);
// Zombie AI on/off (debug menu). Off: idle zombies stand; damage reactions and MV04 recovery still run.
void recvx_en01_set_ai(int enabled);
// Zombie grab/bite (EatCheck, Nage, PlyDG, PlayerLink). Off: EatCheck finds nothing, as before.
void recvx_en01_set_grab(int enabled);
void recvx_en01_set_terrain(const unsigned char* etc,unsigned etc_count,const unsigned char* flr,unsigned flr_count);
void recvx_en01_player_tick(void);
void recvx_en01_player_state(RECVX_EN01_PLAYER_STATE* state);
void recvx_en01_player_reset(void);
// rom->rutp (ATR_WORK records, 36 bytes each) and rom->ruttp (count*count bytes) used by bhCheckRoute.
void recvx_en01_set_routes(const unsigned char* records,unsigned count,const unsigned char* table);
// bhEne01_Init without the linked child actors; `joints` is mlwP->obj_num.
void recvx_en01_init(RECVX_EN01* work,unsigned type,unsigned mdlver,int hp_roll,int hard,unsigned joints,int flr_no,float px,float py,float pz,int ay,const RECVX_EN01* const* previous,unsigned previous_count);
// Joint matrices (mlwP->owP[i].mtx, 16 floats each) used for the nearest-joint rule of weapon.c.
void recvx_en01_set_joints(RECVX_EN01* work,const float* matrices,unsigned count);
void recvx_en01_set_hp(RECVX_EN01* work,int hp);
void recvx_en01_set_enemy_no(RECVX_EN01* work,int number); // sys->enow
void recvx_en01_set_transform(RECVX_EN01* work,float px,float py,float pz,int ay);
// Attack assignment of weapon.c bhCheckEnemies/bhSetGunFire for one enemy.
int recvx_en01_capsule_hit(RECVX_EN01* work,const float start[3],const float end[3],float radius,float hit[3]);
void recvx_en01_hit(RECVX_EN01* work,int weapon,int joint,int damage,unsigned comb_flg_bits,const float* hit_position,const float* direction);
// bhEne01: DmgCheck, mode0 dispatch (Damage/Die), counters, then bhSetMotion's frame advance.
// weapon.c bhCheckBombAtari hit assignment for one enemy the host found inside the blast.
void recvx_en01_bomb(RECVX_EN01* work,const float* position,float radius,int damage_max,int damage_min,const float* contact,float distance);
void recvx_en01_tick(RECVX_EN01* work);
void recvx_en04_set_frame_counts(const unsigned* counts,unsigned count);
void recvx_en04_init(RECVX_EN01* work,unsigned type,int hp_roll,int hard,int floor,float x,float y,float z,int ay);
void recvx_en04_tick(RECVX_EN01* work);
void recvx_enemy_modes(RECVX_EN01* work,int mode0,int mode1,int mode2,int mode3,unsigned type);
void recvx_enemy_release(RECVX_EN01* work,int mode0,int mode1,int mode2,int mode3,unsigned type);
void recvx_enemy_motion(RECVX_EN01* work,unsigned motion,int frame,int add);
// eneset.c bhEne00 (explosive barrel); the wall is sys->mwalp[ct0], the host mirrors it into its collision walls.
typedef struct {unsigned flg,type,attr;int flr_no;float px,py,pz,w,h,d;} RECVX_EN00_WALL;
void recvx_en00_reset(const float* ground,unsigned count);
void recvx_en00_init(RECVX_EN01* work,int flr_no,float px,float py,float pz,int ay);
void recvx_en00_tick(RECVX_EN01* work);
int recvx_en00_wall(const RECVX_EN01* work,RECVX_EN00_WALL* out);
void recvx_en01_state(const RECVX_EN01* work,RECVX_EN01_STATE* state);
#ifdef __cplusplus
}
#endif
