#ifndef RECVX_NATIVE_EN14_H
#define RECVX_NATIVE_EN14_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Alexia (ID 14). Einzige Workquelle: WorldEntity::enemy_work.
   Semantikreferenz en14.c:181-1560; kein Originalkörper/Generator.
   EN13 verknuepft Typ 0 in seinem Init (en13.c:230-235, lkwkp = EN13),
   steuert ihn ueber dieselbe Work (mode2 1/2: en13.c:490-493/532-535) und
   ruft HitMark direkt auf (en13.c:642/761, nach flg &= ~4 in :640/:759).
   EN14 erzeugt selbst keine Child-Works.
   Leere Originalzustände bleiben leer, insbesondere Brain, Nage und Die. */

/* Benannte EXP0-Daten statt ILP32-Zeigerarithmetik. Die unbenutzten Wörter
   erhalten Nullinitialisierung und das originale Allokationsbudget 584.
   beam ist eine stabile Poolidentität (Slot+1, 0=NULL), KEIN Effektbesitzer. */
typedef struct {
    uint32_t unused0;
    float length[11];                         /* 0x004 */
    RECVX_ENEMY_VEC velocity[11];              /* 0x030 */
    uint32_t unused180[33];                   /* 0x0b4 */
    RECVX_ENEMY_VEC position[11];              /* 0x138 */
    RECVX_ENEMY_VEC previous[11];              /* 0x1bc */
    int32_t look_ticks;                       /* 0x240 */
    uint32_t beam;                            /* 0x244 */
} RECVX_EN14_STATE;

/* Ausschließlich Sicht/Dienste der vorhandenen Asset-, Effekt-, Raum- und
   Systembesitzer. Keine eigene sys/plp/eff-Welt. 0 bei read = fehlende Daten;
   find_beam: 1 = Suche ausgeführt (auch handle 0), 0 = Dienst ungebunden.
   Alle fehlenden Dienste werden über w->sv->gap gemeldet. */
typedef struct {
    int (*node_local)(const RECVX_ENEMY_WORK*, uint32_t node,
                      RECVX_ENEMY_VEC* pos, int32_t angles[3]);
    void (*node_angles)(RECVX_ENEMY_WORK*, uint32_t node, const int32_t angles[3]);
    int (*motion_angles)(const RECVX_ENEMY_WORK*, uint32_t motion,
                         uint32_t node, uint32_t frame, int32_t angles[3]);
    /* O_WORK.flg: nur maskierte Bits ändern, dann ggf. owP-Matrix setzen.
       LookPlayer schreibt Bit 2 auf owP[6], danach seine absolute Matrix. */
    void (*joint_flags)(RECVX_ENEMY_WORK*, uint32_t joint, uint32_t mask, uint32_t value);
    void (*joint_matrix_set)(RECVX_ENEMY_WORK*, uint32_t joint, const float matrix[16]);
    /* mlwP=mdl[0], obj_a=mdl[0].objP, obj_b=mdl[1].objP. */
    void (*morph_models)(RECVX_ENEMY_WORK*, uint32_t model_a, uint32_t model_b);
    /* bhCheckFloorEnemy(player.flr_no,px,pz): erster Originaltreffer.
       1 Treffer, 0 kein Treffer, -1 ungebundene Abfrage. */
    int (*floor_enemy)(int floor, float px, float pz, RECVX_ENEMY_ATR* out);
    /* eff[0..511]: erster flg&1, !(stflg&0x1000000), id130,type4.
       Lesen im Folgepass behält die Slotidentität, prüft NICHT erneut Flags. */
    int (*find_beam)(uint32_t* handle);
    int (*beam_line)(uint32_t handle, RECVX_ENEMY_VEC* pos, RECVX_ENEMY_VEC* direction);
    /* bhSetEffectTb: ID256, flg1, type4/6, pos, sx=sy=sz=size,
       restliche sys->ef-Felder bleiben Eigentum des vorhandenen Besitzers.
       -1 = Pool voll (kein Fehler); -2 = ungebunden. */
    int (*acid_alloc)(RECVX_ENEMY_WORK*, uint32_t type, const RECVX_ENEMY_VEC* pos, float size);
    /* Nach erfolgreicher Allokation: stflg|=0x20, txp[0]=w->mdl[2].texP,
       tex_id=0, xn/yn/zn=velocity, px/py/pz=pos, mdlver=se. */
    void (*acid_setup)(RECVX_ENEMY_WORK*, int slot, const RECVX_ENEMY_VEC* pos,
                       const RECVX_ENEMY_VEC* velocity, int se);
    int (*game_mode)(int* out);               /* sys->gm_mode, 1=vorhanden */
} RECVX_EN14_SERVICES;

/* Integrator kann entweder explizit übergeben oder einen dienst-only Resolver
   installieren. Resolver hält KEINE Work-/Raumdaten; Anbieter löst nach w->owner
   auf. NULL beim Bind entfernt ihn. Vor der Raumsteuerung installieren. */
typedef const RECVX_EN14_SERVICES* (*RECVX_EN14_RESOLVER)(const RECVX_ENEMY_WORK*);
void recvx_en14_bind_services(RECVX_EN14_RESOLVER resolver);
void recvx_en14(RECVX_ENEMY_WORK* w);
void recvx_en14_with_services(RECVX_ENEMY_WORK* w, const RECVX_EN14_SERVICES* ex);
/* Direkter EN13-Aufruf: originaler Schaden der Typ-0-Verknüpfung geht zum
   Parent; Typ 0 vermindert hier NICHT die eigenen HP. -1 bei fehlenden Daten. */
int recvx_en14_hitmark(RECVX_ENEMY_WORK* w);
int recvx_en14_hitmark_with_services(RECVX_ENEMY_WORK* w, const RECVX_EN14_SERVICES* ex);
#ifdef __cplusplus
}
#endif
#endif
