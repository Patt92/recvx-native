#pragma once

#include "ninjaapi.h"

typedef struct BH_PWORK BH_PWORK;
typedef struct ML_WORK ML_WORK;

#ifdef __cplusplus
extern "C" {
#endif

/* main.c:151 initializes this exact 128-entry Ninja matrix stack. */
void core_nj_init_matrix_stack(void);
void core_bh_calc_model(BH_PWORK* ewP);
void core_bh_calc_tree(NJS_MATRIX* basP, ML_WORK* mlwP);

#ifdef __cplusplus
}
#endif
