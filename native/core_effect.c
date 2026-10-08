#include "effect.h"
#include "main.h"
#include "recvx_ptr32.h"

extern void core_np_set_memory_l(unsigned int* memory, unsigned int size, int value);

/* Shared tail of effect.c:330-546.  It deliberately executes after the
 * original whole-work clear, so fields not assigned by an entry point retain
 * their original zero value. */
static void core_effect_quad(O_WRK* opp)
{
    opp->mtx = (float(*)[16])opp->mtxbuf;
    opp->pvp = opp->pv;
    opp->tvp = opp->tv;
    opp->pn = 4;
    opp->tv[0].x = -1.0f; opp->tv[0].y = -1.0f; opp->tv[0].z = 0;
    opp->tv[1].x =  1.0f; opp->tv[1].y = -1.0f; opp->tv[1].z = 0;
    opp->tv[2].x = -1.0f; opp->tv[2].y =  1.0f; opp->tv[2].z = 0;
    opp->tv[3].x =  1.0f; opp->tv[3].y =  1.0f; opp->tv[3].z = 0;
    opp->tv[0].u = 0;    opp->tv[0].v = 0;
    opp->tv[1].u = 1.0f; opp->tv[1].v = 0;
    opp->tv[2].u = 0;    opp->tv[2].v = 1.0f;
    opp->tv[3].u = 1.0f; opp->tv[3].v = 1.0f;
}

/* effect.c:249-292 */
unsigned int core_bh_get_gidx(O_WRK* op)
{
    NJS_TEXLIST* tlp;
    NJS_TEXNAME* tnp;

    if (!(op->stflg & 0x20))
    {
        tlp = &sys->ef_tlist;
        tnp = &tlp->textures[sys->ef_tn[op->tex_id] + op->ani_ct];
    }
    else
    {
        tlp = (NJS_TEXLIST*)&op->txp[0]->textures;
        tnp = &tlp->textures[op->tex_id + op->ani_ct];
    }

    return ((NJS_TEXMEMLIST*)recvx_p32_resolve(tnp->texaddr))->globalIndex;
}

/* effect.c:295-315 */
O_WRK* core_bh_set_extra_effect_work(void)
{
    O_WRK* op = eff;
    int i;

    for (i = 0; i < 512; i++, op++)
    {
        if (!(op->flg & 0x3))
        {
            core_np_set_memory_l(&op->flg, sizeof(O_WRK) / 4, 0);
            op->flg = 2;
            return op;
        }
    }

    return NULL;
}

/* effect.c:330-438 */
int core_bh_set_effect(int effno, POINT* pnt, unsigned char* lkp, int lkono)
{
    O_WRK* opp = eff;
    int i;

    for (i = 0;;)
    {
        if (!(opp->flg & 0x3))
        {
            core_np_set_memory_l(&opp->flg, sizeof(O_WRK) / 4, 0);
            opp->flg = 1;
            opp->id = effno;
            opp->type = 0;
            opp->tex_id = -1;
            opp->mdlver = 0;
            opp->flr_no = 0;
            opp->px = pnt->px; opp->py = pnt->py; opp->pz = pnt->pz;
            opp->lox = pnt->ox; opp->loy = pnt->oy; opp->loz = pnt->oz;
            opp->sx = 1.0f; opp->sy = 1.0f; opp->sz = 1.0f;
            opp->ax = 0; opp->ay = 0; opp->az = 0;
            opp->mlwP = &sys->efm[effno];
            if (lkp != NULL) { opp->lkwkp = lkp; opp->flg |= 0x80; }
            else opp->lkwkp = NULL;
            opp->lkono = lkono;
            core_effect_quad(opp);
            return i;
        }
        i++;
        opp++;
        if (i >= 512) return -1;
    }
}

/* effect.c:556-644 */
int core_bh_set_effect_evt(int effno, POINT* pnt, unsigned short type, int ax, int ay)
{
    O_WRK* opp = eff;
    int i;

    for (i = 0;;)
    {
        if (!(opp->flg & 0x3))
        {
            core_np_set_memory_l(&opp->flg, sizeof(O_WRK) / 4, 0);
            opp->flg = 1;
            opp->id = effno;
            opp->type = type;
            opp->tex_id = -1;
            opp->mdlver = 0;
            opp->flr_no = 0;
            opp->px = pnt->px; opp->py = pnt->py; opp->pz = pnt->pz;
            opp->lox = pnt->ox; opp->loy = pnt->oy; opp->loz = pnt->oz;
            opp->sx = 1.0f; opp->sy = 1.0f; opp->sz = 1.0f;
            opp->ax = ax; opp->ay = ay; opp->az = 0;
            opp->mlwP = &sys->efm[effno];
            opp->lkwkp = NULL;
            opp->lkono = 0;
            core_effect_quad(opp);
            return i;
        }
        i++;
        opp++;
        if (i >= 512) return -1;
    }
}

/* effect.c:441-552 */
int core_bh_set_effect_tb(EF_WORK* efp, NJS_POINT3* off, unsigned char* lkp, int lkono)
{
    O_WRK* opp = eff;
    int i;

    for (i = 0;;)
    {
        if (!(opp->flg & 0x3))
        {
            core_np_set_memory_l(&opp->flg, sizeof(O_WRK) / 4, 0);
            opp->flg = efp->flg;
            opp->id = efp->id;
            opp->type = efp->type;
            opp->tex_id = -1;
            opp->mdlver = efp->mdlver;
            opp->flr_no = efp->flr_no;
            opp->px = efp->px; opp->py = efp->py; opp->pz = efp->pz;
            opp->sx = efp->sx; opp->sy = efp->sy; opp->sz = efp->sz;
            opp->sxb = efp->sx; opp->syb = efp->sy; opp->szb = efp->sz;
            opp->ax = efp->ax; opp->ay = efp->ay; opp->az = 0;
            opp->mlwP = &sys->efm[efp->id];
            if (off != NULL) { opp->lox = off->x; opp->loy = off->y; opp->loz = off->z; }
            if (lkp != NULL) { opp->lkwkp = lkp; opp->flg |= 0x80; }
            else opp->lkwkp = NULL;
            opp->lkono = lkono;
            core_effect_quad(opp);
            return i;
        }
        i++;
        opp++;
        if (i >= 512) return -1;
    }
}

/* effect.c:644-733 */
int core_bh_set_shadow(char* jtb, unsigned char* lkp, int lkono, float sx, float sy, float sz)
{
    O_WRK* opp = eff;
    int i;
    int j;

    for (i = 0; i < 512; i++, opp++)
    {
        if (!(opp->flg & 0x3))
        {
            core_np_set_memory_l(&opp->flg, sizeof(O_WRK) / 4, 0);
            opp->flg = 0x240001;
            opp->id = 1;
            opp->type = jtb != NULL ? 0 : 1;
            opp->tex_id = -1;
            opp->mdlver = 0;
            opp->px = 0; opp->py = 0; opp->pz = 0;
            opp->sx = sx; opp->sy = sy; opp->sz = sz;
            opp->sxb = sx; opp->syb = sy; opp->szb = sz;
            opp->ax = 0; opp->ay = 0; opp->az = 0;
            opp->mlwP = &sys->efm[opp->id];
            if (lkp != NULL) { opp->lkwkp = lkp; opp->flg |= 0x80; }
            else opp->lkwkp = NULL;
            opp->lkono = lkono;
            opp->mtx = (float(*)[16])opp->mtxbuf;
            opp->pvp = opp->pv;
            opp->tvp = opp->tv;

            if (jtb == NULL)
            {
                opp->jno[0] = -1;
            }
            else
            {
                for (j = 0; j < 15; j++)
                {
                    opp->jno[j] = *jtb++;
                    if (opp->jno[j] == -1) break;
                }
            }

            return i;
        }
    }

    return -1;
}

/* effect.c:736-739.  The original function has no instructions. */
void core_bh_link_blood(BH_PWORK* work, int joint, NJS_POINT3* offset, float scale)
{
    (void)work;
    (void)joint;
    (void)offset;
    (void)scale;
}

/* effect.c:322-328 */
void core_bh_delete_yakkyou(void)
{
    O_WRK* op = eff;
    int i;

    for (i = 0; i < 512; i++, op++)
    {
        if ((((op->flg & 0x1)) && (op->id == 0)) || (((op->flg & 0x1)) && (op->id == 7)))
        {
            op->flg = 0;
        }
    }
}
