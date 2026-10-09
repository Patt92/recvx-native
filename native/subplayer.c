#include "subplayer.h"
/* PORTIERUNGSLÜCKE (Policy 2026-10-09): Diese Datei band die Tabelle
 * subplayer_ids sowie em60_init, subpl_mv00_tbl und subpl_mv01_tbl ein, die
 * tools/generate_subplayer.py als subplayer.inc aus subpl.c/eneset.c erzeugt
 * hat. Generator und Include liegen in archived/FORBIDDEN_DONTUSE/ und dürfen
 * weder gelesen noch ausgeführt werden.
 *
 * Zu leisten: die Begleitfiguren-Steuerung (Sub_controll) als eigene native
 * Mechanik bestimmen — welche Gegner-IDs überhaupt Begleitfiguren sind, ihre
 * Initialisierung und die beiden Bewegungs-Dispatchtabellen für mode0 1
 * (mode2 0) und mode0 5/6, jeweils mit den 15 ct2-Zuständen. Nicht aus einem
 * Generator über die Originalquellen.
 *
 * Solange das offen ist, melden alle drei Funktionen den dokumentierten
 * Neutralwert: keine Figur wird als Begleitfigur erkannt (recvx_is_subplayer
 * liefert 0), weshalb world.cpp auch keine Begleit-Arbeit mehr bindet und die
 * Dispatch-Diagnose nicht erreicht wird. Kein geratener Ersatzwert.
 *
 * Sichtbare Folge: Begleitfiguren (z. B. Steve als Mitläufer) bewegen sich
 * nicht mehr eigenständig. Sie werden wie gewöhnliche Raumfiguren behandelt
 * und bleiben stehen, solange kein Skript sie animiert. */
int recvx_is_subplayer(unsigned identity){
    (void)identity;
    return 0;
}
void recvx_subplayer_init(RECVX_SUBPLAYER* work){
    (void)work;
}
int recvx_subplayer_tick(RECVX_SUBPLAYER* work){
    (void)work;
    return 0;
}
