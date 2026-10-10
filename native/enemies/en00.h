#ifndef RECVX_NATIVE_EN00_H
#define RECVX_NATIVE_EN00_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Identitaet 0 — Sprengfass. Nativ eigenstaendig bestimmt aus der
   Semantikreferenz eneset.c:391 (bhEne00) und eneset.c:154
   (En00CapColTab). Kein Originalkoerper, kein Generator. */
void recvx_en00(RECVX_ENEMY_WORK* w);
/* Kapselpunkttabelle des Fasses (reine Datentabelle, eneset.c:154). */
extern const RECVX_ENEMY_CPCL recvx_en00_cap_col_tab[5];
#ifdef __cplusplus
}
#endif
#endif
