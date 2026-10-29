#ifndef RECVX_NATIVE_EN25_H
#define RECVX_NATIVE_EN25_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 25 — Deckenfalle (Ceiling Trap). Nativ eigenstaendig bestimmt.
   Semantikreferenz: en25.c (BloodParam :13, Mode0[6] :22, MoveMode2[4] :31,
   bhEne25 :39, Init :53, Brain :108, Move :114, MV00 :161, MV01 :171,
   MV02 :251, MV03 :278, leere Nage/Damage/Die :301/:307/:313, SetEffect :319,
   PlayerControl :358), subpl.c:143 (bhEne_Event) und eneset.c:906
   (bhEne_CallocWork). Kein Originalkoerper, kein Generator, keine zweite
   Workwelt.

   Der Originalblock des Works ist 12 Byte (en25.c:57 bhEne_CallocWork(0xC,8)):
     +0x0 EXP0_UC(0x0)  Spieler in der 6.9er-Box   (en25.c:128/132)
     +0x1 EXP0_UC(0x1)  Spieler in der 6.8er-Box   (en25.c:142/146)
     +0x4 EXP0_ATR(0)   Zeiger auf die eigene bewegliche Flaeche in
                        sys->mwalp[sys->mwal_n++]   (en25.c:58, macros.h:78)
     +0x8 EXP0_UC(0x8)  Falle hat zugeschnappt      (en25.c:82/209)
   Nativ ist +0x4 der Index der Flaeche (mobile_wall_alloc, enemy_work.h:510);
   Inhalt, Nullinitialisierung und Lebensdauer bleiben gleich.

   Einstieg: bhEne25 (en25.c:39); der Dispatch-Eintrag 25 (eneset.c:51)
   gehoert dem Dispatcher-Besitzer und wird hier nur beschrieben, nicht
   angelegt. */
/* sys->gm_flg / sys->st_flg / sys->pt_flg, die en25.c:192-207 beim Zuschnappen
   schreibt. which 0 = gm_flg, 1 = st_flg, 2 = pt_flg (wie RECVX_EN15_SERVICES). */
typedef struct RECVX_EN25_SERVICES {
    uint32_t (*sys_flags)(uint32_t which);
    void     (*sys_flags_set)(uint32_t which, uint32_t set, uint32_t clear);
} RECVX_EN25_SERVICES;
void recvx_en25_bind(const RECVX_EN25_SERVICES* services);
void recvx_en25(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
