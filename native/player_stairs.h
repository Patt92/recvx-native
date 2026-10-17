#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Original ATR_WORK etc record (types.h, 0x24 bytes) of a stair/ladder area.
typedef struct {unsigned char flg,type,id;signed char flr_no;unsigned attr;float px,py,pz,w,h,d;unsigned char prm0,prm1,prm2,prm3;} RECVX_STAIR_AREA;
// One bhSetEffect/bhSetEffectTb request. The host owns the effect slots.
typedef struct {
    unsigned flg,id,type,mdlver;
    int flr_no;
    float px,py,pz;
    float sx,sy,sz;
    int ax,ay,az,lkono;
    int has_offset;
    float offset[3];
} RECVX_STAIR_EFFECT;
// Host services for the original routines (bhGetObjMotion, owP world nodes,
// foot sounds, bhCheckWater, bhSetEffect, vibration, kaidan flag, floors).
typedef struct {
    void* user;
    // bhGetObjMotion(ewP, 0, pos, ang): object 0's key at `frame` (16.16) of
    // `motion`. 0 when the motion has no such key; the caller then zeroes pos.
    int (*motion_root)(void* user,unsigned motion,int frame,float pos[3]);
    unsigned (*motion_frames)(void* user,unsigned motion);
    void (*node_position)(void* user,unsigned node,float out[3]);           // plp->mlwP->owP[node].mtx[12..14]
    // Motion.c:21 bhSetMotion, Knotenteil: SetMtn*(Hokan) schreibt plp->mlwP->objP
    // fuer `motion` am Bild `frame` (16.16, der Stand VOR dem Vorschub) mit
    // mtn_md und zieht bei laufender Mischung hokan_count ab (Motion.c:338/578/853).
    void (*set_motion_nodes)(void* user,unsigned motion,int frame,int mtn_md,int* hokan_count,int hokan_rate);
    // objP[node].pos/ang nach diesem Knotenteil.  bhCalcFixOffset (Motion.c:868)
    // multipliziert genau diese Knotenwerte entlang PlyLegRoute.  0, wenn der
    // Knoten im nativen Modell keinen Skriptknoten hat.
    int (*obj_node)(void* user,unsigned node,float pos[3],int ang[3]);
    // bhCheckFloorSound scans room flrp followed by bhObj001's mflrp; the
    // host receives its exact query operands rather than a synthesized floor.
    int (*floor_sound)(void* user,int floor,float x,float z,int special);
    // CallPlayerFootStepSeEx(list, type, flag, 0, &PlayerPos).
    void (*footstep)(void* user,int sound,int type,int flag);
    // bhCheckWater: 1 when the probe is inside an original type-3 water area
    // with prm0 == 1; `top` then receives fp->py + fp->h.
    int (*water)(void* user,const float position[3],float* top);
    void (*effect)(void* user,const RECVX_STAIR_EFFECT* effect);
    void (*vibration)(void* user,int type,int pattern);                      // StartVibrationEx
    void (*kaidan_flag)(void* user,int used);                                // bhSetUseKaidanFlag/bhClrUseKaidanFlag
    // bhCPM2_act_* case 0: EXP1_I(0) &= ~0x4; EXP1_I(0) |= 0x1E0 — the head control gives up
    // its look target and runs its four return paths (player.c:2891/3153/3360/3522/3948/4143).
    void (*release_head_look)(void* user);
    int (*floor_num)(void* user,float py);                                   // bhCheckFloorNum
    const float* grand;                                                      // rom->grand[32]
    int ply_id;
} RECVX_STAIR_HOST;
// Bits of unsupported_services: an original branch reached a service with no
// native counterpart. Reported by the runtime, never silently dropped.
#define RECVX_STAIR_UNBOUND_EFFECT      0x0001u // effect id outside the native effect pool
#define RECVX_STAIR_UNBOUND_EFFECT_OFF  0x0002u // bhSetEffect(108,...) reads past owP[0].mtx[16]
#define RECVX_STAIR_UNBOUND_VIBRATION   0x0004u // StartVibrationEx has no driver
#define RECVX_STAIR_UNBOUND_WATER       0x0008u // bhCheckWater found no native water area
#define RECVX_STAIR_UNBOUND_FOOTSTEP    0x0010u // CallPlayerFootStepSe list not playable
#define RECVX_STAIR_UNBOUND_ACTION_SE   0x0020u // CallPlayerActionSe 522/523 not playable
#define RECVX_STAIR_UNBOUND_MOTION      0x0040u // a requested motion is not in the native asset
#define RECVX_STAIR_UNBOUND_HOKAN       0x0080u // host supplies no bhSetMotion node part (set_motion_nodes)
#define RECVX_STAIR_UNBOUND_HEADWORK    0x0100u // EXP1_I(0) head-work flags have no native owner
#define RECVX_STAIR_UNBOUND_MTN_ATTR    0x0200u // per-frame mtn_attr has no native consumer
#define RECVX_STAIR_UNBOUND_FIXPOS      0x0400u // bhCalcFixOffset route node missing in the native skeleton
// The running action state. `status` is plp->stflg, `flg`/`flg2` are plp->flg/
// flg2; the runtime seeds flg/status from the live player work before begin and
// writes them back after every tick, so this stays a view of the one native
// player work rather than a second one.
typedef struct {
    int active;
    float px,py,pz;
    int ay;
    int ax,az;                    // plp->ax / plp->az (body pitch/roll); dnu hands
                                  // (POINT*)&plp->px to bhSetEffect, whose
                                  // ox/oy/oz are exactly these three angles
    int frm_mode;                 // plp->frm_mode (bhSetMotion reverse branch)
    unsigned motion;              // plp->mtn_no
    int frame;                    // plp->frm_no, 16.16
    int floor;                    // plp->flr_no
    unsigned unsupported_services;
    unsigned status;              // plp->stflg
    int mode0;
    unsigned flg,flg2;            // plp->flg / plp->flg2
    int mode2,mode3;              // action routine (14..21) and its sub state
    int mtn_add,mtn_md,mtn_tp;
    int hokan_rate,hokan_count;
    int ayp;
    int ct0,ct1,ct2,ct3;
    float spd,rtspd,yn,kdn_py,arp;
    int kdn_fr,floor_b,flr_snd;
    int wpntp,dmlvl,ply_id;
    unsigned st_flg;             // sys->st_flg bits this action maintains (0x4)
    RECVX_STAIR_AREA area;        // htp: the area bhCheckExmAtari selected
    int area_valid;
    // Bildnummer, an der SetMtn* in diesem Durchlauf abgetastet hat
    // (plp->frm_no vor dem Vorschub).  Die Laufzeit zeichnet damit dieselbe
    // Pose, die das Original in owP stehen hat.
    int sample_frame;
} RECVX_STAIR_STATE;
// hitchk.c bhCheckExmAtari: starts one of the six original step actions.
// `action` is the mode2 the caller selected (14 kdu, 15 kdd, 16 dnu, 17 dnd,
// 20 hsu, 21 hsd). The caller has already applied the probe table (type 1 at
// 0.75, type 2 at 4.5 then 6.0/9.1) and bhSetUseKaidanFlag.
/* player.c:216-227 PlMtnAct[wpntp][dmlvl][column]; 0xFFFF ausserhalb der Tabelle. */
unsigned recvx_player_mtn_act(int wpntp,int dmlvl,int column);
/* player.c:239-257 PlFootSnd[ply_id][foot][column]; -1 ausserhalb der Tabelle. */
int recvx_player_foot_snd(int ply_id,int foot,int column);
int recvx_stairs_begin(RECVX_STAIR_STATE* state,const RECVX_STAIR_AREA* area,int action,const float* grand,float px,float py,float pz,int ay,int floor,int dmlvl,int wpntp);
// One original 30-Hz player frame: the whole action routine, then bhSetMotion's
// frame advance and the plp->flg 0x400000 last-frame update. Returns 0 on the
// frame whose termination block handed the player back.
int recvx_stairs_tick(RECVX_STAIR_STATE* state,const RECVX_STAIR_HOST* host);
#ifdef __cplusplus
}
#endif
