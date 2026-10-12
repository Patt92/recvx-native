#ifndef RECVX_NATIVE_EN01_WORK_H
#define RECVX_NATIVE_EN01_WORK_H
#include <stdint.h>
#include "enemy_work.h"
/* Nur wegen der Zeilentypen der EN01-Datentabellen (RECVX_EN01_BT fuer die
   Blutvorlagen), die der Dienstvertrag dieser Datei nennt. en01_tables.h
   bindet seinerseits nur enemy_work.h ein, kein Zyklus. */
#include "en01_tables.h"

/* ===========================================================================
   T2 — EN01 (Zombie) auf dem gemeinsamen Enemy-Work-Vertrag
   ---------------------------------------------------------------------------
   Einziger Gegnerbesitzer bleibt die vorhandene native WorldEntity/RoomWorld.
   Diese Datei legt KEINE zweite Workwelt an: der EN01-Zustand besteht aus
     (a) den Feldern des gemeinsamen RECVX_ENEMY_WORK (native/enemy_work.h) und
     (b) genau EINEM Erweiterungsblock, der im Original per bhEne_CallocWork
         angefordert und an epw->exp0 gehaengt wird (en01.c:2599, 0xB0 Byte).
   Das ist derselbe Block wie im Original, nur mit benannten Feldern statt
   EXP0_I/EXP0_F-Byteoffsets — eine Datenrepraesentations-Anpassung, keine
   andere Entscheidung. Die _Static_assert-Blocke in en01_core.c belegen
   Reihenfolge und Originaloffsets: jedes Feld liegt an seinem originalen
   Offset, die acht Zeigerfelder am Blockanfang ausgenommen, hinter denen
   alles um genau deren Zeigergroessenzuwachs versetzt liegt.

   ABGRENZUNG zum verbotenen Muster in native/en01_damage.h: dort ist
   `RECVX_EN01` ein 16-KiB-OPAKER, per shared_ptr getrennt allokierter Block,
   der Pose/HP/Modi PARALLEL zur WorldEntity traegt — zwei Besitzer. Hier
   traegt der Block ausschliesslich die Felder, die das Original zusaetzlich
   zu BH_PWORK haelt; Pose/HP/Modi liegen allein im gemeinsamen Work und
   werden von dort ueber recvx_enemy_publish() veroeffentlicht.
   native/en01_damage.c/.h sind damit ABGELOEST; sie bleiben nur so lange
   kompilierfaehig, bis der Integrator native/world.cpp umstellt (siehe
   work/native_port_T2_en01.result.md, Abschnitt Migration).

   Alle Fundstellen sind reine Semantikreferenz. Es wird kein Originalkoerper
   kopiert, generiert, kompiliert, gelinkt oder aufgerufen.
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
   Child-/Link-Works eines Zombies
   ---------------------------------------------------------------------------
   bhEne01_Init (en01.c:2598-2670) legt ueber bhEne01_SetLinkEnemy
   (en01.c:2815, intern bhSetEnemy mit der Vorlage zob_lkmtab, en01.c:1093)
   bis zu elf weitere Works an. Die id steuert die Dispatchroutine, die
   bhEne_SetCallFunc (en01.c:2605 ff.) fuer diese id eintraegt.

     exp1  id 1     Oberkoerper, lkono 1              en01.c:2603
     0x1F  Head     lkono 0                           en01.c:2604/2605
     0x20  Arm      zwei Works, lkono 0               en01.c:2606-2608
     0x21  Leg      zwei Works, lkono 0               en01.c:2609-2611
     0x22  Cap      lkono 11, nur mdlver 5/6/22/35..38 en01.c:2612-2626
     0x23  Worm     lkono 9,  nur mdlver 21/22         en01.c:2627-2639
     0x24  Bom      lkono 9,  nur mdlver 40/41         en01.c:2640-2650
     0x25  Scope    lkono 11, nur mdlver 40/41         en01.c:2653-2661
     0x26  Parent   lkono 0, nur wenn !FastCheck       en01.c:2664-2670

   GEMESSEN an den Originalraumdaten (alle 205 RDT unter data/rooms, Enemy-
   Records 36 Byte ab rom+8, Anzahl rom+0x88): 273 aktive id-1-Works in 72
   Raeumen. Davon erhalten 44 ein Cap (mdlver 5/6/35/36/37/38), 8 ein Worm
   (mdlver 21) und 14 ein Bom+Scope (mdlver 40/41). mdlver 22, 24, 25, 43
   sowie 18/19/20 kommen in keinem Raum vor.
   --------------------------------------------------------------------------- */
#define RECVX_EN01_ID_UPPER   1u    /* exp1, Oberkoerper                     */
#define RECVX_EN01_ID_HEAD    0x1Fu
#define RECVX_EN01_ID_ARM     0x20u
#define RECVX_EN01_ID_LEG     0x21u
#define RECVX_EN01_ID_CAP     0x22u
#define RECVX_EN01_ID_WORM    0x23u
#define RECVX_EN01_ID_BOM     0x24u
#define RECVX_EN01_ID_SCOPE   0x25u
#define RECVX_EN01_ID_PARENT  0x26u

/* Die sieben Child-Zeiger, die bhEne01_Init bei einem ZWEITEN Init-Durchlauf
   ausdruecklich erhaelt (en01.c:2677-2691 sichert ((BH_PWORK**)exp0)[0..6]
   und stellt sie nach dem Nullen des Blocks wieder her). Bom (0x1C) und
   Scope (0x20) sind NICHT in dieser Sicherung enthalten; das ist im Original
   so und bleibt so. Ebenso erhalten bleibt der Schattenslot auf 0x24
   (en01.c:2683/2692). */
#define RECVX_EN01_SAVED_CHILDREN 7u

/* ---------------------------------------------------------------------------
   Erweiterungsblock an exp0 — 0xB0 Byte (en01.c:2599)
   ---------------------------------------------------------------------------
   Benannt sind nur Felder, deren Bedeutung an einer Originalstelle belegt
   ist; die Fundstelle steht jeweils dahinter. Felder ohne geklaerte
   Bedeutung behalten einen neutralen Namen und ihren Offset, damit nichts
   geraten wird. Luecken sind ausdruecklich als Padding benannt, weil das
   Original den Block vollstaendig nullt und als Bytefeld adressiert.
   --------------------------------------------------------------------------- */
typedef struct RECVX_EN01_EXP0 {
    /* 0x00 */ struct RECVX_ENEMY_WORK* arm[2];   /* en01.c:2606-2607        */
    /* 0x08 */ struct RECVX_ENEMY_WORK* leg[2];   /* en01.c:2609-2610        */
    /* 0x10 */ struct RECVX_ENEMY_WORK* head;     /* en01.c:2604             */
    /* 0x14 */ struct RECVX_ENEMY_WORK* cap;      /* en01.c:2624             */
    /* 0x18 */ struct RECVX_ENEMY_WORK* worm;     /* en01.c:2636             */
    /* 0x1C */ struct RECVX_ENEMY_WORK* bom;      /* en01.c:2649             */
    /* 0x20 */ struct RECVX_ENEMY_WORK* scope;    /* en01.c:2660             */

    /* 0x24 Schattenslot aus bhSetShadow; Init setzt -1 (en01.c:2601) und
       uebernimmt den Rueckgabewert (en01.c:2740 bzw. :2759). Ueber einen
       zweiten Init hinweg erhalten (en01.c:2683/2692). */
    /* 0x24 */ int32_t  shadow_slot;

    /* 0x28..0x2F Bytefelder. Belegt: 0x28 Bit 0x40 wird in
       bhEne01_SearchNeck als Sichtbedingung gelesen (en01.c:2973);
       0x2C ist der Schrittzaehler in die rot_tbl-Zeilen von
       bhEne01_SetMtn und laeuft 0..7 (en01.c:10372-10400). Die uebrigen
       Bytes dieses Bereichs werden an Stellen gelesen/geschrieben, die
       dieser Teilport noch nicht bestimmt hat. */
    /* 0x28 */ uint8_t  byte28;
    /* 0x29 */ uint8_t  byte29;
    /* 0x2A */ uint8_t  byte2A;
    /* 0x2B */ uint8_t  byte2B;
    /* 0x2C */ uint8_t  flinch_step;
    /* 0x2D */ int8_t   byte2D;
    /* 0x2E */ int8_t   byte2E;
    /* 0x2F */ uint8_t  byte2F;

    /* 0x30 */ int32_t  word30;
    /* 0x34 */ int32_t  word34;
    /* 0x38 Die-Ausblendung aktiv (en01.c:2918).                            */
    /* 0x38 */ int32_t  fade_active;
    /* 0x3C Materialfarbe. Init setzt 0xFFB2B2B2, wenn die unteren 24 Bit
       leer sind (en01.c:2769-2772); bhEne01_Die addiert je Pass ~0x10100,
       solange die unteren 24 Bit > 0 sind (en01.c:2922-2930). */
    /* 0x3C */ uint32_t mat_color;

    /* 0x40 Hauptflagwort (560 Zugriffe). Belegte Bits:
         0x00000001  InitType02/06/15 setzen es (en01.c:4460/4485/4546)
         0x0000000F  Modusgruppe, von InitType02/06/15 geloescht
         0x00000010  bhEne01_SetMtn kehrt sofort zurueck (en01.c:10338)
         0x00000080  Spielergriff aktiv: bhEne01_DmgCheck schreibt den
                     Spieler-Hurt-Zustand (en01.c:3275-3308)
         0x00000800  Naehemarker; Move/Nage/Damage/Die loeschen es und Move
                     setzt es neu (en01.c:2838/2850/2856/2867/2886/2907)
         0x00001000  SearchNeck-Zweig (en01.c:2963)
         0x00002000  Spiegel von comb_flg Bit 2 (en01.c:3255-3262)
         0x00010000  LinkFireEffect genau einmal (en01.c:2512-2516)
         0x00020000  bhEne01_PlayerControl-Gate (en01.c:2943)
         0x00040000  DmgCheck-Bedingung mode0==1/mode3==0 (en01.c:3245)
         0x00080000  bhEne01_PlayerLink-Gate (en01.c:2495)
         0x00200400  DmgCheck setzt beide Bits (en01.c:3252)
         0x01000000  SetMtn: bhEne_GetTranslateMtn  (en01.c:10407)
         0x02000000  SetMtn: bhEne_GetTranslateMtn2 (en01.c:10413)
         0x08000000  Trefferzucken im Oberkoerper (en01.c:10367)
         0x10000000  shp_ct/Morph-Gate (en01.c:2752, :10450); type 10
                     loescht es jeden Pass (en01.c:10445-10448)
         0x80000000  Dieser Work ist der verknuepfte Oberkoerper; waehlt
                     ueberall die B-Variante (en01.c:2721, :2848, :10345) */
    /* 0x40 */ uint32_t flags;

    /* 0x44 Zweites Flagwort. Belegte Bits:
         0x10  InitType11/15 setzen es (en01.c:4517/4535)
         0x20  Trefferrichtung aus njArcTan2(dvx,dvz) (en01.c:3266-3273)
         0x40  DmgCheck-Bedingung gegen plp->at_flg (en01.c:3209)
         0x80  Nage/Damage/Die loeschen es (en01.c:2868/2887/2908);
               bhEne01_FastWalkCheck liest es an fremden Works
               (en01.c:11492), bhEne01_ChgWalkMtn waehlt damit MTN 125
               statt 117 (en01.c:10279-10286) */
    /* 0x44 */ uint32_t flags2;

    /* 0x48 Zaehler; bhEne01 dekrementiert ihn, solange > 0 (en01.c:2483).
       DmgCheck setzt ihn auf 0 (en01.c:3253); bhEne01_CmnFlgCheck prueft an
       fremden Works == 1 (en01.c:11473). */
    /* 0x48 */ int32_t  counter48;

    /* 0x4C Index in en01_PersonalType[27] und in die add_seno-Zeile von
       bhEne01_SePlay. bhEne01_Init loest ihn ueber ene01_typ_tbl[31][2] aus
       mdlver auf (en01.c:2695-2701). */
    /* 0x4C */ int32_t  personal_type;

    /* 0x50 Halswinkel; SearchNeck fuehrt ihn in 910er-Schritten gegen 0
       zurueck und schreibt ihn auf objP[11].ang[1] (en01.c:2968-3004). */
    /* 0x50 */ int32_t  neck_angle;

    /* 0x54 Abstand Spieler<->Gegner, in bhEne01_Move gesetzt
       (en01.c:2846), in SearchNeck gegen 35.0f geprueft (en01.c:2974). */
    /* 0x54 */ float    player_distance;

    /* 0x58 NJS_POINT3, Ziel der Winkelannaeherung ikou() in SetMtn
       (en01.c:10362, :10423). Das Original adressiert exp0+0x58 als Vektor,
       deshalb erscheint 0x5C nirgends als eigener Macrozugriff. */
    /* 0x58 */ RECVX_ENEMY_VEC ikou_target;

    /* 0x64 NJS_POINT3, lokaler Griffpunkt: bhEne01_PlayerLink rechnet ihn
       in Weltkoordinaten und setzt daraus die Spielerposition
       (en01.c:3044-3047). */
    /* 0x64 */ RECVX_ENEMY_VEC grab_point;

    /* 0x70/0x74 Im Original ATR_WORK*-Zeiger. 0x70 ist die zuletzt von
       bhCollisionCheckLine getroffene Flaeche (en01.c:4222), 0x74 die
       Treppenflaeche aus bhKaidanAtrCheck (en01.c:11839). Nativ steht hier
       der Index aus dem gebundenen Dienst (ENEMY_SERVICES.line bzw.
       .stair_area) — ein Index ist im Gegensatz zum Originalzeiger auch 0
       gueltig, deshalb ist der Nativwert -1 die Uebersetzung des
       Original-NULL und wird in Init an genau den Stellen gesetzt, an
       denen das Original den Block nullt. Verbraucher pruefen >= 0.
       OFFEN (Schnittstellenbedarf, nicht von T2 zu aendern): der Dienst
       stair_area liefert nur den Index. Der MV-Zweig liest jedoch aus dem
       Originalzeiger die Bytes prm0/prm1/prm2 (en01.c:6115-6143); dafuer
       muss stair_area wie `line` zusaetzlich den RECVX_ENEMY_ATR-Satz
       ausgeben. Bis dahin meldet der Verbraucher dort gap(). */
    /* 0x70 */ int32_t  word70;
    /* 0x74 */ int32_t  word74;
    /* 0x78 Trefferstimme nur wenn != -1 (en01.c:3317).                     */
    /* 0x78 */ int32_t  hit_voice;
    /* 0x7C */ int32_t  word7C;

    /* 0x80/0x84 Beinmarker: CheckMtnTbl fuehrt die Fusskorrektur fuer die
       Typen 10..13 nur aus, wenn der jeweilige Wert >= 0 ist
       (en01.c:10628/10641/10654/10667). */
    /* 0x80 */ int32_t  leg_state[2];

    /* 0x88/0x8C == -1 laesst bhEne01_CalcEnemy die Wasserkapsel aus den
       Knoten 9 neu berechnen (en01.c:3112-3122). */
    /* 0x88 */ int32_t  water_state[2];

    /* 0x90 */ int32_t  word90;
    /* 0x94 wird sowohl als float (37x) als auch als int (6x) gelesen; der
       Block ist im Original ein Bytefeld. Beide Sichten bleiben erhalten. */
    /* 0x94 */ union { float f; int32_t i; } slot94;
    /* 0x98 */ int32_t  word98;
    /* 0x9C */ float    float9C;

    /* 0xA0 Laufmotion-Index in en01_walk_mtn[6]; bhEne01_GetWalkMotion
       setzt ihn (en01.c:11437-11441), ChgWalkMtn liest ihn
       (en01.c:10253/10268/10291-10292). */
    /* 0xA0 */ int32_t  walk_index;

    /* 0xA4..0xAF Der Originalblock ist 0xB0 gross; dieser Rest wird von
       keiner Stelle der drei EN01-Quellen adressiert, gehoert aber zur
       Allokation und wird wie dort genullt. */
    /* 0xA4 */ uint8_t  tail[12];
    /* --- Native Erweiterung hinter dem 0xB0-Originalblock -----------------
       0x74 traegt im Original den ATR_WORK-Zeiger der Treppenflaeche
       (en01.c:11839/11846); der native Dienst liefert Index und Satz
       getrennt. Der Satz steht hier, damit bhEne01_MV08 prm0/prm1/prm2
       lesen kann (en01.c:6115-6143). Gueltig, solange word74 >= 0. */
    RECVX_ENEMY_ATR stair_rec;
} RECVX_EN01_EXP0;

/* ---------------------------------------------------------------------------
   Dienste, die EN01 zusaetzlich zu RECVX_ENEMY_SERVICES braucht
   ---------------------------------------------------------------------------
   Jeder Eintrag nennt seinen originalen Aufruf. Ein nicht gebundener Dienst
   ist NULL; der Mechanismus meldet ihn dann namentlich ueber services->gap
   und tut nichts. Kein Eintrag darf durch einen plausiblen Ersatzwert
   belegt werden.
   --------------------------------------------------------------------------- */
/* Modellwahl der Kinder: das Original setzt in bhEne01_SetLinkEnemy
   `epp->mlwP = epw->mdl` (en01.c:2827), das Kind benutzt also unveraendert
   die Modelldeskriptorliste des Elternworks. Das ist kein Slotindex;
   child_model erhaelt dafuer diesen Wert. */
#define RECVX_EN01_CHILD_SHARE_PARENT 0xFFFFFFFFu

typedef struct RECVX_EN01_SERVICES {
    /* bhEne01_SetLinkEnemy (en01.c:2815): erzeugt ueber bhSetEnemy einen
       verknuepften Work mit der Vorlage zob_lkmtab (flg 0x81, id 1) und
       setzt anschliessend id/lkono/lkwkp/mlwP/mnwP. Der BESITZER stellt
       Speicher und WorldEntity; EN01 darf beides nicht anlegen. NULL ->
       der Zombie laeuft ohne Kinder und der Mechanismus meldet die Luecke. */
    struct RECVX_ENEMY_WORK* (*child_create)(struct RECVX_ENEMY_WORK* parent,
                                             uint32_t id, int32_t lkono);    /* bhEne_SetCallFunc (en01.c:2605 ff.): traegt die Dispatchroutine der
       Child-id in bhJumpEnemy ein. Gehoert dem Dispatchbesitzer. */
    void (*register_child_dispatch)(uint32_t id);

    /* Modellwahl der Kinder: epp->mdl[0] = parent mdl[slot],
       epp->skp[0] = parent skp[slot], epp->mlwP = eigenes mdl,
       epp->mnwP = parent mnwP (en01.c:2618-2658). */
    void (*child_model)(struct RECVX_ENEMY_WORK* child,
                        struct RECVX_ENEMY_WORK* parent,
                        uint32_t model_slot, int use_parent_skin);

    /* bhEne01_Init waehlt fuer den normalen Zombie das Morphpaar:
       mlwP->objP = mbp[0], obj_a = mbp[0], obj_b = mbp[1]
       (en01.c:2748-2751). */
    void (*select_morph_pair)(struct RECVX_ENEMY_WORK* w);

    /* npSetAllMatColor(mlwP->objP, mlwP->obj_num, color) — Init-Toenung
       (en01.c:2772/2776) und Die-Ausblendung (en01.c:2927). */
    void (*set_material_color)(struct RECVX_ENEMY_WORK* w, uint32_t color);

    /* Der flg-0x80-Pfad verschiebt den Objektbaum um acht Knoten um
       bhSetMotion herum: objP += 8, owP += 8, obj_num -= 8
       (en01.c:2799-2805 und :10347-10353). `shift` 1 = verschieben,
       0 = zuruecknehmen. */
    void (*shift_object_window)(struct RECVX_ENEMY_WORK* w, int shift);

    /* bhEne_CalcDamage (zonzon1.c:725) mit En01s CombWepTbl/CombJointTbl:
       bildet aus dam[] den total_dam des Passes (en01.c:3206). Die beiden
       Tabellen sind EN01-Daten (en01.c:960/985, Datenpfad
       tools/generate_combat_tables.py) und werden deshalb uebergeben; ein
       ungebundener Zeiger ist NULL und der Aufrufer meldet die Luecke. */
    void (*calc_damage)(struct RECVX_ENEMY_WORK* w,
                        const RECVX_EN01_COMBWEP* wep,
                        const RECVX_EN01_COMBJOINT* joint);

    /* zonzon.c:310 bhKaidanAtrCheck(epw, len, &idx) — Treppenflaechensuche
       vor/hinter dem Work (Probe punkt px - len*njSin(ay), pz - len*njCos(ay)
       auf epw->flr_no). Erweiterung des Basisdienstes stair_area, der nur das
       Praedikat liefert: der Verbraucher braucht ausserdem den ATR-Satz
       (prm0/prm1/prm2, en01.c:3545-3566) und den Index fuer stair_use
       (bhSetUseKaidanFlag, hitchk.c:4965). Rueckgabe 1 = Treffer. */
    int (*stair_area_atr)(struct RECVX_ENEMY_WORK* w, float len,
                          RECVX_ENEMY_ATR* rec, int* index);

    /* weapon.c WpnTab[wpnr_no].flg — Trefferpruefung in bhEne01_DmgCheck
       (0x20 Stich, 0x20000000, en01.c:3211-3212). 1 = Wert geliefert. */
    int (*weapon_flg)(uint32_t wpnr_no, uint32_t* out);

    /* plp->way — Winkel, den bhEne_SetBlood2 als `ry` erhaelt
       (en01.c:3396/3408). Gehoert zur Spielersicht; 1 = Wert geliefert. */
    int (*player_way)(int16_t* out);

    /* plp->watr — die Kollisionskapsel des Spielers. bhEne01_EatCheck
       Modus 2 prueft sie mit npCollisionCheckSC gegen eine Kugel vor dem
       Gegnerkopf (en01.c:11711). Der Basisdienst sphere_capsule rechnet den
       Schnitt; die Kapsel selbst gehoert dem Spielerbesitzer.
       1 = geliefert. */
    int (*player_capsule)(RECVX_ENEMY_CAPSULE* out);

    /* --- Bluthelfer des gemeinsamen Gegnerlayers -------------------------
       Diese fuenf Originalaufrufe liegen ausserhalb von EN01 (zonzon.c,
       effect.c); der Basisdienstvertrag fuehrt bisher nur die reinen
       Effektvarianten (blood_effect4/Burst/Pool). EN01 reicht die Aufrufe
       unveraendert durch — der Anbieter ist derselbe wie dort. */
    /* effect.c:736 bhLinkBlood(epw, lnk_obj, ofs, size): Blut am Gelenk. */
    void (*link_blood)(struct RECVX_ENEMY_WORK* w, int lnk_obj,
                       const RECVX_ENEMY_VEC* ofs, float size);
    /* zonzon.c:415 bhEne_SetBlood(epw, type, bt) — bt ist die
       BT_WORK-Tabelle (en01prt_blood_tbl, en01.c:1123). */
    void (*set_blood)(struct RECVX_ENEMY_WORK* w, uint8_t type,
                      const RECVX_EN01_BT* bt);
    /* zonzon.c:466 bhEne_SetBlood2(epw, type, ofp, ry). */
    void (*set_blood2)(struct RECVX_ENEMY_WORK* w, uint8_t type,
                       const RECVX_ENEMY_VEC* ofp, int16_t ry);
    /* zonzon.c:683 bhEne_SetBlood4(epw, lnk_obj, ofs, tex_id, type). */
    void (*set_blood4)(struct RECVX_ENEMY_WORK* w, int lnk_obj,
                       const RECVX_ENEMY_VEC* ofs, int tex_id, int type);
    /* zonzon.c:803 bhEne_SetNikuhenEffect2(epw, type, pos, n, tex_id). */
    void (*nikuhen_effect2)(struct RECVX_ENEMY_WORK* w, int type,
                            const RECVX_ENEMY_VEC* pos, int n, int tex_id);
    /* zonzon.c:768 bhEne_SetNikuhenEffect(epw, type, pos, v, tex_id) — die
       Einzelvariante mit Richtungsvektor (en01.c:10960/11007/11034). */
    void (*nikuhen_effect)(struct RECVX_ENEMY_WORK* w, int type,
                           const RECVX_ENEMY_VEC* pos,
                           const RECVX_ENEMY_VEC* v, int tex_id);
    /* zonzon.c:634 bhEne_SetBlood3(epw, lnk_obj, ofs, dv, n, tex_id, type,
       wcnt) — die Fontaenenvariante der Halsblutung
       (en01.c:10872/10877/10901/10907/10921/10935). */
    void (*set_blood3)(struct RECVX_ENEMY_WORK* w, int lnk_obj,
                       const RECVX_ENEMY_VEC* ofs, const RECVX_ENEMY_VEC* dv,
                       int n, int tex_id, int type, int wcnt);
    /* effect.c bhSetWaterSplash2(epw, pos, n, sx, sy, sz) — Wasserfontaene
       der Laufmotionen im Wasser (en01.c:10829/10833/10837). */
    void (*water_splash)(struct RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos,
                         int n, float sx, float sy, float sz);

    /* --- Effektvorlage sys->ef + bhSetEffectTb (effect.c:436) ------------
       Mehrere EN01-Effekte fuellen die GEMEINSAME Vorlage sys->ef und rufen
       damit bhSetEffectTb; danach schreiben sie Felder des erhaltenen
       eff[]-Slots (en01.c:10977/11037/11051/11128/11201/11240). Vorlage und
       Effektpool gehoeren dem Effektbesitzer, nicht EN01 — deshalb ein
       Vertrag statt eines eigenen sys->ef.
       `tmpl` sind genau die Vorlagenfelder, die EN01 setzt; `off`, `link`
       und `lkono` sind die drei weiteren Parameter von bhSetEffectTb.
       Rueckgabe = Slotindex oder -1 (voller Pool). */
    int (*set_effect_tb)(const RECVX_EN01_EF* tmpl, const RECVX_ENEMY_VEC* off,
                         struct RECVX_ENEMY_WORK* link, int lkono);
    /* Nachtraegliche Felder eines eff[]-Slots. Genau die Felder, die EN01
       nach bhSetEffectTb schreibt (en01.c:10980-10983/11060-11076/
       11131-11139/11204-11215/11243-11256); `mask` waehlt sie. Ein Slot
       -1 (voller Pool) wird vom Aufrufer abgefangen, so wie das Original
       dort `if (eno != -1)` prueft. */
    void (*effect_slot_set)(int slot, uint32_t mask,
                            const RECVX_EN01_EF_SLOT* in);

    /* mlwP->objP[node].pos[0..2] und .ang[0..2] — die LOKALE Pose eines
       Knotens. bhEne_CalcPartsPos (zonzon.c:286) stapelt genau diese
       Werte ueber die Knotenkette einer en01_tree-Zeile
       (en01.c:4287/4327/4328/4348/4384/4388). node_angle_get liefert nur
       die Winkel; hier wird beides in einem Zugriff gebraucht, weil der
       Mechanismus die Kette selbst aufmultipliziert. 1 = geliefert. */
    int (*node_local)(const struct RECVX_ENEMY_WORK* w, uint32_t node,
                      RECVX_ENEMY_VEC* pos, int32_t* ang3);
    /* mlwP->objP[node].pos[0] = pos[2] = 0 — bhEne01_SetMtn verwirft die
       waagerechte Wurzelverschiebung der Motion, damit
       bhEne_GetTranslateMtn(2) sie als Weltbewegung liefert
       (en01.c:10358/10412/10418). Nur diese beiden Komponenten, y bleibt. */
    void (*node_local_clear_xz)(struct RECVX_ENEMY_WORK* w, uint32_t node);
    /* mlwP->objP[node].pos[axis] = v (en01.c:6311 objP[0].pos[1] = 0). */
    void (*node_local_set)(struct RECVX_ENEMY_WORK* w, uint32_t node, uint32_t axis, float v);

    /* rutchk.c:47 bhCheckRouteID(ps) — Index der Wegezone, in der ein Punkt
       liegt, oder 0xFF (en01.c:4617/4621). Der Basisdienst `route(from,to)`
       beantwortet nur die Verbindungsfrage; Brain00/Brain02 brauchen die
       Zone selbst und den Zwischenpunkt. */
    int (*route_id)(const RECVX_ENEMY_VEC* pos);
    /* rutchk.c:5 bhCheckRoute(ps0, ps1, out) — naechster Wegpunkt von ps0
       nach ps1; Rueckgabe wie im Original die Zonennummer bzw. 0xFF
       (en01.c:4625/4639/4677). `out` wird nur bei gueltiger Rueckgabe oder
       im Gleich-Zonen-Fall geschrieben, genau wie dort. */
    int (*route_next)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to,
                      RECVX_ENEMY_VEC* out);

    /* pwksub.c:664 bhSearchPlayer(pp, r) in der Variante bhSearchPlayer2
       (en01b.c:922): prueft den Spieler in einem Sichtkegel um `pos`/`ay`
       und liefert den Blickwinkel zum Spieler oder -1 (en01.c:4566/4719).
       Das Original setzt dafuer vorruebergehend px/py/pz/ay des Works um;
       nativ bekommt der Dienst Punkt und Winkel direkt, weil beides nur
       Eingabe ist. Ein nicht gebundener Spielerbesitzer -> Rueckgabe -1 ist
       NICHT erlaubt (das waere ein erfundener Spielerzustand); der Dienst
       liefert dann 0 in `found` und der Aufrufer meldet die Luecke. */
    int (*search_player)(const struct RECVX_ENEMY_WORK* w,
                         const RECVX_ENEMY_VEC* pos, int32_t ay, int32_t r,
                         int32_t* out_angle);

    /* zonzon.c:373 bhEne_EnemyAtariCheck(pos, flr_no, id, type) — Boden-
       flaeche type 2 mit prm0 == id und prm1 == type (en01.c:4866).
       Rueckgabe 1 = Flaeche gefunden. */
    int (*enemy_atari)(const RECVX_ENEMY_VEC* pos, int flr_no, uint8_t id,
                       uint8_t type);
    /* Dieselbe Suche, aber mit dem ATR-Satz der Flaeche: bhEne01_GakeotiCheck
       und bhEne01_DansaCheck lesen daraus prm2 (en01.c:11545/11572).
       Rueckgabe 1 = Flaeche gefunden, `rec` gesetzt. */
    int (*enemy_atari_atr)(const RECVX_ENEMY_VEC* pos, int flr_no, uint8_t id,
                           uint8_t type, RECVX_ENEMY_ATR* rec);

    /* hitchk.c:5503 bhCheckFloorEffect(flr_no, px, pz) — Bodeneffektflaeche
       unter dem Work; bhEne01_CollCheck prueft prm0 == 2 (en01.c:4062).
       Rueckgabe 1 = Flaeche gefunden, `rec` erhaelt den ATR-Satz. */
    int (*floor_effect)(int flr_no, float px, float pz, RECVX_ENEMY_ATR* rec);

    /* hitchk.c:5532 bhCheckWater(pos) — Wasserflaeche an einer Position
       (en01.c:4069). Rueckgabe 1 = im Wasser. */
    int (*check_water)(const RECVX_ENEMY_VEC* pos);

    /* hitchk.c:5561 bhCheckL2Water(line, out) — Schnitt einer Strecke mit
       der Wasseroberflaeche (type 3, prm0 1); bhEne01_LinkFireEffect nimmt
       daraus die Hoehe y (en01.c:11257). `from`/`vec` sind NJS_LINE.
       Rueckgabe 1 = Flaeche getroffen, `out` gesetzt. */
    int (*water_line)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* vec,
                      RECVX_ENEMY_VEC* out);

    /* zonzon1.c bhEne_SetSanEffect(epw, jno, bt) — Spritzer am Trefferknoten
       in bhEne01_DamageAdd (en01.c:3814), mit en01prt_blood_tbl. */
    void (*san_effect)(struct RECVX_ENEMY_WORK* w, int jno,
                       const RECVX_EN01_BT* bt);

    /* bhCalcFixOffset: Differenz zwischen der Fusskette der neuen lokalen
       Pose und der des letzten Passes (en01.c:10508 ff.). `tree` ist eine
       Zeile von en01_tree (255-terminiert). */
    void (*calc_fix_offset)(struct RECVX_ENEMY_WORK* w, const int8_t* tree,
                            const RECVX_ENEMY_VEC* local, RECVX_ENEMY_VEC* out);

    /* bhGetFrameNum: rechnet frm_no beim Motionwechsel auf die Laenge des
       Zielclips um (en01.c:10297). */
    int32_t (*frame_num)(uint32_t from_frames, uint32_t to_frames, int32_t frm_no);

    /* bhEne_GetTranslateMtn / bhEne_GetTranslateMtn2 (en01.c:10411/10417),
       `variant` 1 bzw. 2. */
    void (*translate_motion)(struct RECVX_ENEMY_WORK* w, uint32_t variant, int32_t frm);
    /* bhAddSpeed(epw, 0) (en01.c:10364/10426).                            */
    void (*add_speed)(struct RECVX_ENEMY_WORK* w, int mode);
    /* ikou(epw, &ziel, 256) (en01.c:10362/10423) ist reine Work-Feld-Logik
       (flg 0x80, px/pz/ay; zonzon.c:10-42) und liegt deshalb im Mechanismus
       selbst (native/en01_core.c) statt hinter einem Dienst. Ebenso ikou3
       (zonzon.c:45).                                                      */

    /* Schreibzugriff auf den vorhandenen nativen Spielerbesitzer. Das
       Original schreibt in bhEne01_DmgCheck plp->mode0..3, plp->flg
       (0x10000 loeschen, 8 setzen, 4 je nach bhDGCdirCheck), stflg
       &= ~0x50000, at_flg = 0, mnwP = mnwPb, *(int*)plp->exp1 |= 4 und
       sys->pad_on &= ~0xF (en01.c:3275-3308). `knocked_down` ist
       (plp->flg2 & 0x200) != 0, also die Weiche des Originals zwischen dem
       Umwerf-Zweig (mode0 2/mode2 1/mode1 aus bhDGCdirCheck(plp->dvx,
       plp->ay)) und `*(int*)&plp->mode0 = 1`; alles davon fuehrt der
       Besitzer aus, weil dvx/dvz, mnwP/mnwPb und pad_on nur dort liegen.
       NULL -> der Mechanismus meldet die Luecke und erfindet keinen
       Spielerzustand. */
    void (*player_hurt_state)(struct RECVX_ENEMY_WORK* w, int knocked_down);
    /* bhEne01_PlayerLink (en01.c:3012-3098), Spielerteil.
       `world_point` ist pd aus epw->exp0+0x64, `ay` = (uint16_t)(epw->ay +
       epw->waxp). Der Besitzer fuehrt dann genau en01.c:3045-3095 aus:
         ply->px = world_point->x; ply->pz = world_point->z; ply->ay = ay;
         opos = njCalcPoint(ply->mtx, {0,0,0})            (:3051, ALTE mtx)
         if (local_offset != NULL)   ply->flg & 0x1000 war gesetzt:
             pos = njCalcPoint(Translate(ply->p)+RotateXYZ(ply->a),
                               {local_offset->x, 0, local_offset->z})
             ply->mtx neu = Translate(pos) + RotateXYZ(ax, ayp, az)  (:3079)
         else
             pos = njCalcPoint(Translate(ply->p)+RotateXYZ(ply->a), {0,0,0})
         bhEne_CollisionCheckWall2(ply, &pos, &opos, out_delta,
                                   ply->ar, ply->ah)       (:3093)
       `local_offset` kommt aus EN01s eigener Tabelle ply_pos[16]
       (en01_tables.h) — die zwoelf XZ-Felder sind EN01-Daten und bleiben
       deshalb bei EN01; der Besitzer waehlt KEINE Tabelle und baut keine
       eigene Auswahl. ply->ayp, ply->mtx, ply->ar/ah liegen nur beim
       Besitzer, darum bleibt der Matrix-/Wandteil hier.
       Rueckgabe 0 = nicht ausfuehrbar (kein gebundener Spielerbesitzer). */
    int  (*player_link_pose)(struct RECVX_ENEMY_WORK* w,
                             const RECVX_ENEMY_VEC* world_point, int32_t ay,
                             const RECVX_EN01_XZ* local_offset,
                             RECVX_ENEMY_VEC* out_delta);

    /* Knotenwinkel des eigenen Modellbaums, mlwP->objP[node].ang[axis]
       (en01.c:2966/2968/2976-2979/3004, en01b.c:849-870). Das Original
       schreibt und liest dieselbe Angabe; der Modellbaum gehoert dem
       Besitzer. axis 0/1/2 = x/y/z. NULL -> der Aufrufer meldet die Luecke
       ueber gap("mlwP->objP[n].ang") und behaelt den Wert im exp0-Block. */
    int32_t (*node_angle_get)(const struct RECVX_ENEMY_WORK* w, uint32_t node,
                               uint32_t axis);
    void    (*node_angle_set)(struct RECVX_ENEMY_WORK* w, uint32_t node,
                               uint32_t axis, int32_t ang);
    /* bhEne01_SePlay ruft ChechPlayEnemySe vor RequestEnemySe, aber nur
       fuer die beiden Stimm-Nummern 20/21 (en01.c:11387-11393). Die
       Basisdienste dafuer stehen schon in RECVX_ENEMY_SERVICES. */

    /* --- Kindfamilie en01sub.c (native/enemies/en01_sub.c) --------------
       Alle folgenden Eintraege sind neu fuer die Kindfamilie. NULL -> der
       Mechanismus meldet die Originalstelle ueber services->gap und bricht
       den betroffenen Zweig ab; es wird nichts ersetzt. */

    /* hitchk.c:1939 bhCheckWallType(pos, flg, ar, ah) mit dem ATR-Satz der
       getroffenen Wandflaeche. Der Basisdienst wall_type ist nur ein
       Praedikat; bhEne01Leg_DG00 liest hp->type (en01sub.c:751-765).
       Rueckgabe 1 = Treffer, `rec` (falls != NULL) gesetzt. */
    int (*wall_type_atr)(const RECVX_ENEMY_VEC* pos, uint32_t flg, float ar,
                         float ah, RECVX_ENEMY_ATR* rec);
    /* hitchk.c:2178 bhCheckWallRefAngle(pos, flg, ar, ah, &ay): Wandabprall
       mit Reflexionswinkel. Schreibt wie das Original `pos` (Eingabe und
       Ausgabe) und `*ay`. Aufrufe en01sub.c:937 (Kopf, &epw->ay),
       :1118 (Kappe, lokales ay). Rueckgabe 1 = Treffer (Original:
       ATR_WORK* != NULL), `rec` (falls != NULL) erhaelt den ATR-Satz. */
    int (*wall_ref_angle)(RECVX_ENEMY_VEC* pos, uint32_t flg, float ar, float ah,
                          int32_t* ay, RECVX_ENEMY_ATR* rec);
    /* owk->mtx eines Modellknotens SCHREIBEN: `w->mlwP->owP[joint].mtx =
       in16`. Lesen geht ueber den Basisdienst joint_matrix. Die Kinder
       Arm/Bein/Kopf/Kappe/Bombe aendern die Gelenkmatrix direkt
       (en01sub.c:295-307, :357-372, :918-922, :1092-1145, :1252). Der
       Besitzer darf die Matrix im selben Pass nicht neu aus objP rechnen,
       wenn owP[joint].flg Bit 0x1 steht (MdlPut.c:296 bhCalcTree). */
    int (*joint_matrix_set)(struct RECVX_ENEMY_WORK* w, uint32_t joint,
                            const float* in16);
    /* owP[joint].flg |= bits — 0x100 (en01sub.c:387/628/968) bzw. 3
       (bhEne01_FlyingCap en01.c:11910, dort noch nicht genutzt). */
    void (*joint_flags_or)(struct RECVX_ENEMY_WORK* w, uint32_t joint,
                           uint32_t bits);
    /* epw->mtx: die WURZELmatrix des Works nach bhCalcModel (MdlPut.c:240-
       262; bei flg 0x80 lkwkp->owP[lkono] * T(lo) * R(a)). bhEne01Scope_Effect
       liest daraus Lage und Winkel (en01sub.c:1979-1987). 1 = geliefert. */
    int (*work_matrix)(const struct RECVX_ENEMY_WORK* w, float* out16);
    /* epw->lkwkp = (unsigned char*)plp (en01sub.c:1417): der Wurm haengt
       sich an den SPIELERwork. `link` 1 = anhaengen; EN01 setzt danach
       w->lkwkp = NULL, weil der Spieler kein Gegnerwork ist, und der
       Besitzer fuehrt bei flg 0x80 den Spielerbezug (plp->mlwP->owP[lkono],
       lkono 0 bzw. 3). `link` 0 = Spielerbezug aufheben; EN01 ruft das
       genau dort, wo das Original lkwkp wieder auf epw->exp1 setzt
       (en01sub.c:1322/:1723), und schreibt danach w->lkwkp selbst.
       Rueckgabe 0 = nicht ausfuehrbar. */
    int (*link_player)(struct RECVX_ENEMY_WORK* w, int link);
    /* plp->mtx — die Wurzelmatrix des Spielers (nicht owP[]). Der Wurm
       setzt sich und den Spieler relativ dazu (en01sub.c:1466/1527/1594).
       1 = geliefert. */
    int (*player_matrix)(float* out16);
    /* PEXP0_F(off) = value — Schreibzugriff auf den exp0-Block des Spielers
       (macros.h:66). en01sub.c:1529/1530 schreibt die Offsets 72 und 80
       (plp->px/pz nach dem Abwurf). */
    void (*player_exp0_float_set)(uint32_t offset, float value);
    /* plp->mlwP->objP[node].pos[0] = pos[2] = 0 (en01sub.c:1532-1534). */
    void (*player_node_clear_xz)(uint32_t node);
    /* sys->pad_on &= ~bits (en01sub.c:1536/1699/1744). Dieselbe Original-
       zeile steht auch in en01.c:7643/7755/7767/7928/8956 (dort bisher als
       Luecke gemeldet). */
    void (*pad_on_clear)(uint32_t bits);
} RECVX_EN01_SERVICES;

/* ---------------------------------------------------------------------------
   exp0-Bloecke der Kindfamilie (en01sub.c)
   ---------------------------------------------------------------------------
   Die Kinder legen eigene Bloecke an: Kopf bhEne_CallocWork(32, 8)
   (en01sub.c:809), Arm/Bein/Kappe/Wurm bhEne_CallocWork(112, 8)
   (:227/:671/:1009/:1302). Bombe, Zielfernrohr und Elternwork haben keinen.

   Belegte Offsets (alle Zugriffe der Datei en01sub.c und en01b.c:328-329):
     0x00 NJS_CNK_OBJECT* — Knoten eines Modellbaums (EXP0_I(0))
     0x04 O_WORK*         — Gelenkwork desselben Baums (EXP0_I(4))
     0x08 int  Kettenlaenge (EXP0_I(8)); Arm/Bein 2, Kopf/Kappe 0
     0x0C int  Blutgelenk des Elternworks (EXP0_I(12), :288/:458/:739)
     0x14 int  von Init genullt (EXP0_I(20)), sonst nie gelesen
     0x30 NJS_MATRIX (exp0[48], :322/:727/:1183) — nur im 112-Byte-Block
   EXP0_F(96)/(100)/(104) liegen INNERHALB dieser Matrix: es sind deren
   Elemente 12/13/14 (Translation). Das Original ueberschreibt mit
   EXP0_F(100) = grand also mtx[13] (en01sub.c:326, :337/343) — dieselbe
   Speicherstelle, deshalb nativ ausdruecklich als mtx[12..14] gefuehrt.

   Die beiden Zeiger des Originals zeigen in den objP[]/owP[]-Baum eines
   Modells. Nativ ist der Baum Besitz des Hosts; ein Zeiger wird deshalb
   als (Work, Knotenindex) gefuehrt: `work` ist der Work, dessen mlwP
   gemeint ist (en01b.c:328: epp->mlwP->objP[11] -> {epp, 11}). work == NULL
   ist die Uebersetzung des Original-NULL. Zeigerarithmetik owk[+-1] bzw.
   obj++ ist Knotenindex +-1 im selben Baum. Das ist eine reine
   Datenrepraesentations-Anpassung; die Offsetasserts in en01_sub.c belegen
   die Reihenfolge (jedes Feld hinter den zwei Referenzen um deren
   Groessenzuwachs versetzt). */
typedef struct RECVX_EN01_NODE_REF {
    struct RECVX_ENEMY_WORK* work;
    int32_t                  node;
} RECVX_EN01_NODE_REF;

typedef struct RECVX_EN01_SUB_EXP0 {
    /* 0x00 */ RECVX_EN01_NODE_REF obj;      /* NJS_CNK_OBJECT*           */
    /* 0x04 */ RECVX_EN01_NODE_REF owk;      /* O_WORK*                   */
    /* 0x08 */ int32_t  node_num;            /* EXP0_I(8)                 */
    /* 0x0C */ int32_t  djnt_no;             /* EXP0_I(12)                */
    /* 0x10 */ int32_t  word10;              /* nie adressiert            */
    /* 0x14 */ int32_t  word14;              /* EXP0_I(20)                */
    /* 0x18 */ uint8_t  pad18[8];            /* bis 0x20 (Kopfblock-Ende) */
} RECVX_EN01_SUB_EXP0;

typedef struct RECVX_EN01_SUB_EXP0_MTX {
    /* 0x00 */ RECVX_EN01_SUB_EXP0 base;
    /* 0x20 */ uint8_t  pad20[16];
    /* 0x30 */ float    mtx[16];             /* exp0[48]; [12..14] = EXP0_F(96/100/104) */
} RECVX_EN01_SUB_EXP0_MTX;

/* ---------------------------------------------------------------------------
   Laufzeitkontext eines Raums
   ---------------------------------------------------------------------------
   bhEne01_GetWalkMotion, _FastCheck, _CmnFlgCheck und _FastWalkCheck laufen
   die globale Gegnertabelle `ene` bis sys->ewk_n ab (en01.c:11420/11451/
   11469/11488). Dafuer reicht T1s RECVX_ENEMY_TABLE; EN01 haelt keine
   eigene Liste.
   --------------------------------------------------------------------------- */
typedef struct RECVX_EN01_CONTEXT {
    RECVX_ENEMY_TABLE*             table;     /* ene / sys->ewk_n / sys->enow */
    const RECVX_ENEMY_SERVICES*    services;  /* T1 Stufe A                   */
    const RECVX_EN01_SERVICES*     en01;      /* Erweiterung dieser Datei     */
    /* sys->rmthp: Raum-MTN-Liste. bhEne01_CheckMtnTbl laeuft seine drei
       Tabellen NUR, wenn die Motionquelle des Works nicht die Raumliste ist
       (en01.c:10496). Entspricht mtn_room == 0 im gemeinsamen Work. */
    uint32_t                       game_mode; /* sys->gm_mode (en01.c:2703)   */
    /* sys->stg_no / sys->rom_no des laufenden Raums. Nur bhEne01Parent_
       NoKaidanCheck liest sie (en01sub.c:177). stg_no 0 ist ein gueltiger
       Wert und zugleich der Tabelleneintrag ene_stg_tbl[0]; deshalb gilt der
       Satz nur bei stage_valid != 0, sonst meldet der Mechanismus die Luecke
       statt mit einem Nullwert zu vergleichen. */
    int32_t                        stg_no;
    int32_t                        rom_no;
    uint32_t                       stage_valid;
} RECVX_EN01_CONTEXT;

void recvx_en01_set_context(const RECVX_EN01_CONTEXT* ctx);

/* --- Variantenaufloesung ---------------------------------------------------
   ene01_typ_tbl[31][2] (en01.c:2541) bildet die Raum-mdlver auf den internen
   en01-Subtyp ab; bhEne01_Init nimmt den ERSTEN passenden Eintrag und laesst
   bei keinem Treffer die 0 stehen (en01.c:2695-2701). Nur die ersten 30 von
   31 Zeilen werden durchsucht — die Schleife laeuft i < 30, Zeile 30
   (mdlver 43) ist damit unerreichbar. Das ist im Original so. */
int32_t recvx_en01_personal_type(uint8_t mdlver);

/* --- bhEne_ChgMtn (zonzon.c:1154) -----------------------------------------
   Reine Work-Feld-Logik, deshalb Teil des Mechanismus und kein Dienst:
   setzt mtn_add immer auf 0x10000 und bei tatsaechlichem Motionwechsel
   mtn_no/frm_no/hokan_count, hokan_rate 0, mtn_md 0x20 und loescht flg
   0x40000 und 0x2000000. Rueckgabe 0 bei Wechsel, -1 wenn die Motion
   bereits lief. */
int recvx_enemy_chg_mtn(RECVX_ENEMY_WORK* w, uint32_t no, int32_t frm, int32_t rate);

/* --- Einstiegspunkte ------------------------------------------------------
   bhEne01 (en01.c:2467) ist der Eintrag, den bhJumpEnemy[1] je Pass ruft.
   `exp0` ist der Erweiterungsblock dieses Works; der Besitzer haelt ihn in
   derselben WorldEntity wie den Work selbst. */
void recvx_en01_init_work(RECVX_ENEMY_WORK* w, RECVX_EN01_EXP0* exp0);
void recvx_en01_control(RECVX_ENEMY_WORK* w);

/* bhEne01_MainLoop (en01.c:2504) und bhEne01_SetMtn (en01.c:10322) einzeln,
   damit die Reihenfolge im Pass pruefbar ist. */
void recvx_en01_main_loop(RECVX_ENEMY_WORK* w);
int  recvx_en01_set_mtn(RECVX_ENEMY_WORK* w);
void recvx_en01_check_mtn_tbl(RECVX_ENEMY_WORK* w, int32_t frm);
void recvx_en01_search_neck(RECVX_ENEMY_WORK* w);
void recvx_en01_dmg_check(RECVX_ENEMY_WORK* w);
void recvx_en01_chg_walk_mtn(RECVX_ENEMY_WORK* w);
void recvx_en01_get_walk_motion(RECVX_ENEMY_WORK* w);
void recvx_en01_se_play(RECVX_ENEMY_WORK* w, int32_t no);

/* Die vier Nachbarschaftsabfragen ueber die Gegnertabelle. */
int recvx_en01_fast_check(const RECVX_ENEMY_WORK* w);
int recvx_en01_cmn_flg_check(const RECVX_ENEMY_WORK* w);
int recvx_en01_fast_walk_check(const RECVX_ENEMY_WORK* w);

/* Zugriff auf den Erweiterungsblock eines Works (exp0), typisiert. */
RECVX_EN01_EXP0* recvx_en01_exp0(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
