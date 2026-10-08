#include "core_model.h"

/* ps2_dummy.c:33; initialized by the original main.c sequence. */
static NJS_MATRIX core_mbuf[128];

void core_nj_init_matrix_stack(void)
{
    njInitMatrix(core_mbuf, 128, 0);
}

/* MdlPut.c:240-266 */
void core_bh_calc_model(BH_PWORK* ewP)
{
    if ((ewP->flg & 0x80))
    {
        njSetMatrix(ewP->mtx, &((BH_PWORK*)ewP->lkwkp)->mlwP->owP[ewP->lkono].mtx);
        njTranslate(ewP->mtx, ewP->lox, ewP->loy, ewP->loz);

        if ((ewP->flg & 0x1000))
        {
            njUnitRotPortion(ewP->mtx);
        }

        njRotateXYZ(ewP->mtx, ewP->ax, ewP->ay, ewP->az);
    }
    else if (!(ewP->flg & 0x1000))
    {
        njUnitMatrix(ewP->mtx);
        njTranslate(ewP->mtx, ewP->px, ewP->py, ewP->pz);
        njRotateXYZ(ewP->mtx, ewP->ax, ewP->ay, ewP->az);
    }

    core_bh_calc_tree(ewP->mtx, ewP->mlwP);
}

/* MdlPut.c:268-310 */
void core_bh_calc_tree(NJS_MATRIX* basP, ML_WORK* mlwP)
{
    O_WORK* owP = mlwP->owP;
    NJS_CNK_OBJECT* objP = mlwP->objP;
    int obj_num;

    njPushMatrix(basP);

    if (basP == NULL)
    {
        njUnitMatrix(NULL);
    }

    obj_num = mlwP->obj_num;

    for (; obj_num > 0; obj_num--, objP++, owP++)
    {
        if (objP->sibling != NULL)
        {
            njPushMatrixEx();
        }

        njTranslate(NULL, objP->pos[0], objP->pos[1], objP->pos[2]);
        njRotateXYZ(NULL, objP->ang[0], objP->ang[1], objP->ang[2]);

        if (!(owP->flg & 0x1))
        {
            njGetMatrix(&owP->mtx);
        }
        else
        {
            njSetMatrix(NULL, &owP->mtx);
        }

        if (objP->child == NULL)
        {
            njPopMatrixEx();
        }
    }

    njPopMatrixEx();
}
