#ifndef RECVX_NATIVE_SUBPLAYER_H
#define RECVX_NATIVE_SUBPLAYER_H
#include "enemies/enemy_work.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C05: Subplayer benutzen ausschliesslich RECVX_ENEMY_WORK in WorldEntity.
   Kein eigener Frame-/Transform-/Motionbesitzer und keine globale Workwelt.
   Originalquellen sind Semantikreferenzen, keine kompilierten Funktionskoerper.

   Belege: subpl.c:12/22/37/38/56 (fuenf Tabellen), :76 (Pass), :87/95 (Init),
   :131/137/143/149 (Move/Event), :155..407 (elf Bewegungszweige), :410 (ID28).
   eneset.c:51..152 (ID-Tabelle), :344..388 (Reihenfolge/Gates).
   ELF SLUS_201.84: Initweiche 0x173F30, gemeinsame Init 0x173F60,
   Tabellen 0x306150/0x306170/0x306350/0x306360/0x3063A0,
   ID-Tabelle 0x3063E0. Die 120 Initzeilen sind alle em60_init.

   Dienste aus enemy_work.h:
   - set_motion liest w->mtn_tp als datP, sampelt VOR Vorschub und schreibt
     ALLE Seiteneffekte direkt auf denselben Work (Hokan, Frame, Attr etc.).
   - calc_model sieht den bereits veraenderten Work nach Dispatch/Bewegung.
   - gap meldet jede erreichte fehlende Bindung; kein Dummyframe/-modell.
   Der umgebende recvx_enemy_control besitzt Mask/Backup/InitDamage/Drawliste.
   Event/Event2 fuehren NICHT noch einmal bhCalcModel oder Passflags aus.
   Freigabe ist unveraendert flg Bit0/stflg beim vorhandenen Besitzer; subpl.c
   besitzt weder privaten Speicher noch eine eigene Release-Prozedur. */

enum {
    RECVX_SUBPL_UNHANDLED = 0,
    RECVX_SUBPL_HANDLED = 1
};

/* Einziger zusaetzlicher Dienst fuer den ORIGINALEN ID199-Init-Sonderzweig.
   Kein zweiter Modellbesitzer: sample_window muss das ausgewaehlte native
   mlwP temporaer auf objP/owP+skip,obj_num-skip begrenzen, EIN bhSetMotion
   ausfuehren und danach die Modellansicht vollstaendig restaurieren. Der
   folgende normale Motionschritt laeuft am restaurierten Modell mit dem
   tatsaechlich fortgeschrittenen Work. Keinen Frame zuruecksetzen.
   ID199 ist ausserhalb der Initweiche; nur initialize_motion kann es erreichen.
   NULL wird dort als "em60_init:mlwP-window-8" gemeldet. */
typedef struct RECVX_SUBPLAYER_MODEL_SERVICES {
    void (*sample_window)(RECVX_ENEMY_WORK* work, uint32_t skip,
                          int add, uint32_t mode);
} RECVX_SUBPLAYER_MODEL_SERVICES;

int recvx_is_subplayer(unsigned identity);
/* Gueltiger Index fuer 40..159. Die ID-Tabelle enthaelt auch 31..39,
   deren Initlesezugriffe vor die Tabelle gehen (ELF bestaetigt), also -1.
   Kein pauschaler Init auf em60 fuer diese ungeklaerten Faelle. */
int recvx_subplayer_init_index(unsigned identity);
void recvx_subplayer_init(RECVX_ENEMY_WORK* work);
/* Gemeinsame Init (em60_init), OHNE mode0-Uebergang der Initweiche. */
void recvx_subplayer_initialize_motion(RECVX_ENEMY_WORK* work,
                                      const RECVX_SUBPLAYER_MODEL_SERVICES* model);
int recvx_subplayer_tick(RECVX_ENEMY_WORK* work);
int recvx_subplayer_dir_target(float px, float pz, int ay,
                               float x, float z, int limit);

void recvx_subplayer_enemy(RECVX_ENEMY_WORK* work);
void recvx_subplayer_enemy_event(RECVX_ENEMY_WORK* work);
void recvx_subplayer_enemy_event2(RECVX_ENEMY_WORK* work);
void recvx_subplayer_enemy28(RECVX_ENEMY_WORK* work);

#ifdef __cplusplus
}
#endif
#endif
