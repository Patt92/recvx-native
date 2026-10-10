/* Pure data tables of the map screen, transcribed from
   src/ps2/veronica/prog/map.c. Data only - no original function body is
   copied, generated, compiled or linked here. tests/native/map_screen_test.cpp
   re-reads the initialiser text of map.c and compares every value. */
#include "map_tables.h"

/* map.c:27 MapPal - const static NJS_ARGB MapPal[32] */
const RECVX_MAP_ARGB recvx_map_pal[32] = {
    {1.0f,0.7f,0.7f,0.7f},{1.0f,0.1f,0.2f,0.4f},{1.0f,0.4f,0.1f,0.2f},{0.5f,1.0f,0.5f,0.0f},
    {1.0f,0.6f,0.1f,0.5f},{0.3f,0.2f,0.2f,0.8f},{1.0f,1.0f,1.0f,0.0f},{1.0f,0.0f,0.4f,0.8f},
    {1.0f,0.8f,0.0f,0.4f},{0.8f,1.0f,0.5f,0.0f},{1.0f,1.0f,0.0f,0.0f},{1.0f,1.0f,0.0f,1.0f},
    {1.0f,0.8f,0.8f,0.0f},{1.0f,0.8f,0.0f,0.8f},{1.0f,0.0f,0.8f,0.8f},{1.0f,0.8f,0.8f,0.0f},
    {1.0f,0.0f,0.8f,0.0f},{1.0f,1.0f,1.0f,1.0f},{1.0f,1.0f,1.0f,1.0f},{0.8f,0.6f,0.4f,0.0f},
    {1.0f,0.8f,0.0f,0.0f},{1.0f,0.0f,0.8f,0.0f},{1.0f,0.0f,0.0f,0.8f},{1.0f,0.0f,1.0f,1.0f},
    {1.0f,0.5f,0.1f,0.1f},{1.0f,0.5f,0.3f,0.3f},{1.0f,1.0f,1.0f,1.0f},{1.0f,1.0f,1.0f,1.0f},
    {1.0f,0.8f,0.8f,0.0f},{1.0f,0.8f,0.0f,0.8f},{1.0f,0.0f,0.8f,0.8f},{1.0f,0.8f,0.8f,0.0f}
};

/* map.c:38 MapCol - const static NJS_COLOR MapCol[3] */
const uint32_t recvx_map_col[3] = {0xFF000800u,0xFF003000u,0xC0A0A0A0u};

/* map.c:42 ItmDat - const static ID_WORK ItmDat[22] {itm_no, flg_no} */
const RECVX_MAP_ITEM_FLAG recvx_map_itm_dat[22] = {
    {59,96},{86,96},{84,97},{56,98},{51,99},{67,100},{68,101},{33,102},{73,103},{62,104},
    {93,105},{76,106},{75,107},{89,108},{90,109},{81,110},{92,111},{105,112},{66,113},{44,114},
    {46,115},{108,116}
};

/* map.c:48 CncDatA - const static unsigned short CncDatA[81] */
const uint16_t recvx_map_cnc_dat_a[81] = {
    65284,1,65285,256,416,65325,40960,40961,41121,65347,41376,41232,41233,41234,65287,4113,65288,4368,4370,4513,4400,
    4401,4402,65289,4625,65294,12337,65295,12592,12594,12560,12561,12562,12706,65296,12849,65345,41520,41521,41522,
    41504,41505,41506,41507,65290,8225,8354,8256,8257,65291,8480,8482,65292,8737,8739,65293,8994,65297,16449,16416,
    16417,16418,16419,16464,65298,16704,65299,20544,20545,20576,20577,20578,65300,24673,65301,24928,24930,24912,65302,
    25185,65535
};

/* map.c:56 CncDatB - const static unsigned short CncDatB[49] */
const uint16_t recvx_map_cnc_dat_b[49] = {
    65303,28785,65304,29040,29042,65294,29297,29299,65295,29554,29556,29568,29569,65296,29811,65297,32897,32880,32881,
    32882,32883,32884,32912,32913,32914,32915,32916,32917,65298,33152,65305,37009,65306,37264,37266,65307,37521,37523,
    65308,37778,37780,65300,38035,38037,65301,38292,38272,38273,65535
};

/* map.c:729 SndTbl - static const int SndTbl[1] inside MapCodeProcess */
const int recvx_map_snd_tbl[1] = {(int)0x80000247};

/* map.c:1042 MrkAtr - static MA_WORK MrkAtr[6] {mrk_no, scale, offset, pal_no} */
const RECVX_MAP_MARKER recvx_map_mrk_atr[6] = {
    {0,3.0f,24.0f,16},{1,3.0f,20.0f,28},{1,3.0f,16.0f,13},
    {1,3.0f,12.0f,14},{3,1.0f,12.0f,30},{2,2.0f,8.0f,31}
};

/* map.c:1146 TexAnm - static const NJS_TEXANIM TexAnm[18] */
const RECVX_MAP_TEXANIM recvx_map_tex_anm[18] = {
    { 40, 56,20,26,  0,  0, 80,112,0,0},
    { 32, 16,16, 8,192,  0,256, 32,0,0},
    { 32, 16,15, 8,256, 32,192,  0,0,0},
    { 16, 32, 8,15,160,  0,192, 64,0,0},
    { 16, 32, 8,16,192, 64,160,  0,0,0},
    { 24, 24,12,12,208, 80,256,128,0,0},
    { 24, 24,12,12,208,128,256,176,0,0},
    { 24, 24,12,12,208, 32,256, 80,0,0},
    { 88, 32,41,16,  0,128,176,192,0,0},
    { 88, 16, 0, 8,  0,192,176,224,0,0},
    { 40, 16, 0, 8, 96, 64,176, 96,0,0},
    { 40, 16, 0, 8, 96, 96,176,128,0,0},
    {104, 16, 0, 8,  0,224,208,256,0,0},
    { 16, 16, 7, 7,224,224,256,256,0,0},
    { 24, 16, 0,16, 96,  0,144, 32,0,0},
    { 24, 16, 0,16, 96,  0,144, 32,0,0},
    { 24, 16,24, 0, 96, 32,144, 64,0,0},
    {128,128, 0, 0,  0,  0,256,256,0,0}
};

/* map.c:1425 SprSet - static const SS_WORK SprSet[11].
   `function` replaces the original function pointer by the procedure index;
   the mp_spr sprite number, act_bit, spr_col and spr_pos are the original
   values. Original order: MP_SET_SILHOUETTE, ARROW_UP/DOWN/LEFT/RIGHT,
   ARROW2_UP/DOWN/LEFT/RIGHT, LR_ZOOM, TITLE. */
const RECVX_MAP_SPRITE_SET recvx_map_spr_set[11] = {
    {RECVX_MP_SPR_SILHOUETTE, RECVX_MAP_FSPR_SILHOUETTE,1535,0xFF004000u,585.0f, 66.0f,-8.0f},
    {RECVX_MP_SPR_ARROW_UP,   RECVX_MAP_FSPR_ARROW,      128,0xFF008000u,320.0f, 40.0f,-6.0f},
    {RECVX_MP_SPR_ARROW_DOWN, RECVX_MAP_FSPR_ARROW,      128,0xFF008000u,320.0f,443.0f,-6.0f},
    {RECVX_MP_SPR_ARROW_LEFT, RECVX_MAP_FSPR_ARROW,      128,0xFF008000u, 32.0f,240.0f,-6.0f},
    {RECVX_MP_SPR_ARROW_RIGHT,RECVX_MAP_FSPR_ARROW,      128,0xFF008000u,610.0f,240.0f,-6.0f},
    {RECVX_MP_SPR_ARROW_UP,   RECVX_MAP_FSPR_ARROW2,     256,0xFF4050A0u,320.0f, 40.0f,-6.0f},
    {RECVX_MP_SPR_ARROW_DOWN, RECVX_MAP_FSPR_ARROW2,     256,0xFF4050A0u,320.0f,443.0f,-6.0f},
    {RECVX_MP_SPR_ARROW_LEFT, RECVX_MAP_FSPR_ARROW2,     256,0xFF4050A0u, 32.0f,240.0f,-6.0f},
    {RECVX_MP_SPR_ARROW_RIGHT,RECVX_MAP_FSPR_ARROW2,     256,0xFF4050A0u,610.0f,240.0f,-6.0f},
    {RECVX_MP_SPR_LR_ZOOM,    RECVX_MAP_FSPR_SPRITE,     256,0xFFFFFFFFu,320.0f, 80.0f,-8.0f},
    {RECVX_MP_SPR_TITLE,      RECVX_MAP_FSPR_SPRITE,    1120,0xFFFFFFFFu, 64.0f, 32.0f,-8.0f}
};

/* map.c:2396 FsubZoomCursor dst_pos0 */
const RECVX_MAP_POINT2 recvx_map_zoom_dst_pos0[4] = {
    { 56.0f, 64.0f},{585.0f, 64.0f},{ 56.0f, 64.0f},{ 56.0f,417.0f}
};
/* map.c:2403 dst_pos1 */
const RECVX_MAP_POINT2 recvx_map_zoom_dst_pos1[4] = {
    { 56.0f,240.0f},{585.0f,240.0f},{320.0f, 64.0f},{320.0f,417.0f}
};
/* map.c:2410 dst_pos2 */
const RECVX_MAP_POINT2 recvx_map_zoom_dst_pos2[4] = {
    { 56.0f,417.0f},{585.0f,417.0f},{585.0f, 64.0f},{585.0f,417.0f}
};

/* map.c:2491 FsubZoomInfomation set_pos - static const NJS_POINT3 */
const float recvx_map_info_set_pos[3] = {441.0f,353.0f,-8.0f};

/* map.c:2580 FsubZoomScreen ScrOut */
const RECVX_MAP_POINT2 recvx_map_scr_out[2] = {{0.0f,0.0f},{640.0f,480.0f}};
/* map.c:2585 ScrIn */
const RECVX_MAP_POINT2 recvx_map_scr_in[2] = {{56.0f,64.0f},{585.0f,417.0f}};

/* map.c:2714 FsubCompass CmpArw - static const NJS_POLYGON_VTX CmpArw[4] */
const RECVX_MAP_POLYGON_VTX recvx_map_cmp_arw[4] = {
    {  0.0f,-30.0f,0.0f,0x6000C000u},{-12.0f,-22.0f,0.0f,0x6000C000u},
    { 12.0f,-22.0f,0.0f,0x6000C000u},{  0.0f, 30.0f,0.0f,0x6000C000u}
};

/* map.c:2763 FsubModeMessage NmlMes - static const MD_WORK NmlMes[2] */
const RECVX_MAP_MODE_MESSAGE recvx_map_nml_mes[2] = {
    {464.0f,441.0f,-7.0f,5},{480.0f,441.0f,-7.0f,9}
};

/* map.c:2838 MapCnc AdjMapA - static const int AdjMapA[3] */
const int recvx_map_adj_map_a[3] = {64,128,832};
