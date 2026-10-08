/* Host port of ps2_NaMatrix.c:1719-1789.
 * The VU instructions there update the current matrix columns exactly as the
 * primitive Ninja operations below; the source-level ordering is preserved.
 */
#include "ninja_matrix.h"
#include <stddef.h>

void core_nj_translate_ex(NJS_VECTOR* vector)
{
    njTranslate(NULL, vector->x, vector->y, vector->z);
}

void core_nj_rotate_ex(Angle* angles, Sint32 local)
{
    if (local != 0) {
        njRotateY(NULL, angles[1]);
        njRotateX(NULL, angles[0]);
        njRotateZ(NULL, angles[2]);
    } else {
        njRotateZ(NULL, angles[2]);
        njRotateY(NULL, angles[1]);
        njRotateX(NULL, angles[0]);
    }
}

void core_nj_scale_ex(NJS_VECTOR* vector)
{
    njScale(NULL, vector->x, vector->y, vector->z);
}

/* ps2_NaMem.c:19-32 */
void core_nj_mem_copy4(void* dst, const void* src, Sint32 num)
{
    int* lpSrc = (int*)src;
    int* lpDst = (int*)dst;

    for (; num != 0; num--)
    {
        *lpDst++ = *lpSrc++;
    }
}
