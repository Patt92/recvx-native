/* ADX-Lautstaerketabelle: reine Spieldaten, kein Originalkern-Code und kein
 * portierter Mechanismus.
 *
 * Die 128 Werte sind die Daempfungsstufen, die der Mixer ueber
 * AudioMixer::adx_gain in einen Faktor umrechnet (10^(wert/200)). Sie kamen
 * vorher aus dem erzeugten adx_tables.inc; dessen Erzeuger schrieb daneben
 * gesperrte Funktionsdateien, die anschliessend geloescht werden mussten.
 * Nach der Policy vom 2026-10-09 liegen die Werte deshalb hier als eigene
 * native Quelle, ohne jede Erzeugung beim Build.
 *
 * Es ist eine Datentabelle: die Bedeutung der Stufen ist damit nicht geklaert.
 */
#include "adx_tables.h"

const short AdxVolTbl[128] = {
    0, -2, -4, -6, -8, -10, -12, -14, -16, -18, -20, -22, -24, -26, -28, -30,
    -32, -34, -36, -38, -40, -42, -44, -46, -48, -50, -52, -54, -56, -58, -60, -62,
    -64, -70, -76, -82, -88, -94, -100, -106, -112, -118, -124, -130, -136, -142, -148, -154,
    -160, -166, -172, -178, -184, -190, -196, -202, -208, -214, -220, -226, -232, -238, -244, -250,
    -256, -264, -272, -280, -288, -296, -304, -312, -320, -328, -336, -344, -352, -360, -368, -376,
    -384, -392, -400, -408, -416, -424, -432, -440, -448, -456, -464, -472, -480, -488, -496, -504,
    -512, -528, -544, -560, -576, -592, -608, -624, -640, -656, -672, -688, -704, -720, -736, -752,
    -768, -784, -800, -816, -832, -848, -864, -880, -896, -912, -928, -944, -960, -976, -992, -999
};
