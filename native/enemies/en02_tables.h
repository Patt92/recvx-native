#ifndef RECVX_NATIVE_EN02_TABLES_H
#define RECVX_NATIVE_EN02_TABLES_H
#include "enemy_work.h"
#include "en03_lib.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Reine Datentabellen des Gulp Worm (en02.c:184-682). Layouts aus en02.h
   (EN02_WE_WORK 0x14, EN02_SANDEFFECTTBL_WORK 0xC) und types.h (DMG_REACT 0x7,
   BLOOD_TBL 0x1C, CPCL); COMBWEP/COMBJOINT sind die Spiegel aus en03_lib.h.
   Werte unveraendert; keine Funktionskoerper. */

typedef struct { uint32_t frm_no; float ofx, ofy, ofz; uint32_t size; } RECVX_EN02_WE;
typedef struct { int32_t mtn_no; const RECVX_EN02_WE* we; uint32_t num; } RECVX_EN02_SANDTBL;
typedef struct { int8_t type[3]; int8_t bloodstain[3]; int8_t exef; } RECVX_EN02_DMG_REACT;
typedef struct { int32_t flg; RECVX_ENEMY_VEC ofp; float rx, ry, rz; } RECVX_EN02_BLOOD;

extern const RECVX_EN02_WE recvx_en02_sd_01[38];
extern const RECVX_EN02_WE recvx_en02_sd_03[54];
extern const RECVX_EN02_WE recvx_en02_sd_11[113];
extern const RECVX_EN02_WE recvx_en02_sd_14[8];
extern const RECVX_EN02_WE recvx_en02_sd_16[47];
extern const RECVX_EN02_WE recvx_en02_sd_17[37];
extern const RECVX_EN02_WE recvx_en02_p0_42[3];
extern const RECVX_EN02_WE recvx_en02_p0_43[2];
/* en02.c:655 SandEffectTbl[9]: Motion -> Sandeffekt-Zeilen, -1 terminiert. */
extern const RECVX_EN02_SANDTBL recvx_en02_sand_tbl[9];
extern const RECVX_EN02_DMG_REACT recvx_en02_dmg_react[21];
extern const RECVX_T3_COMBWEP recvx_en02_comb_wep[21];
extern const RECVX_T3_COMBJOINT recvx_en02_comb_joint[24];
extern const RECVX_EN02_BLOOD recvx_en02_blood_tbl[24];
extern const RECVX_ENEMY_CPCL recvx_en02_cap_col[13];

#ifdef __cplusplus
}
#endif
#endif
