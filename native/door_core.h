#pragma once
#include "ninja_matrix.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t evalflags,model;float pos[3];int ang[3];} RECVX_DOOR_NODE;
// One njCnkSetEasyMultiLight slot the door demo writes. LightSubPnt uses slot
// lgt_no+2; bhSetDoor switches slot 0 off and nothing ever writes slot 1, so
// the demo is lit by its ambient plus these point lights alone.
typedef struct {int on;float col[3];NJS_POINT3 pos;float n_rang,f_rang;} RECVX_DOOR_POINT;
typedef struct RECVX_DOOR_STATE {
    int dmo_atr,status,time;
    int fde_mode,fde_reg,fde_col;float fde_rate;void* fpP;
    int vew_mode,vew_reg,vew_tmp;void* vpP;
    NJS_POINT3 vew_pos,vew_bak;int vew_ang[3],vew_ang_speed,vew_yaw,vew_pitch;
    float vew_speed,vew_adj_pos,vew_adj_ang,vew_adj_pos_add,vew_adj_ang_add;
    void (*vew_prcP)(struct RECVX_DOOR_STATE*);
    int dor_mode,dor_reg,dor_snd,dor_snd_exd,dor_yaw,dor_pitch,dor_ang_speed;
    float dor_flt,dor_speed;void* dpP;RECVX_DOOR_NODE* objP;
    void (*dor_prcP)(struct RECVX_DOOR_STATE*);
    void (*fde_prcP)(struct RECVX_DOOR_STATE*);
    // Number of nodes the model actually has. The procedures of the original
    // form pointers into an unused second leaf; writes past this count stay in
    // the caller's padding and never reach the drawn model.
    unsigned nodes;
    // Procedure numbers the packet selects for the two families bound below.
    int lgt_type,pru_type;
    // LightProc1/2 and DokiProcLgt. lgt_amb is the ambient LightSubAmb put in
    // after the 0.4 clamp; lgt_pnt holds slots 2..4 after LightSubPnt applied
    // the rate and the camera offset. lgt_on turns 1 once the ambient was set,
    // so a demo without a light procedure keeps the drawing it had.
    void (*lgt_prcP)(struct RECVX_DOOR_STATE*);
    int lgt_mode,dki_mode,lgt_on;void* lpP;
    NJS_POINT3 lgt_amb_off,lgt_pnt_rte;
    float lgt_amb[3];RECVX_DOOR_POINT lgt_pnt[3];
    // PuruProc1. Each pass can raise the two requests the procedure makes:
    // CallSystemSe(0, PruSndTbl[snd_no]) once and StartVibrationEx(2, pru_no)
    // up to twice. The owner drains the counters; pru_se_no is -1 when the
    // packet names a sound outside PruSndTbl.
    void (*pru_prcP)(struct RECVX_DOOR_STATE*);
    int pru_mode,pru_reg,pru_tim;void* ppP;
    int pru_se,pru_se_no,pru_vib,pru_vib_atrb,pru_vib_no;
} RECVX_DOOR_STATE;
// Declared size of one MPAC parameter block. family 0 fade, 1 view, 2 door,
// 3 light, 4 shake.
size_t recvx_door_parameter_size(unsigned family,unsigned type);
// Bytes a light block really occupies. LIGHTPROC_WORK is 0x7C because it can
// hold three LGT_WRK, but a packet stores only the lights it declares -- the
// header plus lgt_num of them. LightProc2 still steps a full 0x7C to its
// second block, so only that one is short. 0 when the block does not fit or
// declares more than three lights.
size_t recvx_door_light_extent(const unsigned char* block,size_t bytes,unsigned type);
// Binds the five procedure families; 0 when a type has no procedure.
int recvx_door_configure(RECVX_DOOR_STATE* state,unsigned fade,unsigned view,unsigned door,
                         unsigned light,unsigned shake);
// bhSetDoor with dmo_atr 0x40: status 0x20000000 replaces the packet's shake by
// PuruProcTbl[1] over the built-in DkiPru block (loop 768, pattern 13, wait 20).
void recvx_door_select_doki_shake(RECVX_DOOR_STATE* state);
// Walks the chg_vew chain of view types 4/5/6: every follow-up block must fit
// behind its predecessor and name a defined procedure.
int recvx_door_validate_view(unsigned type,const unsigned char* parameters,size_t bytes,unsigned depth);
// njClipZ of bhSetDoor as positive view-space distances, matching view_clip.h.
// Returns 1 when the dmo_clip table has a case for this demo, 0 for the
// gameplay default (near 2 / far 20000).
int recvx_door_clip(int door_type,int partition,int view_type,float* near_clip,float* far_clip);
// Archive partition that holds MRY.AFS: dmo_clip compares against it.
#define RECVX_DOOR_MRY_PARTITION 5
// One 30 Hz pass of bhControlDoor: shake light, fade, camera, door, light,
// shake -- in that order, then time++.
void recvx_door_step(RECVX_DOOR_STATE* state);
#ifdef __cplusplus
}
#endif
