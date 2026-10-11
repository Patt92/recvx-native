#pragma once
/* ===========================================================================
   A01-A04 Spielerkampf — eigenstaendige native Bestimmung des originalen
   Waffenzustandsautomaten, der Zielaufschaltung, der Munitionsbuchung, des
   Messers und der Trefferzuweisung. Semantikreferenz (kein Originalkoerper,
   kein Generator, kein Originalkern):
     player.c   bhCPM0_action:1777, bhControlPlayer:1349 (Kampfanteile),
                bhCPM1_act_atk:4481, bhCPM2_act_suw:4545, _wpn:4764, _wre:5198,
                _atk:5262, _rld:5953, _knf:6154
     playpch.c  PlyPchMain:18, MixSetToJointRot:150, Get/SetOneObjectMotion:
                185/231, bhSearchEnemy:318, SetLockOnDirection:401,
                bhCPM2_act_suw_pch:480, _atk_pch:602, CheckGunHit:1130,
                bhCPM2_SearchPch:1174
     pwksub.c   bhCalcLockEneYR:170, bhSearchNearEnemy:218,
                bhSearchNextEnemy:506, bhSetGunFire:2670, bhSetYakkyou:3242,
                bhSetMagazine:3372, bhAddSpeed:133
     weapon.c   bhObjWpn:197, bhActionWeapon:217, bhCheckBullet:421,
                bhCountBullet:439, bhCheckGunAtari:473, bhCheckKnifeAtari:1002,
                bhCheckCapCol2Capsule:1496
     Motion.c   bhSetMotion:21, SetMtn*:131-859, bhFixPosition:861,
                bhCalcFixOffset:872, AngZyxToYzx:969, AngYzxToZyx:991
     hitchk.c   bhCheckWallAttrB89:5881

   Besitz (verbindlich, kein zweiter Arbeitssatz):
   - RECVX_COMBAT_PLAYER ist die Kampfsicht auf den EINEN nativen Spieler-
     work. Ihr einziger Besitzer ist recvx::CombatController; sie haelt genau
     die plp-Felder, die der Kampf liest oder schreibt (Fundstellen an den
     Feldern). Lage/Winkel kommen je Pass aus dem Host-Akteur und gehen
     danach dorthin zurueck; es gibt keine BH_PWORK-Kopie und kein sys/plp.
   - Gegner sind ausschliesslich die RECVX_ENEMY_WORKs der vorhandenen
     Gegnertabelle (native/enemy_work.h, nur gelesen/beschrieben wie im
     Original). Keine eigene Ziel- oder Schadensliste.
   - Inventar (swork.pip), Effekte, Licht, Ton, Wandabfragen und Vibration
     sind Dienste ihrer vorhandenen Besitzer. Ein ungebundener Dienst wird
     ueber `gap` mit dem Originalnamen gemeldet und tut nichts.
   =========================================================================== */
#include <stdint.h>
#include "enemy_work.h"
#include "combat_geometry.h"
#ifdef __cplusplus
extern "C" {
#endif

#define RECVX_CB_NODES 64u   /* NormalTbl (Motion.c:7) adressiert 64 Knoten */

/* WPN_TAB (player.h:7, 0x68 Byte). Layoutgleich zu recvx::WPN_TAB. */
typedef struct {
    uint32_t flg, atack_ct, fend_ct, at_cct;
    int16_t  ef_yct, act_ct0, act_ct1, act_ct2;
    float    r, l, rn, rmax;
    RECVX_CB_VEC wp_fps1, wp_fps2, wp_cps;
    uint8_t  ltp, lr, lg, lb;
    uint16_t lnr, lfr;
    float    hrate, ef_scale;
    int16_t  snd_wpno, hiteff;
    uint16_t seno0, seno1;
    int8_t   vib_tp, vib_ct, reserve1, reserve2;
} RECVX_CB_WPN_TAB;

/* EF_WORK (types.h:212, 0x28 Byte). Layoutgleich zu recvx::EF_WORK. */
typedef struct {
    uint32_t flg;
    uint16_t id, type;
    int16_t  flr_no;
    uint16_t mdlver;
    float    px, py, pz, sx, sy, sz;
    int16_t  ay, ax;
} RECVX_CB_EF_WORK;

/* Originaldatentabellen (native/combat_tables_data.inc, weapon.c:116). Das
   Original haelt WpnEffTab als veraenderliche globale Tabelle
   (pwksub.c:2748/3266 schreiben px/py/pz/ay/ax/flg); der Zeiger zeigt auf
   genau diese eine Tabelle, keine Kopie. */
typedef struct {
    const RECVX_CB_WPN_TAB* wpn;          /* WpnTab[23]        */
    RECVX_CB_EF_WORK (*eff)[4];           /* WpnEffTab[23][4]  */
    const uint16_t* mtn_wpn;              /* PlMtnWpn[5]       */
    const int32_t*  knf_atr;              /* KnfAtrTab[4]      */
    const int16_t (*dam_near)[22];        /* EneDamNear[31][22]*/
    const int16_t (*dam_mid)[22];
    const int16_t (*dam_far)[22];
    const int32_t (*ang)[3];              /* WpnAngTab[21][3]  */
} RECVX_CB_TABLES;

/* MN_WORK (types.h:63) einer Spielermotion: flg wie der MTN-Kopf (Byte 3,
   native/actor.cpp:52), frm_num/obj_num wie im Original.
   `has_data` ist `mnwP[mtn].datP != NULL` (bhSetMotion Motion.c:29); die
   eigentlichen Schluesselkanaele liegen beim vorhandenen Posenbesitzer
   (PlayerAsset) und werden ueber den Dienst `motion_angle`
   (GetOneObjectMotion, playpch.c:185) gelesen. Ein eigener Zeiger auf
   NJS_MKEY_A-Speicher waere eine zweite Motionquelle. */
typedef struct {
    uint32_t flg;
    int32_t  obj_num, frm_num;
    int32_t  has_data;
    const uint16_t* atrP;
} RECVX_CB_MOTION;

typedef struct { float pos[3]; int32_t ang[3]; } RECVX_CB_OBJECT; /* NJS_CNK_OBJECT pos/ang */

/* playpch.h:7 search */
typedef struct RECVX_CB_SEARCH {
    struct RECVX_CB_SEARCH* nextP;
    RECVX_ENEMY_WORK* ewP;
    int32_t idx;
    float   dst;
    int32_t dir;
} RECVX_CB_SEARCH;

/* playpch.h:17 PP_WORK (exp2, bhGetFreeMemory-genullt bei PlyPchInit). */
typedef struct {
    int32_t mode, count, gun_delay;
    int32_t obj_r_ang[3], obj_l_ang[3];   /* obj_r/obj_l: nur .ang wird gelesen */
    int32_t ang_tmp[3];
    int32_t arm_dl, arm_dr, arm_al, arm_ar;
    uint32_t lck_n;                        /* endPP - LckTbl                    */
    RECVX_ENEMY_WORK* lck_tbl[64];
    RECVX_CB_SEARCH sch_hed;
    RECVX_CB_SEARCH sch_lst[64];
    int32_t hed_rate;
} RECVX_CB_PP_WORK;

/* Kampfsicht auf den Spielerwork (Fundstellen: types.h:238 BH_PWORK,
   player.h:43 EXP_WORK, playpch.h:17 PP_WORK). */
typedef struct {
    uint32_t flg, flg2, stflg, at_flg;
    uint8_t  mode0, mode1, mode2, mode3;
    int32_t  ct0, ct1;
    float    px, py, pz, spd;
    int32_t  ax, ay, az, ayp;
    /* BH_PWORK haelt wax..wazp als short (types.h:238, vgl. enemy_work.h:214);
       der Kampf rechnet mit genau dieser Breite. */
    int16_t  wax, waxp, way, waz;
    int32_t  ct3;
    int32_t  src_no, wpnr_no, idx_ct, ply_id;
    uint32_t mtn_no, mtn_md, mtn_attr;
    int32_t  frm_no, mtn_add, frm_mode;
    int32_t  hokan_rate;
    uint32_t hokan_count, hokan_ctbak;
    /* exp0 (EXP_WORK): yrct, rtspd und der arp-Zielwert der Vorfuehlerweite;
       arn selbst gehoert dem vorhandenen Bewegungsbesitzer (B00). */
    int32_t  yrct;
    float    rtspd, arp;
    uint32_t exp1_flags;                   /* EXP1_I(0)                         */
    RECVX_CB_PP_WORK pp;                   /* exp2                              */
    /* mlwP: Objektbaum (objP) und O_WORK.flg der logischen Knoten; owP-Matrizen
       sind die des letzten bhCalcModel des Host-Besitzers. `obj` traegt je Pass
       die Pose, die das Motionsampling des Besitzers geschrieben hat (im
       Original ist genau das objP); `obj_dirty` markiert bitweise die Knoten,
       die der Kampf in diesem Pass selbst geschrieben hat (PlyPchMain
       playpch.c:112-130). Nur diese gehen als Ueberschreibung zurueck. */
    uint32_t nodes;
    RECVX_CB_OBJECT obj[RECVX_CB_NODES];
    uint32_t obj_dirty[2];
    uint32_t owp_flg[RECVX_CB_NODES];
    float    owp_mtx[RECVX_CB_NODES][16];
    /* mnwP: die vorhandenen Spielermotions (Basis + bhReadWeaponData). */
    const RECVX_CB_MOTION* mnw;
    uint32_t mnw_n;
} RECVX_COMBAT_PLAYER;

/* Waffenobjekt sys->obwp[0] (bhSetWeapon weapon.c:128): die vom Kampf
   gesetzten Animationsbits und ihr Zustand (bhActionWeapon). */
typedef struct {
    uint32_t flg;
    uint8_t  mode0;
    int32_t  type, lkono;
    int32_t  ax, ay, az;                   /* bhObjWpn                          */
    float    part_pos0;                    /* objP[2].pos[0] (Schlitten)        */
    int32_t  part_ang1;                    /* objP[2].ang[1] (Trommel)          */
    int32_t  part2_hidden_cleared;         /* objP[2].evalflags &= ~8 (wno 12)  */
} RECVX_CB_WEAPON_OBJECT;

/* SYS_WORK-Felder, die der Kampf liest/schreibt. Zeigerfelder zeigen auf die
   Speicher des vorhandenen Besitzers (EventVm-Flagbaenke); NULL = Luecke. */
typedef struct {
    uint32_t pad_on, pad_ps;               /* bhSetPad (pad.c:162)              */
    int32_t  pad_dx, pad_ax;
    uint32_t eor_ct, gm_mode, cam_ncut;
    uint32_t* gm_flg;                       /* bhCk 11                           */
    uint32_t* st_flg;                       /* bhCk 5                            */
    uint32_t* sp_flg;                       /* bhCk 6                            */
    uint32_t ef_flg;
    uint32_t yk_ct, mg_ct;                  /* Huelsen-/Magazinzaehler           */
    /* Kameraschreibzugriffe des Originals aus dem Kampfpfad (player.c:1783-1787
       bhCPM0_action, :4677 act_suw, :6113 act_rld). Der Kamerabesitzer liegt
       ausserhalb dieses Pakets; `cam_written` nennt die geschriebenen Felder
       (Bit 0 pe_ax, Bit 1 pe_pers), damit der Host sie weiterleiten oder als
       Anschlussvertrag melden kann. Nie ein Ersatzwert. */
    int32_t  cam_pe_ax, cam_pe_pers;
    uint32_t cam_written;
} RECVX_COMBAT_SYS;

/* Wandtreffer von bhCheckL2Wall: sys->apos und der vollstaendige ATR-Satz
   sys->ahtp, `id` eine stabile Identitaet fuer die Rueckschreibung. */
typedef struct { RECVX_CB_VEC apos; RECVX_ENEMY_ATR atr; uint32_t id; } RECVX_CB_WALL_HIT;

/* bhSetEffectTb-Anforderung: Vorlage (EF_WORK), Versatz ops (optional),
   Verknuepfung mit dem Spieler (lkp = plp) und Gelenk jno. `has_matrix`
   traegt die Matrix, die bhSetGunFire nach erfolgreicher Allokation in
   op->mtx schreibt (pwksub.c:2814-2820 und Sonderdrehungen). */
typedef struct {
    RECVX_CB_EF_WORK ef;
    int32_t has_offset; RECVX_CB_VEC offset;
    int32_t link_player; int32_t jno;
    int32_t has_matrix; float matrix[16];
} RECVX_CB_EFFECT;

/* rom->lgtp[0] durch bhSetGunFire (pwksub.c:2684-2733). */
typedef struct {
    uint32_t flg, aspd, lkflg, lkno, lkono, mode, lsrc, type;
    float r, g, b; uint32_t nr, fr; float lx, ly, lz;
} RECVX_CB_LIGHT;

typedef struct {
    void* user;
    void    (*gap)(void* user, const char* original);
    /* Inventar (swork.pip ueber recvx::Inventory) */
    int16_t (*check_bullet)(void* user);            /* bhCheckBullet  weapon.c:421 */
    int32_t (*count_bullet)(void* user);            /* bhCountBullet  weapon.c:439 */
    int32_t (*search_bullet)(void* user);           /* bhSearchBullet sub1.c:8862  */
    int32_t (*event_flag)(void* user, uint32_t no);  /* bhCkFlg(sys->ev_flg, no)    */
    void    (*set_event_flag)(void* user, uint32_t no); /* bhStFlg(sys->ev_flg, no) */
    void    (*pt_flg_or)(void* user, uint32_t bits); /* sys->pt_flg |= bits         */
    /* Gegnertabelle (C02): sys->ene / sys->ewk_n */
    RECVX_ENEMY_TABLE* (*enemies)(void* user);
    /* Kollision (Besitzer P03/E01) */
    int32_t (*l2_wall)(void* user, const RECVX_CB_LINE* line, uint32_t flg, float* len, RECVX_CB_WALL_HIT* hit); /* bhCheckL2Wall pwksub.c:718 */
    int32_t (*c2_wall_n)(void* user, const RECVX_CB_CAPSULE* cap, uint32_t flg);   /* bhCheckC2WallN pwksub.c:2172 */
    int32_t (*l2_water)(void* user, const RECVX_CB_LINE* line, RECVX_CB_VEC* hit);  /* bhCheckL2Water */
    int32_t (*check_water)(void* user, const RECVX_CB_VEC* pos);                    /* bhCheckWater   */
    /* Wandzerstoerung (weapon.c:584-595): bhSetExplosion(&ps),
       sys->obwp[prm3].mdflg |= 1, ahtp->flg &= ~1 */
    void    (*explosion)(void* user, const RECVX_CB_VEC* pos);
    void    (*object_mdflg_or)(void* user, uint32_t object, uint32_t bits);
    void    (*wall_disable)(void* user, uint32_t id);
    /* Effekte (Besitzer E-Pakete) */
    int32_t (*effect)(void* user, const RECVX_CB_EFFECT* request);  /* bhSetEffectTb -> Slot/-1 */
    void    (*effect_clear)(void* user, uint32_t id, int32_t flr_no); /* rld: eff[j].flg = 0 */
    void    (*gun_spark)(void* user, const RECVX_CB_VEC* pos, const RECVX_CB_VEC* vec, uint32_t col0, uint32_t col1, int32_t type); /* bhSetEffGunSpark */
    void    (*gun_splash)(void* user, const RECVX_CB_VEC* gvec, const RECVX_CB_VEC* pos, int32_t wattr);  /* bhSetGunSplash weapon.c:1632 */
    void    (*water_splash4)(void* user, const RECVX_CB_VEC* pos, int32_t ang, int32_t type, float sx, float sy, float sz); /* bhSetWaterSplash4 */
    void    (*player_water)(void* user, int32_t effect_param, int32_t splash_type, float scale); /* bhSetEffect(108)+bhSetWaterSplash am Spieler */
    void    (*light0)(void* user, const RECVX_CB_LIGHT* light);
    /* Ton/Vibration */
    void    (*weapon_se)(void* user, const RECVX_CB_VEC* pos, int32_t se, int32_t slot); /* CallPlayerWeaponSeEx */
    void    (*vibration)(void* user, int32_t port, int32_t type);                     /* StartVibrationEx */
    void    (*footstep)(void* user, int32_t floor, float x, float z);                 /* CallPlayerFootStepSe(bhCheckFloorSound(...),1,1) */
    /* GetOneObjectMotion (playpch.c:185): die NJS_MKEY_A-Winkelschluessel des
       Knotens `node` im Frame `frame` der Motion `mtn_no`, aus der vorhandenen
       Motionquelle des Posenbesitzers. 0 = kein Winkelkanal; das Original
       liefert dann 0/0/0. */
    int32_t (*motion_angle)(void* user, uint32_t mtn_no, uint32_t node, uint32_t frame, int32_t* ang3);
} RECVX_COMBAT_SERVICES;

/* Eingabe/Ausgabe eines Passes. Zaehler laufen ueber die Lebensdauer des
   Kerns, Zustandsfelder spiegeln den Stand nach dem Pass. */
typedef struct {
    int active;            /* mode1 != 0 (bhCPM1_act_atk besitzt den Pass)      */
    int entered, released; /* Eintritt/Austritt in diesem Pass                  */
    uint32_t shots;        /* Anzahl bhSetGunFire-Aufrufe seit Erzeugung        */
    uint32_t knife_checks; /* Anzahl bhCheckKnifeAtari-Aufrufe                  */
    uint32_t hits;         /* gemeldete Treffer (bhCheckGunAtari/Knife)         */
    uint32_t empty;        /* SE 260 (leer)                                    */
    uint32_t shells;       /* bhSetYakkyou-Aufrufe                              */
    uint32_t magazines;    /* bhSetMagazine-Aufrufe                             */
    uint32_t reloads;      /* Eintritte in mode2 70 (bhCPM2_act_rld)            */
    uint32_t gun_atari;    /* bhCheckGunAtari-Aufrufe                           */
    uint32_t knife_wall;   /* Messer an der Wand (weapon.c:1086-1089)           */
    /* Zustand nach dem Pass, zur Pruefung der Originalfolgen. */
    uint8_t  mode1, mode2, mode3;
    uint32_t at_flg, flg, stflg;
    uint32_t mtn_no; int32_t frm_no;
    int32_t  src_no;       /* aufgeschaltetes Ziel (plp->src_no), -1 = keines   */
    uint32_t locked;       /* pp.lck_n: Eintraege in LckTbl                     */
} RECVX_COMBAT_RESULT;

typedef struct RECVX_COMBAT_CORE RECVX_COMBAT_CORE;

/* Reine Originaldatentabellen des Spielerkampfs, die in
   native/combat_tables_data.inc fehlen. Sie liegen hier, weil
   combat_tables_data.inc einem anderen Paket gehoert; der Integrator-Patch
   traegt sie dort nach. Regressionstest gegen die Quelle:
   tests/native/combat_state_test.cpp.
     weapon.c:118  WpnAngTab[21][3]   Ruhewinkel des Waffenobjekts (bhObjWpn)
     player.c:214  PlyLegRoute[2][7]  Knotenroute der Fussfixierung           */
extern const int32_t RECVX_CB_WPN_ANG_TAB[21][3];
extern const int8_t  RECVX_CB_PLY_LEG_ROUTE[2][7];

/* Besitzer eines bhSetGunFire-Aufrufs ausserhalb des Spielerpasses
   (bhWeaponSeCall event.c:2988): stflg/at_flg/idx_ct und owP[jno].mtx. */
typedef struct { uint32_t stflg, at_flg; int32_t idx_ct; float mtx[16]; } RECVX_CB_GUN_OWNER;
/* pwksub.c:2670 bhSetGunFire fuer einen vorhandenen Work; keine Kampfzustaende. */
void recvx_combat_gun_fire(const RECVX_CB_TABLES* tables, const RECVX_COMBAT_SERVICES* services,
                           RECVX_COMBAT_SYS* sys, const RECVX_CB_GUN_OWNER* owner,
                           int32_t wno, int32_t jno, int32_t hand, int32_t ang);
RECVX_COMBAT_CORE* recvx_combat_create(const RECVX_CB_TABLES* tables, const RECVX_COMBAT_SERVICES* services);
void recvx_combat_destroy(RECVX_COMBAT_CORE* core);
RECVX_COMBAT_PLAYER* recvx_combat_player(RECVX_COMBAT_CORE* core);
RECVX_CB_WEAPON_OBJECT* recvx_combat_weapon_object(RECVX_COMBAT_CORE* core);
/* PlyPchInit (playpch.c:12) + die Raumladeanteile von bhInitPlayer/
   bhResetPlayer, die der Kampf liest (owP[7]/[11].flg |= 8, player.c:850/944). */
void recvx_combat_room_reset(RECVX_COMBAT_CORE* core);
/* bhSetWeapon (weapon.c:128): Waffenobjekt und plp->flg 0x20000. */
void recvx_combat_set_weapon(RECVX_COMBAT_CORE* core, int32_t wpnr_no);
/* Ein bhControlPlayer-Pass bis vor bhCalcModel: bhCPM0_action (Eintritt bzw.
   mode1-Ruecksetzung), bhCPM1_act_atk, yrct-Drehung, PlyPchMain, bhSetMotion,
   flg 0x400000, bhFixPosition, bhAddSpeed (flg 0x20000000), bhActionWeapon. */
void recvx_combat_tick(RECVX_COMBAT_CORE* core, RECVX_COMBAT_SYS* sys, RECVX_COMBAT_RESULT* out);
/* Nach bhCalcModel (owp_mtx neu): Messerabfrage player.c:1689-1724,
   st_flg 0x4 bei flg 0x4 (player.c:1726) und bhObjWpn. */
void recvx_combat_after_model(RECVX_COMBAT_CORE* core, RECVX_COMBAT_SYS* sys, RECVX_COMBAT_RESULT* out);
/* Motion.c:969 AngZyxToYzx / :991 AngYzxToZyx, wie SetOneObjectMotion
   (playpch.c:231) sie fuer die Knotenblende nutzt; fuer Regressionstests. */
void recvx_combat_ang_zyx_to_yzx(const int32_t zyx[3], int32_t yzx[3]);
void recvx_combat_ang_yzx_to_zyx(const int32_t yzx[3], int32_t zyx[3]);

#ifdef __cplusplus
}
#endif
