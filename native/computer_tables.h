#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// QUAD (sp_evt.c): screen rectangle plus its texture rectangle. The accessory
// lists end at an entry with x1 < 0; an entry with x1 == 0 is skipped but still
// counts for the flashing first slot.
typedef struct {float x1,y1,x2,y2,u1,v1,u2,v2;} RECVX_COM_QUAD;
// comevt_message_tab / comevt_script_tab / comevt_acs_tab, eight terminals.
// Entries 4..6 share one message table and accessory list (3D scanner).
extern const char* const* const recvx_com_message_tab[8];
extern const unsigned recvx_com_message_count[8];
extern const unsigned char* const recvx_com_script_tab[8];
extern const unsigned recvx_com_script_size[8];
extern const RECVX_COM_QUAD* const recvx_com_acs_tab[8];
#ifdef __cplusplus
}
#endif
