#ifndef RECVX_ITEM_TABLES_H
#define RECVX_ITEM_TABLES_H
/* Item-/Kombinations-/Anzeigedaten als eigene native Deklaration.
 *
 * Die Werte liegen in native/item_tables.c (reine Spieldatentabellen). Dieser
 * Header ersetzt include/ps2/veronica/prog/item.h fuer den nativen Build: jener
 * zieht ueber types.h die PS2-Ninja-Header herein, die nativ nicht vorliegen.
 * Die Strukturen sind byteweise dieselben; NJS_POINT3 kommt aus ninja_matrix.h.
 *
 * Feldbedeutungen sind damit nicht geklaert. Wer eine Tabelle auswertet, muss
 * ihre Semantik eigenstaendig belegen.
 */
#include "ninja_matrix.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ITEM_WORK {
    unsigned short max;
    short cmb;
    unsigned short type;
} ITEM_WORK;
typedef struct DSP_WORK {
    short ax, ay, az, hide;
} DSP_WORK;
extern const unsigned int getbulletmax[156][4];
extern const NJS_POINT3 hoseipos[10];
extern const unsigned short idsettbl[16];
extern const unsigned short getmestbl[16];
extern const unsigned short itemidtbl[16];
extern const unsigned char itemflg[41][8];
extern const unsigned short mestbl[14];
extern const short c_a[14][6];
extern const unsigned char curedata[10];
extern const DSP_WORK dsptbl[160];
extern const ITEM_WORK itemdata[156];
extern const short combidata[410];
#ifdef __cplusplus
}
#endif
#endif
