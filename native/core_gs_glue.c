/* Hardware boundary for the original status/inventory GIF code.
 * Game routines are generated unchanged by tools/generate_status_sources.py;
 * this file only replaces EE DMA submission and VU0 conversion entry points. */
#include "core_platform.h"
#include "core_gs.h"

unsigned long recvx_core_gs_scratchpad[1024];

NJS_TEXMEMLIST* recvx_core_texmem(unsigned int handle)
{
    return Ps2_tex_info && handle < Ps2_texmemlist_num ? &Ps2_tex_info[handle] : 0;
}

void sceVu0ITOF12Vector(float* dst, int* src)
{
    int i;
    for (i = 0; i < 4; ++i) dst[i] = (float)src[i] / 4096.0f;
}

void sceVu0FTOI0Vector(int* dst, float* src)
{
    int i;
    for (i = 0; i < 4; ++i) dst[i] = (int)src[i];
}

void sceVu0FTOI4Vector(int* dst, float* src)
{
    int i;
    for (i = 0; i < 4; ++i) dst[i] = (int)(src[i] * 16.0f);
}

/* The original loadImage call uses the GS DMA tag at WORKBASE. The tag argument
 * is an EE address/sentinel and is intentionally not dereferenced on the host. */
void loadImage(void* tags)
{
    (void)tags;
    recvx_core_gs_execute((const uint64_t*)recvx_core_gs_scratchpad);
}

void D2_SyncTag(void) {}
void SyncPath(void) {}

void CoreZbuffOffI(void)
{
    recvx_core_gs_scratchpad[0] = 0x70000002ull;
    recvx_core_gs_scratchpad[1] = 0;
    recvx_core_gs_scratchpad[2] = SCE_GIF_SET_TAG(1, 1, 0, 0, SCE_GIF_PACKED, 1);
    recvx_core_gs_scratchpad[3] = SCE_GIF_PACKED_AD;
    recvx_core_gs_scratchpad[4] = SCE_GS_SET_TEST_1(1, SCE_GS_ALPHA_GEQUAL, 128, SCE_GS_AFAIL_KEEP, 0, 0, 1, SCE_GS_DEPTH_GEQUAL);
    recvx_core_gs_scratchpad[5] = SCE_GS_TEST_1;
    loadImage((void*)0xF0000000);
}
