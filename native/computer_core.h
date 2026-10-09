#pragma once
#include "computer_tables.h"
#ifdef __cplusplus
extern "C" {
#endif
// Text grid of the terminal (COM_EVT_WORK.txt): 11 rows of 29 cells.
#define RECVX_COM_ROWS 11
#define RECVX_COM_COLUMNS 29
#define RECVX_COM_MAX_DRAW 800
// One njDrawQuadTexture (kind 0) or a solid njDrawPolygon2D rectangle (kind 1), in original call order.
typedef struct {int kind,trans,texture;float x1,y1,x2,y2,u1,v1,u2,v2,z;unsigned color;} RECVX_COM_DRAW;
typedef struct {unsigned st_flg,cb_flg,sp_flg,gm_flg,ts_flg;} RECVX_COM_SYSTEM;
// pad_on: held direction bits (1 up, 2 down, 4 left, 8 right) as sys->pad_on & 0xf.
// pad_ps: newly pressed bits (0x800 OK, 0x1000 cancel).
// syRtcGetDate; clock_valid 0 reproduces the original fallback 21:35:12.
typedef struct {unsigned pad_on,pad_ps;int clock_valid,hour,minute,second;} RECVX_COM_INPUT;
// bhDrawSpEvtComCharacter: rom->enep[chr_no] drawn into the character window.
// njSetScreen(dist, w, h, cx, cy) and njUserClipping(2, ...) of the original,
// njTranslate/njRotateXYZ/njScale on the base view matrix (cmat).
typedef struct {
    int active,chr_no,mtn_no,frm_no;
    float px,py,pz,scale,dist,width,height,cx,cy;
    int ax,ay,az;
    float clip_x1,clip_y1,clip_x2,clip_y2;
} RECVX_COM_CHARACTER;
typedef struct {
    int active,finished,stand_player,message_visible,message_number,message_lines;
    unsigned draw_count,sound_count,flag_count,unknown_opcode,draw_overflow;
    unsigned sounds[16],flags[16];
    RECVX_COM_DRAW draw[RECVX_COM_MAX_DRAW];
    RECVX_COM_CHARACTER character;
    RECVX_COM_SYSTEM system;
    // Diagnostics for tests and the runtime state dump; no drawing state.
    unsigned number,com_md0,com_md1,com_flg,txflg,lcflg;
    int tx,ty,kx,ky,wordn;
    unsigned text[RECVX_COM_ROWS][RECVX_COM_COLUMNS];
    char word[2][16];
} RECVX_COM_FRAME;
// bhSysCall (system.c) entering cb_flg 0x400000: bhKeepSpEvtComputer and the task switch; com_num from bhEvtComSet.
void recvx_com_begin(unsigned number,const RECVX_COM_SYSTEM* system);
// One 30 Hz bhSysCallCompEvent pass. Returns 0 once bhExitSpEvtComputer has run.
int recvx_com_tick(const RECVX_COM_INPUT* input,RECVX_COM_FRAME* frame);
#ifdef __cplusplus
}
#endif
