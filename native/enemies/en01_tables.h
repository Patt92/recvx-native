#ifndef RECVX_NATIVE_EN01_TABLES_H
#define RECVX_NATIVE_EN01_TABLES_H
#include <stdint.h>
#include "enemy_work.h"

/* ===========================================================================
   T2 — EN01-Datentabellen
   ---------------------------------------------------------------------------
   Reine Daten-/Layouttabellen, keine Funktionskoerper (AGENTS.md 2026-10-09:
   "Reine Daten-/Layouttabellen sind keine generierten Funktionskoerper").
   Alle Fundstellen sind Semantikreferenz.

   In dieser Datei sind ALLE EN01-Datenfelder definiert, die der portierte
   Ablauf auswertet: die strukturellen Tabellen (unten) und die grossen
   reinen Datenfelder (Abschnitt "Grosse Datenfelder"). Es gibt damit keinen
   offenen Beschaffungsauftrag mehr; `recvx_en01_default_data()` liefert den
   fertig gefuellten RECVX_EN01_DATA-Satz.

   KORREKTUR gegenueber dem vorherigen Stand dieser Datei (T2, 2026-10-10):
   die Felder waren als Beschaffungsauftrag an tools/generate_combat_tables.py
   -> native/combat_tables_data.inc verschoben. Das hat alle datengetriebenen
   Mechanismen (Fusskorrektur, SE, Motionwechsel, Blut, Brandeffekt,
   Griffversatz) dauerhaft auf Lueckenmeldungen stehen lassen, obwohl reine
   Datentabellen hier zulaessig sind (AGENTS.md 2026-10-09). en01_hp_tbl
   steht zusaetzlich in combat_tables_data.inc:615; die Werte dort und hier
   sind identisch, der .inc-Pfad ist aber eine C++-Uebersetzungseinheit mit
   interner Bindung und von C aus nicht erreichbar.
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* --- ene01_typ_tbl[31][2] (en01.c:2541) ----------------------------------
   Spalte 0 = Raum-mdlver, Spalte 1 = interner en01-Subtyp (EXP0 0x4C).
   bhEne01_Init durchsucht NUR i < 30 (en01.c:2696), die letzte Zeile
   (mdlver 43) ist dadurch unerreichbar. Original so belassen. */
#define RECVX_EN01_TYP_ROWS      31u
#define RECVX_EN01_TYP_SEARCHED  30u
extern const int32_t recvx_en01_typ_tbl[RECVX_EN01_TYP_ROWS][2];

/* --- en01_PersonalType[27] (en01.c:1062) ---------------------------------
   adist/ndist = Aktions- und Naehedistanz, ang = Drehrate,
   add_hp = HP-Zuschlag in Prozent (en01.c:2709), add_atk = Angriffszuschlag. */
typedef struct {
    float   adist;
    float   ndist;
    int32_t ang;
    int32_t add_hp;
    int32_t add_atk;
} RECVX_EN01_PERSONAL;
#define RECVX_EN01_PERSONAL_COUNT 27u
extern const RECVX_EN01_PERSONAL recvx_en01_personal[RECVX_EN01_PERSONAL_COUNT];

/* --- Knotenbaeume ---------------------------------------------------------
   en01_flipTree[19] (en01.c:2400) ist die Spiegeltabelle, die bhSetMotion
   ueber epw->mtn_tp erhaelt (en01.c:2596); en01_flipTree2[11]
   (en01.c:2407) gilt fuer den verknuepften Oberkoerper (en01.c:2724).
   en01_tree[8][8] (en01.c:2413) sind die Fussketten, die
   bhEne01_CheckMtnTbl an bhCalcFixOffset gibt; 255 terminiert. */
extern const int8_t recvx_en01_flip_tree[19];
extern const int8_t recvx_en01_flip_tree2[11];
extern const int8_t recvx_en01_tree[8][8];

/* --- en01_walk_mtn[6] (en01.c:1118) -------------------------------------
   Paare (Unterkoerper, Oberkoerper) der drei Laufvarianten; der Index steht
   in EXP0 0xA0 und wird von bhEne01_GetWalkMotion gesetzt. */
extern const int32_t recvx_en01_walk_mtn[6];

/* --- Schattenvorlagen ---------------------------------------------------
   En01SdwTabW[5] (en01.c:1108) fuer den normalen Zombie (bhSetShadow mit
   Modell 1), En01SdwTabB[3] (en01.c:1113) fuer den Oberkoerper (Modell 10).
   -1 terminiert; beide Aufrufe verwenden 6.0f/5.0f/5.0f
   (en01.c:2740 bzw. :2759). */
extern const int8_t recvx_en01_sdw_tab_w[5];
extern const int8_t recvx_en01_sdw_tab_b[3];

/* --- Kapseltabellen -----------------------------------------------------
   Ene01CapColTabA[17] (en01.c:1025) fuer den normalen Zombie,
   Ene01CapColTabB[12] (en01.c:1046) fuer den Oberkoerper; Init waehlt sie
   (en01.c:2736 bzw. :2745). Die Eintraege sind (Knoten1, Knoten2, Radius)
   als 16-Bit-Werte; die Laenge steht beim Verbraucher, nicht als
   Terminator (weapon.c bhCheckCapCol2Capsule). */
typedef RECVX_ENEMY_CPCL RECVX_EN01_CPCL;
extern const RECVX_EN01_CPCL recvx_en01_cap_col_a[17];
extern const RECVX_EN01_CPCL recvx_en01_cap_col_b[12];

/* --- Clip-Gelenke (en01.c:2784-2791) ------------------------------------
   bhEne01_Init setzt clp_jno[0..7] auf diese acht Werte und lok_jno auf 10
   (en01.c:2793). Sie ueberschreiben die Werte, die bhSetEnemy vorher
   gesetzt hat (eneset.c:302). */
extern const int8_t recvx_en01_clp_jno[8];

/* --- En01_PlyMtn_OffsetTbl[4] (en01.c:25) -------------------------------
   Basis-MTN je Spielerfigur; bhEne01_PlayerLink leitet daraus den Index in
   die XZ-Tabellen ab (en01.c:3055-3070). */
extern const int32_t recvx_en01_ply_mtn_offset[4];

/* --- kaidan_ang[4] (en01.c:1345) ---------------------------------------- */
extern const int32_t recvx_en01_kaidan_ang[4];

/* --- add_seno[27] (en01.c:11370) ---------------------------------------
   bhEne01_SePlay addiert fuer SE-Nummern 20..63 den Wert dieser Zeile am
   Index EXP0 0x4C. */
extern const int32_t recvx_en01_add_seno[RECVX_EN01_PERSONAL_COUNT];

/* --- rot_tbl[4][8] (en01.c:10324) --------------------------------------
   Zuckwinkel des Oberkoerpers in bhEne01_SetMtn; Zeile 0/2 positiv,
   1/3 negativ, Spalte ist EXP0 0x2C (0..7). Zeile 0 und 2 sowie 1 und 3
   sind im Original identisch; nicht zusammenfassen. */
extern const int32_t recvx_en01_rot_tbl[4][8];

/* --- Dispatchbreiten (en01.c:2207..2398) --------------------------------
   Nur die Laengen und die Indexquelle, nicht die Funktionszeiger: der
   Port bestimmt die Zustaende selbst.
     bhEne01_Mode0[6]        Index epw->mode0   en01.c:2207
     bhEne01_InitType[16]    Index epw->type    en01.c:2217
     bhEne01_MoveTypeW[16]   Index epw->type    en01.c:2237
     bhEne01_MoveMode2W[17]  Index epw->mode2   en01.c:2257 (13 = NULL)
     bhEne01_NageTypeW[16]   Index epw->type    en01.c:2278 (alle NGType00)
     bhEne01_NageMode2W[4]   Index epw->mode2   en01.c:2298 (3 = NULL)
     bhEne01_DamageTypeW[16] Index epw->type    en01.c:2306
     bhEne01_DamageMode2W[17]Index epw->mode2   en01.c:2326 (12/13 = NULL)
     bhEne01_DieTypeW[16]    Index epw->type    en01.c:2346
     bhEne01_DieMode2W[4]    Index epw->mode2   en01.c:2366
     bhEne01_PlyDmgMode[2]   Index plp->mode2   en01.c:2374
     bhEne01_DmgCheckType[16]Index epw->type    en01.c:2380 (8/9/10 = Dmmy) */
#define RECVX_EN01_MODE0_COUNT      6u
#define RECVX_EN01_TYPE_COUNT       16u
#define RECVX_EN01_MOVE_MODE2_COUNT 17u
#define RECVX_EN01_NAGE_MODE2_COUNT 4u
#define RECVX_EN01_DMG_MODE2_COUNT  17u
#define RECVX_EN01_DIE_MODE2_COUNT  4u

/* Welche der beiden Typfamilien ein epw->type benutzt. Das Original hat je
   Zustandsgruppe eine eigene 16er-Tabelle; MoveTypeW, DamageTypeW, DieTypeW
   und DmgCheckType teilen dasselbe Muster (Typ 2/3/6/7/9 -> Variante 02),
   InitType weicht ab (eigene Eintraege fuer 10/11/15) und NageTypeW ist
   fuer alle Typen gleich. Deshalb bleiben die Muster getrennt. */
extern const uint8_t recvx_en01_type_family[RECVX_EN01_TYPE_COUNT];
extern const uint8_t recvx_en01_init_type_family[RECVX_EN01_TYPE_COUNT];

/* bhEne01_DmgCheckType[8/9/10] ist bhEne01_DmgCheckTypeDmmy, eine leere
   Originalfunktion (en01.c:2461). GEMESSEN kommt Typ 10 in den Raumdaten
   26x und Typ 9 1x vor, dieser Zweig ist also erreichbar und tut im
   Original nichts. */
extern const uint8_t recvx_en01_dmg_check_dummy[RECVX_EN01_TYPE_COUNT];

/* ---------------------------------------------------------------------------
   Zeilentypen der grossen Datentabellen
   ---------------------------------------------------------------------------
   Feldbilder woertlich aus include/ps2/veronica/prog/en01.h (EN01_FMTN_WORK,
   EN01_MTN_WORK, EN01_ATB_WORK, EN01_MTN2_WORK, EN01_KAMI_WORK, EN01_FIRE_WORK,
   EN01_CHG_WORK, EN01_CHK_WORK, EN01_EFF_WORK, EN01_WATER_WORK,
   EN01_FLYINGCAP_WORK), WPNDAMAGE_WORK/COMBWEP_WORK/COMBJOINT_WORK aus
   types.h:1931 / zonzon1.h:28 / zonzon1.h:21, BT_WORK aus zonzon.h:14,
   BP_WORK aus types.h:2210 (= RECVX_ENEMY_BP_WORK), NJS_POINT2_XZ aus
   types.h:35. Nur Feldbild, keine Daten aus diesen Kopfzeilen. */
typedef struct { int32_t type, s_frm, e_frm; } RECVX_EN01_FMTN;
typedef struct { int32_t no; RECVX_EN01_FMTN fmtn[3]; } RECVX_EN01_MTN;
typedef struct { int32_t frm; uint32_t act; } RECVX_EN01_ATB;
typedef struct { int32_t no; RECVX_EN01_ATB atb[4]; } RECVX_EN01_MTN2;
typedef struct { int32_t mtn_no; int16_t frm[4]; } RECVX_EN01_KAMI;
typedef struct { int32_t flg, nm_act, nm_blood, cb_act, cb_blood; } RECVX_EN01_WPNDAMAGE;
typedef struct { int32_t crit; int32_t pt[3]; int32_t timeout; int32_t bonus; } RECVX_EN01_COMBWEP;
typedef struct { int32_t flg; int32_t correct; } RECVX_EN01_COMBJOINT;
typedef struct { int32_t obj; float size, len; RECVX_ENEMY_VEC ofs; } RECVX_EN01_FIRE;
typedef struct { int32_t lnk_obj; float x, y, z, xlen, ylen, size, len; } RECVX_EN01_BT;
typedef struct {
    uint32_t mode, clr_flg, set_flg;
    int32_t  mtn_no1, mtn_no2, type, frm_no;
    uint32_t hokan_count;
    int32_t  mtn_add;
} RECVX_EN01_CHG;
typedef struct { int32_t mtn_no, sfrm, efrm; RECVX_EN01_CHG chg_tbl[2]; } RECVX_EN01_CHK;
typedef struct { int16_t frm; int8_t size, obj; float px, pz; } RECVX_EN01_EFF;
typedef struct { int32_t mtn_no; RECVX_EN01_EFF eff[3]; } RECVX_EN01_WATER;
typedef struct { int32_t angY, angZ, rspd; float v, point; } RECVX_EN01_FLYINGCAP;
typedef struct { float x, z; } RECVX_EN01_XZ;

/* --- Effektvorlage und Effektslot (en01_work.h, set_effect_tb) ----------
   sys->ef ist im Original eine EF_WORK-Vorlage (types.h), von der EN01 nur
   diese Felder beschreibt (gemessen ueber alle sys->ef-Zugriffe in en01.c:
   flg, id, type, px/py/pz, sx/sy/sz, ay, mdlver). Kein vollstaendiges
   EF_WORK: nur die Felder, die dieser Mechanismus setzt. */
typedef struct {
    uint32_t flg;
    int32_t  id;
    int32_t  type;
    float    px, py, pz;
    float    sx, sy, sz;
    int32_t  ay;
    uint8_t  mdlver;
} RECVX_EN01_EF;

/* Die Felder eines eff[]-Slots, die EN01 NACH bhSetEffectTb setzt (gemessen
   ueber alle eff[eno]-Zugriffe in en01.c). `mask` waehlt, welche gelten. */
#define RECVX_EN01_EFS_LINK    0x0001u  /* lkwkp                            */
#define RECVX_EN01_EFS_VEC     0x0002u  /* xn/yn/zn                         */
#define RECVX_EN01_EFS_STFLG   0x0004u  /* stflg |= bits                    */
#define RECVX_EN01_EFS_TEX     0x0008u  /* txp[0] = Modelltexturliste, tex_id */
#define RECVX_EN01_EFS_SPD     0x0010u  /* spd                              */
#define RECVX_EN01_EFS_ANG     0x0020u  /* ax/ay                            */
#define RECVX_EN01_EFS_CT      0x0040u  /* ct0/ct1/ct3                      */
#define RECVX_EN01_EFS_SHP     0x0080u  /* shp_ct                           */
typedef struct {
    struct RECVX_ENEMY_WORK* lkwkp;
    float    xn, yn, zn;
    uint32_t stflg_or;
    /* txp[0] = epw->mlwP->texP: die Texturliste des MODELLS dieses Works.
       Der Anbieter kennt sie; EN01 gibt nur den Work mit. */
    struct RECVX_ENEMY_WORK* tex_from;
    int32_t  tex_id;
    float    spd;
    int32_t  ax, ay;
    int32_t  ct0, ct1, ct3;
    float    shp_ct;
} RECVX_EN01_EF_SLOT;

/* ---------------------------------------------------------------------------
   Grosse Datenfelder — hier definiert (native/en01_tables.c)
   ---------------------------------------------------------------------------
   Werte woertlich aus den Originaldatenfeldern, ohne Umrechnung oder
   Auswahl; Struktur- und Feldreihenfolge wie in include/ps2/veronica/prog.
   Jede Tabelle traegt ihre Fundstelle an der Definition.
   --------------------------------------------------------------------------- */
extern const RECVX_EN01_MTN       recvx_en01_mtn_tbl[26];        /* en01.c:30   */
extern const RECVX_EN01_MTN2      recvx_en01_mtn_tbl2[60];       /* :242        */
extern const RECVX_EN01_MTN2      recvx_en01_mtn_tbl3[16];       /* :786        */
extern const RECVX_EN01_WPNDAMAGE recvx_en01_wpn_damage[22];     /* :934        */
extern const RECVX_EN01_COMBWEP   recvx_en01_comb_wep[21];       /* :960        */
extern const RECVX_EN01_COMBJOINT recvx_en01_comb_joint[18];     /* :985        */
extern const RECVX_EN01_FIRE      recvx_en01_fire_tbl[14];       /* :1007       */
extern const RECVX_ENEMY_BP_WORK  recvx_en01_bld_tbl;            /* :1098       */
extern const RECVX_ENEMY_BP_WORK  recvx_en01_bld_tbl2;           /* :1103       */
extern const RECVX_EN01_BT        recvx_en01_prt_blood_tbl[18];  /* :1123       */
extern const RECVX_EN01_CHK       recvx_en01_chg_mtn_tbl[28];    /* :1145       */
extern const RECVX_EN01_KAMI      recvx_en01_keff_tbl[11];       /* :2044       */
extern const RECVX_EN01_WATER     recvx_en01_weff_tbl[18];       /* :2059       */
extern const int                  recvx_en01_hp_tbl[2][16];      /* :2425       */
extern const float                recvx_en01_kamikami[71];       /* :2431       */
extern const float                recvx_en01_kamikami2[70];      /* :2442       */
extern const float                recvx_en01_mogmog[40];         /* :2452       */
extern const RECVX_EN01_XZ        recvx_en01_cl_mtn01_xz[60];    /* :1347       */
extern const RECVX_EN01_XZ        recvx_en01_cl_mtn02_xz[60];    /* :1410       */
extern const RECVX_EN01_XZ        recvx_en01_cl_mtn04_xz[50];    /* :1473       */
extern const RECVX_EN01_XZ        recvx_en01_cl_mtn05_xz[50];    /* :1526       */
extern const RECVX_EN01_XZ        recvx_en01_ch_mtn01_xz[60];    /* :1579       */
extern const RECVX_EN01_XZ        recvx_en01_ch_mtn02_xz[60];    /* :1642       */
extern const RECVX_EN01_XZ        recvx_en01_ch_mtn04_xz[50];    /* :1705       */
extern const RECVX_EN01_XZ        recvx_en01_ch_mtn05_xz[50];    /* :1758       */
extern const RECVX_EN01_XZ        recvx_en01_st_mtn01_xz[60];    /* :1811       */
extern const RECVX_EN01_XZ        recvx_en01_st_mtn02_xz[60];    /* :1874       */
extern const RECVX_EN01_XZ        recvx_en01_st_mtn04_xz[50];    /* :1937       */
extern const RECVX_EN01_XZ        recvx_en01_st_mtn05_xz[50];    /* :1990       */

/* ---------------------------------------------------------------------------
   Datensatz, den der Mechanismus liest
   ---------------------------------------------------------------------------
   recvx_en01_set_data() (en01_core.h) bindet ihn. `recvx_en01_default_data()`
   liefert ihn vollstaendig aus den Feldern dieser Datei; ein selbst
   zusammengestellter Satz mit NULL-Zeigern bleibt moeglich (Tests), dann
   melden die betroffenen Mechanismen ihre Luecke und setzen KEINEN
   Ersatzwert ein. */
typedef struct RECVX_EN01_DATA {
    /* en01_hp_tbl[2][16] (en01.c:2425; gelesen :2468/:2472, Zeile 0 =
       gm_mode != 2, Zeile 1 = gm_mode == 2). */
    const int (*hp_tbl)[16];
    const RECVX_EN01_MTN*       mtn_tbl;       /*  26  en01.c:30   */
    const RECVX_EN01_MTN2*      mtn_tbl2;      /*  60  en01.c:242  */
    const RECVX_EN01_MTN2*      mtn_tbl3;      /*  16  en01.c:786  */
    const RECVX_EN01_WPNDAMAGE* wpn_damage;    /*  22  en01.c:934  */
    const RECVX_EN01_COMBWEP*   comb_wep;      /*  21  en01.c:960  */
    const RECVX_EN01_COMBJOINT* comb_joint;    /*  18  en01.c:985  */
    const RECVX_EN01_FIRE*      fire_tbl;      /*  14  en01.c:1007 */
    const RECVX_EN01_BT*        blood_tbl;     /*  18  en01.c:1123 */
    const RECVX_EN01_CHK*       chg_mtn_tbl;   /*  28  en01.c:1145 */
    const RECVX_EN01_KAMI*      keff_tbl;      /*  11  en01.c:2044 */
    const RECVX_EN01_WATER*     weff_tbl;      /*  18  en01.c:2059 */
    const float*                kamikami;      /*  71  en01.c:2431 */
    const float*                kamikami2;     /*  70  en01.c:2442 */
    const float*                mogmog;        /*  40  en01.c:2452 */
    const RECVX_ENEMY_BP_WORK*  bld_tbl;       /*      en01.c:1098 */
    const RECVX_ENEMY_BP_WORK*  bld_tbl2;      /*      en01.c:1103 */
    /* ply_pos[16] aus bhEne01_PlayerLink (en01.c:3014-3031): der Index ist
       sys->ply_id * 4 + mtn_no. Die vier Eintraege fuer ply_id 3 sind im
       Original DIESELBEN ch_*-Felder wie fuer ply_id 1; das bleibt so. */
    const RECVX_EN01_XZ*        ply_pos[16];
} RECVX_EN01_DATA;

/* Vollstaendiger Satz aus den Feldern dieser Datei. */
RECVX_EN01_DATA recvx_en01_default_data(void);

#ifdef __cplusplus
}
#endif
#endif
