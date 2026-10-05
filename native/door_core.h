#pragma once
#include "ninja_matrix.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t evalflags,model;float pos[3];int ang[3];} RECVX_DOOR_NODE;
typedef struct RECVX_DOOR_STATE {
    int dmo_atr,status;
    int fde_mode,fde_reg,fde_col;float fde_rate;void* fpP;
    int vew_mode,vew_reg,vew_tmp;void* vpP;
    NJS_POINT3 vew_pos,vew_bak;int vew_ang[3],vew_ang_speed,vew_yaw,vew_pitch;
    float vew_speed,vew_adj_pos,vew_adj_ang,vew_adj_pos_add,vew_adj_ang_add;
    void (*vew_prcP)(struct RECVX_DOOR_STATE*);
    int dor_mode,dor_reg,dor_snd,dor_snd_exd,dor_yaw,dor_pitch,dor_ang_speed;
    float dor_flt,dor_speed;void* dpP;RECVX_DOOR_NODE* objP;
    void (*dor_prcP)(struct RECVX_DOOR_STATE*);
    void (*fde_prcP)(struct RECVX_DOOR_STATE*);
} RECVX_DOOR_STATE;
size_t recvx_door_parameter_size(unsigned family,unsigned type);
int recvx_door_configure(RECVX_DOOR_STATE* state,unsigned fade,unsigned view,unsigned door);
int recvx_door_validate_view(unsigned type,const unsigned char* parameters,size_t bytes,unsigned depth);
void recvx_door_step(RECVX_DOOR_STATE* state);
#ifdef __cplusplus
}
#endif
