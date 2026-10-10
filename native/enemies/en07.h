#ifndef RECVX_NATIVE_EN07_H
#define RECVX_NATIVE_EN07_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 7 — Fledermaus. Nativ eigenstaendig bestimmt aus der
   Semantikreferenz en07.c (bhEne07 und die 43 zugehoerigen Routinen),
   zonzon1.c (bhEne_DirTarget :433, bhEne_GetPartsPos :465,
   bhEne_DGDirCheck :515, bhEne_SetWeponAtr :591, bhArcTan2 :864) und
   pwksub.c:133 (bhAddSpeed). Kein Originalkoerper, kein Generator, keine
   zweite Workwelt. */

/* exp0 der Fledermaus — im Original ein 0x44 Byte grosser Rohblock aus
   bhEne_CallocWork mit festen Offsets. Nativ benannt; Inhalt, Nullstart und
   Lebensdauer bleiben gleich. Die Offsetkommentare belegen die Zuordnung.
   Belegte Besonderheit: EXP0_ATR(7) liegt laut macros.h:78 auf
   exp0 + 7*4 + 4 = 0x20 und ist damit DASSELBE Wort wie EXP0_I(0x20).
   bhEne07_FloorCollision schreibt dort die getroffene Bodenflaeche, und
   bhEne07_ObjEdge liest sie; EXP0_I(0x24) haelt die zuletzt behandelte.
   Nativ wird daraus eine stabile Flaechenidentitaet plus ihr Datensatz,
   weil die nativen Kollisionsdienste keine Originalzeiger herausgeben. */
typedef struct {
    float    dist;            /* 0x00  waagerechter Abstand zum Spieler    */
    /* 0x04..0x13 beruehrt en07 nirgends: die Datei nutzt weder EXP0_ATR(0..3)
       noch EXP0_P(0) (macros.h:78/80), die dort lagen. Der Platz bleibt
       erhalten, damit die folgenden Felder ihre Originallage behalten. */
    uint8_t  reserved_04[0x10];
    RECVX_ENEMY_VEC hang;     /* 0x14  Zielpunkt des Deckenandockens       */
    uint32_t floor_atr;       /* 0x20  EXP0_ATR(7): aktuelle Bodenflaeche  */
    uint32_t floor_atr_done;  /* 0x24  zuletzt behandelte Bodenflaeche     */
    float    vx, vy, vz;      /* 0x28/0x2C/0x30  Eigengeschwindigkeit      */
    uint8_t  sight;           /* 0x34  freie Sichtlinie zum Spieler        */
    uint8_t  atk;             /* 0x35  Angriffsplatz 0..4                  */
    int32_t  hang_wait;       /* 0x38  Sperre bis zum naechsten Andocken   */
    float    ar_target;       /* 0x3C  Zielwert fuer ar                    */
    float    car_target;      /* 0x40  Zielwert fuer car                   */
    /* Nativ zusaetzlich, AUSSERHALB der Originaloffsets (die Struktur endet
       im Original bei 0x44): der Datensatz zur Flaeche floor_atr. Noetig,
       weil die nativen Kollisionsdienste keine Originalzeiger herausgeben;
       alle Felder bis einschliesslich car_target behalten ihre Lage. */
    RECVX_ENEMY_ATR floor_rec;
    int      floor_rec_valid;
} RECVX_EN07_EXT;

/* exp1 — EIN gemeinsamer 8-Byte-Block FUER DEN GANZEN SCHWARM. Die erste
   initialisierte Fledermaus legt ihn an, jede weitere uebernimmt denselben
   Zeiger (en07.c:188-202). Er ist damit der einzige Ort, an dem sich die
   Tiere ueber den Spielergriff verstaendigen. */
typedef struct {
    uint8_t chain;    /* [0] Angriffskette 0..4; 0x63 = Rueckenangriff     */
    uint8_t spare[4]; /* [1..4] im Init genullt                            */
    uint8_t grab;     /* [5] Griff laeuft                                  */
    uint8_t holder;   /* [6] Index der Fledermaus, die den Spieler haelt   */
    uint8_t cooldown; /* [7] Sperre nach einem Griff                       */
} RECVX_EN07_SWARM;

void recvx_en07(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
