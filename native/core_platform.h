/* Host side of platform interfaces used by generated original Ninja/GS routines
 * (status/ninja2d.c, tools/generate_status_sources.py). Only SDK macros, VU0
 * intrinsics and the scratchpad are supplied here; the routines themselves are
 * the original engine sources. Bit layouts follow the GS/GIF register formats
 * that the original libgraph macros encode. */
#ifndef RECVX_CORE_PLATFORM_H
#define RECVX_CORE_PLATFORM_H
#include "types.h"
#include "ps2_dummy.h"
#include "ps2_NaSystem.h"
#include "ps2_NaTextureFunction.h"
#include "ps2_NinjaCnk.h"
#include "ps2_NaSprite.h"
#include "ps2_NaDraw.h"
#include "ps2_NaDraw2D.h"
#include "ps2_NaFog.h"
#include "ps2_texture.h"
#include "main.h"
#include "status_platform.h"
#include "tim2.h"
#include "core_gs.h"
#include <limits.h>

typedef struct {unsigned int w[4];} u_long128;

/* GIF tag: NLOOP 0-14, EOP 15, PRE 46, PRIM 47-57, FLG 58-59, NREG 60-63. */
#define SCE_GIF_SET_TAG(nloop, eop, pre, prim, flg, nreg) \
    ((unsigned long)(nloop) | ((unsigned long)(eop) << 15) | ((unsigned long)(pre) << 46) | \
     ((unsigned long)(prim) << 47) | ((unsigned long)(flg) << 58) | ((unsigned long)(nreg) << 60))
#define SCE_GIF_PACKED 0
#define SCE_GIF_PACKED_AD 0x0e
#define SCE_GS_FALSE 0
#define SCE_GS_TRUE 1
/* PRIM register: PRIM 0-2, IIP 3, TME 4, FGE 5, ABE 6, AA1 7, FST 8, CTXT 9, FIX 10. */
#define SCE_GS_SET_PRIM(prim, iip, tme, fge, abe, aa1, fst, ctxt, fix) \
    ((unsigned long)(prim) | ((unsigned long)(iip) << 3) | ((unsigned long)(tme) << 4) | ((unsigned long)(fge) << 5) | \
     ((unsigned long)(abe) << 6) | ((unsigned long)(aa1) << 7) | ((unsigned long)(fst) << 8) | ((unsigned long)(ctxt) << 9) | \
     ((unsigned long)(fix) << 10))
#define SCE_GS_PRIM_POINT 0
#define SCE_GS_PRIM_LINE 1
#define SCE_GS_PRIM_LINESTRIP 2
#define SCE_GS_PRIM_TRI 3
#define SCE_GS_PRIM_TRISTRIP 4
#define SCE_GS_PRIM_TRIFAN 5
#define SCE_GS_PRIM_SPRITE 6
/* TEST register: ATE 0, ATST 1-3, AREF 4-11, AFAIL 12-13, DATE 14, DATM 15, ZTE 16, ZTST 17-18. */
#define SCE_GS_SET_TEST(ate, atst, aref, afail, date, datm, zte, ztst) \
    ((unsigned long)(ate) | ((unsigned long)(atst) << 1) | ((unsigned long)(aref) << 4) | ((unsigned long)(afail) << 12) | \
     ((unsigned long)(date) << 14) | ((unsigned long)(datm) << 15) | ((unsigned long)(zte) << 16) | ((unsigned long)(ztst) << 17))
#define SCE_GS_SET_TEST_1 SCE_GS_SET_TEST
#define SCE_GS_ALPHA_NEVER 0
#define SCE_GS_ALPHA_ALWAYS 1
#define SCE_GS_ALPHA_LESS 2
#define SCE_GS_ALPHA_LEQUAL 3
#define SCE_GS_ALPHA_EQUAL 4
#define SCE_GS_ALPHA_GEQUAL 5
#define SCE_GS_ALPHA_GREATER 6
#define SCE_GS_ALPHA_NOTEQUAL 7
#define SCE_GS_AFAIL_KEEP 0
#define SCE_GS_AFAIL_FB_ONLY 1
#define SCE_GS_AFAIL_ZB_ONLY 2
#define SCE_GS_AFAIL_RGB_ONLY 3
#define SCE_GS_DEPTH_NEVER 0
#define SCE_GS_DEPTH_ALWAYS 1
#define SCE_GS_DEPTH_GEQUAL 2
#define SCE_GS_DEPTH_GREATER 3
/* ZBUF register: ZBP 0-8, PSM 24-27, ZMSK 32. */
#define SCE_GS_SET_ZBUF(zbp, psm, zmsk) \
    ((unsigned long)(zbp) | ((unsigned long)(psm) << 24) | ((unsigned long)(zmsk) << 32))
#define SCE_GS_SET_ZBUF_1 SCE_GS_SET_ZBUF
#define SCE_GS_PSMCT16S 0x0a
/* RGBAQ register: R 0-7, G 8-15, B 16-23, A 24-31, Q 32-63. */
#define SCE_GS_SET_RGBAQ(r, g, b, a, q) \
    ((unsigned long)(r) | ((unsigned long)(g) << 8) | ((unsigned long)(b) << 16) | ((unsigned long)(a) << 24) | \
     ((unsigned long)(q) << 32))
/* GS texture/clamp/scissor registers (libgraph bit layouts). */
#define SCE_GS_SET_TEX0_1(tbp0, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa, cld) \
    ((unsigned long)(tbp0) | ((unsigned long)(tbw) << 14) | ((unsigned long)(psm) << 20) | ((unsigned long)(tw) << 26) | \
     ((unsigned long)(th) << 30) | ((unsigned long)(tcc) << 34) | ((unsigned long)(tfx) << 35) | \
     ((unsigned long)(cbp) << 37) | ((unsigned long)(cpsm) << 51) | ((unsigned long)(csm) << 55) | \
     ((unsigned long)(csa) << 56) | ((unsigned long)(cld) << 61))
#define SCE_GS_SET_TEX1_1(lcm, mxl, mmag, mmin, mtba, l, k) \
    ((unsigned long)(lcm) | ((unsigned long)(mxl) << 2) | ((unsigned long)(mmag) << 5) | ((unsigned long)(mmin) << 6) | \
     ((unsigned long)(mtba) << 9) | ((unsigned long)(l) << 19) | ((unsigned long)(k) << 32))
#define SCE_GS_SET_CLAMP_1(wms, wmt, minu, maxu, minv, maxv) \
    ((unsigned long)(wms) | ((unsigned long)(wmt) << 2) | ((unsigned long)(minu) << 4) | ((unsigned long)(maxu) << 14) | \
     ((unsigned long)(minv) << 24) | ((unsigned long)(maxv) << 34))
#define SCE_GS_SET_SCISSOR(x0, x1, y0, y1) \
    ((unsigned long)(x0) | ((unsigned long)(x1) << 16) | ((unsigned long)(y0) << 32) | ((unsigned long)(y1) << 48))
#define SCE_GS_PSMCT32 0x00
#define SCE_GS_PSMT8 0x13
#define SCE_GS_PSMT4 0x14
#define SCE_GS_MODULATE 0
#define SCE_GS_NEAREST 0
#define SCE_GS_LINEAR 1
#define SCE_GS_REPEAT 0
#define SCE_GS_CLAMP 1
#define SCE_GS_TEX0_1 0x06
#define SCE_GS_CLAMP_1 0x08
#define SCE_GS_TEX1_1 0x14
#define SCE_GS_SCISSOR_1 0x40
#define SCE_GS_TEXFLUSH 0x3f
int Send_Tim2_dataEx(void* tim2_top_adr, unsigned long send_image_adr, unsigned long send_clut_adr); /* ps2_loadtim2.h:23 */
/* GS register addresses (A+D). */
#define SCE_GS_PRIM 0x00
#define SCE_GS_RGBAQ 0x01
#define SCE_GS_ST 0x02
#define SCE_GS_UV 0x03
#define SCE_GS_XYZF2 0x04
#define SCE_GS_XYZ2 0x05
#define SCE_GS_FOGCOL 0x3d
#define SCE_GS_TEST_1 0x47
#define SCE_GS_ALPHA_1 0x42
#define SCE_GS_PABE 0x49
#define SCE_GS_FBA_1 0x4a
#define SCE_GS_ZBUF_1 0x4e

/* EE scratchpad (0x70000000) used as the GIF packet build area. */
extern unsigned long recvx_core_gs_scratchpad[1024];
#undef WORKBASE
#define WORKBASE (recvx_core_gs_scratchpad)

/* NJS_TEXNAME.texaddr holds a texture-table index on the host (generator representation rewrite). */
NJS_TEXMEMLIST* recvx_core_texmem(unsigned int handle);

/* VU0 macro-mode conversions (fixed point with 0, 4 or 12 fraction bits). */
void sceVu0ITOF12Vector(float* v0, int* v1);
void sceVu0FTOI0Vector(int* v0, float* v1);
void sceVu0FTOI4Vector(int* v0, float* v1);

/* GS/OT services implemented by the host GS. */
int Ps2TexLoad(NJS_TEXMEMLIST* addr);
unsigned int CoreBitCount(unsigned int value);
void CoreAddOT(void* p, unsigned int num, float z, unsigned long prim);
void CoreClearOT(void);
void CoreDrawOTag(void);
void CoreZbuffOffI(void);

extern NJS_TEXMEMLIST* Ps2_tex_info;
extern unsigned int Ps2_texmemlist_num;
extern NJS_TEXLIST* Ps2_current_texlist;
extern unsigned int Ps2_current_texno;
extern NJS_TEXMEMLIST* Ps2_current_texmemlist;
extern unsigned int Ps2_current_gindex;
extern PS2_NJ_SAVE Ps2_nj_save_current;
extern float Ps2_zbuff_a, Ps2_zbuff_b;
extern float fNaViwClipNear, fNaViwClipFar;
extern unsigned char ucNaCnkAttr;
extern float fNaCnkConstantA, fNaCnkConstantR, fNaCnkConstantG, fNaCnkConstantB;
extern NJS_TEXMEMLIST* Ps2_now_tex;
extern unsigned int Ps2_now_bank;
extern unsigned int Ps2_use_pt_flag, Ps2_tex_load_tp_cancel;
extern unsigned long ulNaFogR, ulNaFogG, ulNaFogB;
#endif
