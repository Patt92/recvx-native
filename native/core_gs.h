/* Host graphics synthesizer used by the generated Ninja/GS routines.
 * Original engine routines build GIF packets; this core executes them on a
 * 640x480 framebuffer with a 16-bit Z buffer. Only plain C types cross this
 * header so that both the C platform glue and C++ owners can use it. */
#ifndef RECVX_CORE_GS_H
#define RECVX_CORE_GS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {RECVX_CORE_GS_WIDTH = 640, RECVX_CORE_GS_HEIGHT = 480};
/* Executes a DMA-tag-prefixed GIF packet (as built in the EE scratchpad). */
void recvx_core_gs_execute(const uint64_t* packet);
/* Texture binding for TME primitives: TIM2 picture in EE memory (NULL none) and palette bank. */
void recvx_core_gs_bind_texture(const void* tim2, unsigned bank);
void recvx_core_gs_scissor(int x0, int y0, int x1, int y1);
void recvx_core_gs_scissor_reset(void);
void recvx_core_gs_clear(uint32_t argb);
/* OT entries recorded by the original ordering-table path: texture/bank/ALPHA and GIF data. */
void recvx_core_gs_ot_add(unsigned otz, const void* tim2, unsigned bank, uint64_t alpha, const uint64_t* gif, unsigned qwords);
void recvx_core_gs_ot_clear(void);
void recvx_core_gs_ot_draw(void);
/* Framebuffer, ARGB with the GS alpha range (0x80 = 1.0). */
const uint32_t* recvx_core_gs_framebuffer(void);
#ifdef __cplusplus
}
#endif
#endif
