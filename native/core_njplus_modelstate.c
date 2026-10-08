#include "njplus.h"

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
