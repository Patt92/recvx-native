#pragma once
#include "ninja_matrix.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t evalflags,model;float pos[3];int ang[3];} RECVX_DOOR_NODE;
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
    // Procedure numbers the packet selects for the two families this port does
    // not drive yet (light.c LightProc1/2 and the PuruProc shake). They are
    // reported, not silently dropped.
    int lgt_type,pru_type;
} RECVX_DOOR_STATE;
// Byte size of one MPAC parameter block. family 0 fade, 1 view, 2 door.
size_t recvx_door_parameter_size(unsigned family,unsigned type);
// Binds the fade/view/door procedures; 0 when a type has no procedure.
int recvx_door_configure(RECVX_DOOR_STATE* state,unsigned fade,unsigned view,unsigned door);
// Walks the chg_vew chain of view types 4/5/6: every follow-up block must fit
// behind its predecessor and name a defined procedure.
int recvx_door_validate_view(unsigned type,const unsigned char* parameters,size_t bytes,unsigned depth);
// One 30 Hz pass of bhControlDoor: fade, camera and door procedure.
void recvx_door_step(RECVX_DOOR_STATE* state);
#ifdef __cplusplus
}
#endif
