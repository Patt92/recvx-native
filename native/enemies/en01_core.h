#ifndef RECVX_NATIVE_EN01_CORE_H
#define RECVX_NATIVE_EN01_CORE_H
/* ===========================================================================
   T2 — EN01-Zustandsmaschine (Zombie), nativ eigenstaendig bestimmt
   ---------------------------------------------------------------------------
   Die Fundstellen (en01.c / en01b.c / en01sub.c <zeile>) sind reine
   Semantikreferenz. Kein Originalkoerper wird kopiert, generiert,
   kompiliert, gelinkt oder aufgerufen. Zustandsbesitzer bleibt ausschliess-
   lich RECVX_ENEMY_WORK (native/enemy_work.h) samt dem in native/en01_work.h
   beschriebenen exp0-Block; diese Uebersetzung haelt keinen eigenen Zustand.

   Deklariert sind hier NUR die Funktionen, die native/en01_work.h noch nicht
   nennt (dessen Prototypen gelten unveraendert). Die beiden Kindfamilien
   (en01b.c = verknuepfter Oberkoerper, en01sub.c = Kopf/Arm/Bein/Kappe/Wurm/
   Bombe/Zielfernrohr/Eltern) haben je einen eigenen mode0-Eintritt, genau wie
   im Original.
   =========================================================================== */
#include <stdint.h>
#include "enemy_work.h"
#include "en01_work.h"
#include "en01_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Datenbeschaffung (en01_tables.h, Abschnitt "Grosse Datenfelder") ----
   Ohne diesen Aufruf bleiben alle Datenzeiger NULL und die betroffenen
   Mechanismen melden ihre Luecke. recvx_en01_default_data() (en01_tables.h)
   liefert den vollstaendigen Satz. */
void recvx_en01_set_data(const RECVX_EN01_DATA* data);

/* --- zonzon.c:101 bhDGCdirCheck -----------------------------------------
   Oeffentlich, weil der Spielerbesitzer in player_hurt_state denselben
   Vergleich braucht (en01.c:3292: plp->mode1 aus bhDGCdirCheck(plp->dv,
   plp->ay)) und plp->dvx/dvy/dvz nicht Teil von RECVX_ENEMY_PLAYER sind.
   Der Besitzer MUSS diese Routine aufrufen und keine eigene bauen. */
int recvx_en01_dgc_dir_check(const RECVX_ENEMY_VEC* dv, int32_t rot);

/* --- Hauptfamilie (id 1), Zustandsfunktionen hinter dem mode0-Eintritt --- */
void recvx_en01_move(RECVX_ENEMY_WORK* w);            /* en01.c:2833 */
void recvx_en01_nage(RECVX_ENEMY_WORK* w);            /* en01.c:2865 */
void recvx_en01_damage(RECVX_ENEMY_WORK* w);          /* en01.c:2884 */
void recvx_en01_die(RECVX_ENEMY_WORK* w);             /* en01.c:2905 */
void recvx_en01_player_control(RECVX_ENEMY_WORK* w);  /* en01.c:2941 */
void recvx_en01_player_link(RECVX_ENEMY_WORK* w);     /* en01.c:3012 */
void recvx_en01_calc_enemy(RECVX_ENEMY_WORK* w);      /* en01.c:3102 */
void recvx_en01_set_blood(RECVX_ENEMY_WORK* w, int hp);           /* :3335 */
void recvx_en01_dmg_check_type00(RECVX_ENEMY_WORK* w);            /* :3449 */
void recvx_en01_dmg_check_type02(RECVX_ENEMY_WORK* w);            /* :3743 */
int  recvx_en01_check_exp_head(RECVX_ENEMY_WORK* w);              /* :3710 */
void recvx_en01_damage_add(RECVX_ENEMY_WORK* w);                  /* :3769 */
void recvx_en01_parts_damage_check(RECVX_ENEMY_WORK* w);          /* :3851 */
int  recvx_en01_dmg_mode_jump_check(RECVX_ENEMY_WORK* w);         /* :3898 */
void recvx_en01_coll_check(RECVX_ENEMY_WORK* w);                  /* :3995 */
void recvx_en01_coll_check_push(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* top); /* :4078 */
int  recvx_en01_enemy_hit_chk(RECVX_ENEMY_WORK* w, RECVX_ENEMY_WORK* t, int rng); /* :4186 */
void recvx_en01_coll_check_wall(RECVX_ENEMY_WORK* w);             /* :4206 */

/* --- InitType-Familie (en01.c:2217 Dispatch) ------------------------------ */
void recvx_en01_init_type00(RECVX_ENEMY_WORK* w);     /* :4430 */
void recvx_en01_init_type02(RECVX_ENEMY_WORK* w);     /* :4455 */
void recvx_en01_init_type06(RECVX_ENEMY_WORK* w);     /* :4480 */
void recvx_en01_init_type10(RECVX_ENEMY_WORK* w);     /* :4505 */
void recvx_en01_init_type11(RECVX_ENEMY_WORK* w);     /* :4513 */
void recvx_en01_init_type15(RECVX_ENEMY_WORK* w);     /* :4531 */

/* --- Brain / Aktionsauswahl ---------------------------------------------- */
void recvx_en01_ene_search(RECVX_ENEMY_WORK* w);      /* en01.c:4551 */
void recvx_en01_brain00(RECVX_ENEMY_WORK* w);         /* :4603 */
void recvx_en01_brain02(RECVX_ENEMY_WORK* w);         /* :4661 */
int  recvx_en01_action_mode_check(RECVX_ENEMY_WORK* w);           /* :4691 */

/* --- Move-Familie (en01.c:2237 MoveTypeW, :2257 MoveMode2W) -------------- */
void recvx_en01_mv_type00(RECVX_ENEMY_WORK* w);       /* :4862 */
void recvx_en01_mv_type02(RECVX_ENEMY_WORK* w);       /* :4891 */
void recvx_en01_mv00(RECVX_ENEMY_WORK* w);            /* :4940 */
void recvx_en01_mv01(RECVX_ENEMY_WORK* w);
void recvx_en01_mv02(RECVX_ENEMY_WORK* w);
void recvx_en01_mv03(RECVX_ENEMY_WORK* w);
void recvx_en01_mv04(RECVX_ENEMY_WORK* w);
void recvx_en01_mv05(RECVX_ENEMY_WORK* w);
void recvx_en01_mv06(RECVX_ENEMY_WORK* w);
void recvx_en01_mv07(RECVX_ENEMY_WORK* w);
void recvx_en01_mv08(RECVX_ENEMY_WORK* w);
void recvx_en01_mv09(RECVX_ENEMY_WORK* w);
void recvx_en01_mv10(RECVX_ENEMY_WORK* w);
void recvx_en01_mv11(RECVX_ENEMY_WORK* w);
void recvx_en01_mv12(RECVX_ENEMY_WORK* w);
void recvx_en01_mv14(RECVX_ENEMY_WORK* w);
void recvx_en01_mv15(RECVX_ENEMY_WORK* w);
void recvx_en01_mv16(RECVX_ENEMY_WORK* w);

/* --- Nage-Familie (en01.c:2278, :2298) ------------------------------------ */
void recvx_en01_ng_type00(RECVX_ENEMY_WORK* w);       /* :6735 */
void recvx_en01_ng00(RECVX_ENEMY_WORK* w);            /* :6753 */
void recvx_en01_ng01(RECVX_ENEMY_WORK* w);
void recvx_en01_ng02(RECVX_ENEMY_WORK* w);

/* --- Spielergriff-Schaden (en01.c:7509/:7810, PlyDmgMode :2374) ----------
   Das Original hat die Signatur (BH_PWORK* pl, BH_PWORK* epw): pl ist der
   Spieler, epw der Gegner. Nativ ist w der Gegner (epw) und pl die Sicht
   auf den Spielerbesitzer; die Funktion schreibt ihre Aenderungen an pl
   selbst ueber services->player_set zurueck. */
void recvx_en01_ply_dg00(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl);
void recvx_en01_ply_dg01(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl);
void recvx_en01_enemy_push_chk(RECVX_ENEMY_WORK* w, int mode);    /* :8008 */

/* --- Damage-Familie (en01.c:2306 DamageTypeW, :2326 DamageMode2W) -------- */
void recvx_en01_dg_type00(RECVX_ENEMY_WORK* w);       /* :8131 */
void recvx_en01_dg_type02(RECVX_ENEMY_WORK* w);       /* :8137 */
void recvx_en01_dg00(RECVX_ENEMY_WORK* w);            /* :8143 */
void recvx_en01_dg01(RECVX_ENEMY_WORK* w);
void recvx_en01_dg02(RECVX_ENEMY_WORK* w);
void recvx_en01_dg03(RECVX_ENEMY_WORK* w);
void recvx_en01_dg04(RECVX_ENEMY_WORK* w);
void recvx_en01_dg05(RECVX_ENEMY_WORK* w);
void recvx_en01_dg06(RECVX_ENEMY_WORK* w);
void recvx_en01_dg07(RECVX_ENEMY_WORK* w);
void recvx_en01_dg08(RECVX_ENEMY_WORK* w);
void recvx_en01_dg09(RECVX_ENEMY_WORK* w);
void recvx_en01_dg10(RECVX_ENEMY_WORK* w);
void recvx_en01_dg11(RECVX_ENEMY_WORK* w);
void recvx_en01_dg14(RECVX_ENEMY_WORK* w);
void recvx_en01_dg15(RECVX_ENEMY_WORK* w);
void recvx_en01_dg16(RECVX_ENEMY_WORK* w);

/* --- Die-Familie (en01.c:2346 DieTypeW, :2366 DieMode2W) ----------------- */
void recvx_en01_dd_type00(RECVX_ENEMY_WORK* w);       /* :9922 */
void recvx_en01_dd_type02(RECVX_ENEMY_WORK* w);       /* :9928 */
void recvx_en01_dd00(RECVX_ENEMY_WORK* w);            /* :9934 */
void recvx_en01_dd01(RECVX_ENEMY_WORK* w);
void recvx_en01_dd02(RECVX_ENEMY_WORK* w);
void recvx_en01_dd03(RECVX_ENEMY_WORK* w);

/* --- Effekt-Helfer -------------------------------------------------------- */
void recvx_en01_kami_effect(RECVX_ENEMY_WORK* w, int frm);       /* :10738 */
void recvx_en01_water_effect(RECVX_ENEMY_WORK* w, int frm);      /* :10781 */
void recvx_en01_neck_blood_effect(RECVX_ENEMY_WORK* w, int type);  /* :10852 */
void recvx_en01_neck_blood_effect2(RECVX_ENEMY_WORK* w, int type); /* :10881 */
void recvx_en01_exp_head_effect(RECVX_ENEMY_WORK* w, int type);  /* :10941 */
void recvx_en01_exp_head_effect2(RECVX_ENEMY_WORK* w, int type); /* :10967 */
void recvx_en01_nikuhen_effect(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* ps, int n); /* :10994 */
void recvx_en01_exp_leg_effect(RECVX_ENEMY_WORK* w, int lnk_obj);  /* :11012 */
void recvx_en01_exp_worm_effect(RECVX_ENEMY_WORK* w);              /* :11083 */
void recvx_en01_poison_effect(RECVX_ENEMY_WORK* w, int num);       /* :11190 */
void recvx_en01_link_fire_effect(RECVX_ENEMY_WORK* w, int type);   /* :11225 */
int  recvx_en01_chg_text_id(RECVX_ENEMY_WORK* w, int tex_id);      /* :11317 */

/* --- Abfragen (en01.c:11446..) ------------------------------------------- */
int recvx_en01_zulzul_check(RECVX_ENEMY_WORK* w, int ang, float len,
                            RECVX_ENEMY_VEC* get_pos, int* wang);   /* :11501 */
int recvx_en01_gakeoti_check(RECVX_ENEMY_WORK* w);                 /* :11534 */
int recvx_en01_dansa_check(RECVX_ENEMY_WORK* w, int ang, float ofs_z); /* :11559 */
int recvx_en01_poison_check(RECVX_ENEMY_WORK* w);                  /* :11583 */
int recvx_en01_eat_check(RECVX_ENEMY_WORK* w, int rng, float dist, int mode); /* :11658 */
int recvx_en01_worm_check(RECVX_ENEMY_WORK* w);                    /* :11726 */
int recvx_en01_kaidan_check(RECVX_ENEMY_WORK* w);                  /* :11799 */
int recvx_en01_cut_leg(RECVX_ENEMY_WORK* w);                       /* :11856 */
void recvx_en01_cut_head(RECVX_ENEMY_WORK* w, int prm1);           /* :11883 */
void recvx_en01_flying_cap(RECVX_ENEMY_WORK* w, int tbl_no);       /* :11889 */

/* --- B-Familie (en01b.c) — verknuepfter Oberkoerper, exp0-Bit 0x80000000 -- */
void recvx_en01_dummy(RECVX_ENEMY_WORK* w);           /* en01b.c:144 */
void recvx_en01_brain02_b(RECVX_ENEMY_WORK* w);       /* :150 */
void recvx_en01_mv_type00_b(RECVX_ENEMY_WORK* w);     /* :173 */
void recvx_en01_mv_type02_b(RECVX_ENEMY_WORK* w);     /* :179 */
void recvx_en01_mv13_b(RECVX_ENEMY_WORK* w);          /* :211 */
void recvx_en01_ng_type00_b(RECVX_ENEMY_WORK* w);     /* :229 */
void recvx_en01_ng03_b(RECVX_ENEMY_WORK* w);          /* :247 */
void recvx_en01_dg_type00_b(RECVX_ENEMY_WORK* w);     /* :386 */
void recvx_en01_dg_type02_b(RECVX_ENEMY_WORK* w);     /* :392 */
void recvx_en01_dg05_b(RECVX_ENEMY_WORK* w);          /* :402 */
void recvx_en01_dg09_b(RECVX_ENEMY_WORK* w);          /* :510 */
void recvx_en01_dg12_b(RECVX_ENEMY_WORK* w);          /* :656 */
void recvx_en01_dg13_b(RECVX_ENEMY_WORK* w);          /* :707 */
void recvx_en01_dd_type00_b(RECVX_ENEMY_WORK* w);     /* :727 */
void recvx_en01_dd_type02_b(RECVX_ENEMY_WORK* w);     /* :733 */
void recvx_en01_dd02_b(RECVX_ENEMY_WORK* w);          /* :739 */
void recvx_en01_rot_neck(RECVX_ENEMY_WORK* w, int neck_no,
                         int ply_neck_no);                        /* :849 */
int  recvx_en01_side_rot_neck(RECVX_ENEMY_WORK* w, int neck_no,
                              RECVX_ENEMY_VEC* trg, int neck_ry,
                              int neck_ry_max, int ang);           /* :874 */
int  recvx_en01_search_player2(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* pp,
                               int pp_r, int r);                   /* :922 */

/* --- Gemeinsame Helfer fuer native/enemies/en01_sub.c ----------------------
   Nur Weitergaben der in en01_core.c liegenden Originalhelfer bzw. des dort
   gebundenen Kontexts; kein zweiter Zustand. */
const RECVX_EN01_CONTEXT* recvx_en01_context(void);  /* NULL = nicht gesetzt */
int32_t recvx_en01_niten_dir(float hontai_x, float hontai_z,
                             float target_x, float target_z); /* zonzon.c:70 */
void recvx_en01_ikou(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                     int32_t add_dir);                         /* zonzon.c:10 */

/* --- Kindfamilie (en01sub.c) — ids 1/0x1F..0x26 ---------------------------
   Jede Familie hat ihren eigenen mode0-Eintritt; die Unterfunktionen bleiben
   sichtbar, damit der Dispatchbesitzer und die Tests sie einzeln pruefen
   koennen. */
void recvx_en01_parent(RECVX_ENEMY_WORK* w);           /* en01sub.c:92  */
void recvx_en01_parent_init(RECVX_ENEMY_WORK* w);     /* :96           */
void recvx_en01_parent_move(RECVX_ENEMY_WORK* w);     /* :143          */
void recvx_en01_parent_nearest_check(RECVX_ENEMY_WORK* w);  /* :118 */
void recvx_en01_parent_nokaidan_check(RECVX_ENEMY_WORK* w);  /* :153 */
void recvx_en01_arm(RECVX_ENEMY_WORK* w);              /* :213          */
void recvx_en01_arm_init(RECVX_ENEMY_WORK* w);        /* :217          */
void recvx_en01_arm_move(RECVX_ENEMY_WORK* w);        /* :238          */
void recvx_en01_arm_mv00(RECVX_ENEMY_WORK* w);        /* :253          */
void recvx_en01_arm_damage(RECVX_ENEMY_WORK* w);      /* :257          */
void recvx_en01_arm_dg00(RECVX_ENEMY_WORK* w);        /* :261          */
void recvx_en01_arm_dg01(RECVX_ENEMY_WORK* w);        /* :412          */
void recvx_en01_leg(RECVX_ENEMY_WORK* w);             /* :656          */
void recvx_en01_leg_init(RECVX_ENEMY_WORK* w);        /* :660          */
void recvx_en01_leg_move(RECVX_ENEMY_WORK* w);        /* :681          */
void recvx_en01_leg_mv00(RECVX_ENEMY_WORK* w);        /* :696          */
void recvx_en01_leg_damage(RECVX_ENEMY_WORK* w);      /* :700          */
void recvx_en01_leg_dg00(RECVX_ENEMY_WORK* w);        /* :704          */
void recvx_en01_head(RECVX_ENEMY_WORK* w);            /* :794          */
void recvx_en01_head_init(RECVX_ENEMY_WORK* w);       /* :798          */
void recvx_en01_head_move(RECVX_ENEMY_WORK* w);       /* :819          */
void recvx_en01_head_mv00(RECVX_ENEMY_WORK* w);       /* :834          */
void recvx_en01_head_damage(RECVX_ENEMY_WORK* w);     /* :838          */
void recvx_en01_head_dg00(RECVX_ENEMY_WORK* w);       /* :842          */
void recvx_en01_cap(RECVX_ENEMY_WORK* w);             /* :983          */
void recvx_en01_cap_init(RECVX_ENEMY_WORK* w);        /* :987          */
void recvx_en01_cap_move(RECVX_ENEMY_WORK* w);        /* :1008         */
void recvx_en01_cap_mv00(RECVX_ENEMY_WORK* w);        /* :1023         */
void recvx_en01_cap_damage(RECVX_ENEMY_WORK* w);      /* :1027         */
void recvx_en01_cap_dg00(RECVX_ENEMY_WORK* w);        /* :1031         */
void recvx_en01_cap_dg01(RECVX_ENEMY_WORK* w);        /* :1173         */
void recvx_en01_worm(RECVX_ENEMY_WORK* w);            /* :1259         */
void recvx_en01_worm_init(RECVX_ENEMY_WORK* w);       /* :1263         */
void recvx_en01_worm_move(RECVX_ENEMY_WORK* w);       /* :1321         */
void recvx_en01_worm_mv00(RECVX_ENEMY_WORK* w);       /* :1336         */
void recvx_en01_worm_mv01(RECVX_ENEMY_WORK* w);       /* :1352         */
void recvx_en01_worm_mv02(RECVX_ENEMY_WORK* w);       /* :1568         */
void recvx_en01_bom(RECVX_ENEMY_WORK* w);             /* :1766         */
void recvx_en01_bom_init(RECVX_ENEMY_WORK* w);        /* :1770         */
void recvx_en01_bom_move(RECVX_ENEMY_WORK* w);        /* :1858         */
void recvx_en01_scope(RECVX_ENEMY_WORK* w);           /* :1900         */
void recvx_en01_scope_init(RECVX_ENEMY_WORK* w);      /* :1904         */
void recvx_en01_scope_move(RECVX_ENEMY_WORK* w);      /* :1919         */
void recvx_en01_scope_effect(RECVX_ENEMY_WORK* w);    /* :1960         */

/* --- Geteilte Originalhelfer fuer native/enemies/en26.c -------------------
   EN26 (en26.c) ruft dieselben zonzon.c/pwksub.c/ps2_NaColi.c-Routinen wie
   EN01. Reine Weitergaben der hier liegenden Uebersetzungen, kein zweiter
   Zustand und keine zweite Fassung derselben Originalroutine. */
int32_t recvx_en01_ikou3(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                         int32_t add_dir);                     /* zonzon.c:45   */
int recvx_en01_cdir_check(int32_t my_ang, int32_t trg_ang);    /* zonzon.c:76   */
float recvx_en01_distance_p2p(const RECVX_ENEMY_VEC* a, float bx, float by,
                              float bz);                       /* ps2_NaColi.c:65 */
int recvx_en01_collision_wall2(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps,
                               const RECVX_ENEMY_VEC* ops, RECVX_ENEMY_VEC* pd,
                               float ar, float ah);            /* zonzon.c:1222 */
int recvx_en01_check_dir_wall(RECVX_ENEMY_WORK* w, int32_t ang, float step); /* zonzon.c:1255 */
int recvx_en01_check_side_wall(RECVX_ENEMY_WORK* w, float step, int both);   /* zonzon.c:1302 */
/* bhEne_CalcPartsPos (zonzon.c:286): `mtx16` ist die Streumatrix lcmat des
   Aufrufers (16 Floats); sie traegt danach die Kettenmatrix. */
int recvx_en01_calc_parts_pos(RECVX_ENEMY_WORK* w, float* mtx16,
                              RECVX_ENEMY_VEC* pos, const int8_t* tree,
                              int parts_num, int clr_flg);
void recvx_en01_set_vibration(RECVX_ENEMY_WORK* w, int32_t no); /* zonzon.c:1350 */

#ifdef __cplusplus
}
#endif
#endif
