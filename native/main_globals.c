/* Gemeinsame Globals, woertlich aus src/ps2/veronica/prog/main.c:40-57 (Definition genau einmal). */
#include "types.h"
#include "main.h"
HWS_WORK hwsp;
SYS_WORK sysp;
ROM_WORK romp;
CAM_WORK cam;
BH_PWORK ply;
BH_PWORK ene[128];
O_WRK eff[512];
unsigned char* freemem;
unsigned char* keepmem;
unsigned char* njpmemp __attribute__((aligned(64)));
unsigned char* vwbmemp;
unsigned char* vebmemp;
HWS_WORK* hws = &hwsp;
SYS_WORK* sys = &sysp;
ROM_WORK* rom = &romp;
BH_PWORK* plp = &ply;
