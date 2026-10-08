/* Host port of ps2_NaMatrix.c:1719-1789.
 * The VU instructions there update the current matrix columns exactly as the
 * primitive Ninja operations below; the source-level ordering is preserved.
 */
#include "ninja_matrix.h"
#include <stddef.h>
#include <string.h>

extern Float njInvertSqrt(Float value);
extern void njSinCos(Angle angle, Float* sine, Float* cosine);

typedef struct CORE_NJ_SAVE
{
    unsigned char mode_bk[2];
    unsigned char set_last;
    unsigned char dc_alpha;
    int mode;
} CORE_NJ_SAVE;
typedef signed char CORE_NJS_SYS_ATTR[192];

static CORE_NJ_SAVE core_nj_save_current;

/* ps2_NaSystem.c:486-495. NJS_SYS_ATTR is 192 bytes, but the original API
 * copies exactly the 8-byte PS2_NJ_SAVE state. */
void core_nj_get_system_attr(CORE_NJS_SYS_ATTR* attr)
{
    memcpy(attr, &core_nj_save_current, sizeof(core_nj_save_current));
}

void core_nj_set_system_attr(CORE_NJS_SYS_ATTR* attr)
{
    memcpy(&core_nj_save_current, attr, sizeof(core_nj_save_current));
}

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

/* ps2_NaMatrix.c:737-857.  The VU routine builds the same unit-quaternion
 * rotation as the equations below, then multiplies the matrix's three basis
 * columns; its translation column is not touched. */
void core_nj_rotate(NJS_MATRIX* matrix, NJS_VECTOR* axis, Angle angle)
{
    Float sine, cosine;
    Float inverse_length;
    Float x, y, z, w;
    Float rotation[3][3];
    Float source[3][3];
    Float* target;
    int row, column;

    if (matrix == NULL)
        matrix = pNaMatMatrixStuckPtr;

    angle &= 0xffff;
    angle /= 2;
    njSinCos(angle, &sine, &cosine);

    inverse_length = njInvertSqrt((axis->x * axis->x + axis->y * axis->y) + axis->z * axis->z);
    x = axis->x * inverse_length * sine;
    y = axis->y * inverse_length * sine;
    z = axis->z * inverse_length * sine;
    w = cosine;

    rotation[0][0] = 1.0f - 2.0f * (y * y + z * z);
    rotation[0][1] = 2.0f * (x * y - z * w);
    rotation[0][2] = 2.0f * (x * z + y * w);
    rotation[1][0] = 2.0f * (x * y + z * w);
    rotation[1][1] = 1.0f - 2.0f * (x * x + z * z);
    rotation[1][2] = 2.0f * (y * z - x * w);
    rotation[2][0] = 2.0f * (x * z - y * w);
    rotation[2][1] = 2.0f * (y * z + x * w);
    rotation[2][2] = 1.0f - 2.0f * (x * x + y * y);

    target = *matrix;
    for (column = 0; column < 3; ++column)
        for (row = 0; row < 3; ++row)
            source[column][row] = target[column * 4 + row];

    for (column = 0; column < 3; ++column)
        for (row = 0; row < 3; ++row)
            target[column * 4 + row] =
                (source[0][row] * rotation[0][column] +
                 source[1][row] * rotation[1][column]) +
                source[2][row] * rotation[2][column];
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
