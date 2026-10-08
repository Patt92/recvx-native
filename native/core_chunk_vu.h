/* Native ports of the VU0/VU1 microprograms used by the Ninja chunk renderer.
 *
 * The original computes parts of the chunk pipeline in VU microcode
 * (archived/ps2/vsm/ps2_vu0.vsm, ps2_vu1.vsm; byte-identical to SLUS_201.84).
 * Each routine here is that program written as C: the same operations in the
 * same order, with the register contents that carry data between EE and VU
 * spelled as named inputs and outputs. Hardware timing appears only where it
 * decides a result; each such place names the instruction pair it rests on.
 * tests/native/chunk_vu_test.cpp compares every routine against the microcode
 * executed instruction by instruction (test-only interpreter).
 *
 * Plain C types only, so that the generated original sources and C++ tests can
 * both include this header. */
#ifndef RECVX_CORE_CHUNK_VU_H
#define RECVX_CORE_CHUNK_VU_H
#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * VU0_CALCPOINT_INIT / VU0_CALCPOINT / VU0_CALCPOINT_END (ps2_vu0.vsm 0x0050,
 * 0x01A0, 0x0168), driven by njCnkCvVnPs2 (ps2_NinjaCnk.c:1280).
 *
 * Inputs of VU0_CALCPOINT_INIT, as loaded by the EE before vcallms 0x38:
 *   v[0..2]   vf04..vf06  light X/Y/Z of lights 0..3 (one light per lane)
 *   v[3].x    vf07.x      state/mode bits (bit 7-i state, bit 3-i mode)
 *   v[4..6]   vf08..vf10  NearRR, FarRR, intensity of lights 0..3
 *   v[7..10]  vf11..vf14  colour (r,g,b,i) of light 0..3
 *   view      vf28..vf31  njSetMatrixCN matrix (row i = vf28+i)
 *   clip      vf24..vf27  ClipMatrix2 (loaded by _Make_ClipMatrix)
 *   aspect    vf17.xy     vu1SetScreenAspect
 *   projection vf23.z     vu1SetScreenProjection
 */
typedef struct CORE_VU0_CALCPOINT {
    float light[3][4];       /* vf24..vf26 during the program */
    unsigned int bits;       /* vi10 */
    float near_rr[4];        /* vf12 */
    float far_rr[4];         /* vf13 */
    float intensity[4];      /* vf27 */
    float color[4][3];       /* VU0 memory 71..74, .xyz */
    float view[4][4];        /* vf28..vf31 */
    float clip[4][4];        /* VU0 memory 56..59 */
    float scale[2];          /* vf17.xy after MULz.xy vf17, vf17, vf23z */
    unsigned int clip_and;   /* vi11 */
} CORE_VU0_CALCPOINT;

void core_vu0_calcpoint_init(CORE_VU0_CALCPOINT* cp, const float v[11][4], const float view[16],
                             const float clip[4][4], float aspect_w, float aspect_h, float projection);

/* One vertex: pos = vf04 (x,y,z used), nrm = vf05 (x,y,z used).
 * Writes VU1_STRIP_BUF +0x10 (vf07: colour, w unchanged = `color_w`),
 * +0x20 (vf18: view-space position incl. w), +0x30 (vf06: Sx, Sy, 1/z, Nz)
 * and returns vf11.x (1/z, the INVZ handed to njCalcFogPowerEx). */
float core_vu0_calcpoint(CORE_VU0_CALCPOINT* cp, const float pos[4], const float nrm[4],
                         float color_out[4], float view_out[4], float screen_out[4]);

#ifdef __cplusplus
}
#endif
#endif
