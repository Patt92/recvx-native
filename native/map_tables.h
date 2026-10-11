#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pure data tables of the map screen (src/ps2/veronica/prog/map.c). Data only;
   the mechanism is native (map_screen.cpp). Field order follows the original
   initialisers; tests/native/map_screen_test.cpp compares every value with the
   text of map.c. */

/* NJS_ARGB {a, r, g, b} */
typedef struct {float a,r,g,b;} RECVX_MAP_ARGB;
/* ID_WORK (map.h): inventory item id -> ic_flg (flag group 9) number. */
typedef struct {uint8_t itm_no,flg_no;} RECVX_MAP_ITEM_FLAG;
/* MA_WORK (map.h): marker model slot, xz scale, y offset, default palette. */
typedef struct {int mrk_no;float scale,offset;int pal_no;} RECVX_MAP_MARKER;
/* NJS_TEXANIM {sx, sy, cx, cy, u1, v1, u2, v2, texid, attr}. */
typedef struct {int sx,sy,cx,cy,u1,v1,u2,v2,texid;uint32_t attr;} RECVX_MAP_TEXANIM;
/* SS_WORK (map.h). `function` names the sprite procedure:
   0 FsprSilhouetteDraw, 1 FsprArrowDraw, 2 FsprArrowDraw2, 3 FsprSpriteDraw. */
typedef struct {int spr_no;int function;int act_bit;uint32_t spr_col;float x,y,z;} RECVX_MAP_SPRITE_SET;
enum {RECVX_MAP_FSPR_SILHOUETTE=0,RECVX_MAP_FSPR_ARROW=1,RECVX_MAP_FSPR_ARROW2=2,RECVX_MAP_FSPR_SPRITE=3};
/* mp_spr (map.h:78): sprite number, index into TexAnm. */
enum {
    RECVX_MP_SPR_SILHOUETTE=0,RECVX_MP_SPR_ARROW_DOWN=1,RECVX_MP_SPR_ARROW_UP=2,
    RECVX_MP_SPR_ARROW_LEFT=3,RECVX_MP_SPR_ARROW_RIGHT=4,RECVX_MP_SPR_X_BUTTON=5,
    RECVX_MP_SPR_B_BUTTON=6,RECVX_MP_SPR_A_BUTTON=7,RECVX_MP_SPR_LR_ZOOM=8,
    RECVX_MP_SPR_CHANGE=9,RECVX_MP_SPR_ITEM=10,RECVX_MP_SPR_BOX=11,
    RECVX_MP_SPR_SAVEPOINT=12,RECVX_MP_SPR_DIAMOND=13,RECVX_MP_SPR_10M_D=14,
    RECVX_MP_SPR_10M_L=15,RECVX_MP_SPR_0M=16,RECVX_MP_SPR_TITLE=17,RECVX_MP_SPR_NUM=18
};
/* mp_set (map.h:46): MapEntrySprite selector, index into SprSet. */
enum {
    RECVX_MP_SET_SILHOUETTE=0,RECVX_MP_SET_ARROW_UP=1,RECVX_MP_SET_ARROW_DOWN=2,
    RECVX_MP_SET_ARROW_LEFT=3,RECVX_MP_SET_ARROW_RIGHT=4,RECVX_MP_SET_ARROW2_UP=5,
    RECVX_MP_SET_ARROW2_DOWN=6,RECVX_MP_SET_ARROW2_LEFT=7,RECVX_MP_SET_ARROW2_RIGHT=8,
    RECVX_MP_SET_LR_ZOOM=9,RECVX_MP_SET_TITLE=10,RECVX_MP_SET_NUM=11
};
/* mp_mod (map.h:61): bhControlMap state machine. */
enum {
    RECVX_MP_MOD_FIRST=0,RECVX_MP_MOD_IDX_READ=1,RECVX_MP_MOD_IDX_ANALYZE=2,
    RECVX_MP_MOD_MAP_READ=3,RECVX_MP_MOD_DAT_SET=4,RECVX_MP_MOD_VEW_NORMAL=5,
    RECVX_MP_MOD_VEW_ZOOM=6,RECVX_MP_MOD_WAIT_NORMAL=7,RECVX_MP_MOD_WAIT_ZOOM=8,
    RECVX_MP_MOD_EXIT=9,RECVX_MP_MOD_WAIT=10,RECVX_MP_MOD_WAIT_NOMAP=11
};
/* enum_2 set_mod of bhInitMap (system.c:785-879): status |= (1 << set_mod) & 7. */
enum {RECVX_MP_SET_GAME=0,RECVX_MP_SET_EVENT=1,RECVX_MP_SET_SUB=2};
/* MD_WORK (map.h): mode-message sprite position and sprite number. */
typedef struct {float x,y,z;int spr;} RECVX_MAP_MODE_MESSAGE;
typedef struct {float x,y;} RECVX_MAP_POINT2;
/* NJS_POLYGON_VTX {x, y, z, col}. */
typedef struct {float x,y,z;uint32_t col;} RECVX_MAP_POLYGON_VTX;

extern const RECVX_MAP_ARGB recvx_map_pal[32];            /* map.c:27 MapPal */
extern const uint32_t recvx_map_col[3];                   /* map.c:38 MapCol */
extern const RECVX_MAP_ITEM_FLAG recvx_map_itm_dat[22];   /* map.c:42 ItmDat */
extern const uint16_t recvx_map_cnc_dat_a[81];            /* map.c:48 CncDatA */
extern const uint16_t recvx_map_cnc_dat_b[49];            /* map.c:56 CncDatB */
extern const int recvx_map_snd_tbl[1];                    /* map.c:729 SndTbl (MapCodeProcess) */
extern const RECVX_MAP_MARKER recvx_map_mrk_atr[6];       /* map.c:1042 MrkAtr */
extern const RECVX_MAP_TEXANIM recvx_map_tex_anm[18];     /* map.c:1146 TexAnm */
extern const RECVX_MAP_SPRITE_SET recvx_map_spr_set[11];  /* map.c:1425 SprSet */
extern const RECVX_MAP_POINT2 recvx_map_zoom_dst_pos0[4]; /* map.c:2396 FsubZoomCursor dst_pos0 */
extern const RECVX_MAP_POINT2 recvx_map_zoom_dst_pos1[4]; /* map.c:2403 dst_pos1 */
extern const RECVX_MAP_POINT2 recvx_map_zoom_dst_pos2[4]; /* map.c:2410 dst_pos2 */
extern const float recvx_map_info_set_pos[3];             /* map.c:2491 FsubZoomInfomation set_pos */
extern const RECVX_MAP_POINT2 recvx_map_scr_out[2];       /* map.c:2580 FsubZoomScreen ScrOut */
extern const RECVX_MAP_POINT2 recvx_map_scr_in[2];        /* map.c:2585 ScrIn */
extern const RECVX_MAP_POLYGON_VTX recvx_map_cmp_arw[4];  /* map.c:2714 FsubCompass CmpArw */
extern const RECVX_MAP_MODE_MESSAGE recvx_map_nml_mes[2]; /* map.c:2763 FsubModeMessage NmlMes */
/* map.c:2838 MapCnc AdjMapA. Values confirmed in SLUS_201.84 at 0x0035D010
   (MapCnc loads it via lui 0x36 / addiu -0x2FF0, 0x002B7254). */
extern const int recvx_map_adj_map_a[3];
#ifdef __cplusplus
}
#endif
