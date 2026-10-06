#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    unsigned id,flg,mtn_no,mtn_md,frm_mode,hokan_count;
    int mode0,mode1,mode2,mode3,ct0,ct1,ct2,ct3,ay,mtn_add,hokan_rate;
    float px,pz,spd;
    int64_t frm_no;
    void* mtn_tp;
    int advanced;
} RECVX_SUBPLAYER;
int recvx_is_subplayer(unsigned identity);
void recvx_subplayer_init(RECVX_SUBPLAYER* work);
// Returns zero for unimplemented dispatch modes; callers keep a diagnostic.
int recvx_subplayer_tick(RECVX_SUBPLAYER* work);
#ifdef __cplusplus
}
#endif
