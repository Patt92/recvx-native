#include "types.h"
#include "main.h"

extern void core_np_set_memory(unsigned char* memory, unsigned int size, char value);

/* objitm.c:154-222 */
O_WRK* core_bh_set_object(ETTY_WORK* otp, int no, unsigned char* lkp)
{
    O_WRK* opp = &sys->obwp[no];

    core_np_set_memory((unsigned char*)opp, sizeof(O_WRK), 0);
    opp->flg = otp->flg;
    if (opp->flg & 0x80000000) opp->mdflg |= 0x10;
    if (opp->flg & 0x20000000) opp->mdflg |= 0x40;
    opp->id = otp->id;
    opp->type = (unsigned char)otp->type;
    opp->param = otp->type >> 8;
    opp->flr_no = otp->flr_no;
    opp->mdlver = otp->mdlver;
    opp->draw_tp = otp->prm1;
    opp->px = otp->px; opp->py = otp->py; opp->pz = otp->pz;
    opp->ax = otp->ax; opp->ay = otp->ay; opp->az = otp->az;
    opp->aspd = otp->aspd;
    opp->sx = opp->sxb = 1.0f;
    opp->sy = opp->syb = 1.0f;
    opp->sz = opp->szb = 1.0f;
    opp->hide[0] = otp->hide[0]; opp->hide[1] = otp->hide[1];
    opp->hide[2] = otp->hide[2]; opp->hide[3] = otp->hide[3];
    opp->lkwkp = lkp;
    opp->mtx = (void*)opp->mtxbuf;
    opp->clp_jno[0] = 0;
    opp->clp_jno[1] = -1;
    opp->idx_ct = no;
    return opp;
}

/* objitm.c:119-124 */
void core_bh_init_obj_itm(void)
{
    core_np_set_memory((unsigned char*)sys->obwp, 39936, 0);
    core_np_set_memory((unsigned char*)sys->itwp, 39936, 0);
}

/* objitm.c:225-294 */
O_WRK* core_bh_set_item(ETTY_WORK* itp, int no, unsigned char* lkp)
{
    O_WRK* opp = &sys->itwp[no];

    core_np_set_memory((unsigned char*)opp, sizeof(O_WRK), 0);
    opp->flg = itp->flg;
    if (opp->flg & 0x80000000) opp->mdflg |= 0x10;
    if (opp->flg & 0x20000000) opp->mdflg |= 0x40;
    opp->id = itp->id;
    opp->type = (unsigned char)itp->type;
    opp->param = itp->type >> 8;
    opp->flr_no = itp->flr_no;
    opp->mdlver = itp->mdlver;
    opp->draw_tp = itp->prm1;
    opp->px = itp->px; opp->py = itp->py; opp->pz = itp->pz;
    opp->ax = itp->ax; opp->ay = itp->ay; opp->az = itp->az;
    opp->aspd = itp->aspd;
    opp->sx = opp->sxb = 1.0f;
    opp->sy = opp->syb = 1.0f;
    opp->sz = opp->szb = 1.0f;
    opp->hide[0] = itp->hide[0]; opp->hide[1] = itp->hide[1];
    opp->hide[2] = itp->hide[2]; opp->hide[3] = itp->hide[3];
    opp->lkwkp = lkp;
    opp->mtx = (void*)opp->mtxbuf;
    opp->clp_jno[0] = 0;
    opp->clp_jno[1] = -1;
    opp->idx_ct = no;
    return opp;
}
