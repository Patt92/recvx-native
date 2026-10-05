#pragma once
#include "ninja_matrix.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned short max;short cmb;unsigned short type;} ITEM_WORK;
typedef struct {short ax,ay,az,hide;} DSP_WORK;
extern const ITEM_WORK itemdata[156];
extern const short combidata[410];
extern const unsigned int getbulletmax[156][4];
extern const unsigned char curedata[10];
#ifdef __cplusplus
}
#endif
