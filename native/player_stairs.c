#include "player_stairs.h"
/* PORTIERUNGSLÜCKE (Policy 2026-10-09): Diese Datei band bhCPM2_act_kdu/kdd,
 * bhCPM2_act_hsu/hsd, bhCPM2_act_dnu/dnd sowie PlMtnAct/PlFootSnd/PlKDU und
 * den EXP_WORK-Satz ein, die tools/generate_player_stairs.py als
 * player_stairs.inc aus player.c erzeugt hat. Generator und Include liegen in
 * archived/FORBIDDEN_DONTUSE/ und dürfen weder gelesen noch ausgeführt
 * werden. Mit dem Include ist auch der hier gehaltene eigene Spieler-/
 * System-Arbeitssatz (BH_PWORK plus sys_work/rom_work) entfallen, den die
 * Policy zusätzlich als separaten Work-Owner verbietet.
 *
 * Zu leisten: die Treppen-, Leiter- und Stufenaktionen als eigene native
 * Mechanik bestimmen — Auswahl der Aktion aus der ETC-Fläche (Typ 1/2,
 * attr&1 Leiter, prm0 Richtung), die Bewegungsabläufe je Aktion, der
 * Bodennummernwechsel über rom->grand, die Fußfixierung über PlyLegRoute und
 * die Schrittgeräusche. Nicht aus einem Generator über die Originalquellen.
 *
 * Solange das offen ist, verhält sich der Treppen-Task als dokumentierte
 * Lücke: recvx_stairs_begin übernimmt ausschließlich die vom Aufrufer
 * gelieferte Position/Blickrichtung/Etage in den Zustand und markiert ihn als
 * aktiv, damit runtime.cpp seinen Treppen-Task im nächsten Tick regulär
 * beendet; recvx_stairs_tick führt keine Aktion aus und gibt die Steuerung
 * sofort zurück. Es wird weder eine Bewegung noch eine Motion, ein
 * Etagenwechsel oder ein Geräusch erzeugt, und unsupported_services bleibt 0,
 * weil kein unerreichbarer Dienst angefragt wird.
 *
 * Sichtbare Folge: Treppen und Leitern lassen sich nicht mehr begehen. Die
 * Aktionstaste an einer Treppen-/Leiter-/Stufenfläche verbraucht einen Tick
 * und gibt die Steuerung unverändert zurück; der Spieler bleibt auf seiner
 * Etage stehen. Auch der Skriptbefehl bhPlayerKaidanMotion bewegt niemanden
 * mehr. Kein geratener Ersatzwert und keine nachgebaute Kletterbewegung. */
int recvx_stairs_begin(RECVX_STAIR_STATE* state,const RECVX_STAIR_AREA* area,const float* grand,float px,float py,float pz,int ay,int floor,int dmlvl,int wpntp){
    (void)area;(void)grand;(void)dmlvl;(void)wpntp;
    if(!state)return 0;
    /* Der Aufrufer übernimmt px/py/pz/ay aus diesem Zustand zurück in den
     * Spieler; die Werte bleiben deshalb genau die übergebenen. */
    state->px=px;state->py=py;state->pz=pz;state->ay=ay;
    state->motion=0;state->frame=0;state->floor=floor;
    state->status=0;state->mode0=0;state->unsupported_services=0;
    state->active=1; /* runtime.cpp beendet den Task im nächsten Tick */
    return 1;
}
int recvx_stairs_tick(RECVX_STAIR_STATE* state,const RECVX_STAIR_HOST* host){
    (void)host;
    if(!state||!state->active)return 0;
    state->motion=0;state->frame=0;state->status=0;state->mode0=0;
    state->unsupported_services=0;
    state->active=0;
    return 0;
}
