/* Kopfsteuerung des Spielers (Auftrag B06).
 *
 * Semantikreferenz (nur gelesen, kein Funktionskoerper uebernommen):
 *   bhControlPlayerHead   player.c:6746-7006   Kopfzustand, Winkelkuerzung, Blinken
 *   bhLookNearEnemy       player.c:7009-7050   Blickziel suchen und halten
 *   bhSetHeadRotation     player.c:7053-7204   Winkel zum Blickziel
 *   bhSearchNearEnemy2    pwksub.c:395-503     Zielsuche mit Sichtpruefung
 *   EXP1_*-Makros         macros.h:50-54       Bytezugriffe auf plp->exp1
 *   Kopfflag-Schreiber    player.c:1800, 2112..2752, 2891..4337, 6234..6679,
 *                         pl_evt.c:75/242/321/401, playpch.c:46/73,
 *                         en01.c:7541/7613/7657, en04.c:3508,
 *                         en07.c:2196..2251, en26.c:2749..2871
 *   bhSetPlayer-Vorgabe   player.c:787-789     exp1[0]=1, exp1[34]=-1
 *
 * Der Zustand ist eine Sicht auf den einen vorhandenen Spieler-/Actor-Besitzer:
 * die Laufzeit uebergibt je Bild die lokalen Knotenwinkel 0..5 aus dem
 * Posebesitzer und die Weltmatrix des Kopfknotens; sie schreibt die
 * veraenderten Knotenwinkel 4 und 5 dorthin zurueck.  Es wird keine zweite
 * Spielerwelt gefuehrt.
 *
 * plp->exp1 ist im Original ein typloser Puffer mit festen Byteoffsets.  Die
 * hier benannten Felder sind dieselben Groessen an denselben Offsets; der
 * Kommentar nennt jeweils das Original.
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Kopfflags, EXP1_I(0).  Alle im Original belegten Bits. */
#define RECVX_HEAD_BLINK_ENABLE  0x001u /* Augentextur ueberhaupt tauschen */
#define RECVX_HEAD_BLINK_RUN     0x002u /* Blinkfolge laeuft */
/* 0x4 setzen: die sieben Laufaktionen in ihrem mode3-0-Block (player.c:2112
 * std, 2176 srt, 2237 sta, 2312 wlk, 2438 run, 2575 bak, 2727 bk2), der
 * Laufwende-Uebergang in bak/bk2 (2617/2752, mode2=6 mit mode3=1), die vier
 * Skriptaktionen in pl_evt.c (75/242/321/401) sowie en01.c:7613 und
 * en07.c:2209/2251.  0x4 loeschen: die sechs Treppen-/Stufenroutinen
 * (2891/3153/3360/3522/3948/4143), psh 3741, cro 3897, rpsh 4336, der
 * Angriffseinstieg in bhCPM0_action (1800), knf 6234, damage 6288, die 6373,
 * nage 6598, enedam 6637, enedie 6676, playpch.c:46 (~0x1E4) sowie
 * en01.c:7541/7657, en04.c:3508 und en07.c:2197/2228. */
#define RECVX_HEAD_LOOK          0x004u /* Blicksuche aktiv (bhLookNearEnemy) */
/* 0x8 wird in der ausgelieferten Fassung nur gelesen (player.c:6824) und nur
 * geloescht (playpch.c:73); keine Stelle setzt es.  Der Zweig ist damit im
 * Original unerreichbar - er ist mitportiert, aber nur im Komponententest
 * ausloesbar. */
#define RECVX_HEAD_LOOK_HOLD     0x008u /* Zielwechsel/Punktnachfuehrung angehalten */
#define RECVX_HEAD_LOCKED        0x010u /* Ziel aufgeschaltet (bhSetHeadRotation) */
#define RECVX_HEAD_RETURN_N4_X   0x020u /* Knoten 4 X laeuft zurueck */
#define RECVX_HEAD_RETURN_N4_Y   0x040u /* Knoten 4 Y laeuft zurueck */
#define RECVX_HEAD_RETURN_N5_X   0x080u /* Knoten 5 X laeuft zurueck */
#define RECVX_HEAD_RETURN_N5_Y   0x100u /* Knoten 5 Y laeuft zurueck */
/* player.c:1801 u.a.: die vier Rueckfuehrungen werden stets gemeinsam gesetzt. */
#define RECVX_HEAD_RETURN_ALL    0x1E0u

/* EXP1_UC(120): Nickfolge eines Skripts.  Bit 0x20 senkt die Gierstufe,
 * 0x40 senkt die Neigungsstufe (player.c:6776-6818). */
#define RECVX_HEAD_SCRIPT_ACTIVE 0xF0u
#define RECVX_HEAD_SCRIPT_YAW_DOWN   0x20u
#define RECVX_HEAD_SCRIPT_PITCH_DOWN 0x40u

/* EXP1_I(64): Art des Blickziels (bhSetHeadRotation, player.c:7065-7120).
 * Einziger Schreiber im ganzen Spielcode ist bhLookNearEnemy (player.c:7030)
 * mit dem Wert 2; die uebrigen fuenf Zweige sind in der ausgelieferten
 * Fassung unerreichbar.  Sie sind mitportiert, aber nicht ausloesbar. */
typedef enum {
    RECVX_HEAD_TARGET_POINT   = 1, /* gespeicherter Punkt EXP1_F(72..80) */
    RECVX_HEAD_TARGET_ENEMY   = 2, /* ene[EXP1_S(68)] */
    RECVX_HEAD_TARGET_OBJECT  = 3, /* sys->obwp[EXP1_S(68)] */
    RECVX_HEAD_TARGET_ITEM    = 4, /* sys->itwp[EXP1_S(68)] */
    RECVX_HEAD_TARGET_EFFECT  = 5, /* eff[sys->efid[EXP1_S(68)]] */
    RECVX_HEAD_TARGET_ROOMPOS = 6  /* rom->posp[EXP1_S(68)] */
} RECVX_HEAD_TARGET;

/* Der Kopfzustand, Feld fuer Feld mit seinem Originaloffset in plp->exp1. */
typedef struct {
    uint32_t flags;        /* EXP1_I(0) */
    int32_t blink_frame;   /* EXP1_I(4): Index in PlyEyeTab */
    int32_t blink_timer;   /* EXP1_I(8) */
    float head[3];         /* EXP1_F(12/16/20): Kopfknoten dieses Bildes */
    float head_prev[3];    /* EXP1_F(24/28/32): Kopfknoten des Vorbildes */
    int16_t sum[3];        /* EXP1_S(36/38/40): Winkelsumme dieses Bildes */
    int16_t sum_prev[3];   /* EXP1_S(42/44/46): Winkelsumme des Vorbildes */
    int16_t node4[2];      /* EXP1_S(48/50): gehaltene Winkel des Knotens 4 */
    int16_t node5[2];      /* EXP1_S(54/56): gehaltene Winkel des Knotens 5 */
    int32_t look_timer;    /* EXP1_I(60) */
    int32_t target_kind;   /* EXP1_I(64), siehe RECVX_HEAD_TARGET */
    int16_t target_index;  /* EXP1_S(68) */
    int16_t target_node;   /* EXP1_S(70): lok_jno des Ziels */
    float target_point[3]; /* EXP1_F(72/76/80): gehaltener Zielpunkt */
    int32_t script_limit[2];  /* EXP1_I(84/88): Grenzen der Skriptfolge (X, Y) */
    int32_t script_step[2];   /* EXP1_I(96/100): Schrittweiten (X, Y) */
    int32_t script_value[2];  /* EXP1_I(108/112): laufende Werte (X, Y) */
    uint8_t script_flags;     /* EXP1_UC(120) */
    int32_t model_number;     /* plp->mdl_no: 3 hat keine Augentextur */
    int32_t mode0;            /* plp->mode0: 4 unterdrueckt den Blinkstart */
    int32_t eye_texture;      /* gewaehlter Eintrag aus PlyEyeTab, -1 = unveraendert */
} RECVX_HEAD_STATE;

/* Eingaben eines Originalbildes aus dem vorhandenen Besitzer. */
typedef struct {
    int32_t angles[6][3]; /* plp->mlwP->objP[0..5].ang, lokale Pose */
    int32_t ax,ay,az;     /* plp->ax/ay/az */
    float head[16];       /* plp->mlwP->owP[5].mtx */
    float position[3];    /* plp->px/py/pz */
    float random;         /* -rand()/-2.1474836e9f des Originalbildes, 0..1 */
    int32_t enemy_count;  /* sys->ewk_n: Zahl der Gegnerwerke, nicht der Kandidaten */
} RECVX_HEAD_INPUT;

/* Ein Blickziel, wie bhSearchNearEnemy2 es prueft (pwksub.c:443-477). */
typedef struct {
    int32_t index;      /* Gegnerindex, = EXP1_S(68) */
    int32_t look_node;  /* epp->lok_jno */
    float point[3];     /* Weltpunkt: px/py/pz oder owP[lok_jno].mtx */
    int32_t eligible;   /* flg&1, !(flg&0x8000), !(stflg&0x41000000) erfuellt */
} RECVX_HEAD_CANDIDATE;

/* Dienste, die der Host stellt.  Ohne `wall_clear` gibt es keine Sichtpruefung
 * und damit kein Ziel - es wird keine Ersatzregel eingesetzt. */
typedef struct {
    void* user;
    /* bhCheckC2WallN(&cap, 0x400) == 0: freie Sicht von `from` nach `to`. */
    int (*wall_clear)(void* user,const float from[3],const float to[3]);
    /* Knotenmatrix eines Blickziels (pwksub.c:455-457, player.c:7132-7134). */
    int (*target_node_point)(void* user,int kind,int index,int node,float out[3]);
    /* Fuer `unbound`: ein im Original benutzter Dienst ohne nativen Besitzer. */
    void (*unbound)(void* user,const char* name);
} RECVX_HEAD_HOST;

/* bhSetPlayer (player.c:787-789): exp1[0] = 1, exp1[34] = -1. */
void recvx_head_begin(RECVX_HEAD_STATE* state,int model_number);

/* Ein Bild bhControlPlayerHead.  `candidates` ist die Gegnerliste dieses
 * Bildes in Originalreihenfolge (ene[0..ewk_n-1]).  Die veraenderten
 * Knotenwinkel 4 und 5 stehen danach in `angles` und muessen vom Aufrufer in
 * den Posebesitzer zurueckgeschrieben werden, bevor die Modellmatrizen und
 * damit die Haarsteuerung laufen. */
void recvx_head_tick(RECVX_HEAD_STATE* state,const RECVX_HEAD_INPUT* input,
                     const RECVX_HEAD_CANDIDATE* candidates,unsigned count,
                     const RECVX_HEAD_HOST* host,int32_t angles[6][3]);

/* Die Treppen-/Leiter-/Stufenroutinen setzen im mode3-0-Startblock
 * EXP1_I(0) &= ~0x4 und |= 0x1E0 (player.c:2891/3153/3360/3522/3948/4143).
 * Dieselbe Folge benutzen alle anderen Stellen, die den Blick abschalten. */
void recvx_head_release_look(RECVX_HEAD_STATE* state);

/* Gegenstueck: der Blick wird wieder freigegeben (EXP1_I(0) |= 0x4). */
void recvx_head_enable_look(RECVX_HEAD_STATE* state);

#ifdef __cplusplus
}
#endif
