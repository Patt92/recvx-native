#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#define RECVX_COM_MAX_DRAW 640
// One njDrawQuadTexture (kind 0) or a solid njDrawPolygon2D rectangle (kind 1), in original call order.
typedef struct {int kind,trans,texture;float x1,y1,x2,y2,u1,v1,u2,v2,z;unsigned color;} RECVX_COM_DRAW;
typedef struct {unsigned st_flg,cb_flg,sp_flg;} RECVX_COM_SYSTEM;
typedef struct {unsigned pad_on,pad_ps;int real_time_clock;} RECVX_COM_INPUT;
typedef struct {
    int active,finished,stand_player,message_visible,message_number,message_lines;
    unsigned draw_count,sound_count,flag_count,unsupported_character;
    unsigned sounds[16],flags[16];
    RECVX_COM_DRAW draw[RECVX_COM_MAX_DRAW];
    RECVX_COM_SYSTEM system;
} RECVX_COM_FRAME;
// bhSysCall (system.c) entering cb_flg 0x400000: bhKeepSpEvtComputer and the task switch; com_num from bhEvtComSet.
void recvx_com_begin(unsigned number,const RECVX_COM_SYSTEM* system);
// One 30 Hz bhSysCallCompEvent pass. Returns 0 once bhExitSpEvtComputer has run.
int recvx_com_tick(const RECVX_COM_INPUT* input,RECVX_COM_FRAME* frame);
#ifdef __cplusplus
}
#endif
