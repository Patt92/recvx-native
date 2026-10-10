/* Haarkette des angehaengten Spielerobjekts obwp[2] (Auftrag B04).
 *
 * Semantikreferenz (nur gelesen, kein Funktionskoerper uebernommen):
 *   bhObjClpn      objitm.c:2122-2383   Objekttask der Kette, mode0 0/1
 *   bhCalcHair     player.c:7207-7298   Winkelsumme und Wurzelgeschwindigkeit
 *   HAIR_WORK      objitm.h:7-25        Verlaufsfelder der Kette
 *   bhSetObject    objitm.c:154-213     Anfangszustand des Objektwerks
 *   bhSetPlayer    player.c:756-785     lkmtab-Eintrag, lkono 5, lox/loy/loz
 *   bhResetPlayer  player.c:962-991     dieselbe Bindung nach Figurenwechsel
 *   bhCalcModel    MdlPut.c:240-263     Matrix des angehaengten Objekts
 *   bhJumpObject2  objitm.c:125-140     Verteiler ueber op->id - 1200
 *   bhCyoutenHenkeiSet (case 0) event.c:6026  setzt plp->flg2 0x2
 *
 * Der Zustand ist eine Sicht auf das eine vorhandene Objektwerk des Hosts:
 * die Laufzeit haelt RECVX_HAIR_STATE und uebergibt je Bild die Kopfmatrix und
 * die lokale Spielerpose.  Es wird keine zweite Spieler-Work gefuehrt; Pose
 * und Knotenmatrizen kommen vom vorhandenen Actor-/Posebesitzer.
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Kettenlaenge und Ringpuffer wie im Original: vier Segmente (objitm.c:2232),
 * 128 Eintraege Verlauf (objitm.c:2165) mit Maske 0x7F (objitm.c:2234/2379). */
#define RECVX_HAIR_SEGMENTS 4
#define RECVX_HAIR_HISTORY  128

/* Varianten des Objektverteilers bhJumpObject2 (objitm.c:125-140), Index
 * op->id - 1200 (objitm.c:357).  Beide lkmtab-Eintraege stehen in
 * player.c:22-26: id 1211 fuer ply_id 0, id 1212 fuer ply_id 3. */
typedef enum {
    RECVX_HAIR_VARIANT_NONE = 0, /* kein angehaengtes Haarobjekt */
    RECVX_HAIR_VARIANT_CLPN = 1, /* id 1211 -> Index 11 -> bhObjClpn */
    RECVX_HAIR_VARIANT_WSSG = 2  /* id 1212 -> Index 12 -> bhObjWssg (leer) */
} RECVX_HAIR_VARIANT;

/* HAIR_WORK, objitm.h:7-25.  op->exp3 im Original. */
typedef struct {
    float ox,oy,oz;     /* Wurzel dieses Bildes (objitm.c:2251-2253) */
    float oxb,oyb,ozb;  /* Wurzel des Vorbildes (objitm.c:2247-2249) */
    int32_t ax,ay,az;   /* Winkelsumme dieses Bildes (player.c:7243-7245) */
    int32_t axb,ayb,azb;/* Winkelsumme des Vorbildes (player.c:7215-7217) */
    float spx,spy,spz;  /* Wurzelgeschwindigkeit im Kopfraum (player.c:7265-7267) */
} RECVX_HAIR_WORK;

/* Objektwerk der Kette.  Entspricht den von bhObjClpn benutzten Feldern von
 * O_WRK (flg/mode0/ct0/xn/yn/zn/spd/pv) und den beiden Puffern exp0 und exp3. */
typedef struct {
    uint32_t flg;                                   /* op->flg: 0x80 angehaengt, 0x1000/0x100000 */
    int32_t mode0;                                  /* op->mode0: 0 Init, 1 Lauf, 2 Halt */
    int32_t ct0;                                    /* Schreibzeiger des Verlaufs (objitm.c:2379) */
    float xn,yn,zn;                                 /* gefilterte Wurzelgeschwindigkeit (objitm.c:2214-2216) */
    float spd;                                      /* Fallanteil der Kette (objitm.c:2252-2263) */
    RECVX_HAIR_WORK hair;                           /* op->exp3 */
    float history[RECVX_HAIR_HISTORY][3];           /* op->exp0 + 0: Verlauf (objitm.c:2163-2170) */
    float goal[RECVX_HAIR_SEGMENTS][3];             /* op->exp0 + 4096: Zielpunkt je Segment */
    float point[RECVX_HAIR_SEGMENTS][3];            /* op->exp0 + 8192: gefilterter Punkt je Segment */
    /* op->pv: Weltpunkt je Segment (objitm.c:2374-2376).  Der einzige Leser im
     * Original ist bhDrawLinEffect (effect.c:1120-1129), und der bezieht seine
     * Objektliste ausschliesslich aus sys->ef_lin/ef_linn (effect.c:871-873).
     * Diese Liste wird in keiner Originalquelle gefuellt - sie wird nur dort
     * gelesen -, der Linienzeichner ist also unerreichbar.  Haarobjekte laufen
     * wegen id >= 1210 nach sys->ob_hlg (objitm.c:366-371) und werden von
     * bhDrawObject als Modell gezeichnet, nicht als Linie.  pv wird deshalb
     * geschrieben wie im Original, hat aber keinen aktiven Verbraucher; der
     * Zeichenpass nimmt die Knotenwinkel. */
    float pv[RECVX_HAIR_SEGMENTS][3];
    int32_t angles[RECVX_HAIR_SEGMENTS][2];         /* objP[i].ang[0]/ang[1] (objitm.c:2363/2368) */
} RECVX_HAIR_STATE;

/* Eingaben eines Originalbildes.  Alle Werte stammen aus dem vorhandenen
 * Actor-/Posebesitzer, nicht aus einer eigenen Spielerarbeit. */
typedef struct {
    float head[16];        /* pp->mlwP->owP[5].mtx: Weltmatrix des Kopfknotens */
    int32_t angles[6][3];  /* pp->mlwP->objP[0..5].ang: lokale Pose (player.c:7219-7241) */
    int32_t ax,ay,az;      /* pp->ax/ay/az (player.c:7219-7221) */
    float offset[3];       /* obwp[2].lox/loy/loz (player.c:763-765 bzw. 777-779) */
    int32_t reset;         /* pp->flg2 0x2: Raumwechsel/Versetzen (objitm.c:2184) */
    int32_t ground;        /* pp->flg2 0x100 (objitm.c:2242); im Original nie gesetzt */
} RECVX_HAIR_INPUT;

/* bhSetObject + bhSetPlayer/bhResetPlayer: setzt das Objektwerk auf seinen
 * Anfangszustand und waehlt die Variante nach sys->ply_id.  Ein ply_id ohne
 * lkmtab-Eintrag ergibt RECVX_HAIR_VARIANT_NONE. */
RECVX_HAIR_VARIANT recvx_hair_begin(RECVX_HAIR_STATE* state,int ply_id);

/* Ein Objekttask-Bild von bhObjClpn.  Liefert 1, wenn dieses Bild Winkel
 * erzeugt hat (mode0 1), sonst 0 (Initialbild mode0 0, Halt mode0 2, oder
 * eine Variante ohne Kettenkoerper).  `angles` erhaelt objP[0..3].ang[0..1]. */
int recvx_hair_tick(RECVX_HAIR_STATE* state,RECVX_HAIR_VARIANT variant,
                    const RECVX_HAIR_INPUT* input,int32_t angles[RECVX_HAIR_SEGMENTS][2]);

#ifdef __cplusplus
}
#endif
