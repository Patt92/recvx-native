#ifndef RECVX_NATIVE_EN04_H
#define RECVX_NATIVE_EN04_H
#include <stdint.h>
#include "enemy_work.h"

/* ===========================================================================
   Identitaet 4 — Cerberus (Hund). Nativ eigenstaendig bestimmt.
   ---------------------------------------------------------------------------
   Kein Originalkoerper, kein Generator, keine zweite Workwelt, kein
   Originalkern-Dispatcher. Zustandsbesitzer bleibt ausschliesslich
   RECVX_ENEMY_WORK (native/enemy_work.h) samt dem hier beschriebenen
   exp0-Block; diese Uebersetzungseinheit haelt nur die beiden
   Originalstatics von en04.c (siehe recvx_en04_reset_shared).

   Semantikreferenz (nur gelesen, jede Stelle mit Datei:Zeile belegt):
     en04.c:26    En04_PlyMtn_OffsetTbl[4]
     en04.c:33    en04_mtn_tbl[30]        SE je MTN/Frame
     en04.c:215   en04_mtn_tbl2[6]        Vibration je MTN/Frame
     en04.c:254   en04_tree[1][8]         Knotenkette fuer bhEne_CalcPartsPos
     en04.c:259   En04SdwTab[6]           Schattengelenke
     en04.c:264   En04_WpnDamageTbl[22]
     en04.c:289   CombWepTbl[21]          (im Original alles 0)
     en04.c:312   CombJointTbl[19]        (im Original alles 0)
     en04.c:336   Ene04CapColTab[17]
     en04.c:356   en04_BldTbl             BP_WORK der Blutlache
     en04.c:366   en04prt_blood_tbl[19]   BT_WORK je Koerperteil
     en04.c:390   en04_hp_tbl[2][16]      -> native/combat_tables_data.inc:620
     en04.c:398   en04_flipTree[20]       bhSetMotion-Spiegeltabelle
     en04.c:411   en04_contact_flg / en04_atari_flg (Datei-Statics)
     en04.c:414   bhEne04_Mode0[6]        :424 MoveType :431 NageType
                  :438 DamageType :445 DieType :452 BrainMode2[12]
                  :468 MoveMode2[13] :485 NageMode2[2] :491 DamageMode2[3]
                  :498 DieMode2[1] :503 PlyDmgMode[2]
     en04.c:510   bhEne04_DmmyBrain (leer)
     en04.c:516   bhEne04                 Rahmen eines Passes
     en04.c:558   bhEne04_MainLoop
     en04.c:600   bhEne04_SetCollision
     en04.c:626   bhEne04_DmgChk          :688 ChgDmgMode  :733 DamageAdd
     en04.c:816   bhEne04_PlayerControl   :825 PlayerLink
     en04.c:842   bhEne04_CollisionCheck  :855 CollCheckWall
     en04.c:939   bhEne04_Init
     en04.c:1020  Move :1027 Nage :1034 Damage :1041 Die
     en04.c:1048  MVType00 :1117 MVType01 :1152 MVType02
     en04.c:1187  AtariCheck :1263 AtariCheck2 :1340 EnemyAtariCheck2
     en04.c:1374  EneSearch :1420 Brain :1453 Brain00 :1464 Brain01
                  :1478 Brain02 :1573 Brain04 :1621 Brain06
     en04.c:1656  MV00 :1713 MV01 :1792 MV02 :1970 MV03 :2056 MV04
                  :2089 MV05 :2137 MV06 :2205 MV07 :2415 MV08 :2603 MV09
                  :2662 MV10 :2709 MV11 :2788 MV12 :2826 Escape
     en04.c:2922  NGType00 :2939 NG00 :3052 NG01
     en04.c:3100  DGType00 :3106 DG00 :3246 DG01
     en04.c:3364  DDType00 :3370 DD00
     en04.c:3389  PlyDG00 :3481 PlyDG01 :3562 PlyDamageCheck
     en04.c:3631  SpeedUp :3644 SpeedDown :3657 RunMotion :3703 ChgMtn
     en04.c:3722  SetMtn :3860 CheckMtnTbl :3901 KaidanCheck
     en04.c:3955  ShakeWire :3997 RotNeck :4008 SearchNeck :4048 SePlay
     zonzon.c:10  ikou :45 ikou3 :70 NitenDir_ck :76 bhCdirCheck
                  :101 bhDGCdirCheck :178 bhEne_AngleCheck
                  :207 bhEne_GetTranslateMtn :286 bhEne_CalcPartsPos
                  :310 bhKaidanAtrCheck :373 bhEne_EnemyAtariCheck
                  :402 bhEne_PosCheck :1222 bhEne_CollisionCheckWall2
                  :1255 bhEne_CheckDirWall :1302 bhEne_CheckSideWall
                  :1346 bhEne_SetVibration :1367 bhEne_PlayerSePlay
                  :1376 bhEne_HitCheckParts
     zonzon1.c:725 bhEne_CalcDamage  :806 bhEne_InitDamage
     pwksub.c:133 bhAddSpeed :664 bhSearchPlayer
     effsub3.c:145 bhSetBloodPoolLnk
     en01.c:1345  kaidan_ang[4] (gemeinsame Treppenrichtungen)
     MdlPut.c:240 bhCalcModel
     macros.h:35  EXP0_*-Offsetmakros
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
   Reine Datentabellen des Originals, die der Mechanismus selbst auswertet
   --------------------------------------------------------------------------- */

/* en04.c:264 WPNDAMAGE_WORK (effect.h): Wirkung je Waffe. nm_act/cb_act
   waehlen den Schadenmodus, nm_blood/cb_blood den Bluttyp; flg traegt die
   Zusatzwirkungen (1 Hackfleisch, 2/4 Feuer, 8 kein Blut, 0x10 Saeure).
   Feldfolge wie types.h:1931 WPNDAMAGE_WORK {flg, nm_act, nm_blood, cb_act,
   cb_blood}; die Tabelle en04_tables.c steht zeilengleich in dieser Folge.
   (Vorher stand cb_act vor nm_blood: der toetende Treffer las nm_blood 7 als
   cb_act, ChgDmgMode :720 liess mode0 stehen, der Hund starb nie.) */
typedef struct {
    uint8_t flg;
    uint8_t nm_act;
    uint8_t nm_blood;
    uint8_t cb_act;
    uint8_t cb_blood;
} RECVX_EN04_WPNDAMAGE;

/* en04.c:289 COMBWEP_WORK (zonzon1.h:28, Groesse 0x18) und :312
   COMBJOINT_WORK (zonzon1.h:21, Groesse 0x8). Feldlage wie im T3-Spiegel
   native/en03_lib.h belegt: crit 0x00, pt[3] 0x04, timeout 0x10, bonus 0x14
   bzw. flg 0x00, correct 0x04. En04 fuehrt beide Tabellen vollstaendig mit
   Nullen, deshalb sind die Felder hier nur fuer bhEne_CalcDamage da. */
typedef struct { int32_t crit; int32_t pt[3]; int32_t timeout; int32_t bonus; } RECVX_EN04_COMBWEP;
typedef struct { uint32_t flg; int32_t correct; } RECVX_EN04_COMBJOINT;

/* en04.c:366 BT_WORK (effect.h) in der Feldreihenfolge der Originaltabelle
   (lnk_obj, x, y, z, xlen, ylen, size, len). */
typedef struct {
    int32_t lnk_obj;
    float   x, y, z;
    float   xlen, ylen;
    float   size;
    float   len;
} RECVX_EN04_BT;

/* en04.c:33/215 EN04_MTBL (en04.h des Originals): je MTN bis zu vier
   (frm, act)-Paare, -1 beendet. mtn_tbl loest `act` als SE-Nummer von
   bhEne04_SePlay aus, mtn_tbl2 als Vibrationsstufe. */
typedef struct { uint32_t frm; uint32_t act; } RECVX_EN04_MACT;
typedef struct { uint32_t no; RECVX_EN04_MACT atb[4]; } RECVX_EN04_MTBL;

/* ---------------------------------------------------------------------------
   exp0 — im Original ein 88 Byte grosser Rohblock aus bhEne_CallocWork
   (en04.c:966 `bhEne_CallocWork(88, 8)`) mit festen Offsets (macros.h:35).
   Nativ benannt; Inhalt, Nullstart und Lebensdauer bleiben gleich. Die
   Offsetkommentare und die _Static_asserts in en04.c belegen die Zuordnung.
   --------------------------------------------------------------------------- */

/* 0x48 traegt im Original ZWEI Bedeutungen in demselben Wort:
   EXP0_F(0x48) ist die Sprunggeschwindigkeit (MV07/MV08/DG00/DG01) und
   EXP0_I(0x48) der Zustand von bhEne04_Escape (en04.c:2834). bhEne04_MV06
   nullt vor dem Wechsel in den Escape-Zweig genau dieses Wort zusammen mit
   0x4C/0x50 (en04.c:2196-2198). Diese Ueberlagerung ist Original und bleibt
   als Union erhalten; sie wird nicht in zwei Felder aufgeteilt. */
typedef union { float jump; int32_t esc_mode; } RECVX_EN04_U48;

typedef struct RECVX_EN04_EXP0 {
    /* 0x00 EXP0_UC(0): Suchzustand. Bits 0x1F Zaehler, 0x20 Spieler in
       diesem Zyklus gesehen, 0x40 Sichtergebnis des letzten Zyklus,
       0x80 Suche laeuft noch (bhEne04_EneSearch, en04.c:1374). */
    uint8_t  search;
    uint8_t  reserved_01;
    uint8_t  reserved_02;
    /* 0x03 EXP0_UC(0x3): von bhEne04_ChgDmgMode im Kombozweig genullt
       (en04.c:707). En04 liest das Byte nirgends; der Platz bleibt, damit
       die folgenden Felder ihre Originallage behalten. */
    uint8_t  dmg_sub;
    /* 0x04 EXP0_C(0x4): Etage vor dem Treppenwechsel (en04.c:2727). */
    int8_t   stair_flr;
    /* 0x05 EXP0_C(0x5)/EXP0_UC(0x5): Stufenhoehe prm2 der Treppenflaeche
       (en04.c:2714/2720). */
    int8_t   stair_h;
    /* 0x06 EXP0_US(0x6)/EXP0_S(0x6): Wandtrefferhistorie, je Pass um eins
       nach links geschoben und mit 0xE maskiert (en04.c:864). */
    uint16_t wall_hist;
    /* 0x08..0x0F beruehrt en04 nirgends. Platz erhalten. */
    uint8_t  reserved_08[8];
    /* 0x10 EXP0_I(0x10): die Flagbank des Hundes.
          0x0000000F  Kollisionsmodus (1 = eigene Wand, 2 = Spielerwand)
          0x00000010  Treppenlauf aktiv (bhEne04_CollCheckWall aus)
          0x00000020  bhCheckPlayer erlaubt
          0x00000080  Nackendrehung aktiv (bhEne04_SearchNeck)
          0x00000100  Kombotreffer (waehlt die Rueckwaerts-Trefferanimation)
          0x00000200  Spieler schaut weg (NGType00)
          0x00000400  Sprung laeuft
          0x00000800  Richtungswechsel-Wartezeit aktiv
          0x00001000  harte Kurve verlangt (MV02 Zweig 2/3)
          0x00002000  Spieler im vorderen Sektor
          0x00004000  Fluchtweg frei (bhEne04_Escape)
          0x00008000  Hund steht in einer prm1-5-Flaeche
          0x00010000  Treppe aufwaerts (sonst abwaerts)
          0x00020000  Treppenmodus belegt
          0x00100000  harter Schadenwechsel (MTN 14)
          0x00200000  Spieler in einer prm1-0-Flaeche
          0x00400000  Spieler erkannt -> Verfolgung
          0x02000000  Greifnachlauf (DmgChk-Sperre)
          0x04000000  Griff hat gerade begonnen
          0x00040000  Spielerschaden-Modus laeuft (PlayerControl)
          0x00080000  Spieler haengt an der Hundelage (PlayerLink)
          0x40000000  Wurzeltranslation der Motion uebernehmen (SetMtn) */
    uint32_t flg;
    /* 0x14/0x18 EXP0_I: zwei Abwaertszaehler im Rahmen (en04.c:546/551).
       0x18 ist die Sperre der Feuer-/Saeureeffekte (en04.c:794/806). */
    int32_t  ct_14;
    int32_t  ct_18;
    /* 0x1C EXP0_I(0x1C): Nackenwinkel, +-4096 begrenzt (en04.c:4008). */
    int32_t  neck;
    /* 0x20 EXP0_F(0x20): waagerechter Abstand zum Spielergelenk 1. */
    float    dist;
    /* 0x24/0x28/0x2C EXP0_F: Zielpunkt von ikou/ikou3. Das Original
       uebergibt ihn als (NJS_POINT3*)&EXP0_UC(0x24) (en04.c:1753); 0x28
       traegt in bhEne04_MV08 zusaetzlich die Bodenhoehe (en04.c:2487). */
    RECVX_ENEMY_VEC target;
    /* 0x30/0x34/0x38 EXP0_F: Mehrfachbelegung des Originals in genau diesen
       zwoelf Byte — Treppenstartlage (en04.c:1565/2747), Spielerversatz im
       Griff (PlayerLink :831, PlyDG00 :3419, PlyDG01 :3493). */
    RECVX_ENEMY_VEC link;
    /* 0x3C EXP0_I(0x3C): Ergebnis von bhCheckWallType. Das Original haelt
       den ATR_WORK-Zeiger; en04 prueft ihn ausschliesslich auf != 0
       (en04.c:1766/1855/2007/2849), deshalb traegt das Wort nativ nur das
       Praedikat. */
    int32_t  wall_hit;
    /* 0x40 EXP0_I(0x40): die Treppenflaeche aus bhKaidanAtrCheck. Das
       Original haelt den ATR_WORK-Zeiger und liest daraus prm0 (+0x20),
       prm1 (+0x21) und prm2 (+0x22) (en04.c:2712/2730/3264). Nativ traegt
       das Wort nur das Praedikat; der Datensatz liegt in stair_rec hinter
       dem Originalblock, weil die nativen Dienste keine Originalzeiger
       herausgeben. */
    int32_t  stair_atr;
    /* 0x44 EXP0_I(0x44): Ergebnis von bhCollisionCheckLine zum Spieler. Das
       Original prueft den Zeiger auf != 0 UND liest dessen type-Byte
       (ATR_WORK Offset 0x1, en04.c:1385-1389). Nativ: Praedikat hier,
       type-Byte in line_type. */
    int32_t  line_hit;
    /* 0x48 siehe RECVX_EN04_U48. */
    RECVX_EN04_U48 u48;
    /* 0x4C/0x50 EXP0_I: Fluchtzaehler und Fluchtdrehrest (en04.c:2826). */
    int32_t  esc_ct;
    int32_t  esc_turn;
    /* 0x54 unbenutzt; der Originalblock ist 88 Byte gross. */
    uint32_t reserved_54;

    /* --- Nativ zusaetzlich, AUSSERHALB der Originaloffsets ----------------
       Alle Felder bis einschliesslich reserved_54 behalten ihre Lage. Diese
       Zusaetze ersetzen die Originalzeiger, die die nativen Dienste nicht
       herausgeben; sie erhoehen nur den Verbrauch am 0xC40000-Budget von
       bhEne_CallocWork. */
    RECVX_ENEMY_ATR stair_rec;   /* Datensatz zu stair_atr                 */
    int32_t  stair_idx;          /* Listenindex fuer bhSetUse/ClrUseKaidanFlag */
    uint8_t  line_type;          /* ATR_WORK.type der Linienflaeche (0x44) */
    uint8_t  line_type_valid;
} RECVX_EN04_EXP0;

/* ---------------------------------------------------------------------------
   Datenbeschaffung
   ---------------------------------------------------------------------------
   en04_hp_tbl[2][16] liegt bereits als reines Datenmodul in
   native/combat_tables_data.inc:620. Diese Datei wird in die
   C++-Uebersetzungseinheit native/combat.cpp eingebunden, wo ein
   `const`-Objekt im Namensraum `recvx` liegt; ein C-Modul kann es nicht
   unter seinem Namen binden. Der Integrator reicht den Zeiger deshalb
   genauso durch wie fuer en01_hp_tbl (native/en01_tables.h:246). Bleibt er
   ungebunden, meldet bhEne04_Init die Luecke und setzt KEINEN Ersatzwert.
   --------------------------------------------------------------------------- */
typedef struct RECVX_EN04_DATA {
    const int (*hp_tbl)[16];     /* en04.c:390, Zeile 0 = gm_mode != 2     */
} RECVX_EN04_DATA;

void recvx_en04_set_data(const RECVX_EN04_DATA* data);

/* ---------------------------------------------------------------------------
   Sicht auf sys->obwp[i] (O_WRK, types.h) — die Felder, die en04 schreibt
   ---------------------------------------------------------------------------
   bhEne04 (en04.c:539) laeuft die Objekttabelle ab Index 4 bis rom->obj_n-2
   und ruettelt jedes Objekt mit flg Bit 0, id 52 und flg2 Bit 0; der
   Tuerzweig von bhEne04_MVType00 (en04.c:1100) schreibt ayp/mode3/flg2 des
   Objekts, das die Flaeche in prm2 nennt. Der Objektbaum gehoert dem
   vorhandenen nativen Besitzer; dies ist nur eine transparente Sicht mit
   Lesen-Aendern-Schreiben, weil das Original Felder und Bits mischt.
   RECVX_ENEMY_OBJECT (enemy_work.h) fuehrt id/flg2/mode3/ct0/ay/ayp nicht
   und reicht dafuer nicht aus.
   --------------------------------------------------------------------------- */
typedef struct {
    uint32_t flg, flg2;
    uint16_t id;
    uint8_t  mode1, mode3;
    int32_t  ct0;
    float    px, py, pz;
    int32_t  ay, ayp;
} RECVX_EN04_OBJECT;

/* ---------------------------------------------------------------------------
   Dienste, die EN04 zusaetzlich zu RECVX_ENEMY_SERVICES braucht
   ---------------------------------------------------------------------------
   Jeder Eintrag nennt seinen originalen Aufruf. Ein nicht gebundener Dienst
   ist NULL; der Mechanismus meldet ihn dann namentlich ueber services->gap
   und tut nichts. Kein Eintrag darf durch einen plausiblen Ersatzwert belegt
   werden. Wo native/en03_lib.h (T3) denselben Originalmechanismus schon
   fuehrt, ist das vermerkt — der Integrator bindet beide Seiten an denselben
   Anbieter, damit es keinen zweiten Mechanismus gibt.
   --------------------------------------------------------------------------- */
typedef struct RECVX_EN04_SERVICES {
    /* --- Raum-ATR-Listen --------------------------------------------------
       rom->flrp[0..rom->flr_n-1] gefolgt von sys->mflrp[0..sys->mflr_n-1] in
       Originalreihenfolge (bhEne_EnemyAtariCheck, zonzon.c:373; en04.c:1212
       und :1288 laufen dieselbe Liste selbst ab). Derselbe Anbieter wie
       RECVX_ENEMY_EXT.flr_count/flr_at (native/en03_lib.h). */
    uint32_t (*flr_count)(void);
    int      (*flr_at)(uint32_t index, RECVX_ENEMY_ATR* out);

    /* zonzon.c:310 bhKaidanAtrCheck(epw, len, &idx): Treppenflaeche `len`
       vor dem Gegner. Rueckgabe 1 = getroffen. `rec` erhaelt den ATR-Satz
       (prm0/prm1/prm2 werden gebraucht), `index` den Listenindex fuer
       stair_use. Gleiche Rolle wie RECVX_EN01_SERVICES.stair_area_atr. */
    int (*stair_area_atr)(RECVX_ENEMY_WORK* w, float len,
                          RECVX_ENEMY_ATR* rec, int* index);

    /* rutchk.c bhCheckRoute(from, to, out): naechster Wegpunkt auf dem Weg
       zum Spieler (en04.c:1439/1446). Der Basisdienst RECVX_ENEMY_SERVICES
       .route hat eine andere Signatur (int,int) und ist damit fuer diesen
       Aufruf nicht verwendbar; das ist im T1-Bericht als Vertragsfehler
       vermerkt. Rueckgabe 1 = `out` gesetzt. */
    int (*check_route)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to,
                       RECVX_ENEMY_VEC* out);

    /* zonzon.c:1222 bhEne_CollisionCheckWall2(pw, ps, ops, pd, ar, ah):
       bhCheckDansa, dann bhCheckWallEx und bei Kontakt bhCheckWallType;
       `pd` ist die Verschiebung. Original liefert ATR_WORK*; en04 prueft nur
       != 0 (en04.c:898). Gleiche Rolle wie recvx_t3_collision_check_wall2. */
    int (*collision_check_wall2)(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* ps,
                                 const RECVX_ENEMY_VEC* ops, RECVX_ENEMY_VEC* pd,
                                 float ar, float ah);

    /* zonzon1.c:725 bhEne_CalcDamage(epw, CombWepTbl, CombJointTbl) — bildet
       aus dam[] den total_dam des Passes (en04.c:628). Die beiden Tabellen
       sind En04-Daten und werden deshalb uebergeben. */
    void (*calc_damage)(RECVX_ENEMY_WORK* w, const RECVX_EN04_COMBWEP* wep,
                        const RECVX_EN04_COMBJOINT* joint);

    /* weapon.c WpnTab[wpnr_no].flg (en04.c:633/634). 1 = Wert geliefert. */
    int (*weapon_flg)(uint32_t wpnr_no, uint32_t* out);

    /* npSetAllMatColor(mlwP->objP, mlwP->obj_num, color) — Init-Toenung
       0xFFB2B2B2 (en04.c:993) und Feuertod 0xFF000000 (en04.c:797). */
    void (*set_material_color)(RECVX_ENEMY_WORK* w, uint32_t color);

    /* mlwP->objP[node].pos / .ang: LOKALE Knotenlage des eigenen Modells.
       Lesen braucht bhEne_CalcPartsPos (zonzon.c:286), Schreiben die
       Wurzelversaetze in bhEne04_SetMtn (en04.c:3770 ff.) und die
       Nackenwinkel in bhEne04_RotNeck (en04.c:4001-4003). `axis` 0/1/2. */
    int  (*node_transform)(const RECVX_ENEMY_WORK* w, uint32_t node,
                           RECVX_ENEMY_VEC* pos, int32_t* ang3);
    void (*node_pos_set)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t axis, float v);
    void (*node_ang_set)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t axis, int32_t v);

    /* zonzon.c:415 bhEne_SetBlood(epw, type, bt) und :466
       bhEne_SetBlood2(epw, type, ofp, ry) am GEGNERwork (en04.c:763/767,
       3812/3813). set_blood2 ist nicht const: die Typen 3/6/7/8/10/11
       nullen im Original ofp->z im Speicher des Aufrufers. */
    void (*set_blood)(RECVX_ENEMY_WORK* w, uint8_t type, const RECVX_EN04_BT* bt);
    void (*set_blood2)(RECVX_ENEMY_WORK* w, uint8_t type, RECVX_ENEMY_VEC* ofs, int32_t ry);

    /* Dieselben beiden Originalroutinen, aber mit plp als Work
       (en04.c:2976 `bhEne_SetBlood(plp, 1, NULL)` und en04.c:3609
       `bhEne_SetBlood2(plp, 6, &pos, rot)`). Nativ hat der Spieler einen
       eigenen Besitzer, deshalb ein eigener Eintrag; recvx_t3_set_blood*
       kann den Spielerwork in seiner heutigen Signatur nicht ausdruecken. */
    void (*player_set_blood)(uint8_t type, const RECVX_EN04_BT* bt);
    void (*player_set_blood2)(uint8_t type, RECVX_ENEMY_VEC* ofs, int32_t ry);

    /* zonzon.c:832/996 bhEne_SetDFireEffect / bhEne_SetSanEffect
       (en04.c:790/799/810). */
    int (*set_dfire_effect)(RECVX_ENEMY_WORK* w, int no, const RECVX_EN04_BT* bt, int type);
    int (*set_san_effect)(RECVX_ENEMY_WORK* w, int no, const RECVX_EN04_BT* bt);

    /* zonzon1.c:803 bhEne_SetNikuhenEffect2(epw, type, pos, n, tex_id)
       (en04.c:779). */
    void (*nikuhen_effect2)(RECVX_ENEMY_WORK* w, int type, const RECVX_ENEMY_VEC* pos,
                            int n, int tex_id);

    /* effsub3.c:145 bhSetBloodPoolLnk(ewP, posP, ay, tabP, pal_bnk)
       (en04.c:3379). Nicht derselbe Aufruf wie der Basisdienst blood_pool
       (zonzon1.c:391 bhEne_BloodPool); beide Originale existieren getrennt. */
    int (*blood_pool_lnk)(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                          int32_t ay, const RECVX_ENEMY_BP_WORK* tab, int pal_bnk);

    /* zonzon.c:1376 bhEne_HitCheckParts(plp, pos) — naechstes Spielergelenk
       zum Trefferpunkt; schreibt plp->djnt_no/dvx.. (en04.c:3597). */
    void (*player_hit_check_parts)(const RECVX_ENEMY_VEC* pos);
    /* plp->djnt_no und plp->dpx/dpy/dpz, die bhEne04_NG00 direkt setzt
       (en04.c:2972-2974) bzw. die bhEne04_DmgChk ueber plp->dvx liest
       (en04.c:664). `dv` darf NULL sein. */
    void (*player_hit_set)(int32_t djnt_no, const RECVX_ENEMY_VEC* dv,
                           const RECVX_ENEMY_VEC* dp);
    int  (*player_hit_get)(int32_t* djnt_no, RECVX_ENEMY_VEC* dv, RECVX_ENEMY_VEC* dp);

    /* plp->watr — Kollisionskapsel des Spielers (en04.c:3578). */
    int (*player_capsule)(RECVX_ENEMY_CAPSULE* out);

    /* plp->mlwP->objP[node].pos: bhEne04_PlyDG00 nullt pos[0] und pos[2]
       des Spielerwurzelknotens (en04.c:3452-3454). */
    void (*player_node_pos_set)(uint32_t node, uint32_t axis, float v);

    /* plp->exp0: bhEne04_PlyDG00 legt die Griffendlage in PEXP0_F(0x48) und
       PEXP0_F(0x50) ab (en04.c:3448/3449). Der Block gehoert dem
       Spielerbesitzer; `offset` ist der Originaloffset. */
    void (*player_exp0_float_set)(uint32_t offset, float v);

    /* sys->pad_on &= ~bits (en04.c:671/3464). Gleiche Rolle wie
       RECVX_ENEMY_EXT.pad_on_clear. */
    void (*pad_on_clear)(uint32_t bits);

    /* sys->obwp[] und rom->obj_n (en04.c:539, :1100). */
    uint32_t (*object_count)(void);
    int      (*object_get)(uint32_t index, RECVX_EN04_OBJECT* out);
    void     (*object_set)(uint32_t index, const RECVX_EN04_OBJECT* in);

    /* en04.c:1628/1643 plp->spd, fehlt in der gemeinsamen Player-Sicht. */
    int (*player_speed)(float* out);
    /* en04.c:914-933: CalcPartsPos(plp,{0,1,2,3,4,5},parts_num,1),
       Ziel-Y = plp->py, Vorpose = owP[previous_joint].mtx[x,z]/plp->pyb,
       dann CollisionCheckWall2(plp,...,plp->ar,plp->ah). Keine Gegnerkopie
       als Spielerersatz. 0 bedeutet Dienst nicht ausfuehrbar. */
    int (*player_collision_parts)(uint32_t parts_num, uint32_t previous_joint,
                                  RECVX_ENEMY_VEC* delta);
} RECVX_EN04_SERVICES;

/* ---------------------------------------------------------------------------
   Laufzeitkontext eines Raums
   --------------------------------------------------------------------------- */
typedef struct RECVX_EN04_CONTEXT {
    const RECVX_EN04_SERVICES* en04;      /* Erweiterung dieser Datei      */
    uint32_t                   game_mode; /* sys->gm_mode (en04.c:957)     */
} RECVX_EN04_CONTEXT;

void recvx_en04_set_context(const RECVX_EN04_CONTEXT* ctx);

/* ---------------------------------------------------------------------------
   Einstieg und Zustandsfunktionen
   ---------------------------------------------------------------------------
   bhEne04 (en04.c:516) ist der Eintrag, den die Dispatchtabelle je Pass fuer
   Identitaet 4 ruft. Die Unterfunktionen bleiben sichtbar, damit der
   Dispatchbesitzer und gezielte Pruefungen jede Phase einzeln ansteuern
   koennen, ohne den Zustand nachzubilden.
   --------------------------------------------------------------------------- */
void recvx_en04(RECVX_ENEMY_WORK* w);                 /* en04.c:516  */
void recvx_en04_main_loop(RECVX_ENEMY_WORK* w);       /* en04.c:558  */
void recvx_en04_set_collision(RECVX_ENEMY_WORK* w);   /* en04.c:600  */
void recvx_en04_dmg_chk(RECVX_ENEMY_WORK* w);         /* en04.c:626  */
void recvx_en04_chg_dmg_mode(RECVX_ENEMY_WORK* w);    /* en04.c:688  */
void recvx_en04_damage_add(RECVX_ENEMY_WORK* w);      /* en04.c:733  */
void recvx_en04_player_control(RECVX_ENEMY_WORK* w);  /* en04.c:816  */
void recvx_en04_player_link(RECVX_ENEMY_WORK* w);     /* en04.c:825  */
void recvx_en04_collision_check(RECVX_ENEMY_WORK* w); /* en04.c:842  */
void recvx_en04_coll_check_wall(RECVX_ENEMY_WORK* w); /* en04.c:855  */
void recvx_en04_init_work(RECVX_ENEMY_WORK* w);       /* en04.c:939  */
void recvx_en04_move(RECVX_ENEMY_WORK* w);            /* en04.c:1020 */
void recvx_en04_nage(RECVX_ENEMY_WORK* w);            /* en04.c:1027 */
void recvx_en04_damage(RECVX_ENEMY_WORK* w);          /* en04.c:1034 */
void recvx_en04_die(RECVX_ENEMY_WORK* w);             /* en04.c:1041 */
int  recvx_en04_atari_check(RECVX_ENEMY_WORK* w);     /* en04.c:1187 */
int  recvx_en04_atari_check2(RECVX_ENEMY_WORK* w);    /* en04.c:1263 */
void recvx_en04_ene_search(RECVX_ENEMY_WORK* w);      /* en04.c:1374 */
void recvx_en04_brain(RECVX_ENEMY_WORK* w);           /* en04.c:1420 */
int  recvx_en04_escape(RECVX_ENEMY_WORK* w, int res, int r); /* en04.c:2826 */
int  recvx_en04_ply_damage_check(RECVX_ENEMY_WORK* w, int type); /* en04.c:3562 */
int  recvx_en04_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no, int32_t frm, int32_t rate); /* :3703 */
int  recvx_en04_set_mtn(RECVX_ENEMY_WORK* w);         /* en04.c:3722 */
void recvx_en04_check_mtn_tbl(RECVX_ENEMY_WORK* w, int frm); /* en04.c:3860 */
int  recvx_en04_kaidan_check(RECVX_ENEMY_WORK* w);    /* en04.c:3901 */
void recvx_en04_shake_wire(RECVX_ENEMY_WORK* w, uint32_t object); /* en04.c:3955 */
void recvx_en04_rot_neck(RECVX_ENEMY_WORK* w, int32_t rx, int32_t ry, int32_t rz); /* :3997 */
int  recvx_en04_search_neck(RECVX_ENEMY_WORK* w);     /* en04.c:4008 */
void recvx_en04_se_play(RECVX_ENEMY_WORK* w, int32_t no); /* en04.c:4048 */

/* en04.c:411 en04_contact_flg / en04_atari_flg sind Datei-Statics des
   Originals: EIN Satz fuer alle Hunde eines Raums, von bhEne04_Init
   genullt (en04.c:943/944). Diese Uebersetzung haelt sie genauso; der
   Raumbesitzer kann sie beim Raumwechsel zusaetzlich zuruecksetzen, falls
   kein Hund initialisiert wird. */
void recvx_en04_reset_shared(void);
uint32_t recvx_en04_contact_flags(void);
uint32_t recvx_en04_area_flags(void);

#ifdef __cplusplus
}
#endif
#endif
