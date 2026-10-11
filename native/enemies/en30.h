#ifndef RECVX_NATIVE_EN30_H
#define RECVX_NATIVE_EN30_H

#include "enemy_work.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 30 — Alexia Baby / Larve (en30.c).
   Nativ eigenstaendig bestimmt. Semantikreferenz: en30.c (:24 Mode0[6],
   :33 BrainType[1], :37 MoveMode2[8], :48 DamageMode2[2], :53 DeadMode2[2],
   :59 bhEne30, :96 bhEne30_Init, :184 bhEne30_Brain, :190 bhEne30_BR00,
   :218 bhEne30_Move, :234 bhEne30_MV00, :240 bhEne30_MV01, :345 bhEne30_MV02,
   :424 bhEne30_MV03, :471 bhEne30_MV04, :491 bhEne30_MV05, :566 bhEne30_MV06,
   :642 bhEne30_MV07, :721 bhEne30_Nage, :726 bhEne30_Damage, :733 bhEne30_DG00,
   :760 bhEne30_DG01, :785 bhEne30_Die, :791 bhEne30_DD00, :861 bhEne30_DD01,
   :883 bhEne30_CheckEnemies, :911 bhEne30_DamageInit, :969 bhEne30_CollisionLine,
   :984 bhEne30_SetFluidEffect).
   Kein Originalkoerper, kein Generator, keine zweite Workwelt.

   Die Identitaet steht achtmal in den Raumdaten (RM_5810/RM_9302, je
   Raumindex 2..5; Index 0 ist jeweils EN13). bhEne13_Init verknuepft beim
   ersten Pass ALLE aktiven id-30-Works mit sich (en13.c:217-227: lkwkp =
   EN13, Liste exp0[44..]), bevor der erste EN30-Pass laeuft; bhEne13_
   PutAttacker (en13.c:1033-1071) schaltet einen davon auf MV06 und spielt
   SE 74507. Ohne nativen EN13-Port fehlt dieser Erzeuger (Luecke).
   EN30 arbeitet auf ZWEI Modellen: mdl[0] und mdl[1] sind die Morphquellen
   (obj_a/obj_b, en30.c:176-177). Der Morph schreibt die lokale y-Lage der
   Knoten 5..15 BEIDER Quellen aus den in exp0 gesicherten Ruhelagen von
   mdl[1] (en30.c:147-153 sichern, :415/:461/:829 schreiben). Diese
   Knotenschreibzugriffe gehoeren dem Assetbesitzer, nicht dem Gegnerwork,
   und liegen darum als eigener Dienstsatz hier. Fehlt ein Dienst, bricht
   der betroffene Originalzweig ab und meldet seine Originalroutine. */
typedef struct {
    /* en30.c:147-153: mdl[model].objP[i].pos fuer i < mdl[model].obj_num.
       `max` begrenzt auf die 20 Plaetze des originalen 252-Byte-Blocks;
       Rueckgabe = Anzahl gelesener Knoten, 0 = Dienst/Modell fehlt. */
    uint32_t (*model_node_rest)(const RECVX_ENEMY_WORK* w, uint32_t model,
                                RECVX_ENEMY_VEC* out, uint32_t max);
    /* en30.c:156-159: mlwP->objP[node].pos[1] = y auf EINEM Modell. */
    int (*model_node_set_y)(RECVX_ENEMY_WORK* w, uint32_t model,
                            uint32_t node, float y);
    /* en30.c:161-164 / :809-810: objP[node].evalflags Bit 0x8 je Modell. */
    int (*model_node_hidden)(RECVX_ENEMY_WORK* w, uint32_t model,
                             uint32_t node, int hidden);
    /* en30.c:415/:461/:829: obj_a[node].pos[1] = obj_b[node].pos[1] = y,
       also dieselbe y-Lage in BEIDEN Morphquellen. */
    int (*morph_node_set_y)(RECVX_ENEMY_WORK* w, uint32_t node, float y);
    /* en30.c:176-177 / :361-362 / :438-439: obj_a = mdl[model_a].objP,
       obj_b = mdl[model_b].objP. Interpolation/Zeichnen bleibt beim
       vorhandenen Morphbesitzer. 0 = nicht gebunden. */
    int (*morph_sources)(RECVX_ENEMY_WORK* w, uint32_t model_a, uint32_t model_b);
    /* rutchk.c:5 bhCheckRoute(ps0, ps1, ps2): Rueckgabe Routenindex `now`
       oder 0xFF; `out` wird nur bei rid == nxt (dann = ps1) bzw. im
       Erfolgsfall (Mittelpunkt von rom->rutp[now]) geschrieben.
       -1 = Dienst ohne Raumdaten (Luecke). */
    int (*route_points)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to,
                        RECVX_ENEMY_VEC* out);
} RECVX_EN30_SERVICES;

/* Bindet den Dienstsatz; NULL loest ihn. Kein Spielzustand im Dienstzeiger. */
void recvx_en30_bind(const RECVX_EN30_SERVICES* services);

/* Einstieg: bhEne30 (en30.c:59). */
void recvx_en30(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif

#endif /* RECVX_NATIVE_EN30_H */
