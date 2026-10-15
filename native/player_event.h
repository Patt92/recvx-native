#pragma once
/* ===========================================================================
   A02 Ereignisbewegungen des Spielers - native Bestimmung von
     pl_evt.c:62  bhCPM0_event (bhCPM0_event_tbl[plp->mode2], 10 Eintraege)
     pl_evt.c:68  pl_smove00 .. :649 pl_smove08
   Semantikreferenz, kein Originalkoerper, kein Generator.

   Besitz wie A01 (player_action.h): RECVX_PLAYER_ACTION haelt die plp-/EXP_WORK-
   Felder, RECVX_PLAYER_EVENT die nur hier gebrauchten plp-Felder mode1, ct2 und
   mtn_md. flg/stflg/EXP1_I(0)/pad_old und die Dienste (motion_frames,
   footstep, effect108, water_splash, gap) kommen ueber RECVX_PLAYER_ACTION_IO.
   motion_frames liefert plp->mnwP[mtn_no].frm_num der gerade gueltigen
   Bibliothek (mnwP), nicht zwingend der Spielerbibliothek.

   Nicht dargestellt (gemeldet): die Posenueberblendung selbst (hokan_rate),
   wie in A01; hokan_count wird gefuehrt. mtn_md 0x2 (Spiegelung) wird nur
   gefuehrt.
   =========================================================================== */
#include "player_action.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RECVX_PLAYER_EVENT {
    uint32_t mode1;   /* plp->mode1 (unsigned char) */
    int32_t  ct2;     /* plp->ct2 */
    uint32_t mtn_md;  /* plp->mtn_md */
} RECVX_PLAYER_EVENT;

typedef struct RECVX_PLAYER_EVENT_IO {
    RECVX_PLAYER_ACTION_IO* base;
    uint32_t flr_snd; /* plp->flr_snd (bhCheckFloorP Typ 1) */
    /* CallPlayerFootStepSe(plp->flr_snd, type, 1) ohne bhCheckFloorSound (pl_smove01) */
    void (*floor_step)(void* user, uint32_t flr_snd, uint32_t type);
} RECVX_PLAYER_EVENT_IO;

/* pl_evt.c:62 bhCPM0_event: pl_smove[mode2]; mode2 9 ist erneut pl_smove00.
   Rueckgabe 0, wenn mode2 ausserhalb der Tabelle liegt (gemeldet, nichts ausgefuehrt). */
int recvx_player_event(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_EVENT* e, RECVX_PLAYER_EVENT_IO* io);

#ifdef __cplusplus
}
#endif
