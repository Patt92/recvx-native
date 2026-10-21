#pragma once
/* ===========================================================================
   A01 Laufsteuerung des Spielers — eigenstaendige native Bestimmung von
     player.c:1820 bhControlPlayerPad (Laufanteil)
     player.c:1957 bhCPM1_act_bas  (mode2 0..13)
     player.c:2085 bhCPM2_act_std  :2157 _srt  :2229 _sta  :2280 _wlk
     player.c:2416 bhCPM2_act_run  :2564 _bak  :2716 _bk2
     player.c:1229 bhCheckPlayerKegaMotion
     player.c:1419 bhControlPlayer, Kehrtwende ueber flg 0x100000 / yrct
     pwksub.c:133  bhAddSpeed      :144 bhGetFrameNum
     Motion.c:21   bhSetMotion (nur Uhr: hokan_count, frm_no, Umbruch)
   Semantikreferenz, kein Originalkoerper, kein Generator.

   Besitz: RECVX_PLAYER_ACTION haelt genau die plp-/EXP_WORK-Felder, die diese
   Routinen lesen und schreiben. flg/flg2/stflg/st_flg/EXP1_I(0) gehoeren den
   vorhandenen Besitzern (EventVm-Flagbaenke, Kopfzustand) und kommen per
   Zeiger. mode0/mode1 bleiben beim Spielerbesitzer bzw. der Kampfportierung
   (bhCPM0_action ruft diese Routinen nur mit mode1 == 0).
   Die Spezialaktionen mode2 14..22 (Treppe, Schieben, ...) haben ihre eigenen
   nativen Besitzer; recvx_player_action_bas fuehrt sie nicht aus. Ausnahme ist
   player.c:3887 bhCPM2_act_cro (mode2 19, Hockaufnahme): sie liest und schreibt
   nur Felder dieses Werks und laeuft deshalb hier.

   Nicht dargestellt (gemeldet ueber `gap`, kein Ersatzwert):
   - die Posenueberblendung selbst (hokan_rate); hokan_count wird wie in
     bhSetMotion gefuehrt, weil act_std und KegaMotion ihn lesen.
   =========================================================================== */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RECVX_PLAYER_ACTION {
    /* plp */
    uint32_t mode2, mode3;
    int32_t  ct0, ct3;
    uint32_t mtn_no;
    int32_t  frm_no, mtn_add;
    int32_t  hokan_rate;
    uint32_t hokan_count;
    float    px, pz, spd;
    int32_t  ay, ayp;
    /* EXP_WORK */
    int32_t  wpntp, dmlvl;
    float    arp, rtspd;
    float    spx, spz, spxn, spzn;
    int32_t  yrct;
} RECVX_PLAYER_ACTION;

typedef struct RECVX_PLAYER_ACTION_IO {
    /* Felder anderer Besitzer */
    uint32_t* flg;       /* plp->flg   */
    uint32_t* flg2;      /* plp->flg2  */
    uint32_t* stflg;     /* plp->stflg */
    uint32_t  stflg_owned; /* stflg-Bits anderer Besitzer, nur gelesen (Gift 0x80000/0x200000: Inventar) */
    uint32_t* st_flg;    /* sys->st_flg */
    uint32_t* cb_flg;    /* sys->cb_flg (act_cro: 0x10 = GetItem-Anforderung an bhCheckSubTask) */
    uint32_t* exp1;      /* EXP1_I(0); NULL = vom Kopfbesitzer gefuehrt */
    uint32_t  pad_on, pad_old, pad_ps;  /* sys->pad_on/_old/_ps; act_sta loescht in pad_on */
    float     pad_dx, pad_ax;           /* sys->pad_dx/pad_ax (Analogausschlag) */
    int       analog_valid;             /* 0: kein Analogausschlag vorhanden */
    uint32_t  gm_flg;
    int32_t   ply_id, wpnr_no, hp;
    uint32_t  psh_ct;
    /* Dienste der vorhandenen Besitzer; NULL wird namentlich gemeldet. */
    void* user;
    /* plp->mnwPb[mtn].frm_num der eigenen Spielerbibliothek, 0 = fehlt */
    uint32_t (*motion_frames)(void* user, uint32_t mtn_no);
    /* bhGetTransZ(curve): Element 0 Metadaten, Schluessel ab 1. count = Laenge. */
    const float* (*trans_z)(void* user, uint32_t curve, uint32_t* count);
    /* bhCheckFloorSound(plp, flr_no, owP[node].mtx[12/14]) +
       CallPlayerFootStepSe(fsnd, type, 1) */
    void (*footstep)(void* user, uint32_t node, uint32_t type);
    /* bhSetEffect(108, &plp->px, NULL, ct) */
    void (*effect108)(void* user, int32_t ct);
    /* bhSetWaterSplash(plp, node, type, s, s, s) */
    void (*water_splash)(void* user, int32_t node, int32_t type, float s);
    /* bhSetWaterSplash3(&owP[node].mtx[12], ay, footeff, .8f, .8f, .8f) */
    void (*water_splash3)(void* user, uint32_t node, int32_t ay);
    /* bhSearchNearEnemyB(&EXP1_I(12), ay, 13653, 30.0f) */
    int  (*search_near_enemy_b)(void* user, int32_t ay);
    void (*gap)(void* user, const char* name);
} RECVX_PLAYER_ACTION_IO;

/* bhSetPlayer / bhResetPlayer: Spieleraktion in Ruhe (mode2/mode3 = 0),
   Lage, sichere Lage spx/spz, wpntp aus wpnr_no (nur bhSetPlayer setzt es). */
void recvx_player_action_reset(RECVX_PLAYER_ACTION* a, float px, float pz, int32_t ay,
                               int32_t wpntp, int32_t dmlvl, uint32_t mtn_no);

/* player.c:1114 bhStandPlayerMotion. Dienste der uebrigen Besitzer; ein fehlender Dienst
   wird ueber `gap` namentlich gemeldet, nie ersetzt. */
typedef struct RECVX_PLAYER_STAND_IO {
    uint32_t* flg;         /* plp->flg   (:1136/:1138) */
    uint32_t* stflg;       /* plp->stflg (:1118 gelesen, :1140 geschrieben) */
    uint32_t  stflg_owned; /* stflg-Bits anderer Besitzer, nur gelesen (Gift 0x80000/0x200000: Inventar) */
    int32_t   hp;          /* plp->hp (:1116/:1126) */
    void* user;
    /* :1142 *(int*)&plp->mode0 = 1: mode0 1 / mode1 0 beim Besitzer von mode0/mode1
       (mode2/mode3 = 0 schreibt die Routine selbst im Werk). */
    void (*mode_reset)(void* user);
    /* :1154 plp->mnwP = plp->mnwPb: die eigene Spielerbibliothek (keine Raum-/Gegnermotion). */
    void (*own_motion_bank)(void* user);
    /* :1156 bhSetMotion(plp, 0, 0, NULL) und :1158 bhCalcModel(plp) am Posenbesitzer, nachdem
       mtn_no/frm_no/hokan im Werk stehen. Rueckgabe 0: mnwP[mtn_no].datP fehlt (Motion.c:29
       tastet dann nicht ab) - wird als Luecke gemeldet. */
    int  (*set_motion)(void* user, uint32_t mtn_no);
    void (*gap)(void* user, const char* name);
} RECVX_PLAYER_STAND_IO;

/* player.c:1114 bhStandPlayerMotion, in Originalreihenfolge:
   :1116-1134 EXP0_I(5) dmlvl nach hp (>= 120: stflg 0x280000 -> 1 sonst 0; >= 30: 1; sonst 2),
   :1136 flg &= 0xC96EFFFB, :1138 flg |= 0x8, :1140 stflg &= ~0x18000,
   :1142 *(int*)&mode0 = 1 (mode2/mode3 = 0 hier, mode0/mode1 ueber io->mode_reset),
   :1144/:1145 PEXP0_F(72/80) = EXP_WORK.spx/spz = px/pz (a->px/pz muessen plp->px/pz sein),
   :1147 mtn_no = PlMtnAct[wpntp][dmlvl][0], :1149/:1150 hokan_rate/hokan_count = 0,
   :1152 frm_no = 0, :1154 mnwP = mnwPb, :1156 bhSetMotion(plp,0,0,NULL), :1158 bhCalcModel.
   Keine Bedingung: das Original laeuft bei jedem mode0/mode1 und auch mit gezogener Waffe. */
void recvx_player_stand_motion(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_STAND_IO* io);

/* pwksub.c:144 bhGetFrameNum, Originalvergleich frm > fnm_new eingeschlossen. */
int32_t recvx_player_action_frame_num(uint32_t fnm_old, uint32_t fnm_new, int32_t fno_now);

/* bhControlPlayerPad (nur bei !(flg & 0x10000) aufrufen, wie bhCPM0_action).
   mode1 gehoert der Kampfportierung; `mode1` darf NULL sein. */
void recvx_player_action_pad(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io, uint32_t* mode1);

/* bhCPM1_act_bas. Rueckgabe 1 = mode2 gehoert diesem Modul (0..13, 19),
   0 = Spezialaktion mit eigenem Besitzer (nichts ausgefuehrt). */
int recvx_player_action_bas(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io);

/* bhControlPlayer nach der Moduswahl: Kehrtwende (flg 0x100000, yrct). */
void recvx_player_action_turn(RECVX_PLAYER_ACTION* a, RECVX_PLAYER_ACTION_IO* io);

/* Motion.c bhSetMotion, nur die Uhr fuer mtn_md 0. frm_num = Bildzahl. */
int recvx_player_action_motion(RECVX_PLAYER_ACTION* a, uint32_t frm_num);

#ifdef __cplusplus
}
#endif
