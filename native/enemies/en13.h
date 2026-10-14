#ifndef RECVX_NATIVE_EN13_H
#define RECVX_NATIVE_EN13_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Alexia II (ID 13) und ihr Tentakel-Work bhEne13B (ID 31). Eigenstaendige
   native Umsetzung auf dem WorldEntity-Work; Semantikreferenz
   src/ps2/veronica/prog/en13.c:50-1387 und en13sub.c:33-462. Keine
   Originalkoerper, kein Generator, keine zweite Spieler-/Gegnerwelt.

   EN13 erzeugt seine Kinder selbst (en13.c:96-201): vier EN18-Koerper
   (Typ 0..3 auf mdl[10]/[1]/[4]/[7]), sechs EN18-Kollisionskinder (Typ 4..9,
   alle auf mdl[10], also demselben ML_WORK wie Typ 0) und den EN13B-Work
   (mdl[13], skp[13], mnwP/mnwPb von EN13). Die Dienste unten sind
   Blattzugriffe auf die vorhandenen Besitzer; ein fehlender Dienst wird
   namentlich als Luecke gemeldet. */

typedef struct RECVX_EN13_SERVICES {
    /* eneset.c:170 bhSetEnemy(&tmpl, rom->ene_n) und `mlwP = &epw->mdl[base]`.
       share != NULL: das Kind benutzt denselben ML_WORK wie `share`
       (en13.c:170, Typ 4..9 auf mdl[10] wie exp0[4]). Rueckgabe NULL bei
       voller Tabelle oder fehlendem Modell (Anbieter meldet). */
    RECVX_ENEMY_WORK* (*create_child)(RECVX_ENEMY_WORK* parent, const RECVX_ENEMY_TEMPLATE* tmpl,
                                      uint32_t model_base, RECVX_ENEMY_WORK* share);
    /* EN13B: mnwP = epw->mnwP, mnwPb = epw->mnwPb, skp[0] = epw->skp[13]
       (en13.c:186-191). */
    void (*share_motion)(RECVX_ENEMY_WORK* child, const RECVX_ENEMY_WORK* parent);
    /* bhEne13_StoreObject/RestoreObject (en13.c:928/981): objP[i].pos und die
       Vertexstroeme (Lage und Normale) des aktuellen mlwP->objP. Der Speicher
       ist beim ersten Init unveraendert; Restore stellt ihn wieder her. */
    int (*restore_objects)(RECVX_ENEMY_WORK* w);
    /* `mlwP->objP = exp0[92..104]`: Wurzel 0 des Kindbaums wieder einsetzen. */
    int (*select_first_root)(RECVX_ENEMY_WORK* w);
    /* bhEne13_Finish (en13.c:857): objP[ono].pos *= (px,py,pz) und
       bhEne13_ScaleModel (en13.c:897) auf den Vertexstrom (vx,vy,vz). */
    int (*scale_object)(RECVX_ENEMY_WORK* w, uint32_t ono, const RECVX_ENEMY_VEC* pos_scale,
                        const RECVX_ENEMY_VEC* vertex_scale);
    /* cam.ofx/ofy/ofz (camera.c:36/49/81). */
    void (*camera_offset)(float x, float y, float z);
    /* sys->gm_mode, 1 = vorhanden. */
    int (*game_mode)(int* out);
    /* plp->mnwP: 0 = plp->mnwPb, 1 = epw->mnwP (en13.c:1188/1271). */
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
    /* zonzon1.c:611 bhEne_AttackHitCheck(*ep, ...) gegen einen Gegnerwork
       (en13sub.c:375): watr-Kapsel und owP des Works; schreibt dvx/djnt_no/dpx
       am Work. 1 = Treffer. */
    int (*attack_hit_work)(RECVX_ENEMY_WORK* target, const RECVX_ENEMY_VEC* pos, float ar);
    /* bhEne13B (en13sub.c:52-57): owP[i].mtx[12..14] des Works. */
    int (*node_position)(const RECVX_ENEMY_WORK* w, uint32_t node, RECVX_ENEMY_VEC* out);
} RECVX_EN13_SERVICES;

/* exp0 von EN13 (bhEne_CallocWork(992), en13.c:98). Benannte Felder statt
   EXP0-Offsets; die Ueberlappung der id-30-Liste exp0[44..] mit den
   gespeicherten Wurzeln exp0[92..104] bleibt sichtbar: mehr als zwoelf
   EN30-Works wuerden die Wurzeln ueberschreiben und werden gemeldet. */
typedef struct RECVX_EN13_STATE {
    RECVX_ENEMY_WORK* child[11];    /* [1] Typ0 mdl[10], [2..4] Typ1..3, [5..10] Typ4..9 */
    RECVX_ENEMY_WORK* hatch[12];    /* exp0[44..91]: id-30-Works                */
    int32_t  hatch_n;               /* EXP0_I(108)                               */
    RECVX_ENEMY_WORK* en14;         /* exp0[112]                                 */
    int32_t  unused116[3];          /* EXP0_I(116/120/124) = 100, ohne Leser     */
    int32_t  hatch_no;              /* EXP0_I(128)                               */
    int32_t  store_index[5];        /* EXP0_I(932..948)                          */
    int32_t  timer[3];              /* EXP0_I(964/968/972)                       */
    RECVX_ENEMY_WORK* tentacle;     /* exp0[976]: EN13B                          */
    float    camera;                /* EXP0_F(980)                               */
    int32_t  tenta_no;              /* EXP0_I(984)                               */
} RECVX_EN13_STATE;

/* exp0 von EN13B (bhEne_CallocWork(520), en13sub.c:104): EXP0_ATR(n) an
   exp0+4+4n als mobiler Wandindex, EXP0_P3(n) an exp0+0x84+12n die owP-Lage
   des Vorpasses, EXP0_I(516) (macros.h:78/79). Platz fuer 32 Knoten. */
#define RECVX_EN13B_NODES 32u
typedef struct RECVX_EN13B_STATE {
    int32_t wall[RECVX_EN13B_NODES];
    RECVX_ENEMY_VEC prev[RECVX_EN13B_NODES];
    int32_t hit_n;
} RECVX_EN13B_STATE;

/* Nur Dienstbindung, kein Spielzustand. Vor dem Gegnerpass installieren. */
void recvx_en13_bind(const RECVX_EN13_SERVICES* services);
void recvx_en13(RECVX_ENEMY_WORK* w);
void recvx_en13b(RECVX_ENEMY_WORK* w);
/* Die von EN18 gerufenen EN13-Routinen (en13.c:1033-1094). */
int  recvx_en13_hatch_no(const RECVX_ENEMY_WORK* w, int32_t* out);
int  recvx_en13_tenta_no(const RECVX_ENEMY_WORK* w, int32_t* out);
void recvx_en13_put_attacker(RECVX_ENEMY_WORK* w, int32_t no);
void recvx_en13_tentacle(RECVX_ENEMY_WORK* w, int32_t no);

#ifdef __cplusplus
}
#endif
#endif
