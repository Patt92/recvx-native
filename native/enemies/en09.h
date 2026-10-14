#ifndef RECVX_NATIVE_EN09_H
#define RECVX_NATIVE_EN09_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
   Identitaet 9 — Bandersnatch. Nativ eigenstaendig bestimmt; kein Original-
   koerper, kein Generator, keine zweite Workwelt.

   Semantikreferenz (nur gelesen, jede Stelle mit Datei:Zeile belegt):
     en09.c:19      "// ENEMY: Bandersnatch"
     en09.c:32      En09_PlyMtn_OffsetTbl[4]
     en09.c:40      en09_mtn_tbl[28]        MTN_TBL_FMTN_WORK (Fussfixierung)
     en09.c:72      en09_mtn_tbl2[24]       MTN_TBL_ATB_WORK  (SE je Frame)
     en09.c:315     en09_mtn_tbl3[5]        MTN_TBL_ATB_WORK  (Vibration)
     en09.c:324     mtn_chg_tbl[17]         CHK_TBL_WORK      (Motionwechsel)
     en09.c:464     En09_WpnDamageTbl[22]
     en09.c:490     CombWepTbl[21]
     en09.c:515     CombJointTbl[24]        (im Original komplett 0)
     en09.c:517     Ene09CapColTab[19]
     en09.c:550     En09SdwTab[6]
     en09.c:555     en09prt_blood_tbl[24]
     en09.c:583     en09_mtn23[35]
     en09.c:622     arm4_pos[92]            Armlaenge je Frame, Motion ?
     en09.c:718     arm4_pos2[67]
     en09.c:789     arm4_pos3[143]          mtn_no 2
     en09.c:936     arm4_pos4[128]          mtn_no 3
     en09.c:1068    arm4_pos5[654]          mtn_no 4
     en09.c:1726    arm4_pos6[185]          mtn_no 5
     en09.c:1915    bhEne09_Mode0[6]        Init/Move/Nage/Damage/Die/Event
     en09.c:1925    bhEne09_MoveType[11]    type 0..8 -> MVType00, 9, 10
     en09.c:1940    bhEne09_DamageType[1], :1945 NageType[1], :1950 DieType[1]
     en09.c:1955    bhEne09_BrainMode2[12]  (0/1 Brain00, 2..11 leer)
     en09.c:1971    bhEne09_MoveMode2[15]   MV00..MV14
     en09.c:1990    bhEne09_NageMode2[2], :1996 DamageMode2[11], :2011 DieMode2[1]
     en09.c:2016    en09_tree[4][16]        Knotenketten fuer bhCalcFixOffset
     en09.c:2024    en09_hp_tbl[2][16]      HP nach sys->gm_mode
     en09.c:2037    bhEne09              Rahmen eines Passes
     en09.c:2099    bhEne09_MainLoop
     en09.c:2112    bhEne09_PlayerControl
     en09.c:2128    bhEne09_CalcEnemy
     en09.c:2167    bhEne09_CollCheck, :2186 CollCheckWall, :2326 CollCheckArm (leer)
     en09.c:2332    bhEne09_Init
     en09.c:7396    bhEne09_SetMtn, :7558 MtnTblPlay, :7694 SePlay,
                    :7703 SetSmokeEffect, :7369 ChkDiffAngle, :7383 en09_mogmog
     en09.h:7/16/24/30/42  CHG_TBL_WORK, CHK_TBL_WORK, ARM_POS,
                    MTN_TBL_FMTN_WORK, MTN_TBL_ATB_WORK (Originaltypen)
     eneset.c:62    bhJumpEnemy[9] = bhEne09
     eneset.c:906   bhEne_CallocWork

   Die gemeinsamen Dienste der grossen Gegner liegen in
   native/enemies/en03_lib.h (`recvx_t3_*`, RECVX_ENEMY_EXT); der
   Grundvertrag in native/enemies/enemy_work.h. EN09 bringt keinen eigenen
   Engine-Dienst mit: fehlt einer, wird er namentlich als Luecke gemeldet.
   =========================================================================== */

/* ---------------------------------------------------------------------------
   Originaltypen der Tabellen (include/ps2/veronica/prog/en09.h:7-51).
   Gleiche Felder, gleiche Reihenfolge, gleiche Groessen.
   --------------------------------------------------------------------------- */

/* en09.h:7 CHG_TBL_WORK (0x14 Byte). */
typedef struct {
    int32_t  mtn_no;
    int32_t  type;
    int32_t  frm_no;
    uint32_t h_cnt;
    uint32_t mode;
} RECVX_EN09_CHG_TBL;

/* en09.h:16 CHK_TBL_WORK (0x5C Byte). */
typedef struct {
    int32_t             mtn_no;
    int32_t             s_frm;
    int32_t             e_frm;
    RECVX_EN09_CHG_TBL  chg_tbl[4];
} RECVX_EN09_CHK_TBL;

/* en09.h:24 ARM_POS (0x8 Byte): Armlaenge auf Frame `frm`. */
typedef struct {
    int32_t frm;
    float   len;
} RECVX_EN09_ARM_POS;

/* en09.h:30 MTN_TBL_FMTN_WORK (0x28 Byte): bis zu drei Fussfixierungs-
   abschnitte je Motion; `type` -1 beendet die Liste eines Eintrags, `no` -1
   die Tabelle. */
typedef struct {
    int32_t no;
    struct { int32_t type, s_frm, e_frm; } fmtn[3];
} RECVX_EN09_MTN_FMTN;

/* en09.h:42 MTN_TBL_ATB_WORK (0x2C Byte): bis zu fuenf Frame/Aktion-Paare je
   Motion. In en09_mtn_tbl2 ist `act` die SE-Nummer fuer bhEne09_SePlay, in
   en09_mtn_tbl3 der Vibrationsindex fuer bhEne_SetVibration. */
typedef struct {
    int32_t no;
    struct { int32_t frm; uint32_t act; } atb[5];
} RECVX_EN09_MTN_ATB;

/* ---------------------------------------------------------------------------
   exp0 — im Original ein 0x7C Byte grosser Rohblock aus
   bhEne_CallocWork(0x7C, 8) (en09.c:2376). Nativ benannt und typisiert;
   Inhalt, Nullstart und Lebensdauer bleiben gleich.

   WICHTIG zur Lebensdauer (en09.c:2374-2417): Beim ZWEITEN Init (exp0 != NULL)
   rettet das Original die vier Armlaengen bei 0x00..0x0C, nullt den ganzen
   Block von Hand und schreibt sie zurueck. Der Port muss das genauso tun —
   der Block ist dann NICHT neu alloziert.

   Benannt sind nur Felder, deren Bedeutung an einer Fundstelle belegt ist.
   Alles Uebrige behaelt vorerst seine Originallage als `reserved_*` und wird
   mit der jeweiligen Funktion nachgezogen, statt einen Namen zu raten.
   --------------------------------------------------------------------------- */

/* Bits von EXP0_I(0x18), soweit bisher belegt. Die Maske 0xF ist KEIN Bitfeld,
   sondern eine Fallnummer (en09.c:2205/2223/2264/2297: 1, 2, 3, 6). */
#define RECVX_EN09_ST_COLL_CASE   0x0000000Fu /* en09.c:2205 ff. Wandfall     */
#define RECVX_EN09_ST_NO_PLAYER   0x00000010u /* en09.c:2171 bhCheckPlayer aus */
#define RECVX_EN09_ST_NO_ENEMIES  0x00000020u /* en09.c:2176 bhCheckEnemies aus */
#define RECVX_EN09_ST_NO_WALL     0x00000080u /* en09.c:2194 Wandpass aus      */
#define RECVX_EN09_ST_ROT_UP      0x00000200u /* en09.c:7522 rot_tbl statt _2  */
#define RECVX_EN09_ST_NO_SETMTN   0x00000800u /* en09.c:2105 SetMtn aus        */
#define RECVX_EN09_ST_SEE_PLAYER  0x00001000u /* en09.c:3031 Spieler gesichtet */
#define RECVX_EN09_ST_ARM_OUT     0x00008000u /* en09.c:2064 Armkollision an   */
#define RECVX_EN09_ST_HEAD_ROT    0x00100000u /* en09.c:7507 Kopfdrehung       */
#define RECVX_EN09_ST_GRAB        0x00400000u /* en09.c:2114 Spieler im Griff  */
#define RECVX_EN09_ST_INIT_PLY    0x01000000u /* en09.c:2388 Spielerknoten frei */
#define RECVX_EN09_ST_ROUTE_LOCK  0x02000000u /* en09.c:3160 Wegflaeche gesetzt */
#define RECVX_EN09_ST_TREE_SWAP   0x10000000u /* en09.c:7595 tree[1] statt [0]  */
#define RECVX_EN09_ST_ROOT_XZ     0x20000000u /* en09.c:7495 Wurzel x/z = 0     */
#define RECVX_EN09_ST_ROOT_MTN_XZ 0x40000000u /* en09.c:7486 + GetTranslateMtn 0 */
#define RECVX_EN09_ST_ROOT_MTN_3  0x80000000u /* en09.c:7476 + GetTranslateMtn 1 */

typedef struct {
    /* 0x00..0x0C — die vier Armsegmentlaengen. Quelle beim ersten Init:
       mlwP->objP[7..10].pos[0] (en09.c:2379-2382); beim zweiten Init gerettet
       und zurueckgeschrieben (:2395-2416). bhEne09_SetMtn schreibt sie auf
       Frame 0 in die Knoten zurueck (:7460-7466). */
    float    arm_len[4];

    /* 0x10 — Suchtakt von bhEne09_EneSearch (en09.c:3002-3027):
       Bits 0x1F sind ein Zaehler, der bei 0x1F auf 0 zurueckfaellt; 0x20
       merkt einen Treffer von bhSearchPlayer in diesem Takt, 0x40 haelt
       "Spieler sichtbar" bis zum naechsten Takt ohne Treffer. */
    uint8_t  search;

    /* 0x11 — Nachlauf der Sichtbarkeit (en09.c:3032/3037): bei Sicht auf 0xF
       gesetzt, sonst heruntergezaehlt.
       ORIGINALER DEFEKT: die Abfrage lautet `(temp & 0xFF) < 0` auf einem
       unsigned char (en09.c:3040) und ist damit nie wahr — Bit 0x1000 des
       Zustandswortes faellt ueber diesen Weg nie ab. Unveraendert portieren
       und nicht "reparieren". */
    int8_t   sight_hold;

    /* 0x12 — in den bisher gelesenen Funktionen nicht beruehrt. */
    uint8_t  reserved_12;

    /* 0x13 — Zaehler fuer die Formkurve: en09.c:7505
       shp_ct = en09_mogmog[(++EXP0_UC(0x13) & 0xFF) % 40]. */
    uint8_t  shape_count;

    /* 0x14 — Zaehler, den der Rahmen bhEne09 abwaerts zaehlt (en09.c:2055). */
    int32_t  ct_14;

    /* 0x18 — Zustandswort, Bits siehe RECVX_EN09_ST_* oben. */
    uint32_t state;

    /* 0x1C / 0x20 — zwei weitere Rahmenzaehler (en09.c:2087 / :2092). */
    int32_t  ct_1c;
    int32_t  ct_20;

    /* 0x24 — in den bisher gelesenen Funktionen nicht beruehrt. */
    uint8_t  reserved_24[4];

    /* 0x28 — Abstand zum Spieler, in jedem MVType-Eintritt neu gesetzt
       (en09.c:2959/2975/2989 njDistanceP2P). */
    float    ply_dist;

    /* 0x2C/0x34 — Zielpunkt der Fortbewegung (X und Z), aus bhCheckRoute
       bzw. direkt aus der Spielerlage (en09.c:3065/3070, :3195/:3200).
       HINWEIS: an einer noch nicht portierten Stelle wird 0x2C als int
       gelesen; das wird dort mit Fundstelle aufgeloest, nicht vorher
       geraten. */
    float    trg_x;
    uint8_t  reserved_30[4];
    float    trg_z;

    /* 0x38..0x40 — noch nicht belegt benannt. */
    uint8_t  reserved_38[12];

    /* 0x44/0x4C — zweiter Zielpunkt (X und Z), den der Etagenwechselzweig
       von bhEne09_Brain anlaeuft (en09.c:3193/3200-3201). */
    float    trg2_x;
    uint8_t  reserved_48[4];
    float    trg2_z;

    /* 0x50 — im Original ein ATR_WORK* der zuletzt getroffenen Wandflaeche
       (en09.c:2203: *(ATR_WORK**)(epw->exp0 + 0x50) = hp). Die nativen
       Kollisionsdienste geben keine Originalzeiger heraus, deshalb steht hier
       — wie in native/enemies/en07.h fuer EXP0_ATR(7) — eine stabile
       Flaechenidentitaet; der zugehoerige Datensatz liegt hinter dem
       Originalblock. 0 = keine Flaeche. Die Feldbreite bleibt 4 Byte, damit
       alle folgenden Offsets ihre Originallage behalten. */
    uint32_t wall_atr;

    /* 0x54 — ZWEITE Flaeche: die Treppen-/Wegflaeche, die bhEne09_Brain
       ablegt (en09.c:3093/3156/3181) und bhEne09_JumpCheck wieder liest
       (:3204). Gleiche Behandlung wie wall_atr: stabile Identitaet statt
       Originalzeiger, Datensatz hinter dem Originalblock. 0 = keine. */
    uint32_t route_atr;

    /* 0x58..0x68 — noch nicht belegt benannt. */
    uint8_t  reserved_58[20];

    /* 0x6C — Rueckgabeplatz der Armlaenge: bhEne09_Brain gibt die Adresse
       dieses Feldes an bhEne09_ChkArmLen (en09.c:3118). */
    float    arm_len_out;

    /* 0x70..0x78 — noch nicht belegt benannt. */
    uint8_t  reserved_70[12];

    /* --- Nativ zusaetzlich, AUSSERHALB des Originalblocks (der endet bei
       0x7C): der Datensatz zur Flaeche `wall_atr`. Noetig, weil die nativen
       Kollisionsdienste keine Originalzeiger herausgeben. Alle Felder bis
       einschliesslich reserved_54 behalten ihre Originallage. --- */
    RECVX_ENEMY_ATR wall_rec;
    int             wall_rec_valid;
    RECVX_ENEMY_ATR route_rec;
    int             route_rec_valid;
} RECVX_EN09_EXT;

/* Passgebundene Anbieter am vorhandenen World-/Assetbesitzer. */
typedef struct {
    int (*game_mode)(void);
    int (*weapon_flags)(uint32_t,uint32_t*);
    void (*material)(RECVX_ENEMY_WORK*,uint32_t);
    int (*node)(const RECVX_ENEMY_WORK*,uint32_t,RECVX_ENEMY_VEC*,int32_t*);
    void (*node_pos)(RECVX_ENEMY_WORK*,uint32_t,uint32_t,float);
    void (*node_ang)(RECVX_ENEMY_WORK*,uint32_t,uint32_t,int32_t);
    void (*fix_offset)(RECVX_ENEMY_WORK*,const int8_t*,const RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC*);
    void (*model_init)(RECVX_ENEMY_WORK*);
    void (*player_node_flags_clear)(uint32_t,uint32_t);
    int (*search)(const RECVX_ENEMY_WORK*,const RECVX_ENEMY_VEC*,int32_t,int32_t,int32_t*);
    int (*route)(const RECVX_ENEMY_VEC*,const RECVX_ENEMY_VEC*,RECVX_ENEMY_VEC*);
    int (*wall)(const RECVX_ENEMY_VEC*,uint32_t,float,float,RECVX_ENEMY_ATR*);
    int (*floor_area)(const RECVX_ENEMY_VEC*,int,uint8_t,uint8_t,RECVX_ENEMY_ATR*);
    int (*player_chain)(RECVX_ENEMY_VEC*);
    void (*smoke)(RECVX_ENEMY_WORK*,const RECVX_ENEMY_VEC*,int);
} RECVX_EN09_SERVICES;
void recvx_en09_bind(const RECVX_EN09_SERVICES* services);
void recvx_en09(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
