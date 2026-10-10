#ifndef RECVX_NATIVE_ENEMY_DISPATCH_H
#define RECVX_NATIVE_ENEMY_DISPATCH_H
#include "enemy_work.h"

/* ===========================================================================
   T1 Stufe B — Dispatch- und Abnahmekette aller Gegner
   ---------------------------------------------------------------------------
   Eigenstaendige native Bestimmung der Reihenfolge, die eneset.c vorgibt.
   Kein Originalkoerper, kein Generator, keine zweite Workwelt und
   insbesondere KEIN Dispatcher, der originale bhControlEnemy/bhJumpEnemy
   aufruft: `recvx_enemy_control` fuehrt die Reihenfolge selbst aus und ruft
   die nativen Gegnermechanismen dieses Projekts.

   Fundstellen: eneset.c:51 (bhJumpEnemy[100]), :344 (bhControlEnemy),
   :797 (bhDrawEnemy), :866 (bhDrawEneObject), :933 (bhEne_SetCallFunc).
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

#define RECVX_ENEMY_IDS 100u   /* eneset.c:51 Tabellengroesse */

typedef void (*RECVX_ENEMY_PROC)(RECVX_ENEMY_WORK*);

/* Die Tabelle ist im Original ein globales, zur Laufzeit VERAENDERBARES Array
   (bhEne_SetCallFunc, eneset.c:933; EN03/EN13 tauschen Eintraege). Sie gehoert
   deshalb pro Raum/World zum Zustand und nicht in eine Konstante. */
typedef struct {
    RECVX_ENEMY_PROC jump[RECVX_ENEMY_IDS];
} RECVX_ENEMY_DISPATCH;

/* Belegt die Tabelle mit den in diesem Projekt nativ vorhandenen Gegnern.
   Jede Identitaet ohne nativen Besitzer bleibt NULL; `recvx_enemy_control`
   meldet sie dann namentlich als Luecke ("bhEneNN") und tut nichts. Es wird
   kein Ersatzverhalten eingesetzt. */
void recvx_enemy_dispatch_init(RECVX_ENEMY_DISPATCH* d);

/* eneset.c:933 bhEne_SetCallFunc. */
void recvx_enemy_set_call_func(RECVX_ENEMY_DISPATCH* d, uint32_t id, RECVX_ENEMY_PROC proc);

/* Identitaeten, die das Original auf bhSubpl leitet (eneset.c:84-152):
   31..52, 57..70 und 72..99. Diese gehoeren dem vorhandenen nativen
   Subplayer-Besitzer (native/subplayer.c) und werden hier NICHT doppelt
   gefuehrt — `recvx_enemy_control` ueberspringt sie und meldet keine Luecke. */
int recvx_enemy_is_subplayer_id(uint32_t id);

/* eneset.c:344 bhControlEnemy, vollstaendige Reihenfolge:
     1. plp->ofx = plp->ofz = 0 (unbedingt, VOR dem sp_flg-Test)
     2. nur bei sp_flg & 0x2: en_objn = 0
     3. i = 0 .. ewk_n-1:
          enow = i                      (fuer JEDEN Slot im Bereich)
          Filter recvx_enemy_pass_enabled
          pxb/pyb/pzb <- px/py/pz, axb/ayb/azb <- ax/ay/az
          id > 90: bhControlMask         (Dienst/Besitzer, hier gemeldet)
          jump[id](work)
          bhEne_InitDamage(work)
          flg & 0x80000000: an en_obj anhaengen
   Rueckgabe: Anzahl der Works, deren Gegnerroutine in diesem Pass lief. */
uint32_t recvx_enemy_control(RECVX_ENEMY_TABLE* t, const RECVX_ENEMY_DISPATCH* d,
                             uint32_t sp_flg, const RECVX_ENEMY_SERVICES* sv);

#ifdef __cplusplus
}
#endif
#endif
