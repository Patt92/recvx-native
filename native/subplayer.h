#ifndef RECVX_NATIVE_SUBPLAYER_H
#define RECVX_NATIVE_SUBPLAYER_H
#include <stdint.h>

/* ===========================================================================
   T5 — Begleitfiguren-/Szenenfiguren-Pfad (Subplayer)
   ---------------------------------------------------------------------------
   Eigenstaendige native Bestimmung des Mechanismus, den die Originaldatei
   `subpl.c` beschreibt. Kein Originalkoerper, kein Generator, kein Include
   aus der Quarantaene, keine zweite sys/plp/rom/ene-Welt. Der Zustand liegt
   weiter im einzigen vorhandenen Besitzer (native/world.h WorldEntity haelt
   `RECVX_SUBPLAYER` als Member); diese Dateien fuehren nur den Apparat aus.

   Semantikreferenz (gelesen und mit Datei:Zeile belegt, nicht uebernommen):
     eneset.c:84-152   Identitaeten 31..52, 57..70, 72..99 laufen auf den
                       Subplayer-Apparat; 53..56, 71 und 0..30 nicht.
     eneset.c:372      nur Identitaeten > 90 bekommen vorher bhControlMask.
     subpl.c:12        Subpl_tbl[7]       mode0-Dispatch, Index 2/3/4 = NULL
     subpl.c:22        subpl_init_tbl[120] (Index id-40), alle Eintraege gleich
     subpl.c:37        subpl_mv_tbl[1]    mode2-Dispatch
     subpl.c:38        subpl_mv00_tbl[15] ct2-Dispatch fuer mode0 1
     subpl.c:56        subpl_mv01_tbl[15] ct2-Dispatch fuer mode0 5/6
     subpl.c:76        Rahmen eines Passes (flg-Bits, Dispatch, bhCalcModel)
     subpl.c:87        Init-Einstieg, :95 gemeinsame Initialisierung
     subpl.c:131/137/143/149  mode0-Zweige und die beiden ct2-Weichen
     subpl.c:155/177/198/226/245/264/287/310/332/357  die elf Bewegungszweige
     subpl.c:376       Zielwinkelbegrenzung, auch von pl_evt.c:577/624 benutzt
     subpl.c:393       mv00_subpl10
     subpl.c:410       die leere Routine der Identitaet 28
     Motion.c:21       bhSetMotion (Frameabtastung/Fortschritt) — Fremddienst
     MdlPut.c:240      bhCalcModel — Fremddienst des Objektbaum-Besitzers
     pwksub.c:133      bhAddSpeed (hier nativ enthalten, da rein lokal)
     pwksub.c:445      Zielbarkeitsfilter liest flg 0x8000, das der Pass setzt
     types.h:238       BH_PWORK-Feldbreiten (mode0..3 sind je ein Byte)

   Messung aus den 205 Original-Raumdateien (data/archives/RDX_LNK, Header
   u32(b,16), Anzahl bei +0x88, Satzzeiger bei +8, 36-Byte-Saetze, flg Bit 0):
   759 aktive Figurensaetze, davon 195 auf 45 verschiedenen Subplayer-
   Identitaeten. Haeufigste: 91 (38), 92 (22), 93 (17), 67 (14), 43 und 97
   (je 8). Die kleinste vorkommende Identitaet ist 42, die groesste 99 — der
   Tabellenindex id-40 bleibt damit in allen Raeumen innerhalb der 120
   Eintraege (siehe `recvx_subplayer_init_index` fuer den Rest).
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

struct RECVX_ENEMY_WORK;
struct RECVX_SUBPLAYER;

/* Rueckgabe eines Passes: welcher Dispatchzweig tatsaechlich belegt war. */
enum {
    RECVX_SUBPL_UNHANDLED = 0,  /* Originalzweig ist NULL/ausserhalb -> Luecke */
    RECVX_SUBPL_HANDLED   = 1
};

/* Der Satz BH_PWORK-Felder, den der Subplayer-Apparat liest und schreibt.
   Feldbreiten folgen types.h:238; `frm_no` ist die 16.16-Zahl des Hosts in
   64 Bit, weil der vorhandene Motionbesitzer sie so fuehrt.

   WICHTIG: Dies ist KEIN zweiter Work. Die Struktur liegt als Member in der
   vorhandenen WorldEntity (native/world.h:29); Lage/Winkel/Frame werden vom
   Besitzer vor dem Pass hineingeschrieben und danach zurueckgelesen. */
typedef struct RECVX_SUBPLAYER {
    /* --- Identitaet ----------------------------------------------------- */
    unsigned id;

    /* --- Zustand (BH_PWORK) --------------------------------------------- */
    unsigned flg, mtn_no, mtn_md, frm_mode, hokan_count;
    int mode0, mode1, mode2, mode3;
    int ct0, ct1, ct2, ct3;
    int ay, mtn_add, hokan_rate;
    float px, pz, spd;
    int64_t frm_no;
    void* mtn_tp;

    /* --- Rueckmeldung an den Besitzer ----------------------------------- */
    /* 1, wenn dieser Pass bhSetMotion aufgerufen hat (Motion.c:21). Der
       vorhandene Motionbesitzer schaltet den Frame nur dann weiter. */
    int advanced;
    /* Rueckgabe des letzten bhSetMotion dieses Passes: -1 rueckwaerts
       umgelaufen, +1 vorwaerts umgelaufen, 0 kein Umlauf/keine Motiondaten. */
    int motion_wrapped;
    /* Name der Originalstelle, deren Dienst in diesem Pass nicht gebunden
       war (NULL = keiner). Keine Ersatzwerte; der Besitzer meldet sie. */
    const char* gap;

    /* --- Fremddienste, die der Integrator binden MUSS ------------------- */
    /* Motion.c:21 bhSetMotion(work, add, mode, datP). Erwartet wird die
       originale Semantik: Abtastung VOR dem Fortschritt, danach frm_no um
       `add` weiter, Rueckgabe -1/0/+1. NULL -> `gap` wird gesetzt und der
       Pass meldet nur `advanced`. */
    int (*set_motion)(struct RECVX_SUBPLAYER* work, int add, unsigned mode, void* datP);
    /* MdlPut.c:240 bhCalcModel(work) — Abschluss jedes Passes (subpl.c:83).
       NULL -> `gap`. Im vorhandenen Host uebernimmt das die Rueckschreibung
       in die WorldEntity; siehe Bericht, Abschnitt (b). */
    void (*calc_model)(struct RECVX_SUBPLAYER* work);
} RECVX_SUBPLAYER;

/* eneset.c:84-152 — 1, wenn diese Identitaet auf den Subplayer-Apparat
   geleitet wird. Deckungsgleich mit recvx_enemy_is_subplayer_id
   (native/enemy_dispatch.h), hier ohne Abhaengigkeit auf diesen Header. */
int recvx_is_subplayer(unsigned identity);

/* subpl.c:22 — der Index in subpl_init_tbl[120]. Rueckgabe -1, wenn
   `identity - 40` ausserhalb der 120 Originaleintraege liegt; das Original
   liest dort ueber das Tabellenende hinaus (Identitaeten 31..39 und > 159).
   Kein Raum der 205 Originaldateien erreicht diesen Fall. */
int recvx_subplayer_init_index(unsigned identity);

/* subpl.c:87 Init-Einstieg: setzt mode0 auf 1 und fuehrt die gemeinsame
   Initialisierung (subpl.c:95) aus. Entspricht Subpl_tbl[0]. */
void recvx_subplayer_init(RECVX_SUBPLAYER* work);

/* subpl.c:76 — ein vollstaendiger Pass: Statusbits, mode0-Dispatch,
   Abschluss. Rueckgabe RECVX_SUBPL_HANDLED, wenn der gewaehlte Zweig im
   Original belegt ist; RECVX_SUBPL_UNHANDLED fuer mode0 2/3/4 (dort steht
   NULL in der Tabelle) sowie fuer mode0 > 6 und ct2 >= 15. */
int recvx_subplayer_tick(RECVX_SUBPLAYER* work);

/* subpl.c:376 — Zielwinkelbegrenzung. Getrennt nutzbar, weil pl_evt.c:577
   und :624 sie mit dem Spielerwork und Grenze 910 aufrufen. */
int recvx_subplayer_dir_target(float px, float pz, int ay, float x, float z, int limit);

/* ---------------------------------------------------------------------------
   Anschluss fuer die uebrigen Gegner (RECVX_ENEMY_WORK, native/enemy_work.h)
   ---------------------------------------------------------------------------
   subpl.c:143 und :150 sind die beiden ct2-Weichen, die fast jede
   Gegnertabelle als mode0-Zweig 5 bzw. ueber mode2 benutzt (en01.c:2214,
   en03.c:415, en04.c:421, en07.c:73, en05.c:828, ... insgesamt 34 Dateien).
   `recvx_subplayer_enemy_event` ist der Dienst, den native/enemy_work.h als
   `sv->event` fuehrt. */
void recvx_subplayer_enemy_event(struct RECVX_ENEMY_WORK* work);   /* subpl.c:143 */
void recvx_subplayer_enemy_event2(struct RECVX_ENEMY_WORK* work);  /* subpl.c:149 */
/* subpl.c:76 als Gegnerroutine fuer die Identitaeten 31..52/57..70/72..99. */
void recvx_subplayer_enemy(struct RECVX_ENEMY_WORK* work);
/* subpl.c:410 — die Originalroutine der Identitaet 28 ist leer. Sie bleibt
   leer; das ist kein Versehen und wird nicht ergaenzt. */
void recvx_subplayer_enemy28(struct RECVX_ENEMY_WORK* work);

#ifdef __cplusplus
}
#endif
#endif
