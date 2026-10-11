#ifndef RECVX_NATIVE_EN04_TABLES_H
#define RECVX_NATIVE_EN04_TABLES_H
#include "en04.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const int32_t recvx_en04_player_mtn[4];
extern const RECVX_EN04_MTBL recvx_en04_sfx[30], recvx_en04_vib[6];
extern const uint8_t recvx_en04_tree[8], recvx_en04_flip[20];
extern const int8_t recvx_en04_shadow[6];
extern const RECVX_EN04_WPNDAMAGE recvx_en04_wpn_damage[22];
extern const RECVX_EN04_COMBWEP recvx_en04_combo_wep[21];
extern const RECVX_EN04_COMBJOINT recvx_en04_combo_joint[19];
extern const RECVX_ENEMY_CPCL recvx_en04_capsules[17];
extern const RECVX_ENEMY_BP_WORK recvx_en04_blood_pool;
extern const RECVX_EN04_BT recvx_en04_blood[19];
extern const RECVX_ENEMY_VEC recvx_en04_player_offset[4];
extern const int32_t recvx_en04_stair_angle[4];
extern const RECVX_ENEMY_VEC recvx_en04_wire_offset[10];
extern const int recvx_en04_hp_tbl[2][16];
const RECVX_EN04_DATA* recvx_en04_default_data(void);

#ifdef __cplusplus
}
#endif

#endif
