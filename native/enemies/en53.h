#ifndef RECVX_NATIVE_EN53_H
#define RECVX_NATIVE_EN53_H
#include "enemy_work.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Nosferatu-Kern, eigener Dispatcher-ID 53 (eneset.c:106).
   Semantikreferenz en15.c:3350 ChangeAmbient, :3409 SetMince,
   :3439 CoreInit, :3472 CoreMove, :3539 CoreDie, :3726 bhEne53.
   Die Modell-/Materialquelle bleibt beim WorldEntity-Besitzer. */
typedef struct {
    /* mbp[model]->child->model->plist: nur ChangeAmbient-Bytes patchen,
       nicht pauschal alle RGB-/Alpha-Kanaele eines Modells faerben. */
    int (*child_ambient)(RECVX_ENEMY_WORK*, uint32_t model, uint8_t value);
    int (*model_root)(RECVX_ENEMY_WORK*, uint32_t model);
    int (*morph_pair)(RECVX_ENEMY_WORK*, uint32_t a, uint32_t b);
    int (*morph_indices)(const RECVX_ENEMY_WORK*, uint32_t* a, uint32_t* b);
    /* epw->txp[0] (en15.c:3431), nicht ungeprueft mlwP->texP einsetzen. */
    const void* (*work_texture)(const RECVX_ENEMY_WORK*);
} RECVX_EN53_SERVICES;
void recvx_en53_bind(const RECVX_EN53_SERVICES*);
void recvx_en53(RECVX_ENEMY_WORK*);
/* Native bounded traversal of the original short-word chunk stream.
   n is supplied by the owner; returns 0 for incomplete/invalid streams. */
int recvx_en53_change_ambient(uint8_t* plist, size_t n, uint8_t value);
#ifdef __cplusplus
}
#endif
#endif
