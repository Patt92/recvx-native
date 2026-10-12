#pragma once
#include "player_stairs.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Schiebbare Kisten (objitm.c bhObj001, bhJumpObject[1]) und die Schiebeaktion
 * des Spielers (player.c bhCPM2_act_psh, mode2 18).  Eigenstaendiger nativer
 * Code, siehe player_push_core.c fuer die Fundstellen. */

/* ETTY_WORK-Felder eines Raumobjekts, die bhSetObject in O_WRK uebernimmt. */
typedef struct {int index;unsigned flags;int aspd,param,floor;float x,y,z;} RECVX_PUSH_OBJECT;
/* Hostdienste.  `sound` ist CallPlayerActionSe(sys->psh_snd, 1), `vibration`
 * StartVibrationEx(type, pattern); fehlt ein Dienst, meldet der Zustand das. */
typedef struct {void* user;void (*sound)(void*,int);void (*vibration)(void*,int,int);int ply_id;} RECVX_PUSH_HOST;
typedef struct RECVX_PUSH_CORE RECVX_PUSH_CORE;

#define RECVX_PUSH_UNBOUND_HOKAN     0x0001u /* SetMtn*Hokan-Mischung hat keinen nativen Besitzer */
#define RECVX_PUSH_UNBOUND_MOTION    0x0002u /* Motion fehlt im nativen Spielerasset */
#define RECVX_PUSH_UNBOUND_TRANS     0x0004u /* bhGetTransZ(PlyTrsZ[4][dmlvl]) fehlt */
#define RECVX_PUSH_UNBOUND_SOUND     0x0008u /* CallPlayerActionSe ohne Host */
#define RECVX_PUSH_UNBOUND_VIBRATION 0x0010u /* StartVibrationEx ohne Host */

/* Sicht auf die eine Spielerarbeit, solange die Schiebeaktion laeuft.  Die
 * Laufzeit legt flg/stflg/st_flg/flg2 vor jedem Aufruf hinein und schreibt sie
 * danach zurueck, wie beim Treppentreiber. */
typedef struct {
    int active;
    int phase;                     /* plp->mode3 */
    int contact;                   /* plp->psh_ct beim Einstieg */
    int wall;                      /* plp->psh_idx */
    int ay,ayp;
    int motion;                    /* plp->mtn_no */
    int frame;                     /* abgetastetes Bild (frm_no vor dem Vorschub) */
    int frm_no;                    /* plp->frm_no, 16.16 */
    int frm_mode,mtn_add,mtn_md;
    int hokan_rate,hokan_count;
    int mode0,mode2;
    int dmlvl,wpntp;
    unsigned flags,status,st_flg,flg2; /* plp->flg, plp->stflg, sys->st_flg, plp->flg2 */
    unsigned unsupported_services;
    float x,y,z,speed,rtspd,arp;
} RECVX_PUSH_STATE;

RECVX_PUSH_CORE* recvx_push_create(float room_height,const float* grand,const RECVX_PUSH_OBJECT*,unsigned count,const RECVX_PUSH_HOST*);
void recvx_push_destroy(RECVX_PUSH_CORE*);
/* frm_num je Spielermotion und die bhGetTransZ-Kurve PlyTrsZ[4][*] (Kurve 9);
 * `trans_z` zeigt wie das Original hinter das Groessenwort. */
void recvx_push_motions(RECVX_PUSH_CORE*,const unsigned* frames,unsigned count,const float* trans_z,unsigned trans_count);
/* sys->sp_flg: bhControlObjItm laeuft nur mit Bit 0x4. */
void recvx_push_enabled(RECVX_PUSH_CORE*,unsigned sp_flg);
/* rom->walp gefolgt von den sys->mwalp-Eintraegen, die vor den Kisten
 * vergeben wurden (Gegner, game.c:48-51).  Die Kisteneintraege folgen danach. */
void recvx_push_update_walls(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA* prefix,unsigned n);
/* op->px/py/pz, wie sie ein Raumskript gesetzt haben kann. */
void recvx_push_update_object(RECVX_PUSH_CORE*,unsigned,float,float,float);
void recvx_push_object(RECVX_PUSH_CORE*,unsigned,float[3]);
/* bhControlObjItm: bhObj001 fuer jede Kiste in Objektreihenfolge.  `plp` ist
 * der Spielerzustand (mode3/spd/ay; mode3 kann auf 6 gesetzt werden),
 * `psh_idx`/`stflg` sind plp->psh_idx/plp->stflg, `cb_flg` sys->cb_flg. */
void recvx_push_control(RECVX_PUSH_CORE*,RECVX_PUSH_STATE* plp,unsigned cb_flg,int psh_idx,unsigned stflg);
/* Der Spielerwandpass (bhCheckWallEx) setzt attr 0x20000 auf Kistenwaenden;
 * `attr` sind die Attribute der Kisteneintraege in sys->mwalp-Reihenfolge. */
unsigned recvx_push_wall_count(RECVX_PUSH_CORE*);
void recvx_push_wall_attributes(RECVX_PUSH_CORE*,const unsigned* attr,unsigned n);
/* bhCPM2_act_wlk (player.c:2403): psh_ct > 5 ohne stflg 0x80 -> mode2 18. */
int recvx_push_begin(RECVX_PUSH_STATE*,int psh_ct,float x,float y,float z,int ay,int dmlvl,int wpntp);
/* Ein 30-Hz-Spielerdurchlauf: bhControlPlayerPad, bhCPM2_act_psh, bhSetMotion
 * und die flg-0x400000-Rechnung.  `forward` ist sys->pad_on & 0x1. */
void recvx_push_tick(RECVX_PUSH_CORE*,RECVX_PUSH_STATE*,int forward);
/* sys->mwalp (nur Kisteneintraege), sys->metcp, sys->mflrp. */
unsigned recvx_push_walls(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA**);
unsigned recvx_push_steps(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA**);
/* attr |= set_bits; attr &= ~clear_bits eines sys->metcp-Eintrags
 * (bhSetUseKaidanFlag/bhClrUseKaidanFlag, hitchk.c:4965/5003). */
void recvx_push_step_attr(RECVX_PUSH_CORE*,unsigned index,unsigned set_bits,unsigned clear_bits);
unsigned recvx_push_floors(RECVX_PUSH_CORE*,const RECVX_STAIR_AREA**);
#ifdef __cplusplus
}
#endif
