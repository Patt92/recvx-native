#include "njplus.h"
#include "ps2_NaMatrix.h"

/* njplus.c:813-876.  The original NP_WORK storage is owned by the collision
 * prefix module; these routines intentionally share that same work area. */
extern NP_WORK np;

void core_np_push_mdlstr(NJS_CNK_OBJECT* objp, int obj_n)
{
    int i;

    for (i = 0; i < obj_n; i++, objp++)
    {
        if (objp->model != NULL)
        {
            np.vlist[i] = objp->model->vlist;
        }
    }
}

void core_np_pop_mdlstr(NJS_CNK_OBJECT* objp, int obj_n)
{
    int i;

    for (i = 0; i < obj_n; i++, objp++)
    {
        if (objp->model != NULL)
        {
            objp->model->vlist = np.vlist[i];
        }
    }
}

void core_np_push_mdlstr2(NJS_CNK_OBJECT* objp, int obj_n)
{
    int i;

    for (i = 0; i < obj_n; i++, objp++)
    {
        if ((objp->model != NULL) && (!(objp->evalflags & 0x8)))
        {
            np.mdlstr2[i].pos[0] = objp->pos[0];
            np.mdlstr2[i].pos[1] = objp->pos[1];
            np.mdlstr2[i].pos[2] = objp->pos[2];
            np.mdlstr2[i].vlist = objp->model->vlist;
            np.mdlstr2[i].plist = objp->model->plist;
        }
    }
}

void core_np_pop_mdlstr2(NJS_CNK_OBJECT* objp, int obj_n)
{
    int i;

    for (i = 0; i < obj_n; i++, objp++)
    {
        if ((objp->model != NULL) && (!(objp->evalflags & 0x8)))
        {
            objp->pos[0] = np.mdlstr2[i].pos[0];
            objp->pos[1] = np.mdlstr2[i].pos[1];
            objp->pos[2] = np.mdlstr2[i].pos[2];
            objp->model->vlist = np.mdlstr2[i].vlist;
            objp->model->plist = np.mdlstr2[i].plist;
        }
    }
}

/* njplus.c:1356-1444 */
unsigned int core_np_get_mat_color(NJS_CNK_OBJECT* objp, int obj_n)
{
    int i;
    unsigned int* pnt;

    for (i = 0; i < obj_n; i++, objp++)
    {
        if ((objp->model != NULL) && (!(objp->evalflags & 0x8)))
        {
            pnt = (unsigned int*)objp->model->plist;
            return *++pnt;
        }
    }

    return 0;
}

void core_np_set_all_mat_color(NJS_CNK_OBJECT* objp, int obj_n, unsigned int argb)
{
    int i;
    int offset;
    short head;
    short* plp;
    unsigned char* mat;
    unsigned char a = ((argb & 0xFF000000) >> 24) & 0xFF;
    unsigned char r = ((argb & 0xFF0000) >> 16) & 0xFF;
    unsigned char g = ((argb & 0xFF00) >> 8) & 0xFF;
    unsigned char b = argb & 0xFF;

    for (i = 0; i < obj_n; i++, objp++)
    {
        if ((objp->model != NULL) && (!(objp->evalflags & 0x8)))
        {
            plp = objp->model->plist;

            while (TRUE)
            {
                head = (unsigned char)*plp++;

                if ((head >= 64) && (head < 67))
                {
                    offset = *plp++;
                    plp += offset;
                }
                else if (head == 8)
                {
                    plp++;
                }
                else if ((head >= 17) && (head < 24))
                {
                    mat = (unsigned char*)plp + 2;

                    switch (head)
                    {
                    case 17:
                    case 21:
                        *mat++ = b;
                        *mat++ = g;
                        *mat++ = r;
                        if (*mat != 0) *mat = a;
                        break;
                    case 19:
                    case 23:
                        *mat++ = b;
                        *mat++ = g;
                        *mat++ = r;
                        if (*mat != 0) *mat = a;
                        mat++;
                        *mat++ = b;
                        *mat++ = g;
                        *mat++ = r;
                        if (*mat != 0) *mat = a;
                        break;
                    }

                    offset = *plp++;
                    plp += offset;
                }
                else if ((head >= 56) && (head < 59))
                {
                    offset = *plp++;
                    plp += offset;
                }
                else if (head == 255)
                {
                    break;
                }
            }
        }
    }
}

/* njplus.c:1761-1840 and 2058-2139.  The PS2 VU block copies the source
 * affine matrix twice, subtracts the actor root from its translation, then
 * removes the local object rotation from the first copy. */
static void core_np_init_skin_matrices(void* pwp, int obj_n, int* sknp, int fm)
{
    BH_PWORK* pwork = (BH_PWORK*)pwp;
    O_WORK* owp = pwork->mlwP->owP;
    NJS_CNK_OBJECT* op = pwork->mlwP->objP;
    int i;

    np.bp = np.buff + (obj_n * 128);
    njUnitMatrix(NULL);
    np.sknp = sknp;

    for (i = 0; i < obj_n; i++, op++, owp++)
    {
        int flg;
        int mno;
        int nb;
        unsigned int ulSize;

        if (fm && ((i >= 11) && (i <= 14))) continue;

        flg = *np.sknp++;
        if (flg == 0) continue;

        if (flg & 0x2)
        {
            mno = *np.sknp++;
            nb = *np.sknp++;
            np.sknp += nb;
            np.vlp[mno] = (int*)np.bp;
            ulSize = (nb * 32) + (fm ? 96 : 32);
            ulSize += 64 - (ulSize & 0x3F);
            np.bp += ulSize;
        }
        else
        {
            np.sknp++;
        }

        memcpy(np.mxp[i][0], &owp->mtx, sizeof(NJS_MATRIX));
        (*np.mxp[i][0])[12] -= pwork->px;
        (*np.mxp[i][0])[13] -= pwork->py;
        (*np.mxp[i][0])[14] -= pwork->pz;
        memcpy(np.mxp[i][1], np.mxp[i][0], sizeof(NJS_MATRIX));
        njRotXYZ(np.mxp[i][0], -op->ang[0], -op->ang[1], -op->ang[2]);
    }
}

static void core_np_transform4(const NJS_MATRIX matrix, const NJS_POINT4* source, NJS_POINT4* destination, int translation)
{
    destination->x = matrix[0] * source->x + matrix[4] * source->y + matrix[8] * source->z + (translation ? matrix[12] * source->w : 0.0f);
    destination->y = matrix[1] * source->x + matrix[5] * source->y + matrix[9] * source->z + (translation ? matrix[13] * source->w : 0.0f);
    destination->z = matrix[2] * source->x + matrix[6] * source->y + matrix[10] * source->z + (translation ? matrix[14] * source->w : 0.0f);
    destination->w = matrix[3] * source->x + matrix[7] * source->y + matrix[11] * source->z + (translation ? matrix[15] * source->w : 0.0f);
}

/* njplus.c:1843-2045 and 2142-2352.  Each encoded entry emits a position and
 * normal; VU scalar/vector operations are written out in the same order. */
static void core_np_calc_skin_impl(void* pwp, int obj_n, int* sknp, int fm)
{
    static const float level[10] = { 0.1000000015f, 0.200000003f, 0.3000000119f, 0.400000006f, 0.5f, 0.6000000238f, 0.6999999881f, 0.8000000119f, 0.8999999762f, 1.0f };
    NJS_CNK_OBJECT* op;
    int i;

    if (sknp == NULL) return;

    njPushMatrixEx();
    core_np_init_skin_matrices(pwp, obj_n, sknp, fm);
    np.sknp = sknp;
    op = ((BH_PWORK*)pwp)->mlwP->objP;

    for (i = 0; i < obj_n; i++, op++)
    {
        int flg;
        int rg0;
        int nb;
        int j;
        HDR_PS* pHdr0;
        HDR_PS* pHdr1;
        NJS_POINT4* p0;
        NJS_POINT4* p1;

        if (fm && ((i >= 11) && (i <= 14))) continue;

        flg = *np.sknp++;
        if (flg == 0) continue;
        if (!(flg & 0x2))
        {
            np.sknp++;
            continue;
        }

        pHdr0 = (HDR_PS*)op->model->vlist;
        rg0 = *np.sknp++;
        op->model->vlist = np.vlp[rg0];
        pHdr1 = (HDR_PS*)np.vlp[rg0];
        nb = *np.sknp++;
        pHdr1->ucType = pHdr0->ucType;
        pHdr1->ucAttr = pHdr0->ucAttr;
        pHdr1->usSize = pHdr0->usSize;
        pHdr1->usIndexOfs = pHdr0->usIndexOfs;
        pHdr1->usIndexMax = pHdr0->usIndexMax;
        p0 = (NJS_POINT4*)(pHdr0 + 1);
        p1 = (NJS_POINT4*)(pHdr1 + 1);

        for (j = 0; j < nb; j++)
        {
            float lv;
            NJS_POINT4 first;
            NJS_POINT4 second;

            rg0 = *np.sknp++;
            lv = level[(rg0 >> 16) & 0xFF];
            rg0 >>= 24;
            if (fm && rg0 > 10) rg0 += 4;

            core_np_transform4(*np.mxp[rg0][1], p0, &second, 1);
            if (lv != 1.0f)
            {
                core_np_transform4(*np.mxp[rg0][0], p0, &first, 1);
                p1->x = first.x + ((second.x - first.x) * lv);
                p1->y = first.y + ((second.y - first.y) * lv);
                p1->z = first.z + ((second.z - first.z) * lv);
                p1->w = first.w + ((second.w - first.w) * lv);
                p0++;
                p1++;

                core_np_transform4(*np.mxp[rg0][1], p0, &second, 0);
                core_np_transform4(*np.mxp[rg0][0], p0, &first, 0);
                p1->x = first.x + ((second.x - first.x) * lv);
                p1->y = first.y + ((second.y - first.y) * lv);
                p1->z = first.z + ((second.z - first.z) * lv);
                p1->w = first.w + ((second.w - first.w) * lv);
                p0++;
                p1++;
            }
            else
            {
                *p1++ = second;
                p0++;
                core_np_transform4(*np.mxp[rg0][1], p0, &second, 0);
                *p1++ = second;
                p0++;
            }
        }

        *(int*)p1 = 255;
    }

    njPopMatrixEx();
}

void core_np_init_calc_skin(void* pwp, int obj_n, int* sknp)
{
    core_np_init_skin_matrices(pwp, obj_n, sknp, 0);
}

void core_np_calc_skin(void* pwp, int obj_n, int* sknp)
{
    core_np_calc_skin_impl(pwp, obj_n, sknp, 0);
}

void core_np_init_calc_skin_fm(void* pwp, int obj_n, int* sknp)
{
    core_np_init_skin_matrices(pwp, obj_n, sknp, 1);
}

void core_np_calc_skin_fm(void* pwp, int obj_n, int* sknp)
{
    core_np_calc_skin_impl(pwp, obj_n, sknp, 1);
}

/* njplus.c:1746-1749.  The original routine has no instructions. */
void core_np_cut_skin(BH_PWORK* epw, int prm1, int prm2, int prm3, float prm4, float prm5, float prm6)
{
    (void)epw;
    (void)prm1;
    (void)prm2;
    (void)prm3;
    (void)prm4;
    (void)prm5;
    (void)prm6;
}

/* njplus.c:677-811.  In the original NP_WORK layout mxp[128][1] and
 * mxp[128][2] address the immediately following buff2/bp2 fields.  Address
 * those fields explicitly on the host instead of reproducing C out-of-bounds
 * indexing or the PS2 0x30000000 pointer tag. */
void core_np_transform(NJS_CNK_OBJECT* srcobj, NJS_CNK_OBJECT* dstobj, float no, int ono)
{
    int s_nb;
    NJS_POINT4* dp;
    NJS_POINT4* fp;
    NJS_POINT4* sp;
    HDR_PS* pSrc = (HDR_PS*)srcobj->model->vlist;
    HDR_PS* pDst = (HDR_PS*)dstobj->model->vlist;

    s_nb = pSrc->usIndexMax;
    if (s_nb != pDst->usIndexMax) return;

    sp = (NJS_POINT4*)(pSrc + 1);
    dp = (NJS_POINT4*)(pDst + 1);
    fp = (NJS_POINT4*)np.vlp2[ono];
    ((HDR_PS*)fp)->ucType = pSrc->ucType;
    ((HDR_PS*)fp)->ucAttr = pSrc->ucAttr;
    ((HDR_PS*)fp)->usSize = pSrc->usSize;
    ((HDR_PS*)fp)->usIndexOfs = pSrc->usIndexOfs;
    ((HDR_PS*)fp)->usIndexMax = s_nb;
    no *= 0.001f;
    fp = (NJS_POINT4*)((HDR_PS*)fp + 1);

    for (; s_nb != 0; s_nb--)
    {
        if ((sp->x != dp->x) || (sp->y != dp->y) || (sp->z != dp->z))
        {
            /* VU vsub/vmul/vadd use .xyz masks: destination w survives. */
            *fp = *dp;
            fp->x = sp->x + ((dp->x - sp->x) * no);
            fp->y = sp->y + ((dp->y - sp->y) * no);
            fp->z = sp->z + ((dp->z - sp->z) * no);
            fp++;
            sp++;
            dp++;

            *fp = *dp;
            fp->x = sp->x + ((dp->x - sp->x) * no);
            fp->y = sp->y + ((dp->y - sp->y) * no);
            fp->z = sp->z + ((dp->z - sp->z) * no);
            fp++;
            sp++;
            dp++;
        }
        else
        {
            *fp++ = *sp++;
            *fp++ = *sp++;
            dp += 2;
        }
    }

    ((HDR_PS*)fp)->ucType = 0xFF;
    srcobj->model->vlist = np.vlp2[ono];
}

void core_np_calc_morphing(NJS_CNK_OBJECT* obj_a, NJS_CNK_OBJECT* obj_b, float no, int obj_n)
{
    int i;

    njPushMatrix(NULL);
    np.bp2 = np.buff2;

    for (i = 0; i < obj_n; i++, obj_a++, obj_b++)
    {
        int nb;
        unsigned int ulSize;

        if ((obj_a->model == NULL) || (obj_a->evalflags & 0x8)) continue;

        obj_a->pos[0] = (no * (0.001f * (obj_b->pos[0] - obj_a->pos[0]))) + obj_a->pos[0];
        obj_a->pos[1] = (no * (0.001f * (obj_b->pos[1] - obj_a->pos[1]))) + obj_a->pos[1];
        obj_a->pos[2] = (no * (0.001f * (obj_b->pos[2] - obj_a->pos[2]))) + obj_a->pos[2];
        nb = ((unsigned short*)obj_a->model->vlist)[3];
        np.vlp2[i] = (int*)np.bp2;
        ulSize = (nb * 32) + 76;
        ulSize += 64 - (ulSize & 0x3F);
        np.bp2 += ulSize;
        core_np_transform(obj_a, obj_b, no, i);
    }

    njPopMatrix(1);
}
